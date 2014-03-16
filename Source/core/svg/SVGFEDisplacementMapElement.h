/*
 * Copyright (C) 2006 Oliver Hunt <oliver@nerget.com>
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

#ifndef SVGFEDisplacementMapElement_h
#define SVGFEDisplacementMapElement_h

#include "core/svg/SVGAnimatedEnumeration.h"
#include "core/svg/SVGAnimatedNumber.h"
#include "core/svg/SVGFilterPrimitiveStandardAttributes.h"
#include "platform/graphics/filters/FEDisplacementMap.h"

namespace WebCore {

template<> const SVGEnumerationStringEntries& getStaticStringEntries<ChannelSelectorType>();

class SVGFEDisplacementMapElement FINAL : public SVGFilterPrimitiveStandardAttributes {
public:
    static PassRefPtr<SVGFEDisplacementMapElement> create(Document&);

    static ChannelSelectorType stringToChannel(const String&);

    SVGAnimatedNumber* scale() { return m_scale.get(); }
    SVGAnimatedString* in1() { return m_in1.get(); }
    SVGAnimatedString* in2() { return m_in2.get(); }
    SVGAnimatedEnumeration<ChannelSelectorType>* xChannelSelector() { return m_xChannelSelector.get(); }
    SVGAnimatedEnumeration<ChannelSelectorType>* yChannelSelector() { return m_yChannelSelector.get(); }

private:
    SVGFEDisplacementMapElement(Document&);

    bool isSupportedAttribute(const QualifiedName&);
    virtual void parseAttribute(const QualifiedName&, const AtomicString&) OVERRIDE;
    virtual bool setFilterEffectAttribute(FilterEffect*, const QualifiedName& attrName) OVERRIDE;
    virtual void svgAttributeChanged(const QualifiedName&) OVERRIDE;
    virtual PassRefPtr<FilterEffect> build(SVGFilterBuilder*, Filter*) OVERRIDE;

    RefPtr<SVGAnimatedNumber> m_scale;
    RefPtr<SVGAnimatedString> m_in1;
    RefPtr<SVGAnimatedString> m_in2;
    RefPtr<SVGAnimatedEnumeration<ChannelSelectorType> > m_xChannelSelector;
    RefPtr<SVGAnimatedEnumeration<ChannelSelectorType> > m_yChannelSelector;
};

} // namespace WebCore

#endif // SVGFEDisplacementMapElement_h
