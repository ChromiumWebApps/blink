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
#ifndef SharedWorkerThread_h
#define SharedWorkerThread_h

#include "core/frame/csp/ContentSecurityPolicy.h"
#include "core/workers/WorkerThread.h"

namespace WebCore {

class WorkerThreadStartupData;

class SharedWorkerThread : public WorkerThread {
public:
    static PassRefPtr<SharedWorkerThread> create(const String& name, WorkerLoaderProxy&, WorkerReportingProxy&, PassOwnPtrWillBeRawPtr<WorkerThreadStartupData>);
    virtual ~SharedWorkerThread();

protected:
    virtual PassRefPtrWillBeRawPtr<WorkerGlobalScope> createWorkerGlobalScope(PassOwnPtrWillBeRawPtr<WorkerThreadStartupData>) OVERRIDE;

private:
    SharedWorkerThread(const String& name, WorkerLoaderProxy&, WorkerReportingProxy&, PassOwnPtrWillBeRawPtr<WorkerThreadStartupData>);

    String m_name;
};

} // namespace WebCore

#endif // SharedWorkerThread_h
