/*
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.
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

#ifndef CSSProperty_h
#define CSSProperty_h

#include "CSSPropertyNames.h"
#include "RuntimeEnabledFeatures.h"
#include "core/css/CSSValue.h"
#include "platform/text/TextDirection.h"
#include "platform/text/WritingMode.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefPtr.h"

namespace WebCore {

struct StylePropertyMetadata {
    StylePropertyMetadata(CSSPropertyID propertyID, bool isSetFromShorthand, int indexInShorthandsVector, bool important, bool implicit, bool inherited)
        : m_propertyID(propertyID)
        , m_isSetFromShorthand(isSetFromShorthand)
        , m_indexInShorthandsVector(indexInShorthandsVector)
        , m_important(important)
        , m_implicit(implicit)
        , m_inherited(inherited)
    {
    }

    CSSPropertyID shorthandID() const;

    uint16_t m_propertyID : 10;
    uint16_t m_isSetFromShorthand : 1;
    uint16_t m_indexInShorthandsVector : 2; // If this property was set as part of an ambiguous shorthand, gives the index in the shorthands vector.
    uint16_t m_important : 1;
    uint16_t m_implicit : 1; // Whether or not the property was set implicitly as the result of a shorthand.
    uint16_t m_inherited : 1;
};

class CSSProperty {
    ALLOW_ONLY_INLINE_ALLOCATION();
public:
    CSSProperty(CSSPropertyID propertyID, PassRefPtrWillBeRawPtr<CSSValue> value, bool important = false, bool isSetFromShorthand = false, int indexInShorthandsVector = 0, bool implicit = false)
        : m_metadata(propertyID, isSetFromShorthand, indexInShorthandsVector, important, implicit, isInheritedProperty(propertyID))
        , m_value(value)
    {
    }

    // FIXME: Remove this.
    CSSProperty(StylePropertyMetadata metadata, CSSValue* value)
        : m_metadata(metadata)
        , m_value(value)
    {
    }

    CSSPropertyID id() const { return static_cast<CSSPropertyID>(m_metadata.m_propertyID); }
    bool isSetFromShorthand() const { return m_metadata.m_isSetFromShorthand; };
    CSSPropertyID shorthandID() const { return m_metadata.shorthandID(); };
    bool isImportant() const { return m_metadata.m_important; }

    CSSValue* value() const { return m_value.get(); }

    void wrapValueInCommaSeparatedList();

    static CSSPropertyID resolveDirectionAwareProperty(CSSPropertyID, TextDirection, WritingMode);
    static bool isInheritedProperty(CSSPropertyID);

    const StylePropertyMetadata& metadata() const { return m_metadata; }

    void trace(Visitor* visitor) { visitor->trace(m_value); }

private:
    StylePropertyMetadata m_metadata;
    RefPtrWillBeMember<CSSValue> m_value;
};

inline CSSPropertyID prefixingVariantForPropertyId(CSSPropertyID propId)
{
    if (!RuntimeEnabledFeatures::cssAnimationUnprefixedEnabled() && (propId >= CSSPropertyWebkitAnimation && propId <= CSSPropertyAnimationTimingFunction))
        return propId;

    CSSPropertyID propertyId = CSSPropertyInvalid;
    switch (propId) {
    case CSSPropertyAnimation:
        propertyId = CSSPropertyWebkitAnimation;
        break;
    case CSSPropertyAnimationDelay:
        propertyId = CSSPropertyWebkitAnimationDelay;
        break;
    case CSSPropertyAnimationDirection:
        propertyId = CSSPropertyWebkitAnimationDirection;
        break;
    case CSSPropertyAnimationDuration:
        propertyId = CSSPropertyWebkitAnimationDuration;
        break;
    case CSSPropertyAnimationFillMode:
        propertyId = CSSPropertyWebkitAnimationFillMode;
        break;
    case CSSPropertyAnimationIterationCount:
        propertyId = CSSPropertyWebkitAnimationIterationCount;
        break;
    case CSSPropertyAnimationName:
        propertyId = CSSPropertyWebkitAnimationName;
        break;
    case CSSPropertyAnimationPlayState:
        propertyId = CSSPropertyWebkitAnimationPlayState;
        break;
    case CSSPropertyAnimationTimingFunction:
        propertyId = CSSPropertyWebkitAnimationTimingFunction;
        break;
    case CSSPropertyTransitionDelay:
        propertyId = CSSPropertyWebkitTransitionDelay;
        break;
    case CSSPropertyTransitionDuration:
        propertyId = CSSPropertyWebkitTransitionDuration;
        break;
    case CSSPropertyTransitionProperty:
        propertyId = CSSPropertyWebkitTransitionProperty;
        break;
    case CSSPropertyTransitionTimingFunction:
        propertyId = CSSPropertyWebkitTransitionTimingFunction;
        break;
    case CSSPropertyTransition:
        propertyId = CSSPropertyWebkitTransition;
        break;
    case CSSPropertyWebkitAnimation:
        propertyId = CSSPropertyAnimation;
        break;
    case CSSPropertyWebkitAnimationDelay:
        propertyId = CSSPropertyAnimationDelay;
        break;
    case CSSPropertyWebkitAnimationDirection:
        propertyId = CSSPropertyAnimationDirection;
        break;
    case CSSPropertyWebkitAnimationDuration:
        propertyId = CSSPropertyAnimationDuration;
        break;
    case CSSPropertyWebkitAnimationFillMode:
        propertyId = CSSPropertyAnimationFillMode;
        break;
    case CSSPropertyWebkitAnimationIterationCount:
        propertyId = CSSPropertyAnimationIterationCount;
        break;
    case CSSPropertyWebkitAnimationName:
        propertyId = CSSPropertyAnimationName;
        break;
    case CSSPropertyWebkitAnimationPlayState:
        propertyId = CSSPropertyAnimationPlayState;
        break;
    case CSSPropertyWebkitAnimationTimingFunction:
        propertyId = CSSPropertyAnimationTimingFunction;
        break;
    case CSSPropertyWebkitTransitionDelay:
        propertyId = CSSPropertyTransitionDelay;
        break;
    case CSSPropertyWebkitTransitionDuration:
        propertyId = CSSPropertyTransitionDuration;
        break;
    case CSSPropertyWebkitTransitionProperty:
        propertyId = CSSPropertyTransitionProperty;
        break;
    case CSSPropertyWebkitTransitionTimingFunction:
        propertyId = CSSPropertyTransitionTimingFunction;
        break;
    case CSSPropertyWebkitTransition:
        propertyId = CSSPropertyTransition;
        break;
    default:
        propertyId = propId;
        break;
    }
    ASSERT(propertyId != CSSPropertyInvalid);
    return propertyId;
}

} // namespace WebCore

namespace WTF {
template <> struct VectorTraits<WebCore::CSSProperty> : VectorTraitsBase<WebCore::CSSProperty> {
    static const bool canInitializeWithMemset = true;
    static const bool canMoveWithMemcpy = true;
};
}

#endif // CSSProperty_h
