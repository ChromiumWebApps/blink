/*
 * Copyright (C) 2011 Andreas Kling (kling@webkit.org)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include "config.h"
#include "core/css/CSSValue.h"

#include "core/css/CSSArrayFunctionValue.h"
#include "core/css/CSSAspectRatioValue.h"
#include "core/css/CSSBorderImageSliceValue.h"
#include "core/css/CSSCalculationValue.h"
#include "core/css/CSSCanvasValue.h"
#include "core/css/CSSCrossfadeValue.h"
#include "core/css/CSSCursorImageValue.h"
#include "core/css/CSSFilterValue.h"
#include "core/css/CSSFontFaceSrcValue.h"
#include "core/css/CSSFontFeatureValue.h"
#include "core/css/CSSFontValue.h"
#include "core/css/CSSFunctionValue.h"
#include "core/css/CSSGradientValue.h"
#include "core/css/CSSGridLineNamesValue.h"
#include "core/css/CSSGridTemplateAreasValue.h"
#include "core/css/CSSImageSetValue.h"
#include "core/css/CSSImageValue.h"
#include "core/css/CSSInheritedValue.h"
#include "core/css/CSSInitialValue.h"
#include "core/css/CSSLineBoxContainValue.h"
#include "core/css/CSSPrimitiveValue.h"
#include "core/css/CSSReflectValue.h"
#include "core/css/CSSSVGDocumentValue.h"
#include "core/css/CSSShadowValue.h"
#include "core/css/CSSTimingFunctionValue.h"
#include "core/css/CSSTransformValue.h"
#include "core/css/CSSUnicodeRangeValue.h"
#include "core/css/CSSValueList.h"
#include "core/svg/SVGPaint.h"

namespace WebCore {

struct SameSizeAsCSSValue : public RefCountedWillBeRefCountedGarbageCollected<SameSizeAsCSSValue> {
    uint32_t bitfields;
};

COMPILE_ASSERT(sizeof(CSSValue) <= sizeof(SameSizeAsCSSValue), CSS_value_should_stay_small);

class TextCloneCSSValue : public CSSValue {
public:
    static PassRefPtrWillBeRawPtr<TextCloneCSSValue> create(ClassType classType, const String& text)
    {
        return adoptRefWillBeRefCountedGarbageCollected(new TextCloneCSSValue(classType, text));
    }

    String cssText() const { return m_cssText; }

    void traceAfterDispatch(Visitor* visitor) { CSSValue::traceAfterDispatch(visitor); }

private:
    TextCloneCSSValue(ClassType classType, const String& text)
        : CSSValue(classType, /*isCSSOMSafe*/ true)
        , m_cssText(text)
    {
        m_isTextClone = true;
    }

    String m_cssText;
};

DEFINE_CSS_VALUE_TYPE_CASTS(TextCloneCSSValue, isTextCloneCSSValue());

bool CSSValue::isImplicitInitialValue() const
{
    return m_classType == InitialClass && toCSSInitialValue(this)->isImplicit();
}

CSSValue::Type CSSValue::cssValueType() const
{
    if (isInheritedValue())
        return CSS_INHERIT;
    if (isPrimitiveValue())
        return CSS_PRIMITIVE_VALUE;
    if (isValueList())
        return CSS_VALUE_LIST;
    if (isInitialValue())
        return CSS_INITIAL;
    return CSS_CUSTOM;
}

bool CSSValue::hasFailedOrCanceledSubresources() const
{
    // This should get called for internal instances only.
    ASSERT(!isCSSOMSafe());

    if (isValueList())
        return toCSSValueList(this)->hasFailedOrCanceledSubresources();
    if (classType() == FontFaceSrcClass)
        return toCSSFontFaceSrcValue(this)->hasFailedOrCanceledSubresources();
    if (classType() == ImageClass)
        return toCSSImageValue(this)->hasFailedOrCanceledSubresources();
    if (classType() == CrossfadeClass)
        return toCSSCrossfadeValue(this)->hasFailedOrCanceledSubresources();
    if (classType() == ImageSetClass)
        return toCSSImageSetValue(this)->hasFailedOrCanceledSubresources();

    return false;
}

