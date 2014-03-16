// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/gamepad/Gamepad.h"

namespace WebCore {

PassRefPtrWillBeRawPtr<GamepadButton> GamepadButton::create()
{
    return adoptRefWillBeNoop(new GamepadButton());
}

GamepadButton::GamepadButton()
    : m_value(0.f)
    , m_pressed(false)
{
    ScriptWrappable::init(this);
}

GamepadButton::~GamepadButton()
{
}

void GamepadButton::trace(Visitor* visitor)
{
}

} // namespace WebCore
