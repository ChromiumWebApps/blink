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

#ifndef ImageBitmapFactories_h
#define ImageBitmapFactories_h

#include "bindings/v8/ScriptPromise.h"
#include "bindings/v8/ScriptPromiseResolver.h"
#include "bindings/v8/ScriptState.h"
#include "core/fileapi/FileReaderLoader.h"
#include "core/fileapi/FileReaderLoaderClient.h"
#include "platform/Supplementable.h"
#include "platform/geometry/IntRect.h"
#include "wtf/Forward.h"
#include "wtf/HashSet.h"

namespace WebCore {

class Blob;
class CanvasRenderingContext2D;
class EventTarget;
class ExceptionState;
class HTMLCanvasElement;
class HTMLImageElement;
class HTMLVideoElement;
class ImageBitmap;
class ImageData;
class ExecutionContext;

class ImageBitmapFactories : public Supplement<DOMWindow> {

class ImageBitmapLoader;

public:
    static ScriptPromise createImageBitmap(EventTarget&, HTMLImageElement*, ExceptionState&);
    static ScriptPromise createImageBitmap(EventTarget&, HTMLImageElement*, int sx, int sy, int sw, int sh, ExceptionState&);
    static ScriptPromise createImageBitmap(EventTarget&, HTMLVideoElement*, ExceptionState&);
    static ScriptPromise createImageBitmap(EventTarget&, HTMLVideoElement*, int sx, int sy, int sw, int sh, ExceptionState&);
    static ScriptPromise createImageBitmap(EventTarget&, CanvasRenderingContext2D*, ExceptionState&);
    static ScriptPromise createImageBitmap(EventTarget&, CanvasRenderingContext2D*, int sx, int sy, int sw, int sh, ExceptionState&);
    static ScriptPromise createImageBitmap(EventTarget&, HTMLCanvasElement*, ExceptionState&);
    static ScriptPromise createImageBitmap(EventTarget&, HTMLCanvasElement*, int sx, int sy, int sw, int sh, ExceptionState&);
    static ScriptPromise createImageBitmap(EventTarget&, Blob*, ExceptionState&);
    static ScriptPromise createImageBitmap(EventTarget&, Blob*, int sx, int sy, int sw, int sh, ExceptionState&);
    static ScriptPromise createImageBitmap(EventTarget&, ImageData*, ExceptionState&);
    static ScriptPromise createImageBitmap(EventTarget&, ImageData*, int sx, int sy, int sw, int sh, ExceptionState&);
    static ScriptPromise createImageBitmap(EventTarget&, ImageBitmap*, ExceptionState&);
    static ScriptPromise createImageBitmap(EventTarget&, ImageBitmap*, int sx, int sy, int sw, int sh, ExceptionState&);

    void didFinishLoading(ImageBitmapLoader*);

    virtual ~ImageBitmapFactories() { }

protected:
    static const char* supplementName();

private:
    class ImageBitmapLoader FINAL : public RefCounted<ImageBitmapLoader>, public FileReaderLoaderClient {
    public:
        static PassRefPtr<ImageBitmapLoader> create(ImageBitmapFactories& factory, PassRefPtr<ScriptPromiseResolver> resolver, const IntRect& cropRect)
        {
            return adoptRef(new ImageBitmapLoader(factory, resolver, cropRect));
        }

        void loadBlobAsync(ExecutionContext*, Blob*);

        virtual ~ImageBitmapLoader() { }

    private:
        ImageBitmapLoader(ImageBitmapFactories&, PassRefPtr<ScriptPromiseResolver>, const IntRect&);

        void rejectPromise();

        // FileReaderLoaderClient
        virtual void didStartLoading() OVERRIDE { }
        virtual void didReceiveData() OVERRIDE { }
        virtual void didFinishLoading() OVERRIDE;
        virtual void didFail(FileError::ErrorCode) OVERRIDE;

        ScriptState* m_scriptState;
        FileReaderLoader m_loader;
        ImageBitmapFactories* m_factory;
        RefPtr<ScriptPromiseResolver> m_resolver;
        IntRect m_cropRect;
    };

    static ImageBitmapFactories& from(EventTarget&);

    static ImageBitmapFactories& fromInternal(DOMWindow&);

    void addLoader(PassRefPtr<ImageBitmapLoader>);

    HashSet<RefPtr<ImageBitmapLoader> > m_pendingLoaders;
};

// FIXME: oilpan: remove once DOMWindow and its Supplementable becomes heap allocated.
class WorkerGlobalScopeImageBitmapFactories FINAL : public NoBaseWillBeGarbageCollectedFinalized<WorkerGlobalScopeImageBitmapFactories>, public ImageBitmapFactories, public WillBeHeapSupplement<WorkerGlobalScope> {
    WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(WorkerGlobalScopeImageBitmapFactories);
public:
    virtual ~WorkerGlobalScopeImageBitmapFactories() { }

    virtual void trace(Visitor*);

private:
    friend class ImageBitmapFactories;
    static ImageBitmapFactories& fromInternal(WorkerGlobalScope&);
};

} // namespace WebCore

#endif // ImageBitmapFactories_h
