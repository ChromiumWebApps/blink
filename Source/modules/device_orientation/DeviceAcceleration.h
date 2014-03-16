/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#ifndef DeviceAcceleration_h
#define DeviceAcceleration_h

#include "bindings/v8/ScriptWrappable.h"
#include "heap/Handle.h"
#include "modules/device_orientation/DeviceMotionData.h"
#include "wtf/RefCounted.h"

namespace WebCore {

class DeviceAcceleration : public RefCountedWillBeGarbageCollectedFinalized<DeviceAcceleration>, public ScriptWrappable {
public:
    static PassRefPtrWillBeRawPtr<DeviceAcceleration> create(PassRefPtrWillBeRawPtr<DeviceMotionData::Acceleration> acceleration)
    {
        return adoptRefWillBeNoop(new DeviceAcceleration(acceleration));
    }
    void trace(Visitor*);

    double x(bool& isNull) const;
    double y(bool& isNull) const;
    double z(bool& isNull) const;

private:
    DeviceAcceleration(PassRefPtrWillBeRawPtr<DeviceMotionData::Acceleration>);

    RefPtrWillBeMember<DeviceMotionData::Acceleration> m_acceleration;
};

} // namespace WebCore

#endif // DeviceAcceleration_h
