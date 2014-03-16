/*
 * Copyright (C) 2007, 2008 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef DatabaseThread_h
#define DatabaseThread_h

#include "heap/Handle.h"
#include "public/platform/WebThread.h"
#include "wtf/Deque.h"
#include "wtf/HashMap.h"
#include "wtf/HashSet.h"
#include "wtf/OwnPtr.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefPtr.h"
#include "wtf/ThreadSafeRefCounted.h"
#include "wtf/ThreadingPrimitives.h"

namespace WebCore {

class DatabaseBackend;
class DatabaseTask;
class DatabaseTaskSynchronizer;
class Document;
class MessageLoopInterruptor;
class PendingGCRunner;
class SQLTransactionClient;
class SQLTransactionCoordinator;

class DatabaseThread : public ThreadSafeRefCounted<DatabaseThread> {
public:
    static PassRefPtr<DatabaseThread> create() { return adoptRef(new DatabaseThread); }
    ~DatabaseThread();

    void start();
    void requestTermination(DatabaseTaskSynchronizer* cleanupSync);
    bool terminationRequested(DatabaseTaskSynchronizer* taskSynchronizer = 0) const;

    void scheduleTask(PassOwnPtr<DatabaseTask>);

    void recordDatabaseOpen(DatabaseBackend*);
    void recordDatabaseClosed(DatabaseBackend*);
    bool isDatabaseOpen(DatabaseBackend*);

    bool isDatabaseThread() { return m_thread && m_thread->isCurrentThread(); }

    SQLTransactionClient* transactionClient() { return m_transactionClient.get(); }
    SQLTransactionCoordinator* transactionCoordinator() { return m_transactionCoordinator.get(); }

private:
    DatabaseThread();

    void setupDatabaseThread();
    void cleanupDatabaseThread();
    void cleanupDatabaseThreadCompleted();

    OwnPtr<blink::WebThread> m_thread;

    // This set keeps track of the open databases that have been used on this thread.
    // This must be updated in the database thread though it is constructed and
    // destructed in the context thread.
    WillBePersistentHeapHashSet<RefPtrWillBeMember<DatabaseBackend> > m_openDatabaseSet;

    OwnPtr<SQLTransactionClient> m_transactionClient;
    OwnPtr<SQLTransactionCoordinator> m_transactionCoordinator;
    DatabaseTaskSynchronizer* m_cleanupSync;

    mutable Mutex m_terminationRequestedMutex;
    bool m_terminationRequested;
    OwnPtr<PendingGCRunner> m_pendingGCRunner;
    OwnPtr<MessageLoopInterruptor> m_messageLoopInterruptor;
};

} // namespace WebCore

#endif // DatabaseThread_h
