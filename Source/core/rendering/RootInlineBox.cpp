/*
 * Copyright (C) 2003, 2006, 2008 Apple Inc. All rights reserved.
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
#include "core/rendering/RootInlineBox.h"

#include "core/dom/Document.h"
#include "core/rendering/EllipsisBox.h"
#include "core/rendering/HitTestResult.h"
#include "core/rendering/InlineTextBox.h"
#include "core/rendering/PaintInfo.h"
#include "core/rendering/RenderBlockFlow.h"
#include "core/rendering/RenderFlowThread.h"
#include "core/rendering/RenderInline.h"
#include "core/rendering/RenderView.h"
#include "core/rendering/VerticalPositionCache.h"
#include "platform/text/BidiResolver.h"
#include "wtf/unicode/Unicode.h"

using namespace std;

namespace WebCore {

struct SameSizeAsRootInlineBox : public InlineFlowBox {
    unsigned variables[5];
    void* pointers[4];
};

COMPILE_ASSERT(sizeof(RootInlineBox) == sizeof(SameSizeAsRootInlineBox), RootInlineBox_should_stay_small);

typedef WTF::HashMap<const RootInlineBox*, EllipsisBox*> EllipsisBoxMap;
static EllipsisBoxMap* gEllipsisBoxMap = 0;

RootInlineBox::RootInlineBox(RenderBlockFlow& block)
    : InlineFlowBox(block)
    , m_lineBreakPos(0)
    , m_lineBreakObj(0)
    , m_lineTop(0)
    , m_lineBottom(0)
    , m_lineTopWithLeading(0)
    , m_lineBottomWithLeading(0)
{
    setIsHorizontal(block.isHorizontalWritingMode());
}


void RootInlineBox::destroy()
{
    detachEllipsisBox();
    InlineFlowBox::destroy();
}

void RootInlineBox::detachEllipsisBox()
{
    if (hasEllipsisBox()) {
        EllipsisBox* box = gEllipsisBoxMap->take(this);
        box->setParent(0);
        box->destroy();
        setHasEllipsisBox(false);
    }
}

RenderLineBoxList* RootInlineBox::rendererLineBoxes() const
{
    return block().lineBoxes();
}

void RootInlineBox::clearTruncation()
{
    if (hasEllipsisBox()) {
        detachEllipsisBox();
        InlineFlowBox::clearTruncation();
    }
}

bool RootInlineBox::isHyphenated() const
{
    for (InlineBox* box = firstLeafChild(); box; box = box->nextLeafChild()) {
        if (box->isInlineTextBox()) {
            if (toInlineTextBox(box)->hasHyphen())
                return true;
        }
    }

    return false;
}

int RootInlineBox::baselinePosition(FontBaseline baselineType) const
{
    return boxModelObject()->baselinePosition(baselineType, isFirstLineStyle(), isHorizontal() ? HorizontalLine : VerticalLine, PositionOfInteriorLineBoxes);
}

LayoutUnit RootInlineBox::lineHeight() const
{
    return boxModelObject()->lineHeight(isFirstLineStyle(), isHorizontal() ? HorizontalLine : VerticalLine, PositionOfInteriorLineBoxes);
}

bool RootInlineBox::lineCanAccommodateEllipsis(bool ltr, int blockEdge, int lineBoxEdge, int ellipsisWidth)
{
    // First sanity-check the unoverflowed width of the whole line to see if there is sufficient room.
    int delta = ltr ? lineBoxEdge - blockEdge : blockEdge - lineBoxEdge;
    if (logicalWidth() - delta < ellipsisWidth)
        return false;

    // Next iterate over all the line boxes on the line.  If we find a replaced element that intersects
    // then we refuse to accommodate the ellipsis.  Otherwise we're ok.
    return InlineFlowBox::canAccommodateEllipsis(ltr, blockEdge, ellipsisWidth);
}

float RootInlineBox::placeEllipsis(const AtomicString& ellipsisStr,  bool ltr, float blockLeftEdge, float blockRightEdge, float ellipsisWidth,
                                  InlineBox* markupBox)
{
    // Create an ellipsis box.
    EllipsisBox* ellipsisBox = new EllipsisBox(renderer(), ellipsisStr, this,
        ellipsisWidth - (markupBox ? markupBox->logicalWidth() : 0), logicalHeight(),
        x(), y(), !prevRootBox(), isHorizontal(), markupBox);

    if (!gEllipsisBoxMap)
        gEllipsisBoxMap = new EllipsisBoxMap();
    gEllipsisBoxMap->add(this, ellipsisBox);
    setHasEllipsisBox(true);

    // FIXME: Do we need an RTL version of this?
    if (ltr && (logicalLeft() + logicalWidth() + ellipsisWidth) <= blockRightEdge) {
        ellipsisBox->setLogicalLeft(logicalLeft() + logicalWidth());
        return logicalWidth() + ellipsisWidth;
    }

    // Now attempt to find the nearest glyph horizontally and place just to the right (or left in RTL)
    // of that glyph.  Mark all of the objects that intersect the ellipsis box as not painting (as being
    // truncated).
    bool foundBox = false;
    float truncatedWidth = 0;
    float position = placeEllipsisBox(ltr, blockLeftEdge, blockRightEdge, ellipsisWidth, truncatedWidth, foundBox);
    ellipsisBox->setLogicalLeft(position);
    return truncatedWidth;
}

float RootInlineBox::placeEllipsisBox(bool ltr, float blockLeftEdge, float blockRightEdge, float ellipsisWidth, float &truncatedWidth, bool& foundBox)
{
    float result = InlineFlowBox::placeEllipsisBox(ltr, blockLeftEdge, blockRightEdge, ellipsisWidth, truncatedWidth, foundBox);
    if (result == -1) {
        result = ltr ? blockRightEdge - ellipsisWidth : blockLeftEdge;
        truncatedWidth = blockRightEdge - blockLeftEdge;
    }
    return result;
}

void RootInlineBox::paintEllipsisBox(PaintInfo& paintInfo, const LayoutPoint& paintOffset, LayoutUnit lineTop, LayoutUnit lineBottom) const
{
    if (hasEllipsisBox() && paintInfo.shouldPaintWithinRoot(&renderer()) && renderer().style()->visibility() == VISIBLE
            && paintInfo.phase == PaintPhaseForeground)
        ellipsisBox()->paint(paintInfo, paintOffset, lineTop, lineBottom);
}

void RootInlineBox::paint(PaintInfo& paintInfo, const LayoutPoint& paintOffset, LayoutUnit lineTop, LayoutUnit lineBottom)
{
    InlineFlowBox::paint(paintInfo, paintOffset, lineTop, lineBottom);
    paintEllipsisBox(paintInfo, paintOffset, lineTop, lineBottom);
}

bool RootInlineBox::nodeAtPoint(const HitTestRequest& request, HitTestResult& result, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, LayoutUnit lineTop, LayoutUnit lineBottom)
{
    if (hasEllipsisBox() && visibleToHitTestRequest(request)) {
        if (ellipsisBox()->nodeAtPoint(request, result, locationInContainer, accumulatedOffset, lineTop, lineBottom)) {
            renderer().updateHitTestResult(result, locationInContainer.point() - toLayoutSize(accumulatedOffset));
            return true;
        }
    }
    return InlineFlowBox::nodeAtPoint(request, result, locationInContainer, accumulatedOffset, lineTop, lineBottom);
}

void RootInlineBox::adjustPosition(float dx, float dy)
{
    InlineFlowBox::adjustPosition(dx, dy);
    LayoutUnit blockDirectionDelta = isHorizontal() ? dy : dx; // The block direction delta is a LayoutUnit.
    m_lineTop += blockDirectionDelta;
    m_lineBottom += blockDirectionDelta;
    m_lineTopWithLeading += blockDirectionDelta;
    m_lineBottomWithLeading += blockDirectionDelta;
    if (hasEllipsisBox())
        ellipsisBox()->adjustPosition(dx, dy);
}

void RootInlineBox::childRemoved(InlineBox* box)
{
    if (&box->renderer() == m_lineBreakObj)
        setLineBreakInfo(0, 0, BidiStatus());

    for (RootInlineBox* prev = prevRootBox(); prev && prev->lineBreakObj() == &box->renderer(); prev = prev->prevRootBox()) {
        prev->setLineBreakInfo(0, 0, BidiStatus());
        prev->markDirty();
    }
}

LayoutUnit RootInlineBox::alignBoxesInBlockDirection(LayoutUnit heightOfBlock, GlyphOverflowAndFallbackFontsMap& textBoxDataMap, VerticalPositionCache& verticalPositionCache)
{
    // SVG will handle vertical alignment on its own.
    if (isSVGRootInlineBox())
        return 0;

    LayoutUnit maxPositionTop = 0;
    LayoutUnit maxPositionBottom = 0;
    int maxAscent = 0;
    int maxDescent = 0;
    bool setMaxAscent = false;
    bool setMaxDescent = false;

    // Figure out if we're in no-quirks mode.
    bool noQuirksMode = renderer().document().inNoQuirksMode();

    m_baselineType = requiresIdeographicBaseline(textBoxDataMap) ? IdeographicBaseline : AlphabeticBaseline;

    computeLogicalBoxHeights(this, maxPositionTop, maxPositionBottom, maxAscent, maxDescent, setMaxAscent, setMaxDescent, noQuirksMode,
                             textBoxDataMap, baselineType(), verticalPositionCache);

    if (maxAscent + maxDescent < max(maxPositionTop, maxPositionBottom))
        adjustMaxAscentAndDescent(maxAscent, maxDescent, maxPositionTop, maxPositionBottom);

    LayoutUnit maxHeight = maxAscent + maxDescent;
    LayoutUnit lineTop = heightOfBlock;
    LayoutUnit lineBottom = heightOfBlock;
    LayoutUnit lineTopIncludingMargins = heightOfBlock;
    LayoutUnit lineBottomIncludingMargins = heightOfBlock;
    bool setLineTop = false;
    bool hasAnnotationsBefore = false;
    bool hasAnnotationsAfter = false;
    placeBoxesInBlockDirection(heightOfBlock, maxHeight, maxAscent, noQuirksMode, lineTop, lineBottom, setLineTop,
                               lineTopIncludingMargins, lineBottomIncludingMargins, hasAnnotationsBefore, hasAnnotationsAfter, baselineType());
    m_hasAnnotationsBefore = hasAnnotationsBefore;
    m_hasAnnotationsAfter = hasAnnotationsAfter;

    maxHeight = max<LayoutUnit>(0, maxHeight); // FIXME: Is this really necessary?

    setLineTopBottomPositions(lineTop, lineBottom, heightOfBlock, heightOfBlock + maxHeight);
    setPaginatedLineWidth(block().availableLogicalWidthForContent());

    LayoutUnit annotationsAdjustment = beforeAnnotationsAdjustment();
    if (annotationsAdjustment) {
        // FIXME: Need to handle pagination here. We might have to move to the next page/column as a result of the
        // ruby expansion.
        adjustBlockDirectionPosition(annotationsAdjustment.toFloat());
        heightOfBlock += annotationsAdjustment;
    }

    return heightOfBlock + maxHeight;
}

float RootInlineBox::maxLogicalTop() const
{
    float maxLogicalTop = 0;
    computeMaxLogicalTop(maxLogicalTop);
    return maxLogicalTop;
}

LayoutUnit RootInlineBox::beforeAnnotationsAdjustment() const
{
    LayoutUnit result = 0;

    if (!renderer().style()->isFlippedLinesWritingMode()) {
        // Annotations under the previous line may push us down.
        if (prevRootBox() && prevRootBox()->hasAnnotationsAfter())
            result = prevRootBox()->computeUnderAnnotationAdjustment(lineTop());

        if (!hasAnnotationsBefore())
            return result;

        // Annotations over this line may push us further down.
        LayoutUnit highestAllowedPosition = prevRootBox() ? min(prevRootBox()->lineBottom(), lineTop()) + result : static_cast<LayoutUnit>(block().borderBefore());
        result = computeOverAnnotationAdjustment(highestAllowedPosition);
    } else {
        // Annotations under this line may push us up.
        if (hasAnnotationsBefore())
            result = computeUnderAnnotationAdjustment(prevRootBox() ? prevRootBox()->lineBottom() : static_cast<LayoutUnit>(block().borderBefore()));

        if (!prevRootBox() || !prevRootBox()->hasAnnotationsAfter())
            return result;

        // We have to compute the expansion for annotations over the previous line to see how much we should move.
        LayoutUnit lowestAllowedPosition = max(prevRootBox()->lineBottom(), lineTop()) - result;
        result = prevRootBox()->computeOverAnnotationAdjustment(lowestAllowedPosition);
    }

    return result;
}

GapRects RootInlineBox::lineSelectionGap(RenderBlock* rootBlock, const LayoutPoint& rootBlockPhysicalPosition, const LayoutSize& offsetFromRootBlock,
                                         LayoutUnit selTop, LayoutUnit selHeight, const PaintInfo* paintInfo)
{
    RenderObject::SelectionState lineState = selectionState();

    bool leftGap, rightGap;
    block().getSelectionGapInfo(lineState, leftGap, rightGap);

    GapRects result;

    InlineBox* firstBox = firstSelectedBox();
    InlineBox* lastBox = lastSelectedBox();
    if (leftGap) {
        result.uniteLeft(block().logicalLeftSelectionGap(rootBlock, rootBlockPhysicalPosition, offsetFromRootBlock,
            &firstBox->parent()->renderer(), firstBox->logicalLeft(), selTop, selHeight, paintInfo));
    }
    if (rightGap) {
        result.uniteRight(block().logicalRightSelectionGap(rootBlock, rootBlockPhysicalPosition, offsetFromRootBlock,
            &lastBox->parent()->renderer(), lastBox->logicalRight(), selTop, selHeight, paintInfo));
    }

    // When dealing with bidi text, a non-contiguous selection region is possible.
    // e.g. The logical text aaaAAAbbb (capitals denote RTL text and non-capitals LTR) is layed out
    // visually as 3 text runs |aaa|bbb|AAA| if we select 4 characters from the start of the text the
    // selection will look like (underline denotes selection):
    // |aaa|bbb|AAA|
    //  ___       _
    // We can see that the |bbb| run is not part of the selection while the runs around it are.
    if (firstBox && firstBox != lastBox) {
        // Now fill in any gaps on the line that occurred between two selected elements.
        LayoutUnit lastLogicalLeft = firstBox->logicalRight();
        bool isPreviousBoxSelected = firstBox->selectionState() != RenderObject::SelectionNone;
        for (InlineBox* box = firstBox->nextLeafChild(); box; box = box->nextLeafChild()) {
            if (box->selectionState() != RenderObject::SelectionNone) {
                LayoutRect logicalRect(lastLogicalLeft, selTop, box->logicalLeft() - lastLogicalLeft, selHeight);
                logicalRect.move(renderer().isHorizontalWritingMode() ? offsetFromRootBlock : LayoutSize(offsetFromRootBlock.height(), offsetFromRootBlock.width()));
                LayoutRect gapRect = rootBlock->logicalRectToPhysicalRect(rootBlockPhysicalPosition, logicalRect);
                if (isPreviousBoxSelected && gapRect.width() > 0 && gapRect.height() > 0) {
                    if (paintInfo && box->parent()->renderer().style()->visibility() == VISIBLE)
                        paintInfo->context->fillRect(gapRect, box->parent()->renderer().selectionBackgroundColor());
                    // VisibleSelection may be non-contiguous, see comment above.
                    result.uniteCenter(gapRect);
                }
                lastLogicalLeft = box->logicalRight();
            }
            if (box == lastBox)
                break;
            isPreviousBoxSelected = box->selectionState() != RenderObject::SelectionNone;
        }
    }

    return result;
}

RenderObject::SelectionState RootInlineBox::selectionState()
{
    // Walk over all of the selected boxes.
    RenderObject::SelectionState state = RenderObject::SelectionNone;
    for (InlineBox* box = firstLeafChild(); box; box = box->nextLeafChild()) {
        RenderObject::SelectionState boxState = box->selectionState();
        if ((boxState == RenderObject::SelectionStart && state == RenderObject::SelectionEnd) ||
            (boxState == RenderObject::SelectionEnd && state == RenderObject::SelectionStart))
            state = RenderObject::SelectionBoth;
        else if (state == RenderObject::SelectionNone ||
                 ((boxState == RenderObject::SelectionStart || boxState == RenderObject::SelectionEnd) &&
                  (state == RenderObject::SelectionNone || state == RenderObject::SelectionInside)))
            state = boxState;
        else if (boxState == RenderObject::SelectionNone && state == RenderObject::SelectionStart) {
            // We are past the end of the selection.
            state = RenderObject::SelectionBoth;
        }
        if (state == RenderObject::SelectionBoth)
            break;
    }

    return state;
}

InlineBox* RootInlineBox::firstSelectedBox()
{
    for (InlineBox* box = firstLeafChild(); box; box = box->nextLeafChild()) {
        if (box->selectionState() != RenderObject::SelectionNone)
            return box;
    }

    return 0;
}

InlineBox* RootInlineBox::lastSelectedBox()
{
    for (InlineBox* box = lastLeafChild(); box; box = box->prevLeafChild()) {
        if (box->selectionState() != RenderObject::SelectionNone)
            return box;
    }

    return 0;
}

LayoutUnit RootInlineBox::selectionTop() const
{
    LayoutUnit selectionTop = m_lineTop;

    if (m_hasAnnotationsBefore)
        selectionTop -= !renderer().style()->isFlippedLinesWritingMode() ? computeOverAnnotationAdjustment(m_lineTop) : computeUnderAnnotationAdjustment(m_lineTop);

    if (renderer().style()->isFlippedLinesWritingMode() || !prevRootBox())
        return selectionTop;

    LayoutUnit prevBottom = prevRootBox()->selectionBottom();
    if (prevBottom < selectionTop && block().containsFloats()) {
        // This line has actually been moved further down, probably from a large line-height, but possibly because the
        // line was forced to clear floats.  If so, let's check the offsets, and only be willing to use the previous
        // line's bottom if the offsets are greater on both sides.
        LayoutUnit prevLeft = block().logicalLeftOffsetForLine(prevBottom, false);
        LayoutUnit prevRight = block().logicalRightOffsetForLine(prevBottom, false);
        LayoutUnit newLeft = block().logicalLeftOffsetForLine(selectionTop, false);
        LayoutUnit newRight = block().logicalRightOffsetForLine(selectionTop, false);
        if (prevLeft > newLeft || prevRight < newRight)
            return selectionTop;
    }

    return prevBottom;
}

LayoutUnit RootInlineBox::selectionTopAdjustedForPrecedingBlock() const
{
    LayoutUnit top = selectionTop();

    RenderObject::SelectionState blockSelectionState = root().block().selectionState();
    if (blockSelectionState != RenderObject::SelectionInside && blockSelectionState != RenderObject::SelectionEnd)
        return top;

    LayoutSize offsetToBlockBefore;
    if (RenderBlock* block = root().block().blockBeforeWithinSelectionRoot(offsetToBlockBefore)) {
        if (RootInlineBox* lastLine = block->lastRootBox()) {
            RenderObject::SelectionState lastLineSelectionState = lastLine->selectionState();
            if (lastLineSelectionState != RenderObject::SelectionInside && lastLineSelectionState != RenderObject::SelectionStart)
                return top;

            LayoutUnit lastLineSelectionBottom = lastLine->selectionBottom() + offsetToBlockBefore.height();
            top = max(top, lastLineSelectionBottom);
        }
    }

    return top;
}

LayoutUnit RootInlineBox::selectionBottom() const
{
    LayoutUnit selectionBottom = m_lineBottom;

    if (m_hasAnnotationsAfter)
        selectionBottom += !renderer().style()->isFlippedLinesWritingMode() ? computeUnderAnnotationAdjustment(m_lineBottom) : computeOverAnnotationAdjustment(m_lineBottom);

    if (!renderer().style()->isFlippedLinesWritingMode() || !nextRootBox())
        return selectionBottom;

    LayoutUnit nextTop = nextRootBox()->selectionTop();
    if (nextTop > selectionBottom && block().containsFloats()) {
        // The next line has actually been moved further over, probably from a large line-height, but possibly because the
        // line was forced to clear floats.  If so, let's check the offsets, and only be willing to use the next
        // line's top if the offsets are greater on both sides.
        LayoutUnit nextLeft = block().logicalLeftOffsetForLine(nextTop, false);
        LayoutUnit nextRight = block().logicalRightOffsetForLine(nextTop, false);
        LayoutUnit newLeft = block().logicalLeftOffsetForLine(selectionBottom, false);
        LayoutUnit newRight = block().logicalRightOffsetForLine(selectionBottom, false);
        if (nextLeft > newLeft || nextRight < newRight)
            return selectionBottom;
    }

    return nextTop;
}

int RootInlineBox::blockDirectionPointInLine() const
{
    return !block().style()->isFlippedBlocksWritingMode() ? max(lineTop(), selectionTop()) : min(lineBottom(), selectionBottom());
}

RenderBlockFlow& RootInlineBox::block() const
{
    return toRenderBlockFlow(renderer());
}

static bool isEditableLeaf(InlineBox* leaf)
{
    return leaf && leaf->renderer().node() && leaf->renderer().node()->rendererIsEditable();
}

InlineBox* RootInlineBox::closestLeafChildForPoint(const IntPoint& pointInContents, bool onlyEditableLeaves)
{
    return closestLeafChildForLogicalLeftPosition(block().isHorizontalWritingMode() ? pointInContents.x() : pointInContents.y(), onlyEditableLeaves);
}

InlineBox* RootInlineBox::closestLeafChildForLogicalLeftPosition(int leftPosition, bool onlyEditableLeaves)
{
    InlineBox* firstLeaf = firstLeafChild();
    InlineBox* lastLeaf = lastLeafChild();

    if (firstLeaf != lastLeaf) {
        if (firstLeaf->isLineBreak())
            firstLeaf = firstLeaf->nextLeafChildIgnoringLineBreak();
        else if (lastLeaf->isLineBreak())
            lastLeaf = lastLeaf->prevLeafChildIgnoringLineBreak();
    }

    if (firstLeaf == lastLeaf && (!onlyEditableLeaves || isEditableLeaf(firstLeaf)))
        return firstLeaf;

    // Avoid returning a list marker when possible.
    if (leftPosition <= firstLeaf->logicalLeft() && !firstLeaf->renderer().isListMarker() && (!onlyEditableLeaves || isEditableLeaf(firstLeaf)))
        // The leftPosition coordinate is less or equal to left edge of the firstLeaf.
        // Return it.
        return firstLeaf;

    if (leftPosition >= lastLeaf->logicalRight() && !lastLeaf->renderer().isListMarker() && (!onlyEditableLeaves || isEditableLeaf(lastLeaf)))
        // The leftPosition coordinate is greater or equal to right edge of the lastLeaf.
        // Return it.
        return lastLeaf;

    InlineBox* closestLeaf = 0;
    for (InlineBox* leaf = firstLeaf; leaf; leaf = leaf->nextLeafChildIgnoringLineBreak()) {
        if (!leaf->renderer().isListMarker() && (!onlyEditableLeaves || isEditableLeaf(leaf))) {
            closestLeaf = leaf;
            if (leftPosition < leaf->logicalRight())
                // The x coordinate is less than the right edge of the box.
                // Return it.
                return leaf;
        }
    }

    return closestLeaf ? closestLeaf : lastLeaf;
}

BidiStatus RootInlineBox::lineBreakBidiStatus() const
{
    return BidiStatus(static_cast<WTF::Unicode::Direction>(m_lineBreakBidiStatusEor), static_cast<WTF::Unicode::Direction>(m_lineBreakBidiStatusLastStrong), static_cast<WTF::Unicode::Direction>(m_lineBreakBidiStatusLast), m_lineBreakContext);
}

void RootInlineBox::setLineBreakInfo(RenderObject* obj, unsigned breakPos, const BidiStatus& status)
{
    // When setting lineBreakObj, the RenderObject must not be a RenderInline
    // with no line boxes, otherwise all sorts of invariants are broken later.
    // This has security implications because if the RenderObject does not
    // point to at least one line box, then that RenderInline can be deleted
    // later without resetting the lineBreakObj, leading to use-after-free.
    ASSERT_WITH_SECURITY_IMPLICATION(!obj || obj->isText() || !(obj->isRenderInline() && obj->isBox() && !toRenderBox(obj)->inlineBoxWrapper()));

    m_lineBreakObj = obj;
    m_lineBreakPos = breakPos;
    m_lineBreakBidiStatusEor = status.eor;
    m_lineBreakBidiStatusLastStrong = status.lastStrong;
    m_lineBreakBidiStatusLast = status.last;
    m_lineBreakContext = status.context;
}

EllipsisBox* RootInlineBox::ellipsisBox() const
{
    if (!hasEllipsisBox())
        return 0;
    return gEllipsisBoxMap->get(this);
}

void RootInlineBox::removeLineBoxFromRenderObject()
{
    block().lineBoxes()->removeLineBox(this);
}

void RootInlineBox::extractLineBoxFromRenderObject()
{
    block().lineBoxes()->extractLineBox(this);
}

void RootInlineBox::attachLineBoxToRenderObject()
{
    block().lineBoxes()->attachLineBox(this);
}

LayoutRect RootInlineBox::paddedLayoutOverflowRect(LayoutUnit endPadding) const
{
    LayoutRect lineLayoutOverflow = layoutOverflowRect(lineTop(), lineBottom());
    if (!endPadding)
        return lineLayoutOverflow;

    if (isHorizontal()) {
        if (isLeftToRightDirection())
            lineLayoutOverflow.shiftMaxXEdgeTo(max<LayoutUnit>(lineLayoutOverflow.maxX(), logicalRight() + endPadding));
        else
            lineLayoutOverflow.shiftXEdgeTo(min<LayoutUnit>(lineLayoutOverflow.x(), logicalLeft() - endPadding));
    } else {
        if (isLeftToRightDirection())
            lineLayoutOverflow.shiftMaxYEdgeTo(max<LayoutUnit>(lineLayoutOverflow.maxY(), logicalRight() + endPadding));
        else
            lineLayoutOverflow.shiftYEdgeTo(min<LayoutUnit>(lineLayoutOverflow.y(), logicalLeft() - endPadding));
    }

    return lineLayoutOverflow;
}

static void setAscentAndDescent(int& ascent, int& descent, int newAscent, int newDescent, bool& ascentDescentSet)
{
    if (!ascentDescentSet) {
        ascentDescentSet = true;
        ascent = newAscent;
        descent = newDescent;
    } else {
        ascent = max(ascent, newAscent);
        descent = max(descent, newDescent);
    }
}

void RootInlineBox::ascentAndDescentForBox(InlineBox* box, GlyphOverflowAndFallbackFontsMap& textBoxDataMap, int& ascent, int& descent,
                                           bool& affectsAscent, bool& affectsDescent) const
{
    bool ascentDescentSet = false;

    // Replaced boxes will return 0 for the line-height if line-box-contain says they are
    // not to be included.
    if (box->renderer().isReplaced()) {
        if (renderer().style(isFirstLineStyle())->lineBoxContain() & LineBoxContainReplaced) {
            ascent = box->baselinePosition(baselineType());
            descent = box->lineHeight() - ascent;

            // Replaced elements always affect both the ascent and descent.
            affectsAscent = true;
            affectsDescent = true;
        }
        return;
    }

    Vector<const SimpleFontData*>* usedFonts = 0;
    GlyphOverflow* glyphOverflow = 0;
    if (box->isText()) {
        GlyphOverflowAndFallbackFontsMap::iterator it = textBoxDataMap.find(toInlineTextBox(box));
        usedFonts = it == textBoxDataMap.end() ? 0 : &it->value.first;
        glyphOverflow = it == textBoxDataMap.end() ? 0 : &it->value.second;
    }

    bool includeLeading = includeLeadingForBox(box);
    bool includeFont = includeFontForBox(box);

    bool setUsedFont = false;
    bool setUsedFontWithLeading = false;

    if (usedFonts && !usedFonts->isEmpty() && (includeFont || (box->renderer().style(isFirstLineStyle())->lineHeight().isNegative() && includeLeading))) {
        usedFonts->append(box->renderer().style(isFirstLineStyle())->font().primaryFont());
        for (size_t i = 0; i < usedFonts->size(); ++i) {
            const FontMetrics& fontMetrics = usedFonts->at(i)->fontMetrics();
            int usedFontAscent = fontMetrics.ascent(baselineType());
            int usedFontDescent = fontMetrics.descent(baselineType());
            int halfLeading = (fontMetrics.lineSpacing() - fontMetrics.height()) / 2;
            int usedFontAscentAndLeading = usedFontAscent + halfLeading;
            int usedFontDescentAndLeading = fontMetrics.lineSpacing() - usedFontAscentAndLeading;
            if (includeFont) {
                setAscentAndDescent(ascent, descent, usedFontAscent, usedFontDescent, ascentDescentSet);
                setUsedFont = true;
            }
            if (includeLeading) {
                setAscentAndDescent(ascent, descent, usedFontAscentAndLeading, usedFontDescentAndLeading, ascentDescentSet);
                setUsedFontWithLeading = true;
            }
            if (!affectsAscent)
                affectsAscent = usedFontAscent - box->logicalTop() > 0;
            if (!affectsDescent)
                affectsDescent = usedFontDescent + box->logicalTop() > 0;
        }
    }

    // If leading is included for the box, then we compute that box.
    if (includeLeading && !setUsedFontWithLeading) {
        int ascentWithLeading = box->baselinePosition(baselineType());
        int descentWithLeading = box->lineHeight() - ascentWithLeading;
        setAscentAndDescent(ascent, descent, ascentWithLeading, descentWithLeading, ascentDescentSet);

        // Examine the font box for inline flows and text boxes to see if any part of it is above the baseline.
        // If the top of our font box relative to the root box baseline is above the root box baseline, then
        // we are contributing to the maxAscent value. Descent is similar. If any part of our font box is below
        // the root box's baseline, then we contribute to the maxDescent value.
        affectsAscent = ascentWithLeading - box->logicalTop() > 0;
        affectsDescent = descentWithLeading + box->logicalTop() > 0;
    }

    if (includeFontForBox(box) && !setUsedFont) {
        int fontAscent = box->renderer().style(isFirstLineStyle())->fontMetrics().ascent(baselineType());
        int fontDescent = box->renderer().style(isFirstLineStyle())->fontMetrics().descent(baselineType());
        setAscentAndDescent(ascent, descent, fontAscent, fontDescent, ascentDescentSet);
        affectsAscent = fontAscent - box->logicalTop() > 0;
        affectsDescent = fontDescent + box->logicalTop() > 0;
    }

    if (includeGlyphsForBox(box) && glyphOverflow && glyphOverflow->computeBounds) {
        setAscentAndDescent(ascent, descent, glyphOverflow->top, glyphOverflow->bottom, ascentDescentSet);
        affectsAscent = glyphOverflow->top - box->logicalTop() > 0;
        affectsDescent = glyphOverflow->bottom + box->logicalTop() > 0;
        glyphOverflow->top = min(glyphOverflow->top, max(0, glyphOverflow->top - box->renderer().style(isFirstLineStyle())->fontMetrics().ascent(baselineType())));
        glyphOverflow->bottom = min(glyphOverflow->bottom, max(0, glyphOverflow->bottom - box->renderer().style(isFirstLineStyle())->fontMetrics().descent(baselineType())));
    }

    if (includeMarginForBox(box)) {
        LayoutUnit ascentWithMargin = box->renderer().style(isFirstLineStyle())->fontMetrics().ascent(baselineType());
        LayoutUnit descentWithMargin = box->renderer().style(isFirstLineStyle())->fontMetrics().descent(baselineType());
        if (box->parent() && !box->renderer().isText()) {
            ascentWithMargin += box->boxModelObject()->borderBefore() + box->boxModelObject()->paddingBefore() + box->boxModelObject()->marginBefore();
            descentWithMargin += box->boxModelObject()->borderAfter() + box->boxModelObject()->paddingAfter() + box->boxModelObject()->marginAfter();
        }
        setAscentAndDescent(ascent, descent, ascentWithMargin, descentWithMargin, ascentDescentSet);

        // Treat like a replaced element, since we're using the margin box.
        affectsAscent = true;
        affectsDescent = true;
    }
}

LayoutUnit RootInlineBox::verticalPositionForBox(InlineBox* box, VerticalPositionCache& verticalPositionCache)
{
    if (box->renderer().isText())
        return box->parent()->logicalTop();

    RenderBoxModelObject* renderer = box->boxModelObject();
    ASSERT(renderer->isInline());
    if (!renderer->isInline())
        return 0;

    // This method determines the vertical position for inline elements.
    bool firstLine = isFirstLineStyle();
    if (firstLine && !renderer->document().styleEngine()->usesFirstLineRules())
        firstLine = false;

    // Check the cache.
    bool isRenderInline = renderer->isRenderInline();
    if (isRenderInline && !firstLine) {
        LayoutUnit verticalPosition = verticalPositionCache.get(renderer, baselineType());
        if (verticalPosition != PositionUndefined)
            return verticalPosition;
    }

    LayoutUnit verticalPosition = 0;
    EVerticalAlign verticalAlign = renderer->style()->verticalAlign();
    if (verticalAlign == TOP || verticalAlign == BOTTOM)
        return 0;

    RenderObject* parent = renderer->parent();
    if (parent->isRenderInline() && parent->style()->verticalAlign() != TOP && parent->style()->verticalAlign() != BOTTOM)
        verticalPosition = box->parent()->logicalTop();

    if (verticalAlign != BASELINE) {
        const Font& font = parent->style(firstLine)->font();
        const FontMetrics& fontMetrics = font.fontMetrics();
        int fontSize = font.fontDescription().computedPixelSize();

        LineDirectionMode lineDirection = parent->isHorizontalWritingMode() ? HorizontalLine : VerticalLine;

        if (verticalAlign == SUB)
            verticalPosition += fontSize / 5 + 1;
        else if (verticalAlign == SUPER)
            verticalPosition -= fontSize / 3 + 1;
        else if (verticalAlign == TEXT_TOP)
            verticalPosition += renderer->baselinePosition(baselineType(), firstLine, lineDirection) - fontMetrics.ascent(baselineType());
        else if (verticalAlign == MIDDLE)
            verticalPosition = (verticalPosition - static_cast<LayoutUnit>(fontMetrics.xHeight() / 2) - renderer->lineHeight(firstLine, lineDirection) / 2 + renderer->baselinePosition(baselineType(), firstLine, lineDirection)).round();
        else if (verticalAlign == TEXT_BOTTOM) {
            verticalPosition += fontMetrics.descent(baselineType());
            // lineHeight - baselinePosition is always 0 for replaced elements (except inline blocks), so don't bother wasting time in that case.
            if (!renderer->isReplaced() || renderer->isInlineBlockOrInlineTable())
                verticalPosition -= (renderer->lineHeight(firstLine, lineDirection) - renderer->baselinePosition(baselineType(), firstLine, lineDirection));
        } else if (verticalAlign == BASELINE_MIDDLE)
            verticalPosition += -renderer->lineHeight(firstLine, lineDirection) / 2 + renderer->baselinePosition(baselineType(), firstLine, lineDirection);
        else if (verticalAlign == LENGTH) {
            LayoutUnit lineHeight;
            //Per http://www.w3.org/TR/CSS21/visudet.html#propdef-vertical-align: 'Percentages: refer to the 'line-height' of the element itself'.
            if (renderer->style()->verticalAlignLength().isPercent())
                lineHeight = renderer->style()->computedLineHeight();
            else
                lineHeight = renderer->lineHeight(firstLine, lineDirection);
            verticalPosition -= valueForLength(renderer->style()->verticalAlignLength(), lineHeight);
        }
    }

    // Store the cached value.
    if (isRenderInline && !firstLine)
        verticalPositionCache.set(renderer, baselineType(), verticalPosition);

    return verticalPosition;
}

bool RootInlineBox::includeLeadingForBox(InlineBox* box) const
{
    if (box->renderer().isReplaced() || (box->renderer().isText() && !box->isText()))
        return false;

    LineBoxContain lineBoxContain = renderer().style()->lineBoxContain();
    return (lineBoxContain & LineBoxContainInline) || (box == this && (lineBoxContain & LineBoxContainBlock));
}

bool RootInlineBox::includeFontForBox(InlineBox* box) const
{
    if (box->renderer().isReplaced() || (box->renderer().isText() && !box->isText()))
        return false;

    if (!box->isText() && box->isInlineFlowBox() && !toInlineFlowBox(box)->hasTextChildren())
        return false;

    // For now map "glyphs" to "font" in vertical text mode until the bounds returned by glyphs aren't garbage.
    LineBoxContain lineBoxContain = renderer().style()->lineBoxContain();
    return (lineBoxContain & LineBoxContainFont) || (!isHorizontal() && (lineBoxContain & LineBoxContainGlyphs));
}

bool RootInlineBox::includeGlyphsForBox(InlineBox* box) const
{
    if (box->renderer().isReplaced() || (box->renderer().isText() && !box->isText()))
        return false;

    if (!box->isText() && box->isInlineFlowBox() && !toInlineFlowBox(box)->hasTextChildren())
        return false;

    // FIXME: We can't fit to glyphs yet for vertical text, since the bounds returned are garbage.
    LineBoxContain lineBoxContain = renderer().style()->lineBoxContain();
    return isHorizontal() && (lineBoxContain & LineBoxContainGlyphs);
}

bool RootInlineBox::includeMarginForBox(InlineBox* box) const
{
    if (box->renderer().isReplaced() || (box->renderer().isText() && !box->isText()))
        return false;

    LineBoxContain lineBoxContain = renderer().style()->lineBoxContain();
    return lineBoxContain & LineBoxContainInlineBox;
}


bool RootInlineBox::fitsToGlyphs() const
{
    // FIXME: We can't fit to glyphs yet for vertical text, since the bounds returned are garbage.
    LineBoxContain lineBoxContain = renderer().style()->lineBoxContain();
    return isHorizontal() && (lineBoxContain & LineBoxContainGlyphs);
}

bool RootInlineBox::includesRootLineBoxFontOrLeading() const
{
    LineBoxContain lineBoxContain = renderer().style()->lineBoxContain();
    return (lineBoxContain & LineBoxContainBlock) || (lineBoxContain & LineBoxContainInline) || (lineBoxContain & LineBoxContainFont);
}

Node* RootInlineBox::getLogicalStartBoxWithNode(InlineBox*& startBox) const
{
    Vector<InlineBox*> leafBoxesInLogicalOrder;
    collectLeafBoxesInLogicalOrder(leafBoxesInLogicalOrder);
    for (size_t i = 0; i < leafBoxesInLogicalOrder.size(); ++i) {
        if (leafBoxesInLogicalOrder[i]->renderer().node()) {
            startBox = leafBoxesInLogicalOrder[i];
            return startBox->renderer().node();
        }
    }
    startBox = 0;
    return 0;
}

Node* RootInlineBox::getLogicalEndBoxWithNode(InlineBox*& endBox) const
{
    Vector<InlineBox*> leafBoxesInLogicalOrder;
    collectLeafBoxesInLogicalOrder(leafBoxesInLogicalOrder);
    for (size_t i = leafBoxesInLogicalOrder.size(); i > 0; --i) {
        if (leafBoxesInLogicalOrder[i - 1]->renderer().node()) {
            endBox = leafBoxesInLogicalOrder[i - 1];
            return endBox->renderer().node();
        }
    }
    endBox = 0;
    return 0;
}

#ifndef NDEBUG
const char* RootInlineBox::boxName() const
{
    return "RootInlineBox";
}
#endif

} // namespace WebCore
