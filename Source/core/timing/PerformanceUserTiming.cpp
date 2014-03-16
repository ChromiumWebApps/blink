/*
 * Copyright (C) 2012 Intel Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "core/timing/PerformanceUserTiming.h"

#include "bindings/v8/ExceptionState.h"
#include "core/dom/ExceptionCode.h"
#include "core/timing/Performance.h"
#include "core/timing/PerformanceMark.h"
#include "core/timing/PerformanceMeasure.h"
#include "public/platform/Platform.h"

namespace WebCore {

namespace {

typedef HashMap<String, NavigationTimingFunction> RestrictedKeyMap;
static RestrictedKeyMap restrictedKeyMap()
{
    DEFINE_STATIC_LOCAL(RestrictedKeyMap, map, ());
    if (map.isEmpty()) {
        map.add("navigationStart", &PerformanceTiming::navigationStart);
        map.add("unloadEventStart", &PerformanceTiming::unloadEventStart);
        map.add("unloadEventEnd", &PerformanceTiming::unloadEventEnd);
        map.add("redirectStart", &PerformanceTiming::redirectStart);
        map.add("redirectEnd", &PerformanceTiming::redirectEnd);
        map.add("fetchStart", &PerformanceTiming::fetchStart);
        map.add("domainLookupStart", &PerformanceTiming::domainLookupStart);
        map.add("domainLookupEnd", &PerformanceTiming::domainLookupEnd);
        map.add("connectStart", &PerformanceTiming::connectStart);
        map.add("connectEnd", &PerformanceTiming::connectEnd);
        map.add("secureConnectionStart", &PerformanceTiming::secureConnectionStart);
        map.add("requestStart", &PerformanceTiming::requestStart);
        map.add("responseStart", &PerformanceTiming::responseStart);
        map.add("responseEnd", &PerformanceTiming::responseEnd);
        map.add("domLoading", &PerformanceTiming::domLoading);
        map.add("domInteractive", &PerformanceTiming::domInteractive);
        map.add("domContentLoadedEventStart", &PerformanceTiming::domContentLoadedEventStart);
        map.add("domContentLoadedEventEnd", &PerformanceTiming::domContentLoadedEventEnd);
        map.add("domComplete", &PerformanceTiming::domComplete);
        map.add("loadEventStart", &PerformanceTiming::loadEventStart);
        map.add("loadEventEnd", &PerformanceTiming::loadEventEnd);
    }
    return map;
}

} // namespace anonymous

UserTiming::UserTiming(Performance* performance)
    : m_performance(performance)
{
}

static void insertPerformanceEntry(PerformanceEntryMap& performanceEntryMap, PassRefPtrWillBeRawPtr<PerformanceEntry> performanceEntry)
{
    RefPtrWillBeRawPtr<PerformanceEntry> entry = performanceEntry;
    PerformanceEntryMap::iterator it = performanceEntryMap.find(entry->name());
    if (it != performanceEntryMap.end())
        it->value.append(entry);
    else {
        PerformanceEntryVector vector(1);
        vector[0] = entry;
        performanceEntryMap.set(entry->name(), vector);
    }
}

static void clearPeformanceEntries(PerformanceEntryMap& performanceEntryMap, const String& name)
{
    if (name.isNull()) {
        performanceEntryMap.clear();
        return;
    }

    if (performanceEntryMap.contains(name))
        performanceEntryMap.remove(name);
}

void UserTiming::mark(const String& markName, ExceptionState& exceptionState)
{
    if (restrictedKeyMap().contains(markName)) {
        exceptionState.throwDOMException(SyntaxError, "'" + markName + "' is part of the PerformanceTiming interface, and cannot be used as a mark name.");
        return;
    }

    double startTime = m_performance->now();
    insertPerformanceEntry(m_marksMap, PerformanceMark::create(markName, startTime));
    blink::Platform::current()->histogramCustomCounts("PLT.UserTiming_Mark", static_cast<int>(startTime), 0, 600000, 100);
}

void UserTiming::clearMarks(const String& markName)
{
    clearPeformanceEntries(m_marksMap, markName);
}

double UserTiming::findExistingMarkStartTime(const String& markName, ExceptionState& exceptionState)
{
    if (m_marksMap.contains(markName))
        return m_marksMap.get(markName).last()->startTime();

    if (restrictedKeyMap().contains(markName)) {
        double value = static_cast<double>((m_performance->timing()->*(restrictedKeyMap().get(markName)))());
        if (!value) {
            exceptionState.throwDOMException(InvalidAccessError, "'" + markName + "' is empty: either the event hasn't happened yet, or it would provide cross-origin timing information.");
            return 0.0;
        }
        return value - m_performance->timing()->navigationStart();
    }

    exceptionState.throwDOMException(SyntaxError, "The mark '" + markName + "' does not exist.");
    return 0.0;
}

void UserTiming::measure(const String& measureName, const String& startMark, const String& endMark, ExceptionState& exceptionState)
{
    double startTime = 0.0;
    double endTime = 0.0;

    if (startMark.isNull())
        endTime = m_performance->now();
    else if (endMark.isNull()) {
        endTime = m_performance->now();
        startTime = findExistingMarkStartTime(startMark, exceptionState);
        if (exceptionState.hadException())
            return;
    } else {
        endTime = findExistingMarkStartTime(endMark, exceptionState);
        if (exceptionState.hadException())
            return;
        startTime = findExistingMarkStartTime(startMark, exceptionState);
        if (exceptionState.hadException())
            return;
    }

    insertPerformanceEntry(m_measuresMap, PerformanceMeasure::create(measureName, startTime, endTime));
    if (endTime >= startTime)
        blink::Platform::current()->histogramCustomCounts("PLT.UserTiming_MeasureDuration", static_cast<int>(endTime - startTime), 0, 600000, 100);
}

void UserTiming::clearMeasures(const String& measureName)
{
    clearPeformanceEntries(m_measuresMap, measureName);
}

static PerformanceEntryVector convertToEntrySequence(const PerformanceEntryMap& performanceEntryMap)
{
    PerformanceEntryVector entries;

    for (PerformanceEntryMap::const_iterator it = performanceEntryMap.begin(); it != performanceEntryMap.end(); ++it)
        entries.appendVector(it->value);

    return entries;
}

static PerformanceEntryVector getEntrySequenceByName(const PerformanceEntryMap& performanceEntryMap, const String& name)
{
    PerformanceEntryVector entries;

    PerformanceEntryMap::const_iterator it = performanceEntryMap.find(name);
    if (it != performanceEntryMap.end())
        entries.appendVector(it->value);

    return entries;
}

PerformanceEntryVector UserTiming::getMarks() const
{
    return convertToEntrySequence(m_marksMap);
}

PerformanceEntryVector UserTiming::getMarks(const String& name) const
{
    return getEntrySequenceByName(m_marksMap, name);
}

PerformanceEntryVector UserTiming::getMeasures() const
{
    return convertToEntrySequence(m_measuresMap);
}

PerformanceEntryVector UserTiming::getMeasures(const String& name) const
{
    return getEntrySequenceByName(m_measuresMap, name);
}

void UserTiming::trace(Visitor* visitor)
{
    visitor->trace(m_performance);
    visitor->trace(m_marksMap);
    visitor->trace(m_measuresMap);
}

} // namespace WebCore
