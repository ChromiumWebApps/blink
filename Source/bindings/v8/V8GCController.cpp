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
#include "bindings/v8/V8GCController.h"

#include <algorithm>
#include "V8MutationObserver.h"
#include "V8Node.h"
#include "V8ScriptRunner.h"
#include "bindings/v8/RetainedDOMInfo.h"
#include "bindings/v8/V8AbstractEventListener.h"
#include "bindings/v8/V8Binding.h"
#include "bindings/v8/WrapperTypeInfo.h"
#include "core/dom/Attr.h"
#include "core/dom/NodeTraversal.h"
#include "core/dom/TemplateContentDocumentFragment.h"
#include "core/dom/shadow/ElementShadow.h"
#include "core/dom/shadow/ShadowRoot.h"
#include "core/html/HTMLImageElement.h"
#include "core/html/HTMLTemplateElement.h"
#include "core/svg/SVGElement.h"
#include "platform/TraceEvent.h"

namespace WebCore {

// FIXME: This should use opaque GC roots.
static void addReferencesForNodeWithEventListeners(v8::Isolate* isolate, Node* node, const v8::Persistent<v8::Object>& wrapper)
{
    ASSERT(node->hasEventListeners());

    EventListenerIterator iterator(node);
    while (EventListener* listener = iterator.nextListener()) {
        if (listener->type() != EventListener::JSEventListenerType)
            continue;
        V8AbstractEventListener* v8listener = static_cast<V8AbstractEventListener*>(listener);
        if (!v8listener->hasExistingListenerObject())
            continue;

        isolate->SetReference(wrapper, v8::Persistent<v8::Value>::Cast(v8listener->existingListenerObjectPersistentHandle()));
    }
}

Node* V8GCController::opaqueRootForGC(Node* node, v8::Isolate*)
{
    // FIXME: Remove the special handling for image elements.
    // The same special handling is in V8GCController::gcTree().
    // Maybe should image elements be active DOM nodes?
    // See https://code.google.com/p/chromium/issues/detail?id=164882
    if (node->inDocument() || (node->hasTagName(HTMLNames::imgTag) && toHTMLImageElement(node)->hasPendingActivity()))
        return &node->document();

    if (node->isAttributeNode()) {
        Node* ownerElement = toAttr(node)->ownerElement();
        if (!ownerElement)
            return node;
        node = ownerElement;
    }

    while (Node* parent = node->parentOrShadowHostOrTemplateHostNode())
        node = parent;

    return node;
}

// Regarding a minor GC algorithm for DOM nodes, see this document:
// https://docs.google.com/a/google.com/presentation/d/1uifwVYGNYTZDoGLyCb7sXa7g49mWNMW2gaWvMN5NLk8/edit#slide=id.p
class MinorGCWrapperVisitor : public v8::PersistentHandleVisitor {
public:
    explicit MinorGCWrapperVisitor(v8::Isolate* isolate)
        : m_isolate(isolate)
    { }

    virtual void VisitPersistentHandle(v8::Persistent<v8::Value>* value, uint16_t classId) OVERRIDE
    {
        // A minor DOM GC can collect only Nodes.
        if (classId != v8DOMNodeClassId)
            return;

        // To make minor GC cycle time bounded, we limit the number of wrappers handled
        // by each minor GC cycle to 10000. This value was selected so that the minor
        // GC cycle time is bounded to 20 ms in a case where the new space size
        // is 16 MB and it is full of wrappers (which is almost the worst case).
        // Practically speaking, as far as I crawled real web applications,
        // the number of wrappers handled by each minor GC cycle is at most 3000.
        // So this limit is mainly for pathological micro benchmarks.
        const unsigned wrappersHandledByEachMinorGC = 10000;
        if (m_nodesInNewSpace.size() >= wrappersHandledByEachMinorGC)
            return;

        // Casting to a Handle is safe here, since the Persistent doesn't get GCd
        // during the GC prologue.
        ASSERT((*reinterpret_cast<v8::Handle<v8::Value>*>(value))->IsObject());
        v8::Handle<v8::Object>* wrapper = reinterpret_cast<v8::Handle<v8::Object>*>(value);
        ASSERT(V8DOMWrapper::isDOMWrapper(*wrapper));
        ASSERT(V8Node::hasInstance(*wrapper, m_isolate));
        Node* node = V8Node::toNative(*wrapper);
        // A minor DOM GC can handle only node wrappers in the main world.
        // Note that node->wrapper().IsEmpty() returns true for nodes that
        // do not have wrappers in the main world.
        if (node->containsWrapper()) {
            const WrapperTypeInfo* type = toWrapperTypeInfo(*wrapper);
            ActiveDOMObject* activeDOMObject = type->toActiveDOMObject(*wrapper);
            if (activeDOMObject && activeDOMObject->hasPendingActivity())
                return;
            // FIXME: Remove the special handling for image elements.
            // The same special handling is in V8GCController::opaqueRootForGC().
            // Maybe should image elements be active DOM nodes?
            // See https://code.google.com/p/chromium/issues/detail?id=164882
            if (node->hasTagName(HTMLNames::imgTag) && toHTMLImageElement(node)->hasPendingActivity())
                return;
            // FIXME: Remove the special handling for SVG context elements.
            if (node->isSVGElement() && toSVGElement(node)->isContextElement())
                return;

            m_nodesInNewSpace.append(node);
            node->markV8CollectableDuringMinorGC();
        }
    }

