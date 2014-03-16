/*
 * Copyright (C) 2009 Google Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY GOOGLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL GOOGLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "StorageClientImpl.h"

#include "WebFrameImpl.h"
#include "WebViewImpl.h"
#include "core/storage/StorageNamespace.h"
#include "public/platform/WebStorageNamespace.h"
#include "public/web/WebPermissionClient.h"
#include "public/web/WebViewClient.h"

namespace blink {

StorageClientImpl::StorageClientImpl(WebViewImpl* webView)
    : m_webView(webView)
{
}

PassOwnPtr<WebCore::StorageNamespace> StorageClientImpl::createSessionStorageNamespace()
{
    return adoptPtr(new WebCore::StorageNamespace(adoptPtr(m_webView->client()->createSessionStorageNamespace())));
}

bool StorageClientImpl::canAccessStorage(WebCore::LocalFrame* frame, WebCore::StorageType type) const
{
    WebFrameImpl* webFrame = WebFrameImpl::fromFrame(frame);
    return !webFrame->permissionClient() || webFrame->permissionClient()->allowStorage(webFrame, type == WebCore::LocalStorage);
}

} // namespace blink
