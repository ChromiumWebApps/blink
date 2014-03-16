/**
 * Copyright (C) 2006, 2007, 2010 Apple Inc. All rights reserved.
 *           (C) 2008 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
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
#include "core/rendering/RenderTextControlSingleLine.h"

#include "CSSValueKeywords.h"
#include "core/dom/shadow/ShadowRoot.h"
#include "core/editing/FrameSelection.h"
#include "core/frame/LocalFrame.h"
#include "core/html/shadow/ShadowElementNames.h"
#include "core/rendering/HitTestResult.h"
#include "core/rendering/LayoutRectRecorder.h"
#include "core/rendering/RenderLayer.h"
#include "core/rendering/RenderTheme.h"
#include "platform/PlatformKeyboardEvent.h"
#include "platform/fonts/SimpleFontData.h"

using namespace std;

namespace WebCore {

using namespace HTMLNames;

RenderTextControlSingleLine::RenderTextControlSingleLine(HTMLInputElement* element)
    : RenderTextControl(element)
    , m_shouldDrawCapsLockIndicator(false)
    , m_desiredInnerTextLogicalHeight(-1)
{
    ASSERT(element->hasTagName(inputTag));
}

RenderTextControlSingleLine::~RenderTextControlSingleLine()
{
}

inline Element* RenderTextControlSingleLine::containerElement() const
{
    return inputElement()->userAgentShadowRoot()->getElementById(ShadowElementNames::textFieldContainer());
}

inline Element* RenderTextControlSingleLine::editingViewPortElement() const
{
    return inputElement()->userAgentShadowRoot()->getElementById(ShadowElementNames::editingViewPort());
}

inline HTMLElement* RenderTextControlSingleLine::innerSpinButtonElement() const
{
    return toHTMLElement(inputElement()->userAgentShadowRoot()->getElementById(ShadowElementNames::spinButton()));
}

void RenderTextControlSingleLine::paint(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    RenderTextControl::paint(paintInfo, paintOffset);

    if (paintInfo.phase == PaintPhaseBlockBackground && m_shouldDrawCapsLockIndicator) {
        LayoutRect contentsRect = contentBoxRect();

        // Center in the block progression direction.
        if (isHorizontalWritingMode())
            contentsRect.setY((height() - contentsRect.height()) / 2);
        else
            contentsRect.setX((width() - contentsRect.width()) / 2);

        // Convert the rect into the coords used for painting the content
        contentsRect.moveBy(paintOffset + location());
        RenderTheme::theme().paintCapsLockIndicator(this, paintInfo, pixelSnappedIntRect(contentsRect));
    }
}

LayoutUnit RenderTextControlSingleLine::computeLogicalHeightLimit() const
{
    return containerElement() ? contentLogicalHeight() : logicalHeight();
}

void RenderTextControlSingleLine::layout()
{
    LayoutRectRecorder recorder(*this);
    SubtreeLayoutScope layoutScope(this);

    // FIXME: We should remove the height-related hacks in layout() and
    // styleDidChange(). We need them because
    // - Center the inner elements vertically if the input height is taller than
    //   the intrinsic height of the inner elements.
    // - Shrink the inner elment heights if the input height is samller than the
    //   intrinsic heights of the inner elements.

    // We don't honor paddings and borders for textfields without decorations
    // and type=search if the text height is taller than the contentHeight()
    // because of compability.

    RenderBox* innerTextRenderer = innerTextElement()->renderBox();
    RenderBox* viewPortRenderer = editingViewPortElement() ? editingViewPortElement()->renderBox() : 0;

    // To ensure consistency between layouts, we need to reset any conditionally overriden height.
    if (innerTextRenderer && !innerTextRenderer->style()->logicalHeight().isAuto()) {
        innerTextRenderer->style()->setLogicalHeight(Length(Auto));
        layoutScope.setNeedsLayout(innerTextRenderer);
    }
    if (viewPortRenderer && !viewPortRenderer->style()->logicalHeight().isAuto()) {
        viewPortRenderer->style()->setLogicalHeight(Length(Auto));
        layoutScope.setNeedsLayout(viewPortRenderer);
    }

    RenderBlockFlow::layoutBlock(false);

    Element* container = containerElement();
    RenderBox* containerRenderer = container ? container->renderBox() : 0;

    // Set the text block height
    LayoutUnit desiredLogicalHeight = textBlockLogicalHeight();
    LayoutUnit logicalHeightLimit = computeLogicalHeightLimit();
    if (innerTextRenderer && innerTextRenderer->logicalHeight() > logicalHeightLimit) {
        if (desiredLogicalHeight != innerTextRenderer->logicalHeight())
            layoutScope.setNeedsLayout(this);

        m_desiredInnerTextLogicalHeight = desiredLogicalHeight;

        innerTextRenderer->style()->setLogicalHeight(Length(desiredLogicalHeight, Fixed));
        layoutScope.setNeedsLayout(innerTextRenderer);
        if (viewPortRenderer) {
            viewPortRenderer->style()->setLogicalHeight(Length(desiredLogicalHeight, Fixed));
            layoutScope.setNeedsLayout(viewPortRenderer);
        }
    }
    // The container might be taller because of decoration elements.
    if (containerRenderer) {
        containerRenderer->layoutIfNeeded();
        LayoutUnit containerLogicalHeight = containerRenderer->logicalHeight();
        if (containerLogicalHeight > logicalHeightLimit) {
            containerRenderer->style()->setLogicalHeight(Length(logicalHeightLimit, Fixed));
            layoutScope.setNeedsLayout(this);
        } else if (containerRenderer->logicalHeight() < contentLogicalHeight()) {
            containerRenderer->style()->setLogicalHeight(Length(contentLogicalHeight(), Fixed));
            layoutScope.setNeedsLayout(this);
        } else
            containerRenderer->style()->setLogicalHeight(Length(containerLogicalHeight, Fixed));
    }

    // If we need another layout pass, we have changed one of children's height so we need to relayout them.
    if (needsLayout())
        RenderBlockFlow::layoutBlock(true);

    // Center the child block in the block progression direction (vertical centering for horizontal text fields).
    if (!container && innerTextRenderer && innerTextRenderer->height() != contentLogicalHeight()) {
        LayoutUnit logicalHeightDiff = innerTextRenderer->logicalHeight() - contentLogicalHeight();
        innerTextRenderer->setLogicalTop(innerTextRenderer->logicalTop() - (logicalHeightDiff / 2 + layoutMod(logicalHeightDiff, 2)));
    } else
        centerContainerIfNeeded(containerRenderer);

    // Ignores the paddings for the inner spin button.
    if (RenderBox* innerSpinBox = innerSpinButtonElement() ? innerSpinButtonElement()->renderBox() : 0) {
        RenderBox* parentBox = innerSpinBox->parentBox();
        if (containerRenderer && !containerRenderer->style()->isLeftToRightDirection())
            innerSpinBox->setLogicalLocation(LayoutPoint(-paddingLogicalLeft(), -paddingBefore()));
        else
            innerSpinBox->setLogicalLocation(LayoutPoint(parentBox->logicalWidth() - innerSpinBox->logicalWidth() + paddingLogicalRight(), -paddingBefore()));
        innerSpinBox->setLogicalHeight(logicalHeight() - borderBefore() - borderAfter());
    }

    HTMLElement* placeholderElement = inputElement()->placeholderElement();
    if (RenderBox* placeholderBox = placeholderElement ? placeholderElement->renderBox() : 0) {
        LayoutSize innerTextSize;
        LayoutRectRecorder placeholderBoxRecorder(*placeholderBox);

        if (innerTextRenderer)
            innerTextSize = innerTextRenderer->size();
        placeholderBox->style()->setWidth(Length(innerTextSize.width() - placeholderBox->borderAndPaddingWidth(), Fixed));
        placeholderBox->style()->setHeight(Length(innerTextSize.height() - placeholderBox->borderAndPaddingHeight(), Fixed));
        bool neededLayout = placeholderBox->needsLayout();
        bool placeholderBoxHadLayout = placeholderBox->everHadLayout();
        placeholderBox->layoutIfNeeded();
        LayoutPoint textOffset;
        if (innerTextRenderer)
            textOffset = innerTextRenderer->location();
        if (editingViewPortElement() && editingViewPortElement()->renderBox())
            textOffset += toLayoutSize(editingViewPortElement()->renderBox()->location());
        if (containerRenderer)
            textOffset += toLayoutSize(containerRenderer->location());
        placeholderBox->setLocation(textOffset);

        if (!placeholderBoxHadLayout && placeholderBox->checkForRepaintDuringLayout()) {
            // This assumes a shadow tree without floats. If floats are added, the
            // logic should be shared with RenderBlockFlow::layoutBlockChild.
            placeholderBox->repaint();
        }
        // The placeholder gets layout last, after the parent text control and its other children,
        // so in order to get the correct overflow from the placeholder we need to recompute it now.
        if (neededLayout)
            computeOverflow(clientLogicalBottom());
    }
}

bool RenderTextControlSingleLine::nodeAtPoint(const HitTestRequest& request, HitTestResult& result, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, HitTestAction hitTestAction)
{
    if (!RenderTextControl::nodeAtPoint(request, result, locationInContainer, accumulatedOffset, hitTestAction))
        return false;

    // Say that we hit the inner text element if
    //  - we hit a node inside the inner text element,
    //  - we hit the <input> element (e.g. we're over the border or padding), or
    //  - we hit regions not in any decoration buttons.
    Element* container = containerElement();
    if (result.innerNode()->isDescendantOf(innerTextElement()) || result.innerNode() == node() || (container && container == result.innerNode())) {
        LayoutPoint pointInParent = locationInContainer.point();
        if (container && editingViewPortElement()) {
            if (editingViewPortElement()->renderBox())
                pointInParent -= toLayoutSize(editingViewPortElement()->renderBox()->location());
            if (container->renderBox())
                pointInParent -= toLayoutSize(container->renderBox()->location());
        }
        hitInnerTextElement(result, pointInParent, accumulatedOffset);
    }
    return true;
}

void RenderTextControlSingleLine::styleDidChange(StyleDifference diff, const RenderStyle* oldStyle)
{
    m_desiredInnerTextLogicalHeight = -1;
    RenderTextControl::styleDidChange(diff, oldStyle);

    // We may have set the width and the height in the old style in layout().
    // Reset them now to avoid getting a spurious layout hint.
    Element* viewPort = editingViewPortElement();
    if (RenderObject* viewPortRenderer = viewPort ? viewPort->renderer() : 0) {
        viewPortRenderer->style()->setHeight(Length());
        viewPortRenderer->style()->setWidth(Length());
    }
    Element* container = containerElement();
    if (RenderObject* containerRenderer = container ? container->renderer() : 0) {
        containerRenderer->style()->setHeight(Length());
        containerRenderer->style()->setWidth(Length());
    }
    RenderObject* innerTextRenderer = innerTextElement()->renderer();
    if (innerTextRenderer && diff == StyleDifferenceLayout)
        innerTextRenderer->setNeedsLayout();
    if (HTMLElement* placeholder = inputElement()->placeholderElement())
        placeholder->setInlineStyleProperty(CSSPropertyTextOverflow, textShouldBeTruncated() ? CSSValueEllipsis : CSSValueClip);
    setHasOverflowClip(false);
}

void RenderTextControlSingleLine::capsLockStateMayHaveChanged()
{
    if (!node())
        return;

    // Only draw the caps lock indicator if these things are true:
    // 1) The field is a password field
    // 2) The frame is active
    // 3) The element is focused
    // 4) The caps lock is on
    bool shouldDrawCapsLockIndicator = false;

    if (LocalFrame* frame = document().frame())
        shouldDrawCapsLockIndicator = inputElement()->isPasswordField() && frame->selection().isFocusedAndActive() && document().focusedElement() == node() && PlatformKeyboardEvent::currentCapsLockState();

    if (shouldDrawCapsLockIndicator != m_shouldDrawCapsLockIndicator) {
        m_shouldDrawCapsLockIndicator = shouldDrawCapsLockIndicator;
        repaint();
    }
}

bool RenderTextControlSingleLine::hasControlClip() const
{
    // Apply control clip for text fields with decorations.
    return !!containerElement();
}

LayoutRect RenderTextControlSingleLine::controlClipRect(const LayoutPoint& additionalOffset) const
{
    ASSERT(hasControlClip());
    LayoutRect clipRect = contentBoxRect();
    if (containerElement()->renderBox())
        clipRect = unionRect(clipRect, containerElement()->renderBox()->frameRect());
    clipRect.moveBy(additionalOffset);
    return clipRect;
}

float RenderTextControlSingleLine::getAvgCharWidth(AtomicString family)
{
    // Since Lucida Grande is the default font, we want this to match the width
    // of MS Shell Dlg, the default font for textareas in Firefox, Safari Win and
    // IE for some encodings (in IE, the default font is encoding specific).
    // 901 is the avgCharWidth value in the OS/2 table for MS Shell Dlg.
    if (family == "Lucida Grande")
        return scaleEmToUnits(901);

    return RenderTextControl::getAvgCharWidth(family);
}

LayoutUnit RenderTextControlSingleLine::preferredContentLogicalWidth(float charWidth) const
{
    int factor;
    bool includesDecoration = inputElement()->sizeShouldIncludeDecoration(factor);
    if (factor <= 0)
        factor = 20;

    LayoutUnit result = LayoutUnit::fromFloatCeil(charWidth * factor);

    float maxCharWidth = 0.f;
    AtomicString family = style()->font().fontDescription().family().family();
    // Since Lucida Grande is the default font, we want this to match the width
    // of MS Shell Dlg, the default font for textareas in Firefox, Safari Win and
    // IE for some encodings (in IE, the default font is encoding specific).
    // 4027 is the (xMax - xMin) value in the "head" font table for MS Shell Dlg.
    if (family == "Lucida Grande")
        maxCharWidth = scaleEmToUnits(4027);
    else if (hasValidAvgCharWidth(family))
        maxCharWidth = roundf(style()->font().primaryFont()->maxCharWidth());

    // For text inputs, IE adds some extra width.
    if (maxCharWidth > 0.f)
        result += maxCharWidth - charWidth;

    if (includesDecoration) {
        HTMLElement* spinButton = innerSpinButtonElement();
        if (RenderBox* spinRenderer = spinButton ? spinButton->renderBox() : 0) {
            result += spinRenderer->borderAndPaddingLogicalWidth();
            // Since the width of spinRenderer is not calculated yet, spinRenderer->logicalWidth() returns 0.
            // So computedStyle()->logicalWidth() is used instead.
            result += spinButton->computedStyle()->logicalWidth().value();
        }
    }

    return result;
}

LayoutUnit RenderTextControlSingleLine::computeControlLogicalHeight(LayoutUnit lineHeight, LayoutUnit nonContentHeight) const
{
    return lineHeight + nonContentHeight;
}

void RenderTextControlSingleLine::updateFromElement()
{
    RenderTextControl::updateFromElement();
}

PassRefPtr<RenderStyle> RenderTextControlSingleLine::createInnerTextStyle(const RenderStyle* startStyle) const
{
    RefPtr<RenderStyle> textBlockStyle = RenderStyle::create();
    textBlockStyle->inheritFrom(startStyle);
    adjustInnerTextStyle(textBlockStyle.get());

    textBlockStyle->setWhiteSpace(PRE);
    textBlockStyle->setOverflowWrap(NormalOverflowWrap);
    textBlockStyle->setOverflowX(OHIDDEN);
    textBlockStyle->setOverflowY(OHIDDEN);
    textBlockStyle->setTextOverflow(textShouldBeTruncated() ? TextOverflowEllipsis : TextOverflowClip);

    if (m_desiredInnerTextLogicalHeight >= 0)
        textBlockStyle->setLogicalHeight(Length(m_desiredInnerTextLogicalHeight, Fixed));
    // Do not allow line-height to be smaller than our default.
    if (textBlockStyle->fontMetrics().lineSpacing() > lineHeight(true, HorizontalLine, PositionOfInteriorLineBoxes))
        textBlockStyle->setLineHeight(RenderStyle::initialLineHeight());

    textBlockStyle->setDisplay(BLOCK);
    textBlockStyle->setUnique();

    if (inputElement()->shouldRevealPassword())
        textBlockStyle->setTextSecurity(TSNONE);

    return textBlockStyle.release();
}

bool RenderTextControlSingleLine::textShouldBeTruncated() const
{
    return document().focusedElement() != node() && style()->textOverflow() == TextOverflowEllipsis;
}

void RenderTextControlSingleLine::autoscroll(const IntPoint& position)
{
    RenderBox* renderer = innerTextElement()->renderBox();
    if (!renderer)
        return;

    renderer->autoscroll(position);
}

int RenderTextControlSingleLine::scrollWidth() const
{
    if (innerTextElement())
        return innerTextElement()->scrollWidth();
    return RenderBlockFlow::scrollWidth();
}

int RenderTextControlSingleLine::scrollHeight() const
{
    if (innerTextElement())
        return innerTextElement()->scrollHeight();
    return RenderBlockFlow::scrollHeight();
}

int RenderTextControlSingleLine::scrollLeft() const
{
    if (innerTextElement())
        return innerTextElement()->scrollLeft();
    return RenderBlockFlow::scrollLeft();
}

int RenderTextControlSingleLine::scrollTop() const
{
    if (innerTextElement())
        return innerTextElement()->scrollTop();
    return RenderBlockFlow::scrollTop();
}

void RenderTextControlSingleLine::setScrollLeft(int newLeft)
{
    if (innerTextElement())
        innerTextElement()->setScrollLeft(newLeft);
}

void RenderTextControlSingleLine::setScrollTop(int newTop)
{
    if (innerTextElement())
        innerTextElement()->setScrollTop(newTop);
}

HTMLInputElement* RenderTextControlSingleLine::inputElement() const
{
    return toHTMLInputElement(node());
}

}
