/*
 * Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
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

#ifndef SVGFECompositeElement_h
#define SVGFECompositeElement_h

#include "core/svg/SVGAnimatedEnumeration.h"
#include "core/svg/SVGAnimatedNumber.h"
#include "core/svg/SVGFilterPrimitiveStandardAttributes.h"
#include "platform/graphics/filters/FEComposite.h"

namespace WebCore {

template<> const SVGEnumerationStringEntries& getStaticStringEntries<CompositeOperationType>();

class SVGFECompositeElement FINAL : public SVGFilterPrimitiveStandardAttributes {
public:
    static PassRefPtr<SVGFECompositeElement> create(Document&);

    SVGAnimatedNumber* k1() { return m_k1.get(); }
    SVGAnimatedNumber* k2() { return m_k2.get(); }
    SVGAnimatedNumber* k3() { return m_k3.get(); }
    SVGAnimatedNumber* k4() { return m_k4.get(); }
    SVGAnimatedString* in1() { return m_in1.get(); }
    SVGAnimatedString* in2() { return m_in2.get(); }
    SVGAnimatedEnumeration<CompositeOperationType>* svgOperator() { return m_svgOperator.get(); }

private:
    explicit SVGFECompositeElement(Document&);

    bool isSupportedAttribute(const QualifiedName&);
    virtual void parseAttribute(const QualifiedName&, const AtomicString&) OVERRIDE;
    virtual bool setFilterEffectAttribute(FilterEffect*, const QualifiedName&) OVERRIDE;
    virtual void svgAttributeChanged(const QualifiedName&) OVERRIDE;
    virtual PassRefPtr<FilterEffect> build(SVGFilterBuilder*, Filter*) OVERRIDE;

    RefPtr<SVGAnimatedNumber> m_k1;
    RefPtr<SVGAnimatedNumber> m_k2;
    RefPtr<SVGAnimatedNumber> m_k3;
    RefPtr<SVGAnimatedNumber> m_k4;
    RefPtr<SVGAnimatedString> m_in1;
    RefPtr<SVGAnimatedString> m_in2;
    RefPtr<SVGAnimatedEnumeration<CompositeOperationType> > m_svgOperator;
};

} // namespace WebCore

#endif
