/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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
#include "core/dom/shadow/ShadowRoot.h"

#include "bindings/v8/ExceptionState.h"
#include "core/css/StyleSheetList.h"
#include "core/css/resolver/StyleResolver.h"
#include "core/dom/ElementTraversal.h"
#include "core/dom/SiblingRuleHelper.h"
#include "core/dom/StyleEngine.h"
#include "core/dom/Text.h"
#include "core/dom/shadow/ElementShadow.h"
#include "core/dom/shadow/InsertionPoint.h"
#include "core/dom/shadow/ShadowRootRareData.h"
#include "core/editing/markup.h"
#include "core/html/HTMLShadowElement.h"
#include "public/platform/Platform.h"

namespace WebCore {

struct SameSizeAsShadowRoot : public DocumentFragment, public TreeScope, public DoublyLinkedListNode<ShadowRoot> {
    void* pointers[3];
    unsigned countersAndFlags[1];
};

COMPILE_ASSERT(sizeof(ShadowRoot) == sizeof(SameSizeAsShadowRoot), shadowroot_should_stay_small);

ShadowRoot::ShadowRoot(Document& document, ShadowRootType type)
    : DocumentFragment(0, CreateShadowRoot)
    , TreeScope(*this, document)
    , m_prev(0)
    , m_next(0)
    , m_numberOfStyles(0)
    , m_applyAuthorStyles(false)
    , m_resetStyleInheritance(false)
    , m_type(type)
    , m_registeredWithParentShadowRoot(false)
    , m_descendantInsertionPointsIsValid(false)
{
    ScriptWrappable::init(this);
}

ShadowRoot::~ShadowRoot()
{
    ASSERT(!m_prev);
    ASSERT(!m_next);

    if (m_shadowRootRareData && m_shadowRootRareData->styleSheets())
        m_shadowRootRareData->styleSheets()->detachFromDocument();

    document().styleEngine()->didRemoveShadowRoot(this);

    // We cannot let ContainerNode destructor call willBeDeletedFromDocument()
    // for this ShadowRoot instance because TreeScope destructor
    // clears Node::m_treeScope thus ContainerNode is no longer able
    // to access it Document reference after that.
    willBeDeletedFromDocument();

    // We must remove all of our children first before the TreeScope destructor
    // runs so we don't go through TreeScopeAdopter for each child with a
    // destructed tree scope in each descendant.
    removeDetachedChildren();

    // We must call clearRareData() here since a ShadowRoot class inherits TreeScope
    // as well as Node. See a comment on TreeScope.h for the reason.
    if (hasRareData())
        clearRareData();
}

void ShadowRoot::dispose()
{
    removeDetachedChildren();
}

ShadowRoot* ShadowRoot::olderShadowRootForBindings() const
{
    ShadowRoot* older = olderShadowRoot();
    while (older && !older->shouldExposeToBindings())
        older = older->olderShadowRoot();
    ASSERT(!older || older->shouldExposeToBindings());
    return older;
}

bool ShadowRoot::isOldestAuthorShadowRoot() const
{
    if (type() != AuthorShadowRoot)
        return false;
    if (ShadowRoot* older = olderShadowRoot())
        return older->type() == UserAgentShadowRoot;
    return true;
}

PassRefPtr<Node> ShadowRoot::cloneNode(bool, ExceptionState& exceptionState)
{
    exceptionState.throwDOMException(DataCloneError, "ShadowRoot nodes are not clonable.");
    return nullptr;
}

String ShadowRoot::innerHTML() const
{
    return createMarkup(this, ChildrenOnly);
}

void ShadowRoot::setInnerHTML(const String& markup, ExceptionState& exceptionState)
{
    if (isOrphan()) {
        exceptionState.throwDOMException(InvalidAccessError, "The ShadowRoot does not have a host.");
        return;
    }

    if (RefPtr<DocumentFragment> fragment = createFragmentForInnerOuterHTML(markup, host(), AllowScriptingContent, "innerHTML", exceptionState))
        replaceChildrenWithFragment(this, fragment.release(), exceptionState);
}

void ShadowRoot::recalcStyle(StyleRecalcChange change)
{
    // ShadowRoot doesn't support custom callbacks.
    ASSERT(!hasCustomStyleCallbacks());

    // If we're propagating an Inherit change and this ShadowRoot resets
    // inheritance we don't need to look at the children.
    if (change <= Inherit && resetStyleInheritance() && !needsStyleRecalc() && !childNeedsStyleRecalc())
        return;

    StyleResolver& styleResolver = document().ensureStyleResolver();
    styleResolver.pushParentShadowRoot(*this);

    if (styleChangeType() >= SubtreeStyleChange)
        change = Force;

    if (change < Force && childNeedsStyleRecalc())
        SiblingRuleHelper(this).checkForChildrenAdjacentRuleChanges();

    // There's no style to update so just calling recalcStyle means we're updated.
    clearNeedsStyleRecalc();

    // FIXME: This doesn't handle :hover + div properly like Element::recalcStyle does.
    Text* lastTextNode = 0;
    for (Node* child = lastChild(); child; child = child->previousSibling()) {
        if (child->isTextNode()) {
            toText(child)->recalcTextStyle(change, lastTextNode);
            lastTextNode = toText(child);
        } else if (child->isElementNode()) {
            if (child->shouldCallRecalcStyle(change))
                toElement(child)->recalcStyle(change, lastTextNode);
            if (child->renderer())
                lastTextNode = 0;
        }
    }

    styleResolver.popParentShadowRoot(*this);

    clearChildNeedsStyleRecalc();
}

bool ShadowRoot::isActiveForStyling() const
{
    if (!youngerShadowRoot())
        return true;

    if (InsertionPoint* point = shadowInsertionPointOfYoungerShadowRoot()) {
        if (point->containingShadowRoot())
            return true;
    }
    return false;
}

void ShadowRoot::setApplyAuthorStyles(bool value)
{
    if (isOrphan())
        return;

    if (applyAuthorStyles() == value)
        return;

    m_applyAuthorStyles = value;
    if (!isActiveForStyling())
        return;

    ASSERT(host());
    ASSERT(host()->shadow());
    if (host()->shadow()->didAffectApplyAuthorStyles())
        host()->setNeedsStyleRecalc(SubtreeStyleChange);

    // Since styles in shadow trees can select shadow hosts, set shadow host's needs-recalc flag true.
    // FIXME: host->setNeedsStyleRecalc() should take care of all elements in its shadow tree.
    // However, when host's recalcStyle is skipped (i.e. host's parent has no renderer),
    // no recalc style is invoked for any elements in its shadow tree.
    // This problem occurs when using getComputedStyle() API.
    // So currently host and shadow root's needsStyleRecalc flags are set to be true.
    setNeedsStyleRecalc(SubtreeStyleChange);
}

void ShadowRoot::setResetStyleInheritance(bool value)
{
    if (isOrphan())
        return;

    if (value == resetStyleInheritance())
        return;

    m_resetStyleInheritance = value;
    if (!isActiveForStyling())
        return;

    setNeedsStyleRecalc(SubtreeStyleChange);
}

void ShadowRoot::attach(const AttachContext& context)
{
    StyleResolver& styleResolver = document().ensureStyleResolver();
    styleResolver.pushParentShadowRoot(*this);
    DocumentFragment::attach(context);
    styleResolver.popParentShadowRoot(*this);
}

Node::InsertionNotificationRequest ShadowRoot::insertedInto(ContainerNode* insertionPoint)
{
    DocumentFragment::insertedInto(insertionPoint);

    if (!insertionPoint->inDocument() || !isOldest())
        return InsertionDone;

    // FIXME: When parsing <video controls>, insertedInto() is called many times without invoking removedFrom.
    // For now, we check m_registeredWithParentShadowroot. We would like to ASSERT(!m_registeredShadowRoot) here.
    // https://bugs.webkit.org/show_bug.cig?id=101316
    if (m_registeredWithParentShadowRoot)
        return InsertionDone;

    if (ShadowRoot* root = host()->containingShadowRoot()) {
        root->addChildShadowRoot();
        m_registeredWithParentShadowRoot = true;
    }

    return InsertionDone;
}

void ShadowRoot::removedFrom(ContainerNode* insertionPoint)
{
    if (insertionPoint->inDocument() && m_registeredWithParentShadowRoot) {
        ShadowRoot* root = host()->containingShadowRoot();
        if (!root)
            root = insertionPoint->containingShadowRoot();
        if (root)
            root->removeChildShadowRoot();
        m_registeredWithParentShadowRoot = false;
    }

    DocumentFragment::removedFrom(insertionPoint);
}

void ShadowRoot::childrenChanged(bool changedByParser, Node* beforeChange, Node* afterChange, int childCountDelta)
{
    ContainerNode::childrenChanged(changedByParser, beforeChange, afterChange, childCountDelta);
    if (InsertionPoint* point = shadowInsertionPointOfYoungerShadowRoot()) {
        if (ShadowRoot* root = point->containingShadowRoot())
            root->owner()->setNeedsDistributionRecalc();
    }
}

void ShadowRoot::registerScopedHTMLStyleChild()
{
    ++m_numberOfStyles;
    setHasScopedHTMLStyleChild(true);
}

void ShadowRoot::unregisterScopedHTMLStyleChild()
{
    ASSERT(hasScopedHTMLStyleChild() && m_numberOfStyles > 0);
    --m_numberOfStyles;
    setHasScopedHTMLStyleChild(m_numberOfStyles > 0);
}

ShadowRootRareData* ShadowRoot::ensureShadowRootRareData()
{
    if (m_shadowRootRareData)
        return m_shadowRootRareData.get();

    m_shadowRootRareData = adoptPtr(new ShadowRootRareData);
    return m_shadowRootRareData.get();
}

bool ShadowRoot::containsShadowElements() const
{
    return m_shadowRootRareData ? m_shadowRootRareData->containsShadowElements() : 0;
}

bool ShadowRoot::containsContentElements() const
{
    return m_shadowRootRareData ? m_shadowRootRareData->containsContentElements() : 0;
}

bool ShadowRoot::containsShadowRoots() const
{
    return m_shadowRootRareData ? m_shadowRootRareData->containsShadowRoots() : 0;
}

unsigned ShadowRoot::descendantShadowElementCount() const
{
    return m_shadowRootRareData ? m_shadowRootRareData->descendantShadowElementCount() : 0;
}

HTMLShadowElement* ShadowRoot::shadowInsertionPointOfYoungerShadowRoot() const
{
    return m_shadowRootRareData ? m_shadowRootRareData->shadowInsertionPointOfYoungerShadowRoot() : 0;
}

void ShadowRoot::setShadowInsertionPointOfYoungerShadowRoot(PassRefPtr<HTMLShadowElement> shadowInsertionPoint)
{
    if (!m_shadowRootRareData && !shadowInsertionPoint)
        return;
    ensureShadowRootRareData()->setShadowInsertionPointOfYoungerShadowRoot(shadowInsertionPoint);
}

void ShadowRoot::didAddInsertionPoint(InsertionPoint* insertionPoint)
{
    ensureShadowRootRareData()->didAddInsertionPoint(insertionPoint);
    invalidateDescendantInsertionPoints();
}

void ShadowRoot::didRemoveInsertionPoint(InsertionPoint* insertionPoint)
{
    m_shadowRootRareData->didRemoveInsertionPoint(insertionPoint);
    invalidateDescendantInsertionPoints();
}

void ShadowRoot::addChildShadowRoot()
{
    ensureShadowRootRareData()->didAddChildShadowRoot();
}

void ShadowRoot::removeChildShadowRoot()
{
    // FIXME: Why isn't this an ASSERT?
    if (!m_shadowRootRareData)
        return;
    m_shadowRootRareData->didRemoveChildShadowRoot();
}

unsigned ShadowRoot::childShadowRootCount() const
{
    return m_shadowRootRareData ? m_shadowRootRareData->childShadowRootCount() : 0;
}

void ShadowRoot::invalidateDescendantInsertionPoints()
{
    m_descendantInsertionPointsIsValid = false;
    m_shadowRootRareData->clearDescendantInsertionPoints();
}

const Vector<RefPtr<InsertionPoint> >& ShadowRoot::descendantInsertionPoints()
{
    DEFINE_STATIC_LOCAL(const Vector<RefPtr<InsertionPoint> >, emptyList, ());

    if (m_shadowRootRareData && m_descendantInsertionPointsIsValid)
        return m_shadowRootRareData->descendantInsertionPoints();

    m_descendantInsertionPointsIsValid = true;

    if (!containsInsertionPoints())
        return emptyList;

    Vector<RefPtr<InsertionPoint> > insertionPoints;
    for (Element* element = ElementTraversal::firstWithin(*this); element; element = ElementTraversal::next(*element, this)) {
        if (element->isInsertionPoint())
            insertionPoints.append(toInsertionPoint(element));
    }

    ensureShadowRootRareData()->setDescendantInsertionPoints(insertionPoints);

    return m_shadowRootRareData->descendantInsertionPoints();
}

StyleSheetList* ShadowRoot::styleSheets()
{
    if (!ensureShadowRootRareData()->styleSheets())
        m_shadowRootRareData->setStyleSheets(StyleSheetList::create(this));

    return m_shadowRootRareData->styleSheets();
}

bool ShadowRoot::childrenSupportStyleSharing() const
{
    if (!m_shadowRootRareData)
        return false;
    return !m_shadowRootRareData->childrenAffectedByFirstChildRules()
        && !m_shadowRootRareData->childrenAffectedByLastChildRules()
        && !m_shadowRootRareData->childrenAffectedByDirectAdjacentRules()
        && !m_shadowRootRareData->childrenAffectedByForwardPositionalRules()
        && !m_shadowRootRareData->childrenAffectedByBackwardPositionalRules();
}

bool ShadowRoot::childrenAffectedByPositionalRules() const
{
    return m_shadowRootRareData && (m_shadowRootRareData->childrenAffectedByForwardPositionalRules() || m_shadowRootRareData->childrenAffectedByBackwardPositionalRules());
}

bool ShadowRoot::childrenAffectedByFirstChildRules() const
{
    return m_shadowRootRareData && m_shadowRootRareData->childrenAffectedByFirstChildRules();
}

bool ShadowRoot::childrenAffectedByLastChildRules() const
{
    return m_shadowRootRareData && m_shadowRootRareData->childrenAffectedByLastChildRules();
}

bool ShadowRoot::childrenAffectedByDirectAdjacentRules() const
{
    return m_shadowRootRareData && m_shadowRootRareData->childrenAffectedByDirectAdjacentRules();
}

bool ShadowRoot::childrenAffectedByForwardPositionalRules() const
{
    return m_shadowRootRareData && m_shadowRootRareData->childrenAffectedByForwardPositionalRules();
}

bool ShadowRoot::childrenAffectedByBackwardPositionalRules() const
{
    return m_shadowRootRareData && m_shadowRootRareData->childrenAffectedByBackwardPositionalRules();
}

void ShadowRoot::setChildrenAffectedByForwardPositionalRules()
{
    ensureShadowRootRareData()->setChildrenAffectedByForwardPositionalRules(true);
}

void ShadowRoot::setChildrenAffectedByDirectAdjacentRules()
{
    ensureShadowRootRareData()->setChildrenAffectedByDirectAdjacentRules(true);
}

void ShadowRoot::setChildrenAffectedByBackwardPositionalRules()
{
    ensureShadowRootRareData()->setChildrenAffectedByBackwardPositionalRules(true);
}

void ShadowRoot::setChildrenAffectedByFirstChildRules()
{
    ensureShadowRootRareData()->setChildrenAffectedByFirstChildRules(true);
}

void ShadowRoot::setChildrenAffectedByLastChildRules()
{
    ensureShadowRootRareData()->setChildrenAffectedByLastChildRules(true);
}

}
