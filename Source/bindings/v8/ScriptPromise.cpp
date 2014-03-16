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
#include "bindings/v8/ScriptPromise.h"

#include "bindings/v8/V8Binding.h"
#include "bindings/v8/V8DOMWrapper.h"
#include "bindings/v8/custom/V8PromiseCustom.h"

#include <v8.h>

namespace WebCore {

ScriptPromise::ScriptPromise(v8::Handle<v8::Value> value, v8::Isolate* isolate)
{
    if (value.IsEmpty() || !V8PromiseCustom::isPromise(value, isolate)) {
        m_promise = ScriptValue(v8::Handle<v8::Value>(), isolate);
        V8ThrowException::throwTypeError("the given value is not a Promise", isolate);
        return;
    }
    m_promise = ScriptValue(value, isolate);
}

ScriptPromise ScriptPromise::then(PassOwnPtr<ScriptFunction> onFulfilled, PassOwnPtr<ScriptFunction> onRejected)
{
    if (m_promise.hasNoValue() || !m_promise.isObject())
        return ScriptPromise();
    v8::Handle<v8::Object> promise = m_promise.v8Value().As<v8::Object>();
    return ScriptPromise(V8PromiseCustom::then(promise, adoptByGarbageCollector(onFulfilled), adoptByGarbageCollector(onRejected), isolate()), isolate());
}

ScriptPromise ScriptPromise::createPending()
{
    v8::Isolate* isolate = v8::Isolate::GetCurrent();
    ASSERT(isolate->InContext());
    v8::Handle<v8::Object> promise = V8PromiseCustom::createPromise(v8::Object::New(isolate), isolate);
    return ScriptPromise(promise, isolate);
}

ScriptPromise ScriptPromise::createPending(ExecutionContext* context)
{
    ASSERT(context);
    v8::Isolate* isolate = toIsolate(context);
    ASSERT(isolate->InContext());
    v8::Handle<v8::Context> v8Context = toV8Context(context, DOMWrapperWorld::current(isolate));
    v8::Handle<v8::Object> creationContext = v8Context.IsEmpty() ? v8::Object::New(isolate) : v8Context->Global();
    v8::Handle<v8::Object> promise = V8PromiseCustom::createPromise(creationContext, isolate);
    return ScriptPromise(promise, isolate);
}

ScriptPromise ScriptPromise::cast(const ScriptValue& value)
{
    if (value.hasNoValue())
        return ScriptPromise();
    v8::Local<v8::Value> v8Value(value.v8Value());
    v8::Isolate* isolate = value.isolate();
    if (V8PromiseCustom::isPromise(v8Value, isolate)) {
        return ScriptPromise(v8Value, isolate);
    }
    return ScriptPromise(V8PromiseCustom::toPromise(v8Value, isolate), isolate);
}

} // namespace WebCore
