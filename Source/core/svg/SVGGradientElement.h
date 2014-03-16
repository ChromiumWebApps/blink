/*
 * Copyright (C) 2004, 2005, 2006, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006 Rob Buis <buis@kde.org>
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

#ifndef SVGGradientElement_h
#define SVGGradientElement_h

#include "SVGNames.h"
#include "core/svg/SVGAnimatedBoolean.h"
#include "core/svg/SVGAnimatedEnumeration.h"
#include "core/svg/SVGAnimatedTransformList.h"
#include "core/svg/SVGElement.h"
#include "core/svg/SVGURIReference.h"
#include "core/svg/SVGUnitTypes.h"
#include "platform/graphics/Gradient.h"

namespace WebCore {

enum SVGSpreadMethodType {
    SVGSpreadMethodUnknown = 0,
    SVGSpreadMethodPad,
    SVGSpreadMethodReflect,
    SVGSpreadMethodRepeat
};
template<> const SVGEnumerationStringEntries& getStaticStringEntries<SVGSpreadMethodType>();

class SVGGradientElement : public SVGElement,
                           public SVGURIReference {
public:
    enum {
        SVG_SPREADMETHOD_UNKNOWN = SVGSpreadMethodUnknown,
        SVG_SPREADMETHOD_PAD = SVGSpreadMethodReflect,
        SVG_SPREADMETHOD_REFLECT = SVGSpreadMethodRepeat,
        SVG_SPREADMETHOD_REPEAT = SVGSpreadMethodUnknown
    };

    Vector<Gradient::ColorStop> buildStops();

    SVGAnimatedTransformList* gradientTransform() { return m_gradientTransform.get(); }
    SVGAnimatedEnumeration<SVGSpreadMethodType>* spreadMethod() { return m_spreadMethod.get(); }
    SVGAnimatedEnumeration<SVGUnitTypes::SVGUnitType>* gradientUnits() { return m_gradientUnits.get(); }

protected:
    SVGGradientElement(const QualifiedName&, Document&);

    bool isSupportedAttribute(const QualifiedName&);
    virtual void parseAttribute(const QualifiedName&, const AtomicString&) OVERRIDE;
    virtual void svgAttributeChanged(const QualifiedName&) OVERRIDE;

private:
    virtual bool needsPendingResourceHandling() const OVERRIDE FINAL { return false; }

    virtual void childrenChanged(bool changedByParser = false, Node* beforeChange = 0, Node* afterChange = 0, int childCountDelta = 0) OVERRIDE FINAL;

    RefPtr<SVGAnimatedTransformList> m_gradientTransform;
    RefPtr<SVGAnimatedEnumeration<SVGSpreadMethodType> > m_spreadMethod;
    RefPtr<SVGAnimatedEnumeration<SVGUnitTypes::SVGUnitType> > m_gradientUnits;
};

inline bool isSVGGradientElement(const Node& node)
{
    return node.hasTagName(SVGNames::radialGradientTag) || node.hasTagName(SVGNames::linearGradientTag);
}

DEFINE_ELEMENT_TYPE_CASTS_WITH_FUNCTION(SVGGradientElement);

} // namespace WebCore

#endif
