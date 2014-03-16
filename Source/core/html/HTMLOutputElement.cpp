/*
 * Copyright (c) 2010 Google Inc. All rights reserved.
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
#include "core/html/HTMLOutputElement.h"

#include "HTMLNames.h"
#include "bindings/v8/ExceptionStatePlaceholder.h"

namespace WebCore {

inline HTMLOutputElement::HTMLOutputElement(Document& document, HTMLFormElement* form)
    : HTMLFormControlElement(HTMLNames::outputTag, document, form)
    , m_isDefaultValueMode(true)
    , m_defaultValue("")
    , m_tokens(DOMSettableTokenList::create())
{
    ScriptWrappable::init(this);
}

PassRefPtr<HTMLOutputElement> HTMLOutputElement::create(Document& document, HTMLFormElement* form)
{
    return adoptRef(new HTMLOutputElement(document, form));
}

const AtomicString& HTMLOutputElement::formControlType() const
{
    DEFINE_STATIC_LOCAL(const AtomicString, output, ("output", AtomicString::ConstructFromLiteral));
    return output;
}

bool HTMLOutputElement::supportsFocus() const
{
    return HTMLElement::supportsFocus();
}

void HTMLOutputElement::parseAttribute(const QualifiedName& name, const AtomicString& value)
{
    if (name == HTMLNames::forAttr)
        setFor(value);
    else
        HTMLFormControlElement::parseAttribute(name, value);
}

DOMSettableTokenList* HTMLOutputElement::htmlFor() const
{
    return m_tokens.get();
}

void HTMLOutputElement::setFor(const AtomicString& value)
{
    m_tokens->setValue(value);
}

void HTMLOutputElement::childrenChanged(bool createdByParser, Node* beforeChange, Node* afterChange, int childCountDelta)
{
    HTMLFormControlElement::childrenChanged(createdByParser, beforeChange, afterChange, childCountDelta);

    if (m_isDefaultValueMode)
        m_defaultValue = textContent();
}

void HTMLOutputElement::resetImpl()
{
    // The reset algorithm for output elements is to set the element's
    // value mode flag to "default" and then to set the element's textContent
    // attribute to the default value.
    if (m_defaultValue == value())
        return;
    setTextContent(m_defaultValue);
    m_isDefaultValueMode = true;
}

String HTMLOutputElement::value() const
{
    return textContent();
}

void HTMLOutputElement::setValue(const String& value)
{
    // The value mode flag set to "value" when the value attribute is set.
    m_isDefaultValueMode = false;
    if (value == this->value())
        return;
    setTextContent(value);
}

String HTMLOutputElement::defaultValue() const
{
    return m_defaultValue;
}

void HTMLOutputElement::setDefaultValue(const String& value)
{
    if (m_defaultValue == value)
        return;
    m_defaultValue = value;
    // The spec requires the value attribute set to the default value
    // when the element's value mode flag to "default".
    if (m_isDefaultValueMode)
        setTextContent(value);
}

} // namespace
