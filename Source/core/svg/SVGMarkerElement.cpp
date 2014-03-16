/*
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007 Rob Buis <buis@kde.org>
 * Copyright (C) Research In Motion Limited 2009-2010. All rights reserved.
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

#include "core/svg/SVGMarkerElement.h"

#include "SVGNames.h"
#include "core/rendering/svg/RenderSVGResourceMarker.h"
#include "core/svg/SVGAngleTearOff.h"
#include "core/svg/SVGElementInstance.h"

namespace WebCore {

template<> const SVGEnumerationStringEntries& getStaticStringEntries<SVGMarkerUnitsType>()
{
    DEFINE_STATIC_LOCAL(SVGEnumerationStringEntries, entries, ());
    if (entries.isEmpty()) {
        entries.append(std::make_pair(SVGMarkerUnitsUnknown, emptyString()));
        entries.append(std::make_pair(SVGMarkerUnitsUserSpaceOnUse, "userSpaceOnUse"));
        entries.append(std::make_pair(SVGMarkerUnitsStrokeWidth, "strokeWidth"));
    }
    return entries;
}


inline SVGMarkerElement::SVGMarkerElement(Document& document)
    : SVGElement(SVGNames::markerTag, document)
    , SVGFitToViewBox(this)
    , m_refX(SVGAnimatedLength::create(this, SVGNames::refXAttr, SVGLength::create(LengthModeWidth)))
    , m_refY(SVGAnimatedLength::create(this, SVGNames::refXAttr, SVGLength::create(LengthModeWidth)))
    , m_markerWidth(SVGAnimatedLength::create(this, SVGNames::markerWidthAttr, SVGLength::create(LengthModeWidth)))
    , m_markerHeight(SVGAnimatedLength::create(this, SVGNames::markerHeightAttr, SVGLength::create(LengthModeHeight)))
    , m_orientAngle(SVGAnimatedAngle::create(this))
    , m_markerUnits(SVGAnimatedEnumeration<SVGMarkerUnitsType>::create(this, SVGNames::markerUnitsAttr, SVGMarkerUnitsStrokeWidth))
{
    ScriptWrappable::init(this);

    // Spec: If the markerWidth/markerHeight attribute is not specified, the effect is as if a value of "3" were specified.
    m_markerWidth->setDefaultValueAsString("3");
    m_markerHeight->setDefaultValueAsString("3");

    addToPropertyMap(m_refX);
    addToPropertyMap(m_refY);
    addToPropertyMap(m_markerWidth);
    addToPropertyMap(m_markerHeight);
    addToPropertyMap(m_orientAngle);
    addToPropertyMap(m_markerUnits);
}

PassRefPtr<SVGMarkerElement> SVGMarkerElement::create(Document& document)
{
    return adoptRef(new SVGMarkerElement(document));
}

AffineTransform SVGMarkerElement::viewBoxToViewTransform(float viewWidth, float viewHeight) const
{
    return SVGFitToViewBox::viewBoxToViewTransform(viewBox()->currentValue()->value(), preserveAspectRatio()->currentValue(), viewWidth, viewHeight);
}

bool SVGMarkerElement::isSupportedAttribute(const QualifiedName& attrName)
{
    DEFINE_STATIC_LOCAL(HashSet<QualifiedName>, supportedAttributes, ());
    if (supportedAttributes.isEmpty()) {
        SVGFitToViewBox::addSupportedAttributes(supportedAttributes);
        supportedAttributes.add(SVGNames::markerUnitsAttr);
        supportedAttributes.add(SVGNames::refXAttr);
        supportedAttributes.add(SVGNames::refYAttr);
        supportedAttributes.add(SVGNames::markerWidthAttr);
        supportedAttributes.add(SVGNames::markerHeightAttr);
        supportedAttributes.add(SVGNames::orientAttr);
    }
    return supportedAttributes.contains<SVGAttributeHashTranslator>(attrName);
}

void SVGMarkerElement::parseAttribute(const QualifiedName& name, const AtomicString& value)
{
    SVGParsingError parseError = NoError;

    if (!isSupportedAttribute(name))
        SVGElement::parseAttribute(name, value);
    else if (name == SVGNames::markerUnitsAttr)
        m_markerUnits->setBaseValueAsString(value, parseError);
    else if (name == SVGNames::refXAttr)
        m_refX->setBaseValueAsString(value, AllowNegativeLengths, parseError);
    else if (name == SVGNames::refYAttr)
        m_refY->setBaseValueAsString(value, AllowNegativeLengths, parseError);
    else if (name == SVGNames::markerWidthAttr)
        m_markerWidth->setBaseValueAsString(value, ForbidNegativeLengths, parseError);
    else if (name == SVGNames::markerHeightAttr)
        m_markerHeight->setBaseValueAsString(value, ForbidNegativeLengths, parseError);
    else if (name == SVGNames::orientAttr)
        m_orientAngle->setBaseValueAsString(value, parseError);
    else if (SVGFitToViewBox::parseAttribute(name, value, document(), parseError)) {
    } else
        ASSERT_NOT_REACHED();

    reportAttributeParsingError(parseError, name, value);
}

void SVGMarkerElement::svgAttributeChanged(const QualifiedName& attrName)
{
    if (!isSupportedAttribute(attrName)) {
        SVGElement::svgAttributeChanged(attrName);
        return;
    }

    SVGElementInstance::InvalidationGuard invalidationGuard(this);

    if (attrName == SVGNames::refXAttr
        || attrName == SVGNames::refYAttr
        || attrName == SVGNames::markerWidthAttr
        || attrName == SVGNames::markerHeightAttr)
        updateRelativeLengthsInformation();

    RenderSVGResourceContainer* renderer = toRenderSVGResourceContainer(this->renderer());
    if (renderer)
        renderer->invalidateCacheAndMarkForLayout();
}

void SVGMarkerElement::childrenChanged(bool changedByParser, Node* beforeChange, Node* afterChange, int childCountDelta)
{
    SVGElement::childrenChanged(changedByParser, beforeChange, afterChange, childCountDelta);

    if (changedByParser)
        return;

    if (RenderObject* object = renderer())
        object->setNeedsLayout();
}

void SVGMarkerElement::setOrientToAuto()
{
    m_orientAngle->baseValue()->orientType()->setEnumValue(SVGMarkerOrientAuto);
    invalidateSVGAttributes();
    svgAttributeChanged(SVGNames::orientAttr);
}

void SVGMarkerElement::setOrientToAngle(PassRefPtr<SVGAngleTearOff> angle)
{
    ASSERT(angle);
    RefPtr<SVGAngle> target = angle->target();
    m_orientAngle->baseValue()->newValueSpecifiedUnits(target->unitType(), target->valueInSpecifiedUnits());
    invalidateSVGAttributes();
    svgAttributeChanged(SVGNames::orientAttr);
}

RenderObject* SVGMarkerElement::createRenderer(RenderStyle*)
{
    return new RenderSVGResourceMarker(this);
}

bool SVGMarkerElement::selfHasRelativeLengths() const
{
    return m_refX->currentValue()->isRelative()
        || m_refY->currentValue()->isRelative()
        || m_markerWidth->currentValue()->isRelative()
        || m_markerHeight->currentValue()->isRelative();
}

}
