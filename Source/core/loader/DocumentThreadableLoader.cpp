/*
 * Copyright (C) 2011, 2012 Google Inc. All rights reserved.
 * Copyright (C) 2013, Intel Corporation
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
#include "core/loader/DocumentThreadableLoader.h"

#include "core/dom/Document.h"
#include "core/fetch/CrossOriginAccessControl.h"
#include "core/fetch/FetchRequest.h"
#include "core/fetch/Resource.h"
#include "core/fetch/ResourceFetcher.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/csp/ContentSecurityPolicy.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "core/loader/CrossOriginPreflightResultCache.h"
#include "core/loader/DocumentThreadableLoaderClient.h"
#include "core/loader/FrameLoader.h"
#include "core/loader/ThreadableLoaderClient.h"
#include "platform/SharedBuffer.h"
#include "platform/network/ResourceRequest.h"
#include "platform/weborigin/SchemeRegistry.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "wtf/Assertions.h"

namespace WebCore {

void DocumentThreadableLoader::loadResourceSynchronously(Document* document, const ResourceRequest& request, ThreadableLoaderClient& client, const ThreadableLoaderOptions& options)
{
    // The loader will be deleted as soon as this function exits.
    RefPtr<DocumentThreadableLoader> loader = adoptRef(new DocumentThreadableLoader(document, &client, LoadSynchronously, request, options));
    ASSERT(loader->hasOneRef());
}

PassRefPtr<DocumentThreadableLoader> DocumentThreadableLoader::create(Document* document, ThreadableLoaderClient* client, const ResourceRequest& request, const ThreadableLoaderOptions& options)
{
    RefPtr<DocumentThreadableLoader> loader = adoptRef(new DocumentThreadableLoader(document, client, LoadAsynchronously, request, options));
    if (!loader->resource())
        loader = nullptr;
    return loader.release();
}

DocumentThreadableLoader::DocumentThreadableLoader(Document* document, ThreadableLoaderClient* client, BlockingBehavior blockingBehavior, const ResourceRequest& request, const ThreadableLoaderOptions& options)
    : m_client(client)
    , m_document(document)
    , m_options(options)
    , m_sameOriginRequest(securityOrigin()->canRequest(request.url()))
    , m_simpleRequest(true)
    , m_async(blockingBehavior == LoadAsynchronously)
    , m_timeoutTimer(this, &DocumentThreadableLoader::didTimeout)
{
    ASSERT(document);
    ASSERT(client);
    // Setting an outgoing referer is only supported in the async code path.
    ASSERT(m_async || request.httpReferrer().isEmpty());

    if (m_sameOriginRequest || m_options.crossOriginRequestPolicy == AllowCrossOriginRequests) {
        loadRequest(request);
        return;
    }

    if (m_options.crossOriginRequestPolicy == DenyCrossOriginRequests) {
        m_client->didFail(ResourceError(errorDomainBlinkInternal, 0, request.url().string(), "Cross origin requests are not supported."));
        return;
    }

    makeCrossOriginAccessRequest(request);
}

void DocumentThreadableLoader::makeCrossOriginAccessRequest(const ResourceRequest& request)
{
    ASSERT(m_options.crossOriginRequestPolicy == UseAccessControl);

    if ((m_options.preflightPolicy == ConsiderPreflight && isSimpleCrossOriginAccessRequest(request.httpMethod(), request.httpHeaderFields())) || m_options.preflightPolicy == PreventPreflight) {
        // Cross-origin requests are only allowed for HTTP and registered schemes. We would catch this when checking response headers later, but there is no reason to send a request that's guaranteed to be denied.
        if (!SchemeRegistry::shouldTreatURLSchemeAsCORSEnabled(request.url().protocol())) {
            m_client->didFailAccessControlCheck(ResourceError(errorDomainBlinkInternal, 0, request.url().string(), "Cross origin requests are only supported for HTTP."));
            return;
        }

        ResourceRequest crossOriginRequest(request);
        updateRequestForAccessControl(crossOriginRequest, securityOrigin(), m_options.allowCredentials);
        loadRequest(crossOriginRequest);
    } else {
        m_simpleRequest = false;

        OwnPtr<ResourceRequest> crossOriginRequest = adoptPtr(new ResourceRequest(request));
        // Do not set the Origin header for preflight requests.
        updateRequestForAccessControl(*crossOriginRequest, 0, m_options.allowCredentials);
        m_actualRequest = crossOriginRequest.release();

        if (CrossOriginPreflightResultCache::shared().canSkipPreflight(securityOrigin()->toString(), m_actualRequest->url(), m_options.allowCredentials, m_actualRequest->httpMethod(), m_actualRequest->httpHeaderFields())) {
            preflightSuccess();
        } else {
            ResourceRequest preflightRequest = createAccessControlPreflightRequest(*m_actualRequest, securityOrigin());
            loadRequest(preflightRequest);
        }
    }
}

DocumentThreadableLoader::~DocumentThreadableLoader()
{
}

void DocumentThreadableLoader::cancel()
{
    cancelWithError(ResourceError());
}

void DocumentThreadableLoader::cancelWithError(const ResourceError& error)
{
    RefPtr<DocumentThreadableLoader> protect(this);

    // Cancel can re-enter and m_resource might be null here as a result.
    if (m_client && resource()) {
        ResourceError errorForCallback = error;
        if (errorForCallback.isNull()) {
            // FIXME: This error is sent to the client in didFail(), so it should not be an internal one. Use FrameLoaderClient::cancelledError() instead.
            errorForCallback = ResourceError(errorDomainBlinkInternal, 0, resource()->url().string(), "Load cancelled");
            errorForCallback.setIsCancellation(true);
        }
        m_client->didFail(errorForCallback);
    }
    clearResource();
    m_client = 0;
}

void DocumentThreadableLoader::setDefersLoading(bool value)
{
    if (resource())
        resource()->setDefersLoading(value);
}

void DocumentThreadableLoader::redirectReceived(Resource* resource, ResourceRequest& request, const ResourceResponse& redirectResponse)
{
    ASSERT(m_client);
    ASSERT_UNUSED(resource, resource == this->resource());

    RefPtr<DocumentThreadableLoader> protect(this);
    if (!isAllowedByPolicy(request.url())) {
        m_client->didFailRedirectCheck();
        request = ResourceRequest();
        return;
    }

    // Allow same origin requests to continue after allowing clients to audit the redirect.
    if (isAllowedRedirect(request.url())) {
        if (m_client->isDocumentThreadableLoaderClient())
            static_cast<DocumentThreadableLoaderClient*>(m_client)->willSendRequest(request, redirectResponse);
        return;
    }

    // When using access control, only simple cross origin requests are allowed to redirect. The new request URL must have a supported
    // scheme and not contain the userinfo production. In addition, the redirect response must pass the access control check if the
    // original request was not same-origin.
    if (m_options.crossOriginRequestPolicy == UseAccessControl) {

        InspectorInstrumentation::didReceiveCORSRedirectResponse(m_document->frame(), resource->identifier(), m_document->frame()->loader().documentLoader(), redirectResponse, 0);

        bool allowRedirect = false;
        String accessControlErrorDescription;

        if (m_simpleRequest) {
            allowRedirect = CrossOriginAccessControl::isLegalRedirectLocation(request.url(), accessControlErrorDescription)
                            && (m_sameOriginRequest || passesAccessControlCheck(redirectResponse, m_options.allowCredentials, securityOrigin(), accessControlErrorDescription));
        } else {
            accessControlErrorDescription = "The request was redirected to '"+ request.url().string() + "', which is disallowed for cross-origin requests that require preflight.";
        }

        if (allowRedirect) {
            // FIXME: consider combining this with CORS redirect handling performed by
            // CrossOriginAccessControl::handleRedirect().
            clearResource();

            RefPtr<SecurityOrigin> originalOrigin = SecurityOrigin::create(redirectResponse.url());
            RefPtr<SecurityOrigin> requestOrigin = SecurityOrigin::create(request.url());
            // If the original request wasn't same-origin, then if the request URL origin is not same origin with the original URL origin,
            // set the source origin to a globally unique identifier. (If the original request was same-origin, the origin of the new request
            // should be the original URL origin.)
            if (!m_sameOriginRequest && !originalOrigin->isSameSchemeHostPort(requestOrigin.get()))
                m_options.securityOrigin = SecurityOrigin::createUnique();
            // Force any subsequent requests to use these checks.
            m_sameOriginRequest = false;

            // Since the request is no longer same-origin, if the user didn't request credentials in
            // the first place, update our state so we neither request them nor expect they must be allowed.
            if (m_options.credentialsRequested == ClientDidNotRequestCredentials)
                m_options.allowCredentials = DoNotAllowStoredCredentials;

            // Remove any headers that may have been added by the network layer that cause access control to fail.
            request.clearHTTPContentType();
            request.clearHTTPReferrer();
            request.clearHTTPOrigin();
            request.clearHTTPUserAgent();
            request.clearHTTPAccept();
            makeCrossOriginAccessRequest(request);
            return;
        }

        ResourceError error(errorDomainBlinkInternal, 0, redirectResponse.url().string(), accessControlErrorDescription);
        m_client->didFailAccessControlCheck(error);
    } else {
        m_client->didFailRedirectCheck();
    }
    request = ResourceRequest();
}

void DocumentThreadableLoader::dataSent(Resource* resource, unsigned long long bytesSent, unsigned long long totalBytesToBeSent)
{
    ASSERT(m_client);
    ASSERT_UNUSED(resource, resource == this->resource());
    m_client->didSendData(bytesSent, totalBytesToBeSent);
}

void DocumentThreadableLoader::dataDownloaded(Resource* resource, int dataLength)
{
    ASSERT(m_client);
    ASSERT_UNUSED(resource, resource == this->resource());
    ASSERT(!m_actualRequest);

    m_client->didDownloadData(dataLength);
}

void DocumentThreadableLoader::responseReceived(Resource* resource, const ResourceResponse& response)
{
    ASSERT_UNUSED(resource, resource == this->resource());
    didReceiveResponse(resource->identifier(), response);
}

void DocumentThreadableLoader::didReceiveResponse(unsigned long identifier, const ResourceResponse& response)
{
    ASSERT(m_client);

    String accessControlErrorDescription;
    if (m_actualRequest) {
        // Notifying the inspector here is necessary because a call to preflightFailure() might synchronously
        // cause the underlying ResourceLoader to be cancelled before it tells the inspector about the response.
        // In that case, if we don't tell the inspector about the response now, the resource type in the inspector
        // will default to "other" instead of something more descriptive.
        DocumentLoader* loader = m_document->frame()->loader().documentLoader();
        InspectorInstrumentation::didReceiveResourceResponse(m_document->frame(), identifier, loader, response, resource() ? resource()->loader() : 0);

        if (!passesAccessControlCheck(response, m_options.allowCredentials, securityOrigin(), accessControlErrorDescription)) {
            preflightFailure(response.url().string(), accessControlErrorDescription);
            return;
        }

        if (!passesPreflightStatusCheck(response, accessControlErrorDescription)) {
            preflightFailure(response.url().string(), accessControlErrorDescription);
            return;
        }

        OwnPtr<CrossOriginPreflightResultCacheItem> preflightResult = adoptPtr(new CrossOriginPreflightResultCacheItem(m_options.allowCredentials));
        if (!preflightResult->parse(response, accessControlErrorDescription)
            || !preflightResult->allowsCrossOriginMethod(m_actualRequest->httpMethod(), accessControlErrorDescription)
            || !preflightResult->allowsCrossOriginHeaders(m_actualRequest->httpHeaderFields(), accessControlErrorDescription)) {
            preflightFailure(response.url().string(), accessControlErrorDescription);
            return;
        }

        CrossOriginPreflightResultCache::shared().appendEntry(securityOrigin()->toString(), m_actualRequest->url(), preflightResult.release());
    } else {
        if (!m_sameOriginRequest && m_options.crossOriginRequestPolicy == UseAccessControl) {
            if (!passesAccessControlCheck(response, m_options.allowCredentials, securityOrigin(), accessControlErrorDescription)) {
                m_client->didFailAccessControlCheck(ResourceError(errorDomainBlinkInternal, 0, response.url().string(), accessControlErrorDescription));
                return;
            }
        }

        m_client->didReceiveResponse(identifier, response);
    }
}

void DocumentThreadableLoader::dataReceived(Resource* resource, const char* data, int dataLength)
{
    ASSERT_UNUSED(resource, resource == this->resource());
    didReceiveData(data, dataLength);
}

void DocumentThreadableLoader::didReceiveData(const char* data, int dataLength)
{
    ASSERT(m_client);
    // Preflight data should be invisible to clients.
    if (!m_actualRequest)
        m_client->didReceiveData(data, dataLength);
}

void DocumentThreadableLoader::notifyFinished(Resource* resource)
{
    ASSERT(m_client);
    ASSERT(resource == this->resource());

    m_timeoutTimer.stop();

    if (resource->errorOccurred())
        m_client->didFail(resource->resourceError());
    else
        didFinishLoading(resource->identifier(), resource->loadFinishTime());
}

void DocumentThreadableLoader::didFinishLoading(unsigned long identifier, double finishTime)
{
    if (m_actualRequest) {
        ASSERT(!m_sameOriginRequest);
        ASSERT(m_options.crossOriginRequestPolicy == UseAccessControl);
        preflightSuccess();
    } else
        m_client->didFinishLoading(identifier, finishTime);
}

void DocumentThreadableLoader::didTimeout(Timer<DocumentThreadableLoader>* timer)
{
    ASSERT_UNUSED(timer, timer == &m_timeoutTimer);

    // Using values from net/base/net_error_list.h ERR_TIMED_OUT,
    // Same as existing FIXME above - this error should be coming from FrameLoaderClient to be identifiable.
    static const int timeoutError = -7;
    ResourceError error("net", timeoutError, resource()->url(), String());
    error.setIsTimeout(true);
    cancelWithError(error);
}

void DocumentThreadableLoader::preflightSuccess()
{
    OwnPtr<ResourceRequest> actualRequest;
    actualRequest.swap(m_actualRequest);

    actualRequest->setHTTPOrigin(securityOrigin()->toAtomicString());

    clearResource();

    loadRequest(*actualRequest);
}

void DocumentThreadableLoader::preflightFailure(const String& url, const String& errorDescription)
{
    ResourceError error(errorDomainBlinkInternal, 0, url, errorDescription);
    m_actualRequest = nullptr; // Prevent didFinishLoading() from bypassing access check.
    m_client->didFailAccessControlCheck(error);
}

void DocumentThreadableLoader::loadRequest(const ResourceRequest& request)
{
    // Any credential should have been removed from the cross-site requests.
    const KURL& requestURL = request.url();
    ASSERT(m_sameOriginRequest || requestURL.user().isEmpty());
    ASSERT(m_sameOriginRequest || requestURL.pass().isEmpty());

    ThreadableLoaderOptions options = m_options;
    if (m_async) {
        if (m_actualRequest) {
            options.sniffContent = DoNotSniffContent;
            options.dataBufferingPolicy = BufferData;
        }

        if (m_options.timeoutMilliseconds > 0)
            m_timeoutTimer.startOneShot(m_options.timeoutMilliseconds / 1000.0, FROM_HERE);

        FetchRequest newRequest(request, m_options.initiator, options);
        ASSERT(!resource());
        setResource(m_document->fetcher()->fetchRawResource(newRequest));
        if (resource() && resource()->loader()) {
            unsigned long identifier = resource()->identifier();
            InspectorInstrumentation::documentThreadableLoaderStartedLoadingForClient(m_document, identifier, m_client);
        }
        return;
    }

    FetchRequest fetchRequest(request, m_options.initiator, options);
    ResourcePtr<Resource> resource = m_document->fetcher()->fetchSynchronously(fetchRequest);
    ResourceResponse response = resource ? resource->response() : ResourceResponse();
    unsigned long identifier = resource ? resource->identifier() : std::numeric_limits<unsigned long>::max();
    ResourceError error = resource ? resource->resourceError() : ResourceError();

    InspectorInstrumentation::documentThreadableLoaderStartedLoadingForClient(m_document, identifier, m_client);

    if (!resource) {
        m_client->didFail(error);
        return;
    }

    // No exception for file:/// resources, see <rdar://problem/4962298>.
    // Also, if we have an HTTP response, then it wasn't a network error in fact.
    if (!error.isNull() && !requestURL.isLocalFile() && response.httpStatusCode() <= 0) {
        m_client->didFail(error);
        return;
    }

    // FIXME: A synchronous request does not tell us whether a redirect happened or not, so we guess by comparing the
    // request and response URLs. This isn't a perfect test though, since a server can serve a redirect to the same URL that was
    // requested. Also comparing the request and response URLs as strings will fail if the requestURL still has its credentials.
    if (requestURL != response.url() && (!isAllowedByPolicy(response.url()) || !isAllowedRedirect(response.url()))) {
        m_client->didFailRedirectCheck();
        return;
    }

    didReceiveResponse(identifier, response);

    SharedBuffer* data = resource->resourceBuffer();
    if (data)
        didReceiveData(data->data(), data->size());

    didFinishLoading(identifier, 0.0);
}

bool DocumentThreadableLoader::isAllowedRedirect(const KURL& url) const
{
    if (m_options.crossOriginRequestPolicy == AllowCrossOriginRequests)
        return true;

    return m_sameOriginRequest && securityOrigin()->canRequest(url);
}

bool DocumentThreadableLoader::isAllowedByPolicy(const KURL& url) const
{
    if (m_options.contentSecurityPolicyEnforcement != EnforceConnectSrcDirective)
        return true;
    return m_document->contentSecurityPolicy()->allowConnectToSource(url);
}

SecurityOrigin* DocumentThreadableLoader::securityOrigin() const
{
    return m_options.securityOrigin ? m_options.securityOrigin.get() : m_document->securityOrigin();
}

} // namespace WebCore