template<class ChildClassType>
inline static bool compareCSSValues(const CSSValue& first, const CSSValue& second)
{
    return static_cast<const ChildClassType&>(first).equals(static_cast<const ChildClassType&>(second));
}

bool CSSValue::equals(const CSSValue& other) const
{
    if (m_isTextClone) {
        ASSERT(isCSSOMSafe());
        return toTextCloneCSSValue(this)->cssText() == other.cssText();
    }

    if (m_classType == other.m_classType) {
        switch (m_classType) {
        case AspectRatioClass:
            return compareCSSValues<CSSAspectRatioValue>(*this, other);
        case BorderImageSliceClass:
            return compareCSSValues<CSSBorderImageSliceValue>(*this, other);
        case CanvasClass:
            return compareCSSValues<CSSCanvasValue>(*this, other);
        case CursorImageClass:
            return compareCSSValues<CSSCursorImageValue>(*this, other);
        case FontClass:
            return compareCSSValues<CSSFontValue>(*this, other);
        case FontFaceSrcClass:
            return compareCSSValues<CSSFontFaceSrcValue>(*this, other);
        case FontFeatureClass:
            return compareCSSValues<CSSFontFeatureValue>(*this, other);
        case FunctionClass:
            return compareCSSValues<CSSFunctionValue>(*this, other);
        case LinearGradientClass:
            return compareCSSValues<CSSLinearGradientValue>(*this, other);
        case RadialGradientClass:
            return compareCSSValues<CSSRadialGradientValue>(*this, other);
        case CrossfadeClass:
            return compareCSSValues<CSSCrossfadeValue>(*this, other);
        case ImageClass:
            return compareCSSValues<CSSImageValue>(*this, other);
        case InheritedClass:
            return compareCSSValues<CSSInheritedValue>(*this, other);
        case InitialClass:
            return compareCSSValues<CSSInitialValue>(*this, other);
        case GridLineNamesClass:
            return compareCSSValues<CSSGridLineNamesValue>(*this, other);
        case GridTemplateAreasClass:
            return compareCSSValues<CSSGridTemplateAreasValue>(*this, other);
        case PrimitiveClass:
            return compareCSSValues<CSSPrimitiveValue>(*this, other);
        case ReflectClass:
            return compareCSSValues<CSSReflectValue>(*this, other);
        case ShadowClass:
            return compareCSSValues<CSSShadowValue>(*this, other);
        case CubicBezierTimingFunctionClass:
            return compareCSSValues<CSSCubicBezierTimingFunctionValue>(*this, other);
        case StepsTimingFunctionClass:
            return compareCSSValues<CSSStepsTimingFunctionValue>(*this, other);
        case UnicodeRangeClass:
            return compareCSSValues<CSSUnicodeRangeValue>(*this, other);
        case ValueListClass:
            return compareCSSValues<CSSValueList>(*this, other);
        case CSSTransformClass:
            return compareCSSValues<CSSTransformValue>(*this, other);
        case LineBoxContainClass:
            return compareCSSValues<CSSLineBoxContainValue>(*this, other);
        case CalculationClass:
            return compareCSSValues<CSSCalcValue>(*this, other);
        case ImageSetClass:
            return compareCSSValues<CSSImageSetValue>(*this, other);
        case CSSFilterClass:
            return compareCSSValues<CSSFilterValue>(*this, other);
        case CSSArrayFunctionValueClass:
            return compareCSSValues<CSSArrayFunctionValue>(*this, other);
        case SVGPaintClass:
            return compareCSSValues<SVGPaint>(*this, other);
        case CSSSVGDocumentClass:
            return compareCSSValues<CSSSVGDocumentValue>(*this, other);
        default:
            ASSERT_NOT_REACHED();
            return false;
        }
    } else if (m_classType == ValueListClass && other.m_classType != ValueListClass)
        return toCSSValueList(this)->equals(other);
    else if (m_classType != ValueListClass && other.m_classType == ValueListClass)
        return static_cast<const CSSValueList&>(other).equals(*this);
    return false;
}

