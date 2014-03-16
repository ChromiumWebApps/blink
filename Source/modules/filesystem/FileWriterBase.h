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

#ifndef FileWriterBase_h
#define FileWriterBase_h

#include "heap/Handle.h"
#include "wtf/OwnPtr.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/RefCounted.h"

namespace blink { class WebFileWriter; }

namespace WebCore {

class FileWriterBase : public RefCountedWillBeRefCountedGarbageCollected<FileWriterBase> {
public:
    virtual ~FileWriterBase();
    void initialize(PassOwnPtr<blink::WebFileWriter>, long long length);

    long long position() const
    {
        return m_position;
    }
    long long length() const
    {
        return m_length;
    }

    virtual void trace(Visitor*) { }

protected:
    FileWriterBase();

    blink::WebFileWriter* writer()
    {
        return m_writer.get();
    }

    void setPosition(long long position)
    {
        m_position = position;
    }

    void setLength(long long length)
    {
        m_length = length;
    }

    void seekInternal(long long position);

private:
    friend class WTF::RefCounted<FileWriterBase>;

    OwnPtr<blink::WebFileWriter> m_writer;
    long long m_position;
    long long m_length;
};

} // namespace WebCore

#endif // FileWriterBase_h
