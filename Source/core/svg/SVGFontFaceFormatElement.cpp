/*
 * Copyright (C) 2007 Eric Seidel <eric@webkit.org>
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

#if ENABLE(SVG_FONTS)
#include "core/svg/SVGFontFaceFormatElement.h"

#include "SVGNames.h"
#include "core/svg/SVGFontFaceElement.h"

namespace WebCore {

using namespace SVGNames;

inline SVGFontFaceFormatElement::SVGFontFaceFormatElement(Document& document)
    : SVGElement(font_face_formatTag, document)
{
    ScriptWrappable::init(this);
}

PassRefPtr<SVGFontFaceFormatElement> SVGFontFaceFormatElement::create(Document& document)
{
    return adoptRef(new SVGFontFaceFormatElement(document));
}

void SVGFontFaceFormatElement::childrenChanged(bool changedByParser, Node* beforeChange, Node* afterChange, int childCountDelta)
{
    SVGElement::childrenChanged(changedByParser, beforeChange, afterChange, childCountDelta);

    if (!isSVGFontFaceUriElement(parentNode()))
        return;

    ContainerNode* ancestor = parentNode()->parentNode();
    if (!isSVGFontFaceSrcElement(ancestor))
        return;

    ancestor = ancestor->parentNode();
    if (isSVGFontFaceElement(ancestor))
        toSVGFontFaceElement(ancestor)->rebuildFontFace();
}

}

#endif // ENABLE(SVG_FONTS)
