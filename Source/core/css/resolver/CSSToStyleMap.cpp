/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 2004-2005 Allan Sandfeld Jensen (kde@carewolf.com)
 * Copyright (C) 2006, 2007 Nicholas Shanks (webkit@nickshanks.com)
 * Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010, 2011, 2012 Apple Inc. All rights reserved.
 * Copyright (C) 2007 Alexey Proskuryakov <ap@webkit.org>
 * Copyright (C) 2007, 2008 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
 * Copyright (c) 2011, Code Aurora Forum. All rights reserved.
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

#include "config.h"
#include "core/css/resolver/CSSToStyleMap.h"

#include "CSSValueKeywords.h"
#include "core/animation/css/CSSAnimationData.h"
#include "core/css/CSSBorderImageSliceValue.h"
#include "core/css/CSSPrimitiveValue.h"
#include "core/css/CSSPrimitiveValueMappings.h"
#include "core/css/CSSTimingFunctionValue.h"
#include "core/css/Pair.h"
#include "core/css/Rect.h"
#include "core/css/resolver/StyleResolverState.h"
#include "core/rendering/style/BorderImageLengthBox.h"
#include "core/rendering/style/FillLayer.h"

namespace WebCore {

const CSSToLengthConversionData& CSSToStyleMap::cssToLengthConversionData() const
{
    return m_state.cssToLengthConversionData();
}

bool CSSToStyleMap::useSVGZoomRules() const
{
    return m_state.useSVGZoomRules();
}

PassRefPtr<StyleImage> CSSToStyleMap::styleImage(CSSPropertyID propertyId, CSSValue* value)
{
    return m_elementStyleResources.styleImage(m_state.document().textLinkColors(), m_state.style()->color(), propertyId, value);
}

void CSSToStyleMap::mapFillAttachment(CSSPropertyID, FillLayer* layer, CSSValue* value) const
{
    if (value->isInitialValue()) {
        layer->setAttachment(FillLayer::initialFillAttachment(layer->type()));
        return;
    }

    if (!value->isPrimitiveValue())
        return;

    CSSPrimitiveValue* primitiveValue = toCSSPrimitiveValue(value);
    switch (primitiveValue->getValueID()) {
    case CSSValueFixed:
        layer->setAttachment(FixedBackgroundAttachment);
        break;
    case CSSValueScroll:
        layer->setAttachment(ScrollBackgroundAttachment);
        break;
    case CSSValueLocal:
        layer->setAttachment(LocalBackgroundAttachment);
        break;
    default:
        return;
    }
}

void CSSToStyleMap::mapFillClip(CSSPropertyID, FillLayer* layer, CSSValue* value) const
{
    if (value->isInitialValue()) {
        layer->setClip(FillLayer::initialFillClip(layer->type()));
        return;
    }

    if (!value->isPrimitiveValue())
        return;

    CSSPrimitiveValue* primitiveValue = toCSSPrimitiveValue(value);
    layer->setClip(*primitiveValue);
}

void CSSToStyleMap::mapFillComposite(CSSPropertyID, FillLayer* layer, CSSValue* value) const
{
    if (value->isInitialValue()) {
        layer->setComposite(FillLayer::initialFillComposite(layer->type()));
        return;
    }

    if (!value->isPrimitiveValue())
        return;

    CSSPrimitiveValue* primitiveValue = toCSSPrimitiveValue(value);
    layer->setComposite(*primitiveValue);
}

void CSSToStyleMap::mapFillBlendMode(CSSPropertyID, FillLayer* layer, CSSValue* value) const
{
    if (value->isInitialValue()) {
        layer->setBlendMode(FillLayer::initialFillBlendMode(layer->type()));
        return;
    }

    if (!value->isPrimitiveValue())
        return;

    CSSPrimitiveValue* primitiveValue = toCSSPrimitiveValue(value);
    layer->setBlendMode(*primitiveValue);
}

void CSSToStyleMap::mapFillOrigin(CSSPropertyID, FillLayer* layer, CSSValue* value) const
{
    if (value->isInitialValue()) {
        layer->setOrigin(FillLayer::initialFillOrigin(layer->type()));
        return;
    }

    if (!value->isPrimitiveValue())
        return;

    CSSPrimitiveValue* primitiveValue = toCSSPrimitiveValue(value);
    layer->setOrigin(*primitiveValue);
}


void CSSToStyleMap::mapFillImage(CSSPropertyID property, FillLayer* layer, CSSValue* value)
{
    if (value->isInitialValue()) {
        layer->setImage(FillLayer::initialFillImage(layer->type()));
        return;
    }

    layer->setImage(styleImage(property, value));
}

void CSSToStyleMap::mapFillRepeatX(CSSPropertyID, FillLayer* layer, CSSValue* value) const
{
    if (value->isInitialValue()) {
        layer->setRepeatX(FillLayer::initialFillRepeatX(layer->type()));
        return;
    }

    if (!value->isPrimitiveValue())
        return;

    CSSPrimitiveValue* primitiveValue = toCSSPrimitiveValue(value);
    layer->setRepeatX(*primitiveValue);
}

void CSSToStyleMap::mapFillRepeatY(CSSPropertyID, FillLayer* layer, CSSValue* value) const
{
    if (value->isInitialValue()) {
        layer->setRepeatY(FillLayer::initialFillRepeatY(layer->type()));
        return;
    }

    if (!value->isPrimitiveValue())
        return;

    CSSPrimitiveValue* primitiveValue = toCSSPrimitiveValue(value);
    layer->setRepeatY(*primitiveValue);
}

void CSSToStyleMap::mapFillSize(CSSPropertyID, FillLayer* layer, CSSValue* value) const
{
    if (value->isInitialValue()) {
        layer->setSizeType(FillLayer::initialFillSizeType(layer->type()));
        layer->setSizeLength(FillLayer::initialFillSizeLength(layer->type()));
        return;
    }

    if (!value->isPrimitiveValue())
        return;

    CSSPrimitiveValue* primitiveValue = toCSSPrimitiveValue(value);
    if (primitiveValue->getValueID() == CSSValueContain)
        layer->setSizeType(Contain);
    else if (primitiveValue->getValueID() == CSSValueCover)
        layer->setSizeType(Cover);
    else
        layer->setSizeType(SizeLength);

    LengthSize b = FillLayer::initialFillSizeLength(layer->type());

    if (primitiveValue->getValueID() == CSSValueContain || primitiveValue->getValueID() == CSSValueCover) {
        layer->setSizeLength(b);
        return;
    }

    Length firstLength;
    Length secondLength;

    if (Pair* pair = primitiveValue->getPairValue()) {
        firstLength = pair->first()->convertToLength<AnyConversion>(cssToLengthConversionData());
        secondLength = pair->second()->convertToLength<AnyConversion>(cssToLengthConversionData());
    } else {
        firstLength = primitiveValue->convertToLength<AnyConversion>(cssToLengthConversionData());
        secondLength = Length();
    }

    b.setWidth(firstLength);
    b.setHeight(secondLength);
    layer->setSizeLength(b);
}

void CSSToStyleMap::mapFillXPosition(CSSPropertyID propertyID, FillLayer* layer, CSSValue* value) const
{
    if (value->isInitialValue()) {
        layer->setXPosition(FillLayer::initialFillXPosition(layer->type()));
        return;
    }

    if (!value->isPrimitiveValue())
        return;

    CSSPrimitiveValue* primitiveValue = toCSSPrimitiveValue(value);
    Pair* pair = primitiveValue->getPairValue();
    if (pair) {
        ASSERT_UNUSED(propertyID, propertyID == CSSPropertyBackgroundPositionX || propertyID == CSSPropertyWebkitMaskPositionX);
        primitiveValue = pair->second();
    }

    Length length = primitiveValue->convertToLength<FixedConversion | PercentConversion>(cssToLengthConversionData());

    layer->setXPosition(length);
    if (pair)
        layer->setBackgroundXOrigin(*(pair->first()));
}

void CSSToStyleMap::mapFillYPosition(CSSPropertyID propertyID, FillLayer* layer, CSSValue* value) const
{
    if (value->isInitialValue()) {
        layer->setYPosition(FillLayer::initialFillYPosition(layer->type()));
        return;
    }

    if (!value->isPrimitiveValue())
        return;

    CSSPrimitiveValue* primitiveValue = toCSSPrimitiveValue(value);
    Pair* pair = primitiveValue->getPairValue();
    if (pair) {
        ASSERT_UNUSED(propertyID, propertyID == CSSPropertyBackgroundPositionY || propertyID == CSSPropertyWebkitMaskPositionY);
        primitiveValue = pair->second();
    }

    Length length = primitiveValue->convertToLength<FixedConversion | PercentConversion>(cssToLengthConversionData());

    layer->setYPosition(length);
    if (pair)
        layer->setBackgroundYOrigin(*(pair->first()));
}

void CSSToStyleMap::mapFillMaskSourceType(CSSPropertyID, FillLayer* layer, CSSValue* value)
{
    EMaskSourceType type = FillLayer::initialFillMaskSourceType(layer->type());
    if (value->isInitialValue()) {
        layer->setMaskSourceType(type);
        return;
    }

    if (!value->isPrimitiveValue())
        return;

    switch (toCSSPrimitiveValue(value)->getValueID()) {
    case CSSValueAlpha:
        type = MaskAlpha;
        break;
    case CSSValueLuminance:
        type = MaskLuminance;
        break;
    case CSSValueAuto:
        break;
    default:
        ASSERT_NOT_REACHED();
    }

    layer->setMaskSourceType(type);
}

void CSSToStyleMap::mapAnimationDelay(CSSAnimationData* animation, CSSValue* value) const
{
    if (value->isInitialValue()) {
        animation->setDelay(CSSAnimationData::initialAnimationDelay());
        return;
    }

    if (!value->isPrimitiveValue())
        return;

    animation->setDelay(toCSSPrimitiveValue(value)->computeTime<double, CSSPrimitiveValue::Seconds>());
}

void CSSToStyleMap::mapAnimationDirection(CSSAnimationData* layer, CSSValue* value) const
{
    if (value->isInitialValue()) {
        layer->setDirection(CSSAnimationData::initialAnimationDirection());
        return;
    }

    if (!value->isPrimitiveValue())
        return;

    switch (toCSSPrimitiveValue(value)->getValueID()) {
    case CSSValueNormal:
        layer->setDirection(CSSAnimationData::AnimationDirectionNormal);
        break;
    case CSSValueAlternate:
        layer->setDirection(CSSAnimationData::AnimationDirectionAlternate);
        break;
    case CSSValueReverse:
        layer->setDirection(CSSAnimationData::AnimationDirectionReverse);
        break;
    case CSSValueAlternateReverse:
        layer->setDirection(CSSAnimationData::AnimationDirectionAlternateReverse);
        break;
    default:
        break;
    }
}

void CSSToStyleMap::mapAnimationDuration(CSSAnimationData* animation, CSSValue* value) const
{
    if (value->isInitialValue()) {
        animation->setDuration(CSSAnimationData::initialAnimationDuration());
        return;
    }

    if (!value->isPrimitiveValue())
        return;

    CSSPrimitiveValue* primitiveValue = toCSSPrimitiveValue(value);
    animation->setDuration(primitiveValue->computeTime<double, CSSPrimitiveValue::Seconds>());
}

void CSSToStyleMap::mapAnimationFillMode(CSSAnimationData* layer, CSSValue* value) const
{
    if (value->isInitialValue()) {
        layer->setFillMode(CSSAnimationData::initialAnimationFillMode());
        return;
    }

    if (!value->isPrimitiveValue())
        return;

    CSSPrimitiveValue* primitiveValue = toCSSPrimitiveValue(value);
    switch (primitiveValue->getValueID()) {
    case CSSValueNone:
        layer->setFillMode(AnimationFillModeNone);
        break;
    case CSSValueForwards:
        layer->setFillMode(AnimationFillModeForwards);
        break;
    case CSSValueBackwards:
        layer->setFillMode(AnimationFillModeBackwards);
        break;
    case CSSValueBoth:
        layer->setFillMode(AnimationFillModeBoth);
        break;
    default:
        break;
    }
}

void CSSToStyleMap::mapAnimationIterationCount(CSSAnimationData* animation, CSSValue* value) const
{
    if (value->isInitialValue()) {
        animation->setIterationCount(CSSAnimationData::initialAnimationIterationCount());
        return;
    }

    if (!value->isPrimitiveValue())
        return;

    CSSPrimitiveValue* primitiveValue = toCSSPrimitiveValue(value);
    if (primitiveValue->getValueID() == CSSValueInfinite)
        animation->setIterationCount(CSSAnimationData::IterationCountInfinite);
    else
        animation->setIterationCount(primitiveValue->getFloatValue());
}

void CSSToStyleMap::mapAnimationName(CSSAnimationData* layer, CSSValue* value) const
{
    if (value->isInitialValue()) {
        layer->setName(CSSAnimationData::initialAnimationName());
        return;
    }

    if (!value->isPrimitiveValue())
        return;

    CSSPrimitiveValue* primitiveValue = toCSSPrimitiveValue(value);
    if (primitiveValue->getValueID() == CSSValueNone)
        layer->setIsNoneAnimation(true);
    else
        layer->setName(AtomicString(primitiveValue->getStringValue()));
}

void CSSToStyleMap::mapAnimationPlayState(CSSAnimationData* layer, CSSValue* value) const
{
    if (value->isInitialValue()) {
        layer->setPlayState(CSSAnimationData::initialAnimationPlayState());
        return;
    }

    if (!value->isPrimitiveValue())
        return;

    CSSPrimitiveValue* primitiveValue = toCSSPrimitiveValue(value);
    EAnimPlayState playState = (primitiveValue->getValueID() == CSSValuePaused) ? AnimPlayStatePaused : AnimPlayStatePlaying;
    layer->setPlayState(playState);
}

void CSSToStyleMap::mapAnimationProperty(CSSAnimationData* animation, CSSValue* value) const
{
    if (value->isInitialValue()) {
        animation->setAnimationMode(CSSAnimationData::AnimateAll);
        animation->setProperty(CSSPropertyInvalid);
        return;
    }

    if (!value->isPrimitiveValue())
        return;

    CSSPrimitiveValue* primitiveValue = toCSSPrimitiveValue(value);
    if (primitiveValue->getValueID() == CSSValueAll) {
        animation->setAnimationMode(CSSAnimationData::AnimateAll);
        animation->setProperty(CSSPropertyInvalid);
    } else if (primitiveValue->getValueID() == CSSValueNone) {
        animation->setAnimationMode(CSSAnimationData::AnimateNone);
        animation->setProperty(CSSPropertyInvalid);
    } else {
        animation->setAnimationMode(CSSAnimationData::AnimateSingleProperty);
        animation->setProperty(primitiveValue->getPropertyID());
    }
}

PassRefPtr<TimingFunction> CSSToStyleMap::animationTimingFunction(CSSValue* value, bool allowInitial)
{
    if (allowInitial && value->isInitialValue()) {
        return CSSAnimationData::initialAnimationTimingFunction();
    }

    if (value->isPrimitiveValue()) {
        CSSPrimitiveValue* primitiveValue = toCSSPrimitiveValue(value);
        switch (primitiveValue->getValueID()) {
        case CSSValueLinear:
            return LinearTimingFunction::preset();
            break;
        case CSSValueEase:
            return CubicBezierTimingFunction::preset(CubicBezierTimingFunction::Ease);
            break;
        case CSSValueEaseIn:
            return CubicBezierTimingFunction::preset(CubicBezierTimingFunction::EaseIn);
            break;
        case CSSValueEaseOut:
            return CubicBezierTimingFunction::preset(CubicBezierTimingFunction::EaseOut);
            break;
        case CSSValueEaseInOut:
            return CubicBezierTimingFunction::preset(CubicBezierTimingFunction::EaseInOut);
            break;
        case CSSValueStepStart:
            return StepsTimingFunction::preset(StepsTimingFunction::Start);
            break;
        case CSSValueStepMiddle:
            return StepsTimingFunction::preset(StepsTimingFunction::Middle);
            break;
        case CSSValueStepEnd:
            return StepsTimingFunction::preset(StepsTimingFunction::End);
            break;
        default:
            break;
        }
        return nullptr;
    }

    if (value->isCubicBezierTimingFunctionValue()) {
        CSSCubicBezierTimingFunctionValue* cubicTimingFunction = toCSSCubicBezierTimingFunctionValue(value);
        return CubicBezierTimingFunction::create(cubicTimingFunction->x1(), cubicTimingFunction->y1(), cubicTimingFunction->x2(), cubicTimingFunction->y2());
    } else if (value->isStepsTimingFunctionValue()) {
        CSSStepsTimingFunctionValue* stepsTimingFunction = toCSSStepsTimingFunctionValue(value);
        return StepsTimingFunction::create(stepsTimingFunction->numberOfSteps(), stepsTimingFunction->stepAtPosition());
    }

    return nullptr;
}

void CSSToStyleMap::mapAnimationTimingFunction(CSSAnimationData* animation, CSSValue* value) const
{
    RefPtr<TimingFunction> timingFunction = animationTimingFunction(value, true);
    if (timingFunction) {
        // Step middle timing functions are supported up to this point for use in the Web Animations API,
        // but should not be supported for CSS Animations and Transitions.
        bool isStepMiddleFunction = (timingFunction->type() == TimingFunction::StepsFunction) && (toStepsTimingFunction(*timingFunction).stepAtPosition() == StepsTimingFunction::StepAtMiddle);
        if (isStepMiddleFunction)
            animation->setTimingFunction(CubicBezierTimingFunction::preset(CubicBezierTimingFunction::Ease));
        else
            animation->setTimingFunction(timingFunction);
    }
}

void CSSToStyleMap::mapNinePieceImage(RenderStyle* mutableStyle, CSSPropertyID property, CSSValue* value, NinePieceImage& image)
{
    // If we're not a value list, then we are "none" and don't need to alter the empty image at all.
    if (!value || !value->isValueList())
        return;

    // Retrieve the border image value.
    CSSValueList* borderImage = toCSSValueList(value);

    // Set the image (this kicks off the load).
    CSSPropertyID imageProperty;
    if (property == CSSPropertyWebkitBorderImage)
        imageProperty = CSSPropertyBorderImageSource;
    else if (property == CSSPropertyWebkitMaskBoxImage)
        imageProperty = CSSPropertyWebkitMaskBoxImageSource;
    else
        imageProperty = property;

    for (unsigned i = 0 ; i < borderImage->length() ; ++i) {
        CSSValue* current = borderImage->item(i);

        if (current->isImageValue() || current->isImageGeneratorValue() || current->isImageSetValue())
            image.setImage(styleImage(imageProperty, current));
        else if (current->isBorderImageSliceValue())
            mapNinePieceImageSlice(current, image);
        else if (current->isValueList()) {
            CSSValueList* slashList = toCSSValueList(current);
            // Map in the image slices.
            if (slashList->item(0) && slashList->item(0)->isBorderImageSliceValue())
                mapNinePieceImageSlice(slashList->item(0), image);

            // Map in the border slices.
            if (slashList->item(1))
                image.setBorderSlices(mapNinePieceImageQuad(slashList->item(1)));

            // Map in the outset.
            if (slashList->item(2))
                image.setOutset(mapNinePieceImageQuad(slashList->item(2)));
        } else if (current->isPrimitiveValue()) {
            // Set the appropriate rules for stretch/round/repeat of the slices.
            mapNinePieceImageRepeat(current, image);
        }
    }

    if (property == CSSPropertyWebkitBorderImage) {
        // We have to preserve the legacy behavior of -webkit-border-image and make the border slices
        // also set the border widths. We don't need to worry about percentages, since we don't even support
        // those on real borders yet.
        if (image.borderSlices().top().isLength() && image.borderSlices().top().length().isFixed())
            mutableStyle->setBorderTopWidth(image.borderSlices().top().length().value());
        if (image.borderSlices().right().isLength() && image.borderSlices().right().length().isFixed())
            mutableStyle->setBorderRightWidth(image.borderSlices().right().length().value());
        if (image.borderSlices().bottom().isLength() && image.borderSlices().bottom().length().isFixed())
            mutableStyle->setBorderBottomWidth(image.borderSlices().bottom().length().value());
        if (image.borderSlices().left().isLength() && image.borderSlices().left().length().isFixed())
            mutableStyle->setBorderLeftWidth(image.borderSlices().left().length().value());
    }
}

void CSSToStyleMap::mapNinePieceImageSlice(CSSValue* value, NinePieceImage& image) const
{
    if (!value || !value->isBorderImageSliceValue())
        return;

    // Retrieve the border image value.
    CSSBorderImageSliceValue* borderImageSlice = toCSSBorderImageSliceValue(value);

    // Set up a length box to represent our image slices.
    LengthBox box;
    Quad* slices = borderImageSlice->slices();
    if (slices->top()->isPercentage())
        box.m_top = Length(slices->top()->getDoubleValue(), Percent);
    else
        box.m_top = Length(slices->top()->getIntValue(CSSPrimitiveValue::CSS_NUMBER), Fixed);
    if (slices->bottom()->isPercentage())
        box.m_bottom = Length(slices->bottom()->getDoubleValue(), Percent);
    else
        box.m_bottom = Length((int)slices->bottom()->getFloatValue(CSSPrimitiveValue::CSS_NUMBER), Fixed);
    if (slices->left()->isPercentage())
        box.m_left = Length(slices->left()->getDoubleValue(), Percent);
    else
        box.m_left = Length(slices->left()->getIntValue(CSSPrimitiveValue::CSS_NUMBER), Fixed);
    if (slices->right()->isPercentage())
        box.m_right = Length(slices->right()->getDoubleValue(), Percent);
    else
        box.m_right = Length(slices->right()->getIntValue(CSSPrimitiveValue::CSS_NUMBER), Fixed);
    image.setImageSlices(box);

    // Set our fill mode.
    image.setFill(borderImageSlice->m_fill);
}

static BorderImageLength toBorderImageLength(CSSPrimitiveValue& value, const CSSToLengthConversionData& conversionData)
{
    if (value.isNumber())
        return value.getDoubleValue();
    if (value.isPercentage())
        return Length(value.getDoubleValue(CSSPrimitiveValue::CSS_PERCENTAGE), Percent);
    if (value.getValueID() != CSSValueAuto)
        return value.computeLength<Length>(conversionData);
    return Length(Auto);
}

BorderImageLengthBox CSSToStyleMap::mapNinePieceImageQuad(CSSValue* value) const
{
    if (!value || !value->isPrimitiveValue())
        return BorderImageLengthBox(Length(Auto));

    float zoom = useSVGZoomRules() ? 1.0f : cssToLengthConversionData().zoom();
    Quad* slices = toCSSPrimitiveValue(value)->getQuadValue();

    // Set up a border image length box to represent our image slices.
    const CSSToLengthConversionData& conversionData = cssToLengthConversionData().copyWithAdjustedZoom(zoom);
    return BorderImageLengthBox(
        toBorderImageLength(*slices->top(), conversionData),
        toBorderImageLength(*slices->right(), conversionData),
        toBorderImageLength(*slices->bottom(), conversionData),
        toBorderImageLength(*slices->left(), conversionData));
}

void CSSToStyleMap::mapNinePieceImageRepeat(CSSValue* value, NinePieceImage& image) const
{
    if (!value || !value->isPrimitiveValue())
        return;

    CSSPrimitiveValue* primitiveValue = toCSSPrimitiveValue(value);
    Pair* pair = primitiveValue->getPairValue();
    if (!pair || !pair->first() || !pair->second())
        return;

    CSSValueID firstIdentifier = pair->first()->getValueID();
    CSSValueID secondIdentifier = pair->second()->getValueID();

    ENinePieceImageRule horizontalRule;
    switch (firstIdentifier) {
    case CSSValueStretch:
        horizontalRule = StretchImageRule;
        break;
    case CSSValueRound:
        horizontalRule = RoundImageRule;
        break;
    case CSSValueSpace:
        horizontalRule = SpaceImageRule;
        break;
    default: // CSSValueRepeat
        horizontalRule = RepeatImageRule;
        break;
    }
    image.setHorizontalRule(horizontalRule);

    ENinePieceImageRule verticalRule;
    switch (secondIdentifier) {
    case CSSValueStretch:
        verticalRule = StretchImageRule;
        break;
    case CSSValueRound:
        verticalRule = RoundImageRule;
        break;
    case CSSValueSpace:
        verticalRule = SpaceImageRule;
        break;
    default: // CSSValueRepeat
        verticalRule = RepeatImageRule;
        break;
    }
    image.setVerticalRule(verticalRule);
}

};
