/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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
#include "bindings/v8/ScriptPromiseResolver.h"

#include "bindings/v8/ScriptState.h"
#include "bindings/v8/ScriptValue.h"
#include "bindings/v8/V8Binding.h"
#include "bindings/v8/V8DOMWrapper.h"
#include "bindings/v8/custom/V8PromiseCustom.h"

#include <v8.h>

namespace WebCore {

ScriptPromiseResolver::ScriptPromiseResolver(ExecutionContext* context)
    : m_isolate(toIsolate(context))
    , m_promise(ScriptPromise::createPending(context))
{
}

ScriptPromiseResolver::ScriptPromiseResolver(v8::Isolate* isolate)
    : m_isolate(isolate)
    , m_promise(ScriptPromise::createPending(isolate))
{
}

ScriptPromiseResolver::~ScriptPromiseResolver()
{
    // We don't call "reject" here because it requires a caller
    // to be in a v8 context.

    m_promise.clear();
}

PassRefPtr<ScriptPromiseResolver> ScriptPromiseResolver::create(ExecutionContext* context)
{
    ASSERT(context);
    ASSERT(toIsolate(context)->InContext());
    return adoptRef(new ScriptPromiseResolver(context));
}

PassRefPtr<ScriptPromiseResolver> ScriptPromiseResolver::create(v8::Isolate* isolate)
{
    ASSERT(isolate->InContext());
    return adoptRef(new ScriptPromiseResolver(isolate));
}

bool ScriptPromiseResolver::isPending() const
{
    ASSERT(m_isolate->InContext());
    if (m_promise.hasNoValue())
        return false;
    v8::Local<v8::Object> promise = m_promise.v8Value().As<v8::Object>();
    v8::Local<v8::Object> internal = V8PromiseCustom::getInternal(promise);
    V8PromiseCustom::PromiseState state = V8PromiseCustom::getState(internal);
    return state == V8PromiseCustom::Pending;
}

void ScriptPromiseResolver::resolve(v8::Handle<v8::Value> value)
{
    ASSERT(m_isolate->InContext());
    if (!isPending())
        return;
    V8PromiseCustom::resolve(m_promise.v8Value().As<v8::Object>(), value, m_isolate);
    m_promise.clear();
}

void ScriptPromiseResolver::reject(v8::Handle<v8::Value> value)
{
    ASSERT(m_isolate->InContext());
    if (!isPending())
        return;
    V8PromiseCustom::reject(m_promise.v8Value().As<v8::Object>(), value, m_isolate);
    m_promise.clear();
}

void ScriptPromiseResolver::resolve(ScriptValue value)
{
    ASSERT(m_isolate->InContext());
    resolve(value.v8Value());
}

void ScriptPromiseResolver::reject(ScriptValue value)
{
    ASSERT(m_isolate->InContext());
    reject(value.v8Value());
}

} // namespace WebCore
