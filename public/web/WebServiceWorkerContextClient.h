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

#ifndef WebServiceWorkerContextClient_h
#define WebServiceWorkerContextClient_h

#include "WebWorkerPermissionClientProxy.h"

namespace blink {

class WebString;
class WebServiceWorkerContextProxy;
class WebServiceWorkerResponse;

// This interface is implemented by the client. It is suppoed to be created
// on the main thread and then passed on to the worker thread to be owned
// by a newly created WorkerGlobalScope. All methods of this class, except
// for workerContextFailedToStart(), are called on the worker thread.
// FIXME: Split this into EmbeddedWorkerContextClient and
// ServiceWorkerScriptContextClient when we decide to use EmbeddedWorker
// framework for other implementation (like SharedWorker).
class WebServiceWorkerContextClient {
public:
    virtual ~WebServiceWorkerContextClient() { }

    // A new WorkerGlobalScope is created and started to run on the
    // worker thread.
    // This also gives back a proxy to the client to talk to the
    // newly created WorkerGlobalScope. The proxy is held by WorkerGlobalScope
    // and should not be held by the caller. No proxy methods should be called
    // after workerContextDestroyed() is called.
    virtual void workerContextStarted(WebServiceWorkerContextProxy*) { }

    // WorkerGlobalScope is destroyed. The client should clear the
    // WebServiceWorkerGlobalScopeProxy when this is called.
    virtual void workerContextDestroyed() { }

    // Starting worker context is failed. This could happen when loading
    // worker script fails, or is asked to terminated before the context starts.
    // This is called on the main thread.
    virtual void workerContextFailedToStart() { }

    // Called when the WorkerGlobalScope had an error or an exception.
    virtual void reportException(const WebString& errorMessage, int lineNumber, int columnNumber, const WebString& sourceURL) { }

    // Inspector related messages.
    virtual void dispatchDevToolsMessage(const WebString&) { }
    virtual void saveDevToolsAgentState(const WebString&) { }

    // ServiceWorker specific method. Called after InstallEvent (dispatched
    // via WebServiceWorkerContextProxy) is handled by the ServiceWorker's
    // script context.
    virtual void didHandleInstallEvent(int installEventID) { }

    // ServiceWorker specific methods. Called after FetchEvent is handled by the
    // ServiceWorker's script context. When no response is provided, the browser
    // should fallback to native fetch.
    virtual void didHandleFetchEvent(int fetchEventID) { }
    virtual void didHandleFetchEvent(int fetchEventID, const WebServiceWorkerResponse& response) { }
};

} // namespace blink

#endif // WebWorkerContextClient_h
