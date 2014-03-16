/*
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2006, 2007, 2012 Apple Inc. All rights reserved.
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
#include "core/css/StyleSheetContents.h"

#include "core/css/parser/BisonCSSParser.h"
#include "core/css/CSSStyleSheet.h"
#include "core/css/MediaList.h"
#include "core/css/StylePropertySet.h"
#include "core/css/StyleRule.h"
#include "core/css/StyleRuleImport.h"
#include "core/dom/Document.h"
#include "core/dom/Node.h"
#include "core/dom/StyleEngine.h"
#include "core/fetch/CSSStyleSheetResource.h"
#include "core/frame/UseCounter.h"
#include "platform/TraceEvent.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "wtf/Deque.h"

namespace WebCore {

// Rough size estimate for the memory cache.
unsigned StyleSheetContents::estimatedSizeInBytes() const
{
    // Note that this does not take into account size of the strings hanging from various objects.
    // The assumption is that nearly all of of them are atomic and would exist anyway.
    unsigned size = sizeof(*this);

    // FIXME: This ignores the children of media rules.
    // Most rules are StyleRules.
    size += ruleCount() * StyleRule::averageSizeInBytes();

    for (unsigned i = 0; i < m_importRules.size(); ++i) {
        if (StyleSheetContents* sheet = m_importRules[i]->styleSheet())
            size += sheet->estimatedSizeInBytes();
    }
    return size;
}

StyleSheetContents::StyleSheetContents(StyleRuleImport* ownerRule, const String& originalURL, const CSSParserContext& context)
    : m_ownerRule(ownerRule)
    , m_originalURL(originalURL)
    , m_hasSyntacticallyValidCSSHeader(true)
    , m_didLoadErrorOccur(false)
    , m_usesRemUnits(false)
    , m_isMutable(false)
    , m_isInMemoryCache(false)
    , m_hasFontFaceRule(false)
    , m_hasMediaQueries(false)
    , m_parserContext(context)
{
}

StyleSheetContents::StyleSheetContents(const StyleSheetContents& o)
    : m_ownerRule(nullptr)
    , m_originalURL(o.m_originalURL)
    , m_encodingFromCharsetRule(o.m_encodingFromCharsetRule)
    , m_importRules(o.m_importRules.size())
    , m_childRules(o.m_childRules.size())
    , m_namespaces(o.m_namespaces)
    , m_hasSyntacticallyValidCSSHeader(o.m_hasSyntacticallyValidCSSHeader)
    , m_didLoadErrorOccur(false)
    , m_usesRemUnits(o.m_usesRemUnits)
    , m_isMutable(false)
    , m_isInMemoryCache(false)
    , m_hasFontFaceRule(o.m_hasFontFaceRule)
    , m_hasMediaQueries(o.m_hasMediaQueries)
    , m_parserContext(o.m_parserContext)
{
    ASSERT(o.isCacheable());

    // FIXME: Copy import rules.
    ASSERT(o.m_importRules.isEmpty());

    for (unsigned i = 0; i < m_childRules.size(); ++i)
        m_childRules[i] = o.m_childRules[i]->copy();
}

StyleSheetContents::~StyleSheetContents()
{
#if !ENABLE(OILPAN)
    StyleEngine::removeSheet(this);
    clearRules();
#endif
}

void StyleSheetContents::setHasSyntacticallyValidCSSHeader(bool isValidCss)
{
    if (maybeCacheable() && !isValidCss)
        StyleEngine::removeSheet(this);
    m_hasSyntacticallyValidCSSHeader = isValidCss;
}

bool StyleSheetContents::maybeCacheable() const
{
    // FIXME: StyleSheets with media queries can't be cached because their RuleSet
    // is processed differently based off the media queries, which might resolve
    // differently depending on the context of the parent CSSStyleSheet (e.g.
    // if they are in differently sized iframes). Once RuleSets are media query
    // agnostic, we can restore sharing of StyleSheetContents with medea queries.
    if (m_hasMediaQueries)
        return false;
    // FIXME: Support copying import rules.
    if (!m_importRules.isEmpty())
        return false;
    // FIXME: Support cached stylesheets in import rules.
    if (m_ownerRule)
        return false;
    if (m_didLoadErrorOccur)
        return false;
    // It is not the original sheet anymore.
    if (m_isMutable)
        return false;
    // If the header is valid we are not going to need to check the SecurityOrigin.
    // FIXME: Valid mime type avoids the check too.
    if (!m_hasSyntacticallyValidCSSHeader)
        return false;
    return true;
}

bool StyleSheetContents::isCacheable() const
{
    // This would require dealing with multiple clients for load callbacks.
    if (!loadCompleted())
        return false;
    return maybeCacheable();
}

void StyleSheetContents::parserAppendRule(PassRefPtrWillBeRawPtr<StyleRuleBase> rule)
{
    ASSERT(!rule->isCharsetRule());
    if (rule->isImportRule()) {
        // Parser enforces that @import rules come before anything else except @charset.
        ASSERT(m_childRules.isEmpty());
        StyleRuleImport* importRule = toStyleRuleImport(rule.get());
        if (importRule->mediaQueries())
            setHasMediaQueries();
        m_importRules.append(importRule);
        m_importRules.last()->setParentStyleSheet(this);
        m_importRules.last()->requestStyleSheet();
        return;
    }

    // Add warning message to inspector if dpi/dpcm values are used for screen media.
    if (rule->isMediaRule()) {
        setHasMediaQueries();
        reportMediaQueryWarningIfNeeded(singleOwnerDocument(), toStyleRuleMedia(rule.get())->mediaQueries());
    }

    m_childRules.append(rule);
}

void StyleSheetContents::setHasMediaQueries()
{
    m_hasMediaQueries = true;
    if (parentStyleSheet())
        parentStyleSheet()->setHasMediaQueries();
}

StyleRuleBase* StyleSheetContents::ruleAt(unsigned index) const
{
    ASSERT_WITH_SECURITY_IMPLICATION(index < ruleCount());

    unsigned childVectorIndex = index;
    if (hasCharsetRule()) {
        if (index == 0)
            return 0;
        --childVectorIndex;
    }
    if (childVectorIndex < m_importRules.size())
        return m_importRules[childVectorIndex].get();

    childVectorIndex -= m_importRules.size();
    return m_childRules[childVectorIndex].get();
}

unsigned StyleSheetContents::ruleCount() const
{
    unsigned result = 0;
    result += hasCharsetRule() ? 1 : 0;
    result += m_importRules.size();
    result += m_childRules.size();
    return result;
}

void StyleSheetContents::clearCharsetRule()
{
    m_encodingFromCharsetRule = String();
}

void StyleSheetContents::clearRules()
{
    for (unsigned i = 0; i < m_importRules.size(); ++i) {
        ASSERT(m_importRules.at(i)->parentStyleSheet() == this);
        m_importRules[i]->clearParentStyleSheet();
    }
    m_importRules.clear();
    m_childRules.clear();
    clearCharsetRule();
}

void StyleSheetContents::parserSetEncodingFromCharsetRule(const String& encoding)
{
    // Parser enforces that there is ever only one @charset.
    ASSERT(m_encodingFromCharsetRule.isNull());
    m_encodingFromCharsetRule = encoding;
}

bool StyleSheetContents::wrapperInsertRule(PassRefPtrWillBeRawPtr<StyleRuleBase> rule, unsigned index)
{
    ASSERT(m_isMutable);
    ASSERT_WITH_SECURITY_IMPLICATION(index <= ruleCount());
    // Parser::parseRule doesn't currently allow @charset so we don't need to deal with it.
    ASSERT(!rule->isCharsetRule());

    unsigned childVectorIndex = index;
    // m_childRules does not contain @charset which is always in index 0 if it exists.
    if (hasCharsetRule()) {
        if (childVectorIndex == 0) {
            // Nothing can be inserted before @charset.
            return false;
        }
        --childVectorIndex;
    }

    if (childVectorIndex < m_importRules.size() || (childVectorIndex == m_importRules.size() && rule->isImportRule())) {
        // Inserting non-import rule before @import is not allowed.
        if (!rule->isImportRule())
            return false;

        StyleRuleImport* importRule = toStyleRuleImport(rule.get());
        if (importRule->mediaQueries())
            setHasMediaQueries();

        m_importRules.insert(childVectorIndex, importRule);
        m_importRules[childVectorIndex]->setParentStyleSheet(this);
        m_importRules[childVectorIndex]->requestStyleSheet();
        // FIXME: Stylesheet doesn't actually change meaningfully before the imported sheets are loaded.
        return true;
    }
    // Inserting @import rule after a non-import rule is not allowed.
    if (rule->isImportRule())
        return false;

    if (rule->isMediaRule())
        setHasMediaQueries();

    childVectorIndex -= m_importRules.size();

    if (rule->isFontFaceRule())
        setHasFontFaceRule(true);
    m_childRules.insert(childVectorIndex, rule);
    return true;
}

void StyleSheetContents::wrapperDeleteRule(unsigned index)
{
    ASSERT(m_isMutable);
    ASSERT_WITH_SECURITY_IMPLICATION(index < ruleCount());

    unsigned childVectorIndex = index;
    if (hasCharsetRule()) {
        if (childVectorIndex == 0) {
            clearCharsetRule();
            return;
        }
        --childVectorIndex;
    }
    if (childVectorIndex < m_importRules.size()) {
        m_importRules[childVectorIndex]->clearParentStyleSheet();
        if (m_importRules[childVectorIndex]->isFontFaceRule())
            notifyRemoveFontFaceRule(toStyleRuleFontFace(m_importRules[childVectorIndex].get()));
        m_importRules.remove(childVectorIndex);
        return;
    }
    childVectorIndex -= m_importRules.size();

    if (m_childRules[childVectorIndex]->isFontFaceRule())
        notifyRemoveFontFaceRule(toStyleRuleFontFace(m_childRules[childVectorIndex].get()));
    m_childRules.remove(childVectorIndex);
}

void StyleSheetContents::parserAddNamespace(const AtomicString& prefix, const AtomicString& uri)
{
    if (uri.isNull() || prefix.isNull())
        return;
    PrefixNamespaceURIMap::AddResult result = m_namespaces.add(prefix, uri);
    if (result.isNewEntry)
        return;
    result.storedValue->value = uri;
}

const AtomicString& StyleSheetContents::determineNamespace(const AtomicString& prefix)
{
    if (prefix.isNull())
        return nullAtom; // No namespace. If an element/attribute has a namespace, we won't match it.
    if (prefix == starAtom)
        return starAtom; // We'll match any namespace.
    return m_namespaces.get(prefix);
}

void StyleSheetContents::parseAuthorStyleSheet(const CSSStyleSheetResource* cachedStyleSheet, const SecurityOrigin* securityOrigin)
{
    TRACE_EVENT0("webkit", "StyleSheetContents::parseAuthorStyleSheet");

    bool quirksMode = isQuirksModeBehavior(m_parserContext.mode());

    bool enforceMIMEType = !quirksMode;
    bool hasValidMIMEType = false;
    String sheetText = cachedStyleSheet->sheetText(enforceMIMEType, &hasValidMIMEType);

    CSSParserContext context(parserContext(), UseCounter::getFrom(this));
    BisonCSSParser p(context);
    p.parseSheet(this, sheetText, TextPosition::minimumPosition(), 0, true);

    // If we're loading a stylesheet cross-origin, and the MIME type is not standard, require the CSS
    // to at least start with a syntactically valid CSS rule.
    // This prevents an attacker playing games by injecting CSS strings into HTML, XML, JSON, etc. etc.
    if (!hasValidMIMEType && !hasSyntacticallyValidCSSHeader()) {
        bool isCrossOriginCSS = !securityOrigin || !securityOrigin->canRequest(baseURL());
        if (isCrossOriginCSS) {
            clearRules();
            return;
        }
    }
}

bool StyleSheetContents::parseString(const String& sheetText)
{
    return parseStringAtPosition(sheetText, TextPosition::minimumPosition(), false);
}

bool StyleSheetContents::parseStringAtPosition(const String& sheetText, const TextPosition& startPosition, bool createdByParser)
{
    CSSParserContext context(parserContext(), UseCounter::getFrom(this));
    BisonCSSParser p(context);
    p.parseSheet(this, sheetText, startPosition, 0, createdByParser);

    return true;
}

bool StyleSheetContents::isLoading() const
{
    for (unsigned i = 0; i < m_importRules.size(); ++i) {
        if (m_importRules[i]->isLoading())
            return true;
    }
    return false;
}

bool StyleSheetContents::loadCompleted() const
{
    StyleSheetContents* parentSheet = parentStyleSheet();
    if (parentSheet)
        return parentSheet->loadCompleted();

    StyleSheetContents* root = rootStyleSheet();
    return root->m_loadingClients.isEmpty();
}

void StyleSheetContents::checkLoaded()
{
    if (isLoading())
        return;

    // Avoid |this| being deleted by scripts that run via
    // ScriptableDocumentParser::executeScriptsWaitingForResources().
    // See https://bugs.webkit.org/show_bug.cgi?id=95106
    RefPtrWillBeRawPtr<StyleSheetContents> protect(this);

    StyleSheetContents* parentSheet = parentStyleSheet();
    if (parentSheet) {
        parentSheet->checkLoaded();
        return;
    }

    StyleSheetContents* root = rootStyleSheet();
    if (root->m_loadingClients.isEmpty())
        return;

    // Avoid |CSSSStyleSheet| and |ownerNode| being deleted by scripts that run via
    // ScriptableDocumentParser::executeScriptsWaitingForResources(). Also protect
    // the |CSSStyleSheet| from being deleted during iteration via the |sheetLoaded|
    // method.
    //
    // FIXME: oilpan: with oilpan we do not need to protect the CSSStyleSheets
    // explicitly during the iteration. The iterator will make the pointers
    // strong during iteration so we can just directly iterate root->m_clients.
    WillBeHeapVector<RefPtrWillBeMember<CSSStyleSheet> > protectedClients;
    copyToVector(m_loadingClients, protectedClients);

    for (unsigned i = 0; i < protectedClients.size(); ++i) {
        if (protectedClients[i]->loadCompleted())
            continue;

        // sheetLoaded might be invoked after its owner node is removed from document.
        if (RefPtr<Node> ownerNode = protectedClients[i]->ownerNode()) {
            if (protectedClients[i]->sheetLoaded())
                ownerNode->notifyLoadedSheetAndAllCriticalSubresources(m_didLoadErrorOccur);
        }
    }
}

void StyleSheetContents::notifyLoadedSheet(const CSSStyleSheetResource* sheet)
{
    ASSERT(sheet);
    m_didLoadErrorOccur |= sheet->errorOccurred();
    // updateLayoutIgnorePendingStyleSheets can cause us to create the RuleSet on this
    // sheet before its imports have loaded. So clear the RuleSet when the imports
    // load since the import's subrules are flattened into its parent sheet's RuleSet.
    clearRuleSet();
}

void StyleSheetContents::startLoadingDynamicSheet()
{
    StyleSheetContents* root = rootStyleSheet();
    for (ClientsIterator it = root->m_loadingClients.begin(); it != root->m_loadingClients.end(); ++it)
        (*it)->startLoadingDynamicSheet();
    for (ClientsIterator it = root->m_completedClients.begin(); it != root->m_completedClients.end(); ++it)
        (*it)->startLoadingDynamicSheet();
}

StyleSheetContents* StyleSheetContents::rootStyleSheet() const
{
    const StyleSheetContents* root = this;
    while (root->parentStyleSheet())
        root = root->parentStyleSheet();
    return const_cast<StyleSheetContents*>(root);
}

bool StyleSheetContents::hasSingleOwnerNode() const
{
    return rootStyleSheet()->hasOneClient();
}

Node* StyleSheetContents::singleOwnerNode() const
{
    StyleSheetContents* root = rootStyleSheet();
    if (!root->hasOneClient())
        return 0;
    if (root->m_loadingClients.size())
        return (*root->m_loadingClients.begin())->ownerNode();
    return (*root->m_completedClients.begin())->ownerNode();
}

Document* StyleSheetContents::singleOwnerDocument() const
{
    Node* ownerNode = singleOwnerNode();
    return ownerNode ? &ownerNode->document() : 0;
}

KURL StyleSheetContents::completeURL(const String& url) const
{
    // FIXME: This is only OK when we have a singleOwnerNode, right?
    return m_parserContext.completeURL(url);
}

static bool childRulesHaveFailedOrCanceledSubresources(const WillBeHeapVector<RefPtrWillBeMember<StyleRuleBase> >& rules)
{
    for (unsigned i = 0; i < rules.size(); ++i) {
        const StyleRuleBase* rule = rules[i].get();
        switch (rule->type()) {
        case StyleRuleBase::Style:
            if (toStyleRule(rule)->properties().hasFailedOrCanceledSubresources())
                return true;
            break;
        case StyleRuleBase::FontFace:
            if (toStyleRuleFontFace(rule)->properties().hasFailedOrCanceledSubresources())
                return true;
            break;
        case StyleRuleBase::Media:
            if (childRulesHaveFailedOrCanceledSubresources(toStyleRuleMedia(rule)->childRules()))
                return true;
            break;
        case StyleRuleBase::Import:
            ASSERT_NOT_REACHED();
        case StyleRuleBase::Page:
        case StyleRuleBase::Keyframes:
        case StyleRuleBase::Unknown:
        case StyleRuleBase::Charset:
        case StyleRuleBase::Keyframe:
        case StyleRuleBase::Supports:
        case StyleRuleBase::Viewport:
        case StyleRuleBase::Filter:
            break;
        }
    }
    return false;
}

bool StyleSheetContents::hasFailedOrCanceledSubresources() const
{
    ASSERT(isCacheable());
    return childRulesHaveFailedOrCanceledSubresources(m_childRules);
}

StyleSheetContents* StyleSheetContents::parentStyleSheet() const
{
    return m_ownerRule ? m_ownerRule->parentStyleSheet() : 0;
}

void StyleSheetContents::registerClient(CSSStyleSheet* sheet)
{
    ASSERT(!m_loadingClients.contains(sheet) && !m_completedClients.contains(sheet));
    m_loadingClients.add(sheet);
}

void StyleSheetContents::unregisterClient(CSSStyleSheet* sheet)
{
    ASSERT(m_loadingClients.contains(sheet) || m_completedClients.contains(sheet));
    m_loadingClients.remove(sheet);
    m_completedClients.remove(sheet);
}

void StyleSheetContents::clientLoadCompleted(CSSStyleSheet* sheet)
{
    ASSERT(m_loadingClients.contains(sheet));
    m_loadingClients.remove(sheet);
    m_completedClients.add(sheet);
}

void StyleSheetContents::clientLoadStarted(CSSStyleSheet* sheet)
{
    ASSERT(m_completedClients.contains(sheet));
    m_completedClients.remove(sheet);
    m_loadingClients.add(sheet);
}

void StyleSheetContents::addedToMemoryCache()
{
    ASSERT(!m_isInMemoryCache);
    ASSERT(isCacheable());
    m_isInMemoryCache = true;
}

void StyleSheetContents::removedFromMemoryCache()
{
    ASSERT(m_isInMemoryCache);
    ASSERT(isCacheable());
    m_isInMemoryCache = false;
}

void StyleSheetContents::shrinkToFit()
{
    m_importRules.shrinkToFit();
    m_childRules.shrinkToFit();
}

RuleSet& StyleSheetContents::ensureRuleSet(const MediaQueryEvaluator& medium, AddRuleFlags addRuleFlags)
{
    if (!m_ruleSet) {
        m_ruleSet = RuleSet::create();
        m_ruleSet->addRulesFromSheet(this, medium, addRuleFlags);
    }
    return *m_ruleSet.get();
}

static void clearResolvers(WillBeHeapHashSet<RawPtrWillBeWeakMember<CSSStyleSheet> >& clients)
{
    for (WillBeHeapHashSet<RawPtrWillBeWeakMember<CSSStyleSheet> >::iterator it = clients.begin(); it != clients.end(); ++it) {
        if (Document* document = (*it)->ownerDocument())
            document->styleEngine()->clearResolver();
    }
}

void StyleSheetContents::clearRuleSet()
{
    if (StyleSheetContents* parentSheet = parentStyleSheet())
        parentSheet->clearRuleSet();

    // Don't want to clear the StyleResolver if the RuleSet hasn't been created
    // since we only clear the StyleResolver so that it's members are properly
    // updated in ScopedStyleResolver::addRulesFromSheet.
    if (!m_ruleSet)
        return;

    // Clearing the ruleSet means we need to recreate the styleResolver data structures.
    // See the StyleResolver calls in ScopedStyleResolver::addRulesFromSheet.
    clearResolvers(m_loadingClients);
    clearResolvers(m_completedClients);
    m_ruleSet.clear();
}

static void removeFontFaceRules(WillBeHeapHashSet<RawPtrWillBeWeakMember<CSSStyleSheet> >& clients, const StyleRuleFontFace* fontFaceRule)
{
    for (WillBeHeapHashSet<RawPtrWillBeWeakMember<CSSStyleSheet> >::iterator it = clients.begin(); it != clients.end(); ++it) {
        if (Node* ownerNode = (*it)->ownerNode())
            ownerNode->document().styleEngine()->removeFontFaceRules(WillBeHeapVector<RawPtrWillBeMember<const StyleRuleFontFace> >(1, fontFaceRule));
    }
}

void StyleSheetContents::notifyRemoveFontFaceRule(const StyleRuleFontFace* fontFaceRule)
{
    StyleSheetContents* root = rootStyleSheet();
    removeFontFaceRules(root->m_loadingClients, fontFaceRule);
    removeFontFaceRules(root->m_completedClients, fontFaceRule);
}

static void findFontFaceRulesFromRules(const WillBeHeapVector<RefPtrWillBeMember<StyleRuleBase> >& rules, WillBeHeapVector<RawPtrWillBeMember<const StyleRuleFontFace> >& fontFaceRules)
{
    for (unsigned i = 0; i < rules.size(); ++i) {
        StyleRuleBase* rule = rules[i].get();

        if (rule->isFontFaceRule()) {
            fontFaceRules.append(toStyleRuleFontFace(rule));
        } else if (rule->isMediaRule()) {
            StyleRuleMedia* mediaRule = toStyleRuleMedia(rule);
            // We cannot know whether the media rule matches or not, but
            // for safety, remove @font-face in the media rule (if exists).
            findFontFaceRulesFromRules(mediaRule->childRules(), fontFaceRules);
        }
    }
}

void StyleSheetContents::findFontFaceRules(WillBeHeapVector<RawPtrWillBeMember<const StyleRuleFontFace> >& fontFaceRules)
{
    for (unsigned i = 0; i < m_importRules.size(); ++i) {
        if (!m_importRules[i]->styleSheet())
            continue;
        m_importRules[i]->styleSheet()->findFontFaceRules(fontFaceRules);
    }

    findFontFaceRulesFromRules(childRules(), fontFaceRules);
}

void StyleSheetContents::trace(Visitor* visitor)
{
    visitor->trace(m_ownerRule);
    visitor->trace(m_importRules);
    visitor->trace(m_childRules);
    visitor->trace(m_loadingClients);
    visitor->trace(m_completedClients);
    visitor->trace(m_ruleSet);
}

}
