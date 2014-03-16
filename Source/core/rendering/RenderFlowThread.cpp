/*
 * Copyright (C) 2011 Adobe Systems Incorporated. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "config.h"

#include "core/rendering/RenderFlowThread.h"

#include "core/dom/Node.h"
#include "core/rendering/FlowThreadController.h"
#include "core/rendering/HitTestRequest.h"
#include "core/rendering/HitTestResult.h"
#include "core/rendering/LayoutRectRecorder.h"
#include "core/rendering/PaintInfo.h"
#include "core/rendering/RenderInline.h"
#include "core/rendering/RenderLayer.h"
#include "core/rendering/RenderRegion.h"
#include "core/rendering/RenderView.h"
#include "platform/PODIntervalTree.h"
#include "platform/geometry/TransformState.h"

namespace WebCore {

RenderFlowThread::RenderFlowThread()
    : RenderBlockFlow(0)
    , m_previousRegionCount(0)
    , m_regionsInvalidated(false)
    , m_regionsHaveUniformLogicalHeight(true)
    , m_pageLogicalSizeChanged(false)
{
    setFlowThreadState(InsideOutOfFlowThread);
}

PassRefPtr<RenderStyle> RenderFlowThread::createFlowThreadStyle(RenderStyle* parentStyle)
{
    RefPtr<RenderStyle> newStyle(RenderStyle::create());
    newStyle->inheritFrom(parentStyle);
    newStyle->setDisplay(BLOCK);
    newStyle->setPosition(AbsolutePosition);
    newStyle->setZIndex(0);
    newStyle->setLeft(Length(0, Fixed));
    newStyle->setTop(Length(0, Fixed));
    newStyle->setWidth(Length(100, Percent));
    newStyle->setHeight(Length(100, Percent));
    newStyle->font().update(nullptr);

    return newStyle.release();
}

void RenderFlowThread::addRegionToThread(RenderRegion* renderRegion)
{
    ASSERT(renderRegion);
    m_regionList.add(renderRegion);
    renderRegion->setIsValid(true);
}

void RenderFlowThread::removeRegionFromThread(RenderRegion* renderRegion)
{
    ASSERT(renderRegion);
    m_regionList.remove(renderRegion);
}

void RenderFlowThread::invalidateRegions()
{
    if (m_regionsInvalidated) {
        ASSERT(selfNeedsLayout());
        return;
    }

    m_regionRangeMap.clear();
    setNeedsLayout();

    m_regionsInvalidated = true;
}

class CurrentRenderFlowThreadDisabler {
    WTF_MAKE_NONCOPYABLE(CurrentRenderFlowThreadDisabler);
public:
    CurrentRenderFlowThreadDisabler(RenderView* view)
        : m_view(view)
        , m_renderFlowThread(0)
    {
        m_renderFlowThread = m_view->flowThreadController()->currentRenderFlowThread();
        if (m_renderFlowThread)
            view->flowThreadController()->setCurrentRenderFlowThread(0);
    }
    ~CurrentRenderFlowThreadDisabler()
    {
        if (m_renderFlowThread)
            m_view->flowThreadController()->setCurrentRenderFlowThread(m_renderFlowThread);
    }
private:
    RenderView* m_view;
    RenderFlowThread* m_renderFlowThread;
};

void RenderFlowThread::validateRegions()
{
    if (m_regionsInvalidated) {
        m_regionsInvalidated = false;
        m_regionsHaveUniformLogicalHeight = true;

        if (hasRegions()) {
            LayoutUnit previousRegionLogicalWidth = 0;
            LayoutUnit previousRegionLogicalHeight = 0;
            bool firstRegionVisited = false;

            for (RenderRegionList::iterator iter = m_regionList.begin(); iter != m_regionList.end(); ++iter) {
                RenderRegion* region = *iter;
                LayoutUnit regionLogicalWidth = region->pageLogicalWidth();
                LayoutUnit regionLogicalHeight = region->pageLogicalHeight();

                if (!firstRegionVisited) {
                    firstRegionVisited = true;
                } else {
                    if (m_regionsHaveUniformLogicalHeight && previousRegionLogicalHeight != regionLogicalHeight)
                        m_regionsHaveUniformLogicalHeight = false;
                }

                previousRegionLogicalWidth = regionLogicalWidth;
            }
        }
    }

    updateLogicalWidth(); // Called to get the maximum logical width for the region.
    updateRegionsFlowThreadPortionRect();
}

void RenderFlowThread::layout()
{
    LayoutRectRecorder recorder(*this);
    m_pageLogicalSizeChanged = m_regionsInvalidated && everHadLayout();

    validateRegions();

    CurrentRenderFlowThreadMaintainer currentFlowThreadSetter(this);
    RenderBlockFlow::layout();

    m_pageLogicalSizeChanged = false;

    if (lastRegion())
        lastRegion()->expandToEncompassFlowThreadContentsIfNeeded();
}

void RenderFlowThread::updateLogicalWidth()
{
    setLogicalWidth(initialLogicalWidth());
}

void RenderFlowThread::computeLogicalHeight(LayoutUnit, LayoutUnit logicalTop, LogicalExtentComputedValues& computedValues) const
{
    computedValues.m_position = logicalTop;
    computedValues.m_extent = 0;

    for (RenderRegionList::const_iterator iter = m_regionList.begin(); iter != m_regionList.end(); ++iter) {
        RenderRegion* region = *iter;
        computedValues.m_extent += region->logicalHeightOfAllFlowThreadContent();
    }
}

LayoutRect RenderFlowThread::computeRegionClippingRect(const LayoutPoint& offset, const LayoutRect& flowThreadPortionRect, const LayoutRect& flowThreadPortionOverflowRect) const
{
    LayoutRect regionClippingRect(offset + (flowThreadPortionOverflowRect.location() - flowThreadPortionRect.location()), flowThreadPortionOverflowRect.size());
    if (style()->isFlippedBlocksWritingMode())
        regionClippingRect.move(flowThreadPortionRect.size() - flowThreadPortionOverflowRect.size());
    return regionClippingRect;
}

bool RenderFlowThread::nodeAtPoint(const HitTestRequest& request, HitTestResult& result, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, HitTestAction hitTestAction)
{
    if (hitTestAction == HitTestBlockBackground)
        return false;
    return RenderBlockFlow::nodeAtPoint(request, result, locationInContainer, accumulatedOffset, hitTestAction);
}

bool RenderFlowThread::shouldRepaint(const LayoutRect& r) const
{
    if (view()->document().printing() || r.isEmpty())
        return false;

    return true;
}

void RenderFlowThread::repaintRectangleInRegions(const LayoutRect& repaintRect) const
{
    if (!shouldRepaint(repaintRect) || !hasValidRegionInfo())
        return;

    LayoutStateDisabler layoutStateDisabler(*this); // We can't use layout state to repaint, since the regions are somewhere else.

    // We can't use currentFlowThread as it is possible to have interleaved flow threads and the wrong one could be used.
    // Let each region figure out the proper enclosing flow thread.
    CurrentRenderFlowThreadDisabler disabler(view());

    for (RenderRegionList::const_iterator iter = m_regionList.begin(); iter != m_regionList.end(); ++iter) {
        RenderRegion* region = *iter;

        region->repaintFlowThreadContent(repaintRect);
    }
}

RenderRegion* RenderFlowThread::regionAtBlockOffset(LayoutUnit offset, bool extendLastRegion, RegionAutoGenerationPolicy autoGenerationPolicy)
{
    ASSERT(!m_regionsInvalidated);

    if (autoGenerationPolicy == AllowRegionAutoGeneration)
        autoGenerateRegionsToBlockOffset(offset);

    if (offset <= 0)
        return m_regionList.isEmpty() ? 0 : m_regionList.first();

    RegionSearchAdapter adapter(offset);
    m_regionIntervalTree.allOverlapsWithAdapter<RegionSearchAdapter>(adapter);

    // If no region was found, the offset is in the flow thread overflow.
    // The last region will contain the offset if extendLastRegion is set or if the last region is a set.
    if (!adapter.result() && !m_regionList.isEmpty())
        return m_regionList.last();

    return adapter.result();
}

RenderRegion* RenderFlowThread::regionFromAbsolutePointAndBox(IntPoint absolutePoint, const RenderBox* flowedBox)
{
    if (!flowedBox)
        return 0;

    RenderRegion* startRegion = 0;
    RenderRegion* endRegion = 0;
    getRegionRangeForBox(flowedBox, startRegion, endRegion);

    if (!startRegion)
        return 0;

    for (RenderRegionList::iterator iter = m_regionList.find(startRegion); iter != m_regionList.end(); ++iter) {
        RenderRegion* region = *iter;
        IntRect regionAbsoluteRect(roundedIntPoint(region->localToAbsolute()), roundedIntSize(region->frameRect().size()));
        if (regionAbsoluteRect.contains(absolutePoint))
            return region;

        if (region == endRegion)
            break;
    }

    return 0;
}

LayoutPoint RenderFlowThread::adjustedPositionRelativeToOffsetParent(const RenderBoxModelObject& boxModelObject, const LayoutPoint& startPoint)
{
    LayoutPoint referencePoint = startPoint;

    // FIXME: This needs to be adapted for different writing modes inside the flow thread.
    RenderRegion* startRegion = regionAtBlockOffset(referencePoint.y());
    if (startRegion) {
        // Take into account the offset coordinates of the region.
        RenderObject* currObject = startRegion;
        RenderObject* currOffsetParentRenderer;
        Element* currOffsetParentElement;
        while ((currOffsetParentElement = currObject->offsetParent()) && (currOffsetParentRenderer = currOffsetParentElement->renderer())) {
            if (currObject->isBoxModelObject())
                referencePoint.move(toRenderBoxModelObject(currObject)->offsetLeft(), toRenderBoxModelObject(currObject)->offsetTop());

            // Since we're looking for the offset relative to the body, we must also
            // take into consideration the borders of the region's offsetParent.
            if (currOffsetParentRenderer->isBox() && !currOffsetParentRenderer->isBody())
                referencePoint.move(toRenderBox(currOffsetParentRenderer)->borderLeft(), toRenderBox(currOffsetParentRenderer)->borderTop());

            currObject = currOffsetParentRenderer;
        }

        // We need to check if any of this box's containing blocks start in a different region
        // and if so, drop the object's top position (which was computed relative to its containing block
        // and is no longer valid) and recompute it using the region in which it flows as reference.
        bool wasComputedRelativeToOtherRegion = false;
        const RenderBlock* objContainingBlock = boxModelObject.containingBlock();
        while (objContainingBlock) {
            // Check if this object is in a different region.
            RenderRegion* parentStartRegion = 0;
            RenderRegion* parentEndRegion = 0;
            getRegionRangeForBox(objContainingBlock, parentStartRegion, parentEndRegion);
            if (parentStartRegion && parentStartRegion != startRegion) {
                wasComputedRelativeToOtherRegion = true;
                break;
            }
            objContainingBlock = objContainingBlock->containingBlock();
        }

        if (wasComputedRelativeToOtherRegion) {
            // Get the logical top coordinate of the current object.
            LayoutUnit top = 0;
            if (boxModelObject.isRenderBlock()) {
                top = toRenderBlock(&boxModelObject)->offsetFromLogicalTopOfFirstPage();
            } else {
                if (boxModelObject.containingBlock())
                    top = boxModelObject.containingBlock()->offsetFromLogicalTopOfFirstPage();

                if (boxModelObject.isBox())
                    top += toRenderBox(&boxModelObject)->topLeftLocation().y();
                else if (boxModelObject.isRenderInline())
                    top -= toRenderInline(&boxModelObject)->borderTop();
            }

            // Get the logical top of the region this object starts in
            // and compute the object's top, relative to the region's top.
            LayoutUnit regionLogicalTop = startRegion->pageLogicalTopForOffset(top);
            LayoutUnit topRelativeToRegion = top - regionLogicalTop;
            referencePoint.setY(startRegion->offsetTop() + topRelativeToRegion);

            // Since the top has been overriden, check if the
            // relative/sticky positioning must be reconsidered.
            if (boxModelObject.isRelPositioned())
                referencePoint.move(0, boxModelObject.relativePositionOffset().height());
            else if (boxModelObject.isStickyPositioned())
                referencePoint.move(0, boxModelObject.stickyPositionOffset().height());
        }

        // Since we're looking for the offset relative to the body, we must also
        // take into consideration the borders of the region.
        referencePoint.move(startRegion->borderLeft(), startRegion->borderTop());
    }

    return referencePoint;
}

LayoutUnit RenderFlowThread::pageLogicalTopForOffset(LayoutUnit offset)
{
    RenderRegion* region = regionAtBlockOffset(offset);
    return region ? region->pageLogicalTopForOffset(offset) : LayoutUnit();
}

LayoutUnit RenderFlowThread::pageLogicalWidthForOffset(LayoutUnit offset)
{
    RenderRegion* region = regionAtBlockOffset(offset, true);
    return region ? region->pageLogicalWidth() : contentLogicalWidth();
}

LayoutUnit RenderFlowThread::pageLogicalHeightForOffset(LayoutUnit offset)
{
    RenderRegion* region = regionAtBlockOffset(offset);
    if (!region)
        return 0;

    return region->pageLogicalHeight();
}

LayoutUnit RenderFlowThread::pageRemainingLogicalHeightForOffset(LayoutUnit offset, PageBoundaryRule pageBoundaryRule)
{
    RenderRegion* region = regionAtBlockOffset(offset);
    if (!region)
        return 0;

    LayoutUnit pageLogicalTop = region->pageLogicalTopForOffset(offset);
    LayoutUnit pageLogicalHeight = region->pageLogicalHeight();
    LayoutUnit pageLogicalBottom = pageLogicalTop + pageLogicalHeight;
    LayoutUnit remainingHeight = pageLogicalBottom - offset;
    if (pageBoundaryRule == IncludePageBoundary) {
        // If IncludePageBoundary is set, the line exactly on the top edge of a
        // region will act as being part of the previous region.
        remainingHeight = intMod(remainingHeight, pageLogicalHeight);
    }
    return remainingHeight;
}

RenderRegion* RenderFlowThread::mapFromFlowToRegion(TransformState& transformState) const
{
    if (!hasValidRegionInfo())
        return 0;

    LayoutRect boxRect = transformState.mappedQuad().enclosingBoundingBox();
    flipForWritingMode(boxRect);

    // FIXME: We need to refactor RenderObject::absoluteQuads to be able to split the quads across regions,
    // for now we just take the center of the mapped enclosing box and map it to a region.
    // Note: Using the center in order to avoid rounding errors.

    LayoutPoint center = boxRect.center();
    RenderRegion* renderRegion = const_cast<RenderFlowThread*>(this)->regionAtBlockOffset(isHorizontalWritingMode() ? center.y() : center.x(), true, DisallowRegionAutoGeneration);
    if (!renderRegion)
        return 0;

    LayoutRect flippedRegionRect(renderRegion->flowThreadPortionRect());
    flipForWritingMode(flippedRegionRect);

    transformState.move(renderRegion->contentBoxRect().location() - flippedRegionRect.location());

    return renderRegion;
}

RenderRegion* RenderFlowThread::firstRegion() const
{
    if (!hasValidRegionInfo())
        return 0;
    return m_regionList.first();
}

RenderRegion* RenderFlowThread::lastRegion() const
{
    if (!hasValidRegionInfo())
        return 0;
    return m_regionList.last();
}

void RenderFlowThread::setRegionRangeForBox(const RenderBox* box, LayoutUnit offsetFromLogicalTopOfFirstPage)
{
    if (!hasRegions())
        return;

    // FIXME: Not right for differing writing-modes.
    RenderRegion* startRegion = regionAtBlockOffset(offsetFromLogicalTopOfFirstPage, true);
    RenderRegion* endRegion = regionAtBlockOffset(offsetFromLogicalTopOfFirstPage + box->logicalHeight(), true);
    RenderRegionRangeMap::iterator it = m_regionRangeMap.find(box);
    if (it == m_regionRangeMap.end()) {
        m_regionRangeMap.set(box, RenderRegionRange(startRegion, endRegion));
        return;
    }

    // If nothing changed, just bail.
    RenderRegionRange& range = it->value;
    if (range.startRegion() == startRegion && range.endRegion() == endRegion)
        return;

    range.setRange(startRegion, endRegion);
}

void RenderFlowThread::getRegionRangeForBox(const RenderBox* box, RenderRegion*& startRegion, RenderRegion*& endRegion) const
{
    startRegion = 0;
    endRegion = 0;
    RenderRegionRangeMap::const_iterator it = m_regionRangeMap.find(box);
    if (it == m_regionRangeMap.end())
        return;

    const RenderRegionRange& range = it->value;
    startRegion = range.startRegion();
    endRegion = range.endRegion();
    ASSERT(m_regionList.contains(startRegion) && m_regionList.contains(endRegion));
}

void RenderFlowThread::applyBreakAfterContent(LayoutUnit clientHeight)
{
    // Simulate a region break at height. If it points inside an auto logical height region,
    // then it may determine the region computed autoheight.
    addForcedRegionBreak(clientHeight, this, false);
}

bool RenderFlowThread::regionInRange(const RenderRegion* targetRegion, const RenderRegion* startRegion, const RenderRegion* endRegion) const
{
    ASSERT(targetRegion);

    for (RenderRegionList::const_iterator it = m_regionList.find(const_cast<RenderRegion*>(startRegion)); it != m_regionList.end(); ++it) {
        const RenderRegion* currRegion = *it;
        if (targetRegion == currRegion)
            return true;
        if (currRegion == endRegion)
            break;
    }

    return false;
}

void RenderFlowThread::updateRegionsFlowThreadPortionRect()
{
    LayoutUnit logicalHeight = 0;
    // FIXME: Optimize not to clear the interval all the time. This implies manually managing the tree nodes lifecycle.
    m_regionIntervalTree.clear();
    m_regionIntervalTree.initIfNeeded();
    for (RenderRegionList::iterator iter = m_regionList.begin(); iter != m_regionList.end(); ++iter) {
        RenderRegion* region = *iter;

        LayoutUnit regionLogicalWidth = region->pageLogicalWidth();
        LayoutUnit regionLogicalHeight = std::min<LayoutUnit>(RenderFlowThread::maxLogicalHeight() - logicalHeight, region->logicalHeightOfAllFlowThreadContent());

        LayoutRect regionRect(style()->direction() == LTR ? LayoutUnit() : logicalWidth() - regionLogicalWidth, logicalHeight, regionLogicalWidth, regionLogicalHeight);

        region->setFlowThreadPortionRect(isHorizontalWritingMode() ? regionRect : regionRect.transposedRect());

        m_regionIntervalTree.add(RegionIntervalTree::createInterval(logicalHeight, logicalHeight + regionLogicalHeight, region));

        logicalHeight += regionLogicalHeight;
    }
}

void RenderFlowThread::collectLayerFragments(LayerFragments& layerFragments, const LayoutRect& layerBoundingBox, const LayoutRect& dirtyRect)
{
    ASSERT(!m_regionsInvalidated);

    for (RenderRegionList::const_iterator iter = m_regionList.begin(); iter != m_regionList.end(); ++iter) {
        RenderRegion* region = *iter;
        region->collectLayerFragments(layerFragments, layerBoundingBox, dirtyRect);
    }
}

LayoutRect RenderFlowThread::fragmentsBoundingBox(const LayoutRect& layerBoundingBox)
{
    ASSERT(!m_regionsInvalidated);

    LayoutRect result;
    for (RenderRegionList::const_iterator iter = m_regionList.begin(); iter != m_regionList.end(); ++iter) {
        RenderRegion* region = *iter;
        LayerFragments fragments;
        region->collectLayerFragments(fragments, layerBoundingBox, PaintInfo::infiniteRect());
        for (size_t i = 0; i < fragments.size(); ++i) {
            const LayerFragment& fragment = fragments.at(i);
            LayoutRect fragmentRect(layerBoundingBox);
            fragmentRect.intersect(fragment.paginationClip);
            fragmentRect.moveBy(fragment.paginationOffset);
            result.unite(fragmentRect);
        }
    }

    return result;
}

bool RenderFlowThread::cachedOffsetFromLogicalTopOfFirstRegion(const RenderBox* box, LayoutUnit& result) const
{
    RenderBoxToOffsetMap::const_iterator offsetIterator = m_boxesToOffsetMap.find(box);
    if (offsetIterator == m_boxesToOffsetMap.end())
        return false;

    result = offsetIterator->value;
    return true;
}

void RenderFlowThread::setOffsetFromLogicalTopOfFirstRegion(const RenderBox* box, LayoutUnit offset)
{
    m_boxesToOffsetMap.set(box, offset);
}

void RenderFlowThread::clearOffsetFromLogicalTopOfFirstRegion(const RenderBox* box)
{
    ASSERT(m_boxesToOffsetMap.contains(box));
    m_boxesToOffsetMap.remove(box);
}

const RenderBox* RenderFlowThread::currentStatePusherRenderBox() const
{
    const RenderObject* currentObject = m_statePusherObjectsStack.isEmpty() ? 0 : m_statePusherObjectsStack.last();
    if (currentObject && currentObject->isBox())
        return toRenderBox(currentObject);

    return 0;
}

void RenderFlowThread::pushFlowThreadLayoutState(const RenderObject& object)
{
    if (const RenderBox* currentBoxDescendant = currentStatePusherRenderBox()) {
        LayoutState* layoutState = currentBoxDescendant->view()->layoutState();
        if (layoutState && layoutState->isPaginated()) {
            ASSERT(layoutState->renderer() == currentBoxDescendant);
            LayoutSize offsetDelta = layoutState->m_layoutOffset - layoutState->m_pageOffset;
            setOffsetFromLogicalTopOfFirstRegion(currentBoxDescendant, currentBoxDescendant->isHorizontalWritingMode() ? offsetDelta.height() : offsetDelta.width());
        }
    }

    m_statePusherObjectsStack.add(&object);
}

void RenderFlowThread::popFlowThreadLayoutState()
{
    m_statePusherObjectsStack.removeLast();

    if (const RenderBox* currentBoxDescendant = currentStatePusherRenderBox()) {
        LayoutState* layoutState = currentBoxDescendant->view()->layoutState();
        if (layoutState && layoutState->isPaginated())
            clearOffsetFromLogicalTopOfFirstRegion(currentBoxDescendant);
    }
}

LayoutUnit RenderFlowThread::offsetFromLogicalTopOfFirstRegion(const RenderBlock* currentBlock) const
{
    // First check if we cached the offset for the block if it's an ancestor containing block of the box
    // being currently laid out.
    LayoutUnit offset;
    if (cachedOffsetFromLogicalTopOfFirstRegion(currentBlock, offset))
        return offset;

    // If it's the current box being laid out, use the layout state.
    const RenderBox* currentBoxDescendant = currentStatePusherRenderBox();
    if (currentBlock == currentBoxDescendant) {
        LayoutState* layoutState = view()->layoutState();
        ASSERT(layoutState->renderer() == currentBlock);
        ASSERT(layoutState && layoutState->isPaginated());
        LayoutSize offsetDelta = layoutState->m_layoutOffset - layoutState->m_pageOffset;
        return currentBoxDescendant->isHorizontalWritingMode() ? offsetDelta.height() : offsetDelta.width();
    }

    // As a last resort, take the slow path.
    LayoutRect blockRect(0, 0, currentBlock->width(), currentBlock->height());
    while (currentBlock && !currentBlock->isRenderFlowThread()) {
        RenderBlock* containerBlock = currentBlock->containingBlock();
        ASSERT(containerBlock);
        if (!containerBlock)
            return 0;
        LayoutPoint currentBlockLocation = currentBlock->location();

        if (containerBlock->style()->writingMode() != currentBlock->style()->writingMode()) {
            // We have to put the block rect in container coordinates
            // and we have to take into account both the container and current block flipping modes
            if (containerBlock->style()->isFlippedBlocksWritingMode()) {
                if (containerBlock->isHorizontalWritingMode())
                    blockRect.setY(currentBlock->height() - blockRect.maxY());
                else
                    blockRect.setX(currentBlock->width() - blockRect.maxX());
            }
            currentBlock->flipForWritingMode(blockRect);
        }
        blockRect.moveBy(currentBlockLocation);
        currentBlock = containerBlock;
    }

    return currentBlock->isHorizontalWritingMode() ? blockRect.y() : blockRect.x();
}

void RenderFlowThread::RegionSearchAdapter::collectIfNeeded(const RegionInterval& interval)
{
    if (m_result)
        return;
    if (interval.low() <= m_offset && interval.high() > m_offset)
        m_result = interval.data();
}

void RenderFlowThread::mapLocalToContainer(const RenderLayerModelObject* repaintContainer, TransformState& transformState, MapCoordinatesFlags mode, bool* wasFixed) const
{
    if (this == repaintContainer)
        return;

    if (RenderRegion* region = mapFromFlowToRegion(transformState)) {
        // FIXME: The cast below is probably not the best solution, we may need to find a better way.
        static_cast<const RenderObject*>(region)->mapLocalToContainer(region->containerForRepaint(), transformState, mode, wasFixed);
    }
}

CurrentRenderFlowThreadMaintainer::CurrentRenderFlowThreadMaintainer(RenderFlowThread* renderFlowThread)
    : m_renderFlowThread(renderFlowThread)
    , m_previousRenderFlowThread(0)
{
    if (!m_renderFlowThread)
        return;
    RenderView* view = m_renderFlowThread->view();
    m_previousRenderFlowThread = view->flowThreadController()->currentRenderFlowThread();
    view->flowThreadController()->setCurrentRenderFlowThread(m_renderFlowThread);
}

CurrentRenderFlowThreadMaintainer::~CurrentRenderFlowThreadMaintainer()
{
    if (!m_renderFlowThread)
        return;
    RenderView* view = m_renderFlowThread->view();
    ASSERT(view->flowThreadController()->currentRenderFlowThread() == m_renderFlowThread);
    view->flowThreadController()->setCurrentRenderFlowThread(m_previousRenderFlowThread);
}


} // namespace WebCore
