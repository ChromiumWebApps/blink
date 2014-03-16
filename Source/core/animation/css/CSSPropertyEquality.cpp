// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/animation/css/CSSPropertyEquality.h"

#include "core/animation/css/CSSAnimations.h"
#include "core/rendering/style/RenderStyle.h"
#include "core/rendering/style/ShadowList.h"

namespace WebCore {

namespace {

template <CSSPropertyID property>
bool fillLayersEqual(const FillLayer* aLayer, const FillLayer* bLayer)
{
    if (aLayer == bLayer)
        return true;
    if (!aLayer || !bLayer)
        return false;
    while (aLayer && bLayer) {
        switch (property) {
        case CSSPropertyBackgroundPositionX:
        case CSSPropertyWebkitMaskPositionX:
            if (aLayer->xPosition() != bLayer->xPosition())
                return false;
            break;
        case CSSPropertyBackgroundPositionY:
        case CSSPropertyWebkitMaskPositionY:
            if (aLayer->yPosition() != bLayer->yPosition())
                return false;
            break;
        case CSSPropertyBackgroundSize:
        case CSSPropertyWebkitBackgroundSize:
        case CSSPropertyWebkitMaskSize:
            if (!(aLayer->sizeLength() == bLayer->sizeLength()))
                return false;
            break;
        case CSSPropertyBackgroundImage:
            if (!StyleImage::imagesEquivalent(aLayer->image(), bLayer->image()))
                return false;
            break;
        default:
            ASSERT_NOT_REACHED();
            return true;
        }

        aLayer = aLayer->next();
        bLayer = bLayer->next();
    }

    // FIXME: Shouldn't this be return !aLayer && !bLayer; ?
    return true;
}

template <typename T>
bool ptrsOrValuesEqual(T a, T b)
{
    return a == b || (a && b && *a == *b);
}

}

bool CSSPropertyEquality::propertiesEqual(CSSPropertyID prop, const RenderStyle& a, const RenderStyle& b)
{
    switch (prop) {
    case CSSPropertyBackgroundColor:
        return a.backgroundColor().resolve(a.color()) == b.backgroundColor().resolve(b.color())
            && a.visitedLinkBackgroundColor().resolve(a.color()) == b.visitedLinkBackgroundColor().resolve(b.color());
    case CSSPropertyBackgroundImage:
        return fillLayersEqual<CSSPropertyBackgroundImage>(a.backgroundLayers(), b.backgroundLayers());
    case CSSPropertyBackgroundPositionX:
        return fillLayersEqual<CSSPropertyBackgroundPositionX>(a.backgroundLayers(), b.backgroundLayers());
    case CSSPropertyBackgroundPositionY:
        return fillLayersEqual<CSSPropertyBackgroundPositionY>(a.backgroundLayers(), b.backgroundLayers());
    case CSSPropertyBackgroundSize:
        return fillLayersEqual<CSSPropertyBackgroundSize>(a.backgroundLayers(), b.backgroundLayers());
    case CSSPropertyBaselineShift:
        return ptrsOrValuesEqual<PassRefPtr<SVGLength> >(a.baselineShiftValue(), b.baselineShiftValue());
    case CSSPropertyBorderBottomColor:
        return a.borderBottomColor().resolve(a.color()) == b.borderBottomColor().resolve(b.color())
            && a.visitedLinkBorderBottomColor().resolve(a.color()) == b.visitedLinkBorderBottomColor().resolve(b.color());
    case CSSPropertyBorderBottomLeftRadius:
        return a.borderBottomLeftRadius() == b.borderBottomLeftRadius();
    case CSSPropertyBorderBottomRightRadius:
        return a.borderBottomRightRadius() == b.borderBottomRightRadius();
    case CSSPropertyBorderBottomWidth:
        return a.borderBottomWidth() == b.borderBottomWidth();
    case CSSPropertyBorderImageOutset:
        return a.borderImageOutset() == b.borderImageOutset();
    case CSSPropertyBorderImageSlice:
        return a.borderImageSlices() == b.borderImageSlices();
    case CSSPropertyBorderImageSource:
        return ptrsOrValuesEqual<StyleImage*>(a.borderImageSource(), b.borderImageSource());
    case CSSPropertyBorderImageWidth:
        return a.borderImageWidth() == b.borderImageWidth();
    case CSSPropertyBorderLeftColor:
        return a.borderLeftColor().resolve(a.color()) == b.borderLeftColor().resolve(b.color())
            && a.visitedLinkBorderLeftColor().resolve(a.color()) == b.visitedLinkBorderLeftColor().resolve(b.color());
    case CSSPropertyBorderLeftWidth:
        return a.borderLeftWidth() == b.borderLeftWidth();
    case CSSPropertyBorderRightColor:
        return a.borderRightColor().resolve(a.color()) == b.borderRightColor().resolve(b.color())
            && a.visitedLinkBorderRightColor().resolve(a.color()) == b.visitedLinkBorderRightColor().resolve(b.color());
    case CSSPropertyBorderRightWidth:
        return a.borderRightWidth() == b.borderRightWidth();
    case CSSPropertyBorderTopColor:
        return a.borderTopColor().resolve(a.color()) == b.borderTopColor().resolve(b.color())
            && a.visitedLinkBorderTopColor().resolve(a.color()) == b.visitedLinkBorderTopColor().resolve(b.color());
    case CSSPropertyBorderTopLeftRadius:
        return a.borderTopLeftRadius() == b.borderTopLeftRadius();
    case CSSPropertyBorderTopRightRadius:
        return a.borderTopRightRadius() == b.borderTopRightRadius();
    case CSSPropertyBorderTopWidth:
        return a.borderTopWidth() == b.borderTopWidth();
    case CSSPropertyBottom:
        return a.bottom() == b.bottom();
    case CSSPropertyBoxShadow:
        return ptrsOrValuesEqual<ShadowList*>(a.boxShadow(), b.boxShadow());
    case CSSPropertyClip:
        return a.clip() == b.clip();
    case CSSPropertyColor:
        return a.color() == b.color() && a.visitedLinkColor() == b.visitedLinkColor();
    case CSSPropertyFill:
        return a.fillPaintType() == b.fillPaintType()
            && (a.fillPaintType() != SVGPaint::SVG_PAINTTYPE_RGBCOLOR || a.fillPaintColor() == b.fillPaintColor());
    case CSSPropertyFillOpacity:
        return a.fillOpacity() == b.fillOpacity();
    case CSSPropertyFlexBasis:
        return a.flexBasis() == b.flexBasis();
    case CSSPropertyFlexGrow:
        return a.flexGrow() == b.flexGrow();
    case CSSPropertyFlexShrink:
        return a.flexShrink() == b.flexShrink();
    case CSSPropertyFloodColor:
        return a.floodColor() == b.floodColor();
    case CSSPropertyFloodOpacity:
        return a.floodOpacity() == b.floodOpacity();
    case CSSPropertyFontSize:
        // CSSPropertyFontSize: Must pass a specified size to setFontSize if Text Autosizing is enabled, but a computed size
        // if text zoom is enabled (if neither is enabled it's irrelevant as they're probably the same).
        // FIXME: Should we introduce an option to pass the computed font size here, allowing consumers to
        // enable text zoom rather than Text Autosizing? See http://crbug.com/227545.
        return a.specifiedFontSize() == b.specifiedFontSize();
    case CSSPropertyFontWeight:
        return a.fontWeight() == b.fontWeight();
    case CSSPropertyHeight:
        return a.height() == b.height();
    case CSSPropertyKerning:
        return ptrsOrValuesEqual<PassRefPtr<SVGLength> >(a.kerning(), b.kerning());
    case CSSPropertyLeft:
        return a.left() == b.left();
    case CSSPropertyLetterSpacing:
        return a.letterSpacing() == b.letterSpacing();
    case CSSPropertyLightingColor:
        return a.lightingColor() == b.lightingColor();
    case CSSPropertyLineHeight:
        return a.specifiedLineHeight() == b.specifiedLineHeight();
    case CSSPropertyListStyleImage:
        return ptrsOrValuesEqual<StyleImage*>(a.listStyleImage(), b.listStyleImage());
    case CSSPropertyMarginBottom:
        return a.marginBottom() == b.marginBottom();
    case CSSPropertyMarginLeft:
        return a.marginLeft() == b.marginLeft();
    case CSSPropertyMarginRight:
        return a.marginRight() == b.marginRight();
    case CSSPropertyMarginTop:
        return a.marginTop() == b.marginTop();
    case CSSPropertyMaxHeight:
        return a.maxHeight() == b.maxHeight();
    case CSSPropertyMaxWidth:
        return a.maxWidth() == b.maxWidth();
    case CSSPropertyMinHeight:
        return a.minHeight() == b.minHeight();
    case CSSPropertyMinWidth:
        return a.minWidth() == b.minWidth();
    case CSSPropertyObjectPosition:
        return a.objectPosition() == b.objectPosition();
    case CSSPropertyOpacity:
        return a.opacity() == b.opacity();
    case CSSPropertyOrphans:
        return a.orphans() == b.orphans();
    case CSSPropertyOutlineColor:
        return a.outlineColor().resolve(a.color()) == b.outlineColor().resolve(b.color())
            && a.visitedLinkOutlineColor().resolve(a.color()) == b.visitedLinkOutlineColor().resolve(b.color());
    case CSSPropertyOutlineOffset:
        return a.outlineOffset() == b.outlineOffset();
    case CSSPropertyOutlineWidth:
        return a.outlineWidth() == b.outlineWidth();
    case CSSPropertyPaddingBottom:
        return a.paddingBottom() == b.paddingBottom();
    case CSSPropertyPaddingLeft:
        return a.paddingLeft() == b.paddingLeft();
    case CSSPropertyPaddingRight:
        return a.paddingRight() == b.paddingRight();
    case CSSPropertyPaddingTop:
        return a.paddingTop() == b.paddingTop();
    case CSSPropertyRight:
        return a.right() == b.right();
    case CSSPropertyShapeImageThreshold:
        return a.shapeImageThreshold() == b.shapeImageThreshold();
    case CSSPropertyShapeInside:
        return ptrsOrValuesEqual<ShapeValue*>(a.shapeInside(), b.shapeInside());
    case CSSPropertyShapeMargin:
        return a.shapeMargin() == b.shapeMargin();
    case CSSPropertyShapeOutside:
        return ptrsOrValuesEqual<ShapeValue*>(a.shapeOutside(), b.shapeOutside());
    case CSSPropertyStopColor:
        return a.stopColor() == b.stopColor();
    case CSSPropertyStopOpacity:
        return a.stopOpacity() == b.stopOpacity();
    case CSSPropertyStroke:
        return a.strokePaintType() == b.strokePaintType()
            && (a.strokePaintType() != SVGPaint::SVG_PAINTTYPE_RGBCOLOR || a.strokePaintColor() == b.strokePaintColor());
    case CSSPropertyStrokeDasharray:
        return ptrsOrValuesEqual<PassRefPtr<SVGLengthList> >(a.strokeDashArray(), b.strokeDashArray());
    case CSSPropertyStrokeDashoffset:
        return ptrsOrValuesEqual<PassRefPtr<SVGLength> >(a.strokeDashOffset(), b.strokeDashOffset());
    case CSSPropertyStrokeMiterlimit:
        return a.strokeMiterLimit() == b.strokeMiterLimit();
    case CSSPropertyStrokeOpacity:
        return a.strokeOpacity() == b.strokeOpacity();
    case CSSPropertyStrokeWidth:
        return ptrsOrValuesEqual<PassRefPtr<SVGLength> >(a.strokeWidth(), b.strokeWidth());
    case CSSPropertyTextDecorationColor:
        return a.textDecorationColor().resolve(a.color()) == b.textDecorationColor().resolve(b.color())
            && a.visitedLinkTextDecorationColor().resolve(a.color()) == b.visitedLinkTextDecorationColor().resolve(b.color());
    case CSSPropertyTextIndent:
        return a.textIndent() == b.textIndent();
    case CSSPropertyTextShadow:
        return ptrsOrValuesEqual<ShadowList*>(a.textShadow(), b.textShadow());
    case CSSPropertyTop:
        return a.top() == b.top();
    case CSSPropertyVisibility:
        return a.visibility() == b.visibility();
    case CSSPropertyWebkitBackgroundSize:
        return fillLayersEqual<CSSPropertyWebkitBackgroundSize>(a.backgroundLayers(), b.backgroundLayers());
    case CSSPropertyWebkitBorderHorizontalSpacing:
        return a.horizontalBorderSpacing() == b.horizontalBorderSpacing();
    case CSSPropertyWebkitBorderVerticalSpacing:
        return a.verticalBorderSpacing() == b.verticalBorderSpacing();
    case CSSPropertyWebkitBoxShadow:
        return ptrsOrValuesEqual<ShadowList*>(a.boxShadow(), b.boxShadow());
    case CSSPropertyWebkitClipPath:
        return ptrsOrValuesEqual<ClipPathOperation*>(a.clipPath(), b.clipPath());
    case CSSPropertyWebkitColumnCount:
        return a.columnCount() == b.columnCount();
    case CSSPropertyWebkitColumnGap:
        return a.columnGap() == b.columnGap();
    case CSSPropertyWebkitColumnRuleColor:
        return a.columnRuleColor().resolve(a.color()) == b.columnRuleColor().resolve(b.color())
            && a.visitedLinkColumnRuleColor().resolve(a.color()) == b.visitedLinkColumnRuleColor().resolve(b.color());
    case CSSPropertyWebkitColumnRuleWidth:
        return a.columnRuleWidth() == b.columnRuleWidth();
    case CSSPropertyWebkitColumnWidth:
        return a.columnWidth() == b.columnWidth();
    case CSSPropertyWebkitFilter:
        return a.filter() == b.filter();
    case CSSPropertyWebkitMaskBoxImageOutset:
        return a.maskBoxImageOutset() == b.maskBoxImageOutset();
    case CSSPropertyWebkitMaskBoxImageSlice:
        return a.maskBoxImageSlices() == b.maskBoxImageSlices();
    case CSSPropertyWebkitMaskBoxImageSource:
        return ptrsOrValuesEqual<StyleImage*>(a.maskBoxImageSource(), b.maskBoxImageSource());
    case CSSPropertyWebkitMaskBoxImageWidth:
        return a.maskBoxImageWidth() == b.maskBoxImageWidth();
    case CSSPropertyWebkitMaskImage:
        return ptrsOrValuesEqual<StyleImage*>(a.maskImage(), b.maskImage());
    case CSSPropertyWebkitMaskPositionX:
        return fillLayersEqual<CSSPropertyWebkitMaskPositionX>(a.maskLayers(), b.maskLayers());
    case CSSPropertyWebkitMaskPositionY:
        return fillLayersEqual<CSSPropertyWebkitMaskPositionY>(a.maskLayers(), b.maskLayers());
    case CSSPropertyWebkitMaskSize:
        return fillLayersEqual<CSSPropertyWebkitMaskSize>(a.maskLayers(), b.maskLayers());
    case CSSPropertyWebkitPerspective:
        return a.perspective() == b.perspective();
    case CSSPropertyWebkitPerspectiveOriginX:
        return a.perspectiveOriginX() == b.perspectiveOriginX();
    case CSSPropertyWebkitPerspectiveOriginY:
        return a.perspectiveOriginY() == b.perspectiveOriginY();
    case CSSPropertyWebkitTextStrokeColor:
        return a.textStrokeColor().resolve(a.color()) == b.textStrokeColor().resolve(b.color())
            && a.visitedLinkTextStrokeColor().resolve(a.color()) == b.visitedLinkTextStrokeColor().resolve(b.color());
    case CSSPropertyWebkitTransform:
        return a.transform() == b.transform();
    case CSSPropertyWebkitTransformOriginX:
        return a.transformOriginX() == b.transformOriginX();
    case CSSPropertyWebkitTransformOriginY:
        return a.transformOriginY() == b.transformOriginY();
    case CSSPropertyWebkitTransformOriginZ:
        return a.transformOriginZ() == b.transformOriginZ();
    case CSSPropertyWidows:
        return a.widows() == b.widows();
    case CSSPropertyWidth:
        return a.width() == b.width();
    case CSSPropertyWordSpacing:
        return a.wordSpacing() == b.wordSpacing();
    case CSSPropertyZIndex:
        return a.zIndex() == b.zIndex();
    case CSSPropertyZoom:
        return a.zoom() == b.zoom();
    default:
        ASSERT_NOT_REACHED();
        return true;
    }
}

}
