/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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
#include "V8HTMLAllCollection.h"

#include "V8Element.h"
#include "V8NodeList.h"
#include "bindings/v8/V8Binding.h"
#include "core/dom/NamedNodesCollection.h"
#include "core/html/HTMLAllCollection.h"
#include "core/frame/UseCounter.h"

namespace WebCore {

template<class CallbackInfo>
static v8::Handle<v8::Value> getNamedItems(HTMLAllCollection* collection, AtomicString name, const CallbackInfo& info)
{
    Vector<RefPtr<Element> > namedItems;
    collection->namedItems(name, namedItems);

    if (!namedItems.size())
        return v8Undefined();

    if (namedItems.size() == 1)
        return toV8(namedItems.at(0).release(), info.Holder(), info.GetIsolate());

    // FIXME: HTML5 specification says this should be a HTMLCollection.
    // http://www.whatwg.org/specs/web-apps/current-work/multipage/common-dom-interfaces.html#htmlallcollection
    return toV8(NamedNodesCollection::create(namedItems), info.Holder(), info.GetIsolate());
}

template<class CallbackInfo>
static v8::Handle<v8::Value> getItem(HTMLAllCollection* collection, v8::Handle<v8::Value> argument, const CallbackInfo& info)
{
    v8::Local<v8::Uint32> index = argument->ToArrayIndex();
    if (index.IsEmpty()) {
        V8TRYCATCH_FOR_V8STRINGRESOURCE_RETURN(V8StringResource<>, name, argument, v8::Undefined(info.GetIsolate()));
        v8::Handle<v8::Value> result = getNamedItems(collection, name, info);

        if (result.IsEmpty())
            return v8::Undefined(info.GetIsolate());

        return result;
    }

    RefPtr<Element> result = collection->item(index->Uint32Value());
    return toV8(result.release(), info.Holder(), info.GetIsolate());
}

void V8HTMLAllCollection::itemMethodCustom(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    HTMLAllCollection* imp = V8HTMLAllCollection::toNative(info.Holder());
    v8SetReturnValue(info, getItem(imp, info[0], info));
}

void V8HTMLAllCollection::legacyCallCustom(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    if (info.Length() < 1)
        return;

    HTMLAllCollection* imp = V8HTMLAllCollection::toNative(info.Holder());
    Node& ownerNode = imp->ownerNode();

    UseCounter::count(ownerNode.document(), UseCounter::DocumentAllLegacyCall);

    if (info.Length() == 1) {
        v8SetReturnValue(info, getItem(imp, info[0], info));
        return;
    }

    // If there is a second argument it is the index of the item we want.
    V8TRYCATCH_FOR_V8STRINGRESOURCE_VOID(V8StringResource<>, name, info[0]);
    v8::Local<v8::Uint32> index = info[1]->ToArrayIndex();
    if (index.IsEmpty())
        return;

    if (Node* node = imp->namedItemWithIndex(name, index->Uint32Value())) {
        v8SetReturnValueFast(info, node, imp);
        return;
    }
}

} // namespace WebCore
