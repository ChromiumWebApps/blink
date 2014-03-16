/*
 * Copyright (C) 2007, 2008, 2009, 2010 Apple Inc. All rights reserved.
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
#include "core/html/HTMLVideoElement.h"

#include "CSSPropertyNames.h"
#include "HTMLNames.h"
#include "bindings/v8/ExceptionState.h"
#include "core/dom/Attribute.h"
#include "core/dom/Document.h"
#include "core/dom/ExceptionCode.h"
#include "core/html/HTMLImageLoader.h"
#include "core/html/canvas/CanvasRenderingContext.h"
#include "core/html/parser/HTMLParserIdioms.h"
#include "core/frame/Settings.h"
#include "core/rendering/RenderImage.h"
#include "core/rendering/RenderVideo.h"
#include "platform/UserGestureIndicator.h"

namespace WebCore {

using namespace HTMLNames;

inline HTMLVideoElement::HTMLVideoElement(Document& document)
    : HTMLMediaElement(videoTag, document)
{
    ScriptWrappable::init(this);
    if (document.settings())
        m_defaultPosterURL = AtomicString(document.settings()->defaultVideoPosterURL());
}

PassRefPtr<HTMLVideoElement> HTMLVideoElement::create(Document& document)
{
    RefPtr<HTMLVideoElement> videoElement(adoptRef(new HTMLVideoElement(document)));
    videoElement->suspendIfNeeded();
    return videoElement.release();
}

bool HTMLVideoElement::rendererIsNeeded(const RenderStyle& style)
{
    return HTMLElement::rendererIsNeeded(style);
}

RenderObject* HTMLVideoElement::createRenderer(RenderStyle*)
{
    return new RenderVideo(this);
}

void HTMLVideoElement::attach(const AttachContext& context)
{
    HTMLMediaElement::attach(context);

    updateDisplayState();
    if (shouldDisplayPosterImage()) {
        if (!m_imageLoader)
            m_imageLoader = adoptPtr(new HTMLImageLoader(this));
        m_imageLoader->updateFromElement();
        if (renderer())
            toRenderImage(renderer())->imageResource()->setImageResource(m_imageLoader->image());
    }
}

void HTMLVideoElement::collectStyleForPresentationAttribute(const QualifiedName& name, const AtomicString& value, MutableStylePropertySet* style)
{
    if (name == widthAttr)
        addHTMLLengthToStyle(style, CSSPropertyWidth, value);
    else if (name == heightAttr)
        addHTMLLengthToStyle(style, CSSPropertyHeight, value);
    else
        HTMLMediaElement::collectStyleForPresentationAttribute(name, value, style);
}

bool HTMLVideoElement::isPresentationAttribute(const QualifiedName& name) const
{
    if (name == widthAttr || name == heightAttr)
        return true;
    return HTMLMediaElement::isPresentationAttribute(name);
}

void HTMLVideoElement::parseAttribute(const QualifiedName& name, const AtomicString& value)
{
    if (name == posterAttr) {
        // Force a poster recalc by setting m_displayMode to Unknown directly before calling updateDisplayState.
        HTMLMediaElement::setDisplayMode(Unknown);
        updateDisplayState();
        if (shouldDisplayPosterImage()) {
            if (!m_imageLoader)
                m_imageLoader = adoptPtr(new HTMLImageLoader(this));
            m_imageLoader->updateFromElementIgnoringPreviousError();
        } else {
            if (renderer())
                toRenderImage(renderer())->imageResource()->setImageResource(0);
        }
        // Notify the player when the poster image URL changes.
        if (player())
            player()->setPoster(posterImageURL());
    } else
        HTMLMediaElement::parseAttribute(name, value);
}

unsigned HTMLVideoElement::videoWidth() const
{
    if (!player())
        return 0;
    return player()->naturalSize().width();
}

unsigned HTMLVideoElement::videoHeight() const
{
    if (!player())
        return 0;
    return player()->naturalSize().height();
}

bool HTMLVideoElement::isURLAttribute(const Attribute& attribute) const
{
    return attribute.name() == posterAttr || HTMLMediaElement::isURLAttribute(attribute);
}

const AtomicString HTMLVideoElement::imageSourceURL() const
{
    const AtomicString& url = getAttribute(posterAttr);
    if (!stripLeadingAndTrailingHTMLSpaces(url).isEmpty())
        return url;
    return m_defaultPosterURL;
}

void HTMLVideoElement::setDisplayMode(DisplayMode mode)
{
    DisplayMode oldMode = displayMode();
    KURL poster = posterImageURL();

    if (!poster.isEmpty()) {
        // We have a poster path, but only show it until the user triggers display by playing or seeking and the
        // media engine has something to display.
        if (mode == Video && !hasAvailableVideoFrame())
            mode = PosterWaitingForVideo;
    }

    HTMLMediaElement::setDisplayMode(mode);

    if (renderer() && displayMode() != oldMode)
        renderer()->updateFromElement();
}

void HTMLVideoElement::updateDisplayState()
{
    if (posterImageURL().isEmpty())
        setDisplayMode(Video);
    else if (displayMode() < Poster)
        setDisplayMode(Poster);
}

void HTMLVideoElement::paintCurrentFrameInContext(GraphicsContext* context, const IntRect& destRect) const
{
    MediaPlayer* player = HTMLMediaElement::player();
    if (!player)
        return;
    player->paint(context, destRect);
}

bool HTMLVideoElement::copyVideoTextureToPlatformTexture(blink::WebGraphicsContext3D* context, Platform3DObject texture, GLint level, GLenum type, GLenum internalFormat, bool premultiplyAlpha, bool flipY)
{
    if (!player())
        return false;
    return player()->copyVideoTextureToPlatformTexture(context, texture, level, type, internalFormat, premultiplyAlpha, flipY);
}

bool HTMLVideoElement::hasAvailableVideoFrame() const
{
    if (!player())
        return false;

    return player()->hasVideo() && player()->readyState() >= MediaPlayer::HaveCurrentData;
}

void HTMLVideoElement::didMoveToNewDocument(Document& oldDocument)
{
    if (m_imageLoader)
        m_imageLoader->elementDidMoveToNewDocument();
    HTMLMediaElement::didMoveToNewDocument(oldDocument);
}

unsigned HTMLVideoElement::webkitDecodedFrameCount() const
{
    if (!player())
        return 0;

    return player()->decodedFrameCount();
}

unsigned HTMLVideoElement::webkitDroppedFrameCount() const
{
    if (!player())
        return 0;

    return player()->droppedFrameCount();
}

KURL HTMLVideoElement::posterImageURL() const
{
    String url = stripLeadingAndTrailingHTMLSpaces(imageSourceURL());
    if (url.isEmpty())
        return KURL();
    return document().completeURL(url);
}

KURL HTMLVideoElement::mediaPlayerPosterURL()
{
    return posterImageURL();
}

PassRefPtr<Image> HTMLVideoElement::getSourceImageForCanvas(SourceImageMode mode, SourceImageStatus* status) const
{
    if (!hasAvailableVideoFrame()) {
        *status = InvalidSourceImageStatus;
        return nullptr;
    }

    IntSize intrinsicSize(videoWidth(), videoHeight());
    OwnPtr<ImageBuffer> imageBuffer = ImageBuffer::create(intrinsicSize);
    if (!imageBuffer) {
        *status = InvalidSourceImageStatus;
        return nullptr;
    }

    paintCurrentFrameInContext(imageBuffer->context(), IntRect(IntPoint(0, 0), intrinsicSize));

    *status = NormalSourceImageStatus;
    return imageBuffer->copyImage(mode == CopySourceImageIfVolatile ? CopyBackingStore : DontCopyBackingStore, Unscaled);
}

bool HTMLVideoElement::wouldTaintOrigin(SecurityOrigin* destinationSecurityOrigin) const
{
    return !hasSingleSecurityOrigin() || (!(player() && player()->didPassCORSAccessCheck()) && destinationSecurityOrigin->taintsCanvas(currentSrc()));
}

FloatSize HTMLVideoElement::sourceSize() const
{
    return FloatSize(videoWidth(), videoHeight());
}

}
