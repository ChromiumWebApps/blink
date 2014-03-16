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

#ifndef FontFaceSet_h
#define FontFaceSet_h

#include "bindings/v8/ScriptPromise.h"
#include "core/css/FontFace.h"
#include "core/css/FontFaceSetForEachCallback.h"
#include "core/dom/ActiveDOMObject.h"
#include "core/events/EventListener.h"
#include "core/events/EventTarget.h"
#include "core/events/ThreadLocalEventNames.h"
#include "platform/AsyncMethodRunner.h"
#include "platform/RefCountedSupplement.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefCounted.h"
#include "wtf/Vector.h"

// Mac OS X 10.6 SDK defines check() macro that interfares with our check() method
#ifdef check
#undef check
#endif

namespace WebCore {

class CSSFontFace;
class CSSFontFaceSource;
class CSSFontSelector;
class Dictionary;
class Document;
class ExceptionState;
class Font;
class FontFaceCache;
class FontResource;
class FontsReadyPromiseResolver;
class ExecutionContext;

class FontFaceSet FINAL : public RefCountedSupplement<Document, FontFaceSet>, public ActiveDOMObject, public EventTargetWithInlineData {
    REFCOUNTED_EVENT_TARGET(FontFaceSet);
public:
    virtual ~FontFaceSet();

    DEFINE_ATTRIBUTE_EVENT_LISTENER(loading);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(loadingdone);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(loadingerror);

    bool check(const String& font, const String& text, ExceptionState&);
    ScriptPromise load(const String& font, const String& text, ExceptionState&);
    ScriptPromise ready();

    void add(FontFace*, ExceptionState&);
    void clear();
    bool remove(FontFace*, ExceptionState&);
    void forEach(PassOwnPtr<FontFaceSetForEachCallback>, ScriptValue& thisArg) const;
    void forEach(PassOwnPtr<FontFaceSetForEachCallback>) const;
    bool has(FontFace*, ExceptionState&) const;

    unsigned long size() const;
    AtomicString status() const;

    virtual ExecutionContext* executionContext() const OVERRIDE;
    virtual const AtomicString& interfaceName() const OVERRIDE;

    Document* document() const;

    void didLayout();
    void beginFontLoading(FontFace*);
    void fontLoaded(FontFace*);
    void loadError(FontFace*);

    // ActiveDOMObject
    virtual void suspend() OVERRIDE;
    virtual void resume() OVERRIDE;
    virtual void stop() OVERRIDE;

    static PassRefPtr<FontFaceSet> from(Document&);
    static void didLayout(Document&);

    void addFontFacesToFontFaceCache(FontFaceCache*, CSSFontSelector*);

private:
    typedef RefCountedSupplement<Document, FontFaceSet> SupplementType;

    static PassRefPtr<FontFaceSet> create(Document& document)
    {
        return adoptRef<FontFaceSet>(new FontFaceSet(document));
    }

    class FontLoadHistogram {
    public:
        enum Status { NoWebFonts, HadBlankText, DidNotHaveBlankText, Reported };
        FontLoadHistogram() : m_status(NoWebFonts), m_count(0), m_recorded(false) { }
        void incrementCount() { m_count++; }
        void updateStatus(FontFace*);
        void record();

    private:
        Status m_status;
        int m_count;
        bool m_recorded;
    };

    FontFaceSet(Document&);

    bool hasLoadedFonts() const { return !m_loadedFonts.isEmpty() || !m_failedFonts.isEmpty(); }

    bool inActiveDocumentContext() const;
    void forEachInternal(PassOwnPtr<FontFaceSetForEachCallback>, ScriptValue* thisArg) const;
    void addToLoadingFonts(PassRefPtr<FontFace>);
    void removeFromLoadingFonts(PassRefPtr<FontFace>);
    void fireLoadingEvent();
    void fireDoneEventIfPossible();
    bool resolveFontStyle(const String&, Font&);
    void handlePendingEventsAndPromisesSoon();
    void handlePendingEventsAndPromises();
    const ListHashSet<RefPtr<FontFace> >& cssConnectedFontFaceList() const;
    bool isCSSConnectedFontFace(FontFace*) const;

    HashSet<RefPtr<FontFace> > m_loadingFonts;
    bool m_shouldFireLoadingEvent;
    Vector<OwnPtr<FontsReadyPromiseResolver> > m_readyResolvers;
    FontFaceArray m_loadedFonts;
    FontFaceArray m_failedFonts;
    ListHashSet<RefPtr<FontFace> > m_nonCSSConnectedFaces;

    AsyncMethodRunner<FontFaceSet> m_asyncRunner;

    FontLoadHistogram m_histogram;
};

} // namespace WebCore

#endif // FontFaceSet_h
