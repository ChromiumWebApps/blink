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

#include "modules/speech/SpeechRecognition.h"

#include "bindings/v8/ExceptionState.h"
#include "core/dom/Document.h"
#include "core/dom/ExceptionCode.h"
#include "core/page/Page.h"
#include "modules/speech/SpeechRecognitionController.h"
#include "modules/speech/SpeechRecognitionError.h"
#include "modules/speech/SpeechRecognitionEvent.h"

namespace WebCore {

PassRefPtrWillBeRawPtr<SpeechRecognition> SpeechRecognition::create(ExecutionContext* context)
{
    RefPtrWillBeRawPtr<SpeechRecognition> speechRecognition(adoptRefWillBeRefCountedGarbageCollected(new SpeechRecognition(context)));
    speechRecognition->suspendIfNeeded();
    return speechRecognition.release();
}

void SpeechRecognition::start(ExceptionState& exceptionState)
{
    ASSERT(m_controller);
    if (m_started) {
        exceptionState.throwDOMException(InvalidStateError, "recognition has already started.");
        return;
    }

    setPendingActivity(this);
    m_finalResults.clear();
    m_controller->start(this, m_grammars.get(), m_lang, m_continuous, m_interimResults, m_maxAlternatives);
    m_started = true;
}

void SpeechRecognition::stopFunction()
{
    ASSERT(m_controller);
    if (m_started && !m_stopping) {
        m_stopping = true;
        m_controller->stop(this);
    }
}

void SpeechRecognition::abort()
{
    ASSERT(m_controller);
    if (m_started && !m_stopping) {
        m_stopping = true;
        m_controller->abort(this);
    }
}

void SpeechRecognition::didStartAudio()
{
    dispatchEvent(Event::create(EventTypeNames::audiostart));
}

void SpeechRecognition::didStartSound()
{
    dispatchEvent(Event::create(EventTypeNames::soundstart));
}

void SpeechRecognition::didStartSpeech()
{
    dispatchEvent(Event::create(EventTypeNames::speechstart));
}

void SpeechRecognition::didEndSpeech()
{
    dispatchEvent(Event::create(EventTypeNames::speechend));
}

void SpeechRecognition::didEndSound()
{
    dispatchEvent(Event::create(EventTypeNames::soundend));
}

void SpeechRecognition::didEndAudio()
{
    dispatchEvent(Event::create(EventTypeNames::audioend));
}

void SpeechRecognition::didReceiveResults(const WillBeHeapVector<RefPtrWillBeMember<SpeechRecognitionResult> >& newFinalResults, const WillBeHeapVector<RefPtrWillBeMember<SpeechRecognitionResult> >& currentInterimResults)
{
    unsigned long resultIndex = m_finalResults.size();

    for (size_t i = 0; i < newFinalResults.size(); ++i)
        m_finalResults.append(newFinalResults[i]);

    WillBeHeapVector<RefPtrWillBeMember<SpeechRecognitionResult> > results = m_finalResults;
    for (size_t i = 0; i < currentInterimResults.size(); ++i)
        results.append(currentInterimResults[i]);

    dispatchEvent(SpeechRecognitionEvent::createResult(resultIndex, results));
}

void SpeechRecognition::didReceiveNoMatch(PassRefPtrWillBeRawPtr<SpeechRecognitionResult> result)
{
    dispatchEvent(SpeechRecognitionEvent::createNoMatch(result));
}

void SpeechRecognition::didReceiveError(PassRefPtr<SpeechRecognitionError> error)
{
    dispatchEvent(error);
    m_started = false;
}

void SpeechRecognition::didStart()
{
    dispatchEvent(Event::create(EventTypeNames::start));
}

void SpeechRecognition::didEnd()
{
    m_started = false;
    m_stopping = false;
    if (!m_stoppedByActiveDOMObject)
        dispatchEvent(Event::create(EventTypeNames::end));
    unsetPendingActivity(this);
}

const AtomicString& SpeechRecognition::interfaceName() const
{
    return EventTargetNames::SpeechRecognition;
}

ExecutionContext* SpeechRecognition::executionContext() const
{
    return ActiveDOMObject::executionContext();
}

void SpeechRecognition::stop()
{
    m_stoppedByActiveDOMObject = true;
    if (hasPendingActivity())
        abort();
}

SpeechRecognition::SpeechRecognition(ExecutionContext* context)
    : ActiveDOMObject(context)
    , m_grammars(SpeechGrammarList::create()) // FIXME: The spec is not clear on the default value for the grammars attribute.
    , m_continuous(false)
    , m_interimResults(false)
    , m_maxAlternatives(1)
    , m_controller(0)
    , m_stoppedByActiveDOMObject(false)
    , m_started(false)
    , m_stopping(false)
{
    ScriptWrappable::init(this);
    Document* document = toDocument(executionContext());

    Page* page = document->page();
    ASSERT(page);

    m_controller = SpeechRecognitionController::from(page);
    ASSERT(m_controller);

    // FIXME: Need to hook up with Page to get notified when the visibility changes.
}

SpeechRecognition::~SpeechRecognition()
{
}

void SpeechRecognition::trace(Visitor* visitor)
{
    visitor->trace(m_grammars);
    visitor->trace(m_finalResults);
}

} // namespace WebCore
