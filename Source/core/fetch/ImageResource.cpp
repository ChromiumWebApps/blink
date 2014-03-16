/*
    Copyright (C) 1998 Lars Knoll (knoll@mpi-hd.mpg.de)
    Copyright (C) 2001 Dirk Mueller (mueller@kde.org)
    Copyright (C) 2002 Waldo Bastian (bastian@kde.org)
    Copyright (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
    Copyright (C) 2004, 2005, 2006, 2007 Apple Inc. All rights reserved.

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "config.h"
#include "core/fetch/ImageResource.h"

#include "RuntimeEnabledFeatures.h"
#include "core/fetch/ImageResourceClient.h"
#include "core/fetch/MemoryCache.h"
#include "core/fetch/ResourceClient.h"
#include "core/fetch/ResourceClientWalker.h"
#include "core/fetch/ResourceFetcher.h"
#include "core/frame/FrameView.h"
#include "core/rendering/RenderObject.h"
#include "core/svg/graphics/SVGImage.h"
#include "platform/SharedBuffer.h"
#include "platform/graphics/BitmapImage.h"
#include "wtf/CurrentTime.h"
#include "wtf/StdLibExtras.h"
#include "wtf/Vector.h"

namespace WebCore {

ImageResource::ImageResource(const ResourceRequest& resourceRequest)
    : Resource(resourceRequest, Image)
    , m_devicePixelRatioHeaderValue(1.0)
    , m_image(nullptr)
    , m_loadingMultipartContent(false)
    , m_hasDevicePixelRatioHeaderValue(false)
{
    setStatus(Unknown);
    setCustomAcceptHeader();
}

ImageResource::ImageResource(WebCore::Image* image)
    : Resource(ResourceRequest(""), Image)
    , m_image(image)
{
    setStatus(Cached);
    setLoading(false);
    setCustomAcceptHeader();
}

ImageResource::ImageResource(const ResourceRequest& resourceRequest, WebCore::Image* image)
    : Resource(resourceRequest, Image)
    , m_image(image)
{
    setStatus(Cached);
    setLoading(false);
    setCustomAcceptHeader();
}

ImageResource::~ImageResource()
{
    clearImage();
}

void ImageResource::load(ResourceFetcher* fetcher, const ResourceLoaderOptions& options)
{
    if (!fetcher || fetcher->autoLoadImages())
        Resource::load(fetcher, options);
    else
        setLoading(false);
}

void ImageResource::didAddClient(ResourceClient* c)
{
    if (m_data && !m_image && !errorOccurred()) {
        createImage();
        m_image->setData(m_data, true);
    }

    ASSERT(c->resourceClientType() == ImageResourceClient::expectedType());
    if (m_image && !m_image->isNull())
        static_cast<ImageResourceClient*>(c)->imageChanged(this);

    Resource::didAddClient(c);
}

void ImageResource::didRemoveClient(ResourceClient* c)
{
    ASSERT(c);
    ASSERT(c->resourceClientType() == ImageResourceClient::expectedType());

    m_pendingContainerSizeRequests.remove(static_cast<ImageResourceClient*>(c));
    if (m_svgImageCache)
        m_svgImageCache->removeClientFromCache(static_cast<ImageResourceClient*>(c));

    Resource::didRemoveClient(c);
}

void ImageResource::switchClientsToRevalidatedResource()
{
    ASSERT(resourceToRevalidate());
    ASSERT(resourceToRevalidate()->isImage());
    // Pending container size requests need to be transferred to the revalidated resource.
    if (!m_pendingContainerSizeRequests.isEmpty()) {
        // A copy of pending size requests is needed as they are deleted during Resource::switchClientsToRevalidateResouce().
        ContainerSizeRequests switchContainerSizeRequests;
        for (ContainerSizeRequests::iterator it = m_pendingContainerSizeRequests.begin(); it != m_pendingContainerSizeRequests.end(); ++it)
            switchContainerSizeRequests.set(it->key, it->value);
        Resource::switchClientsToRevalidatedResource();
        ImageResource* revalidatedImageResource = toImageResource(resourceToRevalidate());
        for (ContainerSizeRequests::iterator it = switchContainerSizeRequests.begin(); it != switchContainerSizeRequests.end(); ++it)
            revalidatedImageResource->setContainerSizeForRenderer(it->key, it->value.first, it->value.second);
        return;
    }

    Resource::switchClientsToRevalidatedResource();
}

bool ImageResource::isSafeToUnlock() const
{
    // Note that |m_image| holds a reference to |m_data| in addition to the one held by the Resource parent class.
    return !m_image || (m_image->hasOneRef() && m_data->refCount() == 2);
}

void ImageResource::destroyDecodedDataIfPossible()
{
    if (!hasClients() && !isLoading() && (!m_image || (m_image->hasOneRef() && m_image->isBitmapImage()))) {
        m_image = nullptr;
        setDecodedSize(0);
    } else if (m_image && !errorOccurred()) {
        m_image->destroyDecodedData(true);
    }
}

void ImageResource::allClientsRemoved()
{
    m_pendingContainerSizeRequests.clear();
    if (m_image && !errorOccurred())
        m_image->resetAnimation();
    Resource::allClientsRemoved();
}

pair<WebCore::Image*, float> ImageResource::brokenImage(float deviceScaleFactor)
{
    if (deviceScaleFactor >= 2) {
        DEFINE_STATIC_REF(WebCore::Image, brokenImageHiRes, (WebCore::Image::loadPlatformResource("missingImage@2x")));
        return std::make_pair(brokenImageHiRes, 2);
    }

    DEFINE_STATIC_REF(WebCore::Image, brokenImageLoRes, (WebCore::Image::loadPlatformResource("missingImage")));
    return std::make_pair(brokenImageLoRes, 1);
}

bool ImageResource::willPaintBrokenImage() const
{
    return errorOccurred();
}

WebCore::Image* ImageResource::image()
{
    ASSERT(!isPurgeable());

    if (errorOccurred()) {
        // Returning the 1x broken image is non-ideal, but we cannot reliably access the appropriate
        // deviceScaleFactor from here. It is critical that callers use ImageResource::brokenImage()
        // when they need the real, deviceScaleFactor-appropriate broken image icon.
        return brokenImage(1).first;
    }

    if (m_image)
        return m_image.get();

    return WebCore::Image::nullImage();
}

WebCore::Image* ImageResource::imageForRenderer(const RenderObject* renderer)
{
    ASSERT(!isPurgeable());

    if (errorOccurred()) {
        // Returning the 1x broken image is non-ideal, but we cannot reliably access the appropriate
        // deviceScaleFactor from here. It is critical that callers use ImageResource::brokenImage()
        // when they need the real, deviceScaleFactor-appropriate broken image icon.
        return brokenImage(1).first;
    }

    if (!m_image)
        return WebCore::Image::nullImage();

    if (m_image->isSVGImage()) {
        WebCore::Image* image = m_svgImageCache->imageForRenderer(renderer);
        if (image != WebCore::Image::nullImage())
            return image;
    }

    return m_image.get();
}

void ImageResource::setContainerSizeForRenderer(const ImageResourceClient* renderer, const IntSize& containerSize, float containerZoom)
{
    if (containerSize.isEmpty())
        return;
    ASSERT(renderer);
    ASSERT(containerZoom);
    if (!m_image) {
        m_pendingContainerSizeRequests.set(renderer, SizeAndZoom(containerSize, containerZoom));
        return;
    }
    if (!m_image->isSVGImage()) {
        m_image->setContainerSize(containerSize);
        return;
    }

    m_svgImageCache->setContainerSizeForRenderer(renderer, containerSize, containerZoom);
}

bool ImageResource::usesImageContainerSize() const
{
    if (m_image)
        return m_image->usesContainerSize();

    return false;
}

bool ImageResource::imageHasRelativeWidth() const
{
    if (m_image)
        return m_image->hasRelativeWidth();

    return false;
}

bool ImageResource::imageHasRelativeHeight() const
{
    if (m_image)
        return m_image->hasRelativeHeight();

    return false;
}

LayoutSize ImageResource::imageSizeForRenderer(const RenderObject* renderer, float multiplier, SizeType sizeType)
{
    ASSERT(!isPurgeable());

    if (!m_image)
        return IntSize();

    LayoutSize imageSize;

    if (m_image->isBitmapImage() && (renderer && renderer->shouldRespectImageOrientation() == RespectImageOrientation))
        imageSize = toBitmapImage(m_image.get())->sizeRespectingOrientation();
    else if (m_image->isSVGImage() && sizeType == NormalSize)
        imageSize = m_svgImageCache->imageSizeForRenderer(renderer);
    else
        imageSize = m_image->size();

    if (multiplier == 1.0f)
        return imageSize;

    // Don't let images that have a width/height >= 1 shrink below 1 when zoomed.
    float widthScale = m_image->hasRelativeWidth() ? 1.0f : multiplier;
    float heightScale = m_image->hasRelativeHeight() ? 1.0f : multiplier;
    LayoutSize minimumSize(imageSize.width() > 0 ? 1 : 0, imageSize.height() > 0 ? 1 : 0);
    imageSize.scale(widthScale, heightScale);
    imageSize.clampToMinimumSize(minimumSize);
    ASSERT(multiplier != 1.0f || (imageSize.width().fraction() == 0.0f && imageSize.height().fraction() == 0.0f));
    return imageSize;
}

void ImageResource::computeIntrinsicDimensions(Length& intrinsicWidth, Length& intrinsicHeight, FloatSize& intrinsicRatio)
{
    if (m_image)
        m_image->computeIntrinsicDimensions(intrinsicWidth, intrinsicHeight, intrinsicRatio);
}

void ImageResource::notifyObservers(const IntRect* changeRect)
{
    ResourceClientWalker<ImageResourceClient> w(m_clients);
    while (ImageResourceClient* c = w.next())
        c->imageChanged(this, changeRect);
}

void ImageResource::clear()
{
    prune();
    clearImage();
    m_pendingContainerSizeRequests.clear();
    setEncodedSize(0);
}

void ImageResource::setCustomAcceptHeader()
{
    DEFINE_STATIC_LOCAL(const AtomicString, acceptWebP, ("image/webp,*/*;q=0.8", AtomicString::ConstructFromLiteral));
    setAccept(acceptWebP);
}

