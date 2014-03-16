/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

#ifndef DataObjectItem_h
#define DataObjectItem_h

#include "core/fileapi/File.h"
#include "heap/Handle.h"
#include "platform/SharedBuffer.h"
#include "platform/weborigin/KURL.h"
#include "wtf/RefCounted.h"
#include "wtf/RefPtr.h"
#include "wtf/text/WTFString.h"

namespace WebCore {

class Blob;

class DataObjectItem : public RefCountedWillBeGarbageCollectedFinalized<DataObjectItem> {
public:
    enum Kind {
        StringKind,
        FileKind
    };

    static PassRefPtrWillBeRawPtr<DataObjectItem> createFromString(const String& type, const String& data);
    static PassRefPtrWillBeRawPtr<DataObjectItem> createFromFile(PassRefPtrWillBeRawPtr<File>);
    static PassRefPtrWillBeRawPtr<DataObjectItem> createFromURL(const String& url, const String& title);
    static PassRefPtrWillBeRawPtr<DataObjectItem> createFromHTML(const String& html, const KURL& baseURL);
    static PassRefPtrWillBeRawPtr<DataObjectItem> createFromSharedBuffer(const String& filename, PassRefPtr<SharedBuffer>);
    static PassRefPtrWillBeRawPtr<DataObjectItem> createFromPasteboard(const String& type, uint64_t sequenceNumber);

    Kind kind() const { return m_kind; }
    String type() const { return m_type; }
    String getAsString() const;
    PassRefPtrWillBeRawPtr<Blob> getAsFile() const;

    // Used to support legacy DataTransfer APIs and renderer->browser serialization.
    PassRefPtr<SharedBuffer> sharedBuffer() const { return m_sharedBuffer; }
    String title() const { return m_title; }
    KURL baseURL() const { return m_baseURL; }
    bool isFilename() const;

    void trace(Visitor*);

private:
    enum DataSource {
        PasteboardSource,
        InternalSource,
    };

    DataObjectItem(Kind, const String& type);
    DataObjectItem(Kind, const String& type, uint64_t sequenceNumber);

    DataSource m_source;
    Kind m_kind;
    String m_type;

    String m_data;
    RefPtrWillBeMember<File> m_file;
    RefPtr<SharedBuffer> m_sharedBuffer;
    // Optional metadata. Currently used for URL, HTML, and dragging files in.
    String m_title;
    KURL m_baseURL;

    uint64_t m_sequenceNumber; // Only valid when m_source == PasteboardSource
};

} // namespace WebCore

#endif // DataObjectItem_h
