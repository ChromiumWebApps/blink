/*
 * Copyright (C) 2006, 2007, 2009, 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef DOMWindow_h
#define DOMWindow_h

#include "bindings/v8/Dictionary.h"
#include "bindings/v8/ScriptWrappable.h"
#include "core/events/EventTarget.h"
#include "core/frame/FrameDestructionObserver.h"
#include "heap/Handle.h"
#include "platform/LifecycleContext.h"
#include "platform/Supplementable.h"

#include "wtf/Forward.h"

namespace WebCore {
    class ApplicationCache;
    class BarProp;
    class CSSRuleList;
    class CSSStyleDeclaration;
    class Console;
    class DOMPoint;
    class DOMSelection;
    class DOMURL;
    class DOMWindowProperty;
    class Database;
    class DatabaseCallback;
    class Document;
    class DocumentInit;
    class DOMWindowEventQueue;
    class DOMWindowLifecycleNotifier;
    class Element;
    class EventListener;
    class EventQueue;
    class ExceptionState;
    class FloatRect;
    class LocalFrame;
    class History;
    class IDBFactory;
    class Location;
    class MediaQueryList;
    class MessageEvent;
    class Navigator;
    class Node;
    class Page;
    class PageConsole;
    class Performance;
    class PostMessageTimer;
    class RequestAnimationFrameCallback;
    class ScheduledAction;
    class Screen;
    class ScriptCallStack;
    class SecurityOrigin;
    class SerializedScriptValue;
    class Storage;
    class StyleMedia;
    class DOMWindowCSS;

    struct WindowFeatures;

    typedef Vector<RefPtr<MessagePort>, 1> MessagePortArray;

enum PageshowEventPersistence {
    PageshowEventNotPersisted = 0,
    PageshowEventPersisted = 1
};

    enum SetLocationLocking { LockHistoryBasedOnGestureState, LockHistoryAndBackForwardList };

    class DOMWindow FINAL : public RefCounted<DOMWindow>, public ScriptWrappable, public EventTargetWithInlineData, public FrameDestructionObserver, public Supplementable<DOMWindow>, public LifecycleContext<DOMWindow> {
        REFCOUNTED_EVENT_TARGET(DOMWindow);
    public:
        static PassRefPtr<Document> createDocument(const String& mimeType, const DocumentInit&, bool forceXHTML);
        static PassRefPtr<DOMWindow> create(LocalFrame& frame) { return adoptRef(new DOMWindow(frame)); }
        virtual ~DOMWindow();

        PassRefPtr<Document> installNewDocument(const String& mimeType, const DocumentInit&, bool forceXHTML = false);

        virtual const AtomicString& interfaceName() const OVERRIDE;
        virtual ExecutionContext* executionContext() const OVERRIDE;

        virtual DOMWindow* toDOMWindow() OVERRIDE;

        void registerProperty(DOMWindowProperty*);
        void unregisterProperty(DOMWindowProperty*);

        void reset();

        PassRefPtrWillBeRawPtr<MediaQueryList> matchMedia(const String&);

        unsigned pendingUnloadEventListeners() const;

        static FloatRect adjustWindowRect(LocalFrame&, const FloatRect& pendingChanges);

        bool allowPopUp(); // Call on first window, not target window.
        static bool allowPopUp(LocalFrame& firstFrame);
        static bool canShowModalDialog(const LocalFrame*);
        static bool canShowModalDialogNow(const LocalFrame*);

        // DOM Level 0

        Screen& screen() const;
        History& history() const;
        BarProp& locationbar() const;
        BarProp& menubar() const;
        BarProp& personalbar() const;
        BarProp& scrollbars() const;
        BarProp& statusbar() const;
        BarProp& toolbar() const;
        Navigator& navigator() const;
        Navigator& clientInformation() const { return navigator(); }

        Location& location() const;
        void setLocation(const String& location, DOMWindow* callingWindow, DOMWindow* enteredWindow,
            SetLocationLocking = LockHistoryBasedOnGestureState);

        DOMSelection* getSelection();

        Element* frameElement() const;

        void focus(ExecutionContext* = 0);
        void blur();
        void close(ExecutionContext* = 0);
        void print();
        void stop();

        PassRefPtr<DOMWindow> open(const String& urlString, const AtomicString& frameName, const String& windowFeaturesString,
            DOMWindow* callingWindow, DOMWindow* enteredWindow);

        typedef void (*PrepareDialogFunction)(DOMWindow*, void* context);
        void showModalDialog(const String& urlString, const String& dialogFeaturesString,
            DOMWindow* callingWindow, DOMWindow* enteredWindow, PrepareDialogFunction, void* functionContext);

        void alert(const String& message);
        bool confirm(const String& message);
        String prompt(const String& message, const String& defaultValue);

        bool find(const String&, bool caseSensitive, bool backwards, bool wrap, bool wholeWord, bool searchInFrames, bool showDialog) const;

        bool offscreenBuffering() const;

        int outerHeight() const;
        int outerWidth() const;
        int innerHeight() const;
        int innerWidth() const;
        int screenX() const;
        int screenY() const;
        int screenLeft() const { return screenX(); }
        int screenTop() const { return screenY(); }
        int scrollX() const;
        int scrollY() const;
        int pageXOffset() const { return scrollX(); }
        int pageYOffset() const { return scrollY(); }

        bool closed() const;

        unsigned length() const;

        const AtomicString& name() const;
        void setName(const AtomicString&);

        String status() const;
        void setStatus(const String&);
        String defaultStatus() const;
        void setDefaultStatus(const String&);

        // Self-referential attributes

        DOMWindow* self() const;
        DOMWindow* window() const { return self(); }
        DOMWindow* frames() const { return self(); }

        DOMWindow* opener() const;
        DOMWindow* parent() const;
        DOMWindow* top() const;

        // DOM Level 2 AbstractView Interface

        Document* document() const;

        // CSSOM View Module

        StyleMedia& styleMedia() const;

        // DOM Level 2 Style Interface

        PassRefPtr<CSSStyleDeclaration> getComputedStyle(Element*, const String& pseudoElt) const;

        // WebKit extensions

        PassRefPtrWillBeRawPtr<CSSRuleList> getMatchedCSSRules(Element*, const String& pseudoElt) const;
        double devicePixelRatio() const;

        PassRefPtr<DOMPoint> webkitConvertPointFromPageToNode(Node*, const DOMPoint*) const;
        PassRefPtr<DOMPoint> webkitConvertPointFromNodeToPage(Node*, const DOMPoint*) const;

        Console& console() const;
        PageConsole* pageConsole() const;

        void printErrorMessage(const String&);
        String crossDomainAccessErrorMessage(DOMWindow* callingWindow);
        String sanitizedCrossDomainAccessErrorMessage(DOMWindow* callingWindow);

        void postMessage(PassRefPtr<SerializedScriptValue> message, const MessagePortArray*, const String& targetOrigin, DOMWindow* source, ExceptionState&);
        void postMessageTimerFired(PassOwnPtr<PostMessageTimer>);
        void dispatchMessageEventWithOriginCheck(SecurityOrigin* intendedTargetOrigin, PassRefPtr<Event>, PassRefPtr<ScriptCallStack>);

        void scrollBy(int x, int y, const Dictionary& scrollOptions, ExceptionState&) const;
        void scrollTo(int x, int y, const Dictionary& scrollOptions, ExceptionState&) const;
        void scroll(int x, int y, const Dictionary& scrollOptions, ExceptionState& exceptionState) const { scrollTo(x, y, scrollOptions, exceptionState); }

        void moveBy(float x, float y) const;
        void moveTo(float x, float y) const;

        void resizeBy(float x, float y) const;
        void resizeTo(float width, float height) const;

        // WebKit animation extensions
        int requestAnimationFrame(PassOwnPtr<RequestAnimationFrameCallback>);
        int webkitRequestAnimationFrame(PassOwnPtr<RequestAnimationFrameCallback>);
        void cancelAnimationFrame(int id);

        DOMWindowCSS& css() const;

        // Events
        // EventTarget API
        virtual bool addEventListener(const AtomicString& eventType, PassRefPtr<EventListener>, bool useCapture) OVERRIDE;
        virtual bool removeEventListener(const AtomicString& eventType, EventListener*, bool useCapture) OVERRIDE;
        virtual void removeAllEventListeners() OVERRIDE;

        using EventTarget::dispatchEvent;
        bool dispatchEvent(PassRefPtr<Event> prpEvent, PassRefPtr<EventTarget> prpTarget);

        void dispatchLoadEvent();

        DEFINE_ATTRIBUTE_EVENT_LISTENER(animationend);
        DEFINE_ATTRIBUTE_EVENT_LISTENER(animationiteration);
        DEFINE_ATTRIBUTE_EVENT_LISTENER(animationstart);
        DEFINE_ATTRIBUTE_EVENT_LISTENER(search);
        DEFINE_ATTRIBUTE_EVENT_LISTENER(transitionend);
        DEFINE_ATTRIBUTE_EVENT_LISTENER(wheel);

        DEFINE_MAPPED_ATTRIBUTE_EVENT_LISTENER(webkitanimationstart, webkitAnimationStart);
        DEFINE_MAPPED_ATTRIBUTE_EVENT_LISTENER(webkitanimationiteration, webkitAnimationIteration);
        DEFINE_MAPPED_ATTRIBUTE_EVENT_LISTENER(webkitanimationend, webkitAnimationEnd);
        DEFINE_MAPPED_ATTRIBUTE_EVENT_LISTENER(webkittransitionend, webkitTransitionEnd);

        void captureEvents() { }
        void releaseEvents() { }

        void finishedLoading();

        // HTML 5 key/value storage
        Storage* sessionStorage(ExceptionState&) const;
        Storage* localStorage(ExceptionState&) const;
        Storage* optionalSessionStorage() const { return m_sessionStorage.get(); }
        Storage* optionalLocalStorage() const { return m_localStorage.get(); }

        ApplicationCache* applicationCache() const;
        ApplicationCache* optionalApplicationCache() const { return m_applicationCache.get(); }

        // This is the interface orientation in degrees. Some examples are:
        //  0 is straight up; -90 is when the device is rotated 90 clockwise;
        //  90 is when rotated counter clockwise.
        int orientation() const;

        DEFINE_ATTRIBUTE_EVENT_LISTENER(orientationchange);
        DEFINE_ATTRIBUTE_EVENT_LISTENER(touchstart);
        DEFINE_ATTRIBUTE_EVENT_LISTENER(touchmove);
        DEFINE_ATTRIBUTE_EVENT_LISTENER(touchend);
        DEFINE_ATTRIBUTE_EVENT_LISTENER(touchcancel);

        Performance& performance() const;

        // FIXME: When this DOMWindow is no longer the active DOMWindow (i.e.,
        // when its document is no longer the document that is displayed in its
        // frame), we would like to zero out m_frame to avoid being confused
        // by the document that is currently active in m_frame.
        bool isCurrentlyDisplayedInFrame() const;

        void willDetachDocumentFromFrame();
        DOMWindow* anonymousIndexedGetter(uint32_t);

        bool isInsecureScriptAccess(DOMWindow& callingWindow, const String& urlString);

        PassOwnPtr<LifecycleNotifier<DOMWindow> > createLifecycleNotifier();

        EventQueue* eventQueue() const;
        void enqueueWindowEvent(PassRefPtr<Event>);
        void enqueueDocumentEvent(PassRefPtr<Event>);
        void enqueuePageshowEvent(PageshowEventPersistence);
        void enqueueHashchangeEvent(const String& oldURL, const String& newURL);
        void enqueuePopstateEvent(PassRefPtr<SerializedScriptValue>);
        void dispatchWindowLoadEvent();
        void documentWasClosed();
        void statePopped(PassRefPtr<SerializedScriptValue>);

        // FIXME: This shouldn't be public once DOMWindow becomes ExecutionContext.
        void clearEventQueue();

    protected:
        DOMWindowLifecycleNotifier& lifecycleNotifier();

    private:
        explicit DOMWindow(LocalFrame&);

        Page* page();

        virtual void frameDestroyed() OVERRIDE;
        virtual void willDetachFrameHost() OVERRIDE;

        void clearDocument();
        void resetDOMWindowProperties();
        void willDestroyDocumentInFrame();

        RefPtr<Document> m_document;

        bool m_shouldPrintWhenFinishedLoading;

        HashSet<DOMWindowProperty*> m_properties;

        mutable RefPtrWillBePersistent<Screen> m_screen;
        mutable RefPtr<History> m_history;
        mutable RefPtr<BarProp> m_locationbar;
        mutable RefPtr<BarProp> m_menubar;
        mutable RefPtr<BarProp> m_personalbar;
        mutable RefPtr<BarProp> m_scrollbars;
        mutable RefPtr<BarProp> m_statusbar;
        mutable RefPtr<BarProp> m_toolbar;
        mutable RefPtr<Console> m_console;
        mutable RefPtr<Navigator> m_navigator;
        mutable RefPtr<Location> m_location;
        mutable RefPtr<StyleMedia> m_media;

        String m_status;
        String m_defaultStatus;

        mutable RefPtrWillBePersistent<Storage> m_sessionStorage;
        mutable RefPtrWillBePersistent<Storage> m_localStorage;
        mutable RefPtr<ApplicationCache> m_applicationCache;

        mutable RefPtr<Performance> m_performance;

        mutable RefPtr<DOMWindowCSS> m_css;

        RefPtr<DOMWindowEventQueue> m_eventQueue;
        RefPtr<SerializedScriptValue> m_pendingStateObject;
    };

    inline String DOMWindow::status() const
    {
        return m_status;
    }

    inline String DOMWindow::defaultStatus() const
    {
        return m_defaultStatus;
    }

} // namespace WebCore

#endif // DOMWindow_h
