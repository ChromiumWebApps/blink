/*
 * CSS Media Query
 *
 * Copyright (C) 2006 Kimmo Kinnunen <kimmo.t.kinnunen@nokia.com>.
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY
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
 */

#include "config.h"
#include "core/css/MediaQueryExp.h"

#include "CSSValueKeywords.h"
#include "core/css/CSSAspectRatioValue.h"
#include "core/css/CSSParserValues.h"
#include "core/css/CSSPrimitiveValue.h"
#include "core/html/parser/HTMLParserIdioms.h"
#include "wtf/text/StringBuilder.h"

namespace WebCore {

using namespace MediaFeatureNames;

static inline bool featureWithCSSValueID(const String& mediaFeature, const CSSParserValue* value)
{
    if (!value->id)
        return false;

    return mediaFeature == orientationMediaFeature
        || mediaFeature == viewModeMediaFeature
        || mediaFeature == pointerMediaFeature
        || mediaFeature == scanMediaFeature;
}

static inline bool featureWithValidIdent(const String& mediaFeature, CSSValueID ident)
{
    if (mediaFeature == orientationMediaFeature)
        return ident == CSSValuePortrait || ident == CSSValueLandscape;

    if (mediaFeature == viewModeMediaFeature) {
        switch (ident) {
        case CSSValueWindowed:
        case CSSValueFloating:
        case CSSValueFullscreen:
        case CSSValueMaximized:
        case CSSValueMinimized:
            return true;
        default:
            return false;
        }
    }

    if (mediaFeature == pointerMediaFeature)
        return ident == CSSValueNone || ident == CSSValueCoarse || ident == CSSValueFine;

    if (mediaFeature == scanMediaFeature)
        return ident == CSSValueInterlace || ident == CSSValueProgressive;

    ASSERT_NOT_REACHED();
    return false;
}

static inline bool featureWithValidPositiveLength(const String& mediaFeature, const CSSParserValue* value)
{
    if (!(((value->unit >= CSSPrimitiveValue::CSS_EMS && value->unit <= CSSPrimitiveValue::CSS_PC) || value->unit == CSSPrimitiveValue::CSS_REMS) || (value->unit == CSSPrimitiveValue::CSS_NUMBER && !(value->fValue))) || value->fValue < 0)
        return false;


    return mediaFeature == heightMediaFeature
        || mediaFeature == maxHeightMediaFeature
        || mediaFeature == minHeightMediaFeature
        || mediaFeature == widthMediaFeature
        || mediaFeature == maxWidthMediaFeature
        || mediaFeature == minWidthMediaFeature
        || mediaFeature == deviceHeightMediaFeature
        || mediaFeature == maxDeviceHeightMediaFeature
        || mediaFeature == minDeviceHeightMediaFeature
        || mediaFeature == deviceWidthMediaFeature
        || mediaFeature == minDeviceWidthMediaFeature
        || mediaFeature == maxDeviceWidthMediaFeature;
}

static inline bool featureWithValidDensity(const String& mediaFeature, const CSSParserValue* value)
{
    if ((value->unit != CSSPrimitiveValue::CSS_DPPX && value->unit != CSSPrimitiveValue::CSS_DPI && value->unit != CSSPrimitiveValue::CSS_DPCM) || value->fValue <= 0)
        return false;

    return mediaFeature == resolutionMediaFeature
        || mediaFeature == minResolutionMediaFeature
        || mediaFeature == maxResolutionMediaFeature;
}

static inline bool featureWithPositiveInteger(const String& mediaFeature, const CSSParserValue* value)
{
    if (!value->isInt || value->fValue < 0)
        return false;

    return mediaFeature == colorMediaFeature
        || mediaFeature == maxColorMediaFeature
        || mediaFeature == minColorMediaFeature
        || mediaFeature == colorIndexMediaFeature
        || mediaFeature == maxColorIndexMediaFeature
        || mediaFeature == minColorIndexMediaFeature
        || mediaFeature == monochromeMediaFeature
        || mediaFeature == maxMonochromeMediaFeature
        || mediaFeature == minMonochromeMediaFeature;
}

static inline bool featureWithPositiveNumber(const String& mediaFeature, const CSSParserValue* value)
{
    if (value->unit != CSSPrimitiveValue::CSS_NUMBER || value->fValue < 0)
        return false;

    return mediaFeature == transform2dMediaFeature
        || mediaFeature == transform3dMediaFeature
        || mediaFeature == animationMediaFeature
        || mediaFeature == devicePixelRatioMediaFeature
        || mediaFeature == maxDevicePixelRatioMediaFeature
        || mediaFeature == minDevicePixelRatioMediaFeature;
}

static inline bool featureWithZeroOrOne(const String& mediaFeature, const CSSParserValue* value)
{
    if (!value->isInt || !(value->fValue == 1 || !value->fValue))
        return false;

    return mediaFeature == gridMediaFeature
        || mediaFeature == hoverMediaFeature;
}

static inline bool featureWithAspectRatio(const String& mediaFeature)
{
    return mediaFeature == aspectRatioMediaFeature
        || mediaFeature == deviceAspectRatioMediaFeature
        || mediaFeature == minAspectRatioMediaFeature
        || mediaFeature == maxAspectRatioMediaFeature
        || mediaFeature == minDeviceAspectRatioMediaFeature
        || mediaFeature == maxDeviceAspectRatioMediaFeature;
}

static inline bool featureWithoutValue(const String& mediaFeature)
{
    // Media features that are prefixed by min/max cannot be used without a value.
    return mediaFeature == monochromeMediaFeature
        || mediaFeature == colorMediaFeature
        || mediaFeature == colorIndexMediaFeature
        || mediaFeature == gridMediaFeature
        || mediaFeature == heightMediaFeature
        || mediaFeature == widthMediaFeature
        || mediaFeature == deviceHeightMediaFeature
        || mediaFeature == deviceWidthMediaFeature
        || mediaFeature == orientationMediaFeature
        || mediaFeature == aspectRatioMediaFeature
        || mediaFeature == deviceAspectRatioMediaFeature
        || mediaFeature == hoverMediaFeature
        || mediaFeature == transform2dMediaFeature
        || mediaFeature == transform3dMediaFeature
        || mediaFeature == animationMediaFeature
        || mediaFeature == viewModeMediaFeature
        || mediaFeature == pointerMediaFeature
        || mediaFeature == devicePixelRatioMediaFeature
        || mediaFeature == resolutionMediaFeature
        || mediaFeature == scanMediaFeature;
}

bool MediaQueryExp::isViewportDependent() const
{
    return m_mediaFeature == widthMediaFeature
        || m_mediaFeature == heightMediaFeature
        || m_mediaFeature == minWidthMediaFeature
        || m_mediaFeature == minHeightMediaFeature
        || m_mediaFeature == maxWidthMediaFeature
        || m_mediaFeature == maxHeightMediaFeature
        || m_mediaFeature == orientationMediaFeature
        || m_mediaFeature == aspectRatioMediaFeature
        || m_mediaFeature == minAspectRatioMediaFeature
        || m_mediaFeature == devicePixelRatioMediaFeature
        || m_mediaFeature == resolutionMediaFeature
        || m_mediaFeature == maxAspectRatioMediaFeature;
}

MediaQueryExp::MediaQueryExp(const MediaQueryExp& other)
    : m_mediaFeature(other.mediaFeature())
    , m_value(other.value())
{
}

MediaQueryExp::MediaQueryExp(const String& mediaFeature, PassRefPtrWillBeRawPtr<CSSValue> value)
    : m_mediaFeature(mediaFeature)
    , m_value(value)
{
}

PassOwnPtrWillBeRawPtr<MediaQueryExp> MediaQueryExp::create(const String& mediaFeature, CSSParserValueList* valueList)
{
    RefPtrWillBeRawPtr<CSSValue> cssValue;
    bool isValid = false;
    String lowerMediaFeature = attemptStaticStringCreation(mediaFeature.lower());

    // Create value for media query expression that must have 1 or more values.
    if (valueList && valueList->size() > 0) {
        if (valueList->size() == 1) {
            CSSParserValue* value = valueList->current();

            if (featureWithCSSValueID(lowerMediaFeature, value)) {
                // Media features that use CSSValueIDs.
                cssValue = CSSPrimitiveValue::createIdentifier(value->id);
                if (!featureWithValidIdent(lowerMediaFeature, toCSSPrimitiveValue(cssValue.get())->getValueID()))
                    cssValue.clear();
            } else if (featureWithValidDensity(lowerMediaFeature, value)) {
                // Media features that must have non-negative <density>, ie. dppx, dpi or dpcm.
                cssValue = CSSPrimitiveValue::create(value->fValue, (CSSPrimitiveValue::UnitTypes) value->unit);
            } else if (featureWithValidPositiveLength(lowerMediaFeature, value)) {
                // Media features that must have non-negative <lenght> or number value.
                cssValue = CSSPrimitiveValue::create(value->fValue, (CSSPrimitiveValue::UnitTypes) value->unit);
            } else if (featureWithPositiveInteger(lowerMediaFeature, value)) {
                // Media features that must have non-negative integer value.
                cssValue = CSSPrimitiveValue::create(value->fValue, CSSPrimitiveValue::CSS_NUMBER);
            } else if (featureWithPositiveNumber(lowerMediaFeature, value)) {
                // Media features that must have non-negative number value.
                cssValue = CSSPrimitiveValue::create(value->fValue, CSSPrimitiveValue::CSS_NUMBER);
            } else if (featureWithZeroOrOne(lowerMediaFeature, value)) {
                // Media features that must have (0|1) value.
                cssValue = CSSPrimitiveValue::create(value->fValue, CSSPrimitiveValue::CSS_NUMBER);
            }

            isValid = cssValue;

        } else if (valueList->size() == 3 && featureWithAspectRatio(lowerMediaFeature)) {
            // Create list of values.
            // Currently accepts only <integer>/<integer>.
            // Applicable to device-aspect-ratio and aspec-ratio.
            isValid = true;
            float numeratorValue = 0;
            float denominatorValue = 0;
            // The aspect-ratio must be <integer> (whitespace)? / (whitespace)? <integer>.
            for (unsigned i = 0; i < 3; ++i, valueList->next()) {
                const CSSParserValue* value = valueList->current();
                if (i != 1 && value->unit == CSSPrimitiveValue::CSS_NUMBER && value->fValue > 0 && value->isInt) {
                    if (!i)
                        numeratorValue = value->fValue;
                    else
                        denominatorValue = value->fValue;
                } else if (i == 1 && value->unit == CSSParserValue::Operator && value->iValue == '/') {
                    continue;
                } else {
                    isValid = false;
                    break;
                }
            }

            if (isValid)
                cssValue = CSSAspectRatioValue::create(numeratorValue, denominatorValue);
        }
    } else if (featureWithoutValue(lowerMediaFeature)) {
        isValid = true;
    }

    if (!isValid)
        return nullptr;

    return adoptPtrWillBeNoop(new MediaQueryExp(lowerMediaFeature, cssValue));
}

MediaQueryExp::~MediaQueryExp()
{
}

String MediaQueryExp::serialize() const
{
    StringBuilder result;
    result.append("(");
    result.append(m_mediaFeature.lower());
    if (m_value) {
        result.append(": ");
        result.append(m_value->cssText());
    }
    result.append(")");

    return result.toString();
}

} // namespace
