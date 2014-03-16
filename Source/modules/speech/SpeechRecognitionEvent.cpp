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

#include "config.h"

#include "modules/speech/SpeechRecognitionEvent.h"

namespace WebCore {

SpeechRecognitionEventInit::SpeechRecognitionEventInit()
    : resultIndex(0)
{
}

PassRefPtr<SpeechRecognitionEvent> SpeechRecognitionEvent::create()
{
    return adoptRef(new SpeechRecognitionEvent());
}

PassRefPtr<SpeechRecognitionEvent> SpeechRecognitionEvent::create(const AtomicString& eventName, const SpeechRecognitionEventInit& initializer)
{
    return adoptRef(new SpeechRecognitionEvent(eventName, initializer));
}

PassRefPtr<SpeechRecognitionEvent> SpeechRecognitionEvent::createResult(unsigned long resultIndex, const WillBeHeapVector<RefPtrWillBeMember<SpeechRecognitionResult> >& results)
{
    return adoptRef(new SpeechRecognitionEvent(EventTypeNames::result, resultIndex, SpeechRecognitionResultList::create(results)));
}

PassRefPtr<SpeechRecognitionEvent> SpeechRecognitionEvent::createNoMatch(PassRefPtrWillBeRawPtr<SpeechRecognitionResult> result)
{
    if (result) {
        WillBeHeapVector<RefPtrWillBeMember<SpeechRecognitionResult> > results;
        results.append(result);
        return adoptRef(new SpeechRecognitionEvent(EventTypeNames::nomatch, 0, SpeechRecognitionResultList::create(results)));
    }

    return adoptRef(new SpeechRecognitionEvent(EventTypeNames::nomatch, 0, nullptr));
}

const AtomicString& SpeechRecognitionEvent::interfaceName() const
{
    return EventNames::SpeechRecognitionEvent;
}

SpeechRecognitionEvent::SpeechRecognitionEvent()
    : m_resultIndex(0)
{
    ScriptWrappable::init(this);
}

SpeechRecognitionEvent::SpeechRecognitionEvent(const AtomicString& eventName, const SpeechRecognitionEventInit& initializer)
    : Event(eventName, initializer)
    , m_resultIndex(initializer.resultIndex)
    , m_results(initializer.results)
{
    ScriptWrappable::init(this);
}

SpeechRecognitionEvent::SpeechRecognitionEvent(const AtomicString& eventName, unsigned long resultIndex, PassRefPtrWillBeRawPtr<SpeechRecognitionResultList> results)
    : Event(eventName, /*canBubble=*/false, /*cancelable=*/false)
    , m_resultIndex(resultIndex)
    , m_results(results)
{
    ScriptWrappable::init(this);
}

SpeechRecognitionEvent::~SpeechRecognitionEvent()
{
    ScriptWrappable::init(this);
}

void SpeechRecognitionEvent::trace(Visitor* visitor)
{
    visitor->trace(m_results);
    Event::trace(visitor);
}

} // namespace WebCore
