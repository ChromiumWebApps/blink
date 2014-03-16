/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2010 Apple Inc. All rights reserved.
 *           (C) 2006 Alexey Proskuryakov (ap@nypop.com)
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

#include "config.h"
#include "core/html/HTMLLegendElement.h"

#include "HTMLNames.h"
#include "core/dom/ElementTraversal.h"
#include "core/html/HTMLFieldSetElement.h"
#include "core/html/HTMLFormControlElement.h"

namespace WebCore {

using namespace HTMLNames;


inline HTMLLegendElement::HTMLLegendElement(Document& document)
    : HTMLElement(legendTag, document)
{
    ScriptWrappable::init(this);
}

PassRefPtr<HTMLLegendElement> HTMLLegendElement::create(Document& document)
{
    return adoptRef(new HTMLLegendElement(document));
}

HTMLFormControlElement* HTMLLegendElement::associatedControl()
{
    // Check if there's a fieldset belonging to this legend.
    Element* fieldset = parentElement();
    while (fieldset && !isHTMLFieldSetElement(*fieldset))
        fieldset = fieldset->parentElement();
    if (!fieldset)
        return 0;

    // Find first form element inside the fieldset that is not a legend element.
    // FIXME: Should we consider tabindex?
    return Traversal<HTMLFormControlElement>::next(*fieldset, fieldset);
}

void HTMLLegendElement::focus(bool, FocusType type)
{
    if (isFocusable())
        Element::focus(true, type);

    // To match other browsers' behavior, never restore previous selection.
    if (HTMLFormControlElement* control = associatedControl())
        control->focus(false, type);
}

void HTMLLegendElement::accessKeyAction(bool sendMouseEvents)
{
    if (HTMLFormControlElement* control = associatedControl())
        control->accessKeyAction(sendMouseEvents);
}

HTMLFormElement* HTMLLegendElement::form() const
{
    // According to the specification, If the legend has a fieldset element as
    // its parent, then the form attribute must return the same value as the
    // form attribute on that fieldset element. Otherwise, it must return null.
    ContainerNode* fieldset = parentNode();
    if (!isHTMLFieldSetElement(fieldset))
        return 0;

    return toHTMLFieldSetElement(fieldset)->formOwner();
}

} // namespace
