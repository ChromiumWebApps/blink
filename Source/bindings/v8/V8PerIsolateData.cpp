/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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
#include "bindings/v8/V8PerIsolateData.h"

#include "bindings/v8/DOMDataStore.h"
#include "bindings/v8/ScriptGCEvent.h"
#include "bindings/v8/ScriptProfiler.h"
#include "bindings/v8/V8Binding.h"
#include "bindings/v8/V8HiddenValue.h"
#include "bindings/v8/V8ObjectConstructor.h"
#include "bindings/v8/V8PerContextData.h"
#include "bindings/v8/V8ScriptRunner.h"
#include "wtf/MainThread.h"

namespace WebCore {

static V8PerIsolateData* mainThreadPerIsolateData = 0;

V8PerIsolateData::V8PerIsolateData(v8::Isolate* isolate)
    : m_isolate(isolate)
    , m_isolateHolder(adoptPtr(new gin::IsolateHolder(m_isolate, v8ArrayBufferAllocator())))
    , m_stringCache(adoptPtr(new StringCache()))
    , m_hiddenValue(adoptPtr(new V8HiddenValue()))
    , m_constructorMode(ConstructorMode::CreateNewObject)
    , m_recursionLevel(0)
#ifndef NDEBUG
    , m_internalScriptRecursionLevel(0)
#endif
    , m_gcEventData(adoptPtr(new GCEventData()))
    , m_performingMicrotaskCheckpoint(false)
{
    if (isMainThread())
        mainThreadPerIsolateData = this;
}

V8PerIsolateData::~V8PerIsolateData()
{
    if (isMainThread())
        mainThreadPerIsolateData = 0;
}

v8::Isolate* V8PerIsolateData::mainThreadIsolate()
{
    ASSERT(isMainThread());
    ASSERT(mainThreadPerIsolateData);
    return mainThreadPerIsolateData->isolate();
}

void V8PerIsolateData::ensureInitialized(v8::Isolate* isolate)
{
    ASSERT(isolate);
    if (!isolate->GetData(gin::kEmbedderBlink)) {
        V8PerIsolateData* data = new V8PerIsolateData(isolate);
        isolate->SetData(gin::kEmbedderBlink, data);
    }
}

v8::Persistent<v8::Value>& V8PerIsolateData::ensureLiveRoot()
{
    if (m_liveRoot.isEmpty())
        m_liveRoot.set(m_isolate, v8::Null(m_isolate));
    return m_liveRoot.getUnsafe();
}

void V8PerIsolateData::dispose(v8::Isolate* isolate)
{
    void* data = isolate->GetData(gin::kEmbedderBlink);
    delete static_cast<V8PerIsolateData*>(data);
    isolate->SetData(gin::kEmbedderBlink, 0);
}

V8PerIsolateData::DOMTemplateMap& V8PerIsolateData::currentDOMTemplateMap()
{
    if (DOMWrapperWorld::current(m_isolate)->isMainWorld())
        return m_domTemplateMapForMainWorld;
    return m_domTemplateMapForNonMainWorld;
}

v8::Handle<v8::FunctionTemplate> V8PerIsolateData::domTemplate(void* domTemplateKey, v8::FunctionCallback callback, v8::Handle<v8::Value> data, v8::Handle<v8::Signature> signature, int length)
{
    DOMTemplateMap& domTemplateMap = currentDOMTemplateMap();
    DOMTemplateMap::iterator result = domTemplateMap.find(domTemplateKey);
    if (result != domTemplateMap.end())
        return result->value.newLocal(m_isolate);
    v8::Local<v8::FunctionTemplate> templ = v8::FunctionTemplate::New(m_isolate, callback, data, signature, length);
    domTemplateMap.add(domTemplateKey, UnsafePersistent<v8::FunctionTemplate>(m_isolate, templ));
    return templ;
}

v8::Handle<v8::FunctionTemplate> V8PerIsolateData::existingDOMTemplate(void* domTemplateKey)
{
    DOMTemplateMap& domTemplateMap = currentDOMTemplateMap();
    DOMTemplateMap::iterator result = domTemplateMap.find(domTemplateKey);
    if (result != domTemplateMap.end())
        return result->value.newLocal(m_isolate);
    return v8::Local<v8::FunctionTemplate>();
}

void V8PerIsolateData::setDOMTemplate(void* domTemplateKey, v8::Handle<v8::FunctionTemplate> templ)
{
    currentDOMTemplateMap().add(domTemplateKey, UnsafePersistent<v8::FunctionTemplate>(m_isolate, templ));
}

v8::Local<v8::Context> V8PerIsolateData::ensureRegexContext()
{
    if (!m_perContextDataForRegex)
        m_perContextDataForRegex = V8PerContextData::create(v8::Context::New(m_isolate), DOMWrapperWorld::create());
    return m_perContextDataForRegex->context();
}

bool V8PerIsolateData::hasInstance(const WrapperTypeInfo* info, v8::Handle<v8::Value> value)
{
    return hasInstance(info, value, m_domTemplateMapForMainWorld)
        || hasInstance(info, value, m_domTemplateMapForNonMainWorld);
}

bool V8PerIsolateData::hasInstance(const WrapperTypeInfo* info, v8::Handle<v8::Value> value, DOMTemplateMap& domTemplateMap)
{
    DOMTemplateMap::iterator result = domTemplateMap.find(info);
    if (result == domTemplateMap.end())
        return false;
    v8::Handle<v8::FunctionTemplate> templ = result->value.newLocal(m_isolate);
    return templ->HasInstance(value);
}

v8::Handle<v8::Object> V8PerIsolateData::findInstanceInPrototypeChain(const WrapperTypeInfo* info, v8::Handle<v8::Value> value)
{
    v8::Handle<v8::Object> wrapper = findInstanceInPrototypeChain(info, value, m_domTemplateMapForMainWorld);
    if (!wrapper.IsEmpty())
        return wrapper;
    return findInstanceInPrototypeChain(info, value, m_domTemplateMapForNonMainWorld);
}

v8::Handle<v8::Object> V8PerIsolateData::findInstanceInPrototypeChain(const WrapperTypeInfo* info, v8::Handle<v8::Value> value, DOMTemplateMap& domTemplateMap)
{
    if (value.IsEmpty() || !value->IsObject())
        return v8::Handle<v8::Object>();
    DOMTemplateMap::iterator result = domTemplateMap.find(info);
    if (result == domTemplateMap.end())
        return v8::Handle<v8::Object>();
    v8::Handle<v8::FunctionTemplate> templ = result->value.newLocal(m_isolate);
    return v8::Handle<v8::Object>::Cast(value)->FindInstanceInPrototypeChain(templ);
}

static void constructorOfToString(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    // The DOM constructors' toString functions grab the current toString
    // for Functions by taking the toString function of itself and then
    // calling it with the constructor as its receiver. This means that
    // changes to the Function prototype chain or toString function are
    // reflected when printing DOM constructors. The only wart is that
    // changes to a DOM constructor's toString's toString will cause the
    // toString of the DOM constructor itself to change. This is extremely
    // obscure and unlikely to be a problem.
    v8::Handle<v8::Value> value = info.Callee()->Get(v8AtomicString(info.GetIsolate(), "toString"));
    if (!value->IsFunction()) {
        v8SetReturnValue(info, v8::String::Empty(info.GetIsolate()));
        return;
    }
    v8SetReturnValue(info, V8ScriptRunner::callInternalFunction(v8::Handle<v8::Function>::Cast(value), info.This(), 0, 0, v8::Isolate::GetCurrent()));
}

v8::Handle<v8::FunctionTemplate> V8PerIsolateData::toStringTemplate()
{
    if (m_toStringTemplate.isEmpty())
        m_toStringTemplate.set(m_isolate, v8::FunctionTemplate::New(m_isolate, constructorOfToString));
    return m_toStringTemplate.newLocal(m_isolate);
}

} // namespace WebCore
