/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
 * Copyright (C) 2008, 2009 Google, Inc.
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
#include "platform/image-decoders/ImageDecoder.h"

namespace WebCore {

ImageFrame::ImageFrame()
    : m_bitmap(NativeImageSkia::create())
    , m_allocator(0)
    , m_hasAlpha(false)
    , m_status(FrameEmpty)
    , m_duration(0)
    , m_disposalMethod(DisposeNotSpecified)
    , m_alphaBlendSource(BlendAtopPreviousFrame)
    , m_premultiplyAlpha(true)
    , m_pixelsChanged(false)
    , m_requiredPreviousFrameIndex(kNotFound)
#if !ASSERT_DISABLED
    , m_requiredPreviousFrameIndexValid(false)
#endif
{
}

ImageFrame& ImageFrame::operator=(const ImageFrame& other)
{
    if (this == &other)
        return *this;

    m_bitmap = other.m_bitmap->clone();
    // Keep the pixels locked since we will be writing directly into the
    // bitmap throughout this object's lifetime.
    m_bitmap->bitmap().lockPixels();
    // Be sure to assign this before calling setStatus(), since setStatus() may
    // call notifyBitmapIfPixelsChanged().
    m_pixelsChanged = other.m_pixelsChanged;
    setMemoryAllocator(other.allocator());
    setOriginalFrameRect(other.originalFrameRect());
    setStatus(other.status());
    setDuration(other.duration());
    setDisposalMethod(other.disposalMethod());
    setAlphaBlendSource(other.alphaBlendSource());
    setPremultiplyAlpha(other.premultiplyAlpha());
    // Be sure that this is called after we've called setStatus(), since we
    // look at our status to know what to do with the alpha value.
    setHasAlpha(other.hasAlpha());
    // Copy raw fields to avoid ASSERT failure in requiredPreviousFrameIndex().
    m_requiredPreviousFrameIndex = other.m_requiredPreviousFrameIndex;
#if !ASSERT_DISABLED
    m_requiredPreviousFrameIndexValid = other.m_requiredPreviousFrameIndexValid;
#endif
    return *this;
}

void ImageFrame::clearPixelData()
{
    m_bitmap->bitmap().reset();
    m_status = FrameEmpty;
    // NOTE: Do not reset other members here; clearFrameBufferCache()
    // calls this to free the bitmap data, but other functions like
    // initFrameBuffer() and frameComplete() may still need to read
    // other metadata out of this frame later.
}

void ImageFrame::zeroFillPixelData()
{
    m_bitmap->bitmap().eraseARGB(0, 0, 0, 0);
    m_hasAlpha = true;
}

bool ImageFrame::copyBitmapData(const ImageFrame& other)
{
    if (this == &other)
        return true;

    m_hasAlpha = other.m_hasAlpha;
    m_bitmap->bitmap().reset();
    const NativeImageSkia* otherBitmap = other.m_bitmap.get();
    return otherBitmap->bitmap().copyTo(&m_bitmap->bitmap(), otherBitmap->bitmap().colorType());
}

bool ImageFrame::setSize(int newWidth, int newHeight)
{
    // setSize() should only be called once, it leaks memory otherwise.
    ASSERT(!width() && !height());

    m_bitmap->bitmap().setConfig(SkBitmap::kARGB_8888_Config, newWidth, newHeight);
    if (!m_bitmap->bitmap().allocPixels(m_allocator, 0))
        return false;

    zeroFillPixelData();
    return true;
}

PassRefPtr<NativeImageSkia> ImageFrame::asNewNativeImage() const
{
    return m_bitmap->clone();
}

bool ImageFrame::hasAlpha() const
{
    return m_hasAlpha;
}

void ImageFrame::setHasAlpha(bool alpha)
{
    m_hasAlpha = alpha;

    // If the frame is not fully loaded, there will be transparent pixels,
    // so we can't tell skia we're opaque, even for image types that logically
    // always are (e.g. jpeg).
    if (m_status != FrameComplete)
        alpha = true;
    m_bitmap->bitmap().setAlphaType(alpha ? kPremul_SkAlphaType : kOpaque_SkAlphaType);
}

void ImageFrame::setStatus(Status status)
{
    m_status = status;
    if (m_status == FrameComplete) {
        m_bitmap->bitmap().setAlphaType(m_hasAlpha ? kPremul_SkAlphaType : kOpaque_SkAlphaType);
        // Send pending pixels changed notifications now, because we can't do this after
        // the bitmap was set immutable by setDataComplete().
        notifyBitmapIfPixelsChanged();
        m_bitmap->setDataComplete(); // Tell the bitmap it's done.
    }
}

void ImageFrame::zeroFillFrameRect(const IntRect& rect)
{
    if (rect.isEmpty())
        return;

    m_bitmap->bitmap().eraseArea(rect, SkColorSetARGB(0, 0, 0, 0));
    setHasAlpha(true);
}

} // namespace WebCore
