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

#ifndef AnimationClock_h
#define AnimationClock_h

#include "wtf/CurrentTime.h"
#include "wtf/PassOwnPtr.h"

namespace WebCore {

// FIXME: This value is used to suppress updates when time is required outside of a frame.
// The purpose of allowing the clock to update during such periods is to allow animations
// to have an appropriate start time and for getComputedStyle to attempt to catch-up to a
// compositor animation. However a more accurate system might be to attempt to phase-lock
// with the frame clock.
const double minTimeBeforeUnsynchronizedAnimationClockTick = 0.005;

class AnimationClock {
public:
    static PassOwnPtr<AnimationClock> create(WTF::TimeFunction monotonicallyIncreasingTime = WTF::monotonicallyIncreasingTime)
    {
        return adoptPtr(new AnimationClock(monotonicallyIncreasingTime));
    }

    void updateTime(double time);
    double currentTime();
    void unfreeze() { m_frozen = false; }
    void resetTimeForTesting() { m_time = 0; m_frozen = true; }

private:
    AnimationClock(WTF::TimeFunction monotonicallyIncreasingTime)
        : m_monotonicallyIncreasingTime(monotonicallyIncreasingTime)
        , m_time(0)
        , m_frozen(false)
    {
    }
    WTF::TimeFunction m_monotonicallyIncreasingTime;
    double m_time;
    bool m_frozen;
};

} // namespace WebCore

#endif // AnimationClock_h
