/*
 * Copyright (C) 2007, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2013 Intel Corporation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef TransitionEvent_h
#define TransitionEvent_h

#include "core/events/Event.h"

namespace WebCore {

struct TransitionEventInit : public EventInit {
    TransitionEventInit();

    String propertyName;
    double elapsedTime;
    String pseudoElement;
};

class TransitionEvent FINAL : public Event {
public:
    static PassRefPtr<TransitionEvent> create()
    {
        return adoptRef(new TransitionEvent);
    }
    static PassRefPtr<TransitionEvent> create(const AtomicString& type, const String& propertyName, double elapsedTime, const String& pseudoElement)
    {
        return adoptRef(new TransitionEvent(type, propertyName, elapsedTime, pseudoElement));
    }
    static PassRefPtr<TransitionEvent> create(const AtomicString& type, const TransitionEventInit& initializer)
    {
        return adoptRef(new TransitionEvent(type, initializer));
    }

    virtual ~TransitionEvent();

    const String& propertyName() const;
    double elapsedTime() const;
    const String& pseudoElement() const;

    virtual const AtomicString& interfaceName() const OVERRIDE;

    virtual void trace(Visitor*) OVERRIDE;

private:
    TransitionEvent();
    TransitionEvent(const AtomicString& type, const String& propertyName, double elapsedTime, const String& pseudoElement);
    TransitionEvent(const AtomicString& type, const TransitionEventInit& initializer);

    String m_propertyName;
    double m_elapsedTime;
    String m_pseudoElement;
};

} // namespace WebCore

#endif // TransitionEvent_h