String CSSValue::cssText() const
{
    if (m_isTextClone) {
         ASSERT(isCSSOMSafe());
        return toTextCloneCSSValue(this)->cssText();
    }
    ASSERT(!isCSSOMSafe() || isSubtypeExposedToCSSOM());

    switch (classType()) {
    case AspectRatioClass:
        return toCSSAspectRatioValue(this)->customCSSText();
    case BorderImageSliceClass:
        return toCSSBorderImageSliceValue(this)->customCSSText();
    case CanvasClass:
        return toCSSCanvasValue(this)->customCSSText();
    case CursorImageClass:
        return toCSSCursorImageValue(this)->customCSSText();
    case FontClass:
        return toCSSFontValue(this)->customCSSText();
    case FontFaceSrcClass:
        return toCSSFontFaceSrcValue(this)->customCSSText();
    case FontFeatureClass:
        return toCSSFontFeatureValue(this)->customCSSText();
    case FunctionClass:
        return toCSSFunctionValue(this)->customCSSText();
    case LinearGradientClass:
        return toCSSLinearGradientValue(this)->customCSSText();
    case RadialGradientClass:
        return toCSSRadialGradientValue(this)->customCSSText();
    case CrossfadeClass:
        return toCSSCrossfadeValue(this)->customCSSText();
    case ImageClass:
        return toCSSImageValue(this)->customCSSText();
    case InheritedClass:
        return toCSSInheritedValue(this)->customCSSText();
    case InitialClass:
        return toCSSInitialValue(this)->customCSSText();
    case GridLineNamesClass:
        return toCSSGridLineNamesValue(this)->customCSSText();
    case GridTemplateAreasClass:
        return toCSSGridTemplateAreasValue(this)->customCSSText();
    case PrimitiveClass:
        return toCSSPrimitiveValue(this)->customCSSText();
    case ReflectClass:
        return toCSSReflectValue(this)->customCSSText();
    case ShadowClass:
        return toCSSShadowValue(this)->customCSSText();
    case CubicBezierTimingFunctionClass:
        return toCSSCubicBezierTimingFunctionValue(this)->customCSSText();
    case StepsTimingFunctionClass:
        return toCSSStepsTimingFunctionValue(this)->customCSSText();
    case UnicodeRangeClass:
        return toCSSUnicodeRangeValue(this)->customCSSText();
    case ValueListClass:
        return toCSSValueList(this)->customCSSText();
    case CSSTransformClass:
        return toCSSTransformValue(this)->customCSSText();
    case LineBoxContainClass:
        return toCSSLineBoxContainValue(this)->customCSSText();
    case CalculationClass:
        return toCSSCalcValue(this)->customCSSText();
    case ImageSetClass:
        return toCSSImageSetValue(this)->customCSSText();
    case CSSFilterClass:
        return toCSSFilterValue(this)->customCSSText();
    case CSSArrayFunctionValueClass:
        return toCSSArrayFunctionValue(this)->customCSSText();
    case SVGPaintClass:
        return toSVGPaint(this)->customCSSText();
    case CSSSVGDocumentClass:
        return toCSSSVGDocumentValue(this)->customCSSText();
    }
    ASSERT_NOT_REACHED();
    return String();
}

