/*
 *  Copyright (C) 2012 Samsung Electronics
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "modules/vibration/NavigatorVibration.h"

#include "core/frame/LocalFrame.h"
#include "core/frame/Navigator.h"
#include "core/page/PageVisibilityState.h"
#include "public/platform/Platform.h"
#include "public/platform/WebVibration.h"

namespace WebCore {

// Maximum number of entries in a vibration pattern.
const unsigned kVibrationPatternLengthMax = 99;

NavigatorVibration::NavigatorVibration(Page& page)
    : PageLifecycleObserver(&page)
    , m_timerStart(this, &NavigatorVibration::timerStartFired)
    , m_timerStop(this, &NavigatorVibration::timerStopFired)
    , m_isVibrating(false)
{
}

NavigatorVibration::~NavigatorVibration()
{
    if (m_isVibrating)
        cancelVibration();
}

bool NavigatorVibration::vibrate(const VibrationPattern& pattern)
{
    VibrationPattern sanitized = pattern;
    size_t length = sanitized.size();

    // If the pattern is too long then truncate it.
    if (length > kVibrationPatternLengthMax) {
        sanitized.shrink(kVibrationPatternLengthMax);
        length = kVibrationPatternLengthMax;
    }

    // If any pattern entry is too long then truncate it.
    for (size_t i = 0; i < length; ++i) {
        if (sanitized[i] > blink::kVibrationDurationMax)
            sanitized[i] = blink::kVibrationDurationMax;
    }

    // If the last item in the pattern is a pause then discard it.
    if (length && !(length % 2))
        sanitized.removeLast();

    // Cancelling clears the stored pattern so do it before setting the new one.
    if (m_isVibrating)
        cancelVibration();

    m_pattern = sanitized;

    if (m_timerStart.isActive())
        m_timerStart.stop();

    if (!m_pattern.size())
        return true;

    if (m_pattern.size() == 1 && !m_pattern[0]) {
        m_pattern.clear();
        return true;
    }

    m_timerStart.startOneShot(0, FROM_HERE);
    m_isVibrating = true;
    return true;
}

void NavigatorVibration::cancelVibration()
{
    m_pattern.clear();
    if (m_isVibrating) {
        blink::Platform::current()->cancelVibration();
        m_isVibrating = false;
        m_timerStop.stop();
    }
}

void NavigatorVibration::timerStartFired(Timer<NavigatorVibration>* timer)
{
    ASSERT_UNUSED(timer, timer == &m_timerStart);

    if (m_pattern.size()) {
        m_isVibrating = true;
        blink::Platform::current()->vibrate(m_pattern[0]);
        m_timerStop.startOneShot(m_pattern[0] / 1000.0, FROM_HERE);
        m_pattern.remove(0);
    }
}

void NavigatorVibration::timerStopFired(Timer<NavigatorVibration>* timer)
{
    ASSERT_UNUSED(timer, timer == &m_timerStop);

    if (m_pattern.isEmpty())
        m_isVibrating = false;

    if (m_pattern.size()) {
        m_timerStart.startOneShot(m_pattern[0] / 1000.0, FROM_HERE);
        m_pattern.remove(0);
    }
}

void NavigatorVibration::pageVisibilityChanged()
{
    if (page()->visibilityState() != PageVisibilityStateVisible)
        cancelVibration();
}

void NavigatorVibration::didCommitLoad(LocalFrame* frame)
{
    // A new load has been committed, which means the current page will be
    // unloaded. Cancel all running vibrations.
    cancelVibration();
}

bool NavigatorVibration::vibrate(Navigator& navigator, unsigned time)
{
    VibrationPattern pattern;
    pattern.append(time);
    return NavigatorVibration::vibrate(navigator, pattern);
}

bool NavigatorVibration::vibrate(Navigator& navigator, const VibrationPattern& pattern)
{
    Page* page = navigator.frame()->page();
    if (!page)
        return false;

    if (page->visibilityState() != PageVisibilityStateVisible)
        return false;

    return NavigatorVibration::from(*page).vibrate(pattern);
}

NavigatorVibration& NavigatorVibration::from(Page& page)
{
    NavigatorVibration* navigatorVibration = static_cast<NavigatorVibration*>(Supplement<Page>::from(page, supplementName()));
    if (!navigatorVibration) {
        navigatorVibration = new NavigatorVibration(page);
        Supplement<Page>::provideTo(page, supplementName(), adoptPtr(navigatorVibration));
    }
    return *navigatorVibration;
}

const char* NavigatorVibration::supplementName()
{
    return "NavigatorVibration";
}

} // namespace WebCore
