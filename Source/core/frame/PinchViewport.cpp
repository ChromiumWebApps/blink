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

#include "config.h"
#include "PinchViewport.h"

#include "core/frame/FrameHost.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Settings.h"
#include "core/page/Chrome.h"
#include "core/page/ChromeClient.h"
#include "core/page/Page.h"
#include "core/page/scrolling/ScrollingCoordinator.h"
#include "core/rendering/RenderView.h"
#include "core/rendering/compositing/RenderLayerCompositor.h"
#include "platform/geometry/FloatSize.h"
#include "platform/graphics/GraphicsLayer.h"
#include "platform/graphics/GraphicsLayerFactory.h"
#include "platform/scroll/Scrollbar.h"
#include "public/platform/Platform.h"
#include "public/platform/WebCompositorSupport.h"
#include "public/platform/WebLayer.h"
#include "public/platform/WebLayerTreeView.h"
#include "public/platform/WebScrollbar.h"
#include "public/platform/WebScrollbarLayer.h"

using blink::WebLayer;
using blink::WebLayerTreeView;
using blink::WebScrollbar;
using blink::WebScrollbarLayer;
using WebCore::FrameHost;
using WebCore::GraphicsLayer;
using WebCore::GraphicsLayerFactory;

namespace WebCore {

PinchViewport::PinchViewport(FrameHost& owner)
    : m_owner(owner)
{
}

PinchViewport::~PinchViewport() { }

void PinchViewport::setViewportSize(const WebCore::IntSize& newSize)
{
    if (!m_innerViewportContainerLayer || !m_innerViewportScrollLayer)
        return;

    m_innerViewportContainerLayer->setSize(newSize);
    // The innerviewport scroll layer always has the same size as its clip layer, but
    // the page scale layer lives between them, allowing for non-zero max scroll
    // offset when page scale > 1.
    m_innerViewportScrollLayer->setSize(newSize);

    // Need to re-compute sizes for the overlay scrollbars.
    setupScrollbar(WebScrollbar::Horizontal);
    setupScrollbar(WebScrollbar::Vertical);
}

// Modifies the top of the graphics layer tree to add layers needed to support
// the inner/outer viewport fixed-position model for pinch zoom. When finished,
// the tree will look like this (with * denoting added layers):
//
// *innerViewportContainerLayer (fixed pos container)
//  +- *pageScaleLayer
//  |   +- *innerViewportScrollLayer
//  |       +-- overflowControlsHostLayer (root layer)
//  |           +-- rootTransformLayer (optional)
//  |               +-- outerViewportContainerLayer (fixed pos container) [frame container layer in RenderLayerCompositor]
//  |               |   +-- outerViewportScrollLayer [frame scroll layer in RenderLayerCompositor]
//  |               |       +-- content layers ...
//  |               +-- horizontal ScrollbarLayer (non-overlay)
//  |               +-- verticalScrollbarLayer (non-overlay)
//  |               +-- scroll corner (non-overlay)
//  +- *horizontalScrollbarLayer (overlay)
//  +- *verticalScrollbarLayer (overlay)
//
void PinchViewport::attachToLayerTree(GraphicsLayer* currentLayerTreeRoot, GraphicsLayerFactory* graphicsLayerFactory)
{
    if (!currentLayerTreeRoot) {
        m_innerViewportScrollLayer->removeAllChildren();
        return;
    }

    if (currentLayerTreeRoot->parent() && currentLayerTreeRoot->parent() == m_innerViewportScrollLayer)
        return;

    if (!m_innerViewportScrollLayer) {
        ASSERT(!m_overlayScrollbarHorizontal
            && !m_overlayScrollbarVertical
            && !m_pageScaleLayer
            && !m_innerViewportContainerLayer);

        m_innerViewportContainerLayer = GraphicsLayer::create(graphicsLayerFactory, this);
        m_pageScaleLayer = GraphicsLayer::create(graphicsLayerFactory, this);
        m_innerViewportScrollLayer = GraphicsLayer::create(graphicsLayerFactory, this);
        m_overlayScrollbarHorizontal = GraphicsLayer::create(graphicsLayerFactory, this);
        m_overlayScrollbarVertical = GraphicsLayer::create(graphicsLayerFactory, this);

        WebCore::ScrollingCoordinator* coordinator = m_owner.page().scrollingCoordinator();
        ASSERT(coordinator);
        coordinator->setLayerIsContainerForFixedPositionLayers(m_innerViewportScrollLayer.get(), true);

        // No need for the inner viewport to clip, since the compositing
        // surface takes care of it -- and clipping here would interfere with
        // dynamically-sized viewports on Android.
        m_innerViewportContainerLayer->setMasksToBounds(false);

        m_innerViewportScrollLayer->platformLayer()->setScrollClipLayer(
            m_innerViewportContainerLayer->platformLayer());
        m_innerViewportScrollLayer->platformLayer()->setUserScrollable(true, true);

        m_innerViewportContainerLayer->addChild(m_pageScaleLayer.get());
        m_pageScaleLayer->addChild(m_innerViewportScrollLayer.get());
        m_innerViewportContainerLayer->addChild(m_overlayScrollbarHorizontal.get());
        m_innerViewportContainerLayer->addChild(m_overlayScrollbarVertical.get());

        // Setup the inner viewport overlay scrollbars.
        setupScrollbar(WebScrollbar::Horizontal);
        setupScrollbar(WebScrollbar::Vertical);
    }

    m_innerViewportScrollLayer->removeAllChildren();
    m_innerViewportScrollLayer->addChild(currentLayerTreeRoot);

    // We only need to disable the existing (outer viewport) scrollbars
    // if the existing ones are already overlay.
    // FIXME: If we knew in advance before the overflowControlsHostLayer goes
    // away, we would re-enable the drawing of these scrollbars.
    // FIXME: This doesn't seem to work (at least on Android). Commenting out for now until
    // I figure out how to access RenderLayerCompositor from here.
    // if (GraphicsLayer* scrollbar = m_owner->compositor()->layerForHorizontalScrollbar())
    //    scrollbar->setDrawsContent(!page->mainFrame()->view()->hasOverlayScrollbars());
    // if (GraphicsLayer* scrollbar = m_owner->compositor()->layerForVerticalScrollbar())
    //    scrollbar->setDrawsContent(!page->mainFrame()->view()->hasOverlayScrollbars());
}

void PinchViewport::setupScrollbar(WebScrollbar::Orientation orientation)
{
    bool isHorizontal = orientation == WebScrollbar::Horizontal;
    GraphicsLayer* scrollbarGraphicsLayer = isHorizontal ?
        m_overlayScrollbarHorizontal.get() : m_overlayScrollbarVertical.get();
    OwnPtr<WebScrollbarLayer>& webScrollbarLayer = isHorizontal ?
        m_webOverlayScrollbarHorizontal : m_webOverlayScrollbarVertical;

    const int overlayScrollbarThickness = m_owner.settings().pinchOverlayScrollbarThickness();

    if (!webScrollbarLayer) {
        WebCore::ScrollingCoordinator* coordinator = m_owner.page().scrollingCoordinator();
        ASSERT(coordinator);
        WebCore::ScrollbarOrientation webcoreOrientation = isHorizontal ? WebCore::HorizontalScrollbar : WebCore::VerticalScrollbar;
        webScrollbarLayer = coordinator->createSolidColorScrollbarLayer(webcoreOrientation, overlayScrollbarThickness, false);

        webScrollbarLayer->setClipLayer(m_innerViewportContainerLayer->platformLayer());
        scrollbarGraphicsLayer->setContentsToPlatformLayer(webScrollbarLayer->layer());
        scrollbarGraphicsLayer->setDrawsContent(false);
    }

    int xPosition = isHorizontal ? 0 : m_innerViewportContainerLayer->size().width() - overlayScrollbarThickness;
    int yPosition = isHorizontal ? m_innerViewportContainerLayer->size().height() - overlayScrollbarThickness : 0;
    int width = isHorizontal ? m_innerViewportContainerLayer->size().width() - overlayScrollbarThickness : overlayScrollbarThickness;
    int height = isHorizontal ? overlayScrollbarThickness : m_innerViewportContainerLayer->size().height() - overlayScrollbarThickness;

    // Use the GraphicsLayer to position the scrollbars.
    scrollbarGraphicsLayer->setPosition(WebCore::IntPoint(xPosition, yPosition));
    scrollbarGraphicsLayer->setSize(WebCore::IntSize(width, height));
    scrollbarGraphicsLayer->setContentsRect(WebCore::IntRect(0, 0, width, height));
}

void PinchViewport::registerViewportLayersWithTreeView(WebLayerTreeView* layerTreeView) const
{
    ASSERT(layerTreeView);
    ASSERT(m_owner.page().mainFrame());
    ASSERT(m_owner.page().mainFrame()->contentRenderer());

    WebCore::RenderLayerCompositor* compositor = m_owner.page().mainFrame()->contentRenderer()->compositor();
    // Get the outer viewport scroll layer.
    WebLayer* scrollLayer = compositor->scrollLayer()->platformLayer();

    m_webOverlayScrollbarHorizontal->setScrollLayer(scrollLayer);
    m_webOverlayScrollbarVertical->setScrollLayer(scrollLayer);

    ASSERT(compositor);
    layerTreeView->registerViewportLayers(
        m_pageScaleLayer->platformLayer(),
        m_innerViewportScrollLayer->platformLayer(),
        scrollLayer);
}

void PinchViewport::clearViewportLayersForTreeView(WebLayerTreeView* layerTreeView) const
{
    ASSERT(layerTreeView);

    layerTreeView->clearViewportLayers();
}

void PinchViewport::notifyAnimationStarted(const GraphicsLayer*, double monotonicTime)
{
}

void PinchViewport::paintContents(const GraphicsLayer*, WebCore::GraphicsContext&, WebCore::GraphicsLayerPaintingPhase, const WebCore::IntRect& inClip)
{
}

String PinchViewport::debugName(const GraphicsLayer* graphicsLayer)
{
    String name;
    if (graphicsLayer == m_innerViewportContainerLayer.get()) {
        name = "Inner Viewport Container Layer";
    } else if (graphicsLayer == m_pageScaleLayer.get()) {
        name =  "Page Scale Layer";
    } else if (graphicsLayer == m_innerViewportScrollLayer.get()) {
        name =  "Inner Viewport Scroll Layer";
    } else if (graphicsLayer == m_overlayScrollbarHorizontal.get()) {
        name =  "Overlay Scrollbar Horizontal Layer";
    } else if (graphicsLayer == m_overlayScrollbarVertical.get()) {
        name =  "Overlay Scrollbar Vertical Layer";
    } else {
        ASSERT_NOT_REACHED();
    }

    return name;
}

} // namespace WebCore
