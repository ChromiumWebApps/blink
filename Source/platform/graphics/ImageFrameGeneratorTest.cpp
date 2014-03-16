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
#include "platform/graphics/ImageFrameGenerator.h"

#include "platform/SharedBuffer.h"
#include "platform/Task.h"
#include "platform/graphics/ImageDecodingStore.h"
#include "platform/graphics/test/MockImageDecoder.h"
#include "public/platform/Platform.h"
#include "public/platform/WebThread.h"
#include <gtest/gtest.h>

namespace WebCore {

namespace {

// Helper methods to generate standard sizes.
SkISize fullSize() { return SkISize::Make(100, 100); }

} // namespace

class ImageFrameGeneratorTest : public ::testing::Test, public MockImageDecoderClient {
public:
    virtual void SetUp() OVERRIDE
    {
        ImageDecodingStore::initializeOnce();
        ImageDecodingStore::instance()->setImageCachingEnabled(true);
        m_data = SharedBuffer::create();
        m_generator = ImageFrameGenerator::create(fullSize(), m_data, false);
        useMockImageDecoderFactory();
        m_decodersDestroyed = 0;
        m_frameBufferRequestCount = 0;
        m_status = ImageFrame::FrameEmpty;
    }

    virtual void TearDown() OVERRIDE
    {
        ImageDecodingStore::shutdown();
    }

    virtual void decoderBeingDestroyed() OVERRIDE
    {
        ++m_decodersDestroyed;
    }

    virtual void frameBufferRequested() OVERRIDE
    {
        ++m_frameBufferRequestCount;
    }

    virtual ImageFrame::Status status() OVERRIDE
    {
        ImageFrame::Status currentStatus = m_status;
        m_status = m_nextFrameStatus;
        return currentStatus;
    }

    virtual size_t frameCount() OVERRIDE { return 1; }
    virtual int repetitionCount() const OVERRIDE { return cAnimationNone; }
    virtual float frameDuration() const OVERRIDE { return 0; }

protected:
    void useMockImageDecoderFactory()
    {
        m_generator->setImageDecoderFactory(MockImageDecoderFactory::create(this, fullSize()));
    }

    PassOwnPtr<ScaledImageFragment> createCompleteImage(const SkISize& size)
    {
        SkBitmap bitmap;
        bitmap.setConfig(SkBitmap::kARGB_8888_Config, size.width(), size.height());
        bitmap.allocPixels();
        return ScaledImageFragment::createComplete(size, 0, bitmap);
    }

    void addNewData()
    {
        m_data->append("g", 1);
        m_generator->setData(m_data, false);
    }

    void setFrameStatus(ImageFrame::Status status)  { m_status = m_nextFrameStatus = status; }
    void setNextFrameStatus(ImageFrame::Status status)  { m_nextFrameStatus = status; }

    SkBitmap::Allocator* allocator() const { return m_generator->allocator(); }
    void setAllocator(PassOwnPtr<SkBitmap::Allocator> allocator)
    {
        m_generator->setAllocator(allocator);
    }

    PassOwnPtr<ScaledImageFragment> decode(size_t index)
    {
        ImageDecoder* decoder = 0;
        OwnPtr<ScaledImageFragment> fragment = m_generator->decode(index, &decoder);
        delete decoder;
        return fragment.release();
    }

