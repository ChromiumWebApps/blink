/*
 * Copyright (c) 2013, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "modules/imagebitmap/ImageBitmapFactories.h"

#include "bindings/v8/ExceptionState.h"
#include "bindings/v8/ScriptScope.h"
#include "core/fileapi/Blob.h"
#include "core/html/HTMLCanvasElement.h"
#include "core/html/HTMLImageElement.h"
#include "core/html/HTMLVideoElement.h"
#include "core/html/ImageData.h"
#include "core/html/canvas/CanvasRenderingContext2D.h"
#include "core/frame/DOMWindow.h"
#include "core/frame/ImageBitmap.h"
#include "core/workers/WorkerGlobalScope.h"
#include "platform/SharedBuffer.h"
#include "platform/graphics/BitmapImage.h"
#include "platform/graphics/ImageSource.h"
#include "platform/graphics/skia/NativeImageSkia.h"

namespace WebCore {

static LayoutSize sizeFor(HTMLImageElement* image)
{
    if (ImageResource* cachedImage = image->cachedImage())
        return cachedImage->imageSizeForRenderer(image->renderer(), 1.0f); // FIXME: Not sure about this.
    return IntSize();
}

static IntSize sizeFor(HTMLVideoElement* video)
{
    if (MediaPlayer* player = video->player())
        return player->naturalSize();
    return IntSize();
}

static ScriptPromise fulfillImageBitmap(ExecutionContext* context, PassRefPtrWillBeRawPtr<ImageBitmap> imageBitmap)
{
    RefPtr<ScriptPromiseResolver> resolver = ScriptPromiseResolver::create(context);
    ScriptPromise promise = resolver->promise();
    resolver->resolve(imageBitmap);
    return promise;
}

ScriptPromise ImageBitmapFactories::createImageBitmap(EventTarget& eventTarget, HTMLImageElement* image, ExceptionState& exceptionState)
{
    LayoutSize s = sizeFor(image);
    return createImageBitmap(eventTarget, image, 0, 0, s.width(), s.height(), exceptionState);
}

ScriptPromise ImageBitmapFactories::createImageBitmap(EventTarget& eventTarget, HTMLImageElement* image, int sx, int sy, int sw, int sh, ExceptionState& exceptionState)
{
    // This variant does not work in worker threads.
    ASSERT(eventTarget.toDOMWindow());

    if (!image) {
        exceptionState.throwTypeError("The image element provided is invalid.");
        return ScriptPromise();
    }
    if (!image->cachedImage()) {
        exceptionState.throwDOMException(InvalidStateError, "No image can be retrieved from the provided element.");
        return ScriptPromise();
    }
    if (image->cachedImage()->image()->isSVGImage()) {
        exceptionState.throwDOMException(InvalidStateError, "The image element contains an SVG image, which is unsupported.");
        return ScriptPromise();
    }
    if (!sw || !sh) {
        exceptionState.throwDOMException(IndexSizeError, String::format("The source %s provided is 0.", sw ? "height" : "width"));
        return ScriptPromise();
    }
    if (!image->cachedImage()->image()->currentFrameHasSingleSecurityOrigin()) {
        exceptionState.throwSecurityError("The source image contains image data from multiple origins.");
        return ScriptPromise();
    }
    if (!image->cachedImage()->passesAccessControlCheck(eventTarget.toDOMWindow()->document()->securityOrigin()) && eventTarget.toDOMWindow()->document()->securityOrigin()->taintsCanvas(image->src())) {
        exceptionState.throwSecurityError("Cross-origin access to the source image is denied.");
        return ScriptPromise();
    }
    // FIXME: make ImageBitmap creation asynchronous crbug.com/258082
    return fulfillImageBitmap(eventTarget.executionContext(), ImageBitmap::create(image, IntRect(sx, sy, sw, sh)));
}

ScriptPromise ImageBitmapFactories::createImageBitmap(EventTarget& eventTarget, HTMLVideoElement* video, ExceptionState& exceptionState)
{
    IntSize s = sizeFor(video);
    return createImageBitmap(eventTarget, video, 0, 0, s.width(), s.height(), exceptionState);
}

ScriptPromise ImageBitmapFactories::createImageBitmap(EventTarget& eventTarget, HTMLVideoElement* video, int sx, int sy, int sw, int sh, ExceptionState& exceptionState)
{
    // This variant does not work in worker threads.
    ASSERT(eventTarget.toDOMWindow());

    if (!video) {
        exceptionState.throwTypeError("The video element provided is invalid.");
        return ScriptPromise();
    }
    if (!video->player()) {
        exceptionState.throwDOMException(InvalidStateError, "No player can be retrieved from the provided video element.");
        return ScriptPromise();
    }
    if (video->networkState() == HTMLMediaElement::NETWORK_EMPTY) {
        exceptionState.throwDOMException(InvalidStateError, "The provided element has not retrieved data.");
        return ScriptPromise();
    }
    if (video->player()->readyState() <= MediaPlayer::HaveMetadata) {
        exceptionState.throwDOMException(InvalidStateError, "The provided element's player has no current data.");
        return ScriptPromise();
    }
    if (!sw || !sh) {
        exceptionState.throwDOMException(IndexSizeError, String::format("The source %s provided is 0.", sw ? "height" : "width"));
        return ScriptPromise();
    }
    if (!video->hasSingleSecurityOrigin()) {
        exceptionState.throwSecurityError("The source video contains image data from multiple origins.");
        return ScriptPromise();
    }
    if (!video->player()->didPassCORSAccessCheck() && eventTarget.toDOMWindow()->document()->securityOrigin()->taintsCanvas(video->currentSrc())) {
        exceptionState.throwSecurityError("Cross-origin access to the source video is denied.");
        return ScriptPromise();
    }
    // FIXME: make ImageBitmap creation asynchronous crbug.com/258082
    return fulfillImageBitmap(eventTarget.executionContext(), ImageBitmap::create(video, IntRect(sx, sy, sw, sh)));
}

ScriptPromise ImageBitmapFactories::createImageBitmap(EventTarget& eventTarget, CanvasRenderingContext2D* context, ExceptionState& exceptionState)
{
    return createImageBitmap(eventTarget, context->canvas(), exceptionState);
}

ScriptPromise ImageBitmapFactories::createImageBitmap(EventTarget& eventTarget, CanvasRenderingContext2D* context, int sx, int sy, int sw, int sh, ExceptionState& exceptionState)
{
    return createImageBitmap(eventTarget, context->canvas(), sx, sy, sw, sh, exceptionState);
}

ScriptPromise ImageBitmapFactories::createImageBitmap(EventTarget& eventTarget, HTMLCanvasElement* canvas, ExceptionState& exceptionState)
{
    return createImageBitmap(eventTarget, canvas, 0, 0, canvas->width(), canvas->height(), exceptionState);
}

ScriptPromise ImageBitmapFactories::createImageBitmap(EventTarget& eventTarget, HTMLCanvasElement* canvas, int sx, int sy, int sw, int sh, ExceptionState& exceptionState)
{
    // This variant does not work in worker threads.
    ASSERT(eventTarget.toDOMWindow());

    if (!canvas) {
        exceptionState.throwTypeError("The canvas element provided is invalid.");
        return ScriptPromise();
    }
    if (!canvas->originClean()) {
        exceptionState.throwSecurityError("The canvas element provided is tainted with cross-origin data.");
        return ScriptPromise();
    }
    if (!sw || !sh) {
        exceptionState.throwDOMException(IndexSizeError, String::format("The source %s provided is 0.", sw ? "height" : "width"));
        return ScriptPromise();
    }
    // FIXME: make ImageBitmap creation asynchronous crbug.com/258082
    return fulfillImageBitmap(eventTarget.executionContext(), ImageBitmap::create(canvas, IntRect(sx, sy, sw, sh)));
}

ScriptPromise ImageBitmapFactories::createImageBitmap(EventTarget& eventTarget, Blob* blob, ExceptionState& exceptionState)
{
    if (!blob) {
        exceptionState.throwTypeError("The blob provided is invalid.");
        return ScriptPromise();
    }
    RefPtr<ScriptPromiseResolver> resolver = ScriptPromiseResolver::create(eventTarget.executionContext());
    ScriptPromise promise = resolver->promise();
    RefPtr<ImageBitmapLoader> loader = ImageBitmapFactories::ImageBitmapLoader::create(from(eventTarget), resolver, IntRect());
    from(eventTarget).addLoader(loader);
    loader->loadBlobAsync(eventTarget.executionContext(), blob);
    return promise;
}

ScriptPromise ImageBitmapFactories::createImageBitmap(EventTarget& eventTarget, Blob* blob, int sx, int sy, int sw, int sh, ExceptionState& exceptionState)
{
    if (!blob) {
        exceptionState.throwTypeError("The blob provided is invalid.");
        return ScriptPromise();
    }
    if (!sw || !sh) {
        exceptionState.throwDOMException(IndexSizeError, String::format("The source %s provided is 0.", sw ? "height" : "width"));
        return ScriptPromise();
    }
    RefPtr<ScriptPromiseResolver> resolver = ScriptPromiseResolver::create(eventTarget.executionContext());
    ScriptPromise promise = resolver->promise();
    RefPtr<ImageBitmapLoader> loader = ImageBitmapFactories::ImageBitmapLoader::create(from(eventTarget), resolver, IntRect(sx, sy, sw, sh));
    from(eventTarget).addLoader(loader);
    loader->loadBlobAsync(eventTarget.executionContext(), blob);
    return promise;
}

ScriptPromise ImageBitmapFactories::createImageBitmap(EventTarget& eventTarget, ImageData* data, ExceptionState& exceptionState)
{
    return createImageBitmap(eventTarget, data, 0, 0, data->width(), data->height(), exceptionState);
}

ScriptPromise ImageBitmapFactories::createImageBitmap(EventTarget& eventTarget, ImageData* data, int sx, int sy, int sw, int sh, ExceptionState& exceptionState)
{
    if (!data) {
        exceptionState.throwTypeError("The ImageData provided is invalid.");
        return ScriptPromise();
    }
    if (!sw || !sh) {
        exceptionState.throwDOMException(IndexSizeError, String::format("The source %s provided is 0.", sw ? "height" : "width"));
        return ScriptPromise();
    }
    // FIXME: make ImageBitmap creation asynchronous crbug.com/258082
    return fulfillImageBitmap(eventTarget.executionContext(), ImageBitmap::create(data, IntRect(sx, sy, sw, sh)));
}

ScriptPromise ImageBitmapFactories::createImageBitmap(EventTarget& eventTarget, ImageBitmap* bitmap, ExceptionState& exceptionState)
{
    return createImageBitmap(eventTarget, bitmap, 0, 0, bitmap->width(), bitmap->height(), exceptionState);
}

ScriptPromise ImageBitmapFactories::createImageBitmap(EventTarget& eventTarget, ImageBitmap* bitmap, int sx, int sy, int sw, int sh, ExceptionState& exceptionState)
{
    if (!bitmap) {
        exceptionState.throwTypeError("The ImageBitmap provided is invalid.");
        return ScriptPromise();
    }
    if (!sw || !sh) {
        exceptionState.throwDOMException(IndexSizeError, String::format("The source %s provided is 0.", sw ? "height" : "width"));
        return ScriptPromise();
    }
    // FIXME: make ImageBitmap creation asynchronous crbug.com/258082
    return fulfillImageBitmap(eventTarget.executionContext(), ImageBitmap::create(bitmap, IntRect(sx, sy, sw, sh)));
}

const char* ImageBitmapFactories::supplementName()
{
    return "ImageBitmapFactories";
}

ImageBitmapFactories& ImageBitmapFactories::from(EventTarget& eventTarget)
{
    if (DOMWindow* window = eventTarget.toDOMWindow())
        return fromInternal(*window);

    ASSERT(eventTarget.executionContext()->isWorkerGlobalScope());
    return WorkerGlobalScopeImageBitmapFactories::fromInternal(*toWorkerGlobalScope(eventTarget.executionContext()));
}

ImageBitmapFactories& ImageBitmapFactories::fromInternal(DOMWindow& object)
{
    ImageBitmapFactories* supplement = static_cast<ImageBitmapFactories*>(Supplement<DOMWindow>::from(object, supplementName()));
    if (!supplement) {
        supplement = new ImageBitmapFactories();
        Supplement<DOMWindow>::provideTo(object, supplementName(), adoptPtr(supplement));
    }
    return *supplement;
}

void ImageBitmapFactories::addLoader(PassRefPtr<ImageBitmapLoader> loader)
{
    m_pendingLoaders.add(loader);
}

void ImageBitmapFactories::didFinishLoading(ImageBitmapLoader* loader)
{
    ASSERT(m_pendingLoaders.contains(loader));
    m_pendingLoaders.remove(loader);
}

ImageBitmapFactories::ImageBitmapLoader::ImageBitmapLoader(ImageBitmapFactories& factory, PassRefPtr<ScriptPromiseResolver> resolver, const IntRect& cropRect)
    : m_scriptState(ScriptState::current())
    , m_loader(FileReaderLoader::ReadAsArrayBuffer, this)
    , m_factory(&factory)
    , m_resolver(resolver)
    , m_cropRect(cropRect)
{
}

void ImageBitmapFactories::ImageBitmapLoader::loadBlobAsync(ExecutionContext* context, Blob* blob)
{
    m_loader.start(context, blob->blobDataHandle());
}

void ImageBitmapFactories::ImageBitmapLoader::rejectPromise()
{
    v8::Isolate* isolate = m_scriptState->isolate();
    ScriptScope scope(m_scriptState);
    m_resolver->reject(ScriptValue(v8::Null(isolate), isolate));
    m_factory->didFinishLoading(this);
}

void ImageBitmapFactories::ImageBitmapLoader::didFinishLoading()
{
    if (!m_loader.arrayBufferResult()) {
        rejectPromise();
        return;
    }
    RefPtr<SharedBuffer> sharedBuffer = SharedBuffer::create((char*)m_loader.arrayBufferResult()->data(), m_loader.arrayBufferResult()->byteLength());

    OwnPtr<ImageSource> source = adoptPtr(new ImageSource());
    source->setData(sharedBuffer.get(), true);
    RefPtr<NativeImageSkia> imageSkia = source->createFrameAtIndex(0);
    if (!imageSkia) {
        rejectPromise();
        return;
    }

    RefPtr<Image> image = BitmapImage::create(imageSkia);
    if (!image->width() || !image->height()) {
        rejectPromise();
        return;
    }
    if (!m_cropRect.width() && !m_cropRect.height()) {
        // No cropping variant was called.
        m_cropRect = IntRect(IntPoint(), image->size());
    }

    RefPtrWillBeRawPtr<ImageBitmap> imageBitmap = ImageBitmap::create(image.get(), m_cropRect);
    ScriptScope scope(m_scriptState);
    m_resolver->resolve(imageBitmap.release());
    m_factory->didFinishLoading(this);
}

void ImageBitmapFactories::ImageBitmapLoader::didFail(FileError::ErrorCode)
{
    rejectPromise();
}

ImageBitmapFactories& WorkerGlobalScopeImageBitmapFactories::fromInternal(WorkerGlobalScope& object)
{
    WorkerGlobalScopeImageBitmapFactories* supplement = static_cast<WorkerGlobalScopeImageBitmapFactories*>(WillBeHeapSupplement<WorkerGlobalScope>::from(object, ImageBitmapFactories::supplementName()));
    if (!supplement) {
        supplement = new WorkerGlobalScopeImageBitmapFactories();
        WillBeHeapSupplement<WorkerGlobalScope>::provideTo(object, ImageBitmapFactories::supplementName(), adoptPtrWillBeNoop(supplement));
    }
    return *supplement;
}

void WorkerGlobalScopeImageBitmapFactories::trace(Visitor*)
{
}

} // namespace WebCore
