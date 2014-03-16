/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 2004-2005 Allan Sandfeld Jensen (kde@carewolf.com)
 * Copyright (C) 2006, 2007 Nicholas Shanks (webkit@nickshanks.com)
 * Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010, 2011, 2012 Apple Inc. All rights reserved.
 * Copyright (C) 2007 Alexey Proskuryakov <ap@webkit.org>
 * Copyright (C) 2007, 2008 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
 * Copyright (c) 2011, Code Aurora Forum. All rights reserved.
 * Copyright (C) Research In Motion Limited 2011. All rights reserved.
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "core/css/RuleSet.h"

#include "HTMLNames.h"
#include "RuntimeEnabledFeatures.h"
#include "core/css/CSSFontSelector.h"
#include "core/css/CSSSelector.h"
#include "core/css/CSSSelectorList.h"
#include "core/css/SelectorChecker.h"
#include "core/css/SelectorCheckerFastPath.h"
#include "core/css/SelectorFilter.h"
#include "core/css/StyleRuleImport.h"
#include "core/css/StyleSheetContents.h"
#include "core/html/track/TextTrackCue.h"
#include "heap/HeapTerminatedArrayBuilder.h"
#include "platform/TraceEvent.h"
#include "platform/weborigin/SecurityOrigin.h"

#include "wtf/TerminatedArrayBuilder.h"

