/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2005 Allan Sandfeld Jensen (kde@carewolf.com)
 *           (C) 2005, 2006 Samuel Weinig (sam.weinig@gmail.com)
 * Copyright (C) 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2010, 2012 Google Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "core/rendering/RenderLayerModelObject.h"

#include "core/frame/LocalFrame.h"
#include "core/rendering/RenderLayer.h"
#include "core/rendering/RenderView.h"

using namespace std;

namespace WebCore {

bool RenderLayerModelObject::s_wasFloating = false;

RenderLayerModelObject::RenderLayerModelObject(ContainerNode* node)
    : RenderObject(node)
{
}

RenderLayerModelObject::~RenderLayerModelObject()
{
    // Our layer should have been destroyed and cleared by now
    ASSERT(!hasLayer());
    ASSERT(!m_layer);
}

void RenderLayerModelObject::destroyLayer()
{
    setHasLayer(false);
    m_layer = nullptr;
}

void RenderLayerModelObject::createLayer(LayerType type)
{
    ASSERT(!m_layer);
    m_layer = adoptPtr(new RenderLayer(this, type));
    setHasLayer(true);
    m_layer->insertOnlyThisLayer();
}

bool RenderLayerModelObject::hasSelfPaintingLayer() const
{
    return m_layer && m_layer->isSelfPaintingLayer();
}

ScrollableArea* RenderLayerModelObject::scrollableArea() const
{
    return m_layer ? m_layer->scrollableArea() : 0;
}

void RenderLayerModelObject::willBeDestroyed()
{
    if (isPositioned()) {
        // Don't use this->view() because the document's renderView has been set to 0 during destruction.
        if (LocalFrame* frame = this->frame()) {
            if (FrameView* frameView = frame->view()) {
                if (style()->hasViewportConstrainedPosition())
                    frameView->removeViewportConstrainedObject(this);
            }
        }
    }

    RenderObject::willBeDestroyed();

    destroyLayer();
}

void RenderLayerModelObject::styleWillChange(StyleDifference diff, const RenderStyle* newStyle)
{
    s_wasFloating = isFloating();

    // If our z-index changes value or our visibility changes,
    // we need to dirty our stacking context's z-order list.
    RenderStyle* oldStyle = style();
    if (oldStyle && newStyle) {
        if (parent()) {
            // Do a repaint with the old style first, e.g., for example if we go from
            // having an outline to not having an outline.
            if (diff == StyleDifferenceRepaintLayer) {
                layer()->repainter().repaintIncludingDescendants();
                if (oldStyle->clip() != newStyle->clip())
                    layer()->clipper().clearClipRectsIncludingDescendants();
            } else if (diff == StyleDifferenceRepaint || newStyle->outlineSize() < oldStyle->outlineSize())
                repaint();
        }

        if (diff == StyleDifferenceLayout || diff == StyleDifferenceSimplifiedLayout) {
            // When a layout hint happens, we go ahead and do a repaint of the layer, since the layer could
            // end up being destroyed.
            if (hasLayer()) {
                if (oldStyle->hasClip() != newStyle->hasClip()
                    || oldStyle->clip() != newStyle->clip()) {
                    // Composited layers don't need to be repainted when a parent's clip changes.
                    layer()->repainter().repaintIncludingNonCompositingDescendants(containerForRepaint());
                } else if (!layer()->hasCompositedLayerMapping()) {
                    if (oldStyle->position() != newStyle->position()
                        || oldStyle->zIndex() != newStyle->zIndex()
                        || oldStyle->hasAutoZIndex() != newStyle->hasAutoZIndex()
                        || oldStyle->opacity() != newStyle->opacity()
                        || oldStyle->transform() != newStyle->transform()
                        || oldStyle->filter() != newStyle->filter())
                    layer()->repainter().repaintIncludingDescendants();
                }
            } else if (newStyle->hasTransform() || newStyle->opacity() < 1 || newStyle->hasFilter()) {
                // If we don't have a layer yet, but we are going to get one because of transform or opacity,
                //  then we need to repaint the old position of the object.
                repaint();
            }
        }
    }

    RenderObject::styleWillChange(diff, newStyle);
}

void RenderLayerModelObject::styleDidChange(StyleDifference diff, const RenderStyle* oldStyle)
{
    bool hadTransform = hasTransform();
    bool hadLayer = hasLayer();
    bool layerWasSelfPainting = hadLayer && layer()->isSelfPaintingLayer();

    RenderObject::styleDidChange(diff, oldStyle);
    updateFromStyle();

    LayerType type = layerTypeRequired();
    if (type != NoLayer) {
        if (!layer() && layerCreationAllowedForSubtree()) {
            if (s_wasFloating && isFloating())
                setChildNeedsLayout();
            createLayer(type);
            if (parent() && !needsLayout() && containingBlock()) {
                layer()->repainter().setRepaintStatus(NeedsFullRepaint);
                // There is only one layer to update, it is not worth using |cachedOffset| since
                // we are not sure the value will be used.
                layer()->updateLayerPositions(0);
            }
        }
    } else if (layer() && layer()->parent()) {
        setHasTransform(false); // Either a transform wasn't specified or the object doesn't support transforms, so just null out the bit.
        setHasReflection(false);
        layer()->removeOnlyThisLayer(); // calls destroyLayer() which clears m_layer
        if (s_wasFloating && isFloating())
            setChildNeedsLayout();
        if (hadTransform)
            setNeedsLayoutAndPrefWidthsRecalc();
    }

    if (layer()) {
        // FIXME: Ideally we shouldn't need this setter but we can't easily infer an overflow-only layer
        // from the style.
        layer()->setLayerType(type);

        layer()->styleChanged(diff, oldStyle);
        if (hadLayer && layer()->isSelfPaintingLayer() != layerWasSelfPainting)
            setChildNeedsLayout();
    }

    if (FrameView *frameView = view()->frameView()) {
        bool newStyleIsViewportConstained = style()->hasViewportConstrainedPosition();
        bool oldStyleIsViewportConstrained = oldStyle && oldStyle->hasViewportConstrainedPosition();
        if (newStyleIsViewportConstained != oldStyleIsViewportConstrained) {
            if (newStyleIsViewportConstained && layer())
                frameView->addViewportConstrainedObject(this);
            else
                frameView->removeViewportConstrainedObject(this);
        }
    }
}

void RenderLayerModelObject::addLayerHitTestRects(LayerHitTestRects& rects, const RenderLayer* currentLayer, const LayoutPoint& layerOffset, const LayoutRect& containerRect) const
{
    if (hasLayer()) {
        if (isRenderView()) {
            // RenderView is handled with a special fast-path, but it needs to know the current layer.
            RenderObject::addLayerHitTestRects(rects, layer(), LayoutPoint(), LayoutRect());
        } else {
            // Since a RenderObject never lives outside it's container RenderLayer, we can switch
            // to marking entire layers instead. This may sometimes mark more than necessary (when
            // a layer is made of disjoint objects) but in practice is a significant performance
            // savings.
            layer()->addLayerHitTestRects(rects);
        }
    } else {
        RenderObject::addLayerHitTestRects(rects, currentLayer, layerOffset, containerRect);
    }
}

CompositedLayerMappingPtr RenderLayerModelObject::compositedLayerMapping() const
{
    return m_layer ? m_layer->compositedLayerMapping() : 0;
}

bool RenderLayerModelObject::hasCompositedLayerMapping() const
{
    return m_layer ? m_layer->hasCompositedLayerMapping() : false;
}

CompositedLayerMapping* RenderLayerModelObject::groupedMapping() const
{
    return m_layer ? m_layer->groupedMapping() : 0;
}

} // namespace WebCore

