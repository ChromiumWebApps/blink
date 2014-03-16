/*
 * Copyright (C) 2005, 2006, 2008, 2011 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "core/loader/HistoryItem.h"

#include "core/dom/Document.h"
#include "platform/network/ResourceRequest.h"
#include "wtf/CurrentTime.h"
#include "wtf/text/CString.h"

namespace WebCore {

static long long generateSequenceNumber()
{
    // Initialize to the current time to reduce the likelihood of generating
    // identifiers that overlap with those from past/future browser sessions.
    static long long next = static_cast<long long>(currentTime() * 1000000.0);
    return ++next;
}

HistoryItem::HistoryItem()
    : m_pageScaleFactor(0)
    , m_itemSequenceNumber(generateSequenceNumber())
    , m_documentSequenceNumber(generateSequenceNumber())
{
}

HistoryItem::~HistoryItem()
{
}

inline HistoryItem::HistoryItem(const HistoryItem& item)
    : RefCounted<HistoryItem>()
    , m_urlString(item.m_urlString)
    , m_referrer(item.m_referrer)
    , m_target(item.m_target)
    , m_scrollPoint(item.m_scrollPoint)
    , m_pageScaleFactor(item.m_pageScaleFactor)
    , m_documentState(item.m_documentState)
    , m_itemSequenceNumber(item.m_itemSequenceNumber)
    , m_documentSequenceNumber(item.m_documentSequenceNumber)
    , m_stateObject(item.m_stateObject)
    , m_formContentType(item.m_formContentType)
{
    if (item.m_formData)
        m_formData = item.m_formData->copy();

    unsigned size = item.m_children.size();
    m_children.reserveInitialCapacity(size);
    for (unsigned i = 0; i < size; ++i)
        m_children.uncheckedAppend(item.m_children[i]->copy());
}

PassRefPtr<HistoryItem> HistoryItem::copy() const
{
    return adoptRef(new HistoryItem(*this));
}

void HistoryItem::generateNewSequenceNumbers()
{
    m_itemSequenceNumber = generateSequenceNumber();
    m_documentSequenceNumber = generateSequenceNumber();
}

const String& HistoryItem::urlString() const
{
    return m_urlString;
}

KURL HistoryItem::url() const
{
    return KURL(ParsedURLString, m_urlString);
}

const Referrer& HistoryItem::referrer() const
{
    return m_referrer;
}

const String& HistoryItem::target() const
{
    return m_target;
}

void HistoryItem::setURLString(const String& urlString)
{
    if (m_urlString != urlString)
        m_urlString = urlString;
}

void HistoryItem::setURL(const KURL& url)
{
    setURLString(url.string());
    clearDocumentState();
}

void HistoryItem::setReferrer(const Referrer& referrer)
{
    m_referrer = referrer;
}

void HistoryItem::setTarget(const String& target)
{
    m_target = target;
}

const IntPoint& HistoryItem::scrollPoint() const
{
    return m_scrollPoint;
}

void HistoryItem::setScrollPoint(const IntPoint& point)
{
    m_scrollPoint = point;
}

void HistoryItem::clearScrollPoint()
{
    m_scrollPoint.setX(0);
    m_scrollPoint.setY(0);
}

float HistoryItem::pageScaleFactor() const
{
    return m_pageScaleFactor;
}

void HistoryItem::setPageScaleFactor(float scaleFactor)
{
    m_pageScaleFactor = scaleFactor;
}

void HistoryItem::setDocumentState(const Vector<String>& state)
{
    m_documentState = state;
}

const Vector<String>& HistoryItem::documentState() const
{
    return m_documentState;
}

void HistoryItem::clearDocumentState()
{
    m_documentState.clear();
}

void HistoryItem::setStateObject(PassRefPtr<SerializedScriptValue> object)
{
    m_stateObject = object;
}

void HistoryItem::addChildItem(PassRefPtr<HistoryItem> child)
{
    m_children.append(child);
}

const HistoryItemVector& HistoryItem::children() const
{
    return m_children;
}

void HistoryItem::clearChildren()
{
    m_children.clear();
}

const AtomicString& HistoryItem::formContentType() const
{
    return m_formContentType;
}

void HistoryItem::setFormInfoFromRequest(const ResourceRequest& request)
{
    if (equalIgnoringCase(request.httpMethod(), "POST")) {
        // FIXME: Eventually we have to make this smart enough to handle the case where
        // we have a stream for the body to handle the "data interspersed with files" feature.
        m_formData = request.httpBody();
        m_formContentType = request.httpContentType();
    } else {
        m_formData = nullptr;
        m_formContentType = nullAtom;
    }
}

void HistoryItem::setFormData(PassRefPtr<FormData> formData)
{
    m_formData = formData;
}

void HistoryItem::setFormContentType(const AtomicString& formContentType)
{
    m_formContentType = formContentType;
}

FormData* HistoryItem::formData()
{
    return m_formData.get();
}

bool HistoryItem::isCurrentDocument(Document* doc) const
{
    // FIXME: We should find a better way to check if this is the current document.
    return equalIgnoringFragmentIdentifier(url(), doc->url());
}

} // namespace WebCore