namespace WebCore {

using namespace HTMLNames;

// -----------------------------------------------------------------

static inline bool isSelectorMatchingHTMLBasedOnRuleHash(const CSSSelector& selector)
{
    if (selector.m_match == CSSSelector::Tag) {
        const AtomicString& selectorNamespace = selector.tagQName().namespaceURI();
        if (selectorNamespace != starAtom && selectorNamespace != xhtmlNamespaceURI)
            return false;
        if (selector.relation() == CSSSelector::SubSelector) {
            ASSERT(selector.tagHistory());
            return isSelectorMatchingHTMLBasedOnRuleHash(*selector.tagHistory());
        }
        return true;
    }
    if (SelectorChecker::isCommonPseudoClassSelector(selector))
        return true;
    return selector.m_match == CSSSelector::Id || selector.m_match == CSSSelector::Class;
}

static inline bool selectorListContainsUncommonAttributeSelector(const CSSSelector* selector)
{
    const CSSSelectorList* selectorList = selector->selectorList();
    if (!selectorList)
        return false;
    for (const CSSSelector* selector = selectorList->first(); selector; selector = CSSSelectorList::next(*selector)) {
        for (const CSSSelector* component = selector; component; component = component->tagHistory()) {
            if (component->isAttributeSelector())
                return true;
        }
    }
    return false;
}

static inline bool isCommonAttributeSelectorAttribute(const QualifiedName& attribute)
{
    // These are explicitly tested for equality in canShareStyleWithElement.
    return attribute == typeAttr || attribute == readonlyAttr;
}

static inline bool containsUncommonAttributeSelector(const CSSSelector& selector)
{
    const CSSSelector* current = &selector;
    for (; current; current = current->tagHistory()) {
        // Allow certain common attributes (used in the default style) in the selectors that match the current element.
        if (current->isAttributeSelector() && !isCommonAttributeSelectorAttribute(current->attribute()))
            return true;
        if (selectorListContainsUncommonAttributeSelector(current))
            return true;
        if (current->relation() != CSSSelector::SubSelector) {
            current = current->tagHistory();
            break;
        }
    }

    for (; current; current = current->tagHistory()) {
        if (current->isAttributeSelector())
            return true;
        if (selectorListContainsUncommonAttributeSelector(current))
            return true;
    }
    return false;
}

static inline PropertyWhitelistType determinePropertyWhitelistType(const AddRuleFlags addRuleFlags, const CSSSelector& selector)
{
    for (const CSSSelector* component = &selector; component; component = component->tagHistory()) {
        if (component->pseudoType() == CSSSelector::PseudoCue || (component->m_match == CSSSelector::PseudoElement && component->value() == TextTrackCue::cueShadowPseudoId()))
            return PropertyWhitelistCue;
    }
    return PropertyWhitelistNone;
}

RuleData::RuleData(StyleRule* rule, unsigned selectorIndex, unsigned position, AddRuleFlags addRuleFlags)
    : m_rule(rule)
    , m_selectorIndex(selectorIndex)
    , m_isLastInArray(false)
    , m_position(position)
    , m_hasFastCheckableSelector((addRuleFlags & RuleCanUseFastCheckSelector) && SelectorCheckerFastPath::canUse(selector()))
    , m_specificity(selector().specificity())
    , m_hasMultipartSelector(!!selector().tagHistory())
    , m_hasRightmostSelectorMatchingHTMLBasedOnRuleHash(isSelectorMatchingHTMLBasedOnRuleHash(selector()))
    , m_containsUncommonAttributeSelector(WebCore::containsUncommonAttributeSelector(selector()))
    , m_linkMatchType(SelectorChecker::determineLinkMatchType(selector()))
    , m_hasDocumentSecurityOrigin(addRuleFlags & RuleHasDocumentSecurityOrigin)
    , m_propertyWhitelistType(determinePropertyWhitelistType(addRuleFlags, selector()))
{
    ASSERT(m_position == position);
    ASSERT(m_selectorIndex == selectorIndex);
    SelectorFilter::collectIdentifierHashes(selector(), m_descendantSelectorIdentifierHashes, maximumIdentifierCount);
}

void RuleSet::addToRuleSet(const AtomicString& key, PendingRuleMap& map, const RuleData& ruleData)
{
    OwnPtrWillBeMember<WillBeHeapLinkedStack<RuleData> >& rules = map.add(key, nullptr).storedValue->value;
    if (!rules)
        rules = adoptPtrWillBeNoop(new WillBeHeapLinkedStack<RuleData>);
    rules->push(ruleData);
}

static void extractValuesforSelector(const CSSSelector* selector, AtomicString& id, AtomicString& className, AtomicString& customPseudoElementName, AtomicString& tagName)
{
    switch (selector->m_match) {
    case CSSSelector::Id:
        id = selector->value();
        break;
    case CSSSelector::Class:
        className = selector->value();
        break;
    case CSSSelector::Tag:
        if (selector->tagQName().localName() != starAtom)
            tagName = selector->tagQName().localName();
        break;
    }
    if (selector->isCustomPseudoElement())
        customPseudoElementName = selector->value();
}

bool RuleSet::findBestRuleSetAndAdd(const CSSSelector& component, RuleData& ruleData)
{
    AtomicString id;
    AtomicString className;
    AtomicString customPseudoElementName;
    AtomicString tagName;

#ifndef NDEBUG
    m_allRules.append(ruleData);
#endif

    const CSSSelector* it = &component;
    for (; it->relation() == CSSSelector::SubSelector; it = it->tagHistory()) {
        extractValuesforSelector(it, id, className, customPseudoElementName, tagName);
    }
    extractValuesforSelector(it, id, className, customPseudoElementName, tagName);

    // Prefer rule sets in order of most likely to apply infrequently.
    if (!id.isEmpty()) {
        addToRuleSet(id, ensurePendingRules()->idRules, ruleData);
        return true;
    }
    if (!className.isEmpty()) {
        addToRuleSet(className, ensurePendingRules()->classRules, ruleData);
        return true;
    }
    if (!customPseudoElementName.isEmpty()) {
        // Custom pseudos come before ids and classes in the order of tagHistory, and have a relation of
        // ShadowPseudo between them. Therefore we should never be a situation where extractValuesforSelector
        // finsd id and className in addition to custom pseudo.
        ASSERT(id.isEmpty() && className.isEmpty());
        addToRuleSet(customPseudoElementName, ensurePendingRules()->shadowPseudoElementRules, ruleData);
        return true;
    }

    if (component.pseudoType() == CSSSelector::PseudoCue) {
        m_cuePseudoRules.append(ruleData);
        return true;
    }

    if (SelectorChecker::isCommonPseudoClassSelector(component)) {
        switch (component.pseudoType()) {
        case CSSSelector::PseudoLink:
        case CSSSelector::PseudoVisited:
        case CSSSelector::PseudoAnyLink:
            m_linkPseudoClassRules.append(ruleData);
            return true;
        case CSSSelector::PseudoFocus:
            m_focusPseudoClassRules.append(ruleData);
            return true;
        default:
            ASSERT_NOT_REACHED();
            return true;
        }
    }

    if (!tagName.isEmpty()) {
        addToRuleSet(tagName, ensurePendingRules()->tagRules, ruleData);
        return true;
    }

    return false;
}

void RuleSet::addRule(StyleRule* rule, unsigned selectorIndex, AddRuleFlags addRuleFlags)
{
    RuleData ruleData(rule, selectorIndex, m_ruleCount++, addRuleFlags);
    m_features.collectFeaturesFromRuleData(ruleData);

    if (!findBestRuleSetAndAdd(ruleData.selector(), ruleData)) {
        // If we didn't find a specialized map to stick it in, file under universal rules.
        m_universalRules.append(ruleData);
    }
}

void RuleSet::addPageRule(StyleRulePage* rule)
{
    ensurePendingRules(); // So that m_pageRules.shrinkToFit() gets called.
    m_pageRules.append(rule);
}

void RuleSet::addViewportRule(StyleRuleViewport* rule)
{
    ensurePendingRules(); // So that m_viewportRules.shrinkToFit() gets called.
    m_viewportRules.append(rule);
}

void RuleSet::addFontFaceRule(StyleRuleFontFace* rule)
{
    ensurePendingRules(); // So that m_fontFaceRules.shrinkToFit() gets called.
    m_fontFaceRules.append(rule);
}

void RuleSet::addKeyframesRule(StyleRuleKeyframes* rule)
{
    ensurePendingRules(); // So that m_keyframesRules.shrinkToFit() gets called.
    m_keyframesRules.append(rule);
}

void RuleSet::addChildRules(const WillBeHeapVector<RefPtrWillBeMember<StyleRuleBase> >& rules, const MediaQueryEvaluator& medium, AddRuleFlags addRuleFlags)
{
    for (unsigned i = 0; i < rules.size(); ++i) {
        StyleRuleBase* rule = rules[i].get();

        if (rule->isStyleRule()) {
            StyleRule* styleRule = toStyleRule(rule);

            const CSSSelectorList& selectorList = styleRule->selectorList();
            for (size_t selectorIndex = 0; selectorIndex != kNotFound; selectorIndex = selectorList.indexOfNextSelectorAfter(selectorIndex)) {
                if (selectorList.hasCombinatorCrossingTreeBoundaryAt(selectorIndex)) {
                    m_treeBoundaryCrossingRules.append(MinimalRuleData(styleRule, selectorIndex, addRuleFlags));
                } else if (selectorList.hasShadowDistributedAt(selectorIndex)) {
                    m_shadowDistributedRules.append(MinimalRuleData(styleRule, selectorIndex, addRuleFlags));
                } else {
                    addRule(styleRule, selectorIndex, addRuleFlags);
                }
            }
        } else if (rule->isPageRule()) {
            addPageRule(toStyleRulePage(rule));
        } else if (rule->isMediaRule()) {
            StyleRuleMedia* mediaRule = toStyleRuleMedia(rule);
            if ((!mediaRule->mediaQueries() || medium.eval(mediaRule->mediaQueries(), &m_viewportDependentMediaQueryResults)))
                addChildRules(mediaRule->childRules(), medium, addRuleFlags);
        } else if (rule->isFontFaceRule()) {
            addFontFaceRule(toStyleRuleFontFace(rule));
        } else if (rule->isKeyframesRule()) {
            addKeyframesRule(toStyleRuleKeyframes(rule));
        } else if (rule->isViewportRule()) {
            addViewportRule(toStyleRuleViewport(rule));
        } else if (rule->isSupportsRule() && toStyleRuleSupports(rule)->conditionIsSupported()) {
            addChildRules(toStyleRuleSupports(rule)->childRules(), medium, addRuleFlags);
        }
    }
}

void RuleSet::addRulesFromSheet(StyleSheetContents* sheet, const MediaQueryEvaluator& medium, AddRuleFlags addRuleFlags)
{
    TRACE_EVENT0("webkit", "RuleSet::addRulesFromSheet");

    ASSERT(sheet);

    addRuleFlags = static_cast<AddRuleFlags>(addRuleFlags | RuleCanUseFastCheckSelector);
    const WillBeHeapVector<RefPtrWillBeMember<StyleRuleImport> >& importRules = sheet->importRules();
    for (unsigned i = 0; i < importRules.size(); ++i) {
        StyleRuleImport* importRule = importRules[i].get();
        if (importRule->styleSheet() && (!importRule->mediaQueries() || medium.eval(importRule->mediaQueries(), &m_viewportDependentMediaQueryResults)))
            addRulesFromSheet(importRule->styleSheet(), medium, addRuleFlags);
    }

    addChildRules(sheet->childRules(), medium, addRuleFlags);
}

void RuleSet::addStyleRule(StyleRule* rule, AddRuleFlags addRuleFlags)
{
    for (size_t selectorIndex = 0; selectorIndex != kNotFound; selectorIndex = rule->selectorList().indexOfNextSelectorAfter(selectorIndex))
        addRule(rule, selectorIndex, addRuleFlags);
}

void RuleSet::compactPendingRules(PendingRuleMap& pendingMap, CompactRuleMap& compactMap)
{
    PendingRuleMap::iterator end = pendingMap.end();
    for (PendingRuleMap::iterator it = pendingMap.begin(); it != end; ++it) {
        OwnPtrWillBeRawPtr<WillBeHeapLinkedStack<RuleData> > pendingRules = it->value.release();
        CompactRuleMap::ValueType* compactRules = compactMap.add(it->key, nullptr).storedValue;

        WillBeHeapTerminatedArrayBuilder<RuleData> builder(compactRules->value.release());
        builder.grow(pendingRules->size());
        while (!pendingRules->isEmpty()) {
            builder.append(pendingRules->peek());
            pendingRules->pop();
        }

        compactRules->value = builder.release();
    }
}

void RuleSet::compactRules()
{
    ASSERT(m_pendingRules);
    OwnPtrWillBeRawPtr<PendingRuleMaps> pendingRules = m_pendingRules.release();
    compactPendingRules(pendingRules->idRules, m_idRules);
    compactPendingRules(pendingRules->classRules, m_classRules);
    compactPendingRules(pendingRules->tagRules, m_tagRules);
    compactPendingRules(pendingRules->shadowPseudoElementRules, m_shadowPseudoElementRules);
    m_linkPseudoClassRules.shrinkToFit();
    m_cuePseudoRules.shrinkToFit();
    m_focusPseudoClassRules.shrinkToFit();
    m_universalRules.shrinkToFit();
    m_pageRules.shrinkToFit();
    m_viewportRules.shrinkToFit();
    m_fontFaceRules.shrinkToFit();
    m_keyframesRules.shrinkToFit();
    m_treeBoundaryCrossingRules.shrinkToFit();
    m_shadowDistributedRules.shrinkToFit();
}

void MinimalRuleData::trace(Visitor* visitor)
{
    visitor->trace(m_rule);
}

void RuleData::trace(Visitor* visitor)
{
    visitor->trace(m_rule);
}

void RuleSet::PendingRuleMaps::trace(Visitor* visitor)
{
    visitor->trace(idRules);
    visitor->trace(classRules);
    visitor->trace(tagRules);
    visitor->trace(shadowPseudoElementRules);
}

void RuleSet::trace(Visitor* visitor)
{
#if ENABLE(OILPAN)
    visitor->trace(m_idRules);
    visitor->trace(m_classRules);
    visitor->trace(m_tagRules);
    visitor->trace(m_shadowPseudoElementRules);
    visitor->trace(m_linkPseudoClassRules);
    visitor->trace(m_cuePseudoRules);
    visitor->trace(m_focusPseudoClassRules);
    visitor->trace(m_universalRules);
    visitor->trace(m_pageRules);
    visitor->trace(m_viewportRules);
    visitor->trace(m_fontFaceRules);
    visitor->trace(m_keyframesRules);
    visitor->trace(m_treeBoundaryCrossingRules);
    visitor->trace(m_shadowDistributedRules);
    visitor->trace(m_viewportDependentMediaQueryResults);
    visitor->trace(m_pendingRules);
#ifndef NDEBUG
    visitor->trace(m_allRules);
#endif
#endif
}

#ifndef NDEBUG
void RuleSet::show()
{
    for (WillBeHeapVector<RuleData>::const_iterator it = m_allRules.begin(); it != m_allRules.end(); ++it)
        it->selector().show();
}
#endif

} // namespace WebCore
