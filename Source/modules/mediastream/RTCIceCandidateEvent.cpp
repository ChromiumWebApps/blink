/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include "modules/mediastream/RTCIceCandidateEvent.h"

#include "core/events/ThreadLocalEventNames.h"
#include "modules/mediastream/RTCIceCandidate.h"

namespace WebCore {

PassRefPtr<RTCIceCandidateEvent> RTCIceCandidateEvent::create()
{
    return adoptRef(new RTCIceCandidateEvent);
}

PassRefPtr<RTCIceCandidateEvent> RTCIceCandidateEvent::create(bool canBubble, bool cancelable, PassRefPtr<RTCIceCandidate> candidate)
{
    return adoptRef(new RTCIceCandidateEvent(canBubble, cancelable, candidate));
}

RTCIceCandidateEvent::RTCIceCandidateEvent()
{
    ScriptWrappable::init(this);
}

RTCIceCandidateEvent::RTCIceCandidateEvent(bool canBubble, bool cancelable, PassRefPtr<RTCIceCandidate> candidate)
    : Event(EventTypeNames::icecandidate, canBubble, cancelable)
    , m_candidate(candidate)
{
    ScriptWrappable::init(this);
}

RTCIceCandidateEvent::~RTCIceCandidateEvent()
{
}

RTCIceCandidate* RTCIceCandidateEvent::candidate() const
{
    return m_candidate.get();
}

const AtomicString& RTCIceCandidateEvent::interfaceName() const
{
    return EventNames::RTCIceCandidateEvent;
}

void RTCIceCandidateEvent::trace(Visitor* visitor)
{
    Event::trace(visitor);
}

} // namespace WebCore

