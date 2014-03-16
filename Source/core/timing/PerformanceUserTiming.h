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

#ifndef PerformanceUserTiming_h
#define PerformanceUserTiming_h

#include "core/timing/Performance.h"
#include "core/timing/PerformanceTiming.h"
#include "heap/Handle.h"
#include "wtf/HashMap.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefCounted.h"
#include "wtf/text/StringHash.h"
#include "wtf/text/WTFString.h"

namespace WebCore {

class ExceptionState;
class Performance;
class PerformanceEntry;

typedef unsigned long long (PerformanceTiming::*NavigationTimingFunction)() const;
typedef WillBeHeapHashMap<String, PerformanceEntryVector> PerformanceEntryMap;

class UserTiming : public RefCountedWillBeGarbageCollected<UserTiming> {
public:
    static PassRefPtrWillBeRawPtr<UserTiming> create(Performance* performance)
    {
        return adoptRefWillBeNoop(new UserTiming(performance));
    }

    void mark(const String& markName, ExceptionState&);
    void clearMarks(const String& markName);

    void measure(const String& measureName, const String& startMark, const String& endMark, ExceptionState&);
    void clearMeasures(const String& measureName);

    PerformanceEntryVector getMarks() const;
    PerformanceEntryVector getMeasures() const;

    PerformanceEntryVector getMarks(const String& name) const;
    PerformanceEntryVector getMeasures(const String& name) const;

    void trace(Visitor*);

private:
    explicit UserTiming(Performance*);

    double findExistingMarkStartTime(const String& markName, ExceptionState&);
    RawPtrWillBeMember<Performance> m_performance;
    PerformanceEntryMap m_marksMap;
    PerformanceEntryMap m_measuresMap;
};

}

#endif // !defined(PerformanceUserTiming_h)