    void notifyFinished()
    {
        Node** nodeIterator = m_nodesInNewSpace.begin();
        Node** nodeIteratorEnd = m_nodesInNewSpace.end();
        for (; nodeIterator < nodeIteratorEnd; ++nodeIterator) {
            Node* node = *nodeIterator;
            ASSERT(node->containsWrapper());
            if (node->isV8CollectableDuringMinorGC()) { // This branch is just for performance.
                gcTree(m_isolate, node);
                node->clearV8CollectableDuringMinorGC();
            }
        }
    }

private:
    bool traverseTree(Node* rootNode, Vector<Node*, initialNodeVectorSize>* partiallyDependentNodes)
    {
        // To make each minor GC time bounded, we might need to give up
        // traversing at some point for a large DOM tree. That being said,
        // I could not observe the need even in pathological test cases.
        for (Node* node = rootNode; node; node = NodeTraversal::next(*node)) {
            if (node->containsWrapper()) {
                if (!node->isV8CollectableDuringMinorGC()) {
                    // This node is not in the new space of V8. This indicates that
                    // the minor GC cannot anyway judge reachability of this DOM tree.
                    // Thus we give up traversing the DOM tree.
                    return false;
                }
                node->clearV8CollectableDuringMinorGC();
                partiallyDependentNodes->append(node);
            }
            if (ShadowRoot* shadowRoot = node->youngestShadowRoot()) {
                if (!traverseTree(shadowRoot, partiallyDependentNodes))
                    return false;
            } else if (node->isShadowRoot()) {
                if (ShadowRoot* shadowRoot = toShadowRoot(node)->olderShadowRoot()) {
                    if (!traverseTree(shadowRoot, partiallyDependentNodes))
                        return false;
                }
            }
            // <template> has a |content| property holding a DOM fragment which we must traverse,
            // just like we do for the shadow trees above.
            if (node->hasTagName(HTMLNames::templateTag)) {
                if (!traverseTree(toHTMLTemplateElement(node)->content(), partiallyDependentNodes))
                    return false;
            }
        }
        return true;
    }

    void gcTree(v8::Isolate* isolate, Node* startNode)
    {
        Vector<Node*, initialNodeVectorSize> partiallyDependentNodes;

        Node* node = startNode;
        while (Node* parent = node->parentOrShadowHostOrTemplateHostNode())
            node = parent;

        if (!traverseTree(node, &partiallyDependentNodes))
            return;

        // We completed the DOM tree traversal. All wrappers in the DOM tree are
        // stored in partiallyDependentNodes and are expected to exist in the new space of V8.
        // We report those wrappers to V8 as an object group.
        Node** nodeIterator = partiallyDependentNodes.begin();
        Node** const nodeIteratorEnd = partiallyDependentNodes.end();
        if (nodeIterator == nodeIteratorEnd)
            return;
        v8::UniqueId id(reinterpret_cast<intptr_t>((*nodeIterator)->unsafePersistent().value()));
        for (; nodeIterator != nodeIteratorEnd; ++nodeIterator) {
            // This is safe because we know that GC won't happen before we
            // dispose the UnsafePersistent (we're just preparing a GC). Though,
            // we need to keep the UnsafePersistent alive until we're done with
            // v8::Persistent.
            UnsafePersistent<v8::Object> unsafeWrapper = (*nodeIterator)->unsafePersistent();
            v8::Persistent<v8::Object>* wrapper = unsafeWrapper.persistent();
            wrapper->MarkPartiallyDependent();
            isolate->SetObjectGroupId(v8::Persistent<v8::Value>::Cast(*wrapper), id);
        }
    }

