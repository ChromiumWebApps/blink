/*
 * Copyright (C) 2012 Apple Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "core/rendering/RenderMultiColumnBlock.h"

#include "core/rendering/RenderMultiColumnFlowThread.h"
#include "core/rendering/RenderMultiColumnSet.h"
#include "core/rendering/RenderView.h"

using namespace std;

namespace WebCore {

RenderMultiColumnBlock::RenderMultiColumnBlock(Element* element)
    : RenderBlockFlow(element)
    , m_flowThread(0)
    , m_columnCount(1)
    , m_columnWidth(0)
    , m_columnHeightAvailable(0)
    , m_inBalancingPass(false)
    , m_needsRebalancing(false)
{
}

void RenderMultiColumnBlock::styleDidChange(StyleDifference diff, const RenderStyle* oldStyle)
{
    RenderBlockFlow::styleDidChange(diff, oldStyle);
    for (RenderBox* child = firstChildBox(); child; child = child->nextSiblingBox())
        child->setStyle(RenderStyle::createAnonymousStyleWithDisplay(style(), BLOCK));
}

void RenderMultiColumnBlock::computeColumnCountAndWidth()
{
    // Calculate our column width and column count.
    // FIXME: Can overflow on fast/block/float/float-not-removed-from-next-sibling4.html, see https://bugs.webkit.org/show_bug.cgi?id=68744
    m_columnCount = 1;
    m_columnWidth = contentLogicalWidth();

    ASSERT(!style()->hasAutoColumnCount() || !style()->hasAutoColumnWidth());

    LayoutUnit availWidth = m_columnWidth;
    LayoutUnit colGap = columnGap();
    LayoutUnit colWidth = max<LayoutUnit>(1, LayoutUnit(style()->columnWidth()));
    int colCount = max<int>(1, style()->columnCount());

    if (style()->hasAutoColumnWidth() && !style()->hasAutoColumnCount()) {
        m_columnCount = colCount;
        m_columnWidth = max<LayoutUnit>(0, (availWidth - ((m_columnCount - 1) * colGap)) / m_columnCount);
    } else if (!style()->hasAutoColumnWidth() && style()->hasAutoColumnCount()) {
        m_columnCount = max<LayoutUnit>(1, (availWidth + colGap) / (colWidth + colGap));
        m_columnWidth = ((availWidth + colGap) / m_columnCount) - colGap;
    } else {
        m_columnCount = max<LayoutUnit>(min<LayoutUnit>(colCount, (availWidth + colGap) / (colWidth + colGap)), 1);
        m_columnWidth = ((availWidth + colGap) / m_columnCount) - colGap;
    }
}

bool RenderMultiColumnBlock::updateLogicalWidthAndColumnWidth()
{
    bool relayoutChildren = RenderBlockFlow::updateLogicalWidthAndColumnWidth();
    LayoutUnit oldColumnWidth = m_columnWidth;
    computeColumnCountAndWidth();
    if (m_columnWidth != oldColumnWidth)
        relayoutChildren = true;
    return relayoutChildren;
}

void RenderMultiColumnBlock::checkForPaginationLogicalHeightChange(LayoutUnit& /*pageLogicalHeight*/, bool& /*pageLogicalHeightChanged*/, bool& /*hasSpecifiedPageLogicalHeight*/)
{
    // We don't actually update any of the variables. We just subclassed to adjust our column height.
    updateLogicalHeight();
    m_columnHeightAvailable = max<LayoutUnit>(contentLogicalHeight(), 0);
    setLogicalHeight(0);
}

