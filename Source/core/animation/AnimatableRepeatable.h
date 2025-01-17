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

#ifndef AnimatableRepeatable_h
#define AnimatableRepeatable_h

#include "core/animation/AnimatableValue.h"
#include "wtf/Vector.h"

namespace WebCore {

// This class represents collections of values that animate in a repeated fashion as described by the CSS Transitions spec:
// http://www.w3.org/TR/css3-transitions/#animtype-repeatable-list
class AnimatableRepeatable : public AnimatableValue {
public:
    virtual ~AnimatableRepeatable() { }

    // This will consume the vector passed into it.
    static PassRefPtr<AnimatableRepeatable> create(Vector<RefPtr<AnimatableValue> >& values)
    {
        return adoptRef(new AnimatableRepeatable(values));
    }

    const Vector<RefPtr<AnimatableValue> >& values() const { return m_values; }

protected:
    AnimatableRepeatable()
    {
    }
    AnimatableRepeatable(Vector<RefPtr<AnimatableValue> >& values)
    {
        ASSERT(!values.isEmpty());
        m_values.swap(values);
    }

    static bool interpolateLists(const Vector<RefPtr<AnimatableValue> >& fromValues, const Vector<RefPtr<AnimatableValue> >& toValues, double fraction, Vector<RefPtr<AnimatableValue> >& interpolatedValues);

    virtual bool usesDefaultInterpolationWith(const AnimatableValue*) const OVERRIDE;

    Vector<RefPtr<AnimatableValue> > m_values;

private:
    virtual PassRefPtr<AnimatableValue> interpolateTo(const AnimatableValue*, double fraction) const OVERRIDE;
    virtual PassRefPtr<AnimatableValue> addWith(const AnimatableValue*) const OVERRIDE FINAL;

    virtual AnimatableType type() const OVERRIDE { return TypeRepeatable; }
    virtual bool equalTo(const AnimatableValue*) const OVERRIDE FINAL;
};

DEFINE_TYPE_CASTS(AnimatableRepeatable, AnimatableValue, value, (value->isRepeatable() || value->isStrokeDasharrayList()), (value.isRepeatable() || value.isStrokeDasharrayList()));

} // namespace WebCore

#endif // AnimatableRepeatable_h
