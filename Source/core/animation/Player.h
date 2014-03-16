/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef Player_h
#define Player_h

#include "core/animation/TimedItem.h"
#include "wtf/RefPtr.h"

namespace WebCore {

class DocumentTimeline;
class ExceptionState;

class Player FINAL : public RefCounted<Player> {

public:
    ~Player();
    static PassRefPtr<Player> create(DocumentTimeline&, TimedItem*);

    // Returns whether this player is still current or in effect.
    bool update();

    // timeToEffectChange returns:
    //  infinity  - if this player is no longer in effect
    //  0         - if this player requires an update on the next frame
    //  n         - if this player requires an update after 'n' units of time
    double timeToEffectChange();

    void cancel();

    double currentTime();
    void setCurrentTime(double newCurrentTime);

    bool paused() const { return m_paused && !m_isPausedForTesting; }
    void pause();
    void play();
    void reverse();
    void finish(ExceptionState&);
    bool finished() { return limited(currentTime()); }

    double playbackRate() const { return m_playbackRate; }
    void setPlaybackRate(double);
    const DocumentTimeline* timeline() const { return m_timeline; }
    DocumentTimeline* timeline() { return m_timeline; }

    void timelineDestroyed() { m_timeline = 0; }

    bool hasStartTime() const { return !isNull(m_startTime); }
    double startTime() const { return m_startTime; }
    void setStartTime(double);

    TimedItem* source() { return m_content.get(); }
    TimedItem* source(bool& isNull) { isNull = !m_content; return m_content.get(); }
    void setSource(TimedItem*);

    double timeLag() { return currentTimeWithoutLag() - currentTime(); }

    // Pausing via this method is not reflected in the value returned by
    // paused() and must never overlap with pausing via pause().
    void pauseForTesting(double pauseTime);
    // This should only be used for CSS
    void unpause();

    void setOutdated();
    bool outdated() { return m_outdated; }

    bool maybeStartAnimationOnCompositor();
    void cancelAnimationOnCompositor();
    bool hasActiveAnimationsOnCompositor();

    static bool hasLowerPriority(Player*, Player*);

private:
    Player(DocumentTimeline&, TimedItem*);
    double sourceEnd() const;
    bool limited(double currentTime) const;
    double currentTimeWithoutLag() const;
    double currentTimeWithLag() const;
    void updateTimingState(double newCurrentTime);
    void updateCurrentTimingState();

    double m_playbackRate;
    double m_startTime;
    double m_holdTime;
    double m_storedTimeLag;

    RefPtr<TimedItem> m_content;
    // FIXME: We should keep the timeline alive and have this as non-null
    // but this is tricky to do without Oilpan
    DocumentTimeline* m_timeline;
    // Reflects all pausing, including via pauseForTesting().
    bool m_paused;
    bool m_held;
    bool m_isPausedForTesting;

    // This indicates timing information relevant to the player has changed by
    // means other than the ordinary progression of time
    bool m_outdated;

    unsigned m_sequenceNumber;
};

} // namespace

#endif
