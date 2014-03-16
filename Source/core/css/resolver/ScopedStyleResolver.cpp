/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "core/css/resolver/ScopedStyleResolver.h"

#include "HTMLNames.h"
#include "core/css/CSSStyleSheet.h"
#include "core/css/PageRuleCollector.h"
#include "core/css/RuleFeature.h"
#include "core/css/StyleRule.h"
#include "core/css/StyleSheetContents.h"
#include "core/css/resolver/StyleResolver.h" // For MatchRequest.
#include "core/css/resolver/ViewportStyleResolver.h"
#include "core/dom/Document.h"
#include "core/dom/shadow/ElementShadow.h"
#include "core/dom/shadow/ShadowRoot.h"
#include "core/html/HTMLStyleElement.h"

namespace WebCore {

ContainerNode* ScopedStyleResolver::scopingNodeFor(Document& document, const CSSStyleSheet* sheet)
{
    ASSERT(sheet);

    Document* sheetDocument = sheet->ownerDocument();
    if (!sheetDocument)
        return 0;
    Node* ownerNode = sheet->ownerNode();
    if (!isHTMLStyleElement(ownerNode))
        return &document;

    HTMLStyleElement& styleElement = toHTMLStyleElement(*ownerNode);
    if (!styleElement.scoped()) {
        if (styleElement.isInShadowTree())
            return styleElement.containingShadowRoot();
        return &document;
    }

    ContainerNode* parent = styleElement.parentNode();
    if (!parent)
        return 0;

    return (parent->isElementNode() || parent->isShadowRoot()) ? parent : 0;
}

void ScopedStyleResolver::addRulesFromSheet(CSSStyleSheet* cssSheet, const MediaQueryEvaluator& medium, StyleResolver* resolver)
{
    m_authorStyleSheets.append(cssSheet);
    StyleSheetContents* sheet = cssSheet->contents();

    AddRuleFlags addRuleFlags = resolver->document().securityOrigin()->canRequest(sheet->baseURL()) ? RuleHasDocumentSecurityOrigin : RuleHasNoSpecialState;
    const RuleSet& ruleSet = sheet->ensureRuleSet(medium, addRuleFlags);
    resolver->addMediaQueryResults(ruleSet.viewportDependentMediaQueryResults());
    resolver->processScopedRules(ruleSet, sheet->baseURL(), &m_scopingNode);
}

void ScopedStyleResolver::collectFeaturesTo(RuleFeatureSet& features, HashSet<const StyleSheetContents*>& visitedSharedStyleSheetContents)
{
    for (size_t i = 0; i < m_authorStyleSheets.size(); ++i) {
        StyleSheetContents* contents = m_authorStyleSheets[i]->contents();
        if (contents->hasOneClient() || visitedSharedStyleSheetContents.add(contents).isNewEntry)
            features.add(contents->ruleSet().features());
    }
}

void ScopedStyleResolver::resetAuthorStyle()
{
    m_authorStyleSheets.clear();
    m_keyframesRuleMap.clear();
}

const StyleRuleKeyframes* ScopedStyleResolver::keyframeStylesForAnimation(const StringImpl* animationName)
{
    if (m_keyframesRuleMap.isEmpty())
        return 0;

    KeyframesRuleMap::iterator it = m_keyframesRuleMap.find(animationName);
    if (it == m_keyframesRuleMap.end())
        return 0;

    return it->value.get();
}

void ScopedStyleResolver::addKeyframeStyle(PassRefPtrWillBeRawPtr<StyleRuleKeyframes> rule)
{
    AtomicString s(rule->name());
    if (rule->isVendorPrefixed()) {
        KeyframesRuleMap::iterator it = m_keyframesRuleMap.find(rule->name().impl());
        if (it == m_keyframesRuleMap.end())
            m_keyframesRuleMap.set(s.impl(), rule);
        else if (it->value->isVendorPrefixed())
            m_keyframesRuleMap.set(s.impl(), rule);
    } else {
        m_keyframesRuleMap.set(s.impl(), rule);
    }
}

void ScopedStyleResolver::collectMatchingAuthorRules(ElementRuleCollector& collector, bool includeEmptyRules, bool applyAuthorStyles, CascadeScope cascadeScope, CascadeOrder cascadeOrder)
{
    const ContainerNode* scopingNode = &m_scopingNode;
    unsigned behaviorAtBoundary = SelectorChecker::DoesNotCrossBoundary;

    if (!applyAuthorStyles)
        behaviorAtBoundary |= SelectorChecker::ScopeContainsLastMatchedElement;

    if (m_scopingNode.isShadowRoot()) {
        scopingNode = toShadowRoot(m_scopingNode).host();
        behaviorAtBoundary |= SelectorChecker::ScopeIsShadowHost;
    }

    RuleRange ruleRange = collector.matchedResult().ranges.authorRuleRange();
    for (size_t i = 0; i < m_authorStyleSheets.size(); ++i) {
        MatchRequest matchRequest(&m_authorStyleSheets[i]->contents()->ruleSet(), includeEmptyRules, scopingNode, applyAuthorStyles, i, m_authorStyleSheets[i]);
        collector.collectMatchingRules(matchRequest, ruleRange, static_cast<SelectorChecker::BehaviorAtBoundary>(behaviorAtBoundary), cascadeScope, cascadeOrder);
    }
}

void ScopedStyleResolver::matchPageRules(PageRuleCollector& collector)
{
    // Only consider the global author RuleSet for @page rules, as per the HTML5 spec.
    ASSERT(m_scopingNode.isDocumentNode());
    for (size_t i = 0; i < m_authorStyleSheets.size(); ++i)
        collector.matchPageRules(&m_authorStyleSheets[i]->contents()->ruleSet());
}

void ScopedStyleResolver::collectViewportRulesTo(StyleResolver* resolver) const
{
    if (!m_scopingNode.isDocumentNode())
        return;
    for (size_t i = 0; i < m_authorStyleSheets.size(); ++i)
        resolver->viewportStyleResolver()->collectViewportRules(&m_authorStyleSheets[i]->contents()->ruleSet(), ViewportStyleResolver::AuthorOrigin);
}

} // namespace WebCore
