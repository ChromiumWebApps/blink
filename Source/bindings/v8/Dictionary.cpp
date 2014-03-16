/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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
#include "bindings/v8/Dictionary.h"

#include "V8DOMError.h"
#include "V8EventTarget.h"
#include "V8IDBKeyRange.h"
#include "V8MIDIPort.h"
#include "V8MediaKeyError.h"
#include "V8MessagePort.h"
#include "V8SpeechRecognitionError.h"
#include "V8SpeechRecognitionResult.h"
#include "V8SpeechRecognitionResultList.h"
#include "V8Storage.h"
#include "V8VoidCallback.h"
#include "V8Window.h"
#include "bindings/v8/ArrayValue.h"
#include "bindings/v8/ExceptionMessages.h"
#include "bindings/v8/ExceptionState.h"
#include "bindings/v8/V8Binding.h"
#include "bindings/v8/custom/V8ArrayBufferViewCustom.h"
#include "bindings/v8/custom/V8Uint8ArrayCustom.h"
#include "modules/indexeddb/IDBKeyRange.h"
#include "modules/speech/SpeechRecognitionError.h"
#include "modules/speech/SpeechRecognitionResult.h"
#include "modules/speech/SpeechRecognitionResultList.h"
#include "wtf/MathExtras.h"

#include "V8TextTrack.h"
#include "core/html/track/TrackBase.h"

#include "V8MediaStream.h"
#include "modules/mediastream/MediaStream.h"

