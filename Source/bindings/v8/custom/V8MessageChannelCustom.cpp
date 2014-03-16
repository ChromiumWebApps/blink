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

#include "config.h"
#include "V8MessageChannel.h"

#include "V8MessagePort.h"
#include "bindings/v8/V8Binding.h"
#include "bindings/v8/V8HiddenValue.h"
#include "core/dom/MessageChannel.h"
#include "core/workers/WorkerGlobalScope.h"
#include "wtf/RefPtr.h"

namespace WebCore {

void V8MessageChannel::constructorCustom(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    ExecutionContext* context = currentExecutionContext(info.GetIsolate());

    RefPtr<MessageChannel> obj = MessageChannel::create(context);

    v8::Local<v8::Object> wrapper = info.Holder();

    // Create references from the MessageChannel wrapper to the two
    // MessagePort wrappers to make sure that the MessagePort wrappers
    // stay alive as long as the MessageChannel wrapper is around.
    V8HiddenValue::setHiddenValue(info.GetIsolate(), wrapper, V8HiddenValue::port1(info.GetIsolate()), toV8(obj->port1(), info.Holder(), info.GetIsolate()));
    V8HiddenValue::setHiddenValue(info.GetIsolate(), wrapper, V8HiddenValue::port2(info.GetIsolate()), toV8(obj->port2(), info.Holder(), info.GetIsolate()));

    V8DOMWrapper::associateObjectWithWrapper<V8MessageChannel>(obj.release(), &wrapperTypeInfo, wrapper, info.GetIsolate(), WrapperConfiguration::Dependent);
    info.GetReturnValue().Set(wrapper);
}

} // namespace WebCore
