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

#include "config.h"
#include "core/events/EventPath.h"

#include "EventNames.h"
#include "RuntimeEnabledFeatures.h"
#include "SVGNames.h"
#include "core/dom/FullscreenElementStack.h"
#include "core/dom/Touch.h"
#include "core/dom/TouchList.h"
#include "core/dom/shadow/InsertionPoint.h"
#include "core/dom/shadow/ShadowRoot.h"
#include "core/events/FocusEvent.h"
#include "core/events/MouseEvent.h"
#include "core/events/TouchEvent.h"
#include "core/events/TouchEventContext.h"
#include "core/html/HTMLMediaElement.h"
#include "core/svg/SVGElementInstance.h"
#include "core/svg/SVGUseElement.h"

namespace WebCore {

Node* EventPath::parent(Node* node)
{
    EventPath eventPath(node);
    return eventPath.size() > 1 ? eventPath[1].node() : 0;
}

EventTarget* EventPath::eventTargetRespectingTargetRules(Node* referenceNode)
{
    ASSERT(referenceNode);

    if (referenceNode->isPseudoElement())
        return referenceNode->parentNode();

    if (!referenceNode->isSVGElement() || !referenceNode->isInShadowTree())
        return referenceNode;

    // Spec: The event handling for the non-exposed tree works as if the referenced element had been textually included
    // as a deeply cloned child of the 'use' element, except that events are dispatched to the SVGElementInstance objects.
    Node& rootNode = referenceNode->treeScope().rootNode();
    Element* shadowHostElement = rootNode.isShadowRoot() ? toShadowRoot(rootNode).host() : 0;
    // At this time, SVG nodes are not supported in non-<use> shadow trees.
    if (!shadowHostElement || !shadowHostElement->hasTagName(SVGNames::useTag))
        return referenceNode;
    SVGUseElement* useElement = toSVGUseElement(shadowHostElement);
    if (SVGElementInstance* instance = useElement->instanceForShadowTreeElement(referenceNode))
        return instance;

    return referenceNode;
}

static inline bool inTheSameScope(ShadowRoot* shadowRoot, EventTarget* target)
{
    return target->toNode() && target->toNode()->treeScope().rootNode() == shadowRoot;
}

static inline EventDispatchBehavior determineDispatchBehavior(Event* event, ShadowRoot* shadowRoot, EventTarget* target)
{
    // Video-only full screen is a mode where we use the shadow DOM as an implementation
    // detail that should not be detectable by the web content.
    if (Element* element = FullscreenElementStack::currentFullScreenElementFrom(target->toNode()->document())) {
        // FIXME: We assume that if the full screen element is a media element that it's
        // the video-only full screen. Both here and elsewhere. But that is probably wrong.
        if (isHTMLMediaElement(*element) && shadowRoot && shadowRoot->host() == element)
            return StayInsideShadowDOM;
    }

    // WebKit never allowed selectstart event to cross the the shadow DOM boundary.
    // Changing this breaks existing sites.
    // See https://bugs.webkit.org/show_bug.cgi?id=52195 for details.
    const AtomicString eventType = event->type();
    if (inTheSameScope(shadowRoot, target)
        && (eventType == EventTypeNames::abort
            || eventType == EventTypeNames::change
            || eventType == EventTypeNames::error
            || eventType == EventTypeNames::load
            || eventType == EventTypeNames::reset
            || eventType == EventTypeNames::resize
            || eventType == EventTypeNames::scroll
            || eventType == EventTypeNames::select
            || eventType == EventTypeNames::selectstart))
        return StayInsideShadowDOM;

    return RetargetEvent;
}

EventPath::EventPath(Event* event)
    : m_node(0)
    , m_event(event)
{
}

EventPath::EventPath(Node* node)
    : m_node(node)
    , m_event(0)
{
    resetWith(node);
}

void EventPath::resetWith(Node* node)
{
    ASSERT(node);
    m_node = node;
    m_nodeEventContexts.clear();
    m_treeScopeEventContexts.clear();
    calculatePath();
    calculateAdjustedTargets();
    if (RuntimeEnabledFeatures::shadowDOMEnabled() && !node->isSVGElement())
        calculateTreeScopePrePostOrderNumbers();
}

void EventPath::addNodeEventContext(Node* node)
{
    m_nodeEventContexts.append(NodeEventContext(node, eventTargetRespectingTargetRules(node)));
}

void EventPath::calculatePath()
{
    ASSERT(m_node);
    ASSERT(m_nodeEventContexts.isEmpty());
    m_node->document().updateDistributionForNodeIfNeeded(const_cast<Node*>(m_node));

    Node* current = m_node;
    addNodeEventContext(current);
    if (!m_node->inDocument())
        return;
    while (current) {
        if (current->isShadowRoot() && m_event && determineDispatchBehavior(m_event, toShadowRoot(current), m_node) == StayInsideShadowDOM)
            break;
        Vector<InsertionPoint*, 8> insertionPoints;
        collectDestinationInsertionPoints(*current, insertionPoints);
        if (!insertionPoints.isEmpty()) {
            for (size_t i = 0; i < insertionPoints.size(); ++i) {
                InsertionPoint* insertionPoint = insertionPoints[i];
                if (insertionPoint->isShadowInsertionPoint()) {
                    ShadowRoot* containingShadowRoot = insertionPoint->containingShadowRoot();
                    ASSERT(containingShadowRoot);
                    if (!containingShadowRoot->isOldest())
                        addNodeEventContext(containingShadowRoot->olderShadowRoot());
                }
                addNodeEventContext(insertionPoint);
            }
            current = insertionPoints.last();
            continue;
        }
        if (current->isShadowRoot()) {
            current = current->shadowHost();
            addNodeEventContext(current);
        } else {
            current = current->parentNode();
            if (current)
                addNodeEventContext(current);
        }
    }
}

void EventPath::calculateTreeScopePrePostOrderNumbers()
{
    // Precondition:
    //   - TreeScopes in m_treeScopeEventContexts must be *connected* in the same tree of trees.
    //   - The root tree must be included.
    HashMap<const TreeScope*, TreeScopeEventContext*> treeScopeEventContextMap;
    for (size_t i = 0; i < m_treeScopeEventContexts.size(); ++i)
        treeScopeEventContextMap.add(&m_treeScopeEventContexts[i]->treeScope(), m_treeScopeEventContexts[i].get());
    TreeScopeEventContext* rootTree = 0;
    for (size_t i = 0; i < m_treeScopeEventContexts.size(); ++i) {
        TreeScopeEventContext* treeScopeEventContext = m_treeScopeEventContexts[i].get();
        // Use olderShadowRootOrParentTreeScope here for parent-child relationships.
        // See the definition of trees of trees in the Shado DOM spec: http://w3c.github.io/webcomponents/spec/shadow/
        TreeScope* parent = treeScopeEventContext->treeScope().olderShadowRootOrParentTreeScope();
        if (!parent) {
            ASSERT(!rootTree);
            rootTree = treeScopeEventContext;
            continue;
        }
        ASSERT(treeScopeEventContextMap.find(parent) != treeScopeEventContextMap.end());
        treeScopeEventContextMap.find(parent)->value->addChild(*treeScopeEventContext);
    }
    ASSERT(rootTree);
    rootTree->calculatePrePostOrderNumber(0);
}

TreeScopeEventContext* EventPath::ensureTreeScopeEventContext(Node* currentTarget, TreeScope* treeScope, TreeScopeEventContextMap& treeScopeEventContextMap)
{
    if (!treeScope)
        return 0;
    TreeScopeEventContextMap::AddResult addResult = treeScopeEventContextMap.add(treeScope, TreeScopeEventContext::create(*treeScope));
    TreeScopeEventContext* treeScopeEventContext = addResult.storedValue->value.get();
    if (addResult.isNewEntry) {
        TreeScopeEventContext* parentTreeScopeEventContext = ensureTreeScopeEventContext(0, treeScope->olderShadowRootOrParentTreeScope(), treeScopeEventContextMap);
        if (parentTreeScopeEventContext && parentTreeScopeEventContext->target()) {
            treeScopeEventContext->setTarget(parentTreeScopeEventContext->target());
        } else if (currentTarget) {
            treeScopeEventContext->setTarget(eventTargetRespectingTargetRules(currentTarget));
        }
    } else if (!treeScopeEventContext->target() && currentTarget) {
        treeScopeEventContext->setTarget(eventTargetRespectingTargetRules(currentTarget));
    }
    return treeScopeEventContext;
}

void EventPath::calculateAdjustedTargets()
{
    const TreeScope* lastTreeScope = 0;
    bool isSVGElement = at(0).node()->isSVGElement();

    TreeScopeEventContextMap treeScopeEventContextMap;
    TreeScopeEventContext* lastTreeScopeEventContext = 0;

    for (size_t i = 0; i < size(); ++i) {
        Node* currentNode = at(i).node();
        TreeScope& currentTreeScope = currentNode->treeScope();
        if (lastTreeScope != &currentTreeScope) {
            if (!isSVGElement) {
                lastTreeScopeEventContext = ensureTreeScopeEventContext(currentNode, &currentTreeScope, treeScopeEventContextMap);
            } else {
                TreeScopeEventContextMap::AddResult addResult = treeScopeEventContextMap.add(&currentTreeScope, TreeScopeEventContext::create(currentTreeScope));
                lastTreeScopeEventContext = addResult.storedValue->value.get();
                if (addResult.isNewEntry) {
                    // Don't adjust an event target for SVG.
                    lastTreeScopeEventContext->setTarget(eventTargetRespectingTargetRules(at(0).node()));
                }
            }
        }
        ASSERT(lastTreeScopeEventContext);
        at(i).setTreeScopeEventContext(lastTreeScopeEventContext);
        lastTreeScope = &currentTreeScope;
    }
    m_treeScopeEventContexts.appendRange(treeScopeEventContextMap.values().begin(), treeScopeEventContextMap.values().end());
}

void EventPath::buildRelatedNodeMap(const Node* relatedNode, RelatedTargetMap& relatedTargetMap)
{
    EventPath relatedTargetEventPath(const_cast<Node*>(relatedNode));
    for (size_t i = 0; i < relatedTargetEventPath.m_treeScopeEventContexts.size(); ++i) {
        TreeScopeEventContext* treeScopeEventContext = relatedTargetEventPath.m_treeScopeEventContexts[i].get();
        relatedTargetMap.add(&treeScopeEventContext->treeScope(), treeScopeEventContext->target());
    }
}

EventTarget* EventPath::findRelatedNode(TreeScope* scope, RelatedTargetMap& relatedTargetMap)
{
    Vector<TreeScope*, 32> parentTreeScopes;
    EventTarget* relatedNode = 0;
    while (scope) {
        parentTreeScopes.append(scope);
        RelatedTargetMap::const_iterator iter = relatedTargetMap.find(scope);
        if (iter != relatedTargetMap.end() && iter->value) {
            relatedNode = iter->value;
            break;
        }
        scope = scope->olderShadowRootOrParentTreeScope();
    }
    ASSERT(relatedNode);
    for (Vector<TreeScope*, 32>::iterator iter = parentTreeScopes.begin(); iter < parentTreeScopes.end(); ++iter)
        relatedTargetMap.add(*iter, relatedNode);
    return relatedNode;
}

void EventPath::adjustForRelatedTarget(Node* target, EventTarget* relatedTarget)
{
    if (!target)
        return;
    if (!relatedTarget)
        return;
    Node* relatedNode = relatedTarget->toNode();
    if (!relatedNode)
        return;
    if (target->document() != relatedNode->document())
        return;
    if (!target->inDocument() || !relatedNode->inDocument())
        return;

    RelatedTargetMap relatedNodeMap;
    buildRelatedNodeMap(relatedNode, relatedNodeMap);

    for (size_t i = 0; i < m_treeScopeEventContexts.size(); ++i) {
        TreeScopeEventContext* treeScopeEventContext = m_treeScopeEventContexts[i].get();
        EventTarget* adjustedRelatedTarget = findRelatedNode(&treeScopeEventContext->treeScope(), relatedNodeMap);
        ASSERT(adjustedRelatedTarget);
        treeScopeEventContext->setRelatedTarget(adjustedRelatedTarget);
    }

    shrinkIfNeeded(target, relatedTarget);
}

void EventPath::shrinkIfNeeded(const Node* target, const EventTarget* relatedTarget)
{
    // Synthetic mouse events can have a relatedTarget which is identical to the target.
    bool targetIsIdenticalToToRelatedTarget = (target == relatedTarget);

    for (size_t i = 0; i < size(); ++i) {
        if (targetIsIdenticalToToRelatedTarget) {
            if (target->treeScope().rootNode() == at(i).node()) {
                shrink(i + 1);
                break;
            }
        } else if (at(i).target() == at(i).relatedTarget()) {
            // Event dispatching should be stopped here.
            shrink(i);
            break;
        }
    }
}

void EventPath::adjustForTouchEvent(Node* node, TouchEvent& touchEvent)
{
    WillBeHeapVector<RawPtrWillBeMember<TouchList> > adjustedTouches;
    WillBeHeapVector<RawPtrWillBeMember<TouchList> > adjustedTargetTouches;
    WillBeHeapVector<RawPtrWillBeMember<TouchList> > adjustedChangedTouches;
    Vector<TreeScope*> treeScopes;

    for (size_t i = 0; i < m_treeScopeEventContexts.size(); ++i) {
        TouchEventContext* touchEventContext = m_treeScopeEventContexts[i]->ensureTouchEventContext();
        adjustedTouches.append(&touchEventContext->touches());
        adjustedTargetTouches.append(&touchEventContext->targetTouches());
        adjustedChangedTouches.append(&touchEventContext->changedTouches());
        treeScopes.append(&m_treeScopeEventContexts[i]->treeScope());
    }

    adjustTouchList(node, touchEvent.touches(), adjustedTouches, treeScopes);
    adjustTouchList(node, touchEvent.targetTouches(), adjustedTargetTouches, treeScopes);
    adjustTouchList(node, touchEvent.changedTouches(), adjustedChangedTouches, treeScopes);

#ifndef NDEBUG
    for (size_t i = 0; i < m_treeScopeEventContexts.size(); ++i) {
        TreeScope& treeScope = m_treeScopeEventContexts[i]->treeScope();
        TouchEventContext* touchEventContext = m_treeScopeEventContexts[i]->touchEventContext();
        checkReachability(treeScope, touchEventContext->touches());
        checkReachability(treeScope, touchEventContext->targetTouches());
        checkReachability(treeScope, touchEventContext->changedTouches());
    }
#endif
}

void EventPath::adjustTouchList(const Node* node, const TouchList* touchList, WillBeHeapVector<RawPtrWillBeMember<TouchList> > adjustedTouchList, const Vector<TreeScope*>& treeScopes)
{
    if (!touchList)
        return;
    for (size_t i = 0; i < touchList->length(); ++i) {
        const Touch& touch = *touchList->item(i);
        RelatedTargetMap relatedNodeMap;
        buildRelatedNodeMap(touch.target()->toNode(), relatedNodeMap);
        for (size_t j = 0; j < treeScopes.size(); ++j) {
            adjustedTouchList[j]->append(touch.cloneWithNewTarget(findRelatedNode(treeScopes[j], relatedNodeMap)));
        }
    }
}

#ifndef NDEBUG
void EventPath::checkReachability(TreeScope& treeScope, TouchList& touchList)
{
    for (size_t i = 0; i < touchList.length(); ++i)
        ASSERT(touchList.item(i)->target()->toNode()->treeScope().isInclusiveOlderSiblingShadowRootOrAncestorTreeScopeOf(treeScope));
}
#endif

} // namespace
