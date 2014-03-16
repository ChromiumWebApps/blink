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

#ifndef IDBTransaction_h
#define IDBTransaction_h

#include "bindings/v8/ScriptWrappable.h"
#include "core/dom/ActiveDOMObject.h"
#include "core/dom/DOMError.h"
#include "core/events/Event.h"
#include "core/events/EventListener.h"
#include "core/events/EventTarget.h"
#include "core/events/ThreadLocalEventNames.h"
#include "heap/Handle.h"
#include "modules/indexeddb/IDBMetadata.h"
#include "modules/indexeddb/IndexedDB.h"
#include "public/platform/WebIDBDatabase.h"
#include "wtf/HashSet.h"
#include "wtf/RefCounted.h"

namespace WebCore {

class DOMError;
class ExceptionState;
class IDBCursor;
class IDBDatabase;
class IDBObjectStore;
class IDBOpenDBRequest;
struct IDBObjectStoreMetadata;

class IDBTransaction FINAL : public ScriptWrappable, public RefCounted<IDBTransaction>, public EventTargetWithInlineData, public ActiveDOMObject {
    REFCOUNTED_EVENT_TARGET(IDBTransaction);

public:
    static PassRefPtr<IDBTransaction> create(ExecutionContext*, int64_t, const Vector<String>& objectStoreNames, blink::WebIDBDatabase::TransactionMode, IDBDatabase*);
    static PassRefPtr<IDBTransaction> create(ExecutionContext*, int64_t, IDBDatabase*, IDBOpenDBRequest*, const IDBDatabaseMetadata& previousMetadata);
    virtual ~IDBTransaction();

    static const AtomicString& modeReadOnly();
    static const AtomicString& modeReadWrite();
    static const AtomicString& modeVersionChange();

    static blink::WebIDBDatabase::TransactionMode stringToMode(const String&, ExceptionState&);
    static const AtomicString& modeToString(blink::WebIDBDatabase::TransactionMode);

    blink::WebIDBDatabase* backendDB() const;

    int64_t id() const { return m_id; }
    bool isActive() const { return m_state == Active; }
    bool isFinished() const { return m_state == Finished; }
    bool isReadOnly() const { return m_mode == blink::WebIDBDatabase::TransactionReadOnly; }
    bool isVersionChange() const { return m_mode == blink::WebIDBDatabase::TransactionVersionChange; }

    // Implement the IDBTransaction IDL
    const String& mode() const;
    IDBDatabase* db() const { return m_database.get(); }
    PassRefPtrWillBeRawPtr<DOMError> error() const { return m_error; }
    PassRefPtr<IDBObjectStore> objectStore(const String& name, ExceptionState&);
    void abort(ExceptionState&);

    void registerRequest(IDBRequest*);
    void unregisterRequest(IDBRequest*);
    void objectStoreCreated(const String&, PassRefPtr<IDBObjectStore>);
    void objectStoreDeleted(const String&);
    void setActive(bool);
    void setError(PassRefPtrWillBeRawPtr<DOMError>);

    DEFINE_ATTRIBUTE_EVENT_LISTENER(abort);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(complete);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(error);

    void onAbort(PassRefPtrWillBeRawPtr<DOMError>);
    void onComplete();

    // EventTarget
    virtual const AtomicString& interfaceName() const OVERRIDE;
    virtual ExecutionContext* executionContext() const OVERRIDE;

    using EventTarget::dispatchEvent;
    virtual bool dispatchEvent(PassRefPtr<Event>) OVERRIDE;

    // ActiveDOMObject
    virtual bool hasPendingActivity() const OVERRIDE;
    virtual void stop() OVERRIDE;

private:
    IDBTransaction(ExecutionContext*, int64_t, const Vector<String>&, blink::WebIDBDatabase::TransactionMode, IDBDatabase*, IDBOpenDBRequest*, const IDBDatabaseMetadata&);

    void enqueueEvent(PassRefPtr<Event>);

    enum State {
        Inactive, // Created or started, but not in an event callback
        Active, // Created or started, in creation scope or an event callback
        Finishing, // In the process of aborting or completing.
        Finished, // No more events will fire and no new requests may be filed.
    };

    int64_t m_id;
    RefPtr<IDBDatabase> m_database;
    const Vector<String> m_objectStoreNames;
    IDBOpenDBRequest* m_openDBRequest;
    const blink::WebIDBDatabase::TransactionMode m_mode;
    State m_state;
    bool m_hasPendingActivity;
    bool m_contextStopped;
    RefPtrWillBePersistent<DOMError> m_error;

    ListHashSet<RefPtr<IDBRequest> > m_requestList;

    typedef HashMap<String, RefPtr<IDBObjectStore> > IDBObjectStoreMap;
    IDBObjectStoreMap m_objectStoreMap;

    typedef HashSet<RefPtr<IDBObjectStore> > IDBObjectStoreSet;
    IDBObjectStoreSet m_deletedObjectStores;

    typedef HashMap<RefPtr<IDBObjectStore>, IDBObjectStoreMetadata> IDBObjectStoreMetadataMap;
    IDBObjectStoreMetadataMap m_objectStoreCleanupMap;
    IDBDatabaseMetadata m_previousMetadata;
};

} // namespace WebCore

#endif // IDBTransaction_h
