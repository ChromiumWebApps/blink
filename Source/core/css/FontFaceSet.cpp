/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#include "config.h"
#include "core/css/FontFaceSet.h"

#include "RuntimeEnabledFeatures.h"
#include "bindings/v8/Dictionary.h"
#include "bindings/v8/ScriptPromiseResolver.h"
#include "bindings/v8/ScriptScope.h"
#include "bindings/v8/ScriptState.h"
#include "core/css/CSSFontFaceLoadEvent.h"
#include "core/css/CSSFontSelector.h"
#include "core/css/parser/BisonCSSParser.h"
#include "core/css/CSSSegmentedFontFace.h"
#include "core/css/FontFaceCache.h"
#include "core/css/StylePropertySet.h"
#include "core/css/resolver/StyleResolver.h"
#include "core/dom/Document.h"
#include "core/dom/StyleEngine.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "public/platform/Platform.h"

namespace WebCore {

static const int defaultFontSize = 10;
static const char defaultFontFamily[] = "sans-serif";

class LoadFontPromiseResolver : public CSSSegmentedFontFace::LoadFontCallback {
public:
    static PassRefPtr<LoadFontPromiseResolver> create(const FontFamily& family, ExecutionContext* context)
    {
        int numFamilies = 0;
        for (const FontFamily* f = &family; f; f = f->next())
            numFamilies++;
        return adoptRef<LoadFontPromiseResolver>(new LoadFontPromiseResolver(numFamilies, context));
    }

    ScriptPromise promise() { return m_resolver->promise(); }

    virtual void notifyLoaded(CSSSegmentedFontFace*) OVERRIDE { loaded(); }
    virtual void notifyError(CSSSegmentedFontFace*) OVERRIDE { error(); }
    void loaded();
    void error();

private:
    LoadFontPromiseResolver(int numLoading, ExecutionContext* context)
        : m_numLoading(numLoading)
        , m_errorOccured(false)
        , m_scriptState(ScriptState::current())
        , m_resolver(ScriptPromiseResolver::create(context))
    { }

    int m_numLoading;
    bool m_errorOccured;
    ScriptState* m_scriptState;
    RefPtr<ScriptPromiseResolver> m_resolver;
};

void LoadFontPromiseResolver::loaded()
{
    m_numLoading--;
    if (m_numLoading)
        return;

    ScriptScope scope(m_scriptState);
    if (m_errorOccured)
        m_resolver->reject(ScriptValue::createNull());
    else
        m_resolver->resolve(ScriptValue::createNull());
}

void LoadFontPromiseResolver::error()
{
    m_errorOccured = true;
    loaded();
}

class FontsReadyPromiseResolver {
public:
    static PassOwnPtr<FontsReadyPromiseResolver> create(ExecutionContext* context)
    {
        return adoptPtr(new FontsReadyPromiseResolver(context));
    }

    void resolve(PassRefPtr<FontFaceSet> fontFaceSet)
    {
        ScriptScope scope(m_scriptState);
        m_resolver->resolve(fontFaceSet);
    }

