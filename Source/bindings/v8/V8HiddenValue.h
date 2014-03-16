// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8HiddenValue_h
#define V8HiddenValue_h

#include "bindings/v8/ScopedPersistent.h"
#include <v8.h>

namespace WebCore {

class ScriptWrappable;

#define V8_HIDDEN_VALUES(V) \
    V(arrayBufferData) \
    V(customElementAttached) \
    V(customElementAttributeChanged) \
    V(customElementCreated) \
    V(customElementDetached) \
    V(customElementDocument) \
    V(customElementIsInterfacePrototypeObject) \
    V(customElementNamespaceURI) \
    V(customElementTagName) \
    V(customElementType) \
    V(callback) \
    V(condition) \
    V(data) \
    V(detail) \
    V(document) \
    V(error) \
    V(event) \
    V(idbCursorRequest) \
    V(port1) \
    V(port2) \
    V(state) \
    V(stringData) \
    V(scriptState) \
    V(thenableHiddenPromise) \
    V(toStringString)

class V8HiddenValue {
public:
#define V8_DECLARE_METHOD(name) static v8::Handle<v8::String> name(v8::Isolate* isolate);
    V8_HIDDEN_VALUES(V8_DECLARE_METHOD);
#undef V8_DECLARE_METHOD

    static v8::Local<v8::Value> getHiddenValue(v8::Isolate*, v8::Handle<v8::Object>, v8::Handle<v8::String>);
    static bool setHiddenValue(v8::Isolate*, v8::Handle<v8::Object>, v8::Handle<v8::String>, v8::Handle<v8::Value>);
    static bool deleteHiddenValue(v8::Isolate*, v8::Handle<v8::Object>, v8::Handle<v8::String>);
    static v8::Local<v8::Value> getHiddenValueFromMainWorldWrapper(v8::Isolate*, ScriptWrappable*, v8::Handle<v8::String>);

private:
#define V8_DECLARE_FIELD(name) ScopedPersistent<v8::String> m_##name;
    V8_HIDDEN_VALUES(V8_DECLARE_FIELD);
#undef V8_DECLARE_FIELD
};

} // namespace WebCore

#endif // V8HiddenValue_h
