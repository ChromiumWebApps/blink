/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2010 Apple Inc. All rights reserved.
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
#include "core/html/HTMLOptGroupElement.h"

#include "HTMLNames.h"
#include "core/dom/Document.h"
#include "core/dom/NodeRenderStyle.h"
#include "core/html/HTMLSelectElement.h"
#include "wtf/StdLibExtras.h"

namespace WebCore {

using namespace HTMLNames;

inline HTMLOptGroupElement::HTMLOptGroupElement(Document& document)
    : HTMLElement(optgroupTag, document)
{
    setHasCustomStyleCallbacks();
    ScriptWrappable::init(this);
}

PassRefPtr<HTMLOptGroupElement> HTMLOptGroupElement::create(Document& document)
{
    return adoptRef(new HTMLOptGroupElement(document));
}

bool HTMLOptGroupElement::isDisabledFormControl() const
{
    return fastHasAttribute(disabledAttr);
}

bool HTMLOptGroupElement::rendererIsFocusable() const
{
    // Optgroup elements do not have a renderer so we check the renderStyle instead.
    return renderStyle() && renderStyle()->display() != NONE;
}

void HTMLOptGroupElement::childrenChanged(bool changedByParser, Node* beforeChange, Node* afterChange, int childCountDelta)
{
    recalcSelectOptions();
    HTMLElement::childrenChanged(changedByParser, beforeChange, afterChange, childCountDelta);
}

void HTMLOptGroupElement::parseAttribute(const QualifiedName& name, const AtomicString& value)
{
    HTMLElement::parseAttribute(name, value);
    recalcSelectOptions();

    if (name == disabledAttr)
        didAffectSelector(AffectedSelectorDisabled | AffectedSelectorEnabled);
}

void HTMLOptGroupElement::recalcSelectOptions()
{
    ContainerNode* select = parentNode();
    while (select && !isHTMLSelectElement(*select))
        select = select->parentNode();
    if (select)
        toHTMLSelectElement(select)->setRecalcListItems();
}

void HTMLOptGroupElement::attach(const AttachContext& context)
{
    if (context.resolvedStyle) {
        ASSERT(!m_style || m_style == context.resolvedStyle);
        m_style = context.resolvedStyle;
    }
    HTMLElement::attach(context);
}

void HTMLOptGroupElement::detach(const AttachContext& context)
{
    m_style.clear();
    HTMLElement::detach(context);
}

void HTMLOptGroupElement::updateNonRenderStyle()
{
    m_style = originalStyleForRenderer();
}

RenderStyle* HTMLOptGroupElement::nonRendererStyle() const
{
    return m_style.get();
}

PassRefPtr<RenderStyle> HTMLOptGroupElement::customStyleForRenderer()
{
    updateNonRenderStyle();
    return m_style;
}

String HTMLOptGroupElement::groupLabelText() const
{
    String itemText = getAttribute(labelAttr);

    // In WinIE, leading and trailing whitespace is ignored in options and optgroups. We match this behavior.
    itemText = itemText.stripWhiteSpace();
    // We want to collapse our whitespace too.  This will match other browsers.
    itemText = itemText.simplifyWhiteSpace();

    return itemText;
}

HTMLSelectElement* HTMLOptGroupElement::ownerSelectElement() const
{
    ContainerNode* select = parentNode();
    while (select && !isHTMLSelectElement(*select))
        select = select->parentNode();

    if (!select)
       return 0;

    return toHTMLSelectElement(select);
}

void HTMLOptGroupElement::accessKeyAction(bool)
{
    HTMLSelectElement* select = ownerSelectElement();
    // send to the parent to bring focus to the list box
    if (select && !select->focused())
        select->accessKeyAction(false);
}

} // namespace