    ScriptPromise promise() { return m_resolver->promise(); }

private:
    FontsReadyPromiseResolver(ExecutionContext* context)
        : m_scriptState(ScriptState::current())
        , m_resolver(ScriptPromiseResolver::create(context))
    { }
    ScriptState* m_scriptState;
    RefPtr<ScriptPromiseResolver> m_resolver;
};

FontFaceSet::FontFaceSet(Document& document)
    : ActiveDOMObject(&document)
    , m_shouldFireLoadingEvent(false)
    , m_asyncRunner(this, &FontFaceSet::handlePendingEventsAndPromises)
{
    suspendIfNeeded();
}

FontFaceSet::~FontFaceSet()
{
}

Document* FontFaceSet::document() const
{
    return toDocument(executionContext());
}

bool FontFaceSet::inActiveDocumentContext() const
{
    ExecutionContext* context = executionContext();
    return context && toDocument(context)->isActive();
}

void FontFaceSet::addFontFacesToFontFaceCache(FontFaceCache* fontFaceCache, CSSFontSelector* fontSelector)
{
    for (ListHashSet<RefPtr<FontFace> >::iterator it = m_nonCSSConnectedFaces.begin(); it != m_nonCSSConnectedFaces.end(); ++it)
        fontFaceCache->addFontFace(fontSelector, *it, false);
}

const AtomicString& FontFaceSet::interfaceName() const
{
    return EventTargetNames::FontFaceSet;
}

ExecutionContext* FontFaceSet::executionContext() const
{
    return ActiveDOMObject::executionContext();
}

AtomicString FontFaceSet::status() const
{
    DEFINE_STATIC_LOCAL(AtomicString, loading, ("loading", AtomicString::ConstructFromLiteral));
    DEFINE_STATIC_LOCAL(AtomicString, loaded, ("loaded", AtomicString::ConstructFromLiteral));
    return (!m_loadingFonts.isEmpty() || hasLoadedFonts()) ? loading : loaded;
}

void FontFaceSet::handlePendingEventsAndPromisesSoon()
{
    // setPendingActivity() is unnecessary because m_asyncRunner will be
    // automatically stopped on destruction.
    m_asyncRunner.runAsync();
}

void FontFaceSet::didLayout()
{
    if (document()->frame()->isMainFrame() && m_loadingFonts.isEmpty())
        m_histogram.record();
    if (!RuntimeEnabledFeatures::fontLoadEventsEnabled())
        return;
    if (!m_loadingFonts.isEmpty() || (!hasLoadedFonts() && m_readyResolvers.isEmpty()))
        return;
    handlePendingEventsAndPromisesSoon();
}

void FontFaceSet::handlePendingEventsAndPromises()
{
    fireLoadingEvent();
    fireDoneEventIfPossible();
}

void FontFaceSet::fireLoadingEvent()
{
    if (m_shouldFireLoadingEvent) {
        m_shouldFireLoadingEvent = false;
        dispatchEvent(CSSFontFaceLoadEvent::createForFontFaces(EventTypeNames::loading));
    }
}

void FontFaceSet::suspend()
{
    m_asyncRunner.suspend();
}

void FontFaceSet::resume()
{
    m_asyncRunner.resume();
}

void FontFaceSet::stop()
{
    m_asyncRunner.stop();
}

void FontFaceSet::beginFontLoading(FontFace* fontFace)
{
    m_histogram.incrementCount();
    addToLoadingFonts(fontFace);
}

void FontFaceSet::fontLoaded(FontFace* fontFace)
{
    m_histogram.updateStatus(fontFace);
    if (RuntimeEnabledFeatures::fontLoadEventsEnabled())
        m_loadedFonts.append(fontFace);
    removeFromLoadingFonts(fontFace);
}

void FontFaceSet::loadError(FontFace* fontFace)
{
    m_histogram.updateStatus(fontFace);
    if (RuntimeEnabledFeatures::fontLoadEventsEnabled())
        m_failedFonts.append(fontFace);
    removeFromLoadingFonts(fontFace);
}

void FontFaceSet::addToLoadingFonts(PassRefPtr<FontFace> fontFace)
{
    if (RuntimeEnabledFeatures::fontLoadEventsEnabled() && m_loadingFonts.isEmpty() && !hasLoadedFonts()) {
        m_shouldFireLoadingEvent = true;
        handlePendingEventsAndPromisesSoon();
    }
    m_loadingFonts.add(fontFace);
}

void FontFaceSet::removeFromLoadingFonts(PassRefPtr<FontFace> fontFace)
{
    m_loadingFonts.remove(fontFace);
    if (RuntimeEnabledFeatures::fontLoadEventsEnabled() && m_loadingFonts.isEmpty())
        handlePendingEventsAndPromisesSoon();
}

ScriptPromise FontFaceSet::ready()
{
    if (!inActiveDocumentContext())
        return ScriptPromise();
    OwnPtr<FontsReadyPromiseResolver> resolver = FontsReadyPromiseResolver::create(executionContext());
    ScriptPromise promise = resolver->promise();
    m_readyResolvers.append(resolver.release());
    handlePendingEventsAndPromisesSoon();
    return promise;
}

void FontFaceSet::add(FontFace* fontFace, ExceptionState& exceptionState)
{
    if (!inActiveDocumentContext())
        return;
    if (!fontFace) {
        exceptionState.throwTypeError("The argument is not a FontFace.");
        return;
    }
    if (m_nonCSSConnectedFaces.contains(fontFace))
        return;
    if (isCSSConnectedFontFace(fontFace)) {
        exceptionState.throwDOMException(InvalidModificationError, "Cannot add a CSS-connected FontFace.");
        return;
    }
    CSSFontSelector* fontSelector = document()->styleEngine()->fontSelector();
    m_nonCSSConnectedFaces.add(fontFace);
    fontSelector->fontFaceCache()->addFontFace(fontSelector, fontFace, false);
    if (fontFace->loadStatus() == FontFace::Loading)
        addToLoadingFonts(fontFace);
}

void FontFaceSet::clear()
{
    if (!inActiveDocumentContext())
        return;
    FontFaceCache* fontFaceCache = document()->styleEngine()->fontSelector()->fontFaceCache();
    for (ListHashSet<RefPtr<FontFace> >::iterator it = m_nonCSSConnectedFaces.begin(); it != m_nonCSSConnectedFaces.end(); ++it) {
        fontFaceCache->removeFontFace(it->get(), false);
        if ((*it)->loadStatus() == FontFace::Loading)
            removeFromLoadingFonts(*it);
    }
    m_nonCSSConnectedFaces.clear();
}

bool FontFaceSet::remove(FontFace* fontFace, ExceptionState& exceptionState)
{
    if (!inActiveDocumentContext())
        return false;
    if (!fontFace) {
        exceptionState.throwTypeError("The argument is not a FontFace.");
        return false;
    }
    ListHashSet<RefPtr<FontFace> >::iterator it = m_nonCSSConnectedFaces.find(fontFace);
    if (it != m_nonCSSConnectedFaces.end()) {
        m_nonCSSConnectedFaces.remove(it);
        document()->styleEngine()->fontSelector()->fontFaceCache()->removeFontFace(fontFace, false);
        if (fontFace->loadStatus() == FontFace::Loading)
            removeFromLoadingFonts(fontFace);
        return true;
    }
    if (isCSSConnectedFontFace(fontFace))
        exceptionState.throwDOMException(InvalidModificationError, "Cannot delete a CSS-connected FontFace.");
    return false;
}

bool FontFaceSet::has(FontFace* fontFace, ExceptionState& exceptionState) const
{
    if (!inActiveDocumentContext())
        return false;
    if (!fontFace) {
        exceptionState.throwTypeError("The argument is not a FontFace.");
        return false;
    }
    return m_nonCSSConnectedFaces.contains(fontFace) || isCSSConnectedFontFace(fontFace);
}

const ListHashSet<RefPtr<FontFace> >& FontFaceSet::cssConnectedFontFaceList() const
{
    Document* d = document();
    d->ensureStyleResolver(); // Flush pending style changes.
    return d->styleEngine()->fontSelector()->fontFaceCache()->cssConnectedFontFaces();
}

bool FontFaceSet::isCSSConnectedFontFace(FontFace* fontFace) const
{
    return cssConnectedFontFaceList().contains(fontFace);
}

void FontFaceSet::forEach(PassOwnPtr<FontFaceSetForEachCallback> callback, ScriptValue& thisArg) const
{
    forEachInternal(callback, &thisArg);
}

void FontFaceSet::forEach(PassOwnPtr<FontFaceSetForEachCallback> callback) const
{
    forEachInternal(callback, 0);
}

void FontFaceSet::forEachInternal(PassOwnPtr<FontFaceSetForEachCallback> callback, ScriptValue* thisArg) const
{
    if (!inActiveDocumentContext())
        return;
    const ListHashSet<RefPtr<FontFace> >& cssConnectedFaces = cssConnectedFontFaceList();
    Vector<RefPtr<FontFace> > fontFaces;
    fontFaces.reserveInitialCapacity(cssConnectedFaces.size() + m_nonCSSConnectedFaces.size());
    for (ListHashSet<RefPtr<FontFace> >::const_iterator it = cssConnectedFaces.begin(); it != cssConnectedFaces.end(); ++it)
        fontFaces.append(*it);
    for (ListHashSet<RefPtr<FontFace> >::const_iterator it = m_nonCSSConnectedFaces.begin(); it != m_nonCSSConnectedFaces.end(); ++it)
        fontFaces.append(*it);

    for (size_t i = 0; i < fontFaces.size(); ++i) {
        FontFace* face = fontFaces[i].get();
        if (thisArg)
            callback->handleItem(*thisArg, face, face, const_cast<FontFaceSet*>(this));
        else
            callback->handleItem(face, face, const_cast<FontFaceSet*>(this));
    }
}

unsigned long FontFaceSet::size() const
{
    if (!inActiveDocumentContext())
        return m_nonCSSConnectedFaces.size();
    return cssConnectedFontFaceList().size() + m_nonCSSConnectedFaces.size();
}

void FontFaceSet::fireDoneEventIfPossible()
{
    if (m_shouldFireLoadingEvent)
        return;
    if (!m_loadingFonts.isEmpty() || (!hasLoadedFonts() && m_readyResolvers.isEmpty()))
        return;

    // If the layout was invalidated in between when we thought layout
    // was updated and when we're ready to fire the event, just wait
    // until after the next layout before firing events.
    Document* d = document();
    if (!d->view() || d->view()->needsLayout())
        return;

    if (hasLoadedFonts()) {
        RefPtr<CSSFontFaceLoadEvent> doneEvent;
        RefPtr<CSSFontFaceLoadEvent> errorEvent;
        doneEvent = CSSFontFaceLoadEvent::createForFontFaces(EventTypeNames::loadingdone, m_loadedFonts);
        m_loadedFonts.clear();
        if (!m_failedFonts.isEmpty()) {
            errorEvent = CSSFontFaceLoadEvent::createForFontFaces(EventTypeNames::loadingerror, m_failedFonts);
            m_failedFonts.clear();
        }
        dispatchEvent(doneEvent);
        if (errorEvent)
            dispatchEvent(errorEvent);
    }

    if (!m_readyResolvers.isEmpty()) {
        Vector<OwnPtr<FontsReadyPromiseResolver> > resolvers;
        m_readyResolvers.swap(resolvers);
        for (size_t index = 0; index < resolvers.size(); ++index)
            resolvers[index]->resolve(this);
    }
}

static const String& nullToSpace(const String& s)
{
    DEFINE_STATIC_LOCAL(String, space, (" "));
    return s.isNull() ? space : s;
}

ScriptPromise FontFaceSet::load(const String& fontString, const String& text, ExceptionState& exceptionState)
{
    if (!inActiveDocumentContext())
        return ScriptPromise();

    Font font;
    if (!resolveFontStyle(fontString, font)) {
        exceptionState.throwDOMException(SyntaxError, "Could not resolve '" + fontString + "' as a font.");
        return ScriptPromise();
    }

    CSSFontSelector* fontSelector = document()->styleEngine()->fontSelector();
    FontFaceCache* fontFaceCache = fontSelector->fontFaceCache();
    RefPtr<LoadFontPromiseResolver> resolver = LoadFontPromiseResolver::create(font.fontDescription().family(), executionContext());
    ScriptPromise promise = resolver->promise();
    for (const FontFamily* f = &font.fontDescription().family(); f; f = f->next()) {
        CSSSegmentedFontFace* face = fontFaceCache->get(font.fontDescription(), f->family());
        if (!face) {
            resolver->error();
            continue;
        }
        face->loadFont(font.fontDescription(), nullToSpace(text), resolver);
    }
    fontSelector->loadPendingFonts();
    return promise;
}

bool FontFaceSet::check(const String& fontString, const String& text, ExceptionState& exceptionState)
{
    if (!inActiveDocumentContext())
        return false;

    Font font;
    if (!resolveFontStyle(fontString, font)) {
        exceptionState.throwDOMException(SyntaxError, "Could not resolve '" + fontString + "' as a font.");
        return false;
    }

    FontFaceCache* fontFaceCache = document()->styleEngine()->fontSelector()->fontFaceCache();
    for (const FontFamily* f = &font.fontDescription().family(); f; f = f->next()) {
        CSSSegmentedFontFace* face = fontFaceCache->get(font.fontDescription(), f->family());
        if (face && !face->checkFont(nullToSpace(text)))
            return false;
    }
    return true;
}

bool FontFaceSet::resolveFontStyle(const String& fontString, Font& font)
{
    if (fontString.isEmpty())
        return false;

    // Interpret fontString in the same way as the 'font' attribute of CanvasRenderingContext2D.
    RefPtr<MutableStylePropertySet> parsedStyle = MutableStylePropertySet::create();
    BisonCSSParser::parseValue(parsedStyle.get(), CSSPropertyFont, fontString, true, HTMLStandardMode, 0);
    if (parsedStyle->isEmpty())
        return false;

    String fontValue = parsedStyle->getPropertyValue(CSSPropertyFont);
    if (fontValue == "inherit" || fontValue == "initial")
        return false;

    RefPtr<RenderStyle> style = RenderStyle::create();

    FontFamily fontFamily;
    fontFamily.setFamily(defaultFontFamily);

    FontDescription defaultFontDescription;
    defaultFontDescription.setFamily(fontFamily);
    defaultFontDescription.setSpecifiedSize(defaultFontSize);
    defaultFontDescription.setComputedSize(defaultFontSize);

    style->setFontDescription(defaultFontDescription);

    style->font().update(style->font().fontSelector());

    // Now map the font property longhands into the style.
    CSSPropertyValue properties[] = {
        CSSPropertyValue(CSSPropertyFontFamily, *parsedStyle),
        CSSPropertyValue(CSSPropertyFontStyle, *parsedStyle),
        CSSPropertyValue(CSSPropertyFontVariant, *parsedStyle),
        CSSPropertyValue(CSSPropertyFontWeight, *parsedStyle),
        CSSPropertyValue(CSSPropertyFontSize, *parsedStyle),
        CSSPropertyValue(CSSPropertyLineHeight, *parsedStyle),
    };
    StyleResolver& styleResolver = document()->ensureStyleResolver();
    styleResolver.applyPropertiesToStyle(properties, WTF_ARRAY_LENGTH(properties), style.get());

    font = style->font();
    font.update(document()->styleEngine()->fontSelector());
    return true;
}

void FontFaceSet::FontLoadHistogram::updateStatus(FontFace* fontFace)
{
    if (m_status == Reported)
        return;
    if (fontFace->hadBlankText())
        m_status = HadBlankText;
    else if (m_status == NoWebFonts)
        m_status = DidNotHaveBlankText;
}

void FontFaceSet::FontLoadHistogram::record()
{
    if (!m_recorded) {
        m_recorded = true;
        blink::Platform::current()->histogramCustomCounts("WebFont.WebFontsInPage", m_count, 1, 100, 50);
    }
    if (m_status == HadBlankText || m_status == DidNotHaveBlankText) {
        blink::Platform::current()->histogramEnumeration("WebFont.HadBlankText", m_status == HadBlankText ? 1 : 0, 2);
        m_status = Reported;
    }
}

static const char* supplementName()
{
    return "FontFaceSet";
}

PassRefPtr<FontFaceSet> FontFaceSet::from(Document& document)
{
    RefPtr<FontFaceSet> fonts = static_cast<FontFaceSet*>(SupplementType::from(document, supplementName()));
    if (!fonts) {
        fonts = FontFaceSet::create(document);
        SupplementType::provideTo(document, supplementName(), fonts);
    }

    return fonts.release();
}

void FontFaceSet::didLayout(Document& document)
{
    if (FontFaceSet* fonts = static_cast<FontFaceSet*>(SupplementType::from(document, supplementName())))
        fonts->didLayout();
}


} // namespace WebCore
