/*
 * Copyright (C) 2010 Google Inc.  All rights reserved.
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

#ifndef FileWriter_h
#define FileWriter_h

#include "bindings/v8/ScriptWrappable.h"
#include "core/dom/ActiveDOMObject.h"
#include "core/dom/ExecutionContext.h"
#include "core/events/EventTarget.h"
#include "core/fileapi/FileError.h"
#include "heap/Handle.h"
#include "modules/filesystem/FileWriterBase.h"
#include "public/platform/WebFileWriterClient.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefPtr.h"

namespace WebCore {

class Blob;
class ExceptionState;
class ExecutionContext;

class FileWriter FINAL : public FileWriterBase, public ScriptWrappable, public ActiveDOMObject, public EventTargetWithInlineData, public blink::WebFileWriterClient {
    DEFINE_EVENT_TARGET_REFCOUNTING(RefCountedWillBeRefCountedGarbageCollected<FileWriterBase>);
public:
    static PassRefPtrWillBeRawPtr<FileWriter> create(ExecutionContext*);

    enum ReadyState {
        INIT = 0,
        WRITING = 1,
        DONE = 2
    };

    void write(Blob*, ExceptionState&);
    void seek(long long position, ExceptionState&);
    void truncate(long long length, ExceptionState&);
    void abort(ExceptionState&);
    ReadyState readyState() const { return m_readyState; }
    FileError* error() const { return m_error.get(); }

    // WebFileWriterClient
    virtual void didWrite(long long bytes, bool complete) OVERRIDE;
    virtual void didTruncate() OVERRIDE;
    virtual void didFail(blink::WebFileError) OVERRIDE;

    // ActiveDOMObject
    virtual void stop() OVERRIDE;

    // EventTarget
    virtual const AtomicString& interfaceName() const OVERRIDE;
    virtual ExecutionContext* executionContext() const OVERRIDE { return ActiveDOMObject::executionContext(); }

    DEFINE_ATTRIBUTE_EVENT_LISTENER(writestart);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(progress);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(write);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(abort);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(error);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(writeend);

    virtual void trace(Visitor*) OVERRIDE;

private:
    enum Operation {
        OperationNone,
        OperationWrite,
        OperationTruncate,
        OperationAbort
    };

    FileWriter(ExecutionContext*);

    virtual ~FileWriter();

    void completeAbort();

    void doOperation(Operation);

    void signalCompletion(FileError::ErrorCode);

    void fireEvent(const AtomicString& type);

    void setError(FileError::ErrorCode, ExceptionState&);

    RefPtrWillBeMember<FileError> m_error;
    ReadyState m_readyState;
    Operation m_operationInProgress;
    Operation m_queuedOperation;
    long long m_bytesWritten;
    long long m_bytesToWrite;
    long long m_truncateLength;
    long long m_numAborts;
    long long m_recursionDepth;
    double m_lastProgressNotificationTimeMS;
    RefPtrWillBeMember<Blob> m_blobBeingWritten;
};

} // namespace WebCore

#endif // FileWriter_h
