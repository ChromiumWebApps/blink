/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 * Copyright (C) 2011 Ericsson AB. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef MediaStreamTrack_h
#define MediaStreamTrack_h

#include "bindings/v8/ScriptWrappable.h"
#include "core/dom/ActiveDOMObject.h"
#include "core/events/EventTarget.h"
#include "modules/mediastream/SourceInfo.h"
#include "platform/mediastream/MediaStreamDescriptor.h"
#include "platform/mediastream/MediaStreamSource.h"
#include "wtf/RefCounted.h"
#include "wtf/RefPtr.h"
#include "wtf/Vector.h"
#include "wtf/text/WTFString.h"

namespace WebCore {

class ExceptionState;
class MediaStreamComponent;
class MediaStreamTrackSourcesCallback;

class MediaStreamTrack FINAL : public RefCounted<MediaStreamTrack>, public ScriptWrappable, public ActiveDOMObject, public EventTargetWithInlineData, public MediaStreamSource::Observer {
    REFCOUNTED_EVENT_TARGET(MediaStreamTrack);
public:
    class Observer {
    public:
        virtual ~Observer() { }
        virtual void trackEnded() = 0;
    };

    static PassRefPtr<MediaStreamTrack> create(ExecutionContext*, MediaStreamComponent*);
    virtual ~MediaStreamTrack();

    String kind() const;
    String id() const;
    String label() const;

    bool enabled() const;
    void setEnabled(bool);

    String readyState() const;

    static void getSources(ExecutionContext*, PassOwnPtr<MediaStreamTrackSourcesCallback>, ExceptionState&);
    void stopTrack(ExceptionState&);
    PassRefPtr<MediaStreamTrack> clone(ExecutionContext*);

    DEFINE_ATTRIBUTE_EVENT_LISTENER(mute);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(unmute);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(ended);

    MediaStreamComponent* component();
    bool ended() const;

    void addObserver(Observer*);
    void removeObserver(Observer*);

    // EventTarget
    virtual const AtomicString& interfaceName() const OVERRIDE;
    virtual ExecutionContext* executionContext() const OVERRIDE;

    // ActiveDOMObject
    virtual void stop() OVERRIDE;

private:
    MediaStreamTrack(ExecutionContext*, MediaStreamComponent*);

    // MediaStreamSourceObserver
    virtual void sourceChangedState() OVERRIDE;

    void propagateTrackEnded();
    Vector<Observer*> m_observers;
    bool m_isIteratingObservers;

    bool m_stopped;
    RefPtr<MediaStreamComponent> m_component;
};

typedef Vector<RefPtr<MediaStreamTrack> > MediaStreamTrackVector;

} // namespace WebCore

#endif // MediaStreamTrack_h
