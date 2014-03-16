/*
 * Copyright (C) 2008, 2009, 2010, 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "core/html/shadow/MediaControlElements.h"

#include "RuntimeEnabledFeatures.h"
#include "bindings/v8/ExceptionStatePlaceholder.h"
#include "core/dom/DOMTokenList.h"
#include "core/dom/FullscreenElementStack.h"
#include "core/dom/shadow/ShadowRoot.h"
#include "core/events/MouseEvent.h"
#include "core/events/ThreadLocalEventNames.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Settings.h"
#include "core/html/HTMLVideoElement.h"
#include "core/html/shadow/MediaControls.h"
#include "core/html/track/TextTrack.h"
#include "core/html/track/vtt/VTTRegionList.h"
#include "core/page/EventHandler.h"
#include "core/rendering/RenderMediaControlElements.h"
#include "core/rendering/RenderSlider.h"
#include "core/rendering/RenderTheme.h"
#include "core/rendering/RenderVideo.h"

namespace WebCore {

using namespace HTMLNames;

static const AtomicString& getMediaControlCurrentTimeDisplayElementShadowPseudoId();
static const AtomicString& getMediaControlTimeRemainingDisplayElementShadowPseudoId();

static const double fadeInDuration = 0.1;
static const double fadeOutDuration = 0.3;

MediaControlPanelElement::MediaControlPanelElement(MediaControls& mediaControls)
    : MediaControlDivElement(mediaControls, MediaControlsPanel)
    , m_canBeDragged(false)
    , m_isBeingDragged(false)
    , m_isDisplayed(false)
    , m_opaque(true)
    , m_transitionTimer(this, &MediaControlPanelElement::transitionTimerFired)
{
}

PassRefPtr<MediaControlPanelElement> MediaControlPanelElement::create(MediaControls& mediaControls)
{
    return adoptRef(new MediaControlPanelElement(mediaControls));
}

const AtomicString& MediaControlPanelElement::shadowPseudoId() const
{
    DEFINE_STATIC_LOCAL(AtomicString, id, ("-webkit-media-controls-panel", AtomicString::ConstructFromLiteral));
    return id;
}

void MediaControlPanelElement::startDrag(const LayoutPoint& eventLocation)
{
    if (!m_canBeDragged)
        return;

    if (m_isBeingDragged)
        return;

    RenderObject* renderer = this->renderer();
    if (!renderer || !renderer->isBox())
        return;

    LocalFrame* frame = document().frame();
    if (!frame)
        return;

    m_lastDragEventLocation = eventLocation;

    frame->eventHandler().setCapturingMouseEventsNode(this);

    m_isBeingDragged = true;
}

void MediaControlPanelElement::continueDrag(const LayoutPoint& eventLocation)
{
    if (!m_isBeingDragged)
        return;

    LayoutSize distanceDragged = eventLocation - m_lastDragEventLocation;
    m_cumulativeDragOffset.move(distanceDragged);
    m_lastDragEventLocation = eventLocation;
    setPosition(m_cumulativeDragOffset);
}

void MediaControlPanelElement::endDrag()
{
    if (!m_isBeingDragged)
        return;

    m_isBeingDragged = false;

    LocalFrame* frame = document().frame();
    if (!frame)
        return;

    frame->eventHandler().setCapturingMouseEventsNode(nullptr);
}

void MediaControlPanelElement::startTimer()
{
    stopTimer();

    // The timer is required to set the property display:'none' on the panel,
    // such that captions are correctly displayed at the bottom of the video
    // at the end of the fadeout transition.
    // FIXME: Racing a transition with a setTimeout like this is wrong.
    m_transitionTimer.startOneShot(fadeOutDuration, FROM_HERE);
}

void MediaControlPanelElement::stopTimer()
{
    if (m_transitionTimer.isActive())
        m_transitionTimer.stop();
}

void MediaControlPanelElement::transitionTimerFired(Timer<MediaControlPanelElement>*)
{
    if (!m_opaque)
        hide();

    stopTimer();
}

void MediaControlPanelElement::setPosition(const LayoutPoint& position)
{
    // FIXME: Do we really want to up-convert these to doubles and not round? crbug.com/350474
    double left = position.x().toFloat();
    double top = position.y().toFloat();

    // Set the left and top to control the panel's position; this depends on it being absolute positioned.
    // Set the margin to zero since the position passed in will already include the effect of the margin.
    setInlineStyleProperty(CSSPropertyLeft, left, CSSPrimitiveValue::CSS_PX);
    setInlineStyleProperty(CSSPropertyTop, top, CSSPrimitiveValue::CSS_PX);
    setInlineStyleProperty(CSSPropertyMarginLeft, 0.0, CSSPrimitiveValue::CSS_PX);
    setInlineStyleProperty(CSSPropertyMarginTop, 0.0, CSSPrimitiveValue::CSS_PX);

    classList().add("dragged", IGNORE_EXCEPTION);
}

void MediaControlPanelElement::resetPosition()
{
    removeInlineStyleProperty(CSSPropertyLeft);
    removeInlineStyleProperty(CSSPropertyTop);
    removeInlineStyleProperty(CSSPropertyMarginLeft);
    removeInlineStyleProperty(CSSPropertyMarginTop);

    classList().remove("dragged", IGNORE_EXCEPTION);

    m_cumulativeDragOffset.setX(0);
    m_cumulativeDragOffset.setY(0);
}

void MediaControlPanelElement::makeOpaque()
{
    if (m_opaque)
        return;

    setInlineStyleProperty(CSSPropertyTransitionProperty, CSSPropertyOpacity);
    setInlineStyleProperty(CSSPropertyTransitionDuration, fadeInDuration, CSSPrimitiveValue::CSS_S);
    setInlineStyleProperty(CSSPropertyOpacity, 1.0, CSSPrimitiveValue::CSS_NUMBER);

    m_opaque = true;

    if (m_isDisplayed)
        show();
}

void MediaControlPanelElement::makeTransparent()
{
    if (!m_opaque)
        return;

    setInlineStyleProperty(CSSPropertyTransitionProperty, CSSPropertyOpacity);
    setInlineStyleProperty(CSSPropertyTransitionDuration, fadeOutDuration, CSSPrimitiveValue::CSS_S);
    setInlineStyleProperty(CSSPropertyOpacity, 0.0, CSSPrimitiveValue::CSS_NUMBER);

    m_opaque = false;
    startTimer();
}

void MediaControlPanelElement::defaultEventHandler(Event* event)
{
    MediaControlDivElement::defaultEventHandler(event);

    if (event->isMouseEvent()) {
        LayoutPoint location = toMouseEvent(event)->absoluteLocation();
        if (event->type() == EventTypeNames::mousedown && event->target() == this) {
            startDrag(location);
            event->setDefaultHandled();
        } else if (event->type() == EventTypeNames::mousemove && m_isBeingDragged)
            continueDrag(location);
        else if (event->type() == EventTypeNames::mouseup && m_isBeingDragged) {
            continueDrag(location);
            endDrag();
            event->setDefaultHandled();
        }
    }
}

void MediaControlPanelElement::setCanBeDragged(bool canBeDragged)
{
    if (m_canBeDragged == canBeDragged)
        return;

    m_canBeDragged = canBeDragged;

    if (!canBeDragged)
        endDrag();
}

void MediaControlPanelElement::setIsDisplayed(bool isDisplayed)
{
    m_isDisplayed = isDisplayed;
}

// ----------------------------

MediaControlPanelEnclosureElement::MediaControlPanelEnclosureElement(MediaControls& mediaControls)
    // Mapping onto same MediaControlElementType as panel element, since it has similar properties.
    : MediaControlDivElement(mediaControls, MediaControlsPanel)
{
}

PassRefPtr<MediaControlPanelEnclosureElement> MediaControlPanelEnclosureElement::create(MediaControls& mediaControls)
{
    return adoptRef(new MediaControlPanelEnclosureElement(mediaControls));
}

const AtomicString& MediaControlPanelEnclosureElement::shadowPseudoId() const
{
    DEFINE_STATIC_LOCAL(AtomicString, id, ("-webkit-media-controls-enclosure", AtomicString::ConstructFromLiteral));
    return id;
}

// ----------------------------

MediaControlOverlayEnclosureElement::MediaControlOverlayEnclosureElement(MediaControls& mediaControls)
    // Mapping onto same MediaControlElementType as panel element, since it has similar properties.
    : MediaControlDivElement(mediaControls, MediaControlsPanel)
{
}

PassRefPtr<MediaControlOverlayEnclosureElement> MediaControlOverlayEnclosureElement::create(MediaControls& mediaControls)
{
    return adoptRef(new MediaControlOverlayEnclosureElement(mediaControls));
}

const AtomicString& MediaControlOverlayEnclosureElement::shadowPseudoId() const
{
    DEFINE_STATIC_LOCAL(AtomicString, id, ("-webkit-media-controls-overlay-enclosure", AtomicString::ConstructFromLiteral));
    return id;
}

// ----------------------------

MediaControlMuteButtonElement::MediaControlMuteButtonElement(MediaControls& mediaControls)
    : MediaControlInputElement(mediaControls, MediaMuteButton)
{
}

PassRefPtr<MediaControlMuteButtonElement> MediaControlMuteButtonElement::create(MediaControls& mediaControls)
{
    RefPtr<MediaControlMuteButtonElement> button = adoptRef(new MediaControlMuteButtonElement(mediaControls));
    button->ensureUserAgentShadowRoot();
    button->setType("button");
    return button.release();
}

void MediaControlMuteButtonElement::defaultEventHandler(Event* event)
{
    if (event->type() == EventTypeNames::click) {
        mediaControllerInterface().setMuted(!mediaControllerInterface().muted());
        event->setDefaultHandled();
    }

    HTMLInputElement::defaultEventHandler(event);
}

void MediaControlMuteButtonElement::updateDisplayType()
{
    setDisplayType(mediaControllerInterface().muted() ? MediaUnMuteButton : MediaMuteButton);
}

const AtomicString& MediaControlMuteButtonElement::shadowPseudoId() const
{
    DEFINE_STATIC_LOCAL(AtomicString, id, ("-webkit-media-controls-mute-button", AtomicString::ConstructFromLiteral));
    return id;
}

// ----------------------------

MediaControlPlayButtonElement::MediaControlPlayButtonElement(MediaControls& mediaControls)
    : MediaControlInputElement(mediaControls, MediaPlayButton)
{
}

PassRefPtr<MediaControlPlayButtonElement> MediaControlPlayButtonElement::create(MediaControls& mediaControls)
{
    RefPtr<MediaControlPlayButtonElement> button = adoptRef(new MediaControlPlayButtonElement(mediaControls));
    button->ensureUserAgentShadowRoot();
    button->setType("button");
    return button.release();
}

void MediaControlPlayButtonElement::defaultEventHandler(Event* event)
{
    if (event->type() == EventTypeNames::click) {
        if (mediaControllerInterface().canPlay())
            mediaControllerInterface().play();
        else
            mediaControllerInterface().pause();
        updateDisplayType();
        event->setDefaultHandled();
    }
    HTMLInputElement::defaultEventHandler(event);
}

void MediaControlPlayButtonElement::updateDisplayType()
{
    setDisplayType(mediaControllerInterface().canPlay() ? MediaPlayButton : MediaPauseButton);
}

const AtomicString& MediaControlPlayButtonElement::shadowPseudoId() const
{
    DEFINE_STATIC_LOCAL(AtomicString, id, ("-webkit-media-controls-play-button", AtomicString::ConstructFromLiteral));
    return id;
}

// ----------------------------

MediaControlOverlayPlayButtonElement::MediaControlOverlayPlayButtonElement(MediaControls& mediaControls)
    : MediaControlInputElement(mediaControls, MediaOverlayPlayButton)
{
}

PassRefPtr<MediaControlOverlayPlayButtonElement> MediaControlOverlayPlayButtonElement::create(MediaControls& mediaControls)
{
    RefPtr<MediaControlOverlayPlayButtonElement> button = adoptRef(new MediaControlOverlayPlayButtonElement(mediaControls));
    button->ensureUserAgentShadowRoot();
    button->setType("button");
    return button.release();
}

void MediaControlOverlayPlayButtonElement::defaultEventHandler(Event* event)
{
    if (event->type() == EventTypeNames::click && mediaControllerInterface().canPlay()) {
        mediaControllerInterface().play();
        updateDisplayType();
        event->setDefaultHandled();
    }
    HTMLInputElement::defaultEventHandler(event);
}

void MediaControlOverlayPlayButtonElement::updateDisplayType()
{
    if (mediaControllerInterface().canPlay()) {
        show();
    } else
        hide();
}

const AtomicString& MediaControlOverlayPlayButtonElement::shadowPseudoId() const
{
    DEFINE_STATIC_LOCAL(AtomicString, id, ("-webkit-media-controls-overlay-play-button", AtomicString::ConstructFromLiteral));
    return id;
}


// ----------------------------

MediaControlToggleClosedCaptionsButtonElement::MediaControlToggleClosedCaptionsButtonElement(MediaControls& mediaControls)
    : MediaControlInputElement(mediaControls, MediaShowClosedCaptionsButton)
{
}

PassRefPtr<MediaControlToggleClosedCaptionsButtonElement> MediaControlToggleClosedCaptionsButtonElement::create(MediaControls& mediaControls)
{
    RefPtr<MediaControlToggleClosedCaptionsButtonElement> button = adoptRef(new MediaControlToggleClosedCaptionsButtonElement(mediaControls));
    button->ensureUserAgentShadowRoot();
    button->setType("button");
    button->hide();
    return button.release();
}

void MediaControlToggleClosedCaptionsButtonElement::updateDisplayType()
{
    bool captionsVisible = mediaControllerInterface().closedCaptionsVisible();
    setDisplayType(captionsVisible ? MediaHideClosedCaptionsButton : MediaShowClosedCaptionsButton);
    setChecked(captionsVisible);
}

void MediaControlToggleClosedCaptionsButtonElement::defaultEventHandler(Event* event)
{
    if (event->type() == EventTypeNames::click) {
        mediaControllerInterface().setClosedCaptionsVisible(!mediaControllerInterface().closedCaptionsVisible());
        setChecked(mediaControllerInterface().closedCaptionsVisible());
        updateDisplayType();
        event->setDefaultHandled();
    }

    HTMLInputElement::defaultEventHandler(event);
}

const AtomicString& MediaControlToggleClosedCaptionsButtonElement::shadowPseudoId() const
{
    DEFINE_STATIC_LOCAL(AtomicString, id, ("-webkit-media-controls-toggle-closed-captions-button", AtomicString::ConstructFromLiteral));
    return id;
}

// ----------------------------

MediaControlTimelineElement::MediaControlTimelineElement(MediaControls& mediaControls)
    : MediaControlInputElement(mediaControls, MediaSlider)
{
}

PassRefPtr<MediaControlTimelineElement> MediaControlTimelineElement::create(MediaControls& mediaControls)
{
    RefPtr<MediaControlTimelineElement> timeline = adoptRef(new MediaControlTimelineElement(mediaControls));
    timeline->ensureUserAgentShadowRoot();
    timeline->setType("range");
    timeline->setAttribute(stepAttr, "any");
    return timeline.release();
}

void MediaControlTimelineElement::defaultEventHandler(Event* event)
{
    // Left button is 0. Rejects mouse events not from left button.
    if (event->isMouseEvent() && toMouseEvent(event)->button())
        return;

    if (!inDocument() || !document().isActive())
        return;

    if (event->type() == EventTypeNames::mousedown)
        mediaControllerInterface().beginScrubbing();

    if (event->type() == EventTypeNames::mouseup)
        mediaControllerInterface().endScrubbing();

    MediaControlInputElement::defaultEventHandler(event);

    if (event->type() == EventTypeNames::mouseover || event->type() == EventTypeNames::mouseout || event->type() == EventTypeNames::mousemove)
        return;

    double time = value().toDouble();
    if (event->type() == EventTypeNames::input && time != mediaControllerInterface().currentTime())
        mediaControllerInterface().setCurrentTime(time, IGNORE_EXCEPTION);

    RenderSlider* slider = toRenderSlider(renderer());
    if (slider && slider->inDragMode())
        mediaControls().updateCurrentTimeDisplay();
}

bool MediaControlTimelineElement::willRespondToMouseClickEvents()
{
    return inDocument() && document().isActive();
}

void MediaControlTimelineElement::setPosition(double currentTime)
{
    setValue(String::number(currentTime));
}

void MediaControlTimelineElement::setDuration(double duration)
{
    setFloatingPointAttribute(maxAttr, std::isfinite(duration) ? duration : 0);
}


const AtomicString& MediaControlTimelineElement::shadowPseudoId() const
{
    DEFINE_STATIC_LOCAL(AtomicString, id, ("-webkit-media-controls-timeline", AtomicString::ConstructFromLiteral));
    return id;
}

// ----------------------------

MediaControlVolumeSliderElement::MediaControlVolumeSliderElement(MediaControls& mediaControls)
    : MediaControlInputElement(mediaControls, MediaVolumeSlider)
{
}

PassRefPtr<MediaControlVolumeSliderElement> MediaControlVolumeSliderElement::create(MediaControls& mediaControls)
{
    RefPtr<MediaControlVolumeSliderElement> slider = adoptRef(new MediaControlVolumeSliderElement(mediaControls));
    slider->ensureUserAgentShadowRoot();
    slider->setType("range");
    slider->setAttribute(stepAttr, "any");
    slider->setAttribute(maxAttr, "1");
    return slider.release();
}

void MediaControlVolumeSliderElement::defaultEventHandler(Event* event)
{
    if (event->isMouseEvent() && toMouseEvent(event)->button() != LeftButton)
        return;

    if (!inDocument() || !document().isActive())
        return;

    MediaControlInputElement::defaultEventHandler(event);

    if (event->type() == EventTypeNames::mouseover || event->type() == EventTypeNames::mouseout || event->type() == EventTypeNames::mousemove)
        return;

    double volume = value().toDouble();
    mediaControllerInterface().setVolume(volume, ASSERT_NO_EXCEPTION);
    mediaControllerInterface().setMuted(false);
}

bool MediaControlVolumeSliderElement::willRespondToMouseMoveEvents()
{
    if (!inDocument() || !document().isActive())
        return false;

    return MediaControlInputElement::willRespondToMouseMoveEvents();
}

bool MediaControlVolumeSliderElement::willRespondToMouseClickEvents()
{
    if (!inDocument() || !document().isActive())
        return false;

    return MediaControlInputElement::willRespondToMouseClickEvents();
}

void MediaControlVolumeSliderElement::setVolume(double volume)
{
    if (value().toDouble() != volume)
        setValue(String::number(volume));
}

const AtomicString& MediaControlVolumeSliderElement::shadowPseudoId() const
{
    DEFINE_STATIC_LOCAL(AtomicString, id, ("-webkit-media-controls-volume-slider", AtomicString::ConstructFromLiteral));
    return id;
}

// ----------------------------

MediaControlFullscreenButtonElement::MediaControlFullscreenButtonElement(MediaControls& mediaControls)
    : MediaControlInputElement(mediaControls, MediaEnterFullscreenButton)
{
}

PassRefPtr<MediaControlFullscreenButtonElement> MediaControlFullscreenButtonElement::create(MediaControls& mediaControls)
{
    RefPtr<MediaControlFullscreenButtonElement> button = adoptRef(new MediaControlFullscreenButtonElement(mediaControls));
    button->ensureUserAgentShadowRoot();
    button->setType("button");
    button->hide();
    return button.release();
}

void MediaControlFullscreenButtonElement::defaultEventHandler(Event* event)
{
    if (event->type() == EventTypeNames::click) {
        // Only use the new full screen API if the fullScreenEnabled setting has
        // been explicitly enabled. Otherwise, use the old fullscreen API. This
        // allows apps which embed a WebView to retain the existing full screen
        // video implementation without requiring them to implement their own full
        // screen behavior.
        if (document().settings() && document().settings()->fullScreenEnabled()) {
            if (FullscreenElementStack::isActiveFullScreenElement(&mediaElement()))
                FullscreenElementStack::from(document()).webkitCancelFullScreen();
            else
                FullscreenElementStack::from(document()).requestFullScreenForElement(&mediaElement(), 0, FullscreenElementStack::ExemptIFrameAllowFullScreenRequirement);
        } else {
            mediaControllerInterface().enterFullscreen();
        }
        event->setDefaultHandled();
    }
    HTMLInputElement::defaultEventHandler(event);
}

const AtomicString& MediaControlFullscreenButtonElement::shadowPseudoId() const
{
    DEFINE_STATIC_LOCAL(AtomicString, id, ("-webkit-media-controls-fullscreen-button", AtomicString::ConstructFromLiteral));
    return id;
}

void MediaControlFullscreenButtonElement::setIsFullscreen(bool isFullscreen)
{
    setDisplayType(isFullscreen ? MediaExitFullscreenButton : MediaEnterFullscreenButton);
}

// ----------------------------

MediaControlTimeRemainingDisplayElement::MediaControlTimeRemainingDisplayElement(MediaControls& mediaControls)
    : MediaControlTimeDisplayElement(mediaControls, MediaTimeRemainingDisplay)
{
}

PassRefPtr<MediaControlTimeRemainingDisplayElement> MediaControlTimeRemainingDisplayElement::create(MediaControls& mediaControls)
{
    return adoptRef(new MediaControlTimeRemainingDisplayElement(mediaControls));
}

static const AtomicString& getMediaControlTimeRemainingDisplayElementShadowPseudoId()
{
    DEFINE_STATIC_LOCAL(AtomicString, id, ("-webkit-media-controls-time-remaining-display", AtomicString::ConstructFromLiteral));
    return id;
}

const AtomicString& MediaControlTimeRemainingDisplayElement::shadowPseudoId() const
{
    return getMediaControlTimeRemainingDisplayElementShadowPseudoId();
}

// ----------------------------

MediaControlCurrentTimeDisplayElement::MediaControlCurrentTimeDisplayElement(MediaControls& mediaControls)
    : MediaControlTimeDisplayElement(mediaControls, MediaCurrentTimeDisplay)
{
}

PassRefPtr<MediaControlCurrentTimeDisplayElement> MediaControlCurrentTimeDisplayElement::create(MediaControls& mediaControls)
{
    return adoptRef(new MediaControlCurrentTimeDisplayElement(mediaControls));
}

static const AtomicString& getMediaControlCurrentTimeDisplayElementShadowPseudoId()
{
    DEFINE_STATIC_LOCAL(AtomicString, id, ("-webkit-media-controls-current-time-display", AtomicString::ConstructFromLiteral));
    return id;
}

const AtomicString& MediaControlCurrentTimeDisplayElement::shadowPseudoId() const
{
    return getMediaControlCurrentTimeDisplayElementShadowPseudoId();
}

// ----------------------------

MediaControlTextTrackContainerElement::MediaControlTextTrackContainerElement(MediaControls& mediaControls)
    : MediaControlDivElement(mediaControls, MediaTextTrackDisplayContainer)
    , m_fontSize(0)
{
}

PassRefPtr<MediaControlTextTrackContainerElement> MediaControlTextTrackContainerElement::create(MediaControls& mediaControls)
{
    RefPtr<MediaControlTextTrackContainerElement> element = adoptRef(new MediaControlTextTrackContainerElement(mediaControls));
    element->hide();
    return element.release();
}

RenderObject* MediaControlTextTrackContainerElement::createRenderer(RenderStyle*)
{
    return new RenderTextTrackContainerElement(this);
}

const AtomicString& MediaControlTextTrackContainerElement::textTrackContainerElementShadowPseudoId()
{
    DEFINE_STATIC_LOCAL(AtomicString, id, ("-webkit-media-text-track-container", AtomicString::ConstructFromLiteral));
    return id;
}

const AtomicString& MediaControlTextTrackContainerElement::shadowPseudoId() const
{
    return textTrackContainerElementShadowPseudoId();
}

void MediaControlTextTrackContainerElement::updateDisplay()
{
    if (!mediaControllerInterface().closedCaptionsVisible()) {
        removeChildren();
        return;
    }

    // 1. If the media element is an audio element, or is another playback
    // mechanism with no rendering area, abort these steps. There is nothing to
    // render.
    if (!mediaElement().isVideo())
        return;

    // 2. Let video be the media element or other playback mechanism.
    HTMLVideoElement& video = toHTMLVideoElement(mediaElement());

    // 3. Let output be an empty list of absolutely positioned CSS block boxes.
    Vector<RefPtr<HTMLDivElement> > output;

    // 4. If the user agent is exposing a user interface for video, add to
    // output one or more completely transparent positioned CSS block boxes that
    // cover the same region as the user interface.

    // 5. If the last time these rules were run, the user agent was not exposing
    // a user interface for video, but now it is, let reset be true. Otherwise,
    // let reset be false.

    // There is nothing to be done explicitly for 4th and 5th steps, as
    // everything is handled through CSS. The caption box is on top of the
    // controls box, in a container set with the -webkit-box display property.

    // 6. Let tracks be the subset of video's list of text tracks that have as
    // their rules for updating the text track rendering these rules for
    // updating the display of WebVTT text tracks, and whose text track mode is
    // showing or showing by default.
    // 7. Let cues be an empty list of text track cues.
    // 8. For each track track in tracks, append to cues all the cues from
    // track's list of cues that have their text track cue active flag set.
    CueList activeCues = video.currentlyActiveCues();

    // 9. If reset is false, then, for each text track cue cue in cues: if cue's
    // text track cue display state has a set of CSS boxes, then add those boxes
    // to output, and remove cue from cues.

    // There is nothing explicitly to be done here, as all the caching occurs
    // within the TextTrackCue instance itself. If parameters of the cue change,
    // the display tree is cleared.

    // 10. For each text track cue cue in cues that has not yet had
    // corresponding CSS boxes added to output, in text track cue order, run the
    // following substeps:
    for (size_t i = 0; i < activeCues.size(); ++i) {
        TextTrackCue* cue = activeCues[i].data();

        ASSERT(cue->isActive());
        if (!cue->track() || !cue->track()->isRendered() || !cue->isActive())
            continue;

        cue->updateDisplay(m_videoDisplaySize.size(), *this);
    }

    // 11. Return output.
    if (hasChildren())
        show();
    else
        hide();
}

void MediaControlTextTrackContainerElement::updateSizes()
{
    if (!document().isActive())
        return;

    IntRect videoBox;

    if (!mediaElement().renderer() || !mediaElement().renderer()->isVideo())
        return;
    videoBox = toRenderVideo(mediaElement().renderer())->videoBox();

    if (m_videoDisplaySize == videoBox)
        return;
    m_videoDisplaySize = videoBox;

    float smallestDimension = std::min(m_videoDisplaySize.size().height(), m_videoDisplaySize.size().width());

    float fontSize = smallestDimension * 0.05f;
    if (fontSize != m_fontSize) {
        m_fontSize = fontSize;
        setInlineStyleProperty(CSSPropertyFontSize, fontSize, CSSPrimitiveValue::CSS_PX);
    }
}

// ----------------------------

} // namespace WebCore
