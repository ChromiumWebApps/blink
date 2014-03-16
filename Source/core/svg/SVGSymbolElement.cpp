/*
 * Copyright (C) 2004, 2005 Nikolas Zimmermann <zimmermann@kde.org>
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

#include "config.h"

#include "core/svg/SVGSymbolElement.h"

#include "SVGNames.h"
#include "core/rendering/svg/RenderSVGHiddenContainer.h"
#include "core/svg/SVGElementInstance.h"

namespace WebCore {

inline SVGSymbolElement::SVGSymbolElement(Document& document)
    : SVGElement(SVGNames::symbolTag, document)
    , SVGFitToViewBox(this)
{
    ScriptWrappable::init(this);
}

PassRefPtr<SVGSymbolElement> SVGSymbolElement::create(Document& document)
{
    return adoptRef(new SVGSymbolElement(document));
}

bool SVGSymbolElement::isSupportedAttribute(const QualifiedName& attrName)
{
    DEFINE_STATIC_LOCAL(HashSet<QualifiedName>, supportedAttributes, ());
    if (supportedAttributes.isEmpty())
        SVGFitToViewBox::addSupportedAttributes(supportedAttributes);

    return supportedAttributes.contains<SVGAttributeHashTranslator>(attrName);
}

void SVGSymbolElement::parseAttribute(const QualifiedName& name, const AtomicString& value)
{
    if (!isSupportedAttribute(name)) {
        SVGElement::parseAttribute(name, value);
        return;
    }

    SVGParsingError parseError = NoError;
    if (SVGFitToViewBox::parseAttribute(name, value, document(), parseError)) {
    } else {
        ASSERT_NOT_REACHED();
    }

    reportAttributeParsingError(parseError, name, value);
}

void SVGSymbolElement::svgAttributeChanged(const QualifiedName& attrName)
{
    if (!isSupportedAttribute(attrName)) {
        SVGElement::svgAttributeChanged(attrName);
        return;
    }

    SVGElementInstance::InvalidationGuard invalidationGuard(this);

    // Every other property change is ignored.
    if (attrName == SVGNames::viewBoxAttr)
        updateRelativeLengthsInformation();
}

bool SVGSymbolElement::selfHasRelativeLengths() const
{
    return hasAttribute(SVGNames::viewBoxAttr);
}

RenderObject* SVGSymbolElement::createRenderer(RenderStyle*)
{
    return new RenderSVGHiddenContainer(this);
}

}
