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

#ifndef FileEntrySync_h
#define FileEntrySync_h

#include "heap/Handle.h"
#include "modules/filesystem/EntrySync.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefCounted.h"
#include "wtf/text/WTFString.h"

namespace WebCore {

class ExceptionState;
class File;
class FileWriterSync;

class FileEntrySync FINAL : public EntrySync {
public:
    static PassRefPtrWillBeRawPtr<FileEntrySync> create(PassRefPtrWillBeRawPtr<DOMFileSystemBase> fileSystem, const String& fullPath)
    {
        return adoptRefWillBeNoop(new FileEntrySync(fileSystem, fullPath));
    }

    virtual bool isFile() const OVERRIDE { return true; }

    PassRefPtrWillBeRawPtr<File> file(ExceptionState&);
    PassRefPtrWillBeRawPtr<FileWriterSync> createWriter(ExceptionState&);

    virtual void trace(Visitor*) OVERRIDE;

private:
    FileEntrySync(PassRefPtrWillBeRawPtr<DOMFileSystemBase>, const String& fullPath);
};

DEFINE_TYPE_CASTS(FileEntrySync, EntrySync, entry, entry->isFile(), entry.isFile());

}

#endif // FileEntrySync_h
