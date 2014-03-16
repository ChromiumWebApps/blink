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

#ifndef SpeechRecognitionEvent_h
#define SpeechRecognitionEvent_h

#include "core/events/Event.h"
#include "heap/Handle.h"
#include "heap/Heap.h"
#include "modules/speech/SpeechRecognitionResult.h"
#include "modules/speech/SpeechRecognitionResultList.h"
#include "wtf/RefPtr.h"

namespace WebCore {

class Document;

class SpeechRecognitionEventInit : public EventInit {
    DISALLOW_ALLOCATION();
public:
    SpeechRecognitionEventInit();

    unsigned long resultIndex;
    RefPtrWillBeRawPtr<SpeechRecognitionResultList> results;
};

class SpeechRecognitionEvent FINAL : public Event {
public:
    static PassRefPtr<SpeechRecognitionEvent> create();
    static PassRefPtr<SpeechRecognitionEvent> create(const AtomicString&, const SpeechRecognitionEventInit&);
    virtual ~SpeechRecognitionEvent();

    static PassRefPtr<SpeechRecognitionEvent> createResult(unsigned long resultIndex, const WillBeHeapVector<RefPtrWillBeMember<SpeechRecognitionResult> >& results);
    static PassRefPtr<SpeechRecognitionEvent> createNoMatch(PassRefPtrWillBeRawPtr<SpeechRecognitionResult>);

    unsigned long resultIndex() const { return m_resultIndex; }
    SpeechRecognitionResultList* results() const { return m_results.get(); }

    // These two methods are here to satisfy the specification which requires these attrubutes to exist.
    Document* interpretation() { return 0; }
    Document* emma() { return 0; }

    // Event
    virtual const AtomicString& interfaceName() const OVERRIDE;

    virtual void trace(Visitor*) OVERRIDE;

private:
    SpeechRecognitionEvent();
    SpeechRecognitionEvent(const AtomicString&, const SpeechRecognitionEventInit&);
    SpeechRecognitionEvent(const AtomicString& eventName, unsigned long resultIndex, PassRefPtrWillBeRawPtr<SpeechRecognitionResultList> results);

    unsigned long m_resultIndex;
    RefPtrWillBeMember<SpeechRecognitionResultList> m_results;
};

} // namespace WebCore

#endif // SpeechRecognitionEvent_h
