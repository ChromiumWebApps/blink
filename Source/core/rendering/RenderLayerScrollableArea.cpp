/*
 * Copyright (C) 2006, 2007, 2008, 2009, 2010, 2011, 2012 Apple Inc. All rights reserved.
 *
 * Portions are Copyright (C) 1998 Netscape Communications Corporation.
 *
 * Other contributors:
 *   Robert O'Callahan <roc+@cs.cmu.edu>
 *   David Baron <dbaron@fas.harvard.edu>
 *   Christian Biesinger <cbiesinger@web.de>
 *   Randall Jesup <rjesup@wgate.com>
 *   Roland Mainz <roland.mainz@informatik.med.uni-giessen.de>
 *   Josh Soref <timeless@mac.com>
 *   Boris Zbarsky <bzbarsky@mit.edu>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Alternatively, the contents of this file may be used under the terms
 * of either the Mozilla Public License Version 1.1, found at
 * http://www.mozilla.org/MPL/ (the "MPL") or the GNU General Public
 * License Version 2.0, found at http://www.fsf.org/copyleft/gpl.html
 * (the "GPL"), in which case the provisions of the MPL or the GPL are
 * applicable instead of those above.  If you wish to allow use of your
 * version of this file only under the terms of one of those two
 * licenses (the MPL or the GPL) and not to allow others to use your
 * version of this file under the LGPL, indicate your decision by
 * deletingthe provisions above and replace them with the notice and
 * other provisions required by the MPL or the GPL, as the case may be.
 * If you do not delete the provisions above, a recipient may use your
 * version of this file under any of the LGPL, the MPL or the GPL.
 */

#include "config.h"
#include "core/rendering/RenderLayer.h"

#include "core/css/PseudoStyleRequest.h"
#include "core/dom/shadow/ShadowRoot.h"
#include "core/editing/FrameSelection.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/html/HTMLFrameOwnerElement.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "core/page/EventHandler.h"
#include "core/page/FocusController.h"
#include "core/page/Page.h"
#include "core/page/scrolling/ScrollingCoordinator.h"
#include "core/rendering/LayoutRectRecorder.h"
#include "core/rendering/RenderGeometryMap.h"
#include "core/rendering/RenderScrollbar.h"
#include "core/rendering/RenderScrollbarPart.h"
#include "core/rendering/RenderView.h"
#include "core/rendering/compositing/CompositedLayerMapping.h"
#include "core/rendering/compositing/RenderLayerCompositor.h"
#include "platform/PlatformGestureEvent.h"
#include "platform/PlatformMouseEvent.h"
#include "platform/graphics/GraphicsContextStateSaver.h"
#include "platform/graphics/GraphicsLayer.h"
#include "platform/scroll/ScrollAnimator.h"
#include "platform/scroll/ScrollbarTheme.h"
#include "public/platform/Platform.h"

