/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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
#include "modules/filesystem/DataTransferItemFileSystem.h"

#include "core/clipboard/Clipboard.h"
#include "core/clipboard/DataObject.h"
#include "core/clipboard/DataTransferItem.h"
#include "core/dom/ExecutionContext.h"
#include "core/fileapi/File.h"
#include "modules/filesystem/DOMFilePath.h"
#include "modules/filesystem/DOMFileSystem.h"
#include "modules/filesystem/DirectoryEntry.h"
#include "modules/filesystem/DraggedIsolatedFileSystem.h"
#include "modules/filesystem/Entry.h"
#include "modules/filesystem/FileEntry.h"
#include "platform/AsyncFileSystemCallbacks.h"
#include "platform/FileMetadata.h"

namespace WebCore {

// static
PassRefPtrWillBeRawPtr<Entry> DataTransferItemFileSystem::webkitGetAsEntry(ExecutionContext* executionContext, DataTransferItem& item)
{
    if (!item.dataObjectItem()->isFilename())
        return nullptr;

    // For dragged files getAsFile must be pretty lightweight.
    Blob* file = item.getAsFile().get();
    // The clipboard may not be in a readable state.
    if (!file)
        return nullptr;
    ASSERT(file->isFile());

    DraggedIsolatedFileSystem* filesystem = DraggedIsolatedFileSystem::from(item.clipboard()->dataObject().get());
    DOMFileSystem* domFileSystem = filesystem ? filesystem->getDOMFileSystem(executionContext) : 0;
    if (!filesystem) {
        // IsolatedFileSystem may not be enabled.
        return nullptr;
    }

    ASSERT(domFileSystem);

    // The dropped entries are mapped as top-level entries in the isolated filesystem.
    String virtualPath = DOMFilePath::append("/", toFile(file)->name());

    // FIXME: This involves synchronous file operation. Consider passing file type data when we dispatch drag event.
    FileMetadata metadata;
    if (!getFileMetadata(toFile(file)->path(), metadata))
        return nullptr;

    if (metadata.type == FileMetadata::TypeDirectory)
        return DirectoryEntry::create(domFileSystem, virtualPath);
    return FileEntry::create(domFileSystem, virtualPath);
}

} // namespace WebCore
