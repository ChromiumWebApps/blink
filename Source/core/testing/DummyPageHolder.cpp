/*
 * Copyright (c) 2013, Google Inc. All rights reserved.
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
#include "core/testing/DummyPageHolder.h"

#include "core/frame/DOMWindow.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "wtf/Assertions.h"

namespace WebCore {

PassOwnPtr<DummyPageHolder> DummyPageHolder::create(const IntSize& initialViewSize)
{
    return adoptPtr(new DummyPageHolder(initialViewSize));
}

DummyPageHolder::DummyPageHolder(const IntSize& initialViewSize)
{
    m_pageClients.chromeClient = &m_chromeClient;
    m_pageClients.contextMenuClient = &m_contextMenuClient;
    m_pageClients.editorClient = &m_editorClient;
    m_pageClients.dragClient = &m_dragClient;
    m_pageClients.inspectorClient = &m_inspectorClient;
    m_pageClients.backForwardClient = &m_backForwardClient;

    m_page = adoptPtr(new Page(m_pageClients));

    m_frame = LocalFrame::create(&m_frameLoaderClient, &m_page->frameHost(), 0);
    m_frame->setView(FrameView::create(m_frame.get(), initialViewSize));
    m_frame->init();
}

DummyPageHolder::~DummyPageHolder()
{
    m_page.clear();
    ASSERT(m_frame->hasOneRef());
    m_frame.clear();
}

Page& DummyPageHolder::page() const
{
    return *m_page;
}

LocalFrame& DummyPageHolder::frame() const
{
    return *m_frame;
}

FrameView& DummyPageHolder::frameView() const
{
    return *m_frame->view();
}

Document& DummyPageHolder::document() const
{
    return *m_frame->domWindow()->document();
}

} // namespace WebCore
