/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 * Copyright (C) 2012 Motorola Mobility Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "core/dom/DOMURL.h"

#include "bindings/v8/ExceptionMessages.h"
#include "bindings/v8/ExceptionState.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/ExecutionContext.h"
#include "core/fetch/MemoryCache.h"
#include "core/fileapi/Blob.h"
#include "core/html/PublicURLManager.h"
#include "platform/blob/BlobURL.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "wtf/MainThread.h"

namespace WebCore {

DOMURL::DOMURL(const String& url, const KURL& base, ExceptionState& exceptionState)
{
    ScriptWrappable::init(this);
    if (!base.isValid())
        exceptionState.throwDOMException(SyntaxError, "Invalid base URL");

    m_url = KURL(base, url);
    if (!m_url.isValid())
        exceptionState.throwDOMException(SyntaxError, "Invalid URL");
}

void DOMURL::setInput(const String& value)
{
    KURL url(blankURL(), value);
    if (url.isValid()) {
        m_url = url;
        m_input = String();
    } else {
        m_url = KURL();
        m_input = value;
    }
}

String DOMURL::createObjectURL(ExecutionContext* executionContext, Blob* blob, ExceptionState& exceptionState)
{
    if (!executionContext || !blob)
        return String();
    if (blob->hasBeenClosed()) {
        exceptionState.throwDOMException(InvalidStateError, String(blob->isFile() ? "File" : "Blob") + " has been closed.");
        return String();
    }
    return createPublicURL(executionContext, blob, blob->uuid());
}

String DOMURL::createPublicURL(ExecutionContext* executionContext, URLRegistrable* registrable, const String& uuid)
{
    KURL publicURL = BlobURL::createPublicURL(executionContext->securityOrigin());
    if (publicURL.isEmpty())
        return String();

    executionContext->publicURLManager().registerURL(executionContext->securityOrigin(), publicURL, registrable, uuid);

    return publicURL.string();
}

void DOMURL::revokeObjectURL(ExecutionContext* executionContext, const String& urlString)
{
    if (!executionContext)
        return;

    KURL url(KURL(), urlString);
    MemoryCache::removeURLFromCache(executionContext, url);
    executionContext->publicURLManager().revoke(url);
}

void DOMURL::revokeObjectUUID(ExecutionContext* executionContext, const String& uuid)
{
    if (!executionContext)
        return;

    executionContext->publicURLManager().revoke(uuid);
}

} // namespace WebCore
