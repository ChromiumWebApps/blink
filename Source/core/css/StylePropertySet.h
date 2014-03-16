/*
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2008, 2012 Apple Inc. All rights reserved.
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

#ifndef StylePropertySet_h
#define StylePropertySet_h

#include "CSSPropertyNames.h"
#include "core/css/CSSParserMode.h"
#include "core/css/CSSPrimitiveValue.h"
#include "core/css/CSSProperty.h"
#include "core/css/PropertySetCSSStyleDeclaration.h"
#include "wtf/ListHashSet.h"
#include "wtf/Vector.h"
#include "wtf/text/WTFString.h"

namespace WebCore {

class CSSRule;
class CSSStyleDeclaration;
class Element;
class ImmutableStylePropertySet;
class KURL;
class MutableStylePropertySet;
class StylePropertyShorthand;
class StyleSheetContents;

class StylePropertySet : public RefCountedWillBeRefCountedGarbageCollected<StylePropertySet> {
    friend class PropertyReference;
public:

#if ENABLE(OILPAN)
    // When oilpan is enabled override the finalize method to dispatch to the subclasses'
    // destructor. This can be removed once the MutableStylePropertySet's OwnPtr is moved
    // to the heap.
    void finalize();
#else
    // Override RefCounted's deref() to ensure operator delete is called on
    // the appropriate subclass type.
    void deref();
#endif

    class PropertyReference {
    public:
        PropertyReference(const StylePropertySet& propertySet, unsigned index)
            : m_propertySet(propertySet)
            , m_index(index)
        {
        }

        CSSPropertyID id() const { return static_cast<CSSPropertyID>(propertyMetadata().m_propertyID); }
        CSSPropertyID shorthandID() const { return propertyMetadata().shorthandID(); }

        bool isImportant() const { return propertyMetadata().m_important; }
        bool isInherited() const { return propertyMetadata().m_inherited; }
        bool isImplicit() const { return propertyMetadata().m_implicit; }

        String cssName() const;
        String cssText() const;

        const CSSValue* value() const { return propertyValue(); }
        // FIXME: We should try to remove this mutable overload.
        CSSValue* value() { return const_cast<CSSValue*>(propertyValue()); }

        // FIXME: Remove this.
        CSSProperty toCSSProperty() const { return CSSProperty(propertyMetadata(), const_cast<CSSValue*>(propertyValue())); }

        const StylePropertyMetadata& propertyMetadata() const;

    private:
        const CSSValue* propertyValue() const;

        const StylePropertySet& m_propertySet;
        unsigned m_index;
    };

    unsigned propertyCount() const;
    bool isEmpty() const;
    PropertyReference propertyAt(unsigned index) const { return PropertyReference(*this, index); }
    int findPropertyIndex(CSSPropertyID) const;

    PassRefPtrWillBeRawPtr<CSSValue> getPropertyCSSValue(CSSPropertyID) const;
    String getPropertyValue(CSSPropertyID) const;

    bool propertyIsImportant(CSSPropertyID) const;
    CSSPropertyID getPropertyShorthand(CSSPropertyID) const;
    bool isPropertyImplicit(CSSPropertyID) const;

    PassRefPtr<MutableStylePropertySet> copyBlockProperties() const;

    CSSParserMode cssParserMode() const { return static_cast<CSSParserMode>(m_cssParserMode); }

    PassRefPtr<MutableStylePropertySet> mutableCopy() const;
    PassRefPtr<ImmutableStylePropertySet> immutableCopyIfNeeded() const;

    PassRefPtr<MutableStylePropertySet> copyPropertiesInSet(const Vector<CSSPropertyID>&) const;

    String asText() const;

    bool isMutable() const { return m_isMutable; }

    bool hasFailedOrCanceledSubresources() const;

    static unsigned averageSizeInBytes();

#ifndef NDEBUG
    void showStyle();
#endif

    bool propertyMatches(CSSPropertyID, const CSSValue*) const;

    void trace(Visitor*);
    void traceAfterDispatch(Visitor*) { }

protected:

    enum { MaxArraySize = (1 << 28) - 1 };

    StylePropertySet(CSSParserMode cssParserMode)
        : m_cssParserMode(cssParserMode)
        , m_isMutable(true)
        , m_arraySize(0)
    { }

    StylePropertySet(CSSParserMode cssParserMode, unsigned immutableArraySize)
        : m_cssParserMode(cssParserMode)
        , m_isMutable(false)
        , m_arraySize(std::min(immutableArraySize, unsigned(MaxArraySize)))
    { }

    unsigned m_cssParserMode : 3;
    mutable unsigned m_isMutable : 1;
    unsigned m_arraySize : 28;

    friend class PropertySetCSSStyleDeclaration;
};

class ImmutableStylePropertySet : public StylePropertySet {
public:
    ~ImmutableStylePropertySet();
    static PassRefPtr<ImmutableStylePropertySet> create(const CSSProperty* properties, unsigned count, CSSParserMode);

    unsigned propertyCount() const { return m_arraySize; }

    const RawPtrWillBeMember<CSSValue>* valueArray() const;
    const StylePropertyMetadata* metadataArray() const;
    int findPropertyIndex(CSSPropertyID) const;

    void traceAfterDispatch(Visitor*);

    void* operator new(std::size_t, void* location)
    {
        return location;
    }

    void* m_storage;

private:
    ImmutableStylePropertySet(const CSSProperty*, unsigned count, CSSParserMode);
};

inline const RawPtrWillBeMember<CSSValue>* ImmutableStylePropertySet::valueArray() const
{
    return reinterpret_cast<const RawPtrWillBeMember<CSSValue>*>(const_cast<const void**>(&(this->m_storage)));
}

inline const StylePropertyMetadata* ImmutableStylePropertySet::metadataArray() const
{
    return reinterpret_cast<const StylePropertyMetadata*>(&reinterpret_cast<const char*>(&(this->m_storage))[m_arraySize * sizeof(RawPtrWillBeMember<CSSValue>)]);
}

DEFINE_TYPE_CASTS(ImmutableStylePropertySet, StylePropertySet, set, !set->isMutable(), !set.isMutable());

inline ImmutableStylePropertySet* toImmutableStylePropertySet(const RefPtr<StylePropertySet>& set)
{
    return toImmutableStylePropertySet(set.get());
}

class MutableStylePropertySet : public StylePropertySet {
public:
    ~MutableStylePropertySet() { }
    static PassRefPtr<MutableStylePropertySet> create(CSSParserMode = HTMLQuirksMode);
    static PassRefPtr<MutableStylePropertySet> create(const CSSProperty* properties, unsigned count);

    unsigned propertyCount() const { return m_propertyVector.size(); }

    void addParsedProperties(const WillBeHeapVector<CSSProperty, 256>&);
    void addParsedProperty(const CSSProperty&);

    // These expand shorthand properties into multiple properties.
    bool setProperty(CSSPropertyID, const String& value, bool important = false, StyleSheetContents* contextStyleSheet = 0);
    void setProperty(CSSPropertyID, PassRefPtrWillBeRawPtr<CSSValue>, bool important = false);

    // These do not. FIXME: This is too messy, we can do better.
    bool setProperty(CSSPropertyID, CSSValueID identifier, bool important = false);
    bool setProperty(CSSPropertyID, CSSPropertyID identifier, bool important = false);
    void appendPrefixingVariantProperty(const CSSProperty&);
    void setPrefixingVariantProperty(const CSSProperty&);
    void setProperty(const CSSProperty&, CSSProperty* slot = 0);

    bool removeProperty(CSSPropertyID, String* returnText = 0);
    void removePrefixedOrUnprefixedProperty(CSSPropertyID);
    void removeBlockProperties();
    bool removePropertiesInSet(const CSSPropertyID* set, unsigned length);
    void removeEquivalentProperties(const StylePropertySet*);
    void removeEquivalentProperties(const CSSStyleDeclaration*);

    void mergeAndOverrideOnConflict(const StylePropertySet*);

    void clear();
    void parseDeclaration(const String& styleDeclaration, StyleSheetContents* contextStyleSheet);

    CSSStyleDeclaration* ensureCSSStyleDeclaration();
    int findPropertyIndex(CSSPropertyID) const;

    void traceAfterDispatch(Visitor*);

private:
    explicit MutableStylePropertySet(CSSParserMode);
    explicit MutableStylePropertySet(const StylePropertySet&);
    MutableStylePropertySet(const CSSProperty* properties, unsigned count);

    bool removeShorthandProperty(CSSPropertyID);
    CSSProperty* findCSSPropertyWithID(CSSPropertyID);
    OwnPtr<PropertySetCSSStyleDeclaration> m_cssomWrapper;

    friend class StylePropertySet;

    WillBeHeapVector<CSSProperty, 4> m_propertyVector;
};

DEFINE_TYPE_CASTS(MutableStylePropertySet, StylePropertySet, set, set->isMutable(), set.isMutable());

inline MutableStylePropertySet* toMutableStylePropertySet(const RefPtr<StylePropertySet>& set)
{
    return toMutableStylePropertySet(set.get());
}

inline const StylePropertyMetadata& StylePropertySet::PropertyReference::propertyMetadata() const
{
    if (m_propertySet.isMutable())
        return toMutableStylePropertySet(m_propertySet).m_propertyVector.at(m_index).metadata();
    return toImmutableStylePropertySet(m_propertySet).metadataArray()[m_index];
}

inline const CSSValue* StylePropertySet::PropertyReference::propertyValue() const
{
    if (m_propertySet.isMutable())
        return toMutableStylePropertySet(m_propertySet).m_propertyVector.at(m_index).value();
    return toImmutableStylePropertySet(m_propertySet).valueArray()[m_index];
}

inline unsigned StylePropertySet::propertyCount() const
{
    if (m_isMutable)
        return toMutableStylePropertySet(this)->m_propertyVector.size();
    return m_arraySize;
}

inline bool StylePropertySet::isEmpty() const
{
    return !propertyCount();
}

#if !ENABLE(OILPAN)
inline void StylePropertySet::deref()
{
    if (!derefBase())
        return;

    if (m_isMutable)
        delete toMutableStylePropertySet(this);
    else
        delete toImmutableStylePropertySet(this);
}
#endif // !ENABLE(OILPAN)

inline int StylePropertySet::findPropertyIndex(CSSPropertyID propertyID) const
{
    if (m_isMutable)
        return toMutableStylePropertySet(this)->findPropertyIndex(propertyID);
    return toImmutableStylePropertySet(this)->findPropertyIndex(propertyID);
}

} // namespace WebCore

#endif // StylePropertySet_h
