/*
 * Copyright (C) 1997 Martin Jones (mjones@kde.org)
 *           (C) 1997 Torben Weis (weis@kde.org)
 *           (C) 1998 Waldo Bastian (bastian@kde.org)
 *           (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2008, 2010, 2011 Apple Inc. All rights reserved.
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
#include "core/html/HTMLTableElement.h"

#include "CSSPropertyNames.h"
#include "CSSValueKeywords.h"
#include "HTMLNames.h"
#include "bindings/v8/ExceptionState.h"
#include "bindings/v8/ExceptionStatePlaceholder.h"
#include "core/css/CSSImageValue.h"
#include "core/css/CSSValuePool.h"
#include "core/css/StylePropertySet.h"
#include "core/dom/Attribute.h"
#include "core/dom/ElementTraversal.h"
#include "core/dom/ExceptionCode.h"
#include "core/html/HTMLTableCaptionElement.h"
#include "core/html/HTMLTableCellElement.h"
#include "core/html/HTMLTableRowElement.h"
#include "core/html/HTMLTableRowsCollection.h"
#include "core/html/HTMLTableSectionElement.h"
#include "core/html/parser/HTMLParserIdioms.h"
#include "core/rendering/RenderTable.h"
#include "wtf/StdLibExtras.h"

namespace WebCore {

using namespace HTMLNames;

HTMLTableElement::HTMLTableElement(Document& document)
    : HTMLElement(tableTag, document)
    , m_borderAttr(false)
    , m_borderColorAttr(false)
    , m_frameAttr(false)
    , m_rulesAttr(UnsetRules)
    , m_padding(1)
{
    ScriptWrappable::init(this);
}

PassRefPtr<HTMLTableElement> HTMLTableElement::create(Document& document)
{
    return adoptRef(new HTMLTableElement(document));
}

HTMLTableCaptionElement* HTMLTableElement::caption() const
{
    return Traversal<HTMLTableCaptionElement>::firstChild(*this);
}

void HTMLTableElement::setCaption(PassRefPtr<HTMLTableCaptionElement> newCaption, ExceptionState& exceptionState)
{
    deleteCaption();
    insertBefore(newCaption, firstChild(), exceptionState);
}

HTMLTableSectionElement* HTMLTableElement::tHead() const
{
    for (Element* child = ElementTraversal::firstWithin(*this); child; child = ElementTraversal::nextSibling(*child)) {
        if (child->hasTagName(theadTag))
            return toHTMLTableSectionElement(child);
    }
    return 0;
}

void HTMLTableElement::setTHead(PassRefPtr<HTMLTableSectionElement> newHead, ExceptionState& exceptionState)
{
    deleteTHead();

    Element* child;
    for (child = ElementTraversal::firstWithin(*this); child; child = ElementTraversal::nextSibling(*child)) {
        if (!child->hasTagName(captionTag) && !child->hasTagName(colgroupTag))
            break;
    }

    insertBefore(newHead, child, exceptionState);
}

HTMLTableSectionElement* HTMLTableElement::tFoot() const
{
    for (Element* child = ElementTraversal::firstWithin(*this); child; child = ElementTraversal::nextSibling(*child)) {
        if (child->hasTagName(tfootTag))
            return toHTMLTableSectionElement(child);
    }
    return 0;
}

void HTMLTableElement::setTFoot(PassRefPtr<HTMLTableSectionElement> newFoot, ExceptionState& exceptionState)
{
    deleteTFoot();

    Element* child;
    for (child = ElementTraversal::firstWithin(*this); child; child = ElementTraversal::nextSibling(*child)) {
        if (!child->hasTagName(captionTag) && !child->hasTagName(colgroupTag) && !child->hasTagName(theadTag))
            break;
    }

    insertBefore(newFoot, child, exceptionState);
}

PassRefPtr<HTMLElement> HTMLTableElement::createTHead()
{
    if (HTMLTableSectionElement* existingHead = tHead())
        return existingHead;
    RefPtr<HTMLTableSectionElement> head = HTMLTableSectionElement::create(theadTag, document());
    setTHead(head, IGNORE_EXCEPTION);
    return head.release();
}

void HTMLTableElement::deleteTHead()
{
    removeChild(tHead(), IGNORE_EXCEPTION);
}

PassRefPtr<HTMLElement> HTMLTableElement::createTFoot()
{
    if (HTMLTableSectionElement* existingFoot = tFoot())
        return existingFoot;
    RefPtr<HTMLTableSectionElement> foot = HTMLTableSectionElement::create(tfootTag, document());
    setTFoot(foot, IGNORE_EXCEPTION);
    return foot.release();
}

void HTMLTableElement::deleteTFoot()
{
    removeChild(tFoot(), IGNORE_EXCEPTION);
}

PassRefPtr<HTMLElement> HTMLTableElement::createTBody()
{
    RefPtr<HTMLTableSectionElement> body = HTMLTableSectionElement::create(tbodyTag, document());
    Node* referenceElement = lastBody() ? lastBody()->nextSibling() : 0;

    insertBefore(body, referenceElement);
    return body.release();
}

PassRefPtr<HTMLElement> HTMLTableElement::createCaption()
{
    if (HTMLTableCaptionElement* existingCaption = caption())
        return existingCaption;
    RefPtr<HTMLTableCaptionElement> caption = HTMLTableCaptionElement::create(document());
    setCaption(caption, IGNORE_EXCEPTION);
    return caption.release();
}

void HTMLTableElement::deleteCaption()
{
    removeChild(caption(), IGNORE_EXCEPTION);
}

HTMLTableSectionElement* HTMLTableElement::lastBody() const
{
    for (Node* child = lastChild(); child; child = child->previousSibling()) {
        if (child->hasTagName(tbodyTag))
            return toHTMLTableSectionElement(child);
    }
    return 0;
}

PassRefPtr<HTMLElement> HTMLTableElement::insertRow(int index, ExceptionState& exceptionState)
{
    if (index < -1) {
        exceptionState.throwDOMException(IndexSizeError, "The index provided (" + String::number(index) + ") is less than -1.");
        return nullptr;
    }

    RefPtr<Node> protectFromMutationEvents(this);

    RefPtr<HTMLTableRowElement> lastRow = nullptr;
    RefPtr<HTMLTableRowElement> row = nullptr;
    if (index == -1)
        lastRow = HTMLTableRowsCollection::lastRow(*this);
    else {
        for (int i = 0; i <= index; ++i) {
            row = HTMLTableRowsCollection::rowAfter(*this, lastRow.get());
            if (!row) {
                if (i != index) {
                    exceptionState.throwDOMException(IndexSizeError, "The index provided (" + String::number(index) + ") is greater than the number of rows in the table (" + String::number(i) + ").");
                    return nullptr;
                }
                break;
            }
            lastRow = row;
        }
    }

    RefPtr<ContainerNode> parent;
    if (lastRow)
        parent = row ? row->parentNode() : lastRow->parentNode();
    else {
        parent = lastBody();
        if (!parent) {
            RefPtr<HTMLTableSectionElement> newBody = HTMLTableSectionElement::create(tbodyTag, document());
            RefPtr<HTMLTableRowElement> newRow = HTMLTableRowElement::create(document());
            newBody->appendChild(newRow, exceptionState);
            appendChild(newBody.release(), exceptionState);
            return newRow.release();
        }
    }

    RefPtr<HTMLTableRowElement> newRow = HTMLTableRowElement::create(document());
    parent->insertBefore(newRow, row.get(), exceptionState);
    return newRow.release();
}

void HTMLTableElement::deleteRow(int index, ExceptionState& exceptionState)
{
    if (index < -1) {
        exceptionState.throwDOMException(IndexSizeError, "The index provided (" + String::number(index) + ") is less than -1.");
        return;
    }

    HTMLTableRowElement* row = 0;
    int i = 0;
    if (index == -1)
        row = HTMLTableRowsCollection::lastRow(*this);
    else {
        for (i = 0; i <= index; ++i) {
            row = HTMLTableRowsCollection::rowAfter(*this, row);
            if (!row)
                break;
        }
    }
    if (!row) {
        exceptionState.throwDOMException(IndexSizeError, "The index provided (" + String::number(index) + ") is greater than the number of rows in the table (" + String::number(i) + ").");
        return;
    }
    row->remove(exceptionState);
}

void HTMLTableElement::setNeedsTableStyleRecalc() const
{
    Element* element = ElementTraversal::next(*this, this);
    while (element) {
        element->setNeedsStyleRecalc(LocalStyleChange);
        if (isHTMLTableCellElement(*element))
            element = ElementTraversal::nextSkippingChildren(*element, this);
        else
            element = ElementTraversal::next(*element, this);
    }
}

static bool getBordersFromFrameAttributeValue(const AtomicString& value, bool& borderTop, bool& borderRight, bool& borderBottom, bool& borderLeft)
{
    borderTop = false;
    borderRight = false;
    borderBottom = false;
    borderLeft = false;

    if (equalIgnoringCase(value, "above"))
        borderTop = true;
    else if (equalIgnoringCase(value, "below"))
        borderBottom = true;
    else if (equalIgnoringCase(value, "hsides"))
        borderTop = borderBottom = true;
    else if (equalIgnoringCase(value, "vsides"))
        borderLeft = borderRight = true;
    else if (equalIgnoringCase(value, "lhs"))
        borderLeft = true;
    else if (equalIgnoringCase(value, "rhs"))
        borderRight = true;
    else if (equalIgnoringCase(value, "box") || equalIgnoringCase(value, "border"))
        borderTop = borderBottom = borderLeft = borderRight = true;
    else if (!equalIgnoringCase(value, "void"))
        return false;
    return true;
}

void HTMLTableElement::collectStyleForPresentationAttribute(const QualifiedName& name, const AtomicString& value, MutableStylePropertySet* style)
{
    if (name == widthAttr)
        addHTMLLengthToStyle(style, CSSPropertyWidth, value);
    else if (name == heightAttr)
        addHTMLLengthToStyle(style, CSSPropertyHeight, value);
    else if (name == borderAttr)
        addPropertyToPresentationAttributeStyle(style, CSSPropertyBorderWidth, parseBorderWidthAttribute(value), CSSPrimitiveValue::CSS_PX);
    else if (name == bordercolorAttr) {
        if (!value.isEmpty())
            addHTMLColorToStyle(style, CSSPropertyBorderColor, value);
    } else if (name == bgcolorAttr)
        addHTMLColorToStyle(style, CSSPropertyBackgroundColor, value);
    else if (name == backgroundAttr) {
        String url = stripLeadingAndTrailingHTMLSpaces(value);
        if (!url.isEmpty())
            style->setProperty(CSSProperty(CSSPropertyBackgroundImage, CSSImageValue::create(url, document().completeURL(url))));
    } else if (name == valignAttr) {
        if (!value.isEmpty())
            addPropertyToPresentationAttributeStyle(style, CSSPropertyVerticalAlign, value);
    } else if (name == cellspacingAttr) {
        if (!value.isEmpty())
            addHTMLLengthToStyle(style, CSSPropertyBorderSpacing, value);
    } else if (name == vspaceAttr) {
        addHTMLLengthToStyle(style, CSSPropertyMarginTop, value);
        addHTMLLengthToStyle(style, CSSPropertyMarginBottom, value);
    } else if (name == hspaceAttr) {
        addHTMLLengthToStyle(style, CSSPropertyMarginLeft, value);
        addHTMLLengthToStyle(style, CSSPropertyMarginRight, value);
    } else if (name == alignAttr) {
        if (!value.isEmpty()) {
            if (equalIgnoringCase(value, "center")) {
                addPropertyToPresentationAttributeStyle(style, CSSPropertyWebkitMarginStart, CSSValueAuto);
                addPropertyToPresentationAttributeStyle(style, CSSPropertyWebkitMarginEnd, CSSValueAuto);
            } else
                addPropertyToPresentationAttributeStyle(style, CSSPropertyFloat, value);
        }
    } else if (name == rulesAttr) {
        // The presence of a valid rules attribute causes border collapsing to be enabled.
        if (m_rulesAttr != UnsetRules)
            addPropertyToPresentationAttributeStyle(style, CSSPropertyBorderCollapse, CSSValueCollapse);
    } else if (name == frameAttr) {
        bool borderTop;
        bool borderRight;
        bool borderBottom;
        bool borderLeft;
        if (getBordersFromFrameAttributeValue(value, borderTop, borderRight, borderBottom, borderLeft)) {
            addPropertyToPresentationAttributeStyle(style, CSSPropertyBorderWidth, CSSValueThin);
            addPropertyToPresentationAttributeStyle(style, CSSPropertyBorderTopStyle, borderTop ? CSSValueSolid : CSSValueHidden);
            addPropertyToPresentationAttributeStyle(style, CSSPropertyBorderBottomStyle, borderBottom ? CSSValueSolid : CSSValueHidden);
            addPropertyToPresentationAttributeStyle(style, CSSPropertyBorderLeftStyle, borderLeft ? CSSValueSolid : CSSValueHidden);
            addPropertyToPresentationAttributeStyle(style, CSSPropertyBorderRightStyle, borderRight ? CSSValueSolid : CSSValueHidden);
        }
    } else
        HTMLElement::collectStyleForPresentationAttribute(name, value, style);
}

bool HTMLTableElement::isPresentationAttribute(const QualifiedName& name) const
{
    if (name == widthAttr || name == heightAttr || name == bgcolorAttr || name == backgroundAttr || name == valignAttr || name == vspaceAttr || name == hspaceAttr || name == alignAttr || name == cellspacingAttr || name == borderAttr || name == bordercolorAttr || name == frameAttr || name == rulesAttr)
        return true;
    return HTMLElement::isPresentationAttribute(name);
}

void HTMLTableElement::parseAttribute(const QualifiedName& name, const AtomicString& value)
{
    CellBorders bordersBefore = cellBorders();
    unsigned short oldPadding = m_padding;

    if (name == borderAttr)  {
        // FIXME: This attribute is a mess.
        m_borderAttr = parseBorderWidthAttribute(value);
    } else if (name == bordercolorAttr) {
        m_borderColorAttr = !value.isEmpty();
    } else if (name == frameAttr) {
        // FIXME: This attribute is a mess.
        bool borderTop;
        bool borderRight;
        bool borderBottom;
        bool borderLeft;
        m_frameAttr = getBordersFromFrameAttributeValue(value, borderTop, borderRight, borderBottom, borderLeft);
    } else if (name == rulesAttr) {
        m_rulesAttr = UnsetRules;
        if (equalIgnoringCase(value, "none"))
            m_rulesAttr = NoneRules;
        else if (equalIgnoringCase(value, "groups"))
            m_rulesAttr = GroupsRules;
        else if (equalIgnoringCase(value, "rows"))
            m_rulesAttr = RowsRules;
        else if (equalIgnoringCase(value, "cols"))
            m_rulesAttr = ColsRules;
        else if (equalIgnoringCase(value, "all"))
            m_rulesAttr = AllRules;
    } else if (name == cellpaddingAttr) {
        if (!value.isEmpty())
            m_padding = max(0, value.toInt());
        else
            m_padding = 1;
    } else if (name == colsAttr) {
        // ###
    } else
        HTMLElement::parseAttribute(name, value);

    if (bordersBefore != cellBorders() || oldPadding != m_padding) {
        m_sharedCellStyle = nullptr;
        setNeedsTableStyleRecalc();
    }
}

static PassRefPtr<StylePropertySet> createBorderStyle(CSSValueID value)
{
    RefPtr<MutableStylePropertySet> style = MutableStylePropertySet::create();
    style->setProperty(CSSPropertyBorderTopStyle, value);
    style->setProperty(CSSPropertyBorderBottomStyle, value);
    style->setProperty(CSSPropertyBorderLeftStyle, value);
    style->setProperty(CSSPropertyBorderRightStyle, value);
    return style.release();
}

const StylePropertySet* HTMLTableElement::additionalPresentationAttributeStyle()
{
    if (m_frameAttr)
        return 0;

    if (!m_borderAttr && !m_borderColorAttr) {
        // Setting the border to 'hidden' allows it to win over any border
        // set on the table's cells during border-conflict resolution.
        if (m_rulesAttr != UnsetRules) {
            DEFINE_STATIC_REF(StylePropertySet, solidBorderStyle, (createBorderStyle(CSSValueHidden)));
            return solidBorderStyle;
        }
        return 0;
    }

    if (m_borderColorAttr) {
        DEFINE_STATIC_REF(StylePropertySet, solidBorderStyle, (createBorderStyle(CSSValueSolid)));
        return solidBorderStyle;
    }
    DEFINE_STATIC_REF(StylePropertySet, outsetBorderStyle, (createBorderStyle(CSSValueOutset)));
    return outsetBorderStyle;
}

HTMLTableElement::CellBorders HTMLTableElement::cellBorders() const
{
    switch (m_rulesAttr) {
        case NoneRules:
        case GroupsRules:
            return NoBorders;
        case AllRules:
            return SolidBorders;
        case ColsRules:
            return SolidBordersColsOnly;
        case RowsRules:
            return SolidBordersRowsOnly;
        case UnsetRules:
            if (!m_borderAttr)
                return NoBorders;
            if (m_borderColorAttr)
                return SolidBorders;
            return InsetBorders;
    }
    ASSERT_NOT_REACHED();
    return NoBorders;
}

PassRefPtr<StylePropertySet> HTMLTableElement::createSharedCellStyle()
{
    RefPtr<MutableStylePropertySet> style = MutableStylePropertySet::create();

    switch (cellBorders()) {
    case SolidBordersColsOnly:
        style->setProperty(CSSPropertyBorderLeftWidth, CSSValueThin);
        style->setProperty(CSSPropertyBorderRightWidth, CSSValueThin);
        style->setProperty(CSSPropertyBorderLeftStyle, CSSValueSolid);
        style->setProperty(CSSPropertyBorderRightStyle, CSSValueSolid);
        style->setProperty(CSSPropertyBorderColor, cssValuePool().createInheritedValue());
        break;
    case SolidBordersRowsOnly:
        style->setProperty(CSSPropertyBorderTopWidth, CSSValueThin);
        style->setProperty(CSSPropertyBorderBottomWidth, CSSValueThin);
        style->setProperty(CSSPropertyBorderTopStyle, CSSValueSolid);
        style->setProperty(CSSPropertyBorderBottomStyle, CSSValueSolid);
        style->setProperty(CSSPropertyBorderColor, cssValuePool().createInheritedValue());
        break;
    case SolidBorders:
        style->setProperty(CSSPropertyBorderWidth, cssValuePool().createValue(1, CSSPrimitiveValue::CSS_PX));
        style->setProperty(CSSPropertyBorderStyle, cssValuePool().createIdentifierValue(CSSValueSolid));
        style->setProperty(CSSPropertyBorderColor, cssValuePool().createInheritedValue());
        break;
    case InsetBorders:
        style->setProperty(CSSPropertyBorderWidth, cssValuePool().createValue(1, CSSPrimitiveValue::CSS_PX));
        style->setProperty(CSSPropertyBorderStyle, cssValuePool().createIdentifierValue(CSSValueInset));
        style->setProperty(CSSPropertyBorderColor, cssValuePool().createInheritedValue());
        break;
    case NoBorders:
        // If 'rules=none' then allow any borders set at cell level to take effect.
        break;
    }

    if (m_padding)
        style->setProperty(CSSPropertyPadding, cssValuePool().createValue(m_padding, CSSPrimitiveValue::CSS_PX));

    return style.release();
}

const StylePropertySet* HTMLTableElement::additionalCellStyle()
{
    if (!m_sharedCellStyle)
        m_sharedCellStyle = createSharedCellStyle();
    return m_sharedCellStyle.get();
}

static PassRefPtr<StylePropertySet> createGroupBorderStyle(int rows)
{
    RefPtr<MutableStylePropertySet> style = MutableStylePropertySet::create();
    if (rows) {
        style->setProperty(CSSPropertyBorderTopWidth, CSSValueThin);
        style->setProperty(CSSPropertyBorderBottomWidth, CSSValueThin);
        style->setProperty(CSSPropertyBorderTopStyle, CSSValueSolid);
        style->setProperty(CSSPropertyBorderBottomStyle, CSSValueSolid);
    } else {
        style->setProperty(CSSPropertyBorderLeftWidth, CSSValueThin);
        style->setProperty(CSSPropertyBorderRightWidth, CSSValueThin);
        style->setProperty(CSSPropertyBorderLeftStyle, CSSValueSolid);
        style->setProperty(CSSPropertyBorderRightStyle, CSSValueSolid);
    }
    return style.release();
}

const StylePropertySet* HTMLTableElement::additionalGroupStyle(bool rows)
{
    if (m_rulesAttr != GroupsRules)
        return 0;

    if (rows) {
        DEFINE_STATIC_REF(StylePropertySet, rowBorderStyle, (createGroupBorderStyle(true)));
        return rowBorderStyle;
    }
    DEFINE_STATIC_REF(StylePropertySet, columnBorderStyle, (createGroupBorderStyle(false)));
    return columnBorderStyle;
}

bool HTMLTableElement::isURLAttribute(const Attribute& attribute) const
{
    return attribute.name() == backgroundAttr || HTMLElement::isURLAttribute(attribute);
}

PassRefPtr<HTMLCollection> HTMLTableElement::rows()
{
    return ensureCachedHTMLCollection(TableRows);
}

PassRefPtr<HTMLCollection> HTMLTableElement::tBodies()
{
    return ensureCachedHTMLCollection(TableTBodies);
}

const AtomicString& HTMLTableElement::rules() const
{
    return getAttribute(rulesAttr);
}

const AtomicString& HTMLTableElement::summary() const
{
    return getAttribute(summaryAttr);
}

}
