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

#ifndef AcceleratedImageBufferSurface_h
#define AcceleratedImageBufferSurface_h

#include "platform/graphics/ImageBufferSurface.h"
#include "public/platform/WebGraphicsContext3DProvider.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "wtf/OwnPtr.h"

namespace WebCore {

class PLATFORM_EXPORT AcceleratedImageBufferSurface : public ImageBufferSurface {
    WTF_MAKE_NONCOPYABLE(AcceleratedImageBufferSurface); WTF_MAKE_FAST_ALLOCATED;
public:
    AcceleratedImageBufferSurface(const IntSize&, OpacityMode = NonOpaque, int msaaSampleCount = 0);
    virtual ~AcceleratedImageBufferSurface() { }

    virtual SkCanvas* canvas() const OVERRIDE { return m_surface ? m_surface->getCanvas() : 0; }
    virtual bool isValid() const OVERRIDE { return m_surface; }
    virtual bool isAccelerated() const OVERRIDE { return true; }
    virtual Platform3DObject getBackingTexture() const OVERRIDE;

private:
    OwnPtr<SkSurface> m_surface;
    OwnPtr<blink::WebGraphicsContext3DProvider> m_contextProvider;
};


} // namespace WebCore

#endif
