/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "bindings/v8/V8MutationCallback.h"

#include "V8MutationObserver.h"
#include "V8MutationRecord.h"
#include "bindings/v8/ScriptController.h"
#include "bindings/v8/V8Binding.h"
#include "bindings/v8/V8HiddenValue.h"
#include "core/dom/ExecutionContext.h"
#include "wtf/Assertions.h"

namespace WebCore {

V8MutationCallback::V8MutationCallback(v8::Handle<v8::Function> callback, ExecutionContext* context, v8::Handle<v8::Object> owner, v8::Isolate* isolate)
    : ActiveDOMCallback(context)
    , m_callback(isolate, callback)
    , m_world(DOMWrapperWorld::current(isolate))
    , m_isolate(isolate)
{
    V8HiddenValue::setHiddenValue(m_isolate, owner, V8HiddenValue::callback(m_isolate), callback);
    m_callback.setWeak(this, &setWeakCallback);
}

void V8MutationCallback::call(const Vector<RefPtr<MutationRecord> >& mutations, MutationObserver* observer)
{
    if (!canInvokeCallback())
        return;

    v8::HandleScope handleScope(m_isolate);

    v8::Handle<v8::Context> v8Context = toV8Context(executionContext(), m_world.get());
    if (v8Context.IsEmpty())
        return;

    v8::Context::Scope scope(v8Context);

    v8::Handle<v8::Function> callback = m_callback.newLocal(m_isolate);
    if (callback.IsEmpty())
        return;

    v8::Handle<v8::Value> observerHandle = toV8(observer, v8::Handle<v8::Object>(), m_isolate);
    if (observerHandle.IsEmpty()) {
        if (!isScriptControllerTerminating())
            CRASH();
        return;
    }

    if (!observerHandle->IsObject())
        return;

    v8::Handle<v8::Object> thisObject = v8::Handle<v8::Object>::Cast(observerHandle);
    v8::Handle<v8::Value> argv[] = { v8Array(mutations, m_isolate), observerHandle };

    v8::TryCatch exceptionCatcher;
    exceptionCatcher.SetVerbose(true);
    ScriptController::callFunction(executionContext(), callback, thisObject, 2, argv, m_isolate);
}

void V8MutationCallback::setWeakCallback(const v8::WeakCallbackData<v8::Function, V8MutationCallback>& data)
{
    data.GetParameter()->m_callback.clear();
}

} // namespace WebCore
