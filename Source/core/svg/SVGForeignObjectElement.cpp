/*
 * Copyright (C) 2006 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Nikolas Zimmermann <zimmermann@kde.org>
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
#include "core/svg/SVGForeignObjectElement.h"

#include "XLinkNames.h"
#include "core/rendering/svg/RenderSVGForeignObject.h"
#include "core/rendering/svg/RenderSVGResource.h"
#include "core/svg/SVGElementInstance.h"
#include "core/svg/SVGLength.h"
#include "wtf/Assertions.h"

namespace WebCore {

inline SVGForeignObjectElement::SVGForeignObjectElement(Document& document)
    : SVGGraphicsElement(SVGNames::foreignObjectTag, document)
    , m_x(SVGAnimatedLength::create(this, SVGNames::xAttr, SVGLength::create(LengthModeWidth)))
    , m_y(SVGAnimatedLength::create(this, SVGNames::yAttr, SVGLength::create(LengthModeHeight)))
    , m_width(SVGAnimatedLength::create(this, SVGNames::widthAttr, SVGLength::create(LengthModeWidth)))
    , m_height(SVGAnimatedLength::create(this, SVGNames::heightAttr, SVGLength::create(LengthModeHeight)))
{
    ScriptWrappable::init(this);

    addToPropertyMap(m_x);
    addToPropertyMap(m_y);
    addToPropertyMap(m_width);
    addToPropertyMap(m_height);
}

PassRefPtr<SVGForeignObjectElement> SVGForeignObjectElement::create(Document& document)
{
    return adoptRef(new SVGForeignObjectElement(document));
}

bool SVGForeignObjectElement::isSupportedAttribute(const QualifiedName& attrName)
{
    DEFINE_STATIC_LOCAL(HashSet<QualifiedName>, supportedAttributes, ());
    if (supportedAttributes.isEmpty()) {
        supportedAttributes.add(SVGNames::xAttr);
        supportedAttributes.add(SVGNames::yAttr);
        supportedAttributes.add(SVGNames::widthAttr);
        supportedAttributes.add(SVGNames::heightAttr);
    }
    return supportedAttributes.contains<SVGAttributeHashTranslator>(attrName);
}

void SVGForeignObjectElement::parseAttribute(const QualifiedName& name, const AtomicString& value)
{
    SVGParsingError parseError = NoError;

    if (!isSupportedAttribute(name))
        SVGGraphicsElement::parseAttribute(name, value);
    else if (name == SVGNames::xAttr)
        m_x->setBaseValueAsString(value, AllowNegativeLengths, parseError);
    else if (name == SVGNames::yAttr)
        m_y->setBaseValueAsString(value, AllowNegativeLengths, parseError);
    else if (name == SVGNames::widthAttr)
        m_width->setBaseValueAsString(value, ForbidNegativeLengths, parseError);
    else if (name == SVGNames::heightAttr)
        m_height->setBaseValueAsString(value, ForbidNegativeLengths, parseError);
    else
        ASSERT_NOT_REACHED();

    reportAttributeParsingError(parseError, name, value);
}

void SVGForeignObjectElement::svgAttributeChanged(const QualifiedName& attrName)
{
    if (!isSupportedAttribute(attrName)) {
        SVGGraphicsElement::svgAttributeChanged(attrName);
        return;
    }

    SVGElementInstance::InvalidationGuard invalidationGuard(this);

    bool isLengthAttribute = attrName == SVGNames::xAttr
                          || attrName == SVGNames::yAttr
                          || attrName == SVGNames::widthAttr
                          || attrName == SVGNames::heightAttr;

    if (isLengthAttribute)
        updateRelativeLengthsInformation();

    if (RenderObject* renderer = this->renderer())
        RenderSVGResource::markForLayoutAndParentResourceInvalidation(renderer);
}

RenderObject* SVGForeignObjectElement::createRenderer(RenderStyle*)
{
    return new RenderSVGForeignObject(this);
}

bool SVGForeignObjectElement::rendererIsNeeded(const RenderStyle& style)
{
    // Suppress foreignObject renderers in SVG hidden containers.
    // (https://bugs.webkit.org/show_bug.cgi?id=87297)
    // Note that we currently do not support foreignObject instantiation via <use>, hence it is safe
    // to use parentElement() here. If that changes, this method should be updated to use
    // parentOrShadowHostElement() instead.
    Element* ancestor = parentElement();
    while (ancestor && ancestor->isSVGElement()) {
        if (ancestor->renderer() && ancestor->renderer()->isSVGHiddenContainer())
            return false;

        ancestor = ancestor->parentElement();
    }

    return SVGGraphicsElement::rendererIsNeeded(style);
}

bool SVGForeignObjectElement::selfHasRelativeLengths() const
{
    return m_x->currentValue()->isRelative()
        || m_y->currentValue()->isRelative()
        || m_width->currentValue()->isRelative()
        || m_height->currentValue()->isRelative();
}

}
