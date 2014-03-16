/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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

#include "config.h"
#include "modules/device_orientation/DeviceMotionEvent.h"

#include "core/events/ThreadLocalEventNames.h"
#include "modules/device_orientation/DeviceAcceleration.h"
#include "modules/device_orientation/DeviceMotionData.h"
#include "modules/device_orientation/DeviceRotationRate.h"

namespace WebCore {

DeviceMotionEvent::~DeviceMotionEvent()
{
}

DeviceMotionEvent::DeviceMotionEvent()
    : m_deviceMotionData(DeviceMotionData::create())
{
    ScriptWrappable::init(this);
}

DeviceMotionEvent::DeviceMotionEvent(const AtomicString& eventType, DeviceMotionData* deviceMotionData)
    : Event(eventType, false, false) // Can't bubble, not cancelable
    , m_deviceMotionData(deviceMotionData)
{
    ScriptWrappable::init(this);
}

void DeviceMotionEvent::initDeviceMotionEvent(const AtomicString& type, bool bubbles, bool cancelable, DeviceMotionData* deviceMotionData)
{
    if (dispatched())
        return;

    initEvent(type, bubbles, cancelable);
    m_deviceMotionData = deviceMotionData;

    m_acceleration.clear();
    m_accelerationIncludingGravity.clear();
    m_rotationRate.clear();
}

DeviceAcceleration* DeviceMotionEvent::acceleration()
{
    if (!m_deviceMotionData->acceleration())
        return 0;

    if (!m_acceleration)
        m_acceleration = DeviceAcceleration::create(m_deviceMotionData->acceleration());

    return m_acceleration.get();
}

DeviceAcceleration* DeviceMotionEvent::accelerationIncludingGravity()
{
    if (!m_deviceMotionData->accelerationIncludingGravity())
        return 0;

    if (!m_accelerationIncludingGravity)
        m_accelerationIncludingGravity = DeviceAcceleration::create(m_deviceMotionData->accelerationIncludingGravity());

    return m_accelerationIncludingGravity.get();
}

DeviceRotationRate* DeviceMotionEvent::rotationRate()
{
    if (!m_deviceMotionData->rotationRate())
        return 0;

    if (!m_rotationRate)
        m_rotationRate = DeviceRotationRate::create(m_deviceMotionData->rotationRate());

    return m_rotationRate.get();
}

double DeviceMotionEvent::interval(bool& isNull) const
{
    if (m_deviceMotionData->canProvideInterval())
        return m_deviceMotionData->interval();

    isNull = true;
    return 0;
}

const AtomicString& DeviceMotionEvent::interfaceName() const
{
    return EventNames::DeviceMotionEvent;
}

void DeviceMotionEvent::trace(Visitor* visitor)
{
    visitor->trace(m_deviceMotionData);
    visitor->trace(m_acceleration);
    visitor->trace(m_accelerationIncludingGravity);
    visitor->trace(m_rotationRate);
    Event::trace(visitor);
}

} // namespace WebCore