void CSSValue::destroy()
{
    if (m_isTextClone) {
        ASSERT(isCSSOMSafe());
        delete toTextCloneCSSValue(this);
        return;
    }
    ASSERT(!isCSSOMSafe() || isSubtypeExposedToCSSOM());

    switch (classType()) {
    case AspectRatioClass:
        delete toCSSAspectRatioValue(this);
        return;
    case BorderImageSliceClass:
        delete toCSSBorderImageSliceValue(this);
        return;
    case CanvasClass:
        delete toCSSCanvasValue(this);
        return;
    case CursorImageClass:
        delete toCSSCursorImageValue(this);
        return;
    case FontClass:
        delete toCSSFontValue(this);
        return;
    case FontFaceSrcClass:
        delete toCSSFontFaceSrcValue(this);
        return;
    case FontFeatureClass:
        delete toCSSFontFeatureValue(this);
        return;
    case FunctionClass:
        delete toCSSFunctionValue(this);
        return;
    case LinearGradientClass:
        delete toCSSLinearGradientValue(this);
        return;
    case RadialGradientClass:
        delete toCSSRadialGradientValue(this);
        return;
    case CrossfadeClass:
        delete toCSSCrossfadeValue(this);
        return;
    case ImageClass:
        delete toCSSImageValue(this);
        return;
    case InheritedClass:
        delete toCSSInheritedValue(this);
        return;
    case InitialClass:
        delete toCSSInitialValue(this);
        return;
    case GridLineNamesClass:
        delete toCSSGridLineNamesValue(this);
        return;
    case GridTemplateAreasClass:
        delete toCSSGridTemplateAreasValue(this);
        return;
    case PrimitiveClass:
        delete toCSSPrimitiveValue(this);
        return;
    case ReflectClass:
        delete toCSSReflectValue(this);
        return;
    case ShadowClass:
        delete toCSSShadowValue(this);
        return;
    case CubicBezierTimingFunctionClass:
        delete toCSSCubicBezierTimingFunctionValue(this);
        return;
    case StepsTimingFunctionClass:
        delete toCSSStepsTimingFunctionValue(this);
        return;
    case UnicodeRangeClass:
        delete toCSSUnicodeRangeValue(this);
        return;
    case ValueListClass:
        delete toCSSValueList(this);
        return;
    case CSSTransformClass:
        delete toCSSTransformValue(this);
        return;
    case LineBoxContainClass:
        delete toCSSLineBoxContainValue(this);
        return;
    case CalculationClass:
        delete toCSSCalcValue(this);
        return;
    case ImageSetClass:
        delete toCSSImageSetValue(this);
        return;
    case CSSFilterClass:
        delete toCSSFilterValue(this);
        return;
    case CSSArrayFunctionValueClass:
        delete toCSSArrayFunctionValue(this);
        return;
    case SVGPaintClass:
        delete toSVGPaint(this);
        return;
    case CSSSVGDocumentClass:
        delete toCSSSVGDocumentValue(this);
        return;
    }
    ASSERT_NOT_REACHED();
}

