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
#include "core/css/RuleFeature.h"

#include "HTMLNames.h"
#include "RuntimeEnabledFeatures.h"
#include "core/css/CSSSelector.h"
#include "core/css/CSSSelectorList.h"
#include "core/css/RuleSet.h"
#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/dom/ElementTraversal.h"
#include "core/dom/Node.h"
#include "core/dom/NodeRenderStyle.h"
#include "core/dom/shadow/ElementShadow.h"
#include "core/dom/shadow/ShadowRoot.h"
#include "core/rendering/RenderObject.h"
#include "wtf/BitVector.h"

namespace WebCore {

static bool isSkippableComponentForInvalidation(const CSSSelector& selector)
{
    if (selector.m_match == CSSSelector::Tag
        || selector.m_match == CSSSelector::Id
        || selector.isAttributeSelector())
        return true;
    if (selector.m_match == CSSSelector::PseudoElement) {
        switch (selector.m_pseudoType) {
        case CSSSelector::PseudoBefore:
        case CSSSelector::PseudoAfter:
        case CSSSelector::PseudoBackdrop:
            return true;
        default:
            return false;
        }
    }
    if (selector.m_match != CSSSelector::PseudoClass)
        return false;
    switch (selector.pseudoType()) {
    case CSSSelector::PseudoEmpty:
    case CSSSelector::PseudoFirstChild:
    case CSSSelector::PseudoFirstOfType:
    case CSSSelector::PseudoLastChild:
    case CSSSelector::PseudoLastOfType:
    case CSSSelector::PseudoOnlyChild:
    case CSSSelector::PseudoOnlyOfType:
    case CSSSelector::PseudoNthChild:
    case CSSSelector::PseudoNthOfType:
    case CSSSelector::PseudoNthLastChild:
    case CSSSelector::PseudoNthLastOfType:
    case CSSSelector::PseudoLink:
    case CSSSelector::PseudoVisited:
    case CSSSelector::PseudoAnyLink:
    case CSSSelector::PseudoHover:
    case CSSSelector::PseudoDrag:
    case CSSSelector::PseudoFocus:
    case CSSSelector::PseudoActive:
    case CSSSelector::PseudoChecked:
    case CSSSelector::PseudoEnabled:
    case CSSSelector::PseudoDefault:
    case CSSSelector::PseudoDisabled:
    case CSSSelector::PseudoOptional:
    case CSSSelector::PseudoRequired:
    case CSSSelector::PseudoReadOnly:
    case CSSSelector::PseudoReadWrite:
    case CSSSelector::PseudoValid:
    case CSSSelector::PseudoInvalid:
    case CSSSelector::PseudoIndeterminate:
    case CSSSelector::PseudoTarget:
    case CSSSelector::PseudoLang:
    case CSSSelector::PseudoRoot:
    case CSSSelector::PseudoScope:
    case CSSSelector::PseudoInRange:
    case CSSSelector::PseudoOutOfRange:
    case CSSSelector::PseudoUnresolved:
        return true;
    default:
        return false;
    }
    ASSERT_NOT_REACHED();
    return false;
}

// This method is somewhat conservative in what it accepts.
RuleFeatureSet::InvalidationSetMode RuleFeatureSet::supportsClassDescendantInvalidation(const CSSSelector& selector)
{
    bool foundDescendantRelation = false;
    bool foundIdent = false;
    for (const CSSSelector* component = &selector; component; component = component->tagHistory()) {

        // FIXME: We should allow pseudo elements, but we need to change how they hook
        // into recalcStyle by moving them to recalcOwnStyle instead of recalcChildStyle.

        // FIXME: next up: Tag and Id.
        if (component->m_match == CSSSelector::Class) {
            if (!foundDescendantRelation)
                foundIdent = true;
        } else if (!isSkippableComponentForInvalidation(*component)) {
            return foundDescendantRelation ? UseLocalStyleChange : UseSubtreeStyleChange;
        }
        // FIXME: We can probably support ShadowAll and ShadowDeep.
        switch (component->relation()) {
        case CSSSelector::Descendant:
        case CSSSelector::Child:
            foundDescendantRelation = true;
            // Fall through!
        case CSSSelector::SubSelector:
            continue;
        default:
            return UseLocalStyleChange;
        }
    }
    return foundIdent ? AddFeatures : UseLocalStyleChange;
}

void extractClassIdOrTag(const CSSSelector& selector, Vector<AtomicString>& classes, AtomicString& id, AtomicString& tagName)
{
    if (selector.m_match == CSSSelector::Tag)
        tagName = selector.tagQName().localName();
    else if (selector.m_match == CSSSelector::Id)
        id = selector.value();
    else if (selector.m_match == CSSSelector::Class)
        classes.append(selector.value());
}

RuleFeatureSet::RuleFeatureSet()
    : m_targetedStyleRecalcEnabled(RuntimeEnabledFeatures::targetedStyleRecalcEnabled())
{
}

RuleFeatureSet::InvalidationSetMode RuleFeatureSet::updateClassInvalidationSets(const CSSSelector& selector)
{
    InvalidationSetMode mode = supportsClassDescendantInvalidation(selector);
    if (mode != AddFeatures)
        return mode;

    Vector<AtomicString> classes;
    AtomicString id;
    AtomicString tagName;

    const CSSSelector* lastSelector = &selector;
    for (; lastSelector; lastSelector = lastSelector->tagHistory()) {
        extractClassIdOrTag(*lastSelector, classes, id, tagName);
        if (lastSelector->m_match == CSSSelector::Class)
            ensureClassInvalidationSet(lastSelector->value());
        if (lastSelector->relation() != CSSSelector::SubSelector)
            break;
    }

    if (!lastSelector)
        return AddFeatures;

    for (const CSSSelector* current = lastSelector->tagHistory(); current; current = current->tagHistory()) {
        if (current->m_match == CSSSelector::Class) {
            DescendantInvalidationSet& invalidationSet = ensureClassInvalidationSet(current->value());
            if (!id.isEmpty())
                invalidationSet.addId(id);
            if (!tagName.isEmpty())
                invalidationSet.addTagName(tagName);
            for (Vector<AtomicString>::const_iterator it = classes.begin(); it != classes.end(); ++it) {
                invalidationSet.addClass(*it);
            }
        }
    }
    return AddFeatures;
}

void RuleFeatureSet::addAttributeInASelector(const AtomicString& attributeName)
{
    m_metadata.attrsInRules.add(attributeName);
}

void RuleFeatureSet::collectFeaturesFromRuleData(const RuleData& ruleData)
{
    FeatureMetadata metadata;
    InvalidationSetMode mode = UseSubtreeStyleChange;
    if (m_targetedStyleRecalcEnabled)
        mode = updateClassInvalidationSets(ruleData.selector());

    collectFeaturesFromSelector(ruleData.selector(), metadata, mode);
    m_metadata.add(metadata);

    if (metadata.foundSiblingSelector)
        siblingRules.append(RuleFeature(ruleData.rule(), ruleData.selectorIndex(), ruleData.hasDocumentSecurityOrigin()));
    if (ruleData.containsUncommonAttributeSelector())
        uncommonAttributeRules.append(RuleFeature(ruleData.rule(), ruleData.selectorIndex(), ruleData.hasDocumentSecurityOrigin()));
}

DescendantInvalidationSet& RuleFeatureSet::ensureClassInvalidationSet(const AtomicString& className)
{
    InvalidationSetMap::AddResult addResult = m_classInvalidationSets.add(className, nullptr);
    if (addResult.isNewEntry)
        addResult.storedValue->value = DescendantInvalidationSet::create();
    return *addResult.storedValue->value;
}

void RuleFeatureSet::collectFeaturesFromSelector(const CSSSelector& selector)
{
    collectFeaturesFromSelector(selector, m_metadata, UseSubtreeStyleChange);
}

void RuleFeatureSet::collectFeaturesFromSelector(const CSSSelector& selector, RuleFeatureSet::FeatureMetadata& metadata, InvalidationSetMode mode)
{
    unsigned maxDirectAdjacentSelectors = 0;

    for (const CSSSelector* current = &selector; current; current = current->tagHistory()) {
        if (current->m_match == CSSSelector::Id) {
            metadata.idsInRules.add(current->value());
        } else if (current->m_match == CSSSelector::Class && mode != AddFeatures) {
            DescendantInvalidationSet& invalidationSet = ensureClassInvalidationSet(current->value());
            if (mode == UseSubtreeStyleChange)
                invalidationSet.setWholeSubtreeInvalid();
        } else if (current->isAttributeSelector()) {
            metadata.attrsInRules.add(current->attribute().localName());
        }
        if (current->pseudoType() == CSSSelector::PseudoFirstLine)
            metadata.usesFirstLineRules = true;
        if (current->isDirectAdjacentSelector()) {
            maxDirectAdjacentSelectors++;
        } else if (maxDirectAdjacentSelectors) {
            if (maxDirectAdjacentSelectors > metadata.maxDirectAdjacentSelectors)
                metadata.maxDirectAdjacentSelectors = maxDirectAdjacentSelectors;
            maxDirectAdjacentSelectors = 0;
        }
        if (current->isSiblingSelector())
            metadata.foundSiblingSelector = true;

        collectFeaturesFromSelectorList(current->selectorList(), metadata, mode);

        if (mode == UseLocalStyleChange && current->relation() != CSSSelector::SubSelector)
            mode = UseSubtreeStyleChange;
    }

    ASSERT(!maxDirectAdjacentSelectors);
}

void RuleFeatureSet::collectFeaturesFromSelectorList(const CSSSelectorList* selectorList, RuleFeatureSet::FeatureMetadata& metadata, InvalidationSetMode mode)
{
    if (!selectorList)
        return;

    for (const CSSSelector* selector = selectorList->first(); selector; selector = CSSSelectorList::next(*selector))
        collectFeaturesFromSelector(*selector, metadata, mode);
}

void RuleFeatureSet::FeatureMetadata::add(const FeatureMetadata& other)
{
    usesFirstLineRules = usesFirstLineRules || other.usesFirstLineRules;
    maxDirectAdjacentSelectors = std::max(maxDirectAdjacentSelectors, other.maxDirectAdjacentSelectors);

    HashSet<AtomicString>::const_iterator end = other.idsInRules.end();
    for (HashSet<AtomicString>::const_iterator it = other.idsInRules.begin(); it != end; ++it)
        idsInRules.add(*it);
    end = other.attrsInRules.end();
    for (HashSet<AtomicString>::const_iterator it = other.attrsInRules.begin(); it != end; ++it)
        attrsInRules.add(*it);
}

void RuleFeatureSet::FeatureMetadata::clear()
{
    idsInRules.clear();
    attrsInRules.clear();
    usesFirstLineRules = false;
    foundSiblingSelector = false;
    maxDirectAdjacentSelectors = 0;
}

void RuleFeatureSet::add(const RuleFeatureSet& other)
{
    for (InvalidationSetMap::const_iterator it = other.m_classInvalidationSets.begin(); it != other.m_classInvalidationSets.end(); ++it) {
        ensureClassInvalidationSet(it->key).combine(*it->value);
    }

    m_metadata.add(other.m_metadata);

    siblingRules.appendVector(other.siblingRules);
    uncommonAttributeRules.appendVector(other.uncommonAttributeRules);
}

void RuleFeatureSet::clear()
{
    siblingRules.clear();
    uncommonAttributeRules.clear();
    m_metadata.clear();
    m_classInvalidationSets.clear();
    m_pendingInvalidationMap.clear();
}

void RuleFeatureSet::scheduleStyleInvalidationForClassChange(const SpaceSplitString& changedClasses, Element* element)
{
    unsigned changedSize = changedClasses.size();
    for (unsigned i = 0; i < changedSize; ++i) {
        addClassToInvalidationSet(changedClasses[i], element);
    }
}

void RuleFeatureSet::scheduleStyleInvalidationForClassChange(const SpaceSplitString& oldClasses, const SpaceSplitString& newClasses, Element* element)
{
    if (!oldClasses.size())
        scheduleStyleInvalidationForClassChange(newClasses, element);

    // Class vectors tend to be very short. This is faster than using a hash table.
    BitVector remainingClassBits;
    remainingClassBits.ensureSize(oldClasses.size());

    for (unsigned i = 0; i < newClasses.size(); ++i) {
        bool found = false;
        for (unsigned j = 0; j < oldClasses.size(); ++j) {
            if (newClasses[i] == oldClasses[j]) {
                // Mark each class that is still in the newClasses so we can skip doing
                // an n^2 search below when looking for removals. We can't break from
                // this loop early since a class can appear more than once.
                remainingClassBits.quickSet(j);
                found = true;
            }
        }
        // Class was added.
        if (!found)
            addClassToInvalidationSet(newClasses[i], element);
    }

    for (unsigned i = 0; i < oldClasses.size(); ++i) {
        if (remainingClassBits.quickGet(i))
            continue;
        // Class was removed.
        addClassToInvalidationSet(oldClasses[i], element);
    }
}

void RuleFeatureSet::addClassToInvalidationSet(const AtomicString& className, Element* element)
{
    if (RefPtr<DescendantInvalidationSet> invalidationSet = m_classInvalidationSets.get(className)) {
        ensurePendingInvalidationList(element).append(invalidationSet);
        element->setNeedsStyleInvalidation();
    }
}

RuleFeatureSet::InvalidationList& RuleFeatureSet::ensurePendingInvalidationList(Element* element)
{
    PendingInvalidationMap::AddResult addResult = m_pendingInvalidationMap.add(element, nullptr);
    if (addResult.isNewEntry)
        addResult.storedValue->value = adoptPtr(new InvalidationList);
    return *addResult.storedValue->value;
}

void RuleFeatureSet::computeStyleInvalidation(Document& document)
{
    Vector<AtomicString> invalidationClasses;
    if (Element* documentElement = document.documentElement()) {
        if (documentElement->childNeedsStyleInvalidation())
            invalidateStyleForClassChange(documentElement, invalidationClasses, false);
    }
    document.clearChildNeedsStyleInvalidation();
    document.clearNeedsStyleInvalidation();
    m_pendingInvalidationMap.clear();
}

void RuleFeatureSet::clearStyleInvalidation(Node* node)
{
    node->clearChildNeedsStyleInvalidation();
    node->clearNeedsStyleInvalidation();
    if (node->isElementNode())
        m_pendingInvalidationMap.remove(toElement(node));
}

bool RuleFeatureSet::invalidateStyleForClassChangeOnChildren(Element* element, Vector<AtomicString>& invalidationClasses, bool foundInvalidationSet)
{
    bool someChildrenNeedStyleRecalc = false;
    for (ShadowRoot* root = element->youngestShadowRoot(); root; root = root->olderShadowRoot()) {
        for (Element* child = ElementTraversal::firstWithin(*root); child; child = ElementTraversal::nextSibling(*child)) {
            bool childRecalced = invalidateStyleForClassChange(child, invalidationClasses, foundInvalidationSet);
            someChildrenNeedStyleRecalc = someChildrenNeedStyleRecalc || childRecalced;
        }
        root->clearChildNeedsStyleInvalidation();
        root->clearNeedsStyleInvalidation();
    }
    for (Element* child = ElementTraversal::firstWithin(*element); child; child = ElementTraversal::nextSibling(*child)) {
        bool childRecalced = invalidateStyleForClassChange(child, invalidationClasses, foundInvalidationSet);
        someChildrenNeedStyleRecalc = someChildrenNeedStyleRecalc || childRecalced;
    }
    return someChildrenNeedStyleRecalc;
}

bool RuleFeatureSet::invalidateStyleForClassChange(Element* element, Vector<AtomicString>& invalidationClasses, bool foundInvalidationSet)
{
    bool thisElementNeedsStyleRecalc = false;
    int oldSize = invalidationClasses.size();
    if (element->needsStyleInvalidation()) {
        if (InvalidationList* invalidationList = m_pendingInvalidationMap.get(element)) {
            // FIXME: it's really only necessary to clone the render style for this element, not full style recalc.
            thisElementNeedsStyleRecalc = true;
            foundInvalidationSet = true;
            for (InvalidationList::const_iterator it = invalidationList->begin(); it != invalidationList->end(); ++it) {
                if ((*it)->wholeSubtreeInvalid()) {
                    element->setNeedsStyleRecalc(SubtreeStyleChange);
                    // Even though we have set needsStyleRecalc on the whole subtree, we need to keep walking over the subtree
                    // in order to clear the invalidation dirty bits on all elements.
                    // FIXME: we can optimize this by having a dedicated function that just traverses the tree and removes the dirty bits,
                    // without checking classes etc.
                    break;
                }
                (*it)->getClasses(invalidationClasses);
            }
        }
    }

    if (element->hasClass()) {
        const SpaceSplitString& classNames = element->classNames();
        for (Vector<AtomicString>::const_iterator it = invalidationClasses.begin(); it != invalidationClasses.end(); ++it) {
            if (classNames.contains(*it)) {
                thisElementNeedsStyleRecalc = true;
                break;
            }
        }
    }

    bool someChildrenNeedStyleRecalc = false;
    // foundInvalidationSet will be true if we are in a subtree of a node with a DescendantInvalidationSet on it.
    // We need to check all nodes in the subtree of such a node.
    if (foundInvalidationSet || element->childNeedsStyleInvalidation()) {
        someChildrenNeedStyleRecalc = invalidateStyleForClassChangeOnChildren(element, invalidationClasses, foundInvalidationSet);
    }

    if (thisElementNeedsStyleRecalc) {
        element->setNeedsStyleRecalc(LocalStyleChange);
    } else if (foundInvalidationSet && someChildrenNeedStyleRecalc) {
        // Clone the RenderStyle in order to preserve correct style sharing, if possible. Otherwise recalc style.
        if (RenderObject* renderer = element->renderer()) {
            ASSERT(renderer->style());
            renderer->setStyleInternal(RenderStyle::clone(renderer->style()));
        } else {
            element->setNeedsStyleRecalc(LocalStyleChange);
        }
    }

    invalidationClasses.remove(oldSize, invalidationClasses.size() - oldSize);
    element->clearChildNeedsStyleInvalidation();
    element->clearNeedsStyleInvalidation();

    return thisElementNeedsStyleRecalc;
}

} // namespace WebCore
