/*
 * Copyright (C) 2004, 2005, 2006, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007 Rob Buis <buis@kde.org>
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

#ifndef SVGPolyElement_h
#define SVGPolyElement_h

#include "SVGNames.h"
#include "core/svg/SVGAnimatedBoolean.h"
#include "core/svg/SVGAnimatedPointList.h"
#include "core/svg/SVGGeometryElement.h"

namespace WebCore {

class SVGPolyElement : public SVGGeometryElement {
public:
    SVGAnimatedPointList* points() { return m_points.get(); }

    PassRefPtr<SVGPointListTearOff> pointsFromJavascript() { return m_points->baseVal(); }
    PassRefPtr<SVGPointListTearOff> animatedPoints() { return m_points->animVal(); }

protected:
    SVGPolyElement(const QualifiedName&, Document&);

private:
    virtual bool supportsFocus() const OVERRIDE FINAL { return hasFocusEventListeners(); }

    bool isSupportedAttribute(const QualifiedName&);
    virtual void parseAttribute(const QualifiedName&, const AtomicString&) OVERRIDE FINAL;
    virtual void svgAttributeChanged(const QualifiedName&) OVERRIDE FINAL;

    virtual bool supportsMarkers() const OVERRIDE FINAL { return true; }

private:
    RefPtr<SVGAnimatedPointList> m_points;

};

inline bool isSVGPolyElement(const Node& node)
{
    return node.hasTagName(SVGNames::polygonTag) || node.hasTagName(SVGNames::polylineTag);
}

DEFINE_ELEMENT_TYPE_CASTS_WITH_FUNCTION(SVGPolyElement);

} // namespace WebCore

#endif
