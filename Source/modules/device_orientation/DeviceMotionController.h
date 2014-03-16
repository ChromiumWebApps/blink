/*
 * Copyright 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2012 Samsung Electronics. All rights reserved.
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

#ifndef DeviceMotionController_h
#define DeviceMotionController_h

#include "core/dom/DocumentSupplementable.h"
#include "core/events/Event.h"
#include "core/frame/DOMWindowLifecycleObserver.h"
#include "core/frame/DeviceSensorEventController.h"

namespace WebCore {

class DeviceMotionData;
class DOMWindow;

class DeviceMotionController FINAL : public DeviceSensorEventController, public DocumentSupplement, public DOMWindowLifecycleObserver {

public:
    virtual ~DeviceMotionController();

    static const char* supplementName();
    static DeviceMotionController& from(Document&);

    void didChangeDeviceMotion(DeviceMotionData*);

private:
    explicit DeviceMotionController(Document&);
    virtual void registerWithDispatcher() OVERRIDE;
    virtual void unregisterWithDispatcher() OVERRIDE;

    // Inherited from DOMWindowLifecycleObserver.
    virtual void didAddEventListener(DOMWindow*, const AtomicString&) OVERRIDE;
    virtual void didRemoveEventListener(DOMWindow*, const AtomicString&) OVERRIDE;
    virtual void didRemoveAllEventListeners(DOMWindow*) OVERRIDE;

    virtual bool hasLastData() OVERRIDE;
    virtual PassRefPtr<Event> getLastEvent() OVERRIDE;
    virtual bool isNullEvent(Event*) OVERRIDE;
};

} // namespace WebCore

#endif // DeviceMotionController_h
