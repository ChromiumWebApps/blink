/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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
#include "modules/indexeddb/IDBTransaction.h"

#include "bindings/v8/ExceptionState.h"
#include "bindings/v8/ExceptionStatePlaceholder.h"
#include "core/dom/ExecutionContext.h"
#include "core/events/EventQueue.h"
#include "core/inspector/ScriptCallStack.h"
#include "modules/indexeddb/IDBDatabase.h"
#include "modules/indexeddb/IDBEventDispatcher.h"
#include "modules/indexeddb/IDBIndex.h"
#include "modules/indexeddb/IDBObjectStore.h"
#include "modules/indexeddb/IDBOpenDBRequest.h"
#include "modules/indexeddb/IDBPendingTransactionMonitor.h"
#include "modules/indexeddb/IDBTracing.h"

using blink::WebIDBDatabase;

namespace WebCore {

PassRefPtr<IDBTransaction> IDBTransaction::create(ExecutionContext* context, int64_t id, const Vector<String>& objectStoreNames, WebIDBDatabase::TransactionMode mode, IDBDatabase* db)
{
    IDBOpenDBRequest* openDBRequest = 0;
    RefPtr<IDBTransaction> transaction(adoptRef(new IDBTransaction(context, id, objectStoreNames, mode, db, openDBRequest, IDBDatabaseMetadata())));
    transaction->suspendIfNeeded();
    return transaction.release();
}

PassRefPtr<IDBTransaction> IDBTransaction::create(ExecutionContext* context, int64_t id, IDBDatabase* db, IDBOpenDBRequest* openDBRequest, const IDBDatabaseMetadata& previousMetadata)
{
    RefPtr<IDBTransaction> transaction(adoptRef(new IDBTransaction(context, id, Vector<String>(), WebIDBDatabase::TransactionVersionChange, db, openDBRequest, previousMetadata)));
    transaction->suspendIfNeeded();
    return transaction.release();
}

const AtomicString& IDBTransaction::modeReadOnly()
{
    DEFINE_STATIC_LOCAL(AtomicString, readonly, ("readonly", AtomicString::ConstructFromLiteral));
    return readonly;
}

const AtomicString& IDBTransaction::modeReadWrite()
{
    DEFINE_STATIC_LOCAL(AtomicString, readwrite, ("readwrite", AtomicString::ConstructFromLiteral));
    return readwrite;
}

const AtomicString& IDBTransaction::modeVersionChange()
{
    DEFINE_STATIC_LOCAL(AtomicString, versionchange, ("versionchange", AtomicString::ConstructFromLiteral));
    return versionchange;
}

IDBTransaction::IDBTransaction(ExecutionContext* context, int64_t id, const Vector<String>& objectStoreNames, WebIDBDatabase::TransactionMode mode, IDBDatabase* db, IDBOpenDBRequest* openDBRequest, const IDBDatabaseMetadata& previousMetadata)
    : ActiveDOMObject(context)
    , m_id(id)
    , m_database(db)
    , m_objectStoreNames(objectStoreNames)
    , m_openDBRequest(openDBRequest)
    , m_mode(mode)
    , m_state(Active)
    , m_hasPendingActivity(true)
    , m_contextStopped(false)
    , m_previousMetadata(previousMetadata)
{
    ScriptWrappable::init(this);
    if (mode == WebIDBDatabase::TransactionVersionChange) {
        // Not active until the callback.
        m_state = Inactive;
    }

    // We pass a reference of this object before it can be adopted.
    relaxAdoptionRequirement();
    if (m_state == Active)
        IDBPendingTransactionMonitor::addNewTransaction(this);
    m_database->transactionCreated(this);
}

IDBTransaction::~IDBTransaction()
{
    ASSERT(m_state == Finished || m_contextStopped);
    ASSERT(m_requestList.isEmpty() || m_contextStopped);
}

const String& IDBTransaction::mode() const
{
    return modeToString(m_mode);
}

void IDBTransaction::setError(PassRefPtrWillBeRawPtr<DOMError> error)
{
    ASSERT(m_state != Finished);
    ASSERT(error);

    // The first error to be set is the true cause of the
    // transaction abort.
    if (!m_error) {
        m_error = error;
    }
}

PassRefPtr<IDBObjectStore> IDBTransaction::objectStore(const String& name, ExceptionState& exceptionState)
{
    if (m_state == Finished) {
        exceptionState.throwDOMException(InvalidStateError, IDBDatabase::transactionFinishedErrorMessage);
        return nullptr;
    }

    IDBObjectStoreMap::iterator it = m_objectStoreMap.find(name);
    if (it != m_objectStoreMap.end())
        return it->value;

    if (!isVersionChange() && !m_objectStoreNames.contains(name)) {
        exceptionState.throwDOMException(NotFoundError, IDBDatabase::noSuchObjectStoreErrorMessage);
        return nullptr;
    }

    int64_t objectStoreId = m_database->findObjectStoreId(name);
    if (objectStoreId == IDBObjectStoreMetadata::InvalidId) {
        ASSERT(isVersionChange());
        exceptionState.throwDOMException(NotFoundError, IDBDatabase::noSuchObjectStoreErrorMessage);
        return nullptr;
    }

    const IDBDatabaseMetadata& metadata = m_database->metadata();

    RefPtr<IDBObjectStore> objectStore = IDBObjectStore::create(metadata.objectStores.get(objectStoreId), this);
    objectStoreCreated(name, objectStore);
    return objectStore.release();
}

void IDBTransaction::objectStoreCreated(const String& name, PassRefPtr<IDBObjectStore> prpObjectStore)
{
    ASSERT(m_state != Finished);
    RefPtr<IDBObjectStore> objectStore = prpObjectStore;
    m_objectStoreMap.set(name, objectStore);
    if (isVersionChange())
        m_objectStoreCleanupMap.set(objectStore, objectStore->metadata());
}

void IDBTransaction::objectStoreDeleted(const String& name)
{
    ASSERT(m_state != Finished);
    ASSERT(isVersionChange());
    IDBObjectStoreMap::iterator it = m_objectStoreMap.find(name);
    if (it != m_objectStoreMap.end()) {
        RefPtr<IDBObjectStore> objectStore = it->value;
        m_objectStoreMap.remove(name);
        objectStore->markDeleted();
        m_objectStoreCleanupMap.set(objectStore, objectStore->metadata());
        m_deletedObjectStores.add(objectStore);
    }
}

void IDBTransaction::setActive(bool active)
{
    ASSERT_WITH_MESSAGE(m_state != Finished, "A finished transaction tried to setActive(%s)", active ? "true" : "false");
    if (m_state == Finishing)
        return;
    ASSERT(active != (m_state == Active));
    m_state = active ? Active : Inactive;

    if (!active && m_requestList.isEmpty())
        backendDB()->commit(m_id);
}

void IDBTransaction::abort(ExceptionState& exceptionState)
{
    if (m_state == Finishing || m_state == Finished) {
        exceptionState.throwDOMException(InvalidStateError, IDBDatabase::transactionFinishedErrorMessage);
        return;
    }

    m_state = Finishing;

    if (m_contextStopped)
        return;

    while (!m_requestList.isEmpty()) {
        RefPtr<IDBRequest> request = *m_requestList.begin();
        m_requestList.remove(request);
        request->abort();
    }

    RefPtr<IDBTransaction> selfRef = this;
    backendDB()->abort(m_id);
}

void IDBTransaction::registerRequest(IDBRequest* request)
{
    ASSERT(request);
    ASSERT(m_state == Active);
    m_requestList.add(request);
}

void IDBTransaction::unregisterRequest(IDBRequest* request)
{
    ASSERT(request);
    // If we aborted the request, it will already have been removed.
    m_requestList.remove(request);
}

void IDBTransaction::onAbort(PassRefPtrWillBeRawPtr<DOMError> prpError)
{
    IDB_TRACE("IDBTransaction::onAbort");
    if (m_contextStopped) {
        RefPtr<IDBTransaction> protect(this);
        m_database->transactionFinished(this);
        return;
    }

    RefPtrWillBeRawPtr<DOMError> error = prpError;
    ASSERT(m_state != Finished);

    if (m_state != Finishing) {
        ASSERT(error.get());
        setError(error.release());

        // Abort was not triggered by front-end, so outstanding requests must
        // be aborted now.
        while (!m_requestList.isEmpty()) {
            RefPtr<IDBRequest> request = *m_requestList.begin();
            m_requestList.remove(request);
            request->abort();
        }
        m_state = Finishing;
    }

    if (isVersionChange()) {
        for (IDBObjectStoreMetadataMap::iterator it = m_objectStoreCleanupMap.begin(); it != m_objectStoreCleanupMap.end(); ++it)
            it->key->setMetadata(it->value);
        m_database->setMetadata(m_previousMetadata);
        m_database->close();
    }
    m_objectStoreCleanupMap.clear();

    // Enqueue events before notifying database, as database may close which enqueues more events and order matters.
    enqueueEvent(Event::createBubble(EventTypeNames::abort));

    // If script has stopped and GC has completed, database may have last reference to this object.
    RefPtr<IDBTransaction> protect(this);
    m_database->transactionFinished(this);
}

void IDBTransaction::onComplete()
{
    IDB_TRACE("IDBTransaction::onComplete");
    if (m_contextStopped) {
        RefPtr<IDBTransaction> protect(this);
        m_database->transactionFinished(this);
        return;
    }

    ASSERT(m_state != Finished);
    m_state = Finishing;
    m_objectStoreCleanupMap.clear();

    // Enqueue events before notifying database, as database may close which enqueues more events and order matters.
    enqueueEvent(Event::create(EventTypeNames::complete));

    // If script has stopped and GC has completed, database may have last reference to this object.
    RefPtr<IDBTransaction> protect(this);
    m_database->transactionFinished(this);
}

bool IDBTransaction::hasPendingActivity() const
{
    // FIXME: In an ideal world, we should return true as long as anyone has a or can
    //        get a handle to us or any child request object and any of those have
    //        event listeners. This is  in order to handle user generated events properly.
    return m_hasPendingActivity && !m_contextStopped;
}

WebIDBDatabase::TransactionMode IDBTransaction::stringToMode(const String& modeString, ExceptionState& exceptionState)
{
    if (modeString.isNull()
        || modeString == IDBTransaction::modeReadOnly())
        return WebIDBDatabase::TransactionReadOnly;
    if (modeString == IDBTransaction::modeReadWrite())
        return WebIDBDatabase::TransactionReadWrite;

    exceptionState.throwTypeError("The mode provided ('" + modeString + "') is not one of 'readonly' or 'readwrite'.");
    return WebIDBDatabase::TransactionReadOnly;
}

const AtomicString& IDBTransaction::modeToString(WebIDBDatabase::TransactionMode mode)
{
    switch (mode) {
    case WebIDBDatabase::TransactionReadOnly:
        return IDBTransaction::modeReadOnly();
        break;

    case WebIDBDatabase::TransactionReadWrite:
        return IDBTransaction::modeReadWrite();
        break;

    case WebIDBDatabase::TransactionVersionChange:
        return IDBTransaction::modeVersionChange();
        break;
    }

    ASSERT_NOT_REACHED();
    return IDBTransaction::modeReadOnly();
}

const AtomicString& IDBTransaction::interfaceName() const
{
    return EventTargetNames::IDBTransaction;
}

ExecutionContext* IDBTransaction::executionContext() const
{
    return ActiveDOMObject::executionContext();
}

bool IDBTransaction::dispatchEvent(PassRefPtr<Event> event)
{
    IDB_TRACE("IDBTransaction::dispatchEvent");
    ASSERT(m_state != Finished);
    ASSERT(m_hasPendingActivity);
    ASSERT(executionContext());
    ASSERT(event->target() == this);
    m_state = Finished;

    // Break reference cycles.
    for (IDBObjectStoreMap::iterator it = m_objectStoreMap.begin(); it != m_objectStoreMap.end(); ++it)
        it->value->transactionFinished();
    m_objectStoreMap.clear();
    for (IDBObjectStoreSet::iterator it = m_deletedObjectStores.begin(); it != m_deletedObjectStores.end(); ++it)
        (*it)->transactionFinished();
    m_deletedObjectStores.clear();

    Vector<RefPtr<EventTarget> > targets;
    targets.append(this);
    targets.append(db());

    // FIXME: When we allow custom event dispatching, this will probably need to change.
    ASSERT(event->type() == EventTypeNames::complete || event->type() == EventTypeNames::abort);
    bool returnValue = IDBEventDispatcher::dispatch(event.get(), targets);
    // FIXME: Try to construct a test where |this| outlives openDBRequest and we
    // get a crash.
    if (m_openDBRequest) {
        ASSERT(isVersionChange());
        m_openDBRequest->transactionDidFinishAndDispatch();
    }
    m_hasPendingActivity = false;
    return returnValue;
}

void IDBTransaction::stop()
{
    if (m_contextStopped)
        return;

    m_contextStopped = true;

    abort(IGNORE_EXCEPTION);
}

void IDBTransaction::enqueueEvent(PassRefPtr<Event> event)
{
    ASSERT_WITH_MESSAGE(m_state != Finished, "A finished transaction tried to enqueue an event of type %s.", event->type().utf8().data());
    if (m_contextStopped || !executionContext())
        return;

    EventQueue* eventQueue = executionContext()->eventQueue();
    event->setTarget(this);
    eventQueue->enqueueEvent(event);
}

blink::WebIDBDatabase* IDBTransaction::backendDB() const
{
    return m_database->backend();
}

} // namespace WebCore
