/*
 * Copyright (C) 2011 Apple Inc. All Rights Reserved.
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
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "platform/scroll/ScrollbarTheme.h"

#include "RuntimeEnabledFeatures.h"
#include "platform/PlatformMouseEvent.h"
#include "platform/scroll/ScrollbarThemeClient.h"
#include "platform/scroll/ScrollbarThemeMock.h"
#include "platform/scroll/ScrollbarThemeOverlayMock.h"
#include "public/platform/Platform.h"
#include "public/platform/WebPoint.h"
#include "public/platform/WebRect.h"
#include "public/platform/WebScrollbarBehavior.h"

#if !OS(MACOSX)
#include "public/platform/WebRect.h"
#include "public/platform/WebThemeEngine.h"
#endif

namespace WebCore {

bool ScrollbarTheme::gMockScrollbarsEnabled = false;

bool ScrollbarTheme::paint(ScrollbarThemeClient* scrollbar, GraphicsContext* graphicsContext, const IntRect& damageRect)
{
    // Create the ScrollbarControlPartMask based on the damageRect
    ScrollbarControlPartMask scrollMask = NoPart;

    IntRect backButtonStartPaintRect;
    IntRect backButtonEndPaintRect;
    IntRect forwardButtonStartPaintRect;
    IntRect forwardButtonEndPaintRect;
    if (hasButtons(scrollbar)) {
        backButtonStartPaintRect = backButtonRect(scrollbar, BackButtonStartPart, true);
        if (damageRect.intersects(backButtonStartPaintRect))
            scrollMask |= BackButtonStartPart;
        backButtonEndPaintRect = backButtonRect(scrollbar, BackButtonEndPart, true);
        if (damageRect.intersects(backButtonEndPaintRect))
            scrollMask |= BackButtonEndPart;
        forwardButtonStartPaintRect = forwardButtonRect(scrollbar, ForwardButtonStartPart, true);
        if (damageRect.intersects(forwardButtonStartPaintRect))
            scrollMask |= ForwardButtonStartPart;
        forwardButtonEndPaintRect = forwardButtonRect(scrollbar, ForwardButtonEndPart, true);
        if (damageRect.intersects(forwardButtonEndPaintRect))
            scrollMask |= ForwardButtonEndPart;
    }

    IntRect startTrackRect;
    IntRect thumbRect;
    IntRect endTrackRect;
    IntRect trackPaintRect = trackRect(scrollbar, true);
    if (damageRect.intersects(trackPaintRect))
        scrollMask |= TrackBGPart;
    bool thumbPresent = hasThumb(scrollbar);
    if (thumbPresent) {
        IntRect track = trackRect(scrollbar);
        splitTrack(scrollbar, track, startTrackRect, thumbRect, endTrackRect);
        if (damageRect.intersects(thumbRect))
            scrollMask |= ThumbPart;
        if (damageRect.intersects(startTrackRect))
            scrollMask |= BackTrackPart;
        if (damageRect.intersects(endTrackRect))
            scrollMask |= ForwardTrackPart;
    }

    // Paint the scrollbar background (only used by custom CSS scrollbars).
    paintScrollbarBackground(graphicsContext, scrollbar);

    // Paint the back and forward buttons.
    if (scrollMask & BackButtonStartPart)
        paintButton(graphicsContext, scrollbar, backButtonStartPaintRect, BackButtonStartPart);
    if (scrollMask & BackButtonEndPart)
        paintButton(graphicsContext, scrollbar, backButtonEndPaintRect, BackButtonEndPart);
    if (scrollMask & ForwardButtonStartPart)
        paintButton(graphicsContext, scrollbar, forwardButtonStartPaintRect, ForwardButtonStartPart);
    if (scrollMask & ForwardButtonEndPart)
        paintButton(graphicsContext, scrollbar, forwardButtonEndPaintRect, ForwardButtonEndPart);

    if (scrollMask & TrackBGPart)
        paintTrackBackground(graphicsContext, scrollbar, trackPaintRect);

    if ((scrollMask & ForwardTrackPart) || (scrollMask & BackTrackPart)) {
        // Paint the track pieces above and below the thumb.
        if (scrollMask & BackTrackPart)
            paintTrackPiece(graphicsContext, scrollbar, startTrackRect, BackTrackPart);
        if (scrollMask & ForwardTrackPart)
            paintTrackPiece(graphicsContext, scrollbar, endTrackRect, ForwardTrackPart);

        paintTickmarks(graphicsContext, scrollbar, trackPaintRect);
    }

    // Paint the thumb.
    if (scrollMask & ThumbPart)
        paintThumb(graphicsContext, scrollbar, thumbRect);

    return true;
}

ScrollbarPart ScrollbarTheme::hitTest(ScrollbarThemeClient* scrollbar, const IntPoint& position)
{
    ScrollbarPart result = NoPart;
    if (!scrollbar->enabled())
        return result;

    IntPoint testPosition = scrollbar->convertFromContainingWindow(position);
    testPosition.move(scrollbar->x(), scrollbar->y());

    if (!scrollbar->frameRect().contains(testPosition))
        return NoPart;

    result = ScrollbarBGPart;

    IntRect track = trackRect(scrollbar);
    if (track.contains(testPosition)) {
        IntRect beforeThumbRect;
        IntRect thumbRect;
        IntRect afterThumbRect;
        splitTrack(scrollbar, track, beforeThumbRect, thumbRect, afterThumbRect);
        if (thumbRect.contains(testPosition))
            result = ThumbPart;
        else if (beforeThumbRect.contains(testPosition))
            result = BackTrackPart;
        else if (afterThumbRect.contains(testPosition))
            result = ForwardTrackPart;
        else
            result = TrackBGPart;
    } else if (backButtonRect(scrollbar, BackButtonStartPart).contains(testPosition)) {
        result = BackButtonStartPart;
    } else if (backButtonRect(scrollbar, BackButtonEndPart).contains(testPosition)) {
        result = BackButtonEndPart;
    } else if (forwardButtonRect(scrollbar, ForwardButtonStartPart).contains(testPosition)) {
        result = ForwardButtonStartPart;
    } else if (forwardButtonRect(scrollbar, ForwardButtonEndPart).contains(testPosition)) {
        result = ForwardButtonEndPart;
    }
    return result;
}

void ScrollbarTheme::invalidatePart(ScrollbarThemeClient* scrollbar, ScrollbarPart part)
{
    if (part == NoPart)
        return;

    IntRect result;
    switch (part) {
    case BackButtonStartPart:
        result = backButtonRect(scrollbar, BackButtonStartPart, true);
        break;
    case BackButtonEndPart:
        result = backButtonRect(scrollbar, BackButtonEndPart, true);
        break;
    case ForwardButtonStartPart:
        result = forwardButtonRect(scrollbar, ForwardButtonStartPart, true);
        break;
    case ForwardButtonEndPart:
        result = forwardButtonRect(scrollbar, ForwardButtonEndPart, true);
        break;
    case TrackBGPart:
        result = trackRect(scrollbar, true);
        break;
    case ScrollbarBGPart:
        result = scrollbar->frameRect();
        break;
    default: {
        IntRect beforeThumbRect, thumbRect, afterThumbRect;
        splitTrack(scrollbar, trackRect(scrollbar), beforeThumbRect, thumbRect, afterThumbRect);
        if (part == BackTrackPart)
            result = beforeThumbRect;
        else if (part == ForwardTrackPart)
            result = afterThumbRect;
        else
            result = thumbRect;
    }
    }
    result.moveBy(-scrollbar->location());
    scrollbar->invalidateRect(result);
}

void ScrollbarTheme::paintScrollCorner(GraphicsContext* context, const IntRect& cornerRect)
{
    if (cornerRect.isEmpty())
        return;

#if OS(MACOSX)
    context->fillRect(cornerRect, Color::white);
#else
    blink::Platform::current()->themeEngine()->paint(context->canvas(), blink::WebThemeEngine::PartScrollbarCorner, blink::WebThemeEngine::StateNormal, blink::WebRect(cornerRect), 0);
#endif
}

void ScrollbarTheme::paintOverhangBackground(GraphicsContext* context, const IntRect& horizontalOverhangRect, const IntRect& verticalOverhangRect, const IntRect& dirtyRect)
{
    context->setFillColor(Color::white);
    if (!horizontalOverhangRect.isEmpty())
        context->fillRect(intersection(horizontalOverhangRect, dirtyRect));
    if (!verticalOverhangRect.isEmpty())
        context->fillRect(intersection(verticalOverhangRect, dirtyRect));
}

bool ScrollbarTheme::shouldCenterOnThumb(ScrollbarThemeClient* scrollbar, const PlatformMouseEvent& evt)
{
    return blink::Platform::current()->scrollbarBehavior()->shouldCenterOnThumb(static_cast<blink::WebScrollbarBehavior::Button>(evt.button()), evt.shiftKey(), evt.altKey());
}

bool ScrollbarTheme::shouldSnapBackToDragOrigin(ScrollbarThemeClient* scrollbar, const PlatformMouseEvent& evt)
{
    IntPoint mousePosition = scrollbar->convertFromContainingWindow(evt.position());
    mousePosition.move(scrollbar->x(), scrollbar->y());
    return blink::Platform::current()->scrollbarBehavior()->shouldSnapBackToDragOrigin(mousePosition, trackRect(scrollbar), scrollbar->orientation() == HorizontalScrollbar);
}

// Returns the size represented by track taking into account scrolling past
// the end of the document.
static float usedTotalSize(ScrollbarThemeClient* scrollbar)
{
    float overhangAtStart = -scrollbar->currentPos();
    float overhangAtEnd = scrollbar->currentPos() + scrollbar->visibleSize() - scrollbar->totalSize();
    float overhang = std::max(0.0f, std::max(overhangAtStart, overhangAtEnd));
    return scrollbar->totalSize() + overhang;
}

int ScrollbarTheme::thumbPosition(ScrollbarThemeClient* scrollbar)
{
    if (scrollbar->enabled()) {
        float size = usedTotalSize(scrollbar) - scrollbar->visibleSize();
        // Avoid doing a floating point divide by zero and return 1 when usedTotalSize == visibleSize.
        if (!size)
            return 1;
        float pos = std::max(0.0f, scrollbar->currentPos()) * (trackLength(scrollbar) - thumbLength(scrollbar)) / size;
        return (pos < 1 && pos > 0) ? 1 : pos;
    }
    return 0;
}

int ScrollbarTheme::thumbLength(ScrollbarThemeClient* scrollbar)
{
    if (!scrollbar->enabled())
        return 0;

    float overhang = 0;
    if (scrollbar->currentPos() < 0)
        overhang = -scrollbar->currentPos();
    else if (scrollbar->visibleSize() + scrollbar->currentPos() > scrollbar->totalSize())
        overhang = scrollbar->currentPos() + scrollbar->visibleSize() - scrollbar->totalSize();
    float proportion = (scrollbar->visibleSize() - overhang) / usedTotalSize(scrollbar);
    int trackLen = trackLength(scrollbar);
    int length = round(proportion * trackLen);
    length = std::max(length, minimumThumbLength(scrollbar));
    if (length > trackLen)
        length = 0; // Once the thumb is below the track length, it just goes away (to make more room for the track).
    return length;
}

int ScrollbarTheme::trackPosition(ScrollbarThemeClient* scrollbar)
{
    IntRect constrainedTrackRect = constrainTrackRectToTrackPieces(scrollbar, trackRect(scrollbar));
    return (scrollbar->orientation() == HorizontalScrollbar) ? constrainedTrackRect.x() - scrollbar->x() : constrainedTrackRect.y() - scrollbar->y();
}

int ScrollbarTheme::trackLength(ScrollbarThemeClient* scrollbar)
{
    IntRect constrainedTrackRect = constrainTrackRectToTrackPieces(scrollbar, trackRect(scrollbar));
    return (scrollbar->orientation() == HorizontalScrollbar) ? constrainedTrackRect.width() : constrainedTrackRect.height();
}

IntRect ScrollbarTheme::thumbRect(ScrollbarThemeClient* scrollbar)
{
    if (!hasThumb(scrollbar))
        return IntRect();

    IntRect track = trackRect(scrollbar);
    IntRect startTrackRect;
    IntRect thumbRect;
    IntRect endTrackRect;
    splitTrack(scrollbar, track, startTrackRect, thumbRect, endTrackRect);

    return thumbRect;
}

int ScrollbarTheme::thumbThickness(ScrollbarThemeClient* scrollbar)
{
    IntRect track = trackRect(scrollbar);
    return scrollbar->orientation() == HorizontalScrollbar ? track.height() : track.width();
}

int ScrollbarTheme::minimumThumbLength(ScrollbarThemeClient* scrollbar)
{
    return scrollbarThickness(scrollbar->controlSize());
}

void ScrollbarTheme::splitTrack(ScrollbarThemeClient* scrollbar, const IntRect& unconstrainedTrackRect, IntRect& beforeThumbRect, IntRect& thumbRect, IntRect& afterThumbRect)
{
    // This function won't even get called unless we're big enough to have some combination of these three rects where at least
    // one of them is non-empty.
    IntRect trackRect = constrainTrackRectToTrackPieces(scrollbar, unconstrainedTrackRect);
    int thumbPos = thumbPosition(scrollbar);
    if (scrollbar->orientation() == HorizontalScrollbar) {
        thumbRect = IntRect(trackRect.x() + thumbPos, trackRect.y(), thumbLength(scrollbar), scrollbar->height());
        beforeThumbRect = IntRect(trackRect.x(), trackRect.y(), thumbPos + thumbRect.width() / 2, trackRect.height());
        afterThumbRect = IntRect(trackRect.x() + beforeThumbRect.width(), trackRect.y(), trackRect.maxX() - beforeThumbRect.maxX(), trackRect.height());
    } else {
        thumbRect = IntRect(trackRect.x(), trackRect.y() + thumbPos, scrollbar->width(), thumbLength(scrollbar));
        beforeThumbRect = IntRect(trackRect.x(), trackRect.y(), trackRect.width(), thumbPos + thumbRect.height() / 2);
        afterThumbRect = IntRect(trackRect.x(), trackRect.y() + beforeThumbRect.height(), trackRect.width(), trackRect.maxY() - beforeThumbRect.maxY());
    }
}

ScrollbarTheme* ScrollbarTheme::theme()
{
    if (ScrollbarTheme::mockScrollbarsEnabled()) {
        if (RuntimeEnabledFeatures::overlayScrollbarsEnabled()) {
            DEFINE_STATIC_LOCAL(ScrollbarThemeOverlayMock, overlayMockTheme, ());
            return &overlayMockTheme;
        }

        DEFINE_STATIC_LOCAL(ScrollbarThemeMock, mockTheme, ());
        return &mockTheme;
    }
    return nativeTheme();
}

void ScrollbarTheme::setMockScrollbarsEnabled(bool flag)
{
    gMockScrollbarsEnabled = flag;
}

bool ScrollbarTheme::mockScrollbarsEnabled()
{
    return gMockScrollbarsEnabled;
}

}
