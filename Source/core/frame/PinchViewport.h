/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#ifndef PinchViewport_h
#define PinchViewport_h

#include "platform/geometry/IntSize.h"
#include "platform/graphics/GraphicsLayerClient.h"
#include "public/platform/WebScrollbar.h"
#include "public/platform/WebSize.h"
#include "wtf/OwnPtr.h"
#include "wtf/PassOwnPtr.h"

namespace blink {
class WebLayerTreeView;
class WebScrollbarLayer;
}

namespace WebCore {

class FrameHost;
class GraphicsContext;
class GraphicsLayer;
class GraphicsLayerFactory;
class IntRect;
class IntSize;

class PinchViewport FINAL : WebCore::GraphicsLayerClient {
public:
    PinchViewport(FrameHost&);
    virtual ~PinchViewport();

    void attachToLayerTree(WebCore::GraphicsLayer*, GraphicsLayerFactory*);
    WebCore::GraphicsLayer* rootGraphicsLayer()
    {
        return m_innerViewportContainerLayer.get();
    }
    void setViewportSize(const WebCore::IntSize&);

    void registerViewportLayersWithTreeView(blink::WebLayerTreeView*) const;
    void clearViewportLayersForTreeView(blink::WebLayerTreeView*) const;

private:

    // GraphicsLayerClient implementation.
    virtual void notifyAnimationStarted(const WebCore::GraphicsLayer*, double monotonicTime) OVERRIDE;
    virtual void paintContents(const WebCore::GraphicsLayer*, WebCore::GraphicsContext&, WebCore::GraphicsLayerPaintingPhase, const WebCore::IntRect& inClip) OVERRIDE;
    virtual String debugName(const WebCore::GraphicsLayer*) OVERRIDE;

    void setupScrollbar(blink::WebScrollbar::Orientation);

    WebCore::FrameHost& m_owner;
    OwnPtr<WebCore::GraphicsLayer> m_innerViewportContainerLayer;
    OwnPtr<WebCore::GraphicsLayer> m_pageScaleLayer;
    OwnPtr<WebCore::GraphicsLayer> m_innerViewportScrollLayer;
    OwnPtr<WebCore::GraphicsLayer> m_overlayScrollbarHorizontal;
    OwnPtr<WebCore::GraphicsLayer> m_overlayScrollbarVertical;
    OwnPtr<blink::WebScrollbarLayer> m_webOverlayScrollbarHorizontal;
    OwnPtr<blink::WebScrollbarLayer> m_webOverlayScrollbarVertical;
};

} // namespace WebCore

#endif // PinchViewport_h
