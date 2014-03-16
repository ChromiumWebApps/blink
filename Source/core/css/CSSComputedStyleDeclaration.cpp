/*
 * Copyright (C) 2004 Zack Rusin <zack@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011, 2012 Apple Inc. All rights reserved.
 * Copyright (C) 2007 Alexey Proskuryakov <ap@webkit.org>
 * Copyright (C) 2007 Nicholas Shanks <webkit@nickshanks.com>
 * Copyright (C) 2011 Sencha, Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301  USA
 */

#include "config.h"
#include "core/css/CSSComputedStyleDeclaration.h"

#include "CSSPropertyNames.h"
#include "FontFamilyNames.h"
#include "RuntimeEnabledFeatures.h"
#include "StylePropertyShorthand.h"
#include "bindings/v8/ExceptionState.h"
#include "core/animation/DocumentAnimations.h"
#include "core/css/BasicShapeFunctions.h"
#include "core/css/CSSArrayFunctionValue.h"
#include "core/css/CSSAspectRatioValue.h"
#include "core/css/CSSBorderImage.h"
#include "core/css/CSSFilterValue.h"
#include "core/css/CSSFontFeatureValue.h"
#include "core/css/CSSFontValue.h"
#include "core/css/CSSFunctionValue.h"
#include "core/css/CSSGridLineNamesValue.h"
#include "core/css/CSSGridTemplateAreasValue.h"
#include "core/css/CSSLineBoxContainValue.h"
#include "core/css/parser/BisonCSSParser.h"
#include "core/css/CSSPrimitiveValue.h"
#include "core/css/CSSPrimitiveValueMappings.h"
#include "core/css/CSSReflectValue.h"
#include "core/css/CSSSelector.h"
#include "core/css/CSSShadowValue.h"
#include "core/css/CSSTimingFunctionValue.h"
#include "core/css/CSSTransformValue.h"
#include "core/css/CSSValueList.h"
#include "core/css/CSSValuePool.h"
#include "core/css/Pair.h"
#include "core/css/Rect.h"
#include "core/css/RuntimeCSSEnabled.h"
#include "core/css/StylePropertySet.h"
#include "core/css/resolver/StyleResolver.h"
#include "core/dom/Document.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/PseudoElement.h"
#include "core/rendering/RenderBox.h"
#include "core/rendering/RenderGrid.h"
#include "core/rendering/style/ContentData.h"
#include "core/rendering/style/CounterContent.h"
#include "core/rendering/style/CursorList.h"
#include "core/rendering/style/RenderStyle.h"
#include "core/rendering/style/ShadowList.h"
#include "core/rendering/style/ShapeValue.h"
#include "platform/fonts/FontFeatureSettings.h"
#include "wtf/text/StringBuilder.h"

