/*
 * Copyright (C) 2007 Apple Inc. All rights reserved.
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
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
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
#include "core/frame/Console.h"

#include "bindings/v8/ScriptCallStackFactory.h"
#include "core/frame/ConsoleTypes.h"
#include "core/frame/FrameHost.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/PageConsole.h"
#include "core/inspector/ConsoleAPITypes.h"
#include "core/inspector/ScriptArguments.h"
#include "core/inspector/ScriptCallStack.h"
#include "core/page/Chrome.h"
#include "core/page/ChromeClient.h"
#include "core/timing/MemoryInfo.h"
#include "platform/TraceEvent.h"
#include "wtf/text/CString.h"
#include "wtf/text/WTFString.h"

namespace WebCore {

Console::Console(LocalFrame* frame)
    : DOMWindowProperty(frame)
{
    ScriptWrappable::init(this);
}

Console::~Console()
{
}

ExecutionContext* Console::context()
{
    if (!m_frame)
        return 0;
    return m_frame->document();
}

void Console::reportMessageToClient(MessageLevel level, const String& message, PassRefPtr<ScriptCallStack> callStack)
{
    if (!m_frame || !m_frame->host() || !callStack.get())
        return;

    String stackTrace;
    if (m_frame->host()->chrome().client().shouldReportDetailedMessageForSource(callStack->at(0).sourceURL())) {
        RefPtr<ScriptCallStack> fullStack = createScriptCallStack(ScriptCallStack::maxCallStackSizeToCapture);
        stackTrace = PageConsole::formatStackTraceString(message, fullStack);
    }
    m_frame->host()->chrome().client().addMessageToConsole(ConsoleAPIMessageSource, level, message, callStack->at(0).lineNumber(), callStack->at(0).sourceURL(), stackTrace);
}

PassRefPtrWillBeRawPtr<MemoryInfo> Console::memory() const
{
    // FIXME: Because we create a new object here each time,
    // console.memory !== console.memory, which seems wrong.
    return MemoryInfo::create(m_frame);
}

} // namespace WebCore
