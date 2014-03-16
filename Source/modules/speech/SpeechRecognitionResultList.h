/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef SpeechRecognitionResultList_h
#define SpeechRecognitionResultList_h

#include "bindings/v8/ScriptWrappable.h"
#include "heap/Handle.h"
#include "modules/speech/SpeechRecognitionResult.h"
#include "wtf/RefCounted.h"
#include "wtf/Vector.h"

namespace WebCore {

class SpeechRecognitionResultList : public RefCountedWillBeGarbageCollectedFinalized<SpeechRecognitionResultList>, public ScriptWrappable {
public:
    static PassRefPtrWillBeRawPtr<SpeechRecognitionResultList> create(const WillBeHeapVector<RefPtrWillBeMember<SpeechRecognitionResult> >&);

    unsigned long length() { return m_results.size(); }
    SpeechRecognitionResult* item(unsigned long index);

    void trace(Visitor*);

private:
    explicit SpeechRecognitionResultList(const WillBeHeapVector<RefPtrWillBeMember<SpeechRecognitionResult> >&);

    WillBeHeapVector<RefPtrWillBeMember<SpeechRecognitionResult> > m_results;
};

} // namespace WebCore

#endif // SpeechRecognitionResultList_h
