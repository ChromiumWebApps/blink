/*
 * Copyright (C) 1998, 1999 Torben Weis <weis@kde.org>
 *                     1999-2001 Lars Knoll <knoll@kde.org>
 *                     1999-2001 Antti Koivisto <koivisto@kde.org>
 *                     2000-2001 Simon Hausmann <hausmann@kde.org>
 *                     2000-2001 Dirk Mueller <mueller@kde.org>
 *                     2000 Stefan Schimanski <1Stein@gmx.de>
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2008 Eric Seidel <eric@webkit.org>
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
 */

#ifndef Frame_h
#define Frame_h

#include "wtf/Forward.h"
#include "wtf/HashSet.h"
#include "wtf/RefCounted.h"

namespace blink {
class WebLayer;
}

namespace WebCore {

class Document;
class DOMWindow;
class ChromeClient;
class FrameDestructionObserver;
class FrameHost;
class HTMLFrameOwnerElement;
class Page;
class RenderView;
class Settings;

class Frame : public RefCounted<Frame> {
public:
    virtual bool isLocalFrame() const { return false; }
    virtual bool isRemoteFrame() const { return false; }

    virtual ~Frame();

    void addDestructionObserver(FrameDestructionObserver*);
    void removeDestructionObserver(FrameDestructionObserver*);

    virtual void willDetachFrameHost();
    virtual void detachFromFrameHost();

    // NOTE: Page is moving out of Blink up into the browser process as
    // part of the site-isolation (out of process iframes) work.
    // FrameHost should be used instead where possible.
    Page* page() const;
    FrameHost* host() const; // Null when the frame is detached.

    bool isMainFrame() const;

    // FIXME: DOMWindow and Document should both be moved to LocalFrame
    // after RemoteFrame is complete enough to exist without them.
    virtual void setDOMWindow(PassRefPtr<DOMWindow>);
    DOMWindow* domWindow() const;
    Document* document() const;

    ChromeClient& chromeClient() const;

    RenderView* contentRenderer() const; // Root of the render tree for the document contained in this frame.

    int64_t frameID() const { return m_frameID; }

    // FIXME: These should move to RemoteFrame when that is instantiated.
    void setRemotePlatformLayer(blink::WebLayer* remotePlatformLayer) { m_remotePlatformLayer = remotePlatformLayer; }
    blink::WebLayer* remotePlatformLayer() const { return m_remotePlatformLayer; }

    Settings* settings() const; // can be null

protected:
    Frame(FrameHost*, HTMLFrameOwnerElement*);

    FrameHost* m_host;
    HTMLFrameOwnerElement* m_ownerElement;

    RefPtr<DOMWindow> m_domWindow;

private:

    HashSet<FrameDestructionObserver*> m_destructionObservers;

    // Temporary hack for history.
    int64_t m_frameID;

    blink::WebLayer* m_remotePlatformLayer;
};

inline DOMWindow* Frame::domWindow() const
{
    return m_domWindow.get();
}
} // namespace WebCore

#endif // Frame_h
