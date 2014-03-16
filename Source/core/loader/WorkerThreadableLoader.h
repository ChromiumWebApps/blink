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

#ifndef WorkerThreadableLoader_h
#define WorkerThreadableLoader_h

#include "core/loader/ThreadableLoader.h"
#include "core/loader/ThreadableLoaderClient.h"
#include "core/loader/ThreadableLoaderClientWrapper.h"
#include "heap/Handle.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefCounted.h"
#include "wtf/RefPtr.h"
#include "wtf/Threading.h"
#include "wtf/text/WTFString.h"

namespace WebCore {

    class ResourceError;
    class ResourceRequest;
    class WorkerGlobalScope;
    class WorkerLoaderProxy;
    struct CrossThreadResourceResponseData;
    struct CrossThreadResourceRequestData;

    class WorkerThreadableLoader FINAL : public RefCounted<WorkerThreadableLoader>, public ThreadableLoader {
        WTF_MAKE_FAST_ALLOCATED;
    public:
        static void loadResourceSynchronously(WorkerGlobalScope*, const ResourceRequest&, ThreadableLoaderClient&, const ThreadableLoaderOptions&);
        static PassRefPtr<WorkerThreadableLoader> create(WorkerGlobalScope* workerGlobalScope, PassRefPtr<ThreadableLoaderClientWrapper> clientWrapper, PassOwnPtr<ThreadableLoaderClient> clientBridge, const ResourceRequest& request, const ThreadableLoaderOptions& options)
        {
            return adoptRef(new WorkerThreadableLoader(workerGlobalScope, clientWrapper, clientBridge, request, options));
        }

        virtual ~WorkerThreadableLoader();

        virtual void cancel() OVERRIDE;

        bool done() const { return m_workerClientWrapper->done(); }

        using RefCounted<WorkerThreadableLoader>::ref;
        using RefCounted<WorkerThreadableLoader>::deref;

    protected:
        virtual void refThreadableLoader() OVERRIDE { ref(); }
        virtual void derefThreadableLoader() OVERRIDE { deref(); }

    private:
        // Creates a loader on the main thread and bridges communication between
        // the main thread and the worker context's thread where WorkerThreadableLoader runs.
        //
        // Regarding the bridge and lifetimes of items used in callbacks, there are a few cases:
        //
        // all cases. All tasks posted from the worker context's thread are ok because
        //    the last task posted always is "mainThreadDestroy", so MainThreadBridge is
        //    around for all tasks that use it on the main thread.
        //
        // case 1. worker.terminate is called.
        //    In this case, no more tasks are posted from the worker object's thread to the worker
        //    context's thread -- WorkerGlobalScopeProxy implementation enforces this.
        //
        // case 2. xhr gets aborted and the worker context continues running.
        //    The ThreadableLoaderClientWrapper has the underlying client cleared, so no more calls
        //    go through it.  All tasks posted from the worker object's thread to the worker context's
        //    thread do "ThreadableLoaderClientWrapper::ref" (automatically inside of the cross thread copy
        //    done in createCallbackTask), so the ThreadableLoaderClientWrapper instance is there until all
        //    tasks are executed.
        class MainThreadBridge FINAL : public ThreadableLoaderClient {
        public:
            // All executed on the worker context's thread.
            MainThreadBridge(PassRefPtr<ThreadableLoaderClientWrapper>, PassOwnPtr<ThreadableLoaderClient>, WorkerLoaderProxy&, const ResourceRequest&, const ThreadableLoaderOptions&, const String& outgoingReferrer);
            void cancel();
            void destroy();

        private:
            // Executed on the worker context's thread.
            void clearClientWrapper();

            // All executed on the main thread.
            static void mainThreadDestroy(ExecutionContext*, MainThreadBridge*);
            virtual ~MainThreadBridge();

            static void mainThreadCreateLoader(ExecutionContext*, MainThreadBridge*, PassOwnPtr<CrossThreadResourceRequestData>, ThreadableLoaderOptions, const String& outgoingReferrer);
            static void mainThreadCancel(ExecutionContext*, MainThreadBridge*);
            virtual void didSendData(unsigned long long bytesSent, unsigned long long totalBytesToBeSent) OVERRIDE;
            virtual void didReceiveResponse(unsigned long identifier, const ResourceResponse&) OVERRIDE;
            virtual void didReceiveData(const char*, int dataLength) OVERRIDE;
            virtual void didDownloadData(int dataLength) OVERRIDE;
            virtual void didReceiveCachedMetadata(const char*, int dataLength) OVERRIDE;
            virtual void didFinishLoading(unsigned long identifier, double finishTime) OVERRIDE;
            virtual void didFail(const ResourceError&) OVERRIDE;
            virtual void didFailAccessControlCheck(const ResourceError&) OVERRIDE;
            virtual void didFailRedirectCheck() OVERRIDE;

            // Only to be used on the main thread.
            RefPtr<ThreadableLoader> m_mainThreadLoader;
            OwnPtr<ThreadableLoaderClient> m_clientBridge;

            // ThreadableLoaderClientWrapper is to be used on the worker context thread.
            // The ref counting is done on either thread.
            RefPtr<ThreadableLoaderClientWrapper> m_workerClientWrapper;

            // Used on the worker context thread.
            WorkerLoaderProxy& m_loaderProxy;
        };

        WorkerThreadableLoader(WorkerGlobalScope*, PassRefPtr<ThreadableLoaderClientWrapper>, PassOwnPtr<ThreadableLoaderClient>, const ResourceRequest&, const ThreadableLoaderOptions&);

        RefPtrWillBePersistent<WorkerGlobalScope> m_workerGlobalScope;
        RefPtr<ThreadableLoaderClientWrapper> m_workerClientWrapper;
        MainThreadBridge& m_bridge;
    };

} // namespace WebCore

#endif // WorkerThreadableLoader_h
