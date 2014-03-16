/*
 * Copyright (C) 2004, 2005, 2007, 2008 Nikolas Zimmermann <zimmermann@kde.org>
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

#ifndef SVGStopElement_h
#define SVGStopElement_h

#include "SVGNames.h"
#include "core/svg/SVGAnimatedNumber.h"
#include "core/svg/SVGElement.h"

namespace WebCore {

class SVGStopElement FINAL : public SVGElement {
public:
    static PassRefPtr<SVGStopElement> create(Document&);

    Color stopColorIncludingOpacity() const;

    SVGAnimatedNumber* offset() { return m_offset.get(); }

private:
    explicit SVGStopElement(Document&);

    bool isSupportedAttribute(const QualifiedName&);
    virtual void parseAttribute(const QualifiedName&, const AtomicString&) OVERRIDE;
    virtual void svgAttributeChanged(const QualifiedName&) OVERRIDE;

    virtual RenderObject* createRenderer(RenderStyle*) OVERRIDE;
    virtual bool rendererIsNeeded(const RenderStyle&) OVERRIDE;

    RefPtr<SVGAnimatedNumber> m_offset;
};

} // namespace WebCore

#endif
