/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#ifndef ElementStyleResources_h
#define ElementStyleResources_h

#include "CSSPropertyNames.h"
#include "heap/Handle.h"
#include "platform/graphics/Color.h"
#include "wtf/HashMap.h"
#include "wtf/Noncopyable.h"

namespace WebCore {

class CSSCursorImageValue;
class CSSImageValue;
class CSSImageGeneratorValue;
class CSSImageSetValue;
class CSSSVGDocumentValue;
class CSSValue;
class FilterOperation;
class StyleImage;
class TextLinkColors;

typedef WillBePersistentHeapHashMap<FilterOperation*, RefPtrWillBeMember<CSSSVGDocumentValue> > PendingSVGDocumentMap;
typedef WillBePersistentHeapHashMap<CSSPropertyID, RefPtrWillBeMember<CSSValue> > PendingImagePropertyMap;

// Holds information about resources, requested by stylesheets.
// Lifetime: per-element style resolve.
class ElementStyleResources {
WTF_MAKE_NONCOPYABLE(ElementStyleResources);
public:
    ElementStyleResources();

    PassRefPtr<StyleImage> styleImage(const TextLinkColors&, Color currentColor, CSSPropertyID, CSSValue*);

    PassRefPtr<StyleImage> generatedOrPendingFromValue(CSSPropertyID, CSSImageGeneratorValue*);
    PassRefPtr<StyleImage> cachedOrPendingFromValue(CSSPropertyID, CSSImageValue*);
    PassRefPtr<StyleImage> setOrPendingFromValue(CSSPropertyID, CSSImageSetValue*);
    PassRefPtr<StyleImage> cursorOrPendingFromValue(CSSPropertyID, CSSCursorImageValue*);

    const PendingImagePropertyMap& pendingImageProperties() const { return m_pendingImageProperties; }
    const PendingSVGDocumentMap& pendingSVGDocuments() const { return m_pendingSVGDocuments; }

    float deviceScaleFactor() const { return m_deviceScaleFactor; }
    void setDeviceScaleFactor(float deviceScaleFactor) { m_deviceScaleFactor = deviceScaleFactor; }

    void addPendingSVGDocument(FilterOperation*, CSSSVGDocumentValue*);

private:
    PendingImagePropertyMap m_pendingImageProperties;
    PendingSVGDocumentMap m_pendingSVGDocuments;
    float m_deviceScaleFactor;
};

} // namespace WebCore

#endif // ElementStyleResources_h
