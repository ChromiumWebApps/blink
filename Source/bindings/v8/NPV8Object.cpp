/*
 * Copyright (C) 2004, 2006 Apple Computer, Inc.  All rights reserved.
 * Copyright (C) 2007, 2008, 2009 Google, Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include "bindings/v8/NPV8Object.h"

#include "bindings/v8/ScriptController.h"
#include "bindings/v8/ScriptSourceCode.h"
#include "bindings/v8/V8Binding.h"
#include "bindings/v8/V8GCController.h"
#include "bindings/v8/V8NPUtils.h"
#include "bindings/v8/V8ObjectConstructor.h"
#include "bindings/v8/V8ScriptRunner.h"
#include "bindings/v8/WrapperTypeInfo.h"
#include "bindings/v8/npruntime_impl.h"
#include "bindings/v8/npruntime_priv.h"
#include "core/frame/DOMWindow.h"
#include "core/frame/LocalFrame.h"
#include "platform/UserGestureIndicator.h"
#include "wtf/OwnPtr.h"

#include <stdio.h>
#include "wtf/StringExtras.h"
#include "wtf/text/WTFString.h"

using namespace WebCore;

namespace WebCore {

const WrapperTypeInfo* npObjectTypeInfo()
{
    static const WrapperTypeInfo typeInfo = { gin::kEmbedderBlink, 0, 0, 0, 0, 0, 0, 0, WrapperTypeObjectPrototype, false };
    return &typeInfo;
}

// FIXME: Comments on why use malloc and free.
static NPObject* allocV8NPObject(NPP, NPClass*)
{
    return static_cast<NPObject*>(malloc(sizeof(V8NPObject)));
}

static void freeV8NPObject(NPObject* npObject)
{
    V8NPObject* v8NpObject = reinterpret_cast<V8NPObject*>(npObject);
    disposeUnderlyingV8Object(npObject, v8::Isolate::GetCurrent());
    free(v8NpObject);
}

static NPClass V8NPObjectClass = {
    NP_CLASS_STRUCT_VERSION,
    allocV8NPObject,
    freeV8NPObject,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

static v8::Local<v8::Context> mainWorldContext(v8::Isolate* isolate, NPP npp, NPObject* npObject)
{
    ASSERT(npObject->_class == &V8NPObjectClass);
    V8NPObject* object = reinterpret_cast<V8NPObject*>(npObject);
    DOMWindow* window = object->rootObject;
    if (!window || !window->isCurrentlyDisplayedInFrame())
        return v8::Local<v8::Context>();
    return toV8Context(isolate, object->rootObject->frame(), DOMWrapperWorld::mainWorld());
}

static PassOwnPtr<v8::Handle<v8::Value>[]> createValueListFromVariantArgs(const NPVariant* arguments, uint32_t argumentCount, NPObject* owner, v8::Isolate* isolate)
{
    OwnPtr<v8::Handle<v8::Value>[]> argv = adoptArrayPtr(new v8::Handle<v8::Value>[argumentCount]);
    for (uint32_t index = 0; index < argumentCount; index++) {
        const NPVariant* arg = &arguments[index];
        argv[index] = convertNPVariantToV8Object(arg, owner, isolate);
    }
    return argv.release();
}

// Create an identifier (null terminated utf8 char*) from the NPIdentifier.
static v8::Local<v8::String> npIdentifierToV8Identifier(NPIdentifier name, v8::Isolate* isolate)
{
    PrivateIdentifier* identifier = static_cast<PrivateIdentifier*>(name);
    if (identifier->isString)
        return v8AtomicString(isolate, static_cast<const char*>(identifier->value.string));

    char buffer[32];
    snprintf(buffer, sizeof(buffer), "%d", identifier->value.number);
    return v8AtomicString(isolate, buffer);
}

NPObject* v8ObjectToNPObject(v8::Handle<v8::Object> object)
{
    return reinterpret_cast<NPObject*>(object->GetAlignedPointerFromInternalField(v8DOMWrapperObjectIndex));
}

NPObject* npCreateV8ScriptObject(NPP npp, v8::Handle<v8::Object> object, DOMWindow* root, v8::Isolate* isolate)
{
    // Check to see if this object is already wrapped.
    if (object->InternalFieldCount() == npObjectInternalFieldCount) {
        const WrapperTypeInfo* typeInfo = static_cast<const WrapperTypeInfo*>(object->GetAlignedPointerFromInternalField(v8DOMWrapperTypeIndex));
        if (typeInfo == npObjectTypeInfo()) {
            NPObject* returnValue = v8ObjectToNPObject(object);
            _NPN_RetainObject(returnValue);
            return returnValue;
        }
    }

    V8NPObjectVector* objectVector = 0;
    if (V8PerContextData* perContextData = V8PerContextData::from(object->CreationContext())) {
        int v8ObjectHash = object->GetIdentityHash();
        ASSERT(v8ObjectHash);
        V8NPObjectMap* v8NPObjectMap = perContextData->v8NPObjectMap();
        V8NPObjectMap::iterator iter = v8NPObjectMap->find(v8ObjectHash);
        if (iter != v8NPObjectMap->end()) {
            V8NPObjectVector& objects = iter->value;
            for (size_t index = 0; index < objects.size(); ++index) {
                V8NPObject* v8npObject = objects.at(index);
                if (v8npObject->v8Object == object && v8npObject->rootObject == root) {
                    _NPN_RetainObject(&v8npObject->object);
                    return reinterpret_cast<NPObject*>(v8npObject);
                }
            }
            objectVector = &iter->value;
        } else {
            objectVector = &v8NPObjectMap->set(v8ObjectHash, V8NPObjectVector()).storedValue->value;
        }
    }

    V8NPObject* v8npObject = reinterpret_cast<V8NPObject*>(_NPN_CreateObject(npp, &V8NPObjectClass));
    // This is uninitialized memory, we need to clear it so that
    // Persistent::Reset won't try to Dispose anything bogus.
    new (&v8npObject->v8Object) v8::Persistent<v8::Object>();
    v8npObject->v8Object.Reset(isolate, object);
    v8npObject->rootObject = root;

    if (objectVector)
        objectVector->append(v8npObject);

    return reinterpret_cast<NPObject*>(v8npObject);
}

V8NPObject* npObjectToV8NPObject(NPObject* npObject)
{
    if (npObject->_class != &V8NPObjectClass)
        return 0;
    V8NPObject* v8NpObject = reinterpret_cast<V8NPObject*>(npObject);
    if (v8NpObject->v8Object.IsEmpty())
        return 0;
    return v8NpObject;
}

void disposeUnderlyingV8Object(NPObject* npObject, v8::Isolate* isolate)
{
    ASSERT(npObject);
    V8NPObject* v8NpObject = npObjectToV8NPObject(npObject);
    if (!v8NpObject)
        return;
    v8::HandleScope scope(isolate);
    v8::Handle<v8::Object> v8Object = v8::Local<v8::Object>::New(isolate, v8NpObject->v8Object);
    ASSERT(!v8Object->CreationContext().IsEmpty());
    if (V8PerContextData* perContextData = V8PerContextData::from(v8Object->CreationContext())) {
        V8NPObjectMap* v8NPObjectMap = perContextData->v8NPObjectMap();
        int v8ObjectHash = v8Object->GetIdentityHash();
        ASSERT(v8ObjectHash);
        V8NPObjectMap::iterator iter = v8NPObjectMap->find(v8ObjectHash);
        if (iter != v8NPObjectMap->end()) {
            V8NPObjectVector& objects = iter->value;
            for (size_t index = 0; index < objects.size(); ++index) {
                if (objects.at(index) == v8NpObject) {
                    objects.remove(index);
                    break;
                }
            }
            if (objects.isEmpty())
                v8NPObjectMap->remove(v8ObjectHash);
        }
    }
    v8NpObject->v8Object.Reset();
    v8NpObject->rootObject = 0;
}

} // namespace WebCore

bool _NPN_Invoke(NPP npp, NPObject* npObject, NPIdentifier methodName, const NPVariant* arguments, uint32_t argumentCount, NPVariant* result)
{
    if (!npObject)
        return false;

    v8::Isolate* isolate = v8::Isolate::GetCurrent();

    V8NPObject* v8NpObject = npObjectToV8NPObject(npObject);
    if (!v8NpObject) {
        if (npObject->_class->invoke)
            return npObject->_class->invoke(npObject, methodName, arguments, argumentCount, result);

        VOID_TO_NPVARIANT(*result);
        return true;
    }

    PrivateIdentifier* identifier = static_cast<PrivateIdentifier*>(methodName);
    if (!identifier->isString)
        return false;

    if (!strcmp(identifier->value.string, "eval")) {
        if (argumentCount != 1)
            return false;
        if (arguments[0].type != NPVariantType_String)
            return false;
        return _NPN_Evaluate(npp, npObject, const_cast<NPString*>(&arguments[0].value.stringValue), result);
    }

    v8::HandleScope handleScope(isolate);
    // FIXME: should use the plugin's owner frame as the security context.
    v8::Handle<v8::Context> context = mainWorldContext(isolate, npp, npObject);
    if (context.IsEmpty())
        return false;

    v8::Context::Scope scope(context);
    ExceptionCatcher exceptionCatcher;

    v8::Handle<v8::Object> v8Object = v8::Local<v8::Object>::New(isolate, v8NpObject->v8Object);
    v8::Handle<v8::Value> functionObject = v8Object->Get(v8AtomicString(isolate, identifier->value.string));
    if (functionObject.IsEmpty() || functionObject->IsNull()) {
        NULL_TO_NPVARIANT(*result);
        return false;
    }
    if (functionObject->IsUndefined()) {
        VOID_TO_NPVARIANT(*result);
        return false;
    }

    LocalFrame* frame = v8NpObject->rootObject->frame();
    ASSERT(frame);

    // Call the function object.
    v8::Handle<v8::Function> function = v8::Handle<v8::Function>::Cast(functionObject);
    OwnPtr<v8::Handle<v8::Value>[]> argv = createValueListFromVariantArgs(arguments, argumentCount, npObject, isolate);
    v8::Local<v8::Value> resultObject = frame->script().callFunction(function, v8Object, argumentCount, argv.get());

    // If we had an error, return false.  The spec is a little unclear here, but says "Returns true if the method was
    // successfully invoked".  If we get an error return value, was that successfully invoked?
    if (resultObject.IsEmpty())
        return false;

    convertV8ObjectToNPVariant(resultObject, npObject, result, isolate);
    return true;
}

// FIXME: Fix it same as _NPN_Invoke (HandleScope and such).
bool _NPN_InvokeDefault(NPP npp, NPObject* npObject, const NPVariant* arguments, uint32_t argumentCount, NPVariant* result)
{
    if (!npObject)
        return false;

    v8::Isolate* isolate = v8::Isolate::GetCurrent();

    V8NPObject* v8NpObject = npObjectToV8NPObject(npObject);
    if (!v8NpObject) {
        if (npObject->_class->invokeDefault)
            return npObject->_class->invokeDefault(npObject, arguments, argumentCount, result);

        VOID_TO_NPVARIANT(*result);
        return true;
    }

    VOID_TO_NPVARIANT(*result);

    v8::HandleScope handleScope(isolate);
    v8::Handle<v8::Context> context = mainWorldContext(isolate, npp, npObject);
    if (context.IsEmpty())
        return false;

    v8::Context::Scope scope(context);
    ExceptionCatcher exceptionCatcher;

    // Lookup the function object and call it.
    v8::Local<v8::Object> functionObject = v8::Local<v8::Object>::New(isolate, v8NpObject->v8Object);
    if (!functionObject->IsFunction())
        return false;

    v8::Local<v8::Value> resultObject;
    v8::Handle<v8::Function> function = v8::Local<v8::Function>::Cast(functionObject);
    if (!function->IsNull()) {
        LocalFrame* frame = v8NpObject->rootObject->frame();
        ASSERT(frame);

        OwnPtr<v8::Handle<v8::Value>[]> argv = createValueListFromVariantArgs(arguments, argumentCount, npObject, isolate);
        resultObject = frame->script().callFunction(function, functionObject, argumentCount, argv.get());
    }
    // If we had an error, return false.  The spec is a little unclear here, but says "Returns true if the method was
    // successfully invoked".  If we get an error return value, was that successfully invoked?
    if (resultObject.IsEmpty())
        return false;

    convertV8ObjectToNPVariant(resultObject, npObject, result, isolate);
    return true;
}

bool _NPN_Evaluate(NPP npp, NPObject* npObject, NPString* npScript, NPVariant* result)
{
    // FIXME: Give the embedder a way to control this.
    bool popupsAllowed = false;
    return _NPN_EvaluateHelper(npp, popupsAllowed, npObject, npScript, result);
}

bool _NPN_EvaluateHelper(NPP npp, bool popupsAllowed, NPObject* npObject, NPString* npScript, NPVariant* result)
{
    VOID_TO_NPVARIANT(*result);
    if (!npObject)
        return false;

    V8NPObject* v8NpObject = npObjectToV8NPObject(npObject);
    if (!v8NpObject)
        return false;

    v8::Isolate* isolate = v8::Isolate::GetCurrent();
    v8::HandleScope handleScope(isolate);
    v8::Handle<v8::Context> context = mainWorldContext(isolate, npp, npObject);
    if (context.IsEmpty())
        return false;

    v8::Context::Scope scope(context);
    ExceptionCatcher exceptionCatcher;

    // FIXME: Is this branch still needed after switching to using UserGestureIndicator?
    String filename;
    if (!popupsAllowed)
        filename = "npscript";

    LocalFrame* frame = v8NpObject->rootObject->frame();
    ASSERT(frame);

    String script = String::fromUTF8(npScript->UTF8Characters, npScript->UTF8Length);

    UserGestureIndicator gestureIndicator(popupsAllowed ? DefinitelyProcessingNewUserGesture : PossiblyProcessingUserGesture);
    v8::Local<v8::Value> v8result = frame->script().executeScriptAndReturnValue(context, ScriptSourceCode(script, KURL(ParsedURLString, filename)));

    if (v8result.IsEmpty())
        return false;

    if (_NPN_IsAlive(npObject))
        convertV8ObjectToNPVariant(v8result, npObject, result, isolate);
    return true;
}

bool _NPN_GetProperty(NPP npp, NPObject* npObject, NPIdentifier propertyName, NPVariant* result)
{
    if (!npObject)
        return false;

    if (V8NPObject* object = npObjectToV8NPObject(npObject)) {
        v8::Isolate* isolate = v8::Isolate::GetCurrent();
        v8::HandleScope handleScope(isolate);
        v8::Handle<v8::Context> context = mainWorldContext(isolate, npp, npObject);
        if (context.IsEmpty())
            return false;

        v8::Context::Scope scope(context);
        ExceptionCatcher exceptionCatcher;

        v8::Handle<v8::Object> obj = v8::Local<v8::Object>::New(isolate, object->v8Object);
        v8::Local<v8::Value> v8result = obj->Get(npIdentifierToV8Identifier(propertyName, isolate));

        if (v8result.IsEmpty())
            return false;

        convertV8ObjectToNPVariant(v8result, npObject, result, isolate);
        return true;
    }

    if (npObject->_class->hasProperty && npObject->_class->getProperty) {
        if (npObject->_class->hasProperty(npObject, propertyName))
            return npObject->_class->getProperty(npObject, propertyName, result);
    }

    VOID_TO_NPVARIANT(*result);
    return false;
}

bool _NPN_SetProperty(NPP npp, NPObject* npObject, NPIdentifier propertyName, const NPVariant* value)
{
    if (!npObject)
        return false;

    if (V8NPObject* object = npObjectToV8NPObject(npObject)) {
        v8::Isolate* isolate = v8::Isolate::GetCurrent();
        v8::HandleScope handleScope(isolate);
        v8::Handle<v8::Context> context = mainWorldContext(isolate, npp, npObject);
        if (context.IsEmpty())
            return false;

        v8::Context::Scope scope(context);
        ExceptionCatcher exceptionCatcher;

        v8::Handle<v8::Object> obj = v8::Local<v8::Object>::New(isolate, object->v8Object);
        obj->Set(npIdentifierToV8Identifier(propertyName, context->GetIsolate()), convertNPVariantToV8Object(value, object->rootObject->frame()->script().windowScriptNPObject(), context->GetIsolate()));
        return true;
    }

    if (npObject->_class->setProperty)
        return npObject->_class->setProperty(npObject, propertyName, value);

    return false;
}

bool _NPN_RemoveProperty(NPP npp, NPObject* npObject, NPIdentifier propertyName)
{
    if (!npObject)
        return false;

    V8NPObject* object = npObjectToV8NPObject(npObject);
    if (!object)
        return false;

    v8::Isolate* isolate = v8::Isolate::GetCurrent();
    v8::HandleScope handleScope(isolate);
    v8::Handle<v8::Context> context = mainWorldContext(isolate, npp, npObject);
    if (context.IsEmpty())
        return false;
    v8::Context::Scope scope(context);
    ExceptionCatcher exceptionCatcher;

    v8::Handle<v8::Object> obj = v8::Local<v8::Object>::New(isolate, object->v8Object);
    // FIXME: Verify that setting to undefined is right.
    obj->Set(npIdentifierToV8Identifier(propertyName, isolate), v8::Undefined(isolate));
    return true;
}

bool _NPN_HasProperty(NPP npp, NPObject* npObject, NPIdentifier propertyName)
{
    if (!npObject)
        return false;

    if (V8NPObject* object = npObjectToV8NPObject(npObject)) {
        v8::Isolate* isolate = v8::Isolate::GetCurrent();
        v8::HandleScope handleScope(isolate);
        v8::Handle<v8::Context> context = mainWorldContext(isolate, npp, npObject);
        if (context.IsEmpty())
            return false;
        v8::Context::Scope scope(context);
        ExceptionCatcher exceptionCatcher;

        v8::Handle<v8::Object> obj = v8::Local<v8::Object>::New(isolate, object->v8Object);
        return obj->Has(npIdentifierToV8Identifier(propertyName, isolate));
    }

    if (npObject->_class->hasProperty)
        return npObject->_class->hasProperty(npObject, propertyName);
    return false;
}

bool _NPN_HasMethod(NPP npp, NPObject* npObject, NPIdentifier methodName)
{
    if (!npObject)
        return false;

    if (V8NPObject* object = npObjectToV8NPObject(npObject)) {
        v8::Isolate* isolate = v8::Isolate::GetCurrent();
        v8::HandleScope handleScope(isolate);
        v8::Handle<v8::Context> context = mainWorldContext(isolate, npp, npObject);
        if (context.IsEmpty())
            return false;
        v8::Context::Scope scope(context);
        ExceptionCatcher exceptionCatcher;

        v8::Handle<v8::Object> obj = v8::Local<v8::Object>::New(isolate, object->v8Object);
        v8::Handle<v8::Value> prop = obj->Get(npIdentifierToV8Identifier(methodName, isolate));
        return prop->IsFunction();
    }

    if (npObject->_class->hasMethod)
        return npObject->_class->hasMethod(npObject, methodName);
    return false;
}

void _NPN_SetException(NPObject* npObject, const NPUTF8 *message)
{
    if (!npObject || !npObjectToV8NPObject(npObject)) {
        // We won't be able to find a proper scope for this exception, so just throw it.
        // This is consistent with JSC, which throws a global exception all the time.
        throwError(v8GeneralError, message, v8::Isolate::GetCurrent());
        return;
    }

    v8::Isolate* isolate = v8::Isolate::GetCurrent();
    v8::HandleScope handleScope(isolate);
    v8::Handle<v8::Context> context = mainWorldContext(isolate, 0, npObject);
    if (context.IsEmpty())
        return;

    v8::Context::Scope scope(context);
    ExceptionCatcher exceptionCatcher;

    throwError(v8GeneralError, message, context->GetIsolate());
}

bool _NPN_Enumerate(NPP npp, NPObject* npObject, NPIdentifier** identifier, uint32_t* count)
{
    if (!npObject)
        return false;

    if (V8NPObject* object = npObjectToV8NPObject(npObject)) {
        v8::Isolate* isolate = v8::Isolate::GetCurrent();
        v8::HandleScope handleScope(isolate);
        v8::Local<v8::Context> context = mainWorldContext(isolate, npp, npObject);
        if (context.IsEmpty())
            return false;
        v8::Context::Scope scope(context);
        ExceptionCatcher exceptionCatcher;

        v8::Handle<v8::Object> obj = v8::Local<v8::Object>::New(isolate, object->v8Object);

        // FIXME: http://b/issue?id=1210340: Use a v8::Object::Keys() method when it exists, instead of evaluating javascript.

        // FIXME: Figure out how to cache this helper function.  Run a helper function that collects the properties
        // on the object into an array.
        const char enumeratorCode[] =
            "(function (obj) {"
            "  var props = [];"
            "  for (var prop in obj) {"
            "    props[props.length] = prop;"
            "  }"
            "  return props;"
            "});";
        v8::Handle<v8::String> source = v8AtomicString(isolate, enumeratorCode);
        v8::Handle<v8::Value> result = V8ScriptRunner::compileAndRunInternalScript(source, context->GetIsolate());
        ASSERT(!result.IsEmpty());
        ASSERT(result->IsFunction());
        v8::Handle<v8::Function> enumerator = v8::Handle<v8::Function>::Cast(result);
        v8::Handle<v8::Value> argv[] = { obj };
        v8::Local<v8::Value> propsObj = V8ScriptRunner::callInternalFunction(enumerator, v8::Handle<v8::Object>::Cast(result), WTF_ARRAY_LENGTH(argv), argv, context->GetIsolate());
        if (propsObj.IsEmpty())
            return false;

        // Convert the results into an array of NPIdentifiers.
        v8::Handle<v8::Array> props = v8::Handle<v8::Array>::Cast(propsObj);
        *count = props->Length();
        *identifier = static_cast<NPIdentifier*>(calloc(*count, sizeof(NPIdentifier)));
        for (uint32_t i = 0; i < *count; ++i) {
            v8::Local<v8::Value> name = props->Get(v8::Integer::New(context->GetIsolate(), i));
            (*identifier)[i] = getStringIdentifier(v8::Local<v8::String>::Cast(name));
        }
        return true;
    }

    if (NP_CLASS_STRUCT_VERSION_HAS_ENUM(npObject->_class) && npObject->_class->enumerate)
       return npObject->_class->enumerate(npObject, identifier, count);

    return false;
}

bool _NPN_Construct(NPP npp, NPObject* npObject, const NPVariant* arguments, uint32_t argumentCount, NPVariant* result)
{
    if (!npObject)
        return false;

    v8::Isolate* isolate = v8::Isolate::GetCurrent();

    if (V8NPObject* object = npObjectToV8NPObject(npObject)) {
        v8::HandleScope handleScope(isolate);
        v8::Handle<v8::Context> context = mainWorldContext(isolate, npp, npObject);
        if (context.IsEmpty())
            return false;
        v8::Context::Scope scope(context);
        ExceptionCatcher exceptionCatcher;

        // Lookup the constructor function.
        v8::Handle<v8::Object> ctorObj = v8::Local<v8::Object>::New(isolate, object->v8Object);
        if (!ctorObj->IsFunction())
            return false;

        // Call the constructor.
        v8::Local<v8::Value> resultObject;
        v8::Handle<v8::Function> ctor = v8::Handle<v8::Function>::Cast(ctorObj);
        if (!ctor->IsNull()) {
            LocalFrame* frame = object->rootObject->frame();
            ASSERT(frame);
            OwnPtr<v8::Handle<v8::Value>[]> argv = createValueListFromVariantArgs(arguments, argumentCount, npObject, isolate);
            resultObject = V8ObjectConstructor::newInstanceInDocument(ctor, argumentCount, argv.get(), frame ? frame->document() : 0);
        }

        if (resultObject.IsEmpty())
            return false;

        convertV8ObjectToNPVariant(resultObject, npObject, result, isolate);
        return true;
    }

    if (NP_CLASS_STRUCT_VERSION_HAS_CTOR(npObject->_class) && npObject->_class->construct)
        return npObject->_class->construct(npObject, arguments, argumentCount, result);

    return false;
}
