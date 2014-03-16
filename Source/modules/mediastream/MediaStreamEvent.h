/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

#ifndef MediaStreamEvent_h
#define MediaStreamEvent_h

#include "core/events/Event.h"
#include "modules/mediastream/MediaStream.h"
#include "wtf/text/AtomicString.h"

namespace WebCore {

struct MediaStreamEventInit : public EventInit {
    MediaStreamEventInit();

    RefPtr<MediaStream> stream;
};

class MediaStreamEvent FINAL : public Event {
public:
    virtual ~MediaStreamEvent();

    static PassRefPtr<MediaStreamEvent> create();
    static PassRefPtr<MediaStreamEvent> create(const AtomicString& type, bool canBubble, bool cancelable, PassRefPtr<MediaStream>);
    static PassRefPtr<MediaStreamEvent> create(const AtomicString& type, const MediaStreamEventInit& initializer);

    MediaStream* stream() const;
    MediaStream* stream(bool&) const;

    virtual const AtomicString& interfaceName() const OVERRIDE;

    virtual void trace(Visitor*) OVERRIDE;

private:
    MediaStreamEvent();
    MediaStreamEvent(const AtomicString& type, bool canBubble, bool cancelable, PassRefPtr<MediaStream>);
    MediaStreamEvent(const AtomicString& type, const MediaStreamEventInit&);

    RefPtr<MediaStream> m_stream;
};

} // namespace WebCore

#endif // MediaStreamEvent_h
