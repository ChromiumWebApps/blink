// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DOMWindowDeviceMotion_h
#define DOMWindowDeviceMotion_h

#include "core/events/EventTarget.h"

namespace WebCore {

class DOMWindowDeviceMotion {
public:
    DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(devicemotion);
};

} // namespace WebCore

#endif // DOMWindowDeviceMotion_h
