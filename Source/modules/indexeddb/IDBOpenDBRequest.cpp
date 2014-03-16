/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#include "config.h"
#include "modules/indexeddb/IDBOpenDBRequest.h"

#include "bindings/v8/Nullable.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/ExecutionContext.h"
#include "modules/indexeddb/IDBDatabase.h"
#include "modules/indexeddb/IDBDatabaseCallbacks.h"
#include "modules/indexeddb/IDBPendingTransactionMonitor.h"
#include "modules/indexeddb/IDBTracing.h"
#include "modules/indexeddb/IDBVersionChangeEvent.h"

using blink::WebIDBDatabase;

namespace WebCore {

PassRefPtr<IDBOpenDBRequest> IDBOpenDBRequest::create(ExecutionContext* context, PassRefPtr<IDBDatabaseCallbacks> callbacks, int64_t transactionId, int64_t version)
{
    RefPtr<IDBOpenDBRequest> request(adoptRef(new IDBOpenDBRequest(context, callbacks, transactionId, version)));
    request->suspendIfNeeded();
    return request.release();
}

IDBOpenDBRequest::IDBOpenDBRequest(ExecutionContext* context, PassRefPtr<IDBDatabaseCallbacks> callbacks, int64_t transactionId, int64_t version)
    : IDBRequest(context, IDBAny::createNull(), 0)
    , m_databaseCallbacks(callbacks)
    , m_transactionId(transactionId)
    , m_version(version)
{
    ASSERT(!resultAsAny());
    ScriptWrappable::init(this);
}

IDBOpenDBRequest::~IDBOpenDBRequest()
{
}

const AtomicString& IDBOpenDBRequest::interfaceName() const
{
    return EventTargetNames::IDBOpenDBRequest;
}

void IDBOpenDBRequest::onBlocked(int64_t oldVersion)
{
    IDB_TRACE("IDBOpenDBRequest::onBlocked()");
    if (!shouldEnqueueEvent())
        return;
    Nullable<unsigned long long> newVersionNullable = (m_version == IDBDatabaseMetadata::DefaultIntVersion) ? Nullable<unsigned long long>() : Nullable<unsigned long long>(m_version);
    enqueueEvent(IDBVersionChangeEvent::create(EventTypeNames::blocked, oldVersion, newVersionNullable));
}

void IDBOpenDBRequest::onUpgradeNeeded(int64_t oldVersion, PassOwnPtr<WebIDBDatabase> backend, const IDBDatabaseMetadata& metadata, blink::WebIDBDataLoss dataLoss, String dataLossMessage)
{
    IDB_TRACE("IDBOpenDBRequest::onUpgradeNeeded()");
    if (m_contextStopped || !executionContext()) {
        OwnPtr<WebIDBDatabase> db = backend;
        db->abort(m_transactionId);
        db->close();
        return;
    }
    if (!shouldEnqueueEvent())
        return;

    ASSERT(m_databaseCallbacks);

    RefPtr<IDBDatabase> idbDatabase = IDBDatabase::create(executionContext(), backend, m_databaseCallbacks.release());
    idbDatabase->setMetadata(metadata);

    if (oldVersion == IDBDatabaseMetadata::NoIntVersion) {
        // This database hasn't had an integer version before.
        oldVersion = IDBDatabaseMetadata::DefaultIntVersion;
    }
    IDBDatabaseMetadata oldMetadata(metadata);
    oldMetadata.intVersion = oldVersion;

    m_transaction = IDBTransaction::create(executionContext(), m_transactionId, idbDatabase.get(), this, oldMetadata);
    setResult(IDBAny::create(idbDatabase.release()));

    if (m_version == IDBDatabaseMetadata::NoIntVersion)
        m_version = 1;
    enqueueEvent(IDBVersionChangeEvent::create(EventTypeNames::upgradeneeded, oldVersion, m_version, dataLoss, dataLossMessage));
}

void IDBOpenDBRequest::onSuccess(PassOwnPtr<WebIDBDatabase> backend, const IDBDatabaseMetadata& metadata)
{
    IDB_TRACE("IDBOpenDBRequest::onSuccess()");
    if (m_contextStopped || !executionContext()) {
        OwnPtr<WebIDBDatabase> db = backend;
        if (db)
            db->close();
        return;
    }
    if (!shouldEnqueueEvent())
        return;

    RefPtr<IDBDatabase> idbDatabase;
    if (resultAsAny()) {
        // Previous onUpgradeNeeded call delivered the backend.
        ASSERT(!backend.get());
        idbDatabase = resultAsAny()->idbDatabase();
        ASSERT(idbDatabase);
        ASSERT(!m_databaseCallbacks);
    } else {
        ASSERT(backend.get());
        ASSERT(m_databaseCallbacks);
        idbDatabase = IDBDatabase::create(executionContext(), backend, m_databaseCallbacks.release());
        setResult(IDBAny::create(idbDatabase.get()));
    }
    idbDatabase->setMetadata(metadata);
    enqueueEvent(Event::create(EventTypeNames::success));
}

bool IDBOpenDBRequest::shouldEnqueueEvent() const
{
    if (m_contextStopped || !executionContext())
        return false;
    ASSERT(m_readyState == PENDING || m_readyState == DONE);
    if (m_requestAborted)
        return false;
    return true;
}

bool IDBOpenDBRequest::dispatchEvent(PassRefPtr<Event> event)
{
    // If the connection closed between onUpgradeNeeded and the delivery of the "success" event,
    // an "error" event should be fired instead.
    if (event->type() == EventTypeNames::success && resultAsAny()->type() == IDBAny::IDBDatabaseType && resultAsAny()->idbDatabase()->isClosePending()) {
        dequeueEvent(event.get());
        setResult(nullptr);
        onError(DOMError::create(AbortError, "The connection was closed."));
        return false;
    }

    return IDBRequest::dispatchEvent(event);
}

} // namespace WebCore
