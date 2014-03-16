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

#include "config.h"
#include "core/svg/properties/NewSVGAnimatedProperty.h"

#include "core/svg/SVGElement.h"

namespace WebCore {

NewSVGAnimatedPropertyBase::NewSVGAnimatedPropertyBase(AnimatedPropertyType type, SVGElement* contextElement, const QualifiedName& attributeName)
    : m_type(type)
    , m_isReadOnly(false)
    , m_isAnimating(false)
    , m_contextElement(contextElement)
    , m_attributeName(attributeName)
{
    ASSERT(m_contextElement);
    ASSERT(m_attributeName != nullQName());
    // FIXME: setContextElement should be delayed until V8 wrapper is created.
    // FIXME: oilpan: or we can remove this backref ptr hack in oilpan.
    m_contextElement->setContextElement();
}

NewSVGAnimatedPropertyBase::~NewSVGAnimatedPropertyBase()
{
    ASSERT(!isAnimating());
}

void NewSVGAnimatedPropertyBase::animationStarted()
{
    ASSERT(!isAnimating());
    m_isAnimating = true;
}

void NewSVGAnimatedPropertyBase::animationEnded()
{
    ASSERT(isAnimating());
    m_isAnimating = false;
}

void NewSVGAnimatedPropertyBase::synchronizeAttribute()
{
    ASSERT(needsSynchronizeAttribute());
    AtomicString value(currentValueBase()->valueAsString());
    m_contextElement->setSynchronizedLazyAttribute(m_attributeName, value);
}

bool NewSVGAnimatedPropertyBase::isSpecified() const
{
    return isAnimating() || contextElement()->hasAttribute(attributeName());
}

void NewSVGAnimatedPropertyBase::commitChange()
{
    contextElement()->invalidateSVGAttributes();
    contextElement()->svgAttributeChanged(m_attributeName);
}

} // namespace WebCore
