/*
 * Copyright (C) 2007 Apple Inc.  All rights reserved.
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
#include "core/frame/History.h"

#include "bindings/v8/ExceptionState.h"
#include "core/dom/Document.h"
#include "core/dom/ExceptionCode.h"
#include "core/frame/LocalFrame.h"
#include "core/loader/DocumentLoader.h"
#include "core/loader/FrameLoader.h"
#include "core/loader/FrameLoaderClient.h"
#include "core/loader/HistoryItem.h"
#include "core/page/BackForwardClient.h"
#include "core/page/Page.h"
#include "platform/weborigin/KURL.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "wtf/MainThread.h"

namespace WebCore {

History::History(LocalFrame* frame)
    : DOMWindowProperty(frame)
    , m_lastStateObjectRequested(nullptr)
{
    ScriptWrappable::init(this);
}

unsigned History::length() const
{
    if (!m_frame)
        return 0;
    if (!m_frame->page())
        return 0;
    return m_frame->page()->backForward().backForwardListCount();
}

SerializedScriptValue* History::state()
{
    m_lastStateObjectRequested = stateInternal();
    return m_lastStateObjectRequested.get();
}

SerializedScriptValue* History::stateInternal() const
{
    if (!m_frame)
        return 0;

    if (HistoryItem* historyItem = m_frame->loader().currentItem())
        return historyItem->stateObject();

    return 0;
}

bool History::stateChanged() const
{
    return m_lastStateObjectRequested != stateInternal();
}

bool History::isSameAsCurrentState(SerializedScriptValue* state) const
{
    return state == stateInternal();
}

void History::back(ExecutionContext* context)
{
    go(context, -1);
}

void History::forward(ExecutionContext* context)
{
    go(context, 1);
}

void History::go(ExecutionContext* context, int distance)
{
    if (!m_frame)
        return;

    ASSERT(isMainThread());
    Document* activeDocument = toDocument(context);
    if (!activeDocument)
        return;

    if (!activeDocument->canNavigate(m_frame))
        return;

    m_frame->navigationScheduler().scheduleHistoryNavigation(distance);
}

KURL History::urlForState(const String& urlString)
{
    Document* document = m_frame->document();

    if (urlString.isNull())
        return document->url();
    if (urlString.isEmpty())
        return document->baseURL();

    return KURL(document->baseURL(), urlString);
}

void History::stateObjectAdded(PassRefPtr<SerializedScriptValue> data, const String& /* title */, const String& urlString, UpdateBackForwardListPolicy updateBackForwardListPolicy, ExceptionState& exceptionState)
{
    if (!m_frame || !m_frame->page())
        return;

    KURL fullURL = urlForState(urlString);
    if (!fullURL.isValid() || !m_frame->document()->securityOrigin()->canRequest(fullURL)) {
        // We can safely expose the URL to JavaScript, as a) no redirection takes place: JavaScript already had this URL, b) JavaScript can only access a same-origin History object.
        exceptionState.throwSecurityError("A history state object with URL '" + fullURL.elidedString() + "' cannot be created in a document with origin '" + m_frame->document()->securityOrigin()->toString() + "'.");
        return;
    }
    m_frame->loader().updateForSameDocumentNavigation(fullURL, SameDocumentNavigationHistoryApi, data, updateBackForwardListPolicy);
}

} // namespace WebCore
