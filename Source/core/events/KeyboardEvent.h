/*
 * Copyright (C) 2001 Peter Kelly (pmk@post.com)
 * Copyright (C) 2001 Tobias Anton (anton@stud.fbi.fh-darmstadt.de)
 * Copyright (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef KeyboardEvent_h
#define KeyboardEvent_h

#include "core/events/EventDispatchMediator.h"
#include "core/events/UIEventWithKeyState.h"

namespace WebCore {

class EventDispatcher;
class Node;
class PlatformKeyboardEvent;

struct KeyboardEventInit : public UIEventInit {
    KeyboardEventInit();

    String keyIdentifier;
    unsigned location;
    bool ctrlKey;
    bool altKey;
    bool shiftKey;
    bool metaKey;
    bool repeat;
};

class KeyboardEvent FINAL : public UIEventWithKeyState {
public:
    enum KeyLocationCode {
        DOM_KEY_LOCATION_STANDARD   = 0x00,
        DOM_KEY_LOCATION_LEFT       = 0x01,
        DOM_KEY_LOCATION_RIGHT      = 0x02,
        DOM_KEY_LOCATION_NUMPAD     = 0x03
    };

    static PassRefPtr<KeyboardEvent> create()
    {
        return adoptRef(new KeyboardEvent);
    }

    static PassRefPtr<KeyboardEvent> create(const PlatformKeyboardEvent& platformEvent, AbstractView* view)
    {
        return adoptRef(new KeyboardEvent(platformEvent, view));
    }

    static PassRefPtr<KeyboardEvent> create(const AtomicString& type, const KeyboardEventInit& initializer)
    {
        return adoptRef(new KeyboardEvent(type, initializer));
    }

    static PassRefPtr<KeyboardEvent> create(const AtomicString& type, bool canBubble, bool cancelable, AbstractView* view,
        const String& keyIdentifier, unsigned location,
        bool ctrlKey, bool altKey, bool shiftKey, bool metaKey, bool altGraphKey)
    {
        return adoptRef(new KeyboardEvent(type, canBubble, cancelable, view, keyIdentifier, location,
        ctrlKey, altKey, shiftKey, metaKey, altGraphKey));
    }

    virtual ~KeyboardEvent();

    void initKeyboardEvent(const AtomicString& type, bool canBubble, bool cancelable, AbstractView*,
        const String& keyIdentifier, unsigned location,
        bool ctrlKey, bool altKey, bool shiftKey, bool metaKey, bool altGraphKey = false);

    const String& keyIdentifier() const { return m_keyIdentifier; }
    unsigned location() const { return m_location; }

    bool getModifierState(const String& keyIdentifier) const;

    bool altGraphKey() const { return m_altGraphKey; }

    const PlatformKeyboardEvent* keyEvent() const { return m_keyEvent.get(); }

    virtual int keyCode() const OVERRIDE; // key code for keydown and keyup, character for keypress
    virtual int charCode() const OVERRIDE; // character code for keypress, 0 for keydown and keyup
    bool repeat() const { return m_isAutoRepeat; }

    virtual const AtomicString& interfaceName() const OVERRIDE;
    virtual bool isKeyboardEvent() const OVERRIDE;
    virtual int which() const OVERRIDE;

    virtual void trace(Visitor*) OVERRIDE;

private:
    KeyboardEvent();
    KeyboardEvent(const PlatformKeyboardEvent&, AbstractView*);
    KeyboardEvent(const AtomicString&, const KeyboardEventInit&);
    KeyboardEvent(const AtomicString& type, bool canBubble, bool cancelable, AbstractView*,
        const String& keyIdentifier, unsigned location,
        bool ctrlKey, bool altKey, bool shiftKey, bool metaKey, bool altGraphKey);

    OwnPtr<PlatformKeyboardEvent> m_keyEvent;
    String m_keyIdentifier;
    unsigned m_location;
    bool m_altGraphKey : 1;
    bool m_isAutoRepeat : 1;
};

class KeyboardEventDispatchMediator : public EventDispatchMediator {
public:
    static PassRefPtr<KeyboardEventDispatchMediator> create(PassRefPtr<KeyboardEvent>);
private:
    explicit KeyboardEventDispatchMediator(PassRefPtr<KeyboardEvent>);
    virtual bool dispatchEvent(EventDispatcher*) const OVERRIDE;
};

DEFINE_EVENT_TYPE_CASTS(KeyboardEvent);

} // namespace WebCore

#endif // KeyboardEvent_h
