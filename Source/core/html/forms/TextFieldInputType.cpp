/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2011 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "core/html/forms/TextFieldInputType.h"

#include "HTMLNames.h"
#include "bindings/v8/ExceptionStatePlaceholder.h"
#include "core/dom/NodeRenderStyle.h"
#include "core/dom/shadow/ShadowRoot.h"
#include "core/editing/FrameSelection.h"
#include "core/editing/TextIterator.h"
#include "core/events/BeforeTextInsertedEvent.h"
#include "core/events/KeyboardEvent.h"
#include "core/events/TextEvent.h"
#include "core/frame/FrameHost.h"
#include "core/frame/LocalFrame.h"
#include "core/html/FormDataList.h"
#include "core/html/HTMLInputElement.h"
#include "core/html/shadow/ShadowElementNames.h"
#include "core/html/shadow/TextControlInnerElements.h"
#include "core/page/Chrome.h"
#include "core/page/ChromeClient.h"
#include "core/rendering/RenderDetailsMarker.h"
#include "core/rendering/RenderLayer.h"
#include "core/rendering/RenderTextControlSingleLine.h"
#include "core/rendering/RenderTheme.h"
#include "wtf/text/WTFString.h"

namespace WebCore {

using namespace HTMLNames;

class DataListIndicatorElement FINAL : public HTMLDivElement {
private:
    inline DataListIndicatorElement(Document& document) : HTMLDivElement(document) { }
    inline HTMLInputElement* hostInput() const { return toHTMLInputElement(shadowHost()); }

    virtual RenderObject* createRenderer(RenderStyle*) OVERRIDE
    {
        return new RenderDetailsMarker(this);
    }

    virtual void* preDispatchEventHandler(Event* event) OVERRIDE
    {
        // Chromium opens autofill popup in a mousedown event listener
        // associated to the document. We don't want to open it in this case
        // because we opens a datalist chooser later.
        // FIXME: We should dispatch mousedown events even in such case.
        if (event->type() == EventTypeNames::mousedown)
            event->stopPropagation();
        return 0;
    }

    virtual void defaultEventHandler(Event* event) OVERRIDE
    {
        ASSERT(document().isActive());
        if (event->type() != EventTypeNames::click)
            return;
        HTMLInputElement* host = hostInput();
        if (host && !host->isDisabledOrReadOnly()) {
            document().frameHost()->chrome().openTextDataListChooser(*host);
            event->setDefaultHandled();
        }
    }

