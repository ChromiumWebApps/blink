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

#ifndef ScriptPromise_h
#define ScriptPromise_h

#include "bindings/v8/ScopedPersistent.h"
#include "bindings/v8/ScriptFunction.h"
#include "bindings/v8/ScriptValue.h"
#include "bindings/v8/V8ScriptRunner.h"
#include "wtf/Forward.h"
#include <v8.h>

namespace WebCore {

class ExecutionContext;

// ScriptPromise is the class for representing Promise values in C++ world.
// ScriptPromise holds a Promise.
// So holding a ScriptPromise as a member variable in DOM object causes
// memory leaks since it has a reference from C++ to V8.
//
class ScriptPromise {
public:
    // Constructs an empty promise.
    ScriptPromise() { }

    // Constructs a ScriptPromise from |promise|.
    // If |promise| is not a Promise object, throws a v8 TypeError.
    ScriptPromise(v8::Handle<v8::Value> promise, v8::Isolate*);

    ScriptPromise then(PassOwnPtr<ScriptFunction> onFulfilled, PassOwnPtr<ScriptFunction> onRejected = PassOwnPtr<ScriptFunction>());

    bool isObject() const
    {
        return m_promise.isObject();
    }

    bool isNull() const
    {
        return m_promise.isNull();
    }

    bool isUndefinedOrNull() const
    {
        return m_promise.isUndefined() || m_promise.isNull();
    }

    v8::Handle<v8::Value> v8Value() const
    {
        return m_promise.v8Value();
    }

    v8::Isolate* isolate() const
    {
        return m_promise.isolate();
    }

    bool hasNoValue() const
    {
        return m_promise.hasNoValue();
    }

    void clear()
    {
        m_promise.clear();
    }

    // Constructs and returns a ScriptPromise from |value|.
    // if |value| is not a Promise object, returns a Promise object
    // resolved with |value|.
    static ScriptPromise cast(const ScriptValue& /*value*/);

private:
    friend class ScriptPromiseResolver;

    static ScriptPromise createPending();
    static ScriptPromise createPending(ExecutionContext*);
    static ScriptPromise createPending(v8::Isolate*) { return createPending(); }

    ScriptValue m_promise;
};

} // namespace WebCore


#endif // ScriptPromise_h
