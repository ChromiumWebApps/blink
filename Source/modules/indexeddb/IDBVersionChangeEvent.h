/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef IDBVersionChangeEvent_h
#define IDBVersionChangeEvent_h

#include "bindings/v8/Nullable.h"
#include "bindings/v8/SerializedScriptValue.h"
#include "core/events/Event.h"
#include "modules/indexeddb/IDBAny.h"
#include "modules/indexeddb/IDBRequest.h"
#include "public/platform/WebIDBTypes.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefPtr.h"
#include "wtf/text/WTFString.h"

namespace WebCore {

struct IDBVersionChangeEventInit : public EventInit {
    IDBVersionChangeEventInit();

    unsigned long long oldVersion;
    Nullable<unsigned long long> newVersion;
};

class IDBVersionChangeEvent FINAL : public Event {
public:
    static PassRefPtr<IDBVersionChangeEvent> create()
    {
        return adoptRef(new IDBVersionChangeEvent());
    }
    static PassRefPtr<IDBVersionChangeEvent> create(const AtomicString& eventType, unsigned long long oldVersion, const Nullable<unsigned long long>& newVersion, blink::WebIDBDataLoss dataLoss = blink::WebIDBDataLossNone, const String& dataLossMessage = String())
    {
        return adoptRef(new IDBVersionChangeEvent(eventType, oldVersion, newVersion, dataLoss, dataLossMessage));
    }
    static PassRefPtr<IDBVersionChangeEvent> create(const AtomicString& eventType, const IDBVersionChangeEventInit& initializer)
    {
        return adoptRef(new IDBVersionChangeEvent(eventType, initializer));
    }

    unsigned long long oldVersion() const { return m_oldVersion; }
    unsigned long long newVersion(bool& isNull) const;

    const AtomicString& dataLoss() const;
    const String& dataLossMessage() const { return m_dataLossMessage; }

    virtual const AtomicString& interfaceName() const OVERRIDE;

    virtual void trace(Visitor*) OVERRIDE;

private:
    IDBVersionChangeEvent();
    IDBVersionChangeEvent(const AtomicString& eventType, unsigned long long oldVersion, const Nullable<unsigned long long>& newVersion, blink::WebIDBDataLoss, const String& dataLoss);
    IDBVersionChangeEvent(const AtomicString& eventType, const IDBVersionChangeEventInit&);

    unsigned long long m_oldVersion;
    Nullable<unsigned long long> m_newVersion;
    bool m_dataLoss;
    String m_dataLossMessage;
};

} // namespace WebCore

#endif // IDBVersionChangeEvent_h