void CSSValue::finalize()
{
    if (m_isTextClone) {
        ASSERT(isCSSOMSafe());
        static_cast<TextCloneCSSValue*>(this)->~TextCloneCSSValue();
        return;
    }
    ASSERT(!isCSSOMSafe() || isSubtypeExposedToCSSOM());

    switch (classType()) {
    case AspectRatioClass:
        toCSSAspectRatioValue(this)->~CSSAspectRatioValue();
        return;
    case BorderImageSliceClass:
        toCSSBorderImageSliceValue(this)->~CSSBorderImageSliceValue();
        return;
    case CanvasClass:
        toCSSCanvasValue(this)->~CSSCanvasValue();
        return;
    case CursorImageClass:
        toCSSCursorImageValue(this)->~CSSCursorImageValue();
        return;
    case FontClass:
        toCSSFontValue(this)->~CSSFontValue();
        return;
    case FontFaceSrcClass:
        toCSSFontFaceSrcValue(this)->~CSSFontFaceSrcValue();
        return;
    case FontFeatureClass:
        toCSSFontFeatureValue(this)->~CSSFontFeatureValue();
        return;
    case FunctionClass:
        toCSSFunctionValue(this)->~CSSFunctionValue();
        return;
    case LinearGradientClass:
        toCSSLinearGradientValue(this)->~CSSLinearGradientValue();
        return;
    case RadialGradientClass:
        toCSSRadialGradientValue(this)->~CSSRadialGradientValue();
        return;
    case CrossfadeClass:
        toCSSCrossfadeValue(this)->~CSSCrossfadeValue();
        return;
    case ImageClass:
        toCSSImageValue(this)->~CSSImageValue();
        return;
    case InheritedClass:
        toCSSInheritedValue(this)->~CSSInheritedValue();
        return;
    case InitialClass:
        toCSSInitialValue(this)->~CSSInitialValue();
        return;
    case GridLineNamesClass:
        toCSSGridLineNamesValue(this)->~CSSGridLineNamesValue();
        return;
    case GridTemplateAreasClass:
        toCSSGridTemplateAreasValue(this)->~CSSGridTemplateAreasValue();
        return;
    case PrimitiveClass:
        toCSSPrimitiveValue(this)->~CSSPrimitiveValue();
        return;
    case ReflectClass:
        toCSSReflectValue(this)->~CSSReflectValue();
        return;
    case ShadowClass:
        toCSSShadowValue(this)->~CSSShadowValue();
        return;
    case CubicBezierTimingFunctionClass:
        toCSSCubicBezierTimingFunctionValue(this)->~CSSCubicBezierTimingFunctionValue();
        return;
    case StepsTimingFunctionClass:
        toCSSStepsTimingFunctionValue(this)->~CSSStepsTimingFunctionValue();
        return;
    case UnicodeRangeClass:
        toCSSUnicodeRangeValue(this)->~CSSUnicodeRangeValue();
        return;
    case ValueListClass:
        toCSSValueList(this)->~CSSValueList();
        return;
    case CSSTransformClass:
        toCSSTransformValue(this)->~CSSTransformValue();
        return;
    case LineBoxContainClass:
        toCSSLineBoxContainValue(this)->~CSSLineBoxContainValue();
        return;
    case CalculationClass:
        toCSSCalcValue(this)->~CSSCalcValue();
        return;
    case ImageSetClass:
        toCSSImageSetValue(this)->~CSSImageSetValue();
        return;
    case CSSFilterClass:
        toCSSFilterValue(this)->~CSSFilterValue();
        return;
    case CSSArrayFunctionValueClass:
        toCSSArrayFunctionValue(this)->~CSSArrayFunctionValue();
        return;
    case SVGPaintClass:
        toSVGPaint(this)->~SVGPaint();
        return;
    case CSSSVGDocumentClass:
        toCSSSVGDocumentValue(this)->~CSSSVGDocumentValue();
        return;
    }
    ASSERT_NOT_REACHED();
}

