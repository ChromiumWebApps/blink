/*
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * (C) 2002-2003 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2002, 2006, 2012 Apple Computer, Inc.
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

#ifndef CSSRuleList_h
#define CSSRuleList_h

#include "heap/Handle.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefPtr.h"
#include "wtf/Vector.h"

namespace WebCore {

class CSSRule;
class CSSStyleSheet;

class CSSRuleList : public NoBaseWillBeGarbageCollectedFinalized<CSSRuleList> {
    WTF_MAKE_NONCOPYABLE(CSSRuleList);
    WTF_MAKE_FAST_ALLOCATED_WILL_BE_REMOVED;
public:
    virtual ~CSSRuleList();

#if !ENABLE(OILPAN)
    virtual void ref() = 0;
    virtual void deref() = 0;
#endif

    virtual unsigned length() const = 0;
    virtual CSSRule* item(unsigned index) const = 0;

    virtual CSSStyleSheet* styleSheet() const = 0;

    virtual void trace(Visitor*) = 0;

protected:
    CSSRuleList();
};

class StaticCSSRuleList FINAL : public CSSRuleList {
public:
    static PassRefPtrWillBeRawPtr<StaticCSSRuleList> create()
    {
        return adoptRefWillBeNoop(new StaticCSSRuleList());
    }

#if !ENABLE(OILPAN)
    virtual void ref() OVERRIDE { ++m_refCount; }
    virtual void deref() OVERRIDE;
#endif

    WillBeHeapVector<RefPtrWillBeMember<CSSRule> >& rules() { return m_rules; }

    virtual CSSStyleSheet* styleSheet() const OVERRIDE { return 0; }

    virtual void trace(Visitor*) OVERRIDE;

private:
    StaticCSSRuleList();
    virtual ~StaticCSSRuleList();

    virtual unsigned length() const OVERRIDE { return m_rules.size(); }
    virtual CSSRule* item(unsigned index) const OVERRIDE { return index < m_rules.size() ? m_rules[index].get() : 0; }

    WillBeHeapVector<RefPtrWillBeMember<CSSRule> > m_rules;
#if !ENABLE(OILPAN)
    unsigned m_refCount;
#endif
};

template <class Rule>
class LiveCSSRuleList FINAL : public CSSRuleList {
public:
    static PassOwnPtrWillBeRawPtr<LiveCSSRuleList> create(Rule* rule)
    {
        return adoptPtrWillBeNoop(new LiveCSSRuleList(rule));
    }

#if !ENABLE(OILPAN)
    virtual void ref() OVERRIDE { m_rule->ref(); }
    virtual void deref() OVERRIDE { m_rule->deref(); }
#endif

    virtual void trace(Visitor* visitor) OVERRIDE
    {
        visitor->trace(m_rule);
    }

private:
    LiveCSSRuleList(Rule* rule) : m_rule(rule) { }

    virtual unsigned length() const OVERRIDE { return m_rule->length(); }
    virtual CSSRule* item(unsigned index) const OVERRIDE { return m_rule->item(index); }
    virtual CSSStyleSheet* styleSheet() const OVERRIDE { return m_rule->parentStyleSheet(); }

    RawPtrWillBeMember<Rule> m_rule;
};

} // namespace WebCore

#endif // CSSRuleList_h
