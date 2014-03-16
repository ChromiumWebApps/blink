// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "FetchEvent.h"

#include "core/events/ThreadLocalEventNames.h"
#include "modules/serviceworkers/ServiceWorkerGlobalScope.h"
#include "wtf/RefPtr.h"

namespace WebCore {

PassRefPtr<FetchEvent> FetchEvent::create()
{
    return adoptRef(new FetchEvent());
}

PassRefPtr<FetchEvent> FetchEvent::create(PassRefPtr<RespondWithObserver> observer)
{
    return adoptRef(new FetchEvent(observer));
}

void FetchEvent::respondWith(const ScriptValue& value)
{
    m_observer->respondWith(value);
}

const AtomicString& FetchEvent::interfaceName() const
{
    return EventNames::FetchEvent;
}

FetchEvent::FetchEvent()
{
    ScriptWrappable::init(this);
}

FetchEvent::FetchEvent(PassRefPtr<RespondWithObserver> observer)
    : Event(EventTypeNames::fetch, /*canBubble=*/false, /*cancelable=*/true)
    , m_observer(observer)
{
    ScriptWrappable::init(this);
}

} // namespace WebCore
