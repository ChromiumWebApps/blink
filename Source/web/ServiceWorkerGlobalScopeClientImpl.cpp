/*
 * Copyright (C) 2014 Google Inc. All rights reserved.
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
#include "ServiceWorkerGlobalScopeClientImpl.h"

#include "WebServiceWorkerContextClient.h"
#include "modules/serviceworkers/Response.h"
#include "platform/NotImplemented.h"
#include "public/platform/WebServiceWorkerResponse.h"
#include "wtf/PassOwnPtr.h"

namespace blink {

PassOwnPtr<WebCore::ServiceWorkerGlobalScopeClient> ServiceWorkerGlobalScopeClientImpl::create(PassOwnPtr<WebServiceWorkerContextClient> client)
{
    return adoptPtr(new ServiceWorkerGlobalScopeClientImpl(client));
}

ServiceWorkerGlobalScopeClientImpl::~ServiceWorkerGlobalScopeClientImpl()
{
}

void ServiceWorkerGlobalScopeClientImpl::didHandleInstallEvent(int installEventID)
{
    if (m_client)
        m_client->didHandleInstallEvent(installEventID);
}

void ServiceWorkerGlobalScopeClientImpl::didHandleFetchEvent(int fetchEventID, PassRefPtr<WebCore::Response> response)
{
    if (!m_client)
        return;
    if (!response) {
        m_client->didHandleFetchEvent(fetchEventID);
        return;
    }

    WebServiceWorkerResponse webResponse;
    response->populateWebServiceWorkerResponse(webResponse);
    m_client->didHandleFetchEvent(fetchEventID, webResponse);
}

ServiceWorkerGlobalScopeClientImpl::ServiceWorkerGlobalScopeClientImpl(PassOwnPtr<WebServiceWorkerContextClient> client)
    : m_client(client)
{
}

} // namespace blink
