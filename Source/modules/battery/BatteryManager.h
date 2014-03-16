// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BatteryManager_h
#define BatteryManager_h

#include "core/dom/ContextLifecycleObserver.h"
#include "core/dom/Document.h"
#include "core/events/EventTarget.h"
#include "modules/battery/BatteryStatus.h"

namespace WebCore {

class Navigator;

class BatteryManager FINAL : public ContextLifecycleObserver, public RefCounted<BatteryManager>, public EventTargetWithInlineData {
    REFCOUNTED_EVENT_TARGET(BatteryManager);
public:
    virtual ~BatteryManager();
    static PassRefPtr<BatteryManager> create(ExecutionContext*);

    // EventTarget implementation.
    virtual const WTF::AtomicString& interfaceName() const OVERRIDE { return EventTargetNames::BatteryManager; }
    virtual ExecutionContext* executionContext() const OVERRIDE { return ContextLifecycleObserver::executionContext(); }

    bool charging();
    double chargingTime();
    double dischargingTime();
    double level();

    DEFINE_ATTRIBUTE_EVENT_LISTENER(chargingchange);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(chargingtimechange);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(dischargingtimechange);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(levelchange);

    void didChangeBatteryStatus(PassRefPtr<Event>, PassOwnPtr<BatteryStatus>);

private:
    explicit BatteryManager(ExecutionContext*);

    OwnPtr<BatteryStatus> m_batteryStatus;
};

}

#endif // BatteryManager_h

