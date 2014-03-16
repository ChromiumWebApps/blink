// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/animation/InterpolationEffect.h"

#include <gtest/gtest.h>

namespace WebCore {

class AnimationInterpolationEffectTest : public ::testing::Test {
protected:
    InterpolableValue* interpolationValue(Interpolation& interpolation)
    {
        return interpolation.getCachedValueForTesting();
    }

    double getInterpolableNumber(PassRefPtr<Interpolation> value)
    {
        return toInterpolableNumber(interpolationValue(*value.get()))->value();
    }
};

TEST_F(AnimationInterpolationEffectTest, SingleInterpolation)
{
    RefPtr<InterpolationEffect> interpolationEffect = InterpolationEffect::create();
    interpolationEffect->addInterpolation(Interpolation::create(InterpolableNumber::create(0), InterpolableNumber::create(10)),
        RefPtr<TimingFunction>(), 0, 1, -1, 2);

    OwnPtr<Vector<RefPtr<Interpolation> > > activeInterpolations = interpolationEffect->getActiveInterpolations(-2);
    EXPECT_EQ(0ul, activeInterpolations->size());

    activeInterpolations = interpolationEffect->getActiveInterpolations(-0.5);
    EXPECT_EQ(1ul, activeInterpolations->size());
    EXPECT_EQ(-5, getInterpolableNumber(activeInterpolations->at(0)));

    activeInterpolations = interpolationEffect->getActiveInterpolations(0.5);
    EXPECT_EQ(1ul, activeInterpolations->size());
    EXPECT_FLOAT_EQ(5, getInterpolableNumber(activeInterpolations->at(0)));

    activeInterpolations = interpolationEffect->getActiveInterpolations(1.5);
    EXPECT_EQ(1ul, activeInterpolations->size());
    EXPECT_FLOAT_EQ(15, getInterpolableNumber(activeInterpolations->at(0)));

    activeInterpolations = interpolationEffect->getActiveInterpolations(3);
    EXPECT_EQ(0ul, activeInterpolations->size());
}

TEST_F(AnimationInterpolationEffectTest, MultipleInterpolations)
{
    RefPtr<InterpolationEffect> interpolationEffect = InterpolationEffect::create();
    interpolationEffect->addInterpolation(Interpolation::create(InterpolableNumber::create(10), InterpolableNumber::create(15)),
        RefPtr<TimingFunction>(), 1, 2, 1, 3);
    interpolationEffect->addInterpolation(Interpolation::create(InterpolableNumber::create(0), InterpolableNumber::create(1)),
        LinearTimingFunction::preset(), 0, 1, 0, 1);
    interpolationEffect->addInterpolation(Interpolation::create(InterpolableNumber::create(1), InterpolableNumber::create(6)),
        CubicBezierTimingFunction::preset(CubicBezierTimingFunction::Ease), 0.5, 1.5, 0.5, 1.5);

    OwnPtr<Vector<RefPtr<Interpolation> > > activeInterpolations = interpolationEffect->getActiveInterpolations(-0.5);
    EXPECT_EQ(0ul, activeInterpolations->size());

    activeInterpolations = interpolationEffect->getActiveInterpolations(0);
    EXPECT_EQ(1ul, activeInterpolations->size());
    EXPECT_FLOAT_EQ(0, getInterpolableNumber(activeInterpolations->at(0)));

    activeInterpolations = interpolationEffect->getActiveInterpolations(0.5);
    EXPECT_EQ(2ul, activeInterpolations->size());
    EXPECT_FLOAT_EQ(0.5f, getInterpolableNumber(activeInterpolations->at(0)));
    EXPECT_FLOAT_EQ(1, getInterpolableNumber(activeInterpolations->at(1)));

    activeInterpolations = interpolationEffect->getActiveInterpolations(1);
    EXPECT_EQ(2ul, activeInterpolations->size());
    EXPECT_FLOAT_EQ(10, getInterpolableNumber(activeInterpolations->at(0)));
    EXPECT_FLOAT_EQ(5.0120168f, getInterpolableNumber(activeInterpolations->at(1)));

    activeInterpolations = interpolationEffect->getActiveInterpolations(1.5);
    EXPECT_EQ(1ul, activeInterpolations->size());
    EXPECT_FLOAT_EQ(12.5f, getInterpolableNumber(activeInterpolations->at(0)));

    activeInterpolations = interpolationEffect->getActiveInterpolations(2);
    EXPECT_EQ(1ul, activeInterpolations->size());
    EXPECT_FLOAT_EQ(15, getInterpolableNumber(activeInterpolations->at(0)));
}

}