    Vector<Node*> m_nodesInNewSpace;
    v8::Isolate* m_isolate;
};

class MajorGCWrapperVisitor : public v8::PersistentHandleVisitor {
public:
    explicit MajorGCWrapperVisitor(v8::Isolate* isolate, bool constructRetainedObjectInfos)
        : m_isolate(isolate)
        , m_liveRootGroupIdSet(false)
        , m_constructRetainedObjectInfos(constructRetainedObjectInfos)
    {
    }

    virtual void VisitPersistentHandle(v8::Persistent<v8::Value>* value, uint16_t classId) OVERRIDE
    {
        if (classId != v8DOMNodeClassId && classId != v8DOMObjectClassId)
            return;

        // Casting to a Handle is safe here, since the Persistent doesn't get GCd
        // during the GC prologue.
        ASSERT((*reinterpret_cast<v8::Handle<v8::Value>*>(value))->IsObject());
        v8::Handle<v8::Object>* wrapper = reinterpret_cast<v8::Handle<v8::Object>*>(value);
        ASSERT(V8DOMWrapper::isDOMWrapper(*wrapper));

        if (value->IsIndependent())
            return;

        const WrapperTypeInfo* type = toWrapperTypeInfo(*wrapper);
        void* object = toNative(*wrapper);

        ActiveDOMObject* activeDOMObject = type->toActiveDOMObject(*wrapper);
        if (activeDOMObject && activeDOMObject->hasPendingActivity())
            m_isolate->SetObjectGroupId(*value, liveRootId());

        if (classId == v8DOMNodeClassId) {
            ASSERT(V8Node::hasInstance(*wrapper, m_isolate));
            Node* node = static_cast<Node*>(object);
            if (node->hasEventListeners())
                addReferencesForNodeWithEventListeners(m_isolate, node, v8::Persistent<v8::Object>::Cast(*value));
            Node* root = V8GCController::opaqueRootForGC(node, m_isolate);
            m_isolate->SetObjectGroupId(*value, v8::UniqueId(reinterpret_cast<intptr_t>(root)));
            if (m_constructRetainedObjectInfos)
                m_groupsWhichNeedRetainerInfo.append(root);
        } else if (classId == v8DOMObjectClassId) {
            type->visitDOMWrapper(object, v8::Persistent<v8::Object>::Cast(*value), m_isolate);
        } else {
            ASSERT_NOT_REACHED();
        }
    }

    void notifyFinished()
    {
        if (!m_constructRetainedObjectInfos)
            return;
        std::sort(m_groupsWhichNeedRetainerInfo.begin(), m_groupsWhichNeedRetainerInfo.end());
        Node* alreadyAdded = 0;
        v8::HeapProfiler* profiler = m_isolate->GetHeapProfiler();
        for (size_t i = 0; i < m_groupsWhichNeedRetainerInfo.size(); ++i) {
            Node* root = m_groupsWhichNeedRetainerInfo[i];
            if (root != alreadyAdded) {
                profiler->SetRetainedObjectInfo(v8::UniqueId(reinterpret_cast<intptr_t>(root)), new RetainedDOMInfo(root));
                alreadyAdded = root;
            }
        }
    }

private:
    v8::UniqueId liveRootId()
    {
        const v8::Persistent<v8::Value>& liveRoot = V8PerIsolateData::from(m_isolate)->ensureLiveRoot();
        const intptr_t* idPointer = reinterpret_cast<const intptr_t*>(&liveRoot);
        v8::UniqueId id(*idPointer);
        if (!m_liveRootGroupIdSet) {
            m_isolate->SetObjectGroupId(liveRoot, id);
            m_liveRootGroupIdSet = true;
        }
        return id;
    }