namespace WebCore {

// List of all properties we know how to compute, omitting shorthands.
// NOTE: Do not use this list, use computableProperties() instead
// to respect runtime enabling of CSS properties.
static const CSSPropertyID staticComputableProperties[] = {
    CSSPropertyAnimationDelay,
    CSSPropertyAnimationDirection,
    CSSPropertyAnimationDuration,
    CSSPropertyAnimationFillMode,
    CSSPropertyAnimationIterationCount,
    CSSPropertyAnimationName,
    CSSPropertyAnimationPlayState,
    CSSPropertyAnimationTimingFunction,
    CSSPropertyBackgroundAttachment,
    CSSPropertyBackgroundBlendMode,
    CSSPropertyBackgroundClip,
    CSSPropertyBackgroundColor,
    CSSPropertyBackgroundImage,
    CSSPropertyBackgroundOrigin,
    CSSPropertyBackgroundPosition, // more-specific background-position-x/y are non-standard
    CSSPropertyBackgroundRepeat,
    CSSPropertyBackgroundSize,
    CSSPropertyBorderBottomColor,
    CSSPropertyBorderBottomLeftRadius,
    CSSPropertyBorderBottomRightRadius,
    CSSPropertyBorderBottomStyle,
    CSSPropertyBorderBottomWidth,
    CSSPropertyBorderCollapse,
    CSSPropertyBorderImageOutset,
    CSSPropertyBorderImageRepeat,
    CSSPropertyBorderImageSlice,
    CSSPropertyBorderImageSource,
    CSSPropertyBorderImageWidth,
    CSSPropertyBorderLeftColor,
    CSSPropertyBorderLeftStyle,
    CSSPropertyBorderLeftWidth,
    CSSPropertyBorderRightColor,
    CSSPropertyBorderRightStyle,
    CSSPropertyBorderRightWidth,
    CSSPropertyBorderTopColor,
    CSSPropertyBorderTopLeftRadius,
    CSSPropertyBorderTopRightRadius,
    CSSPropertyBorderTopStyle,
    CSSPropertyBorderTopWidth,
    CSSPropertyBottom,
    CSSPropertyBoxShadow,
    CSSPropertyBoxSizing,
    CSSPropertyCaptionSide,
    CSSPropertyClear,
    CSSPropertyClip,
    CSSPropertyColor,
    CSSPropertyCursor,
    CSSPropertyDirection,
    CSSPropertyDisplay,
    CSSPropertyEmptyCells,
    CSSPropertyFloat,
    CSSPropertyFontFamily,
    CSSPropertyFontKerning,
    CSSPropertyFontSize,
    CSSPropertyFontStyle,
    CSSPropertyFontVariant,
    CSSPropertyFontVariantLigatures,
    CSSPropertyFontWeight,
    CSSPropertyHeight,
    CSSPropertyImageRendering,
    CSSPropertyIsolation,
    CSSPropertyJustifySelf,
    CSSPropertyLeft,
    CSSPropertyLetterSpacing,
    CSSPropertyLineHeight,
    CSSPropertyListStyleImage,
    CSSPropertyListStylePosition,
    CSSPropertyListStyleType,
    CSSPropertyMarginBottom,
    CSSPropertyMarginLeft,
    CSSPropertyMarginRight,
    CSSPropertyMarginTop,
    CSSPropertyMaxHeight,
    CSSPropertyMaxWidth,
    CSSPropertyMinHeight,
    CSSPropertyMinWidth,
    CSSPropertyMixBlendMode,
    CSSPropertyObjectFit,
    CSSPropertyObjectPosition,
    CSSPropertyOpacity,
    CSSPropertyOrphans,
    CSSPropertyOutlineColor,
    CSSPropertyOutlineOffset,
    CSSPropertyOutlineStyle,
    CSSPropertyOutlineWidth,
    CSSPropertyOverflowWrap,
    CSSPropertyOverflowX,
    CSSPropertyOverflowY,
    CSSPropertyPaddingBottom,
    CSSPropertyPaddingLeft,
    CSSPropertyPaddingRight,
    CSSPropertyPaddingTop,
    CSSPropertyPageBreakAfter,
    CSSPropertyPageBreakBefore,
    CSSPropertyPageBreakInside,
    CSSPropertyPointerEvents,
    CSSPropertyPosition,
    CSSPropertyResize,
    CSSPropertyRight,
    CSSPropertyScrollBehavior,
    CSSPropertySpeak,
    CSSPropertyTableLayout,
    CSSPropertyTabSize,
    CSSPropertyTextAlign,
    CSSPropertyTextAlignLast,
    CSSPropertyTextDecoration,
    CSSPropertyTextDecorationLine,
    CSSPropertyTextDecorationStyle,
    CSSPropertyTextDecorationColor,
    CSSPropertyTextJustify,
    CSSPropertyTextUnderlinePosition,
    CSSPropertyTextIndent,
    CSSPropertyTextRendering,
    CSSPropertyTextShadow,
    CSSPropertyTextOverflow,
    CSSPropertyTextTransform,
    CSSPropertyTop,
    CSSPropertyTouchAction,
    CSSPropertyTouchActionDelay,
    CSSPropertyTransitionDelay,
    CSSPropertyTransitionDuration,
    CSSPropertyTransitionProperty,
    CSSPropertyTransitionTimingFunction,
    CSSPropertyUnicodeBidi,
    CSSPropertyVerticalAlign,
    CSSPropertyVisibility,
    CSSPropertyWhiteSpace,
    CSSPropertyWidows,
    CSSPropertyWidth,
    CSSPropertyWillChange,
    CSSPropertyWordBreak,
    CSSPropertyWordSpacing,
    CSSPropertyWordWrap,
    CSSPropertyZIndex,
    CSSPropertyZoom,

    CSSPropertyWebkitAnimationDelay,
    CSSPropertyWebkitAnimationDirection,
    CSSPropertyWebkitAnimationDuration,
    CSSPropertyWebkitAnimationFillMode,
    CSSPropertyWebkitAnimationIterationCount,
    CSSPropertyWebkitAnimationName,
    CSSPropertyWebkitAnimationPlayState,
    CSSPropertyWebkitAnimationTimingFunction,
    CSSPropertyWebkitAppearance,
    CSSPropertyWebkitBackfaceVisibility,
    CSSPropertyWebkitBackgroundClip,
    CSSPropertyWebkitBackgroundComposite,
    CSSPropertyWebkitBackgroundOrigin,
    CSSPropertyWebkitBackgroundSize,
    CSSPropertyWebkitBorderFit,
    CSSPropertyWebkitBorderHorizontalSpacing,
    CSSPropertyWebkitBorderImage,
    CSSPropertyWebkitBorderVerticalSpacing,
    CSSPropertyWebkitBoxAlign,
    CSSPropertyWebkitBoxDecorationBreak,
    CSSPropertyWebkitBoxDirection,
    CSSPropertyWebkitBoxFlex,
    CSSPropertyWebkitBoxFlexGroup,
    CSSPropertyWebkitBoxLines,
    CSSPropertyWebkitBoxOrdinalGroup,
    CSSPropertyWebkitBoxOrient,
    CSSPropertyWebkitBoxPack,
    CSSPropertyWebkitBoxReflect,
    CSSPropertyWebkitBoxShadow,
    CSSPropertyWebkitClipPath,
    CSSPropertyWebkitColumnBreakAfter,
    CSSPropertyWebkitColumnBreakBefore,
    CSSPropertyWebkitColumnBreakInside,
    CSSPropertyWebkitColumnAxis,
    CSSPropertyWebkitColumnCount,
    CSSPropertyWebkitColumnGap,
    CSSPropertyWebkitColumnProgression,
    CSSPropertyWebkitColumnRuleColor,
    CSSPropertyWebkitColumnRuleStyle,
    CSSPropertyWebkitColumnRuleWidth,
    CSSPropertyWebkitColumnSpan,
    CSSPropertyWebkitColumnWidth,
    CSSPropertyWebkitFilter,
    CSSPropertyAlignContent,
    CSSPropertyAlignItems,
    CSSPropertyAlignSelf,
    CSSPropertyFlexBasis,
    CSSPropertyFlexGrow,
    CSSPropertyFlexShrink,
    CSSPropertyFlexDirection,
    CSSPropertyFlexWrap,
    CSSPropertyJustifyContent,
    CSSPropertyWebkitFontSmoothing,
    CSSPropertyGridAutoColumns,
    CSSPropertyGridAutoFlow,
    CSSPropertyGridAutoRows,
    CSSPropertyGridColumnEnd,
    CSSPropertyGridColumnStart,
    CSSPropertyGridTemplateColumns,
    CSSPropertyGridTemplateRows,
    CSSPropertyGridRowEnd,
    CSSPropertyGridRowStart,
    CSSPropertyWebkitHighlight,
    CSSPropertyWebkitHyphenateCharacter,
    CSSPropertyWebkitLineBoxContain,
    CSSPropertyWebkitLineBreak,
    CSSPropertyWebkitLineClamp,
    CSSPropertyWebkitLocale,
    CSSPropertyWebkitMarginBeforeCollapse,
    CSSPropertyWebkitMarginAfterCollapse,
    CSSPropertyWebkitMaskBoxImage,
    CSSPropertyWebkitMaskBoxImageOutset,
    CSSPropertyWebkitMaskBoxImageRepeat,
    CSSPropertyWebkitMaskBoxImageSlice,
    CSSPropertyWebkitMaskBoxImageSource,
    CSSPropertyWebkitMaskBoxImageWidth,
    CSSPropertyWebkitMaskClip,
    CSSPropertyWebkitMaskComposite,
    CSSPropertyWebkitMaskImage,
    CSSPropertyWebkitMaskOrigin,
    CSSPropertyWebkitMaskPosition,
    CSSPropertyWebkitMaskRepeat,
    CSSPropertyWebkitMaskSize,
    CSSPropertyOrder,
    CSSPropertyWebkitPerspective,
    CSSPropertyWebkitPerspectiveOrigin,
    CSSPropertyWebkitPrintColorAdjust,
    CSSPropertyWebkitRtlOrdering,
    CSSPropertyShapeInside,
    CSSPropertyShapeOutside,
    CSSPropertyShapePadding,
    CSSPropertyShapeImageThreshold,
    CSSPropertyShapeMargin,
    CSSPropertyWebkitTapHighlightColor,
    CSSPropertyWebkitTextCombine,
    CSSPropertyWebkitTextDecorationsInEffect,
    CSSPropertyWebkitTextEmphasisColor,
    CSSPropertyWebkitTextEmphasisPosition,
    CSSPropertyWebkitTextEmphasisStyle,
    CSSPropertyWebkitTextFillColor,
    CSSPropertyWebkitTextOrientation,
    CSSPropertyWebkitTextSecurity,
    CSSPropertyWebkitTextStrokeColor,
    CSSPropertyWebkitTextStrokeWidth,
    CSSPropertyWebkitTransform,
    CSSPropertyWebkitTransformOrigin,
    CSSPropertyWebkitTransformStyle,
    CSSPropertyWebkitTransitionDelay,
    CSSPropertyWebkitTransitionDuration,
    CSSPropertyWebkitTransitionProperty,
    CSSPropertyWebkitTransitionTimingFunction,
    CSSPropertyWebkitUserDrag,
    CSSPropertyWebkitUserModify,
    CSSPropertyWebkitUserSelect,
    CSSPropertyWebkitWritingMode,
    CSSPropertyWebkitAppRegion,
    CSSPropertyWebkitWrapFlow,
    CSSPropertyWebkitWrapThrough,
    CSSPropertyBufferedRendering,
    CSSPropertyClipPath,
    CSSPropertyClipRule,
    CSSPropertyMask,
    CSSPropertyFilter,
    CSSPropertyFloodColor,
    CSSPropertyFloodOpacity,
    CSSPropertyLightingColor,
    CSSPropertyStopColor,
    CSSPropertyStopOpacity,
    CSSPropertyColorInterpolation,
    CSSPropertyColorInterpolationFilters,
    CSSPropertyColorRendering,
    CSSPropertyFill,
    CSSPropertyFillOpacity,
    CSSPropertyFillRule,
    CSSPropertyMarkerEnd,
    CSSPropertyMarkerMid,
    CSSPropertyMarkerStart,
    CSSPropertyMaskType,
    CSSPropertyMaskSourceType,
    CSSPropertyShapeRendering,
    CSSPropertyStroke,
    CSSPropertyStrokeDasharray,
    CSSPropertyStrokeDashoffset,
    CSSPropertyStrokeLinecap,
    CSSPropertyStrokeLinejoin,
    CSSPropertyStrokeMiterlimit,
    CSSPropertyStrokeOpacity,
    CSSPropertyStrokeWidth,
    CSSPropertyAlignmentBaseline,
    CSSPropertyBaselineShift,
    CSSPropertyDominantBaseline,
    CSSPropertyKerning,
    CSSPropertyTextAnchor,
    CSSPropertyWritingMode,
    CSSPropertyGlyphOrientationHorizontal,
    CSSPropertyGlyphOrientationVertical,
    CSSPropertyVectorEffect,
    CSSPropertyPaintOrder
};

static const Vector<CSSPropertyID>& computableProperties()
{
    DEFINE_STATIC_LOCAL(Vector<CSSPropertyID>, properties, ());
    if (properties.isEmpty())
        RuntimeCSSEnabled::filterEnabledCSSPropertiesIntoVector(staticComputableProperties, WTF_ARRAY_LENGTH(staticComputableProperties), properties);
    return properties;
}

static CSSValueID valueForRepeatRule(int rule)
{
    switch (rule) {
        case RepeatImageRule:
            return CSSValueRepeat;
        case RoundImageRule:
            return CSSValueRound;
        case SpaceImageRule:
            return CSSValueSpace;
        default:
            return CSSValueStretch;
    }
}

static PassRefPtrWillBeRawPtr<CSSBorderImageSliceValue> valueForNinePieceImageSlice(const NinePieceImage& image)
{
    // Create the slices.
    RefPtrWillBeRawPtr<CSSPrimitiveValue> top;
    RefPtrWillBeRawPtr<CSSPrimitiveValue> right;
    RefPtrWillBeRawPtr<CSSPrimitiveValue> bottom;
    RefPtrWillBeRawPtr<CSSPrimitiveValue> left;

    if (image.imageSlices().top().isPercent())
        top = cssValuePool().createValue(image.imageSlices().top().value(), CSSPrimitiveValue::CSS_PERCENTAGE);
    else
        top = cssValuePool().createValue(image.imageSlices().top().value(), CSSPrimitiveValue::CSS_NUMBER);

    if (image.imageSlices().right() == image.imageSlices().top() && image.imageSlices().bottom() == image.imageSlices().top()
        && image.imageSlices().left() == image.imageSlices().top()) {
        right = top;
        bottom = top;
        left = top;
    } else {
        if (image.imageSlices().right().isPercent())
            right = cssValuePool().createValue(image.imageSlices().right().value(), CSSPrimitiveValue::CSS_PERCENTAGE);
        else
            right = cssValuePool().createValue(image.imageSlices().right().value(), CSSPrimitiveValue::CSS_NUMBER);

        if (image.imageSlices().bottom() == image.imageSlices().top() && image.imageSlices().right() == image.imageSlices().left()) {
            bottom = top;
            left = right;
        } else {
            if (image.imageSlices().bottom().isPercent())
                bottom = cssValuePool().createValue(image.imageSlices().bottom().value(), CSSPrimitiveValue::CSS_PERCENTAGE);
            else
                bottom = cssValuePool().createValue(image.imageSlices().bottom().value(), CSSPrimitiveValue::CSS_NUMBER);

            if (image.imageSlices().left() == image.imageSlices().right())
                left = right;
            else {
                if (image.imageSlices().left().isPercent())
                    left = cssValuePool().createValue(image.imageSlices().left().value(), CSSPrimitiveValue::CSS_PERCENTAGE);
                else
                    left = cssValuePool().createValue(image.imageSlices().left().value(), CSSPrimitiveValue::CSS_NUMBER);
            }
        }
    }

    RefPtrWillBeRawPtr<Quad> quad = Quad::create();
    quad->setTop(top);
    quad->setRight(right);
    quad->setBottom(bottom);
    quad->setLeft(left);

    return CSSBorderImageSliceValue::create(cssValuePool().createValue(quad.release()), image.fill());
}

static PassRefPtrWillBeRawPtr<CSSPrimitiveValue> valueForNinePieceImageQuad(const BorderImageLengthBox& box, const RenderStyle& style)
{
    // Create the slices.
    RefPtrWillBeRawPtr<CSSPrimitiveValue> top;
    RefPtrWillBeRawPtr<CSSPrimitiveValue> right;
    RefPtrWillBeRawPtr<CSSPrimitiveValue> bottom;
    RefPtrWillBeRawPtr<CSSPrimitiveValue> left;

    if (box.top().isNumber())
        top = cssValuePool().createValue(box.top().number(), CSSPrimitiveValue::CSS_NUMBER);
    else
        top = cssValuePool().createValue(box.top().length(), style);

    if (box.right() == box.top() && box.bottom() == box.top() && box.left() == box.top()) {
        right = top;
        bottom = top;
        left = top;
    } else {
        if (box.right().isNumber())
            right = cssValuePool().createValue(box.right().number(), CSSPrimitiveValue::CSS_NUMBER);
        else
            right = cssValuePool().createValue(box.right().length(), style);

        if (box.bottom() == box.top() && box.right() == box.left()) {
            bottom = top;
            left = right;
        } else {
            if (box.bottom().isNumber())
                bottom = cssValuePool().createValue(box.bottom().number(), CSSPrimitiveValue::CSS_NUMBER);
            else
                bottom = cssValuePool().createValue(box.bottom().length(), style);

            if (box.left() == box.right())
                left = right;
            else {
                if (box.left().isNumber())
                    left = cssValuePool().createValue(box.left().number(), CSSPrimitiveValue::CSS_NUMBER);
                else
                    left = cssValuePool().createValue(box.left().length(), style);
            }
        }
    }

    RefPtrWillBeRawPtr<Quad> quad = Quad::create();
    quad->setTop(top);
    quad->setRight(right);
    quad->setBottom(bottom);
    quad->setLeft(left);

    return cssValuePool().createValue(quad.release());
}

static PassRefPtrWillBeRawPtr<CSSValue> valueForNinePieceImageRepeat(const NinePieceImage& image)
{
    RefPtrWillBeRawPtr<CSSPrimitiveValue> horizontalRepeat;
    RefPtrWillBeRawPtr<CSSPrimitiveValue> verticalRepeat;

    horizontalRepeat = cssValuePool().createIdentifierValue(valueForRepeatRule(image.horizontalRule()));
    if (image.horizontalRule() == image.verticalRule())
        verticalRepeat = horizontalRepeat;
    else
        verticalRepeat = cssValuePool().createIdentifierValue(valueForRepeatRule(image.verticalRule()));
    return cssValuePool().createValue(Pair::create(horizontalRepeat.release(), verticalRepeat.release(), Pair::DropIdenticalValues));
}

static PassRefPtrWillBeRawPtr<CSSValue> valueForNinePieceImage(const NinePieceImage& image, const RenderStyle& style)
{
    if (!image.hasImage())
        return cssValuePool().createIdentifierValue(CSSValueNone);

    // Image first.
    RefPtrWillBeRawPtr<CSSValue> imageValue;
    if (image.image())
        imageValue = image.image()->cssValue();

    // Create the image slice.
    RefPtrWillBeRawPtr<CSSBorderImageSliceValue> imageSlices = valueForNinePieceImageSlice(image);

    // Create the border area slices.
    RefPtrWillBeRawPtr<CSSValue> borderSlices = valueForNinePieceImageQuad(image.borderSlices(), style);

    // Create the border outset.
    RefPtrWillBeRawPtr<CSSValue> outset = valueForNinePieceImageQuad(image.outset(), style);

    // Create the repeat rules.
    RefPtrWillBeRawPtr<CSSValue> repeat = valueForNinePieceImageRepeat(image);

    return createBorderImageValue(imageValue.release(), imageSlices.release(), borderSlices.release(), outset.release(), repeat.release());
}

inline static PassRefPtrWillBeRawPtr<CSSPrimitiveValue> zoomAdjustedPixelValue(double value, const RenderStyle& style)
{
    return cssValuePool().createValue(adjustFloatForAbsoluteZoom(value, style), CSSPrimitiveValue::CSS_PX);
}

inline static PassRefPtrWillBeRawPtr<CSSPrimitiveValue> zoomAdjustedNumberValue(double value, const RenderStyle& style)
{
    return cssValuePool().createValue(value / style.effectiveZoom(), CSSPrimitiveValue::CSS_NUMBER);
}

static PassRefPtrWillBeRawPtr<CSSPrimitiveValue> zoomAdjustedPixelValueForLength(const Length& length, const RenderStyle& style)
{
    if (length.isFixed())
        return zoomAdjustedPixelValue(length.value(), style);
    return cssValuePool().createValue(length, style);
}

static PassRefPtrWillBeRawPtr<CSSValue> valueForReflection(const StyleReflection* reflection, const RenderStyle& style)
{
    if (!reflection)
        return cssValuePool().createIdentifierValue(CSSValueNone);

    RefPtrWillBeRawPtr<CSSPrimitiveValue> offset;
    if (reflection->offset().isPercent())
        offset = cssValuePool().createValue(reflection->offset().percent(), CSSPrimitiveValue::CSS_PERCENTAGE);
    else
        offset = zoomAdjustedPixelValue(reflection->offset().value(), style);

    RefPtrWillBeRawPtr<CSSPrimitiveValue> direction;
    switch (reflection->direction()) {
    case ReflectionBelow:
        direction = cssValuePool().createIdentifierValue(CSSValueBelow);
        break;
    case ReflectionAbove:
        direction = cssValuePool().createIdentifierValue(CSSValueAbove);
        break;
    case ReflectionLeft:
        direction = cssValuePool().createIdentifierValue(CSSValueLeft);
        break;
    case ReflectionRight:
        direction = cssValuePool().createIdentifierValue(CSSValueRight);
        break;
    }

    return CSSReflectValue::create(direction.release(), offset.release(), valueForNinePieceImage(reflection->mask(), style));
}

static PassRefPtrWillBeRawPtr<CSSValueList> createPositionListForLayer(CSSPropertyID propertyID, const FillLayer* layer, const RenderStyle& style)
{
    RefPtrWillBeRawPtr<CSSValueList> positionList = CSSValueList::createSpaceSeparated();
    if (layer->isBackgroundXOriginSet()) {
        ASSERT_UNUSED(propertyID, propertyID == CSSPropertyBackgroundPosition || propertyID == CSSPropertyWebkitMaskPosition);
        positionList->append(cssValuePool().createValue(layer->backgroundXOrigin()));
    }
    positionList->append(zoomAdjustedPixelValueForLength(layer->xPosition(), style));
    if (layer->isBackgroundYOriginSet()) {
        ASSERT(propertyID == CSSPropertyBackgroundPosition || propertyID == CSSPropertyWebkitMaskPosition);
        positionList->append(cssValuePool().createValue(layer->backgroundYOrigin()));
    }
    positionList->append(zoomAdjustedPixelValueForLength(layer->yPosition(), style));
    return positionList.release();
}

static PassRefPtrWillBeRawPtr<CSSValue> valueForPositionOffset(RenderStyle& style, CSSPropertyID propertyID, const RenderObject* renderer)
{
    Length l;
    switch (propertyID) {
        case CSSPropertyLeft:
            l = style.left();
            break;
        case CSSPropertyRight:
            l = style.right();
            break;
        case CSSPropertyTop:
            l = style.top();
            break;
        case CSSPropertyBottom:
            l = style.bottom();
            break;
        default:
            return nullptr;
    }

    if (l.isPercent() && renderer && renderer->isBox()) {
        LayoutUnit containingBlockSize = (propertyID == CSSPropertyLeft || propertyID == CSSPropertyRight) ?
            toRenderBox(renderer)->containingBlockLogicalWidthForContent() :
            toRenderBox(renderer)->containingBlockLogicalHeightForContent(ExcludeMarginBorderPadding);
        return zoomAdjustedPixelValue(valueForLength(l, containingBlockSize), style);
    }
    if (l.isAuto()) {
        // FIXME: It's not enough to simply return "auto" values for one offset if the other side is defined.
        // In other words if left is auto and right is not auto, then left's computed value is negative right().
        // So we should get the opposite length unit and see if it is auto.
        return cssValuePool().createValue(l);
    }

    return zoomAdjustedPixelValueForLength(l, style);
}

PassRefPtrWillBeRawPtr<CSSPrimitiveValue> CSSComputedStyleDeclaration::currentColorOrValidColor(const RenderStyle& style, const StyleColor& color) const
{
    // This function does NOT look at visited information, so that computed style doesn't expose that.
    return cssValuePool().createColorValue(color.resolve(style.color()).rgb());
}

static PassRefPtrWillBeRawPtr<CSSValueList> valuesForBorderRadiusCorner(LengthSize radius, const RenderStyle& style)
{
    RefPtrWillBeRawPtr<CSSValueList> list = CSSValueList::createSpaceSeparated();
    if (radius.width().type() == Percent)
        list->append(cssValuePool().createValue(radius.width().percent(), CSSPrimitiveValue::CSS_PERCENTAGE));
    else
        list->append(zoomAdjustedPixelValueForLength(radius.width(), style));
    if (radius.height().type() == Percent)
        list->append(cssValuePool().createValue(radius.height().percent(), CSSPrimitiveValue::CSS_PERCENTAGE));
    else
        list->append(zoomAdjustedPixelValueForLength(radius.height(), style));
    return list.release();
}

static PassRefPtrWillBeRawPtr<CSSValue> valueForBorderRadiusCorner(LengthSize radius, const RenderStyle& style)
{
    RefPtrWillBeRawPtr<CSSValueList> list = valuesForBorderRadiusCorner(radius, style);
    if (list->item(0)->equals(*list->item(1)))
        return list->item(0);
    return list.release();
}

static PassRefPtrWillBeRawPtr<CSSValueList> valueForBorderRadiusShorthand(const RenderStyle& style)
{
    RefPtrWillBeRawPtr<CSSValueList> list = CSSValueList::createSlashSeparated();

    bool showHorizontalBottomLeft = style.borderTopRightRadius().width() != style.borderBottomLeftRadius().width();
    bool showHorizontalBottomRight = showHorizontalBottomLeft || (style.borderBottomRightRadius().width() != style.borderTopLeftRadius().width());
    bool showHorizontalTopRight = showHorizontalBottomRight || (style.borderTopRightRadius().width() != style.borderTopLeftRadius().width());

    bool showVerticalBottomLeft = style.borderTopRightRadius().height() != style.borderBottomLeftRadius().height();
    bool showVerticalBottomRight = showVerticalBottomLeft || (style.borderBottomRightRadius().height() != style.borderTopLeftRadius().height());
    bool showVerticalTopRight = showVerticalBottomRight || (style.borderTopRightRadius().height() != style.borderTopLeftRadius().height());

    RefPtrWillBeRawPtr<CSSValueList> topLeftRadius = valuesForBorderRadiusCorner(style.borderTopLeftRadius(), style);
    RefPtrWillBeRawPtr<CSSValueList> topRightRadius = valuesForBorderRadiusCorner(style.borderTopRightRadius(), style);
    RefPtrWillBeRawPtr<CSSValueList> bottomRightRadius = valuesForBorderRadiusCorner(style.borderBottomRightRadius(), style);
    RefPtrWillBeRawPtr<CSSValueList> bottomLeftRadius = valuesForBorderRadiusCorner(style.borderBottomLeftRadius(), style);

    RefPtrWillBeRawPtr<CSSValueList> horizontalRadii = CSSValueList::createSpaceSeparated();
    horizontalRadii->append(topLeftRadius->item(0));
    if (showHorizontalTopRight)
        horizontalRadii->append(topRightRadius->item(0));
    if (showHorizontalBottomRight)
        horizontalRadii->append(bottomRightRadius->item(0));
    if (showHorizontalBottomLeft)
        horizontalRadii->append(bottomLeftRadius->item(0));

    list->append(horizontalRadii.release());

    RefPtrWillBeRawPtr<CSSValueList> verticalRadii = CSSValueList::createSpaceSeparated();
    verticalRadii->append(topLeftRadius->item(1));
    if (showVerticalTopRight)
        verticalRadii->append(topRightRadius->item(1));
    if (showVerticalBottomRight)
        verticalRadii->append(bottomRightRadius->item(1));
    if (showVerticalBottomLeft)
        verticalRadii->append(bottomLeftRadius->item(1));

    if (!verticalRadii->equals(*toCSSValueList(list->item(0))))
        list->append(verticalRadii.release());

    return list.release();
}

static LayoutRect sizingBox(RenderObject* renderer)
{
    if (!renderer->isBox())
        return LayoutRect();

    RenderBox* box = toRenderBox(renderer);
    return box->style()->boxSizing() == BORDER_BOX ? box->borderBoxRect() : box->computedCSSContentBoxRect();
}

static PassRefPtrWillBeRawPtr<CSSTransformValue> valueForMatrixTransform(const TransformationMatrix& transform, const RenderStyle& style)
{
    RefPtrWillBeRawPtr<CSSTransformValue> transformValue;
    if (transform.isAffine()) {
        transformValue = CSSTransformValue::create(CSSTransformValue::MatrixTransformOperation);

        transformValue->append(cssValuePool().createValue(transform.a(), CSSPrimitiveValue::CSS_NUMBER));
        transformValue->append(cssValuePool().createValue(transform.b(), CSSPrimitiveValue::CSS_NUMBER));
        transformValue->append(cssValuePool().createValue(transform.c(), CSSPrimitiveValue::CSS_NUMBER));
        transformValue->append(cssValuePool().createValue(transform.d(), CSSPrimitiveValue::CSS_NUMBER));
        transformValue->append(zoomAdjustedNumberValue(transform.e(), style));
        transformValue->append(zoomAdjustedNumberValue(transform.f(), style));
    } else {
        transformValue = CSSTransformValue::create(CSSTransformValue::Matrix3DTransformOperation);

        transformValue->append(cssValuePool().createValue(transform.m11(), CSSPrimitiveValue::CSS_NUMBER));
        transformValue->append(cssValuePool().createValue(transform.m12(), CSSPrimitiveValue::CSS_NUMBER));
        transformValue->append(cssValuePool().createValue(transform.m13(), CSSPrimitiveValue::CSS_NUMBER));
        transformValue->append(cssValuePool().createValue(transform.m14(), CSSPrimitiveValue::CSS_NUMBER));

        transformValue->append(cssValuePool().createValue(transform.m21(), CSSPrimitiveValue::CSS_NUMBER));
        transformValue->append(cssValuePool().createValue(transform.m22(), CSSPrimitiveValue::CSS_NUMBER));
        transformValue->append(cssValuePool().createValue(transform.m23(), CSSPrimitiveValue::CSS_NUMBER));
        transformValue->append(cssValuePool().createValue(transform.m24(), CSSPrimitiveValue::CSS_NUMBER));

        transformValue->append(cssValuePool().createValue(transform.m31(), CSSPrimitiveValue::CSS_NUMBER));
        transformValue->append(cssValuePool().createValue(transform.m32(), CSSPrimitiveValue::CSS_NUMBER));
        transformValue->append(cssValuePool().createValue(transform.m33(), CSSPrimitiveValue::CSS_NUMBER));
        transformValue->append(cssValuePool().createValue(transform.m34(), CSSPrimitiveValue::CSS_NUMBER));

        transformValue->append(zoomAdjustedNumberValue(transform.m41(), style));
        transformValue->append(zoomAdjustedNumberValue(transform.m42(), style));
        transformValue->append(zoomAdjustedNumberValue(transform.m43(), style));
        transformValue->append(cssValuePool().createValue(transform.m44(), CSSPrimitiveValue::CSS_NUMBER));
    }

    return transformValue.release();
}

static PassRefPtrWillBeRawPtr<CSSValue> computedTransform(RenderObject* renderer, const RenderStyle& style)
{
    if (!renderer || !renderer->hasTransform() || !style.hasTransform())
        return cssValuePool().createIdentifierValue(CSSValueNone);

    IntRect box;
    if (renderer->isBox())
        box = pixelSnappedIntRect(toRenderBox(renderer)->borderBoxRect());

    TransformationMatrix transform;
    style.applyTransform(transform, box.size(), RenderStyle::ExcludeTransformOrigin);

    // FIXME: Need to print out individual functions (https://bugs.webkit.org/show_bug.cgi?id=23924)
    RefPtrWillBeRawPtr<CSSValueList> list = CSSValueList::createSpaceSeparated();
    list->append(valueForMatrixTransform(transform, style));

    return list.release();
}

PassRefPtrWillBeRawPtr<CSSValue> CSSComputedStyleDeclaration::valueForFilter(const RenderObject* renderer, const RenderStyle& style) const
{
    if (style.filter().operations().isEmpty())
        return cssValuePool().createIdentifierValue(CSSValueNone);

    RefPtrWillBeRawPtr<CSSValueList> list = CSSValueList::createSpaceSeparated();

    RefPtrWillBeRawPtr<CSSFilterValue> filterValue;

    Vector<RefPtr<FilterOperation> >::const_iterator end = style.filter().operations().end();
    for (Vector<RefPtr<FilterOperation> >::const_iterator it = style.filter().operations().begin(); it != end; ++it) {
        FilterOperation* filterOperation = (*it).get();
        switch (filterOperation->type()) {
        case FilterOperation::REFERENCE:
            filterValue = CSSFilterValue::create(CSSFilterValue::ReferenceFilterOperation);
            filterValue->append(cssValuePool().createValue(toReferenceFilterOperation(filterOperation)->url(), CSSPrimitiveValue::CSS_STRING));
            break;
        case FilterOperation::GRAYSCALE:
            filterValue = CSSFilterValue::create(CSSFilterValue::GrayscaleFilterOperation);
            filterValue->append(cssValuePool().createValue(toBasicColorMatrixFilterOperation(filterOperation)->amount(), CSSPrimitiveValue::CSS_NUMBER));
            break;
        case FilterOperation::SEPIA:
            filterValue = CSSFilterValue::create(CSSFilterValue::SepiaFilterOperation);
            filterValue->append(cssValuePool().createValue(toBasicColorMatrixFilterOperation(filterOperation)->amount(), CSSPrimitiveValue::CSS_NUMBER));
            break;
        case FilterOperation::SATURATE:
            filterValue = CSSFilterValue::create(CSSFilterValue::SaturateFilterOperation);
            filterValue->append(cssValuePool().createValue(toBasicColorMatrixFilterOperation(filterOperation)->amount(), CSSPrimitiveValue::CSS_NUMBER));
            break;
        case FilterOperation::HUE_ROTATE:
            filterValue = CSSFilterValue::create(CSSFilterValue::HueRotateFilterOperation);
            filterValue->append(cssValuePool().createValue(toBasicColorMatrixFilterOperation(filterOperation)->amount(), CSSPrimitiveValue::CSS_DEG));
            break;
        case FilterOperation::INVERT:
            filterValue = CSSFilterValue::create(CSSFilterValue::InvertFilterOperation);
            filterValue->append(cssValuePool().createValue(toBasicComponentTransferFilterOperation(filterOperation)->amount(), CSSPrimitiveValue::CSS_NUMBER));
            break;
        case FilterOperation::OPACITY:
            filterValue = CSSFilterValue::create(CSSFilterValue::OpacityFilterOperation);
            filterValue->append(cssValuePool().createValue(toBasicComponentTransferFilterOperation(filterOperation)->amount(), CSSPrimitiveValue::CSS_NUMBER));
            break;
        case FilterOperation::BRIGHTNESS:
            filterValue = CSSFilterValue::create(CSSFilterValue::BrightnessFilterOperation);
            filterValue->append(cssValuePool().createValue(toBasicComponentTransferFilterOperation(filterOperation)->amount(), CSSPrimitiveValue::CSS_NUMBER));
            break;
        case FilterOperation::CONTRAST:
            filterValue = CSSFilterValue::create(CSSFilterValue::ContrastFilterOperation);
            filterValue->append(cssValuePool().createValue(toBasicComponentTransferFilterOperation(filterOperation)->amount(), CSSPrimitiveValue::CSS_NUMBER));
            break;
        case FilterOperation::BLUR:
            filterValue = CSSFilterValue::create(CSSFilterValue::BlurFilterOperation);
            filterValue->append(zoomAdjustedPixelValue(toBlurFilterOperation(filterOperation)->stdDeviation().value(), style));
            break;
        case FilterOperation::DROP_SHADOW: {
            DropShadowFilterOperation* dropShadowOperation = toDropShadowFilterOperation(filterOperation);
            filterValue = CSSFilterValue::create(CSSFilterValue::DropShadowFilterOperation);
            // We want our computed style to look like that of a text shadow (has neither spread nor inset style).
            ShadowData shadow(dropShadowOperation->location(), dropShadowOperation->stdDeviation(), 0, Normal, dropShadowOperation->color());
            filterValue->append(valueForShadowData(shadow, style, false));
            break;
        }
        default:
            filterValue = CSSFilterValue::create(CSSFilterValue::UnknownFilterOperation);
            break;
        }
        list->append(filterValue.release());
    }

    return list.release();
}

static PassRefPtrWillBeRawPtr<CSSValue> specifiedValueForGridTrackBreadth(const GridLength& trackBreadth, const RenderStyle& style)
{
    if (!trackBreadth.isLength())
        return cssValuePool().createValue(trackBreadth.flex(), CSSPrimitiveValue::CSS_FR);

    const Length& trackBreadthLength = trackBreadth.length();
    if (trackBreadthLength.isAuto())
        return cssValuePool().createIdentifierValue(CSSValueAuto);
    return zoomAdjustedPixelValueForLength(trackBreadthLength, style);
}

static PassRefPtrWillBeRawPtr<CSSValue> specifiedValueForGridTrackSize(const GridTrackSize& trackSize, const RenderStyle& style)
{
    switch (trackSize.type()) {
    case LengthTrackSizing:
        return specifiedValueForGridTrackBreadth(trackSize.length(), style);
    case MinMaxTrackSizing:
        RefPtrWillBeRawPtr<CSSValueList> minMaxTrackBreadths = CSSValueList::createCommaSeparated();
        minMaxTrackBreadths->append(specifiedValueForGridTrackBreadth(trackSize.minTrackBreadth(), style));
        minMaxTrackBreadths->append(specifiedValueForGridTrackBreadth(trackSize.maxTrackBreadth(), style));
        return CSSFunctionValue::create("minmax(", minMaxTrackBreadths);
    }
    ASSERT_NOT_REACHED();
    return nullptr;
}

static void addValuesForNamedGridLinesAtIndex(const OrderedNamedGridLines& orderedNamedGridLines, size_t i, CSSValueList& list)
{
    const Vector<String>& namedGridLines = orderedNamedGridLines.get(i);
    if (namedGridLines.isEmpty())
        return;

    RefPtrWillBeRawPtr<CSSGridLineNamesValue> lineNames = CSSGridLineNamesValue::create();
    for (size_t j = 0; j < namedGridLines.size(); ++j)
        lineNames->append(cssValuePool().createValue(namedGridLines[j], CSSPrimitiveValue::CSS_STRING));
    list.append(lineNames.release());
}

static PassRefPtrWillBeRawPtr<CSSValue> valueForGridTrackList(GridTrackSizingDirection direction, RenderObject* renderer, const RenderStyle& style)
{
    const Vector<GridTrackSize>& trackSizes = direction == ForColumns ? style.gridTemplateColumns() : style.gridTemplateRows();
    const OrderedNamedGridLines& orderedNamedGridLines = direction == ForColumns ? style.orderedNamedGridColumnLines() : style.orderedNamedGridRowLines();

    // Handle the 'none' case here.
    if (!trackSizes.size()) {
        ASSERT(orderedNamedGridLines.isEmpty());
        return cssValuePool().createIdentifierValue(CSSValueNone);
    }

    RefPtrWillBeRawPtr<CSSValueList> list = CSSValueList::createSpaceSeparated();
    if (renderer && renderer->isRenderGrid()) {
        const Vector<LayoutUnit>& trackPositions = direction == ForColumns ? toRenderGrid(renderer)->columnPositions() : toRenderGrid(renderer)->rowPositions();
        // There are at least #tracks + 1 grid lines (trackPositions). Apart from that, the grid container can generate implicit grid tracks,
        // so we'll have more trackPositions than trackSizes as the latter only contain the explicit grid.
        ASSERT(trackPositions.size() - 1 >= trackSizes.size());

        for (size_t i = 0; i < trackSizes.size(); ++i) {
            addValuesForNamedGridLinesAtIndex(orderedNamedGridLines, i, *list);
            list->append(zoomAdjustedPixelValue(trackPositions[i + 1] - trackPositions[i], style));
        }
    } else {
        for (size_t i = 0; i < trackSizes.size(); ++i) {
            addValuesForNamedGridLinesAtIndex(orderedNamedGridLines, i, *list);
            list->append(specifiedValueForGridTrackSize(trackSizes[i], style));
        }
    }
    // Those are the trailing <string>* allowed in the syntax.
    addValuesForNamedGridLinesAtIndex(orderedNamedGridLines, trackSizes.size(), *list);
    return list.release();
}

static PassRefPtrWillBeRawPtr<CSSValue> valueForGridPosition(const GridPosition& position)
{
    if (position.isAuto())
        return cssValuePool().createIdentifierValue(CSSValueAuto);

    if (position.isNamedGridArea())
        return cssValuePool().createValue(position.namedGridLine(), CSSPrimitiveValue::CSS_STRING);

    RefPtrWillBeRawPtr<CSSValueList> list = CSSValueList::createSpaceSeparated();
    if (position.isSpan()) {
        list->append(cssValuePool().createIdentifierValue(CSSValueSpan));
        list->append(cssValuePool().createValue(position.spanPosition(), CSSPrimitiveValue::CSS_NUMBER));
    } else
        list->append(cssValuePool().createValue(position.integerPosition(), CSSPrimitiveValue::CSS_NUMBER));

    if (!position.namedGridLine().isNull())
        list->append(cssValuePool().createValue(position.namedGridLine(), CSSPrimitiveValue::CSS_STRING));
    return list;
}
static PassRefPtrWillBeRawPtr<CSSValue> createTransitionPropertyValue(const CSSAnimationData* animation)
{
    RefPtrWillBeRawPtr<CSSValue> propertyValue;
    if (animation->animationMode() == CSSAnimationData::AnimateNone)
        propertyValue = cssValuePool().createIdentifierValue(CSSValueNone);
    else if (animation->animationMode() == CSSAnimationData::AnimateAll)
        propertyValue = cssValuePool().createIdentifierValue(CSSValueAll);
    else
        propertyValue = cssValuePool().createValue(getPropertyNameString(animation->property()), CSSPrimitiveValue::CSS_STRING);
    return propertyValue.release();
}
static PassRefPtrWillBeRawPtr<CSSValue> valueForTransitionProperty(const CSSAnimationDataList* animList)
{
    RefPtrWillBeRawPtr<CSSValueList> list = CSSValueList::createCommaSeparated();
    if (animList) {
        for (size_t i = 0; i < animList->size(); ++i)
            list->append(createTransitionPropertyValue(animList->animation(i)));
    } else
        list->append(cssValuePool().createIdentifierValue(CSSValueAll));
    return list.release();
}

static PassRefPtrWillBeRawPtr<CSSValue> valueForAnimationDelay(const CSSAnimationDataList* animList)
{
    RefPtrWillBeRawPtr<CSSValueList> list = CSSValueList::createCommaSeparated();
    if (animList) {
        for (size_t i = 0; i < animList->size(); ++i)
            list->append(cssValuePool().createValue(animList->animation(i)->delay(), CSSPrimitiveValue::CSS_S));
    } else {
        // Note that initialAnimationDelay() is used for both transitions and animations
        list->append(cssValuePool().createValue(CSSAnimationData::initialAnimationDelay(), CSSPrimitiveValue::CSS_S));
    }
    return list.release();
}

static PassRefPtrWillBeRawPtr<CSSValue> valueForAnimationDuration(const CSSAnimationDataList* animList)
{
    RefPtrWillBeRawPtr<CSSValueList> list = CSSValueList::createCommaSeparated();
    if (animList) {
        for (size_t i = 0; i < animList->size(); ++i)
            list->append(cssValuePool().createValue(animList->animation(i)->duration(), CSSPrimitiveValue::CSS_S));
    } else {
        // Note that initialAnimationDuration() is used for both transitions and animations
        list->append(cssValuePool().createValue(CSSAnimationData::initialAnimationDuration(), CSSPrimitiveValue::CSS_S));
    }
    return list.release();
}

static PassRefPtrWillBeRawPtr<CSSValue> createTimingFunctionValue(const TimingFunction* timingFunction)
{
    switch (timingFunction->type()) {
    case TimingFunction::CubicBezierFunction:
        {
            const CubicBezierTimingFunction* bezierTimingFunction = toCubicBezierTimingFunction(timingFunction);
            if (bezierTimingFunction->subType() != CubicBezierTimingFunction::Custom) {
                CSSValueID valueId = CSSValueInvalid;
                switch (bezierTimingFunction->subType()) {
                case CubicBezierTimingFunction::Ease:
                    valueId = CSSValueEase;
                    break;
                case CubicBezierTimingFunction::EaseIn:
                    valueId = CSSValueEaseIn;
                    break;
                case CubicBezierTimingFunction::EaseOut:
                    valueId = CSSValueEaseOut;
                    break;
                case CubicBezierTimingFunction::EaseInOut:
                    valueId = CSSValueEaseInOut;
                    break;
                default:
                    ASSERT_NOT_REACHED();
                    return nullptr;
                }
                return cssValuePool().createIdentifierValue(valueId);
            }
            return CSSCubicBezierTimingFunctionValue::create(bezierTimingFunction->x1(), bezierTimingFunction->y1(), bezierTimingFunction->x2(), bezierTimingFunction->y2());
        }

    case TimingFunction::StepsFunction:
        {
            const StepsTimingFunction* stepsTimingFunction = toStepsTimingFunction(timingFunction);
            if (stepsTimingFunction->subType() == StepsTimingFunction::Custom)
                return CSSStepsTimingFunctionValue::create(stepsTimingFunction->numberOfSteps(), stepsTimingFunction->stepAtPosition());

            CSSValueID valueId;
            switch (stepsTimingFunction->subType()) {
            case StepsTimingFunction::Start:
                valueId = CSSValueStepStart;
                break;
            case StepsTimingFunction::End:
                valueId = CSSValueStepEnd;
                break;
            default:
                ASSERT_NOT_REACHED();
                return nullptr;
            }
            return cssValuePool().createIdentifierValue(valueId);
        }

    default:
        return cssValuePool().createIdentifierValue(CSSValueLinear);
    }
}

static PassRefPtrWillBeRawPtr<CSSValue> valueForAnimationTimingFunction(const CSSAnimationDataList* animList)
{
    RefPtrWillBeRawPtr<CSSValueList> list = CSSValueList::createCommaSeparated();
    if (animList) {
        for (size_t i = 0; i < animList->size(); ++i)
            list->append(createTimingFunctionValue(animList->animation(i)->timingFunction()));
    } else
        // Note that initialAnimationTimingFunction() is used for both transitions and animations
        list->append(createTimingFunctionValue(CSSAnimationData::initialAnimationTimingFunction().get()));
    return list.release();
}

static PassRefPtrWillBeRawPtr<CSSValue> valueForAnimationFillMode(unsigned fillMode)
{
    switch (fillMode) {
    case AnimationFillModeNone:
        return cssValuePool().createIdentifierValue(CSSValueNone);
    case AnimationFillModeForwards:
        return cssValuePool().createIdentifierValue(CSSValueForwards);
    case AnimationFillModeBackwards:
        return cssValuePool().createIdentifierValue(CSSValueBackwards);
    case AnimationFillModeBoth:
        return cssValuePool().createIdentifierValue(CSSValueBoth);
    default:
        ASSERT_NOT_REACHED();
        return nullptr;
    }
}

static PassRefPtrWillBeRawPtr<CSSValue> valueForAnimationDirection(CSSAnimationData::AnimationDirection direction)
{
    switch (direction) {
    case CSSAnimationData::AnimationDirectionNormal:
        return cssValuePool().createIdentifierValue(CSSValueNormal);
    case CSSAnimationData::AnimationDirectionAlternate:
        return cssValuePool().createIdentifierValue(CSSValueAlternate);
    case CSSAnimationData::AnimationDirectionReverse:
        return cssValuePool().createIdentifierValue(CSSValueReverse);
    case CSSAnimationData::AnimationDirectionAlternateReverse:
        return cssValuePool().createIdentifierValue(CSSValueAlternateReverse);
    default:
        ASSERT_NOT_REACHED();
        return nullptr;
    }
}

static PassRefPtrWillBeRawPtr<CSSValue> valueForWillChange(const Vector<CSSPropertyID>& willChangeProperties, bool willChangeContents, bool willChangeScrollPosition)
{
    RefPtrWillBeRawPtr<CSSValueList> list = CSSValueList::createCommaSeparated();
    if (willChangeContents)
        list->append(cssValuePool().createIdentifierValue(CSSValueContents));
    if (willChangeScrollPosition)
        list->append(cssValuePool().createIdentifierValue(CSSValueScrollPosition));
    for (size_t i = 0; i < willChangeProperties.size(); ++i)
        list->append(cssValuePool().createIdentifierValue(willChangeProperties[i]));
    if (!list->length())
        list->append(cssValuePool().createIdentifierValue(CSSValueAuto));
    return list.release();
}

static PassRefPtrWillBeRawPtr<CSSValue> createLineBoxContainValue(unsigned lineBoxContain)
{
    if (!lineBoxContain)
        return cssValuePool().createIdentifierValue(CSSValueNone);
    return CSSLineBoxContainValue::create(lineBoxContain);
}

CSSComputedStyleDeclaration::CSSComputedStyleDeclaration(PassRefPtr<Node> n, bool allowVisitedStyle, const String& pseudoElementName)
    : m_node(n)
    , m_allowVisitedStyle(allowVisitedStyle)
    , m_refCount(1)
{
    unsigned nameWithoutColonsStart = pseudoElementName[0] == ':' ? (pseudoElementName[1] == ':' ? 2 : 1) : 0;
    m_pseudoElementSpecifier = CSSSelector::pseudoId(CSSSelector::parsePseudoType(
        AtomicString(pseudoElementName.substring(nameWithoutColonsStart))));
}

CSSComputedStyleDeclaration::~CSSComputedStyleDeclaration()
{
}

void CSSComputedStyleDeclaration::ref()
{
    ++m_refCount;
}

void CSSComputedStyleDeclaration::deref()
{
    ASSERT(m_refCount);
    if (!--m_refCount)
        delete this;
}

String CSSComputedStyleDeclaration::cssText() const
{
    StringBuilder result;
    const Vector<CSSPropertyID>& properties = computableProperties();

    for (unsigned i = 0; i < properties.size(); i++) {
        if (i)
            result.append(' ');
        result.append(getPropertyName(properties[i]));
        result.append(": ", 2);
        result.append(getPropertyValue(properties[i]));
        result.append(';');
    }

    return result.toString();
}

void CSSComputedStyleDeclaration::setCSSText(const String&, ExceptionState& exceptionState)
{
    exceptionState.throwDOMException(NoModificationAllowedError, "These styles are computed, and therefore read-only.");
}

static CSSValueID cssIdentifierForFontSizeKeyword(int keywordSize)
{
    ASSERT_ARG(keywordSize, keywordSize);
    ASSERT_ARG(keywordSize, keywordSize <= 8);
    return static_cast<CSSValueID>(CSSValueXxSmall + keywordSize - 1);
}

PassRefPtrWillBeRawPtr<CSSValue> CSSComputedStyleDeclaration::getFontSizeCSSValuePreferringKeyword() const
{
    if (!m_node)
        return nullptr;

    m_node->document().updateLayoutIgnorePendingStylesheets();

    RefPtr<RenderStyle> style = m_node->computedStyle(m_pseudoElementSpecifier);
    if (!style)
        return nullptr;

    if (int keywordSize = style->fontDescription().keywordSize())
        return cssValuePool().createIdentifierValue(cssIdentifierForFontSizeKeyword(keywordSize));


    return zoomAdjustedPixelValue(style->fontDescription().computedPixelSize(), *style);
}

bool CSSComputedStyleDeclaration::useFixedFontDefaultSize() const
{
    if (!m_node)
        return false;

    RefPtr<RenderStyle> style = m_node->computedStyle(m_pseudoElementSpecifier);
    if (!style)
        return false;

    return style->fontDescription().useFixedDefaultSize();
}

PassRefPtrWillBeRawPtr<CSSValue> CSSComputedStyleDeclaration::valueForShadowData(const ShadowData& shadow, const RenderStyle& style, bool useSpread) const
{
    RefPtrWillBeRawPtr<CSSPrimitiveValue> x = zoomAdjustedPixelValue(shadow.x(), style);
    RefPtrWillBeRawPtr<CSSPrimitiveValue> y = zoomAdjustedPixelValue(shadow.y(), style);
    RefPtrWillBeRawPtr<CSSPrimitiveValue> blur = zoomAdjustedPixelValue(shadow.blur(), style);
    RefPtrWillBeRawPtr<CSSPrimitiveValue> spread = useSpread ? zoomAdjustedPixelValue(shadow.spread(), style) : PassRefPtrWillBeRawPtr<CSSPrimitiveValue>();
    RefPtrWillBeRawPtr<CSSPrimitiveValue> shadowStyle = shadow.style() == Normal ? PassRefPtrWillBeRawPtr<CSSPrimitiveValue>() : cssValuePool().createIdentifierValue(CSSValueInset);
    RefPtrWillBeRawPtr<CSSPrimitiveValue> color = currentColorOrValidColor(style, shadow.color());
    return CSSShadowValue::create(x.release(), y.release(), blur.release(), spread.release(), shadowStyle.release(), color.release());
}

PassRefPtrWillBeRawPtr<CSSValue> CSSComputedStyleDeclaration::valueForShadowList(const ShadowList* shadowList, const RenderStyle& style, bool useSpread) const
{
    if (!shadowList)
        return cssValuePool().createIdentifierValue(CSSValueNone);

    RefPtrWillBeRawPtr<CSSValueList> list = CSSValueList::createCommaSeparated();
    size_t shadowCount = shadowList->shadows().size();
    for (size_t i = 0; i < shadowCount; ++i)
        list->append(valueForShadowData(shadowList->shadows()[i], style, useSpread));
    return list.release();
}

PassRefPtrWillBeRawPtr<CSSValue> CSSComputedStyleDeclaration::getPropertyCSSValue(CSSPropertyID propertyID) const
{
    return getPropertyCSSValue(propertyID, UpdateLayout);
}

static CSSValueID identifierForFamily(const AtomicString& family)
{
    if (family == FontFamilyNames::webkit_cursive)
        return CSSValueCursive;
    if (family == FontFamilyNames::webkit_fantasy)
        return CSSValueFantasy;
    if (family == FontFamilyNames::webkit_monospace)
        return CSSValueMonospace;
    if (family == FontFamilyNames::webkit_pictograph)
        return CSSValueWebkitPictograph;
    if (family == FontFamilyNames::webkit_sans_serif)
        return CSSValueSansSerif;
    if (family == FontFamilyNames::webkit_serif)
        return CSSValueSerif;
    return CSSValueInvalid;
}

static PassRefPtrWillBeRawPtr<CSSPrimitiveValue> valueForFamily(const AtomicString& family)
{
    if (CSSValueID familyIdentifier = identifierForFamily(family))
        return cssValuePool().createIdentifierValue(familyIdentifier);
    return cssValuePool().createValue(family.string(), CSSPrimitiveValue::CSS_STRING);
}

static PassRefPtrWillBeRawPtr<CSSValue> renderTextDecorationFlagsToCSSValue(int textDecoration)
{
    // Blink value is ignored.
    RefPtrWillBeRawPtr<CSSValueList> list = CSSValueList::createSpaceSeparated();
    if (textDecoration & TextDecorationUnderline)
        list->append(cssValuePool().createIdentifierValue(CSSValueUnderline));
    if (textDecoration & TextDecorationOverline)
        list->append(cssValuePool().createIdentifierValue(CSSValueOverline));
    if (textDecoration & TextDecorationLineThrough)
        list->append(cssValuePool().createIdentifierValue(CSSValueLineThrough));

    if (!list->length())
        return cssValuePool().createIdentifierValue(CSSValueNone);
    return list.release();
}

static PassRefPtrWillBeRawPtr<CSSValue> valueForTextDecorationStyle(TextDecorationStyle textDecorationStyle)
{
    switch (textDecorationStyle) {
    case TextDecorationStyleSolid:
        return cssValuePool().createIdentifierValue(CSSValueSolid);
    case TextDecorationStyleDouble:
        return cssValuePool().createIdentifierValue(CSSValueDouble);
    case TextDecorationStyleDotted:
        return cssValuePool().createIdentifierValue(CSSValueDotted);
    case TextDecorationStyleDashed:
        return cssValuePool().createIdentifierValue(CSSValueDashed);
    case TextDecorationStyleWavy:
        return cssValuePool().createIdentifierValue(CSSValueWavy);
    }

    ASSERT_NOT_REACHED();
    return cssValuePool().createExplicitInitialValue();
}

static PassRefPtrWillBeRawPtr<CSSValue> valueForFillRepeat(EFillRepeat xRepeat, EFillRepeat yRepeat)
{
    // For backwards compatibility, if both values are equal, just return one of them. And
    // if the two values are equivalent to repeat-x or repeat-y, just return the shorthand.
    if (xRepeat == yRepeat)
        return cssValuePool().createValue(xRepeat);
    if (xRepeat == RepeatFill && yRepeat == NoRepeatFill)
        return cssValuePool().createIdentifierValue(CSSValueRepeatX);
    if (xRepeat == NoRepeatFill && yRepeat == RepeatFill)
        return cssValuePool().createIdentifierValue(CSSValueRepeatY);

    RefPtrWillBeRawPtr<CSSValueList> list = CSSValueList::createSpaceSeparated();
    list->append(cssValuePool().createValue(xRepeat));
    list->append(cssValuePool().createValue(yRepeat));
    return list.release();
}

static PassRefPtrWillBeRawPtr<CSSValue> valueForFillSourceType(EMaskSourceType type)
{
    switch (type) {
    case MaskAlpha:
        return cssValuePool().createValue(CSSValueAlpha);
    case MaskLuminance:
        return cssValuePool().createValue(CSSValueLuminance);
    }

    ASSERT_NOT_REACHED();

    return nullptr;
}

static PassRefPtrWillBeRawPtr<CSSValue> valueForFillSize(const FillSize& fillSize, const RenderStyle& style)
{
    if (fillSize.type == Contain)
        return cssValuePool().createIdentifierValue(CSSValueContain);

    if (fillSize.type == Cover)
        return cssValuePool().createIdentifierValue(CSSValueCover);

    if (fillSize.size.height().isAuto())
        return zoomAdjustedPixelValueForLength(fillSize.size.width(), style);

    RefPtrWillBeRawPtr<CSSValueList> list = CSSValueList::createSpaceSeparated();
    list->append(zoomAdjustedPixelValueForLength(fillSize.size.width(), style));
    list->append(zoomAdjustedPixelValueForLength(fillSize.size.height(), style));
    return list.release();
}

static PassRefPtrWillBeRawPtr<CSSValue> valueForContentData(const RenderStyle& style)
{
    RefPtrWillBeRawPtr<CSSValueList> list = CSSValueList::createSpaceSeparated();
    for (const ContentData* contentData = style.contentData(); contentData; contentData = contentData->next()) {
        if (contentData->isCounter()) {
            const CounterContent* counter = static_cast<const CounterContentData*>(contentData)->counter();
            ASSERT(counter);
            list->append(cssValuePool().createValue(counter->identifier(), CSSPrimitiveValue::CSS_COUNTER_NAME));
        } else if (contentData->isImage()) {
            const StyleImage* image = static_cast<const ImageContentData*>(contentData)->image();
            ASSERT(image);
            list->append(image->cssValue());
        } else if (contentData->isText())
            list->append(cssValuePool().createValue(static_cast<const TextContentData*>(contentData)->text(), CSSPrimitiveValue::CSS_STRING));
    }
    return list.release();
}

static PassRefPtrWillBeRawPtr<CSSValue> valueForCounterDirectives(const RenderStyle& style, CSSPropertyID propertyID)
{
    const CounterDirectiveMap* map = style.counterDirectives();
    if (!map)
        return nullptr;

    RefPtrWillBeRawPtr<CSSValueList> list = CSSValueList::createSpaceSeparated();
    for (CounterDirectiveMap::const_iterator it = map->begin(); it != map->end(); ++it) {
        list->append(cssValuePool().createValue(it->key, CSSPrimitiveValue::CSS_STRING));
        short number = propertyID == CSSPropertyCounterIncrement ? it->value.incrementValue() : it->value.resetValue();
        list->append(cssValuePool().createValue((double)number, CSSPrimitiveValue::CSS_NUMBER));
    }
    return list.release();
}

static void logUnimplementedPropertyID(CSSPropertyID propertyID)
{
    DEFINE_STATIC_LOCAL(HashSet<CSSPropertyID>, propertyIDSet, ());
    if (!propertyIDSet.add(propertyID).isNewEntry)
        return;

    WTF_LOG_ERROR("WebKit does not yet implement getComputedStyle for '%s'.", getPropertyName(propertyID));
}

static PassRefPtrWillBeRawPtr<CSSValueList> valueForFontFamily(RenderStyle& style)
{
    const FontFamily& firstFamily = style.fontDescription().family();
    RefPtrWillBeRawPtr<CSSValueList> list = CSSValueList::createCommaSeparated();
    for (const FontFamily* family = &firstFamily; family; family = family->next())
        list->append(valueForFamily(family->family()));
    return list.release();
}

static PassRefPtrWillBeRawPtr<CSSPrimitiveValue> valueForLineHeight(RenderStyle& style)
{
    Length length = style.lineHeight();
    if (length.isNegative())
        return cssValuePool().createIdentifierValue(CSSValueNormal);

    return zoomAdjustedPixelValue(floatValueForLength(length, style.fontDescription().specifiedSize()), style);
}

static PassRefPtrWillBeRawPtr<CSSPrimitiveValue> valueForFontSize(RenderStyle& style)
{
    return zoomAdjustedPixelValue(style.fontDescription().computedPixelSize(), style);
}

static PassRefPtrWillBeRawPtr<CSSPrimitiveValue> valueForFontStyle(RenderStyle& style)
{
    if (style.fontDescription().style() == FontStyleItalic)
        return cssValuePool().createIdentifierValue(CSSValueItalic);
    return cssValuePool().createIdentifierValue(CSSValueNormal);
}

static PassRefPtrWillBeRawPtr<CSSPrimitiveValue> valueForFontVariant(RenderStyle& style)
{
    if (style.fontDescription().variant() == FontVariantSmallCaps)
        return cssValuePool().createIdentifierValue(CSSValueSmallCaps);
    return cssValuePool().createIdentifierValue(CSSValueNormal);
}

static PassRefPtrWillBeRawPtr<CSSPrimitiveValue> valueForFontWeight(RenderStyle& style)
{
    switch (style.fontDescription().weight()) {
    case FontWeight100:
        return cssValuePool().createIdentifierValue(CSSValue100);
    case FontWeight200:
        return cssValuePool().createIdentifierValue(CSSValue200);
    case FontWeight300:
        return cssValuePool().createIdentifierValue(CSSValue300);
    case FontWeightNormal:
        return cssValuePool().createIdentifierValue(CSSValueNormal);
    case FontWeight500:
        return cssValuePool().createIdentifierValue(CSSValue500);
    case FontWeight600:
        return cssValuePool().createIdentifierValue(CSSValue600);
    case FontWeightBold:
        return cssValuePool().createIdentifierValue(CSSValueBold);
    case FontWeight800:
        return cssValuePool().createIdentifierValue(CSSValue800);
    case FontWeight900:
        return cssValuePool().createIdentifierValue(CSSValue900);
    }
    ASSERT_NOT_REACHED();
    return cssValuePool().createIdentifierValue(CSSValueNormal);
}

static PassRefPtrWillBeRawPtr<CSSValue> valueForShape(const RenderStyle& style, ShapeValue* shapeValue)
{
    if (!shapeValue)
        return cssValuePool().createIdentifierValue(CSSValueNone);
    if (shapeValue->type() == ShapeValue::Outside)
        return cssValuePool().createIdentifierValue(CSSValueOutsideShape);
    if (shapeValue->type() == ShapeValue::Box)
        return cssValuePool().createValue(shapeValue->layoutBox());
    if (shapeValue->type() == ShapeValue::Image) {
        if (shapeValue->image())
            return shapeValue->image()->cssValue();
        return cssValuePool().createIdentifierValue(CSSValueNone);
    }

    ASSERT(shapeValue->type() == ShapeValue::Shape);

    RefPtrWillBeRawPtr<CSSValueList> list = CSSValueList::createSpaceSeparated();
    list->append(valueForBasicShape(style, shapeValue->shape()));
    if (shapeValue->layoutBox() != BoxMissing)
        list->append(cssValuePool().createValue(shapeValue->layoutBox()));
    return list.release();
}

static PassRefPtrWillBeRawPtr<CSSValue> touchActionFlagsToCSSValue(TouchAction touchAction)
{
    RefPtrWillBeRawPtr<CSSValueList> list = CSSValueList::createSpaceSeparated();
    if (touchAction == TouchActionAuto)
        list->append(cssValuePool().createIdentifierValue(CSSValueAuto));
    if (touchAction & TouchActionNone) {
        ASSERT(touchAction == TouchActionNone);
        list->append(cssValuePool().createIdentifierValue(CSSValueNone));
    }
    if (touchAction == (TouchActionPanX | TouchActionPanY | TouchActionPinchZoom)) {
        list->append(cssValuePool().createIdentifierValue(CSSValueManipulation));
    } else {
        if (touchAction & TouchActionPanX)
            list->append(cssValuePool().createIdentifierValue(CSSValuePanX));
        if (touchAction & TouchActionPanY)
            list->append(cssValuePool().createIdentifierValue(CSSValuePanY));
    }
    ASSERT(list->length());
    return list.release();
}

static bool isLayoutDependent(CSSPropertyID propertyID, PassRefPtr<RenderStyle> style, RenderObject* renderer)
{
    // Some properties only depend on layout in certain conditions which
    // are specified in the main switch statement below. So we can avoid
    // forcing layout in those conditions. The conditions in this switch
    // statement must remain in sync with the conditions in the main switch.
    // FIXME: Some of these cases could be narrowed down or optimized better.
    switch (propertyID) {
    case CSSPropertyBottom:
    case CSSPropertyGridTemplateColumns:
    case CSSPropertyGridTemplateRows:
    case CSSPropertyHeight:
    case CSSPropertyLeft:
    case CSSPropertyRight:
    case CSSPropertyTop:
    case CSSPropertyWebkitPerspectiveOrigin:
    case CSSPropertyWebkitTransform:
    case CSSPropertyWebkitTransformOrigin:
    case CSSPropertyWidth:
    case CSSPropertyWebkitFilter:
        return true;
    case CSSPropertyMargin:
        return renderer && renderer->isBox() && (!style || !style->marginBottom().isFixed() || !style->marginTop().isFixed() || !style->marginLeft().isFixed() || !style->marginRight().isFixed());
    case CSSPropertyMarginLeft:
        return renderer && renderer->isBox() && (!style || !style->marginLeft().isFixed());
    case CSSPropertyMarginRight:
        return renderer && renderer->isBox() && (!style || !style->marginRight().isFixed());
    case CSSPropertyMarginTop:
        return renderer && renderer->isBox() && (!style || !style->marginTop().isFixed());
    case CSSPropertyMarginBottom:
        return renderer && renderer->isBox() && (!style || !style->marginBottom().isFixed());
    case CSSPropertyPadding:
        return renderer && renderer->isBox() && (!style || !style->paddingBottom().isFixed() || !style->paddingTop().isFixed() || !style->paddingLeft().isFixed() || !style->paddingRight().isFixed());
    case CSSPropertyPaddingBottom:
        return renderer && renderer->isBox() && (!style || !style->paddingBottom().isFixed());
    case CSSPropertyPaddingLeft:
        return renderer && renderer->isBox() && (!style || !style->paddingLeft().isFixed());
    case CSSPropertyPaddingRight:
        return renderer && renderer->isBox() && (!style || !style->paddingRight().isFixed());
    case CSSPropertyPaddingTop:
        return renderer && renderer->isBox() && (!style || !style->paddingTop().isFixed());
    default:
        return false;
    }
}

PassRefPtr<RenderStyle> CSSComputedStyleDeclaration::computeRenderStyle(CSSPropertyID propertyID) const
{
    Node* styledNode = this->styledNode();
    ASSERT(styledNode);
    return styledNode->computedStyle(styledNode->isPseudoElement() ? NOPSEUDO : m_pseudoElementSpecifier);
}

Node* CSSComputedStyleDeclaration::styledNode() const
{
    if (!m_node)
        return 0;
    if (m_node->isElementNode()) {
        if (PseudoElement* element = toElement(m_node)->pseudoElement(m_pseudoElementSpecifier))
            return element;
    }
    return m_node.get();
}

static PassRefPtrWillBeRawPtr<CSSValueList> valueForItemPositionWithOverflowAlignment(ItemPosition itemPosition, OverflowAlignment overflowAlignment)
{
    RefPtrWillBeRawPtr<CSSValueList> result = CSSValueList::createSpaceSeparated();
    result->append(CSSPrimitiveValue::create(itemPosition));
    if (itemPosition >= ItemPositionCenter && overflowAlignment != OverflowAlignmentDefault)
        result->append(CSSPrimitiveValue::create(overflowAlignment));
    return result.release();
}

PassRefPtrWillBeRawPtr<CSSValue> CSSComputedStyleDeclaration::getPropertyCSSValue(CSSPropertyID propertyID, EUpdateLayout updateLayout) const
{
    Node* styledNode = this->styledNode();
    if (!styledNode)
        return nullptr;
    RenderObject* renderer = styledNode->renderer();
    RefPtr<RenderStyle> style;

    if (updateLayout) {
        Document& document = styledNode->document();

        // A timing update may be required if a compositor animation is running or animations
        // have been updated via the api.
        DocumentAnimations::updateAnimationTimingForGetComputedStyle(*styledNode, propertyID);

        document.updateStyleForNodeIfNeeded(styledNode);

        // The style recalc could have caused the styled node to be discarded or replaced
        // if it was a PseudoElement so we need to update it.
        styledNode = this->styledNode();
        renderer = styledNode->renderer();

        style = computeRenderStyle(propertyID);

        bool forceFullLayout = isLayoutDependent(propertyID, style, renderer)
            || styledNode->isInShadowTree()
            || (document.ownerElement() && document.ensureStyleResolver().hasViewportDependentMediaQueries());

        if (forceFullLayout) {
            document.updateLayoutIgnorePendingStylesheets();
            styledNode = this->styledNode();
            style = computeRenderStyle(propertyID);
            renderer = styledNode->renderer();
        }
    } else {
        style = computeRenderStyle(propertyID);
    }

    if (!style)
        return nullptr;

    propertyID = CSSProperty::resolveDirectionAwareProperty(propertyID, style->direction(), style->writingMode());

    switch (propertyID) {
        case CSSPropertyInvalid:
            break;

        case CSSPropertyBackgroundColor:
            return m_allowVisitedStyle ? cssValuePool().createColorValue(style->visitedDependentColor(CSSPropertyBackgroundColor).rgb()) : currentColorOrValidColor(*style, style->backgroundColor());
        case CSSPropertyBackgroundImage:
        case CSSPropertyWebkitMaskImage: {
            const FillLayer* layers = propertyID == CSSPropertyWebkitMaskImage ? style->maskLayers() : style->backgroundLayers();
            if (!layers)
                return cssValuePool().createIdentifierValue(CSSValueNone);

            if (!layers->next()) {
                if (layers->image())
                    return layers->image()->cssValue();

                return cssValuePool().createIdentifierValue(CSSValueNone);
            }

            RefPtrWillBeRawPtr<CSSValueList> list = CSSValueList::createCommaSeparated();
            for (const FillLayer* currLayer = layers; currLayer; currLayer = currLayer->next()) {
                if (currLayer->image())
                    list->append(currLayer->image()->cssValue());
                else
                    list->append(cssValuePool().createIdentifierValue(CSSValueNone));
            }
            return list.release();
        }
        case CSSPropertyBackgroundSize:
        case CSSPropertyWebkitBackgroundSize:
        case CSSPropertyWebkitMaskSize: {
            const FillLayer* layers = propertyID == CSSPropertyWebkitMaskSize ? style->maskLayers() : style->backgroundLayers();
            if (!layers->next())
                return valueForFillSize(layers->size(), *style);

            RefPtrWillBeRawPtr<CSSValueList> list = CSSValueList::createCommaSeparated();
            for (const FillLayer* currLayer = layers; currLayer; currLayer = currLayer->next())
                list->append(valueForFillSize(currLayer->size(), *style));

            return list.release();
        }
        case CSSPropertyBackgroundRepeat:
        case CSSPropertyWebkitMaskRepeat: {
            const FillLayer* layers = propertyID == CSSPropertyWebkitMaskRepeat ? style->maskLayers() : style->backgroundLayers();
            if (!layers->next())
                return valueForFillRepeat(layers->repeatX(), layers->repeatY());

            RefPtrWillBeRawPtr<CSSValueList> list = CSSValueList::createCommaSeparated();
            for (const FillLayer* currLayer = layers; currLayer; currLayer = currLayer->next())
                list->append(valueForFillRepeat(currLayer->repeatX(), currLayer->repeatY()));

            return list.release();
        }
        case CSSPropertyMaskSourceType: {
            const FillLayer* layers = style->maskLayers();

            if (!layers)
                return cssValuePool().createIdentifierValue(CSSValueNone);

            if (!layers->next())
                return valueForFillSourceType(layers->maskSourceType());

            RefPtrWillBeRawPtr<CSSValueList> list = CSSValueList::createCommaSeparated();
            for (const FillLayer* currLayer = layers; currLayer; currLayer = currLayer->next())
                list->append(valueForFillSourceType(currLayer->maskSourceType()));

            return list.release();
        }
        case CSSPropertyWebkitBackgroundComposite:
        case CSSPropertyWebkitMaskComposite: {
            const FillLayer* layers = propertyID == CSSPropertyWebkitMaskComposite ? style->maskLayers() : style->backgroundLayers();
            if (!layers->next())
                return cssValuePool().createValue(layers->composite());

            RefPtrWillBeRawPtr<CSSValueList> list = CSSValueList::createCommaSeparated();
            for (const FillLayer* currLayer = layers; currLayer; currLayer = currLayer->next())
                list->append(cssValuePool().createValue(currLayer->composite()));

            return list.release();
        }
        case CSSPropertyBackgroundAttachment: {
            const FillLayer* layers = style->backgroundLayers();
            if (!layers->next())
                return cssValuePool().createValue(layers->attachment());

            RefPtrWillBeRawPtr<CSSValueList> list = CSSValueList::createCommaSeparated();
            for (const FillLayer* currLayer = layers; currLayer; currLayer = currLayer->next())
                list->append(cssValuePool().createValue(currLayer->attachment()));

            return list.release();
        }
        case CSSPropertyBackgroundClip:
        case CSSPropertyBackgroundOrigin:
        case CSSPropertyWebkitBackgroundClip:
        case CSSPropertyWebkitBackgroundOrigin:
        case CSSPropertyWebkitMaskClip:
        case CSSPropertyWebkitMaskOrigin: {
            const FillLayer* layers = (propertyID == CSSPropertyWebkitMaskClip || propertyID == CSSPropertyWebkitMaskOrigin) ? style->maskLayers() : style->backgroundLayers();
            bool isClip = propertyID == CSSPropertyBackgroundClip || propertyID == CSSPropertyWebkitBackgroundClip || propertyID == CSSPropertyWebkitMaskClip;
            if (!layers->next()) {
                EFillBox box = isClip ? layers->clip() : layers->origin();
                return cssValuePool().createValue(box);
            }

            RefPtrWillBeRawPtr<CSSValueList> list = CSSValueList::createCommaSeparated();
            for (const FillLayer* currLayer = layers; currLayer; currLayer = currLayer->next()) {
                EFillBox box = isClip ? currLayer->clip() : currLayer->origin();
                list->append(cssValuePool().createValue(box));
            }

            return list.release();
        }
        case CSSPropertyBackgroundPosition:
        case CSSPropertyWebkitMaskPosition: {
            const FillLayer* layers = propertyID == CSSPropertyWebkitMaskPosition ? style->maskLayers() : style->backgroundLayers();
            if (!layers->next())
                return createPositionListForLayer(propertyID, layers, *style);

            RefPtrWillBeRawPtr<CSSValueList> list = CSSValueList::createCommaSeparated();
            for (const FillLayer* currLayer = layers; currLayer; currLayer = currLayer->next())
                list->append(createPositionListForLayer(propertyID, currLayer, *style));
            return list.release();
        }
        case CSSPropertyBackgroundPositionX:
        case CSSPropertyWebkitMaskPositionX: {
            const FillLayer* layers = propertyID == CSSPropertyWebkitMaskPositionX ? style->maskLayers() : style->backgroundLayers();
            if (!layers->next())
                return cssValuePool().createValue(layers->xPosition());

            RefPtrWillBeRawPtr<CSSValueList> list = CSSValueList::createCommaSeparated();
            for (const FillLayer* currLayer = layers; currLayer; currLayer = currLayer->next())
                list->append(cssValuePool().createValue(currLayer->xPosition()));

            return list.release();
        }
        case CSSPropertyBackgroundPositionY:
        case CSSPropertyWebkitMaskPositionY: {
            const FillLayer* layers = propertyID == CSSPropertyWebkitMaskPositionY ? style->maskLayers() : style->backgroundLayers();
            if (!layers->next())
                return cssValuePool().createValue(layers->yPosition());

            RefPtrWillBeRawPtr<CSSValueList> list = CSSValueList::createCommaSeparated();
            for (const FillLayer* currLayer = layers; currLayer; currLayer = currLayer->next())
                list->append(cssValuePool().createValue(currLayer->yPosition()));

            return list.release();
        }
        case CSSPropertyBorderCollapse:
            if (style->borderCollapse())
                return cssValuePool().createIdentifierValue(CSSValueCollapse);
            return cssValuePool().createIdentifierValue(CSSValueSeparate);
        case CSSPropertyBorderSpacing: {
            RefPtrWillBeRawPtr<CSSValueList> list = CSSValueList::createSpaceSeparated();
            list->append(zoomAdjustedPixelValue(style->horizontalBorderSpacing(), *style));
            list->append(zoomAdjustedPixelValue(style->verticalBorderSpacing(), *style));
            return list.release();
        }
        case CSSPropertyWebkitBorderHorizontalSpacing:
            return zoomAdjustedPixelValue(style->horizontalBorderSpacing(), *style);
        case CSSPropertyWebkitBorderVerticalSpacing:
            return zoomAdjustedPixelValue(style->verticalBorderSpacing(), *style);
        case CSSPropertyBorderImageSource:
            if (style->borderImageSource())
                return style->borderImageSource()->cssValue();
            return cssValuePool().createIdentifierValue(CSSValueNone);
        case CSSPropertyBorderTopColor:
            return m_allowVisitedStyle ? cssValuePool().createColorValue(style->visitedDependentColor(CSSPropertyBorderTopColor).rgb()) : currentColorOrValidColor(*style, style->borderTopColor());
        case CSSPropertyBorderRightColor:
            return m_allowVisitedStyle ? cssValuePool().createColorValue(style->visitedDependentColor(CSSPropertyBorderRightColor).rgb()) : currentColorOrValidColor(*style, style->borderRightColor());
        case CSSPropertyBorderBottomColor:
            return m_allowVisitedStyle ? cssValuePool().createColorValue(style->visitedDependentColor(CSSPropertyBorderBottomColor).rgb()) : currentColorOrValidColor(*style, style->borderBottomColor());
        case CSSPropertyBorderLeftColor:
            return m_allowVisitedStyle ? cssValuePool().createColorValue(style->visitedDependentColor(CSSPropertyBorderLeftColor).rgb()) : currentColorOrValidColor(*style, style->borderLeftColor());
        case CSSPropertyBorderTopStyle:
            return cssValuePool().createValue(style->borderTopStyle());
        case CSSPropertyBorderRightStyle:
            return cssValuePool().createValue(style->borderRightStyle());
        case CSSPropertyBorderBottomStyle:
            return cssValuePool().createValue(style->borderBottomStyle());
        case CSSPropertyBorderLeftStyle:
            return cssValuePool().createValue(style->borderLeftStyle());
        case CSSPropertyBorderTopWidth:
            return zoomAdjustedPixelValue(style->borderTopWidth(), *style);
        case CSSPropertyBorderRightWidth:
            return zoomAdjustedPixelValue(style->borderRightWidth(), *style);
        case CSSPropertyBorderBottomWidth:
            return zoomAdjustedPixelValue(style->borderBottomWidth(), *style);
        case CSSPropertyBorderLeftWidth:
            return zoomAdjustedPixelValue(style->borderLeftWidth(), *style);
        case CSSPropertyBottom:
            return valueForPositionOffset(*style, CSSPropertyBottom, renderer);
        case CSSPropertyWebkitBoxAlign:
            return cssValuePool().createValue(style->boxAlign());
        case CSSPropertyWebkitBoxDecorationBreak:
            if (style->boxDecorationBreak() == DSLICE)
                return cssValuePool().createIdentifierValue(CSSValueSlice);
        return cssValuePool().createIdentifierValue(CSSValueClone);
        case CSSPropertyWebkitBoxDirection:
            return cssValuePool().createValue(style->boxDirection());
        case CSSPropertyWebkitBoxFlex:
            return cssValuePool().createValue(style->boxFlex(), CSSPrimitiveValue::CSS_NUMBER);
        case CSSPropertyWebkitBoxFlexGroup:
            return cssValuePool().createValue(style->boxFlexGroup(), CSSPrimitiveValue::CSS_NUMBER);
        case CSSPropertyWebkitBoxLines:
            return cssValuePool().createValue(style->boxLines());
        case CSSPropertyWebkitBoxOrdinalGroup:
            return cssValuePool().createValue(style->boxOrdinalGroup(), CSSPrimitiveValue::CSS_NUMBER);
        case CSSPropertyWebkitBoxOrient:
            return cssValuePool().createValue(style->boxOrient());
        case CSSPropertyWebkitBoxPack:
            return cssValuePool().createValue(style->boxPack());
        case CSSPropertyWebkitBoxReflect:
            return valueForReflection(style->boxReflect(), *style);
        case CSSPropertyBoxShadow:
        case CSSPropertyWebkitBoxShadow:
            return valueForShadowList(style->boxShadow(), *style, true);
        case CSSPropertyCaptionSide:
            return cssValuePool().createValue(style->captionSide());
        case CSSPropertyClear:
            return cssValuePool().createValue(style->clear());
        case CSSPropertyColor:
            return cssValuePool().createColorValue(m_allowVisitedStyle ? style->visitedDependentColor(CSSPropertyColor).rgb() : style->color().rgb());
        case CSSPropertyWebkitPrintColorAdjust:
            return cssValuePool().createValue(style->printColorAdjust());
        case CSSPropertyWebkitColumnAxis:
            return cssValuePool().createValue(style->columnAxis());
        case CSSPropertyWebkitColumnCount:
            if (style->hasAutoColumnCount())
                return cssValuePool().createIdentifierValue(CSSValueAuto);
            return cssValuePool().createValue(style->columnCount(), CSSPrimitiveValue::CSS_NUMBER);
        case CSSPropertyColumnFill:
            if (RuntimeEnabledFeatures::regionBasedColumnsEnabled())
                return cssValuePool().createValue(style->columnFill());
            return nullptr;
        case CSSPropertyWebkitColumnGap:
            if (style->hasNormalColumnGap())
                return cssValuePool().createIdentifierValue(CSSValueNormal);
            return zoomAdjustedPixelValue(style->columnGap(), *style);
        case CSSPropertyWebkitColumnProgression:
            return cssValuePool().createValue(style->columnProgression());
        case CSSPropertyWebkitColumnRuleColor:
            return m_allowVisitedStyle ? cssValuePool().createColorValue(style->visitedDependentColor(CSSPropertyOutlineColor).rgb()) : currentColorOrValidColor(*style, style->columnRuleColor());
        case CSSPropertyWebkitColumnRuleStyle:
            return cssValuePool().createValue(style->columnRuleStyle());
        case CSSPropertyWebkitColumnRuleWidth:
            return zoomAdjustedPixelValue(style->columnRuleWidth(), *style);
        case CSSPropertyWebkitColumnSpan:
            return cssValuePool().createIdentifierValue(style->columnSpan() ? CSSValueAll : CSSValueNone);
        case CSSPropertyWebkitColumnBreakAfter:
            return cssValuePool().createValue(style->columnBreakAfter());
        case CSSPropertyWebkitColumnBreakBefore:
            return cssValuePool().createValue(style->columnBreakBefore());
        case CSSPropertyWebkitColumnBreakInside:
            return cssValuePool().createValue(style->columnBreakInside());
        case CSSPropertyWebkitColumnWidth:
            if (style->hasAutoColumnWidth())
                return cssValuePool().createIdentifierValue(CSSValueAuto);
            return zoomAdjustedPixelValue(style->columnWidth(), *style);
        case CSSPropertyTabSize:
            return cssValuePool().createValue(style->tabSize(), CSSPrimitiveValue::CSS_NUMBER);
        case CSSPropertyCursor: {
            RefPtrWillBeRawPtr<CSSValueList> list;
            CursorList* cursors = style->cursors();
            if (cursors && cursors->size() > 0) {
                list = CSSValueList::createCommaSeparated();
                for (unsigned i = 0; i < cursors->size(); ++i)
                    if (StyleImage* image = cursors->at(i).image())
                        list->append(image->cssValue());
            }
            RefPtrWillBeRawPtr<CSSValue> value = cssValuePool().createValue(style->cursor());
            if (list) {
                list->append(value.release());
                return list.release();
            }
            return value.release();
        }
        case CSSPropertyDirection:
            return cssValuePool().createValue(style->direction());
        case CSSPropertyDisplay:
            return cssValuePool().createValue(style->display());
        case CSSPropertyEmptyCells:
            return cssValuePool().createValue(style->emptyCells());
        case CSSPropertyAlignContent:
            return cssValuePool().createValue(style->alignContent());
        case CSSPropertyAlignItems:
            return valueForItemPositionWithOverflowAlignment(style->alignItems(), style->alignItemsOverflowAlignment());
        case CSSPropertyAlignSelf: {
            ItemPosition alignSelf = style->alignSelf();
            if (alignSelf == ItemPositionAuto) {
                Node* parent = styledNode->parentNode();
                if (parent && parent->computedStyle())
                    alignSelf = parent->computedStyle()->alignItems();
                else
                    alignSelf = ItemPositionStretch;
            }
            return valueForItemPositionWithOverflowAlignment(alignSelf, style->alignSelfOverflowAlignment());
        }
        case CSSPropertyFlex:
            return valuesForShorthandProperty(flexShorthand());
        case CSSPropertyFlexBasis:
            return cssValuePool().createValue(style->flexBasis());
        case CSSPropertyFlexDirection:
            return cssValuePool().createValue(style->flexDirection());
        case CSSPropertyFlexFlow:
            return valuesForShorthandProperty(flexFlowShorthand());
        case CSSPropertyFlexGrow:
            return cssValuePool().createValue(style->flexGrow());
        case CSSPropertyFlexShrink:
            return cssValuePool().createValue(style->flexShrink());
        case CSSPropertyFlexWrap:
            return cssValuePool().createValue(style->flexWrap());
        case CSSPropertyJustifyContent:
            return cssValuePool().createValue(style->justifyContent());
        case CSSPropertyOrder:
            return cssValuePool().createValue(style->order(), CSSPrimitiveValue::CSS_NUMBER);
        case CSSPropertyFloat:
            if (style->display() != NONE && style->hasOutOfFlowPosition())
                return cssValuePool().createIdentifierValue(CSSValueNone);
            return cssValuePool().createValue(style->floating());
        case CSSPropertyFont: {
            RefPtrWillBeRawPtr<CSSFontValue> computedFont = CSSFontValue::create();
            computedFont->style = valueForFontStyle(*style);
            computedFont->variant = valueForFontVariant(*style);
            computedFont->weight = valueForFontWeight(*style);
            computedFont->size = valueForFontSize(*style);
            computedFont->lineHeight = valueForLineHeight(*style);
            computedFont->family = valueForFontFamily(*style);
            return computedFont.release();
        }
        case CSSPropertyFontFamily: {
            RefPtrWillBeRawPtr<CSSValueList> fontFamilyList = valueForFontFamily(*style);
            // If there's only a single family, return that as a CSSPrimitiveValue.
            // NOTE: Gecko always returns this as a comma-separated CSSPrimitiveValue string.
            if (fontFamilyList->length() == 1)
                return fontFamilyList->item(0);
            return fontFamilyList.release();
        }
        case CSSPropertyFontSize:
            return valueForFontSize(*style);
        case CSSPropertyFontStyle:
            return valueForFontStyle(*style);
        case CSSPropertyFontVariant:
            return valueForFontVariant(*style);
        case CSSPropertyFontWeight:
            return valueForFontWeight(*style);
        case CSSPropertyWebkitFontFeatureSettings: {
            const FontFeatureSettings* featureSettings = style->fontDescription().featureSettings();
            if (!featureSettings || !featureSettings->size())
                return cssValuePool().createIdentifierValue(CSSValueNormal);
            RefPtrWillBeRawPtr<CSSValueList> list = CSSValueList::createCommaSeparated();
            for (unsigned i = 0; i < featureSettings->size(); ++i) {
                const FontFeature& feature = featureSettings->at(i);
                RefPtrWillBeRawPtr<CSSFontFeatureValue> featureValue = CSSFontFeatureValue::create(feature.tag(), feature.value());
                list->append(featureValue.release());
            }
            return list.release();
        }
        case CSSPropertyGridAutoFlow:
            return cssValuePool().createValue(style->gridAutoFlow());

        // Specs mention that getComputedStyle() should return the used value of the property instead of the computed
        // one for grid-definition-{rows|columns} but not for the grid-auto-{rows|columns} as things like
        // grid-auto-columns: 2fr; cannot be resolved to a value in pixels as the '2fr' means very different things
        // depending on the size of the explicit grid or the number of implicit tracks added to the grid. See
        // http://lists.w3.org/Archives/Public/www-style/2013Nov/0014.html
        case CSSPropertyGridAutoColumns:
            return specifiedValueForGridTrackSize(style->gridAutoColumns(), *style);
        case CSSPropertyGridAutoRows:
            return specifiedValueForGridTrackSize(style->gridAutoRows(), *style);

        case CSSPropertyGridTemplateColumns:
            return valueForGridTrackList(ForColumns, renderer, *style);
        case CSSPropertyGridTemplateRows:
            return valueForGridTrackList(ForRows, renderer, *style);

        case CSSPropertyGridColumnStart:
            return valueForGridPosition(style->gridColumnStart());
        case CSSPropertyGridColumnEnd:
            return valueForGridPosition(style->gridColumnEnd());
        case CSSPropertyGridRowStart:
            return valueForGridPosition(style->gridRowStart());
        case CSSPropertyGridRowEnd:
            return valueForGridPosition(style->gridRowEnd());
        case CSSPropertyGridColumn:
            return valuesForGridShorthand(gridColumnShorthand());
        case CSSPropertyGridRow:
            return valuesForGridShorthand(gridRowShorthand());
        case CSSPropertyGridArea:
            return valuesForGridShorthand(gridAreaShorthand());

        case CSSPropertyGridTemplateAreas:
            if (!style->namedGridAreaRowCount()) {
                ASSERT(!style->namedGridAreaColumnCount());
                return cssValuePool().createIdentifierValue(CSSValueNone);
            }

            return CSSGridTemplateAreasValue::create(style->namedGridArea(), style->namedGridAreaRowCount(), style->namedGridAreaColumnCount());

        case CSSPropertyHeight:
            if (renderer) {
                // According to http://www.w3.org/TR/CSS2/visudet.html#the-height-property,
                // the "height" property does not apply for non-replaced inline elements.
                if (!renderer->isReplaced() && renderer->isInline())
                    return cssValuePool().createIdentifierValue(CSSValueAuto);
                return zoomAdjustedPixelValue(sizingBox(renderer).height(), *style);
            }
            return zoomAdjustedPixelValueForLength(style->height(), *style);
        case CSSPropertyWebkitHighlight:
            if (style->highlight() == nullAtom)
                return cssValuePool().createIdentifierValue(CSSValueNone);
            return cssValuePool().createValue(style->highlight(), CSSPrimitiveValue::CSS_STRING);
        case CSSPropertyWebkitHyphenateCharacter:
            if (style->hyphenationString().isNull())
                return cssValuePool().createIdentifierValue(CSSValueAuto);
            return cssValuePool().createValue(style->hyphenationString(), CSSPrimitiveValue::CSS_STRING);
        case CSSPropertyWebkitBorderFit:
            if (style->borderFit() == BorderFitBorder)
                return cssValuePool().createIdentifierValue(CSSValueBorder);
            return cssValuePool().createIdentifierValue(CSSValueLines);
        case CSSPropertyImageRendering:
            return CSSPrimitiveValue::create(style->imageRendering());
        case CSSPropertyIsolation:
            return cssValuePool().createValue(style->isolation());
        case CSSPropertyJustifySelf:
            return valueForItemPositionWithOverflowAlignment(style->justifySelf(), style->justifySelfOverflowAlignment());
        case CSSPropertyLeft:
            return valueForPositionOffset(*style, CSSPropertyLeft, renderer);
        case CSSPropertyLetterSpacing:
            if (!style->letterSpacing())
                return cssValuePool().createIdentifierValue(CSSValueNormal);
            return zoomAdjustedPixelValue(style->letterSpacing(), *style);
        case CSSPropertyWebkitLineClamp:
            if (style->lineClamp().isNone())
                return cssValuePool().createIdentifierValue(CSSValueNone);
            return cssValuePool().createValue(style->lineClamp().value(), style->lineClamp().isPercentage() ? CSSPrimitiveValue::CSS_PERCENTAGE : CSSPrimitiveValue::CSS_NUMBER);
        case CSSPropertyLineHeight:
            return valueForLineHeight(*style);
        case CSSPropertyListStyleImage:
            if (style->listStyleImage())
                return style->listStyleImage()->cssValue();
            return cssValuePool().createIdentifierValue(CSSValueNone);
        case CSSPropertyListStylePosition:
            return cssValuePool().createValue(style->listStylePosition());
        case CSSPropertyListStyleType:
            return cssValuePool().createValue(style->listStyleType());
        case CSSPropertyWebkitLocale:
            if (style->locale().isNull())
                return cssValuePool().createIdentifierValue(CSSValueAuto);
            return cssValuePool().createValue(style->locale(), CSSPrimitiveValue::CSS_STRING);
        case CSSPropertyMarginTop: {
            Length marginTop = style->marginTop();
            if (marginTop.isFixed() || !renderer || !renderer->isBox())
                return zoomAdjustedPixelValueForLength(marginTop, *style);
            return zoomAdjustedPixelValue(toRenderBox(renderer)->marginTop(), *style);
        }
        case CSSPropertyMarginRight: {
            Length marginRight = style->marginRight();
            if (marginRight.isFixed() || !renderer || !renderer->isBox())
                return zoomAdjustedPixelValueForLength(marginRight, *style);
            float value;
            if (marginRight.isPercent()) {
                // RenderBox gives a marginRight() that is the distance between the right-edge of the child box
                // and the right-edge of the containing box, when display == BLOCK. Let's calculate the absolute
                // value of the specified margin-right % instead of relying on RenderBox's marginRight() value.
                value = minimumValueForLength(marginRight, toRenderBox(renderer)->containingBlockLogicalWidthForContent());
            } else {
                value = toRenderBox(renderer)->marginRight();
            }
            return zoomAdjustedPixelValue(value, *style);
        }
        case CSSPropertyMarginBottom: {
            Length marginBottom = style->marginBottom();
            if (marginBottom.isFixed() || !renderer || !renderer->isBox())
                return zoomAdjustedPixelValueForLength(marginBottom, *style);
            return zoomAdjustedPixelValue(toRenderBox(renderer)->marginBottom(), *style);
        }
        case CSSPropertyMarginLeft: {
            Length marginLeft = style->marginLeft();
            if (marginLeft.isFixed() || !renderer || !renderer->isBox())
                return zoomAdjustedPixelValueForLength(marginLeft, *style);
            return zoomAdjustedPixelValue(toRenderBox(renderer)->marginLeft(), *style);
        }
        case CSSPropertyWebkitUserModify:
            return cssValuePool().createValue(style->userModify());
        case CSSPropertyMaxHeight: {
            const Length& maxHeight = style->maxHeight();
            if (maxHeight.isUndefined())
                return cssValuePool().createIdentifierValue(CSSValueNone);
            return zoomAdjustedPixelValueForLength(maxHeight, *style);
        }
        case CSSPropertyMaxWidth: {
            const Length& maxWidth = style->maxWidth();
            if (maxWidth.isUndefined())
                return cssValuePool().createIdentifierValue(CSSValueNone);
            return zoomAdjustedPixelValueForLength(maxWidth, *style);
        }
        case CSSPropertyMinHeight:
            // FIXME: For flex-items, min-height:auto should compute to min-content.
            if (style->minHeight().isAuto())
                return zoomAdjustedPixelValue(0, *style);
            return zoomAdjustedPixelValueForLength(style->minHeight(), *style);
        case CSSPropertyMinWidth:
            // FIXME: For flex-items, min-width:auto should compute to min-content.
            if (style->minWidth().isAuto())
                return zoomAdjustedPixelValue(0, *style);
            return zoomAdjustedPixelValueForLength(style->minWidth(), *style);
        case CSSPropertyObjectFit:
            return cssValuePool().createValue(style->objectFit());
        case CSSPropertyObjectPosition:
            return cssValuePool().createValue(
                Pair::create(
                    zoomAdjustedPixelValueForLength(style->objectPosition().x(), *style),
                    zoomAdjustedPixelValueForLength(style->objectPosition().y(), *style),
                    Pair::KeepIdenticalValues));
        case CSSPropertyOpacity:
            return cssValuePool().createValue(style->opacity(), CSSPrimitiveValue::CSS_NUMBER);
        case CSSPropertyOrphans:
            if (style->hasAutoOrphans())
                return cssValuePool().createIdentifierValue(CSSValueAuto);
            return cssValuePool().createValue(style->orphans(), CSSPrimitiveValue::CSS_NUMBER);
        case CSSPropertyOutlineColor:
            return m_allowVisitedStyle ? cssValuePool().createColorValue(style->visitedDependentColor(CSSPropertyOutlineColor).rgb()) : currentColorOrValidColor(*style, style->outlineColor());
        case CSSPropertyOutlineOffset:
            return zoomAdjustedPixelValue(style->outlineOffset(), *style);
        case CSSPropertyOutlineStyle:
            if (style->outlineStyleIsAuto())
                return cssValuePool().createIdentifierValue(CSSValueAuto);
            return cssValuePool().createValue(style->outlineStyle());
        case CSSPropertyOutlineWidth:
            return zoomAdjustedPixelValue(style->outlineWidth(), *style);
        case CSSPropertyOverflow:
            return cssValuePool().createValue(max(style->overflowX(), style->overflowY()));
        case CSSPropertyOverflowWrap:
            return cssValuePool().createValue(style->overflowWrap());
        case CSSPropertyOverflowX:
            return cssValuePool().createValue(style->overflowX());
        case CSSPropertyOverflowY:
            return cssValuePool().createValue(style->overflowY());
        case CSSPropertyPaddingTop: {
            Length paddingTop = style->paddingTop();
            if (paddingTop.isFixed() || !renderer || !renderer->isBox())
                return zoomAdjustedPixelValueForLength(paddingTop, *style);
            return zoomAdjustedPixelValue(toRenderBox(renderer)->computedCSSPaddingTop(), *style);
        }
        case CSSPropertyPaddingRight: {
            Length paddingRight = style->paddingRight();
            if (paddingRight.isFixed() || !renderer || !renderer->isBox())
                return zoomAdjustedPixelValueForLength(paddingRight, *style);
            return zoomAdjustedPixelValue(toRenderBox(renderer)->computedCSSPaddingRight(), *style);
        }
        case CSSPropertyPaddingBottom: {
            Length paddingBottom = style->paddingBottom();
            if (paddingBottom.isFixed() || !renderer || !renderer->isBox())
                return zoomAdjustedPixelValueForLength(paddingBottom, *style);
            return zoomAdjustedPixelValue(toRenderBox(renderer)->computedCSSPaddingBottom(), *style);
        }
        case CSSPropertyPaddingLeft: {
            Length paddingLeft = style->paddingLeft();
            if (paddingLeft.isFixed() || !renderer || !renderer->isBox())
                return zoomAdjustedPixelValueForLength(paddingLeft, *style);
            return zoomAdjustedPixelValue(toRenderBox(renderer)->computedCSSPaddingLeft(), *style);
        }
        case CSSPropertyPageBreakAfter:
            return cssValuePool().createValue(style->pageBreakAfter());
        case CSSPropertyPageBreakBefore:
            return cssValuePool().createValue(style->pageBreakBefore());
        case CSSPropertyPageBreakInside: {
            EPageBreak pageBreak = style->pageBreakInside();
            ASSERT(pageBreak != PBALWAYS);
            if (pageBreak == PBALWAYS)
                return nullptr;
            return cssValuePool().createValue(style->pageBreakInside());
        }
        case CSSPropertyPosition:
            return cssValuePool().createValue(style->position());
        case CSSPropertyRight:
            return valueForPositionOffset(*style, CSSPropertyRight, renderer);
        case CSSPropertyWebkitRubyPosition:
            return cssValuePool().createValue(style->rubyPosition());
        case CSSPropertyScrollBehavior:
            return cssValuePool().createValue(style->scrollBehavior());
        case CSSPropertyTableLayout:
            return cssValuePool().createValue(style->tableLayout());
        case CSSPropertyTextAlign:
            return cssValuePool().createValue(style->textAlign());
        case CSSPropertyTextAlignLast:
            return cssValuePool().createValue(style->textAlignLast());
        case CSSPropertyTextDecoration:
            return valuesForShorthandProperty(textDecorationShorthand());
        case CSSPropertyTextDecorationLine:
            return renderTextDecorationFlagsToCSSValue(style->textDecoration());
        case CSSPropertyTextDecorationStyle:
            return valueForTextDecorationStyle(style->textDecorationStyle());
        case CSSPropertyTextDecorationColor:
            return currentColorOrValidColor(*style, style->textDecorationColor());
        case CSSPropertyTextJustify:
            return cssValuePool().createValue(style->textJustify());
        case CSSPropertyTextUnderlinePosition:
            return cssValuePool().createValue(style->textUnderlinePosition());
        case CSSPropertyWebkitTextDecorationsInEffect:
            return renderTextDecorationFlagsToCSSValue(style->textDecorationsInEffect());
        case CSSPropertyWebkitTextFillColor:
            return currentColorOrValidColor(*style, style->textFillColor());
        case CSSPropertyWebkitTextEmphasisColor:
            return currentColorOrValidColor(*style, style->textEmphasisColor());
        case CSSPropertyWebkitTextEmphasisPosition:
            return cssValuePool().createValue(style->textEmphasisPosition());
        case CSSPropertyWebkitTextEmphasisStyle:
            switch (style->textEmphasisMark()) {
            case TextEmphasisMarkNone:
                return cssValuePool().createIdentifierValue(CSSValueNone);
            case TextEmphasisMarkCustom:
                return cssValuePool().createValue(style->textEmphasisCustomMark(), CSSPrimitiveValue::CSS_STRING);
            case TextEmphasisMarkAuto:
                ASSERT_NOT_REACHED();
                // Fall through
            case TextEmphasisMarkDot:
            case TextEmphasisMarkCircle:
            case TextEmphasisMarkDoubleCircle:
            case TextEmphasisMarkTriangle:
            case TextEmphasisMarkSesame: {
                RefPtrWillBeRawPtr<CSSValueList> list = CSSValueList::createSpaceSeparated();
                list->append(cssValuePool().createValue(style->textEmphasisFill()));
                list->append(cssValuePool().createValue(style->textEmphasisMark()));
                return list.release();
            }
            }
        case CSSPropertyTextIndent: {
            RefPtrWillBeRawPtr<CSSValue> textIndent = zoomAdjustedPixelValueForLength(style->textIndent(), *style);
            if (RuntimeEnabledFeatures::css3TextEnabled() && style->textIndentLine() == TextIndentEachLine) {
                RefPtrWillBeRawPtr<CSSValueList> list = CSSValueList::createSpaceSeparated();
                list->append(textIndent.release());
                list->append(cssValuePool().createIdentifierValue(CSSValueEachLine));
                return list.release();
            }
            return textIndent.release();
        }
        case CSSPropertyTextShadow:
            return valueForShadowList(style->textShadow(), *style, false);
        case CSSPropertyTextRendering:
            return cssValuePool().createValue(style->fontDescription().textRendering());
        case CSSPropertyTextOverflow:
            if (style->textOverflow())
                return cssValuePool().createIdentifierValue(CSSValueEllipsis);
            return cssValuePool().createIdentifierValue(CSSValueClip);
        case CSSPropertyWebkitTextSecurity:
            return cssValuePool().createValue(style->textSecurity());
        case CSSPropertyWebkitTextStrokeColor:
            return currentColorOrValidColor(*style, style->textStrokeColor());
        case CSSPropertyWebkitTextStrokeWidth:
            return zoomAdjustedPixelValue(style->textStrokeWidth(), *style);
        case CSSPropertyTextTransform:
            return cssValuePool().createValue(style->textTransform());
        case CSSPropertyTop:
            return valueForPositionOffset(*style, CSSPropertyTop, renderer);
        case CSSPropertyTouchAction:
            return touchActionFlagsToCSSValue(style->touchAction());
        case CSSPropertyTouchActionDelay:
            return cssValuePool().createValue(style->touchActionDelay());
        case CSSPropertyUnicodeBidi:
            return cssValuePool().createValue(style->unicodeBidi());
        case CSSPropertyVerticalAlign:
            switch (style->verticalAlign()) {
                case BASELINE:
                    return cssValuePool().createIdentifierValue(CSSValueBaseline);
                case MIDDLE:
                    return cssValuePool().createIdentifierValue(CSSValueMiddle);
                case SUB:
                    return cssValuePool().createIdentifierValue(CSSValueSub);
                case SUPER:
                    return cssValuePool().createIdentifierValue(CSSValueSuper);
                case TEXT_TOP:
                    return cssValuePool().createIdentifierValue(CSSValueTextTop);
                case TEXT_BOTTOM:
                    return cssValuePool().createIdentifierValue(CSSValueTextBottom);
                case TOP:
                    return cssValuePool().createIdentifierValue(CSSValueTop);
                case BOTTOM:
                    return cssValuePool().createIdentifierValue(CSSValueBottom);
                case BASELINE_MIDDLE:
                    return cssValuePool().createIdentifierValue(CSSValueWebkitBaselineMiddle);
                case LENGTH:
                    return cssValuePool().createValue(style->verticalAlignLength());
            }
            ASSERT_NOT_REACHED();
            return nullptr;
        case CSSPropertyVisibility:
            return cssValuePool().createValue(style->visibility());
        case CSSPropertyWhiteSpace:
            return cssValuePool().createValue(style->whiteSpace());
        case CSSPropertyWidows:
            if (style->hasAutoWidows())
                return cssValuePool().createIdentifierValue(CSSValueAuto);
            return cssValuePool().createValue(style->widows(), CSSPrimitiveValue::CSS_NUMBER);
        case CSSPropertyWidth:
            if (renderer) {
                // According to http://www.w3.org/TR/CSS2/visudet.html#the-width-property,
                // the "width" property does not apply for non-replaced inline elements.
                if (!renderer->isReplaced() && renderer->isInline())
                    return cssValuePool().createIdentifierValue(CSSValueAuto);
                return zoomAdjustedPixelValue(sizingBox(renderer).width(), *style);
            }
            return zoomAdjustedPixelValueForLength(style->width(), *style);
        case CSSPropertyWillChange:
            return valueForWillChange(style->willChangeProperties(), style->willChangeContents(), style->willChangeScrollPosition());
        case CSSPropertyWordBreak:
            return cssValuePool().createValue(style->wordBreak());
        case CSSPropertyWordSpacing:
            return zoomAdjustedPixelValue(style->wordSpacing(), *style);
        case CSSPropertyWordWrap:
            return cssValuePool().createValue(style->overflowWrap());
        case CSSPropertyWebkitLineBreak:
            return cssValuePool().createValue(style->lineBreak());
        case CSSPropertyResize:
            return cssValuePool().createValue(style->resize());
        case CSSPropertyFontKerning:
            return cssValuePool().createValue(style->fontDescription().kerning());
        case CSSPropertyWebkitFontSmoothing:
            return cssValuePool().createValue(style->fontDescription().fontSmoothing());
        case CSSPropertyFontVariantLigatures: {
            FontDescription::LigaturesState commonLigaturesState = style->fontDescription().commonLigaturesState();
            FontDescription::LigaturesState discretionaryLigaturesState = style->fontDescription().discretionaryLigaturesState();
            FontDescription::LigaturesState historicalLigaturesState = style->fontDescription().historicalLigaturesState();
            FontDescription::LigaturesState contextualLigaturesState = style->fontDescription().contextualLigaturesState();
            if (commonLigaturesState == FontDescription::NormalLigaturesState && discretionaryLigaturesState == FontDescription::NormalLigaturesState
                && historicalLigaturesState == FontDescription::NormalLigaturesState && contextualLigaturesState == FontDescription::NormalLigaturesState)
                return cssValuePool().createIdentifierValue(CSSValueNormal);

            RefPtrWillBeRawPtr<CSSValueList> valueList = CSSValueList::createSpaceSeparated();
            if (commonLigaturesState != FontDescription::NormalLigaturesState)
                valueList->append(cssValuePool().createIdentifierValue(commonLigaturesState == FontDescription::DisabledLigaturesState ? CSSValueNoCommonLigatures : CSSValueCommonLigatures));
            if (discretionaryLigaturesState != FontDescription::NormalLigaturesState)
                valueList->append(cssValuePool().createIdentifierValue(discretionaryLigaturesState == FontDescription::DisabledLigaturesState ? CSSValueNoDiscretionaryLigatures : CSSValueDiscretionaryLigatures));
            if (historicalLigaturesState != FontDescription::NormalLigaturesState)
                valueList->append(cssValuePool().createIdentifierValue(historicalLigaturesState == FontDescription::DisabledLigaturesState ? CSSValueNoHistoricalLigatures : CSSValueHistoricalLigatures));
            if (contextualLigaturesState != FontDescription::NormalLigaturesState)
                valueList->append(cssValuePool().createIdentifierValue(contextualLigaturesState == FontDescription::DisabledLigaturesState ? CSSValueNoContextual : CSSValueContextual));
            return valueList;
        }
        case CSSPropertyZIndex:
            if (style->hasAutoZIndex())
                return cssValuePool().createIdentifierValue(CSSValueAuto);
            return cssValuePool().createValue(style->zIndex(), CSSPrimitiveValue::CSS_NUMBER);
        case CSSPropertyZoom:
            return cssValuePool().createValue(style->zoom(), CSSPrimitiveValue::CSS_NUMBER);
        case CSSPropertyBoxSizing:
            if (style->boxSizing() == CONTENT_BOX)
                return cssValuePool().createIdentifierValue(CSSValueContentBox);
            return cssValuePool().createIdentifierValue(CSSValueBorderBox);
        case CSSPropertyWebkitAppRegion:
            return cssValuePool().createIdentifierValue(style->getDraggableRegionMode() == DraggableRegionDrag ? CSSValueDrag : CSSValueNoDrag);
        case CSSPropertyAnimationDelay:
            ASSERT(RuntimeEnabledFeatures::cssAnimationUnprefixedEnabled());
        case CSSPropertyWebkitAnimationDelay:
            return valueForAnimationDelay(style->animations());
        case CSSPropertyAnimationDirection:
            ASSERT(RuntimeEnabledFeatures::cssAnimationUnprefixedEnabled());
        case CSSPropertyWebkitAnimationDirection: {
            RefPtrWillBeRawPtr<CSSValueList> list = CSSValueList::createCommaSeparated();
            const CSSAnimationDataList* t = style->animations();
            if (t) {
                for (size_t i = 0; i < t->size(); ++i)
                    list->append(valueForAnimationDirection(t->animation(i)->direction()));
            } else
                list->append(cssValuePool().createIdentifierValue(CSSValueNormal));
            return list.release();
        }
        case CSSPropertyAnimationDuration:
            ASSERT(RuntimeEnabledFeatures::cssAnimationUnprefixedEnabled());
        case CSSPropertyWebkitAnimationDuration:
            return valueForAnimationDuration(style->animations());
        case CSSPropertyAnimationFillMode:
            ASSERT(RuntimeEnabledFeatures::cssAnimationUnprefixedEnabled());
        case CSSPropertyWebkitAnimationFillMode: {
            RefPtrWillBeRawPtr<CSSValueList> list = CSSValueList::createCommaSeparated();
            const CSSAnimationDataList* t = style->animations();
            if (t) {
                for (size_t i = 0; i < t->size(); ++i)
                    list->append(valueForAnimationFillMode(t->animation(i)->fillMode()));
            } else
                list->append(cssValuePool().createIdentifierValue(CSSValueNone));
            return list.release();
        }
        case CSSPropertyAnimationIterationCount:
            ASSERT(RuntimeEnabledFeatures::cssAnimationUnprefixedEnabled());
        case CSSPropertyWebkitAnimationIterationCount: {
            RefPtrWillBeRawPtr<CSSValueList> list = CSSValueList::createCommaSeparated();
            const CSSAnimationDataList* t = style->animations();
            if (t) {
                for (size_t i = 0; i < t->size(); ++i) {
                    double iterationCount = t->animation(i)->iterationCount();
                    if (iterationCount == CSSAnimationData::IterationCountInfinite)
                        list->append(cssValuePool().createIdentifierValue(CSSValueInfinite));
                    else
                        list->append(cssValuePool().createValue(iterationCount, CSSPrimitiveValue::CSS_NUMBER));
                }
            } else
                list->append(cssValuePool().createValue(CSSAnimationData::initialAnimationIterationCount(), CSSPrimitiveValue::CSS_NUMBER));
            return list.release();
        }
        case CSSPropertyAnimationName:
            ASSERT(RuntimeEnabledFeatures::cssAnimationUnprefixedEnabled());
        case CSSPropertyWebkitAnimationName: {
            RefPtrWillBeRawPtr<CSSValueList> list = CSSValueList::createCommaSeparated();
            const CSSAnimationDataList* t = style->animations();
            if (t) {
                for (size_t i = 0; i < t->size(); ++i)
                    list->append(cssValuePool().createValue(t->animation(i)->name(), CSSPrimitiveValue::CSS_STRING));
            } else
                list->append(cssValuePool().createIdentifierValue(CSSValueNone));
            return list.release();
        }
        case CSSPropertyAnimationPlayState:
            ASSERT(RuntimeEnabledFeatures::cssAnimationUnprefixedEnabled());
        case CSSPropertyWebkitAnimationPlayState: {
            RefPtrWillBeRawPtr<CSSValueList> list = CSSValueList::createCommaSeparated();
            const CSSAnimationDataList* t = style->animations();
            if (t) {
                for (size_t i = 0; i < t->size(); ++i) {
                    int prop = t->animation(i)->playState();
                    if (prop == AnimPlayStatePlaying)
                        list->append(cssValuePool().createIdentifierValue(CSSValueRunning));
                    else
                        list->append(cssValuePool().createIdentifierValue(CSSValuePaused));
                }
            } else
                list->append(cssValuePool().createIdentifierValue(CSSValueRunning));
            return list.release();
        }
        case CSSPropertyAnimationTimingFunction:
            ASSERT(RuntimeEnabledFeatures::cssAnimationUnprefixedEnabled());
        case CSSPropertyWebkitAnimationTimingFunction:
            return valueForAnimationTimingFunction(style->animations());
        case CSSPropertyAnimation:
        case CSSPropertyWebkitAnimation: {
            const CSSAnimationDataList* animations = style->animations();
            if (animations) {
                RefPtrWillBeRawPtr<CSSValueList> animationsList = CSSValueList::createCommaSeparated();
                for (size_t i = 0; i < animations->size(); ++i) {
                    RefPtrWillBeRawPtr<CSSValueList> list = CSSValueList::createSpaceSeparated();
                    const CSSAnimationData* animation = animations->animation(i);
                    list->append(cssValuePool().createValue(animation->name(), CSSPrimitiveValue::CSS_STRING));
                    list->append(cssValuePool().createValue(animation->duration(), CSSPrimitiveValue::CSS_S));
                    list->append(createTimingFunctionValue(animation->timingFunction()));
                    list->append(cssValuePool().createValue(animation->delay(), CSSPrimitiveValue::CSS_S));
                    if (animation->iterationCount() == CSSAnimationData::IterationCountInfinite)
                        list->append(cssValuePool().createIdentifierValue(CSSValueInfinite));
                    else
                        list->append(cssValuePool().createValue(animation->iterationCount(), CSSPrimitiveValue::CSS_NUMBER));
                    list->append(valueForAnimationDirection(animation->direction()));
                    list->append(valueForAnimationFillMode(animation->fillMode()));
                    if (animation->playState() == AnimPlayStatePaused)
                        list->append(cssValuePool().createIdentifierValue(CSSValuePaused));
                    else
                        list->append(cssValuePool().createIdentifierValue(CSSValueRunning));
                    animationsList->append(list);
                }
                return animationsList.release();
            }

            RefPtrWillBeRawPtr<CSSValueList> list = CSSValueList::createSpaceSeparated();
            // animation-name default value.
            list->append(cssValuePool().createIdentifierValue(CSSValueNone));
            list->append(cssValuePool().createValue(CSSAnimationData::initialAnimationDuration(), CSSPrimitiveValue::CSS_S));
            list->append(createTimingFunctionValue(CSSAnimationData::initialAnimationTimingFunction().get()));
            list->append(cssValuePool().createValue(CSSAnimationData::initialAnimationDelay(), CSSPrimitiveValue::CSS_S));
            list->append(cssValuePool().createValue(CSSAnimationData::initialAnimationIterationCount(), CSSPrimitiveValue::CSS_NUMBER));
            list->append(valueForAnimationDirection(CSSAnimationData::initialAnimationDirection()));
            list->append(valueForAnimationFillMode(CSSAnimationData::initialAnimationFillMode()));
            // Initial animation-play-state.
            list->append(cssValuePool().createIdentifierValue(CSSValueRunning));
            return list.release();
        }
        case CSSPropertyWebkitAppearance:
            return cssValuePool().createValue(style->appearance());
        case CSSPropertyWebkitAspectRatio:
            if (!style->hasAspectRatio())
                return cssValuePool().createIdentifierValue(CSSValueNone);
            return CSSAspectRatioValue::create(style->aspectRatioNumerator(), style->aspectRatioDenominator());
        case CSSPropertyWebkitBackfaceVisibility:
            return cssValuePool().createIdentifierValue((style->backfaceVisibility() == BackfaceVisibilityHidden) ? CSSValueHidden : CSSValueVisible);
        case CSSPropertyWebkitBorderImage:
            return valueForNinePieceImage(style->borderImage(), *style);
        case CSSPropertyBorderImageOutset:
            return valueForNinePieceImageQuad(style->borderImage().outset(), *style);
        case CSSPropertyBorderImageRepeat:
            return valueForNinePieceImageRepeat(style->borderImage());
        case CSSPropertyBorderImageSlice:
            return valueForNinePieceImageSlice(style->borderImage());
        case CSSPropertyBorderImageWidth:
            return valueForNinePieceImageQuad(style->borderImage().borderSlices(), *style);
        case CSSPropertyWebkitMaskBoxImage:
            return valueForNinePieceImage(style->maskBoxImage(), *style);
        case CSSPropertyWebkitMaskBoxImageOutset:
            return valueForNinePieceImageQuad(style->maskBoxImage().outset(), *style);
        case CSSPropertyWebkitMaskBoxImageRepeat:
            return valueForNinePieceImageRepeat(style->maskBoxImage());
        case CSSPropertyWebkitMaskBoxImageSlice:
            return valueForNinePieceImageSlice(style->maskBoxImage());
        case CSSPropertyWebkitMaskBoxImageWidth:
            return valueForNinePieceImageQuad(style->maskBoxImage().borderSlices(), *style);
        case CSSPropertyWebkitMaskBoxImageSource:
            if (style->maskBoxImageSource())
                return style->maskBoxImageSource()->cssValue();
            return cssValuePool().createIdentifierValue(CSSValueNone);
        case CSSPropertyWebkitFontSizeDelta:
            // Not a real style property -- used by the editing engine -- so has no computed value.
            break;
        case CSSPropertyWebkitMarginBottomCollapse:
        case CSSPropertyWebkitMarginAfterCollapse:
            return cssValuePool().createValue(style->marginAfterCollapse());
        case CSSPropertyWebkitMarginTopCollapse:
        case CSSPropertyWebkitMarginBeforeCollapse:
            return cssValuePool().createValue(style->marginBeforeCollapse());
        case CSSPropertyWebkitPerspective:
            if (!style->hasPerspective())
                return cssValuePool().createIdentifierValue(CSSValueNone);
            return zoomAdjustedPixelValue(style->perspective(), *style);
        case CSSPropertyWebkitPerspectiveOrigin: {
            RefPtrWillBeRawPtr<CSSValueList> list = CSSValueList::createSpaceSeparated();
            if (renderer) {
                LayoutRect box;
                if (renderer->isBox())
                    box = toRenderBox(renderer)->borderBoxRect();

                list->append(zoomAdjustedPixelValue(minimumValueForLength(style->perspectiveOriginX(), box.width()), *style));
                list->append(zoomAdjustedPixelValue(minimumValueForLength(style->perspectiveOriginY(), box.height()), *style));
            }
            else {
                list->append(zoomAdjustedPixelValueForLength(style->perspectiveOriginX(), *style));
                list->append(zoomAdjustedPixelValueForLength(style->perspectiveOriginY(), *style));

            }
            return list.release();
        }
        case CSSPropertyWebkitRtlOrdering:
            return cssValuePool().createIdentifierValue(style->rtlOrdering() ? CSSValueVisual : CSSValueLogical);
        case CSSPropertyWebkitTapHighlightColor:
            return currentColorOrValidColor(*style, style->tapHighlightColor());
        case CSSPropertyWebkitUserDrag:
            return cssValuePool().createValue(style->userDrag());
        case CSSPropertyWebkitUserSelect:
            return cssValuePool().createValue(style->userSelect());
        case CSSPropertyBorderBottomLeftRadius:
            return valueForBorderRadiusCorner(style->borderBottomLeftRadius(), *style);
        case CSSPropertyBorderBottomRightRadius:
            return valueForBorderRadiusCorner(style->borderBottomRightRadius(), *style);
        case CSSPropertyBorderTopLeftRadius:
            return valueForBorderRadiusCorner(style->borderTopLeftRadius(), *style);
        case CSSPropertyBorderTopRightRadius:
            return valueForBorderRadiusCorner(style->borderTopRightRadius(), *style);
        case CSSPropertyClip: {
            if (!style->hasClip())
                return cssValuePool().createIdentifierValue(CSSValueAuto);
            RefPtrWillBeRawPtr<Rect> rect = Rect::create();
            rect->setTop(zoomAdjustedPixelValue(style->clip().top().value(), *style));
            rect->setRight(zoomAdjustedPixelValue(style->clip().right().value(), *style));
            rect->setBottom(zoomAdjustedPixelValue(style->clip().bottom().value(), *style));
            rect->setLeft(zoomAdjustedPixelValue(style->clip().left().value(), *style));
            return cssValuePool().createValue(rect.release());
        }
        case CSSPropertySpeak:
            return cssValuePool().createValue(style->speak());
        case CSSPropertyWebkitTransform:
            return computedTransform(renderer, *style);
        case CSSPropertyWebkitTransformOrigin: {
            RefPtrWillBeRawPtr<CSSValueList> list = CSSValueList::createSpaceSeparated();
            if (renderer) {
                LayoutRect box;
                if (renderer->isBox())
                    box = toRenderBox(renderer)->borderBoxRect();

                list->append(zoomAdjustedPixelValue(minimumValueForLength(style->transformOriginX(), box.width()), *style));
                list->append(zoomAdjustedPixelValue(minimumValueForLength(style->transformOriginY(), box.height()), *style));
                if (style->transformOriginZ() != 0)
                    list->append(zoomAdjustedPixelValue(style->transformOriginZ(), *style));
            } else {
                list->append(zoomAdjustedPixelValueForLength(style->transformOriginX(), *style));
                list->append(zoomAdjustedPixelValueForLength(style->transformOriginY(), *style));
                if (style->transformOriginZ() != 0)
                    list->append(zoomAdjustedPixelValue(style->transformOriginZ(), *style));
            }
            return list.release();
        }
        case CSSPropertyWebkitTransformStyle:
            return cssValuePool().createIdentifierValue((style->transformStyle3D() == TransformStyle3DPreserve3D) ? CSSValuePreserve3d : CSSValueFlat);
        case CSSPropertyTransitionDelay:
        case CSSPropertyWebkitTransitionDelay:
            return valueForAnimationDelay(style->transitions());
        case CSSPropertyTransitionDuration:
        case CSSPropertyWebkitTransitionDuration:
            return valueForAnimationDuration(style->transitions());
        case CSSPropertyTransitionProperty:
        case CSSPropertyWebkitTransitionProperty:
            return valueForTransitionProperty(style->transitions());
        case CSSPropertyTransitionTimingFunction:
        case CSSPropertyWebkitTransitionTimingFunction:
            return valueForAnimationTimingFunction(style->transitions());
        case CSSPropertyTransition:
        case CSSPropertyWebkitTransition: {
            const CSSAnimationDataList* animList = style->transitions();
            if (animList) {
                RefPtrWillBeRawPtr<CSSValueList> transitionsList = CSSValueList::createCommaSeparated();
                for (size_t i = 0; i < animList->size(); ++i) {
                    RefPtrWillBeRawPtr<CSSValueList> list = CSSValueList::createSpaceSeparated();
                    const CSSAnimationData* animation = animList->animation(i);
                    list->append(createTransitionPropertyValue(animation));
                    list->append(cssValuePool().createValue(animation->duration(), CSSPrimitiveValue::CSS_S));
                    list->append(createTimingFunctionValue(animation->timingFunction()));
                    list->append(cssValuePool().createValue(animation->delay(), CSSPrimitiveValue::CSS_S));
                    transitionsList->append(list);
                }
                return transitionsList.release();
            }

            RefPtrWillBeRawPtr<CSSValueList> list = CSSValueList::createSpaceSeparated();
            // transition-property default value.
            list->append(cssValuePool().createIdentifierValue(CSSValueAll));
            list->append(cssValuePool().createValue(CSSAnimationData::initialAnimationDuration(), CSSPrimitiveValue::CSS_S));
            list->append(createTimingFunctionValue(CSSAnimationData::initialAnimationTimingFunction().get()));
            list->append(cssValuePool().createValue(CSSAnimationData::initialAnimationDelay(), CSSPrimitiveValue::CSS_S));
            return list.release();
        }
        case CSSPropertyPointerEvents:
            return cssValuePool().createValue(style->pointerEvents());
        case CSSPropertyWebkitWritingMode:
            return cssValuePool().createValue(style->writingMode());
        case CSSPropertyWebkitTextCombine:
            return cssValuePool().createValue(style->textCombine());
        case CSSPropertyWebkitTextOrientation:
            return CSSPrimitiveValue::create(style->textOrientation());
        case CSSPropertyWebkitLineBoxContain:
            return createLineBoxContainValue(style->lineBoxContain());
        case CSSPropertyContent:
            return valueForContentData(*style);
        case CSSPropertyCounterIncrement:
            return valueForCounterDirectives(*style, propertyID);
        case CSSPropertyCounterReset:
            return valueForCounterDirectives(*style, propertyID);
        case CSSPropertyWebkitClipPath:
            if (ClipPathOperation* operation = style->clipPath()) {
                if (operation->type() == ClipPathOperation::SHAPE)
                    return valueForBasicShape(*style, toShapeClipPathOperation(operation)->basicShape());
                if (operation->type() == ClipPathOperation::REFERENCE)
                    return CSSPrimitiveValue::create(toReferenceClipPathOperation(operation)->url(), CSSPrimitiveValue::CSS_URI);
            }
            return cssValuePool().createIdentifierValue(CSSValueNone);
        case CSSPropertyWebkitWrapFlow:
            return cssValuePool().createValue(style->wrapFlow());
        case CSSPropertyShapeMargin:
            return cssValuePool().createValue(style->shapeMargin());
        case CSSPropertyShapePadding:
            return cssValuePool().createValue(style->shapePadding());
        case CSSPropertyShapeImageThreshold:
            return cssValuePool().createValue(style->shapeImageThreshold(), CSSPrimitiveValue::CSS_NUMBER);
        case CSSPropertyShapeInside:
            return valueForShape(*style, style->shapeInside());
        case CSSPropertyShapeOutside:
            return valueForShape(*style, style->shapeOutside());
        case CSSPropertyWebkitWrapThrough:
            return cssValuePool().createValue(style->wrapThrough());
        case CSSPropertyWebkitFilter:
            return valueForFilter(renderer, *style);
        case CSSPropertyMixBlendMode:
            return cssValuePool().createValue(style->blendMode());

        case CSSPropertyBackgroundBlendMode: {
            const FillLayer* layers = style->backgroundLayers();
            if (!layers->next())
                return cssValuePool().createValue(layers->blendMode());

            RefPtrWillBeRawPtr<CSSValueList> list = CSSValueList::createCommaSeparated();
            for (const FillLayer* currLayer = layers; currLayer; currLayer = currLayer->next())
                list->append(cssValuePool().createValue(currLayer->blendMode()));

            return list.release();
        }
        case CSSPropertyBackground:
            return valuesForBackgroundShorthand();
        case CSSPropertyBorder: {
            RefPtrWillBeRawPtr<CSSValue> value = getPropertyCSSValue(CSSPropertyBorderTop, DoNotUpdateLayout);
            const CSSPropertyID properties[3] = { CSSPropertyBorderRight, CSSPropertyBorderBottom,
                                        CSSPropertyBorderLeft };
            for (size_t i = 0; i < WTF_ARRAY_LENGTH(properties); ++i) {
                if (!compareCSSValuePtr<CSSValue>(value, getPropertyCSSValue(properties[i], DoNotUpdateLayout)))
                    return nullptr;
            }
            return value.release();
        }
        case CSSPropertyBorderBottom:
            return valuesForShorthandProperty(borderBottomShorthand());
        case CSSPropertyBorderColor:
            return valuesForSidesShorthand(borderColorShorthand());
        case CSSPropertyBorderLeft:
            return valuesForShorthandProperty(borderLeftShorthand());
        case CSSPropertyBorderImage:
            return valueForNinePieceImage(style->borderImage(), *style);
        case CSSPropertyBorderRadius:
            return valueForBorderRadiusShorthand(*style);
        case CSSPropertyBorderRight:
            return valuesForShorthandProperty(borderRightShorthand());
        case CSSPropertyBorderStyle:
            return valuesForSidesShorthand(borderStyleShorthand());
        case CSSPropertyBorderTop:
            return valuesForShorthandProperty(borderTopShorthand());
        case CSSPropertyBorderWidth:
            return valuesForSidesShorthand(borderWidthShorthand());
        case CSSPropertyWebkitColumnRule:
            return valuesForShorthandProperty(webkitColumnRuleShorthand());
        case CSSPropertyWebkitColumns:
            return valuesForShorthandProperty(webkitColumnsShorthand());
        case CSSPropertyListStyle:
            return valuesForShorthandProperty(listStyleShorthand());
        case CSSPropertyMargin:
            return valuesForSidesShorthand(marginShorthand());
        case CSSPropertyOutline:
            return valuesForShorthandProperty(outlineShorthand());
        case CSSPropertyPadding:
            return valuesForSidesShorthand(paddingShorthand());
        /* Individual properties not part of the spec */
        case CSSPropertyBackgroundRepeatX:
        case CSSPropertyBackgroundRepeatY:
            break;
        case CSSPropertyInternalCallback:
            // This property is hidden from the web.
            return nullptr;

        /* Unimplemented CSS 3 properties (including CSS3 shorthand properties) */
        case CSSPropertyWebkitTextEmphasis:
        case CSSPropertyTextLineThroughColor:
        case CSSPropertyTextLineThroughMode:
        case CSSPropertyTextLineThroughStyle:
        case CSSPropertyTextLineThroughWidth:
        case CSSPropertyTextOverlineColor:
        case CSSPropertyTextOverlineMode:
        case CSSPropertyTextOverlineStyle:
        case CSSPropertyTextOverlineWidth:
        case CSSPropertyTextUnderlineColor:
        case CSSPropertyTextUnderlineMode:
        case CSSPropertyTextUnderlineStyle:
        case CSSPropertyTextUnderlineWidth:
            break;

        /* Directional properties are resolved by resolveDirectionAwareProperty() before the switch. */
        case CSSPropertyWebkitBorderEnd:
        case CSSPropertyWebkitBorderEndColor:
        case CSSPropertyWebkitBorderEndStyle:
        case CSSPropertyWebkitBorderEndWidth:
        case CSSPropertyWebkitBorderStart:
        case CSSPropertyWebkitBorderStartColor:
        case CSSPropertyWebkitBorderStartStyle:
        case CSSPropertyWebkitBorderStartWidth:
        case CSSPropertyWebkitBorderAfter:
        case CSSPropertyWebkitBorderAfterColor:
        case CSSPropertyWebkitBorderAfterStyle:
        case CSSPropertyWebkitBorderAfterWidth:
        case CSSPropertyWebkitBorderBefore:
        case CSSPropertyWebkitBorderBeforeColor:
        case CSSPropertyWebkitBorderBeforeStyle:
        case CSSPropertyWebkitBorderBeforeWidth:
        case CSSPropertyWebkitMarginEnd:
        case CSSPropertyWebkitMarginStart:
        case CSSPropertyWebkitMarginAfter:
        case CSSPropertyWebkitMarginBefore:
        case CSSPropertyWebkitPaddingEnd:
        case CSSPropertyWebkitPaddingStart:
        case CSSPropertyWebkitPaddingAfter:
        case CSSPropertyWebkitPaddingBefore:
        case CSSPropertyWebkitLogicalWidth:
        case CSSPropertyWebkitLogicalHeight:
        case CSSPropertyWebkitMinLogicalWidth:
        case CSSPropertyWebkitMinLogicalHeight:
        case CSSPropertyWebkitMaxLogicalWidth:
        case CSSPropertyWebkitMaxLogicalHeight:
            ASSERT_NOT_REACHED();
            break;

        /* Unimplemented @font-face properties */
        case CSSPropertyFontStretch:
        case CSSPropertySrc:
        case CSSPropertyUnicodeRange:
            break;

        /* Other unimplemented properties */
        case CSSPropertyPage: // for @page
        case CSSPropertyQuotes: // FIXME: needs implementation
        case CSSPropertySize: // for @page
            break;

        /* Unimplemented -webkit- properties */
        case CSSPropertyWebkitBorderRadius:
        case CSSPropertyWebkitMarginCollapse:
        case CSSPropertyWebkitMask:
        case CSSPropertyWebkitMaskRepeatX:
        case CSSPropertyWebkitMaskRepeatY:
        case CSSPropertyWebkitPerspectiveOriginX:
        case CSSPropertyWebkitPerspectiveOriginY:
        case CSSPropertyWebkitTextStroke:
        case CSSPropertyWebkitTransformOriginX:
        case CSSPropertyWebkitTransformOriginY:
        case CSSPropertyWebkitTransformOriginZ:
            break;

        /* @viewport rule properties */
        case CSSPropertyMaxZoom:
        case CSSPropertyMinZoom:
        case CSSPropertyOrientation:
        case CSSPropertyUserZoom:
            break;

        // Internal properties that shouldn't be exposed throught getComputedStyle.
        case CSSPropertyInternalMarqueeDirection:
        case CSSPropertyInternalMarqueeIncrement:
        case CSSPropertyInternalMarqueeRepetition:
        case CSSPropertyInternalMarqueeSpeed:
        case CSSPropertyInternalMarqueeStyle:
            ASSERT_NOT_REACHED();
            return nullptr;

        case CSSPropertyBufferedRendering:
        case CSSPropertyClipPath:
        case CSSPropertyClipRule:
        case CSSPropertyMask:
        case CSSPropertyEnableBackground:
        case CSSPropertyFilter:
        case CSSPropertyFloodColor:
        case CSSPropertyFloodOpacity:
        case CSSPropertyLightingColor:
        case CSSPropertyStopColor:
        case CSSPropertyStopOpacity:
        case CSSPropertyColorInterpolation:
        case CSSPropertyColorInterpolationFilters:
        case CSSPropertyColorProfile:
        case CSSPropertyColorRendering:
        case CSSPropertyFill:
        case CSSPropertyFillOpacity:
        case CSSPropertyFillRule:
        case CSSPropertyMarker:
        case CSSPropertyMarkerEnd:
        case CSSPropertyMarkerMid:
        case CSSPropertyMarkerStart:
        case CSSPropertyMaskType:
        case CSSPropertyShapeRendering:
        case CSSPropertyStroke:
        case CSSPropertyStrokeDasharray:
        case CSSPropertyStrokeDashoffset:
        case CSSPropertyStrokeLinecap:
        case CSSPropertyStrokeLinejoin:
        case CSSPropertyStrokeMiterlimit:
        case CSSPropertyStrokeOpacity:
        case CSSPropertyStrokeWidth:
        case CSSPropertyAlignmentBaseline:
        case CSSPropertyBaselineShift:
        case CSSPropertyDominantBaseline:
        case CSSPropertyGlyphOrientationHorizontal:
        case CSSPropertyGlyphOrientationVertical:
        case CSSPropertyKerning:
        case CSSPropertyTextAnchor:
        case CSSPropertyVectorEffect:
        case CSSPropertyPaintOrder:
        case CSSPropertyWritingMode:
            return getSVGPropertyCSSValue(propertyID, DoNotUpdateLayout);
    }

    logUnimplementedPropertyID(propertyID);
    return nullptr;
}

String CSSComputedStyleDeclaration::getPropertyValue(CSSPropertyID propertyID) const
{
    RefPtrWillBeRawPtr<CSSValue> value = getPropertyCSSValue(propertyID);
    if (value)
        return value->cssText();
    return "";
}


unsigned CSSComputedStyleDeclaration::length() const
{
    Node* node = m_node.get();
    if (!node)
        return 0;

    RenderStyle* style = node->computedStyle(m_pseudoElementSpecifier);
    if (!style)
        return 0;

    return computableProperties().size();
}

String CSSComputedStyleDeclaration::item(unsigned i) const
{
    if (i >= length())
        return "";

    return getPropertyNameString(computableProperties()[i]);
}

bool CSSComputedStyleDeclaration::cssPropertyMatches(CSSPropertyID propertyID, const CSSValue* propertyValue) const
{
    if (propertyID == CSSPropertyFontSize && propertyValue->isPrimitiveValue() && m_node) {
        m_node->document().updateLayoutIgnorePendingStylesheets();
        RenderStyle* style = m_node->computedStyle(m_pseudoElementSpecifier);
        if (style && style->fontDescription().keywordSize()) {
            CSSValueID sizeValue = cssIdentifierForFontSizeKeyword(style->fontDescription().keywordSize());
            const CSSPrimitiveValue* primitiveValue = toCSSPrimitiveValue(propertyValue);
            if (primitiveValue->isValueID() && primitiveValue->getValueID() == sizeValue)
                return true;
        }
    }
    RefPtrWillBeRawPtr<CSSValue> value = getPropertyCSSValue(propertyID);
    return value && propertyValue && value->equals(*propertyValue);
}

PassRefPtr<MutableStylePropertySet> CSSComputedStyleDeclaration::copyProperties() const
{
    return copyPropertiesInSet(computableProperties());
}

PassRefPtrWillBeRawPtr<CSSValueList> CSSComputedStyleDeclaration::valuesForShorthandProperty(const StylePropertyShorthand& shorthand) const
{
    RefPtrWillBeRawPtr<CSSValueList> list = CSSValueList::createSpaceSeparated();
    for (size_t i = 0; i < shorthand.length(); ++i) {
        RefPtrWillBeRawPtr<CSSValue> value = getPropertyCSSValue(shorthand.properties()[i], DoNotUpdateLayout);
        list->append(value);
    }
    return list.release();
}

PassRefPtrWillBeRawPtr<CSSValueList> CSSComputedStyleDeclaration::valuesForSidesShorthand(const StylePropertyShorthand& shorthand) const
{
    RefPtrWillBeRawPtr<CSSValueList> list = CSSValueList::createSpaceSeparated();
    // Assume the properties are in the usual order top, right, bottom, left.
    RefPtrWillBeRawPtr<CSSValue> topValue = getPropertyCSSValue(shorthand.properties()[0], DoNotUpdateLayout);
    RefPtrWillBeRawPtr<CSSValue> rightValue = getPropertyCSSValue(shorthand.properties()[1], DoNotUpdateLayout);
    RefPtrWillBeRawPtr<CSSValue> bottomValue = getPropertyCSSValue(shorthand.properties()[2], DoNotUpdateLayout);
    RefPtrWillBeRawPtr<CSSValue> leftValue = getPropertyCSSValue(shorthand.properties()[3], DoNotUpdateLayout);

    // All 4 properties must be specified.
    if (!topValue || !rightValue || !bottomValue || !leftValue)
        return nullptr;

    bool showLeft = !compareCSSValuePtr(rightValue, leftValue);
    bool showBottom = !compareCSSValuePtr(topValue, bottomValue) || showLeft;
    bool showRight = !compareCSSValuePtr(topValue, rightValue) || showBottom;

    list->append(topValue.release());
    if (showRight)
        list->append(rightValue.release());
    if (showBottom)
        list->append(bottomValue.release());
    if (showLeft)
        list->append(leftValue.release());

    return list.release();
}

PassRefPtrWillBeRawPtr<CSSValueList> CSSComputedStyleDeclaration::valuesForGridShorthand(const StylePropertyShorthand& shorthand) const
{
    RefPtrWillBeRawPtr<CSSValueList> list = CSSValueList::createSlashSeparated();
    for (size_t i = 0; i < shorthand.length(); ++i) {
        RefPtrWillBeRawPtr<CSSValue> value = getPropertyCSSValue(shorthand.properties()[i], DoNotUpdateLayout);
        list->append(value.release());
    }
    return list.release();
}

PassRefPtr<MutableStylePropertySet> CSSComputedStyleDeclaration::copyPropertiesInSet(const Vector<CSSPropertyID>& properties) const
{
    WillBeHeapVector<CSSProperty, 256> list;
    list.reserveInitialCapacity(properties.size());
    for (unsigned i = 0; i < properties.size(); ++i) {
        RefPtrWillBeRawPtr<CSSValue> value = getPropertyCSSValue(properties[i]);
        if (value)
            list.append(CSSProperty(properties[i], value.release(), false));
    }
    return MutableStylePropertySet::create(list.data(), list.size());
}

CSSRule* CSSComputedStyleDeclaration::parentRule() const
{
    return 0;
}

PassRefPtrWillBeRawPtr<CSSValue> CSSComputedStyleDeclaration::getPropertyCSSValue(const String& propertyName)
{
    CSSPropertyID propertyID = cssPropertyID(propertyName);
    if (!propertyID)
        return nullptr;
    RefPtrWillBeRawPtr<CSSValue> value = getPropertyCSSValue(propertyID);
    return value ? value->cloneForCSSOM() : nullptr;
}

String CSSComputedStyleDeclaration::getPropertyValue(const String& propertyName)
{
    CSSPropertyID propertyID = cssPropertyID(propertyName);
    if (!propertyID || !RuntimeCSSEnabled::isCSSPropertyEnabled(propertyID))
        return String();
    return getPropertyValue(propertyID);
}

String CSSComputedStyleDeclaration::getPropertyPriority(const String&)
{
    // All computed styles have a priority of not "important".
    return "";
}

String CSSComputedStyleDeclaration::getPropertyShorthand(const String&)
{
    return "";
}

bool CSSComputedStyleDeclaration::isPropertyImplicit(const String&)
{
    return false;
}

void CSSComputedStyleDeclaration::setProperty(const String& name, const String&, const String&, ExceptionState& exceptionState)
{
    exceptionState.throwDOMException(NoModificationAllowedError, "These styles are computed, and therefore the '" + name + "' property is read-only.");
}

String CSSComputedStyleDeclaration::removeProperty(const String& name, ExceptionState& exceptionState)
{
    exceptionState.throwDOMException(NoModificationAllowedError, "These styles are computed, and therefore the '" + name + "' property is read-only.");
    return String();
}

PassRefPtrWillBeRawPtr<CSSValue> CSSComputedStyleDeclaration::getPropertyCSSValueInternal(CSSPropertyID propertyID)
{
    return getPropertyCSSValue(propertyID);
}

String CSSComputedStyleDeclaration::getPropertyValueInternal(CSSPropertyID propertyID)
{
    return getPropertyValue(propertyID);
}

void CSSComputedStyleDeclaration::setPropertyInternal(CSSPropertyID id, const String&, bool, ExceptionState& exceptionState)
{
    exceptionState.throwDOMException(NoModificationAllowedError, "These styles are computed, and therefore the '" + getPropertyNameString(id) + "' property is read-only.");
}

PassRefPtrWillBeRawPtr<CSSValueList> CSSComputedStyleDeclaration::valuesForBackgroundShorthand() const
{
    static const CSSPropertyID propertiesBeforeSlashSeperator[5] = { CSSPropertyBackgroundColor, CSSPropertyBackgroundImage,
                                                                     CSSPropertyBackgroundRepeat, CSSPropertyBackgroundAttachment,
                                                                     CSSPropertyBackgroundPosition };
    static const CSSPropertyID propertiesAfterSlashSeperator[3] = { CSSPropertyBackgroundSize, CSSPropertyBackgroundOrigin,
                                                                    CSSPropertyBackgroundClip };

    RefPtrWillBeRawPtr<CSSValueList> list = CSSValueList::createSlashSeparated();
    list->append(valuesForShorthandProperty(StylePropertyShorthand(CSSPropertyBackground, propertiesBeforeSlashSeperator, WTF_ARRAY_LENGTH(propertiesBeforeSlashSeperator))));
    list->append(valuesForShorthandProperty(StylePropertyShorthand(CSSPropertyBackground, propertiesAfterSlashSeperator, WTF_ARRAY_LENGTH(propertiesAfterSlashSeperator))));
    return list.release();
}

} // namespace WebCore
