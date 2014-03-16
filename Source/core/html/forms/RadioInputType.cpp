/*
 * Copyright (C) 2005, 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Google Inc. All rights reserved.
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
#include "core/html/forms/RadioInputType.h"

#include "HTMLNames.h"
#include "InputTypeNames.h"
#include "core/dom/ElementTraversal.h"
#include "core/events/KeyboardEvent.h"
#include "core/events/MouseEvent.h"
#include "core/html/HTMLInputElement.h"
#include "core/page/SpatialNavigation.h"
#include "platform/text/PlatformLocale.h"
#include "wtf/PassOwnPtr.h"

namespace WebCore {

using namespace HTMLNames;

PassRefPtr<InputType> RadioInputType::create(HTMLInputElement& element)
{
    return adoptRef(new RadioInputType(element));
}

const AtomicString& RadioInputType::formControlType() const
{
    return InputTypeNames::radio;
}

bool RadioInputType::valueMissing(const String&) const
{
    return element().isInRequiredRadioButtonGroup() && !element().checkedRadioButtonForGroup();
}

String RadioInputType::valueMissingText() const
{
    return locale().queryString(blink::WebLocalizedString::ValidationValueMissingForRadio);
}

void RadioInputType::handleClickEvent(MouseEvent* event)
{
    event->setDefaultHandled();
}

void RadioInputType::handleKeydownEvent(KeyboardEvent* event)
{
    BaseCheckableInputType::handleKeydownEvent(event);
    if (event->defaultHandled())
        return;
    const String& key = event->keyIdentifier();
    if (key != "Up" && key != "Down" && key != "Left" && key != "Right")
        return;

    // Left and up mean "previous radio button".
    // Right and down mean "next radio button".
    // Tested in WinIE, and even for RTL, left still means previous radio button
    // (and so moves to the right). Seems strange, but we'll match it. However,
    // when using Spatial Navigation, we need to be able to navigate without
    // changing the selection.
    Document& document = element().document();
    if (isSpatialNavigationEnabled(document.frame()))
        return;
    bool forward = (key == "Down" || key == "Right");

    // We can only stay within the form's children if the form hasn't been demoted to a leaf because
    // of malformed HTML.
    HTMLElement* htmlElement = &element();
    while ((htmlElement = (forward ? Traversal<HTMLElement>::next(*htmlElement) : Traversal<HTMLElement>::previous(*htmlElement)))) {
        // Once we encounter a form element, we know we're through.
        if (isHTMLFormElement(*htmlElement))
            break;
        // Look for more radio buttons.
        if (!isHTMLInputElement(*htmlElement))
            continue;
        HTMLInputElement* inputElement = toHTMLInputElement(htmlElement);
        if (inputElement->form() != element().form())
            break;
        if (inputElement->isRadioButton() && inputElement->name() == element().name() && inputElement->isFocusable()) {
            RefPtr<HTMLInputElement> protector(inputElement);
            document.setFocusedElement(inputElement);
            inputElement->dispatchSimulatedClick(event, SendNoEvents);
            event->setDefaultHandled();
            return;
        }
    }
}

void RadioInputType::handleKeyupEvent(KeyboardEvent* event)
{
    const String& key = event->keyIdentifier();
    if (key != "U+0020")
        return;
    // If an unselected radio is tabbed into (because the entire group has nothing
    // checked, or because of some explicit .focus() call), then allow space to check it.
    if (element().checked())
        return;
    dispatchSimulatedClickIfActive(event);
}

bool RadioInputType::isKeyboardFocusable() const
{
    if (!InputType::isKeyboardFocusable())
        return false;

    // When using Spatial Navigation, every radio button should be focusable.
    if (isSpatialNavigationEnabled(element().document().frame()))
        return true;

    // Never allow keyboard tabbing to leave you in the same radio group. Always
    // skip any other elements in the group.
    Element* currentFocusedElement = element().document().focusedElement();
    if (isHTMLInputElement(currentFocusedElement)) {
        HTMLInputElement& focusedInput = toHTMLInputElement(*currentFocusedElement);
        if (focusedInput.isRadioButton() && focusedInput.form() == element().form() && focusedInput.name() == element().name())
            return false;
    }

    // Allow keyboard focus if we're checked or if nothing in the group is checked.
    return element().checked() || !element().checkedRadioButtonForGroup();
}

bool RadioInputType::shouldSendChangeEventAfterCheckedChanged()
{
    // Don't send a change event for a radio button that's getting unchecked.
    // This was done to match the behavior of other browsers.
    return element().checked();
}

PassOwnPtr<ClickHandlingState> RadioInputType::willDispatchClick()
{
    // An event handler can use preventDefault or "return false" to reverse the selection we do here.
    // The ClickHandlingState object contains what we need to undo what we did here in didDispatchClick.

    // We want radio groups to end up in sane states, i.e., to have something checked.
    // Therefore if nothing is currently selected, we won't allow the upcoming action to be "undone", since
    // we want some object in the radio group to actually get selected.

    OwnPtr<ClickHandlingState> state = adoptPtr(new ClickHandlingState);

    state->checked = element().checked();
    state->checkedRadioButton = element().checkedRadioButtonForGroup();
    element().setChecked(true, DispatchChangeEvent);

    return state.release();
}

void RadioInputType::didDispatchClick(Event* event, const ClickHandlingState& state)
{
    if (event->defaultPrevented() || event->defaultHandled()) {
        // Restore the original selected radio button if possible.
        // Make sure it is still a radio button and only do the restoration if it still belongs to our group.
        HTMLInputElement* checkedRadioButton = state.checkedRadioButton.get();
        if (checkedRadioButton
            && checkedRadioButton->isRadioButton()
            && checkedRadioButton->form() == element().form()
            && checkedRadioButton->name() == element().name()) {
            checkedRadioButton->setChecked(true);
        }
    }

    // The work we did in willDispatchClick was default handling.
    event->setDefaultHandled();
}

bool RadioInputType::isRadioButton() const
{
    return true;
}

bool RadioInputType::supportsIndeterminateAppearance() const
{
    return false;
}

} // namespace WebCore