    RefPtr<SharedBuffer> m_data;
    RefPtr<ImageFrameGenerator> m_generator;
    int m_decodersDestroyed;
    int m_frameBufferRequestCount;
    ImageFrame::Status m_status;
    ImageFrame::Status m_nextFrameStatus;
};

TEST_F(ImageFrameGeneratorTest, cacheHit)
{
    const ScaledImageFragment* fullImage = ImageDecodingStore::instance()->insertAndLockCache(
        m_generator.get(), createCompleteImage(fullSize()));
    EXPECT_EQ(fullSize(), fullImage->scaledSize());
    ImageDecodingStore::instance()->unlockCache(m_generator.get(), fullImage);

    const ScaledImageFragment* tempImage = m_generator->decodeAndScale(fullSize());
    EXPECT_EQ(fullImage, tempImage);
    EXPECT_EQ(fullSize(), tempImage->scaledSize());
    EXPECT_TRUE(m_generator->hasAlpha(0));
    ImageDecodingStore::instance()->unlockCache(m_generator.get(), tempImage);
    EXPECT_EQ(0, m_frameBufferRequestCount);
}

TEST_F(ImageFrameGeneratorTest, cacheMissWithIncompleteDecode)
{
    setFrameStatus(ImageFrame::FramePartial);

    const ScaledImageFragment* tempImage = m_generator->decodeAndScale(fullSize());
    EXPECT_FALSE(tempImage->isComplete());
    EXPECT_EQ(1, m_frameBufferRequestCount);
    ImageDecodingStore::instance()->unlockCache(m_generator.get(), tempImage);
    EXPECT_EQ(2, ImageDecodingStore::instance()->cacheEntries());
    EXPECT_EQ(1, ImageDecodingStore::instance()->imageCacheEntries());
    EXPECT_EQ(1, ImageDecodingStore::instance()->decoderCacheEntries());

    addNewData();
    tempImage = m_generator->decodeAndScale(fullSize());
    EXPECT_FALSE(tempImage->isComplete());
    EXPECT_EQ(2, m_frameBufferRequestCount);
    ImageDecodingStore::instance()->unlockCache(m_generator.get(), tempImage);
    EXPECT_EQ(3, ImageDecodingStore::instance()->cacheEntries());
    EXPECT_EQ(2, ImageDecodingStore::instance()->imageCacheEntries());
    EXPECT_EQ(1, ImageDecodingStore::instance()->decoderCacheEntries());
    EXPECT_EQ(0, m_decodersDestroyed);
}

TEST_F(ImageFrameGeneratorTest, incompleteDecodeBecomesComplete)
{
    setFrameStatus(ImageFrame::FramePartial);

    const ScaledImageFragment* tempImage = m_generator->decodeAndScale(fullSize());
    EXPECT_FALSE(tempImage->isComplete());
    EXPECT_EQ(1, m_frameBufferRequestCount);
    EXPECT_EQ(0, m_decodersDestroyed);
    ImageDecodingStore::instance()->unlockCache(m_generator.get(), tempImage);
    EXPECT_EQ(2, ImageDecodingStore::instance()->cacheEntries());
    EXPECT_EQ(1, ImageDecodingStore::instance()->imageCacheEntries());
    EXPECT_EQ(1, ImageDecodingStore::instance()->decoderCacheEntries());

    setFrameStatus(ImageFrame::FrameComplete);
    addNewData();

    tempImage = m_generator->decodeAndScale(fullSize());
    EXPECT_TRUE(tempImage->isComplete());
    EXPECT_EQ(2, m_frameBufferRequestCount);
    EXPECT_EQ(1, m_decodersDestroyed);
    ImageDecodingStore::instance()->unlockCache(m_generator.get(), tempImage);
    EXPECT_EQ(2, ImageDecodingStore::instance()->cacheEntries());
    EXPECT_EQ(2, ImageDecodingStore::instance()->imageCacheEntries());
    EXPECT_EQ(0, ImageDecodingStore::instance()->decoderCacheEntries());

    tempImage = m_generator->decodeAndScale(fullSize());
    EXPECT_TRUE(tempImage->isComplete());
    EXPECT_EQ(2, m_frameBufferRequestCount);
    ImageDecodingStore::instance()->unlockCache(m_generator.get(), tempImage);
}

static void decodeThreadMain(ImageFrameGenerator* generator)
{
    const ScaledImageFragment* tempImage = generator->decodeAndScale(fullSize());
    ImageDecodingStore::instance()->unlockCache(generator, tempImage);
}

TEST_F(ImageFrameGeneratorTest, incompleteDecodeBecomesCompleteMultiThreaded)
{
    setFrameStatus(ImageFrame::FramePartial);

    const ScaledImageFragment* tempImage = m_generator->decodeAndScale(fullSize());
    EXPECT_FALSE(tempImage->isComplete());
    EXPECT_EQ(1, m_frameBufferRequestCount);
    EXPECT_EQ(0, m_decodersDestroyed);
    ImageDecodingStore::instance()->unlockCache(m_generator.get(), tempImage);
    EXPECT_EQ(2, ImageDecodingStore::instance()->cacheEntries());
    EXPECT_EQ(1, ImageDecodingStore::instance()->imageCacheEntries());
    EXPECT_EQ(1, ImageDecodingStore::instance()->decoderCacheEntries());

    // LocalFrame can now be decoded completely.
    setFrameStatus(ImageFrame::FrameComplete);
    addNewData();
    OwnPtr<blink::WebThread> thread = adoptPtr(blink::Platform::current()->createThread("DecodeThread"));
    thread->postTask(new Task(WTF::bind(&decodeThreadMain, m_generator.get())));
    thread.clear();

    EXPECT_EQ(2, m_frameBufferRequestCount);
    EXPECT_EQ(1, m_decodersDestroyed);
    EXPECT_EQ(2, ImageDecodingStore::instance()->cacheEntries());
    EXPECT_EQ(2, ImageDecodingStore::instance()->imageCacheEntries());
    EXPECT_EQ(0, ImageDecodingStore::instance()->decoderCacheEntries());

    tempImage = m_generator->decodeAndScale(fullSize());
    EXPECT_TRUE(tempImage->isComplete());
    EXPECT_EQ(2, m_frameBufferRequestCount);
    ImageDecodingStore::instance()->unlockCache(m_generator.get(), tempImage);
}

TEST_F(ImageFrameGeneratorTest, incompleteBitmapCopied)
{
    setFrameStatus(ImageFrame::FramePartial);

    const ScaledImageFragment* tempImage= m_generator->decodeAndScale(fullSize());
    EXPECT_FALSE(tempImage->isComplete());
    EXPECT_EQ(1, m_frameBufferRequestCount);

    ImageDecoder* tempDecoder = 0;
    EXPECT_TRUE(ImageDecodingStore::instance()->lockDecoder(m_generator.get(), fullSize(), &tempDecoder));
    ASSERT_TRUE(tempDecoder);
    EXPECT_NE(tempDecoder->frameBufferAtIndex(0)->getSkBitmap().getPixels(), tempImage->bitmap().getPixels());
    ImageDecodingStore::instance()->unlockCache(m_generator.get(), tempImage);
    ImageDecodingStore::instance()->unlockDecoder(m_generator.get(), tempDecoder);
}

TEST_F(ImageFrameGeneratorTest, resumeDecodeEmptyFrameTurnsComplete)
{
    m_generator = ImageFrameGenerator::create(fullSize(), m_data, false, true);
    useMockImageDecoderFactory();
    setFrameStatus(ImageFrame::FrameComplete);

    const ScaledImageFragment* tempImage = m_generator->decodeAndScale(fullSize(), 0);
    EXPECT_TRUE(tempImage->isComplete());
    ImageDecodingStore::instance()->unlockCache(m_generator.get(), tempImage);

    setFrameStatus(ImageFrame::FrameEmpty);
    setNextFrameStatus(ImageFrame::FrameComplete);
    EXPECT_FALSE(m_generator->decodeAndScale(fullSize(), 1));
}

TEST_F(ImageFrameGeneratorTest, frameHasAlpha)
{
    setFrameStatus(ImageFrame::FramePartial);
    ImageDecodingStore::instance()->unlockCache(m_generator.get(), m_generator->decodeAndScale(fullSize(), 0));
    EXPECT_TRUE(m_generator->hasAlpha(0));

    ImageDecoder* tempDecoder = 0;
    EXPECT_TRUE(ImageDecodingStore::instance()->lockDecoder(m_generator.get(), fullSize(), &tempDecoder));
    ASSERT_TRUE(tempDecoder);
    static_cast<MockImageDecoder*>(tempDecoder)->setFrameHasAlpha(false);
    ImageDecodingStore::instance()->unlockDecoder(m_generator.get(), tempDecoder);

    setFrameStatus(ImageFrame::FrameComplete);
    ImageDecodingStore::instance()->unlockCache(m_generator.get(), m_generator->decodeAndScale(fullSize(), 0));
    EXPECT_FALSE(m_generator->hasAlpha(0));
}

namespace {

class MockAllocator : public SkBitmap::Allocator {
public:
    // N starts from 0.
    MockAllocator(int failAtNthCall)
        : m_callCount(0)
        , m_failAtNthCall(failAtNthCall)
        , m_defaultAllocator(adoptPtr(new DiscardablePixelRefAllocator()))
    {
    }

