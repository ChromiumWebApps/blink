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
#include "core/frame/FrameHost.h"

#include "core/frame/PageConsole.h"
#include "core/page/Chrome.h"
#include "core/page/ChromeClient.h"
#include "core/page/Page.h"

namespace WebCore {

PassOwnPtr<FrameHost> FrameHost::create(Page& page)
{
    return adoptPtr(new FrameHost(page));
}

FrameHost::FrameHost(Page& page)
    : m_page(page)
    , m_console(PageConsole::create(*this))
    , m_pinchViewport(*this)
{
}

// Explicitly in the .cpp to avoid default constructor in .h
FrameHost::~FrameHost()
{
}

Settings& FrameHost::settings() const
{
    return m_page.settings();
}

Chrome& FrameHost::chrome() const
{
    return m_page.chrome();
}

PageConsole& FrameHost::console() const
{
    return *m_console;
}

UseCounter& FrameHost::useCounter() const
{
    return m_page.useCounter();
}

float FrameHost::deviceScaleFactor() const
{
    return m_page.deviceScaleFactor();
}

PinchViewport& FrameHost::pinchViewport()
{
    return m_pinchViewport;
}

}
