/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef ScrollingCoordinator_h
#define ScrollingCoordinator_h

#include "core/rendering/RenderObject.h"
#include "platform/PlatformWheelEvent.h"
#include "platform/geometry/IntRect.h"
#include "platform/scroll/ScrollTypes.h"
#include "wtf/text/WTFString.h"

namespace blink {
class WebLayer;
class WebScrollbarLayer;
}

namespace WebCore {

typedef unsigned MainThreadScrollingReasons;

class Document;
class LocalFrame;
class FrameView;
class GraphicsLayer;
class Page;
class Region;
class ScrollableArea;
class ViewportConstraints;

class ScrollingCoordinator {
public:
    static PassOwnPtr<ScrollingCoordinator> create(Page*);
    ~ScrollingCoordinator();

    void pageDestroyed();

    // Return whether this scrolling coordinator handles scrolling for the given frame view.
    bool coordinatesScrollingForFrameView(FrameView*) const;

    // Called when any frame has done its layout.
    void notifyLayoutUpdated();

    // Should be called after compositing has been updated.
    void updateAfterCompositingChange();

    bool needsToUpdateAfterCompositingChange() const { return m_scrollGestureRegionIsDirty || m_touchEventTargetRectsAreDirty || frameViewIsDirty(); }

    // Should be called whenever a wheel event handler is added or removed in the
    // frame view's underlying document.
    void frameViewWheelEventHandlerCountChanged(FrameView*);

    // Should be called whenever the slow repaint objects counter changes between zero and one.
    void frameViewHasSlowRepaintObjectsDidChange(FrameView*);

    // Should be called whenever the set of fixed objects changes.
    void frameViewFixedObjectsDidChange(FrameView*);

    // Should be called whenever the root layer for the given frame view changes.
    void frameViewRootLayerDidChange(FrameView*);

#if OS(MACOSX)
    // Dispatched by the scrolling tree during handleWheelEvent. This is required as long as scrollbars are painted on the main thread.
    void handleWheelEventPhase(PlatformWheelEventPhase);
#endif

    enum MainThreadScrollingReasonFlags {
        HasSlowRepaintObjects = 1 << 0,
        HasViewportConstrainedObjectsWithoutSupportingFixedLayers = 1 << 1,
        HasNonLayerViewportConstrainedObjects = 1 << 2,
    };

    MainThreadScrollingReasons mainThreadScrollingReasons() const;
    bool shouldUpdateScrollLayerPositionOnMainThread() const { return mainThreadScrollingReasons() != 0; }

    PassOwnPtr<blink::WebScrollbarLayer> createSolidColorScrollbarLayer(ScrollbarOrientation, int thumbThickness, bool isLeftSideVerticalScrollbar);

    void willDestroyScrollableArea(ScrollableArea*);
    // Returns true if the coordinator handled this change.
    bool scrollableAreaScrollLayerDidChange(ScrollableArea*);
    void scrollableAreaScrollbarLayerDidChange(ScrollableArea*, ScrollbarOrientation);
    void setLayerIsContainerForFixedPositionLayers(GraphicsLayer*, bool);
    void updateLayerPositionConstraint(RenderLayer*);
    void touchEventTargetRectsDidChange();
    void willDestroyRenderLayer(RenderLayer*);

    void updateScrollParentForGraphicsLayer(GraphicsLayer* child, RenderLayer* parent);
    void updateClipParentForGraphicsLayer(GraphicsLayer* child, RenderLayer* parent);

    static String mainThreadScrollingReasonsAsText(MainThreadScrollingReasons);
    String mainThreadScrollingReasonsAsText() const;
    Region computeShouldHandleScrollGestureOnMainThreadRegion(const LocalFrame*, const IntPoint& frameLocation) const;

    void updateTouchEventTargetRectsIfNeeded();

    // For testing purposes only. This ScrollingCoordinator is reused between layout test, and must be reset
    // for the results to be valid.
    void reset();

protected:
    explicit ScrollingCoordinator(Page*);

    bool isForMainFrame(ScrollableArea*) const;

    unsigned computeCurrentWheelEventHandlerCount();
    GraphicsLayer* scrollLayerForFrameView(FrameView*);
    GraphicsLayer* counterScrollingLayerForFrameView(FrameView*);

    Page* m_page;

    // Dirty flags used to idenfity what really needs to be computed after compositing is updated.
    bool m_scrollGestureRegionIsDirty;
    bool m_touchEventTargetRectsAreDirty;
    bool m_shouldScrollOnMainThreadDirty;

private:
    void recomputeWheelEventHandlerCountForFrameView(FrameView*);
    void setShouldUpdateScrollLayerPositionOnMainThread(MainThreadScrollingReasons);

    bool hasVisibleSlowRepaintViewportConstrainedObjects(FrameView*) const;

    bool touchHitTestingEnabled() const;
    void setShouldHandleScrollGestureOnMainThreadRegion(const Region&);
    void setTouchEventTargetRects(const LayerHitTestRects&);
    void computeTouchEventTargetRects(LayerHitTestRects&);
    void setWheelEventHandlerCount(unsigned);

    blink::WebScrollbarLayer* addWebScrollbarLayer(ScrollableArea*, ScrollbarOrientation, PassOwnPtr<blink::WebScrollbarLayer>);
    blink::WebScrollbarLayer* getWebScrollbarLayer(ScrollableArea*, ScrollbarOrientation);
    void removeWebScrollbarLayer(ScrollableArea*, ScrollbarOrientation);

    bool frameViewIsDirty() const;

    typedef HashMap<ScrollableArea*, OwnPtr<blink::WebScrollbarLayer> > ScrollbarMap;
    ScrollbarMap m_horizontalScrollbars;
    ScrollbarMap m_verticalScrollbars;
    HashSet<const RenderLayer*> m_layersWithTouchRects;
    bool m_wasFrameScrollable;

    // This is retained for testing.
    MainThreadScrollingReasons m_lastMainThreadScrollingReasons;
};

} // namespace WebCore

#endif // ScrollingCoordinator_h
