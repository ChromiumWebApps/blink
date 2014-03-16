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

#ifndef Animation_h
#define Animation_h

#include "core/animation/AnimationEffect.h"
#include "core/animation/EffectInput.h"
#include "core/animation/TimedItem.h"
#include "core/animation/TimingInput.h"
#include "heap/Handle.h"
#include "wtf/RefPtr.h"

namespace WebCore {

class Element;
class Dictionary;

class Animation FINAL : public TimedItem {

public:
    enum Priority { DefaultPriority, TransitionPriority };

    static PassRefPtr<Animation> create(PassRefPtr<Element>, PassRefPtrWillBeRawPtr<AnimationEffect>, const Timing&, Priority = DefaultPriority, PassOwnPtr<EventDelegate> = nullptr);
    // Web Animations API Bindings constructors.
    static PassRefPtr<Animation> create(Element*, PassRefPtrWillBeRawPtr<AnimationEffect>, const Dictionary& timingInputDictionary);
    static PassRefPtr<Animation> create(Element*, PassRefPtrWillBeRawPtr<AnimationEffect>, double duration);
    static PassRefPtr<Animation> create(Element*, PassRefPtrWillBeRawPtr<AnimationEffect>);
    static PassRefPtr<Animation> create(Element*, const Vector<Dictionary>& keyframeDictionaryVector, const Dictionary& timingInputDictionary);
    static PassRefPtr<Animation> create(Element*, const Vector<Dictionary>& keyframeDictionaryVector, double duration);
    static PassRefPtr<Animation> create(Element*, const Vector<Dictionary>& keyframeDictionaryVector);

    // FIXME: Move all of these setter methods out of Animation,
    // possibly into a new class (TimingInput?).
    static void setStartDelay(Timing&, double startDelay);
    static void setEndDelay(Timing&, double endDelay);
    static void setFillMode(Timing&, String fillMode);
    static void setIterationStart(Timing&, double iterationStart);
    static void setIterationCount(Timing&, double iterationCount);
    static void setIterationDuration(Timing&, double iterationDuration);
    static void setPlaybackRate(Timing&, double playbackRate);
    static void setPlaybackDirection(Timing&, String direction);
    static void setTimingFunction(Timing&, String timingFunctionString);

    virtual bool isAnimation() const OVERRIDE { return true; }

    const AnimationEffect::CompositableValueList* compositableValues() const
    {
        ASSERT(m_compositableValues);
        return m_compositableValues.get();
    }

    bool affects(CSSPropertyID) const;
    const AnimationEffect* effect() const { return m_effect.get(); }
    Priority priority() const { return m_priority; }
    Element* target() { return m_target.get(); }

    bool isCandidateForAnimationOnCompositor() const;
    // Must only be called once and assumes to be part of a player without a start time.
    bool maybeStartAnimationOnCompositor();
    bool hasActiveAnimationsOnCompositor() const;
    bool hasActiveAnimationsOnCompositor(CSSPropertyID) const;
    void cancelAnimationOnCompositor();
    void pauseAnimationForTestingOnCompositor(double pauseTime);

protected:
    void applyEffects(bool previouslyInEffect);
    void clearEffects();
    virtual void updateChildrenAndEffects() const OVERRIDE;
    virtual void didAttach() OVERRIDE;
    virtual void willDetach() OVERRIDE;
    virtual double calculateTimeToEffectChange(bool forwards, double inheritedTime, double timeToNextIteration) const OVERRIDE;

private:
    static void populateTiming(Timing&, Dictionary);

    Animation(PassRefPtr<Element>, PassRefPtrWillBeRawPtr<AnimationEffect>, const Timing&, Priority, PassOwnPtr<EventDelegate>);

    RefPtr<Element> m_target;
    RefPtrWillBePersistent<AnimationEffect> m_effect;

    bool m_activeInAnimationStack;
    OwnPtr<AnimationEffect::CompositableValueList> m_compositableValues;

    Priority m_priority;

    Vector<int> m_compositorAnimationIds;

    friend class CSSAnimations;
    friend class AnimationAnimationV8Test;
};

DEFINE_TYPE_CASTS(Animation, TimedItem, timedItem, timedItem->isAnimation(), timedItem.isAnimation());

} // namespace WebCore

#endif
