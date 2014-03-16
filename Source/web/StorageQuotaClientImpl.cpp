/*
 * Copyright (C) 2014 Google Inc. All rights reserved.
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
#include "StorageQuotaClientImpl.h"

#include "WebFrameClient.h"
#include "WebFrameImpl.h"
#include "bindings/v8/ScriptPromise.h"
#include "bindings/v8/ScriptPromiseResolver.h"
#include "core/dom/DOMError.h"
#include "core/dom/Document.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/ExecutionContext.h"
#include "modules/quota/DeprecatedStorageQuotaCallbacksImpl.h"
#include "modules/quota/StorageErrorCallback.h"
#include "modules/quota/StorageQuotaCallback.h"
#include "modules/quota/StorageQuotaCallbacksImpl.h"
#include "modules/quota/StorageUsageCallback.h"
#include "public/platform/WebStorageQuotaType.h"
#include "wtf/Threading.h"

using namespace WebCore;

namespace blink {

PassOwnPtr<StorageQuotaClientImpl> StorageQuotaClientImpl::create()
{
    return adoptPtr(new StorageQuotaClientImpl());
}

StorageQuotaClientImpl::~StorageQuotaClientImpl()
{
}

void StorageQuotaClientImpl::requestQuota(ExecutionContext* executionContext, WebStorageQuotaType storageType, unsigned long long newQuotaInBytes, PassOwnPtr<StorageQuotaCallback> successCallback, PassOwnPtr<StorageErrorCallback> errorCallback)
{
    ASSERT(executionContext);

    if (executionContext->isDocument()) {
        Document* document = toDocument(executionContext);
        WebFrameImpl* webFrame = WebFrameImpl::fromFrame(document->frame());
        OwnPtr<StorageQuotaCallbacks> callbacks = DeprecatedStorageQuotaCallbacksImpl::create(successCallback, errorCallback);
        webFrame->client()->requestStorageQuota(webFrame, storageType, newQuotaInBytes, callbacks.release());
    } else {
        // Requesting quota in Worker is not supported.
        executionContext->postTask(StorageErrorCallback::CallbackTask::create(errorCallback, NotSupportedError));
    }
}

ScriptPromise StorageQuotaClientImpl::requestPersistentQuota(ExecutionContext* executionContext, unsigned long long newQuotaInBytes)
{
    ASSERT(executionContext);

    RefPtr<ScriptPromiseResolver> resolver = ScriptPromiseResolver::create(executionContext);
    ScriptPromise promise = resolver->promise();

    if (executionContext->isDocument()) {
        Document* document = toDocument(executionContext);
        WebFrameImpl* webFrame = WebFrameImpl::fromFrame(document->frame());
        OwnPtr<StorageQuotaCallbacks> callbacks = StorageQuotaCallbacksImpl::create(resolver, executionContext);
        webFrame->client()->requestStorageQuota(webFrame, WebStorageQuotaTypePersistent, newQuotaInBytes, callbacks.release());
    } else {
        // Requesting quota in Worker is not supported.
        resolver->reject(DOMError::create(NotSupportedError));
    }

    return promise;
}

StorageQuotaClientImpl::StorageQuotaClientImpl()
{
}

} // namespace blink
