/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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
#include "core/inspector/WorkerDebuggerAgent.h"

#include "bindings/v8/ScriptDebugServer.h"
#include "core/workers/WorkerGlobalScope.h"
#include "core/workers/WorkerThread.h"
#include "wtf/MessageQueue.h"

namespace WebCore {

namespace {

Mutex& workerDebuggerAgentsMutex()
{
    AtomicallyInitializedStatic(Mutex&, mutex = *new Mutex);
    return mutex;
}

typedef HashMap<WorkerThread*, WorkerDebuggerAgent*> WorkerDebuggerAgents;

WorkerDebuggerAgents& workerDebuggerAgents()
{
    DEFINE_STATIC_LOCAL(WorkerDebuggerAgents, agents, ());
    return agents;
}


class RunInspectorCommandsTask FINAL : public ScriptDebugServer::Task {
public:
    explicit RunInspectorCommandsTask(WorkerThread* thread)
        : m_thread(thread) { }
    virtual ~RunInspectorCommandsTask() { }
    virtual void run() OVERRIDE
    {
        // Process all queued debugger commands. WorkerThread is certainly
        // alive if this task is being executed.
        while (MessageQueueMessageReceived == m_thread->runLoop().runDebuggerTask(WorkerRunLoop::DontWaitForMessage)) { }
    }

private:
    WorkerThread* m_thread;
};

} // namespace

PassOwnPtr<WorkerDebuggerAgent> WorkerDebuggerAgent::create(WorkerScriptDebugServer* scriptDebugServer, WorkerGlobalScope* inspectedWorkerGlobalScope, InjectedScriptManager* injectedScriptManager)
{
    return adoptPtr(new WorkerDebuggerAgent(scriptDebugServer, inspectedWorkerGlobalScope, injectedScriptManager));
}

WorkerDebuggerAgent::WorkerDebuggerAgent(WorkerScriptDebugServer* scriptDebugServer, WorkerGlobalScope* inspectedWorkerGlobalScope, InjectedScriptManager* injectedScriptManager)
    : InspectorDebuggerAgent(injectedScriptManager)
    , m_scriptDebugServer(scriptDebugServer)
    , m_inspectedWorkerGlobalScope(inspectedWorkerGlobalScope)
{
    MutexLocker lock(workerDebuggerAgentsMutex());
    workerDebuggerAgents().set(inspectedWorkerGlobalScope->thread(), this);
}

WorkerDebuggerAgent::~WorkerDebuggerAgent()
{
    MutexLocker lock(workerDebuggerAgentsMutex());
    ASSERT(workerDebuggerAgents().contains(m_inspectedWorkerGlobalScope->thread()));
    workerDebuggerAgents().remove(m_inspectedWorkerGlobalScope->thread());
}

void WorkerDebuggerAgent::interruptAndDispatchInspectorCommands(WorkerThread* thread)
{
    MutexLocker lock(workerDebuggerAgentsMutex());
    WorkerDebuggerAgent* agent = workerDebuggerAgents().get(thread);
    if (agent)
        agent->m_scriptDebugServer->interruptAndRunTask(adoptPtr(new RunInspectorCommandsTask(thread)));
}

void WorkerDebuggerAgent::startListeningScriptDebugServer()
{
    scriptDebugServer().addListener(this);
}

void WorkerDebuggerAgent::stopListeningScriptDebugServer()
{
    scriptDebugServer().removeListener(this);
}

WorkerScriptDebugServer& WorkerDebuggerAgent::scriptDebugServer()
{
    return *m_scriptDebugServer;
}

InjectedScript WorkerDebuggerAgent::injectedScriptForEval(ErrorString* error, const int* executionContextId)
{
    if (executionContextId) {
        *error = "Execution context id is not supported for workers as there is only one execution context.";
        return InjectedScript();
    }
    ScriptState* scriptState = scriptStateFromWorkerGlobalScope(m_inspectedWorkerGlobalScope);
    return injectedScriptManager()->injectedScriptFor(scriptState);
}

void WorkerDebuggerAgent::muteConsole()
{
    // We don't need to mute console for workers.
}

void WorkerDebuggerAgent::unmuteConsole()
{
    // We don't need to mute console for workers.
}

} // namespace WebCore
