/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007 Apple Inc. All rights reserved.
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
#include "core/html/FormAssociatedElement.h"

#include "HTMLNames.h"
#include "core/dom/IdTargetObserver.h"
#include "core/html/HTMLFormControlElement.h"
#include "core/html/HTMLFormElement.h"
#include "core/html/HTMLObjectElement.h"
#include "core/html/ValidityState.h"

namespace WebCore {

using namespace HTMLNames;

class FormAttributeTargetObserver : IdTargetObserver {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static PassOwnPtr<FormAttributeTargetObserver> create(const AtomicString& id, FormAssociatedElement*);
    virtual void idTargetChanged() OVERRIDE;

private:
    FormAttributeTargetObserver(const AtomicString& id, FormAssociatedElement*);

    FormAssociatedElement* m_element;
};

FormAssociatedElement::FormAssociatedElement()
    : m_formWasSetByParser(false)
{
}

FormAssociatedElement::~FormAssociatedElement()
{
    // We can't call setForm here because it contains virtual calls.
}

ValidityState* FormAssociatedElement::validity()
{
    if (!m_validityState)
        m_validityState = ValidityState::create(this);

    return m_validityState.get();
}

void FormAssociatedElement::didMoveToNewDocument(Document& oldDocument)
{
    HTMLElement* element = toHTMLElement(this);
    if (element->fastHasAttribute(formAttr))
        m_formAttributeTargetObserver = nullptr;
}

void FormAssociatedElement::insertedInto(ContainerNode* insertionPoint)
{
    if (!m_formWasSetByParser || insertionPoint->highestAncestor() != m_form->highestAncestor())
        resetFormOwner();

    if (!insertionPoint->inDocument())
        return;

    HTMLElement* element = toHTMLElement(this);
    if (element->fastHasAttribute(formAttr))
        resetFormAttributeTargetObserver();
}

void FormAssociatedElement::removedFrom(ContainerNode* insertionPoint)
{
    HTMLElement* element = toHTMLElement(this);
    if (insertionPoint->inDocument() && element->fastHasAttribute(formAttr))
        m_formAttributeTargetObserver = nullptr;
    // If the form and element are both in the same tree, preserve the connection to the form.
    // Otherwise, null out our form and remove ourselves from the form's list of elements.
    if (m_form && element->highestAncestor() != m_form->highestAncestor())
        resetFormOwner();
}

HTMLFormElement* FormAssociatedElement::findAssociatedForm(const HTMLElement* element)
{
    const AtomicString& formId(element->fastGetAttribute(formAttr));
    // 3. If the element is reassociateable, has a form content attribute, and
    // is itself in a Document, then run these substeps:
    if (!formId.isNull() && element->inDocument()) {
        // 3.1. If the first element in the Document to have an ID that is
        // case-sensitively equal to the element's form content attribute's
        // value is a form element, then associate the form-associated element
        // with that form element.
        // 3.2. Abort the "reset the form owner" steps.
        Element* newFormCandidate = element->treeScope().getElementById(formId);
        return isHTMLFormElement(newFormCandidate) ? toHTMLFormElement(newFormCandidate) : 0;
    }
    // 4. Otherwise, if the form-associated element in question has an ancestor
    // form element, then associate the form-associated element with the nearest
    // such ancestor form element.
    return element->findFormAncestor();
}

void FormAssociatedElement::formRemovedFromTree(const Node& formRoot)
{
    ASSERT(m_form);
    if (toHTMLElement(this)->highestAncestor() == formRoot)
        return;
    resetFormOwner();
}

void FormAssociatedElement::associateByParser(HTMLFormElement* form)
{
    if (form && form->inDocument()) {
        m_formWasSetByParser = true;
        setForm(form);
        form->didAssociateByParser();
    }
}

void FormAssociatedElement::setForm(HTMLFormElement* newForm)
{
    if (m_form.get() == newForm)
        return;
    willChangeForm();
    if (m_form)
        m_form->disassociate(*this);
    if (newForm) {
        m_form = newForm->createWeakPtr();
        m_form->associate(*this);
    } else {
        m_form = WeakPtr<HTMLFormElement>();
    }
    didChangeForm();
}

void FormAssociatedElement::willChangeForm()
{
}

void FormAssociatedElement::didChangeForm()
{
}

void FormAssociatedElement::resetFormOwner()
{
    m_formWasSetByParser = false;
    HTMLElement* element = toHTMLElement(this);
    const AtomicString& formId(element->fastGetAttribute(formAttr));
    HTMLFormElement* nearestForm = element->findFormAncestor();
    // 1. If the element's form owner is not null, and either the element is not
    // reassociateable or its form content attribute is not present, and the
    // element's form owner is its nearest form element ancestor after the
    // change to the ancestor chain, then do nothing, and abort these steps.
    if (m_form && formId.isNull() && m_form.get() == nearestForm)
        return;

    HTMLFormElement* originalForm = m_form.get();
    setForm(findAssociatedForm(element));
    // FIXME: Move didAssociateFormControl call to didChangeForm or
    // HTMLFormElement::associate.
    if (m_form && m_form.get() != originalForm && m_form->inDocument())
        element->document().didAssociateFormControl(element);
}

void FormAssociatedElement::formAttributeChanged()
{
    resetFormOwner();
    resetFormAttributeTargetObserver();
}

bool FormAssociatedElement::customError() const
{
    const HTMLElement* element = toHTMLElement(this);
    return element->willValidate() && !m_customValidationMessage.isEmpty();
}

bool FormAssociatedElement::hasBadInput() const
{
    return false;
}

bool FormAssociatedElement::patternMismatch() const
{
    return false;
}

bool FormAssociatedElement::rangeOverflow() const
{
    return false;
}

bool FormAssociatedElement::rangeUnderflow() const
{
    return false;
}

bool FormAssociatedElement::stepMismatch() const
{
    return false;
}

bool FormAssociatedElement::tooLong() const
{
    return false;
}

bool FormAssociatedElement::typeMismatch() const
{
    return false;
}

bool FormAssociatedElement::valid() const
{
    bool someError = typeMismatch() || stepMismatch() || rangeUnderflow() || rangeOverflow()
        || tooLong() || patternMismatch() || valueMissing() || hasBadInput() || customError();
    return !someError;
}

bool FormAssociatedElement::valueMissing() const
{
    return false;
}

String FormAssociatedElement::customValidationMessage() const
{
    return m_customValidationMessage;
}

String FormAssociatedElement::validationMessage() const
{
    return customError() ? m_customValidationMessage : String();
}

void FormAssociatedElement::setCustomValidity(const String& error)
{
    m_customValidationMessage = error;
}

void FormAssociatedElement::resetFormAttributeTargetObserver()
{
    HTMLElement* element = toHTMLElement(this);
    const AtomicString& formId(element->fastGetAttribute(formAttr));
    if (!formId.isNull() && element->inDocument())
        m_formAttributeTargetObserver = FormAttributeTargetObserver::create(formId, this);
    else
        m_formAttributeTargetObserver = nullptr;
}

void FormAssociatedElement::formAttributeTargetChanged()
{
    resetFormOwner();
}

const AtomicString& FormAssociatedElement::name() const
{
    const AtomicString& name = toHTMLElement(this)->getNameAttribute();
    return name.isNull() ? emptyAtom : name;
}

bool FormAssociatedElement::isFormControlElementWithState() const
{
    return false;
}

const HTMLElement& toHTMLElement(const FormAssociatedElement& associatedElement)
{
    if (associatedElement.isFormControlElement())
        return toHTMLFormControlElement(associatedElement);
    // Assumes the element is an HTMLObjectElement
    return toHTMLObjectElement(associatedElement);
}

const HTMLElement* toHTMLElement(const FormAssociatedElement* associatedElement)
{
    ASSERT(associatedElement);
    return &toHTMLElement(*associatedElement);
}

HTMLElement* toHTMLElement(FormAssociatedElement* associatedElement)
{
    return const_cast<HTMLElement*>(toHTMLElement(static_cast<const FormAssociatedElement*>(associatedElement)));
}

HTMLElement& toHTMLElement(FormAssociatedElement& associatedElement)
{
    return const_cast<HTMLElement&>(toHTMLElement(static_cast<const FormAssociatedElement&>(associatedElement)));
}

PassOwnPtr<FormAttributeTargetObserver> FormAttributeTargetObserver::create(const AtomicString& id, FormAssociatedElement* element)
{
    return adoptPtr(new FormAttributeTargetObserver(id, element));
}

FormAttributeTargetObserver::FormAttributeTargetObserver(const AtomicString& id, FormAssociatedElement* element)
    : IdTargetObserver(toHTMLElement(element)->treeScope().idTargetObserverRegistry(), id)
    , m_element(element)
{
}

void FormAttributeTargetObserver::idTargetChanged()
{
    m_element->formAttributeTargetChanged();
}

} // namespace Webcore
