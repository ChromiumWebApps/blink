/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2009, 2011 Apple Inc. All rights reserved.
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
#include "core/html/HTMLAreaElement.h"

#include "HTMLNames.h"
#include "core/html/HTMLImageElement.h"
#include "core/html/HTMLMapElement.h"
#include "core/rendering/HitTestResult.h"
#include "core/rendering/RenderImage.h"
#include "core/rendering/RenderView.h"
#include "platform/LengthFunctions.h"
#include "platform/graphics/Path.h"
#include "platform/transforms/AffineTransform.h"

using namespace std;

namespace WebCore {

using namespace HTMLNames;

inline HTMLAreaElement::HTMLAreaElement(Document& document)
    : HTMLAnchorElement(areaTag, document)
    , m_lastSize(-1, -1)
    , m_shape(Unknown)
{
    ScriptWrappable::init(this);
}

PassRefPtr<HTMLAreaElement> HTMLAreaElement::create(Document& document)
{
    return adoptRef(new HTMLAreaElement(document));
}

void HTMLAreaElement::parseAttribute(const QualifiedName& name, const AtomicString& value)
{
    if (name == shapeAttr) {
        if (equalIgnoringCase(value, "default"))
            m_shape = Default;
        else if (equalIgnoringCase(value, "circle"))
            m_shape = Circle;
        else if (equalIgnoringCase(value, "poly"))
            m_shape = Poly;
        else if (equalIgnoringCase(value, "rect"))
            m_shape = Rect;
        invalidateCachedRegion();
    } else if (name == coordsAttr) {
        m_coords = parseHTMLAreaElementCoords(value.string());
        invalidateCachedRegion();
    } else if (name == altAttr || name == accesskeyAttr) {
        // Do nothing.
    } else
        HTMLAnchorElement::parseAttribute(name, value);
}

void HTMLAreaElement::invalidateCachedRegion()
{
    m_lastSize = LayoutSize(-1, -1);
}

bool HTMLAreaElement::mapMouseEvent(LayoutPoint location, const LayoutSize& size, HitTestResult& result)
{
    if (m_lastSize != size) {
        m_region = adoptPtr(new Path(getRegion(size)));
        m_lastSize = size;
    }

    if (!m_region->contains(location))
        return false;

    result.setInnerNode(this);
    result.setURLElement(this);
    return true;
}

Path HTMLAreaElement::computePath(RenderObject* obj) const
{
    if (!obj)
        return Path();

    // FIXME: This doesn't work correctly with transforms.
    FloatPoint absPos = obj->localToAbsolute();

    // Default should default to the size of the containing object.
    LayoutSize size = m_lastSize;
    if (m_shape == Default)
        size = obj->absoluteOutlineBounds().size();

    Path p = getRegion(size);
    float zoomFactor = obj->style()->effectiveZoom();
    if (zoomFactor != 1.0f) {
        AffineTransform zoomTransform;
        zoomTransform.scale(zoomFactor);
        p.transform(zoomTransform);
    }

    p.translate(toFloatSize(absPos));
    return p;
}

LayoutRect HTMLAreaElement::computeRect(RenderObject* obj) const
{
    return enclosingLayoutRect(computePath(obj).boundingRect());
}

Path HTMLAreaElement::getRegion(const LayoutSize& size) const
{
    if (m_coords.isEmpty() && m_shape != Default)
        return Path();

    LayoutUnit width = size.width();
    LayoutUnit height = size.height();

    // If element omits the shape attribute, select shape based on number of coordinates.
    Shape shape = m_shape;
    if (shape == Unknown) {
        if (m_coords.size() == 3)
            shape = Circle;
        else if (m_coords.size() == 4)
            shape = Rect;
        else if (m_coords.size() >= 6)
            shape = Poly;
    }

    Path path;
    switch (shape) {
        case Poly:
            if (m_coords.size() >= 6) {
                int numPoints = m_coords.size() / 2;
                path.moveTo(FloatPoint(minimumValueForLength(m_coords[0], width).toFloat(), minimumValueForLength(m_coords[1], height).toFloat()));
                for (int i = 1; i < numPoints; ++i)
                    path.addLineTo(FloatPoint(minimumValueForLength(m_coords[i * 2], width).toFloat(), minimumValueForLength(m_coords[i * 2 + 1], height).toFloat()));
                path.closeSubpath();
            }
            break;
        case Circle:
            if (m_coords.size() >= 3) {
                Length radius = m_coords[2];
                float r = min(minimumValueForLength(radius, width).toFloat(), minimumValueForLength(radius, height).toFloat());
                path.addEllipse(FloatRect(minimumValueForLength(m_coords[0], width).toFloat() - r, minimumValueForLength(m_coords[1], height).toFloat() - r, 2 * r, 2 * r));
            }
            break;
        case Rect:
            if (m_coords.size() >= 4) {
                float x0 = minimumValueForLength(m_coords[0], width).toFloat();
                float y0 = minimumValueForLength(m_coords[1], height).toFloat();
                float x1 = minimumValueForLength(m_coords[2], width).toFloat();
                float y1 = minimumValueForLength(m_coords[3], height).toFloat();
                path.addRect(FloatRect(x0, y0, x1 - x0, y1 - y0));
            }
            break;
        case Default:
            path.addRect(FloatRect(0, 0, width.toFloat(), height.toFloat()));
            break;
        case Unknown:
            break;
    }

    return path;
}

HTMLImageElement* HTMLAreaElement::imageElement() const
{
    Element* mapElement = parentElement();
    while (mapElement && !isHTMLMapElement(*mapElement))
        mapElement = mapElement->parentElement();

    if (!mapElement)
        return 0;

    return toHTMLMapElement(*mapElement).imageElement();
}

bool HTMLAreaElement::isKeyboardFocusable() const
{
    return isFocusable();
}

bool HTMLAreaElement::isMouseFocusable() const
{
    return isFocusable();
}

bool HTMLAreaElement::rendererIsFocusable() const
{
    HTMLImageElement* image = imageElement();
    if (!image || !image->renderer() || image->renderer()->style()->visibility() != VISIBLE)
        return false;

    return supportsFocus() && Element::tabIndex() >= 0;
}

void HTMLAreaElement::setFocus(bool shouldBeFocused)
{
    if (focused() == shouldBeFocused)
        return;

    HTMLAnchorElement::setFocus(shouldBeFocused);

    HTMLImageElement* imageElement = this->imageElement();
    if (!imageElement)
        return;

    RenderObject* renderer = imageElement->renderer();
    if (!renderer || !renderer->isImage())
        return;

    toRenderImage(renderer)->areaElementFocusChanged(this);
}

void HTMLAreaElement::updateFocusAppearance(bool restorePreviousSelection)
{
    if (!isFocusable())
        return;

    HTMLImageElement* imageElement = this->imageElement();
    if (!imageElement)
        return;

    imageElement->updateFocusAppearance(restorePreviousSelection);
}

bool HTMLAreaElement::supportsFocus() const
{
    // If the AREA element was a link, it should support focus.
    // FIXME: This means that an AREA that is not a link cannot be made focusable through contenteditable or tabindex. Is it correct?
    return isLink();
}

}
