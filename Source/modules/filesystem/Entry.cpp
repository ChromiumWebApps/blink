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
#include "modules/filesystem/Entry.h"

#include "core/dom/ExecutionContext.h"
#include "core/fileapi/FileError.h"
#include "core/html/VoidCallback.h"
#include "modules/filesystem/DirectoryEntry.h"
#include "modules/filesystem/EntryCallback.h"
#include "modules/filesystem/ErrorCallback.h"
#include "modules/filesystem/FileSystemCallbacks.h"
#include "modules/filesystem/MetadataCallback.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "wtf/text/StringBuilder.h"

namespace WebCore {

Entry::Entry(PassRefPtrWillBeRawPtr<DOMFileSystemBase> fileSystem, const String& fullPath)
    : EntryBase(fileSystem, fullPath)
{
    ScriptWrappable::init(this);
}

void Entry::getMetadata(PassOwnPtr<MetadataCallback> successCallback, PassOwnPtr<ErrorCallback> errorCallback)
{
    m_fileSystem->getMetadata(this, successCallback, errorCallback);
}

void Entry::moveTo(PassRefPtrWillBeRawPtr<DirectoryEntry> parent, const String& name, PassOwnPtr<EntryCallback> successCallback, PassOwnPtr<ErrorCallback> errorCallback) const
{
    m_fileSystem->move(this, parent.get(), name, successCallback, errorCallback);
}

void Entry::copyTo(PassRefPtrWillBeRawPtr<DirectoryEntry> parent, const String& name, PassOwnPtr<EntryCallback> successCallback, PassOwnPtr<ErrorCallback> errorCallback) const
{
    m_fileSystem->copy(this, parent.get(), name, successCallback, errorCallback);
}

void Entry::remove(PassOwnPtr<VoidCallback> successCallback, PassOwnPtr<ErrorCallback> errorCallback) const
{
    m_fileSystem->remove(this, successCallback, errorCallback);
}

void Entry::getParent(PassOwnPtr<EntryCallback> successCallback, PassOwnPtr<ErrorCallback> errorCallback) const
{
    m_fileSystem->getParent(this, successCallback, errorCallback);
}

void Entry::trace(Visitor* visitor)
{
    EntryBase::trace(visitor);
}

} // namespace WebCore
