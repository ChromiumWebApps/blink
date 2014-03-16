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

#include "config.h"
#include "modules/filesystem/DirectoryReaderSync.h"

#include "bindings/v8/ExceptionState.h"
#include "core/dom/ExceptionCode.h"
#include "modules/filesystem/DirectoryEntry.h"
#include "modules/filesystem/DirectoryEntrySync.h"
#include "modules/filesystem/EntriesCallback.h"
#include "modules/filesystem/EntrySync.h"
#include "modules/filesystem/ErrorCallback.h"
#include "modules/filesystem/FileEntrySync.h"

namespace WebCore {

class DirectoryReaderSync::EntriesCallbackHelper : public EntriesCallback {
public:
    EntriesCallbackHelper(PassRefPtrWillBeRawPtr<DirectoryReaderSync> reader)
        : m_reader(reader)
    {
    }

    virtual void handleEvent(const EntryHeapVector& entries) OVERRIDE
    {
        EntrySyncHeapVector syncEntries;
        syncEntries.reserveInitialCapacity(entries.size());
        for (size_t i = 0; i < entries.size(); ++i)
            syncEntries.uncheckedAppend(EntrySync::create(entries[i].get()));
        m_reader->addEntries(syncEntries);
    }

private:
    RefPtrWillBePersistent<DirectoryReaderSync> m_reader;
};

class DirectoryReaderSync::ErrorCallbackHelper : public ErrorCallback {
public:
    ErrorCallbackHelper(PassRefPtrWillBeRawPtr<DirectoryReaderSync> reader)
        : m_reader(reader)
    {
    }

    virtual void handleEvent(FileError* error) OVERRIDE
    {
        m_reader->setError(error->code());
    }

private:
    RefPtrWillBePersistent<DirectoryReaderSync> m_reader;
};

DirectoryReaderSync::DirectoryReaderSync(PassRefPtrWillBeRawPtr<DOMFileSystemBase> fileSystem, const String& fullPath)
    : DirectoryReaderBase(fileSystem, fullPath)
    , m_callbacksId(0)
    , m_errorCode(FileError::OK)
{
    ScriptWrappable::init(this);
}

DirectoryReaderSync::~DirectoryReaderSync()
{
}

EntrySyncHeapVector DirectoryReaderSync::readEntries(ExceptionState& exceptionState)
{
    if (!m_callbacksId) {
        m_callbacksId = filesystem()->readDirectory(this, m_fullPath, adoptPtr(new EntriesCallbackHelper(this)), adoptPtr(new ErrorCallbackHelper(this)), DOMFileSystemBase::Synchronous);
    }

    if (m_errorCode == FileError::OK && m_hasMoreEntries && m_entries.isEmpty())
        m_fileSystem->waitForAdditionalResult(m_callbacksId);

    if (m_errorCode != FileError::OK) {
        FileError::throwDOMException(exceptionState, m_errorCode);
        return EntrySyncHeapVector();
    }

    EntrySyncHeapVector result;
    result.swap(m_entries);
    return result;
}

void DirectoryReaderSync::trace(Visitor* visitor)
{
    visitor->trace(m_entries);
    DirectoryReaderBase::trace(visitor);
}

} // namespace
