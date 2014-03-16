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

#include "config.h"
#include "core/animation/DocumentTimeline.h"

#include "core/animation/ActiveAnimations.h"
#include "core/animation/AnimationClock.h"
#include "core/dom/Document.h"
#include "core/frame/FrameView.h"
#include "core/page/Page.h"
#include "platform/TraceEvent.h"

namespace WebCore {

// This value represents 1 frame at 30Hz plus a little bit of wiggle room.
// TODO: Plumb a nominal framerate through and derive this value from that.
const double DocumentTimeline::s_minimumDelay = 0.04;


PassRefPtr<DocumentTimeline> DocumentTimeline::create(Document* document, PassOwnPtr<PlatformTiming> timing)
{
    return adoptRef(new DocumentTimeline(document, timing));
}

DocumentTimeline::DocumentTimeline(Document* document, PassOwnPtr<PlatformTiming> timing)
    : m_zeroTime(nullValue())
    , m_document(document)
{
    if (!timing)
        m_timing = adoptPtr(new DocumentTimelineTiming(this));
    else
        m_timing = timing;

    ASSERT(document);
}

DocumentTimeline::~DocumentTimeline()
{
    for (HashSet<Player*>::iterator it = m_players.begin(); it != m_players.end(); ++it)
        (*it)->timelineDestroyed();
}

Player* DocumentTimeline::createPlayer(TimedItem* child)
{
    RefPtr<Player> player = Player::create(*this, child);
    Player* result = player.get();
    m_players.add(result);
    setOutdatedPlayer(result);
    return result;
}

Player* DocumentTimeline::play(TimedItem* child)
{
    Player* player = createPlayer(child);
    player->setStartTime(currentTime());
    return player;
}

void DocumentTimeline::wake()
{
    m_timing->serviceOnNextFrame();
}

void DocumentTimeline::serviceAnimations()
{
    TRACE_EVENT0("webkit", "DocumentTimeline::serviceAnimations");

    m_timing->cancelWake();
    m_hasOutdatedPlayer = false;

    double timeToNextEffect = std::numeric_limits<double>::infinity();
    Vector<Player*> players;
    for (HashSet<RefPtr<Player> >::iterator it = m_playersNeedingUpdate.begin(); it != m_playersNeedingUpdate.end(); ++it)
        players.append(it->get());

    std::sort(players.begin(), players.end(), Player::hasLowerPriority);

    for (size_t i = 0; i < players.size(); ++i) {
        Player* player = players[i];
        if (player->update())
            timeToNextEffect = std::min(timeToNextEffect, player->timeToEffectChange());
        else
            m_playersNeedingUpdate.remove(player);
    }

    ASSERT(!m_playersNeedingUpdate.isEmpty() || timeToNextEffect == std::numeric_limits<double>::infinity());
    if (timeToNextEffect < s_minimumDelay)
        m_timing->serviceOnNextFrame();
    else if (timeToNextEffect != std::numeric_limits<double>::infinity())
        m_timing->wakeAfter(timeToNextEffect - s_minimumDelay);

    ASSERT(!m_hasOutdatedPlayer);
}

void DocumentTimeline::setZeroTime(double zeroTime)
{
    ASSERT(isNull(m_zeroTime));
    m_zeroTime = zeroTime;
    ASSERT(!isNull(m_zeroTime));
    serviceAnimations();
}

void DocumentTimeline::DocumentTimelineTiming::wakeAfter(double duration)
{
    m_timer.startOneShot(duration, FROM_HERE);
}

void DocumentTimeline::DocumentTimelineTiming::cancelWake()
{
    m_timer.stop();
}

void DocumentTimeline::DocumentTimelineTiming::serviceOnNextFrame()
{
    if (m_timeline->m_document && m_timeline->m_document->view())
        m_timeline->m_document->view()->scheduleAnimation();
}

double DocumentTimeline::currentTime()
{
    if (!m_document)
        return std::numeric_limits<double>::quiet_NaN();
    return m_document->animationClock().currentTime() - m_zeroTime;
}

void DocumentTimeline::pauseAnimationsForTesting(double pauseTime)
{
    for (HashSet<RefPtr<Player> >::iterator it = m_playersNeedingUpdate.begin(); it != m_playersNeedingUpdate.end(); ++it)
        (*it)->pauseForTesting(pauseTime);
    serviceAnimations();
}

void DocumentTimeline::setOutdatedPlayer(Player* player)
{
    m_playersNeedingUpdate.add(player);
    m_hasOutdatedPlayer = true;
    if (m_document && m_document->page() && !m_document->page()->animator().isServicingAnimations())
        m_timing->serviceOnNextFrame();
}

size_t DocumentTimeline::numberOfActiveAnimationsForTesting() const
{
    if (isNull(m_zeroTime))
        return 0;
    // Includes all players whose directly associated timed items
    // are current or in effect.
    if (isNull(m_zeroTime))
        return 0;
    size_t count = 0;
    for (HashSet<RefPtr<Player> >::iterator it = m_playersNeedingUpdate.begin(); it != m_playersNeedingUpdate.end(); ++it) {
        const TimedItem* timedItem = (*it)->source();
        if ((*it)->hasStartTime())
            count += (timedItem && (timedItem->isCurrent() || timedItem->isInEffect()));
    }
    return count;
}

void DocumentTimeline::detachFromDocument() {
    m_document = 0;
}

} // namespace
