/*
 * Copyright (C) 2008, 2009 Google Inc. All rights reserved.
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
#include "bindings/v8/V8NodeFilterCondition.h"

#include "V8Node.h"
#include "bindings/v8/ScriptController.h"
#include "bindings/v8/ScriptState.h"
#include "bindings/v8/V8HiddenValue.h"
#include "core/dom/Node.h"
#include "core/dom/NodeFilter.h"
#include "wtf/OwnPtr.h"

namespace WebCore {

V8NodeFilterCondition::V8NodeFilterCondition(v8::Handle<v8::Value> filter, v8::Handle<v8::Object> owner, v8::Isolate* isolate)
    : m_filter(isolate, filter)
{
    V8HiddenValue::setHiddenValue(isolate, owner, V8HiddenValue::condition(isolate), filter);
    m_filter.setWeak(this, &setWeakCallback);
}

V8NodeFilterCondition::~V8NodeFilterCondition()
{
}

short V8NodeFilterCondition::acceptNode(ScriptState* state, Node* node) const
{
    v8::Isolate* isolate = state->isolate();
    ASSERT(isolate->InContext());
    v8::HandleScope handleScope(isolate);
    v8::Handle<v8::Value> filter = m_filter.newLocal(isolate);
    ASSERT(!filter.IsEmpty());
    if (!filter->IsObject())
        return NodeFilter::FILTER_ACCEPT;

    v8::TryCatch exceptionCatcher;

    v8::Handle<v8::Function> callback;
    if (filter->IsFunction())
        callback = v8::Handle<v8::Function>::Cast(filter);
    else {
        v8::Local<v8::Value> value = filter->ToObject()->Get(v8AtomicString(isolate, "acceptNode"));
        if (value.IsEmpty() || !value->IsFunction()) {
            throwTypeError("NodeFilter object does not have an acceptNode function", state->isolate());
            return NodeFilter::FILTER_REJECT;
        }
        callback = v8::Handle<v8::Function>::Cast(value);
    }

    OwnPtr<v8::Handle<v8::Value>[]> info = adoptArrayPtr(new v8::Handle<v8::Value>[1]);
    info[0] = toV8(node, v8::Handle<v8::Object>(), state->isolate());

    v8::Handle<v8::Object> object = isolate->GetCurrentContext()->Global();
    v8::Handle<v8::Value> result = ScriptController::callFunction(state->executionContext(), callback, object, 1, info.get(), isolate);

    if (exceptionCatcher.HasCaught()) {
        state->setException(exceptionCatcher.Exception());
        return NodeFilter::FILTER_REJECT;
    }

    ASSERT(!result.IsEmpty());

    return result->Int32Value();
}

void V8NodeFilterCondition::setWeakCallback(const v8::WeakCallbackData<v8::Value, V8NodeFilterCondition>& data)
{
    data.GetParameter()->m_filter.clear();
}

} // namespace WebCore