    v8::Isolate* m_isolate;
    Vector<Node*> m_groupsWhichNeedRetainerInfo;
    bool m_liveRootGroupIdSet;
    bool m_constructRetainedObjectInfos;
};

void V8GCController::gcPrologue(v8::GCType type, v8::GCCallbackFlags flags)
{
    // FIXME: It would be nice if the GC callbacks passed the Isolate directly....
    v8::Isolate* isolate = v8::Isolate::GetCurrent();
    if (type == v8::kGCTypeScavenge)
        minorGCPrologue(isolate);
    else if (type == v8::kGCTypeMarkSweepCompact)
        majorGCPrologue(flags & v8::kGCCallbackFlagConstructRetainedObjectInfos, isolate);
}

void V8GCController::minorGCPrologue(v8::Isolate* isolate)
{
    TRACE_EVENT_BEGIN0("v8", "minorGC");
    if (isMainThread()) {
        {
            TRACE_EVENT_SCOPED_SAMPLING_STATE("Blink", "DOMMinorGC");
            v8::HandleScope scope(isolate);
            MinorGCWrapperVisitor visitor(isolate);
            v8::V8::VisitHandlesForPartialDependence(isolate, &visitor);
            visitor.notifyFinished();
        }
        V8PerIsolateData::from(isolate)->setPreviousSamplingState(TRACE_EVENT_GET_SAMPLING_STATE());
        TRACE_EVENT_SET_SAMPLING_STATE("V8", "V8MinorGC");
    }
}

// Create object groups for DOM tree nodes.
void V8GCController::majorGCPrologue(bool constructRetainedObjectInfos, v8::Isolate* isolate)
{
    v8::HandleScope scope(isolate);
    TRACE_EVENT_BEGIN0("v8", "majorGC");
    if (isMainThread()) {
        {
            TRACE_EVENT_SCOPED_SAMPLING_STATE("Blink", "DOMMajorGC");
            MajorGCWrapperVisitor visitor(isolate, constructRetainedObjectInfos);
            v8::V8::VisitHandlesWithClassIds(&visitor);
            visitor.notifyFinished();
        }
        V8PerIsolateData::from(isolate)->setPreviousSamplingState(TRACE_EVENT_GET_SAMPLING_STATE());
        TRACE_EVENT_SET_SAMPLING_STATE("V8", "V8MajorGC");
    } else {
        MajorGCWrapperVisitor visitor(isolate, constructRetainedObjectInfos);
        v8::V8::VisitHandlesWithClassIds(&visitor);
        visitor.notifyFinished();
    }
}

void V8GCController::gcEpilogue(v8::GCType type, v8::GCCallbackFlags flags)
{
    // FIXME: It would be nice if the GC callbacks passed the Isolate directly....
    v8::Isolate* isolate = v8::Isolate::GetCurrent();
    if (type == v8::kGCTypeScavenge)
        minorGCEpilogue(isolate);
    else if (type == v8::kGCTypeMarkSweepCompact)
        majorGCEpilogue(isolate);

    // Force a Blink heap garbage collection when a garbage collection
    // was forced from V8. This is used for tests that force GCs from
    // JavaScript to verify that objects die when expected.
    if (flags & v8::kGCCallbackFlagForced)
        Heap::collectGarbage(ThreadState::HeapPointersOnStack, Heap::ForcedForTesting);
}

void V8GCController::minorGCEpilogue(v8::Isolate* isolate)
{
    TRACE_EVENT_END0("v8", "minorGC");
    if (isMainThread())
        TRACE_EVENT_SET_NONCONST_SAMPLING_STATE(V8PerIsolateData::from(isolate)->previousSamplingState());
}

void V8GCController::majorGCEpilogue(v8::Isolate* isolate)
{
    v8::HandleScope scope(isolate);

    TRACE_EVENT_END0("v8", "majorGC");
    if (isMainThread())
        TRACE_EVENT_SET_NONCONST_SAMPLING_STATE(V8PerIsolateData::from(isolate)->previousSamplingState());
}

void V8GCController::collectGarbage(v8::Isolate* isolate)
{
    V8ExecutionScope scope(isolate);
    V8ScriptRunner::compileAndRunInternalScript(v8String(isolate, "if (gc) gc();"), isolate);
}

}  // namespace WebCore
