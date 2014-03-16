// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef InterpolationEffect_h
#define InterpolationEffect_h

#include "core/animation/Interpolation.h"
#include "platform/animation/TimingFunction.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/RefCounted.h"


namespace WebCore {

class InterpolationEffect : public RefCounted<InterpolationEffect> {

public:
    static PassRefPtr<InterpolationEffect> create() { return adoptRef(new InterpolationEffect()); }

    PassOwnPtr<Vector<RefPtr<Interpolation> > > getActiveInterpolations(double fraction) const;

    void addInterpolation(PassRefPtr<Interpolation> interpolation, PassRefPtr<TimingFunction> easing, double start, double end, double applyFrom, double applyTo)
    {
        m_interpolations.append(InterpolationRecord::create(interpolation, easing, start, end, applyFrom, applyTo));
    }

private:
    InterpolationEffect()
    { }

    class InterpolationRecord {
    public:
        RefPtr<Interpolation> m_interpolation;
        RefPtr<TimingFunction> m_easing;
        double m_start;
        double m_end;
        double m_applyFrom;
        double m_applyTo;
        static PassOwnPtr<InterpolationRecord> create(PassRefPtr<Interpolation> interpolation, PassRefPtr<TimingFunction> easing, double start, double end, double applyFrom, double applyTo)
        {
            return adoptPtr(new InterpolationRecord(interpolation, easing, start, end, applyFrom, applyTo));
        }
    private:
        InterpolationRecord(PassRefPtr<Interpolation> interpolation, PassRefPtr<TimingFunction> easing, double start, double end, double applyFrom, double applyTo)
            : m_interpolation(interpolation)
            , m_easing(easing)
            , m_start(start)
            , m_end(end)
            , m_applyFrom(applyFrom)
            , m_applyTo(applyTo)
        { }
    };

    Vector<OwnPtr<InterpolationRecord> > m_interpolations;
};

}

#endif
