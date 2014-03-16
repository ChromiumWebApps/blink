/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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
#include "modules/indexeddb/IDBDatabaseCallbacks.h"

#include "modules/indexeddb/IDBDatabase.h"

namespace WebCore {

PassRefPtr<IDBDatabaseCallbacks> IDBDatabaseCallbacks::create()
{
    return adoptRef(new IDBDatabaseCallbacks());
}

IDBDatabaseCallbacks::IDBDatabaseCallbacks()
    : m_database(0)
{
}

IDBDatabaseCallbacks::~IDBDatabaseCallbacks()
{
}

void IDBDatabaseCallbacks::onForcedClose()
{
    if (m_database)
        m_database->forceClose();
}

void IDBDatabaseCallbacks::onVersionChange(int64_t oldVersion, int64_t newVersion)
{
    if (m_database)
        m_database->onVersionChange(oldVersion, newVersion);
}

void IDBDatabaseCallbacks::connect(IDBDatabase* database)
{
    ASSERT(!m_database);
    ASSERT(database);
    m_database = database;
}

void IDBDatabaseCallbacks::onAbort(int64_t transactionId, PassRefPtrWillBeRawPtr<DOMError> error)
{
    if (m_database)
        m_database->onAbort(transactionId, error);
}

void IDBDatabaseCallbacks::onComplete(int64_t transactionId)
{
    if (m_database)
        m_database->onComplete(transactionId);
}

} // namespace WebCore
