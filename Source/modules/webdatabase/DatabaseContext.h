/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
 * Copyright (C) 2011 Google, Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifndef DatabaseContext_h
#define DatabaseContext_h

#include "core/dom/ActiveDOMObject.h"
#include "heap/Handle.h"
#include "wtf/PassRefPtr.h"
#include "wtf/ThreadSafeRefCounted.h"

namespace WebCore {

class Database;
class DatabaseBackendBase;
class DatabaseContext;
class DatabaseTaskSynchronizer;
class DatabaseThread;
class ExecutionContext;
class SecurityOrigin;

class DatabaseContext FINAL : public ThreadSafeRefCounted<DatabaseContext>, public ActiveDOMObject {
public:
    friend class DatabaseManager;

    static PassRefPtr<DatabaseContext> create(ExecutionContext*);

    virtual ~DatabaseContext();

    // For life-cycle management (inherited from ActiveDOMObject):
    virtual void contextDestroyed() OVERRIDE;
    virtual void willStop() OVERRIDE;
    virtual void stop() OVERRIDE;

    PassRefPtr<DatabaseContext> backend();
    DatabaseThread* databaseThread();

    void setHasOpenDatabases() { m_hasOpenDatabases = true; }
    void didOpenDatabase(DatabaseBackendBase&);
    void didCloseDatabase(DatabaseBackendBase&);
    // Blocks the caller thread until cleanup tasks are completed.
    void stopDatabases();

    bool allowDatabaseAccess() const;

    SecurityOrigin* securityOrigin() const;
    bool isContextThread() const;

private:
    explicit DatabaseContext(ExecutionContext*);

    void stopSyncDatabases();

    RefPtr<DatabaseThread> m_databaseThread;
    // The contents of m_openSyncDatabases are raw pointers. It's safe because
    // DatabaseBackendSync is always closed before destruction.
    HashSet<DatabaseBackendBase*> m_openSyncDatabases;
    bool m_hasOpenDatabases; // This never changes back to false, even after the database thread is closed.
    bool m_isRegistered;
    bool m_hasRequestedTermination;
};

} // namespace WebCore

#endif // DatabaseContext_h
