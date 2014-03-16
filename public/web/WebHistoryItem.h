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

#ifndef WebHistoryItem_h
#define WebHistoryItem_h

#include "../platform/WebCommon.h"
#include "../platform/WebPrivatePtr.h"
#include "../platform/WebReferrerPolicy.h"

namespace WebCore { class HistoryItem; }

namespace blink {
class WebHTTPBody;
class WebString;
class WebSerializedScriptValue;
struct WebPoint;
template <typename T> class WebVector;

// Represents a frame-level navigation entry in session history.  A
// WebHistoryItem is a node in a tree.
//
// Copying a WebHistoryItem is cheap.
//
class WebHistoryItem {
public:
    ~WebHistoryItem() { reset(); }

    WebHistoryItem() { }
    WebHistoryItem(const WebHistoryItem& h) { assign(h); }
    WebHistoryItem& operator=(const WebHistoryItem& h)
    {
        assign(h);
        return *this;
    }

    BLINK_EXPORT void initialize();
    BLINK_EXPORT void reset();
    BLINK_EXPORT void assign(const WebHistoryItem&);

    bool isNull() const { return m_private.isNull(); }

    BLINK_EXPORT WebString urlString() const;
    BLINK_EXPORT void setURLString(const WebString&);

    BLINK_EXPORT WebString referrer() const;
    BLINK_EXPORT WebReferrerPolicy referrerPolicy() const;
    BLINK_EXPORT void setReferrer(const WebString&, WebReferrerPolicy);

    BLINK_EXPORT WebString target() const;
    BLINK_EXPORT void setTarget(const WebString&);

    BLINK_EXPORT WebPoint scrollOffset() const;
    BLINK_EXPORT void setScrollOffset(const WebPoint&);

    BLINK_EXPORT float pageScaleFactor() const;
    BLINK_EXPORT void setPageScaleFactor(float);

    BLINK_EXPORT WebVector<WebString> documentState() const;
    BLINK_EXPORT void setDocumentState(const WebVector<WebString>&);

    BLINK_EXPORT long long itemSequenceNumber() const;
    BLINK_EXPORT void setItemSequenceNumber(long long);

    BLINK_EXPORT long long documentSequenceNumber() const;
    BLINK_EXPORT void setDocumentSequenceNumber(long long);

    BLINK_EXPORT WebSerializedScriptValue stateObject() const;
    BLINK_EXPORT void setStateObject(const WebSerializedScriptValue&);

    BLINK_EXPORT WebString httpContentType() const;
    BLINK_EXPORT void setHTTPContentType(const WebString&);

    BLINK_EXPORT WebHTTPBody httpBody() const;
    BLINK_EXPORT void setHTTPBody(const WebHTTPBody&);

    BLINK_EXPORT WebVector<WebHistoryItem> children() const;
    BLINK_EXPORT void setChildren(const WebVector<WebHistoryItem>&);
    BLINK_EXPORT void appendToChildren(const WebHistoryItem&);

    BLINK_EXPORT WebVector<WebString> getReferencedFilePaths() const;

#if BLINK_IMPLEMENTATION
    WebHistoryItem(const WTF::PassRefPtr<WebCore::HistoryItem>&);
    WebHistoryItem& operator=(const WTF::PassRefPtr<WebCore::HistoryItem>&);
    operator WTF::PassRefPtr<WebCore::HistoryItem>() const;
#endif

private:
    void ensureMutable();
    WebPrivatePtr<WebCore::HistoryItem> m_private;
};

} // namespace blink

#endif
