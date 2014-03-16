/*
 * Copyright (C) 2002, 2003 The Karbon Developers
 * Copyright (C) 2006, 2007 Rob Buis <buis@kde.org>
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

#ifndef SVGParserUtilities_h
#define SVGParserUtilities_h

#include "core/svg/SVGTransform.h"
#include "platform/text/ParserUtilities.h"
#include "wtf/HashSet.h"

typedef pair<unsigned, unsigned> UnicodeRange;
typedef Vector<UnicodeRange> UnicodeRanges;

namespace WebCore {

class FloatPoint;
class FloatRect;
class SVGPointList;

template <typename CharType>
bool parseSVGNumber(CharType* ptr, size_t length, double& number);
bool parseNumber(const LChar*& ptr, const LChar* end, float& number, bool skip = true);
bool parseNumber(const UChar*& ptr, const UChar* end, float& number, bool skip = true);
bool parseNumberOptionalNumber(const String& s, float& h, float& v);
bool parseArcFlag(const LChar*& ptr, const LChar* end, bool& flag);
bool parseArcFlag(const UChar*& ptr, const UChar* end, bool& flag);

template <typename CharType>
bool parseFloatPoint(const CharType*& current, const CharType* end, FloatPoint&);
template <typename CharType>
bool parseFloatPoint2(const CharType*& current, const CharType* end, FloatPoint&, FloatPoint&);
template <typename CharType>
bool parseFloatPoint3(const CharType*& current, const CharType* end, FloatPoint&, FloatPoint&, FloatPoint&);

// SVG allows several different whitespace characters:
// http://www.w3.org/TR/SVG/paths.html#PathDataBNF
template <typename CharType>
inline bool isSVGSpace(CharType c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

template <typename CharType>
inline bool skipOptionalSVGSpaces(const CharType*& ptr, const CharType* end)
{
    while (ptr < end && isSVGSpace(*ptr))
        ptr++;
    return ptr < end;
}

template <typename CharType>
inline bool skipOptionalSVGSpacesOrDelimiter(const CharType*& ptr, const CharType* end, char delimiter = ',')
{
    if (ptr < end && !isSVGSpace(*ptr) && *ptr != delimiter)
        return false;
    if (skipOptionalSVGSpaces(ptr, end)) {
        if (ptr < end && *ptr == delimiter) {
            ptr++;
            skipOptionalSVGSpaces(ptr, end);
        }
    }
    return ptr < end;
}

Vector<String> parseDelimitedString(const String& input, const char seperator);
bool parseKerningUnicodeString(const String& input, UnicodeRanges&, HashSet<String>& stringList);
bool parseGlyphName(const String& input, HashSet<String>& values);

template<typename CharType>
bool parseAndSkipTransformType(const CharType*& ptr, const CharType* end, SVGTransformType&);
SVGTransformType parseTransformType(const String&);

} // namespace WebCore

#endif // SVGParserUtilities_h
