/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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
#include "platform/graphics/DeferredImageDecoder.h"

#include "platform/graphics/DecodingImageGenerator.h"
#include "platform/graphics/ImageDecodingStore.h"
#include "platform/graphics/LazyDecodingPixelRef.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "wtf/PassOwnPtr.h"

namespace WebCore {

namespace {

// URI label for a lazily decoded SkPixelRef.
const char labelLazyDecoded[] = "lazy";

// URI label for SkDiscardablePixelRef.
const char labelDiscardable[] = "discardable";

} // namespace

bool DeferredImageDecoder::s_enabled = false;
bool DeferredImageDecoder::s_skiaDiscardableMemoryEnabled = false;

DeferredImageDecoder::DeferredImageDecoder(PassOwnPtr<ImageDecoder> actualDecoder)
    : m_allDataReceived(false)
    , m_lastDataSize(0)
    , m_dataChanged(false)
    , m_actualDecoder(actualDecoder)
    , m_orientation(DefaultImageOrientation)
    , m_repetitionCount(cAnimationNone)
{
}

DeferredImageDecoder::~DeferredImageDecoder()
{
}

PassOwnPtr<DeferredImageDecoder> DeferredImageDecoder::create(const SharedBuffer& data, ImageSource::AlphaOption alphaOption, ImageSource::GammaAndColorProfileOption gammaAndColorOption)
{
    OwnPtr<ImageDecoder> actualDecoder = ImageDecoder::create(data, alphaOption, gammaAndColorOption);
    return actualDecoder ? adoptPtr(new DeferredImageDecoder(actualDecoder.release())) : nullptr;
}

PassOwnPtr<DeferredImageDecoder> DeferredImageDecoder::createForTesting(PassOwnPtr<ImageDecoder> decoder)
{
    return adoptPtr(new DeferredImageDecoder(decoder));
}

bool DeferredImageDecoder::isLazyDecoded(const SkBitmap& bitmap)
{
    return bitmap.pixelRef()
        && bitmap.pixelRef()->getURI()
        && (!memcmp(bitmap.pixelRef()->getURI(), labelLazyDecoded, sizeof(labelLazyDecoded))
            || !memcmp(bitmap.pixelRef()->getURI(), labelDiscardable, sizeof(labelDiscardable)));
}

void DeferredImageDecoder::setEnabled(bool enabled)
{
    s_enabled = enabled;
#if !OS(ANDROID)
    // FIXME: This code is temporary to enable discardable memory for
    // non-Android platforms. In the future all platforms will be
    // the same and we can remove this code.
    s_skiaDiscardableMemoryEnabled = enabled;
    if (enabled)
        ImageDecodingStore::setImageCachingEnabled(false);
#endif
}

String DeferredImageDecoder::filenameExtension() const
{
    return m_actualDecoder ? m_actualDecoder->filenameExtension() : m_filenameExtension;
}

ImageFrame* DeferredImageDecoder::frameBufferAtIndex(size_t index)
{
    prepareLazyDecodedFrames();
    if (index < m_lazyDecodedFrames.size()) {
        // ImageFrameGenerator has the latest known alpha state. There will
        // be a performance boost if this frame is opaque.
        m_lazyDecodedFrames[index]->setHasAlpha(m_frameGenerator->hasAlpha(index));
        return m_lazyDecodedFrames[index].get();
    }
    if (m_actualDecoder)
        return m_actualDecoder->frameBufferAtIndex(index);
    return 0;
}

void DeferredImageDecoder::setData(SharedBuffer* data, bool allDataReceived)
{
    if (m_actualDecoder) {
        const bool firstData = !m_data && data;
        const bool moreData = data && data->size() > m_lastDataSize;
        m_dataChanged = firstData || moreData;
        m_data = data;
        m_lastDataSize = data->size();
        m_allDataReceived = allDataReceived;
        m_actualDecoder->setData(data, allDataReceived);
        prepareLazyDecodedFrames();
    }

    if (m_frameGenerator)
        m_frameGenerator->setData(data, allDataReceived);
}

bool DeferredImageDecoder::isSizeAvailable()
{
    // m_actualDecoder is 0 only if image decoding is deferred and that
    // means image header decoded successfully and size is available.
    return m_actualDecoder ? m_actualDecoder->isSizeAvailable() : true;
}

IntSize DeferredImageDecoder::size() const
{
    return m_actualDecoder ? m_actualDecoder->size() : m_size;
}

IntSize DeferredImageDecoder::frameSizeAtIndex(size_t index) const
{
    // FIXME: LocalFrame size is assumed to be uniform. This might not be true for
    // future supported codecs.
    return m_actualDecoder ? m_actualDecoder->frameSizeAtIndex(index) : m_size;
}

size_t DeferredImageDecoder::frameCount()
{
    return m_actualDecoder ? m_actualDecoder->frameCount() : m_lazyDecodedFrames.size();
}

int DeferredImageDecoder::repetitionCount() const
{
    return m_actualDecoder ? m_actualDecoder->repetitionCount() : m_repetitionCount;
}

size_t DeferredImageDecoder::clearCacheExceptFrame(size_t clearExceptFrame)
{
    // If image decoding is deferred then frame buffer cache is managed by
    // the compositor and this call is ignored.
    return m_actualDecoder ? m_actualDecoder->clearCacheExceptFrame(clearExceptFrame) : 0;
}

bool DeferredImageDecoder::frameHasAlphaAtIndex(size_t index) const
{
    if (m_actualDecoder)
        return m_actualDecoder->frameHasAlphaAtIndex(index);
    if (!m_frameGenerator->isMultiFrame())
        return m_frameGenerator->hasAlpha(index);
    return true;
}

bool DeferredImageDecoder::frameIsCompleteAtIndex(size_t index) const
{
    if (m_actualDecoder)
        return m_actualDecoder->frameIsCompleteAtIndex(index);
    if (index < m_lazyDecodedFrames.size())
        return m_lazyDecodedFrames[index]->status() == ImageFrame::FrameComplete;
    return false;
}

float DeferredImageDecoder::frameDurationAtIndex(size_t index) const
{
    if (m_actualDecoder)
        return m_actualDecoder->frameDurationAtIndex(index);
    if (index < m_lazyDecodedFrames.size())
        return m_lazyDecodedFrames[index]->duration();
    return 0;
}

unsigned DeferredImageDecoder::frameBytesAtIndex(size_t index) const
{
    // If frame decoding is deferred then it is not managed by MemoryCache
    // so return 0 here.
    return m_frameGenerator ? 0 : m_actualDecoder->frameBytesAtIndex(index);
}

ImageOrientation DeferredImageDecoder::orientation() const
{
    return m_actualDecoder ? m_actualDecoder->orientation() : m_orientation;
}

void DeferredImageDecoder::activateLazyDecoding()
{
    if (m_frameGenerator)
        return;
    m_size = m_actualDecoder->size();
    m_orientation = m_actualDecoder->orientation();
    m_filenameExtension = m_actualDecoder->filenameExtension();
    const bool isSingleFrame = m_actualDecoder->repetitionCount() == cAnimationNone || (m_allDataReceived && m_actualDecoder->frameCount() == 1u);
    m_frameGenerator = ImageFrameGenerator::create(SkISize::Make(m_actualDecoder->decodedSize().width(), m_actualDecoder->decodedSize().height()), m_data, m_allDataReceived, !isSingleFrame);
}

void DeferredImageDecoder::prepareLazyDecodedFrames()
{
    if (!s_enabled
        || !m_actualDecoder
        || !m_actualDecoder->isSizeAvailable()
        || m_actualDecoder->filenameExtension() == "ico")
        return;

    activateLazyDecoding();

    const size_t previousSize = m_lazyDecodedFrames.size();
    m_lazyDecodedFrames.resize(m_actualDecoder->frameCount());
    for (size_t i = previousSize; i < m_lazyDecodedFrames.size(); ++i) {
        OwnPtr<ImageFrame> frame(adoptPtr(new ImageFrame()));
        frame->setSkBitmap(createBitmap(i));
        frame->setDuration(m_actualDecoder->frameDurationAtIndex(i));
        frame->setStatus(m_actualDecoder->frameIsCompleteAtIndex(i) ? ImageFrame::FrameComplete : ImageFrame::FramePartial);
        m_lazyDecodedFrames[i] = frame.release();
    }

    // The last lazy decoded frame created from previous call might be
    // incomplete so update its state.
    if (previousSize) {
        const size_t lastFrame = previousSize - 1;
        m_lazyDecodedFrames[lastFrame]->setStatus(m_actualDecoder->frameIsCompleteAtIndex(lastFrame) ? ImageFrame::FrameComplete : ImageFrame::FramePartial);

        // If data has changed then create a new bitmap. This forces
        // Skia to decode again.
        if (m_dataChanged) {
            m_dataChanged = false;
            m_lazyDecodedFrames[lastFrame]->setSkBitmap(createBitmap(lastFrame));
        }
    }

    if (m_allDataReceived) {
        m_repetitionCount = m_actualDecoder->repetitionCount();
        m_actualDecoder.clear();
        m_data = nullptr;
    }
}

// Creates either a SkBitmap backed by SkDiscardablePixelRef or a SkBitmap using the
// legacy LazyDecodingPixelRef.
SkBitmap DeferredImageDecoder::createBitmap(size_t index)
{
    // This code is temporary until the transition to SkDiscardablePixelRef is complete.
    if (s_skiaDiscardableMemoryEnabled)
        return createSkiaDiscardableBitmap(index);
    return createLazyDecodingBitmap(index);
}

// Creates a SkBitmap that is backed by SkDiscardablePixelRef.
SkBitmap DeferredImageDecoder::createSkiaDiscardableBitmap(size_t index)
{
    IntSize decodedSize = m_actualDecoder->decodedSize();
    ASSERT(decodedSize.width() > 0);
    ASSERT(decodedSize.height() > 0);

    SkImageInfo info;
    info.fWidth = decodedSize.width();
    info.fHeight = decodedSize.height();
    info.fColorType = kBGRA_8888_SkColorType;
    info.fAlphaType = kPremul_SkAlphaType;

    SkBitmap bitmap;
    DecodingImageGenerator* generator = new DecodingImageGenerator(m_frameGenerator, info, index);
    bool installed = SkInstallDiscardablePixelRef(generator, &bitmap);
    ASSERT_UNUSED(installed, installed);
    bitmap.pixelRef()->setURI(labelDiscardable);
    generator->setGenerationId(bitmap.getGenerationID());
    return bitmap;
}

SkBitmap DeferredImageDecoder::createLazyDecodingBitmap(size_t index)
{
    IntSize decodedSize = m_actualDecoder->decodedSize();
    ASSERT(decodedSize.width() > 0);
    ASSERT(decodedSize.height() > 0);

    SkImageInfo info;
    info.fWidth = decodedSize.width();
    info.fHeight = decodedSize.height();
    info.fColorType = kPMColor_SkColorType;
    info.fAlphaType = kPremul_SkAlphaType;

    // Creates a lazily decoded SkPixelRef that references the entire image without scaling.
    SkBitmap bitmap;
    bitmap.setConfig(info);
    bitmap.setPixelRef(new LazyDecodingPixelRef(info, m_frameGenerator, index))->unref();

    // Use the URI to identify this as a lazily decoded SkPixelRef of type LazyDecodingPixelRef.
    // FIXME: It would be more useful to give the actual image URI.
    bitmap.pixelRef()->setURI(labelLazyDecoded);

    // Inform the bitmap that we will never change the pixels. This is a performance hint
    // subsystems that may try to cache this bitmap (e.g. pictures, pipes, gpu, pdf, etc.)
    bitmap.setImmutable();

    return bitmap;
}

bool DeferredImageDecoder::hotSpot(IntPoint& hotSpot) const
{
    // TODO: Implement.
    return m_actualDecoder ? m_actualDecoder->hotSpot(hotSpot) : false;
}

} // namespace WebCore
