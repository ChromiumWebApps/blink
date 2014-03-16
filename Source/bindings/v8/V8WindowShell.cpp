/*
 * Copyright (C) 2008, 2009, 2011 Google Inc. All rights reserved.
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
#include "bindings/v8/V8WindowShell.h"

#include "RuntimeEnabledFeatures.h"
#include "V8Document.h"
#include "V8HTMLCollection.h"
#include "V8HTMLDocument.h"
#include "V8Window.h"
#include "bindings/v8/DOMWrapperWorld.h"
#include "bindings/v8/ScriptController.h"
#include "bindings/v8/V8Binding.h"
#include "bindings/v8/V8GCForContextDispose.h"
#include "bindings/v8/V8HiddenValue.h"
#include "bindings/v8/V8Initializer.h"
#include "bindings/v8/V8ObjectConstructor.h"
#include "bindings/v8/V8PerContextData.h"
#include "core/html/HTMLCollection.h"
#include "core/html/HTMLIFrameElement.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "core/loader/DocumentLoader.h"
#include "core/loader/FrameLoader.h"
#include "core/loader/FrameLoaderClient.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/csp/ContentSecurityPolicy.h"
#include "platform/TraceEvent.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "public/platform/Platform.h"
#include "wtf/Assertions.h"
#include "wtf/OwnPtr.h"
#include "wtf/StringExtras.h"
#include "wtf/text/CString.h"
#include <algorithm>
#include <utility>
#include <v8-debug.h>
#include <v8.h>

namespace WebCore {

static void checkDocumentWrapper(v8::Handle<v8::Object> wrapper, Document* document)
{
    ASSERT(V8Document::toNative(wrapper) == document);
    ASSERT(!document->isHTMLDocument() || (V8Document::toNative(v8::Handle<v8::Object>::Cast(wrapper->GetPrototype())) == document));
}

static void setInjectedScriptContextDebugId(v8::Handle<v8::Context> targetContext, int debugId)
{
    V8PerContextDebugData::setContextDebugData(targetContext, "injected", debugId);
}

PassOwnPtr<V8WindowShell> V8WindowShell::create(LocalFrame* frame, PassRefPtr<DOMWrapperWorld> world, v8::Isolate* isolate)
{
    return adoptPtr(new V8WindowShell(frame, world, isolate));
}

V8WindowShell::V8WindowShell(LocalFrame* frame, PassRefPtr<DOMWrapperWorld> world, v8::Isolate* isolate)
    : m_frame(frame)
    , m_world(world)
    , m_isolate(isolate)
{
}

void V8WindowShell::disposeContext(GlobalDetachmentBehavior behavior)
{
    if (!m_perContextData)
        return;

    v8::HandleScope handleScope(m_isolate);
    v8::Handle<v8::Context> context = m_perContextData->context();
    m_frame->loader().client()->willReleaseScriptContext(context, m_world->worldId());

    if (behavior == DetachGlobal)
        context->DetachGlobal();

    m_perContextData.clear();

    // It's likely that disposing the context has created a lot of
    // garbage. Notify V8 about this so it'll have a chance of cleaning
    // it up when idle.
    V8GCForContextDispose::instanceTemplate().notifyContextDisposed(m_frame->isMainFrame());
}

void V8WindowShell::clearForClose()
{
    if (!m_perContextData)
        return;

    m_document.clear();
    disposeContext(DoNotDetachGlobal);
}

void V8WindowShell::clearForNavigation()
{
    if (!m_perContextData)
        return;

    v8::HandleScope handleScope(m_isolate);
    m_document.clear();

    v8::Handle<v8::Context> context = m_perContextData->context();
    v8::Context::Scope contextScope(context);

    // Clear the document wrapper cache before turning on access checks on
    // the old DOMWindow wrapper. This way, access to the document wrapper
    // will be protected by the security checks on the DOMWindow wrapper.
    clearDocumentProperty();

    v8::Handle<v8::Object> windowWrapper = V8Window::findInstanceInPrototypeChain(m_global.newLocal(m_isolate), m_isolate);
    ASSERT(!windowWrapper.IsEmpty());
    windowWrapper->TurnOnAccessCheck();
    disposeContext(DetachGlobal);
}

// Create a new environment and setup the global object.
//
// The global object corresponds to a DOMWindow instance. However, to
// allow properties of the JS DOMWindow instance to be shadowed, we
// use a shadow object as the global object and use the JS DOMWindow
// instance as the prototype for that shadow object. The JS DOMWindow
// instance is undetectable from JavaScript code because the __proto__
// accessors skip that object.
//
// The shadow object and the DOMWindow instance are seen as one object
// from JavaScript. The JavaScript object that corresponds to a
// DOMWindow instance is the shadow object. When mapping a DOMWindow
// instance to a V8 object, we return the shadow object.
//
// To implement split-window, see
//   1) https://bugs.webkit.org/show_bug.cgi?id=17249
//   2) https://wiki.mozilla.org/Gecko:SplitWindow
//   3) https://bugzilla.mozilla.org/show_bug.cgi?id=296639
// we need to split the shadow object further into two objects:
// an outer window and an inner window. The inner window is the hidden
// prototype of the outer window. The inner window is the default
// global object of the context. A variable declared in the global
// scope is a property of the inner window.
//
// The outer window sticks to a LocalFrame, it is exposed to JavaScript
// via window.window, window.self, window.parent, etc. The outer window
// has a security token which is the domain. The outer window cannot
// have its own properties. window.foo = 'x' is delegated to the
// inner window.
//
// When a frame navigates to a new page, the inner window is cut off
// the outer window, and the outer window identify is preserved for
// the frame. However, a new inner window is created for the new page.
// If there are JS code holds a closure to the old inner window,
// it won't be able to reach the outer window via its global object.
bool V8WindowShell::initializeIfNeeded()
{
    if (m_perContextData)
        return true;

    DOMWrapperWorld::setWorldOfInitializingWindow(m_world.get());
    bool result = initialize();
    DOMWrapperWorld::setWorldOfInitializingWindow(0);
    return result;
}

bool V8WindowShell::initialize()
{
    TRACE_EVENT0("v8", "V8WindowShell::initialize");
    TRACE_EVENT_SCOPED_SAMPLING_STATE("Blink", "InitializeWindow");

    v8::HandleScope handleScope(m_isolate);

    createContext();

    if (!m_perContextData)
        return false;

    v8::Handle<v8::Context> context = m_perContextData->context();
    v8::Context::Scope contextScope(context);

    if (m_global.isEmpty()) {
        m_global.set(m_isolate, context->Global());
        if (m_global.isEmpty()) {
            disposeContext(DoNotDetachGlobal);
            return false;
        }
    }

    if (!m_world->isMainWorld()) {
        V8WindowShell* mainWindow = m_frame->script().existingWindowShell(DOMWrapperWorld::mainWorld());
        if (mainWindow && !mainWindow->context().IsEmpty())
            setInjectedScriptContextDebugId(context, m_frame->script().contextDebugId(mainWindow->context()));
    }

    m_perContextData->setActivityLogger(V8DOMActivityLogger::activityLogger(m_world->worldId()));
    if (!installDOMWindow()) {
        disposeContext(DoNotDetachGlobal);
        return false;
    }

    if (m_world->isMainWorld()) {
        updateDocument();
        if (m_frame->document()) {
            setSecurityToken(m_frame->document()->securityOrigin());
            ContentSecurityPolicy* csp = m_frame->document()->contentSecurityPolicy();
            context->AllowCodeGenerationFromStrings(csp->allowEval(0, ContentSecurityPolicy::SuppressReport));
            context->SetErrorMessageForCodeGenerationFromStrings(v8String(m_isolate, csp->evalDisabledErrorMessage()));
        }
    } else {
        // Using the default security token means that the canAccess is always
        // called, which is slow.
        // FIXME: Use tokens where possible. This will mean keeping track of all
        //        created contexts so that they can all be updated when the
        //        document domain
        //        changes.
        context->UseDefaultSecurityToken();

        SecurityOrigin* origin = m_world->isolatedWorldSecurityOrigin();
        if (origin && InspectorInstrumentation::hasFrontends()) {
            ScriptState* scriptState = ScriptState::forContext(v8::Local<v8::Context>::New(m_isolate, context));
            InspectorInstrumentation::didCreateIsolatedContext(m_frame, scriptState, origin);
        }
    }
    m_frame->loader().client()->didCreateScriptContext(context, m_world->extensionGroup(), m_world->worldId());
    return true;
}

void V8WindowShell::createContext()
{
    // The documentLoader pointer could be 0 during frame shutdown.
    // FIXME: Can we remove this check?
    if (!m_frame->loader().documentLoader())
        return;

    // Create a new environment using an empty template for the shadow
    // object. Reuse the global object if one has been created earlier.
    v8::Handle<v8::ObjectTemplate> globalTemplate = V8Window::getShadowObjectTemplate(m_isolate);
    if (globalTemplate.IsEmpty())
        return;

    double contextCreationStartInSeconds = currentTime();

    // Dynamically tell v8 about our extensions now.
    const V8Extensions& extensions = ScriptController::registeredExtensions();
    OwnPtr<const char*[]> extensionNames = adoptArrayPtr(new const char*[extensions.size()]);
    int index = 0;
    int extensionGroup = m_world->extensionGroup();
    int worldId = m_world->worldId();
    for (size_t i = 0; i < extensions.size(); ++i) {
        if (!m_frame->loader().client()->allowScriptExtension(extensions[i]->name(), extensionGroup, worldId))
            continue;

        extensionNames[index++] = extensions[i]->name();
    }
    v8::ExtensionConfiguration extensionConfiguration(index, extensionNames.get());

    v8::Handle<v8::Context> context = v8::Context::New(m_isolate, &extensionConfiguration, globalTemplate, m_global.newLocal(m_isolate));
    if (context.IsEmpty())
        return;
    m_perContextData = V8PerContextData::create(context, m_world);

    double contextCreationDurationInMilliseconds = (currentTime() - contextCreationStartInSeconds) * 1000;
    const char* histogramName = "WebCore.V8WindowShell.createContext.MainWorld";
    if (!m_world->isMainWorld())
        histogramName = "WebCore.V8WindowShell.createContext.IsolatedWorld";
    blink::Platform::current()->histogramCustomCounts(histogramName, contextCreationDurationInMilliseconds, 0, 10000, 50);
}

static v8::Handle<v8::Object> toInnerGlobalObject(v8::Handle<v8::Context> context)
{
    return v8::Handle<v8::Object>::Cast(context->Global()->GetPrototype());
}

bool V8WindowShell::installDOMWindow()
{
    DOMWindow* window = m_frame->domWindow();
    v8::Local<v8::Object> windowWrapper = V8ObjectConstructor::newInstance(V8PerContextData::from(m_perContextData->context())->constructorForType(&V8Window::wrapperTypeInfo));
    if (windowWrapper.IsEmpty())
        return false;

    V8Window::installPerContextEnabledProperties(windowWrapper, window, m_isolate);

    V8DOMWrapper::setNativeInfo(v8::Handle<v8::Object>::Cast(windowWrapper->GetPrototype()), &V8Window::wrapperTypeInfo, window);

    // Install the windowWrapper as the prototype of the innerGlobalObject.
    // The full structure of the global object is as follows:
    //
    // outerGlobalObject (Empty object, remains after navigation)
    //   -- has prototype --> innerGlobalObject (Holds global variables, changes during navigation)
    //   -- has prototype --> DOMWindow instance
    //   -- has prototype --> Window.prototype
    //   -- has prototype --> Object.prototype
    //
    // Note: Much of this prototype structure is hidden from web content. The
    //       outer, inner, and DOMWindow instance all appear to be the same
    //       JavaScript object.
    //
    v8::Handle<v8::Object> innerGlobalObject = toInnerGlobalObject(m_perContextData->context());
    V8DOMWrapper::setNativeInfo(innerGlobalObject, &V8Window::wrapperTypeInfo, window);
    innerGlobalObject->SetPrototype(windowWrapper);
    V8DOMWrapper::associateObjectWithWrapper<V8Window>(PassRefPtr<DOMWindow>(window), &V8Window::wrapperTypeInfo, windowWrapper, m_isolate, WrapperConfiguration::Dependent);
    return true;
}

void V8WindowShell::updateDocumentWrapper(v8::Handle<v8::Object> wrapper)
{
    ASSERT(m_world->isMainWorld());
    m_document.set(m_isolate, wrapper);
}

void V8WindowShell::updateDocumentProperty()
{
    if (!m_world->isMainWorld())
        return;

    v8::HandleScope handleScope(m_isolate);
    v8::Handle<v8::Context> context = m_perContextData->context();
    v8::Context::Scope contextScope(context);

    v8::Handle<v8::Value> documentWrapper = toV8(m_frame->document(), v8::Handle<v8::Object>(), context->GetIsolate());
    ASSERT(documentWrapper == m_document.newLocal(m_isolate) || m_document.isEmpty());
    if (m_document.isEmpty())
        updateDocumentWrapper(v8::Handle<v8::Object>::Cast(documentWrapper));
    checkDocumentWrapper(m_document.newLocal(m_isolate), m_frame->document());

    // If instantiation of the document wrapper fails, clear the cache
    // and let the DOMWindow accessor handle access to the document.
    if (documentWrapper.IsEmpty()) {
        clearDocumentProperty();
        return;
    }
    ASSERT(documentWrapper->IsObject());
    context->Global()->ForceSet(v8AtomicString(m_isolate, "document"), documentWrapper, static_cast<v8::PropertyAttribute>(v8::ReadOnly | v8::DontDelete));

    // We also stash a reference to the document on the inner global object so that
    // DOMWindow objects we obtain from JavaScript references are guaranteed to have
    // live Document objects.
    V8HiddenValue::setHiddenValue(m_isolate, toInnerGlobalObject(context), V8HiddenValue::document(m_isolate), documentWrapper);
}

void V8WindowShell::clearDocumentProperty()
{
    ASSERT(m_perContextData);
    if (!m_world->isMainWorld())
        return;
    v8::HandleScope handleScope(m_isolate);
    m_perContextData->context()->Global()->ForceDelete(v8AtomicString(m_isolate, "document"));
}

void V8WindowShell::setSecurityToken(SecurityOrigin* origin)
{
    ASSERT(m_world->isMainWorld());
    // If two tokens are equal, then the SecurityOrigins canAccess each other.
    // If two tokens are not equal, then we have to call canAccess.
    // Note: we can't use the HTTPOrigin if it was set from the DOM.
    String token;
    // We stick with an empty token if document.domain was modified or if we
    // are in the initial empty document, so that we can do a full canAccess
    // check in those cases.
    if (!origin->domainWasSetInDOM()
        && !m_frame->loader().stateMachine()->isDisplayingInitialEmptyDocument())
        token = origin->toString();

    // An empty or "null" token means we always have to call
    // canAccess. The toString method on securityOrigins returns the
    // string "null" for empty security origins and for security
    // origins that should only allow access to themselves. In this
    // case, we use the global object as the security token to avoid
    // calling canAccess when a script accesses its own objects.
    v8::HandleScope handleScope(m_isolate);
    v8::Handle<v8::Context> context = m_perContextData->context();
    if (token.isEmpty() || token == "null") {
        context->UseDefaultSecurityToken();
        return;
    }

    CString utf8Token = token.utf8();
    // NOTE: V8 does identity comparison in fast path, must use a symbol
    // as the security token.
    context->SetSecurityToken(v8AtomicString(m_isolate, utf8Token.data(), utf8Token.length()));
}

void V8WindowShell::updateDocument()
{
    ASSERT(m_world->isMainWorld());
    if (m_global.isEmpty())
        return;
    if (!m_perContextData)
        return;
    updateDocumentProperty();
    updateSecurityOrigin(m_frame->document()->securityOrigin());
}

static v8::Handle<v8::Value> getNamedProperty(HTMLDocument* htmlDocument, const AtomicString& key, v8::Handle<v8::Object> creationContext, v8::Isolate* isolate)
{
    if (!htmlDocument->hasNamedItem(key) && !htmlDocument->hasExtraNamedItem(key))
        return v8Undefined();

    RefPtr<HTMLCollection> items = htmlDocument->documentNamedItems(key);
    if (items->isEmpty())
        return v8Undefined();

    if (items->hasExactlyOneItem()) {
        Element* element = items->item(0);
        LocalFrame* frame = 0;
        if (element->hasTagName(HTMLNames::iframeTag) && (frame = toHTMLIFrameElement(element)->contentFrame()))
            return toV8(frame->domWindow(), creationContext, isolate);
        return toV8(element, creationContext, isolate);
    }
    return toV8(items.release(), creationContext, isolate);
}

static void getter(v8::Local<v8::String> property, const v8::PropertyCallbackInfo<v8::Value>& info)
{
    // FIXME: Consider passing StringImpl directly.
    AtomicString name = toCoreAtomicString(property);
    HTMLDocument* htmlDocument = V8HTMLDocument::toNative(info.Holder());
    ASSERT(htmlDocument);
    v8::Handle<v8::Value> result = getNamedProperty(htmlDocument, name, info.Holder(), info.GetIsolate());
    if (!result.IsEmpty()) {
        v8SetReturnValue(info, result);
        return;
    }
    v8::Handle<v8::Value> prototype = info.Holder()->GetPrototype();
    if (prototype->IsObject()) {
        v8SetReturnValue(info, prototype.As<v8::Object>()->Get(property));
        return;
    }
}

void V8WindowShell::namedItemAdded(HTMLDocument* document, const AtomicString& name)
{
    ASSERT(m_world->isMainWorld());

    if (!m_perContextData)
        return;

    v8::HandleScope handleScope(m_isolate);
    v8::Context::Scope contextScope(m_perContextData->context());

    ASSERT(!m_document.isEmpty());
    v8::Handle<v8::Object> documentHandle = m_document.newLocal(m_isolate);
    checkDocumentWrapper(documentHandle, document);
    documentHandle->SetAccessor(v8String(m_isolate, name), getter);
}

void V8WindowShell::namedItemRemoved(HTMLDocument* document, const AtomicString& name)
{
    ASSERT(m_world->isMainWorld());

    if (!m_perContextData)
        return;

    if (document->hasNamedItem(name) || document->hasExtraNamedItem(name))
        return;

    v8::HandleScope handleScope(m_isolate);
    v8::Context::Scope contextScope(m_perContextData->context());

    ASSERT(!m_document.isEmpty());
    v8::Handle<v8::Object> documentHandle = m_document.newLocal(m_isolate);
    checkDocumentWrapper(documentHandle, document);
    documentHandle->Delete(v8String(m_isolate, name));
}

void V8WindowShell::updateSecurityOrigin(SecurityOrigin* origin)
{
    ASSERT(m_world->isMainWorld());
    if (!m_perContextData)
        return;
    v8::HandleScope handleScope(m_isolate);
    setSecurityToken(origin);
}

} // WebCore
