/*
 * Copyright (C) 2010 Apple Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "core/accessibility/AXMenuListOption.h"

#include "core/html/HTMLOptionElement.h"

namespace WebCore {

using namespace HTMLNames;

AXMenuListOption::AXMenuListOption()
{
}

void AXMenuListOption::setElement(HTMLElement* element)
{
    ASSERT_ARG(element, isHTMLOptionElement(element));
    m_element = element;
}

Element* AXMenuListOption::actionElement() const
{
    return m_element.get();
}

bool AXMenuListOption::isEnabled() const
{
    // isDisabledFormControl() returns true if the parent <select> element is disabled,
    // which we don't want.
    return !toHTMLOptionElement(m_element)->ownElementDisabled();
}

bool AXMenuListOption::isVisible() const
{
    if (!m_parent)
        return false;

    // In a single-option select with the popup collapsed, only the selected
    // item is considered visible.
    return !m_parent->isOffScreen() || isSelected();
}

bool AXMenuListOption::isOffScreen() const
{
    // Invisible list options are considered to be offscreen.
    return !isVisible();
}

bool AXMenuListOption::isSelected() const
{
    return toHTMLOptionElement(m_element)->selected();
}

void AXMenuListOption::setSelected(bool b)
{
    if (!canSetSelectedAttribute())
        return;

    toHTMLOptionElement(m_element)->setSelected(b);
}

bool AXMenuListOption::canSetSelectedAttribute() const
{
    return isEnabled();
}

bool AXMenuListOption::computeAccessibilityIsIgnored() const
{
    return accessibilityIsIgnoredByDefault();
}

LayoutRect AXMenuListOption::elementRect() const
{
    AXObject* parent = parentObject();
    ASSERT(parent->isMenuListPopup());

    AXObject* grandparent = parent->parentObject();
    ASSERT(grandparent->isMenuList());

    return grandparent->elementRect();
}

String AXMenuListOption::stringValue() const
{
    return toHTMLOptionElement(m_element)->text();
}

} // namespace WebCore
