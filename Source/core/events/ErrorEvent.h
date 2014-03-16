/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

#ifndef ErrorEvent_h
#define ErrorEvent_h

#include "bindings/v8/DOMWrapperWorld.h"
#include "core/events/Event.h"
#include "wtf/RefPtr.h"
#include "wtf/text/WTFString.h"

namespace WebCore {

struct ErrorEventInit : public EventInit {
    ErrorEventInit();

    String message;
    String filename;
    unsigned lineno;
    unsigned colno;
};

class ErrorEvent FINAL : public Event {
public:
    static PassRefPtr<ErrorEvent> create()
    {
        return adoptRef(new ErrorEvent);
    }
    static PassRefPtr<ErrorEvent> create(const String& message, const String& fileName, unsigned lineNumber, unsigned columnNumber, DOMWrapperWorld* world)
    {
        return adoptRef(new ErrorEvent(message, fileName, lineNumber, columnNumber, world));
    }
    static PassRefPtr<ErrorEvent> create(const AtomicString& type, const ErrorEventInit& initializer)
    {
        return adoptRef(new ErrorEvent(type, initializer));
    }
    static PassRefPtr<ErrorEvent> createSanitizedError(DOMWrapperWorld* world)
    {
        return adoptRef(new ErrorEvent("Script error.", String(), 0, 0, world));
    }
    virtual ~ErrorEvent();

    // As 'message' is exposed to JavaScript, never return unsanitizedMessage.
    const String& message() const { return m_sanitizedMessage; }
    const String& filename() const { return m_fileName; }
    unsigned lineno() const { return m_lineNumber; }
    unsigned colno() const { return m_columnNumber; }

    // 'messageForConsole' is not exposed to JavaScript, and prefers 'm_unsanitizedMessage'.
    const String& messageForConsole() const { return !m_unsanitizedMessage.isEmpty() ? m_unsanitizedMessage : m_sanitizedMessage; }

    virtual const AtomicString& interfaceName() const OVERRIDE;

    DOMWrapperWorld* world() const { return m_world.get(); }

    void setUnsanitizedMessage(const String&);

    virtual void trace(Visitor*) OVERRIDE;

private:
    ErrorEvent();
    ErrorEvent(const String& message, const String& fileName, unsigned lineNumber, unsigned columnNumber, DOMWrapperWorld*);
    ErrorEvent(const AtomicString&, const ErrorEventInit&);

    String m_unsanitizedMessage;
    String m_sanitizedMessage;
    String m_fileName;
    unsigned m_lineNumber;
    unsigned m_columnNumber;

    RefPtr<DOMWrapperWorld> m_world;
};

} // namespace WebCore

#endif // ErrorEvent_h
