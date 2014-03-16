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
#include "bindings/v8/CustomElementConstructorBuilder.h"

#include "HTMLNames.h"
#include "SVGNames.h"
#include "V8Document.h"
#include "V8HTMLElementWrapperFactory.h"
#include "V8SVGElementWrapperFactory.h"
#include "bindings/v8/CustomElementBinding.h"
#include "bindings/v8/DOMWrapperWorld.h"
#include "bindings/v8/Dictionary.h"
#include "bindings/v8/ExceptionState.h"
#include "bindings/v8/ScriptState.h"
#include "bindings/v8/UnsafePersistent.h"
#include "bindings/v8/V8Binding.h"
#include "bindings/v8/V8HiddenValue.h"
#include "bindings/v8/V8PerContextData.h"
#include "core/dom/Document.h"
#include "core/dom/custom/CustomElementCallbackDispatcher.h"
#include "core/dom/custom/CustomElementDefinition.h"
#include "core/dom/custom/CustomElementDescriptor.h"
#include "core/dom/custom/CustomElementException.h"
#include "wtf/Assertions.h"

namespace WebCore {

static void constructCustomElement(const v8::FunctionCallbackInfo<v8::Value>&);

CustomElementConstructorBuilder::CustomElementConstructorBuilder(ScriptState* state, const Dictionary* options)
    : m_context(state->context())
    , m_options(options)
    , m_wrapperType(0)
{
    ASSERT(m_context == v8::Isolate::GetCurrent()->GetCurrentContext());
}

bool CustomElementConstructorBuilder::isFeatureAllowed() const
{
    return DOMWrapperWorld::world(m_context)->isMainWorld();
}

bool CustomElementConstructorBuilder::validateOptions(const AtomicString& type, QualifiedName& tagName, ExceptionState& exceptionState)
{
    ASSERT(m_prototype.IsEmpty());

    ScriptValue prototypeScriptValue;
    if (m_options->get("prototype", prototypeScriptValue) && !prototypeScriptValue.isNull()) {
        if (!prototypeScriptValue.isObject()) {
            CustomElementException::throwException(CustomElementException::PrototypeNotAnObject, type, exceptionState);
            return false;
        }
        m_prototype = prototypeScriptValue.v8Value().As<v8::Object>();
    } else {
        m_prototype = v8::Object::New(m_context->GetIsolate());
        v8::Local<v8::Object> basePrototype = V8PerContextData::from(m_context)->prototypeForType(&V8HTMLElement::wrapperTypeInfo);
        if (!basePrototype.IsEmpty())
            m_prototype->SetPrototype(basePrototype);
    }

    AtomicString extends;
    bool extendsProvidedAndNonNull = m_options->get("extends", extends);

    if (!V8PerContextData::from(m_context)) {
        // FIXME: This should generate an InvalidContext exception at a later point.
        CustomElementException::throwException(CustomElementException::ContextDestroyedCheckingPrototype, type, exceptionState);
        return false;
    }

    AtomicString namespaceURI = HTMLNames::xhtmlNamespaceURI;
    if (hasValidPrototypeChainFor(&V8SVGElement::wrapperTypeInfo))
        namespaceURI = SVGNames::svgNamespaceURI;

    AtomicString localName;

    if (extendsProvidedAndNonNull) {
        localName = extends.lower();

        if (!Document::isValidName(localName)) {
            CustomElementException::throwException(CustomElementException::ExtendsIsInvalidName, type, exceptionState);
            return false;
        }
        if (CustomElement::isValidName(localName)) {
            CustomElementException::throwException(CustomElementException::ExtendsIsCustomElementName, type, exceptionState);
            return false;
        }
    } else {
        if (namespaceURI == SVGNames::svgNamespaceURI) {
            CustomElementException::throwException(CustomElementException::ExtendsIsInvalidName, type, exceptionState);
            return false;
        }
        localName = type;
    }

    if (!extendsProvidedAndNonNull)
        m_wrapperType = &V8HTMLElement::wrapperTypeInfo;
    else if (namespaceURI == HTMLNames::xhtmlNamespaceURI)
        m_wrapperType = findWrapperTypeForHTMLTagName(localName);
    else
        m_wrapperType = findWrapperTypeForSVGTagName(localName);

    ASSERT(m_wrapperType);
    tagName = QualifiedName(nullAtom, localName, namespaceURI);
    return m_wrapperType;
}

PassRefPtr<CustomElementLifecycleCallbacks> CustomElementConstructorBuilder::createCallbacks()
{
    ASSERT(!m_prototype.IsEmpty());

    RefPtr<ExecutionContext> executionContext(toExecutionContext(m_context));

    v8::TryCatch exceptionCatcher;
    exceptionCatcher.SetVerbose(true);

    v8::Isolate* isolate = v8::Isolate::GetCurrent();
    v8::Handle<v8::Function> created = retrieveCallback(isolate, "createdCallback");
    v8::Handle<v8::Function> attached = retrieveCallback(isolate, "attachedCallback");
    v8::Handle<v8::Function> detached = retrieveCallback(isolate, "detachedCallback");
    v8::Handle<v8::Function> attributeChanged = retrieveCallback(isolate, "attributeChangedCallback");

    m_callbacks = V8CustomElementLifecycleCallbacks::create(executionContext.get(), m_prototype, created, attached, detached, attributeChanged);
    return m_callbacks.get();
}

v8::Handle<v8::Function> CustomElementConstructorBuilder::retrieveCallback(v8::Isolate* isolate, const char* name)
{
    v8::Handle<v8::Value> value = m_prototype->Get(v8String(isolate, name));
    if (value.IsEmpty() || !value->IsFunction())
        return v8::Handle<v8::Function>();
    return value.As<v8::Function>();
}

bool CustomElementConstructorBuilder::createConstructor(Document* document, CustomElementDefinition* definition, ExceptionState& exceptionState)
{
    ASSERT(!m_prototype.IsEmpty());
    ASSERT(m_constructor.IsEmpty());
    ASSERT(document);

    v8::Isolate* isolate = m_context->GetIsolate();

    if (!prototypeIsValid(definition->descriptor().type(), exceptionState))
        return false;

    v8::Local<v8::FunctionTemplate> constructorTemplate = v8::FunctionTemplate::New(isolate);
    constructorTemplate->SetCallHandler(constructCustomElement);
    m_constructor = constructorTemplate->GetFunction();
    if (m_constructor.IsEmpty()) {
        CustomElementException::throwException(CustomElementException::ContextDestroyedRegisteringDefinition, definition->descriptor().type(), exceptionState);
        return false;
    }

    const CustomElementDescriptor& descriptor = definition->descriptor();

    v8::Handle<v8::String> v8TagName = v8String(isolate, descriptor.localName());
    v8::Handle<v8::Value> v8Type;
    if (descriptor.isTypeExtension())
        v8Type = v8String(isolate, descriptor.type());
    else
        v8Type = v8::Null(isolate);

    m_constructor->SetName(v8Type->IsNull() ? v8TagName : v8Type.As<v8::String>());

    V8HiddenValue::setHiddenValue(isolate, m_constructor, V8HiddenValue::customElementDocument(isolate), toV8(document, m_context->Global(), isolate));
    V8HiddenValue::setHiddenValue(isolate, m_constructor, V8HiddenValue::customElementNamespaceURI(isolate), v8String(isolate, descriptor.namespaceURI()));
    V8HiddenValue::setHiddenValue(isolate, m_constructor, V8HiddenValue::customElementTagName(isolate), v8TagName);
    V8HiddenValue::setHiddenValue(isolate, m_constructor, V8HiddenValue::customElementType(isolate), v8Type);

    v8::Handle<v8::String> prototypeKey = v8String(isolate, "prototype");
    ASSERT(m_constructor->HasOwnProperty(prototypeKey));
    // This sets the property *value*; calling Set is safe because
    // "prototype" is a non-configurable data property so there can be
    // no side effects.
    m_constructor->Set(prototypeKey, m_prototype);
    // This *configures* the property. ForceSet of a function's
    // "prototype" does not affect the value, but can reconfigure the
    // property.
    m_constructor->ForceSet(prototypeKey, m_prototype, v8::PropertyAttribute(v8::ReadOnly | v8::DontEnum | v8::DontDelete));

    V8HiddenValue::setHiddenValue(isolate, m_prototype, V8HiddenValue::customElementIsInterfacePrototypeObject(isolate), v8::True(isolate));
    m_prototype->ForceSet(v8String(isolate, "constructor"), m_constructor, v8::DontEnum);

    return true;
}

bool CustomElementConstructorBuilder::prototypeIsValid(const AtomicString& type, ExceptionState& exceptionState) const
{
    if (m_prototype->InternalFieldCount() || !V8HiddenValue::getHiddenValue(m_context->GetIsolate(), m_prototype, V8HiddenValue::customElementIsInterfacePrototypeObject(m_context->GetIsolate())).IsEmpty()) {
        CustomElementException::throwException(CustomElementException::PrototypeInUse, type, exceptionState);
        return false;
    }

    if (m_prototype->GetPropertyAttributes(v8String(m_context->GetIsolate(), "constructor")) & v8::DontDelete) {
        CustomElementException::throwException(CustomElementException::ConstructorPropertyNotConfigurable, type, exceptionState);
        return false;
    }

    return true;
}

bool CustomElementConstructorBuilder::didRegisterDefinition(CustomElementDefinition* definition) const
{
    ASSERT(!m_constructor.IsEmpty());

    return m_callbacks->setBinding(definition, CustomElementBinding::create(m_context->GetIsolate(), m_prototype, m_wrapperType));
}

ScriptValue CustomElementConstructorBuilder::bindingsReturnValue() const
{
    return ScriptValue(m_constructor, m_context->GetIsolate());
}

bool CustomElementConstructorBuilder::hasValidPrototypeChainFor(const WrapperTypeInfo* type) const
{
    v8::Handle<v8::Object> elementPrototype = V8PerContextData::from(m_context)->prototypeForType(type);
    if (elementPrototype.IsEmpty())
        return false;

    v8::Handle<v8::Value> chain = m_prototype;
    while (!chain.IsEmpty() && chain->IsObject()) {
        if (chain == elementPrototype)
            return true;
        chain = chain.As<v8::Object>()->GetPrototype();
    }

    return false;
}

static void constructCustomElement(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    v8::Isolate* isolate = info.GetIsolate();

    if (!info.IsConstructCall()) {
        throwTypeError("DOM object constructor cannot be called as a function.", isolate);
        return;
    }

    if (info.Length() > 0) {
        throwTypeError("This constructor should be called without arguments.", isolate);
        return;
    }

    Document* document = V8Document::toNative(V8HiddenValue::getHiddenValue(info.GetIsolate(), info.Callee(), V8HiddenValue::customElementDocument(isolate)).As<v8::Object>());
    V8TRYCATCH_FOR_V8STRINGRESOURCE_VOID(V8StringResource<>, namespaceURI, V8HiddenValue::getHiddenValue(isolate, info.Callee(), V8HiddenValue::customElementNamespaceURI(isolate)));
    V8TRYCATCH_FOR_V8STRINGRESOURCE_VOID(V8StringResource<>, tagName, V8HiddenValue::getHiddenValue(isolate, info.Callee(), V8HiddenValue::customElementTagName(isolate)));
    v8::Handle<v8::Value> maybeType = V8HiddenValue::getHiddenValue(info.GetIsolate(), info.Callee(), V8HiddenValue::customElementType(isolate));
    V8TRYCATCH_FOR_V8STRINGRESOURCE_VOID(V8StringResource<>, type, maybeType);

    ExceptionState exceptionState(ExceptionState::ConstructionContext, "CustomElement", info.Holder(), info.GetIsolate());
    CustomElementCallbackDispatcher::CallbackDeliveryScope deliveryScope;
    RefPtr<Element> element = document->createElementNS(namespaceURI, tagName, maybeType->IsNull() ? nullAtom : type, exceptionState);
    if (exceptionState.throwIfNeeded())
        return;
    v8SetReturnValueFast(info, element.release(), document);
}

} // namespace WebCore