inline void ImageResource::createImage()
{
    // Create the image if it doesn't yet exist.
    if (m_image)
        return;

    if (m_response.mimeType() == "image/svg+xml") {
        RefPtr<SVGImage> svgImage = SVGImage::create(this);
        m_svgImageCache = SVGImageCache::create(svgImage.get());
        m_image = svgImage.release();
    } else {
        m_image = BitmapImage::create(this);
    }

    if (m_image) {
        // Send queued container size requests.
        if (m_image->usesContainerSize()) {
            for (ContainerSizeRequests::iterator it = m_pendingContainerSizeRequests.begin(); it != m_pendingContainerSizeRequests.end(); ++it)
                setContainerSizeForRenderer(it->key, it->value.first, it->value.second);
        }
        m_pendingContainerSizeRequests.clear();
    }
}

inline void ImageResource::clearImage()
{
    // If our Image has an observer, it's always us so we need to clear the back pointer
    // before dropping our reference.
    if (m_image)
        m_image->setImageObserver(0);
    m_image.clear();
}

void ImageResource::appendData(const char* data, int length)
{
    Resource::appendData(data, length);
    if (!m_loadingMultipartContent)
        updateImage(false);
}

void ImageResource::updateImage(bool allDataReceived)
{
    TRACE_EVENT0("webkit", "ImageResource::updateImage");

    if (m_data)
        createImage();

    bool sizeAvailable = false;

    // Have the image update its data from its internal buffer.
    // It will not do anything now, but will delay decoding until
    // queried for info (like size or specific image frames).
    if (m_image)
        sizeAvailable = m_image->setData(m_data, allDataReceived);

    // Go ahead and tell our observers to try to draw if we have either
    // received all the data or the size is known. Each chunk from the
    // network causes observers to repaint, which will force that chunk
    // to decode.
    if (sizeAvailable || allDataReceived) {
        if (!m_image || m_image->isNull()) {
            error(errorOccurred() ? status() : DecodeError);
            if (inCache())
                memoryCache()->remove(this);
            return;
        }

        // It would be nice to only redraw the decoded band of the image, but with the current design
        // (decoding delayed until painting) that seems hard.
        notifyObservers();
    }
}

