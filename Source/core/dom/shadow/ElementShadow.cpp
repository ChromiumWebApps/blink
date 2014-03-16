/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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
#include "core/dom/shadow/ElementShadow.h"


#include "core/css/StyleSheetList.h"
#include "core/dom/ContainerNodeAlgorithms.h"
#include "core/dom/ElementTraversal.h"
#include "core/dom/NodeTraversal.h"
#include "core/dom/shadow/ContentDistribution.h"
#include "core/html/HTMLContentElement.h"
#include "core/html/HTMLShadowElement.h"

namespace WebCore {

class DistributionPool {
public:
    explicit DistributionPool(const ContainerNode&);
    void clear();
    ~DistributionPool();
    void distributeTo(InsertionPoint*, ElementShadow*);
    void populateChildren(const ContainerNode&);

private:
    void detachNonDistributedNodes();
    Vector<Node*, 32> m_nodes;
    Vector<bool, 32> m_distributed;
};

inline DistributionPool::DistributionPool(const ContainerNode& parent)
{
    populateChildren(parent);
}

inline void DistributionPool::clear()
{
    detachNonDistributedNodes();
    m_nodes.clear();
    m_distributed.clear();
}

inline void DistributionPool::populateChildren(const ContainerNode& parent)
{
    clear();
    for (Node* child = parent.firstChild(); child; child = child->nextSibling()) {
        if (isActiveInsertionPoint(*child)) {
            InsertionPoint* insertionPoint = toInsertionPoint(child);
            for (size_t i = 0; i < insertionPoint->size(); ++i)
                m_nodes.append(insertionPoint->at(i));
        } else {
            m_nodes.append(child);
        }
    }
    m_distributed.resize(m_nodes.size());
    m_distributed.fill(false);
}

void DistributionPool::distributeTo(InsertionPoint* insertionPoint, ElementShadow* elementShadow)
{
    ContentDistribution distribution;

    for (size_t i = 0; i < m_nodes.size(); ++i) {
        if (m_distributed[i])
            continue;

        if (isHTMLContentElement(*insertionPoint) && !toHTMLContentElement(insertionPoint)->canSelectNode(m_nodes, i))
            continue;

        Node* node = m_nodes[i];
        distribution.append(node);
        elementShadow->didDistributeNode(node, insertionPoint);
        m_distributed[i] = true;
    }

    // Distributes fallback elements
    if (insertionPoint->isContentInsertionPoint() && distribution.isEmpty()) {
        for (Node* fallbackNode = insertionPoint->firstChild(); fallbackNode; fallbackNode = fallbackNode->nextSibling()) {
            distribution.append(fallbackNode);
            elementShadow->didDistributeNode(fallbackNode, insertionPoint);
        }
    }
    insertionPoint->setDistribution(distribution);
}

inline DistributionPool::~DistributionPool()
{
    detachNonDistributedNodes();
}

inline void DistributionPool::detachNonDistributedNodes()
{
    for (size_t i = 0; i < m_nodes.size(); ++i) {
        if (m_distributed[i])
            continue;
        if (m_nodes[i]->renderer())
            m_nodes[i]->lazyReattachIfAttached();
    }
}

PassOwnPtr<ElementShadow> ElementShadow::create()
{
    return adoptPtr(new ElementShadow());
}

ElementShadow::ElementShadow()
    : m_needsDistributionRecalc(false)
    , m_applyAuthorStyles(false)
    , m_needsSelectFeatureSet(false)
{
}

ElementShadow::~ElementShadow()
{
    removeDetachedShadowRoots();
}

ShadowRoot& ElementShadow::addShadowRoot(Element& shadowHost, ShadowRoot::ShadowRootType type)
{
    RefPtr<ShadowRoot> shadowRoot = ShadowRoot::create(shadowHost.document(), type);

    for (ShadowRoot* root = youngestShadowRoot(); root; root = root->olderShadowRoot())
        root->lazyReattachIfAttached();

    shadowRoot->setParentOrShadowHostNode(&shadowHost);
    shadowRoot->setParentTreeScope(shadowHost.treeScope());
    m_shadowRoots.push(shadowRoot.get());
    ChildNodeInsertionNotifier(shadowHost).notify(*shadowRoot);
    setNeedsDistributionRecalc();

    // addShadowRoot() affects apply-author-styles. However, we know that the youngest shadow root has not had any children yet.
    // The youngest shadow root's apply-author-styles is default (false). So we can just set m_applyAuthorStyles false.
    m_applyAuthorStyles = false;

    shadowHost.didAddShadowRoot(*shadowRoot);
    InspectorInstrumentation::didPushShadowRoot(&shadowHost, shadowRoot.get());

    ASSERT(m_shadowRoots.head());
    ASSERT(shadowRoot.get() == m_shadowRoots.head());
    return *m_shadowRoots.head();
}

void ElementShadow::removeDetachedShadowRoots()
{
    // Dont protect this ref count.
    Element* shadowHost = host();
    ASSERT(shadowHost);

    while (RefPtr<ShadowRoot> oldRoot = m_shadowRoots.head()) {
        InspectorInstrumentation::willPopShadowRoot(shadowHost, oldRoot.get());
        shadowHost->document().removeFocusedElementOfSubtree(oldRoot.get());
        m_shadowRoots.removeHead();
        oldRoot->setParentOrShadowHostNode(0);
        oldRoot->setParentTreeScope(shadowHost->document());
        oldRoot->setPrev(0);
        oldRoot->setNext(0);
    }

}

void ElementShadow::attach(const Node::AttachContext& context)
{
    Node::AttachContext childrenContext(context);
    childrenContext.resolvedStyle = 0;

    for (ShadowRoot* root = youngestShadowRoot(); root; root = root->olderShadowRoot()) {
        if (root->needsAttach())
            root->attach(childrenContext);
    }
}

void ElementShadow::detach(const Node::AttachContext& context)
{
    Node::AttachContext childrenContext(context);
    childrenContext.resolvedStyle = 0;

    for (ShadowRoot* root = youngestShadowRoot(); root; root = root->olderShadowRoot())
        root->detach(childrenContext);
}

void ElementShadow::removeAllEventListeners()
{
    for (ShadowRoot* root = youngestShadowRoot(); root; root = root->olderShadowRoot()) {
        for (Node* node = root; node; node = NodeTraversal::next(*node))
            node->removeAllEventListeners();
    }
}

void ElementShadow::setNeedsDistributionRecalc()
{
    if (m_needsDistributionRecalc)
        return;
    m_needsDistributionRecalc = true;
    host()->markAncestorsWithChildNeedsDistributionRecalc();
    clearDistribution();
}

bool ElementShadow::didAffectApplyAuthorStyles()
{
    bool applyAuthorStyles = resolveApplyAuthorStyles();

    if (m_applyAuthorStyles == applyAuthorStyles)
        return false;

    m_applyAuthorStyles = applyAuthorStyles;
    return true;
}

bool ElementShadow::containsActiveStyles() const
{
    for (ShadowRoot* root = youngestShadowRoot(); root; root = root->olderShadowRoot()) {
        if (root->hasScopedHTMLStyleChild())
            return true;
        if (!root->containsShadowElements())
            return false;
    }
    return false;
}

bool ElementShadow::hasSameStyles(ElementShadow *other) const
{
    ShadowRoot* root = youngestShadowRoot();
    ShadowRoot* otherRoot = other->youngestShadowRoot();
    while (root || otherRoot) {
        if (!root || !otherRoot)
            return false;

        StyleSheetList* list = root->styleSheets();
        StyleSheetList* otherList = otherRoot->styleSheets();

        if (list->length() != otherList->length())
            return false;

        for (size_t i = 0; i < list->length(); i++) {
            if (toCSSStyleSheet(list->item(i))->contents() != toCSSStyleSheet(otherList->item(i))->contents())
                return false;
        }
        root = root->olderShadowRoot();
        otherRoot = otherRoot->olderShadowRoot();
    }

    return true;
}

bool ElementShadow::resolveApplyAuthorStyles() const
{
    for (const ShadowRoot* shadowRoot = youngestShadowRoot(); shadowRoot; shadowRoot = shadowRoot->olderShadowRoot()) {
        if (shadowRoot->applyAuthorStyles())
            return true;
        if (!shadowRoot->containsShadowElements())
            break;
    }
    return false;
}

const InsertionPoint* ElementShadow::finalDestinationInsertionPointFor(const Node* key) const
{
    NodeToDestinationInsertionPoints::const_iterator it = m_nodeToInsertionPoints.find(key);
    return it == m_nodeToInsertionPoints.end() ? 0: it->value.last().get();
}

const DestinationInsertionPoints* ElementShadow::destinationInsertionPointsFor(const Node* key) const
{
    NodeToDestinationInsertionPoints::const_iterator it = m_nodeToInsertionPoints.find(key);
    return it == m_nodeToInsertionPoints.end() ? 0: &it->value;
}

void ElementShadow::distribute()
{
    host()->setNeedsStyleRecalc(SubtreeStyleChange);
    Vector<HTMLShadowElement*, 32> shadowInsertionPoints;
    DistributionPool pool(*host());

    for (ShadowRoot* root = youngestShadowRoot(); root; root = root->olderShadowRoot()) {
        HTMLShadowElement* shadowInsertionPoint = 0;
        const Vector<RefPtr<InsertionPoint> >& insertionPoints = root->descendantInsertionPoints();
        for (size_t i = 0; i < insertionPoints.size(); ++i) {
            InsertionPoint* point = insertionPoints[i].get();
            if (!point->isActive())
                continue;
            if (isHTMLShadowElement(*point)) {
                ASSERT(!shadowInsertionPoint);
                shadowInsertionPoint = toHTMLShadowElement(point);
                shadowInsertionPoints.append(shadowInsertionPoint);
            } else {
                pool.distributeTo(point, this);
                if (ElementShadow* shadow = shadowWhereNodeCanBeDistributed(*point))
                    shadow->setNeedsDistributionRecalc();
            }
        }
    }

    for (size_t i = shadowInsertionPoints.size(); i > 0; --i) {
        HTMLShadowElement* shadowInsertionPoint = shadowInsertionPoints[i - 1];
        ShadowRoot* root = shadowInsertionPoint->containingShadowRoot();
        ASSERT(root);
        if (root->isOldest()) {
            pool.distributeTo(shadowInsertionPoint, this);
        } else if (root->olderShadowRoot()->type() == root->type()) {
            // Only allow reprojecting older shadow roots between the same type to
            // disallow reprojecting UA elements into author shadows.
            DistributionPool olderShadowRootPool(*root->olderShadowRoot());
            olderShadowRootPool.distributeTo(shadowInsertionPoint, this);
            root->olderShadowRoot()->setShadowInsertionPointOfYoungerShadowRoot(shadowInsertionPoint);
        }
        if (ElementShadow* shadow = shadowWhereNodeCanBeDistributed(*shadowInsertionPoint))
            shadow->setNeedsDistributionRecalc();
    }
}

void ElementShadow::didDistributeNode(const Node* node, InsertionPoint* insertionPoint)
{
    NodeToDestinationInsertionPoints::AddResult result = m_nodeToInsertionPoints.add(node, DestinationInsertionPoints());
    result.storedValue->value.append(insertionPoint);
}

const SelectRuleFeatureSet& ElementShadow::ensureSelectFeatureSet()
{
    if (!m_needsSelectFeatureSet)
        return m_selectFeatures;

    m_selectFeatures.clear();
    for (ShadowRoot* root = oldestShadowRoot(); root; root = root->youngerShadowRoot())
        collectSelectFeatureSetFrom(*root);
    m_needsSelectFeatureSet = false;
    return m_selectFeatures;
}

void ElementShadow::collectSelectFeatureSetFrom(ShadowRoot& root)
{
    if (!root.containsShadowRoots() && !root.containsContentElements())
        return;

    for (Element* element = ElementTraversal::firstWithin(root); element; element = ElementTraversal::next(*element, &root)) {
        if (ElementShadow* shadow = element->shadow())
            m_selectFeatures.add(shadow->ensureSelectFeatureSet());
        if (!isHTMLContentElement(*element))
            continue;
        const CSSSelectorList& list = toHTMLContentElement(*element).selectorList();
        for (const CSSSelector* selector = list.first(); selector; selector = CSSSelectorList::next(*selector)) {
            for (const CSSSelector* component = selector; component; component = component->tagHistory())
                m_selectFeatures.collectFeaturesFromSelector(*component);
        }
    }
}

void ElementShadow::didAffectSelector(AffectedSelectorMask mask)
{
    if (ensureSelectFeatureSet().hasSelectorFor(mask))
        setNeedsDistributionRecalc();
}

void ElementShadow::willAffectSelector()
{
    for (ElementShadow* shadow = this; shadow; shadow = shadow->containingShadow()) {
        if (shadow->needsSelectFeatureSet())
            break;
        shadow->setNeedsSelectFeatureSet();
    }
    setNeedsDistributionRecalc();
}

void ElementShadow::clearDistribution()
{
    m_nodeToInsertionPoints.clear();

    for (ShadowRoot* root = youngestShadowRoot(); root; root = root->olderShadowRoot())
        root->setShadowInsertionPointOfYoungerShadowRoot(nullptr);
}

} // namespace
