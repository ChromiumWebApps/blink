/*
 * This file is part of the WebKit project.
 *
 * Copyright (C) 2006 Oliver Hunt <ojh16@student.canterbury.ac.nz>
 * Copyright (C) 2006 Apple Computer Inc.
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

#include "core/rendering/svg/RenderSVGTSpan.h"

#include "SVGNames.h"
#include "core/rendering/svg/SVGRenderSupport.h"
#include "core/svg/SVGAltGlyphElement.h"

namespace WebCore {

RenderSVGTSpan::RenderSVGTSpan(Element* element)
    : RenderSVGInline(element)
{
}

bool RenderSVGTSpan::isChildAllowed(RenderObject* child, RenderStyle*) const
{
    // Always allow text (except empty textnodes and <br>).
    if (child->isText())
        return SVGRenderSupport::isRenderableTextNode(child);

#if ENABLE(SVG_FONTS)
    // Only allow other types of  children if this is not an 'altGlyph'.
    ASSERT(node());
    if (isSVGAltGlyphElement(*node()))
        return false;
#endif

    return child->isSVGInline() && !child->isSVGTextPath();
}

}
