/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2009 Apple Inc. All rights reserved.
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

#ifndef HTMLElement_h
#define HTMLElement_h

#include "core/dom/Element.h"

namespace WebCore {

class DocumentFragment;
class HTMLCollection;
class HTMLFormElement;
class ExceptionState;

enum TranslateAttributeMode {
    TranslateAttributeYes,
    TranslateAttributeNo,
    TranslateAttributeInherit
};

class HTMLElement : public Element {
public:
    static PassRefPtr<HTMLElement> create(const QualifiedName& tagName, Document&);

    virtual String title() const OVERRIDE FINAL;

    virtual short tabIndex() const OVERRIDE;
    void setTabIndex(int);

    void setInnerText(const String&, ExceptionState&);
    void setOuterText(const String&, ExceptionState&);

    virtual bool hasCustomFocusLogic() const;
    virtual bool supportsFocus() const OVERRIDE;

    String contentEditable() const;
    void setContentEditable(const String&, ExceptionState&);

    virtual bool draggable() const;
    void setDraggable(bool);

    bool spellcheck() const;
    void setSpellcheck(bool);

    bool translate() const;
    void setTranslate(bool);

    void click();

    virtual void accessKeyAction(bool sendMouseEvents) OVERRIDE;

    bool ieForbidsInsertHTML() const;

    virtual HTMLFormElement* formOwner() const { return 0; }

    HTMLFormElement* findFormAncestor() const;

    bool hasDirectionAuto() const;
    TextDirection directionalityIfhasDirAutoAttribute(bool& isAuto) const;

    virtual bool isHTMLUnknownElement() const { return false; }

    virtual bool isLabelable() const { return false; }
    // http://www.whatwg.org/specs/web-apps/current-work/multipage/elements.html#interactive-content
    virtual bool isInteractiveContent() const;
    virtual void defaultEventHandler(Event*) OVERRIDE;

    static const AtomicString& eventNameForAttributeName(const QualifiedName& attrName);

    virtual bool matchesReadOnlyPseudoClass() const OVERRIDE;
    virtual bool matchesReadWritePseudoClass() const OVERRIDE;

protected:
    HTMLElement(const QualifiedName& tagName, Document&, ConstructionType);

    void addHTMLLengthToStyle(MutableStylePropertySet*, CSSPropertyID, const String& value);
    void addHTMLColorToStyle(MutableStylePropertySet*, CSSPropertyID, const String& color);

    void applyAlignmentAttributeToStyle(const AtomicString&, MutableStylePropertySet*);
    void applyBorderAttributeToStyle(const AtomicString&, MutableStylePropertySet*);

    virtual void parseAttribute(const QualifiedName&, const AtomicString&) OVERRIDE;
    virtual bool isPresentationAttribute(const QualifiedName&) const OVERRIDE;
    virtual void collectStyleForPresentationAttribute(const QualifiedName&, const AtomicString&, MutableStylePropertySet*) OVERRIDE;
    unsigned parseBorderWidthAttribute(const AtomicString&) const;

    virtual void childrenChanged(bool changedByParser = false, Node* beforeChange = 0, Node* afterChange = 0, int childCountDelta = 0) OVERRIDE;
    void calculateAndAdjustDirectionality();

private:
    virtual String nodeName() const OVERRIDE FINAL;

    void mapLanguageAttributeToLocale(const AtomicString&, MutableStylePropertySet*);

    PassRefPtr<DocumentFragment> textToFragment(const String&, ExceptionState&);

    void dirAttributeChanged(const AtomicString&);
    void adjustDirectionalityIfNeededAfterChildAttributeChanged(Element* child);
    void adjustDirectionalityIfNeededAfterChildrenChanged(Node* beforeChange, int childCountDelta);
    TextDirection directionality(Node** strongDirectionalityTextNode= 0) const;

    TranslateAttributeMode translateAttributeMode() const;

    void handleKeypressEvent(KeyboardEvent*);
    bool supportsSpatialNavigationFocus() const;
};

DEFINE_ELEMENT_TYPE_CASTS(HTMLElement, isHTMLElement());

template <typename T> bool isElementOfType(const HTMLElement&);
template <> inline bool isElementOfType<HTMLElement>(const HTMLElement&) { return true; }

inline HTMLElement::HTMLElement(const QualifiedName& tagName, Document& document, ConstructionType type = CreateHTMLElement)
    : Element(tagName, &document, type)
{
    ASSERT(!tagName.localName().isNull());
    ScriptWrappable::init(this);
}

} // namespace WebCore

#include "HTMLElementTypeHelpers.h"

#endif // HTMLElement_h
