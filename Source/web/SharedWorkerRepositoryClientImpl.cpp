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
#include "SharedWorkerRepositoryClientImpl.h"

#include "WebContentSecurityPolicy.h"
#include "WebFrameClient.h"
#include "WebFrameImpl.h"
#include "WebKit.h"
#include "WebSharedWorker.h"
#include "WebSharedWorkerRepositoryClient.h"
#include "bindings/v8/ExceptionMessages.h"
#include "bindings/v8/ExceptionState.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/ExecutionContext.h"
#include "core/events/Event.h"
#include "core/events/ThreadLocalEventNames.h"
#include "core/frame/csp/ContentSecurityPolicy.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "core/workers/SharedWorker.h"
#include "core/workers/WorkerScriptLoader.h"
#include "core/workers/WorkerScriptLoaderClient.h"
#include "platform/network/ResourceResponse.h"
#include "public/platform/WebMessagePortChannel.h"
#include "public/platform/WebString.h"
#include "public/platform/WebURL.h"

using namespace WebCore;

namespace blink {

// Callback class that keeps the SharedWorker and WebSharedWorker objects alive while connecting.
class SharedWorkerConnector : private WebSharedWorkerConnector::ConnectListener {
public:
    SharedWorkerConnector(PassRefPtrWillBeRawPtr<SharedWorker> worker, const KURL& url, const String& name, PassOwnPtr<WebMessagePortChannel> channel, PassOwnPtr<WebSharedWorkerConnector> webWorkerConnector)
        : m_worker(worker)
        , m_url(url)
        , m_name(name)
        , m_webWorkerConnector(webWorkerConnector)
        , m_channel(channel) { }

    virtual ~SharedWorkerConnector();
    void connect();

private:
    // WebSharedWorkerConnector::ConnectListener overrides.
    virtual void connected() OVERRIDE;
    virtual void scriptLoadFailed() OVERRIDE;

    RefPtrWillBePersistent<SharedWorker> m_worker;
    KURL m_url;
    String m_name;
    OwnPtr<WebSharedWorkerConnector> m_webWorkerConnector;
    OwnPtr<WebMessagePortChannel> m_channel;
};

SharedWorkerConnector::~SharedWorkerConnector()
{
    m_worker->unsetPreventGC();
}
void SharedWorkerConnector::connect()
{
    m_worker->setPreventGC();
    m_webWorkerConnector->connect(m_channel.leakPtr(), this);
}

void SharedWorkerConnector::connected()
{
    // Free ourselves (this releases the SharedWorker so it can be freed as well if unreferenced).
    delete this;
}

void SharedWorkerConnector::scriptLoadFailed()
{
    m_worker->dispatchEvent(Event::createCancelable(EventTypeNames::error));
    // Free ourselves (this releases the SharedWorker so it can be freed as well if unreferenced).
    delete this;
}

static WebSharedWorkerRepositoryClient::DocumentID getId(void* document)
{
    ASSERT(document);
    return reinterpret_cast<WebSharedWorkerRepositoryClient::DocumentID>(document);
}

void SharedWorkerRepositoryClientImpl::connect(PassRefPtrWillBeRawPtr<SharedWorker> worker, PassOwnPtr<WebMessagePortChannel> port, const KURL& url, const String& name, ExceptionState& exceptionState)
{
    ASSERT(m_client);

    // No nested workers (for now) - connect() should only be called from document context.
    ASSERT(worker->executionContext()->isDocument());
    Document* document = toDocument(worker->executionContext());
    OwnPtr<WebSharedWorkerConnector> webWorkerConnector = adoptPtr(m_client->createSharedWorkerConnector(url, name, getId(document), worker->executionContext()->contentSecurityPolicy()->deprecatedHeader(), static_cast<blink::WebContentSecurityPolicyType>(worker->executionContext()->contentSecurityPolicy()->deprecatedHeaderType())));
    if (!webWorkerConnector) {
        // Existing worker does not match this url, so return an error back to the caller.
        exceptionState.throwDOMException(URLMismatchError, "The location of the SharedWorker named '" + name + "' does not exactly match the provided URL ('" + url.elidedString() + "').");
        return;
    }

    // The connector object manages its own lifecycle (and the lifecycles of the two worker objects).
    // It will free itself once connecting is completed.
    SharedWorkerConnector* connector = new SharedWorkerConnector(worker, url, name, port, webWorkerConnector.release());
    connector->connect();
}

void SharedWorkerRepositoryClientImpl::documentDetached(Document* document)
{
    ASSERT(m_client);
    m_client->documentDetached(getId(document));
}

SharedWorkerRepositoryClientImpl::SharedWorkerRepositoryClientImpl(WebSharedWorkerRepositoryClient* client)
    : m_client(client)
{
}

} // namespace blink
