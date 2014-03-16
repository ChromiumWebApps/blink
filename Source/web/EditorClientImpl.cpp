/*
 * Copyright (C) 2006, 2007 Apple, Inc.  All rights reserved.
 * Copyright (C) 2012 Google, Inc.  All rights reserved.
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
#include "EditorClientImpl.h"

#include "WebFrameImpl.h"
#include "WebPermissionClient.h"
#include "WebViewClient.h"
#include "WebViewImpl.h"
#include "core/editing/SelectionType.h"

using namespace WebCore;

namespace blink {

EditorClientImpl::EditorClientImpl(WebViewImpl* webview)
    : m_webView(webview)
{
}

EditorClientImpl::~EditorClientImpl()
{
}

void EditorClientImpl::respondToChangedSelection(WebCore::SelectionType selectionType)
{
    if (m_webView->client())
        m_webView->client()->didChangeSelection(selectionType != WebCore::RangeSelection);
}

void EditorClientImpl::respondToChangedContents()
{
    if (m_webView->client())
        m_webView->client()->didChangeContents();
}

bool EditorClientImpl::canCopyCut(LocalFrame* frame, bool defaultValue) const
{
    WebFrameImpl* webFrame = WebFrameImpl::fromFrame(frame);
    if (!webFrame->permissionClient())
        return defaultValue;
    return webFrame->permissionClient()->allowWriteToClipboard(webFrame, defaultValue);
}

bool EditorClientImpl::canPaste(LocalFrame* frame, bool defaultValue) const
{
    WebFrameImpl* webFrame = WebFrameImpl::fromFrame(frame);
    if (!webFrame->permissionClient())
        return defaultValue;
    return webFrame->permissionClient()->allowReadFromClipboard(webFrame, defaultValue);
}

void EditorClientImpl::didExecuteCommand(String commandName)
{
    if (m_webView->client())
        m_webView->client()->didExecuteCommand(WebString(commandName));
}

bool EditorClientImpl::handleKeyboardEvent()
{
    return m_webView->client() && m_webView->client()->handleCurrentKeyboardEvent();
}

} // namesace blink
