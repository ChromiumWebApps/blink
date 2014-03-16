/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#ifndef DirectoryReaderSync_h
#define DirectoryReaderSync_h

#include "bindings/v8/ScriptWrappable.h"
#include "core/fileapi/FileError.h"
#include "heap/Handle.h"
#include "modules/filesystem/DirectoryReaderBase.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefCounted.h"
#include "wtf/Vector.h"
#include "wtf/text/WTFString.h"

namespace WebCore {

class EntrySync;
class ExceptionState;

typedef WillBeHeapVector<RefPtrWillBeMember<EntrySync> > EntrySyncHeapVector;

class DirectoryReaderSync : public DirectoryReaderBase, public ScriptWrappable {
public:
    static PassRefPtrWillBeRawPtr<DirectoryReaderSync> create(PassRefPtrWillBeRawPtr<DOMFileSystemBase> fileSystem, const String& fullPath)
    {
        return adoptRefWillBeNoop(new DirectoryReaderSync(fileSystem, fullPath));
    }

    virtual ~DirectoryReaderSync();

    EntrySyncHeapVector readEntries(ExceptionState&);

    void addEntries(const EntrySyncHeapVector& entries)
    {
        m_entries.appendVector(entries);
    }

    void setError(FileError::ErrorCode code)
    {
        m_errorCode = code;
    }

    virtual void trace(Visitor*) OVERRIDE;

private:
    class EntriesCallbackHelper;
    class ErrorCallbackHelper;

    DirectoryReaderSync(PassRefPtrWillBeRawPtr<DOMFileSystemBase>, const String& fullPath);

    int m_callbacksId;
    EntrySyncHeapVector m_entries;
    FileError::ErrorCode m_errorCode;
};

} // namespace

#endif // DirectoryReaderSync_h
