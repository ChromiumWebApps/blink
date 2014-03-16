/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#ifndef SpeechInputEvent_h
#define SpeechInputEvent_h

#if ENABLE(INPUT_SPEECH)

#include "core/events/Event.h"
#include "core/speech/SpeechInputResultList.h"
#include "heap/Handle.h"
#include "wtf/PassRefPtr.h"

namespace WebCore {

class SpeechInputEvent FINAL : public Event {
public:
    static PassRefPtr<SpeechInputEvent> create();
    static PassRefPtr<SpeechInputEvent> create(const AtomicString& eventType, const SpeechInputResultArray& results);
    virtual ~SpeechInputEvent();

    SpeechInputResultList* results() const { return m_results.get(); }

    virtual const AtomicString& interfaceName() const OVERRIDE;

    virtual void trace(Visitor*) OVERRIDE;

private:
    SpeechInputEvent();
    SpeechInputEvent(const AtomicString& eventType, const SpeechInputResultArray& results);

    RefPtrWillBeMember<SpeechInputResultList> m_results;
};

} // namespace WebCore

#endif // ENABLE(INPUT_SPEECH)

#endif // SpeechInputEvent_h
