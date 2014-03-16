/*
 * Copyright (C) 2004, 2005, 2006, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006 Rob Buis <buis@kde.org>
 * Copyright (C) 2009 Apple Inc. All rights reserved.
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
 */

#ifndef SVGElement_h
#define SVGElement_h

#include "SVGElementTypeHelpers.h"
#include "core/dom/Element.h"
#include "core/svg/SVGAnimatedString.h"
#include "core/svg/SVGParsingError.h"
#include "core/svg/properties/SVGPropertyInfo.h"
#include "platform/Timer.h"
#include "wtf/HashMap.h"
#include "wtf/OwnPtr.h"

namespace WebCore {

class AffineTransform;
class CSSCursorImageValue;
class Document;
class NewSVGAnimatedPropertyBase;
class SubtreeLayoutScope;
class SVGCursorElement;
class SVGDocumentExtensions;
class SVGElementInstance;
class SVGElementRareData;
class SVGFitToViewBox;
class SVGSVGElement;

void mapAttributeToCSSProperty(HashMap<StringImpl*, CSSPropertyID>* propertyNameToIdMap, const QualifiedName& attrName);

class SVGElement : public Element {
public:
    virtual ~SVGElement();

    bool isOutermostSVGSVGElement() const;

    virtual String title() const OVERRIDE;
    bool hasRelativeLengths() const { return !m_elementsWithRelativeLengths.isEmpty(); }
    virtual bool supportsMarkers() const { return false; }
    PassRefPtrWillBeRawPtr<CSSValue> getPresentationAttribute(const AtomicString& name);
    static bool isAnimatableCSSProperty(const QualifiedName&);
    enum CTMScope {
        NearestViewportScope, // Used by SVGGraphicsElement::getCTM()
        ScreenScope, // Used by SVGGraphicsElement::getScreenCTM()
        AncestorScope // Used by SVGSVGElement::get{Enclosure|Intersection}List()
    };
    virtual AffineTransform localCoordinateSpaceTransform(CTMScope) const;
    virtual bool needsPendingResourceHandling() const { return true; }

    bool instanceUpdatesBlocked() const;
    void setInstanceUpdatesBlocked(bool);

    const AtomicString& xmlbase() const;
    void setXMLbase(const AtomicString&);

    const AtomicString& xmllang() const;
    void setXMLlang(const AtomicString&);

    const AtomicString& xmlspace() const;
    void setXMLspace(const AtomicString&);

    SVGSVGElement* ownerSVGElement() const;
    SVGElement* viewportElement() const;

    SVGDocumentExtensions& accessDocumentSVGExtensions();

    virtual bool isSVGGraphicsElement() const { return false; }
    virtual bool isFilterEffect() const { return false; }
    virtual bool isTextContent() const { return false; }
    virtual bool isTextPositioning() const { return false; }
    virtual bool isStructurallyExternal() const { return false; }

    // For SVGTests
    virtual bool isValid() const { return true; }

    virtual void svgAttributeChanged(const QualifiedName&);

    PassRefPtr<NewSVGAnimatedPropertyBase> propertyFromAttribute(const QualifiedName& attributeName);
    static AnimatedPropertyType animatedPropertyTypeForCSSAttribute(const QualifiedName& attributeName);

    void sendSVGLoadEventIfPossible(bool sendParentLoadEvents = false);
    void sendSVGLoadEventIfPossibleAsynchronously();
    void svgLoadEventTimerFired(Timer<SVGElement>*);
    virtual Timer<SVGElement>* svgLoadEventTimer();

    virtual AffineTransform* supplementalTransform() { return 0; }

    void invalidateSVGAttributes() { ensureUniqueElementData().m_animatedSVGAttributesAreDirty = true; }

    const HashSet<SVGElementInstance*>& instancesForElement() const;

    bool getBoundingBox(FloatRect&);

    void setCursorElement(SVGCursorElement*);
    void cursorElementRemoved();
    void setCursorImageValue(CSSCursorImageValue*);
    void cursorImageValueRemoved();

    SVGElement* correspondingElement();
    void setCorrespondingElement(SVGElement*);

    void synchronizeAnimatedSVGAttribute(const QualifiedName&) const;

    virtual PassRefPtr<RenderStyle> customStyleForRenderer() OVERRIDE FINAL;

    virtual void synchronizeRequiredFeatures() { }
    virtual void synchronizeRequiredExtensions() { }
    virtual void synchronizeSystemLanguage() { }

#ifndef NDEBUG
    virtual bool isAnimatableAttribute(const QualifiedName&) const;
#endif

    MutableStylePropertySet* animatedSMILStyleProperties() const;
    MutableStylePropertySet* ensureAnimatedSMILStyleProperties();
    void setUseOverrideComputedStyle(bool);

