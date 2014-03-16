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

#ifndef NewSVGProperty_h
#define NewSVGProperty_h

#include "core/svg/properties/SVGPropertyInfo.h"
#include "wtf/Noncopyable.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefCounted.h"
#include "wtf/text/WTFString.h"

namespace WebCore {

class ExceptionState;
class QualifiedName;
class SVGElement;
class SVGAnimationElement;

class NewSVGPropertyBase : public RefCounted<NewSVGPropertyBase> {
    WTF_MAKE_NONCOPYABLE(NewSVGPropertyBase);

public:
    // Properties do not have a primitive type by default
    typedef void PrimitiveType;

    virtual ~NewSVGPropertyBase()
    {
        ASSERT(!m_ownerList);
    }

    // FIXME: remove this in WebAnimations transition.
    // This is used from SVGAnimatedNewPropertyAnimator for its animate-by-string implementation.
    virtual PassRefPtr<NewSVGPropertyBase> cloneForAnimation(const String&) const = 0;

    virtual String valueAsString() const = 0;

    // FIXME: remove below and just have this inherit AnimatableValue in WebAnimations transition.
    virtual void add(PassRefPtr<NewSVGPropertyBase>, SVGElement*) = 0;
    virtual void calculateAnimatedValue(SVGAnimationElement*, float percentage, unsigned repeatCount, PassRefPtr<NewSVGPropertyBase> from, PassRefPtr<NewSVGPropertyBase> to, PassRefPtr<NewSVGPropertyBase> toAtEndOfDurationValue, SVGElement*) = 0;
    virtual float calculateDistance(PassRefPtr<NewSVGPropertyBase> to, SVGElement*) = 0;

    AnimatedPropertyType type()
    {
        return m_type;
    }

    NewSVGPropertyBase* ownerList() const
    {
        return m_ownerList;
    }

    void setOwnerList(NewSVGPropertyBase* ownerList)
    {
        // Previous owner list must be cleared before setting new owner list.
        ASSERT((!ownerList && m_ownerList) || (ownerList && !m_ownerList));

        m_ownerList = ownerList;
    }

protected:
    explicit NewSVGPropertyBase(AnimatedPropertyType type)
        : m_type(type)
        , m_ownerList(0)
    {
    }

private:
    const AnimatedPropertyType m_type;

    // FIXME: oilpan: This is kept as a raw ptr to break reference cycle. Should be Member in oilpan.
    NewSVGPropertyBase* m_ownerList;
};

}

#endif // NewSVGProperty_h
