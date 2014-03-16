// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/serviceworkers/Cache.h"

#include "bindings/v8/ScriptPromise.h"
#include "bindings/v8/ScriptPromiseResolver.h"
#include "core/dom/DOMError.h"
#include "core/dom/ExceptionCode.h"
#include "platform/NotImplemented.h"

namespace WebCore {

PassRefPtr<Cache> Cache::create(const Vector<String>& urlStrings)
{
    RefPtr<Cache> cache = adoptRef(new Cache);
    // FIXME: Implement.
    notImplemented();
    return cache;
}

Cache::~Cache()
{
}

ScriptPromise Cache::match(ExecutionContext* executionContext, const String& urlString)
{
    RefPtr<ScriptPromiseResolver> resolver = ScriptPromiseResolver::create(executionContext);
    ScriptPromise promise = resolver->promise();

    // FIXME: Implement.
    notImplemented();
    resolver->reject(DOMError::create(NotSupportedError));
    return promise;
}

ScriptPromise Cache::ready(ExecutionContext* executionContext)
{
    RefPtr<ScriptPromiseResolver> resolver = ScriptPromiseResolver::create(executionContext);
    ScriptPromise promise = resolver->promise();

    // FIXME: Implement.
    notImplemented();
    resolver->reject(DOMError::create(NotSupportedError));
    return promise;
}

Cache::Cache()
{
    ScriptWrappable::init(this);
}

} // namespace WebCore