    virtual bool willRespondToMouseClickEvents() OVERRIDE
    {
        return hostInput() && !hostInput()->isDisabledOrReadOnly() && document().isActive();
    }

public:
    static PassRefPtr<DataListIndicatorElement> create(Document& document)
    {
        RefPtr<DataListIndicatorElement> element = adoptRef(new DataListIndicatorElement(document));
        element->setShadowPseudoId(AtomicString("-webkit-calendar-picker-indicator", AtomicString::ConstructFromLiteral));
        element->setAttribute(idAttr, ShadowElementNames::pickerIndicator());
        return element.release();
    }

};

TextFieldInputType::TextFieldInputType(HTMLInputElement& element)
    : InputType(element)
{
}

TextFieldInputType::~TextFieldInputType()
{
    if (SpinButtonElement* spinButton = spinButtonElement())
        spinButton->removeSpinButtonOwner();
}

SpinButtonElement* TextFieldInputType::spinButtonElement() const
{
    return toSpinButtonElement(element().userAgentShadowRoot()->getElementById(ShadowElementNames::spinButton()));
}

bool TextFieldInputType::shouldShowFocusRingOnMouseFocus() const
{
    return true;
}

bool TextFieldInputType::isTextField() const
{
    return true;
}

bool TextFieldInputType::valueMissing(const String& value) const
{
    return element().isRequired() && value.isEmpty();
}

bool TextFieldInputType::canSetSuggestedValue()
{
    return true;
}

void TextFieldInputType::setValue(const String& sanitizedValue, bool valueChanged, TextFieldEventBehavior eventBehavior)
{
    // Grab this input element to keep reference even if JS event handler
    // changes input type.
    RefPtr<HTMLInputElement> input(element());

    // We don't ask InputType::setValue to dispatch events because
    // TextFieldInputType dispatches events different way from InputType.
    InputType::setValue(sanitizedValue, valueChanged, DispatchNoEvent);

    if (valueChanged)
        input->updateView();

    unsigned max = visibleValue().length();
    if (input->focused())
        input->setSelectionRange(max, max);
    else
        input->cacheSelectionInResponseToSetValue(max);

    if (!valueChanged)
        return;

    switch (eventBehavior) {
    case DispatchChangeEvent:
        // If the user is still editing this field, dispatch an input event rather than a change event.
        // The change event will be dispatched when editing finishes.
        if (input->focused())
            input->dispatchFormControlInputEvent();
        else
            input->dispatchFormControlChangeEvent();
        break;

    case DispatchInputAndChangeEvent: {
        input->dispatchFormControlInputEvent();
        input->dispatchFormControlChangeEvent();
        break;
    }

    case DispatchNoEvent:
        break;
    }

    if (!input->focused())
        input->setTextAsOfLastFormControlChangeEvent(sanitizedValue);
}

void TextFieldInputType::handleKeydownEvent(KeyboardEvent* event)
{
    if (!element().focused())
        return;
    if (Chrome* chrome = this->chrome()) {
        chrome->client().handleKeyboardEventOnTextField(element(), *event);
        return;
    }
    event->setDefaultHandled();
}

void TextFieldInputType::handleKeydownEventForSpinButton(KeyboardEvent* event)
{
    if (element().isDisabledOrReadOnly())
        return;
    const String& key = event->keyIdentifier();
    if (key == "Up")
        spinButtonStepUp();
    else if (key == "Down")
        spinButtonStepDown();
    else
        return;
    event->setDefaultHandled();
}

void TextFieldInputType::forwardEvent(Event* event)
{
    if (SpinButtonElement* spinButton = spinButtonElement()) {
        spinButton->forwardEvent(event);
        if (event->defaultHandled())
            return;
    }

    if (element().renderer() && (event->isMouseEvent() || event->isDragEvent() || event->hasInterface(EventNames::WheelEvent) || event->type() == EventTypeNames::blur || event->type() == EventTypeNames::focus)) {
        RenderTextControlSingleLine* renderTextControl = toRenderTextControlSingleLine(element().renderer());
        if (event->type() == EventTypeNames::blur) {
            if (RenderBox* innerTextRenderer = element().innerTextElement()->renderBox()) {
                // FIXME: This class has no need to know about RenderLayer!
                if (RenderLayer* innerLayer = innerTextRenderer->layer()) {
                    RenderLayerScrollableArea* innerScrollableArea = innerLayer->scrollableArea();
                    IntSize scrollOffset(!renderTextControl->style()->isLeftToRightDirection() ? innerScrollableArea->scrollWidth() : 0, 0);
                    innerScrollableArea->scrollToOffset(scrollOffset, ScrollOffsetClamped);
                }
            }

            renderTextControl->capsLockStateMayHaveChanged();
        } else if (event->type() == EventTypeNames::focus) {
            renderTextControl->capsLockStateMayHaveChanged();
        }

        element().forwardEvent(event);
    }
}

void TextFieldInputType::handleFocusEvent(Element* oldFocusedNode, FocusType focusType)
{
    InputType::handleFocusEvent(oldFocusedNode, focusType);
    element().beginEditing();
}

void TextFieldInputType::handleBlurEvent()
{
    InputType::handleBlurEvent();
    element().endEditing();
}

bool TextFieldInputType::shouldSubmitImplicitly(Event* event)
{
    return (event->type() == EventTypeNames::textInput && event->hasInterface(EventNames::TextEvent) && toTextEvent(event)->data() == "\n") || InputType::shouldSubmitImplicitly(event);
}

RenderObject* TextFieldInputType::createRenderer(RenderStyle*) const
{
    return new RenderTextControlSingleLine(&element());
}

bool TextFieldInputType::needsContainer() const
{
#if ENABLE(INPUT_SPEECH)
    return element().isSpeechEnabled();
#else
    return false;
#endif
}

bool TextFieldInputType::shouldHaveSpinButton() const
{
    return RenderTheme::theme().shouldHaveSpinButton(&element());
}

void TextFieldInputType::createShadowSubtree()
{
    ASSERT(element().shadow());
    ShadowRoot* shadowRoot = element().userAgentShadowRoot();
    ASSERT(!shadowRoot->hasChildren());

    Document& document = element().document();
    bool shouldHaveSpinButton = this->shouldHaveSpinButton();
    bool shouldHaveDataListIndicator = element().hasValidDataListOptions();
    bool createsContainer = shouldHaveSpinButton || shouldHaveDataListIndicator || needsContainer();

    RefPtr<TextControlInnerTextElement> innerEditor = TextControlInnerTextElement::create(document);
    if (!createsContainer) {
        shadowRoot->appendChild(innerEditor.release());
        return;
    }

    RefPtr<TextControlInnerContainer> container = TextControlInnerContainer::create(document);
    container->setShadowPseudoId(AtomicString("-webkit-textfield-decoration-container", AtomicString::ConstructFromLiteral));
    shadowRoot->appendChild(container);

    RefPtr<EditingViewPortElement> editingViewPort = EditingViewPortElement::create(document);
    editingViewPort->appendChild(innerEditor.release());
    container->appendChild(editingViewPort.release());

#if ENABLE(INPUT_SPEECH)
    if (element().isSpeechEnabled())
        container->appendChild(InputFieldSpeechButtonElement::create(document));
#endif

    if (shouldHaveDataListIndicator)
        container->appendChild(DataListIndicatorElement::create(document));
    // FIXME: Because of a special handling for a spin button in
    // RenderTextControlSingleLine, we need to put it to the last position. It's
    // inconsistent with multiple-fields date/time types.
    if (shouldHaveSpinButton)
        container->appendChild(SpinButtonElement::create(document, *this));

    // See listAttributeTargetChanged too.
}

Element* TextFieldInputType::containerElement() const
{
    return element().userAgentShadowRoot()->getElementById(ShadowElementNames::textFieldContainer());
}

void TextFieldInputType::destroyShadowSubtree()
{
    InputType::destroyShadowSubtree();
    if (SpinButtonElement* spinButton = spinButtonElement())
        spinButton->removeSpinButtonOwner();
}

void TextFieldInputType::listAttributeTargetChanged()
{
    Element* picker = element().userAgentShadowRoot()->getElementById(ShadowElementNames::pickerIndicator());
    bool didHavePickerIndicator = picker;
    bool willHavePickerIndicator = element().hasValidDataListOptions();
    if (didHavePickerIndicator == willHavePickerIndicator)
        return;
    if (willHavePickerIndicator) {
        Document& document = element().document();
        if (Element* container = containerElement()) {
            container->insertBefore(DataListIndicatorElement::create(document), spinButtonElement());
        } else {
            // FIXME: The following code is similar to createShadowSubtree(),
            // but they are different. We should simplify the code by making
            // containerElement mandatory.
            RefPtr<Element> rpContainer = TextControlInnerContainer::create(document);
            rpContainer->setShadowPseudoId(AtomicString("-webkit-textfield-decoration-container", AtomicString::ConstructFromLiteral));
            RefPtr<Element> innerEditor = element().innerTextElement();
            innerEditor->parentNode()->replaceChild(rpContainer.get(), innerEditor.get());
            RefPtr<Element> editingViewPort = EditingViewPortElement::create(document);
            editingViewPort->appendChild(innerEditor.release());
            rpContainer->appendChild(editingViewPort.release());
            rpContainer->appendChild(DataListIndicatorElement::create(document));
            if (element().document().focusedElement() == element())
                element().updateFocusAppearance(true /* restore selection */);
        }
    } else {
        picker->remove(ASSERT_NO_EXCEPTION);
    }
}

void TextFieldInputType::attributeChanged()
{
    // FIXME: Updating on any attribute update should be unnecessary. We should
    // figure out what attributes affect.
    updateView();
}

void TextFieldInputType::disabledAttributeChanged()
{
    if (SpinButtonElement* spinButton = spinButtonElement())
        spinButton->releaseCapture();
}

void TextFieldInputType::readonlyAttributeChanged()
{
    if (SpinButtonElement* spinButton = spinButtonElement())
        spinButton->releaseCapture();
}

bool TextFieldInputType::supportsReadOnly() const
{
    return true;
}

bool TextFieldInputType::shouldUseInputMethod() const
{
    return true;
}

static bool isASCIILineBreak(UChar c)
{
    return c == '\r' || c == '\n';
}

static String limitLength(const String& string, unsigned maxLength)
{
    unsigned newLength = std::min(maxLength, string.length());
    if (newLength == string.length())
        return string;
    if (newLength > 0 && U16_IS_LEAD(string[newLength - 1]))
        --newLength;
    return string.left(newLength);
}

String TextFieldInputType::sanitizeValue(const String& proposedValue) const
{
    return limitLength(proposedValue.removeCharacters(isASCIILineBreak), HTMLInputElement::maximumLength);
}

void TextFieldInputType::handleBeforeTextInsertedEvent(BeforeTextInsertedEvent* event)
{
    // Make sure that the text to be inserted will not violate the maxLength.

    // We use HTMLInputElement::innerTextValue() instead of
    // HTMLInputElement::value() because they can be mismatched by
    // sanitizeValue() in HTMLInputElement::subtreeHasChanged() in some cases.
    unsigned oldLength = element().innerTextValue().length();

    // selectionLength represents the selection length of this text field to be
    // removed by this insertion.
    // If the text field has no focus, we don't need to take account of the
    // selection length. The selection is the source of text drag-and-drop in
    // that case, and nothing in the text field will be removed.
    unsigned selectionLength = element().focused() ? plainText(element().document().frame()->selection().selection().toNormalizedRange().get()).length() : 0;
    ASSERT(oldLength >= selectionLength);

    // Selected characters will be removed by the next text event.
    unsigned baseLength = oldLength - selectionLength;
    unsigned maxLength = static_cast<unsigned>(isTextType() ? element().maxLength() : HTMLInputElement::maximumLength); // maxLength can never be negative.
    unsigned appendableLength = maxLength > baseLength ? maxLength - baseLength : 0;

    // Truncate the inserted text to avoid violating the maxLength and other constraints.
    String eventText = event->text();
    unsigned textLength = eventText.length();
    while (textLength > 0 && isASCIILineBreak(eventText[textLength - 1]))
        textLength--;
    eventText.truncate(textLength);
    eventText.replace("\r\n", " ");
    eventText.replace('\r', ' ');
    eventText.replace('\n', ' ');

    event->setText(limitLength(eventText, appendableLength));
}

bool TextFieldInputType::shouldRespectListAttribute()
{
    return true;
}

void TextFieldInputType::updatePlaceholderText()
{
    if (!supportsPlaceholder())
        return;
    HTMLElement* placeholder = element().placeholderElement();
    String placeholderText = element().strippedPlaceholder();
    if (placeholderText.isEmpty()) {
        if (placeholder)
            placeholder->remove(ASSERT_NO_EXCEPTION);
        return;
    }
    if (!placeholder) {
        RefPtr<HTMLElement> newElement = HTMLDivElement::create(element().document());
        placeholder = newElement.get();
        placeholder->setShadowPseudoId(AtomicString("-webkit-input-placeholder", AtomicString::ConstructFromLiteral));
        placeholder->setAttribute(idAttr, ShadowElementNames::placeholder());
        Element* container = containerElement();
        Node* previous = container ? container : element().innerTextElement();
        previous->parentNode()->insertBefore(placeholder, previous->nextSibling());
        ASSERT_WITH_SECURITY_IMPLICATION(placeholder->parentNode() == previous->parentNode());
    }
    placeholder->setTextContent(placeholderText);
}

bool TextFieldInputType::appendFormData(FormDataList& list, bool multipart) const
{
    InputType::appendFormData(list, multipart);
    const AtomicString& dirnameAttrValue = element().fastGetAttribute(dirnameAttr);
    if (!dirnameAttrValue.isNull())
        list.appendData(dirnameAttrValue, element().directionForFormData());
    return true;
}

String TextFieldInputType::convertFromVisibleValue(const String& visibleValue) const
{
    return visibleValue;
}

void TextFieldInputType::subtreeHasChanged()
{
    ASSERT(element().renderer());

    bool wasChanged = element().wasChangedSinceLastFormControlChangeEvent();
    element().setChangedSinceLastFormControlChangeEvent(true);

    // We don't need to call sanitizeUserInputValue() function here because
    // HTMLInputElement::handleBeforeTextInsertedEvent() has already called
    // sanitizeUserInputValue().
    // sanitizeValue() is needed because IME input doesn't dispatch BeforeTextInsertedEvent.
    element().setValueFromRenderer(sanitizeValue(convertFromVisibleValue(element().innerTextValue())));
    element().updatePlaceholderVisibility(false);
    // Recalc for :invalid change.
    element().setNeedsStyleRecalc(SubtreeStyleChange);

    didSetValueByUserEdit(wasChanged ? ValueChangeStateChanged : ValueChangeStateNone);
}

void TextFieldInputType::didSetValueByUserEdit(ValueChangeState state)
{
    if (!element().focused())
        return;
    if (Chrome* chrome = this->chrome())
        chrome->client().didChangeValueInTextField(element());
}

void TextFieldInputType::spinButtonStepDown()
{
    stepUpFromRenderer(-1);
}

void TextFieldInputType::spinButtonStepUp()
{
    stepUpFromRenderer(1);
}

void TextFieldInputType::updateView()
{
    if (!element().suggestedValue().isNull()) {
        element().setInnerTextValue(element().suggestedValue());
        element().updatePlaceholderVisibility(false);
    } else if (!element().formControlValueMatchesRenderer()) {
        // Update the renderer value if the formControlValueMatchesRenderer() flag is false.
        // It protects an unacceptable renderer value from being overwritten with the DOM value.
        element().setInnerTextValue(visibleValue());
        element().updatePlaceholderVisibility(false);
    }
}

void TextFieldInputType::focusAndSelectSpinButtonOwner()
{
    RefPtr<HTMLInputElement> input(element());
    input->focus();
    input->select();
}

bool TextFieldInputType::shouldSpinButtonRespondToMouseEvents()
{
    return !element().isDisabledOrReadOnly();
}

bool TextFieldInputType::shouldSpinButtonRespondToWheelEvents()
{
    return shouldSpinButtonRespondToMouseEvents() && element().focused();
}

} // namespace WebCore
