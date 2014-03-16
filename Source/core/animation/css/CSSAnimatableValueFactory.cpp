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
#include "core/animation/css/CSSAnimatableValueFactory.h"

#include "CSSValueKeywords.h"
#include "core/animation/AnimatableClipPathOperation.h"
#include "core/animation/AnimatableColor.h"
#include "core/animation/AnimatableDouble.h"
#include "core/animation/AnimatableFilterOperations.h"
#include "core/animation/AnimatableImage.h"
#include "core/animation/AnimatableLength.h"
#include "core/animation/AnimatableLengthBox.h"
#include "core/animation/AnimatableLengthBoxAndBool.h"
#include "core/animation/AnimatableLengthPoint.h"
#include "core/animation/AnimatableLengthSize.h"
#include "core/animation/AnimatableRepeatable.h"
#include "core/animation/AnimatableSVGLength.h"
#include "core/animation/AnimatableSVGPaint.h"
#include "core/animation/AnimatableShadow.h"
#include "core/animation/AnimatableShapeValue.h"
#include "core/animation/AnimatableStrokeDasharrayList.h"
#include "core/animation/AnimatableTransform.h"
#include "core/animation/AnimatableUnknown.h"
#include "core/animation/AnimatableVisibility.h"
#include "core/animation/css/CSSAnimations.h"
#include "core/css/CSSCalculationValue.h"
#include "core/css/CSSPrimitiveValue.h"
#include "core/css/CSSPrimitiveValueMappings.h"
#include "core/rendering/style/RenderStyle.h"
#include "platform/Length.h"
#include "platform/LengthBox.h"

