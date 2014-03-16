/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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
#include "core/rendering/RenderIFrame.h"

#include "HTMLNames.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/html/HTMLIFrameElement.h"
#include "core/rendering/LayoutRectRecorder.h"
#include "core/rendering/RenderView.h"

namespace WebCore {

using namespace HTMLNames;

RenderIFrame::RenderIFrame(Element* element)
    : RenderPart(element)
{
}

bool RenderIFrame::shouldComputeSizeAsReplaced() const
{
    return true;
}

bool RenderIFrame::isInlineBlockOrInlineTable() const
{
    return isInline();
}

LayerType RenderIFrame::layerTypeRequired() const
{
    LayerType type = RenderPart::layerTypeRequired();
    if (type != NoLayer)
        return type;

    if (style()->resize() != RESIZE_NONE)
        return NormalLayer;

    return ForcedLayer;
}

RenderView* RenderIFrame::contentRootRenderer() const
{
    // FIXME: Is this always a valid cast? What about plugins?
    ASSERT(!widget() || widget()->isFrameView());
    FrameView* childFrameView = toFrameView(widget());
    return childFrameView ? childFrameView->frame().contentRenderer() : 0;
}

void RenderIFrame::layout()
{
    ASSERT(needsLayout());

    LayoutRectRecorder recorder(*this);
    updateLogicalWidth();
    // No kids to layout as a replaced element.
    updateLogicalHeight();

    m_overflow.clear();
    addVisualEffectOverflow();
    updateLayerTransform();

    clearNeedsLayout();
}

}
