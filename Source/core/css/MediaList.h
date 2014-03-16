/*
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2006, 2008, 2009, 2010, 2012 Apple Inc. All rights reserved.
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

#ifndef MediaList_h
#define MediaList_h

#include "core/dom/ExceptionCode.h"
#include "heap/Handle.h"
#include "wtf/Forward.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefCounted.h"
#include "wtf/Vector.h"
#include "wtf/text/WTFString.h"

namespace WebCore {

class CSSRule;
class CSSStyleSheet;
class Document;
class ExceptionState;
class MediaList;
class MediaQuery;

class MediaQuerySet : public RefCountedWillBeGarbageCollected<MediaQuerySet> {
    DECLARE_EMPTY_DESTRUCTOR_WILL_BE_REMOVED(MediaQuerySet);
public:
    static PassRefPtrWillBeRawPtr<MediaQuerySet> create()
    {
        return adoptRefWillBeNoop(new MediaQuerySet());
    }
    static PassRefPtrWillBeRawPtr<MediaQuerySet> create(const String& mediaString);

    bool set(const String&);
    bool add(const String&);
    bool remove(const String&);

    void addMediaQuery(PassOwnPtrWillBeRawPtr<MediaQuery>);

    const WillBeHeapVector<OwnPtrWillBeMember<MediaQuery> >& queryVector() const { return m_queries; }

    String mediaText() const;

    PassRefPtrWillBeRawPtr<MediaQuerySet> copy() const { return adoptRefWillBeNoop(new MediaQuerySet(*this)); }

    void trace(Visitor*);

private:
    MediaQuerySet();
    MediaQuerySet(const MediaQuerySet&);

    WillBeHeapVector<OwnPtrWillBeMember<MediaQuery> > m_queries;
};

class MediaList : public RefCountedWillBeGarbageCollectedFinalized<MediaList> {
public:
    static PassRefPtrWillBeRawPtr<MediaList> create(MediaQuerySet* mediaQueries, CSSStyleSheet* parentSheet)
    {
        return adoptRefWillBeNoop(new MediaList(mediaQueries, parentSheet));
    }
    static PassRefPtrWillBeRawPtr<MediaList> create(MediaQuerySet* mediaQueries, CSSRule* parentRule)
    {
        return adoptRefWillBeNoop(new MediaList(mediaQueries, parentRule));
    }

    ~MediaList();

    unsigned length() const { return m_mediaQueries->queryVector().size(); }
    String item(unsigned index) const;
    void deleteMedium(const String& oldMedium, ExceptionState&);
    void appendMedium(const String& newMedium, ExceptionState&);

    String mediaText() const { return m_mediaQueries->mediaText(); }
    void setMediaText(const String&);

    // Not part of CSSOM.
    CSSRule* parentRule() const { return m_parentRule; }
    CSSStyleSheet* parentStyleSheet() const { return m_parentStyleSheet; }

#if !ENABLE(OILPAN)
    void clearParentStyleSheet() { ASSERT(m_parentStyleSheet); m_parentStyleSheet = nullptr; }
    void clearParentRule() { ASSERT(m_parentRule); m_parentRule = nullptr; }
#endif

    const MediaQuerySet* queries() const { return m_mediaQueries.get(); }

    void reattach(MediaQuerySet*);

    void trace(Visitor*);

private:
    MediaList();
    MediaList(MediaQuerySet*, CSSStyleSheet* parentSheet);
    MediaList(MediaQuerySet*, CSSRule* parentRule);

    RefPtrWillBeMember<MediaQuerySet> m_mediaQueries;
    // Cleared in ~CSSStyleSheet destructor when oilpan is not enabled.
    RawPtrWillBeMember<CSSStyleSheet> m_parentStyleSheet;
    // Cleared in the ~CSSMediaRule and ~CSSImportRule destructors when oilpan is not enabled.
    RawPtrWillBeMember<CSSRule> m_parentRule;
};

// Adds message to inspector console whenever dpi or dpcm values are used for "screen" media.
void reportMediaQueryWarningIfNeeded(Document*, const MediaQuerySet*);

} // namespace

#endif
