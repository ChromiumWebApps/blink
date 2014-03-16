/*
 * (C) 1999 Lars Knoll (knoll@kde.org)
 * (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc. All rights reserved.
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
 *
 */

#include "config.h"
#include "core/rendering/InlineTextBox.h"

#include "core/dom/Document.h"
#include "core/dom/DocumentMarkerController.h"
#include "core/dom/RenderedDocumentMarker.h"
#include "core/dom/Text.h"
#include "core/editing/Editor.h"
#include "core/editing/InputMethodController.h"
#include "core/frame/LocalFrame.h"
#include "core/page/Page.h"
#include "core/frame/Settings.h"
#include "core/rendering/AbstractInlineTextBox.h"
#include "core/rendering/EllipsisBox.h"
#include "core/rendering/HitTestResult.h"
#include "core/rendering/PaintInfo.h"
#include "core/rendering/RenderBR.h"
#include "core/rendering/RenderBlock.h"
#include "core/rendering/RenderCombineText.h"
#include "core/rendering/RenderRubyRun.h"
#include "core/rendering/RenderRubyText.h"
#include "core/rendering/RenderTheme.h"
#include "core/rendering/style/ShadowList.h"
#include "core/rendering/svg/SVGTextRunRenderingContext.h"
#include "platform/fonts/FontCache.h"
#include "platform/fonts/WidthIterator.h"
#include "platform/graphics/DrawLooper.h"
#include "platform/graphics/GraphicsContextStateSaver.h"
#include "wtf/Vector.h"
#include "wtf/text/CString.h"
#include "wtf/text/StringBuilder.h"

using namespace std;

