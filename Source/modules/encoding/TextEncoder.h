/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#ifndef TextEncoder_h
#define TextEncoder_h

#include "heap/Handle.h"
#include "wtf/ArrayBufferView.h"
#include "wtf/RefCounted.h"
#include "wtf/Uint8Array.h"
#include "wtf/text/TextCodec.h"
#include "wtf/text/TextEncoding.h"
#include "wtf/text/WTFString.h"

namespace WebCore {

class Dictionary;
class ExceptionState;

class TextEncoder FINAL : public RefCountedWillBeGarbageCollectedFinalized<TextEncoder> {
public:
    static PassRefPtrWillBeRawPtr<TextEncoder> create(const String& utfLabel, ExceptionState&);
    ~TextEncoder();

    // Implement the IDL
    String encoding() const;
    PassRefPtr<Uint8Array> encode(const String&, const Dictionary&);

    void trace(Visitor*) { }

private:
    TextEncoder(const String& encoding);

    WTF::TextEncoding m_encoding;
    OwnPtr<WTF::TextCodec> m_codec;
};

} // namespace WebCore

#endif // TextEncoder_h
