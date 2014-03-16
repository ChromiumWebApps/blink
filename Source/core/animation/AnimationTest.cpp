// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/animation/Animation.h"

#include "bindings/v8/Dictionary.h"
#include "core/animation/AnimatableLength.h"
#include "core/animation/AnimationClock.h"
#include "core/animation/AnimationHelpers.h"
#include "core/animation/AnimationTestHelper.h"
#include "core/animation/DocumentTimeline.h"
#include "core/animation/KeyframeEffectModel.h"
#include "core/animation/Timing.h"

#include <gtest/gtest.h>

namespace WebCore {

class AnimationAnimationTest : public ::testing::Test {
protected:
    AnimationAnimationTest()
        : document(Document::create())
        , element(document->createElement("foo", ASSERT_NO_EXCEPTION))
    {
        document->animationClock().resetTimeForTesting();
        document->timeline().setZeroTime(0);
        EXPECT_EQ(0, document->timeline().currentTime());
    }

    RefPtr<Document> document;
    RefPtr<Element> element;
};

class AnimationAnimationV8Test : public AnimationAnimationTest {
protected:
    AnimationAnimationV8Test()
        : m_isolate(v8::Isolate::GetCurrent())
        , m_scope(V8ExecutionScope::create(m_isolate))
    {
    }

    template<typename T>
    static PassRefPtr<Animation> createAnimation(Element* element, Vector<Dictionary> keyframeDictionaryVector, T timingInput)
    {
        return Animation::create(element, EffectInput::convert(element, keyframeDictionaryVector, true), timingInput);
    }
    static PassRefPtr<Animation> createAnimation(Element* element, Vector<Dictionary> keyframeDictionaryVector)
    {
        return Animation::create(element, EffectInput::convert(element, keyframeDictionaryVector, true));
    }

