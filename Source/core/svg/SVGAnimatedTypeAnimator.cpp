/*
 * Copyright (C) Research In Motion Limited 2011-2012. All rights reserved.
 * Copyright (C) 2013 Samsung Electronics. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "core/svg/SVGAnimatedTypeAnimator.h"

#include "core/svg/SVGAnimateTransformElement.h"
#include "core/svg/SVGAnimatedColor.h"
#include "core/svg/SVGAnimationElement.h"
#include "core/svg/SVGElement.h"
#include "core/svg/SVGElementInstance.h"
#include "core/svg/SVGLength.h"
#include "core/svg/SVGLengthList.h"
#include "core/svg/SVGNumber.h"
#include "core/svg/SVGPaint.h"
#include "core/svg/SVGPointList.h"
#include "core/svg/SVGString.h"
#include "core/svg/SVGTransformList.h"

namespace WebCore {

SVGAnimatedTypeAnimator::SVGAnimatedTypeAnimator(AnimatedPropertyType type, SVGAnimationElement* animationElement, SVGElement* contextElement)
    : m_type(type)
    , m_animationElement(animationElement)
    , m_contextElement(contextElement)
{
    ASSERT(m_animationElement);
    ASSERT(m_contextElement);
    ASSERT(m_type != AnimatedPoint
        && m_type != AnimatedStringList
        && m_type != AnimatedTransform
        && m_type != AnimatedUnknown);

    const QualifiedName& attributeName = m_animationElement->attributeName();
    m_animatedProperty = m_contextElement->propertyFromAttribute(attributeName);
    if (m_animatedProperty)
        ASSERT(m_animatedProperty->type() == m_type);
}

SVGAnimatedTypeAnimator::~SVGAnimatedTypeAnimator()
{
}

PassRefPtr<NewSVGPropertyBase> SVGAnimatedTypeAnimator::createPropertyForAnimation(const String& value)
{
    if (isAnimatingSVGDom()) {
        // SVG DOM animVal animation code-path.

        if (m_type == AnimatedTransformList) {
            // TransformList must be animated via <animateTransform>,
            // and its {from,by,to} attribute values needs to be parsed w.r.t. its "type" attribute.
            // Spec: http://www.w3.org/TR/SVG/single-page.html#animate-AnimateTransformElement
            ASSERT(m_animationElement);
            SVGTransformType transformType = toSVGAnimateTransformElement(m_animationElement)->transformType();
            return SVGTransformList::create(transformType, value);
        }

        ASSERT(m_animatedProperty);
        return m_animatedProperty->currentValueBase()->cloneForAnimation(value);
    }

    ASSERT(isAnimatingCSSProperty());

    // CSS properties animation code-path.
    // Create a basic instance of the corresponding SVG property.
    // The instance will not have full context info. (e.g. SVGLengthMode)

    switch (m_type) {
    case AnimatedColor:
        return SVGColorProperty::create(value.isEmpty() ? StyleColor::currentColor() : SVGPaint::colorFromRGBColorString(value));
    case AnimatedNumber: {
        RefPtr<SVGNumber> property = SVGNumber::create();
        property->setValueAsString(value, IGNORE_EXCEPTION);
        return property.release();
    }
    case AnimatedLength: {
        RefPtr<SVGLength> property = SVGLength::create(LengthModeOther);
        property->setValueAsString(value, IGNORE_EXCEPTION);
        return property.release();
    }
    case AnimatedLengthList: {
        RefPtr<SVGLengthList> property = SVGLengthList::create(LengthModeOther);
        property->setValueAsString(value, IGNORE_EXCEPTION);
        return property.release();
    }
    case AnimatedString: {
        RefPtr<SVGString> property = SVGString::create();
        property->setValueAsString(value, IGNORE_EXCEPTION);
        return property.release();
    }

    // These types don't appear in the table in SVGElement::cssPropertyToTypeMap() and thus don't need support.
    case AnimatedBoolean:
    case AnimatedNumberList:
    case AnimatedNumberOptionalNumber:
    case AnimatedPoint:
    case AnimatedPoints:
    case AnimatedRect:
    case AnimatedTransform:
    case AnimatedTransformList:
        ASSERT_NOT_REACHED();

    // These properties are not yet migrated to NewProperty implementation. see http://crbug.com/308818
    case AnimatedAngle:
    case AnimatedEnumeration:
    case AnimatedInteger:
    case AnimatedIntegerOptionalInteger:
    case AnimatedPath:
    case AnimatedPreserveAspectRatio:
    case AnimatedStringList:
        ASSERT_NOT_REACHED();

    case AnimatedUnknown:
        ASSERT_NOT_REACHED();
    };

    ASSERT_NOT_REACHED();
    return nullptr;
}

PassRefPtr<NewSVGPropertyBase> SVGAnimatedTypeAnimator::constructFromString(const String& value)
{
    return createPropertyForAnimation(value);
}

void SVGAnimatedTypeAnimator::calculateFromAndToValues(RefPtr<NewSVGPropertyBase>& from, RefPtr<NewSVGPropertyBase>& to, const String& fromString, const String& toString)
{
    from = constructFromString(fromString);
    to = constructFromString(toString);
}

void SVGAnimatedTypeAnimator::calculateFromAndByValues(RefPtr<NewSVGPropertyBase>& from, RefPtr<NewSVGPropertyBase>& to, const String& fromString, const String& byString)
{
    from = constructFromString(fromString);
    to = constructFromString(byString);
    to->add(from, m_contextElement);
}

namespace {

typedef void (NewSVGAnimatedPropertyBase::*NewSVGAnimatedPropertyMethod)();

void invokeMethodOnAllTargetProperties(const Vector<SVGElement*>& list, const QualifiedName& attributeName, NewSVGAnimatedPropertyMethod method)
{
    Vector<SVGElement*>::const_iterator it = list.begin();
    Vector<SVGElement*>::const_iterator itEnd = list.end();
    for (; it != itEnd; ++it) {
        RefPtr<NewSVGAnimatedPropertyBase> animatedProperty = (*it)->propertyFromAttribute(attributeName);
        if (animatedProperty)
            (animatedProperty.get()->*method)();
    }
}

void setAnimatedValueOnAllTargetProperties(const Vector<SVGElement*>& list, const QualifiedName& attributeName, PassRefPtr<NewSVGPropertyBase> passValue)
{
    RefPtr<NewSVGPropertyBase> value = passValue;

    Vector<SVGElement*>::const_iterator it = list.begin();
    Vector<SVGElement*>::const_iterator itEnd = list.end();
    for (; it != itEnd; ++it) {
        RefPtr<NewSVGAnimatedPropertyBase> animatedProperty = (*it)->propertyFromAttribute(attributeName);
        if (animatedProperty)
            animatedProperty->setAnimatedValue(value);
    }
}

}

PassRefPtr<NewSVGPropertyBase> SVGAnimatedTypeAnimator::resetAnimation(const Vector<SVGElement*>& list)
{
    ASSERT(isAnimatingSVGDom());
    RefPtr<NewSVGPropertyBase> animatedValue = m_animatedProperty->createAnimatedValue();
    ASSERT(animatedValue->type() == m_type);
    setAnimatedValueOnAllTargetProperties(list, m_animatedProperty->attributeName(), animatedValue);

    return animatedValue.release();
}

PassRefPtr<NewSVGPropertyBase> SVGAnimatedTypeAnimator::startAnimValAnimation(const Vector<SVGElement*>& list)
{
    ASSERT(isAnimatingSVGDom());
    SVGElementInstance::InstanceUpdateBlocker blocker(m_contextElement);

    invokeMethodOnAllTargetProperties(list, m_animatedProperty->attributeName(), &NewSVGAnimatedPropertyBase::animationStarted);

    return resetAnimation(list);
}

void SVGAnimatedTypeAnimator::stopAnimValAnimation(const Vector<SVGElement*>& list)
{
    ASSERT(isAnimatingSVGDom());
    SVGElementInstance::InstanceUpdateBlocker blocker(m_contextElement);

    invokeMethodOnAllTargetProperties(list, m_animatedProperty->attributeName(), &NewSVGAnimatedPropertyBase::animationEnded);
}

PassRefPtr<NewSVGPropertyBase> SVGAnimatedTypeAnimator::resetAnimValToBaseVal(const Vector<SVGElement*>& list)
{
    SVGElementInstance::InstanceUpdateBlocker blocker(m_contextElement);

    return resetAnimation(list);
}

class ParsePropertyFromString {
public:
    explicit ParsePropertyFromString(SVGAnimatedTypeAnimator* animator)
        : m_animator(animator)
    {
    }

    PassRefPtr<NewSVGPropertyBase> operator()(SVGAnimationElement*, const String& value)
    {
        return m_animator->createPropertyForAnimation(value);
    }

private:
    SVGAnimatedTypeAnimator* m_animator;
};

void SVGAnimatedTypeAnimator::calculateAnimatedValue(float percentage, unsigned repeatCount, NewSVGPropertyBase* from, NewSVGPropertyBase* to, NewSVGPropertyBase* toAtEndOfDuration, NewSVGPropertyBase* animated)
{
    ASSERT(m_animationElement);
    ASSERT(m_contextElement);

    RefPtr<NewSVGPropertyBase> fromValue = m_animationElement->animationMode() == ToAnimation ? animated : from;
    RefPtr<NewSVGPropertyBase> toValue = to;
    RefPtr<NewSVGPropertyBase> toAtEndOfDurationValue = toAtEndOfDuration;
    RefPtr<NewSVGPropertyBase> animatedValue = animated;

    // Apply CSS inheritance rules.
    ParsePropertyFromString parsePropertyFromString(this);
    m_animationElement->adjustForInheritance<RefPtr<NewSVGPropertyBase>, ParsePropertyFromString>(parsePropertyFromString, m_animationElement->fromPropertyValueType(), fromValue, m_contextElement);
    m_animationElement->adjustForInheritance<RefPtr<NewSVGPropertyBase>, ParsePropertyFromString>(parsePropertyFromString, m_animationElement->toPropertyValueType(), toValue, m_contextElement);

    animatedValue->calculateAnimatedValue(m_animationElement, percentage, repeatCount, fromValue, toValue, toAtEndOfDurationValue, m_contextElement);
}

float SVGAnimatedTypeAnimator::calculateDistance(const String& fromString, const String& toString)
{
    ASSERT(m_animationElement);
    ASSERT(m_contextElement);
    RefPtr<NewSVGPropertyBase> fromValue = createPropertyForAnimation(fromString);
    RefPtr<NewSVGPropertyBase> toValue = createPropertyForAnimation(toString);
    return fromValue->calculateDistance(toValue, m_contextElement);
}

}
