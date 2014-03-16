/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2010, 2011, 2012 Apple Inc. All rights reserved.
 * Copyright (C) 2014 Samsung Electronics. All rights reserved.
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
#include "core/html/HTMLFormControlsCollection.h"

#include "HTMLNames.h"
#include "core/frame/UseCounter.h"
#include "core/html/HTMLFieldSetElement.h"
#include "core/html/HTMLFormElement.h"
#include "core/html/HTMLImageElement.h"
#include "wtf/HashSet.h"

namespace WebCore {

using namespace HTMLNames;

// Since the collections are to be "live", we have to do the
// calculation every time if anything has changed.

HTMLFormControlsCollection::HTMLFormControlsCollection(ContainerNode& ownerNode)
    : HTMLCollection(ownerNode, FormControls, OverridesItemAfter)
    , m_cachedElement(0)
    , m_cachedElementOffsetInArray(0)
{
    ASSERT(isHTMLFormElement(ownerNode) || isHTMLFieldSetElement(ownerNode));
    ScriptWrappable::init(this);
}

PassRefPtr<HTMLFormControlsCollection> HTMLFormControlsCollection::create(ContainerNode& ownerNode, CollectionType)
{
    return adoptRef(new HTMLFormControlsCollection(ownerNode));
}

HTMLFormControlsCollection::~HTMLFormControlsCollection()
{
}

const Vector<FormAssociatedElement*>& HTMLFormControlsCollection::formControlElements() const
{
    ASSERT(isHTMLFormElement(ownerNode()) || isHTMLFieldSetElement(ownerNode()));
    if (isHTMLFormElement(ownerNode()))
        return toHTMLFormElement(ownerNode()).associatedElements();
    return toHTMLFieldSetElement(ownerNode()).associatedElements();
}

const Vector<HTMLImageElement*>& HTMLFormControlsCollection::formImageElements() const
{
    return toHTMLFormElement(ownerNode()).imageElements();
}

static unsigned findFormAssociatedElement(const Vector<FormAssociatedElement*>& associatedElements, Element* element)
{
    unsigned i = 0;
    for (; i < associatedElements.size(); ++i) {
        FormAssociatedElement* associatedElement = associatedElements[i];
        if (associatedElement->isEnumeratable() && toHTMLElement(associatedElement) == element)
            break;
    }
    return i;
}

Element* HTMLFormControlsCollection::virtualItemAfter(Element* previous) const
{
    const Vector<FormAssociatedElement*>& associatedElements = formControlElements();
    unsigned offset;
    if (!previous)
        offset = 0;
    else if (m_cachedElement == previous)
        offset = m_cachedElementOffsetInArray + 1;
    else
        offset = findFormAssociatedElement(associatedElements, previous) + 1;

    for (unsigned i = offset; i < associatedElements.size(); ++i) {
        FormAssociatedElement* associatedElement = associatedElements[i];
        if (associatedElement->isEnumeratable()) {
            m_cachedElement = toHTMLElement(associatedElement);
            m_cachedElementOffsetInArray = i;
            return m_cachedElement;
        }
    }
    return 0;
}

void HTMLFormControlsCollection::invalidateCache(Document* oldDocument) const
{
    HTMLCollection::invalidateCache(oldDocument);
    m_cachedElement = 0;
    m_cachedElementOffsetInArray = 0;
}

static HTMLElement* firstNamedItem(const Vector<FormAssociatedElement*>& elementsArray,
    const Vector<HTMLImageElement*>* imageElementsArray, const QualifiedName& attrName, const String& name)
{
    ASSERT(attrName == idAttr || attrName == nameAttr);

    for (unsigned i = 0; i < elementsArray.size(); ++i) {
        HTMLElement* element = toHTMLElement(elementsArray[i]);
        if (elementsArray[i]->isEnumeratable() && element->fastGetAttribute(attrName) == name)
            return element;
    }

    if (!imageElementsArray)
        return 0;

    for (unsigned i = 0; i < imageElementsArray->size(); ++i) {
        HTMLImageElement* element = (*imageElementsArray)[i];
        if (element->fastGetAttribute(attrName) == name) {
            UseCounter::count(element->document(), UseCounter::FormNameAccessForImageElement);
            return element;
        }
    }

    return 0;
}

Element* HTMLFormControlsCollection::namedItem(const AtomicString& name) const
{
    // http://msdn.microsoft.com/workshop/author/dhtml/reference/methods/nameditem.asp
    // This method first searches for an object with a matching id
    // attribute. If a match is not found, the method then searches for an
    // object with a matching name attribute, but only on those elements
    // that are allowed a name attribute.
    const Vector<HTMLImageElement*>* imagesElements = isHTMLFieldSetElement(ownerNode()) ? 0 : &formImageElements();
    if (HTMLElement* item = firstNamedItem(formControlElements(), imagesElements, idAttr, name))
        return item;

    return firstNamedItem(formControlElements(), imagesElements, nameAttr, name);
}

void HTMLFormControlsCollection::updateIdNameCache() const
{
    if (hasValidIdNameCache())
        return;

    HashSet<StringImpl*> foundInputElements;

    const Vector<FormAssociatedElement*>& elementsArray = formControlElements();

    for (unsigned i = 0; i < elementsArray.size(); ++i) {
        FormAssociatedElement* associatedElement = elementsArray[i];
        if (associatedElement->isEnumeratable()) {
            HTMLElement* element = toHTMLElement(associatedElement);
            const AtomicString& idAttrVal = element->getIdAttribute();
            const AtomicString& nameAttrVal = element->getNameAttribute();
            if (!idAttrVal.isEmpty()) {
                appendIdCache(idAttrVal, element);
                foundInputElements.add(idAttrVal.impl());
            }
            if (!nameAttrVal.isEmpty() && idAttrVal != nameAttrVal) {
                appendNameCache(nameAttrVal, element);
                foundInputElements.add(nameAttrVal.impl());
            }
        }
    }

    if (isHTMLFormElement(ownerNode())) {
        const Vector<HTMLImageElement*>& imageElementsArray = formImageElements();
        for (unsigned i = 0; i < imageElementsArray.size(); ++i) {
            HTMLImageElement* element = imageElementsArray[i];
            const AtomicString& idAttrVal = element->getIdAttribute();
            const AtomicString& nameAttrVal = element->getNameAttribute();
            if (!idAttrVal.isEmpty() && !foundInputElements.contains(idAttrVal.impl()))
                appendIdCache(idAttrVal, element);
            if (!nameAttrVal.isEmpty() && idAttrVal != nameAttrVal && !foundInputElements.contains(nameAttrVal.impl()))
                appendNameCache(nameAttrVal, element);
        }
    }

    setHasValidIdNameCache();
}

void HTMLFormControlsCollection::namedGetter(const AtomicString& name, bool& radioNodeListEnabled, RefPtr<RadioNodeList>& radioNodeList, bool& elementEnabled, RefPtr<Element>& element)
{
    Vector<RefPtr<Element> > namedItems;
    this->namedItems(name, namedItems);

    if (namedItems.isEmpty())
        return;

    if (namedItems.size() == 1) {
        elementEnabled = true;
        element = namedItems.first();
        return;
    }

    radioNodeListEnabled = true;
    radioNodeList = ownerNode().radioNodeList(name);
}

void HTMLFormControlsCollection::supportedPropertyNames(Vector<String>& names)
{
    // http://www.whatwg.org/specs/web-apps/current-work/multipage/common-dom-interfaces.html#htmlformcontrolscollection-0:
    // The supported property names consist of the non-empty values of all the id and name attributes
    // of all the elements represented by the collection, in tree order, ignoring later duplicates,
    // with the id of an element preceding its name if it contributes both, they differ from each
    // other, and neither is the duplicate of an earlier entry.
    HashSet<AtomicString> existingNames;
    unsigned length = this->length();
    for (unsigned i = 0; i < length; ++i) {
        Element* element = item(i);
        ASSERT(element);
        const AtomicString& idAttribute = element->getIdAttribute();
        if (!idAttribute.isEmpty()) {
            HashSet<AtomicString>::AddResult addResult = existingNames.add(idAttribute);
            if (addResult.isNewEntry)
                names.append(idAttribute);
        }
        const AtomicString& nameAttribute = element->getNameAttribute();
        if (!nameAttribute.isEmpty()) {
            HashSet<AtomicString>::AddResult addResult = existingNames.add(nameAttribute);
            if (addResult.isNewEntry)
                names.append(nameAttribute);
        }
    }
}

}
