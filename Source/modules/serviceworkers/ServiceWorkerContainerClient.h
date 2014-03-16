// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ServiceWorkerContainerClient_h
#define ServiceWorkerContainerClient_h

#include "core/page/Page.h"
#include "core/workers/WorkerClients.h"
#include "wtf/Forward.h"

namespace blink {
class WebServiceWorkerProvider;
}

namespace WebCore {

class ExecutionContext;

// This mainly exists to provide access to WebServiceWorkerProvider.
// Owned by Page (or WorkerClients).
class ServiceWorkerContainerClient FINAL :
    public Supplement<Page>,
    public Supplement<WorkerClients> {
    WTF_MAKE_NONCOPYABLE(ServiceWorkerContainerClient);
public:
    static PassOwnPtr<ServiceWorkerContainerClient> create(PassOwnPtr<blink::WebServiceWorkerProvider>);
    virtual ~ServiceWorkerContainerClient();

    blink::WebServiceWorkerProvider* provider() { return m_provider.get(); }

    static const char* supplementName();
    static ServiceWorkerContainerClient* from(ExecutionContext*);

protected:
    explicit ServiceWorkerContainerClient(PassOwnPtr<blink::WebServiceWorkerProvider>);

    OwnPtr<blink::WebServiceWorkerProvider> m_provider;
};

void provideServiceWorkerContainerClientToWorker(WorkerClients*, PassOwnPtr<blink::WebServiceWorkerProvider>);

} // namespace WebCore

#endif // ServiceWorkerContainerClient_h
