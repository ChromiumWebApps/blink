/*
 * Copyright (C) 1997 Martin Jones (mjones@kde.org)
 *           (C) 1997 Torben Weis (weis@kde.org)
 *           (C) 1998 Waldo Bastian (bastian@kde.org)
 *           (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2010 Apple Inc. All rights reserved.
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
#include "core/html/HTMLTableCellElement.h"

#include "CSSPropertyNames.h"
#include "CSSValueKeywords.h"
#include "HTMLNames.h"
#include "core/dom/Attribute.h"
#include "core/html/HTMLTableElement.h"
#include "core/rendering/RenderTableCell.h"

using std::max;
using std::min;

namespace WebCore {

// Clamp rowspan at 8k to match Firefox.
static const int maxRowspan = 8190;

using namespace HTMLNames;

inline HTMLTableCellElement::HTMLTableCellElement(const QualifiedName& tagName, Document& document)
    : HTMLTablePartElement(tagName, document)
{
    ScriptWrappable::init(this);
}

PassRefPtr<HTMLTableCellElement> HTMLTableCellElement::create(const QualifiedName& tagName, Document& document)
{
    return adoptRef(new HTMLTableCellElement(tagName, document));
}

int HTMLTableCellElement::colSpan() const
{
    const AtomicString& colSpanValue = fastGetAttribute(colspanAttr);
    return max(1, colSpanValue.toInt());
}

int HTMLTableCellElement::rowSpan() const
{
    const AtomicString& rowSpanValue = fastGetAttribute(rowspanAttr);
    return max(1, min(rowSpanValue.toInt(), maxRowspan));
}

int HTMLTableCellElement::cellIndex() const
{
    if (!isHTMLTableRowElement(parentElement()))
        return -1;

    int index = 0;
    for (const HTMLTableCellElement* element = Traversal<HTMLTableCellElement>::previousSibling(*this); element; element = Traversal<HTMLTableCellElement>::previousSibling(*element))
        ++index;

    return index;
}

bool HTMLTableCellElement::isPresentationAttribute(const QualifiedName& name) const
{
    if (name == nowrapAttr || name == widthAttr || name == heightAttr)
        return true;
    return HTMLTablePartElement::isPresentationAttribute(name);
}

void HTMLTableCellElement::collectStyleForPresentationAttribute(const QualifiedName& name, const AtomicString& value, MutableStylePropertySet* style)
{
    if (name == nowrapAttr)
        addPropertyToPresentationAttributeStyle(style, CSSPropertyWhiteSpace, CSSValueWebkitNowrap);
    else if (name == widthAttr) {
        if (!value.isEmpty()) {
            int widthInt = value.toInt();
            if (widthInt > 0) // width="0" is ignored for compatibility with WinIE.
                addHTMLLengthToStyle(style, CSSPropertyWidth, value);
        }
    } else if (name == heightAttr) {
        if (!value.isEmpty()) {
            int heightInt = value.toInt();
            if (heightInt > 0) // height="0" is ignored for compatibility with WinIE.
                addHTMLLengthToStyle(style, CSSPropertyHeight, value);
        }
    } else
        HTMLTablePartElement::collectStyleForPresentationAttribute(name, value, style);
}

void HTMLTableCellElement::parseAttribute(const QualifiedName& name, const AtomicString& value)
{
    if (name == rowspanAttr) {
        if (renderer() && renderer()->isTableCell())
            toRenderTableCell(renderer())->colSpanOrRowSpanChanged();
    } else if (name == colspanAttr) {
        if (renderer() && renderer()->isTableCell())
            toRenderTableCell(renderer())->colSpanOrRowSpanChanged();
    } else
        HTMLTablePartElement::parseAttribute(name, value);
}

const StylePropertySet* HTMLTableCellElement::additionalPresentationAttributeStyle()
{
    if (HTMLTableElement* table = findParentTable())
        return table->additionalCellStyle();
    return 0;
}

bool HTMLTableCellElement::isURLAttribute(const Attribute& attribute) const
{
    return attribute.name() == backgroundAttr || HTMLTablePartElement::isURLAttribute(attribute);
}

const AtomicString& HTMLTableCellElement::abbr() const
{
    return fastGetAttribute(abbrAttr);
}

const AtomicString& HTMLTableCellElement::axis() const
{
    return fastGetAttribute(axisAttr);
}

void HTMLTableCellElement::setColSpan(int n)
{
    setIntegralAttribute(colspanAttr, n);
}

const AtomicString& HTMLTableCellElement::headers() const
{
    return fastGetAttribute(headersAttr);
}

void HTMLTableCellElement::setRowSpan(int n)
{
    setIntegralAttribute(rowspanAttr, n);
}

const AtomicString& HTMLTableCellElement::scope() const
{
    return fastGetAttribute(scopeAttr);
}

HTMLTableCellElement* HTMLTableCellElement::cellAbove() const
{
    RenderObject* cellRenderer = renderer();
    if (!cellRenderer)
        return 0;
    if (!cellRenderer->isTableCell())
        return 0;

    RenderTableCell* tableCellRenderer = toRenderTableCell(cellRenderer);
    RenderTableCell* cellAboveRenderer = tableCellRenderer->table()->cellAbove(tableCellRenderer);
    if (!cellAboveRenderer)
        return 0;

    return toHTMLTableCellElement(cellAboveRenderer->node());
}

} // namespace WebCore
