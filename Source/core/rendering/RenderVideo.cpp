/*
 * Copyright (C) 2007, 2008, 2009, 2010 Apple Inc.  All rights reserved.
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

#include "core/rendering/RenderVideo.h"

#include "HTMLNames.h"
#include "core/dom/Document.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/html/HTMLVideoElement.h"
#include "core/rendering/LayoutRectRecorder.h"
#include "core/rendering/PaintInfo.h"
#include "core/rendering/RenderFullScreen.h"
#include "platform/graphics/media/MediaPlayer.h"
#include "public/platform/WebLayer.h"

namespace WebCore {

using namespace HTMLNames;

RenderVideo::RenderVideo(HTMLVideoElement* video)
    : RenderMedia(video)
{
    setIntrinsicSize(calculateIntrinsicSize());
}

RenderVideo::~RenderVideo()
{
}

IntSize RenderVideo::defaultSize()
{
    // These values are specified in the spec.
    static const int cDefaultWidth = 300;
    static const int cDefaultHeight = 150;

    return IntSize(cDefaultWidth, cDefaultHeight);
}

void RenderVideo::intrinsicSizeChanged()
{
    if (videoElement()->shouldDisplayPosterImage())
        RenderMedia::intrinsicSizeChanged();
    updateIntrinsicSize();
}

void RenderVideo::updateIntrinsicSize()
{
    LayoutSize size = calculateIntrinsicSize();
    size.scale(style()->effectiveZoom());

    // Never set the element size to zero when in a media document.
    if (size.isEmpty() && node()->ownerDocument() && node()->ownerDocument()->isMediaDocument())
        return;

    if (size == intrinsicSize())
        return;

    setIntrinsicSize(size);
    setPreferredLogicalWidthsDirty();
    setNeedsLayout();
}

LayoutSize RenderVideo::calculateIntrinsicSize()
{
    HTMLVideoElement* video = videoElement();

    // Spec text from 4.8.6
    //
    // The intrinsic width of a video element's playback area is the intrinsic width
    // of the video resource, if that is available; otherwise it is the intrinsic
    // width of the poster frame, if that is available; otherwise it is 300 CSS pixels.
    //
    // The intrinsic height of a video element's playback area is the intrinsic height
    // of the video resource, if that is available; otherwise it is the intrinsic
    // height of the poster frame, if that is available; otherwise it is 150 CSS pixels.
    MediaPlayer* player = mediaElement()->player();
    if (player && video->readyState() >= HTMLVideoElement::HAVE_METADATA) {
        LayoutSize size = player->naturalSize();
        if (!size.isEmpty())
            return size;
    }

    if (video->shouldDisplayPosterImage() && !m_cachedImageSize.isEmpty() && !imageResource()->errorOccurred())
        return m_cachedImageSize;

    // <video> in standalone media documents should not use the default 300x150
    // size since they also have audio-only files. By setting the intrinsic
    // size to 300x1 the video will resize itself in these cases, and audio will
    // have the correct height (it needs to be > 0 for controls to render properly).
    if (video->ownerDocument() && video->ownerDocument()->isMediaDocument())
        return LayoutSize(defaultSize().width(), 1);

    return defaultSize();
}

void RenderVideo::imageChanged(WrappedImagePtr newImage, const IntRect* rect)
{
    RenderMedia::imageChanged(newImage, rect);

    // Cache the image intrinsic size so we can continue to use it to draw the image correctly
    // even if we know the video intrinsic size but aren't able to draw video frames yet
    // (we don't want to scale the poster to the video size without keeping aspect ratio).
    if (videoElement()->shouldDisplayPosterImage())
        m_cachedImageSize = intrinsicSize();

    // The intrinsic size is now that of the image, but in case we already had the
    // intrinsic size of the video we call this here to restore the video size.
    updateIntrinsicSize();
}

IntRect RenderVideo::videoBox() const
{
    const LayoutSize* overriddenIntrinsicSize = 0;
    if (videoElement()->shouldDisplayPosterImage())
        overriddenIntrinsicSize = &m_cachedImageSize;

    return pixelSnappedIntRect(replacedContentRect(overriddenIntrinsicSize));
}

bool RenderVideo::shouldDisplayVideo() const
{
    return !videoElement()->shouldDisplayPosterImage();
}

void RenderVideo::paintReplaced(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    MediaPlayer* mediaPlayer = mediaElement()->player();
    bool displayingPoster = videoElement()->shouldDisplayPosterImage();
    if (!displayingPoster && !mediaPlayer)
        return;

    LayoutRect rect = videoBox();
    if (rect.isEmpty())
        return;
    rect.moveBy(paintOffset);

    LayoutRect contentRect = contentBoxRect();
    contentRect.moveBy(paintOffset);
    GraphicsContext* context = paintInfo.context;
    bool clip = !contentRect.contains(rect);
    if (clip) {
        context->save();
        context->clip(contentRect);
    }

    if (displayingPoster)
        paintIntoRect(context, rect);
    else if ((document().view() && document().view()->paintBehavior() & PaintBehaviorFlattenCompositingLayers) || !acceleratedRenderingInUse())
        mediaPlayer->paint(context, pixelSnappedIntRect(rect));

    if (clip)
        context->restore();
}

bool RenderVideo::acceleratedRenderingInUse()
{
    blink::WebLayer* webLayer = mediaElement()->platformLayer();
    return webLayer && !webLayer->isOrphan();
}

void RenderVideo::layout()
{
    LayoutRectRecorder recorder(*this);
    updatePlayer();
    RenderMedia::layout();
}

HTMLVideoElement* RenderVideo::videoElement() const
{
    return toHTMLVideoElement(node());
}

void RenderVideo::updateFromElement()
{
    RenderMedia::updateFromElement();
    updatePlayer();
}

void RenderVideo::updatePlayer()
{
    updateIntrinsicSize();

    MediaPlayer* mediaPlayer = mediaElement()->player();
    if (!mediaPlayer)
        return;

    if (!videoElement()->isActive())
        return;

    contentChanged(VideoChanged);
}

LayoutUnit RenderVideo::computeReplacedLogicalWidth(ShouldComputePreferred shouldComputePreferred) const
{
    return RenderReplaced::computeReplacedLogicalWidth(shouldComputePreferred);
}

LayoutUnit RenderVideo::computeReplacedLogicalHeight() const
{
    return RenderReplaced::computeReplacedLogicalHeight();
}

LayoutUnit RenderVideo::minimumReplacedHeight() const
{
    return RenderReplaced::minimumReplacedHeight();
}

bool RenderVideo::supportsAcceleratedRendering() const
{
    return !!mediaElement()->platformLayer();
}

static const RenderBlock* rendererPlaceholder(const RenderObject* renderer)
{
    RenderObject* parent = renderer->parent();
    if (!parent)
        return 0;

    RenderFullScreen* fullScreen = parent->isRenderFullScreen() ? toRenderFullScreen(parent) : 0;
    if (!fullScreen)
        return 0;

    return fullScreen->placeholder();
}

LayoutUnit RenderVideo::offsetLeft() const
{
    if (const RenderBlock* block = rendererPlaceholder(this))
        return block->offsetLeft();
    return RenderMedia::offsetLeft();
}

LayoutUnit RenderVideo::offsetTop() const
{
    if (const RenderBlock* block = rendererPlaceholder(this))
        return block->offsetTop();
    return RenderMedia::offsetTop();
}

LayoutUnit RenderVideo::offsetWidth() const
{
    if (const RenderBlock* block = rendererPlaceholder(this))
        return block->offsetWidth();
    return RenderMedia::offsetWidth();
}

LayoutUnit RenderVideo::offsetHeight() const
{
    if (const RenderBlock* block = rendererPlaceholder(this))
        return block->offsetHeight();
    return RenderMedia::offsetHeight();
}

} // namespace WebCore
