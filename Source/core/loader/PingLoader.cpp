/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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
 *
 */

#include "config.h"
#include "core/loader/PingLoader.h"

#include "FetchInitiatorTypeNames.h"
#include "core/dom/Document.h"
#include "core/fetch/FetchContext.h"
#include "core/frame/LocalFrame.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "core/loader/FrameLoader.h"
#include "core/loader/FrameLoaderClient.h"
#include "core/loader/UniqueIdentifier.h"
#include "core/page/Page.h"
#include "platform/exported/WrappedResourceRequest.h"
#include "platform/network/FormData.h"
#include "platform/network/ResourceError.h"
#include "platform/network/ResourceRequest.h"
#include "platform/network/ResourceResponse.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "platform/weborigin/SecurityPolicy.h"
#include "public/platform/Platform.h"
#include "public/platform/WebURLLoader.h"
#include "public/platform/WebURLResponse.h"
#include "wtf/OwnPtr.h"

namespace WebCore {

void PingLoader::loadImage(LocalFrame* frame, const KURL& url)
{
    if (!frame->document()->securityOrigin()->canDisplay(url)) {
        FrameLoader::reportLocalLoadFailed(frame, url.string());
        return;
    }

    ResourceRequest request(url);
    request.setTargetType(ResourceRequest::TargetIsPing);
    request.setHTTPHeaderField("Cache-Control", "max-age=0");
    frame->loader().fetchContext().addAdditionalRequestHeaders(frame->document(), request, FetchSubresource);

    FetchInitiatorInfo initiatorInfo;
    initiatorInfo.name = FetchInitiatorTypeNames::ping;
    PingLoader::start(frame, request, initiatorInfo);
}

// http://www.whatwg.org/specs/web-apps/current-work/multipage/links.html#hyperlink-auditing
void PingLoader::sendPing(LocalFrame* frame, const KURL& pingURL, const KURL& destinationURL)
{
    ResourceRequest request(pingURL);
    request.setTargetType(ResourceRequest::TargetIsPing);
    request.setHTTPMethod("POST");
    request.setHTTPContentType("text/ping");
    request.setHTTPBody(FormData::create("PING"));
    request.setHTTPHeaderField("Cache-Control", "max-age=0");
    frame->loader().fetchContext().addAdditionalRequestHeaders(frame->document(), request, FetchSubresource);

    RefPtr<SecurityOrigin> pingOrigin = SecurityOrigin::create(pingURL);
    // addAdditionalRequestHeaders() will have added a referrer for same origin requests,
    // but the spec omits the referrer for same origin.
    if (frame->document()->securityOrigin()->isSameSchemeHostPort(pingOrigin.get()))
        request.clearHTTPReferrer();

    request.setHTTPHeaderField("Ping-To", AtomicString(destinationURL.string()));

    // Ping-From follows the same rules as the default referrer beahavior for subresource requests.
    // FIXME: Should Ping-From obey ReferrerPolicy?
    if (!SecurityPolicy::shouldHideReferrer(pingURL, frame->document()->url().string()))
        request.setHTTPHeaderField("Ping-From", AtomicString(frame->document()->url().string()));

    FetchInitiatorInfo initiatorInfo;
    initiatorInfo.name = FetchInitiatorTypeNames::ping;
    PingLoader::start(frame, request, initiatorInfo);
}

void PingLoader::sendViolationReport(LocalFrame* frame, const KURL& reportURL, PassRefPtr<FormData> report, ViolationReportType type)
{
    ResourceRequest request(reportURL);
    request.setTargetType(ResourceRequest::TargetIsSubresource);
    request.setHTTPMethod("POST");
    request.setHTTPContentType(type == ContentSecurityPolicyViolationReport ? "application/csp-report" : "application/json");
    request.setHTTPBody(report);
    frame->loader().fetchContext().addAdditionalRequestHeaders(frame->document(), request, FetchSubresource);

    FetchInitiatorInfo initiatorInfo;
    initiatorInfo.name = FetchInitiatorTypeNames::violationreport;
    PingLoader::start(frame, request, initiatorInfo, SecurityOrigin::create(reportURL)->isSameSchemeHostPort(frame->document()->securityOrigin()) ? AllowStoredCredentials : DoNotAllowStoredCredentials);
}

void PingLoader::start(LocalFrame* frame, ResourceRequest& request, const FetchInitiatorInfo& initiatorInfo, StoredCredentials credentialsAllowed)
{
    OwnPtr<PingLoader> pingLoader = adoptPtr(new PingLoader(frame, request, initiatorInfo, credentialsAllowed));

    // Leak the ping loader, since it will kill itself as soon as it receives a response.
    PingLoader* ALLOW_UNUSED leakedPingLoader = pingLoader.leakPtr();
}

PingLoader::PingLoader(LocalFrame* frame, ResourceRequest& request, const FetchInitiatorInfo& initiatorInfo, StoredCredentials credentialsAllowed)
    : PageLifecycleObserver(frame->page())
    , m_timeout(this, &PingLoader::timeout)
    , m_url(request.url())
    , m_identifier(createUniqueIdentifier())
{
    frame->loader().client()->didDispatchPingLoader(request.url());

    m_loader = adoptPtr(blink::Platform::current()->createURLLoader());
    ASSERT(m_loader);
    blink::WrappedResourceRequest wrappedRequest(request);
    wrappedRequest.setAllowStoredCredentials(credentialsAllowed == AllowStoredCredentials);
    m_loader->loadAsynchronously(wrappedRequest, this);

    InspectorInstrumentation::willSendRequest(frame, m_identifier, frame->loader().documentLoader(), request, ResourceResponse(), initiatorInfo);

    // If the server never responds, FrameLoader won't be able to cancel this load and
    // we'll sit here waiting forever. Set a very generous timeout, just in case.
    m_timeout.startOneShot(60000, FROM_HERE);
}

PingLoader::~PingLoader()
{
    if (m_loader)
        m_loader->cancel();
}

void PingLoader::didReceiveResponse(blink::WebURLLoader*, const blink::WebURLResponse&)
{
    if (Page* page = this->page())
        InspectorInstrumentation::didFailLoading(page->mainFrame(), m_identifier, ResourceError::cancelledError(m_url));
    delete this;
}

void PingLoader::didReceiveData(blink::WebURLLoader*, const char* data, int dataLength, int encodedDataLength)
{
    if (Page* page = this->page())
        InspectorInstrumentation::didFailLoading(page->mainFrame(), m_identifier, ResourceError::cancelledError(m_url));
    delete this;
}

void PingLoader::didFinishLoading(blink::WebURLLoader*, double, int64_t)
{
    if (Page* page = this->page())
        InspectorInstrumentation::didFailLoading(page->mainFrame(), m_identifier, ResourceError::cancelledError(m_url));
    delete this;
}

void PingLoader::didFail(blink::WebURLLoader*, const blink::WebURLError& resourceError)
{
    if (Page* page = this->page())
        InspectorInstrumentation::didFailLoading(page->mainFrame(), m_identifier, ResourceError(resourceError));
    delete this;
}

void PingLoader::timeout(Timer<PingLoader>*)
{
    if (Page* page = this->page())
        InspectorInstrumentation::didFailLoading(page->mainFrame(), m_identifier, ResourceError::cancelledError(m_url));
    delete this;
}

}
