/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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
#include "WebRange.h"

#include "WebExceptionCode.h"
#include "WebFrameImpl.h"
#include "WebNode.h"
#include "bindings/v8/ExceptionState.h"
#include "bindings/v8/ExceptionStatePlaceholder.h"
#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/dom/Range.h"
#include "core/dom/shadow/ShadowRoot.h"
#include "core/editing/FrameSelection.h"
#include "core/editing/PlainTextRange.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "public/platform/WebFloatQuad.h"
#include "public/platform/WebString.h"
#include "wtf/PassRefPtr.h"

using namespace WebCore;

namespace blink {

class WebRangePrivate : public Range {
};

void WebRange::reset()
{
    assign(0);
}

void WebRange::assign(const WebRange& other)
{
    WebRangePrivate* p = const_cast<WebRangePrivate*>(other.m_private);
    if (p)
        p->ref();
    assign(p);
}

int WebRange::startOffset() const
{
    return m_private->startOffset();
}

int WebRange::endOffset() const
{
    return m_private->endOffset();
}

WebNode WebRange::startContainer(WebExceptionCode& exceptionCode) const
{
    // FIXME: Create a wrapper class that just sets the internal int.
    TrackExceptionState exceptionState;
    RefPtr<Node> node(m_private->startContainer(exceptionState));
    exceptionCode = exceptionState.code();
    return node.release();
}

WebNode WebRange::endContainer(WebExceptionCode& exceptionCode) const
{
    // FIXME: Create a wrapper class that just sets the internal int.
    TrackExceptionState exceptionState;
    RefPtr<Node> node(m_private->endContainer(exceptionState));
    exceptionCode = exceptionState.code();
    return node.release();
}

WebString WebRange::toHTMLText() const
{
    return m_private->toHTML();
}

WebString WebRange::toPlainText() const
{
    return m_private->text();
}

WebRange WebRange::expandedToParagraph() const
{
    WebRange copy(*this);
    copy.m_private->expand("block", IGNORE_EXCEPTION);
    return copy;
}

// static
WebRange WebRange::fromDocumentRange(WebFrame* frame, int start, int length)
{
    WebCore::LocalFrame* webFrame = toWebFrameImpl(frame)->frame();
    Element* selectionRoot = webFrame->selection().rootEditableElement();
    ContainerNode* scope = selectionRoot ? selectionRoot : webFrame->document()->documentElement();
    return PlainTextRange(start, start + length).createRange(*scope);
}

WebVector<WebFloatQuad> WebRange::textQuads() const
{
    if (isNull())
        return WebVector<WebFloatQuad>();

    LocalFrame* frame = m_private->ownerDocument().frame();
    if (!frame)
        return WebVector<WebFloatQuad>();

    Vector<FloatQuad> quads;
    m_private->textQuads(quads);
    for (unsigned i = 0; i < quads.size(); ++i) {
        quads[i].setP1(frame->view()->contentsToWindow(roundedIntPoint(quads[i].p1())));
        quads[i].setP2(frame->view()->contentsToWindow(roundedIntPoint(quads[i].p2())));
        quads[i].setP3(frame->view()->contentsToWindow(roundedIntPoint(quads[i].p3())));
        quads[i].setP4(frame->view()->contentsToWindow(roundedIntPoint(quads[i].p4())));
    }

    return quads;
}

WebRange::WebRange(const WTF::PassRefPtr<WebCore::Range>& range)
    : m_private(static_cast<WebRangePrivate*>(range.leakRef()))
{
}

WebRange& WebRange::operator=(const WTF::PassRefPtr<WebCore::Range>& range)
{
    assign(static_cast<WebRangePrivate*>(range.leakRef()));
    return *this;
}

WebRange::operator WTF::PassRefPtr<WebCore::Range>() const
{
    return PassRefPtr<Range>(const_cast<WebRangePrivate*>(m_private));
}

void WebRange::assign(WebRangePrivate* p)
{
    // p is already ref'd for us by the caller
    if (m_private)
        m_private->deref();
    m_private = p;
}

} // namespace blink
