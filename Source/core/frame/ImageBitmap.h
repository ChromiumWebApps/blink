// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ImageBitmap_h
#define ImageBitmap_h

#include "bindings/v8/ScriptWrappable.h"
#include "core/html/HTMLImageElement.h"
#include "core/html/canvas/CanvasImageSource.h"
#include "heap/Handle.h"
#include "platform/geometry/IntRect.h"
#include "platform/graphics/Image.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefCounted.h"

namespace WebCore {

class HTMLCanvasElement;
class HTMLVideoElement;
class ImageData;

class ImageBitmap FINAL : public RefCountedWillBeGarbageCollectedFinalized<ImageBitmap>, public ScriptWrappable, public ImageLoaderClient, public CanvasImageSource {
    WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(ImageBitmap);
public:
    static PassRefPtrWillBeRawPtr<ImageBitmap> create(HTMLImageElement*, const IntRect&);
    static PassRefPtrWillBeRawPtr<ImageBitmap> create(HTMLVideoElement*, const IntRect&);
    static PassRefPtrWillBeRawPtr<ImageBitmap> create(HTMLCanvasElement*, const IntRect&);
    static PassRefPtrWillBeRawPtr<ImageBitmap> create(ImageData*, const IntRect&);
    static PassRefPtrWillBeRawPtr<ImageBitmap> create(ImageBitmap*, const IntRect&);
    static PassRefPtrWillBeRawPtr<ImageBitmap> create(Image*, const IntRect&);

    PassRefPtr<Image> bitmapImage() const;
    PassRefPtr<HTMLImageElement> imageElement() const { return m_imageElement; }

    IntRect bitmapRect() const { return m_bitmapRect; }

    int width() const { return m_cropRect.width(); }
    int height() const { return m_cropRect.height(); }
    IntSize size() const { return m_cropRect.size(); }

    virtual ~ImageBitmap();

    // CanvasImageSource implementation
    virtual PassRefPtr<Image> getSourceImageForCanvas(SourceImageMode, SourceImageStatus*) const OVERRIDE;
    virtual bool wouldTaintOrigin(SecurityOrigin*) const OVERRIDE { return false; };
    virtual void adjustDrawRects(FloatRect* srcRect, FloatRect* dstRect) const OVERRIDE;
    virtual FloatSize sourceSize() const OVERRIDE;

    virtual void trace(Visitor*);

private:
    ImageBitmap(HTMLImageElement*, const IntRect&);
    ImageBitmap(HTMLVideoElement*, const IntRect&);
    ImageBitmap(HTMLCanvasElement*, const IntRect&);
    ImageBitmap(ImageData*, const IntRect&);
    ImageBitmap(ImageBitmap*, const IntRect&);
    ImageBitmap(Image*, const IntRect&);

    // ImageLoaderClient
    virtual void notifyImageSourceChanged() OVERRIDE;
    virtual bool requestsHighLiveResourceCachePriority() OVERRIDE { return true; }

    // ImageBitmaps constructed from HTMLImageElements hold a reference to the HTMLImageElement until
    // the image source changes.
    RefPtr<HTMLImageElement> m_imageElement;
    RefPtr<Image> m_bitmap;

    IntRect m_bitmapRect; // The rect where the underlying Image should be placed in reference to the ImageBitmap.
    IntRect m_cropRect;

    // The offset by which the desired Image is stored internally.
    // ImageBitmaps constructed from HTMLImageElements reference the entire ImageResource and may have a non-zero bitmap offset.
    // ImageBitmaps not constructed from HTMLImageElements always pre-crop and store the image at (0, 0).
    IntPoint m_bitmapOffset;

};

} // namespace WebCore

#endif // ImageBitmap_h
