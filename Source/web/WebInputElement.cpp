/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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
#include "WebInputElement.h"

#include "HTMLNames.h"
#include "RuntimeEnabledFeatures.h"
#include "WebElementCollection.h"
#include "core/dom/shadow/ElementShadow.h"
#include "core/dom/shadow/ShadowRoot.h"
#include "core/html/HTMLDataListElement.h"
#include "core/html/HTMLInputElement.h"
#include "core/html/shadow/ShadowElementNames.h"
#include "core/html/shadow/TextControlInnerElements.h"
#include "public/platform/WebString.h"
#include "wtf/PassRefPtr.h"

using namespace WebCore;

namespace blink {

bool WebInputElement::isTextField() const
{
    return constUnwrap<HTMLInputElement>()->isTextField();
}

bool WebInputElement::isText() const
{
    return constUnwrap<HTMLInputElement>()->isText();
}

bool WebInputElement::isPasswordField() const
{
    return constUnwrap<HTMLInputElement>()->isPasswordField();
}

bool WebInputElement::isImageButton() const
{
    return constUnwrap<HTMLInputElement>()->isImageButton();
}

bool WebInputElement::isRadioButton() const
{
    return constUnwrap<HTMLInputElement>()->isRadioButton();
}

bool WebInputElement::isCheckbox() const
{
    return constUnwrap<HTMLInputElement>()->isCheckbox();
}

int WebInputElement::maxLength() const
{
    return constUnwrap<HTMLInputElement>()->maxLength();
}

bool WebInputElement::isActivatedSubmit() const
{
    return constUnwrap<HTMLInputElement>()->isActivatedSubmit();
}

void WebInputElement::setActivatedSubmit(bool activated)
{
    unwrap<HTMLInputElement>()->setActivatedSubmit(activated);
}

int WebInputElement::size() const
{
    return constUnwrap<HTMLInputElement>()->size();
}

void WebInputElement::setEditingValue(const WebString& value)
{
    unwrap<HTMLInputElement>()->setEditingValue(value);
}

bool WebInputElement::isValidValue(const WebString& value) const
{
    return constUnwrap<HTMLInputElement>()->isValidValue(value);
}

void WebInputElement::setChecked(bool nowChecked, bool sendChangeEvent)
{
    unwrap<HTMLInputElement>()->setChecked(nowChecked, sendChangeEvent ? DispatchChangeEvent : DispatchNoEvent);
}

bool WebInputElement::isChecked() const
{
    return constUnwrap<HTMLInputElement>()->checked();
}

bool WebInputElement::isMultiple() const
{
    return constUnwrap<HTMLInputElement>()->multiple();
}

WebElementCollection WebInputElement::dataListOptions() const
{
    if (HTMLDataListElement* dataList = toHTMLDataListElement(constUnwrap<HTMLInputElement>()->list()))
        return WebElementCollection(dataList->options());
    return WebElementCollection();
}

WebString WebInputElement::localizeValue(const WebString& proposedValue) const
{
    return constUnwrap<HTMLInputElement>()->localizeValue(proposedValue);
}

bool WebInputElement::isSpeechInputEnabled() const
{
#if ENABLE(INPUT_SPEECH)
    return constUnwrap<HTMLInputElement>()->isSpeechEnabled();
#else
    return false;
#endif
}

#if ENABLE(INPUT_SPEECH)
static inline InputFieldSpeechButtonElement* speechButtonElement(const WebInputElement* webInput)
{
    return toInputFieldSpeechButtonElement(webInput->constUnwrap<HTMLInputElement>()->userAgentShadowRoot()->getElementById(ShadowElementNames::speechButton()));
}
#endif

WebInputElement::SpeechInputState WebInputElement::getSpeechInputState() const
{
#if ENABLE(INPUT_SPEECH)
    if (InputFieldSpeechButtonElement* speechButton = speechButtonElement(this))
        return static_cast<WebInputElement::SpeechInputState>(speechButton->state());
#endif

    return Idle;
}

void WebInputElement::startSpeechInput()
{
#if ENABLE(INPUT_SPEECH)
    if (InputFieldSpeechButtonElement* speechButton = speechButtonElement(this))
        speechButton->startSpeechInput();
#endif
}

void WebInputElement::stopSpeechInput()
{
#if ENABLE(INPUT_SPEECH)
    if (InputFieldSpeechButtonElement* speechButton = speechButtonElement(this))
        speechButton->stopSpeechInput();
#endif
}

int WebInputElement::defaultMaxLength()
{
    return HTMLInputElement::maximumLength;
}

// FIXME: Remove this once password_generation_manager.h stops using it.
WebElement WebInputElement::decorationElementFor(void*)
{
    return passwordGeneratorButtonElement();
}

WebElement WebInputElement::passwordGeneratorButtonElement() const
{
    return WebElement(constUnwrap<HTMLInputElement>()->passwordGeneratorButtonElement());
}

void WebInputElement::setShouldRevealPassword(bool value)
{
    unwrap<HTMLInputElement>()->setShouldRevealPassword(value);
}

WebInputElement::WebInputElement(const PassRefPtr<HTMLInputElement>& elem)
    : WebFormControlElement(elem)
{
}

WebInputElement& WebInputElement::operator=(const PassRefPtr<HTMLInputElement>& elem)
{
    m_private = elem;
    return *this;
}

WebInputElement::operator PassRefPtr<HTMLInputElement>() const
{
    return toHTMLInputElement(m_private.get());
}

WebInputElement* toWebInputElement(WebElement* webElement)
{
    if (!webElement->unwrap<Element>()->hasTagName(HTMLNames::inputTag))
        return 0;

    return static_cast<WebInputElement*>(webElement);
}
} // namespace blink
