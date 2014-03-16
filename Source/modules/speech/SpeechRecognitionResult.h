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

#ifndef SpeechRecognitionResult_h
#define SpeechRecognitionResult_h

#include "bindings/v8/ScriptWrappable.h"
#include "heap/Handle.h"
#include "modules/speech/SpeechRecognitionAlternative.h"
#include "wtf/RefCounted.h"
#include "wtf/Vector.h"

namespace WebCore {

// FIXME: oilpan: the platform outer layer (WebSpeechRecognitionResult) depends on
// holding a WebPrivatePtr (a RefPtr) to this result object. When/if such pointers
// can be to GCed objects, we can drop the extra reference counting layer.
class SpeechRecognitionResult : public RefCountedWillBeGarbageCollectedFinalized<SpeechRecognitionResult>, public ScriptWrappable {
public:
    ~SpeechRecognitionResult();
    static PassRefPtrWillBeRawPtr<SpeechRecognitionResult> create(const WillBeHeapVector<RefPtrWillBeMember<SpeechRecognitionAlternative> >&, bool final);

    unsigned long length() { return m_alternatives.size(); }
    SpeechRecognitionAlternative* item(unsigned long index);
    bool isFinal() { return m_final; }

    void trace(Visitor*);

private:
    SpeechRecognitionResult(const WillBeHeapVector<RefPtrWillBeMember<SpeechRecognitionAlternative> >&, bool final);

    bool m_final;
    WillBeHeapVector<RefPtrWillBeMember<SpeechRecognitionAlternative> > m_alternatives;
};

} // namespace WebCore

#endif // SpeechRecognitionResult_h
