/*
 * Copyright 2010, The Android Open Source Project
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
#include "modules/device_orientation/DeviceOrientationEvent.h"

#include "core/events/ThreadLocalEventNames.h"
#include "modules/device_orientation/DeviceOrientationData.h"

namespace WebCore {

DeviceOrientationEvent::~DeviceOrientationEvent()
{
}

DeviceOrientationEvent::DeviceOrientationEvent()
    : m_orientation(DeviceOrientationData::create())
{
    ScriptWrappable::init(this);
}

DeviceOrientationEvent::DeviceOrientationEvent(const AtomicString& eventType, DeviceOrientationData* orientation)
    : Event(eventType, false, false) // Can't bubble, not cancelable
    , m_orientation(orientation)
{
    ScriptWrappable::init(this);
}

void DeviceOrientationEvent::initDeviceOrientationEvent(const AtomicString& type, bool bubbles, bool cancelable, DeviceOrientationData* orientation)
{
    if (dispatched())
        return;

    initEvent(type, bubbles, cancelable);
    m_orientation = orientation;
}

double DeviceOrientationEvent::alpha(bool& isNull) const
{
    if (m_orientation->canProvideAlpha())
        return m_orientation->alpha();

    isNull = true;
    return 0;
}

double DeviceOrientationEvent::beta(bool& isNull) const
{
    if (m_orientation->canProvideBeta())
        return m_orientation->beta();

    isNull = true;
    return 0;
}

double DeviceOrientationEvent::gamma(bool& isNull) const
{
    if (m_orientation->canProvideGamma())
        return m_orientation->gamma();

    isNull = true;
    return 0;
}

bool DeviceOrientationEvent::absolute(bool& isNull) const
{
    if (m_orientation->canProvideAbsolute())
        return m_orientation->absolute();

    isNull = true;
    return 0;
}

const AtomicString& DeviceOrientationEvent::interfaceName() const
{
    return EventNames::DeviceOrientationEvent;
}

void DeviceOrientationEvent::trace(Visitor* visitor)
{
    visitor->trace(m_orientation);
    Event::trace(visitor);
}

} // namespace WebCore
