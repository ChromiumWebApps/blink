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

#include "core/workers/DedicatedWorkerThread.h"

#include "core/workers/DedicatedWorkerGlobalScope.h"
#include "core/workers/WorkerObjectProxy.h"
#include "core/workers/WorkerThreadStartupData.h"

namespace WebCore {

PassRefPtr<DedicatedWorkerThread> DedicatedWorkerThread::create(WorkerLoaderProxy& workerLoaderProxy, WorkerObjectProxy& workerObjectProxy, double timeOrigin, PassOwnPtrWillBeRawPtr<WorkerThreadStartupData> startupData)
{
    return adoptRef(new DedicatedWorkerThread(workerLoaderProxy, workerObjectProxy, timeOrigin, startupData));
}

DedicatedWorkerThread::DedicatedWorkerThread(WorkerLoaderProxy& workerLoaderProxy, WorkerObjectProxy& workerObjectProxy, double timeOrigin, PassOwnPtrWillBeRawPtr<WorkerThreadStartupData> startupData)
    : WorkerThread(workerLoaderProxy, workerObjectProxy, startupData)
    , m_workerObjectProxy(workerObjectProxy)
    , m_timeOrigin(timeOrigin)
{
}

DedicatedWorkerThread::~DedicatedWorkerThread()
{
}

PassRefPtrWillBeRawPtr<WorkerGlobalScope> DedicatedWorkerThread::createWorkerGlobalScope(PassOwnPtrWillBeRawPtr<WorkerThreadStartupData> startupData)
{
    return DedicatedWorkerGlobalScope::create(this, startupData, m_timeOrigin);
}

void DedicatedWorkerThread::runEventLoop()
{
    // Notify the parent object of our current active state before calling the superclass to run the event loop.
    m_workerObjectProxy.reportPendingActivity(workerGlobalScope()->hasPendingActivity());
    WorkerThread::runEventLoop();
}

} // namespace WebCore