    v8::Isolate* m_isolate;

private:
    OwnPtr<V8ExecutionScope> m_scope;
};

TEST_F(AnimationAnimationV8Test, CanCreateAnAnimation)
{
    Vector<Dictionary> jsKeyframes;
    v8::Handle<v8::Object> keyframe1 = v8::Object::New(m_isolate);
    v8::Handle<v8::Object> keyframe2 = v8::Object::New(m_isolate);

    setV8ObjectPropertyAsString(keyframe1, "width", "100px");
    setV8ObjectPropertyAsString(keyframe1, "offset", "0");
    setV8ObjectPropertyAsString(keyframe1, "easing", "ease-in-out");
    setV8ObjectPropertyAsString(keyframe2, "width", "0px");
    setV8ObjectPropertyAsString(keyframe2, "offset", "1");
    setV8ObjectPropertyAsString(keyframe2, "easing", "cubic-bezier(1, 1, 0.3, 0.3)");

    jsKeyframes.append(Dictionary(keyframe1, m_isolate));
    jsKeyframes.append(Dictionary(keyframe2, m_isolate));

    String value1;
    ASSERT_TRUE(jsKeyframes[0].get("width", value1));
    ASSERT_EQ("100px", value1);

    String value2;
    ASSERT_TRUE(jsKeyframes[1].get("width", value2));
    ASSERT_EQ("0px", value2);

    RefPtr<Animation> animation = createAnimation(element.get(), jsKeyframes, 0);

    Element* target = animation->target();
    EXPECT_EQ(*element.get(), *target);

    const KeyframeEffectModel::KeyframeVector keyframes =
        toKeyframeEffectModel(animation->effect())->getFrames();

    EXPECT_EQ(0, keyframes[0]->offset());
    EXPECT_EQ(1, keyframes[1]->offset());

    const AnimatableValue* keyframe1Width = keyframes[0]->propertyValue(CSSPropertyWidth);
    const AnimatableValue* keyframe2Width = keyframes[1]->propertyValue(CSSPropertyWidth);
    ASSERT(keyframe1Width);
    ASSERT(keyframe2Width);

    EXPECT_TRUE(keyframe1Width->isLength());
    EXPECT_TRUE(keyframe2Width->isLength());

    EXPECT_EQ("100px", toAnimatableLength(keyframe1Width)->toCSSValue()->cssText());
    EXPECT_EQ("0px", toAnimatableLength(keyframe2Width)->toCSSValue()->cssText());

    EXPECT_EQ(*(CubicBezierTimingFunction::preset(CubicBezierTimingFunction::EaseInOut)), *keyframes[0]->easing());
    EXPECT_EQ(*(CubicBezierTimingFunction::create(1, 1, 0.3, 0.3).get()), *keyframes[1]->easing());
}

TEST_F(AnimationAnimationV8Test, CanSetDuration)
{
    Vector<Dictionary, 0> jsKeyframes;
    double duration = 2;

    RefPtr<Animation> animation = createAnimation(element.get(), jsKeyframes, duration);

    EXPECT_EQ(duration, animation->specifiedTiming().iterationDuration);
}

TEST_F(AnimationAnimationV8Test, CanOmitSpecifiedDuration)
{
    Vector<Dictionary, 0> jsKeyframes;
    RefPtr<Animation> animation = createAnimation(element.get(), jsKeyframes);
    EXPECT_TRUE(std::isnan(animation->specifiedTiming().iterationDuration));
}

TEST_F(AnimationAnimationV8Test, NegativeDurationIsAuto)
{
    Vector<Dictionary, 0> jsKeyframes;
    RefPtr<Animation> animation = createAnimation(element.get(), jsKeyframes, -2);
    EXPECT_TRUE(std::isnan(animation->specifiedTiming().iterationDuration));
}

TEST_F(AnimationAnimationV8Test, SpecifiedGetters)
{
    Vector<Dictionary, 0> jsKeyframes;

    v8::Handle<v8::Object> timingInput = v8::Object::New(m_isolate);
    setV8ObjectPropertyAsNumber(timingInput, "delay", 2);
    setV8ObjectPropertyAsNumber(timingInput, "endDelay", 0.5);
    setV8ObjectPropertyAsString(timingInput, "fill", "backwards");
    setV8ObjectPropertyAsNumber(timingInput, "iterationStart", 2);
    setV8ObjectPropertyAsNumber(timingInput, "iterations", 10);
    setV8ObjectPropertyAsNumber(timingInput, "playbackRate", 2);
    setV8ObjectPropertyAsString(timingInput, "direction", "reverse");
    setV8ObjectPropertyAsString(timingInput, "easing", "step-start");
    Dictionary timingInputDictionary = Dictionary(v8::Handle<v8::Value>::Cast(timingInput), m_isolate);

    RefPtr<Animation> animation = createAnimation(element.get(), jsKeyframes, timingInputDictionary);

    RefPtr<TimedItemTiming> specified = animation->specified();
    EXPECT_EQ(2, specified->delay());
    EXPECT_EQ(0.5, specified->endDelay());
    EXPECT_EQ("backwards", specified->fill());
    EXPECT_EQ(2, specified->iterationStart());
    EXPECT_EQ(10, specified->iterations());
    EXPECT_EQ(2, specified->playbackRate());
    EXPECT_EQ("reverse", specified->direction());
    EXPECT_EQ("step-start", specified->easing());
}

TEST_F(AnimationAnimationV8Test, SpecifiedDurationGetter)
{
    Vector<Dictionary, 0> jsKeyframes;

    v8::Handle<v8::Object> timingInputWithDuration = v8::Object::New(m_isolate);
    setV8ObjectPropertyAsNumber(timingInputWithDuration, "duration", 2.5);
    Dictionary timingInputDictionaryWithDuration = Dictionary(v8::Handle<v8::Value>::Cast(timingInputWithDuration), m_isolate);

    RefPtr<Animation> animationWithDuration = createAnimation(element.get(), jsKeyframes, timingInputDictionaryWithDuration);

    RefPtr<TimedItemTiming> specifiedWithDuration = animationWithDuration->specified();
    bool isNumber = false;
    double numberDuration = std::numeric_limits<double>::quiet_NaN();
    bool isString = false;
    String stringDuration = "";
    specifiedWithDuration->getDuration("duration", isNumber, numberDuration, isString, stringDuration);
    EXPECT_TRUE(isNumber);
    EXPECT_EQ(2.5, numberDuration);
    EXPECT_FALSE(isString);
    EXPECT_EQ("", stringDuration);


    v8::Handle<v8::Object> timingInputNoDuration = v8::Object::New(m_isolate);
    Dictionary timingInputDictionaryNoDuration = Dictionary(v8::Handle<v8::Value>::Cast(timingInputNoDuration), m_isolate);

    RefPtr<Animation> animationNoDuration = createAnimation(element.get(), jsKeyframes, timingInputDictionaryNoDuration);

    RefPtr<TimedItemTiming> specifiedNoDuration = animationNoDuration->specified();
    isNumber = false;
    numberDuration = std::numeric_limits<double>::quiet_NaN();
    isString = false;
    stringDuration = "";
    specifiedNoDuration->getDuration("duration", isNumber, numberDuration, isString, stringDuration);
    EXPECT_FALSE(isNumber);
    EXPECT_TRUE(std::isnan(numberDuration));
    EXPECT_TRUE(isString);
    EXPECT_EQ("auto", stringDuration);
}

TEST_F(AnimationAnimationV8Test, SpecifiedSetters)
{
    Vector<Dictionary, 0> jsKeyframes;
    v8::Handle<v8::Object> timingInput = v8::Object::New(m_isolate);
    Dictionary timingInputDictionary = Dictionary(v8::Handle<v8::Value>::Cast(timingInput), m_isolate);
    RefPtr<Animation> animation = createAnimation(element.get(), jsKeyframes, timingInputDictionary);

    RefPtr<TimedItemTiming> specified = animation->specified();

    EXPECT_EQ(0, specified->delay());
    specified->setDelay(2);
    EXPECT_EQ(2, specified->delay());

    EXPECT_EQ(0, specified->endDelay());
    specified->setEndDelay(0.5);
    EXPECT_EQ(0.5, specified->endDelay());

    EXPECT_EQ("auto", specified->fill());
    specified->setFill("backwards");
    EXPECT_EQ("backwards", specified->fill());

    EXPECT_EQ(0, specified->iterationStart());
    specified->setIterationStart(2);
    EXPECT_EQ(2, specified->iterationStart());

    EXPECT_EQ(1, specified->iterations());
    specified->setIterations(10);
    EXPECT_EQ(10, specified->iterations());

    EXPECT_EQ(1, specified->playbackRate());
    specified->setPlaybackRate(2);
    EXPECT_EQ(2, specified->playbackRate());

    EXPECT_EQ("normal", specified->direction());
    specified->setDirection("reverse");
    EXPECT_EQ("reverse", specified->direction());

    EXPECT_EQ("linear", specified->easing());
    specified->setEasing("step-start");
    EXPECT_EQ("step-start", specified->easing());
}

TEST_F(AnimationAnimationV8Test, SetSpecifiedDuration)
{
    Vector<Dictionary, 0> jsKeyframes;
    v8::Handle<v8::Object> timingInput = v8::Object::New(m_isolate);
    Dictionary timingInputDictionary = Dictionary(v8::Handle<v8::Value>::Cast(timingInput), m_isolate);
    RefPtr<Animation> animation = createAnimation(element.get(), jsKeyframes, timingInputDictionary);

    RefPtr<TimedItemTiming> specified = animation->specified();

    bool isNumber = false;
    double numberDuration = std::numeric_limits<double>::quiet_NaN();
    bool isString = false;
    String stringDuration = "";
    specified->getDuration("duration", isNumber, numberDuration, isString, stringDuration);
    EXPECT_FALSE(isNumber);
    EXPECT_TRUE(std::isnan(numberDuration));
    EXPECT_TRUE(isString);
    EXPECT_EQ("auto", stringDuration);

    specified->setDuration("duration", 2.5);
    isNumber = false;
    numberDuration = std::numeric_limits<double>::quiet_NaN();
    isString = false;
    stringDuration = "";
    specified->getDuration("duration", isNumber, numberDuration, isString, stringDuration);
    EXPECT_TRUE(isNumber);
    EXPECT_EQ(2.5, numberDuration);
    EXPECT_FALSE(isString);
    EXPECT_EQ("", stringDuration);
}

TEST_F(AnimationAnimationTest, TimeToEffectChange)
{
    Timing timing;
    timing.iterationDuration = 100;
    timing.startDelay = 100;
    timing.endDelay = 100;
    timing.fillMode = Timing::FillModeNone;
    RefPtr<Animation> animation = Animation::create(nullptr, nullptr, timing);
    RefPtr<Player> player = document->timeline().play(animation.get());
    double inf = std::numeric_limits<double>::infinity();

    EXPECT_EQ(100, animation->timeToForwardsEffectChange());
    EXPECT_EQ(inf, animation->timeToReverseEffectChange());

    player->setCurrentTime(100);
    EXPECT_EQ(0, animation->timeToForwardsEffectChange());
    EXPECT_EQ(0, animation->timeToReverseEffectChange());

    player->setCurrentTime(199);
    EXPECT_EQ(0, animation->timeToForwardsEffectChange());
    EXPECT_EQ(0, animation->timeToReverseEffectChange());

    player->setCurrentTime(200);
    // End-exclusive.
    EXPECT_EQ(inf, animation->timeToForwardsEffectChange());
    EXPECT_EQ(0, animation->timeToReverseEffectChange());

    player->setCurrentTime(300);
    EXPECT_EQ(inf, animation->timeToForwardsEffectChange());
    EXPECT_EQ(100, animation->timeToReverseEffectChange());
}

TEST_F(AnimationAnimationTest, TimeToEffectChangeWithPlaybackRate)
{
    Timing timing;
    timing.iterationDuration = 100;
    timing.startDelay = 100;
    timing.endDelay = 100;
    timing.playbackRate = 2;
    timing.fillMode = Timing::FillModeNone;
    RefPtr<Animation> animation = Animation::create(nullptr, nullptr, timing);
    RefPtr<Player> player = document->timeline().play(animation.get());
    double inf = std::numeric_limits<double>::infinity();

    EXPECT_EQ(100, animation->timeToForwardsEffectChange());
    EXPECT_EQ(inf, animation->timeToReverseEffectChange());

    player->setCurrentTime(100);
    EXPECT_EQ(0, animation->timeToForwardsEffectChange());
    EXPECT_EQ(0, animation->timeToReverseEffectChange());

    player->setCurrentTime(149);
    EXPECT_EQ(0, animation->timeToForwardsEffectChange());
    EXPECT_EQ(0, animation->timeToReverseEffectChange());

    player->setCurrentTime(150);
    // End-exclusive.
    EXPECT_EQ(inf, animation->timeToForwardsEffectChange());
    EXPECT_EQ(0, animation->timeToReverseEffectChange());

    player->setCurrentTime(200);
    EXPECT_EQ(inf, animation->timeToForwardsEffectChange());
    EXPECT_EQ(50, animation->timeToReverseEffectChange());
}

TEST_F(AnimationAnimationTest, TimeToEffectChangeWithNegativePlaybackRate)
{
    Timing timing;
    timing.iterationDuration = 100;
    timing.startDelay = 100;
    timing.endDelay = 100;
    timing.playbackRate = -2;
    timing.fillMode = Timing::FillModeNone;
    RefPtr<Animation> animation = Animation::create(nullptr, nullptr, timing);
    RefPtr<Player> player = document->timeline().play(animation.get());
    double inf = std::numeric_limits<double>::infinity();

    EXPECT_EQ(100, animation->timeToForwardsEffectChange());
    EXPECT_EQ(inf, animation->timeToReverseEffectChange());

    player->setCurrentTime(100);
    EXPECT_EQ(0, animation->timeToForwardsEffectChange());
    EXPECT_EQ(0, animation->timeToReverseEffectChange());

    player->setCurrentTime(149);
    EXPECT_EQ(0, animation->timeToForwardsEffectChange());
    EXPECT_EQ(0, animation->timeToReverseEffectChange());

    player->setCurrentTime(150);
    EXPECT_EQ(inf, animation->timeToForwardsEffectChange());
    EXPECT_EQ(0, animation->timeToReverseEffectChange());

    player->setCurrentTime(200);
    EXPECT_EQ(inf, animation->timeToForwardsEffectChange());
    EXPECT_EQ(50, animation->timeToReverseEffectChange());
}

} // namespace WebCore
