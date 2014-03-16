/*
 * Copyright (C) 2007, 2013 Apple Inc. All rights reserved.
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

#ifndef SQLTransaction_h
#define SQLTransaction_h

#include "bindings/v8/ScriptWrappable.h"
#include "heap/Handle.h"
#include "modules/webdatabase/AbstractSQLTransaction.h"
#include "modules/webdatabase/SQLCallbackWrapper.h"
#include "modules/webdatabase/SQLStatement.h"
#include "modules/webdatabase/SQLTransactionStateMachine.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefPtr.h"

namespace WebCore {

class AbstractSQLTransactionBackend;
class Database;
class ExceptionState;
class SQLError;
class SQLStatementCallback;
class SQLStatementErrorCallback;
class SQLTransactionCallback;
class SQLTransactionErrorCallback;
class SQLValue;
class VoidCallback;

class SQLTransaction FINAL : public AbstractSQLTransaction, public SQLTransactionStateMachine<SQLTransaction>, public ScriptWrappable {
public:
    static PassRefPtrWillBeRawPtr<SQLTransaction> create(Database*, PassOwnPtr<SQLTransactionCallback>,
        PassOwnPtr<VoidCallback> successCallback, PassOwnPtr<SQLTransactionErrorCallback>,
        bool readOnly);
    virtual void trace(Visitor*) OVERRIDE;

    void performPendingCallback();

    void executeSQL(const String& sqlStatement, const Vector<SQLValue>& arguments,
        PassOwnPtr<SQLStatementCallback>, PassOwnPtr<SQLStatementErrorCallback>, ExceptionState&);

    Database* database() { return m_database.get(); }

    PassOwnPtr<SQLTransactionErrorCallback> releaseErrorCallback();

private:
    SQLTransaction(Database*, PassOwnPtr<SQLTransactionCallback>,
        PassOwnPtr<VoidCallback> successCallback, PassOwnPtr<SQLTransactionErrorCallback>,
        bool readOnly);

    void clearCallbackWrappers();

    // APIs called from the backend published via AbstractSQLTransaction:
    virtual void requestTransitToState(SQLTransactionState) OVERRIDE;
    virtual bool hasCallback() const OVERRIDE;
    virtual bool hasSuccessCallback() const OVERRIDE;
    virtual bool hasErrorCallback() const OVERRIDE;
    virtual void setBackend(AbstractSQLTransactionBackend*) OVERRIDE;

    // State Machine functions:
    virtual StateFunction stateFunctionFor(SQLTransactionState) OVERRIDE;
    bool computeNextStateAndCleanupIfNeeded();

    // State functions:
    SQLTransactionState deliverTransactionCallback();
    SQLTransactionState deliverTransactionErrorCallback();
    SQLTransactionState deliverStatementCallback();
    SQLTransactionState deliverQuotaIncreaseCallback();
    SQLTransactionState deliverSuccessCallback();

    SQLTransactionState unreachableState();
    SQLTransactionState sendToBackendState();

    SQLTransactionState nextStateForTransactionError();

    RefPtrWillBeMember<Database> m_database;
    RefPtr<AbstractSQLTransactionBackend> m_backend;
    SQLCallbackWrapper<SQLTransactionCallback> m_callbackWrapper;
    SQLCallbackWrapper<VoidCallback> m_successCallbackWrapper;
    SQLCallbackWrapper<SQLTransactionErrorCallback> m_errorCallbackWrapper;

    bool m_executeSqlAllowed;
    RefPtr<SQLError> m_transactionError;

    bool m_readOnly;
};

} // namespace WebCore

#endif // SQLTransaction_h
