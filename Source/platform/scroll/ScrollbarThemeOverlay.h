/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef ScrollbarThemeOverlay_h
#define ScrollbarThemeOverlay_h

#include "platform/scroll/ScrollbarTheme.h"

namespace WebCore {

// This scrollbar theme is used to get overlay scrollbar for platforms other
// than Mac. Mac's overlay scrollbars are in ScrollbarThemeMac*.
class PLATFORM_EXPORT ScrollbarThemeOverlay : public ScrollbarTheme {
public:
    enum HitTestBehavior { AllowHitTest, DisallowHitTest };

    ScrollbarThemeOverlay(int thumbThickness, int scrollbarMargin, HitTestBehavior);
    ScrollbarThemeOverlay(int thumbThickness, int scrollbarMargin, HitTestBehavior, Color);
    virtual ~ScrollbarThemeOverlay() { }

    virtual int scrollbarThickness(ScrollbarControlSize) OVERRIDE;
    virtual bool usesOverlayScrollbars() const OVERRIDE;

    virtual int thumbPosition(ScrollbarThemeClient*) OVERRIDE;
    virtual int thumbLength(ScrollbarThemeClient*) OVERRIDE;

    virtual bool hasButtons(ScrollbarThemeClient*) OVERRIDE { return false; };
    virtual bool hasThumb(ScrollbarThemeClient*) OVERRIDE;

    virtual IntRect backButtonRect(ScrollbarThemeClient*, ScrollbarPart, bool painting = false) OVERRIDE;
    virtual IntRect forwardButtonRect(ScrollbarThemeClient*, ScrollbarPart, bool painting = false) OVERRIDE;
    virtual IntRect trackRect(ScrollbarThemeClient*, bool painting = false) OVERRIDE;
    virtual int thumbThickness(ScrollbarThemeClient*) OVERRIDE;

    virtual void paintThumb(GraphicsContext*, ScrollbarThemeClient*, const IntRect&) OVERRIDE;
    virtual ScrollbarPart hitTest(ScrollbarThemeClient*, const IntPoint&) OVERRIDE;

private:
    int m_thumbThickness;
    int m_scrollbarMargin;
    HitTestBehavior m_allowHitTest;
    Color m_color;
    const bool m_useSolidColor;
};

} // namespace WebCore

#endif
