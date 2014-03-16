/*
 * Copyright (C) 2008, 2009 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifndef WorkerGlobalScopeFileSystem_h
#define WorkerGlobalScopeFileSystem_h

#include "heap/Handle.h"
#include "modules/filesystem/DOMFileSystemSync.h"
#include "wtf/PassRefPtr.h"

namespace WebCore {

class EntryCallback;
class EntrySync;
class ErrorCallback;
class ExceptionState;
class FileSystemCallback;
class WorkerGlobalScope;

class WorkerGlobalScopeFileSystem {
public:
    enum {
        TEMPORARY,
        PERSISTENT,
    };

    static void webkitRequestFileSystem(WorkerGlobalScope&, int type, long long size, PassOwnPtr<FileSystemCallback> successCallback, PassOwnPtr<ErrorCallback>);
    static PassRefPtrWillBeRawPtr<DOMFileSystemSync> webkitRequestFileSystemSync(WorkerGlobalScope&, int type, long long size, ExceptionState&);
    static void webkitResolveLocalFileSystemURL(WorkerGlobalScope&, const String& url, PassOwnPtr<EntryCallback> successCallback, PassOwnPtr<ErrorCallback>);
    static PassRefPtrWillBeRawPtr<EntrySync> webkitResolveLocalFileSystemSyncURL(WorkerGlobalScope&, const String& url, ExceptionState&);

private:
    WorkerGlobalScopeFileSystem();
    ~WorkerGlobalScopeFileSystem();
};

} // namespace WebCore

#endif // WorkerGlobalScopeFileSystem_h