    virtual bool allocPixelRef(SkBitmap* bitmap, SkColorTable* colorTable) OVERRIDE
    {
        if (m_callCount++ == m_failAtNthCall)
            return false;
        return m_defaultAllocator->allocPixelRef(bitmap, colorTable);
    }

    int m_callCount;
    int m_failAtNthCall;
    OwnPtr<SkBitmap::Allocator> m_defaultAllocator;
};

} // namespace

TEST_F(ImageFrameGeneratorTest, decodingAllocatorFailure)
{
    // Try to emulate allocation failures at different stages. For now, the
    // first allocation is for the bitmap in ImageFrame, the second is for the
    // copy of partial bitmap. The loop will still work if the number or purpose
    // of allocations change in the future.
    for (int i = 0; ; ++i) {
        SCOPED_TRACE(testing::Message() << "Allocation failure at call " << i);
        setFrameStatus(ImageFrame::FramePartial);
        setAllocator(adoptPtr(new MockAllocator(i)));
        OwnPtr<ScaledImageFragment> image = decode(0);
        if (i >= static_cast<MockAllocator*>(allocator())->m_callCount) {
            // We have tested failures of all stages. This time all allocations
            // were successful.
            EXPECT_TRUE(image);
            break;
        }
        EXPECT_FALSE(image);
    }
}

} // namespace WebCore
