/*
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * (C) 2002-2003 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2002, 2006, 2008, 2012, 2013 Apple Inc. All rights reserved.
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

#ifndef StyleRule_h
#define StyleRule_h

#include "core/css/CSSSelectorList.h"
#include "core/css/MediaList.h"
#include "heap/Handle.h"
#include "wtf/RefPtr.h"

namespace WebCore {

class CSSRule;
class CSSStyleRule;
class CSSStyleSheet;
class MutableStylePropertySet;
class StylePropertySet;

class StyleRuleBase : public RefCountedWillBeRefCountedGarbageCollected<StyleRuleBase> {
    DECLARE_GC_INFO;
    WTF_MAKE_FAST_ALLOCATED_WILL_BE_REMOVED;
public:
    enum Type {
        Unknown, // Not used.
        Style,
        Charset, // Not used. These are internally strings owned by the style sheet.
        Import,
        Media,
        FontFace,
        Page,
        Keyframes,
        Keyframe, // Not used. These are internally non-rule StyleKeyframe objects.
        Supports = 12,
        Viewport = 15,
        Filter = 17
    };

    Type type() const { return static_cast<Type>(m_type); }

    bool isCharsetRule() const { return type() == Charset; }
    bool isFontFaceRule() const { return type() == FontFace; }
    bool isKeyframesRule() const { return type() == Keyframes; }
    bool isMediaRule() const { return type() == Media; }
    bool isPageRule() const { return type() == Page; }
    bool isStyleRule() const { return type() == Style; }
    bool isSupportsRule() const { return type() == Supports; }
    bool isViewportRule() const { return type() == Viewport; }
    bool isImportRule() const { return type() == Import; }
    bool isFilterRule() const { return type() == Filter; }

    PassRefPtr<StyleRuleBase> copy() const;

#if !ENABLE(OILPAN)
    void deref()
    {
        if (derefBase())
            destroy();
    }
#endif // !ENABLE(OILPAN)

    // FIXME: There shouldn't be any need for the null parent version.
    PassRefPtr<CSSRule> createCSSOMWrapper(CSSStyleSheet* parentSheet = 0) const;
    PassRefPtr<CSSRule> createCSSOMWrapper(CSSRule* parentRule) const;

    void trace(Visitor*);
    void traceAfterDispatch(Visitor*) { };
    void finalize();

protected:
    StyleRuleBase(Type type) : m_type(type) { }
    StyleRuleBase(const StyleRuleBase& o) : m_type(o.m_type) { }

    ~StyleRuleBase() { }

private:
    void destroy();

    PassRefPtr<CSSRule> createCSSOMWrapper(CSSStyleSheet* parentSheet, CSSRule* parentRule) const;

    unsigned m_type : 5;
};

class StyleRule : public StyleRuleBase {
    WTF_MAKE_FAST_ALLOCATED_WILL_BE_REMOVED;
public:
    static PassRefPtrWillBeRawPtr<StyleRule> create() { return adoptRefCountedWillBeRefCountedGarbageCollected(new StyleRule()); }

    ~StyleRule();

    const CSSSelectorList& selectorList() const { return m_selectorList; }
    const StylePropertySet* properties() const { return m_properties.get(); }
    MutableStylePropertySet* mutableProperties();

    void parserAdoptSelectorVector(Vector<OwnPtr<CSSParserSelector> >& selectors) { m_selectorList.adoptSelectorVector(selectors); }
    void wrapperAdoptSelectorList(CSSSelectorList& selectors) { m_selectorList.adopt(selectors); }
    void setProperties(PassRefPtr<StylePropertySet>);

    PassRefPtr<StyleRule> copy() const { return adoptRefCountedWillBeRefCountedGarbageCollected(new StyleRule(*this)); }

    static unsigned averageSizeInBytes();

    void traceAfterDispatch(Visitor* visitor) { StyleRuleBase::traceAfterDispatch(visitor); }

private:
    StyleRule();
    StyleRule(const StyleRule&);

    RefPtr<StylePropertySet> m_properties;
    CSSSelectorList m_selectorList;
};

class StyleRuleFontFace : public StyleRuleBase {
public:
    static PassRefPtrWillBeRawPtr<StyleRuleFontFace> create() { return adoptRefCountedWillBeRefCountedGarbageCollected(new StyleRuleFontFace); }

    ~StyleRuleFontFace();

    const StylePropertySet* properties() const { return m_properties.get(); }
    MutableStylePropertySet* mutableProperties();

    void setProperties(PassRefPtr<StylePropertySet>);

    PassRefPtr<StyleRuleFontFace> copy() const { return adoptRefCountedWillBeRefCountedGarbageCollected(new StyleRuleFontFace(*this)); }

    void traceAfterDispatch(Visitor* visitor) { StyleRuleBase::traceAfterDispatch(visitor); }

private:
    StyleRuleFontFace();
    StyleRuleFontFace(const StyleRuleFontFace&);

    RefPtr<StylePropertySet> m_properties;
};

class StyleRulePage : public StyleRuleBase {
public:
    static PassRefPtrWillBeRawPtr<StyleRulePage> create() { return adoptRefCountedWillBeRefCountedGarbageCollected(new StyleRulePage); }

    ~StyleRulePage();

    const CSSSelector* selector() const { return m_selectorList.first(); }
    const StylePropertySet* properties() const { return m_properties.get(); }
    MutableStylePropertySet* mutableProperties();

    void parserAdoptSelectorVector(Vector<OwnPtr<CSSParserSelector> >& selectors) { m_selectorList.adoptSelectorVector(selectors); }
    void wrapperAdoptSelectorList(CSSSelectorList& selectors) { m_selectorList.adopt(selectors); }
    void setProperties(PassRefPtr<StylePropertySet>);

    PassRefPtr<StyleRulePage> copy() const { return adoptRefCountedWillBeRefCountedGarbageCollected(new StyleRulePage(*this)); }

    void traceAfterDispatch(Visitor* visitor) { StyleRuleBase::traceAfterDispatch(visitor); }

private:
    StyleRulePage();
    StyleRulePage(const StyleRulePage&);

    RefPtr<StylePropertySet> m_properties;
    CSSSelectorList m_selectorList;
};

class StyleRuleGroup : public StyleRuleBase {
public:
    const Vector<RefPtr<StyleRuleBase> >& childRules() const { return m_childRules; }

    void wrapperInsertRule(unsigned, PassRefPtr<StyleRuleBase>);
    void wrapperRemoveRule(unsigned);

    void traceAfterDispatch(Visitor* visitor) { StyleRuleBase::traceAfterDispatch(visitor); }

protected:
    StyleRuleGroup(Type, Vector<RefPtr<StyleRuleBase> >& adoptRule);
    StyleRuleGroup(const StyleRuleGroup&);

private:
    Vector<RefPtr<StyleRuleBase> > m_childRules;
};

class StyleRuleMedia : public StyleRuleGroup {
public:
    static PassRefPtrWillBeRawPtr<StyleRuleMedia> create(PassRefPtr<MediaQuerySet> media, Vector<RefPtr<StyleRuleBase> >& adoptRules)
    {
        return adoptRefCountedWillBeRefCountedGarbageCollected(new StyleRuleMedia(media, adoptRules));
    }

    MediaQuerySet* mediaQueries() const { return m_mediaQueries.get(); }

    PassRefPtr<StyleRuleMedia> copy() const { return adoptRefCountedWillBeRefCountedGarbageCollected(new StyleRuleMedia(*this)); }

    void traceAfterDispatch(Visitor* visitor) { StyleRuleGroup::traceAfterDispatch(visitor); }

private:
    StyleRuleMedia(PassRefPtr<MediaQuerySet>, Vector<RefPtr<StyleRuleBase> >& adoptRules);
    StyleRuleMedia(const StyleRuleMedia&);

    RefPtr<MediaQuerySet> m_mediaQueries;
};

class StyleRuleSupports : public StyleRuleGroup {
public:
    static PassRefPtrWillBeRawPtr<StyleRuleSupports> create(const String& conditionText, bool conditionIsSupported, Vector<RefPtr<StyleRuleBase> >& adoptRules)
    {
        return adoptRefCountedWillBeRefCountedGarbageCollected(new StyleRuleSupports(conditionText, conditionIsSupported, adoptRules));
    }

    String conditionText() const { return m_conditionText; }
    bool conditionIsSupported() const { return m_conditionIsSupported; }
    PassRefPtr<StyleRuleSupports> copy() const { return adoptRefCountedWillBeRefCountedGarbageCollected(new StyleRuleSupports(*this)); }

    void traceAfterDispatch(Visitor* visitor) { StyleRuleGroup::traceAfterDispatch(visitor); }

private:
    StyleRuleSupports(const String& conditionText, bool conditionIsSupported, Vector<RefPtr<StyleRuleBase> >& adoptRules);
    StyleRuleSupports(const StyleRuleSupports&);

    String m_conditionText;
    bool m_conditionIsSupported;
};

class StyleRuleViewport : public StyleRuleBase {
public:
    static PassRefPtrWillBeRawPtr<StyleRuleViewport> create() { return adoptRefCountedWillBeRefCountedGarbageCollected(new StyleRuleViewport); }

    ~StyleRuleViewport();

    const StylePropertySet* properties() const { return m_properties.get(); }
    MutableStylePropertySet* mutableProperties();

    void setProperties(PassRefPtr<StylePropertySet>);

    PassRefPtr<StyleRuleViewport> copy() const { return adoptRefCountedWillBeRefCountedGarbageCollected(new StyleRuleViewport(*this)); }

    void traceAfterDispatch(Visitor* visitor) { StyleRuleBase::traceAfterDispatch(visitor); }

private:
    StyleRuleViewport();
    StyleRuleViewport(const StyleRuleViewport&);

    RefPtr<StylePropertySet> m_properties;
};

class StyleRuleFilter : public StyleRuleBase {
public:
    static PassRefPtrWillBeRawPtr<StyleRuleFilter> create(const String& filterName) { return adoptRefCountedWillBeRefCountedGarbageCollected(new StyleRuleFilter(filterName)); }

    ~StyleRuleFilter();

    const String& filterName() const { return m_filterName; }

    const StylePropertySet* properties() const { return m_properties.get(); }
    MutableStylePropertySet* mutableProperties();

    void setProperties(PassRefPtr<StylePropertySet>);

    PassRefPtr<StyleRuleFilter> copy() const { return adoptRefCountedWillBeRefCountedGarbageCollected(new StyleRuleFilter(*this)); }

    void traceAfterDispatch(Visitor* visitor) { StyleRuleBase::traceAfterDispatch(visitor); }

private:
    StyleRuleFilter(const String&);
    StyleRuleFilter(const StyleRuleFilter&);

    String m_filterName;
    RefPtr<StylePropertySet> m_properties;
};

#define DEFINE_STYLE_RULE_TYPE_CASTS(Type) \
    DEFINE_TYPE_CASTS(StyleRule##Type, StyleRuleBase, rule, rule->is##Type##Rule(), rule.is##Type##Rule())

DEFINE_TYPE_CASTS(StyleRule, StyleRuleBase, rule, rule->isStyleRule(), rule.isStyleRule());
DEFINE_STYLE_RULE_TYPE_CASTS(FontFace);
DEFINE_STYLE_RULE_TYPE_CASTS(Page);
DEFINE_STYLE_RULE_TYPE_CASTS(Media);
DEFINE_STYLE_RULE_TYPE_CASTS(Supports);
DEFINE_STYLE_RULE_TYPE_CASTS(Viewport);
DEFINE_STYLE_RULE_TYPE_CASTS(Filter);

} // namespace WebCore

#endif // StyleRule_h
