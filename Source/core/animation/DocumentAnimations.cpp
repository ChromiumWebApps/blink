/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "core/animation/DocumentAnimations.h"

#include "core/animation/ActiveAnimations.h"
#include "core/animation/AnimationClock.h"
#include "core/animation/DocumentTimeline.h"
#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/dom/Node.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/rendering/RenderView.h"
#include "core/rendering/compositing/RenderLayerCompositor.h"

namespace WebCore {

namespace {

void updateAnimationTiming(Document& document)
{
    document.timeline().serviceAnimations();
    document.transitionTimeline().serviceAnimations();
}

} // namespace

void DocumentAnimations::updateAnimationTimingForAnimationFrame(Document& document, double monotonicAnimationStartTime)
{
    document.animationClock().updateTime(monotonicAnimationStartTime);
    updateAnimationTiming(document);
}

void DocumentAnimations::updateAnimationTimingForGetComputedStyle(Node& node, CSSPropertyID property)
{
    if (!node.isElementNode())
        return;
    const Element& element = toElement(node);
    if (element.document().timeline().hasOutdatedPlayer()) {
        updateAnimationTiming(element.document());
        return;
    }
    if (const ActiveAnimations* activeAnimations = element.activeAnimations()) {
        if (activeAnimations->hasActiveAnimationsOnCompositor(property))
            updateAnimationTiming(element.document());
    }
}

void DocumentAnimations::startPendingAnimations(Document& document)
{
    ASSERT(document.lifecycle().state() == DocumentLifecycle::CompositingClean);
    if (document.cssPendingAnimations().startPendingAnimations()) {
        ASSERT(document.view());
        document.view()->scheduleAnimation();
    }

    document.animationClock().unfreeze();
}

} // namespace WebCore
