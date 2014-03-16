/*
 * Copyright (C) 2012 Adobe Systems Incorporated. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER “AS IS” AND ANY
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
#include "core/rendering/shapes/ShapeInsideInfo.h"

#include "core/rendering/InlineIterator.h"
#include "core/rendering/RenderBlock.h"

namespace WebCore {

LineSegmentRange::LineSegmentRange(const InlineIterator& start, const InlineIterator& end)
    : start(start.root(), start.object(), start.offset())
    , end(end.root(), end.object(), end.offset())
    {
    }

bool ShapeInsideInfo::isEnabledFor(const RenderBlock& renderer)
{
    ShapeValue* shapeValue = renderer.style()->resolvedShapeInside();
    if (!shapeValue)
        return false;

    switch (shapeValue->type()) {
    case ShapeValue::Shape:
        return shapeValue->shape() && shapeValue->shape()->type() != BasicShape::BasicShapeInsetRectangleType && shapeValue->shape()->type() != BasicShape::BasicShapeInsetType;
    case ShapeValue::Image:
        return shapeValue->isImageValid() && checkShapeImageOrigin(renderer.document(), *(shapeValue->image()->cachedImage()));
    case ShapeValue::Box:
        return true;
    case ShapeValue::Outside:
        return false;
    }

    return false;
}

bool ShapeInsideInfo::updateSegmentsForLine(LayoutSize lineOffset, LayoutUnit lineHeight)
{
    bool result = updateSegmentsForLine(lineOffset.height(), lineHeight);
    for (size_t i = 0; i < m_segments.size(); i++) {
        m_segments[i].logicalLeft -= lineOffset.width();
        m_segments[i].logicalRight -= lineOffset.width();
    }
    return result;
}

bool ShapeInsideInfo::updateSegmentsForLine(LayoutUnit lineTop, LayoutUnit lineHeight)
{
    ASSERT(lineHeight >= 0);
    m_referenceBoxLineTop = lineTop - logicalTopOffset();
    m_lineHeight = lineHeight;
    m_segments.clear();
    m_segmentRanges.clear();

    if (lineOverlapsShapeBounds())
        m_segments = computeSegmentsForLine(lineTop, lineHeight);

    return m_segments.size();
}

bool ShapeInsideInfo::adjustLogicalLineTop(float minSegmentWidth)
{
    const Shape& shape = computedShape();
    if (m_lineHeight <= 0 || logicalLineTop() > shapeLogicalBottom())
        return false;

    LayoutUnit newLineTop;
    if (shape.firstIncludedIntervalLogicalTop(m_referenceBoxLineTop, FloatSize(minSegmentWidth, m_lineHeight.toFloat()), newLineTop)) {
        if (newLineTop > m_referenceBoxLineTop) {
            m_referenceBoxLineTop = newLineTop;
            return true;
        }
    }

    return false;
}

ShapeValue* ShapeInsideInfo::shapeValue() const
{
    return m_renderer.style()->resolvedShapeInside();
}

const RenderStyle* ShapeInsideInfo::styleForWritingMode() const
{
    return m_renderer.style();
}

LayoutUnit ShapeInsideInfo::computeFirstFitPositionForFloat(const FloatSize& floatSize) const
{
    if (!floatSize.width() || shapeLogicalBottom() < logicalLineTop())
        return 0;

    LayoutUnit firstFitPosition = 0;
    if (computedShape().firstIncludedIntervalLogicalTop(m_referenceBoxLineTop, floatSize, firstFitPosition) && (m_referenceBoxLineTop <= firstFitPosition))
        return firstFitPosition;

    return 0;
}

}
