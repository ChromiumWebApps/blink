/*
 * Copyright (C) 2007, 2008, 2009, 2010 Apple Inc. All rights reserved.
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

#include "core/rendering/RenderMedia.h"

#include "core/html/HTMLMediaElement.h"
#include "core/rendering/LayoutRectRecorder.h"
#include "core/rendering/RenderView.h"

namespace WebCore {

RenderMedia::RenderMedia(HTMLMediaElement* video)
    : RenderImage(video)
{
    setImageResource(RenderImageResource::create());
}

RenderMedia::~RenderMedia()
{
}

HTMLMediaElement* RenderMedia::mediaElement() const
{
    return toHTMLMediaElement(node());
}

void RenderMedia::layout()
{
    LayoutRectRecorder recorder(*this);
    LayoutSize oldSize = contentBoxRect().size();

    RenderImage::layout();

    RenderBox* controlsRenderer = toRenderBox(m_children.firstChild());
    if (!controlsRenderer)
        return;

    bool controlsNeedLayout = controlsRenderer->needsLayout();
    LayoutSize newSize = contentBoxRect().size();
    if (newSize == oldSize && !controlsNeedLayout)
        return;

    LayoutStateMaintainer statePusher(*this, locationOffset());

    controlsRenderer->setLocation(LayoutPoint(borderLeft(), borderTop()) + LayoutSize(paddingLeft(), paddingTop()));
    controlsRenderer->style()->setHeight(Length(newSize.height(), Fixed));
    controlsRenderer->style()->setWidth(Length(newSize.width(), Fixed));
    controlsRenderer->forceLayout();
    clearNeedsLayout();

    statePusher.pop();
}

void RenderMedia::paintReplaced(PaintInfo&, const LayoutPoint&)
{
}

} // namespace WebCore
