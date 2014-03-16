// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/battery/NavigatorBattery.h"

#include "core/frame/LocalFrame.h"
#include "modules/battery/BatteryManager.h"

namespace WebCore {

NavigatorBattery::NavigatorBattery()
{
}

NavigatorBattery::~NavigatorBattery()
{
}

BatteryManager* NavigatorBattery::battery(Navigator& navigator)
{
    return NavigatorBattery::from(navigator).batteryManager(navigator);
}

BatteryManager* NavigatorBattery::batteryManager(Navigator& navigator)
{
    if (!m_batteryManager && navigator.frame())
        m_batteryManager = BatteryManager::create(navigator.frame()->document());
    return m_batteryManager.get();
}

const char* NavigatorBattery::supplementName()
{
    return "NavigatorBattery";
}

NavigatorBattery& NavigatorBattery::from(Navigator& navigator)
{
    NavigatorBattery* supplement = static_cast<NavigatorBattery*>(Supplement<Navigator>::from(navigator, supplementName()));
    if (!supplement) {
        supplement = new NavigatorBattery();
        provideTo(navigator, supplementName(), adoptPtr(supplement));
    }
    return *supplement;
}

} // namespace WebCore
