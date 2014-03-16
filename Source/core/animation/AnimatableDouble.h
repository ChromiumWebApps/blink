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

#ifndef AnimatableDouble_h
#define AnimatableDouble_h

#include "core/animation/AnimatableValue.h"
#include "core/css/CSSValue.h"

namespace WebCore {

class AnimatableDouble FINAL : public AnimatableValue {
public:
    virtual ~AnimatableDouble() { }

    enum Constraint {
        Unconstrained,
        InterpolationIsNonContinuousWithZero,
    };

    static PassRefPtr<AnimatableDouble> create(double number, Constraint constraint = Unconstrained)
    {
        return adoptRef(new AnimatableDouble(number, constraint));
    }

    PassRefPtrWillBeRawPtr<CSSValue> toCSSValue() const;
    double toDouble() const { return m_number; }

protected:
    virtual PassRefPtr<AnimatableValue> interpolateTo(const AnimatableValue*, double fraction) const OVERRIDE;
    virtual PassRefPtr<AnimatableValue> addWith(const AnimatableValue*) const OVERRIDE;
    virtual bool usesDefaultInterpolationWith(const AnimatableValue*) const OVERRIDE;

private:
    AnimatableDouble(double number, Constraint constraint)
        : m_number(number)
        , m_constraint(constraint)
    {
    }
    virtual AnimatableType type() const OVERRIDE { return TypeDouble; }
    virtual bool equalTo(const AnimatableValue*) const OVERRIDE;
    virtual double distanceTo(const AnimatableValue*) const OVERRIDE;

    double m_number;
    Constraint m_constraint;
};

DEFINE_ANIMATABLE_VALUE_TYPE_CASTS(AnimatableDouble, isDouble());

} // namespace WebCore

#endif // AnimatableDouble_h
