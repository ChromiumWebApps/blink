// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FetchEvent_h
#define FetchEvent_h

#include "core/events/Event.h"
#include "modules/serviceworkers/RespondWithObserver.h"

namespace WebCore {

class ExecutionContext;
class RespondWithObserver;

// A fetch event is dispatched by the client to a service worker's script
// context. RespondWithObserver can be used to notify the client about the
// service worker's response.
class FetchEvent FINAL : public Event {
public:
    static PassRefPtr<FetchEvent> create();
    static PassRefPtr<FetchEvent> create(PassRefPtr<RespondWithObserver>);
    virtual ~FetchEvent() { }

    void respondWith(const ScriptValue&);

    virtual const AtomicString& interfaceName() const OVERRIDE;

protected:
    FetchEvent();
    explicit FetchEvent(PassRefPtr<RespondWithObserver>);

private:
    RefPtr<RespondWithObserver> m_observer;
};

} // namespace WebCore

#endif // FetchEvent_h