bool RenderMultiColumnBlock::shouldRelayoutMultiColumnBlock()
{
    if (!m_needsRebalancing)
        return false;

    // Column heights may change here because of balancing. We may have to do multiple layout
    // passes, depending on how the contents is fitted to the changed column heights. In most
    // cases, laying out again twice or even just once will suffice. Sometimes we need more
    // passes than that, though, but the number of retries should not exceed the number of
    // columns, unless we have a bug.
    bool needsRelayout = false;
    for (RenderBox* childBox = firstChildBox(); childBox; childBox = childBox->nextSiblingBox()) {
        if (childBox != m_flowThread && childBox->isRenderMultiColumnSet()) {
            RenderMultiColumnSet* multicolSet = toRenderMultiColumnSet(childBox);
            if (multicolSet->recalculateBalancedHeight(!m_inBalancingPass)) {
                multicolSet->setChildNeedsLayout(MarkOnlyThis);
                needsRelayout = true;
            }
        }
    }

    if (needsRelayout)
        m_flowThread->setChildNeedsLayout(MarkOnlyThis);

    m_inBalancingPass = needsRelayout;
    return needsRelayout;
}

void RenderMultiColumnBlock::addChild(RenderObject* newChild, RenderObject* beforeChild)
{
    if (!m_flowThread) {
        m_flowThread = RenderMultiColumnFlowThread::createAnonymous(&document());
        m_flowThread->setStyle(RenderStyle::createAnonymousStyleWithDisplay(style(), BLOCK));
        RenderBlockFlow::addChild(m_flowThread);
    }
    m_flowThread->addChild(newChild, beforeChild);
}

RenderObject* RenderMultiColumnBlock::layoutSpecialExcludedChild(bool relayoutChildren, SubtreeLayoutScope& layoutScope)
{
    if (!m_flowThread)
        return 0;

    // Update the dimensions of our regions before we lay out the flow thread.
    // FIXME: Eventually this is going to get way more complicated, and we will be destroying regions
    // instead of trying to keep them around.
    bool shouldInvalidateRegions = false;
    for (RenderBox* childBox = firstChildBox(); childBox; childBox = childBox->nextSiblingBox()) {
        if (childBox == m_flowThread)
            continue;

        if (relayoutChildren || childBox->needsLayout()) {
            if (!m_inBalancingPass && childBox->isRenderMultiColumnSet())
                toRenderMultiColumnSet(childBox)->prepareForLayout();
            shouldInvalidateRegions = true;
        }
    }

    if (shouldInvalidateRegions)
        m_flowThread->invalidateRegions();

    if (relayoutChildren)
        layoutScope.setChildNeedsLayout(m_flowThread);

    if (requiresBalancing()) {
        // At the end of multicol layout, relayoutForPagination() is called unconditionally, but if
        // no children are to be laid out (e.g. fixed width with layout already being up-to-date),
        // we want to prevent it from doing any work, so that the column balancing machinery doesn't
        // kick in and trigger additional unnecessary layout passes. Actually, it's not just a good
        // idea in general to not waste time on balancing content that hasn't been re-laid out; we
        // are actually required to guarantee this. The calculation of implicit breaks needs to be
        // preceded by a proper layout pass, since it's layout that sets up content runs, and the
        // runs get deleted right after every pass.
        m_needsRebalancing = shouldInvalidateRegions || m_flowThread->needsLayout();
    }

    setLogicalTopForChild(m_flowThread, borderBefore() + paddingBefore());
    m_flowThread->layoutIfNeeded();
    determineLogicalLeftPositionForChild(m_flowThread);

    return m_flowThread;
}

const char* RenderMultiColumnBlock::renderName() const
{
    if (isFloating())
        return "RenderMultiColumnBlock (floating)";
    if (isOutOfFlowPositioned())
        return "RenderMultiColumnBlock (positioned)";
    if (isAnonymousBlock())
        return "RenderMultiColumnBlock (anonymous)";
    // FIXME: Temporary hack while the new generated content system is being implemented.
    if (isPseudoElement())
        return "RenderMultiColumnBlock (generated)";
    if (isAnonymous())
        return "RenderMultiColumnBlock (generated)";
    if (isRelPositioned())
        return "RenderMultiColumnBlock (relative positioned)";
    return "RenderMultiColumnBlock";
}

}