namespace WebCore {

const int ResizerControlExpandRatioForTouch = 2;

RenderLayerScrollableArea::RenderLayerScrollableArea(RenderBox* box)
    : m_box(box)
    , m_inResizeMode(false)
    , m_scrollDimensionsDirty(true)
    , m_inOverflowRelayout(false)
    , m_needsCompositedScrolling(false)
    , m_willUseCompositedScrollingHasBeenRecorded(false)
    , m_isScrollableAreaHasBeenRecorded(false)
    , m_forceNeedsCompositedScrolling(DoNotForceCompositedScrolling)
    , m_scrollCorner(0)
    , m_resizer(0)
{
    ScrollableArea::setConstrainsScrollingToContentEdge(false);

    Node* node = m_box->node();
    if (node && node->isElementNode()) {
        // We save and restore only the scrollOffset as the other scroll values are recalculated.
        Element* element = toElement(node);
        m_scrollOffset = element->savedLayerScrollOffset();
        if (!m_scrollOffset.isZero())
            scrollAnimator()->setCurrentPosition(FloatPoint(m_scrollOffset.width(), m_scrollOffset.height()));
        element->setSavedLayerScrollOffset(IntSize());
    }

    updateResizerAreaSet();
}

RenderLayerScrollableArea::~RenderLayerScrollableArea()
{
    if (inResizeMode() && !m_box->documentBeingDestroyed()) {
        if (LocalFrame* frame = m_box->frame())
            frame->eventHandler().resizeScrollableAreaDestroyed();
    }

    if (LocalFrame* frame = m_box->frame()) {
        if (FrameView* frameView = frame->view()) {
            frameView->removeScrollableArea(this);
        }
    }

    if (m_box->frame() && m_box->frame()->page()) {
        if (ScrollingCoordinator* scrollingCoordinator = m_box->frame()->page()->scrollingCoordinator())
            scrollingCoordinator->willDestroyScrollableArea(this);
    }

    if (!m_box->documentBeingDestroyed()) {
        Node* node = m_box->node();
        if (node && node->isElementNode())
            toElement(node)->setSavedLayerScrollOffset(m_scrollOffset);
    }

    if (LocalFrame* frame = m_box->frame()) {
        if (FrameView* frameView = frame->view())
            frameView->removeResizerArea(m_box);
    }

    destroyScrollbar(HorizontalScrollbar);
    destroyScrollbar(VerticalScrollbar);

    if (m_scrollCorner)
        m_scrollCorner->destroy();
    if (m_resizer)
        m_resizer->destroy();
}

GraphicsLayer* RenderLayerScrollableArea::layerForScrolling() const
{
    return m_box->hasCompositedLayerMapping() ? m_box->compositedLayerMapping()->scrollingContentsLayer() : 0;
}

GraphicsLayer* RenderLayerScrollableArea::layerForHorizontalScrollbar() const
{
    // See crbug.com/343132.
    DisableCompositingQueryAsserts disabler;

    return m_box->hasCompositedLayerMapping() ? m_box->compositedLayerMapping()->layerForHorizontalScrollbar() : 0;
}

GraphicsLayer* RenderLayerScrollableArea::layerForVerticalScrollbar() const
{
    // See crbug.com/343132.
    DisableCompositingQueryAsserts disabler;

    return m_box->hasCompositedLayerMapping() ? m_box->compositedLayerMapping()->layerForVerticalScrollbar() : 0;
}

GraphicsLayer* RenderLayerScrollableArea::layerForScrollCorner() const
{
    // See crbug.com/343132.
    DisableCompositingQueryAsserts disabler;

    return m_box->hasCompositedLayerMapping() ? m_box->compositedLayerMapping()->layerForScrollCorner() : 0;
}

void RenderLayerScrollableArea::invalidateScrollbarRect(Scrollbar* scrollbar, const IntRect& rect)
{
    // See crbug.com/343132.
    DisableCompositingQueryAsserts disabler;

    if (scrollbar == m_vBar.get()) {
        if (GraphicsLayer* layer = layerForVerticalScrollbar()) {
            layer->setNeedsDisplayInRect(rect);
            return;
        }
    } else {
        if (GraphicsLayer* layer = layerForHorizontalScrollbar()) {
            layer->setNeedsDisplayInRect(rect);
            return;
        }
    }

    IntRect scrollRect = rect;
    // If we are not yet inserted into the tree, there is no need to repaint.
    if (!m_box->parent())
        return;

    if (scrollbar == m_vBar.get())
        scrollRect.move(verticalScrollbarStart(0, m_box->width()), m_box->borderTop());
    else
        scrollRect.move(horizontalScrollbarStart(0), m_box->height() - m_box->borderBottom() - scrollbar->height());

    if (scrollRect.isEmpty())
        return;

    LayoutRect repaintRect = scrollRect;
    m_box->flipForWritingMode(repaintRect);

    IntRect intRect = pixelSnappedIntRect(repaintRect);

    if (RuntimeEnabledFeatures::repaintAfterLayoutEnabled() && m_box->frameView()->isInPerformLayout()) {
        if (scrollbar == m_vBar.get()) {
            m_verticalBarDamage = intRect;
            m_hasVerticalBarDamage = true;
        } else {
            m_horizontalBarDamage = intRect;
            m_hasHorizontalBarDamage = true;
        }

    } else {
        m_box->repaintRectangle(intRect);
    }
}

void RenderLayerScrollableArea::invalidateScrollCornerRect(const IntRect& rect)
{
    if (GraphicsLayer* layer = layerForScrollCorner()) {
        layer->setNeedsDisplayInRect(rect);
        return;
    }

    if (m_scrollCorner)
        m_scrollCorner->repaintRectangle(rect);
    if (m_resizer)
        m_resizer->repaintRectangle(rect);
}

bool RenderLayerScrollableArea::isActive() const
{
    Page* page = m_box->frame()->page();
    return page && page->focusController().isActive();
}

bool RenderLayerScrollableArea::isScrollCornerVisible() const
{
    return !scrollCornerRect().isEmpty();
}

static int cornerStart(const RenderStyle* style, int minX, int maxX, int thickness)
{
    if (style->shouldPlaceBlockDirectionScrollbarOnLogicalLeft())
        return minX + style->borderLeftWidth();
    return maxX - thickness - style->borderRightWidth();
}

static IntRect cornerRect(const RenderStyle* style, const Scrollbar* horizontalScrollbar, const Scrollbar* verticalScrollbar, const IntRect& bounds)
{
    int horizontalThickness;
    int verticalThickness;
    if (!verticalScrollbar && !horizontalScrollbar) {
        // FIXME: This isn't right. We need to know the thickness of custom scrollbars
        // even when they don't exist in order to set the resizer square size properly.
        horizontalThickness = ScrollbarTheme::theme()->scrollbarThickness();
        verticalThickness = horizontalThickness;
    } else if (verticalScrollbar && !horizontalScrollbar) {
        horizontalThickness = verticalScrollbar->width();
        verticalThickness = horizontalThickness;
    } else if (horizontalScrollbar && !verticalScrollbar) {
        verticalThickness = horizontalScrollbar->height();
        horizontalThickness = verticalThickness;
    } else {
        horizontalThickness = verticalScrollbar->width();
        verticalThickness = horizontalScrollbar->height();
    }
    return IntRect(cornerStart(style, bounds.x(), bounds.maxX(), horizontalThickness),
        bounds.maxY() - verticalThickness - style->borderBottomWidth(),
        horizontalThickness, verticalThickness);
}


IntRect RenderLayerScrollableArea::scrollCornerRect() const
{
    // We have a scrollbar corner when a scrollbar is visible and not filling the entire length of the box.
    // This happens when:
    // (a) A resizer is present and at least one scrollbar is present
    // (b) Both scrollbars are present.
    bool hasHorizontalBar = horizontalScrollbar();
    bool hasVerticalBar = verticalScrollbar();
    bool hasResizer = m_box->style()->resize() != RESIZE_NONE;
    if ((hasHorizontalBar && hasVerticalBar) || (hasResizer && (hasHorizontalBar || hasVerticalBar)))
        return cornerRect(m_box->style(), horizontalScrollbar(), verticalScrollbar(), m_box->pixelSnappedBorderBoxRect());
    return IntRect();
}

IntRect RenderLayerScrollableArea::convertFromScrollbarToContainingView(const Scrollbar* scrollbar, const IntRect& scrollbarRect) const
{
    RenderView* view = m_box->view();
    if (!view)
        return scrollbarRect;

    IntRect rect = scrollbarRect;
    rect.move(scrollbarOffset(scrollbar));

    return view->frameView()->convertFromRenderer(m_box, rect);
}

IntRect RenderLayerScrollableArea::convertFromContainingViewToScrollbar(const Scrollbar* scrollbar, const IntRect& parentRect) const
{
    RenderView* view = m_box->view();
    if (!view)
        return parentRect;

    IntRect rect = view->frameView()->convertToRenderer(m_box, parentRect);
    rect.move(-scrollbarOffset(scrollbar));
    return rect;
}

IntPoint RenderLayerScrollableArea::convertFromScrollbarToContainingView(const Scrollbar* scrollbar, const IntPoint& scrollbarPoint) const
{
    RenderView* view = m_box->view();
    if (!view)
        return scrollbarPoint;

    IntPoint point = scrollbarPoint;
    point.move(scrollbarOffset(scrollbar));
    return view->frameView()->convertFromRenderer(m_box, point);
}

IntPoint RenderLayerScrollableArea::convertFromContainingViewToScrollbar(const Scrollbar* scrollbar, const IntPoint& parentPoint) const
{
    RenderView* view = m_box->view();
    if (!view)
        return parentPoint;

    IntPoint point = view->frameView()->convertToRenderer(m_box, parentPoint);

    point.move(-scrollbarOffset(scrollbar));
    return point;
}

int RenderLayerScrollableArea::scrollSize(ScrollbarOrientation orientation) const
{
    IntSize scrollDimensions = maximumScrollPosition() - minimumScrollPosition();
    return (orientation == HorizontalScrollbar) ? scrollDimensions.width() : scrollDimensions.height();
}

void RenderLayerScrollableArea::setScrollOffset(const IntPoint& newScrollOffset)
{
    if (!m_box->isMarquee()) {
        // Ensure that the dimensions will be computed if they need to be (for overflow:hidden blocks).
        if (m_scrollDimensionsDirty)
            computeScrollDimensions();
    }

    if (scrollOffset() == toIntSize(newScrollOffset))
        return;

    setScrollOffset(toIntSize(newScrollOffset));

    LocalFrame* frame = m_box->frame();
    ASSERT(frame);

    RefPtr<FrameView> frameView = m_box->frameView();

    InspectorInstrumentation::willScrollLayer(m_box);

    // Update the positions of our child layers (if needed as only fixed layers should be impacted by a scroll).
    // We don't update compositing layers, because we need to do a deep update from the compositing ancestor.
    if (!frameView->isInPerformLayout()) {
        // If we're in the middle of layout, we'll just update layers once layout has finished.
        layer()->updateLayerPositionsAfterOverflowScroll();
        // Update regions, scrolling may change the clip of a particular region.
        frameView->updateAnnotatedRegions();
        // FIXME: We shouldn't call updateWidgetPositions() here since it might tear down the render tree,
        // for now we just crash to avoid allowing an attacker to use after free.
        frameView->updateWidgetPositions();
        RELEASE_ASSERT(frameView->renderView());
        updateCompositingLayersAfterScroll();
    }

    RenderLayerModelObject* repaintContainer = m_box->containerForRepaint();
    // The caret rect needs to be invalidated after scrolling
    frame->selection().setCaretRectNeedsUpdate();

    FloatQuad quadForFakeMouseMoveEvent = FloatQuad(layer()->repainter().repaintRect());
    if (repaintContainer)
        quadForFakeMouseMoveEvent = repaintContainer->localToAbsoluteQuad(quadForFakeMouseMoveEvent);
    frame->eventHandler().dispatchFakeMouseMoveEventSoonInQuad(quadForFakeMouseMoveEvent);

    bool requiresRepaint = true;

    if (m_box->view()->compositor()->inCompositingMode()) {
        bool onlyScrolledCompositedLayers = scrollsOverflow()
            && !layer()->hasVisibleNonLayerContent()
            && !layer()->hasNonCompositedChild()
            && !layer()->hasBlockSelectionGapBounds()
            && !m_box->isMarquee();

        if (usesCompositedScrolling() || onlyScrolledCompositedLayers)
            requiresRepaint = false;
    }

    // Just schedule a full repaint of our object.
    if (requiresRepaint) {
        if (RuntimeEnabledFeatures::repaintAfterLayoutEnabled() && m_box->frameView()->isInPerformLayout())
            m_box->setShouldDoFullRepaintAfterLayout(true);
        else
            m_box->repaintUsingContainer(repaintContainer, pixelSnappedIntRect(layer()->repainter().repaintRect()));
    }

    // Schedule the scroll DOM event.
    if (m_box->node())
        m_box->node()->document().enqueueScrollEventForNode(m_box->node());

    if (AXObjectCache* cache = m_box->document().existingAXObjectCache())
        cache->handleScrollPositionChanged(m_box);

    InspectorInstrumentation::didScrollLayer(m_box);
}

IntPoint RenderLayerScrollableArea::scrollPosition() const
{
    return IntPoint(m_scrollOffset);
}

IntPoint RenderLayerScrollableArea::minimumScrollPosition() const
{
    return -scrollOrigin();
}

IntPoint RenderLayerScrollableArea::maximumScrollPosition() const
{
    if (!m_box->hasOverflowClip())
        return -scrollOrigin();

    return -scrollOrigin() + enclosingIntRect(m_overflowRect).size() - enclosingIntRect(m_box->clientBoxRect()).size();
}

IntRect RenderLayerScrollableArea::visibleContentRect(IncludeScrollbarsInRect scrollbarInclusion) const
{
    int verticalScrollbarWidth = 0;
    int horizontalScrollbarHeight = 0;
    if (scrollbarInclusion == IncludeScrollbars) {
        verticalScrollbarWidth = (verticalScrollbar() && !verticalScrollbar()->isOverlayScrollbar()) ? verticalScrollbar()->width() : 0;
        horizontalScrollbarHeight = (horizontalScrollbar() && !horizontalScrollbar()->isOverlayScrollbar()) ? horizontalScrollbar()->height() : 0;
    }

    return IntRect(IntPoint(scrollXOffset(), scrollYOffset()),
        IntSize(max(0, layer()->size().width() - verticalScrollbarWidth), max(0, layer()->size().height() - horizontalScrollbarHeight)));
}

int RenderLayerScrollableArea::visibleHeight() const
{
    return layer()->size().height();
}

int RenderLayerScrollableArea::visibleWidth() const
{
    return layer()->size().width();
}

IntSize RenderLayerScrollableArea::contentsSize() const
{
    return IntSize(scrollWidth(), scrollHeight());
}

IntSize RenderLayerScrollableArea::overhangAmount() const
{
    return IntSize();
}

IntPoint RenderLayerScrollableArea::lastKnownMousePosition() const
{
    return m_box->frame() ? m_box->frame()->eventHandler().lastKnownMousePosition() : IntPoint();
}

bool RenderLayerScrollableArea::shouldSuspendScrollAnimations() const
{
    RenderView* view = m_box->view();
    if (!view)
        return true;
    return view->frameView()->shouldSuspendScrollAnimations();
}

bool RenderLayerScrollableArea::scrollbarsCanBeActive() const
{
    RenderView* view = m_box->view();
    if (!view)
        return false;
    return view->frameView()->scrollbarsCanBeActive();
}

IntRect RenderLayerScrollableArea::scrollableAreaBoundingBox() const
{
    return m_box->absoluteBoundingBoxRect();
}

bool RenderLayerScrollableArea::userInputScrollable(ScrollbarOrientation orientation) const
{
    if (m_box->isIntristicallyScrollable(orientation))
        return true;

    EOverflow overflowStyle = (orientation == HorizontalScrollbar) ?
        m_box->style()->overflowX() : m_box->style()->overflowY();
    return (overflowStyle == OSCROLL || overflowStyle == OAUTO || overflowStyle == OOVERLAY);
}

bool RenderLayerScrollableArea::shouldPlaceVerticalScrollbarOnLeft() const
{
    return m_box->style()->shouldPlaceBlockDirectionScrollbarOnLogicalLeft();
}

int RenderLayerScrollableArea::pageStep(ScrollbarOrientation orientation) const
{
    int length = (orientation == HorizontalScrollbar) ?
        m_box->pixelSnappedClientWidth() : m_box->pixelSnappedClientHeight();
    int minPageStep = static_cast<float>(length) * ScrollableArea::minFractionToStepWhenPaging();
    int pageStep = max(minPageStep, length - ScrollableArea::maxOverlapBetweenPages());

    return max(pageStep, 1);
}

RenderLayer* RenderLayerScrollableArea::layer() const
{
    return m_box->layer();
}

int RenderLayerScrollableArea::scrollWidth() const
{
    if (m_scrollDimensionsDirty)
        const_cast<RenderLayerScrollableArea*>(this)->computeScrollDimensions();
    return snapSizeToPixel(m_overflowRect.width(), m_box->clientLeft() + m_box->x());
}

int RenderLayerScrollableArea::scrollHeight() const
{
    if (m_scrollDimensionsDirty)
        const_cast<RenderLayerScrollableArea*>(this)->computeScrollDimensions();
    return snapSizeToPixel(m_overflowRect.height(), m_box->clientTop() + m_box->y());
}

void RenderLayerScrollableArea::computeScrollDimensions()
{
    m_scrollDimensionsDirty = false;

    m_overflowRect = m_box->layoutOverflowRect();
    m_box->flipForWritingMode(m_overflowRect);

    int scrollableLeftOverflow = m_overflowRect.x() - m_box->borderLeft() - (m_box->style()->shouldPlaceBlockDirectionScrollbarOnLogicalLeft() ? m_box->verticalScrollbarWidth() : 0);
    int scrollableTopOverflow = m_overflowRect.y() - m_box->borderTop();
    setScrollOrigin(IntPoint(-scrollableLeftOverflow, -scrollableTopOverflow));
}

void RenderLayerScrollableArea::scrollToOffset(const IntSize& scrollOffset, ScrollOffsetClamping clamp)
{
    IntSize newScrollOffset = clamp == ScrollOffsetClamped ? clampScrollOffset(scrollOffset) : scrollOffset;
    if (newScrollOffset != adjustedScrollOffset())
        scrollToOffsetWithoutAnimation(-scrollOrigin() + newScrollOffset);
}

void RenderLayerScrollableArea::updateAfterLayout()
{
    // List box parts handle the scrollbars by themselves so we have nothing to do.
    if (m_box->style()->appearance() == ListboxPart)
        return;

    LayoutRectRecorder recorder(*m_box);

    m_scrollDimensionsDirty = true;
    IntSize originalScrollOffset = adjustedScrollOffset();

    computeScrollDimensions();

    if (!m_box->isMarquee()) {
        // Layout may cause us to be at an invalid scroll position. In this case we need
        // to pull our scroll offsets back to the max (or push them up to the min).
        IntSize clampedScrollOffset = clampScrollOffset(adjustedScrollOffset());
        if (clampedScrollOffset != adjustedScrollOffset())
            scrollToOffset(clampedScrollOffset);
    }

    if (originalScrollOffset != adjustedScrollOffset())
        scrollToOffsetWithoutAnimation(-scrollOrigin() + adjustedScrollOffset());

    bool hasHorizontalOverflow = this->hasHorizontalOverflow();
    bool hasVerticalOverflow = this->hasVerticalOverflow();

    {
        // Hits in compositing/overflow/automatically-opt-into-composited-scrolling-after-style-change.html.
        DisableCompositingQueryAsserts disabler;

        // overflow:scroll should just enable/disable.
        if (m_box->style()->overflowX() == OSCROLL)
            horizontalScrollbar()->setEnabled(hasHorizontalOverflow);
        if (m_box->style()->overflowY() == OSCROLL)
            verticalScrollbar()->setEnabled(hasVerticalOverflow);
    }

    // overflow:auto may need to lay out again if scrollbars got added/removed.
    bool autoHorizontalScrollBarChanged = m_box->hasAutoHorizontalScrollbar() && (hasHorizontalScrollbar() != hasHorizontalOverflow);
    bool autoVerticalScrollBarChanged = m_box->hasAutoVerticalScrollbar() && (hasVerticalScrollbar() != hasVerticalOverflow);

    if (autoHorizontalScrollBarChanged || autoVerticalScrollBarChanged) {
        if (m_box->hasAutoHorizontalScrollbar())
            setHasHorizontalScrollbar(hasHorizontalOverflow);
        if (m_box->hasAutoVerticalScrollbar())
            setHasVerticalScrollbar(hasVerticalOverflow);

        if (hasVerticalOverflow || hasHorizontalOverflow)
            updateScrollCornerStyle();

        layer()->updateSelfPaintingLayer();

        // Force an update since we know the scrollbars have changed things.
        if (m_box->document().hasAnnotatedRegions())
            m_box->document().setAnnotatedRegionsDirty(true);

        if (!RuntimeEnabledFeatures::repaintAfterLayoutEnabled())
            m_box->repaint();

        if (m_box->style()->overflowX() == OAUTO || m_box->style()->overflowY() == OAUTO) {
            if (!m_inOverflowRelayout) {
                // Our proprietary overflow: overlay value doesn't trigger a layout.
                m_inOverflowRelayout = true;
                SubtreeLayoutScope layoutScope(m_box);
                layoutScope.setNeedsLayout(m_box);
                if (m_box->isRenderBlock()) {
                    RenderBlock* block = toRenderBlock(m_box);
                    block->scrollbarsChanged(autoHorizontalScrollBarChanged, autoVerticalScrollBarChanged);
                    block->layoutBlock(true);
                } else {
                    m_box->layout();
                }
                m_inOverflowRelayout = false;
            }
        }
    }

    {
        // Hits in compositing/overflow/automatically-opt-into-composited-scrolling-after-style-change.html.
        DisableCompositingQueryAsserts disabler;

        // Set up the range (and page step/line step).
        if (Scrollbar* horizontalScrollbar = this->horizontalScrollbar()) {
            int clientWidth = m_box->pixelSnappedClientWidth();
            horizontalScrollbar->setProportion(clientWidth, overflowRect().width());
        }
        if (Scrollbar* verticalScrollbar = this->verticalScrollbar()) {
            int clientHeight = m_box->pixelSnappedClientHeight();
            verticalScrollbar->setProportion(clientHeight, overflowRect().height());
        }
    }

    updateScrollableAreaSet(hasScrollableHorizontalOverflow() || hasScrollableVerticalOverflow());

    {
        // FIXME: We should not be allowing repaint during layout. crbug.com/336251
        AllowRepaintScope scoper(m_box->view()->frameView());

        // FIXME: Remove incremental compositing updates after fixing the chicken/egg issues
        // https://code.google.com/p/chromium/issues/detail?id=343756
        DisableCompositingQueryAsserts disabler;
        m_box->view()->compositor()->updateLayerCompositingState(m_box->layer());
    }
}

bool RenderLayerScrollableArea::hasHorizontalOverflow() const
{
    ASSERT(!m_scrollDimensionsDirty);

    return scrollWidth() > m_box->pixelSnappedClientWidth();
}

bool RenderLayerScrollableArea::hasVerticalOverflow() const
{
    ASSERT(!m_scrollDimensionsDirty);

    return scrollHeight() > m_box->pixelSnappedClientHeight();
}

bool RenderLayerScrollableArea::hasScrollableHorizontalOverflow() const
{
    return hasHorizontalOverflow() && m_box->scrollsOverflowX();
}

bool RenderLayerScrollableArea::hasScrollableVerticalOverflow() const
{
    return hasVerticalOverflow() && m_box->scrollsOverflowY();
}

static bool overflowRequiresScrollbar(EOverflow overflow)
{
    return overflow == OSCROLL;
}

static bool overflowDefinesAutomaticScrollbar(EOverflow overflow)
{
    return overflow == OAUTO || overflow == OOVERLAY;
}

void RenderLayerScrollableArea::updateAfterStyleChange(const RenderStyle* oldStyle)
{
    // List box parts handle the scrollbars by themselves so we have nothing to do.
    if (m_box->style()->appearance() == ListboxPart)
        return;

    // RenderView shouldn't provide scrollbars on its own.
    if (m_box->isRenderView())
        return;

    if (!m_scrollDimensionsDirty)
        updateScrollableAreaSet(hasScrollableHorizontalOverflow() || hasScrollableVerticalOverflow());

    EOverflow overflowX = m_box->style()->overflowX();
    EOverflow overflowY = m_box->style()->overflowY();

    // To avoid doing a relayout in updateScrollbarsAfterLayout, we try to keep any automatic scrollbar that was already present.
    bool needsHorizontalScrollbar = (hasHorizontalScrollbar() && overflowDefinesAutomaticScrollbar(overflowX)) || overflowRequiresScrollbar(overflowX);
    bool needsVerticalScrollbar = (hasVerticalScrollbar() && overflowDefinesAutomaticScrollbar(overflowY)) || overflowRequiresScrollbar(overflowY);
    setHasHorizontalScrollbar(needsHorizontalScrollbar);
    setHasVerticalScrollbar(needsVerticalScrollbar);

    // With overflow: scroll, scrollbars are always visible but may be disabled.
    // When switching to another value, we need to re-enable them (see bug 11985).
    if (needsHorizontalScrollbar && oldStyle && oldStyle->overflowX() == OSCROLL && overflowX != OSCROLL) {
        ASSERT(hasHorizontalScrollbar());
        m_hBar->setEnabled(true);
    }

    if (needsVerticalScrollbar && oldStyle && oldStyle->overflowY() == OSCROLL && overflowY != OSCROLL) {
        ASSERT(hasVerticalScrollbar());
        m_vBar->setEnabled(true);
    }

    // FIXME: Need to detect a swap from custom to native scrollbars (and vice versa).
    if (m_hBar)
        m_hBar->styleChanged();
    if (m_vBar)
        m_vBar->styleChanged();

    updateScrollCornerStyle();
    updateResizerAreaSet();
    updateResizerStyle();
}

IntSize RenderLayerScrollableArea::clampScrollOffset(const IntSize& scrollOffset) const
{
    int maxX = scrollWidth() - m_box->pixelSnappedClientWidth();
    int maxY = scrollHeight() - m_box->pixelSnappedClientHeight();

    int x = std::max(std::min(scrollOffset.width(), maxX), 0);
    int y = std::max(std::min(scrollOffset.height(), maxY), 0);
    return IntSize(x, y);
}

IntRect RenderLayerScrollableArea::rectForHorizontalScrollbar(const IntRect& borderBoxRect) const
{
    if (!m_hBar)
        return IntRect();

    const IntRect& scrollCorner = scrollCornerRect();

    return IntRect(horizontalScrollbarStart(borderBoxRect.x()),
        borderBoxRect.maxY() - m_box->borderBottom() - m_hBar->height(),
        borderBoxRect.width() - (m_box->borderLeft() + m_box->borderRight()) - scrollCorner.width(),
        m_hBar->height());
}

IntRect RenderLayerScrollableArea::rectForVerticalScrollbar(const IntRect& borderBoxRect) const
{
    if (!m_vBar)
        return IntRect();

    const IntRect& scrollCorner = scrollCornerRect();

    return IntRect(verticalScrollbarStart(borderBoxRect.x(), borderBoxRect.maxX()),
        borderBoxRect.y() + m_box->borderTop(),
        m_vBar->width(),
        borderBoxRect.height() - (m_box->borderTop() + m_box->borderBottom()) - scrollCorner.height());
}

LayoutUnit RenderLayerScrollableArea::verticalScrollbarStart(int minX, int maxX) const
{
    if (m_box->style()->shouldPlaceBlockDirectionScrollbarOnLogicalLeft())
        return minX + m_box->borderLeft();
    return maxX - m_box->borderRight() - m_vBar->width();
}

LayoutUnit RenderLayerScrollableArea::horizontalScrollbarStart(int minX) const
{
    int x = minX + m_box->borderLeft();
    if (m_box->style()->shouldPlaceBlockDirectionScrollbarOnLogicalLeft())
        x += m_vBar ? m_vBar->width() : resizerCornerRect(m_box->pixelSnappedBorderBoxRect(), ResizerForPointer).width();
    return x;
}

IntSize RenderLayerScrollableArea::scrollbarOffset(const Scrollbar* scrollbar) const
{
    if (scrollbar == m_vBar.get())
        return IntSize(verticalScrollbarStart(0, m_box->width()), m_box->borderTop());

    if (scrollbar == m_hBar.get())
        return IntSize(horizontalScrollbarStart(0), m_box->height() - m_box->borderBottom() - scrollbar->height());

    ASSERT_NOT_REACHED();
    return IntSize();
}

static inline RenderObject* rendererForScrollbar(RenderObject* renderer)
{
    if (Node* node = renderer->node()) {
        if (ShadowRoot* shadowRoot = node->containingShadowRoot()) {
            if (shadowRoot->type() == ShadowRoot::UserAgentShadowRoot)
                return shadowRoot->host()->renderer();
        }
    }

    return renderer;
}

PassRefPtr<Scrollbar> RenderLayerScrollableArea::createScrollbar(ScrollbarOrientation orientation)
{
    RefPtr<Scrollbar> widget;
    RenderObject* actualRenderer = rendererForScrollbar(m_box);
    bool hasCustomScrollbarStyle = actualRenderer->isBox() && actualRenderer->style()->hasPseudoStyle(SCROLLBAR);
    if (hasCustomScrollbarStyle) {
        widget = RenderScrollbar::createCustomScrollbar(this, orientation, actualRenderer->node());
    } else {
        widget = Scrollbar::create(this, orientation, RegularScrollbar);
        if (orientation == HorizontalScrollbar)
            didAddScrollbar(widget.get(), HorizontalScrollbar);
        else
            didAddScrollbar(widget.get(), VerticalScrollbar);
    }
    m_box->document().view()->addChild(widget.get());
    return widget.release();
}

void RenderLayerScrollableArea::destroyScrollbar(ScrollbarOrientation orientation)
{
    RefPtr<Scrollbar>& scrollbar = orientation == HorizontalScrollbar ? m_hBar : m_vBar;
    if (!scrollbar)
        return;

    if (!scrollbar->isCustomScrollbar())
        willRemoveScrollbar(scrollbar.get(), orientation);

    scrollbar->removeFromParent();
    scrollbar->disconnectFromScrollableArea();
    scrollbar = nullptr;
}

void RenderLayerScrollableArea::setHasHorizontalScrollbar(bool hasScrollbar)
{
    if (hasScrollbar == hasHorizontalScrollbar())
        return;

    if (hasScrollbar) {
        // This doesn't hit in any tests, but since the equivalent code in setHasVerticalScrollbar
        // does, presumably this code does as well.
        DisableCompositingQueryAsserts disabler;
        m_hBar = createScrollbar(HorizontalScrollbar);
    } else {
        destroyScrollbar(HorizontalScrollbar);
    }

    // Destroying or creating one bar can cause our scrollbar corner to come and go. We need to update the opposite scrollbar's style.
    if (m_hBar)
        m_hBar->styleChanged();
    if (m_vBar)
        m_vBar->styleChanged();

    // Force an update since we know the scrollbars have changed things.
    if (m_box->document().hasAnnotatedRegions())
        m_box->document().setAnnotatedRegionsDirty(true);
}

void RenderLayerScrollableArea::setHasVerticalScrollbar(bool hasScrollbar)
{
    if (hasScrollbar == hasVerticalScrollbar())
        return;

    if (hasScrollbar) {
        // Hits in compositing/overflow/automatically-opt-into-composited-scrolling-after-style-change.html
        DisableCompositingQueryAsserts disabler;
        m_vBar = createScrollbar(VerticalScrollbar);
    } else {
        destroyScrollbar(VerticalScrollbar);
    }

    // Destroying or creating one bar can cause our scrollbar corner to come and go. We need to update the opposite scrollbar's style.
    if (m_hBar)
        m_hBar->styleChanged();
    if (m_vBar)
        m_vBar->styleChanged();

    // Force an update since we know the scrollbars have changed things.
    if (m_box->document().hasAnnotatedRegions())
        m_box->document().setAnnotatedRegionsDirty(true);
}

int RenderLayerScrollableArea::verticalScrollbarWidth(OverlayScrollbarSizeRelevancy relevancy) const
{
    if (!m_vBar || (m_vBar->isOverlayScrollbar() && (relevancy == IgnoreOverlayScrollbarSize || !m_vBar->shouldParticipateInHitTesting())))
        return 0;
    return m_vBar->width();
}

int RenderLayerScrollableArea::horizontalScrollbarHeight(OverlayScrollbarSizeRelevancy relevancy) const
{
    if (!m_hBar || (m_hBar->isOverlayScrollbar() && (relevancy == IgnoreOverlayScrollbarSize || !m_hBar->shouldParticipateInHitTesting())))
        return 0;
    return m_hBar->height();
}

void RenderLayerScrollableArea::positionOverflowControls()
{
    RenderGeometryMap geometryMap(UseTransforms);
    RenderView* view = m_box->view();
    if (m_box->layer() != view->layer() && m_box->layer()->parent())
        geometryMap.pushMappingsToAncestor(m_box->layer()->parent(), 0);

    LayoutPoint offsetFromRoot = LayoutPoint(geometryMap.absolutePoint(FloatPoint()));
    positionOverflowControls(toIntSize(roundedIntPoint(offsetFromRoot)));
}

void RenderLayerScrollableArea::positionOverflowControls(const IntSize& offsetFromRoot)
{
    if (!hasScrollbar() && !m_box->canResize())
        return;

    const IntRect borderBox = m_box->pixelSnappedBorderBoxRect();
    if (Scrollbar* verticalScrollbar = this->verticalScrollbar()) {
        IntRect vBarRect = rectForVerticalScrollbar(borderBox);
        vBarRect.move(offsetFromRoot);
        verticalScrollbar->setFrameRect(vBarRect);
    }

    if (Scrollbar* horizontalScrollbar = this->horizontalScrollbar()) {
        IntRect hBarRect = rectForHorizontalScrollbar(borderBox);
        hBarRect.move(offsetFromRoot);
        horizontalScrollbar->setFrameRect(hBarRect);
    }

    const IntRect& scrollCorner = scrollCornerRect();
    if (m_scrollCorner)
        m_scrollCorner->setFrameRect(scrollCorner);

    if (m_resizer)
        m_resizer->setFrameRect(resizerCornerRect(borderBox, ResizerForPointer));

    // FIXME, this should eventually be removed, once we are certain that composited
    // controls get correctly positioned on a compositor update. For now, conservatively
    // leaving this unchanged.
    if (m_box->hasCompositedLayerMapping())
        m_box->compositedLayerMapping()->positionOverflowControlsLayers(offsetFromRoot);
}

bool RenderLayerScrollableArea::scrollsOverflow() const
{
    if (FrameView* frameView = m_box->view()->frameView())
        return frameView->containsScrollableArea(this);

    return false;
}

void RenderLayerScrollableArea::updateScrollCornerStyle()
{
    if (!m_scrollCorner && !hasScrollbar())
        return;
    if (!m_scrollCorner && hasOverlayScrollbars())
        return;

    RenderObject* actualRenderer = rendererForScrollbar(m_box);
    RefPtr<RenderStyle> corner = m_box->hasOverflowClip() ? actualRenderer->getUncachedPseudoStyle(PseudoStyleRequest(SCROLLBAR_CORNER), actualRenderer->style()) : PassRefPtr<RenderStyle>(nullptr);
    if (corner) {
        if (!m_scrollCorner) {
            m_scrollCorner = RenderScrollbarPart::createAnonymous(&m_box->document());
            m_scrollCorner->setParent(m_box);
        }
        m_scrollCorner->setStyle(corner.release());
    } else if (m_scrollCorner) {
        m_scrollCorner->destroy();
        m_scrollCorner = 0;
    }
}

void RenderLayerScrollableArea::paintOverflowControls(GraphicsContext* context, const IntPoint& paintOffset, const IntRect& damageRect, bool paintingOverlayControls)
{
    // Don't do anything if we have no overflow.
    if (!m_box->hasOverflowClip())
        return;

    IntPoint adjustedPaintOffset = paintOffset;
    if (paintingOverlayControls)
        adjustedPaintOffset = m_cachedOverlayScrollbarOffset;

    // Move the scrollbar widgets if necessary. We normally move and resize widgets during layout,
    // but sometimes widgets can move without layout occurring (most notably when you scroll a
    // document that contains fixed positioned elements).
    positionOverflowControls(toIntSize(adjustedPaintOffset));

    // Overlay scrollbars paint in a second pass through the layer tree so that they will paint
    // on top of everything else. If this is the normal painting pass, paintingOverlayControls
    // will be false, and we should just tell the root layer that there are overlay scrollbars
    // that need to be painted. That will cause the second pass through the layer tree to run,
    // and we'll paint the scrollbars then. In the meantime, cache tx and ty so that the
    // second pass doesn't need to re-enter the RenderTree to get it right.
    if (hasOverlayScrollbars() && !paintingOverlayControls) {
        m_cachedOverlayScrollbarOffset = paintOffset;
        // It's not necessary to do the second pass if the scrollbars paint into layers.
        if ((m_hBar && layerForHorizontalScrollbar()) || (m_vBar && layerForVerticalScrollbar()))
            return;
        IntRect localDamgeRect = damageRect;
        localDamgeRect.moveBy(-paintOffset);
        if (!overflowControlsIntersectRect(localDamgeRect))
            return;

        RenderView* renderView = m_box->view();

        RenderLayer* paintingRoot = layer()->enclosingCompositingLayer();
        if (!paintingRoot)
            paintingRoot = renderView->layer();

        paintingRoot->setContainsDirtyOverlayScrollbars(true);
        return;
    }

    // This check is required to avoid painting custom CSS scrollbars twice.
    if (paintingOverlayControls && !hasOverlayScrollbars())
        return;

    // Now that we're sure the scrollbars are in the right place, paint them.
    if (m_hBar && !layerForHorizontalScrollbar())
        m_hBar->paint(context, damageRect);
    if (m_vBar && !layerForVerticalScrollbar())
        m_vBar->paint(context, damageRect);

    if (layerForScrollCorner())
        return;

    // We fill our scroll corner with white if we have a scrollbar that doesn't run all the way up to the
    // edge of the box.
    paintScrollCorner(context, adjustedPaintOffset, damageRect);

    // Paint our resizer last, since it sits on top of the scroll corner.
    paintResizer(context, adjustedPaintOffset, damageRect);
}

void RenderLayerScrollableArea::paintScrollCorner(GraphicsContext* context, const IntPoint& paintOffset, const IntRect& damageRect)
{
    IntRect absRect = scrollCornerRect();
    absRect.moveBy(paintOffset);
    if (!absRect.intersects(damageRect))
        return;

    if (context->updatingControlTints()) {
        updateScrollCornerStyle();
        return;
    }

    if (m_scrollCorner) {
        m_scrollCorner->paintIntoRect(context, paintOffset, absRect);
        return;
    }

    // We don't want to paint white if we have overlay scrollbars, since we need
    // to see what is behind it.
    if (!hasOverlayScrollbars())
        context->fillRect(absRect, Color::white);
}

bool RenderLayerScrollableArea::hitTestOverflowControls(HitTestResult& result, const IntPoint& localPoint)
{
    if (!hasScrollbar() && !m_box->canResize())
        return false;

    IntRect resizeControlRect;
    if (m_box->style()->resize() != RESIZE_NONE) {
        resizeControlRect = resizerCornerRect(m_box->pixelSnappedBorderBoxRect(), ResizerForPointer);
        if (resizeControlRect.contains(localPoint))
            return true;
    }

    int resizeControlSize = max(resizeControlRect.height(), 0);
    if (m_vBar && m_vBar->shouldParticipateInHitTesting()) {
        LayoutRect vBarRect(verticalScrollbarStart(0, m_box->width()),
            m_box->borderTop(),
            m_vBar->width(),
            m_box->height() - (m_box->borderTop() + m_box->borderBottom()) - (m_hBar ? m_hBar->height() : resizeControlSize));
        if (vBarRect.contains(localPoint)) {
            result.setScrollbar(m_vBar.get());
            return true;
        }
    }

    resizeControlSize = max(resizeControlRect.width(), 0);
    if (m_hBar && m_hBar->shouldParticipateInHitTesting()) {
        LayoutRect hBarRect(horizontalScrollbarStart(0),
            m_box->height() - m_box->borderBottom() - m_hBar->height(),
            m_box->width() - (m_box->borderLeft() + m_box->borderRight()) - (m_vBar ? m_vBar->width() : resizeControlSize),
            m_hBar->height());
        if (hBarRect.contains(localPoint)) {
            result.setScrollbar(m_hBar.get());
            return true;
        }
    }

    // FIXME: We should hit test the m_scrollCorner and pass it back through the result.

    return false;
}

IntRect RenderLayerScrollableArea::resizerCornerRect(const IntRect& bounds, ResizerHitTestType resizerHitTestType) const
{
    if (m_box->style()->resize() == RESIZE_NONE)
        return IntRect();
    IntRect corner = cornerRect(m_box->style(), horizontalScrollbar(), verticalScrollbar(), bounds);

    if (resizerHitTestType == ResizerForTouch) {
        // We make the resizer virtually larger for touch hit testing. With the
        // expanding ratio k = ResizerControlExpandRatioForTouch, we first move
        // the resizer rect (of width w & height h), by (-w * (k-1), -h * (k-1)),
        // then expand the rect by new_w/h = w/h * k.
        int expandRatio = ResizerControlExpandRatioForTouch - 1;
        corner.move(-corner.width() * expandRatio, -corner.height() * expandRatio);
        corner.expand(corner.width() * expandRatio, corner.height() * expandRatio);
    }

    return corner;
}

IntRect RenderLayerScrollableArea::scrollCornerAndResizerRect() const
{
    IntRect scrollCornerAndResizer = scrollCornerRect();
    if (scrollCornerAndResizer.isEmpty())
        scrollCornerAndResizer = resizerCornerRect(m_box->pixelSnappedBorderBoxRect(), ResizerForPointer);
    return scrollCornerAndResizer;
}

bool RenderLayerScrollableArea::overflowControlsIntersectRect(const IntRect& localRect) const
{
    const IntRect borderBox = m_box->pixelSnappedBorderBoxRect();

    if (rectForHorizontalScrollbar(borderBox).intersects(localRect))
        return true;

    if (rectForVerticalScrollbar(borderBox).intersects(localRect))
        return true;

    if (scrollCornerRect().intersects(localRect))
        return true;

    if (resizerCornerRect(borderBox, ResizerForPointer).intersects(localRect))
        return true;

    return false;
}

void RenderLayerScrollableArea::paintResizer(GraphicsContext* context, const IntPoint& paintOffset, const IntRect& damageRect)
{
    if (m_box->style()->resize() == RESIZE_NONE)
        return;

    IntRect absRect = resizerCornerRect(m_box->pixelSnappedBorderBoxRect(), ResizerForPointer);
    absRect.moveBy(paintOffset);
    if (!absRect.intersects(damageRect))
        return;

    if (context->updatingControlTints()) {
        updateResizerStyle();
        return;
    }

    if (m_resizer) {
        m_resizer->paintIntoRect(context, paintOffset, absRect);
        return;
    }

    drawPlatformResizerImage(context, absRect);

    // Draw a frame around the resizer (1px grey line) if there are any scrollbars present.
    // Clipping will exclude the right and bottom edges of this frame.
    if (!hasOverlayScrollbars() && hasScrollbar()) {
        GraphicsContextStateSaver stateSaver(*context);
        context->clip(absRect);
        IntRect largerCorner = absRect;
        largerCorner.setSize(IntSize(largerCorner.width() + 1, largerCorner.height() + 1));
        context->setStrokeColor(Color(217, 217, 217));
        context->setStrokeThickness(1.0f);
        context->setFillColor(Color::transparent);
        context->drawRect(largerCorner);
    }
}

bool RenderLayerScrollableArea::isPointInResizeControl(const IntPoint& absolutePoint, ResizerHitTestType resizerHitTestType) const
{
    if (!m_box->canResize())
        return false;

    IntPoint localPoint = roundedIntPoint(m_box->absoluteToLocal(absolutePoint, UseTransforms));
    IntRect localBounds(0, 0, m_box->pixelSnappedWidth(), m_box->pixelSnappedHeight());
    return resizerCornerRect(localBounds, resizerHitTestType).contains(localPoint);
}

bool RenderLayerScrollableArea::hitTestResizerInFragments(const LayerFragments& layerFragments, const HitTestLocation& hitTestLocation) const
{
    if (!m_box->canResize())
        return false;

    if (layerFragments.isEmpty())
        return false;

    for (int i = layerFragments.size() - 1; i >= 0; --i) {
        const LayerFragment& fragment = layerFragments.at(i);
        if (fragment.backgroundRect.intersects(hitTestLocation) && resizerCornerRect(pixelSnappedIntRect(fragment.layerBounds), ResizerForPointer).contains(hitTestLocation.roundedPoint()))
            return true;
    }

    return false;
}

void RenderLayerScrollableArea::updateResizerAreaSet()
{
    LocalFrame* frame = m_box->frame();
    if (!frame)
        return;
    FrameView* frameView = frame->view();
    if (!frameView)
        return;
    if (m_box->canResize())
        frameView->addResizerArea(m_box);
    else
        frameView->removeResizerArea(m_box);
}

void RenderLayerScrollableArea::updateResizerStyle()
{
    if (!m_resizer && !m_box->canResize())
        return;

    RenderObject* actualRenderer = rendererForScrollbar(m_box);
    RefPtr<RenderStyle> resizer = m_box->hasOverflowClip() ? actualRenderer->getUncachedPseudoStyle(PseudoStyleRequest(RESIZER), actualRenderer->style()) : PassRefPtr<RenderStyle>(nullptr);
    if (resizer) {
        if (!m_resizer) {
            m_resizer = RenderScrollbarPart::createAnonymous(&m_box->document());
            m_resizer->setParent(m_box);
        }
        m_resizer->setStyle(resizer.release());
    } else if (m_resizer) {
        m_resizer->destroy();
        m_resizer = 0;
    }
}

void RenderLayerScrollableArea::drawPlatformResizerImage(GraphicsContext* context, IntRect resizerCornerRect)
{
    float deviceScaleFactor = WebCore::deviceScaleFactor(m_box->frame());

    RefPtr<Image> resizeCornerImage;
    IntSize cornerResizerSize;
    if (deviceScaleFactor >= 2) {
        DEFINE_STATIC_REF(Image, resizeCornerImageHiRes, (Image::loadPlatformResource("textAreaResizeCorner@2x")));
        resizeCornerImage = resizeCornerImageHiRes;
        cornerResizerSize = resizeCornerImage->size();
        cornerResizerSize.scale(0.5f);
    } else {
        DEFINE_STATIC_REF(Image, resizeCornerImageLoRes, (Image::loadPlatformResource("textAreaResizeCorner")));
        resizeCornerImage = resizeCornerImageLoRes;
        cornerResizerSize = resizeCornerImage->size();
    }

    if (m_box->style()->shouldPlaceBlockDirectionScrollbarOnLogicalLeft()) {
        context->save();
        context->translate(resizerCornerRect.x() + cornerResizerSize.width(), resizerCornerRect.y() + resizerCornerRect.height() - cornerResizerSize.height());
        context->scale(FloatSize(-1.0, 1.0));
        context->drawImage(resizeCornerImage.get(), IntRect(IntPoint(), cornerResizerSize));
        context->restore();
        return;
    }
    IntRect imageRect(resizerCornerRect.maxXMaxYCorner() - cornerResizerSize, cornerResizerSize);
    context->drawImage(resizeCornerImage.get(), imageRect);
}

IntSize RenderLayerScrollableArea::offsetFromResizeCorner(const IntPoint& absolutePoint) const
{
    // Currently the resize corner is either the bottom right corner or the bottom left corner.
    // FIXME: This assumes the location is 0, 0. Is this guaranteed to always be the case?
    IntSize elementSize = layer()->size();
    if (m_box->style()->shouldPlaceBlockDirectionScrollbarOnLogicalLeft())
        elementSize.setWidth(0);
    IntPoint resizerPoint = IntPoint(elementSize);
    IntPoint localPoint = roundedIntPoint(m_box->absoluteToLocal(absolutePoint, UseTransforms));
    return localPoint - resizerPoint;
}

void RenderLayerScrollableArea::resize(const PlatformEvent& evt, const LayoutSize& oldOffset)
{
    // FIXME: This should be possible on generated content but is not right now.
    if (!inResizeMode() || !m_box->canResize() || !m_box->node())
        return;

    ASSERT(m_box->node()->isElementNode());
    Element* element = toElement(m_box->node());

    Document& document = element->document();

    IntPoint pos;
    const PlatformGestureEvent* gevt = 0;

    switch (evt.type()) {
    case PlatformEvent::MouseMoved:
        if (!document.frame()->eventHandler().mousePressed())
            return;
        pos = static_cast<const PlatformMouseEvent*>(&evt)->position();
        break;
    case PlatformEvent::GestureScrollUpdate:
    case PlatformEvent::GestureScrollUpdateWithoutPropagation:
        pos = static_cast<const PlatformGestureEvent*>(&evt)->position();
        gevt = static_cast<const PlatformGestureEvent*>(&evt);
        pos = gevt->position();
        pos.move(gevt->deltaX(), gevt->deltaY());
        break;
    default:
        ASSERT_NOT_REACHED();
    }

    float zoomFactor = m_box->style()->effectiveZoom();

    LayoutSize newOffset = offsetFromResizeCorner(document.view()->windowToContents(pos));
    newOffset.setWidth(newOffset.width() / zoomFactor);
    newOffset.setHeight(newOffset.height() / zoomFactor);

    LayoutSize currentSize = LayoutSize(m_box->width() / zoomFactor, m_box->height() / zoomFactor);
    LayoutSize minimumSize = element->minimumSizeForResizing().shrunkTo(currentSize);
    element->setMinimumSizeForResizing(minimumSize);

    LayoutSize adjustedOldOffset = LayoutSize(oldOffset.width() / zoomFactor, oldOffset.height() / zoomFactor);
    if (m_box->style()->shouldPlaceBlockDirectionScrollbarOnLogicalLeft()) {
        newOffset.setWidth(-newOffset.width());
        adjustedOldOffset.setWidth(-adjustedOldOffset.width());
    }

    LayoutSize difference = (currentSize + newOffset - adjustedOldOffset).expandedTo(minimumSize) - currentSize;

    bool isBoxSizingBorder = m_box->style()->boxSizing() == BORDER_BOX;

    EResize resize = m_box->style()->resize();
    if (resize != RESIZE_VERTICAL && difference.width()) {
        if (element->isFormControlElement()) {
            // Make implicit margins from the theme explicit (see <http://bugs.webkit.org/show_bug.cgi?id=9547>).
            element->setInlineStyleProperty(CSSPropertyMarginLeft, m_box->marginLeft() / zoomFactor, CSSPrimitiveValue::CSS_PX);
            element->setInlineStyleProperty(CSSPropertyMarginRight, m_box->marginRight() / zoomFactor, CSSPrimitiveValue::CSS_PX);
        }
        LayoutUnit baseWidth = m_box->width() - (isBoxSizingBorder ? LayoutUnit() : m_box->borderAndPaddingWidth());
        baseWidth = baseWidth / zoomFactor;
        element->setInlineStyleProperty(CSSPropertyWidth, roundToInt(baseWidth + difference.width()), CSSPrimitiveValue::CSS_PX);
    }

    if (resize != RESIZE_HORIZONTAL && difference.height()) {
        if (element->isFormControlElement()) {
            // Make implicit margins from the theme explicit (see <http://bugs.webkit.org/show_bug.cgi?id=9547>).
            element->setInlineStyleProperty(CSSPropertyMarginTop, m_box->marginTop() / zoomFactor, CSSPrimitiveValue::CSS_PX);
            element->setInlineStyleProperty(CSSPropertyMarginBottom, m_box->marginBottom() / zoomFactor, CSSPrimitiveValue::CSS_PX);
        }
        LayoutUnit baseHeight = m_box->height() - (isBoxSizingBorder ? LayoutUnit() : m_box->borderAndPaddingHeight());
        baseHeight = baseHeight / zoomFactor;
        element->setInlineStyleProperty(CSSPropertyHeight, roundToInt(baseHeight + difference.height()), CSSPrimitiveValue::CSS_PX);
    }

    document.updateLayout();

    // FIXME (Radar 4118564): We should also autoscroll the window as necessary to keep the point under the cursor in view.
}

LayoutRect RenderLayerScrollableArea::exposeRect(const LayoutRect& rect, const ScrollAlignment& alignX, const ScrollAlignment& alignY)
{
    LayoutRect localExposeRect(m_box->absoluteToLocalQuad(FloatQuad(FloatRect(rect)), UseTransforms).boundingBox());
    LayoutRect layerBounds(0, 0, m_box->clientWidth(), m_box->clientHeight());
    LayoutRect r = ScrollAlignment::getRectToExpose(layerBounds, localExposeRect, alignX, alignY);

    IntSize clampedScrollOffset = clampScrollOffset(adjustedScrollOffset() + toIntSize(roundedIntRect(r).location()));
    if (clampedScrollOffset == adjustedScrollOffset())
        return rect;

    IntSize oldScrollOffset = adjustedScrollOffset();
    scrollToOffset(clampedScrollOffset);
    IntSize scrollOffsetDifference = adjustedScrollOffset() - oldScrollOffset;
    localExposeRect.move(-scrollOffsetDifference);
    return LayoutRect(m_box->localToAbsoluteQuad(FloatQuad(FloatRect(localExposeRect)), UseTransforms).boundingBox());
}

void RenderLayerScrollableArea::updateScrollableAreaSet(bool hasOverflow)
{
    LocalFrame* frame = m_box->frame();
    if (!frame)
        return;

    FrameView* frameView = frame->view();
    if (!frameView)
        return;

    bool isVisibleToHitTest = m_box->visibleToHitTesting();
    if (HTMLFrameOwnerElement* owner = frame->ownerElement())
        isVisibleToHitTest &= owner->renderer() && owner->renderer()->visibleToHitTesting();

    bool requiresScrollableArea = hasOverflow && isVisibleToHitTest;
    bool updatedScrollableAreaSet = false;
    if (requiresScrollableArea) {
        if (frameView->addScrollableArea(this))
            updatedScrollableAreaSet = true;
    } else {
        if (frameView->removeScrollableArea(this))
            updatedScrollableAreaSet = true;
    }

    if (updatedScrollableAreaSet) {
        // Count the total number of RenderLayers that are scrollable areas for
        // any period. We only want to record this at most once per RenderLayer.
        if (requiresScrollableArea && !m_isScrollableAreaHasBeenRecorded) {
            blink::Platform::current()->histogramEnumeration("Renderer.CompositedScrolling", IsScrollableAreaBucket, CompositedScrollingHistogramMax);
            m_isScrollableAreaHasBeenRecorded = true;
        }

        // We always want composited scrolling if compositor driven accelerated
        // scrolling is enabled. Since we will not update needs composited scrolling
        // in this case, we must force our state to update.
        if (m_box->compositorDrivenAcceleratedScrollingEnabled())
            layer()->didUpdateNeedsCompositedScrolling();
        else if (requiresScrollableArea)
            m_box->view()->compositor()->setNeedsUpdateCompositingRequirementsState();
        else
            setNeedsCompositedScrolling(false);
    }
}

void RenderLayerScrollableArea::updateNeedsCompositedScrolling()
{
    TRACE_EVENT0("comp-scroll", "RenderLayer::updateNeedsCompositedScrolling");

    layer()->stackingNode()->updateDescendantsAreContiguousInStackingOrder();
    layer()->updateDescendantDependentFlags();

    ASSERT(scrollsOverflow());
    const bool needsToBeStackingContainer = m_box->acceleratedCompositingForOverflowScrollEnabled()
        && layer()->stackingNode()->descendantsAreContiguousInStackingOrder()
        && !layer()->hasUnclippedDescendant();

    const bool needsToBeStackingContainerDidChange = layer()->stackingNode()->setNeedsToBeStackingContainer(needsToBeStackingContainer);

    const bool needsCompositedScrolling = needsToBeStackingContainer
        || m_box->compositorDrivenAcceleratedScrollingEnabled();

    // We gather a boolean value for use with Google UMA histograms to
    // quantify the actual effects of a set of patches attempting to
    // relax composited scrolling requirements, thereby increasing the
    // number of composited overflow divs.
    if (m_box->acceleratedCompositingForOverflowScrollEnabled())
        blink::Platform::current()->histogramEnumeration("Renderer.NeedsCompositedScrolling", needsCompositedScrolling, 2);

    const bool needsCompositedScrollingDidChange = setNeedsCompositedScrolling(needsCompositedScrolling);

    if (needsToBeStackingContainerDidChange || needsCompositedScrollingDidChange) {
        // Note, the z-order lists may need to be rebuilt, but our code guarantees
        // that we have not affected stacking, so we will not dirty
        // m_descendantsAreContiguousInStackingOrder for either us or our stacking
        // context or container.
        layer()->didUpdateNeedsCompositedScrolling();
    }
}

bool RenderLayerScrollableArea::setNeedsCompositedScrolling(bool needsCompositedScrolling)
{
    if (this->needsCompositedScrolling() == needsCompositedScrolling)
        return false;

    // Count the total number of RenderLayers which need composited scrolling at
    // some point. This should be recorded at most once per RenderLayer, so we
    // check m_willUseCompositedScrollingHasBeenRecorded.
    if (m_box->acceleratedCompositingForOverflowScrollEnabled() && !m_willUseCompositedScrollingHasBeenRecorded) {
        blink::Platform::current()->histogramEnumeration("Renderer.CompositedScrolling", WillUseCompositedScrollingBucket, CompositedScrollingHistogramMax);
        m_willUseCompositedScrollingHasBeenRecorded = true;
    }

    m_needsCompositedScrolling = needsCompositedScrolling;

    return true;
}

void RenderLayerScrollableArea::updateHasVisibleNonLayerContent()
{
    layer()->updateHasVisibleNonLayerContent();
}

void RenderLayerScrollableArea::updateCompositingLayersAfterScroll()
{
    RenderLayerCompositor* compositor = m_box->view()->compositor();
    if (compositor->inCompositingMode()) {
        // FIXME: Our stacking container is guaranteed to contain all of our descendants that may need
        // repositioning, so we should be able to enqueue a partial update compositing layers from there.
        // this feature was overridden for now by deferred compositing updates.
        if (usesCompositedScrolling())
            compositor->setNeedsCompositingUpdate(CompositingUpdateOnCompositedScroll);
        else
            compositor->setNeedsCompositingUpdate(CompositingUpdateOnScroll);
    }
}

bool RenderLayerScrollableArea::usesCompositedScrolling() const
{
    // Scroll form controls on the main thread so they exhibit correct touch scroll event bubbling
    if (m_box && (m_box->isIntristicallyScrollable(VerticalScrollbar) || m_box->isIntristicallyScrollable(HorizontalScrollbar)))
        return false;

    DisableCompositingQueryAsserts disabler;
    return m_box->hasCompositedLayerMapping() && m_box->compositedLayerMapping()->scrollingLayer();
}

bool RenderLayerScrollableArea::adjustForForceCompositedScrollingMode(bool value) const
{
    switch (m_forceNeedsCompositedScrolling) {
    case DoNotForceCompositedScrolling:
        return value;
    case CompositedScrollingAlwaysOn:
        return true;
    case CompositedScrollingAlwaysOff:
        return false;
    }

    ASSERT_NOT_REACHED();
    return value;
}

bool RenderLayerScrollableArea::needsCompositedScrolling() const
{
    return adjustForForceCompositedScrollingMode(m_needsCompositedScrolling);
}

void RenderLayerScrollableArea::setForceNeedsCompositedScrolling(ForceNeedsCompositedScrollingMode mode)
{
    if (m_forceNeedsCompositedScrolling == mode)
        return;

    m_forceNeedsCompositedScrolling = mode;
    layer()->didUpdateNeedsCompositedScrolling();
}

} // Namespace WebCore
