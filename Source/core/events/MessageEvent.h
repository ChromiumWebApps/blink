/*
 * Copyright (C) 2007 Henry Mason (hmason@mac.com)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
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
 *
 */

#ifndef MessageEvent_h
#define MessageEvent_h

#include "bindings/v8/SerializedScriptValue.h"
#include "core/events/Event.h"
#include "core/events/EventTarget.h"
#include "core/dom/MessagePort.h"
#include "core/fileapi/Blob.h"
#include "core/frame/DOMWindow.h"
#include "wtf/ArrayBuffer.h"

namespace WebCore {

struct MessageEventInit : public EventInit {
    MessageEventInit();

    String origin;
    String lastEventId;
    RefPtr<EventTarget> source;
    MessagePortArray ports;
};

class MessageEvent FINAL : public Event {
public:
    static PassRefPtr<MessageEvent> create()
    {
        return adoptRef(new MessageEvent);
    }
    static PassRefPtr<MessageEvent> create(PassOwnPtr<MessagePortArray> ports, const String& origin = String(), const String& lastEventId = String(), PassRefPtr<EventTarget> source = nullptr)
    {
        return adoptRef(new MessageEvent(origin, lastEventId, source, ports));
    }
    static PassRefPtr<MessageEvent> create(PassOwnPtr<MessagePortArray> ports, PassRefPtr<SerializedScriptValue> data, const String& origin = String(), const String& lastEventId = String(), PassRefPtr<EventTarget> source = nullptr)
    {
        return adoptRef(new MessageEvent(data, origin, lastEventId, source, ports));
    }
    static PassRefPtr<MessageEvent> create(PassOwnPtr<MessagePortChannelArray> channels, PassRefPtr<SerializedScriptValue> data, const String& origin = String(), const String& lastEventId = String(), PassRefPtr<EventTarget> source = nullptr)
    {
        return adoptRef(new MessageEvent(data, origin, lastEventId, source, channels));
    }
    static PassRefPtr<MessageEvent> create(const String& data, const String& origin = String())
    {
        return adoptRef(new MessageEvent(data, origin));
    }
    static PassRefPtr<MessageEvent> create(PassRefPtrWillBeRawPtr<Blob> data, const String& origin = String())
    {
        return adoptRef(new MessageEvent(data, origin));
    }
    static PassRefPtr<MessageEvent> create(PassRefPtr<ArrayBuffer> data, const String& origin = String())
    {
        return adoptRef(new MessageEvent(data, origin));
    }
    static PassRefPtr<MessageEvent> create(const AtomicString& type, const MessageEventInit& initializer, ExceptionState&);
    virtual ~MessageEvent();

    void initMessageEvent(const AtomicString& type, bool canBubble, bool cancelable, const String& origin, const String& lastEventId, DOMWindow* source, PassOwnPtr<MessagePortArray>);
    void initMessageEvent(const AtomicString& type, bool canBubble, bool cancelable, PassRefPtr<SerializedScriptValue> data, const String& origin, const String& lastEventId, DOMWindow* source, PassOwnPtr<MessagePortArray>);

    const String& origin() const { return m_origin; }
    const String& lastEventId() const { return m_lastEventId; }
    EventTarget* source() const { return m_source.get(); }
    EventTarget* source(bool& isNull) const { isNull = !m_source; return m_source.get(); }
    MessagePortArray ports() const { return m_ports ? *m_ports : MessagePortArray(); }
    MessagePortChannelArray* channels() const { return m_channels ? m_channels.get() : 0; }

    virtual const AtomicString& interfaceName() const OVERRIDE;

    enum DataType {
        DataTypeScriptValue,
        DataTypeSerializedScriptValue,
        DataTypeString,
        DataTypeBlob,
        DataTypeArrayBuffer
    };
    DataType dataType() const { return m_dataType; }
    SerializedScriptValue* dataAsSerializedScriptValue() const { ASSERT(m_dataType == DataTypeScriptValue || m_dataType == DataTypeSerializedScriptValue); return m_dataAsSerializedScriptValue.get(); }
    String dataAsString() const { ASSERT(m_dataType == DataTypeString); return m_dataAsString; }
    Blob* dataAsBlob() const { ASSERT(m_dataType == DataTypeBlob); return m_dataAsBlob.get(); }
    ArrayBuffer* dataAsArrayBuffer() const { ASSERT(m_dataType == DataTypeArrayBuffer); return m_dataAsArrayBuffer.get(); }

    void setSerializedData(PassRefPtr<SerializedScriptValue> data)
    {
        ASSERT(!m_dataAsSerializedScriptValue);
        m_dataAsSerializedScriptValue = data;
    }

    void entangleMessagePorts(ExecutionContext*);

    virtual void trace(Visitor*) OVERRIDE;

private:
    MessageEvent();
    MessageEvent(const AtomicString&, const MessageEventInit&);
    MessageEvent(const String& origin, const String& lastEventId, PassRefPtr<EventTarget> source, PassOwnPtr<MessagePortArray>);
    MessageEvent(PassRefPtr<SerializedScriptValue> data, const String& origin, const String& lastEventId, PassRefPtr<EventTarget> source, PassOwnPtr<MessagePortArray>);
    MessageEvent(PassRefPtr<SerializedScriptValue> data, const String& origin, const String& lastEventId, PassRefPtr<EventTarget> source, PassOwnPtr<MessagePortChannelArray>);

    explicit MessageEvent(const String& data, const String& origin);
    explicit MessageEvent(PassRefPtrWillBeRawPtr<Blob> data, const String& origin);
    explicit MessageEvent(PassRefPtr<ArrayBuffer> data, const String& origin);

    DataType m_dataType;
    RefPtr<SerializedScriptValue> m_dataAsSerializedScriptValue;
    String m_dataAsString;
    RefPtrWillBePersistent<Blob> m_dataAsBlob;
    RefPtr<ArrayBuffer> m_dataAsArrayBuffer;
    String m_origin;
    String m_lastEventId;
    RefPtr<EventTarget> m_source;
    // m_ports are the MessagePorts in an engtangled state, and m_channels are
    // the MessageChannels in a disentangled state. Only one of them can be
    // non-empty at a time. entangleMessagePorts() moves between the states.
    OwnPtr<MessagePortArray> m_ports;
    OwnPtr<MessagePortChannelArray> m_channels;
};

} // namespace WebCore

#endif // MessageEvent_h
