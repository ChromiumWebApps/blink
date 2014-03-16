/*
 * Copyright (C) 2012 Apple Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "core/html/track/InbandTextTrack.h"

#include "bindings/v8/ExceptionStatePlaceholder.h"
#include "core/html/track/vtt/VTTCue.h"
#include "platform/Logging.h"
#include "public/platform/WebInbandTextTrack.h"
#include "public/platform/WebString.h"
#include <math.h>

using blink::WebInbandTextTrack;
using blink::WebString;

namespace WebCore {

PassRefPtr<InbandTextTrack> InbandTextTrack::create(Document& document, WebInbandTextTrack* webTrack)
{
    return adoptRef(new InbandTextTrack(document, webTrack));
}

InbandTextTrack::InbandTextTrack(Document& document, WebInbandTextTrack* webTrack)
    : TextTrack(document, emptyAtom, webTrack->label(), webTrack->language(), webTrack->id(), InBand)
    , m_webTrack(webTrack)
{
    m_webTrack->setClient(this);

    switch (m_webTrack->kind()) {
    case WebInbandTextTrack::KindSubtitles:
        setKind(TextTrack::subtitlesKeyword());
        break;
    case WebInbandTextTrack::KindCaptions:
        setKind(TextTrack::captionsKeyword());
        break;
    case WebInbandTextTrack::KindDescriptions:
        setKind(TextTrack::descriptionsKeyword());
        break;
    case WebInbandTextTrack::KindChapters:
        setKind(TextTrack::chaptersKeyword());
        break;
    case WebInbandTextTrack::KindMetadata:
        setKind(TextTrack::metadataKeyword());
        break;
    case WebInbandTextTrack::KindNone:
    default:
        ASSERT_NOT_REACHED();
        break;
    }
}

InbandTextTrack::~InbandTextTrack()
{
    // Make sure m_webTrack was cleared by trackRemoved() before destruction.
    ASSERT(!m_webTrack);
}

size_t InbandTextTrack::inbandTrackIndex()
{
    ASSERT(m_webTrack);
    return m_webTrack->textTrackIndex();
}

void InbandTextTrack::setTrackList(TextTrackList* trackList)
{
    TextTrack::setTrackList(trackList);
    if (trackList)
        return;

    ASSERT(m_webTrack);
    m_webTrack->setClient(0);
    m_webTrack = 0;
}

void InbandTextTrack::addWebVTTCue(double start, double end, const WebString& id, const WebString& content, const WebString& settings)
{
    RefPtr<VTTCue> cue = VTTCue::create(document(), start, end, content);
    cue->setId(id);
    cue->parseSettings(settings);
    addCue(cue);
}

} // namespace WebCore
