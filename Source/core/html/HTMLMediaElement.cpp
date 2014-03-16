/*
 * Copyright (C) 2007, 2008, 2009, 2010, 2011, 2012, 2013 Apple Inc. All rights reserved.
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

#include "config.h"
#include "core/html/HTMLMediaElement.h"

#include <limits>
#include "HTMLNames.h"
#include "RuntimeEnabledFeatures.h"
#include "bindings/v8/ExceptionState.h"
#include "bindings/v8/ExceptionStatePlaceholder.h"
#include "bindings/v8/ScriptController.h"
#include "bindings/v8/ScriptEventListener.h"
#include "core/css/MediaList.h"
#include "core/dom/Attribute.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/FullscreenElementStack.h"
#include "core/dom/shadow/ShadowRoot.h"
#include "core/events/Event.h"
#include "core/events/ThreadLocalEventNames.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Settings.h"
#include "core/frame/UseCounter.h"
#include "core/frame/csp/ContentSecurityPolicy.h"
#include "core/html/HTMLMediaSource.h"
#include "core/html/HTMLSourceElement.h"
#include "core/html/HTMLTrackElement.h"
#include "core/html/MediaController.h"
#include "core/html/MediaError.h"
#include "core/html/MediaFragmentURIParser.h"
#include "core/html/TimeRanges.h"
#include "core/html/shadow/MediaControls.h"
#include "core/html/track/InbandTextTrack.h"
#include "core/html/track/TextTrackCueList.h"
#include "core/html/track/TextTrackList.h"
#include "core/loader/FrameLoader.h"
#include "core/rendering/RenderVideo.h"
#include "core/rendering/RenderView.h"
#include "core/rendering/compositing/RenderLayerCompositor.h"
#include "platform/ContentType.h"
#include "platform/Language.h"
#include "platform/Logging.h"
#include "platform/MIMETypeFromURL.h"
#include "platform/MIMETypeRegistry.h"
#include "platform/NotImplemented.h"
#include "platform/UserGestureIndicator.h"
#include "platform/graphics/GraphicsLayer.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "public/platform/Platform.h"
#include "public/platform/WebContentDecryptionModule.h"
#include "public/platform/WebInbandTextTrack.h"
#include "wtf/CurrentTime.h"
#include "wtf/MathExtras.h"
#include "wtf/NonCopyingSort.h"
#include "wtf/Uint8Array.h"
#include "wtf/text/CString.h"

#if ENABLE(WEB_AUDIO)
#include "platform/audio/AudioSourceProvider.h"
#include "modules/webaudio/MediaElementAudioSourceNode.h"
#endif

using namespace std;
using blink::WebInbandTextTrack;
using blink::WebMimeRegistry;

namespace WebCore {

#if !LOG_DISABLED
static String urlForLoggingMedia(const KURL& url)
{
    static const unsigned maximumURLLengthForLogging = 128;

    if (url.string().length() < maximumURLLengthForLogging)
        return url.string();
    return url.string().substring(0, maximumURLLengthForLogging) + "...";
}

static const char* boolString(bool val)
{
    return val ? "true" : "false";
}
#endif

#ifndef LOG_MEDIA_EVENTS
// Default to not logging events because so many are generated they can overwhelm the rest of
// the logging.
#define LOG_MEDIA_EVENTS 0
#endif

#ifndef LOG_CACHED_TIME_WARNINGS
// Default to not logging warnings about excessive drift in the cached media time because it adds a
// fair amount of overhead and logging.
#define LOG_CACHED_TIME_WARNINGS 0
#endif

// URL protocol used to signal that the media source API is being used.
static const char mediaSourceBlobProtocol[] = "blob";

using namespace HTMLNames;
using namespace std;

typedef HashMap<Document*, HashSet<HTMLMediaElement*> > DocumentElementSetMap;
static DocumentElementSetMap& documentToElementSetMap()
{
    DEFINE_STATIC_LOCAL(DocumentElementSetMap, map, ());
    return map;
}

static void addElementToDocumentMap(HTMLMediaElement* element, Document* document)
{
    DocumentElementSetMap& map = documentToElementSetMap();
    HashSet<HTMLMediaElement*> set = map.take(document);
    set.add(element);
    map.add(document, set);
}

static void removeElementFromDocumentMap(HTMLMediaElement* element, Document* document)
{
    DocumentElementSetMap& map = documentToElementSetMap();
    HashSet<HTMLMediaElement*> set = map.take(document);
    set.remove(element);
    if (!set.isEmpty())
        map.add(document, set);
}

class TrackDisplayUpdateScope {
public:
    TrackDisplayUpdateScope(HTMLMediaElement* mediaElement)
    {
        m_mediaElement = mediaElement;
        m_mediaElement->beginIgnoringTrackDisplayUpdateRequests();
    }
    ~TrackDisplayUpdateScope()
    {
        ASSERT(m_mediaElement);
        m_mediaElement->endIgnoringTrackDisplayUpdateRequests();
    }

private:
    HTMLMediaElement* m_mediaElement;
};

static bool canLoadURL(const KURL& url, const ContentType& contentType, const String& keySystem)
{
    DEFINE_STATIC_LOCAL(const String, codecs, ("codecs"));

    String contentMIMEType = contentType.type().lower();
    String contentTypeCodecs = contentType.parameter(codecs);

    // If the MIME type is missing or is not meaningful, try to figure it out from the URL.
    if (contentMIMEType.isEmpty() || contentMIMEType == "application/octet-stream" || contentMIMEType == "text/plain") {
        if (url.protocolIsData())
            contentMIMEType = mimeTypeFromDataURL(url.string());
    }

    // If no MIME type is specified, always attempt to load.
    if (contentMIMEType.isEmpty())
        return true;

    // 4.8.10.3 MIME types - In the absence of a specification to the contrary, the MIME type "application/octet-stream"
    // when used with parameters, e.g. "application/octet-stream;codecs=theora", is a type that the user agent knows
    // it cannot render.
    if (contentMIMEType != "application/octet-stream" || contentTypeCodecs.isEmpty()) {
        WebMimeRegistry::SupportsType supported = blink::Platform::current()->mimeRegistry()->supportsMediaMIMEType(contentMIMEType, contentTypeCodecs, keySystem.lower());
        return supported > WebMimeRegistry::IsNotSupported;
    }

    return false;
}

WebMimeRegistry::SupportsType HTMLMediaElement::supportsType(const ContentType& contentType, const String& keySystem)
{
    DEFINE_STATIC_LOCAL(const String, codecs, ("codecs"));

    if (!RuntimeEnabledFeatures::mediaEnabled())
        return WebMimeRegistry::IsNotSupported;

    String type = contentType.type().lower();
    // The codecs string is not lower-cased because MP4 values are case sensitive
    // per http://tools.ietf.org/html/rfc4281#page-7.
    String typeCodecs = contentType.parameter(codecs);
    String system = keySystem.lower();

    if (type.isEmpty())
        return WebMimeRegistry::IsNotSupported;

    // 4.8.10.3 MIME types - The canPlayType(type) method must return the empty string if type is a type that the
    // user agent knows it cannot render or is the type "application/octet-stream"
    if (type == "application/octet-stream")
        return WebMimeRegistry::IsNotSupported;

    return blink::Platform::current()->mimeRegistry()->supportsMediaMIMEType(type, typeCodecs, system);
}

URLRegistry* HTMLMediaElement::s_mediaStreamRegistry = 0;

void HTMLMediaElement::setMediaStreamRegistry(URLRegistry* registry)
{
    ASSERT(!s_mediaStreamRegistry);
    s_mediaStreamRegistry = registry;
}

bool HTMLMediaElement::isMediaStreamURL(const String& url)
{
    return s_mediaStreamRegistry ? s_mediaStreamRegistry->contains(url) : false;
}

HTMLMediaElement::HTMLMediaElement(const QualifiedName& tagName, Document& document)
    : HTMLElement(tagName, document)
    , ActiveDOMObject(&document)
    , m_loadTimer(this, &HTMLMediaElement::loadTimerFired)
    , m_progressEventTimer(this, &HTMLMediaElement::progressEventTimerFired)
    , m_playbackProgressTimer(this, &HTMLMediaElement::playbackProgressTimerFired)
    , m_playedTimeRanges()
    , m_asyncEventQueue(GenericEventQueue::create(this))
    , m_playbackRate(1.0f)
    , m_defaultPlaybackRate(1.0f)
    , m_networkState(NETWORK_EMPTY)
    , m_readyState(HAVE_NOTHING)
    , m_readyStateMaximum(HAVE_NOTHING)
    , m_volume(1.0f)
    , m_lastSeekTime(0)
    , m_previousProgressTime(numeric_limits<double>::max())
    , m_duration(numeric_limits<double>::quiet_NaN())
    , m_lastTimeUpdateEventWallTime(0)
    , m_lastTimeUpdateEventMovieTime(numeric_limits<double>::max())
    , m_loadState(WaitingForSource)
    , m_webLayer(0)
    , m_opaque(false)
    , m_preload(MediaPlayer::Auto)
    , m_displayMode(Unknown)
    , m_cachedTime(MediaPlayer::invalidTime())
    , m_cachedTimeWallClockUpdateTime(0)
    , m_minimumWallClockTimeToCacheMediaTime(0)
    , m_fragmentStartTime(MediaPlayer::invalidTime())
    , m_fragmentEndTime(MediaPlayer::invalidTime())
    , m_pendingActionFlags(0)
    , m_userGestureRequiredForPlay(false)
    , m_playing(false)
    , m_shouldDelayLoadEvent(false)
    , m_haveFiredLoadedData(false)
    , m_active(true)
    , m_autoplaying(true)
    , m_muted(false)
    , m_paused(true)
    , m_seeking(false)
    , m_sentStalledEvent(false)
    , m_sentEndEvent(false)
    , m_pausedInternal(false)
    , m_closedCaptionsVisible(false)
    , m_completelyLoaded(false)
    , m_havePreparedToPlay(false)
    , m_tracksAreReady(true)
    , m_haveVisibleTextTrack(false)
    , m_processingPreferenceChange(false)
    , m_lastTextTrackUpdateTime(-1)
    , m_textTracks(nullptr)
    , m_ignoreTrackDisplayUpdate(0)
#if ENABLE(WEB_AUDIO)
    , m_audioSourceNode(0)
#endif
{
    ASSERT(RuntimeEnabledFeatures::mediaEnabled());

    WTF_LOG(Media, "HTMLMediaElement::HTMLMediaElement");
    ScriptWrappable::init(this);

    if (document.settings() && document.settings()->mediaPlaybackRequiresUserGesture())
        m_userGestureRequiredForPlay = true;

    // We must always have a ShadowRoot so children like <source> will not render
    // as they never have an insertion point.
    ensureUserAgentShadowRoot();
    setHasCustomStyleCallbacks();
    addElementToDocumentMap(this, &document);
}

HTMLMediaElement::~HTMLMediaElement()
{
    WTF_LOG(Media, "HTMLMediaElement::~HTMLMediaElement");

    m_asyncEventQueue->close();

    setShouldDelayLoadEvent(false);

    if (m_textTracks)
        m_textTracks->clearOwner();

    if (m_mediaController) {
        m_mediaController->removeMediaElement(this);
        m_mediaController = nullptr;
    }

    closeMediaSource();

    removeElementFromDocumentMap(this, &document());

    // Destroying the player may cause a resource load to be canceled,
    // which could result in userCancelledLoad() being called back.
    // Setting m_completelyLoaded ensures that such a call will not cause
    // us to dispatch an abort event, which would result in a crash.
    // See http://crbug.com/233654 for more details.
    m_completelyLoaded = true;

    // Destroying the player may cause a resource load to be canceled,
    // which could result in Document::dispatchWindowLoadEvent() being
    // called via ResourceFetch::didLoadResource() then
    // FrameLoader::loadDone(). To prevent load event dispatching during
    // object destruction, we use Document::incrementLoadEventDelayCount().
    // See http://crbug.com/275223 for more details.
    document().incrementLoadEventDelayCount();

    clearMediaPlayerAndAudioSourceProviderClient();

    document().decrementLoadEventDelayCount();
}

void HTMLMediaElement::didMoveToNewDocument(Document& oldDocument)
{
    WTF_LOG(Media, "HTMLMediaElement::didMoveToNewDocument");

    if (m_shouldDelayLoadEvent) {
        document().incrementLoadEventDelayCount();
        // Note: Keeping the load event delay count increment on oldDocument that was added
        // when m_shouldDelayLoadEvent was set so that destruction of m_player can not
        // cause load event dispatching in oldDocument.
    } else {
        // Incrementing the load event delay count so that destruction of m_player can not
        // cause load event dispatching in oldDocument.
        oldDocument.incrementLoadEventDelayCount();
    }

    removeElementFromDocumentMap(this, &oldDocument);
    addElementToDocumentMap(this, &document());

    // FIXME: This is a temporary fix to prevent this object from causing the
    // MediaPlayer to dereference LocalFrame and FrameLoader pointers from the
    // previous document. A proper fix would provide a mechanism to allow this
    // object to refresh the MediaPlayer's LocalFrame and FrameLoader references on
    // document changes so that playback can be resumed properly.
    userCancelledLoad();

    // Decrement the load event delay count on oldDocument now that m_player has been destroyed
    // and there is no risk of dispatching a load event from within the destructor.
    oldDocument.decrementLoadEventDelayCount();

    ActiveDOMObject::didMoveToNewExecutionContext(&document());
    HTMLElement::didMoveToNewDocument(oldDocument);
}

bool HTMLMediaElement::hasCustomFocusLogic() const
{
    return true;
}

bool HTMLMediaElement::supportsFocus() const
{
    if (ownerDocument()->isMediaDocument())
        return false;

    // If no controls specified, we should still be able to focus the element if it has tabIndex.
    return controls() || HTMLElement::supportsFocus();
}

bool HTMLMediaElement::isMouseFocusable() const
{
    return false;
}

void HTMLMediaElement::parseAttribute(const QualifiedName& name, const AtomicString& value)
{
    if (name == srcAttr) {
        // Trigger a reload, as long as the 'src' attribute is present.
        if (!value.isNull()) {
            clearMediaPlayer(LoadMediaResource);
            scheduleDelayedAction(LoadMediaResource);
        }
    } else if (name == controlsAttr)
        configureMediaControls();
    else if (name == preloadAttr) {
        if (equalIgnoringCase(value, "none"))
            m_preload = MediaPlayer::None;
        else if (equalIgnoringCase(value, "metadata"))
            m_preload = MediaPlayer::MetaData;
        else {
            // The spec does not define an "invalid value default" but "auto" is suggested as the
            // "missing value default", so use it for everything except "none" and "metadata"
            m_preload = MediaPlayer::Auto;
        }

        // The attribute must be ignored if the autoplay attribute is present
        if (!autoplay() && m_player)
            m_player->setPreload(m_preload);

    } else if (name == mediagroupAttr)
        setMediaGroup(value);
    else if (name == onbeforeloadAttr)
        setAttributeEventListener(EventTypeNames::beforeload, createAttributeEventListener(this, name, value));
    else
        HTMLElement::parseAttribute(name, value);
}

void HTMLMediaElement::finishParsingChildren()
{
    HTMLElement::finishParsingChildren();

    if (!RuntimeEnabledFeatures::videoTrackEnabled())
        return;

    if (Traversal<HTMLTrackElement>::firstChild(*this))
        scheduleDelayedAction(LoadTextTrackResource);
}

bool HTMLMediaElement::rendererIsNeeded(const RenderStyle& style)
{
    return controls() ? HTMLElement::rendererIsNeeded(style) : false;
}

RenderObject* HTMLMediaElement::createRenderer(RenderStyle*)
{
    return new RenderMedia(this);
}

Node::InsertionNotificationRequest HTMLMediaElement::insertedInto(ContainerNode* insertionPoint)
{
    WTF_LOG(Media, "HTMLMediaElement::insertedInto");

    HTMLElement::insertedInto(insertionPoint);
    if (insertionPoint->inDocument()) {
        m_active = true;

        if (!getAttribute(srcAttr).isEmpty() && m_networkState == NETWORK_EMPTY)
            scheduleDelayedAction(LoadMediaResource);
    }

    configureMediaControls();
    return InsertionDone;
}

void HTMLMediaElement::removedFrom(ContainerNode* insertionPoint)
{
    WTF_LOG(Media, "HTMLMediaElement::removedFrom");

    m_active = false;
    if (insertionPoint->inDocument() && insertionPoint->document().isActive()) {
        configureMediaControls();
        if (m_networkState > NETWORK_EMPTY)
            pause();
    }

    HTMLElement::removedFrom(insertionPoint);
}

void HTMLMediaElement::attach(const AttachContext& context)
{
    HTMLElement::attach(context);

    if (renderer())
        renderer()->updateFromElement();
}

void HTMLMediaElement::didRecalcStyle(StyleRecalcChange)
{
    if (renderer())
        renderer()->updateFromElement();
}

void HTMLMediaElement::scheduleDelayedAction(DelayedActionType actionType)
{
    WTF_LOG(Media, "HTMLMediaElement::scheduleDelayedAction");

    if ((actionType & LoadMediaResource) && !(m_pendingActionFlags & LoadMediaResource)) {
        prepareForLoad();
        m_pendingActionFlags |= LoadMediaResource;
    }

    if (RuntimeEnabledFeatures::videoTrackEnabled() && (actionType & LoadTextTrackResource))
        m_pendingActionFlags |= LoadTextTrackResource;

    if (!m_loadTimer.isActive())
        m_loadTimer.startOneShot(0, FROM_HERE);
}

void HTMLMediaElement::scheduleNextSourceChild()
{
    // Schedule the timer to try the next <source> element WITHOUT resetting state ala prepareForLoad.
    m_pendingActionFlags |= LoadMediaResource;
    m_loadTimer.startOneShot(0, FROM_HERE);
}

void HTMLMediaElement::scheduleEvent(const AtomicString& eventName)
{
    scheduleEvent(Event::createCancelable(eventName));
}

void HTMLMediaElement::scheduleEvent(PassRefPtr<Event> event)
{
#if LOG_MEDIA_EVENTS
    WTF_LOG(Media, "HTMLMediaElement::scheduleEvent - scheduling '%s'", event->type().ascii().data());
#endif
    m_asyncEventQueue->enqueueEvent(event);
}

void HTMLMediaElement::loadTimerFired(Timer<HTMLMediaElement>*)
{
    RefPtr<HTMLMediaElement> protect(this); // loadNextSourceChild may fire 'beforeload', which can make arbitrary DOM mutations.

    if (RuntimeEnabledFeatures::videoTrackEnabled() && (m_pendingActionFlags & LoadTextTrackResource))
        configureTextTracks();

    if (m_pendingActionFlags & LoadMediaResource) {
        if (m_loadState == LoadingFromSourceElement)
            loadNextSourceChild();
        else
            loadInternal();
    }

    m_pendingActionFlags = 0;
}

PassRefPtr<MediaError> HTMLMediaElement::error() const
{
    return m_error;
}

void HTMLMediaElement::setSrc(const AtomicString& url)
{
    setAttribute(srcAttr, url);
}

HTMLMediaElement::NetworkState HTMLMediaElement::networkState() const
{
    return m_networkState;
}

String HTMLMediaElement::canPlayType(const String& mimeType, const String& keySystem) const
{
    if (!keySystem.isNull())
        UseCounter::count(document(), UseCounter::CanPlayTypeKeySystem);

    WebMimeRegistry::SupportsType support = supportsType(ContentType(mimeType), keySystem);
    String canPlay;

    // 4.8.10.3
    switch (support)
    {
        case WebMimeRegistry::IsNotSupported:
            canPlay = emptyString();
            break;
        case WebMimeRegistry::MayBeSupported:
            canPlay = "maybe";
            break;
        case WebMimeRegistry::IsSupported:
            canPlay = "probably";
            break;
    }

    WTF_LOG(Media, "HTMLMediaElement::canPlayType(%s, %s) -> %s", mimeType.utf8().data(), keySystem.utf8().data(), canPlay.utf8().data());

    return canPlay;
}

void HTMLMediaElement::load()
{
    RefPtr<HTMLMediaElement> protect(this); // loadInternal may result in a 'beforeload' event, which can make arbitrary DOM mutations.

    WTF_LOG(Media, "HTMLMediaElement::load()");

    if (UserGestureIndicator::processingUserGesture())
        m_userGestureRequiredForPlay = false;

    prepareForLoad();
    loadInternal();
    prepareToPlay();
}

void HTMLMediaElement::prepareForLoad()
{
    WTF_LOG(Media, "HTMLMediaElement::prepareForLoad");

    // Perform the cleanup required for the resource load algorithm to run.
    stopPeriodicTimers();
    m_loadTimer.stop();
    m_sentEndEvent = false;
    m_sentStalledEvent = false;
    m_haveFiredLoadedData = false;
    m_completelyLoaded = false;
    m_havePreparedToPlay = false;
    m_displayMode = Unknown;

    // 1 - Abort any already-running instance of the resource selection algorithm for this element.
    m_loadState = WaitingForSource;
    m_currentSourceNode = nullptr;

    // 2 - If there are any tasks from the media element's media element event task source in
    // one of the task queues, then remove those tasks.
    cancelPendingEventsAndCallbacks();

    // 3 - If the media element's networkState is set to NETWORK_LOADING or NETWORK_IDLE, queue
    // a task to fire a simple event named abort at the media element.
    if (m_networkState == NETWORK_LOADING || m_networkState == NETWORK_IDLE)
        scheduleEvent(EventTypeNames::abort);

    closeMediaSource();

    createMediaPlayer();

    // 4 - If the media element's networkState is not set to NETWORK_EMPTY, then run these substeps
    if (m_networkState != NETWORK_EMPTY) {
        // 4.1 - Queue a task to fire a simple event named emptied at the media element.
        scheduleEvent(EventTypeNames::emptied);

        // 4.2 - If a fetching process is in progress for the media element, the user agent should stop it.
        m_networkState = NETWORK_EMPTY;

        // 4.3 - Forget the media element's media-resource-specific tracks.
        forgetResourceSpecificTracks();

        // 4.4 - If readyState is not set to HAVE_NOTHING, then set it to that state.
        m_readyState = HAVE_NOTHING;
        m_readyStateMaximum = HAVE_NOTHING;

        // 4.5 - If the paused attribute is false, then set it to true.
        m_paused = true;

        // 4.6 - If seeking is true, set it to false.
        m_seeking = false;

        // 4.7 - Set the current playback position to 0.
        //       Set the official playback position to 0.
        //       If this changed the official playback position, then queue a task to fire a simple event named timeupdate at the media element.
        // FIXME: Add support for firing this event.

        // 4.8 - Set the initial playback position to 0.
        // FIXME: Make this less subtle. The position only becomes 0 because of the createMediaPlayer() call
        // above.
        refreshCachedTime();
        invalidateCachedTime();

        // 4.9 - Set the timeline offset to Not-a-Number (NaN).
        // 4.10 - Update the duration attribute to Not-a-Number (NaN).


        updateMediaController();
        if (RuntimeEnabledFeatures::videoTrackEnabled())
            updateActiveTextTrackCues(0);
    }

    // 5 - Set the playbackRate attribute to the value of the defaultPlaybackRate attribute.
    setPlaybackRate(defaultPlaybackRate());

    // 6 - Set the error attribute to null and the autoplaying flag to true.
    m_error = nullptr;
    m_autoplaying = true;

    // 7 - Invoke the media element's resource selection algorithm.

    // 8 - Note: Playback of any previously playing media resource for this element stops.

    // The resource selection algorithm
    // 1 - Set the networkState to NETWORK_NO_SOURCE
    m_networkState = NETWORK_NO_SOURCE;

    // 2 - Asynchronously await a stable state.

    m_playedTimeRanges = TimeRanges::create();

    // FIXME: Investigate whether these can be moved into m_networkState != NETWORK_EMPTY block above
    // so they are closer to the relevant spec steps.
    m_lastSeekTime = 0;
    m_duration = numeric_limits<double>::quiet_NaN();

    // The spec doesn't say to block the load event until we actually run the asynchronous section
    // algorithm, but do it now because we won't start that until after the timer fires and the
    // event may have already fired by then.
    setShouldDelayLoadEvent(true);

    configureMediaControls();
}

void HTMLMediaElement::loadInternal()
{
    // Some of the code paths below this function dispatch the BeforeLoad event. This ASSERT helps
    // us catch those bugs more quickly without needing all the branches to align to actually
    // trigger the event.
    ASSERT(!NoEventDispatchAssertion::isEventDispatchForbidden());

    // HTMLMediaElement::textTracksAreReady will need "... the text tracks whose mode was not in the
    // disabled state when the element's resource selection algorithm last started".
    if (RuntimeEnabledFeatures::videoTrackEnabled()) {
        m_textTracksWhenResourceSelectionBegan.clear();
        if (m_textTracks) {
            for (unsigned i = 0; i < m_textTracks->length(); ++i) {
                TextTrack* track = m_textTracks->item(i);
                if (track->mode() != TextTrack::disabledKeyword())
                    m_textTracksWhenResourceSelectionBegan.append(track);
            }
        }
    }

    selectMediaResource();
}

void HTMLMediaElement::selectMediaResource()
{
    WTF_LOG(Media, "HTMLMediaElement::selectMediaResource");

    enum Mode { attribute, children };

    // 3 - If the media element has a src attribute, then let mode be attribute.
    Mode mode = attribute;
    if (!fastHasAttribute(srcAttr)) {
        // Otherwise, if the media element does not have a src attribute but has a source
        // element child, then let mode be children and let candidate be the first such
        // source element child in tree order.
        if (HTMLSourceElement* element = Traversal<HTMLSourceElement>::firstChild(*this)) {
            mode = children;
            m_nextChildNodeToConsider = element;
            m_currentSourceNode = nullptr;
        } else {
            // Otherwise the media element has neither a src attribute nor a source element
            // child: set the networkState to NETWORK_EMPTY, and abort these steps; the
            // synchronous section ends.
            m_loadState = WaitingForSource;
            setShouldDelayLoadEvent(false);
            m_networkState = NETWORK_EMPTY;

            WTF_LOG(Media, "HTMLMediaElement::selectMediaResource, nothing to load");
            return;
        }
    }

    // 4 - Set the media element's delaying-the-load-event flag to true (this delays the load event),
    // and set its networkState to NETWORK_LOADING.
    setShouldDelayLoadEvent(true);
    m_networkState = NETWORK_LOADING;

    // 5 - Queue a task to fire a simple event named loadstart at the media element.
    scheduleEvent(EventTypeNames::loadstart);

    // 6 - If mode is attribute, then run these substeps
    if (mode == attribute) {
        m_loadState = LoadingFromSrcAttr;

        // If the src attribute's value is the empty string ... jump down to the failed step below
        KURL mediaURL = getNonEmptyURLAttribute(srcAttr);
        if (mediaURL.isEmpty()) {
            mediaLoadingFailed(MediaPlayer::FormatError);
            WTF_LOG(Media, "HTMLMediaElement::selectMediaResource, empty 'src'");
            return;
        }

        if (!isSafeToLoadURL(mediaURL, Complain) || !dispatchBeforeLoadEvent(mediaURL.string())) {
            mediaLoadingFailed(MediaPlayer::FormatError);
            return;
        }

        // No type or key system information is available when the url comes
        // from the 'src' attribute so MediaPlayer
        // will have to pick a media engine based on the file extension.
        ContentType contentType((String()));
        loadResource(mediaURL, contentType, String());
        WTF_LOG(Media, "HTMLMediaElement::selectMediaResource, using 'src' attribute url");
        return;
    }

    // Otherwise, the source elements will be used
    loadNextSourceChild();
}

void HTMLMediaElement::loadNextSourceChild()
{
    ContentType contentType((String()));
    String keySystem;
    KURL mediaURL = selectNextSourceChild(&contentType, &keySystem, Complain);
    if (!mediaURL.isValid()) {
        waitForSourceChange();
        return;
    }

    // Recreate the media player for the new url
    createMediaPlayer();

    m_loadState = LoadingFromSourceElement;
    loadResource(mediaURL, contentType, keySystem);
}

void HTMLMediaElement::loadResource(const KURL& url, ContentType& contentType, const String& keySystem)
{
    ASSERT(isSafeToLoadURL(url, Complain));

    WTF_LOG(Media, "HTMLMediaElement::loadResource(%s, %s, %s)", urlForLoggingMedia(url).utf8().data(), contentType.raw().utf8().data(), keySystem.utf8().data());

    LocalFrame* frame = document().frame();
    if (!frame) {
        mediaLoadingFailed(MediaPlayer::FormatError);
        return;
    }

    // The resource fetch algorithm
    m_networkState = NETWORK_LOADING;

    // Set m_currentSrc *before* changing to the cache url, the fact that we are loading from the app
    // cache is an internal detail not exposed through the media element API.
    m_currentSrc = url;

    WTF_LOG(Media, "HTMLMediaElement::loadResource - m_currentSrc -> %s", urlForLoggingMedia(m_currentSrc).utf8().data());

    blink::WebMediaPlayer::LoadType loadType = blink::WebMediaPlayer::LoadTypeURL;

    startProgressEventTimer();

    // Reset display mode to force a recalculation of what to show because we are resetting the player.
    setDisplayMode(Unknown);

    if (!autoplay())
        m_player->setPreload(m_preload);

    if (fastHasAttribute(mutedAttr))
        m_muted = true;
    updateVolume();

    ASSERT(!m_mediaSource);

    bool attemptLoad = true;

    if (url.protocolIs(mediaSourceBlobProtocol)) {
        if (isMediaStreamURL(url.string())) {
            loadType = blink::WebMediaPlayer::LoadTypeMediaStream;
            m_userGestureRequiredForPlay = false;
        } else {
            m_mediaSource = HTMLMediaSource::lookup(url.string());

            if (m_mediaSource) {
                loadType = blink::WebMediaPlayer::LoadTypeMediaSource;

                if (!m_mediaSource->attachToElement(this)) {
                    // Forget our reference to the MediaSource, so we leave it alone
                    // while processing remainder of load failure.
                    m_mediaSource = nullptr;
                    attemptLoad = false;
                }
            }
        }
    }

    if (attemptLoad && canLoadURL(url, contentType, keySystem)) {
        m_player->load(loadType, url);
    } else {
        mediaLoadingFailed(MediaPlayer::FormatError);
    }

    // If there is no poster to display, allow the media engine to render video frames as soon as
    // they are available.
    updateDisplayState();

    if (renderer())
        renderer()->updateFromElement();
}

static bool trackIndexCompare(TextTrack* a,
                              TextTrack* b)
{
    return a->trackIndex() - b->trackIndex() < 0;
}

static bool eventTimeCueCompare(const std::pair<double, TextTrackCue*>& a,
                                const std::pair<double, TextTrackCue*>& b)
{
    // 12 - Sort the tasks in events in ascending time order (tasks with earlier
    // times first).
    if (a.first != b.first)
        return a.first - b.first < 0;

    // If the cues belong to different text tracks, it doesn't make sense to
    // compare the two tracks by the relative cue order, so return the relative
    // track order.
    if (a.second->track() != b.second->track())
        return trackIndexCompare(a.second->track(), b.second->track());

    // 12 - Further sort tasks in events that have the same time by the
    // relative text track cue order of the text track cues associated
    // with these tasks.
    return a.second->cueIndex() - b.second->cueIndex() < 0;
}


void HTMLMediaElement::updateActiveTextTrackCues(double movieTime)
{
    // 4.8.10.8 Playing the media resource

    //  If the current playback position changes while the steps are running,
    //  then the user agent must wait for the steps to complete, and then must
    //  immediately rerun the steps.
    if (ignoreTrackDisplayUpdateRequests())
        return;

    // 1 - Let current cues be a list of cues, initialized to contain all the
    // cues of all the hidden, showing, or showing by default text tracks of the
    // media element (not the disabled ones) whose start times are less than or
    // equal to the current playback position and whose end times are greater
    // than the current playback position.
    CueList currentCues;

    // The user agent must synchronously unset [the text track cue active] flag
    // whenever ... the media element's readyState is changed back to HAVE_NOTHING.
    if (m_readyState != HAVE_NOTHING && m_player)
        currentCues = m_cueTree.allOverlaps(m_cueTree.createInterval(movieTime, movieTime));

    CueList previousCues;
    CueList missedCues;

    // 2 - Let other cues be a list of cues, initialized to contain all the cues
    // of hidden, showing, and showing by default text tracks of the media
    // element that are not present in current cues.
    previousCues = m_currentlyActiveCues;

    // 3 - Let last time be the current playback position at the time this
    // algorithm was last run for this media element, if this is not the first
    // time it has run.
    double lastTime = m_lastTextTrackUpdateTime;

    // 4 - If the current playback position has, since the last time this
    // algorithm was run, only changed through its usual monotonic increase
    // during normal playback, then let missed cues be the list of cues in other
    // cues whose start times are greater than or equal to last time and whose
    // end times are less than or equal to the current playback position.
    // Otherwise, let missed cues be an empty list.
    if (lastTime >= 0 && m_lastSeekTime < movieTime) {
        CueList potentiallySkippedCues =
            m_cueTree.allOverlaps(m_cueTree.createInterval(lastTime, movieTime));

        for (size_t i = 0; i < potentiallySkippedCues.size(); ++i) {
            double cueStartTime = potentiallySkippedCues[i].low();
            double cueEndTime = potentiallySkippedCues[i].high();

            // Consider cues that may have been missed since the last seek time.
            if (cueStartTime > max(m_lastSeekTime, lastTime) && cueEndTime < movieTime)
                missedCues.append(potentiallySkippedCues[i]);
        }
    }

    m_lastTextTrackUpdateTime = movieTime;

    // 5 - If the time was reached through the usual monotonic increase of the
    // current playback position during normal playback, and if the user agent
    // has not fired a timeupdate event at the element in the past 15 to 250ms
    // and is not still running event handlers for such an event, then the user
    // agent must queue a task to fire a simple event named timeupdate at the
    // element. (In the other cases, such as explicit seeks, relevant events get
    // fired as part of the overall process of changing the current playback
    // position.)
    if (!m_seeking && m_lastSeekTime <= lastTime)
        scheduleTimeupdateEvent(true);

    // Explicitly cache vector sizes, as their content is constant from here.
    size_t currentCuesSize = currentCues.size();
    size_t missedCuesSize = missedCues.size();
    size_t previousCuesSize = previousCues.size();

    // 6 - If all of the cues in current cues have their text track cue active
    // flag set, none of the cues in other cues have their text track cue active
    // flag set, and missed cues is empty, then abort these steps.
    bool activeSetChanged = missedCuesSize;

    for (size_t i = 0; !activeSetChanged && i < previousCuesSize; ++i)
        if (!currentCues.contains(previousCues[i]) && previousCues[i].data()->isActive())
            activeSetChanged = true;

    for (size_t i = 0; i < currentCuesSize; ++i) {
        currentCues[i].data()->updateDisplayTree(movieTime);

        if (!currentCues[i].data()->isActive())
            activeSetChanged = true;
    }

    if (!activeSetChanged)
        return;

    // 7 - If the time was reached through the usual monotonic increase of the
    // current playback position during normal playback, and there are cues in
    // other cues that have their text track cue pause-on-exi flag set and that
    // either have their text track cue active flag set or are also in missed
    // cues, then immediately pause the media element.
    for (size_t i = 0; !m_paused && i < previousCuesSize; ++i) {
        if (previousCues[i].data()->pauseOnExit()
            && previousCues[i].data()->isActive()
            && !currentCues.contains(previousCues[i]))
            pause();
    }

    for (size_t i = 0; !m_paused && i < missedCuesSize; ++i) {
        if (missedCues[i].data()->pauseOnExit())
            pause();
    }

    // 8 - Let events be a list of tasks, initially empty. Each task in this
    // list will be associated with a text track, a text track cue, and a time,
    // which are used to sort the list before the tasks are queued.
    Vector<std::pair<double, TextTrackCue*> > eventTasks;

    // 8 - Let affected tracks be a list of text tracks, initially empty.
    Vector<TextTrack*> affectedTracks;

    for (size_t i = 0; i < missedCuesSize; ++i) {
        // 9 - For each text track cue in missed cues, prepare an event named enter
        // for the TextTrackCue object with the text track cue start time.
        eventTasks.append(std::make_pair(missedCues[i].data()->startTime(),
                                         missedCues[i].data()));

        // 10 - For each text track [...] in missed cues, prepare an event
        // named exit for the TextTrackCue object with the  with the later of
        // the text track cue end time and the text track cue start time.

        // Note: An explicit task is added only if the cue is NOT a zero or
        // negative length cue. Otherwise, the need for an exit event is
        // checked when these tasks are actually queued below. This doesn't
        // affect sorting events before dispatch either, because the exit
        // event has the same time as the enter event.
        if (missedCues[i].data()->startTime() < missedCues[i].data()->endTime())
            eventTasks.append(std::make_pair(missedCues[i].data()->endTime(),
                                             missedCues[i].data()));
    }

    for (size_t i = 0; i < previousCuesSize; ++i) {
        // 10 - For each text track cue in other cues that has its text
        // track cue active flag set prepare an event named exit for the
        // TextTrackCue object with the text track cue end time.
        if (!currentCues.contains(previousCues[i]))
            eventTasks.append(std::make_pair(previousCues[i].data()->endTime(),
                                             previousCues[i].data()));
    }

    for (size_t i = 0; i < currentCuesSize; ++i) {
        // 11 - For each text track cue in current cues that does not have its
        // text track cue active flag set, prepare an event named enter for the
        // TextTrackCue object with the text track cue start time.
        if (!previousCues.contains(currentCues[i]))
            eventTasks.append(std::make_pair(currentCues[i].data()->startTime(),
                                             currentCues[i].data()));
    }

    // 12 - Sort the tasks in events in ascending time order (tasks with earlier
    // times first).
    nonCopyingSort(eventTasks.begin(), eventTasks.end(), eventTimeCueCompare);

    for (size_t i = 0; i < eventTasks.size(); ++i) {
        if (!affectedTracks.contains(eventTasks[i].second->track()))
            affectedTracks.append(eventTasks[i].second->track());

        // 13 - Queue each task in events, in list order.
        RefPtr<Event> event;

        // Each event in eventTasks may be either an enterEvent or an exitEvent,
        // depending on the time that is associated with the event. This
        // correctly identifies the type of the event, if the startTime is
        // less than the endTime in the cue.
        if (eventTasks[i].second->startTime() >= eventTasks[i].second->endTime()) {
            event = Event::create(EventTypeNames::enter);
            event->setTarget(eventTasks[i].second);
            m_asyncEventQueue->enqueueEvent(event.release());

            event = Event::create(EventTypeNames::exit);
            event->setTarget(eventTasks[i].second);
            m_asyncEventQueue->enqueueEvent(event.release());
        } else {
            if (eventTasks[i].first == eventTasks[i].second->startTime())
                event = Event::create(EventTypeNames::enter);
            else
                event = Event::create(EventTypeNames::exit);

            event->setTarget(eventTasks[i].second);
            m_asyncEventQueue->enqueueEvent(event.release());
        }
    }

    // 14 - Sort affected tracks in the same order as the text tracks appear in
    // the media element's list of text tracks, and remove duplicates.
    nonCopyingSort(affectedTracks.begin(), affectedTracks.end(), trackIndexCompare);

    // 15 - For each text track in affected tracks, in the list order, queue a
    // task to fire a simple event named cuechange at the TextTrack object, and, ...
    for (size_t i = 0; i < affectedTracks.size(); ++i) {
        RefPtr<Event> event = Event::create(EventTypeNames::cuechange);
        event->setTarget(affectedTracks[i]);

        m_asyncEventQueue->enqueueEvent(event.release());

        // ... if the text track has a corresponding track element, to then fire a
        // simple event named cuechange at the track element as well.
        if (affectedTracks[i]->trackType() == TextTrack::TrackElement) {
            RefPtr<Event> event = Event::create(EventTypeNames::cuechange);
            HTMLTrackElement* trackElement = static_cast<LoadableTextTrack*>(affectedTracks[i])->trackElement();
            ASSERT(trackElement);
            event->setTarget(trackElement);

            m_asyncEventQueue->enqueueEvent(event.release());
        }
    }

    // 16 - Set the text track cue active flag of all the cues in the current
    // cues, and unset the text track cue active flag of all the cues in the
    // other cues.
    for (size_t i = 0; i < currentCuesSize; ++i)
        currentCues[i].data()->setIsActive(true);

    for (size_t i = 0; i < previousCuesSize; ++i)
        if (!currentCues.contains(previousCues[i]))
            previousCues[i].data()->setIsActive(false);

    // Update the current active cues.
    m_currentlyActiveCues = currentCues;

    if (activeSetChanged)
        updateTextTrackDisplay();
}

bool HTMLMediaElement::textTracksAreReady() const
{
    // 4.8.10.12.1 Text track model
    // ...
    // The text tracks of a media element are ready if all the text tracks whose mode was not
    // in the disabled state when the element's resource selection algorithm last started now
    // have a text track readiness state of loaded or failed to load.
    for (unsigned i = 0; i < m_textTracksWhenResourceSelectionBegan.size(); ++i) {
        if (m_textTracksWhenResourceSelectionBegan[i]->readinessState() == TextTrack::Loading
            || m_textTracksWhenResourceSelectionBegan[i]->readinessState() == TextTrack::NotLoaded)
            return false;
    }

    return true;
}

void HTMLMediaElement::textTrackReadyStateChanged(TextTrack* track)
{
    if (m_player && m_textTracksWhenResourceSelectionBegan.contains(track)) {
        if (track->readinessState() != TextTrack::Loading)
            setReadyState(m_player->readyState());
    } else {
        // The track readiness state might have changed as a result of the user
        // clicking the captions button. In this case, a check whether all the
        // resources have failed loading should be done in order to hide the CC button.
        if (hasMediaControls() && track->readinessState() == TextTrack::FailedToLoad)
            mediaControls()->refreshClosedCaptionsButtonVisibility();
    }
}

void HTMLMediaElement::textTrackModeChanged(TextTrack* track)
{
    if (track->trackType() == TextTrack::TrackElement) {
        // 4.8.10.12.3 Sourcing out-of-band text tracks
        // ... when a text track corresponding to a track element is created with text track
        // mode set to disabled and subsequently changes its text track mode to hidden, showing,
        // or showing by default for the first time, the user agent must immediately and synchronously
        // run the following algorithm ...

        for (HTMLTrackElement* trackElement = Traversal<HTMLTrackElement>::firstChild(*this); trackElement; trackElement = Traversal<HTMLTrackElement>::nextSibling(*trackElement)) {
            if (trackElement->track() != track)
                continue;

            // Mark this track as "configured" so configureTextTracks won't change the mode again.
            track->setHasBeenConfigured(true);
            if (track->mode() != TextTrack::disabledKeyword()) {
                if (trackElement->readyState() == HTMLTrackElement::LOADED)
                    textTrackAddCues(track, track->cues());

                // If this is the first added track, create the list of text tracks.
                if (!m_textTracks)
                    m_textTracks = TextTrackList::create(this);
            }
            break;
        }
    } else if (track->trackType() == TextTrack::AddTrack && track->mode() != TextTrack::disabledKeyword()) {
        textTrackAddCues(track, track->cues());
    }

    configureTextTrackDisplay(AssumeVisibleChange);

    ASSERT(textTracks()->contains(track));
    textTracks()->scheduleChangeEvent();
}

void HTMLMediaElement::textTrackKindChanged(TextTrack* track)
{
    if (track->kind() != TextTrack::captionsKeyword() && track->kind() != TextTrack::subtitlesKeyword() && track->mode() == TextTrack::showingKeyword())
        track->setMode(TextTrack::hiddenKeyword());
}

void HTMLMediaElement::beginIgnoringTrackDisplayUpdateRequests()
{
    ++m_ignoreTrackDisplayUpdate;
}

void HTMLMediaElement::endIgnoringTrackDisplayUpdateRequests()
{
    ASSERT(m_ignoreTrackDisplayUpdate);
    --m_ignoreTrackDisplayUpdate;
    if (!m_ignoreTrackDisplayUpdate && m_active)
        updateActiveTextTrackCues(currentTime());
}

void HTMLMediaElement::textTrackAddCues(TextTrack* track, const TextTrackCueList* cues)
{
    WTF_LOG(Media, "HTMLMediaElement::textTrackAddCues");
    if (track->mode() == TextTrack::disabledKeyword())
        return;

    TrackDisplayUpdateScope scope(this);
    for (size_t i = 0; i < cues->length(); ++i)
        textTrackAddCue(cues->item(i)->track(), cues->item(i));
}

void HTMLMediaElement::textTrackRemoveCues(TextTrack*, const TextTrackCueList* cues)
{
    WTF_LOG(Media, "HTMLMediaElement::textTrackRemoveCues");

    TrackDisplayUpdateScope scope(this);
    for (size_t i = 0; i < cues->length(); ++i)
        textTrackRemoveCue(cues->item(i)->track(), cues->item(i));
}

void HTMLMediaElement::textTrackAddCue(TextTrack* track, PassRefPtr<TextTrackCue> cue)
{
    if (track->mode() == TextTrack::disabledKeyword())
        return;

    // Negative duration cues need be treated in the interval tree as
    // zero-length cues.
    double endTime = max(cue->startTime(), cue->endTime());

    CueInterval interval = m_cueTree.createInterval(cue->startTime(), endTime, cue.get());
    if (!m_cueTree.contains(interval))
        m_cueTree.add(interval);
    updateActiveTextTrackCues(currentTime());
}

void HTMLMediaElement::textTrackRemoveCue(TextTrack*, PassRefPtr<TextTrackCue> cue)
{
    // Negative duration cues need to be treated in the interval tree as
    // zero-length cues.
    double endTime = max(cue->startTime(), cue->endTime());

    CueInterval interval = m_cueTree.createInterval(cue->startTime(), endTime, cue.get());
    m_cueTree.remove(interval);

    // Since the cue will be removed from the media element and likely the
    // TextTrack might also be destructed, notifying the region of the cue
    // removal shouldn't be done.
    cue->notifyRegionWhenRemovingDisplayTree(false);

    size_t index = m_currentlyActiveCues.find(interval);
    if (index != kNotFound) {
        m_currentlyActiveCues.remove(index);
        cue->setIsActive(false);
    }
    cue->removeDisplayTree();
    updateActiveTextTrackCues(currentTime());

    cue->notifyRegionWhenRemovingDisplayTree(true);
}


bool HTMLMediaElement::isSafeToLoadURL(const KURL& url, InvalidURLAction actionIfInvalid)
{
    if (!url.isValid()) {
        WTF_LOG(Media, "HTMLMediaElement::isSafeToLoadURL(%s) -> FALSE because url is invalid", urlForLoggingMedia(url).utf8().data());
        return false;
    }

    LocalFrame* frame = document().frame();
    if (!frame || !document().securityOrigin()->canDisplay(url)) {
        if (actionIfInvalid == Complain)
            FrameLoader::reportLocalLoadFailed(frame, url.elidedString());
        WTF_LOG(Media, "HTMLMediaElement::isSafeToLoadURL(%s) -> FALSE rejected by SecurityOrigin", urlForLoggingMedia(url).utf8().data());
        return false;
    }

    if (!document().contentSecurityPolicy()->allowMediaFromSource(url)) {
        WTF_LOG(Media, "HTMLMediaElement::isSafeToLoadURL(%s) -> rejected by Content Security Policy", urlForLoggingMedia(url).utf8().data());
        return false;
    }

    return true;
}

void HTMLMediaElement::startProgressEventTimer()
{
    if (m_progressEventTimer.isActive())
        return;

    m_previousProgressTime = WTF::currentTime();
    // 350ms is not magic, it is in the spec!
    m_progressEventTimer.startRepeating(0.350, FROM_HERE);
}

void HTMLMediaElement::waitForSourceChange()
{
    WTF_LOG(Media, "HTMLMediaElement::waitForSourceChange");

    stopPeriodicTimers();
    m_loadState = WaitingForSource;

    // 6.17 - Waiting: Set the element's networkState attribute to the NETWORK_NO_SOURCE value
    m_networkState = NETWORK_NO_SOURCE;

    // 6.18 - Set the element's delaying-the-load-event flag to false. This stops delaying the load event.
    setShouldDelayLoadEvent(false);

    updateDisplayState();

    if (renderer())
        renderer()->updateFromElement();
}

void HTMLMediaElement::noneSupported()
{
    WTF_LOG(Media, "HTMLMediaElement::noneSupported");

    stopPeriodicTimers();
    m_loadState = WaitingForSource;
    m_currentSourceNode = nullptr;

    // 4.8.10.5
    // 6 - Reaching this step indicates that the media resource failed to load or that the given
    // URL could not be resolved. In one atomic operation, run the following steps:

    // 6.1 - Set the error attribute to a new MediaError object whose code attribute is set to
    // MEDIA_ERR_SRC_NOT_SUPPORTED.
    m_error = MediaError::create(MediaError::MEDIA_ERR_SRC_NOT_SUPPORTED);

    // 6.2 - Forget the media element's media-resource-specific text tracks.
    forgetResourceSpecificTracks();

    // 6.3 - Set the element's networkState attribute to the NETWORK_NO_SOURCE value.
    m_networkState = NETWORK_NO_SOURCE;

    // 7 - Queue a task to fire a simple event named error at the media element.
    scheduleEvent(EventTypeNames::error);

    closeMediaSource();

    // 8 - Set the element's delaying-the-load-event flag to false. This stops delaying the load event.
    setShouldDelayLoadEvent(false);

    // 9 - Abort these steps. Until the load() method is invoked or the src attribute is changed,
    // the element won't attempt to load another resource.

    updateDisplayState();

    if (renderer())
        renderer()->updateFromElement();
}

void HTMLMediaElement::mediaEngineError(PassRefPtr<MediaError> err)
{
    WTF_LOG(Media, "HTMLMediaElement::mediaEngineError(%d)", static_cast<int>(err->code()));

    // 1 - The user agent should cancel the fetching process.
    stopPeriodicTimers();
    m_loadState = WaitingForSource;

    // 2 - Set the error attribute to a new MediaError object whose code attribute is
    // set to MEDIA_ERR_NETWORK/MEDIA_ERR_DECODE.
    m_error = err;

    // 3 - Queue a task to fire a simple event named error at the media element.
    scheduleEvent(EventTypeNames::error);

    closeMediaSource();

    // 4 - Set the element's networkState attribute to the NETWORK_EMPTY value and queue a
    // task to fire a simple event called emptied at the element.
    m_networkState = NETWORK_EMPTY;
    scheduleEvent(EventTypeNames::emptied);

    // 5 - Set the element's delaying-the-load-event flag to false. This stops delaying the load event.
    setShouldDelayLoadEvent(false);

    // 6 - Abort the overall resource selection algorithm.
    m_currentSourceNode = nullptr;
}

void HTMLMediaElement::cancelPendingEventsAndCallbacks()
{
    WTF_LOG(Media, "HTMLMediaElement::cancelPendingEventsAndCallbacks");
    m_asyncEventQueue->cancelAllEvents();

    for (HTMLSourceElement* source = Traversal<HTMLSourceElement>::firstChild(*this); source; source = Traversal<HTMLSourceElement>::nextSibling(*source))
        source->cancelPendingErrorEvent();
}

void HTMLMediaElement::mediaPlayerNetworkStateChanged()
{
    setNetworkState(m_player->networkState());
}

void HTMLMediaElement::mediaLoadingFailed(MediaPlayer::NetworkState error)
{
    stopPeriodicTimers();

    // If we failed while trying to load a <source> element, the movie was never parsed, and there are more
    // <source> children, schedule the next one
    if (m_readyState < HAVE_METADATA && m_loadState == LoadingFromSourceElement) {

        // resource selection algorithm
        // Step 9.Otherwise.9 - Failed with elements: Queue a task, using the DOM manipulation task source, to fire a simple event named error at the candidate element.
        if (m_currentSourceNode)
            m_currentSourceNode->scheduleErrorEvent();
        else
            WTF_LOG(Media, "HTMLMediaElement::setNetworkState - error event not sent, <source> was removed");

        // 9.Otherwise.10 - Asynchronously await a stable state. The synchronous section consists of all the remaining steps of this algorithm until the algorithm says the synchronous section has ended.

        // 9.Otherwise.11 - Forget the media element's media-resource-specific tracks.
        forgetResourceSpecificTracks();

        if (havePotentialSourceChild()) {
            WTF_LOG(Media, "HTMLMediaElement::setNetworkState - scheduling next <source>");
            scheduleNextSourceChild();
        } else {
            WTF_LOG(Media, "HTMLMediaElement::setNetworkState - no more <source> elements, waiting");
            waitForSourceChange();
        }

        return;
    }

    if (error == MediaPlayer::NetworkError && m_readyState >= HAVE_METADATA)
        mediaEngineError(MediaError::create(MediaError::MEDIA_ERR_NETWORK));
    else if (error == MediaPlayer::DecodeError)
        mediaEngineError(MediaError::create(MediaError::MEDIA_ERR_DECODE));
    else if ((error == MediaPlayer::FormatError || error == MediaPlayer::NetworkError) && m_loadState == LoadingFromSrcAttr)
        noneSupported();

    updateDisplayState();
    if (hasMediaControls())
        mediaControls()->reset();
}

void HTMLMediaElement::setNetworkState(MediaPlayer::NetworkState state)
{
    WTF_LOG(Media, "HTMLMediaElement::setNetworkState(%d) - current state is %d", static_cast<int>(state), static_cast<int>(m_networkState));

    if (state == MediaPlayer::Empty) {
        // Just update the cached state and leave, we can't do anything.
        m_networkState = NETWORK_EMPTY;
        return;
    }

    if (state == MediaPlayer::FormatError || state == MediaPlayer::NetworkError || state == MediaPlayer::DecodeError) {
        mediaLoadingFailed(state);
        return;
    }

    if (state == MediaPlayer::Idle) {
        if (m_networkState > NETWORK_IDLE) {
            changeNetworkStateFromLoadingToIdle();
            setShouldDelayLoadEvent(false);
        } else {
            m_networkState = NETWORK_IDLE;
        }
    }

    if (state == MediaPlayer::Loading) {
        if (m_networkState < NETWORK_LOADING || m_networkState == NETWORK_NO_SOURCE)
            startProgressEventTimer();
        m_networkState = NETWORK_LOADING;
    }

    if (state == MediaPlayer::Loaded) {
        if (m_networkState != NETWORK_IDLE)
            changeNetworkStateFromLoadingToIdle();
        m_completelyLoaded = true;
    }
}

void HTMLMediaElement::changeNetworkStateFromLoadingToIdle()
{
    m_progressEventTimer.stop();

    // Schedule one last progress event so we guarantee that at least one is fired
    // for files that load very quickly.
    scheduleEvent(EventTypeNames::progress);
    scheduleEvent(EventTypeNames::suspend);
    m_networkState = NETWORK_IDLE;
}

void HTMLMediaElement::mediaPlayerReadyStateChanged()
{
    setReadyState(m_player->readyState());
}

void HTMLMediaElement::setReadyState(MediaPlayer::ReadyState state)
{
    WTF_LOG(Media, "HTMLMediaElement::setReadyState(%d) - current state is %d,", static_cast<int>(state), static_cast<int>(m_readyState));

    // Set "wasPotentiallyPlaying" BEFORE updating m_readyState, potentiallyPlaying() uses it
    bool wasPotentiallyPlaying = potentiallyPlaying();

    ReadyState oldState = m_readyState;
    ReadyState newState = static_cast<ReadyState>(state);

    bool tracksAreReady = !RuntimeEnabledFeatures::videoTrackEnabled() || textTracksAreReady();

    if (newState == oldState && m_tracksAreReady == tracksAreReady)
        return;

    m_tracksAreReady = tracksAreReady;

    if (tracksAreReady)
        m_readyState = newState;
    else {
        // If a media file has text tracks the readyState may not progress beyond HAVE_FUTURE_DATA until
        // the text tracks are ready, regardless of the state of the media file.
        if (newState <= HAVE_METADATA)
            m_readyState = newState;
        else
            m_readyState = HAVE_CURRENT_DATA;
    }

    if (oldState > m_readyStateMaximum)
        m_readyStateMaximum = oldState;

    if (m_networkState == NETWORK_EMPTY)
        return;

    if (m_seeking) {
        // 4.8.10.9, step 9 note: If the media element was potentially playing immediately before
        // it started seeking, but seeking caused its readyState attribute to change to a value
        // lower than HAVE_FUTURE_DATA, then a waiting will be fired at the element.
        if (wasPotentiallyPlaying && m_readyState < HAVE_FUTURE_DATA)
            scheduleEvent(EventTypeNames::waiting);

        // 4.8.10.9 steps 12-14
        if (m_readyState >= HAVE_CURRENT_DATA)
            finishSeek();
    } else {
        if (wasPotentiallyPlaying && m_readyState < HAVE_FUTURE_DATA) {
            // 4.8.10.8
            scheduleTimeupdateEvent(false);
            scheduleEvent(EventTypeNames::waiting);
        }
    }

    if (m_readyState >= HAVE_METADATA && oldState < HAVE_METADATA) {
        prepareMediaFragmentURI();
        scheduleEvent(EventTypeNames::durationchange);
        if (isVideo())
            scheduleEvent(EventTypeNames::resize);
        scheduleEvent(EventTypeNames::loadedmetadata);
        if (hasMediaControls())
            mediaControls()->reset();
        if (renderer())
            renderer()->updateFromElement();
    }

    bool shouldUpdateDisplayState = false;

    if (m_readyState >= HAVE_CURRENT_DATA && oldState < HAVE_CURRENT_DATA && !m_haveFiredLoadedData) {
        m_haveFiredLoadedData = true;
        shouldUpdateDisplayState = true;
        scheduleEvent(EventTypeNames::loadeddata);
        setShouldDelayLoadEvent(false);
        applyMediaFragmentURI();
    }

    bool isPotentiallyPlaying = potentiallyPlaying();
    if (m_readyState == HAVE_FUTURE_DATA && oldState <= HAVE_CURRENT_DATA && tracksAreReady) {
        scheduleEvent(EventTypeNames::canplay);
        if (isPotentiallyPlaying)
            scheduleEvent(EventTypeNames::playing);
        shouldUpdateDisplayState = true;
    }

    if (m_readyState == HAVE_ENOUGH_DATA && oldState < HAVE_ENOUGH_DATA && tracksAreReady) {
        if (oldState <= HAVE_CURRENT_DATA) {
            scheduleEvent(EventTypeNames::canplay);
            if (isPotentiallyPlaying)
                scheduleEvent(EventTypeNames::playing);
        }

        if (m_autoplaying && m_paused && autoplay() && !document().isSandboxed(SandboxAutomaticFeatures) && !m_userGestureRequiredForPlay) {
            m_paused = false;
            invalidateCachedTime();
            scheduleEvent(EventTypeNames::play);
            scheduleEvent(EventTypeNames::playing);
        }

        scheduleEvent(EventTypeNames::canplaythrough);

        shouldUpdateDisplayState = true;
    }

    if (shouldUpdateDisplayState) {
        updateDisplayState();
        if (hasMediaControls())
            mediaControls()->refreshClosedCaptionsButtonVisibility();
    }

    updatePlayState();
    updateMediaController();
    if (RuntimeEnabledFeatures::videoTrackEnabled())
        updateActiveTextTrackCues(currentTime());
}

void HTMLMediaElement::progressEventTimerFired(Timer<HTMLMediaElement>*)
{
    ASSERT(m_player);
    if (m_networkState != NETWORK_LOADING)
        return;

    double time = WTF::currentTime();
    double timedelta = time - m_previousProgressTime;

    if (m_player->didLoadingProgress()) {
        scheduleEvent(EventTypeNames::progress);
        m_previousProgressTime = time;
        m_sentStalledEvent = false;
        if (renderer())
            renderer()->updateFromElement();
    } else if (timedelta > 3.0 && !m_sentStalledEvent) {
        scheduleEvent(EventTypeNames::stalled);
        m_sentStalledEvent = true;
        setShouldDelayLoadEvent(false);
    }
}

void HTMLMediaElement::addPlayedRange(double start, double end)
{
    WTF_LOG(Media, "HTMLMediaElement::addPlayedRange(%f, %f)", start, end);
    if (!m_playedTimeRanges)
        m_playedTimeRanges = TimeRanges::create();
    m_playedTimeRanges->add(start, end);
}

bool HTMLMediaElement::supportsSave() const
{
    return m_player ? m_player->supportsSave() : false;
}

void HTMLMediaElement::prepareToPlay()
{
    WTF_LOG(Media, "HTMLMediaElement::prepareToPlay(%p)", this);
    if (m_havePreparedToPlay)
        return;
    m_havePreparedToPlay = true;
    m_player->prepareToPlay();
}

void HTMLMediaElement::seek(double time, ExceptionState& exceptionState)
{
    WTF_LOG(Media, "HTMLMediaElement::seek(%f)", time);

    // 4.8.10.9 Seeking

    // 1 - If the media element's readyState is HAVE_NOTHING, then raise an InvalidStateError exception.
    if (m_readyState == HAVE_NOTHING || !m_player) {
        exceptionState.throwDOMException(InvalidStateError, "The element's readyState is HAVE_NOTHING.");
        return;
    }

    // If the media engine has been told to postpone loading data, let it go ahead now.
    if (m_preload < MediaPlayer::Auto && m_readyState < HAVE_FUTURE_DATA)
        prepareToPlay();

    // Get the current time before setting m_seeking, m_lastSeekTime is returned once it is set.
    refreshCachedTime();
    double now = currentTime();

    // 2 - If the element's seeking IDL attribute is true, then another instance of this algorithm is
    // already running. Abort that other instance of the algorithm without waiting for the step that
    // it is running to complete.
    // Nothing specific to be done here.

    // 3 - Set the seeking IDL attribute to true.
    // The flag will be cleared when the engine tells us the time has actually changed.
    m_seeking = true;

    // 5 - If the new playback position is later than the end of the media resource, then let it be the end
    // of the media resource instead.
    time = min(time, duration());

    // 6 - If the new playback position is less than the earliest possible position, let it be that position instead.
    time = max(time, 0.0);

    // Ask the media engine for the time value in the movie's time scale before comparing with current time. This
    // is necessary because if the seek time is not equal to currentTime but the delta is less than the movie's
    // time scale, we will ask the media engine to "seek" to the current movie time, which may be a noop and
    // not generate a timechanged callback. This means m_seeking will never be cleared and we will never
    // fire a 'seeked' event.
#if !LOG_DISABLED
    double mediaTime = m_player->mediaTimeForTimeValue(time);
    if (time != mediaTime)
        WTF_LOG(Media, "HTMLMediaElement::seek(%f) - media timeline equivalent is %f", time, mediaTime);
#endif
    time = m_player->mediaTimeForTimeValue(time);

    // 7 - If the (possibly now changed) new playback position is not in one of the ranges given in the
    // seekable attribute, then let it be the position in one of the ranges given in the seekable attribute
    // that is the nearest to the new playback position. ... If there are no ranges given in the seekable
    // attribute then set the seeking IDL attribute to false and abort these steps.
    RefPtr<TimeRanges> seekableRanges = seekable();

    // Short circuit seeking to the current time by just firing the events if no seek is required.
    // Don't skip calling the media engine if we are in poster mode because a seek should always
    // cancel poster display.
    bool noSeekRequired = !seekableRanges->length() || (time == now && displayMode() != Poster);

    // Always notify the media engine of a seek if the source is not closed. This ensures that the source is
    // always in a flushed state when the 'seeking' event fires.
    if (m_mediaSource && m_mediaSource->isClosed())
        noSeekRequired = false;

    if (noSeekRequired) {
        if (time == now) {
            scheduleEvent(EventTypeNames::seeking);
            // FIXME: There must be a stable state before timeupdate+seeked are dispatched and seeking
            // is reset to false. See http://crbug.com/266631
            scheduleTimeupdateEvent(false);
            scheduleEvent(EventTypeNames::seeked);
        }
        m_seeking = false;
        return;
    }
    time = seekableRanges->nearest(time);

    if (m_playing) {
        if (m_lastSeekTime < now)
            addPlayedRange(m_lastSeekTime, now);
    }
    m_lastSeekTime = time;
    m_sentEndEvent = false;

    // 8 - Queue a task to fire a simple event named seeking at the element.
    scheduleEvent(EventTypeNames::seeking);

    // 9 - Set the current playback position to the given new playback position
    m_player->seek(time);

    // 10-14 are handled, if necessary, when the engine signals a readystate change or otherwise
    // satisfies seek completion and signals a time change.
}

void HTMLMediaElement::finishSeek()
{
    WTF_LOG(Media, "HTMLMediaElement::finishSeek");

    // 4.8.10.9 Seeking completion
    // 12 - Set the seeking IDL attribute to false.
    m_seeking = false;

    // 13 - Queue a task to fire a simple event named timeupdate at the element.
    scheduleTimeupdateEvent(false);

    // 14 - Queue a task to fire a simple event named seeked at the element.
    scheduleEvent(EventTypeNames::seeked);

    setDisplayMode(Video);
}

HTMLMediaElement::ReadyState HTMLMediaElement::readyState() const
{
    return m_readyState;
}

bool HTMLMediaElement::hasAudio() const
{
    return m_player ? m_player->hasAudio() : false;
}

bool HTMLMediaElement::seeking() const
{
    return m_seeking;
}

void HTMLMediaElement::refreshCachedTime() const
{
    m_cachedTime = m_player->currentTime();
    m_cachedTimeWallClockUpdateTime = WTF::currentTime();
}

void HTMLMediaElement::invalidateCachedTime()
{
    WTF_LOG(Media, "HTMLMediaElement::invalidateCachedTime");

    // Don't try to cache movie time when playback first starts as the time reported by the engine
    // sometimes fluctuates for a short amount of time, so the cached time will be off if we take it
    // too early.
    static const double minimumTimePlayingBeforeCacheSnapshot = 0.5;

    m_minimumWallClockTimeToCacheMediaTime = WTF::currentTime() + minimumTimePlayingBeforeCacheSnapshot;
    m_cachedTime = MediaPlayer::invalidTime();
}

// playback state
double HTMLMediaElement::currentTime() const
{
#if LOG_CACHED_TIME_WARNINGS
    static const double minCachedDeltaForWarning = 0.01;
#endif

    if (!m_player)
        return 0;

    if (m_seeking) {
        WTF_LOG(Media, "HTMLMediaElement::currentTime - seeking, returning %f", m_lastSeekTime);
        return m_lastSeekTime;
    }

    if (m_cachedTime != MediaPlayer::invalidTime() && m_paused) {
#if LOG_CACHED_TIME_WARNINGS
        double delta = m_cachedTime - m_player->currentTime();
        if (delta > minCachedDeltaForWarning)
            WTF_LOG(Media, "HTMLMediaElement::currentTime - WARNING, cached time is %f seconds off of media time when paused", delta);
#endif
        return m_cachedTime;
    }

    refreshCachedTime();

    return m_cachedTime;
}

void HTMLMediaElement::setCurrentTime(double time, ExceptionState& exceptionState)
{
    if (m_mediaController) {
        exceptionState.throwDOMException(InvalidStateError, "The element is slaved to a MediaController.");
        return;
    }
    seek(time, exceptionState);
}

double HTMLMediaElement::duration() const
{
    if (!m_player || m_readyState < HAVE_METADATA)
        return numeric_limits<double>::quiet_NaN();

    // FIXME: Refactor so m_duration is kept current (in both MSE and
    // non-MSE cases) once we have transitioned from HAVE_NOTHING ->
    // HAVE_METADATA. Currently, m_duration may be out of date for at least MSE
    // case because MediaSourceBase and SourceBuffer do not notify the element
    // directly upon duration changes caused by endOfStream, remove, or append
    // operations; rather the notification is triggered by the WebMediaPlayer
    // implementation observing that the underlying engine has updated duration
    // and notifying the element to consult its MediaSource for current
    // duration. See http://crbug.com/266644

    if (m_mediaSource)
        return m_mediaSource->duration();

    return m_player->duration();
}

bool HTMLMediaElement::paused() const
{
    return m_paused;
}

double HTMLMediaElement::defaultPlaybackRate() const
{
    return m_defaultPlaybackRate;
}

void HTMLMediaElement::setDefaultPlaybackRate(double rate)
{
    if (m_defaultPlaybackRate != rate) {
        m_defaultPlaybackRate = rate;
        scheduleEvent(EventTypeNames::ratechange);
    }
}

double HTMLMediaElement::playbackRate() const
{
    return m_playbackRate;
}

void HTMLMediaElement::setPlaybackRate(double rate)
{
    WTF_LOG(Media, "HTMLMediaElement::setPlaybackRate(%f)", rate);

    if (m_playbackRate != rate) {
        m_playbackRate = rate;
        invalidateCachedTime();
        scheduleEvent(EventTypeNames::ratechange);
    }

    if (m_player && potentiallyPlaying() && m_player->rate() != rate && !m_mediaController)
        m_player->setRate(rate);
}

void HTMLMediaElement::updatePlaybackRate()
{
    double effectiveRate = m_mediaController ? m_mediaController->playbackRate() : m_playbackRate;
    if (m_player && potentiallyPlaying() && m_player->rate() != effectiveRate)
        m_player->setRate(effectiveRate);
}

bool HTMLMediaElement::ended() const
{
    // 4.8.10.8 Playing the media resource
    // The ended attribute must return true if the media element has ended
    // playback and the direction of playback is forwards, and false otherwise.
    return endedPlayback() && m_playbackRate > 0;
}

bool HTMLMediaElement::autoplay() const
{
    return fastHasAttribute(autoplayAttr);
}

String HTMLMediaElement::preload() const
{
    switch (m_preload) {
    case MediaPlayer::None:
        return "none";
        break;
    case MediaPlayer::MetaData:
        return "metadata";
        break;
    case MediaPlayer::Auto:
        return "auto";
        break;
    }

    ASSERT_NOT_REACHED();
    return String();
}

void HTMLMediaElement::setPreload(const AtomicString& preload)
{
    WTF_LOG(Media, "HTMLMediaElement::setPreload(%s)", preload.utf8().data());
    setAttribute(preloadAttr, preload);
}

void HTMLMediaElement::play()
{
    WTF_LOG(Media, "HTMLMediaElement::play()");

    if (m_userGestureRequiredForPlay && !UserGestureIndicator::processingUserGesture())
        return;
    if (UserGestureIndicator::processingUserGesture())
        m_userGestureRequiredForPlay = false;

    playInternal();
}

void HTMLMediaElement::playInternal()
{
    WTF_LOG(Media, "HTMLMediaElement::playInternal");

    // 4.8.10.9. Playing the media resource
    if (!m_player || m_networkState == NETWORK_EMPTY)
        scheduleDelayedAction(LoadMediaResource);

    if (endedPlayback())
        seek(0, IGNORE_EXCEPTION);

    if (m_mediaController)
        m_mediaController->bringElementUpToSpeed(this);

    if (m_paused) {
        m_paused = false;
        invalidateCachedTime();
        scheduleEvent(EventTypeNames::play);

        if (m_readyState <= HAVE_CURRENT_DATA)
            scheduleEvent(EventTypeNames::waiting);
        else if (m_readyState >= HAVE_FUTURE_DATA)
            scheduleEvent(EventTypeNames::playing);
    }
    m_autoplaying = false;

    updatePlayState();
    updateMediaController();
}

void HTMLMediaElement::pause()
{
    WTF_LOG(Media, "HTMLMediaElement::pause()");

    if (!m_player || m_networkState == NETWORK_EMPTY)
        scheduleDelayedAction(LoadMediaResource);

    m_autoplaying = false;

    if (!m_paused) {
        m_paused = true;
        scheduleTimeupdateEvent(false);
        scheduleEvent(EventTypeNames::pause);
    }

    updatePlayState();
}

void HTMLMediaElement::closeMediaSource()
{
    if (!m_mediaSource)
        return;

    m_mediaSource->close();
    m_mediaSource = nullptr;
}

bool HTMLMediaElement::loop() const
{
    return fastHasAttribute(loopAttr);
}

void HTMLMediaElement::setLoop(bool b)
{
    WTF_LOG(Media, "HTMLMediaElement::setLoop(%s)", boolString(b));
    setBooleanAttribute(loopAttr, b);
}

bool HTMLMediaElement::controls() const
{
    LocalFrame* frame = document().frame();

    // always show controls when scripting is disabled
    if (frame && !frame->script().canExecuteScripts(NotAboutToExecuteScript))
        return true;

    // Always show controls when in full screen mode.
    if (isFullscreen())
        return true;

    return fastHasAttribute(controlsAttr);
}

void HTMLMediaElement::setControls(bool b)
{
    WTF_LOG(Media, "HTMLMediaElement::setControls(%s)", boolString(b));
    setBooleanAttribute(controlsAttr, b);
}

double HTMLMediaElement::volume() const
{
    return m_volume;
}

void HTMLMediaElement::setVolume(double vol, ExceptionState& exceptionState)
{
    WTF_LOG(Media, "HTMLMediaElement::setVolume(%f)", vol);

    if (vol < 0.0f || vol > 1.0f) {
        exceptionState.throwDOMException(IndexSizeError, ExceptionMessages::indexOutsideRange("volume", vol, 0.0, ExceptionMessages::InclusiveBound, 1.0, ExceptionMessages::InclusiveBound));
        return;
    }

    if (m_volume != vol) {
        m_volume = vol;
        updateVolume();
        scheduleEvent(EventTypeNames::volumechange);
    }
}

bool HTMLMediaElement::muted() const
{
    return m_muted;
}

void HTMLMediaElement::setMuted(bool muted)
{
    WTF_LOG(Media, "HTMLMediaElement::setMuted(%s)", boolString(muted));

    if (m_muted == muted)
        return;

    m_muted = muted;

    updateVolume();

    if (hasMediaControls())
        mediaControls()->changedMute();

    scheduleEvent(EventTypeNames::volumechange);
}

void HTMLMediaElement::beginScrubbing()
{
    WTF_LOG(Media, "HTMLMediaElement::beginScrubbing - paused() is %s", boolString(paused()));

    if (!paused()) {
        if (ended()) {
            // Because a media element stays in non-paused state when it reaches end, playback resumes
            // when the slider is dragged from the end to another position unless we pause first. Do
            // a "hard pause" so an event is generated, since we want to stay paused after scrubbing finishes.
            pause();
        } else {
            // Not at the end but we still want to pause playback so the media engine doesn't try to
            // continue playing during scrubbing. Pause without generating an event as we will
            // unpause after scrubbing finishes.
            setPausedInternal(true);
        }
    }
}

void HTMLMediaElement::endScrubbing()
{
    WTF_LOG(Media, "HTMLMediaElement::endScrubbing - m_pausedInternal is %s", boolString(m_pausedInternal));

    if (m_pausedInternal)
        setPausedInternal(false);
}

// The spec says to fire periodic timeupdate events (those sent while playing) every
// "15 to 250ms", we choose the slowest frequency
static const double maxTimeupdateEventFrequency = 0.25;

void HTMLMediaElement::startPlaybackProgressTimer()
{
    if (m_playbackProgressTimer.isActive())
        return;

    m_previousProgressTime = WTF::currentTime();
    m_playbackProgressTimer.startRepeating(maxTimeupdateEventFrequency, FROM_HERE);
}

void HTMLMediaElement::playbackProgressTimerFired(Timer<HTMLMediaElement>*)
{
    ASSERT(m_player);

    if (m_fragmentEndTime != MediaPlayer::invalidTime() && currentTime() >= m_fragmentEndTime && m_playbackRate > 0) {
        m_fragmentEndTime = MediaPlayer::invalidTime();
        if (!m_mediaController && !m_paused) {
            UseCounter::count(document(), UseCounter::HTMLMediaElementPauseAtFragmentEnd);
            // changes paused to true and fires a simple event named pause at the media element.
            pause();
        }
    }

    if (!m_seeking)
        scheduleTimeupdateEvent(true);

    if (!m_playbackRate)
        return;

    if (!m_paused && hasMediaControls())
        mediaControls()->playbackProgressed();

    if (RuntimeEnabledFeatures::videoTrackEnabled())
        updateActiveTextTrackCues(currentTime());
}

void HTMLMediaElement::scheduleTimeupdateEvent(bool periodicEvent)
{
    double now = WTF::currentTime();
    double timedelta = now - m_lastTimeUpdateEventWallTime;

    // throttle the periodic events
    if (periodicEvent && timedelta < maxTimeupdateEventFrequency)
        return;

    // Some media engines make multiple "time changed" callbacks at the same time, but we only want one
    // event at a given time so filter here
    double movieTime = currentTime();
    if (movieTime != m_lastTimeUpdateEventMovieTime) {
        scheduleEvent(EventTypeNames::timeupdate);
        m_lastTimeUpdateEventWallTime = now;
        m_lastTimeUpdateEventMovieTime = movieTime;
    }
}

bool HTMLMediaElement::canPlay() const
{
    return paused() || ended() || m_readyState < HAVE_METADATA;
}

void HTMLMediaElement::mediaPlayerDidAddTextTrack(WebInbandTextTrack* webTrack)
{
    if (!RuntimeEnabledFeatures::videoTrackEnabled())
        return;

    // 4.8.10.12.2 Sourcing in-band text tracks
    // 1. Associate the relevant data with a new text track and its corresponding new TextTrack object.
    RefPtr<InbandTextTrack> textTrack = InbandTextTrack::create(document(), webTrack);

    // 2. Set the new text track's kind, label, and language based on the semantics of the relevant data,
    // as defined by the relevant specification. If there is no label in that data, then the label must
    // be set to the empty string.
    // 3. Associate the text track list of cues with the rules for updating the text track rendering appropriate
    // for the format in question.
    // 4. If the new text track's kind is metadata, then set the text track in-band metadata track dispatch type
    // as follows, based on the type of the media resource:
    // 5. Populate the new text track's list of cues with the cues parsed so far, folllowing the guidelines for exposing
    // cues, and begin updating it dynamically as necessary.
    //   - Thess are all done by the media engine.

    // 6. Set the new text track's readiness state to loaded.
    textTrack->setReadinessState(TextTrack::Loaded);

    // 7. Set the new text track's mode to the mode consistent with the user's preferences and the requirements of
    // the relevant specification for the data.
    //  - This will happen in configureTextTracks()
    scheduleDelayedAction(LoadTextTrackResource);

    // 8. Add the new text track to the media element's list of text tracks.
    // 9. Fire an event with the name addtrack, that does not bubble and is not cancelable, and that uses the TrackEvent
    // interface, with the track attribute initialized to the text track's TextTrack object, at the media element's
    // textTracks attribute's TextTrackList object.
    addTextTrack(textTrack.get());
}

void HTMLMediaElement::mediaPlayerDidRemoveTextTrack(WebInbandTextTrack* webTrack)
{
    if (!RuntimeEnabledFeatures::videoTrackEnabled())
        return;

    if (!m_textTracks)
        return;

    // This cast is safe because we created the InbandTextTrack with the WebInbandTextTrack
    // passed to mediaPlayerDidAddTextTrack.
    RefPtr<InbandTextTrack> textTrack = static_cast<InbandTextTrack*>(webTrack->client());
    if (!textTrack)
        return;

    removeTextTrack(textTrack.get());
}

void HTMLMediaElement::closeCaptionTracksChanged()
{
    if (hasMediaControls())
        mediaControls()->closedCaptionTracksChanged();
}

void HTMLMediaElement::addTextTrack(TextTrack* track)
{
    textTracks()->append(track);

    closeCaptionTracksChanged();
}

void HTMLMediaElement::removeTextTrack(TextTrack* track)
{
    TrackDisplayUpdateScope scope(this);
    m_textTracks->remove(track);

    closeCaptionTracksChanged();
}

void HTMLMediaElement::forgetResourceSpecificTracks()
{
    if (m_textTracks) {
        TrackDisplayUpdateScope scope(this);
        m_textTracks->removeAllInbandTracks();
        closeCaptionTracksChanged();
    }
}

PassRefPtr<TextTrack> HTMLMediaElement::addTextTrack(const AtomicString& kind, const AtomicString& label, const AtomicString& language, ExceptionState& exceptionState)
{
    ASSERT(RuntimeEnabledFeatures::videoTrackEnabled());

    // 4.8.10.12.4 Text track API
    // The addTextTrack(kind, label, language) method of media elements, when invoked, must run the following steps:

    // 1. If kind is not one of the following strings, then throw a SyntaxError exception and abort these steps
    if (!TextTrack::isValidKindKeyword(kind)) {
        exceptionState.throwDOMException(SyntaxError, "The 'kind' provided ('" + kind + "') is invalid.");
        return nullptr;
    }

    // 2. If the label argument was omitted, let label be the empty string.
    // 3. If the language argument was omitted, let language be the empty string.
    // 4. Create a new TextTrack object.

    // 5. Create a new text track corresponding to the new object, and set its text track kind to kind, its text
    // track label to label, its text track language to language...
    RefPtr<TextTrack> textTrack = TextTrack::create(document(), kind, label, language);

    // Note, due to side effects when changing track parameters, we have to
    // first append the track to the text track list.

    // 6. Add the new text track to the media element's list of text tracks.
    addTextTrack(textTrack.get());

    // ... its text track readiness state to the text track loaded state ...
    textTrack->setReadinessState(TextTrack::Loaded);

    // ... its text track mode to the text track hidden mode, and its text track list of cues to an empty list ...
    textTrack->setMode(TextTrack::hiddenKeyword());

    return textTrack.release();
}

TextTrackList* HTMLMediaElement::textTracks()
{
    ASSERT(RuntimeEnabledFeatures::videoTrackEnabled());

    if (!m_textTracks)
        m_textTracks = TextTrackList::create(this);

    return m_textTracks.get();
}

void HTMLMediaElement::didAddTrackElement(HTMLTrackElement* trackElement)
{
    if (!RuntimeEnabledFeatures::videoTrackEnabled())
        return;

    // 4.8.10.12.3 Sourcing out-of-band text tracks
    // When a track element's parent element changes and the new parent is a media element,
    // then the user agent must add the track element's corresponding text track to the
    // media element's list of text tracks ... [continues in TextTrackList::append]
    RefPtr<TextTrack> textTrack = trackElement->track();
    if (!textTrack)
        return;

    addTextTrack(textTrack.get());

    // Do not schedule the track loading until parsing finishes so we don't start before all tracks
    // in the markup have been added.
    if (isFinishedParsingChildren())
        scheduleDelayedAction(LoadTextTrackResource);

    if (hasMediaControls())
        mediaControls()->closedCaptionTracksChanged();
}

void HTMLMediaElement::didRemoveTrackElement(HTMLTrackElement* trackElement)
{
    if (!RuntimeEnabledFeatures::videoTrackEnabled())
        return;

#if !LOG_DISABLED
    KURL url = trackElement->getNonEmptyURLAttribute(srcAttr);
    WTF_LOG(Media, "HTMLMediaElement::didRemoveTrackElement - 'src' is %s", urlForLoggingMedia(url).utf8().data());
#endif

    RefPtr<TextTrack> textTrack = trackElement->track();
    if (!textTrack)
        return;

    textTrack->setHasBeenConfigured(false);

    if (!m_textTracks)
        return;

    // 4.8.10.12.3 Sourcing out-of-band text tracks
    // When a track element's parent element changes and the old parent was a media element,
    // then the user agent must remove the track element's corresponding text track from the
    // media element's list of text tracks.
    removeTextTrack(textTrack.get());

    size_t index = m_textTracksWhenResourceSelectionBegan.find(textTrack.get());
    if (index != kNotFound)
        m_textTracksWhenResourceSelectionBegan.remove(index);
}

static int textTrackLanguageSelectionScore(const TextTrack& track)
{
    if (track.language().isEmpty())
        return 0;

    Vector<AtomicString> languages = userPreferredLanguages();
    size_t languageMatchIndex = indexOfBestMatchingLanguageInList(track.language(), languages);
    if (languageMatchIndex >= languages.size())
        return 0;

    // Matching a track language is more important than matching track type, so this multiplier must be
    // greater than the maximum value returned by textTrackSelectionScore.
    return (languages.size() - languageMatchIndex) * 10;
}

static int textTrackSelectionScore(const TextTrack& track, Settings* settings)
{
    int trackScore = 0;

    if (!settings)
        return trackScore;

    if (track.kind() != TextTrack::captionsKeyword() && track.kind() != TextTrack::subtitlesKeyword())
        return trackScore;

    if (track.kind() == TextTrack::subtitlesKeyword() && settings->shouldDisplaySubtitles())
        trackScore = 1;
    else if (track.kind() == TextTrack::captionsKeyword() && settings->shouldDisplayCaptions())
        trackScore = 1;

    return trackScore + textTrackLanguageSelectionScore(track);
}

void HTMLMediaElement::configureTextTrackGroup(const TrackGroup& group)
{
    ASSERT(group.tracks.size());

    WTF_LOG(Media, "HTMLMediaElement::configureTextTrackGroup(%d)", group.kind);

    Settings* settings = document().settings();

    // First, find the track in the group that should be enabled (if any).
    Vector<RefPtr<TextTrack> > currentlyEnabledTracks;
    RefPtr<TextTrack> trackToEnable;
    RefPtr<TextTrack> defaultTrack;
    RefPtr<TextTrack> fallbackTrack;
    int highestTrackScore = 0;
    for (size_t i = 0; i < group.tracks.size(); ++i) {
        RefPtr<TextTrack> textTrack = group.tracks[i];

        if (m_processingPreferenceChange && textTrack->mode() == TextTrack::showingKeyword())
            currentlyEnabledTracks.append(textTrack);

        int trackScore = textTrackSelectionScore(*textTrack, settings);
        if (trackScore) {
            // * If the text track kind is { [subtitles or captions] [descriptions] } and the user has indicated an interest in having a
            // track with this text track kind, text track language, and text track label enabled, and there is no
            // other text track in the media element's list of text tracks with a text track kind of either subtitles
            // or captions whose text track mode is showing
            // ...
            // * If the text track kind is chapters and the text track language is one that the user agent has reason
            // to believe is appropriate for the user, and there is no other text track in the media element's list of
            // text tracks with a text track kind of chapters whose text track mode is showing
            //    Let the text track mode be showing.
            if (trackScore > highestTrackScore) {
                highestTrackScore = trackScore;
                trackToEnable = textTrack;
            }

            if (!defaultTrack && textTrack->isDefault())
                defaultTrack = textTrack;
            if (!defaultTrack && !fallbackTrack)
                fallbackTrack = textTrack;
        } else if (!group.visibleTrack && !defaultTrack && textTrack->isDefault()) {
            // * If the track element has a default attribute specified, and there is no other text track in the media
            // element's list of text tracks whose text track mode is showing or showing by default
            //    Let the text track mode be showing by default.
            defaultTrack = textTrack;
        }
    }

    if (!trackToEnable && defaultTrack)
        trackToEnable = defaultTrack;

    // If no track matches the user's preferred language and non was marked 'default', enable the first track
    // because the user has explicitly stated a preference for this kind of track.
    if (!fallbackTrack && m_closedCaptionsVisible && group.kind == TrackGroup::CaptionsAndSubtitles)
        fallbackTrack = group.tracks[0];

    if (!trackToEnable && fallbackTrack)
        trackToEnable = fallbackTrack;

    if (currentlyEnabledTracks.size()) {
        for (size_t i = 0; i < currentlyEnabledTracks.size(); ++i) {
            RefPtr<TextTrack> textTrack = currentlyEnabledTracks[i];
            if (textTrack != trackToEnable)
                textTrack->setMode(TextTrack::disabledKeyword());
        }
    }

    if (trackToEnable)
        trackToEnable->setMode(TextTrack::showingKeyword());
}

void HTMLMediaElement::configureTextTracks()
{
    TrackGroup captionAndSubtitleTracks(TrackGroup::CaptionsAndSubtitles);
    TrackGroup descriptionTracks(TrackGroup::Description);
    TrackGroup chapterTracks(TrackGroup::Chapter);
    TrackGroup metadataTracks(TrackGroup::Metadata);
    TrackGroup otherTracks(TrackGroup::Other);

    if (!m_textTracks)
        return;

    for (size_t i = 0; i < m_textTracks->length(); ++i) {
        RefPtr<TextTrack> textTrack = m_textTracks->item(i);
        if (!textTrack)
            continue;

        String kind = textTrack->kind();
        TrackGroup* currentGroup;
        if (kind == TextTrack::subtitlesKeyword() || kind == TextTrack::captionsKeyword())
            currentGroup = &captionAndSubtitleTracks;
        else if (kind == TextTrack::descriptionsKeyword())
            currentGroup = &descriptionTracks;
        else if (kind == TextTrack::chaptersKeyword())
            currentGroup = &chapterTracks;
        else if (kind == TextTrack::metadataKeyword())
            currentGroup = &metadataTracks;
        else
            currentGroup = &otherTracks;

        if (!currentGroup->visibleTrack && textTrack->mode() == TextTrack::showingKeyword())
            currentGroup->visibleTrack = textTrack;
        if (!currentGroup->defaultTrack && textTrack->isDefault())
            currentGroup->defaultTrack = textTrack;

        // Do not add this track to the group if it has already been automatically configured
        // as we only want to call configureTextTrack once per track so that adding another
        // track after the initial configuration doesn't reconfigure every track - only those
        // that should be changed by the new addition. For example all metadata tracks are
        // disabled by default, and we don't want a track that has been enabled by script
        // to be disabled automatically when a new metadata track is added later.
        if (textTrack->hasBeenConfigured())
            continue;

        if (textTrack->language().length())
            currentGroup->hasSrcLang = true;
        currentGroup->tracks.append(textTrack);
    }

    if (captionAndSubtitleTracks.tracks.size())
        configureTextTrackGroup(captionAndSubtitleTracks);
    if (descriptionTracks.tracks.size())
        configureTextTrackGroup(descriptionTracks);
    if (chapterTracks.tracks.size())
        configureTextTrackGroup(chapterTracks);
    if (metadataTracks.tracks.size())
        configureTextTrackGroup(metadataTracks);
    if (otherTracks.tracks.size())
        configureTextTrackGroup(otherTracks);

    if (hasMediaControls())
        mediaControls()->closedCaptionTracksChanged();
}

bool HTMLMediaElement::havePotentialSourceChild()
{
    // Stash the current <source> node and next nodes so we can restore them after checking
    // to see there is another potential.
    RefPtr<HTMLSourceElement> currentSourceNode = m_currentSourceNode;
    RefPtr<Node> nextNode = m_nextChildNodeToConsider;

    KURL nextURL = selectNextSourceChild(0, 0, DoNothing);

    m_currentSourceNode = currentSourceNode;
    m_nextChildNodeToConsider = nextNode;

    return nextURL.isValid();
}

KURL HTMLMediaElement::selectNextSourceChild(ContentType* contentType, String* keySystem, InvalidURLAction actionIfInvalid)
{
#if !LOG_DISABLED
    // Don't log if this was just called to find out if there are any valid <source> elements.
    bool shouldLog = actionIfInvalid != DoNothing;
    if (shouldLog)
        WTF_LOG(Media, "HTMLMediaElement::selectNextSourceChild");
#endif

    if (!m_nextChildNodeToConsider) {
#if !LOG_DISABLED
        if (shouldLog)
            WTF_LOG(Media, "HTMLMediaElement::selectNextSourceChild -> 0x0000, \"\"");
#endif
        return KURL();
    }

    KURL mediaURL;
    Node* node;
    HTMLSourceElement* source = 0;
    String type;
    String system;
    bool lookingForStartNode = m_nextChildNodeToConsider;
    bool canUseSourceElement = false;
    bool okToLoadSourceURL;

    NodeVector potentialSourceNodes;
    getChildNodes(*this, potentialSourceNodes);

    for (unsigned i = 0; !canUseSourceElement && i < potentialSourceNodes.size(); ++i) {
        node = potentialSourceNodes[i].get();
        if (lookingForStartNode && m_nextChildNodeToConsider != node)
            continue;
        lookingForStartNode = false;

        if (!isHTMLSourceElement(*node))
            continue;
        if (node->parentNode() != this)
            continue;

        source = toHTMLSourceElement(node);

        // If candidate does not have a src attribute, or if its src attribute's value is the empty string ... jump down to the failed step below
        mediaURL = source->getNonEmptyURLAttribute(srcAttr);
#if !LOG_DISABLED
        if (shouldLog)
            WTF_LOG(Media, "HTMLMediaElement::selectNextSourceChild - 'src' is %s", urlForLoggingMedia(mediaURL).utf8().data());
#endif
        if (mediaURL.isEmpty())
            goto check_again;

        type = source->type();
        // FIXME(82965): Add support for keySystem in <source> and set system from source.
        if (type.isEmpty() && mediaURL.protocolIsData())
            type = mimeTypeFromDataURL(mediaURL);
        if (!type.isEmpty() || !system.isEmpty()) {
#if !LOG_DISABLED
            if (shouldLog)
                WTF_LOG(Media, "HTMLMediaElement::selectNextSourceChild - 'type' is '%s' - key system is '%s'", type.utf8().data(), system.utf8().data());
#endif
            if (!supportsType(ContentType(type), system))
                goto check_again;
        }

        // Is it safe to load this url?
        okToLoadSourceURL = isSafeToLoadURL(mediaURL, actionIfInvalid) && dispatchBeforeLoadEvent(mediaURL.string());

        // A 'beforeload' event handler can mutate the DOM, so check to see if the source element is still a child node.
        if (node->parentNode() != this) {
            WTF_LOG(Media, "HTMLMediaElement::selectNextSourceChild : 'beforeload' removed current element");
            source = 0;
            goto check_again;
        }

        if (!okToLoadSourceURL)
            goto check_again;

        // Making it this far means the <source> looks reasonable.
        canUseSourceElement = true;

check_again:
        if (!canUseSourceElement && actionIfInvalid == Complain && source)
            source->scheduleErrorEvent();
    }

    if (canUseSourceElement) {
        if (contentType)
            *contentType = ContentType(type);
        if (keySystem)
            *keySystem = system;
        m_currentSourceNode = source;
        m_nextChildNodeToConsider = source->nextSibling();
    } else {
        m_currentSourceNode = nullptr;
        m_nextChildNodeToConsider = nullptr;
    }

#if !LOG_DISABLED
    if (shouldLog)
        WTF_LOG(Media, "HTMLMediaElement::selectNextSourceChild -> %p, %s", m_currentSourceNode.get(), canUseSourceElement ? urlForLoggingMedia(mediaURL).utf8().data() : "");
#endif
    return canUseSourceElement ? mediaURL : KURL();
}

void HTMLMediaElement::sourceWasAdded(HTMLSourceElement* source)
{
    WTF_LOG(Media, "HTMLMediaElement::sourceWasAdded(%p)", source);

#if !LOG_DISABLED
    KURL url = source->getNonEmptyURLAttribute(srcAttr);
    WTF_LOG(Media, "HTMLMediaElement::sourceWasAdded - 'src' is %s", urlForLoggingMedia(url).utf8().data());
#endif

    // We should only consider a <source> element when there is not src attribute at all.
    if (fastHasAttribute(srcAttr))
        return;

    // 4.8.8 - If a source element is inserted as a child of a media element that has no src
    // attribute and whose networkState has the value NETWORK_EMPTY, the user agent must invoke
    // the media element's resource selection algorithm.
    if (networkState() == HTMLMediaElement::NETWORK_EMPTY) {
        scheduleDelayedAction(LoadMediaResource);
        m_nextChildNodeToConsider = source;
        return;
    }

    if (m_currentSourceNode && source == m_currentSourceNode->nextSibling()) {
        WTF_LOG(Media, "HTMLMediaElement::sourceWasAdded - <source> inserted immediately after current source");
        m_nextChildNodeToConsider = source;
        return;
    }

    if (m_nextChildNodeToConsider)
        return;

    // 4.8.9.5, resource selection algorithm, source elements section:
    // 21. Wait until the node after pointer is a node other than the end of the list. (This step might wait forever.)
    // 22. Asynchronously await a stable state...
    // 23. Set the element's delaying-the-load-event flag back to true (this delays the load event again, in case
    // it hasn't been fired yet).
    setShouldDelayLoadEvent(true);

    // 24. Set the networkState back to NETWORK_LOADING.
    m_networkState = NETWORK_LOADING;

    // 25. Jump back to the find next candidate step above.
    m_nextChildNodeToConsider = source;
    scheduleNextSourceChild();
}

void HTMLMediaElement::sourceWasRemoved(HTMLSourceElement* source)
{
    WTF_LOG(Media, "HTMLMediaElement::sourceWasRemoved(%p)", source);

#if !LOG_DISABLED
    KURL url = source->getNonEmptyURLAttribute(srcAttr);
    WTF_LOG(Media, "HTMLMediaElement::sourceWasRemoved - 'src' is %s", urlForLoggingMedia(url).utf8().data());
#endif

    if (source != m_currentSourceNode && source != m_nextChildNodeToConsider)
        return;

    if (source == m_nextChildNodeToConsider) {
        if (m_currentSourceNode)
            m_nextChildNodeToConsider = m_currentSourceNode->nextSibling();
        WTF_LOG(Media, "HTMLMediaElement::sourceRemoved - m_nextChildNodeToConsider set to %p", m_nextChildNodeToConsider.get());
    } else if (source == m_currentSourceNode) {
        // Clear the current source node pointer, but don't change the movie as the spec says:
        // 4.8.8 - Dynamically modifying a source element and its attribute when the element is already
        // inserted in a video or audio element will have no effect.
        m_currentSourceNode = nullptr;
        WTF_LOG(Media, "HTMLMediaElement::sourceRemoved - m_currentSourceNode set to 0");
    }
}

void HTMLMediaElement::mediaPlayerTimeChanged()
{
    WTF_LOG(Media, "HTMLMediaElement::mediaPlayerTimeChanged");

    if (RuntimeEnabledFeatures::videoTrackEnabled())
        updateActiveTextTrackCues(currentTime());

    invalidateCachedTime();

    // 4.8.10.9 steps 12-14. Needed if no ReadyState change is associated with the seek.
    if (m_seeking && m_readyState >= HAVE_CURRENT_DATA && !m_player->seeking())
        finishSeek();

    // Always call scheduleTimeupdateEvent when the media engine reports a time discontinuity,
    // it will only queue a 'timeupdate' event if we haven't already posted one at the current
    // movie time.
    scheduleTimeupdateEvent(false);

    double now = currentTime();
    double dur = duration();

    // When the current playback position reaches the end of the media resource when the direction of
    // playback is forwards, then the user agent must follow these steps:
    if (!std::isnan(dur) && dur && now >= dur && m_playbackRate > 0) {
        // If the media element has a loop attribute specified and does not have a current media controller,
        if (loop() && !m_mediaController) {
            m_sentEndEvent = false;
            //  then seek to the earliest possible position of the media resource and abort these steps.
            seek(0, IGNORE_EXCEPTION);
        } else {
            // If the media element does not have a current media controller, and the media element
            // has still ended playback, and the direction of playback is still forwards, and paused
            // is false,
            if (!m_mediaController && !m_paused) {
                // changes paused to true and fires a simple event named pause at the media element.
                m_paused = true;
                scheduleEvent(EventTypeNames::pause);
            }
            // Queue a task to fire a simple event named ended at the media element.
            if (!m_sentEndEvent) {
                m_sentEndEvent = true;
                scheduleEvent(EventTypeNames::ended);
            }
            // If the media element has a current media controller, then report the controller state
            // for the media element's current media controller.
            updateMediaController();
        }
    }
    else
        m_sentEndEvent = false;

    updatePlayState();
}

void HTMLMediaElement::mediaPlayerDurationChanged()
{
    WTF_LOG(Media, "HTMLMediaElement::mediaPlayerDurationChanged");
    durationChanged(duration());
}

void HTMLMediaElement::durationChanged(double duration)
{
    WTF_LOG(Media, "HTMLMediaElement::durationChanged(%f)", duration);

    // Abort if duration unchanged.
    if (m_duration == duration)
        return;

    m_duration = duration;
    scheduleEvent(EventTypeNames::durationchange);

    if (hasMediaControls())
        mediaControls()->reset();
    if (renderer())
        renderer()->updateFromElement();

    if (currentTime() > duration)
        seek(duration, IGNORE_EXCEPTION);
}

void HTMLMediaElement::mediaPlayerPlaybackStateChanged()
{
    WTF_LOG(Media, "HTMLMediaElement::mediaPlayerPlaybackStateChanged");

    if (!m_player || m_pausedInternal)
        return;

    if (m_player->paused())
        pause();
    else
        playInternal();
}

void HTMLMediaElement::mediaPlayerRequestFullscreen()
{
    WTF_LOG(Media, "HTMLMediaElement::mediaPlayerRequestFullscreen");
    enterFullscreen();
}

void HTMLMediaElement::mediaPlayerRequestSeek(double time)
{
    // The player is the source of this seek request.
    if (m_mediaController) {
        m_mediaController->setCurrentTime(time, IGNORE_EXCEPTION);
        return;
    }
    setCurrentTime(time, IGNORE_EXCEPTION);
}

// MediaPlayerPresentation methods
void HTMLMediaElement::mediaPlayerRepaint()
{
    if (m_webLayer)
        m_webLayer->invalidate();

    updateDisplayState();
    if (renderer())
        renderer()->repaint();
}

void HTMLMediaElement::mediaPlayerSizeChanged()
{
    WTF_LOG(Media, "HTMLMediaElement::mediaPlayerSizeChanged");

    ASSERT(hasVideo()); // "resize" makes no sense absent video.
    if (m_readyState > HAVE_NOTHING && isVideo())
        scheduleEvent(EventTypeNames::resize);

    if (renderer())
        renderer()->updateFromElement();
}

PassRefPtr<TimeRanges> HTMLMediaElement::buffered() const
{
    if (!m_player)
        return TimeRanges::create();

    if (m_mediaSource)
        return m_mediaSource->buffered();

    return m_player->buffered();
}

PassRefPtr<TimeRanges> HTMLMediaElement::played()
{
    if (m_playing) {
        double time = currentTime();
        if (time > m_lastSeekTime)
            addPlayedRange(m_lastSeekTime, time);
    }

    if (!m_playedTimeRanges)
        m_playedTimeRanges = TimeRanges::create();

    return m_playedTimeRanges->copy();
}

PassRefPtr<TimeRanges> HTMLMediaElement::seekable() const
{
    if (m_player) {
        double maxTimeSeekable = m_player->maxTimeSeekable();
        if (maxTimeSeekable)
            return TimeRanges::create(0, maxTimeSeekable);
    }
    return TimeRanges::create();
}

bool HTMLMediaElement::potentiallyPlaying() const
{
    // "pausedToBuffer" means the media engine's rate is 0, but only because it had to stop playing
    // when it ran out of buffered data. A movie is this state is "potentially playing", modulo the
    // checks in couldPlayIfEnoughData().
    bool pausedToBuffer = m_readyStateMaximum >= HAVE_FUTURE_DATA && m_readyState < HAVE_FUTURE_DATA;
    return (pausedToBuffer || m_readyState >= HAVE_FUTURE_DATA) && couldPlayIfEnoughData() && !isBlockedOnMediaController();
}

bool HTMLMediaElement::couldPlayIfEnoughData() const
{
    return !paused() && !endedPlayback() && !stoppedDueToErrors() && !pausedForUserInteraction();
}

bool HTMLMediaElement::endedPlayback() const
{
    double dur = duration();
    if (!m_player || std::isnan(dur))
        return false;

    // 4.8.10.8 Playing the media resource

    // A media element is said to have ended playback when the element's
    // readyState attribute is HAVE_METADATA or greater,
    if (m_readyState < HAVE_METADATA)
        return false;

    // and the current playback position is the end of the media resource and the direction
    // of playback is forwards, Either the media element does not have a loop attribute specified,
    // or the media element has a current media controller.
    double now = currentTime();
    if (m_playbackRate > 0)
        return dur > 0 && now >= dur && (!loop() || m_mediaController);

    // or the current playback position is the earliest possible position and the direction
    // of playback is backwards
    if (m_playbackRate < 0)
        return now <= 0;

    return false;
}

bool HTMLMediaElement::stoppedDueToErrors() const
{
    if (m_readyState >= HAVE_METADATA && m_error) {
        RefPtr<TimeRanges> seekableRanges = seekable();
        if (!seekableRanges->contain(currentTime()))
            return true;
    }

    return false;
}

bool HTMLMediaElement::pausedForUserInteraction() const
{
//    return !paused() && m_readyState >= HAVE_FUTURE_DATA && [UA requires a decitions from the user]
    return false;
}

void HTMLMediaElement::updateVolume()
{
    if (webMediaPlayer())
        webMediaPlayer()->setVolume(playerVolume());

    if (hasMediaControls())
        mediaControls()->changedVolume();
}

double HTMLMediaElement::playerVolume() const
{
    double volumeMultiplier = 1;
    bool shouldMute = m_muted;

    if (m_mediaController) {
        volumeMultiplier *= m_mediaController->volume();
        shouldMute = m_mediaController->muted();
    }

    return shouldMute ? 0 : m_volume * volumeMultiplier;
}

void HTMLMediaElement::updatePlayState()
{
    if (!m_player)
        return;

    if (m_pausedInternal) {
        if (!m_player->paused())
            m_player->pause();
        refreshCachedTime();
        m_playbackProgressTimer.stop();
        if (hasMediaControls())
            mediaControls()->playbackStopped();
        return;
    }

    bool shouldBePlaying = potentiallyPlaying();
    bool playerPaused = m_player->paused();

    WTF_LOG(Media, "HTMLMediaElement::updatePlayState - shouldBePlaying = %s, playerPaused = %s",
        boolString(shouldBePlaying), boolString(playerPaused));

    if (shouldBePlaying) {
        setDisplayMode(Video);
        invalidateCachedTime();

        if (playerPaused) {
            // Set rate, muted before calling play in case they were set before the media engine was setup.
            // The media engine should just stash the rate and muted values since it isn't already playing.
            m_player->setRate(m_playbackRate);
            updateVolume();

            m_player->play();
        }

        if (hasMediaControls())
            mediaControls()->playbackStarted();
        startPlaybackProgressTimer();
        m_playing = true;

    } else { // Should not be playing right now
        if (!playerPaused)
            m_player->pause();
        refreshCachedTime();

        m_playbackProgressTimer.stop();
        m_playing = false;
        double time = currentTime();
        if (time > m_lastSeekTime)
            addPlayedRange(m_lastSeekTime, time);

        if (couldPlayIfEnoughData())
            prepareToPlay();

        if (hasMediaControls())
            mediaControls()->playbackStopped();
    }

    updateMediaController();

    if (renderer())
        renderer()->updateFromElement();
}

void HTMLMediaElement::setPausedInternal(bool b)
{
    m_pausedInternal = b;
    updatePlayState();
}

void HTMLMediaElement::stopPeriodicTimers()
{
    m_progressEventTimer.stop();
    m_playbackProgressTimer.stop();
}

void HTMLMediaElement::userCancelledLoad()
{
    WTF_LOG(Media, "HTMLMediaElement::userCancelledLoad");

    // If the media data fetching process is aborted by the user:

    // 1 - The user agent should cancel the fetching process.
    clearMediaPlayer(-1);

    if (m_networkState == NETWORK_EMPTY || m_completelyLoaded)
        return;

    // 2 - Set the error attribute to a new MediaError object whose code attribute is set to MEDIA_ERR_ABORTED.
    m_error = MediaError::create(MediaError::MEDIA_ERR_ABORTED);

    // 3 - Queue a task to fire a simple event named error at the media element.
    scheduleEvent(EventTypeNames::abort);

    closeMediaSource();

    // 4 - If the media element's readyState attribute has a value equal to HAVE_NOTHING, set the
    // element's networkState attribute to the NETWORK_EMPTY value and queue a task to fire a
    // simple event named emptied at the element. Otherwise, set the element's networkState
    // attribute to the NETWORK_IDLE value.
    if (m_readyState == HAVE_NOTHING) {
        m_networkState = NETWORK_EMPTY;
        scheduleEvent(EventTypeNames::emptied);
    }
    else
        m_networkState = NETWORK_IDLE;

    // 5 - Set the element's delaying-the-load-event flag to false. This stops delaying the load event.
    setShouldDelayLoadEvent(false);

    // 6 - Abort the overall resource selection algorithm.
    m_currentSourceNode = nullptr;

    // Reset m_readyState since m_player is gone.
    m_readyState = HAVE_NOTHING;
    updateMediaController();
    if (RuntimeEnabledFeatures::videoTrackEnabled())
        updateActiveTextTrackCues(0);
}

void HTMLMediaElement::clearMediaPlayerAndAudioSourceProviderClient()
{
#if ENABLE(WEB_AUDIO)
    if (m_audioSourceNode)
        m_audioSourceNode->lock();

    if (audioSourceProvider())
        audioSourceProvider()->setClient(0);
#endif

    m_player.clear();

#if ENABLE(WEB_AUDIO)
    if (m_audioSourceNode)
        m_audioSourceNode->unlock();
#endif
}

void HTMLMediaElement::clearMediaPlayer(int flags)
{
    forgetResourceSpecificTracks();

    closeMediaSource();

    clearMediaPlayerAndAudioSourceProviderClient();

    stopPeriodicTimers();
    m_loadTimer.stop();

    m_pendingActionFlags &= ~flags;
    m_loadState = WaitingForSource;

    if (m_textTracks)
        configureTextTrackDisplay(AssumeNoVisibleChange);
}

void HTMLMediaElement::stop()
{
    WTF_LOG(Media, "HTMLMediaElement::stop");

    m_active = false;
    userCancelledLoad();

    // Stop the playback without generating events
    m_playing = false;
    setPausedInternal(true);

    if (renderer())
        renderer()->updateFromElement();

    stopPeriodicTimers();
    cancelPendingEventsAndCallbacks();

    m_asyncEventQueue->close();
}

bool HTMLMediaElement::hasPendingActivity() const
{
    return (hasAudio() && isPlaying()) || m_asyncEventQueue->hasPendingEvents();
}

void HTMLMediaElement::contextDestroyed()
{
    if (m_mediaController)
        m_mediaController->clearExecutionContext();
    ActiveDOMObject::contextDestroyed();
}

bool HTMLMediaElement::isFullscreen() const
{
    return FullscreenElementStack::isActiveFullScreenElement(this);
}

void HTMLMediaElement::enterFullscreen()
{
    WTF_LOG(Media, "HTMLMediaElement::enterFullscreen");

    if (document().settings() && document().settings()->fullScreenEnabled())
        FullscreenElementStack::from(document()).requestFullScreenForElement(this, 0, FullscreenElementStack::ExemptIFrameAllowFullScreenRequirement);
}

void HTMLMediaElement::didBecomeFullscreenElement()
{
    if (hasMediaControls())
        mediaControls()->enteredFullscreen();
    if (RuntimeEnabledFeatures::overlayFullscreenVideoEnabled() && isVideo())
        document().renderView()->compositor()->setCompositingLayersNeedRebuild();
}

void HTMLMediaElement::willStopBeingFullscreenElement()
{
    if (hasMediaControls())
        mediaControls()->exitedFullscreen();
    if (RuntimeEnabledFeatures::overlayFullscreenVideoEnabled() && isVideo())
        document().renderView()->compositor()->setCompositingLayersNeedRebuild();
}

blink::WebLayer* HTMLMediaElement::platformLayer() const
{
    return m_webLayer;
}

bool HTMLMediaElement::hasClosedCaptions() const
{
    if (RuntimeEnabledFeatures::videoTrackEnabled() && m_textTracks) {
        for (unsigned i = 0; i < m_textTracks->length(); ++i) {
            if (m_textTracks->item(i)->readinessState() == TextTrack::FailedToLoad)
                continue;

            if (m_textTracks->item(i)->kind() == TextTrack::captionsKeyword()
                || m_textTracks->item(i)->kind() == TextTrack::subtitlesKeyword())
                return true;
        }
    }
    return false;
}

bool HTMLMediaElement::closedCaptionsVisible() const
{
    return m_closedCaptionsVisible;
}

void HTMLMediaElement::updateTextTrackDisplay()
{
    WTF_LOG(Media, "HTMLMediaElement::updateTextTrackDisplay");

    if (!hasMediaControls() && !createMediaControls())
        return;

    mediaControls()->updateTextTrackDisplay();
}

void HTMLMediaElement::setClosedCaptionsVisible(bool closedCaptionVisible)
{
    WTF_LOG(Media, "HTMLMediaElement::setClosedCaptionsVisible(%s)", boolString(closedCaptionVisible));

    if (!m_player || !hasClosedCaptions())
        return;

    m_closedCaptionsVisible = closedCaptionVisible;

    if (RuntimeEnabledFeatures::videoTrackEnabled()) {
        m_processingPreferenceChange = true;
        markCaptionAndSubtitleTracksAsUnconfigured();
        m_processingPreferenceChange = false;

        updateTextTrackDisplay();
    }
}

unsigned HTMLMediaElement::webkitAudioDecodedByteCount() const
{
    if (!m_player)
        return 0;
    return m_player->audioDecodedByteCount();
}

unsigned HTMLMediaElement::webkitVideoDecodedByteCount() const
{
    if (!m_player)
        return 0;
    return m_player->videoDecodedByteCount();
}

bool HTMLMediaElement::isURLAttribute(const Attribute& attribute) const
{
    return attribute.name() == srcAttr || HTMLElement::isURLAttribute(attribute);
}

void HTMLMediaElement::setShouldDelayLoadEvent(bool shouldDelay)
{
    if (m_shouldDelayLoadEvent == shouldDelay)
        return;

    WTF_LOG(Media, "HTMLMediaElement::setShouldDelayLoadEvent(%s)", boolString(shouldDelay));

    m_shouldDelayLoadEvent = shouldDelay;
    if (shouldDelay)
        document().incrementLoadEventDelayCount();
    else
        document().decrementLoadEventDelayCount();
}


MediaControls* HTMLMediaElement::mediaControls() const
{
    return toMediaControls(userAgentShadowRoot()->firstChild());
}

bool HTMLMediaElement::hasMediaControls() const
{
    if (ShadowRoot* userAgent = userAgentShadowRoot()) {
        Node* node = userAgent->firstChild();
        ASSERT_WITH_SECURITY_IMPLICATION(!node || node->isMediaControls());
        return node;
    }

    return false;
}

bool HTMLMediaElement::createMediaControls()
{
    if (hasMediaControls())
        return true;

    RefPtr<MediaControls> mediaControls = MediaControls::create(*this);
    if (!mediaControls)
        return false;

    mediaControls->reset();
    if (isFullscreen())
        mediaControls->enteredFullscreen();

    ensureUserAgentShadowRoot().appendChild(mediaControls);

    if (!controls() || !inDocument())
        mediaControls->hide();

    return true;
}

void HTMLMediaElement::configureMediaControls()
{
    if (!controls() || !inDocument()) {
        if (hasMediaControls())
            mediaControls()->hide();
        return;
    }

    if (!hasMediaControls() && !createMediaControls())
        return;

    mediaControls()->reset();
    mediaControls()->show();
}

void HTMLMediaElement::configureTextTrackDisplay(VisibilityChangeAssumption assumption)
{
    ASSERT(m_textTracks);
    WTF_LOG(Media, "HTMLMediaElement::configureTextTrackDisplay");

    if (m_processingPreferenceChange)
        return;

    bool haveVisibleTextTrack = false;
    for (unsigned i = 0; i < m_textTracks->length(); ++i) {
        if (m_textTracks->item(i)->mode() == TextTrack::showingKeyword()) {
            haveVisibleTextTrack = true;
            break;
        }
    }

    if (assumption == AssumeNoVisibleChange
        && m_haveVisibleTextTrack == haveVisibleTextTrack) {
        updateActiveTextTrackCues(currentTime());
        return;
    }
    m_haveVisibleTextTrack = haveVisibleTextTrack;
    m_closedCaptionsVisible = m_haveVisibleTextTrack;

    if (!m_haveVisibleTextTrack && !hasMediaControls())
        return;
    if (!hasMediaControls() && !createMediaControls())
        return;

    mediaControls()->changedClosedCaptionsVisibility();

    if (RuntimeEnabledFeatures::videoTrackEnabled()) {
        updateActiveTextTrackCues(currentTime());
        updateTextTrackDisplay();
    }
}

void HTMLMediaElement::markCaptionAndSubtitleTracksAsUnconfigured()
{
    if (!m_textTracks)
        return;

    // Mark all tracks as not "configured" so that configureTextTracks()
    // will reconsider which tracks to display in light of new user preferences
    // (e.g. default tracks should not be displayed if the user has turned off
    // captions and non-default tracks should be displayed based on language
    // preferences if the user has turned captions on).
    for (unsigned i = 0; i < m_textTracks->length(); ++i) {
        RefPtr<TextTrack> textTrack = m_textTracks->item(i);
        String kind = textTrack->kind();

        if (kind == TextTrack::subtitlesKeyword() || kind == TextTrack::captionsKeyword())
            textTrack->setHasBeenConfigured(false);
    }
    configureTextTracks();
}


void* HTMLMediaElement::preDispatchEventHandler(Event* event)
{
    if (event && event->type() == EventTypeNames::webkitfullscreenchange)
        configureMediaControls();

    return 0;
}

void HTMLMediaElement::createMediaPlayer()
{
#if ENABLE(WEB_AUDIO)
    if (m_audioSourceNode)
        m_audioSourceNode->lock();
#endif

    if (m_mediaSource)
        closeMediaSource();

    m_player = MediaPlayer::create(this);

#if ENABLE(WEB_AUDIO)
    if (m_audioSourceNode) {
        // When creating the player, make sure its AudioSourceProvider knows about the MediaElementAudioSourceNode.
        if (audioSourceProvider())
            audioSourceProvider()->setClient(m_audioSourceNode);

        m_audioSourceNode->unlock();
    }
#endif
}

#if ENABLE(WEB_AUDIO)
void HTMLMediaElement::setAudioSourceNode(MediaElementAudioSourceNode* sourceNode)
{
    m_audioSourceNode = sourceNode;

    if (m_audioSourceNode)
        m_audioSourceNode->lock();

    if (audioSourceProvider())
        audioSourceProvider()->setClient(m_audioSourceNode);

    if (m_audioSourceNode)
        m_audioSourceNode->unlock();
}

AudioSourceProvider* HTMLMediaElement::audioSourceProvider()
{
    if (m_player)
        return m_player->audioSourceProvider();

    return 0;
}
#endif

const AtomicString& HTMLMediaElement::mediaGroup() const
{
    return fastGetAttribute(mediagroupAttr);
}

void HTMLMediaElement::setMediaGroup(const AtomicString& group)
{
    // When a media element is created with a mediagroup attribute, and when a media element's mediagroup
    // attribute is set, changed, or removed, the user agent must run the following steps:
    // 1. Let m [this] be the media element in question.
    // 2. Let m have no current media controller, if it currently has one.
    setControllerInternal(nullptr);

    // 3. If m's mediagroup attribute is being removed, then abort these steps.
    if (group.isNull() || group.isEmpty())
        return;

    // 4. If there is another media element whose Document is the same as m's Document (even if one or both
    // of these elements are not actually in the Document),
    HashSet<HTMLMediaElement*> elements = documentToElementSetMap().get(&document());
    for (HashSet<HTMLMediaElement*>::iterator i = elements.begin(); i != elements.end(); ++i) {
        if (*i == this)
            continue;

        // and which also has a mediagroup attribute, and whose mediagroup attribute has the same value as
        // the new value of m's mediagroup attribute,
        if ((*i)->mediaGroup() == group) {
            //  then let controller be that media element's current media controller.
            setControllerInternal((*i)->controller());
            return;
        }
    }

    // Otherwise, let controller be a newly created MediaController.
    setControllerInternal(MediaController::create(Node::executionContext()));
}

MediaController* HTMLMediaElement::controller() const
{
    return m_mediaController.get();
}

void HTMLMediaElement::setController(PassRefPtr<MediaController> controller)
{
    // 4.8.10.11.2 Media controllers: controller attribute.
    // On setting, it must first remove the element's mediagroup attribute, if any,
    removeAttribute(mediagroupAttr);
    // and then set the current media controller to the given value.
    setControllerInternal(controller);
}

void HTMLMediaElement::setControllerInternal(PassRefPtr<MediaController> controller)
{
    if (m_mediaController)
        m_mediaController->removeMediaElement(this);

    m_mediaController = controller;

    if (m_mediaController) {
        UseCounter::count(document(), UseCounter::HTMLMediaElementControllerNotNull);
        m_mediaController->addMediaElement(this);
    }
}

void HTMLMediaElement::updateMediaController()
{
    if (m_mediaController)
        m_mediaController->reportControllerState();
}

bool HTMLMediaElement::isBlocked() const
{
    // A media element is a blocked media element if its readyState attribute is in the
    // HAVE_NOTHING state, the HAVE_METADATA state, or the HAVE_CURRENT_DATA state,
    if (m_readyState <= HAVE_CURRENT_DATA)
        return true;

    // or if the element has paused for user interaction.
    return pausedForUserInteraction();
}

bool HTMLMediaElement::isBlockedOnMediaController() const
{
    if (!m_mediaController)
        return false;

    // A media element is blocked on its media controller if the MediaController is a blocked
    // media controller,
    if (m_mediaController->isBlocked())
        return true;

    // or if its media controller position is either before the media resource's earliest possible
    // position relative to the MediaController's timeline or after the end of the media resource
    // relative to the MediaController's timeline.
    double mediaControllerPosition = m_mediaController->currentTime();
    if (mediaControllerPosition < 0 || mediaControllerPosition > duration())
        return true;

    return false;
}

void HTMLMediaElement::prepareMediaFragmentURI()
{
    MediaFragmentURIParser fragmentParser(m_currentSrc);
    double dur = duration();

    double start = fragmentParser.startTime();
    if (start != MediaFragmentURIParser::invalidTimeValue() && start > 0) {
        m_fragmentStartTime = start;
        if (m_fragmentStartTime > dur)
            m_fragmentStartTime = dur;
    } else
        m_fragmentStartTime = MediaPlayer::invalidTime();

    double end = fragmentParser.endTime();
    if (end != MediaFragmentURIParser::invalidTimeValue() && end > 0 && end > m_fragmentStartTime) {
        m_fragmentEndTime = end;
        if (m_fragmentEndTime > dur)
            m_fragmentEndTime = dur;
    } else
        m_fragmentEndTime = MediaPlayer::invalidTime();

    if (m_fragmentStartTime != MediaPlayer::invalidTime() && m_readyState < HAVE_FUTURE_DATA)
        prepareToPlay();
}

void HTMLMediaElement::applyMediaFragmentURI()
{
    if (m_fragmentStartTime != MediaPlayer::invalidTime()) {
        m_sentEndEvent = false;
        UseCounter::count(document(), UseCounter::HTMLMediaElementSeekToFragmentStart);
        seek(m_fragmentStartTime, IGNORE_EXCEPTION);
    }
}

MediaPlayerClient::CORSMode HTMLMediaElement::mediaPlayerCORSMode() const
{
    const AtomicString& crossOriginMode = fastGetAttribute(crossoriginAttr);
    if (crossOriginMode.isNull())
        return Unspecified;
    if (equalIgnoringCase(crossOriginMode, "use-credentials"))
        return UseCredentials;
    return Anonymous;
}

void HTMLMediaElement::mediaPlayerSetWebLayer(blink::WebLayer* webLayer)
{
    if (webLayer == m_webLayer)
        return;

    // If either of the layers is null we need to enable or disable compositing. This is done by triggering a style recalc.
    if (!m_webLayer || !webLayer)
        scheduleLayerUpdate();

    if (m_webLayer)
        GraphicsLayer::unregisterContentsLayer(m_webLayer);
    m_webLayer = webLayer;
    if (m_webLayer) {
        m_webLayer->setOpaque(m_opaque);
        GraphicsLayer::registerContentsLayer(m_webLayer);
    }
}

void HTMLMediaElement::mediaPlayerSetOpaque(bool opaque)
{
    m_opaque = opaque;
    if (m_webLayer)
        m_webLayer->setOpaque(m_opaque);
}

void HTMLMediaElement::mediaPlayerMediaSourceOpened(blink::WebMediaSource* webMediaSource)
{
    m_mediaSource->setWebMediaSourceAndOpen(adoptPtr(webMediaSource));
}

bool HTMLMediaElement::isInteractiveContent() const
{
    return fastHasAttribute(controlsAttr);
}

}
