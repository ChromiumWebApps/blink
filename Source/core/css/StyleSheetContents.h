/*
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2006, 2007, 2008, 2009, 2010, 2012 Apple Inc. All rights reserved.
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

#ifndef StyleSheetContents_h
#define StyleSheetContents_h

#include "core/css/CSSParserMode.h"
#include "core/css/RuleSet.h"
#include "heap/Handle.h"
#include "platform/weborigin/KURL.h"
#include "wtf/HashMap.h"
#include "wtf/ListHashSet.h"
#include "wtf/RefCounted.h"
#include "wtf/Vector.h"
#include "wtf/text/AtomicStringHash.h"
#include "wtf/text/StringHash.h"
#include "wtf/text/TextPosition.h"


namespace WebCore {

class CSSStyleSheet;
class CSSStyleSheetResource;
class Document;
class Node;
class SecurityOrigin;
class StyleRuleBase;
class StyleRuleFontFace;
class StyleRuleImport;

class StyleSheetContents : public RefCountedWillBeGarbageCollectedFinalized<StyleSheetContents> {
public:
    static PassRefPtrWillBeRawPtr<StyleSheetContents> create(const CSSParserContext& context)
    {
        return adoptRefWillBeNoop(new StyleSheetContents(0, String(), context));
    }
    static PassRefPtrWillBeRawPtr<StyleSheetContents> create(const String& originalURL, const CSSParserContext& context)
    {
        return adoptRefWillBeNoop(new StyleSheetContents(0, originalURL, context));
    }
    static PassRefPtrWillBeRawPtr<StyleSheetContents> create(StyleRuleImport* ownerRule, const String& originalURL, const CSSParserContext& context)
    {
        return adoptRefWillBeNoop(new StyleSheetContents(ownerRule, originalURL, context));
    }

    ~StyleSheetContents();

    const CSSParserContext& parserContext() const { return m_parserContext; }

    const AtomicString& determineNamespace(const AtomicString& prefix);

    void parseAuthorStyleSheet(const CSSStyleSheetResource*, const SecurityOrigin*);
    bool parseString(const String&);
    bool parseStringAtPosition(const String&, const TextPosition&, bool);

    bool isCacheable() const;
    bool maybeCacheable() const;

    bool isLoading() const;

    void checkLoaded();
    void startLoadingDynamicSheet();

    StyleSheetContents* rootStyleSheet() const;
    bool hasSingleOwnerNode() const;
    Node* singleOwnerNode() const;
    Document* singleOwnerDocument() const;

    const String& charset() const { return m_parserContext.charset(); }

    bool loadCompleted() const;
    bool hasFailedOrCanceledSubresources() const;

    KURL completeURL(const String& url) const;

    void setHasSyntacticallyValidCSSHeader(bool isValidCss);
    bool hasSyntacticallyValidCSSHeader() const { return m_hasSyntacticallyValidCSSHeader; }

    void setHasFontFaceRule(bool b) { m_hasFontFaceRule = b; }
    bool hasFontFaceRule() const { return m_hasFontFaceRule; }
    void findFontFaceRules(WillBeHeapVector<RawPtrWillBeMember<const StyleRuleFontFace> >& fontFaceRules);

    void parserAddNamespace(const AtomicString& prefix, const AtomicString& uri);
    void parserAppendRule(PassRefPtrWillBeRawPtr<StyleRuleBase>);
    void parserSetEncodingFromCharsetRule(const String& encoding);
    void parserSetUsesRemUnits(bool b) { m_usesRemUnits = b; }

    void clearRules();

    bool hasCharsetRule() const { return !m_encodingFromCharsetRule.isNull(); }
    String encodingFromCharsetRule() const { return m_encodingFromCharsetRule; }
    // Rules other than @charset and @import.
    const WillBeHeapVector<RefPtrWillBeMember<StyleRuleBase> >& childRules() const { return m_childRules; }
    const WillBeHeapVector<RefPtrWillBeMember<StyleRuleImport> >& importRules() const { return m_importRules; }

    void notifyLoadedSheet(const CSSStyleSheetResource*);

    StyleSheetContents* parentStyleSheet() const;
    StyleRuleImport* ownerRule() const { return m_ownerRule; }
    void clearOwnerRule() { m_ownerRule = nullptr; }

    // Note that href is the URL that started the redirect chain that led to
    // this style sheet. This property probably isn't useful for much except
    // the JavaScript binding (which needs to use this value for security).
    String originalURL() const { return m_originalURL; }
    const KURL& baseURL() const { return m_parserContext.baseURL(); }

    unsigned ruleCount() const;
    StyleRuleBase* ruleAt(unsigned index) const;

    bool usesRemUnits() const { return m_usesRemUnits; }

    unsigned estimatedSizeInBytes() const;

    bool wrapperInsertRule(PassRefPtrWillBeRawPtr<StyleRuleBase>, unsigned index);
    void wrapperDeleteRule(unsigned index);

    PassRefPtrWillBeRawPtr<StyleSheetContents> copy() const
    {
        return adoptRefWillBeNoop(new StyleSheetContents(*this));
    }

    void registerClient(CSSStyleSheet*);
    void unregisterClient(CSSStyleSheet*);
    bool hasOneClient() { return (m_loadingClients.size() + m_completedClients.size()) == 1; }
    void clientLoadCompleted(CSSStyleSheet*);
    void clientLoadStarted(CSSStyleSheet*);

    bool isMutable() const { return m_isMutable; }
    void setMutable() { m_isMutable = true; }

    bool isInMemoryCache() const { return m_isInMemoryCache; }
    void addedToMemoryCache();
    void removedFromMemoryCache();

    void setHasMediaQueries();
    bool hasMediaQueries() { return m_hasMediaQueries; }

    void shrinkToFit();
    RuleSet& ruleSet() { ASSERT(m_ruleSet); return *m_ruleSet.get(); }
    RuleSet& ensureRuleSet(const MediaQueryEvaluator&, AddRuleFlags);
    void clearRuleSet();

    void trace(Visitor*);

private:
    StyleSheetContents(StyleRuleImport* ownerRule, const String& originalURL, const CSSParserContext&);
    StyleSheetContents(const StyleSheetContents&);
    void notifyRemoveFontFaceRule(const StyleRuleFontFace*);

    void clearCharsetRule();

    RawPtrWillBeMember<StyleRuleImport> m_ownerRule;

    String m_originalURL;

    String m_encodingFromCharsetRule;
    WillBeHeapVector<RefPtrWillBeMember<StyleRuleImport> > m_importRules;
    WillBeHeapVector<RefPtrWillBeMember<StyleRuleBase> > m_childRules;
    typedef HashMap<AtomicString, AtomicString> PrefixNamespaceURIMap;
    PrefixNamespaceURIMap m_namespaces;

    bool m_hasSyntacticallyValidCSSHeader : 1;
    bool m_didLoadErrorOccur : 1;
    bool m_usesRemUnits : 1;
    bool m_isMutable : 1;
    bool m_isInMemoryCache : 1;
    bool m_hasFontFaceRule : 1;
    bool m_hasMediaQueries : 1;

    CSSParserContext m_parserContext;

    WillBeHeapHashSet<RawPtrWillBeWeakMember<CSSStyleSheet> > m_loadingClients;
    WillBeHeapHashSet<RawPtrWillBeWeakMember<CSSStyleSheet> > m_completedClients;
    typedef WillBeHeapHashSet<RawPtrWillBeWeakMember<CSSStyleSheet> >::iterator ClientsIterator;

    OwnPtrWillBeMember<RuleSet> m_ruleSet;
};

} // namespace

#endif
