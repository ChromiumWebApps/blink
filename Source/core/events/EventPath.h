/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
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

#ifndef EventPath_h
#define EventPath_h

#include "core/events/NodeEventContext.h"
#include "core/events/TreeScopeEventContext.h"

#include "wtf/HashMap.h"
#include "wtf/Vector.h"

namespace WebCore {

class Event;
class EventTarget;
class Node;
class TouchEvent;
class TouchList;
class TreeScope;

enum EventDispatchBehavior {
    RetargetEvent,
    StayInsideShadowDOM
};

class EventPath {
public:
    explicit EventPath(Event*);
    explicit EventPath(Node*);
    void resetWith(Node*);

    NodeEventContext& operator[](size_t index) { return m_nodeEventContexts[index]; }
    const NodeEventContext& operator[](size_t index) const { return m_nodeEventContexts[index]; }
    const NodeEventContext& last() const { return m_nodeEventContexts[size() - 1]; }

    bool isEmpty() const { return m_nodeEventContexts.isEmpty(); }
    size_t size() const { return m_nodeEventContexts.size(); }

    void adjustForRelatedTarget(Node*, EventTarget* relatedTarget);
    void adjustForTouchEvent(Node*, TouchEvent&);

    static Node* parent(Node*);
    static EventTarget* eventTargetRespectingTargetRules(Node*);

private:
    EventPath();

    NodeEventContext& at(size_t index) { return m_nodeEventContexts[index]; }

    void addNodeEventContext(Node*);

    void calculatePath();
    void calculateAdjustedTargets();
    void calculateTreeScopePrePostOrderNumbers();

    void shrink(size_t newSize) { m_nodeEventContexts.shrink(newSize); }
    void shrinkIfNeeded(const Node* target, const EventTarget* relatedTarget);

    void adjustTouchList(const Node*, const TouchList*, WillBeHeapVector<RawPtrWillBeMember<TouchList> > adjustedTouchList, const Vector<TreeScope*>& treeScopes);

    typedef HashMap<TreeScope*, RefPtr<TreeScopeEventContext> > TreeScopeEventContextMap;
    TreeScopeEventContext* ensureTreeScopeEventContext(Node* currentTarget, TreeScope*, TreeScopeEventContextMap&);

    typedef HashMap<TreeScope*, EventTarget*> RelatedTargetMap;

    static void buildRelatedNodeMap(const Node*, RelatedTargetMap&);
    static EventTarget* findRelatedNode(TreeScope*, RelatedTargetMap&);

#ifndef NDEBUG
    static void checkReachability(TreeScope&, TouchList&);
#endif

    Vector<NodeEventContext, 64> m_nodeEventContexts;
    Node* m_node;
    Event* m_event;
    Vector<RefPtr<TreeScopeEventContext> > m_treeScopeEventContexts;
};

} // namespace

#endif
