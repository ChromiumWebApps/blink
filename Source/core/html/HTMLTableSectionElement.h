/*
 * Copyright (C) 1997 Martin Jones (mjones@kde.org)
 *           (C) 1997 Torben Weis (weis@kde.org)
 *           (C) 1998 Waldo Bastian (bastian@kde.org)
 *           (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007 Apple Inc. All rights reserved.
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
 *
 */

#ifndef HTMLTableSectionElement_h
#define HTMLTableSectionElement_h

#include "core/html/HTMLTablePartElement.h"

namespace WebCore {

class ExceptionState;

class HTMLTableSectionElement FINAL : public HTMLTablePartElement {
public:
    static PassRefPtr<HTMLTableSectionElement> create(const QualifiedName&, Document&);

    PassRefPtr<HTMLElement> insertRow(int index, ExceptionState&);
    void deleteRow(int index, ExceptionState&);

    int numRows() const;

    PassRefPtr<HTMLCollection> rows();

private:
    HTMLTableSectionElement(const QualifiedName& tagName, Document&);

    virtual const StylePropertySet* additionalPresentationAttributeStyle() OVERRIDE;
};

inline bool isHTMLTableSectionElement(const Node& node)
{
    return node.hasTagName(HTMLNames::tbodyTag) || node.hasTagName(HTMLNames::tfootTag) || node.hasTagName(HTMLNames::theadTag);
}

DEFINE_ELEMENT_TYPE_CASTS_WITH_FUNCTION(HTMLTableSectionElement);

} //namespace

#endif