    virtual bool haveLoadedRequiredResources();

    virtual bool addEventListener(const AtomicString& eventType, PassRefPtr<EventListener>, bool useCapture) OVERRIDE FINAL;
    virtual bool removeEventListener(const AtomicString& eventType, EventListener*, bool useCapture) OVERRIDE FINAL;

    void invalidateRelativeLengthClients(SubtreeLayoutScope* = 0);

    bool isContextElement() const { return m_isContextElement; }
    void setContextElement() { m_isContextElement = true; }

    void addToPropertyMap(PassRefPtr<NewSVGAnimatedPropertyBase>);

    SVGAnimatedString* className() { return m_className.get(); }

protected:
    SVGElement(const QualifiedName&, Document&, ConstructionType = CreateSVGElement);

    virtual void parseAttribute(const QualifiedName&, const AtomicString&) OVERRIDE;

    virtual void finishParsingChildren() OVERRIDE;
    virtual void attributeChanged(const QualifiedName&, const AtomicString&, AttributeModificationReason = ModifiedDirectly) OVERRIDE;

    virtual bool isPresentationAttribute(const QualifiedName&) const OVERRIDE;
    virtual void collectStyleForPresentationAttribute(const QualifiedName&, const AtomicString&, MutableStylePropertySet*) OVERRIDE;
    virtual bool rendererIsNeeded(const RenderStyle&) OVERRIDE;

    virtual InsertionNotificationRequest insertedInto(ContainerNode*) OVERRIDE;
    virtual void removedFrom(ContainerNode*) OVERRIDE;
    virtual void childrenChanged(bool changedByParser = false, Node* beforeChange = 0, Node* afterChange = 0, int childCountDelta = 0) OVERRIDE;

    static CSSPropertyID cssPropertyIdForSVGAttributeName(const QualifiedName&);
    void updateRelativeLengthsInformation() { updateRelativeLengthsInformation(selfHasRelativeLengths(), this); }
    void updateRelativeLengthsInformation(bool hasRelativeLengths, SVGElement*);

    virtual bool selfHasRelativeLengths() const { return false; }

    SVGElementRareData* svgRareData() const;
    SVGElementRareData* ensureSVGRareData();

    bool hasSVGRareData() const { return m_hasSVGRareData; }
    void setHasSVGRareData() { m_hasSVGRareData = true; }
    void clearHasSVGRareData() { m_hasSVGRareData = false; }

    // SVGFitToViewBox::parseAttribute uses reportAttributeParsingError.
    friend class SVGFitToViewBox;
    void reportAttributeParsingError(SVGParsingError, const QualifiedName&, const AtomicString&);
    bool hasFocusEventListeners() const;

private:
    friend class SVGElementInstance;

    // FIXME: Author shadows should be allowed
    // https://bugs.webkit.org/show_bug.cgi?id=77938
    virtual bool areAuthorShadowsAllowed() const OVERRIDE FINAL { return false; }

    RenderStyle* computedStyle(PseudoId = NOPSEUDO);
    virtual RenderStyle* virtualComputedStyle(PseudoId pseudoElementSpecifier = NOPSEUDO) OVERRIDE FINAL { return computedStyle(pseudoElementSpecifier); }
    virtual void willRecalcStyle(StyleRecalcChange) OVERRIDE;
    virtual bool isKeyboardFocusable() const OVERRIDE;

    void buildPendingResourcesIfNeeded();

    void mapInstanceToElement(SVGElementInstance*);
    void removeInstanceMapping(SVGElementInstance*);

    void cleanupAnimatedProperties();
    friend class CleanUpAnimatedPropertiesCaller;

    HashSet<SVGElement*> m_elementsWithRelativeLengths;

    typedef HashMap<QualifiedName, RefPtr<NewSVGAnimatedPropertyBase> > AttributeToPropertyMap;
    AttributeToPropertyMap m_newAttributeToPropertyMap;

#if !ASSERT_DISABLED
    bool m_inRelativeLengthClientsInvalidation;
#endif
    unsigned m_isContextElement : 1;
    unsigned m_hasSVGRareData : 1;

    RefPtr<SVGAnimatedString> m_className;
};

struct SVGAttributeHashTranslator {
    static unsigned hash(const QualifiedName& key)
    {
        if (key.hasPrefix()) {
            QualifiedNameComponents components = { nullAtom.impl(), key.localName().impl(), key.namespaceURI().impl() };
            return hashComponents(components);
        }
        return DefaultHash<QualifiedName>::Hash::hash(key);
    }
    static bool equal(const QualifiedName& a, const QualifiedName& b) { return a.matches(b); }
};

DEFINE_ELEMENT_TYPE_CASTS(SVGElement, isSVGElement());

}

#endif
