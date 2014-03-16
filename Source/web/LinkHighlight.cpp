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

#include "LinkHighlight.h"

#include "SkMatrix44.h"
#include "WebFrameImpl.h"
#include "WebKit.h"
#include "WebViewImpl.h"
#include "core/dom/Node.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/rendering/RenderLayer.h"
#include "core/rendering/RenderLayerModelObject.h"
#include "core/rendering/RenderObject.h"
#include "core/rendering/RenderView.h"
#include "core/rendering/compositing/CompositedLayerMapping.h"
#include "core/rendering/style/ShadowData.h"
#include "platform/graphics/Color.h"
#include "public/platform/Platform.h"
#include "public/platform/WebAnimationCurve.h"
#include "public/platform/WebCompositorSupport.h"
#include "public/platform/WebFloatAnimationCurve.h"
#include "public/platform/WebFloatPoint.h"
#include "public/platform/WebRect.h"
#include "public/platform/WebSize.h"
#include "wtf/CurrentTime.h"
#include "wtf/Vector.h"

using namespace WebCore;

namespace blink {

class WebViewImpl;

PassOwnPtr<LinkHighlight> LinkHighlight::create(Node* node, WebViewImpl* owningWebViewImpl)
{
    return adoptPtr(new LinkHighlight(node, owningWebViewImpl));
}

LinkHighlight::LinkHighlight(Node* node, WebViewImpl* owningWebViewImpl)
    : m_node(node)
    , m_owningWebViewImpl(owningWebViewImpl)
    , m_currentGraphicsLayer(0)
    , m_geometryNeedsUpdate(false)
    , m_isAnimating(false)
    , m_startTime(monotonicallyIncreasingTime())
{
    ASSERT(m_node);
    ASSERT(owningWebViewImpl);
    WebCompositorSupport* compositorSupport = Platform::current()->compositorSupport();
    m_contentLayer = adoptPtr(compositorSupport->createContentLayer(this));
    m_clipLayer = adoptPtr(compositorSupport->createLayer());
    m_clipLayer->setAnchorPoint(WebFloatPoint());
    m_clipLayer->addChild(m_contentLayer->layer());
    m_contentLayer->layer()->setAnimationDelegate(this);
    m_contentLayer->layer()->setDrawsContent(true);
    m_contentLayer->layer()->setOpacity(1);
    m_geometryNeedsUpdate = true;
    updateGeometry();
}

LinkHighlight::~LinkHighlight()
{
    clearGraphicsLayerLinkHighlightPointer();
    releaseResources();
}

WebContentLayer* LinkHighlight::contentLayer()
{
    return m_contentLayer.get();
}

WebLayer* LinkHighlight::clipLayer()
{
    return m_clipLayer.get();
}

void LinkHighlight::releaseResources()
{
    m_node.clear();
}

RenderLayer* LinkHighlight::computeEnclosingCompositingLayer()
{
    if (!m_node || !m_node->renderer())
        return 0;

    // Find the nearest enclosing composited layer and attach to it. We may need to cross frame boundaries
    // to find a suitable layer.
    RenderObject* renderer = m_node->renderer();
    RenderLayerModelObject* repaintContainer;
    do {
        repaintContainer = renderer->containerForRepaint();
        if (!repaintContainer) {
            renderer = renderer->frame()->ownerRenderer();
            if (!renderer)
                return 0;
        }
    } while (!repaintContainer);
    RenderLayer* renderLayer = repaintContainer->layer();

    if (!renderLayer || renderLayer->compositingState() == NotComposited)
        return 0;

    GraphicsLayer* newGraphicsLayer = renderLayer->compositedLayerMapping()->mainGraphicsLayer();
    m_clipLayer->setTransform(SkMatrix44());

    if (!newGraphicsLayer->drawsContent()) {
        if (renderLayer->scrollableArea() && renderLayer->scrollableArea()->usesCompositedScrolling()) {
            ASSERT(renderLayer->hasCompositedLayerMapping() && renderLayer->compositedLayerMapping()->scrollingContentsLayer());
            newGraphicsLayer = renderLayer->compositedLayerMapping()->scrollingContentsLayer();
        }
    }

    if (m_currentGraphicsLayer != newGraphicsLayer) {
        if (m_currentGraphicsLayer)
            clearGraphicsLayerLinkHighlightPointer();

        m_currentGraphicsLayer = newGraphicsLayer;
        m_currentGraphicsLayer->addLinkHighlight(this);
    }

    return renderLayer;
}

static void convertTargetSpaceQuadToCompositedLayer(const FloatQuad& targetSpaceQuad, RenderObject* targetRenderer, RenderObject* compositedRenderer, FloatQuad& compositedSpaceQuad)
{
    ASSERT(targetRenderer);
    ASSERT(compositedRenderer);

    for (unsigned i = 0; i < 4; ++i) {
        IntPoint point;
        switch (i) {
        case 0: point = roundedIntPoint(targetSpaceQuad.p1()); break;
        case 1: point = roundedIntPoint(targetSpaceQuad.p2()); break;
        case 2: point = roundedIntPoint(targetSpaceQuad.p3()); break;
        case 3: point = roundedIntPoint(targetSpaceQuad.p4()); break;
        }

        point = targetRenderer->frame()->view()->contentsToWindow(point);
        point = compositedRenderer->frame()->view()->windowToContents(point);
        FloatPoint floatPoint = compositedRenderer->absoluteToLocal(point, UseTransforms);

        switch (i) {
        case 0: compositedSpaceQuad.setP1(floatPoint); break;
        case 1: compositedSpaceQuad.setP2(floatPoint); break;
        case 2: compositedSpaceQuad.setP3(floatPoint); break;
        case 3: compositedSpaceQuad.setP4(floatPoint); break;
        }
    }
}

static void addQuadToPath(const FloatQuad& quad, Path& path)
{
    // FIXME: Make this create rounded quad-paths, just like the axis-aligned case.
    path.moveTo(quad.p1());
    path.addLineTo(quad.p2());
    path.addLineTo(quad.p3());
    path.addLineTo(quad.p4());
    path.closeSubpath();
}

void LinkHighlight::computeQuads(Node* node, Vector<FloatQuad>& outQuads) const
{
    if (!node || !node->renderer())
        return;

    RenderObject* renderer = node->renderer();

    // For inline elements, absoluteQuads will return a line box based on the line-height
    // and font metrics, which is technically incorrect as replaced elements like images
    // should use their intristic height and expand the linebox  as needed. To get an
    // appropriately sized highlight we descend into the children and have them add their
    // boxes.
    if (renderer->isRenderInline()) {
        for (Node* child = node->firstChild(); child; child = child->nextSibling())
            computeQuads(child, outQuads);
    } else {
        renderer->absoluteQuads(outQuads);
    }

}

bool LinkHighlight::computeHighlightLayerPathAndPosition(RenderLayer* compositingLayer)
{
    if (!m_node || !m_node->renderer() || !m_currentGraphicsLayer)
        return false;

    ASSERT(compositingLayer);

    // Get quads for node in absolute coordinates.
    Vector<FloatQuad> quads;
    computeQuads(m_node.get(), quads);
    ASSERT(quads.size());

    // Adjust for offset between target graphics layer and the node's renderer.
    FloatPoint positionAdjust = IntPoint(m_currentGraphicsLayer->offsetFromRenderer());

    Path newPath;
    for (size_t quadIndex = 0; quadIndex < quads.size(); ++quadIndex) {
        FloatQuad absoluteQuad = quads[quadIndex];
        absoluteQuad.move(-positionAdjust.x(), -positionAdjust.y());

        // Transform node quads in target absolute coords to local coordinates in the compositor layer.
        FloatQuad transformedQuad;
        convertTargetSpaceQuadToCompositedLayer(absoluteQuad, m_node->renderer(), compositingLayer->renderer(), transformedQuad);

        // FIXME: for now, we'll only use rounded paths if we have a single node quad. The reason for this is that
        // we may sometimes get a chain of adjacent boxes (e.g. for text nodes) which end up looking like sausage
        // links: these should ideally be merged into a single rect before creating the path, but that's
        // another CL.
        if (quads.size() == 1 && transformedQuad.isRectilinear()) {
            FloatSize rectRoundingRadii(3, 3);
            newPath.addRoundedRect(transformedQuad.boundingBox(), rectRoundingRadii);
        } else
            addQuadToPath(transformedQuad, newPath);
    }

    FloatRect boundingRect = newPath.boundingRect();
    newPath.translate(-toFloatSize(boundingRect.location()));

    bool pathHasChanged = !(newPath == m_path);
    if (pathHasChanged) {
        m_path = newPath;
        m_contentLayer->layer()->setBounds(enclosingIntRect(boundingRect).size());
    }

    m_contentLayer->layer()->setPosition(boundingRect.location());

    return pathHasChanged;
}

void LinkHighlight::paintContents(WebCanvas* canvas, const WebRect& webClipRect, bool, WebFloatRect&)
{
    if (!m_node || !m_node->renderer())
        return;

    GraphicsContext gc(canvas);
    IntRect clipRect(IntPoint(webClipRect.x, webClipRect.y), IntSize(webClipRect.width, webClipRect.height));
    gc.clip(clipRect);
    gc.setFillColor(m_node->renderer()->style()->tapHighlightColor());
    gc.fillPath(m_path);
}

void LinkHighlight::startHighlightAnimationIfNeeded()
{
    if (m_isAnimating)
        return;

    m_isAnimating = true;
    const float startOpacity = 1;
    // FIXME: Should duration be configurable?
    const float fadeDuration = 0.1f;
    const float minPreFadeDuration = 0.1f;

    m_contentLayer->layer()->setOpacity(startOpacity);

    WebCompositorSupport* compositorSupport = Platform::current()->compositorSupport();

    OwnPtr<WebFloatAnimationCurve> curve = adoptPtr(compositorSupport->createFloatAnimationCurve());

    curve->add(WebFloatKeyframe(0, startOpacity));
    // Make sure we have displayed for at least minPreFadeDuration before starting to fade out.
    float extraDurationRequired = std::max(0.f, minPreFadeDuration - static_cast<float>(monotonicallyIncreasingTime() - m_startTime));
    if (extraDurationRequired)
        curve->add(WebFloatKeyframe(extraDurationRequired, startOpacity));
    // For layout tests we don't fade out.
    curve->add(WebFloatKeyframe(fadeDuration + extraDurationRequired, blink::layoutTestMode() ? startOpacity : 0));

    OwnPtr<WebAnimation> animation = adoptPtr(compositorSupport->createAnimation(*curve, WebAnimation::TargetPropertyOpacity));

    m_contentLayer->layer()->setDrawsContent(true);
    m_contentLayer->layer()->addAnimation(animation.leakPtr());

    invalidate();
    m_owningWebViewImpl->scheduleAnimation();
}

void LinkHighlight::clearGraphicsLayerLinkHighlightPointer()
{
    if (m_currentGraphicsLayer) {
        m_currentGraphicsLayer->removeLinkHighlight(this);
        m_currentGraphicsLayer = 0;
    }
}

void LinkHighlight::notifyAnimationStarted(double, blink::WebAnimation::TargetProperty)
{
}

void LinkHighlight::notifyAnimationFinished(double, blink::WebAnimation::TargetProperty)
{
    // Since WebViewImpl may hang on to us for a while, make sure we
    // release resources as soon as possible.
    clearGraphicsLayerLinkHighlightPointer();
    releaseResources();
}

void LinkHighlight::updateGeometry()
{
    // To avoid unnecessary updates (e.g. other entities have requested animations from our WebViewImpl),
    // only proceed if we actually requested an update.
    if (!m_geometryNeedsUpdate)
        return;

    m_geometryNeedsUpdate = false;

    RenderLayer* compositingLayer = computeEnclosingCompositingLayer();
    if (compositingLayer && computeHighlightLayerPathAndPosition(compositingLayer)) {
        // We only need to invalidate the layer if the highlight size has changed, otherwise
        // we can just re-position the layer without needing to repaint.
        m_contentLayer->layer()->invalidate();

        if (m_currentGraphicsLayer)
            m_currentGraphicsLayer->addRepaintRect(FloatRect(layer()->position().x, layer()->position().y, layer()->bounds().width, layer()->bounds().height));
    } else if (!m_node || !m_node->renderer()) {
        clearGraphicsLayerLinkHighlightPointer();
        releaseResources();
    }
}

void LinkHighlight::clearCurrentGraphicsLayer()
{
    m_currentGraphicsLayer = 0;
    m_geometryNeedsUpdate = true;
}

void LinkHighlight::invalidate()
{
    // Make sure we update geometry on the next callback from WebViewImpl::layout().
    m_geometryNeedsUpdate = true;
}

WebLayer* LinkHighlight::layer()
{
    return clipLayer();
}

} // namespace WeKit