namespace WebCore {

struct SameSizeAsInlineTextBox : public InlineBox {
    unsigned variables[1];
    unsigned short variables2[2];
    void* pointers[2];
};

COMPILE_ASSERT(sizeof(InlineTextBox) == sizeof(SameSizeAsInlineTextBox), InlineTextBox_should_stay_small);

typedef WTF::HashMap<const InlineTextBox*, LayoutRect> InlineTextBoxOverflowMap;
static InlineTextBoxOverflowMap* gTextBoxesWithOverflow;

static const int misspellingLineThickness = 3;

void InlineTextBox::destroy()
{
    AbstractInlineTextBox::willDestroy(this);

    if (!knownToHaveNoOverflow() && gTextBoxesWithOverflow)
        gTextBoxesWithOverflow->remove(this);
    InlineBox::destroy();
}

void InlineTextBox::markDirty(bool dirty)
{
    if (dirty) {
        m_len = 0;
        m_start = 0;
    }
    InlineBox::markDirty(dirty);
}

LayoutRect InlineTextBox::logicalOverflowRect() const
{
    if (knownToHaveNoOverflow() || !gTextBoxesWithOverflow)
        return enclosingIntRect(logicalFrameRect());
    return gTextBoxesWithOverflow->get(this);
}

void InlineTextBox::setLogicalOverflowRect(const LayoutRect& rect)
{
    ASSERT(!knownToHaveNoOverflow());
    if (!gTextBoxesWithOverflow)
        gTextBoxesWithOverflow = new InlineTextBoxOverflowMap;
    gTextBoxesWithOverflow->add(this, rect);
}

int InlineTextBox::baselinePosition(FontBaseline baselineType) const
{
    if (!isText() || !parent())
        return 0;
    if (parent()->renderer() == renderer().parent())
        return parent()->baselinePosition(baselineType);
    return toRenderBoxModelObject(renderer().parent())->baselinePosition(baselineType, isFirstLineStyle(), isHorizontal() ? HorizontalLine : VerticalLine, PositionOnContainingLine);
}

LayoutUnit InlineTextBox::lineHeight() const
{
    if (!isText() || !renderer().parent())
        return 0;
    if (renderer().isBR())
        return toRenderBR(renderer()).lineHeight(isFirstLineStyle());
    if (parent()->renderer() == renderer().parent())
        return parent()->lineHeight();
    return toRenderBoxModelObject(renderer().parent())->lineHeight(isFirstLineStyle(), isHorizontal() ? HorizontalLine : VerticalLine, PositionOnContainingLine);
}

LayoutUnit InlineTextBox::selectionTop()
{
    return root().selectionTop();
}

LayoutUnit InlineTextBox::selectionBottom()
{
    return root().selectionBottom();
}

LayoutUnit InlineTextBox::selectionHeight()
{
    return root().selectionHeight();
}

bool InlineTextBox::isSelected(int startPos, int endPos) const
{
    int sPos = max(startPos - m_start, 0);
    // The position after a hard line break is considered to be past its end.
    // See the corresponding code in InlineTextBox::selectionState.
    int ePos = min(endPos - m_start, int(m_len) + (isLineBreak() ? 0 : 1));
    return (sPos < ePos);
}

RenderObject::SelectionState InlineTextBox::selectionState()
{
    RenderObject::SelectionState state = renderer().selectionState();
    if (state == RenderObject::SelectionStart || state == RenderObject::SelectionEnd || state == RenderObject::SelectionBoth) {
        int startPos, endPos;
        renderer().selectionStartEnd(startPos, endPos);
        // The position after a hard line break is considered to be past its end.
        // See the corresponding code in InlineTextBox::isSelected.
        int lastSelectable = start() + len() - (isLineBreak() ? 1 : 0);

        // FIXME: Remove -webkit-line-break: LineBreakAfterWhiteSpace.
        int endOfLineAdjustmentForCSSLineBreak = renderer().style()->lineBreak() == LineBreakAfterWhiteSpace ? -1 : 0;
        bool start = (state != RenderObject::SelectionEnd && startPos >= m_start && startPos <= m_start + m_len + endOfLineAdjustmentForCSSLineBreak);
        bool end = (state != RenderObject::SelectionStart && endPos > m_start && endPos <= lastSelectable);
        if (start && end)
            state = RenderObject::SelectionBoth;
        else if (start)
            state = RenderObject::SelectionStart;
        else if (end)
            state = RenderObject::SelectionEnd;
        else if ((state == RenderObject::SelectionEnd || startPos < m_start) &&
                 (state == RenderObject::SelectionStart || endPos > lastSelectable))
            state = RenderObject::SelectionInside;
        else if (state == RenderObject::SelectionBoth)
            state = RenderObject::SelectionNone;
    }

    // If there are ellipsis following, make sure their selection is updated.
    if (m_truncation != cNoTruncation && root().ellipsisBox()) {
        EllipsisBox* ellipsis = root().ellipsisBox();
        if (state != RenderObject::SelectionNone) {
            int start, end;
            selectionStartEnd(start, end);
            // The ellipsis should be considered to be selected if the end of
            // the selection is past the beginning of the truncation and the
            // beginning of the selection is before or at the beginning of the
            // truncation.
            ellipsis->setSelectionState(end >= m_truncation && start <= m_truncation ?
                RenderObject::SelectionInside : RenderObject::SelectionNone);
        } else
            ellipsis->setSelectionState(RenderObject::SelectionNone);
    }

    return state;
}

LayoutRect InlineTextBox::localSelectionRect(int startPos, int endPos)
{
    int sPos = max(startPos - m_start, 0);
    int ePos = min(endPos - m_start, (int)m_len);

    if (sPos > ePos)
        return LayoutRect();

    FontCachePurgePreventer fontCachePurgePreventer;

    LayoutUnit selTop = selectionTop();
    LayoutUnit selHeight = selectionHeight();
    RenderStyle* styleToUse = textRenderer().style(isFirstLineStyle());
    const Font& font = styleToUse->font();

    StringBuilder charactersWithHyphen;
    bool respectHyphen = ePos == m_len && hasHyphen();
    TextRun textRun = constructTextRun(styleToUse, font, respectHyphen ? &charactersWithHyphen : 0);

    FloatPoint startingPoint = FloatPoint(logicalLeft(), selTop.toFloat());
    LayoutRect r;
    if (sPos || ePos != static_cast<int>(m_len))
        r = enclosingIntRect(font.selectionRectForText(textRun, startingPoint, selHeight, sPos, ePos));
    else // Avoid computing the font width when the entire line box is selected as an optimization.
        r = enclosingIntRect(FloatRect(startingPoint, FloatSize(m_logicalWidth, selHeight.toFloat())));

    LayoutUnit logicalWidth = r.width();
    if (r.x() > logicalRight())
        logicalWidth  = 0;
    else if (r.maxX() > logicalRight())
        logicalWidth = logicalRight() - r.x();

    LayoutPoint topPoint = isHorizontal() ? LayoutPoint(r.x(), selTop) : LayoutPoint(selTop, r.x());
    LayoutUnit width = isHorizontal() ? logicalWidth : selHeight;
    LayoutUnit height = isHorizontal() ? selHeight : logicalWidth;

    return LayoutRect(topPoint, LayoutSize(width, height));
}

void InlineTextBox::deleteLine()
{
    toRenderText(renderer()).removeTextBox(this);
    destroy();
}

void InlineTextBox::extractLine()
{
    if (extracted())
        return;

    toRenderText(renderer()).extractTextBox(this);
}

void InlineTextBox::attachLine()
{
    if (!extracted())
        return;

    toRenderText(renderer()).attachTextBox(this);
}

float InlineTextBox::placeEllipsisBox(bool flowIsLTR, float visibleLeftEdge, float visibleRightEdge, float ellipsisWidth, float &truncatedWidth, bool& foundBox)
{
    if (foundBox) {
        m_truncation = cFullTruncation;
        return -1;
    }

    // For LTR this is the left edge of the box, for RTL, the right edge in parent coordinates.
    float ellipsisX = flowIsLTR ? visibleRightEdge - ellipsisWidth : visibleLeftEdge + ellipsisWidth;

    // Criteria for full truncation:
    // LTR: the left edge of the ellipsis is to the left of our text run.
    // RTL: the right edge of the ellipsis is to the right of our text run.
    bool ltrFullTruncation = flowIsLTR && ellipsisX <= logicalLeft();
    bool rtlFullTruncation = !flowIsLTR && ellipsisX >= logicalLeft() + logicalWidth();
    if (ltrFullTruncation || rtlFullTruncation) {
        // Too far.  Just set full truncation, but return -1 and let the ellipsis just be placed at the edge of the box.
        m_truncation = cFullTruncation;
        foundBox = true;
        return -1;
    }

    bool ltrEllipsisWithinBox = flowIsLTR && (ellipsisX < logicalRight());
    bool rtlEllipsisWithinBox = !flowIsLTR && (ellipsisX > logicalLeft());
    if (ltrEllipsisWithinBox || rtlEllipsisWithinBox) {
        foundBox = true;

        // The inline box may have different directionality than it's parent.  Since truncation
        // behavior depends both on both the parent and the inline block's directionality, we
        // must keep track of these separately.
        bool ltr = isLeftToRightDirection();
        if (ltr != flowIsLTR) {
            // Width in pixels of the visible portion of the box, excluding the ellipsis.
            int visibleBoxWidth = visibleRightEdge - visibleLeftEdge  - ellipsisWidth;
            ellipsisX = ltr ? logicalLeft() + visibleBoxWidth : logicalRight() - visibleBoxWidth;
        }

        int offset = offsetForPosition(ellipsisX, false);
        if (offset == 0) {
            // No characters should be rendered.  Set ourselves to full truncation and place the ellipsis at the min of our start
            // and the ellipsis edge.
            m_truncation = cFullTruncation;
            truncatedWidth += ellipsisWidth;
            return min(ellipsisX, logicalLeft());
        }

        // Set the truncation index on the text run.
        m_truncation = offset;

        // If we got here that means that we were only partially truncated and we need to return the pixel offset at which
        // to place the ellipsis.
        float widthOfVisibleText = toRenderText(renderer()).width(m_start, offset, textPos(), flowIsLTR ? LTR : RTL, isFirstLineStyle());

        // The ellipsis needs to be placed just after the last visible character.
        // Where "after" is defined by the flow directionality, not the inline
        // box directionality.
        // e.g. In the case of an LTR inline box truncated in an RTL flow then we can
        // have a situation such as |Hello| -> |...He|
        truncatedWidth += widthOfVisibleText + ellipsisWidth;
        if (flowIsLTR)
            return logicalLeft() + widthOfVisibleText;
        else
            return logicalRight() - widthOfVisibleText - ellipsisWidth;
    }
    truncatedWidth += logicalWidth();
    return -1;
}

Color correctedTextColor(Color textColor, Color backgroundColor)
{
    // Adjust the text color if it is too close to the background color,
    // by darkening or lightening it to move it further away.

    int d = differenceSquared(textColor, backgroundColor);
    // semi-arbitrarily chose 65025 (255^2) value here after a few tests;
    if (d > 65025) {
        return textColor;
    }

    int distanceFromWhite = differenceSquared(textColor, Color::white);
    int distanceFromBlack = differenceSquared(textColor, Color::black);

    if (distanceFromWhite < distanceFromBlack) {
        return textColor.dark();
    }

    return textColor.light();
}

void updateGraphicsContext(GraphicsContext* context, const Color& fillColor, const Color& strokeColor, float strokeThickness)
{
    TextDrawingModeFlags mode = context->textDrawingMode();
    if (strokeThickness > 0) {
        TextDrawingModeFlags newMode = mode | TextModeStroke;
        if (mode != newMode) {
            context->setTextDrawingMode(newMode);
            mode = newMode;
        }
    }

    if (mode & TextModeFill && fillColor != context->fillColor())
        context->setFillColor(fillColor);

    if (mode & TextModeStroke) {
        if (strokeColor != context->strokeColor())
            context->setStrokeColor(strokeColor);
        if (strokeThickness != context->strokeThickness())
            context->setStrokeThickness(strokeThickness);
    }
}

bool InlineTextBox::isLineBreak() const
{
    return renderer().isBR() || (renderer().style()->preserveNewline() && len() == 1 && (*textRenderer().text().impl())[start()] == '\n');
}

bool InlineTextBox::nodeAtPoint(const HitTestRequest& request, HitTestResult& result, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, LayoutUnit /* lineTop */, LayoutUnit /*lineBottom*/)
{
    if (isLineBreak())
        return false;

    FloatPoint boxOrigin = locationIncludingFlipping();
    boxOrigin.moveBy(accumulatedOffset);
    FloatRect rect(boxOrigin, size());
    if (m_truncation != cFullTruncation && visibleToHitTestRequest(request) && locationInContainer.intersects(rect)) {
        renderer().updateHitTestResult(result, flipForWritingMode(locationInContainer.point() - toLayoutSize(accumulatedOffset)));
        if (!result.addNodeToRectBasedTestResult(renderer().node(), request, locationInContainer, rect))
            return true;
    }
    return false;
}

static void paintTextWithShadows(GraphicsContext* context,
    const RenderObject& renderer, const Font& font, const TextRun& textRun,
    const AtomicString& emphasisMark, int emphasisMarkOffset,
    int startOffset, int endOffset, int truncationPoint,
    const FloatPoint& textOrigin, const FloatRect& boxRect,
    const ShadowList* shadowList, bool stroked, bool horizontal)
{
    // Text shadows are disabled when printing. http://crbug.com/258321
    bool hasShadow = shadowList && !context->printing();

    if (hasShadow) {
        DrawLooper drawLooper;
        for (size_t i = shadowList->shadows().size(); i--; ) {
            const ShadowData& shadow = shadowList->shadows()[i];
            float shadowX = horizontal ? shadow.x() : shadow.y();
            float shadowY = horizontal ? shadow.y() : -shadow.x();
            FloatSize offset(shadowX, shadowY);
            drawLooper.addShadow(offset, shadow.blur(), shadow.color(),
                DrawLooper::ShadowRespectsTransforms, DrawLooper::ShadowIgnoresAlpha);
        }
        drawLooper.addUnmodifiedContent();
        context->setDrawLooper(drawLooper);
    }

    TextRunPaintInfo textRunPaintInfo(textRun);
    textRunPaintInfo.bounds = boxRect;
    if (startOffset <= endOffset) {
        textRunPaintInfo.from = startOffset;
        textRunPaintInfo.to = endOffset;
        if (emphasisMark.isEmpty())
            context->drawText(font, textRunPaintInfo, textOrigin);
        else
            context->drawEmphasisMarks(font, textRunPaintInfo, emphasisMark, textOrigin + IntSize(0, emphasisMarkOffset));
    } else {
        if (endOffset > 0) {
            textRunPaintInfo.from = 0;
            textRunPaintInfo.to = endOffset;
            if (emphasisMark.isEmpty())
                context->drawText(font, textRunPaintInfo, textOrigin);
            else
                context->drawEmphasisMarks(font, textRunPaintInfo, emphasisMark, textOrigin + IntSize(0, emphasisMarkOffset));
        }
        if (startOffset < truncationPoint) {
            textRunPaintInfo.from = startOffset;
            textRunPaintInfo.to = truncationPoint;
            if (emphasisMark.isEmpty())
                context->drawText(font, textRunPaintInfo, textOrigin);
            else
                context->drawEmphasisMarks(font, textRunPaintInfo, emphasisMark, textOrigin + IntSize(0, emphasisMarkOffset));
        }
    }

    if (hasShadow)
        context->clearDrawLooper();
}

bool InlineTextBox::getEmphasisMarkPosition(RenderStyle* style, TextEmphasisPosition& emphasisPosition) const
{
    // This function returns true if there are text emphasis marks and they are suppressed by ruby text.
    if (style->textEmphasisMark() == TextEmphasisMarkNone)
        return false;

    emphasisPosition = style->textEmphasisPosition();
    if (emphasisPosition == TextEmphasisPositionUnder)
        return true; // Ruby text is always over, so it cannot suppress emphasis marks under.

    RenderBlock* containingBlock = renderer().containingBlock();
    if (!containingBlock->isRubyBase())
        return true; // This text is not inside a ruby base, so it does not have ruby text over it.

    if (!containingBlock->parent()->isRubyRun())
        return true; // Cannot get the ruby text.

    RenderRubyText* rubyText = toRenderRubyRun(containingBlock->parent())->rubyText();

    // The emphasis marks over are suppressed only if there is a ruby text box and it not empty.
    return !rubyText || !rubyText->firstLineBox();
}

void InlineTextBox::paint(PaintInfo& paintInfo, const LayoutPoint& paintOffset, LayoutUnit /*lineTop*/, LayoutUnit /*lineBottom*/)
{
    if (isLineBreak() || !paintInfo.shouldPaintWithinRoot(&renderer()) || renderer().style()->visibility() != VISIBLE
        || m_truncation == cFullTruncation || paintInfo.phase == PaintPhaseOutline || !m_len)
        return;

    ASSERT(paintInfo.phase != PaintPhaseSelfOutline && paintInfo.phase != PaintPhaseChildOutlines);

    LayoutUnit logicalLeftSide = logicalLeftVisualOverflow();
    LayoutUnit logicalRightSide = logicalRightVisualOverflow();
    LayoutUnit logicalStart = logicalLeftSide + (isHorizontal() ? paintOffset.x() : paintOffset.y());
    LayoutUnit logicalExtent = logicalRightSide - logicalLeftSide;

    LayoutUnit paintEnd = isHorizontal() ? paintInfo.rect.maxX() : paintInfo.rect.maxY();
    LayoutUnit paintStart = isHorizontal() ? paintInfo.rect.x() : paintInfo.rect.y();

    LayoutPoint adjustedPaintOffset = roundedIntPoint(paintOffset);

    if (logicalStart >= paintEnd || logicalStart + logicalExtent <= paintStart)
        return;

    bool isPrinting = textRenderer().document().printing();

    // Determine whether or not we're selected.
    bool haveSelection = !isPrinting && paintInfo.phase != PaintPhaseTextClip && selectionState() != RenderObject::SelectionNone;
    if (!haveSelection && paintInfo.phase == PaintPhaseSelection)
        // When only painting the selection, don't bother to paint if there is none.
        return;

    if (m_truncation != cNoTruncation) {
        if (renderer().containingBlock()->style()->isLeftToRightDirection() != isLeftToRightDirection()) {
            // Make the visible fragment of text hug the edge closest to the rest of the run by moving the origin
            // at which we start drawing text.
            // e.g. In the case of LTR text truncated in an RTL Context, the correct behavior is:
            // |Hello|CBA| -> |...He|CBA|
            // In order to draw the fragment "He" aligned to the right edge of it's box, we need to start drawing
            // farther to the right.
            // NOTE: WebKit's behavior differs from that of IE which appears to just overlay the ellipsis on top of the
            // truncated string i.e.  |Hello|CBA| -> |...lo|CBA|
            LayoutUnit widthOfVisibleText = toRenderText(renderer()).width(m_start, m_truncation, textPos(), isLeftToRightDirection() ? LTR : RTL, isFirstLineStyle());
            LayoutUnit widthOfHiddenText = m_logicalWidth - widthOfVisibleText;
            // FIXME: The hit testing logic also needs to take this translation into account.
            LayoutSize truncationOffset(isLeftToRightDirection() ? widthOfHiddenText : -widthOfHiddenText, 0);
            adjustedPaintOffset.move(isHorizontal() ? truncationOffset : truncationOffset.transposedSize());
        }
    }

    GraphicsContext* context = paintInfo.context;

    RenderObject& rendererToUse = renderer();
    RenderStyle* styleToUse = rendererToUse.style(isFirstLineStyle());

    adjustedPaintOffset.move(0, styleToUse->isHorizontalWritingMode() ? 0 : -logicalHeight());

    FloatPoint boxOrigin = locationIncludingFlipping();
    // FIXME: Shouldn't these offsets be rounded?
    boxOrigin.move(adjustedPaintOffset.x().toFloat(), adjustedPaintOffset.y().toFloat());
    FloatRect boxRect(boxOrigin, LayoutSize(logicalWidth(), logicalHeight()));

    RenderCombineText* combinedText = styleToUse->hasTextCombine() && textRenderer().isCombineText() && toRenderCombineText(textRenderer()).isCombined() ? &toRenderCombineText(textRenderer()) : 0;

    bool shouldRotate = !isHorizontal() && !combinedText;
    if (shouldRotate)
        context->concatCTM(rotation(boxRect, Clockwise));

    // Determine whether or not we have composition underlines to draw.
    bool containsComposition = renderer().node() && renderer().frame()->inputMethodController().compositionNode() == renderer().node();
    bool useCustomUnderlines = containsComposition && renderer().frame()->inputMethodController().compositionUsesCustomUnderlines();

    // Determine the text colors and selection colors.
    Color textFillColor;
    Color textStrokeColor;
    Color emphasisMarkColor;
    float textStrokeWidth = styleToUse->textStrokeWidth();

    // Text shadows are disabled when printing. http://crbug.com/258321
    const ShadowList* textShadow = (context->printing() || paintInfo.forceBlackText()) ? 0 : styleToUse->textShadow();

    if (paintInfo.forceBlackText()) {
        textFillColor = Color::black;
        textStrokeColor = Color::black;
        emphasisMarkColor = Color::black;
    } else {
        textFillColor = rendererToUse.resolveColor(styleToUse, CSSPropertyWebkitTextFillColor);

        bool forceBackgroundToWhite = false;
        if (isPrinting) {
            if (styleToUse->printColorAdjust() == PrintColorAdjustEconomy)
                forceBackgroundToWhite = true;
            if (textRenderer().document().settings() && textRenderer().document().settings()->shouldPrintBackgrounds())
                forceBackgroundToWhite = false;
        }

        // Make the text fill color legible against a white background
        if (forceBackgroundToWhite)
            textFillColor = correctedTextColor(textFillColor, Color::white);

        textStrokeColor = rendererToUse.resolveColor(styleToUse, CSSPropertyWebkitTextStrokeColor);

        // Make the text stroke color legible against a white background
        if (forceBackgroundToWhite)
            textStrokeColor = correctedTextColor(textStrokeColor, Color::white);

        emphasisMarkColor = rendererToUse.resolveColor(styleToUse, CSSPropertyWebkitTextEmphasisColor);

        // Make the text stroke color legible against a white background
        if (forceBackgroundToWhite)
            emphasisMarkColor = correctedTextColor(emphasisMarkColor, Color::white);
    }

    bool paintSelectedTextOnly = (paintInfo.phase == PaintPhaseSelection);
    bool paintSelectedTextSeparately = false;

    Color selectionFillColor = textFillColor;
    Color selectionStrokeColor = textStrokeColor;
    Color selectionEmphasisMarkColor = emphasisMarkColor;
    float selectionStrokeWidth = textStrokeWidth;
    const ShadowList* selectionShadow = textShadow;
    if (haveSelection) {
        // Check foreground color first.
        Color foreground = paintInfo.forceBlackText() ? Color::black : renderer().selectionForegroundColor();
        if (foreground != selectionFillColor) {
            if (!paintSelectedTextOnly)
                paintSelectedTextSeparately = true;
            selectionFillColor = foreground;
        }

        Color emphasisMarkForeground = paintInfo.forceBlackText() ? Color::black : renderer().selectionEmphasisMarkColor();
        if (emphasisMarkForeground != selectionEmphasisMarkColor) {
            if (!paintSelectedTextOnly)
                paintSelectedTextSeparately = true;
            selectionEmphasisMarkColor = emphasisMarkForeground;
        }

        if (RenderStyle* pseudoStyle = renderer().getCachedPseudoStyle(SELECTION)) {
            // Text shadows are disabled when printing. http://crbug.com/258321
            const ShadowList* shadow = (context->printing() || paintInfo.forceBlackText()) ? 0 : pseudoStyle->textShadow();
            if (shadow != selectionShadow) {
                if (!paintSelectedTextOnly)
                    paintSelectedTextSeparately = true;
                selectionShadow = shadow;
            }

            float strokeWidth = pseudoStyle->textStrokeWidth();
            if (strokeWidth != selectionStrokeWidth) {
                if (!paintSelectedTextOnly)
                    paintSelectedTextSeparately = true;
                selectionStrokeWidth = strokeWidth;
            }

            Color stroke = paintInfo.forceBlackText() ? Color::black : rendererToUse.resolveColor(pseudoStyle, CSSPropertyWebkitTextStrokeColor);
            if (stroke != selectionStrokeColor) {
                if (!paintSelectedTextOnly)
                    paintSelectedTextSeparately = true;
                selectionStrokeColor = stroke;
            }
        }
    }

    // Set our font.
    const Font& font = styleToUse->font();

    FloatPoint textOrigin = FloatPoint(boxOrigin.x(), boxOrigin.y() + font.fontMetrics().ascent());

    if (combinedText)
        combinedText->adjustTextOrigin(textOrigin, boxRect);

    // 1. Paint backgrounds behind text if needed. Examples of such backgrounds include selection
    // and composition underlines.
    if (paintInfo.phase != PaintPhaseSelection && paintInfo.phase != PaintPhaseTextClip && !isPrinting) {

        if (containsComposition && !useCustomUnderlines) {
            paintCompositionBackground(context, boxOrigin, styleToUse, font,
                renderer().frame()->inputMethodController().compositionStart(),
                renderer().frame()->inputMethodController().compositionEnd());
        }

        paintDocumentMarkers(context, boxOrigin, styleToUse, font, true);

        if (haveSelection && !useCustomUnderlines)
            paintSelection(context, boxOrigin, styleToUse, font, selectionFillColor);
    }

    // 2. Now paint the foreground, including text and decorations like underline/overline (in quirks mode only).
    int length = m_len;
    int maximumLength;
    StringView string;
    if (!combinedText) {
        string = textRenderer().text().createView();
        if (static_cast<unsigned>(length) != string.length() || m_start)
            string.narrow(m_start, length);
        maximumLength = textRenderer().textLength() - m_start;
    } else {
        combinedText->getStringToRender(m_start, string, length);
        maximumLength = length;
    }

    StringBuilder charactersWithHyphen;
    TextRun textRun = constructTextRun(styleToUse, font, string, maximumLength, hasHyphen() ? &charactersWithHyphen : 0);
    if (hasHyphen())
        length = textRun.length();

    int sPos = 0;
    int ePos = 0;
    if (paintSelectedTextOnly || paintSelectedTextSeparately)
        selectionStartEnd(sPos, ePos);

    if (m_truncation != cNoTruncation) {
        sPos = min<int>(sPos, m_truncation);
        ePos = min<int>(ePos, m_truncation);
        length = m_truncation;
    }

    int emphasisMarkOffset = 0;
    TextEmphasisPosition emphasisMarkPosition;
    bool hasTextEmphasis = getEmphasisMarkPosition(styleToUse, emphasisMarkPosition);
    const AtomicString& emphasisMark = hasTextEmphasis ? styleToUse->textEmphasisMarkString() : nullAtom;
    if (!emphasisMark.isEmpty())
        emphasisMarkOffset = emphasisMarkPosition == TextEmphasisPositionOver ? -font.fontMetrics().ascent() - font.emphasisMarkDescent(emphasisMark) : font.fontMetrics().descent() + font.emphasisMarkAscent(emphasisMark);

    if (!paintSelectedTextOnly) {
        // For stroked painting, we have to change the text drawing mode.  It's probably dangerous to leave that mutated as a side
        // effect, so only when we know we're stroking, do a save/restore.
        GraphicsContextStateSaver stateSaver(*context, textStrokeWidth > 0);

        updateGraphicsContext(context, textFillColor, textStrokeColor, textStrokeWidth);
        if (!paintSelectedTextSeparately || ePos <= sPos) {
            // FIXME: Truncate right-to-left text correctly.
            paintTextWithShadows(context, rendererToUse, font, textRun, nullAtom, 0, 0, length, length, textOrigin, boxRect, textShadow, textStrokeWidth > 0, isHorizontal());
        } else {
            paintTextWithShadows(context, rendererToUse, font, textRun, nullAtom, 0, ePos, sPos, length, textOrigin, boxRect, textShadow, textStrokeWidth > 0, isHorizontal());
        }

        if (!emphasisMark.isEmpty()) {
            updateGraphicsContext(context, emphasisMarkColor, textStrokeColor, textStrokeWidth);

            DEFINE_STATIC_LOCAL(TextRun, objectReplacementCharacterTextRun, (&objectReplacementCharacter, 1));
            TextRun& emphasisMarkTextRun = combinedText ? objectReplacementCharacterTextRun : textRun;
            FloatPoint emphasisMarkTextOrigin = combinedText ? FloatPoint(boxOrigin.x() + boxRect.width() / 2, boxOrigin.y() + font.fontMetrics().ascent()) : textOrigin;
            if (combinedText)
                context->concatCTM(rotation(boxRect, Clockwise));

            int startOffset = 0;
            int endOffset = length;
            int paintRunLength = length;
            if (combinedText) {
                startOffset = 0;
                endOffset = objectReplacementCharacterTextRun.length();
                paintRunLength = endOffset;
            } else if (paintSelectedTextSeparately && ePos > sPos) {
                startOffset = ePos;
                endOffset = sPos;
            }
            // FIXME: Truncate right-to-left text correctly.
            paintTextWithShadows(context, rendererToUse, combinedText ? combinedText->originalFont() : font, emphasisMarkTextRun, emphasisMark, emphasisMarkOffset, startOffset, endOffset, paintRunLength, emphasisMarkTextOrigin, boxRect, textShadow, textStrokeWidth > 0, isHorizontal());

            if (combinedText)
                context->concatCTM(rotation(boxRect, Counterclockwise));
        }
    }

    if ((paintSelectedTextOnly || paintSelectedTextSeparately) && sPos < ePos) {
        // paint only the text that is selected
        GraphicsContextStateSaver stateSaver(*context, selectionStrokeWidth > 0);

        updateGraphicsContext(context, selectionFillColor, selectionStrokeColor, selectionStrokeWidth);
        paintTextWithShadows(context, rendererToUse, font, textRun, nullAtom, 0, sPos, ePos, length, textOrigin, boxRect, selectionShadow, selectionStrokeWidth > 0, isHorizontal());
        if (!emphasisMark.isEmpty()) {
            updateGraphicsContext(context, selectionEmphasisMarkColor, textStrokeColor, textStrokeWidth);

            DEFINE_STATIC_LOCAL(TextRun, objectReplacementCharacterTextRun, (&objectReplacementCharacter, 1));
            TextRun& emphasisMarkTextRun = combinedText ? objectReplacementCharacterTextRun : textRun;
            FloatPoint emphasisMarkTextOrigin = combinedText ? FloatPoint(boxOrigin.x() + boxRect.width() / 2, boxOrigin.y() + font.fontMetrics().ascent()) : textOrigin;
            if (combinedText)
                context->concatCTM(rotation(boxRect, Clockwise));

            int startOffset = combinedText ? 0 : sPos;
            int endOffset = combinedText ? objectReplacementCharacterTextRun.length() : ePos;
            int paintRunLength = combinedText ? endOffset : length;
            paintTextWithShadows(context, rendererToUse, combinedText ? combinedText->originalFont() : font, emphasisMarkTextRun, emphasisMark, emphasisMarkOffset, startOffset, endOffset, paintRunLength, emphasisMarkTextOrigin, boxRect, selectionShadow, selectionStrokeWidth > 0, isHorizontal());

            if (combinedText)
                context->concatCTM(rotation(boxRect, Counterclockwise));
        }
    }

    // Paint decorations
    TextDecoration textDecorations = styleToUse->textDecorationsInEffect();
    if (textDecorations != TextDecorationNone && paintInfo.phase != PaintPhaseSelection) {
        updateGraphicsContext(context, textFillColor, textStrokeColor, textStrokeWidth);
        if (combinedText)
            context->concatCTM(rotation(boxRect, Clockwise));
        paintDecoration(context, boxOrigin, textDecorations, textShadow);
        if (combinedText)
            context->concatCTM(rotation(boxRect, Counterclockwise));
    }

    if (paintInfo.phase == PaintPhaseForeground) {
        paintDocumentMarkers(context, boxOrigin, styleToUse, font, false);

        if (useCustomUnderlines) {
            const Vector<CompositionUnderline>& underlines = renderer().frame()->inputMethodController().customCompositionUnderlines();
            size_t numUnderlines = underlines.size();

            for (size_t index = 0; index < numUnderlines; ++index) {
                const CompositionUnderline& underline = underlines[index];

                if (underline.endOffset <= start())
                    // underline is completely before this run.  This might be an underline that sits
                    // before the first run we draw, or underlines that were within runs we skipped
                    // due to truncation.
                    continue;

                if (underline.startOffset <= end()) {
                    // underline intersects this run.  Paint it.
                    paintCompositionUnderline(context, boxOrigin, underline);
                    if (underline.endOffset > end() + 1)
                        // underline also runs into the next run. Bail now, no more marker advancement.
                        break;
                } else
                    // underline is completely after this run, bail.  A later run will paint it.
                    break;
            }
        }
    }

    if (shouldRotate)
        context->concatCTM(rotation(boxRect, Counterclockwise));
}

void InlineTextBox::selectionStartEnd(int& sPos, int& ePos)
{
    int startPos, endPos;
    if (renderer().selectionState() == RenderObject::SelectionInside) {
        startPos = 0;
        endPos = textRenderer().textLength();
    } else {
        textRenderer().selectionStartEnd(startPos, endPos);
        if (renderer().selectionState() == RenderObject::SelectionStart)
            endPos = textRenderer().textLength();
        else if (renderer().selectionState() == RenderObject::SelectionEnd)
            startPos = 0;
    }

    sPos = max(startPos - m_start, 0);
    ePos = min(endPos - m_start, (int)m_len);
}

void alignSelectionRectToDevicePixels(FloatRect& rect)
{
    float maxX = floorf(rect.maxX());
    rect.setX(floorf(rect.x()));
    rect.setWidth(roundf(maxX - rect.x()));
}

void InlineTextBox::paintSelection(GraphicsContext* context, const FloatPoint& boxOrigin, RenderStyle* style, const Font& font, Color textColor)
{
    if (context->paintingDisabled())
        return;

    // See if we have a selection to paint at all.
    int sPos, ePos;
    selectionStartEnd(sPos, ePos);
    if (sPos >= ePos)
        return;

    Color c = renderer().selectionBackgroundColor();
    if (!c.alpha())
        return;

    // If the text color ends up being the same as the selection background, invert the selection
    // background.
    if (textColor == c)
        c = Color(0xff - c.red(), 0xff - c.green(), 0xff - c.blue());

    GraphicsContextStateSaver stateSaver(*context);
    updateGraphicsContext(context, c, c, 0); // Don't draw text at all!

    // If the text is truncated, let the thing being painted in the truncation
    // draw its own highlight.
    int length = m_truncation != cNoTruncation ? m_truncation : m_len;
    StringView string = textRenderer().text().createView();

    if (string.length() != static_cast<unsigned>(length) || m_start)
        string.narrow(m_start, length);

    StringBuilder charactersWithHyphen;
    bool respectHyphen = ePos == length && hasHyphen();
    TextRun textRun = constructTextRun(style, font, string, textRenderer().textLength() - m_start, respectHyphen ? &charactersWithHyphen : 0);
    if (respectHyphen)
        ePos = textRun.length();

    LayoutUnit selectionBottom = root().selectionBottom();
    LayoutUnit selectionTop = root().selectionTopAdjustedForPrecedingBlock();

    int deltaY = roundToInt(renderer().style()->isFlippedLinesWritingMode() ? selectionBottom - logicalBottom() : logicalTop() - selectionTop);
    int selHeight = max(0, roundToInt(selectionBottom - selectionTop));

    FloatPoint localOrigin(boxOrigin.x(), boxOrigin.y() - deltaY);
    FloatRect clipRect(localOrigin, FloatSize(m_logicalWidth, selHeight));
    alignSelectionRectToDevicePixels(clipRect);

    context->clip(clipRect);

    context->drawHighlightForText(font, textRun, localOrigin, selHeight, c, sPos, ePos);
}

void InlineTextBox::paintCompositionBackground(GraphicsContext* context, const FloatPoint& boxOrigin, RenderStyle* style, const Font& font, int startPos, int endPos)
{
    int offset = m_start;
    int sPos = max(startPos - offset, 0);
    int ePos = min(endPos - offset, (int)m_len);

    if (sPos >= ePos)
        return;

    GraphicsContextStateSaver stateSaver(*context);

    Color c = Color(225, 221, 85);

    updateGraphicsContext(context, c, c, 0); // Don't draw text at all!

    int deltaY = renderer().style()->isFlippedLinesWritingMode() ? selectionBottom() - logicalBottom() : logicalTop() - selectionTop();
    int selHeight = selectionHeight();
    FloatPoint localOrigin(boxOrigin.x(), boxOrigin.y() - deltaY);
    context->drawHighlightForText(font, constructTextRun(style, font), localOrigin, selHeight, c, sPos, ePos);
}

static StrokeStyle textDecorationStyleToStrokeStyle(TextDecorationStyle decorationStyle)
{
    StrokeStyle strokeStyle = SolidStroke;
    switch (decorationStyle) {
    case TextDecorationStyleSolid:
        strokeStyle = SolidStroke;
        break;
    case TextDecorationStyleDouble:
        strokeStyle = DoubleStroke;
        break;
    case TextDecorationStyleDotted:
        strokeStyle = DottedStroke;
        break;
    case TextDecorationStyleDashed:
        strokeStyle = DashedStroke;
        break;
    case TextDecorationStyleWavy:
        strokeStyle = WavyStroke;
        break;
    }

    return strokeStyle;
}

static int computeUnderlineOffset(const TextUnderlinePosition underlinePosition, const FontMetrics& fontMetrics, const InlineTextBox* inlineTextBox, const float textDecorationThickness)
{
    // Compute the gap between the font and the underline. Use at least one
    // pixel gap, if underline is thick then use a bigger gap.
    const int gap = std::max<int>(1, ceilf(textDecorationThickness / 2.f));

    // FIXME: We support only horizontal text for now.
    switch (underlinePosition) {
    case TextUnderlinePositionAuto:
        return fontMetrics.ascent() + gap; // Position underline near the alphabetic baseline.
    case TextUnderlinePositionUnder: {
        // Position underline relative to the under edge of the lowest element's content box.
        const float offset = inlineTextBox->root().maxLogicalTop() - inlineTextBox->logicalTop();
        if (offset > 0)
            return inlineTextBox->logicalHeight() + gap + offset;
        return inlineTextBox->logicalHeight() + gap;
    }
    }

    ASSERT_NOT_REACHED();
    return fontMetrics.ascent() + gap;
}

static void adjustStepToDecorationLength(float& step, float& controlPointDistance, float length)
{
    ASSERT(step > 0);

    if (length <= 0)
        return;

    unsigned stepCount = static_cast<unsigned>(length / step);

    // Each Bezier curve starts at the same pixel that the previous one
    // ended. We need to subtract (stepCount - 1) pixels when calculating the
    // length covered to account for that.
    float uncoveredLength = length - (stepCount * step - (stepCount - 1));
    float adjustment = uncoveredLength / stepCount;
    step += adjustment;
    controlPointDistance += adjustment;
}

/*
 * Draw one cubic Bezier curve and repeat the same pattern long the the decoration's axis.
 * The start point (p1), controlPoint1, controlPoint2 and end point (p2) of the Bezier curve
 * form a diamond shape:
 *
 *                              step
 *                         |-----------|
 *
 *                   controlPoint1
 *                         +
 *
 *
 *                  . .
 *                .     .
 *              .         .
 * (x1, y1) p1 +           .            + p2 (x2, y2) - <--- Decoration's axis
 *                          .         .               |
 *                            .     .                 |
 *                              . .                   | controlPointDistance
 *                                                    |
 *                                                    |
 *                         +                          -
 *                   controlPoint2
 *
 *             |-----------|
 *                 step
 */
static void strokeWavyTextDecoration(GraphicsContext* context, FloatPoint p1, FloatPoint p2, float strokeThickness)
{
    context->adjustLineToPixelBoundaries(p1, p2, strokeThickness, context->strokeStyle());

    Path path;
    path.moveTo(p1);

    // Distance between decoration's axis and Bezier curve's control points.
    // The height of the curve is based on this distance. Use a minimum of 6 pixels distance since
    // the actual curve passes approximately at half of that distance, that is 3 pixels.
    // The minimum height of the curve is also approximately 3 pixels. Increases the curve's height
    // as strockThickness increases to make the curve looks better.
    float controlPointDistance = 3 * max<float>(2, strokeThickness);

    // Increment used to form the diamond shape between start point (p1), control
    // points and end point (p2) along the axis of the decoration. Makes the
    // curve wider as strockThickness increases to make the curve looks better.
    float step = 2 * max<float>(2, strokeThickness);

    bool isVerticalLine = (p1.x() == p2.x());

    if (isVerticalLine) {
        ASSERT(p1.x() == p2.x());

        float xAxis = p1.x();
        float y1;
        float y2;

        if (p1.y() < p2.y()) {
            y1 = p1.y();
            y2 = p2.y();
        } else {
            y1 = p2.y();
            y2 = p1.y();
        }

        adjustStepToDecorationLength(step, controlPointDistance, y2 - y1);
        FloatPoint controlPoint1(xAxis + controlPointDistance, 0);
        FloatPoint controlPoint2(xAxis - controlPointDistance, 0);

        for (float y = y1; y + 2 * step <= y2;) {
            controlPoint1.setY(y + step);
            controlPoint2.setY(y + step);
            y += 2 * step;
            path.addBezierCurveTo(controlPoint1, controlPoint2, FloatPoint(xAxis, y));
        }
    } else {
        ASSERT(p1.y() == p2.y());

        float yAxis = p1.y();
        float x1;
        float x2;

        if (p1.x() < p2.x()) {
            x1 = p1.x();
            x2 = p2.x();
        } else {
            x1 = p2.x();
            x2 = p1.x();
        }

        adjustStepToDecorationLength(step, controlPointDistance, x2 - x1);
        FloatPoint controlPoint1(0, yAxis + controlPointDistance);
        FloatPoint controlPoint2(0, yAxis - controlPointDistance);

        for (float x = x1; x + 2 * step <= x2;) {
            controlPoint1.setX(x + step);
            controlPoint2.setX(x + step);
            x += 2 * step;
            path.addBezierCurveTo(controlPoint1, controlPoint2, FloatPoint(x, yAxis));
        }
    }

    context->setShouldAntialias(true);
    context->strokePath(path);
}

static bool shouldSetDecorationAntialias(TextDecorationStyle decorationStyle)
{
    return decorationStyle == TextDecorationStyleDotted || decorationStyle == TextDecorationStyleDashed;
}

static bool shouldSetDecorationAntialias(TextDecorationStyle underline, TextDecorationStyle overline, TextDecorationStyle linethrough)
{
    return shouldSetDecorationAntialias(underline) || shouldSetDecorationAntialias(overline) || shouldSetDecorationAntialias(linethrough);
}

static void paintAppliedDecoration(GraphicsContext* context, FloatPoint start, float width, float doubleOffset, int wavyOffsetFactor,
    RenderObject::AppliedTextDecoration decoration, float thickness, bool antialiasDecoration, bool isPrinting)
{
    context->setStrokeStyle(textDecorationStyleToStrokeStyle(decoration.style));
    context->setStrokeColor(decoration.color);

    switch (decoration.style) {
    case TextDecorationStyleWavy:
        strokeWavyTextDecoration(context, start + FloatPoint(0, doubleOffset * wavyOffsetFactor), start + FloatPoint(width, doubleOffset * wavyOffsetFactor), thickness);
        break;
    case TextDecorationStyleDotted:
    case TextDecorationStyleDashed:
        context->setShouldAntialias(antialiasDecoration);
        // Fall through
    default:
        context->drawLineForText(start, width, isPrinting);

        if (decoration.style == TextDecorationStyleDouble)
            context->drawLineForText(start + FloatPoint(0, doubleOffset), width, isPrinting);
    }
}

void InlineTextBox::paintDecoration(GraphicsContext* context, const FloatPoint& boxOrigin, TextDecoration deco, const ShadowList* shadowList)
{
    GraphicsContextStateSaver stateSaver(*context);

    if (m_truncation == cFullTruncation)
        return;

    FloatPoint localOrigin = boxOrigin;

    float width = m_logicalWidth;
    if (m_truncation != cNoTruncation) {
        width = toRenderText(renderer()).width(m_start, m_truncation, textPos(), isLeftToRightDirection() ? LTR : RTL, isFirstLineStyle());
        if (!isLeftToRightDirection())
            localOrigin.move(m_logicalWidth - width, 0);
    }

    // Get the text decoration colors.
    RenderObject::AppliedTextDecoration underline, overline, linethrough;

    renderer().getTextDecorations(deco, underline, overline, linethrough, true);
    if (isFirstLineStyle())
        renderer().getTextDecorations(deco, underline, overline, linethrough, true, true);

    // Use a special function for underlines to get the positioning exactly right.
    bool isPrinting = textRenderer().document().printing();

    bool linesAreOpaque = !isPrinting && (!(deco & TextDecorationUnderline) || underline.color.alpha() == 255) && (!(deco & TextDecorationOverline) || overline.color.alpha() == 255) && (!(deco & TextDecorationLineThrough) || linethrough.color.alpha() == 255);

    RenderStyle* styleToUse = renderer().style(isFirstLineStyle());
    int baseline = styleToUse->fontMetrics().ascent();

    size_t shadowCount = shadowList ? shadowList->shadows().size() : 0;
    // Set the thick of the line to be 10% (or something else ?)of the computed font size and not less than 1px.
    // Using computedFontSize should take care of zoom as well.

    // Update Underline thickness, in case we have Faulty Font Metrics calculating underline thickness by old method.
    float textDecorationThickness = styleToUse->fontMetrics().underlineThickness();
    int fontHeightInt  = (int)(styleToUse->fontMetrics().floatHeight() + 0.5);
    if ((textDecorationThickness == 0.f) || (textDecorationThickness >= (fontHeightInt >> 1)))
        textDecorationThickness = std::max(1.f, styleToUse->computedFontSize() / 10.f);

    context->setStrokeThickness(textDecorationThickness);

    bool antialiasDecoration = shouldSetDecorationAntialias(overline.style, underline.style, linethrough.style)
        && RenderBoxModelObject::shouldAntialiasLines(context);

    float extraOffset = 0;
    if (!linesAreOpaque && shadowCount > 1) {
        FloatRect clipRect(localOrigin, FloatSize(width, baseline + 2));
        for (size_t i = shadowCount; i--; ) {
            const ShadowData& s = shadowList->shadows()[i];
            FloatRect shadowRect(localOrigin, FloatSize(width, baseline + 2));
            shadowRect.inflate(s.blur());
            float shadowX = isHorizontal() ? s.x() : s.y();
            float shadowY = isHorizontal() ? s.y() : -s.x();
            shadowRect.move(shadowX, shadowY);
            clipRect.unite(shadowRect);
            extraOffset = max(extraOffset, max(0.0f, shadowY) + s.blur());
        }
        context->clip(clipRect);
        extraOffset += baseline + 2;
        localOrigin.move(0, extraOffset);
    }

    for (size_t i = max(static_cast<size_t>(1), shadowCount); i--; ) {
        // Even if we have no shadows, we still want to run the code below this once.
        if (i < shadowCount) {
            if (!i) {
                // The last set of lines paints normally inside the clip.
                localOrigin.move(0, -extraOffset);
                extraOffset = 0;
            }
            const ShadowData& shadow = shadowList->shadows()[i];
            float shadowX = isHorizontal() ? shadow.x() : shadow.y();
            float shadowY = isHorizontal() ? shadow.y() : -shadow.x();
            context->setShadow(FloatSize(shadowX, shadowY - extraOffset), shadow.blur(), shadow.color());
        }

        // Offset between lines - always non-zero, so lines never cross each other.
        float doubleOffset = textDecorationThickness + 1.f;

        if (deco & TextDecorationUnderline) {
            const int underlineOffset = computeUnderlineOffset(styleToUse->textUnderlinePosition(), styleToUse->fontMetrics(), this, textDecorationThickness);
            paintAppliedDecoration(context, localOrigin + FloatPoint(0, underlineOffset), width, doubleOffset, 1, underline, textDecorationThickness, antialiasDecoration, isPrinting);
        }
        if (deco & TextDecorationOverline) {
            paintAppliedDecoration(context, localOrigin, width, -doubleOffset, 1, overline, textDecorationThickness, antialiasDecoration, isPrinting);
        }
        if (deco & TextDecorationLineThrough) {
            const float lineThroughOffset = 2 * baseline / 3;
            paintAppliedDecoration(context, localOrigin + FloatPoint(0, lineThroughOffset), width, doubleOffset, 0, linethrough, textDecorationThickness, antialiasDecoration, isPrinting);
        }
    }
}

static GraphicsContext::DocumentMarkerLineStyle lineStyleForMarkerType(DocumentMarker::MarkerType markerType)
{
    switch (markerType) {
    case DocumentMarker::Spelling:
        return GraphicsContext::DocumentMarkerSpellingLineStyle;
    case DocumentMarker::Grammar:
        return GraphicsContext::DocumentMarkerGrammarLineStyle;
    default:
        ASSERT_NOT_REACHED();
        return GraphicsContext::DocumentMarkerSpellingLineStyle;
    }
}

void InlineTextBox::paintDocumentMarker(GraphicsContext* pt, const FloatPoint& boxOrigin, DocumentMarker* marker, RenderStyle* style, const Font& font, bool grammar)
{
    // Never print spelling/grammar markers (5327887)
    if (textRenderer().document().printing())
        return;

    if (m_truncation == cFullTruncation)
        return;

    float start = 0; // start of line to draw, relative to tx
    float width = m_logicalWidth; // how much line to draw

    // Determine whether we need to measure text
    bool markerSpansWholeBox = true;
    if (m_start <= (int)marker->startOffset())
        markerSpansWholeBox = false;
    if ((end() + 1) != marker->endOffset()) // end points at the last char, not past it
        markerSpansWholeBox = false;
    if (m_truncation != cNoTruncation)
        markerSpansWholeBox = false;

    if (!markerSpansWholeBox || grammar) {
        int startPosition = max<int>(marker->startOffset() - m_start, 0);
        int endPosition = min<int>(marker->endOffset() - m_start, m_len);

        if (m_truncation != cNoTruncation)
            endPosition = min<int>(endPosition, m_truncation);

        // Calculate start & width
        int deltaY = renderer().style()->isFlippedLinesWritingMode() ? selectionBottom() - logicalBottom() : logicalTop() - selectionTop();
        int selHeight = selectionHeight();
        FloatPoint startPoint(boxOrigin.x(), boxOrigin.y() - deltaY);
        TextRun run = constructTextRun(style, font);

        // FIXME: Convert the document markers to float rects.
        IntRect markerRect = enclosingIntRect(font.selectionRectForText(run, startPoint, selHeight, startPosition, endPosition));
        start = markerRect.x() - startPoint.x();
        width = markerRect.width();

        // Store rendered rects for bad grammar markers, so we can hit-test against it elsewhere in order to
        // display a toolTip. We don't do this for misspelling markers.
        if (grammar) {
            markerRect.move(-boxOrigin.x(), -boxOrigin.y());
            markerRect = renderer().localToAbsoluteQuad(FloatRect(markerRect)).enclosingBoundingBox();
            toRenderedDocumentMarker(marker)->setRenderedRect(markerRect);
        }
    }

    // IMPORTANT: The misspelling underline is not considered when calculating the text bounds, so we have to
    // make sure to fit within those bounds.  This means the top pixel(s) of the underline will overlap the
    // bottom pixel(s) of the glyphs in smaller font sizes.  The alternatives are to increase the line spacing (bad!!)
    // or decrease the underline thickness.  The overlap is actually the most useful, and matches what AppKit does.
    // So, we generally place the underline at the bottom of the text, but in larger fonts that's not so good so
    // we pin to two pixels under the baseline.
    int lineThickness = misspellingLineThickness;
    int baseline = renderer().style(isFirstLineStyle())->fontMetrics().ascent();
    int descent = logicalHeight() - baseline;
    int underlineOffset;
    if (descent <= (2 + lineThickness)) {
        // Place the underline at the very bottom of the text in small/medium fonts.
        underlineOffset = logicalHeight() - lineThickness;
    } else {
        // In larger fonts, though, place the underline up near the baseline to prevent a big gap.
        underlineOffset = baseline + 2;
    }
    pt->drawLineForDocumentMarker(FloatPoint(boxOrigin.x() + start, boxOrigin.y() + underlineOffset), width, lineStyleForMarkerType(marker->type()));
}

void InlineTextBox::paintTextMatchMarker(GraphicsContext* pt, const FloatPoint& boxOrigin, DocumentMarker* marker, RenderStyle* style, const Font& font)
{
    // Use same y positioning and height as for selection, so that when the selection and this highlight are on
    // the same word there are no pieces sticking out.
    int deltaY = renderer().style()->isFlippedLinesWritingMode() ? selectionBottom() - logicalBottom() : logicalTop() - selectionTop();
    int selHeight = selectionHeight();

    int sPos = max(marker->startOffset() - m_start, (unsigned)0);
    int ePos = min(marker->endOffset() - m_start, (unsigned)m_len);
    TextRun run = constructTextRun(style, font);

    // Always compute and store the rect associated with this marker. The computed rect is in absolute coordinates.
    IntRect markerRect = enclosingIntRect(font.selectionRectForText(run, IntPoint(x(), selectionTop()), selHeight, sPos, ePos));
    markerRect = renderer().localToAbsoluteQuad(FloatRect(markerRect)).enclosingBoundingBox();
    toRenderedDocumentMarker(marker)->setRenderedRect(markerRect);

    // Optionally highlight the text
    if (renderer().frame()->editor().markedTextMatchesAreHighlighted()) {
        Color color = marker->activeMatch() ?
            RenderTheme::theme().platformActiveTextSearchHighlightColor() :
            RenderTheme::theme().platformInactiveTextSearchHighlightColor();
        GraphicsContextStateSaver stateSaver(*pt);
        updateGraphicsContext(pt, color, color, 0); // Don't draw text at all!
        pt->clip(FloatRect(boxOrigin.x(), boxOrigin.y() - deltaY, m_logicalWidth, selHeight));
        pt->drawHighlightForText(font, run, FloatPoint(boxOrigin.x(), boxOrigin.y() - deltaY), selHeight, color, sPos, ePos);
    }
}

void InlineTextBox::paintDocumentMarkers(GraphicsContext* pt, const FloatPoint& boxOrigin, RenderStyle* style, const Font& font, bool background)
{
    if (!renderer().node())
        return;

    Vector<DocumentMarker*> markers = renderer().document().markers().markersFor(renderer().node());
    Vector<DocumentMarker*>::const_iterator markerIt = markers.begin();

    // Give any document markers that touch this run a chance to draw before the text has been drawn.
    // Note end() points at the last char, not one past it like endOffset and ranges do.
    for ( ; markerIt != markers.end(); ++markerIt) {
        DocumentMarker* marker = *markerIt;

        // Paint either the background markers or the foreground markers, but not both
        switch (marker->type()) {
            case DocumentMarker::Grammar:
            case DocumentMarker::Spelling:
                if (background)
                    continue;
                break;
            case DocumentMarker::TextMatch:
                if (!background)
                    continue;
                break;
            default:
                continue;
        }

        if (marker->endOffset() <= start())
            // marker is completely before this run.  This might be a marker that sits before the
            // first run we draw, or markers that were within runs we skipped due to truncation.
            continue;

        if (marker->startOffset() > end())
            // marker is completely after this run, bail.  A later run will paint it.
            break;

        // marker intersects this run.  Paint it.
        switch (marker->type()) {
            case DocumentMarker::Spelling:
                paintDocumentMarker(pt, boxOrigin, marker, style, font, false);
                break;
            case DocumentMarker::Grammar:
                paintDocumentMarker(pt, boxOrigin, marker, style, font, true);
                break;
            case DocumentMarker::TextMatch:
                paintTextMatchMarker(pt, boxOrigin, marker, style, font);
                break;
            default:
                ASSERT_NOT_REACHED();
        }

    }
}

void InlineTextBox::paintCompositionUnderline(GraphicsContext* ctx, const FloatPoint& boxOrigin, const CompositionUnderline& underline)
{
    if (m_truncation == cFullTruncation)
        return;

    float start = 0; // start of line to draw, relative to tx
    float width = m_logicalWidth; // how much line to draw
    bool useWholeWidth = true;
    unsigned paintStart = m_start;
    unsigned paintEnd = end() + 1; // end points at the last char, not past it
    if (paintStart <= underline.startOffset) {
        paintStart = underline.startOffset;
        useWholeWidth = false;
        start = toRenderText(renderer()).width(m_start, paintStart - m_start, textPos(), isLeftToRightDirection() ? LTR : RTL, isFirstLineStyle());
    }
    if (paintEnd != underline.endOffset) {      // end points at the last char, not past it
        paintEnd = min(paintEnd, (unsigned)underline.endOffset);
        useWholeWidth = false;
    }
    if (m_truncation != cNoTruncation) {
        paintEnd = min(paintEnd, (unsigned)m_start + m_truncation);
        useWholeWidth = false;
    }
    if (!useWholeWidth) {
        width = toRenderText(renderer()).width(paintStart, paintEnd - paintStart, textPos() + start, isLeftToRightDirection() ? LTR : RTL, isFirstLineStyle());
    }

    // Thick marked text underlines are 2px thick as long as there is room for the 2px line under the baseline.
    // All other marked text underlines are 1px thick.
    // If there's not enough space the underline will touch or overlap characters.
    int lineThickness = 1;
    int baseline = renderer().style(isFirstLineStyle())->fontMetrics().ascent();
    if (underline.thick && logicalHeight() - baseline >= 2)
        lineThickness = 2;

    // We need to have some space between underlines of subsequent clauses, because some input methods do not use different underline styles for those.
    // We make each line shorter, which has a harmless side effect of shortening the first and last clauses, too.
    start += 1;
    width -= 2;

    ctx->setStrokeColor(underline.color);
    ctx->setStrokeThickness(lineThickness);
    ctx->drawLineForText(FloatPoint(boxOrigin.x() + start, boxOrigin.y() + logicalHeight() - lineThickness), width, textRenderer().document().printing());
}

int InlineTextBox::caretMinOffset() const
{
    return m_start;
}

int InlineTextBox::caretMaxOffset() const
{
    return m_start + m_len;
}

float InlineTextBox::textPos() const
{
    // When computing the width of a text run, RenderBlock::computeInlineDirectionPositionsForLine() doesn't include the actual offset
    // from the containing block edge in its measurement. textPos() should be consistent so the text are rendered in the same width.
    if (logicalLeft() == 0)
        return 0;
    return logicalLeft() - root().logicalLeft();
}

int InlineTextBox::offsetForPosition(float lineOffset, bool includePartialGlyphs) const
{
    if (isLineBreak())
        return 0;

    if (lineOffset - logicalLeft() > logicalWidth())
        return isLeftToRightDirection() ? len() : 0;
    if (lineOffset - logicalLeft() < 0)
        return isLeftToRightDirection() ? 0 : len();

    FontCachePurgePreventer fontCachePurgePreventer;

    RenderText& text = toRenderText(renderer());
    RenderStyle* style = text.style(isFirstLineStyle());
    const Font& font = style->font();
    return font.offsetForPosition(constructTextRun(style, font), lineOffset - logicalLeft(), includePartialGlyphs);
}

float InlineTextBox::positionForOffset(int offset) const
{
    ASSERT(offset >= m_start);
    ASSERT(offset <= m_start + m_len);

    if (isLineBreak())
        return logicalLeft();

    FontCachePurgePreventer fontCachePurgePreventer;

    RenderText& text = toRenderText(renderer());
    RenderStyle* styleToUse = text.style(isFirstLineStyle());
    ASSERT(styleToUse);
    const Font& font = styleToUse->font();
    int from = !isLeftToRightDirection() ? offset - m_start : 0;
    int to = !isLeftToRightDirection() ? m_len : offset - m_start;
    // FIXME: Do we need to add rightBearing here?
    return font.selectionRectForText(constructTextRun(styleToUse, font), IntPoint(logicalLeft(), 0), 0, from, to).maxX();
}

bool InlineTextBox::containsCaretOffset(int offset) const
{
    // Offsets before the box are never "in".
    if (offset < m_start)
        return false;

    int pastEnd = m_start + m_len;

    // Offsets inside the box (not at either edge) are always "in".
    if (offset < pastEnd)
        return true;

    // Offsets outside the box are always "out".
    if (offset > pastEnd)
        return false;

    // Offsets at the end are "out" for line breaks (they are on the next line).
    if (isLineBreak())
        return false;

    // Offsets at the end are "in" for normal boxes (but the caller has to check affinity).
    return true;
}

void InlineTextBox::characterWidths(Vector<float>& widths) const
{
    FontCachePurgePreventer fontCachePurgePreventer;

    RenderStyle* styleToUse = textRenderer().style(isFirstLineStyle());
    const Font& font = styleToUse->font();

    TextRun textRun = constructTextRun(styleToUse, font);

    GlyphBuffer glyphBuffer;
    WidthIterator it(&font, textRun);
    float lastWidth = 0;
    widths.resize(m_len);
    for (unsigned i = 0; i < m_len; i++) {
        it.advance(i + 1, &glyphBuffer);
        widths[i] = it.m_runWidthSoFar - lastWidth;
        lastWidth = it.m_runWidthSoFar;
    }
}

TextRun InlineTextBox::constructTextRun(RenderStyle* style, const Font& font, StringBuilder* charactersWithHyphen) const
{
    ASSERT(style);
    ASSERT(textRenderer().text());

    StringView string = textRenderer().text().createView();
    unsigned startPos = start();
    unsigned length = len();

    if (string.length() != length || startPos)
        string.narrow(startPos, length);

    return constructTextRun(style, font, string, textRenderer().textLength() - startPos, charactersWithHyphen);
}

TextRun InlineTextBox::constructTextRun(RenderStyle* style, const Font& font, StringView string, int maximumLength, StringBuilder* charactersWithHyphen) const
{
    ASSERT(style);

    if (charactersWithHyphen) {
        const AtomicString& hyphenString = style->hyphenString();
        charactersWithHyphen->reserveCapacity(string.length() + hyphenString.length());
        charactersWithHyphen->append(string);
        charactersWithHyphen->append(hyphenString);
        string = charactersWithHyphen->toString().createView();
        maximumLength = string.length();
    }

    ASSERT(maximumLength >= static_cast<int>(string.length()));

    TextRun run(string, textPos(), expansion(), expansionBehavior(), direction(), dirOverride() || style->rtlOrdering() == VisualOrder, !textRenderer().canUseSimpleFontCodePath());
    run.setTabSize(!style->collapseWhiteSpace(), style->tabSize());
    run.setCharacterScanForCodePath(!textRenderer().canUseSimpleFontCodePath());
    if (textRunNeedsRenderingContext(font))
        run.setRenderingContext(SVGTextRunRenderingContext::create(&textRenderer()));

    // Propagate the maximum length of the characters buffer to the TextRun, even when we're only processing a substring.
    run.setCharactersLength(maximumLength);
    ASSERT(run.charactersLength() >= run.length());
    return run;
}

TextRun InlineTextBox::constructTextRunForInspector(RenderStyle* style, const Font& font) const
{
    return InlineTextBox::constructTextRun(style, font);
}

#ifndef NDEBUG

const char* InlineTextBox::boxName() const
{
    return "InlineTextBox";
}

void InlineTextBox::showBox(int printedCharacters) const
{
    const RenderText& obj = toRenderText(renderer());
    String value = obj.text();
    value = value.substring(start(), len());
    value.replaceWithLiteral('\\', "\\\\");
    value.replaceWithLiteral('\n', "\\n");
    printedCharacters += fprintf(stderr, "%s\t%p", boxName(), this);
    for (; printedCharacters < showTreeCharacterOffset; printedCharacters++)
        fputc(' ', stderr);
    printedCharacters = fprintf(stderr, "\t%s %p", obj.renderName(), &obj);
    const int rendererCharacterOffset = 24;
    for (; printedCharacters < rendererCharacterOffset; printedCharacters++)
        fputc(' ', stderr);
    fprintf(stderr, "(%d,%d) \"%s\"\n", start(), start() + len(), value.utf8().data());
}

#endif

} // namespace WebCore