void CSSValue::trace(Visitor* visitor)
{
    if (m_isTextClone) {
        ASSERT(isCSSOMSafe());
        static_cast<TextCloneCSSValue*>(this)->traceAfterDispatch(visitor);
        return;
    }
    ASSERT(!isCSSOMSafe() || isSubtypeExposedToCSSOM());

    switch (classType()) {
    case AspectRatioClass:
        static_cast<CSSAspectRatioValue*>(this)->traceAfterDispatch(visitor);
        return;
    case BorderImageSliceClass:
        static_cast<CSSBorderImageSliceValue*>(this)->traceAfterDispatch(visitor);
        return;
    case CanvasClass:
        static_cast<CSSCanvasValue*>(this)->traceAfterDispatch(visitor);
        return;
    case CursorImageClass:
        static_cast<CSSCursorImageValue*>(this)->traceAfterDispatch(visitor);
        return;
    case FontClass:
        static_cast<CSSFontValue*>(this)->traceAfterDispatch(visitor);
        return;
    case FontFaceSrcClass:
        static_cast<CSSFontFaceSrcValue*>(this)->traceAfterDispatch(visitor);
        return;
    case FontFeatureClass:
        static_cast<CSSFontFeatureValue*>(this)->traceAfterDispatch(visitor);
        return;
    case FunctionClass:
        static_cast<CSSFunctionValue*>(this)->traceAfterDispatch(visitor);
        return;
    case LinearGradientClass:
        static_cast<CSSLinearGradientValue*>(this)->traceAfterDispatch(visitor);
        return;
    case RadialGradientClass:
        static_cast<CSSRadialGradientValue*>(this)->traceAfterDispatch(visitor);
        return;
    case CrossfadeClass:
        static_cast<CSSCrossfadeValue*>(this)->traceAfterDispatch(visitor);
        return;
    case ImageClass:
        static_cast<CSSImageValue*>(this)->traceAfterDispatch(visitor);
        return;
    case InheritedClass:
        static_cast<CSSInheritedValue*>(this)->traceAfterDispatch(visitor);
        return;
    case InitialClass:
        static_cast<CSSInitialValue*>(this)->traceAfterDispatch(visitor);
        return;
    case GridLineNamesClass:
        static_cast<CSSGridLineNamesValue*>(this)->traceAfterDispatch(visitor);
        return;
    case GridTemplateAreasClass:
        static_cast<CSSGridTemplateAreasValue*>(this)->traceAfterDispatch(visitor);
        return;
    case PrimitiveClass:
        static_cast<CSSPrimitiveValue*>(this)->traceAfterDispatch(visitor);
        return;
    case ReflectClass:
        static_cast<CSSReflectValue*>(this)->traceAfterDispatch(visitor);
        return;
    case ShadowClass:
        static_cast<CSSShadowValue*>(this)->traceAfterDispatch(visitor);
        return;
    case CubicBezierTimingFunctionClass:
        static_cast<CSSCubicBezierTimingFunctionValue*>(this)->traceAfterDispatch(visitor);
        return;
    case StepsTimingFunctionClass:
        static_cast<CSSStepsTimingFunctionValue*>(this)->traceAfterDispatch(visitor);
        return;
    case UnicodeRangeClass:
        static_cast<CSSUnicodeRangeValue*>(this)->traceAfterDispatch(visitor);
        return;
    case ValueListClass:
        static_cast<CSSValueList*>(this)->traceAfterDispatch(visitor);
        return;
    case CSSTransformClass:
        static_cast<CSSTransformValue*>(this)->traceAfterDispatch(visitor);
        return;
    case LineBoxContainClass:
        static_cast<CSSLineBoxContainValue*>(this)->traceAfterDispatch(visitor);
        return;
    case CalculationClass:
        static_cast<CSSCalcValue*>(this)->traceAfterDispatch(visitor);
        return;
    case ImageSetClass:
        static_cast<CSSImageSetValue*>(this)->traceAfterDispatch(visitor);
        return;
    case CSSFilterClass:
        static_cast<CSSFilterValue*>(this)->traceAfterDispatch(visitor);
        return;
    case CSSArrayFunctionValueClass:
        static_cast<CSSArrayFunctionValue*>(this)->traceAfterDispatch(visitor);
        return;
    case SVGPaintClass:
        static_cast<SVGPaint*>(this)->traceAfterDispatch(visitor);
        return;
    case CSSSVGDocumentClass:
        static_cast<CSSSVGDocumentValue*>(this)->traceAfterDispatch(visitor);
        return;
    }
    ASSERT_NOT_REACHED();
}

PassRefPtrWillBeRawPtr<CSSValue> CSSValue::cloneForCSSOM() const
{
    switch (classType()) {
    case PrimitiveClass:
        return toCSSPrimitiveValue(this)->cloneForCSSOM();
    case ValueListClass:
        return toCSSValueList(this)->cloneForCSSOM();
    case ImageClass:
    case CursorImageClass:
        return toCSSImageValue(this)->cloneForCSSOM();
    case CSSFilterClass:
        return toCSSFilterValue(this)->cloneForCSSOM();
    case CSSArrayFunctionValueClass:
        return toCSSArrayFunctionValue(this)->cloneForCSSOM();
    case CSSTransformClass:
        return toCSSTransformValue(this)->cloneForCSSOM();
    case ImageSetClass:
        return toCSSImageSetValue(this)->cloneForCSSOM();
    case SVGPaintClass:
        return toSVGPaint(this)->cloneForCSSOM();
    default:
        ASSERT(!isSubtypeExposedToCSSOM());
        return TextCloneCSSValue::create(classType(), cssText());
    }
}

}
