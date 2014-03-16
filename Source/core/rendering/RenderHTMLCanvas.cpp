/*
 * Copyright (C) 2004, 2006, 2007 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
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
#include "core/rendering/RenderHTMLCanvas.h"

#include "core/html/HTMLCanvasElement.h"
#include "core/html/canvas/CanvasRenderingContext.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/page/Page.h"
#include "core/rendering/PaintInfo.h"
#include "core/rendering/RenderView.h"

namespace WebCore {

using namespace HTMLNames;

RenderHTMLCanvas::RenderHTMLCanvas(HTMLCanvasElement* element)
    : RenderReplaced(element, element->size())
{
    view()->frameView()->setIsVisuallyNonEmpty();
}

LayerType RenderHTMLCanvas::layerTypeRequired() const
{
    LayerType type = RenderReplaced::layerTypeRequired();
    if (type != NoLayer)
        return type;

    HTMLCanvasElement* canvas = toHTMLCanvasElement(node());
    if (canvas && canvas->renderingContext() && canvas->renderingContext()->isAccelerated())
        return NormalLayer;

    return NoLayer;
}

void RenderHTMLCanvas::paintReplaced(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    GraphicsContext* context = paintInfo.context;

    LayoutRect contentRect = contentBoxRect();
    contentRect.moveBy(paintOffset);
    LayoutRect paintRect = replacedContentRect();
    paintRect.moveBy(paintOffset);

    bool clip = !contentRect.contains(paintRect);
    if (clip) {
        // Not allowed to overflow the content box.
        paintInfo.context->save();
        paintInfo.context->clip(pixelSnappedIntRect(contentRect));
    }

    bool useLowQualityScale = style()->imageRendering() == ImageRenderingOptimizeContrast;
    toHTMLCanvasElement(node())->paint(context, paintRect, useLowQualityScale);

    if (clip)
        context->restore();
}

void RenderHTMLCanvas::canvasSizeChanged()
{
    IntSize canvasSize = toHTMLCanvasElement(node())->size();
    LayoutSize zoomedSize(canvasSize.width() * style()->effectiveZoom(), canvasSize.height() * style()->effectiveZoom());

    if (zoomedSize == intrinsicSize())
        return;

    setIntrinsicSize(zoomedSize);

    if (!parent())
        return;

    if (!preferredLogicalWidthsDirty())
        setPreferredLogicalWidthsDirty();

    LayoutSize oldSize = size();
    updateLogicalWidth();
    updateLogicalHeight();
    if (oldSize == size())
        return;

    if (!selfNeedsLayout())
        setNeedsLayout();
}

} // namespace WebCore