namespace WebCore {

Dictionary::Dictionary()
    : m_isolate(0)
{
}

Dictionary::Dictionary(const v8::Handle<v8::Value>& options, v8::Isolate* isolate)
    : m_options(options)
    , m_isolate(isolate)
{
    ASSERT(m_isolate);
}

Dictionary::~Dictionary()
{
}

Dictionary& Dictionary::operator=(const Dictionary& optionsObject)
{
    m_options = optionsObject.m_options;
    m_isolate = optionsObject.m_isolate;
    return *this;
}

bool Dictionary::isObject() const
{
    return !isUndefinedOrNull() && m_options->IsObject();
}

bool Dictionary::isUndefinedOrNull() const
{
    if (m_options.IsEmpty())
        return true;
    return WebCore::isUndefinedOrNull(m_options);
}

bool Dictionary::hasProperty(const String& key) const
{
    if (isUndefinedOrNull())
        return false;
    v8::Local<v8::Object> options = m_options->ToObject();
    ASSERT(!options.IsEmpty());

    ASSERT(m_isolate);
    ASSERT(m_isolate == v8::Isolate::GetCurrent());
    v8::Handle<v8::String> v8Key = v8String(m_isolate, key);
    if (!options->Has(v8Key))
        return false;

    return true;
}

bool Dictionary::getKey(const String& key, v8::Local<v8::Value>& value) const
{
    if (isUndefinedOrNull())
        return false;
    v8::Local<v8::Object> options = m_options->ToObject();
    ASSERT(!options.IsEmpty());

    ASSERT(m_isolate);
    ASSERT(m_isolate == v8::Isolate::GetCurrent());
    v8::Handle<v8::String> v8Key = v8String(m_isolate, key);
    if (!options->Has(v8Key))
        return false;
    value = options->Get(v8Key);
    if (value.IsEmpty())
        return false;
    return true;
}

bool Dictionary::get(const String& key, v8::Local<v8::Value>& value) const
{
    return getKey(key, value);
}

bool Dictionary::get(const String& key, bool& value) const
{
    v8::Local<v8::Value> v8Value;
    if (!getKey(key, v8Value))
        return false;

    v8::Local<v8::Boolean> v8Bool = v8Value->ToBoolean();
    if (v8Bool.IsEmpty())
        return false;
    value = v8Bool->Value();
    return true;
}

bool Dictionary::convert(ConversionContext& context, const String& key, bool& value) const
{
    ConversionContextScope scope(context);
    get(key, value);
    return true;
}

bool Dictionary::get(const String& key, int32_t& value) const
{
    v8::Local<v8::Value> v8Value;
    if (!getKey(key, v8Value))
        return false;

    v8::Local<v8::Int32> v8Int32 = v8Value->ToInt32();
    if (v8Int32.IsEmpty())
        return false;
    value = v8Int32->Value();
    return true;
}

bool Dictionary::get(const String& key, double& value, bool& hasValue) const
{
    v8::Local<v8::Value> v8Value;
    if (!getKey(key, v8Value)) {
        hasValue = false;
        return false;
    }

    hasValue = true;
    V8TRYCATCH_RETURN(v8::Local<v8::Number>, v8Number, v8Value->ToNumber(), false);
    if (v8Number.IsEmpty())
        return false;
    value = v8Number->Value();
    return true;
}

bool Dictionary::get(const String& key, double& value) const
{
    bool unused;
    return get(key, value, unused);
}

bool Dictionary::convert(ConversionContext& context, const String& key, double& value) const
{
    ConversionContextScope scope(context);

    bool hasValue = false;
    if (!get(key, value, hasValue) && hasValue) {
        context.throwTypeError(ExceptionMessages::incorrectPropertyType(key, "is not of type 'double'."));
        return false;
    }
    return true;
}

template<typename StringType>
inline bool Dictionary::getStringType(const String& key, StringType& value) const
{
    v8::Local<v8::Value> v8Value;
    if (!getKey(key, v8Value))
        return false;

    V8TRYCATCH_FOR_V8STRINGRESOURCE_RETURN(V8StringResource<>, stringValue, v8Value, false);
    value = stringValue;
    return true;
}

bool Dictionary::get(const String& key, String& value) const
{
    return getStringType(key, value);
}

bool Dictionary::get(const String& key, AtomicString& value) const
{
    return getStringType(key, value);
}

bool Dictionary::convert(ConversionContext& context, const String& key, String& value) const
{
    ConversionContextScope scope(context);

    v8::Local<v8::Value> v8Value;
    if (!getKey(key, v8Value))
        return true;

    V8TRYCATCH_FOR_V8STRINGRESOURCE_RETURN(V8StringResource<>, stringValue, v8Value, false);
    value = stringValue;
    return true;
}

bool Dictionary::get(const String& key, ScriptValue& value) const
{
    v8::Local<v8::Value> v8Value;
    if (!getKey(key, v8Value))
        return false;

    value = ScriptValue(v8Value, m_isolate);
    return true;
}

bool Dictionary::convert(ConversionContext& context, const String& key, ScriptValue& value) const
{
    ConversionContextScope scope(context);

    get(key, value);
    return true;
}

bool Dictionary::get(const String& key, unsigned short& value) const
{
    v8::Local<v8::Value> v8Value;
    if (!getKey(key, v8Value))
        return false;

    v8::Local<v8::Int32> v8Int32 = v8Value->ToInt32();
    if (v8Int32.IsEmpty())
        return false;
    value = static_cast<unsigned short>(v8Int32->Value());
    return true;
}

bool Dictionary::get(const String& key, short& value) const
{
    v8::Local<v8::Value> v8Value;
    if (!getKey(key, v8Value))
        return false;

    v8::Local<v8::Int32> v8Int32 = v8Value->ToInt32();
    if (v8Int32.IsEmpty())
        return false;
    value = static_cast<short>(v8Int32->Value());
    return true;
}

bool Dictionary::get(const String& key, unsigned& value) const
{
    v8::Local<v8::Value> v8Value;
    if (!getKey(key, v8Value))
        return false;

    v8::Local<v8::Int32> v8Int32 = v8Value->ToInt32();
    if (v8Int32.IsEmpty())
        return false;
    value = static_cast<unsigned>(v8Int32->Value());
    return true;
}

bool Dictionary::get(const String& key, unsigned long& value) const
{
    v8::Local<v8::Value> v8Value;
    if (!getKey(key, v8Value))
        return false;

    v8::Local<v8::Integer> v8Integer = v8Value->ToInteger();
    if (v8Integer.IsEmpty())
        return false;
    value = static_cast<unsigned long>(v8Integer->Value());
    return true;
}

bool Dictionary::get(const String& key, unsigned long long& value) const
{
    v8::Local<v8::Value> v8Value;
    if (!getKey(key, v8Value))
        return false;

    V8TRYCATCH_RETURN(v8::Local<v8::Number>, v8Number, v8Value->ToNumber(), false);
    if (v8Number.IsEmpty())
        return false;
    double d = v8Number->Value();
    doubleToInteger(d, value);
    return true;
}

bool Dictionary::get(const String& key, RefPtr<DOMWindow>& value) const
{
    v8::Local<v8::Value> v8Value;
    if (!getKey(key, v8Value))
        return false;

    // We need to handle a DOMWindow specially, because a DOMWindow wrapper
    // exists on a prototype chain of v8Value.
    value = toDOMWindow(v8Value, m_isolate);
    return true;
}

bool Dictionary::get(const String& key, RefPtrWillBeMember<Storage>& value) const
{
    v8::Local<v8::Value> v8Value;
    if (!getKey(key, v8Value))
        return false;

    value = V8Storage::toNativeWithTypeCheck(m_isolate, v8Value);
    return true;
}

bool Dictionary::get(const String& key, MessagePortArray& value) const
{
    v8::Local<v8::Value> v8Value;
    if (!getKey(key, v8Value))
        return false;

    ASSERT(m_isolate);
    ASSERT(m_isolate == v8::Isolate::GetCurrent());
    if (WebCore::isUndefinedOrNull(v8Value))
        return true;
    bool success = false;
    value = toRefPtrNativeArray<MessagePort, V8MessagePort>(v8Value, key, m_isolate, &success);
    return success;
}

bool Dictionary::convert(ConversionContext& context, const String& key, MessagePortArray& value) const
{
    ConversionContextScope scope(context);

    v8::Local<v8::Value> v8Value;
    if (!getKey(key, v8Value))
        return true;

    return get(key, value);
}

bool Dictionary::get(const String& key, HashSet<AtomicString>& value) const
{
    v8::Local<v8::Value> v8Value;
    if (!getKey(key, v8Value))
        return false;

    // FIXME: Support array-like objects
    if (!v8Value->IsArray())
        return false;

    ASSERT(m_isolate);
    ASSERT(m_isolate == v8::Isolate::GetCurrent());
    v8::Local<v8::Array> v8Array = v8::Local<v8::Array>::Cast(v8Value);
    for (size_t i = 0; i < v8Array->Length(); ++i) {
        v8::Local<v8::Value> indexedValue = v8Array->Get(v8::Integer::New(m_isolate, i));
        V8TRYCATCH_FOR_V8STRINGRESOURCE_RETURN(V8StringResource<>, stringValue, indexedValue, false);
        value.add(stringValue);
    }

    return true;
}

bool Dictionary::convert(ConversionContext& context, const String& key, HashSet<AtomicString>& value) const
{
    ConversionContextScope scope(context);

    v8::Local<v8::Value> v8Value;
    if (!getKey(key, v8Value))
        return true;

    if (context.isNullable() && WebCore::isUndefinedOrNull(v8Value))
        return true;

    if (!v8Value->IsArray()) {
        context.throwTypeError(ExceptionMessages::notASequenceTypeProperty(key));
        return false;
    }

    return get(key, value);
}

bool Dictionary::getWithUndefinedOrNullCheck(const String& key, String& value) const
{
    v8::Local<v8::Value> v8Value;
    if (!getKey(key, v8Value) || WebCore::isUndefinedOrNull(v8Value))
        return false;

    V8TRYCATCH_FOR_V8STRINGRESOURCE_RETURN(V8StringResource<>, stringValue, v8Value, false);
    value = stringValue;
    return true;
}

bool Dictionary::get(const String& key, RefPtr<Uint8Array>& value) const
{
    v8::Local<v8::Value> v8Value;
    if (!getKey(key, v8Value))
        return false;

    value = V8Uint8Array::toNativeWithTypeCheck(m_isolate, v8Value);
    return true;
}

bool Dictionary::get(const String& key, RefPtr<ArrayBufferView>& value) const
{
    v8::Local<v8::Value> v8Value;
    if (!getKey(key, v8Value))
        return false;

    value = V8ArrayBufferView::toNativeWithTypeCheck(m_isolate, v8Value);
    return true;
}

bool Dictionary::get(const String& key, RefPtr<MIDIPort>& value) const
{
    v8::Local<v8::Value> v8Value;
    if (!getKey(key, v8Value))
        return false;

    value = V8MIDIPort::toNativeWithTypeCheck(m_isolate, v8Value);
    return true;
}

bool Dictionary::get(const String& key, RefPtr<MediaKeyError>& value) const
{
    v8::Local<v8::Value> v8Value;
    if (!getKey(key, v8Value))
        return false;

    value = V8MediaKeyError::toNativeWithTypeCheck(m_isolate, v8Value);
    return true;
}

bool Dictionary::get(const String& key, RefPtr<TrackBase>& value) const
{
    v8::Local<v8::Value> v8Value;
    if (!getKey(key, v8Value))
        return false;

    TrackBase* source = 0;
    if (v8Value->IsObject()) {
        v8::Handle<v8::Object> wrapper = v8::Handle<v8::Object>::Cast(v8Value);

        // FIXME: this will need to be changed so it can also return an AudioTrack or a VideoTrack once
        // we add them.
        v8::Handle<v8::Object> track = V8TextTrack::findInstanceInPrototypeChain(wrapper, m_isolate);
        if (!track.IsEmpty())
            source = V8TextTrack::toNative(track);
    }
    value = source;
    return true;
}

bool Dictionary::get(const String& key, RefPtr<SpeechRecognitionError>& value) const
{
    v8::Local<v8::Value> v8Value;
    if (!getKey(key, v8Value))
        return false;

    value = V8SpeechRecognitionError::toNativeWithTypeCheck(m_isolate, v8Value);
    return true;
}

bool Dictionary::get(const String& key, RefPtrWillBeRawPtr<SpeechRecognitionResult>& value) const
{
    v8::Local<v8::Value> v8Value;
    if (!getKey(key, v8Value))
        return false;

    value = V8SpeechRecognitionResult::toNativeWithTypeCheck(m_isolate, v8Value);
    return true;
}

bool Dictionary::get(const String& key, RefPtrWillBeRawPtr<SpeechRecognitionResultList>& value) const
{
    v8::Local<v8::Value> v8Value;
    if (!getKey(key, v8Value))
        return false;

    value = V8SpeechRecognitionResultList::toNativeWithTypeCheck(m_isolate, v8Value);
    return true;
}

bool Dictionary::get(const String& key, RefPtr<MediaStream>& value) const
{
    v8::Local<v8::Value> v8Value;
    if (!getKey(key, v8Value))
        return false;

    value = V8MediaStream::toNativeWithTypeCheck(m_isolate, v8Value);
    return true;
}

bool Dictionary::get(const String& key, RefPtr<EventTarget>& value) const
{
    v8::Local<v8::Value> v8Value;
    if (!getKey(key, v8Value))
        return false;

    value = nullptr;
    // We need to handle a DOMWindow specially, because a DOMWindow wrapper
    // exists on a prototype chain of v8Value.
    if (v8Value->IsObject()) {
        v8::Handle<v8::Object> wrapper = v8::Handle<v8::Object>::Cast(v8Value);
        v8::Handle<v8::Object> window = V8Window::findInstanceInPrototypeChain(wrapper, m_isolate);
        if (!window.IsEmpty()) {
            value = toWrapperTypeInfo(window)->toEventTarget(window);
            return true;
        }
    }

    if (V8DOMWrapper::isDOMWrapper(v8Value)) {
        v8::Handle<v8::Object> wrapper = v8::Handle<v8::Object>::Cast(v8Value);
        value = toWrapperTypeInfo(wrapper)->toEventTarget(wrapper);
    }
    return true;
}

bool Dictionary::get(const String& key, Dictionary& value) const
{
    v8::Local<v8::Value> v8Value;
    if (!getKey(key, v8Value))
        return false;

    if (v8Value->IsObject()) {
        ASSERT(m_isolate);
        ASSERT(m_isolate == v8::Isolate::GetCurrent());
        value = Dictionary(v8Value, m_isolate);
    }

    return true;
}

bool Dictionary::convert(ConversionContext& context, const String& key, Dictionary& value) const
{
    ConversionContextScope scope(context);

    v8::Local<v8::Value> v8Value;
    if (!getKey(key, v8Value))
        return true;

    if (v8Value->IsObject())
        return get(key, value);

    if (context.isNullable() && WebCore::isUndefinedOrNull(v8Value))
        return true;

    context.throwTypeError(ExceptionMessages::incorrectPropertyType(key, "does not have a Dictionary type."));
    return false;
}

bool Dictionary::get(const String& key, Vector<String>& value) const
{
    v8::Local<v8::Value> v8Value;
    if (!getKey(key, v8Value))
        return false;

    if (!v8Value->IsArray())
        return false;

    v8::Local<v8::Array> v8Array = v8::Local<v8::Array>::Cast(v8Value);
    for (size_t i = 0; i < v8Array->Length(); ++i) {
        v8::Local<v8::Value> indexedValue = v8Array->Get(v8::Uint32::New(m_isolate, i));
        V8TRYCATCH_FOR_V8STRINGRESOURCE_RETURN(V8StringResource<>, stringValue, indexedValue, false);
        value.append(stringValue);
    }

    return true;
}

bool Dictionary::convert(ConversionContext& context, const String& key, Vector<String>& value) const
{
    ConversionContextScope scope(context);

    v8::Local<v8::Value> v8Value;
    if (!getKey(key, v8Value))
        return true;

    if (context.isNullable() && WebCore::isUndefinedOrNull(v8Value))
        return true;

    if (!v8Value->IsArray()) {
        context.throwTypeError(ExceptionMessages::notASequenceTypeProperty(key));
        return false;
    }

    return get(key, value);
}

bool Dictionary::get(const String& key, ArrayValue& value) const
{
    v8::Local<v8::Value> v8Value;
    if (!getKey(key, v8Value))
        return false;

    if (!v8Value->IsArray())
        return false;

    ASSERT(m_isolate);
    ASSERT(m_isolate == v8::Isolate::GetCurrent());
    value = ArrayValue(v8::Local<v8::Array>::Cast(v8Value), m_isolate);
    return true;
}

bool Dictionary::convert(ConversionContext& context, const String& key, ArrayValue& value) const
{
    ConversionContextScope scope(context);

    v8::Local<v8::Value> v8Value;
    if (!getKey(key, v8Value))
        return true;

    if (context.isNullable() && WebCore::isUndefinedOrNull(v8Value))
        return true;

    if (!v8Value->IsArray()) {
        context.throwTypeError(ExceptionMessages::notASequenceTypeProperty(key));
        return false;
    }

    return get(key, value);
}

bool Dictionary::get(const String& key, RefPtrWillBeRawPtr<DOMError>& value) const
{
    v8::Local<v8::Value> v8Value;
    if (!getKey(key, v8Value))
        return false;

    value = V8DOMError::toNativeWithTypeCheck(m_isolate, v8Value);
    return true;
}

bool Dictionary::get(const String& key, OwnPtr<VoidCallback>& value) const
{
    v8::Local<v8::Value> v8Value;
    if (!getKey(key, v8Value))
        return false;

    if (!v8Value->IsFunction())
        return false;

    value = V8VoidCallback::create(v8::Handle<v8::Function>::Cast(v8Value), currentExecutionContext(m_isolate));
    return true;
}

bool Dictionary::getOwnPropertiesAsStringHashMap(HashMap<String, String>& hashMap) const
{
    if (!isObject())
        return false;

    v8::Handle<v8::Object> options = m_options->ToObject();
    if (options.IsEmpty())
        return false;

    v8::Local<v8::Array> properties = options->GetOwnPropertyNames();
    if (properties.IsEmpty())
        return true;
    for (uint32_t i = 0; i < properties->Length(); ++i) {
        v8::Local<v8::String> key = properties->Get(i)->ToString();
        if (!options->Has(key))
            continue;

        v8::Local<v8::Value> value = options->Get(key);
        V8TRYCATCH_FOR_V8STRINGRESOURCE_RETURN(V8StringResource<>, stringKey, key, false);
        V8TRYCATCH_FOR_V8STRINGRESOURCE_RETURN(V8StringResource<>, stringValue, value, false);
        if (!static_cast<const String&>(stringKey).isEmpty())
            hashMap.set(stringKey, stringValue);
    }

    return true;
}

bool Dictionary::getOwnPropertyNames(Vector<String>& names) const
{
    if (!isObject())
        return false;

    v8::Handle<v8::Object> options = m_options->ToObject();
    if (options.IsEmpty())
        return false;

    v8::Local<v8::Array> properties = options->GetOwnPropertyNames();
    if (properties.IsEmpty())
        return true;
    for (uint32_t i = 0; i < properties->Length(); ++i) {
        v8::Local<v8::String> key = properties->Get(i)->ToString();
        if (!options->Has(key))
            continue;
        V8TRYCATCH_FOR_V8STRINGRESOURCE_RETURN(V8StringResource<>, stringKey, key, false);
        names.append(stringKey);
    }

    return true;
}

void Dictionary::ConversionContext::resetPerPropertyContext()
{
    if (m_dirty) {
        m_dirty = false;
        m_isNullable = false;
        m_propertyTypeName = "";
    }
}

Dictionary::ConversionContext& Dictionary::ConversionContext::setConversionType(const String& typeName, bool isNullable)
{
    ASSERT(!m_dirty);
    m_dirty = true;
    m_isNullable = isNullable;
    m_propertyTypeName = typeName;

    return *this;
}

void Dictionary::ConversionContext::throwTypeError(const String& detail)
{
    exceptionState().throwTypeError(detail);
}

} // namespace WebCore
