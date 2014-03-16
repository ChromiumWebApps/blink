/*
 * Copyright (C) 2004, 2005 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006 Rob Buis <buis@kde.org>
 * Copyright (C) 2007 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Cameron McCormack <cam@mcc.id.au>
 * Copyright (C) Research In Motion Limited 2011. All rights reserved.
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

#ifndef SVGAnimationElement_h
#define SVGAnimationElement_h

#include "core/svg/SVGAnimatedBoolean.h"
#include "core/svg/SVGTests.h"
#include "core/svg/animation/SVGSMILElement.h"
#include "platform/animation/UnitBezier.h"
#include "wtf/Functional.h"

namespace WebCore {

enum AnimationMode {
    NoAnimation,
    FromToAnimation,
    FromByAnimation,
    ToAnimation,
    ByAnimation,
    ValuesAnimation,
    PathAnimation // Used by AnimateMotion.
};

// If we have 'inherit' as animation value, we need to grab the value
// during the animation since the value can be animated itself.
enum AnimatedPropertyValueType {
    RegularPropertyValue,
    InheritValue
};

enum CalcMode {
    CalcModeDiscrete,
    CalcModeLinear,
    CalcModePaced,
    CalcModeSpline
};

class ConditionEventListener;
class TimeContainer;
class SVGAnimatedType;

class SVGAnimationElement : public SVGSMILElement,
                            public SVGTests {
public:
    // SVGAnimationElement
    float getStartTime() const;
    float getCurrentTime() const;
    float getSimpleDuration() const;

    void beginElement();
    void beginElementAt(float offset);
    void endElement();
    void endElementAt(float offset);

    static bool isTargetAttributeCSSProperty(SVGElement*, const QualifiedName&);

    virtual bool isAdditive() const;
    bool isAccumulated() const;
    AnimationMode animationMode() const { return m_animationMode; }
    CalcMode calcMode() const { return m_calcMode; }

    enum ShouldApplyAnimation {
        DontApplyAnimation,
        ApplyCSSAnimation,
        ApplyXMLAnimation
    };

    ShouldApplyAnimation shouldApplyAnimation(SVGElement* targetElement, const QualifiedName& attributeName);

    AnimatedPropertyValueType fromPropertyValueType() const { return m_fromPropertyValueType; }
    AnimatedPropertyValueType toPropertyValueType() const { return m_toPropertyValueType; }

    // FIXME: In C++11, remove this as we can use default template argument.
    template<typename AnimatedType>
    void adjustForInheritance(AnimatedType (*parseTypeFromString)(SVGAnimationElement*, const String&),
                              AnimatedPropertyValueType valueType, AnimatedType& animatedType, SVGElement* contextElement)
    {
        ASSERT(parseTypeFromString);
        adjustForInheritance<AnimatedType, AnimatedType (*)(SVGAnimationElement*, const String&)>(parseTypeFromString, valueType, animatedType, contextElement);
    }

    template<typename AnimatedType, typename ParseTypeFromStringType>
    void adjustForInheritance(ParseTypeFromStringType parseTypeFromString, AnimatedPropertyValueType valueType, AnimatedType& animatedType, SVGElement* contextElement)
    {
        if (valueType != InheritValue)
            return;
        // Replace 'inherit' by its computed property value.
        String typeString;
        adjustForInheritance(contextElement, attributeName(), typeString);
        animatedType = parseTypeFromString(this, typeString);
    }

    template<typename AnimatedType>
    bool adjustFromToListValues(const AnimatedType& fromList, const AnimatedType& toList, AnimatedType& animatedList, float percentage, bool resizeAnimatedListIfNeeded = true)
    {
        // If no 'to' value is given, nothing to animate.
        unsigned toListSize = toList.size();
        if (!toListSize)
            return false;

        // If the 'from' value is given and it's length doesn't match the 'to' value list length, fallback to a discrete animation.
        unsigned fromListSize = fromList.size();
        if (fromListSize != toListSize && fromListSize) {
            if (percentage < 0.5) {
                if (animationMode() != ToAnimation)
                    animatedList = AnimatedType(fromList);
            } else
                animatedList = AnimatedType(toList);

            return false;
        }

        ASSERT(!fromListSize || fromListSize == toListSize);
        if (resizeAnimatedListIfNeeded && animatedList.size() < toListSize)
            animatedList.resize(toListSize);

        return true;
    }

    template<typename AnimatedType>
    void animateDiscreteType(float percentage, const AnimatedType& fromType, const AnimatedType& toType, AnimatedType& animatedType)
    {
        if ((animationMode() == FromToAnimation && percentage > 0.5) || animationMode() == ToAnimation || percentage == 1) {
            animatedType = AnimatedType(toType);
            return;
        }
        animatedType = AnimatedType(fromType);
    }

    void animateAdditiveNumber(float percentage, unsigned repeatCount, float fromNumber, float toNumber, float toAtEndOfDurationNumber, float& animatedNumber)
    {
        float number;
        if (calcMode() == CalcModeDiscrete)
            number = percentage < 0.5 ? fromNumber : toNumber;
        else
            number = (toNumber - fromNumber) * percentage + fromNumber;

        if (isAccumulated() && repeatCount)
            number += toAtEndOfDurationNumber * repeatCount;

        if (isAdditive() && animationMode() != ToAnimation)
            animatedNumber += number;
        else
            animatedNumber = number;
    }

protected:
    SVGAnimationElement(const QualifiedName&, Document&);

    void computeCSSPropertyValue(SVGElement*, CSSPropertyID, String& value);
    void determinePropertyValueTypes(const String& from, const String& to);

    bool isSupportedAttribute(const QualifiedName&);
    virtual void parseAttribute(const QualifiedName&, const AtomicString&) OVERRIDE;
    virtual void svgAttributeChanged(const QualifiedName&) OVERRIDE;

    enum AttributeType {
        AttributeTypeCSS,
        AttributeTypeXML,
        AttributeTypeAuto
    };
    AttributeType attributeType() const { return m_attributeType; }

    String toValue() const;
    String byValue() const;
    String fromValue() const;

    // from SVGSMILElement
    virtual void startedActiveInterval() OVERRIDE;
    virtual void updateAnimation(float percent, unsigned repeat, SVGSMILElement* resultElement) OVERRIDE;

    AnimatedPropertyValueType m_fromPropertyValueType;
    AnimatedPropertyValueType m_toPropertyValueType;

    virtual void setTargetElement(SVGElement*) OVERRIDE;
    virtual void setAttributeName(const QualifiedName&) OVERRIDE;
    AnimatedPropertyType determineAnimatedPropertyType() const;

    bool hasInvalidCSSAttributeType() const { return m_hasInvalidCSSAttributeType; }

    virtual void updateAnimationMode();
    void setAnimationMode(AnimationMode animationMode) { m_animationMode = animationMode; }
    void setCalcMode(CalcMode calcMode) { m_calcMode = calcMode; }

private:
    virtual void animationAttributeChanged() OVERRIDE;
    void setAttributeType(const AtomicString&);

    void checkInvalidCSSAttributeType(SVGElement*);

    virtual bool calculateToAtEndOfDurationValue(const String& toAtEndOfDurationString) = 0;
    virtual bool calculateFromAndToValues(const String& fromString, const String& toString) = 0;
    virtual bool calculateFromAndByValues(const String& fromString, const String& byString) = 0;
    virtual void calculateAnimatedValue(float percent, unsigned repeatCount, SVGSMILElement* resultElement) = 0;
    virtual float calculateDistance(const String& /*fromString*/, const String& /*toString*/) { return -1.f; }

    void currentValuesForValuesAnimation(float percent, float& effectivePercent, String& from, String& to);
    void calculateKeyTimesForCalcModePaced();
    float calculatePercentFromKeyPoints(float percent) const;
    void currentValuesFromKeyPoints(float percent, float& effectivePercent, String& from, String& to) const;
    float calculatePercentForSpline(float percent, unsigned splineIndex) const;
    float calculatePercentForFromTo(float percent) const;
    unsigned calculateKeyTimesIndex(float percent) const;

    void adjustForInheritance(SVGElement* targetElement, const QualifiedName& attributeName, String&);

    void setCalcMode(const AtomicString&);

    bool m_animationValid;

    AttributeType m_attributeType;
    Vector<String> m_values;
    // FIXME: We should probably use doubles for this, but there's no point
    // making such a change unless all SVG logic for sampling animations is
    // changed to use doubles.
    Vector<float> m_keyTimes;
    Vector<float> m_keyPoints;
    Vector<UnitBezier> m_keySplines;
    String m_lastValuesAnimationFrom;
    String m_lastValuesAnimationTo;
    bool m_hasInvalidCSSAttributeType;
    CalcMode m_calcMode;
    AnimationMode m_animationMode;
};

} // namespace WebCore

#endif // SVGAnimationElement_h
