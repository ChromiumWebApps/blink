/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 2004-2005 Allan Sandfeld Jensen (kde@carewolf.com)
 * Copyright (C) 2006, 2007 Nicholas Shanks (webkit@nickshanks.com)
 * Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010, 2011, 2012, 2013 Apple Inc. All rights reserved.
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
#include "core/css/PageRuleCollector.h"

#include "core/css/StylePropertySet.h"
#include "core/css/StyleRule.h"
#include "core/css/resolver/StyleResolverState.h"

namespace WebCore {

static inline bool comparePageRules(const StyleRulePage* r1, const StyleRulePage* r2)
{
    return r1->selector()->specificity() < r2->selector()->specificity();
}

bool PageRuleCollector::isLeftPage(const RenderStyle* rootElementStyle, int pageIndex) const
{
    bool isFirstPageLeft = false;
    ASSERT(rootElementStyle);
    if (!rootElementStyle->isLeftToRightDirection())
        isFirstPageLeft = true;

    return (pageIndex + (isFirstPageLeft ? 1 : 0)) % 2;
}

bool PageRuleCollector::isFirstPage(int pageIndex) const
{
    // FIXME: In case of forced left/right page, page at index 1 (not 0) can be the first page.
    return (!pageIndex);
}

String PageRuleCollector::pageName(int /* pageIndex */) const
{
    // FIXME: Implement page index to page name mapping.
    return "";
}

PageRuleCollector::PageRuleCollector(const RenderStyle* rootElementStyle, int pageIndex)
    : m_isLeftPage(isLeftPage(rootElementStyle, pageIndex))
    , m_isFirstPage(isFirstPage(pageIndex))
    , m_pageName(pageName(pageIndex)) { }

void PageRuleCollector::matchPageRules(RuleSet* rules)
{
    if (!rules)
        return;

    rules->compactRulesIfNeeded();
    WillBeHeapVector<RawPtrWillBeMember<StyleRulePage> > matchedPageRules;
    matchPageRulesForList(matchedPageRules, rules->pageRules(), m_isLeftPage, m_isFirstPage, m_pageName);
    if (matchedPageRules.isEmpty())
        return;

    std::stable_sort(matchedPageRules.begin(), matchedPageRules.end(), comparePageRules);

    for (unsigned i = 0; i < matchedPageRules.size(); i++)
        m_result.addMatchedProperties(&matchedPageRules[i]->properties());
}

static bool checkPageSelectorComponents(const CSSSelector* selector, bool isLeftPage, bool isFirstPage, const String& pageName)
{
    for (const CSSSelector* component = selector; component; component = component->tagHistory()) {
        if (component->m_match == CSSSelector::Tag) {
            const AtomicString& localName = component->tagQName().localName();
            if (localName != starAtom && localName != pageName)
                return false;
        }

        CSSSelector::PseudoType pseudoType = component->pseudoType();
        if ((pseudoType == CSSSelector::PseudoLeftPage && !isLeftPage)
            || (pseudoType == CSSSelector::PseudoRightPage && isLeftPage)
            || (pseudoType == CSSSelector::PseudoFirstPage && !isFirstPage))
        {
            return false;
        }
    }
    return true;
}

void PageRuleCollector::matchPageRulesForList(WillBeHeapVector<RawPtrWillBeMember<StyleRulePage> >& matchedRules, const WillBeHeapVector<RawPtrWillBeMember<StyleRulePage> >& rules, bool isLeftPage, bool isFirstPage, const String& pageName)
{
    for (unsigned i = 0; i < rules.size(); ++i) {
        StyleRulePage* rule = rules[i];

        if (!checkPageSelectorComponents(rule->selector(), isLeftPage, isFirstPage, pageName))
            continue;

        // If the rule has no properties to apply, then ignore it.
        const StylePropertySet& properties = rule->properties();
        if (properties.isEmpty())
            continue;

        // Add this rule to our list of matched rules.
        matchedRules.append(rule);
    }
}

} // namespace WebCore
