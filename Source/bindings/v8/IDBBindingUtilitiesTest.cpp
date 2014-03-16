/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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
#include "bindings/v8/IDBBindingUtilities.h"

#include "bindings/v8/V8Binding.h"
#include "bindings/v8/V8PerIsolateData.h"
#include "modules/indexeddb/IDBKey.h"
#include "modules/indexeddb/IDBKeyPath.h"

#include <gtest/gtest.h>

using namespace WebCore;

namespace {

PassRefPtr<IDBKey> checkKeyFromValueAndKeyPathInternal(const ScriptValue& value, const String& keyPath)
{
    IDBKeyPath idbKeyPath(keyPath);
    EXPECT_TRUE(idbKeyPath.isValid());

    return createIDBKeyFromScriptValueAndKeyPath(0, value, idbKeyPath);
}

void checkKeyPathNullValue(const ScriptValue& value, const String& keyPath)
{
    RefPtr<IDBKey> idbKey = checkKeyFromValueAndKeyPathInternal(value, keyPath);
    ASSERT_FALSE(idbKey.get());
}

bool injectKey(PassRefPtr<IDBKey> key, ScriptValue& value, const String& keyPath)
{
    IDBKeyPath idbKeyPath(keyPath);
    EXPECT_TRUE(idbKeyPath.isValid());
    ScriptValue keyValue = idbKeyToScriptValue(0, key);
    return injectV8KeyIntoV8Value(keyValue.v8Value(), value.v8Value(), idbKeyPath, v8::Isolate::GetCurrent());
}

void checkInjection(PassRefPtr<IDBKey> prpKey, ScriptValue& value, const String& keyPath)
{
    RefPtr<IDBKey> key = prpKey;
    bool result = injectKey(key, value, keyPath);
    ASSERT_TRUE(result);
    RefPtr<IDBKey> extractedKey = checkKeyFromValueAndKeyPathInternal(value, keyPath);
    EXPECT_TRUE(key->isEqual(extractedKey.get()));
}

void checkInjectionFails(PassRefPtr<IDBKey> key, ScriptValue& value, const String& keyPath)
{
    EXPECT_FALSE(injectKey(key, value, keyPath));
}

void checkKeyPathStringValue(const ScriptValue& value, const String& keyPath, const String& expected)
{
    RefPtr<IDBKey> idbKey = checkKeyFromValueAndKeyPathInternal(value, keyPath);
    ASSERT_TRUE(idbKey.get());
    ASSERT_EQ(IDBKey::StringType, idbKey->type());
    ASSERT_TRUE(expected == idbKey->string());
}

void checkKeyPathNumberValue(const ScriptValue& value, const String& keyPath, int expected)
{
    RefPtr<IDBKey> idbKey = checkKeyFromValueAndKeyPathInternal(value, keyPath);
    ASSERT_TRUE(idbKey.get());
    ASSERT_EQ(IDBKey::NumberType, idbKey->type());
    ASSERT_TRUE(expected == idbKey->number());
}

class IDBKeyFromValueAndKeyPathTest : public testing::Test {
public:
    IDBKeyFromValueAndKeyPathTest()
        : m_scope(V8ExecutionScope::create(v8::Isolate::GetCurrent()))
    {
    }
private:
    OwnPtr<V8ExecutionScope> m_scope;
};

TEST_F(IDBKeyFromValueAndKeyPathTest, TopLevelPropertyStringValue)
{
    v8::Isolate* isolate = v8::Isolate::GetCurrent();
    v8::Local<v8::Object> object = v8::Object::New(isolate);
    object->Set(v8AtomicString(isolate, "foo"), v8AtomicString(isolate, "zoo"));

    ScriptValue scriptValue(object, isolate);

    checkKeyPathStringValue(scriptValue, "foo", "zoo");
    checkKeyPathNullValue(scriptValue, "bar");
}

TEST_F(IDBKeyFromValueAndKeyPathTest, TopLevelPropertyNumberValue)
{
    v8::Isolate* isolate = v8::Isolate::GetCurrent();
    v8::Local<v8::Object> object = v8::Object::New(isolate);
    object->Set(v8AtomicString(isolate, "foo"), v8::Number::New(isolate, 456));

    ScriptValue scriptValue(object, isolate);

    checkKeyPathNumberValue(scriptValue, "foo", 456);
    checkKeyPathNullValue(scriptValue, "bar");
}

TEST_F(IDBKeyFromValueAndKeyPathTest, SubProperty)
{
    v8::Isolate* isolate = v8::Isolate::GetCurrent();
    v8::Local<v8::Object> object = v8::Object::New(isolate);
    v8::Local<v8::Object> subProperty = v8::Object::New(isolate);
    subProperty->Set(v8AtomicString(isolate, "bar"), v8AtomicString(isolate, "zee"));
    object->Set(v8AtomicString(isolate, "foo"), subProperty);

    ScriptValue scriptValue(object, isolate);

    checkKeyPathStringValue(scriptValue, "foo.bar", "zee");
    checkKeyPathNullValue(scriptValue, "bar");
}

class InjectIDBKeyTest : public IDBKeyFromValueAndKeyPathTest {
};

TEST_F(InjectIDBKeyTest, TopLevelPropertyStringValue)
{
    v8::Isolate* isolate = v8::Isolate::GetCurrent();
    v8::Local<v8::Object> object = v8::Object::New(isolate);
    object->Set(v8AtomicString(isolate, "foo"), v8AtomicString(isolate, "zoo"));

    ScriptValue foozoo(object, isolate);
    checkInjection(IDBKey::createString("myNewKey"), foozoo, "bar");
    checkInjection(IDBKey::createNumber(1234), foozoo, "bar");

    checkInjectionFails(IDBKey::createString("key"), foozoo, "foo.bar");
}

TEST_F(InjectIDBKeyTest, SubProperty)
{
    v8::Isolate* isolate = v8::Isolate::GetCurrent();
    v8::Local<v8::Object> object = v8::Object::New(isolate);
    v8::Local<v8::Object> subProperty = v8::Object::New(isolate);
    subProperty->Set(v8AtomicString(isolate, "bar"), v8AtomicString(isolate, "zee"));
    object->Set(v8AtomicString(isolate, "foo"), subProperty);

    ScriptValue scriptObject(object, isolate);
    checkInjection(IDBKey::createString("myNewKey"), scriptObject, "foo.baz");
    checkInjection(IDBKey::createNumber(789), scriptObject, "foo.baz");
    checkInjection(IDBKey::createDate(4567), scriptObject, "foo.baz");
    checkInjection(IDBKey::createDate(4567), scriptObject, "bar");
    checkInjection(IDBKey::createArray(IDBKey::KeyArray()), scriptObject, "foo.baz");
    checkInjection(IDBKey::createArray(IDBKey::KeyArray()), scriptObject, "bar");

    checkInjectionFails(IDBKey::createString("zoo"), scriptObject, "foo.bar.baz");
    checkInjection(IDBKey::createString("zoo"), scriptObject, "foo.xyz.foo");
}

} // namespace
