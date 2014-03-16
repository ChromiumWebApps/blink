/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#include "config.h"
#include "WebDOMActivityLogger.h"

#include "bindings/v8/V8Binding.h"
#include "bindings/v8/V8DOMActivityLogger.h"
#include "core/dom/Document.h"
#include "core/frame/DOMWindow.h"
#include "wtf/PassRefPtr.h"
#include "wtf/text/WTFString.h"

using namespace WebCore;

namespace blink {

class DOMActivityLoggerContainer : public V8DOMActivityLogger {
public:
    explicit DOMActivityLoggerContainer(PassOwnPtr<WebDOMActivityLogger> logger)
        : m_domActivityLogger(logger)
    {
    }

    virtual void log(const String& apiName, int argc, const v8::Handle<v8::Value>* argv, const String& extraInfo) OVERRIDE
    {
        KURL url;
        String title;
        if (Document* document = currentDOMWindow(v8::Isolate::GetCurrent())->document()) {
            url = document->url();
            title = document->title();
        }
        m_domActivityLogger->log(WebString(apiName), argc, argv, WebString(extraInfo), WebURL(url), WebString(title));
    }

private:
    OwnPtr<WebDOMActivityLogger> m_domActivityLogger;
};

bool hasDOMActivityLogger(int worldId)
{
    return V8DOMActivityLogger::activityLogger(worldId);
}

void setDOMActivityLogger(int worldId, WebDOMActivityLogger* logger)
{
    ASSERT(logger);
    V8DOMActivityLogger::setActivityLogger(worldId, adoptPtr(new DOMActivityLoggerContainer(adoptPtr(logger))));
}

} // namespace blink