namespace WebCore {

static PassRefPtr<AnimatableValue> createFromLength(const Length& length, const RenderStyle& style)
{
    switch (length.type()) {
    case Fixed:
        return AnimatableLength::create(adjustFloatForAbsoluteZoom(length.value(), style), AnimatableLength::UnitTypePixels);
    case Percent:
        return AnimatableLength::create(length.value(), AnimatableLength::UnitTypePercentage);
    case Calculated:
        return AnimatableLength::create(CSSCalcValue::createExpressionNode(length.calculationValue()->expression(), style.effectiveZoom()));
    case Auto:
    case Intrinsic:
    case MinIntrinsic:
    case MinContent:
    case MaxContent:
    case FillAvailable:
    case FitContent:
        return AnimatableUnknown::create(CSSPrimitiveValue::create(length));
    case Undefined:
        return AnimatableUnknown::create(CSSValueNone);
    case ExtendToZoom: // Does not apply to elements.
    case DeviceWidth:
    case DeviceHeight:
        ASSERT_NOT_REACHED();
        return nullptr;
    }
    ASSERT_NOT_REACHED();
    return nullptr;
}

static PassRefPtr<AnimatableValue> createFromLineHeight(const Length& length, const RenderStyle& style)
{
    if (length.type() == Percent) {
        double value = length.value();
        // -100% is used to represent "normal" line height.
        if (value == -100)
            return AnimatableUnknown::create(CSSValueNormal);
        return AnimatableDouble::create(value);
    }
    return createFromLength(length, style);
}

inline static PassRefPtr<AnimatableValue> createFromDouble(double value, AnimatableDouble::Constraint constraint = AnimatableDouble::Unconstrained)
{
    return AnimatableDouble::create(value, constraint);
}

inline static PassRefPtr<AnimatableValue> createFromLengthBox(const LengthBox& lengthBox, const RenderStyle& style)
{
    return AnimatableLengthBox::create(
        createFromLength(lengthBox.left(), style),
        createFromLength(lengthBox.right(), style),
        createFromLength(lengthBox.top(), style),
        createFromLength(lengthBox.bottom(), style));
}

static PassRefPtr<AnimatableValue> createFromBorderImageLength(const BorderImageLength& borderImageLength, const RenderStyle& style)
{
    if (borderImageLength.isNumber())
        return createFromDouble(borderImageLength.number());
    return createFromLength(borderImageLength.length(), style);
}

inline static PassRefPtr<AnimatableValue> createFromBorderImageLengthBox(const BorderImageLengthBox& borderImageLengthBox, const RenderStyle& style)
{
    return AnimatableLengthBox::create(
        createFromBorderImageLength(borderImageLengthBox.left(), style),
        createFromBorderImageLength(borderImageLengthBox.right(), style),
        createFromBorderImageLength(borderImageLengthBox.top(), style),
        createFromBorderImageLength(borderImageLengthBox.bottom(), style));
}

inline static PassRefPtr<AnimatableValue> createFromLengthBoxAndBool(const LengthBox lengthBox, const bool flag, const RenderStyle& style)
{
    return AnimatableLengthBoxAndBool::create(
        createFromLengthBox(lengthBox, style),
        flag);
}

inline static PassRefPtr<AnimatableValue> createFromLengthPoint(const LengthPoint& lengthPoint, const RenderStyle& style)
{
    return AnimatableLengthPoint::create(
        createFromLength(lengthPoint.x(), style),
        createFromLength(lengthPoint.y(), style));
}

inline static PassRefPtr<AnimatableValue> createFromLengthSize(const LengthSize& lengthSize, const RenderStyle& style)
{
    return AnimatableLengthSize::create(
        createFromLength(lengthSize.width(), style),
        createFromLength(lengthSize.height(), style));
}

inline static PassRefPtr<AnimatableValue> createFromStyleImage(StyleImage* image)
{
    if (image)
        return AnimatableImage::create(image);
    return AnimatableUnknown::create(CSSValueNone);
}

inline static PassRefPtr<AnimatableValue> createFromFillSize(const FillSize& fillSize, const RenderStyle& style)
{
    switch (fillSize.type) {
    case SizeLength:
        return createFromLengthSize(fillSize.size, style);
    case Contain:
    case Cover:
    case SizeNone:
        return AnimatableUnknown::create(CSSPrimitiveValue::create(fillSize.type));
    default:
        ASSERT_NOT_REACHED();
        return nullptr;
    }
}

inline static PassRefPtr<AnimatableValue> createFromBackgroundPosition(const Length& length, bool originIsSet, BackgroundEdgeOrigin origin, const RenderStyle& style)
{
    if (!originIsSet || origin == LeftEdge || origin == TopEdge)
        return createFromLength(length, style);

    return AnimatableLength::create(CSSCalcValue::createExpressionNode(
        CSSCalcValue::createExpressionNode(CSSPrimitiveValue::create(100, CSSPrimitiveValue::CSS_PERCENTAGE), true),
        CSSCalcValue::createExpressionNode(length, style.effectiveZoom()),
        CalcSubtract));
}

template<CSSPropertyID property>
inline static PassRefPtr<AnimatableValue> createFromFillLayers(const FillLayer* fillLayer, const RenderStyle& style)
{
    ASSERT(fillLayer);
    Vector<RefPtr<AnimatableValue> > values;
    while (fillLayer) {
        if (property == CSSPropertyBackgroundImage || property == CSSPropertyWebkitMaskImage) {
            if (!fillLayer->isImageSet())
                break;
            values.append(createFromStyleImage(fillLayer->image()));
        } else if (property == CSSPropertyBackgroundPositionX || property == CSSPropertyWebkitMaskPositionX) {
            if (!fillLayer->isXPositionSet())
                break;
            values.append(createFromBackgroundPosition(fillLayer->xPosition(), fillLayer->isBackgroundXOriginSet(), fillLayer->backgroundXOrigin(), style));
        } else if (property == CSSPropertyBackgroundPositionY || property == CSSPropertyWebkitMaskPositionY) {
            if (!fillLayer->isYPositionSet())
                break;
            values.append(createFromBackgroundPosition(fillLayer->yPosition(), fillLayer->isBackgroundYOriginSet(), fillLayer->backgroundYOrigin(), style));
        } else if (property == CSSPropertyBackgroundSize || property == CSSPropertyWebkitMaskSize) {
            if (!fillLayer->isSizeSet())
                break;
            values.append(createFromFillSize(fillLayer->size(), style));
        } else {
            ASSERT_NOT_REACHED();
        }
        fillLayer = fillLayer->next();
    }
    return AnimatableRepeatable::create(values);
}

PassRefPtr<AnimatableValue> CSSAnimatableValueFactory::createFromColor(CSSPropertyID property, const RenderStyle& style)
{
    Color color = style.colorIncludingFallback(property, false);
    Color visitedLinkColor = style.colorIncludingFallback(property, true);
    return AnimatableColor::create(color, visitedLinkColor);
}

inline static PassRefPtr<AnimatableValue> createFromShapeValue(ShapeValue* value)
{
    if (value)
        return AnimatableShapeValue::create(value);
    return AnimatableUnknown::create(CSSValueAuto);
}

static double fontWeightToDouble(FontWeight fontWeight)
{
    switch (fontWeight) {
    case FontWeight100:
        return 100;
    case FontWeight200:
        return 200;
    case FontWeight300:
        return 300;
    case FontWeight400:
        return 400;
    case FontWeight500:
        return 500;
    case FontWeight600:
        return 600;
    case FontWeight700:
        return 700;
    case FontWeight800:
        return 800;
    case FontWeight900:
        return 900;
    }

    ASSERT_NOT_REACHED();
    return 400;
}

static PassRefPtr<AnimatableValue> createFromFontWeight(FontWeight fontWeight)
{
    return createFromDouble(fontWeightToDouble(fontWeight));
}

// FIXME: Generate this function.
PassRefPtr<AnimatableValue> CSSAnimatableValueFactory::create(CSSPropertyID property, const RenderStyle& style)
{
    ASSERT(CSSAnimations::isAnimatableProperty(property));
    switch (property) {
    case CSSPropertyBackgroundColor:
        return createFromColor(property, style);
    case CSSPropertyBackgroundImage:
        return createFromFillLayers<CSSPropertyBackgroundImage>(style.backgroundLayers(), style);
    case CSSPropertyBackgroundPositionX:
        return createFromFillLayers<CSSPropertyBackgroundPositionX>(style.backgroundLayers(), style);
    case CSSPropertyBackgroundPositionY:
        return createFromFillLayers<CSSPropertyBackgroundPositionY>(style.backgroundLayers(), style);
    case CSSPropertyBackgroundSize:
    case CSSPropertyWebkitBackgroundSize:
        return createFromFillLayers<CSSPropertyBackgroundSize>(style.backgroundLayers(), style);
    case CSSPropertyBaselineShift:
        return AnimatableSVGLength::create(style.baselineShiftValue());
    case CSSPropertyBorderBottomColor:
        return createFromColor(property, style);
    case CSSPropertyBorderBottomLeftRadius:
        return createFromLengthSize(style.borderBottomLeftRadius(), style);
    case CSSPropertyBorderBottomRightRadius:
        return createFromLengthSize(style.borderBottomRightRadius(), style);
    case CSSPropertyBorderBottomWidth:
        return createFromDouble(style.borderBottomWidth());
    case CSSPropertyBorderImageOutset:
        return createFromBorderImageLengthBox(style.borderImageOutset(), style);
    case CSSPropertyBorderImageSlice:
        return createFromLengthBox(style.borderImageSlices(), style);
    case CSSPropertyBorderImageSource:
        return createFromStyleImage(style.borderImageSource());
    case CSSPropertyBorderImageWidth:
        return createFromBorderImageLengthBox(style.borderImageWidth(), style);
    case CSSPropertyBorderLeftColor:
        return createFromColor(property, style);
    case CSSPropertyBorderLeftWidth:
        return createFromDouble(style.borderLeftWidth());
    case CSSPropertyBorderRightColor:
        return createFromColor(property, style);
    case CSSPropertyBorderRightWidth:
        return createFromDouble(style.borderRightWidth());
    case CSSPropertyBorderTopColor:
        return createFromColor(property, style);
    case CSSPropertyBorderTopLeftRadius:
        return createFromLengthSize(style.borderTopLeftRadius(), style);
    case CSSPropertyBorderTopRightRadius:
        return createFromLengthSize(style.borderTopRightRadius(), style);
    case CSSPropertyBorderTopWidth:
        return createFromDouble(style.borderTopWidth());
    case CSSPropertyBottom:
        return createFromLength(style.bottom(), style);
    case CSSPropertyBoxShadow:
    case CSSPropertyWebkitBoxShadow:
        return AnimatableShadow::create(style.boxShadow());
    case CSSPropertyClip:
        if (style.hasClip())
            return createFromLengthBox(style.clip(), style);
        return AnimatableUnknown::create(CSSPrimitiveValue::create(CSSValueAuto));
    case CSSPropertyColor:
        return createFromColor(property, style);
    case CSSPropertyFillOpacity:
        return createFromDouble(style.fillOpacity());
    case CSSPropertyFill:
        return AnimatableSVGPaint::create(style.svgStyle()->fillPaintType(), style.svgStyle()->fillPaintColor(), style.svgStyle()->fillPaintUri());
    case CSSPropertyFlexGrow:
        return createFromDouble(style.flexGrow(), AnimatableDouble::InterpolationIsNonContinuousWithZero);
    case CSSPropertyFlexShrink:
        return createFromDouble(style.flexShrink(), AnimatableDouble::InterpolationIsNonContinuousWithZero);
    case CSSPropertyFlexBasis:
        return createFromLength(style.flexBasis(), style);
    case CSSPropertyFloodColor:
        return createFromColor(property, style);
    case CSSPropertyFloodOpacity:
        return createFromDouble(style.floodOpacity());
    case CSSPropertyFontSize:
        // Must pass a specified size to setFontSize if Text Autosizing is enabled, but a computed size
        // if text zoom is enabled (if neither is enabled it's irrelevant as they're probably the same).
        // FIXME: Should we introduce an option to pass the computed font size here, allowing consumers to
        // enable text zoom rather than Text Autosizing? See http://crbug.com/227545.
        return createFromDouble(style.specifiedFontSize());
    case CSSPropertyFontWeight:
        return createFromFontWeight(style.fontWeight());
    case CSSPropertyHeight:
        return createFromLength(style.height(), style);
    case CSSPropertyKerning:
        return AnimatableSVGLength::create(style.kerning());
    case CSSPropertyLightingColor:
        return createFromColor(property, style);
    case CSSPropertyListStyleImage:
        return createFromStyleImage(style.listStyleImage());
    case CSSPropertyLeft:
        return createFromLength(style.left(), style);
    case CSSPropertyLetterSpacing:
        return createFromDouble(style.letterSpacing());
    case CSSPropertyLineHeight:
        return createFromLineHeight(style.specifiedLineHeight(), style);
    case CSSPropertyMarginBottom:
        return createFromLength(style.marginBottom(), style);
    case CSSPropertyMarginLeft:
        return createFromLength(style.marginLeft(), style);
    case CSSPropertyMarginRight:
        return createFromLength(style.marginRight(), style);
    case CSSPropertyMarginTop:
        return createFromLength(style.marginTop(), style);
    case CSSPropertyMaxHeight:
        return createFromLength(style.maxHeight(), style);
    case CSSPropertyMaxWidth:
        return createFromLength(style.maxWidth(), style);
    case CSSPropertyMinHeight:
        return createFromLength(style.minHeight(), style);
    case CSSPropertyMinWidth:
        return createFromLength(style.minWidth(), style);
    case CSSPropertyObjectPosition:
        return createFromLengthPoint(style.objectPosition(), style);
    case CSSPropertyOpacity:
        return createFromDouble(style.opacity());
    case CSSPropertyOrphans:
        return createFromDouble(style.orphans());
    case CSSPropertyOutlineColor:
        return createFromColor(property, style);
    case CSSPropertyOutlineOffset:
        return createFromDouble(style.outlineOffset());
    case CSSPropertyOutlineWidth:
        return createFromDouble(style.outlineWidth());
    case CSSPropertyPaddingBottom:
        return createFromLength(style.paddingBottom(), style);
    case CSSPropertyPaddingLeft:
        return createFromLength(style.paddingLeft(), style);
    case CSSPropertyPaddingRight:
        return createFromLength(style.paddingRight(), style);
    case CSSPropertyPaddingTop:
        return createFromLength(style.paddingTop(), style);
    case CSSPropertyRight:
        return createFromLength(style.right(), style);
    case CSSPropertyStrokeWidth:
        return AnimatableSVGLength::create(style.strokeWidth());
    case CSSPropertyStopColor:
        return createFromColor(property, style);
    case CSSPropertyStopOpacity:
        return createFromDouble(style.stopOpacity());
    case CSSPropertyStrokeDasharray:
        return AnimatableStrokeDasharrayList::create(style.strokeDashArray());
    case CSSPropertyStrokeDashoffset:
        return AnimatableSVGLength::create(style.strokeDashOffset());
    case CSSPropertyStrokeMiterlimit:
        return createFromDouble(style.strokeMiterLimit());
    case CSSPropertyStrokeOpacity:
        return createFromDouble(style.strokeOpacity());
    case CSSPropertyStroke:
        return AnimatableSVGPaint::create(style.svgStyle()->strokePaintType(), style.svgStyle()->strokePaintColor(), style.svgStyle()->strokePaintUri());
    case CSSPropertyTextDecorationColor:
        return AnimatableColor::create(style.textDecorationColor().resolve(style.color()), style.visitedLinkTextDecorationColor().resolve(style.visitedLinkColor()));
    case CSSPropertyTextIndent:
        return createFromLength(style.textIndent(), style);
    case CSSPropertyTextShadow:
        return AnimatableShadow::create(style.textShadow());
    case CSSPropertyTop:
        return createFromLength(style.top(), style);
    case CSSPropertyWebkitBorderHorizontalSpacing:
        return createFromDouble(style.horizontalBorderSpacing());
    case CSSPropertyWebkitBorderVerticalSpacing:
        return createFromDouble(style.verticalBorderSpacing());
    case CSSPropertyWebkitClipPath:
        if (ClipPathOperation* operation = style.clipPath())
            return AnimatableClipPathOperation::create(operation);
        return AnimatableUnknown::create(CSSValueNone);
    case CSSPropertyWebkitColumnCount:
        return createFromDouble(style.columnCount());
    case CSSPropertyWebkitColumnGap:
        return createFromDouble(style.columnGap());
    case CSSPropertyWebkitColumnRuleColor:
        return createFromColor(property, style);
    case CSSPropertyWebkitColumnRuleWidth:
        return createFromDouble(style.columnRuleWidth());
    case CSSPropertyWebkitColumnWidth:
        return createFromDouble(style.columnWidth());
    case CSSPropertyWebkitFilter:
        return AnimatableFilterOperations::create(style.filter());
    case CSSPropertyWebkitMaskBoxImageOutset:
        return createFromBorderImageLengthBox(style.maskBoxImageOutset(), style);
    case CSSPropertyWebkitMaskBoxImageSlice:
        return createFromLengthBoxAndBool(style.maskBoxImageSlices(), style.maskBoxImageSlicesFill(), style);
    case CSSPropertyWebkitMaskBoxImageSource:
        return createFromStyleImage(style.maskBoxImageSource());
    case CSSPropertyWebkitMaskBoxImageWidth:
        return createFromBorderImageLengthBox(style.maskBoxImageWidth(), style);
    case CSSPropertyWebkitMaskImage:
        return createFromFillLayers<CSSPropertyWebkitMaskImage>(style.maskLayers(), style);
    case CSSPropertyWebkitMaskPositionX:
        return createFromFillLayers<CSSPropertyWebkitMaskPositionX>(style.maskLayers(), style);
    case CSSPropertyWebkitMaskPositionY:
        return createFromFillLayers<CSSPropertyWebkitMaskPositionY>(style.maskLayers(), style);
    case CSSPropertyWebkitMaskSize:
        return createFromFillLayers<CSSPropertyWebkitMaskSize>(style.maskLayers(), style);
    case CSSPropertyWebkitPerspective:
        return createFromDouble(style.perspective());
    case CSSPropertyWebkitPerspectiveOriginX:
        return createFromLength(style.perspectiveOriginX(), style);
    case CSSPropertyWebkitPerspectiveOriginY:
        return createFromLength(style.perspectiveOriginY(), style);
    case CSSPropertyShapeInside:
        return createFromShapeValue(style.shapeInside());
    case CSSPropertyShapeOutside:
        return createFromShapeValue(style.shapeOutside());
    case CSSPropertyShapeMargin:
        return createFromLength(style.shapeMargin(), style);
    case CSSPropertyShapeImageThreshold:
        return createFromDouble(style.shapeImageThreshold());
    case CSSPropertyWebkitTextStrokeColor:
        return createFromColor(property, style);
    case CSSPropertyWebkitTransform:
        return AnimatableTransform::create(style.transform());
    case CSSPropertyWebkitTransformOriginX:
        return createFromLength(style.transformOriginX(), style);
    case CSSPropertyWebkitTransformOriginY:
        return createFromLength(style.transformOriginY(), style);
    case CSSPropertyWebkitTransformOriginZ:
        return createFromDouble(style.transformOriginZ());
    case CSSPropertyWidows:
        return createFromDouble(style.widows());
    case CSSPropertyWidth:
        return createFromLength(style.width(), style);
    case CSSPropertyWordSpacing:
        return createFromDouble(style.wordSpacing());
    case CSSPropertyVisibility:
        return AnimatableVisibility::create(style.visibility());
    case CSSPropertyZIndex:
        return createFromDouble(style.zIndex());
    case CSSPropertyZoom:
        return createFromDouble(style.zoom());
    default:
        ASSERT_NOT_REACHED();
        // This return value is to avoid a release crash if possible.
        return AnimatableUnknown::create(nullptr);
    }
}

} // namespace WebCore
