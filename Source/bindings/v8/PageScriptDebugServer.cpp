/*
 * Copyright (c) 2011 Google Inc. All rights reserved.
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
#include "bindings/v8/PageScriptDebugServer.h"


#include "V8Window.h"
#include "bindings/v8/DOMWrapperWorld.h"
#include "bindings/v8/ScriptController.h"
#include "bindings/v8/ScriptSourceCode.h"
#include "bindings/v8/V8Binding.h"
#include "bindings/v8/V8ScriptRunner.h"
#include "bindings/v8/V8WindowShell.h"
#include "core/frame/FrameHost.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/PageConsole.h"
#include "core/frame/UseCounter.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "core/inspector/ScriptDebugListener.h"
#include "core/page/Page.h"
#include "wtf/OwnPtr.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/StdLibExtras.h"
#include "wtf/TemporaryChange.h"
#include "wtf/text/StringBuilder.h"

namespace WebCore {

static LocalFrame* retrieveFrameWithGlobalObjectCheck(v8::Handle<v8::Context> context)
{
    if (context.IsEmpty())
        return 0;

    // FIXME: This is a temporary hack for crbug.com/345014.
    // Currently it's possible that V8 can trigger Debugger::ProcessDebugEvent for a context
    // that is being initialized (i.e., inside Context::New() of the context).
    // We should fix the V8 side so that it won't trigger the event for a half-baked context
    // because there is no way in the embedder side to check if the context is half-baked or not.
    if (isMainThread() && DOMWrapperWorld::windowIsBeingInitialized())
        return 0;

    v8::Handle<v8::Value> global = V8Window::findInstanceInPrototypeChain(context->Global(), context->GetIsolate());
    if (global.IsEmpty())
        return 0;

    return toFrameIfNotDetached(context);
}

void PageScriptDebugServer::setPreprocessorSource(const String& preprocessorSource)
{
    if (preprocessorSource.isEmpty())
        m_preprocessorSourceCode.clear();
    else
        m_preprocessorSourceCode = adoptPtr(new ScriptSourceCode(preprocessorSource));
    m_scriptPreprocessor.clear();
}

PageScriptDebugServer& PageScriptDebugServer::shared()
{
    DEFINE_STATIC_LOCAL(PageScriptDebugServer, server, ());
    return server;
}

PageScriptDebugServer::PageScriptDebugServer()
    : ScriptDebugServer(v8::Isolate::GetCurrent())
    , m_pausedPage(0)
{
}

void PageScriptDebugServer::addListener(ScriptDebugListener* listener, Page* page)
{
    ScriptController& scriptController = page->mainFrame()->script();
    if (!scriptController.canExecuteScripts(NotAboutToExecuteScript))
        return;

    v8::HandleScope scope(m_isolate);
    v8::Local<v8::Context> debuggerContext = v8::Debug::GetDebugContext();
    v8::Context::Scope contextScope(debuggerContext);

    v8::Local<v8::Object> debuggerScript = m_debuggerScript.newLocal(m_isolate);
    if (!m_listenersMap.size()) {
        ensureDebuggerScriptCompiled();
        ASSERT(!debuggerScript->IsUndefined());
        v8::Debug::SetDebugEventListener2(&PageScriptDebugServer::v8DebugEventCallback, v8::External::New(m_isolate, this));
    }
    m_listenersMap.set(page, listener);

    V8WindowShell* shell = scriptController.existingWindowShell(DOMWrapperWorld::mainWorld());
    if (!shell || !shell->isContextInitialized())
        return;
    v8::Local<v8::Context> context = shell->context();
    v8::Handle<v8::Function> getScriptsFunction = v8::Local<v8::Function>::Cast(debuggerScript->Get(v8AtomicString(m_isolate, "getScripts")));
    v8::Handle<v8::Value> argv[] = { context->GetEmbedderData(0) };
    v8::Handle<v8::Value> value = V8ScriptRunner::callInternalFunction(getScriptsFunction, debuggerScript, WTF_ARRAY_LENGTH(argv), argv, m_isolate);
    if (value.IsEmpty())
        return;
    ASSERT(!value->IsUndefined() && value->IsArray());
    v8::Handle<v8::Array> scriptsArray = v8::Handle<v8::Array>::Cast(value);
    for (unsigned i = 0; i < scriptsArray->Length(); ++i)
        dispatchDidParseSource(listener, v8::Handle<v8::Object>::Cast(scriptsArray->Get(v8::Integer::New(m_isolate, i))));
}

void PageScriptDebugServer::removeListener(ScriptDebugListener* listener, Page* page)
{
    if (!m_listenersMap.contains(page))
        return;

    if (m_pausedPage == page)
        continueProgram();

    m_listenersMap.remove(page);

    if (m_listenersMap.isEmpty())
        v8::Debug::SetDebugEventListener2(0);
    // FIXME: Remove all breakpoints set by the agent.
}

void PageScriptDebugServer::setClientMessageLoop(PassOwnPtr<ClientMessageLoop> clientMessageLoop)
{
    m_clientMessageLoop = clientMessageLoop;
}

void PageScriptDebugServer::compileScript(ScriptState* state, const String& expression, const String& sourceURL, String* scriptId, String* exceptionMessage)
{
    ExecutionContext* executionContext = state->executionContext();
    RefPtr<LocalFrame> protect = toDocument(executionContext)->frame();
    ScriptDebugServer::compileScript(state, expression, sourceURL, scriptId, exceptionMessage);
    if (!scriptId->isNull())
        m_compiledScriptURLs.set(*scriptId, sourceURL);
}

void PageScriptDebugServer::clearCompiledScripts()
{
    ScriptDebugServer::clearCompiledScripts();
    m_compiledScriptURLs.clear();
}

void PageScriptDebugServer::runScript(ScriptState* state, const String& scriptId, ScriptValue* result, bool* wasThrown, String* exceptionMessage)
{
    String sourceURL = m_compiledScriptURLs.take(scriptId);

    ExecutionContext* executionContext = state->executionContext();
    LocalFrame* frame = toDocument(executionContext)->frame();
    InspectorInstrumentationCookie cookie;
    if (frame)
        cookie = InspectorInstrumentation::willEvaluateScript(frame, sourceURL, TextPosition::minimumPosition().m_line.oneBasedInt());

    RefPtr<LocalFrame> protect = frame;
    ScriptDebugServer::runScript(state, scriptId, result, wasThrown, exceptionMessage);

    if (frame)
        InspectorInstrumentation::didEvaluateScript(cookie);
}

ScriptDebugListener* PageScriptDebugServer::getDebugListenerForContext(v8::Handle<v8::Context> context)
{
    v8::HandleScope scope(m_isolate);
    LocalFrame* frame = retrieveFrameWithGlobalObjectCheck(context);
    if (!frame)
        return 0;
    return m_listenersMap.get(frame->page());
}

void PageScriptDebugServer::runMessageLoopOnPause(v8::Handle<v8::Context> context)
{
    v8::HandleScope scope(m_isolate);
    LocalFrame* frame = retrieveFrameWithGlobalObjectCheck(context);
    m_pausedPage = frame->page();

    // Wait for continue or step command.
    m_clientMessageLoop->run(m_pausedPage);

    // The listener may have been removed in the nested loop.
    if (ScriptDebugListener* listener = m_listenersMap.get(m_pausedPage))
        listener->didContinue();

    m_pausedPage = 0;
}

void PageScriptDebugServer::quitMessageLoopOnPause()
{
    m_clientMessageLoop->quitNow();
}

void PageScriptDebugServer::preprocessBeforeCompile(const v8::Debug::EventDetails& eventDetails)
{
    v8::Handle<v8::Context> eventContext = eventDetails.GetEventContext();
    LocalFrame* frame = retrieveFrameWithGlobalObjectCheck(eventContext);
    if (!frame)
        return;

    if (!canPreprocess(frame))
        return;

    v8::Handle<v8::Object> eventData = eventDetails.GetEventData();
    v8::Local<v8::Context> debugContext = v8::Debug::GetDebugContext();
    v8::Context::Scope contextScope(debugContext);
    v8::TryCatch tryCatch;
    // <script> tag source and attribute value source are preprocessed before we enter V8.
    // Avoid preprocessing any internal scripts by processing only eval source in this V8 event handler.
    v8::Handle<v8::Value> argvEventData[] = { eventData };
    v8::Handle<v8::Value> v8Value = callDebuggerMethod("isEvalCompilation", WTF_ARRAY_LENGTH(argvEventData), argvEventData);
    if (v8Value.IsEmpty() || !v8Value->ToBoolean()->Value())
        return;

    // The name and source are in the JS event data.
    String scriptName = toCoreStringWithUndefinedOrNullCheck(callDebuggerMethod("getScriptName", WTF_ARRAY_LENGTH(argvEventData), argvEventData));
    String script = toCoreStringWithUndefinedOrNullCheck(callDebuggerMethod("getScriptSource", WTF_ARRAY_LENGTH(argvEventData), argvEventData));

    String preprocessedSource  = m_scriptPreprocessor->preprocessSourceCode(script, scriptName);

    v8::Handle<v8::Value> argvPreprocessedScript[] = { eventData, v8String(debugContext->GetIsolate(), preprocessedSource) };
    callDebuggerMethod("setScriptSource", WTF_ARRAY_LENGTH(argvPreprocessedScript), argvPreprocessedScript);
}

static bool isCreatingPreprocessor = false;

bool PageScriptDebugServer::canPreprocess(LocalFrame* frame)
{
    ASSERT(frame);

    if (!m_preprocessorSourceCode || !frame->page() || isCreatingPreprocessor)
        return false;

    // We delay the creation of the preprocessor until just before the first JS from the
    // Web page to ensure that the debugger's console initialization code has completed.
    if (!m_scriptPreprocessor) {
        TemporaryChange<bool> isPreprocessing(isCreatingPreprocessor, true);
        m_scriptPreprocessor = adoptPtr(new ScriptPreprocessor(*m_preprocessorSourceCode.get(), frame));
    }

    if (m_scriptPreprocessor->isValid())
        return true;

    m_scriptPreprocessor.clear();
    // Don't retry the compile if we fail one time.
    m_preprocessorSourceCode.clear();
    return false;
}

// Source to Source processing iff debugger enabled and it has loaded a preprocessor.
PassOwnPtr<ScriptSourceCode> PageScriptDebugServer::preprocess(LocalFrame* frame, const ScriptSourceCode& sourceCode)
{
    if (!canPreprocess(frame))
        return PassOwnPtr<ScriptSourceCode>();

    String preprocessedSource = m_scriptPreprocessor->preprocessSourceCode(sourceCode.source(), sourceCode.url());
    return adoptPtr(new ScriptSourceCode(preprocessedSource, sourceCode.url()));
}

String PageScriptDebugServer::preprocessEventListener(LocalFrame* frame, const String& source, const String& url, const String& functionName)
{
    if (!canPreprocess(frame))
        return source;

    return m_scriptPreprocessor->preprocessSourceCode(source, url, functionName);
}

void PageScriptDebugServer::muteWarningsAndDeprecations()
{
    PageConsole::mute();
    UseCounter::muteForInspector();
}

void PageScriptDebugServer::unmuteWarningsAndDeprecations()
{
    PageConsole::unmute();
    UseCounter::unmuteForInspector();
}

} // namespace WebCore
