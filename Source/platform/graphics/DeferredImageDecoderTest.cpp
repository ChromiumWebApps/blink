/*
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
#include "platform/graphics/DeferredImageDecoder.h"

#include "SkBitmapDevice.h"
#include "SkCanvas.h"
#include "SkPicture.h"
#include "platform/SharedBuffer.h"
#include "platform/Task.h"
#include "platform/graphics/ImageDecodingStore.h"
#include "platform/graphics/skia/NativeImageSkia.h"
#include "platform/graphics/test/MockImageDecoder.h"
#include "public/platform/Platform.h"
#include "public/platform/WebThread.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefPtr.h"
#include <gtest/gtest.h>

namespace WebCore {

namespace {

// Raw data for a PNG file with 1x1 white pixels.
const unsigned char whitePNG[] = {
    0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0x00, 0x00,
    0x00, 0x0d, 0x49, 0x48, 0x44, 0x52, 0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x01, 0x08, 0x02, 0x00, 0x00, 0x00, 0x90,
    0x77, 0x53, 0xde, 0x00, 0x00, 0x00, 0x01, 0x73, 0x52, 0x47,
    0x42, 0x00, 0xae, 0xce, 0x1c, 0xe9, 0x00, 0x00, 0x00, 0x09,
    0x70, 0x48, 0x59, 0x73, 0x00, 0x00, 0x0b, 0x13, 0x00, 0x00,
    0x0b, 0x13, 0x01, 0x00, 0x9a, 0x9c, 0x18, 0x00, 0x00, 0x00,
    0x0c, 0x49, 0x44, 0x41, 0x54, 0x08, 0xd7, 0x63, 0xf8, 0xff,
    0xff, 0x3f, 0x00, 0x05, 0xfe, 0x02, 0xfe, 0xdc, 0xcc, 0x59,
    0xe7, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4e, 0x44, 0xae,
    0x42, 0x60, 0x82,
};

static SkCanvas* createRasterCanvas(int width, int height)
{
    SkAutoTUnref<SkBaseDevice> device(new SkBitmapDevice(SkBitmap::kARGB_8888_Config, width, height));
    return new SkCanvas(device);
}

struct Rasterizer {
    SkCanvas* canvas;
    SkPicture* picture;
};

} // namespace

class DeferredImageDecoderTest : public ::testing::Test, public MockImageDecoderClient {
public:
    virtual void SetUp() OVERRIDE
    {
        ImageDecodingStore::initializeOnce();
        DeferredImageDecoder::setEnabled(true);
        m_data = SharedBuffer::create(whitePNG, sizeof(whitePNG));
        OwnPtr<MockImageDecoder> decoder = MockImageDecoder::create(this);
        m_actualDecoder = decoder.get();
        m_actualDecoder->setSize(1, 1);
        m_lazyDecoder = DeferredImageDecoder::createForTesting(decoder.release());
        m_canvas.reset(createRasterCanvas(100, 100));
        m_frameBufferRequestCount = 0;
        m_frameCount = 1;
        m_repetitionCount = cAnimationNone;
        m_status = ImageFrame::FrameComplete;
        m_frameDuration = 0;
        m_decodedSize = m_actualDecoder->size();
    }

    virtual void TearDown() OVERRIDE
    {
        ImageDecodingStore::shutdown();
    }

    virtual void decoderBeingDestroyed() OVERRIDE
    {
        m_actualDecoder = 0;
    }

    virtual void frameBufferRequested() OVERRIDE
    {
        ++m_frameBufferRequestCount;
    }

    virtual size_t frameCount() OVERRIDE
    {
        return m_frameCount;
    }

    virtual int repetitionCount() const OVERRIDE
    {
        return m_repetitionCount;
    }

    virtual ImageFrame::Status status() OVERRIDE
    {
        return m_status;
    }

    virtual float frameDuration() const OVERRIDE
    {
        return m_frameDuration;
    }

    virtual IntSize decodedSize() const OVERRIDE
    {
        return m_decodedSize;
    }

protected:
    void useMockImageDecoderFactory()
    {
        m_lazyDecoder->frameGenerator()->setImageDecoderFactory(MockImageDecoderFactory::create(this, m_decodedSize));
    }

    // Don't own this but saves the pointer to query states.
    MockImageDecoder* m_actualDecoder;
    OwnPtr<DeferredImageDecoder> m_lazyDecoder;
    SkPicture m_picture;
    SkAutoTUnref<SkCanvas> m_canvas;
    int m_frameBufferRequestCount;
    RefPtr<SharedBuffer> m_data;
    size_t m_frameCount;
    int m_repetitionCount;
    ImageFrame::Status m_status;
    float m_frameDuration;
    IntSize m_decodedSize;
};

TEST_F(DeferredImageDecoderTest, drawIntoSkPicture)
{
    m_lazyDecoder->setData(m_data.get(), true);
    RefPtr<NativeImageSkia> image = m_lazyDecoder->frameBufferAtIndex(0)->asNewNativeImage();
    EXPECT_EQ(1, image->bitmap().width());
    EXPECT_EQ(1, image->bitmap().height());
    EXPECT_FALSE(image->bitmap().isNull());
    EXPECT_TRUE(image->bitmap().isImmutable());

    SkCanvas* tempCanvas = m_picture.beginRecording(100, 100);
    tempCanvas->drawBitmap(image->bitmap(), 0, 0);
    m_picture.endRecording();
    EXPECT_EQ(0, m_frameBufferRequestCount);

    m_canvas->drawPicture(m_picture);
    EXPECT_EQ(0, m_frameBufferRequestCount);

    SkBitmap canvasBitmap;
    canvasBitmap.setConfig(SkBitmap::kARGB_8888_Config, 100, 100);
    ASSERT_TRUE(m_canvas->readPixels(&canvasBitmap, 0, 0));
    SkAutoLockPixels autoLock(canvasBitmap);
    EXPECT_EQ(SkColorSetARGB(255, 255, 255, 255), canvasBitmap.getColor(0, 0));
}

TEST_F(DeferredImageDecoderTest, drawIntoSkPictureProgressive)
{
    RefPtr<SharedBuffer> partialData = SharedBuffer::create(m_data->data(), m_data->size() - 10);

    // Received only half the file.
    m_lazyDecoder->setData(partialData.get(), false);
    RefPtr<NativeImageSkia> image = m_lazyDecoder->frameBufferAtIndex(0)->asNewNativeImage();
    SkCanvas* tempCanvas = m_picture.beginRecording(100, 100);
    tempCanvas->drawBitmap(image->bitmap(), 0, 0);
    m_picture.endRecording();
    m_canvas->drawPicture(m_picture);

    // Fully received the file and draw the SkPicture again.
    m_lazyDecoder->setData(m_data.get(), true);
    image = m_lazyDecoder->frameBufferAtIndex(0)->asNewNativeImage();
    tempCanvas = m_picture.beginRecording(100, 100);
    tempCanvas->drawBitmap(image->bitmap(), 0, 0);
    m_picture.endRecording();
    m_canvas->drawPicture(m_picture);

    SkBitmap canvasBitmap;
    canvasBitmap.setConfig(SkBitmap::kARGB_8888_Config, 100, 100);
    ASSERT_TRUE(m_canvas->readPixels(&canvasBitmap, 0, 0));
    SkAutoLockPixels autoLock(canvasBitmap);
    EXPECT_EQ(SkColorSetARGB(255, 255, 255, 255), canvasBitmap.getColor(0, 0));
}

static void rasterizeMain(SkCanvas* canvas, SkPicture* picture)
{
    canvas->drawPicture(*picture);
}

TEST_F(DeferredImageDecoderTest, decodeOnOtherThread)
{
    m_lazyDecoder->setData(m_data.get(), true);
    RefPtr<NativeImageSkia> image = m_lazyDecoder->frameBufferAtIndex(0)->asNewNativeImage();
    EXPECT_EQ(1, image->bitmap().width());
    EXPECT_EQ(1, image->bitmap().height());
    EXPECT_FALSE(image->bitmap().isNull());
    EXPECT_TRUE(image->bitmap().isImmutable());

    SkCanvas* tempCanvas = m_picture.beginRecording(100, 100);
    tempCanvas->drawBitmap(image->bitmap(), 0, 0);
    m_picture.endRecording();
    EXPECT_EQ(0, m_frameBufferRequestCount);

    // Create a thread to rasterize SkPicture.
    OwnPtr<blink::WebThread> thread = adoptPtr(blink::Platform::current()->createThread("RasterThread"));
    thread->postTask(new Task(WTF::bind(&rasterizeMain, m_canvas.get(), &m_picture)));
    thread.clear();
    EXPECT_EQ(0, m_frameBufferRequestCount);

    SkBitmap canvasBitmap;
    canvasBitmap.setConfig(SkBitmap::kARGB_8888_Config, 100, 100);
    ASSERT_TRUE(m_canvas->readPixels(&canvasBitmap, 0, 0));
    SkAutoLockPixels autoLock(canvasBitmap);
    EXPECT_EQ(SkColorSetARGB(255, 255, 255, 255), canvasBitmap.getColor(0, 0));
}

TEST_F(DeferredImageDecoderTest, singleFrameImageLoading)
{
    m_status = ImageFrame::FramePartial;
    m_lazyDecoder->setData(m_data.get(), false);
    EXPECT_FALSE(m_lazyDecoder->frameIsCompleteAtIndex(0));
    ImageFrame* frame = m_lazyDecoder->frameBufferAtIndex(0);
    unsigned firstId = frame->getSkBitmap().getGenerationID();
    EXPECT_EQ(ImageFrame::FramePartial, frame->status());
    EXPECT_TRUE(m_actualDecoder);

    m_status = ImageFrame::FrameComplete;
    m_data->append(" ", 1);
    m_lazyDecoder->setData(m_data.get(), true);
    EXPECT_FALSE(m_actualDecoder);
    EXPECT_TRUE(m_lazyDecoder->frameIsCompleteAtIndex(0));
    frame = m_lazyDecoder->frameBufferAtIndex(0);
    unsigned secondId = frame->getSkBitmap().getGenerationID();
    EXPECT_EQ(ImageFrame::FrameComplete, frame->status());
    EXPECT_FALSE(m_frameBufferRequestCount);
    EXPECT_NE(firstId, secondId);

    EXPECT_EQ(secondId, m_lazyDecoder->frameBufferAtIndex(0)->getSkBitmap().getGenerationID());
}

TEST_F(DeferredImageDecoderTest, multiFrameImageLoading)
{
    m_repetitionCount = 10;
    m_frameCount = 1;
    m_frameDuration = 10;
    m_status = ImageFrame::FramePartial;
    m_lazyDecoder->setData(m_data.get(), false);
    EXPECT_EQ(ImageFrame::FramePartial, m_lazyDecoder->frameBufferAtIndex(0)->status());
    unsigned firstId = m_lazyDecoder->frameBufferAtIndex(0)->getSkBitmap().getGenerationID();
    EXPECT_FALSE(m_lazyDecoder->frameIsCompleteAtIndex(0));
    EXPECT_EQ(10.0f, m_lazyDecoder->frameBufferAtIndex(0)->duration());
    EXPECT_EQ(10.0f, m_lazyDecoder->frameDurationAtIndex(0));

    m_frameCount = 2;
    m_frameDuration = 20;
    m_status = ImageFrame::FrameComplete;
    m_data->append(" ", 1);
    m_lazyDecoder->setData(m_data.get(), false);
    EXPECT_EQ(ImageFrame::FrameComplete, m_lazyDecoder->frameBufferAtIndex(0)->status());
    EXPECT_EQ(ImageFrame::FrameComplete, m_lazyDecoder->frameBufferAtIndex(1)->status());
    unsigned secondId = m_lazyDecoder->frameBufferAtIndex(0)->getSkBitmap().getGenerationID();
    EXPECT_NE(firstId, secondId);
    EXPECT_TRUE(m_lazyDecoder->frameIsCompleteAtIndex(0));
    EXPECT_TRUE(m_lazyDecoder->frameIsCompleteAtIndex(1));
    EXPECT_EQ(20.0f, m_lazyDecoder->frameDurationAtIndex(1));
    EXPECT_EQ(10.0f, m_lazyDecoder->frameBufferAtIndex(0)->duration());
    EXPECT_EQ(20.0f, m_lazyDecoder->frameBufferAtIndex(1)->duration());
    EXPECT_TRUE(m_actualDecoder);

    m_frameCount = 3;
    m_frameDuration = 30;
    m_status = ImageFrame::FrameComplete;
    m_lazyDecoder->setData(m_data.get(), true);
    EXPECT_FALSE(m_actualDecoder);
    EXPECT_EQ(ImageFrame::FrameComplete, m_lazyDecoder->frameBufferAtIndex(0)->status());
    EXPECT_EQ(ImageFrame::FrameComplete, m_lazyDecoder->frameBufferAtIndex(1)->status());
    EXPECT_EQ(ImageFrame::FrameComplete, m_lazyDecoder->frameBufferAtIndex(2)->status());
    EXPECT_EQ(secondId, m_lazyDecoder->frameBufferAtIndex(0)->getSkBitmap().getGenerationID());
    EXPECT_TRUE(m_lazyDecoder->frameIsCompleteAtIndex(0));
    EXPECT_TRUE(m_lazyDecoder->frameIsCompleteAtIndex(1));
    EXPECT_TRUE(m_lazyDecoder->frameIsCompleteAtIndex(2));
    EXPECT_EQ(10.0f, m_lazyDecoder->frameDurationAtIndex(0));
    EXPECT_EQ(20.0f, m_lazyDecoder->frameDurationAtIndex(1));
    EXPECT_EQ(30.0f, m_lazyDecoder->frameDurationAtIndex(2));
    EXPECT_EQ(10.0f, m_lazyDecoder->frameBufferAtIndex(0)->duration());
    EXPECT_EQ(20.0f, m_lazyDecoder->frameBufferAtIndex(1)->duration());
    EXPECT_EQ(30.0f, m_lazyDecoder->frameBufferAtIndex(2)->duration());
    EXPECT_EQ(10, m_lazyDecoder->repetitionCount());
}

TEST_F(DeferredImageDecoderTest, decodedSize)
{
    m_decodedSize = IntSize(22, 33);
    m_lazyDecoder->setData(m_data.get(), true);
    RefPtr<NativeImageSkia> image = m_lazyDecoder->frameBufferAtIndex(0)->asNewNativeImage();
    EXPECT_EQ(m_decodedSize.width(), image->bitmap().width());
    EXPECT_EQ(m_decodedSize.height(), image->bitmap().height());
    EXPECT_FALSE(image->bitmap().isNull());
    EXPECT_TRUE(image->bitmap().isImmutable());

    useMockImageDecoderFactory();

    // The following code should not fail any assert.
    SkCanvas* tempCanvas = m_picture.beginRecording(100, 100);
    tempCanvas->drawBitmap(image->bitmap(), 0, 0);
    m_picture.endRecording();
    EXPECT_EQ(0, m_frameBufferRequestCount);
    m_canvas->drawPicture(m_picture);
    EXPECT_EQ(1, m_frameBufferRequestCount);
}

} // namespace WebCore
