/*
 * Copyright (C) 2013 Adobe Systems Incorporated. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef LineWidth_h
#define LineWidth_h

#include "platform/LayoutUnit.h"

namespace WebCore {

class FloatingObject;
class RenderBlock;
class RenderObject;
class RenderRubyRun;
class RenderBlockFlow;

struct LineSegment;

enum IndentTextOrNot { DoNotIndentText, IndentText };

class LineWidth {
public:
    LineWidth(RenderBlockFlow&, bool isFirstLine, IndentTextOrNot shouldIndentText);

    bool fitsOnLine() const { return currentWidth() <= (m_availableWidth + LayoutUnit::epsilon()); }
    bool fitsOnLine(float extra) const { return currentWidth() + extra <= (m_availableWidth + LayoutUnit::epsilon()); }

    float currentWidth() const { return m_committedWidth + m_uncommittedWidth; }
    // FIXME: We should eventually replace these three functions by ones that work on a higher abstraction.
    float uncommittedWidth() const { return m_uncommittedWidth; }
    float committedWidth() const { return m_committedWidth; }
    float availableWidth() const { return m_availableWidth; }

    void updateAvailableWidth(LayoutUnit minimumHeight = 0);
    void shrinkAvailableWidthForNewFloatIfNeeded(FloatingObject*);
    void addUncommittedWidth(float delta) { m_uncommittedWidth += delta; }
    void commit();
    void applyOverhang(RenderRubyRun*, RenderObject* startRenderer, RenderObject* endRenderer);
    void fitBelowFloats(bool isFirstLine = false);

    void updateCurrentShapeSegment();

    bool shouldIndentText() const { return m_shouldIndentText == IndentText; }

private:
    void computeAvailableWidthFromLeftAndRight();
    void updateLineDimension(LayoutUnit newLineTop, LayoutUnit newLineWidth, const float& newLineLeft, const float& newLineRight);
    void wrapNextToShapeOutside(bool isFirstLine);

    RenderBlockFlow& m_block;
    float m_uncommittedWidth;
    float m_committedWidth;
    float m_overhangWidth; // The amount by which |m_availableWidth| has been inflated to account for possible contraction due to ruby overhang.
    float m_left;
    float m_right;
    float m_availableWidth;
    const LineSegment* m_segment;
    bool m_isFirstLine;
    IndentTextOrNot m_shouldIndentText;
};

}

#endif // LineWidth_h