void ImageResource::finishOnePart()
{
    if (m_loadingMultipartContent)
        clear();
    updateImage(true);
    if (m_loadingMultipartContent)
        m_data.clear();
    Resource::finishOnePart();
}

void ImageResource::error(Resource::Status status)
{
    clear();
    Resource::error(status);
    notifyObservers();
}

void ImageResource::responseReceived(const ResourceResponse& response)
{
    if (m_loadingMultipartContent && m_data)
        finishOnePart();
    else if (response.isMultipart())
        m_loadingMultipartContent = true;
    if (RuntimeEnabledFeatures::clientHintsDprEnabled()) {
        m_devicePixelRatioHeaderValue = response.httpHeaderField("DPR").toFloat(&m_hasDevicePixelRatioHeaderValue);
        if (!m_hasDevicePixelRatioHeaderValue || m_devicePixelRatioHeaderValue <= 0.0) {
            m_devicePixelRatioHeaderValue = 1.0;
            m_hasDevicePixelRatioHeaderValue = false;
        }
    }
    Resource::responseReceived(response);
}

void ImageResource::decodedSizeChanged(const WebCore::Image* image, int delta)
{
    if (!image || image != m_image)
        return;

    setDecodedSize(decodedSize() + delta);
}

void ImageResource::didDraw(const WebCore::Image* image)
{
    if (!image || image != m_image)
        return;

    double timeStamp = FrameView::currentFrameTimeStamp();
    if (!timeStamp) // If didDraw is called outside of a LocalFrame paint.
        timeStamp = currentTime();

    Resource::didAccessDecodedData(timeStamp);
}

bool ImageResource::shouldPauseAnimation(const WebCore::Image* image)
{
    if (!image || image != m_image)
        return false;

    ResourceClientWalker<ImageResourceClient> w(m_clients);
    while (ImageResourceClient* c = w.next()) {
        if (c->willRenderImage(this))
            return false;
    }

    return true;
}

void ImageResource::animationAdvanced(const WebCore::Image* image)
{
    if (!image || image != m_image)
        return;
    notifyObservers();
}

void ImageResource::changedInRect(const WebCore::Image* image, const IntRect& rect)
{
    if (!image || image != m_image)
        return;
    notifyObservers(&rect);
}

bool ImageResource::currentFrameKnownToBeOpaque(const RenderObject* renderer)
{
    WebCore::Image* image = imageForRenderer(renderer);
    if (image->isBitmapImage())
        image->nativeImageForCurrentFrame(); // force decode
    return image->currentFrameKnownToBeOpaque();
}

bool ImageResource::isAccessAllowed(SecurityOrigin* securityOrigin)
{
    if (!image()->currentFrameHasSingleSecurityOrigin())
        return false;
    if (passesAccessControlCheck(securityOrigin))
        return true;
    return !securityOrigin->taintsCanvas(response().url());
}

} // namespace WebCore
