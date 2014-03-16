/*
 * Copyright (C) 2006, 2007 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef FocusController_h
#define FocusController_h

#include "core/page/FocusType.h"
#include "platform/geometry/LayoutRect.h"
#include "wtf/Forward.h"
#include "wtf/Noncopyable.h"
#include "wtf/RefPtr.h"

namespace WebCore {

struct FocusCandidate;
class Document;
class Element;
class LocalFrame;
class HTMLFrameOwnerElement;
class HTMLShadowElement;
class IntRect;
class KeyboardEvent;
class Node;
class Page;
class TreeScope;

class FocusNavigationScope {
public:
    Node* rootNode() const;
    Element* owner() const;
    static FocusNavigationScope focusNavigationScopeOf(Node*);
    static FocusNavigationScope ownedByNonFocusableFocusScopeOwner(Node*);
    static FocusNavigationScope ownedByShadowHost(Node*);
    static FocusNavigationScope ownedByShadowInsertionPoint(HTMLShadowElement*);
    static FocusNavigationScope ownedByIFrame(HTMLFrameOwnerElement*);

private:
    explicit FocusNavigationScope(TreeScope*);
    TreeScope* m_rootTreeScope;
};

class FocusController {
    WTF_MAKE_NONCOPYABLE(FocusController); WTF_MAKE_FAST_ALLOCATED;
public:
    static PassOwnPtr<FocusController> create(Page*);

    void setFocusedFrame(PassRefPtr<LocalFrame>);
    LocalFrame* focusedFrame() const { return m_focusedFrame.get(); }
    LocalFrame* focusedOrMainFrame() const;

    bool setInitialFocus(FocusType);
    bool advanceFocus(FocusType type) { return advanceFocus(type, false); }

    bool setFocusedElement(Element*, PassRefPtr<LocalFrame>, FocusType = FocusTypeNone);

    void setActive(bool);
    bool isActive() const { return m_isActive; }

    void setFocused(bool);
    bool isFocused() const { return m_isFocused; }

    void setContainingWindowIsVisible(bool);
    bool containingWindowIsVisible() const { return m_containingWindowIsVisible; }

private:
    explicit FocusController(Page*);

    bool advanceFocus(FocusType, bool initialFocus);
    bool advanceFocusDirectionally(FocusType);
    bool advanceFocusInDocumentOrder(FocusType, bool initialFocus);

    Node* findFocusableNodeAcrossFocusScope(FocusType, FocusNavigationScope startScope, Node* start);
    Node* findFocusableNodeRecursively(FocusType, FocusNavigationScope, Node* start);
    Node* findFocusableNodeDecendingDownIntoFrameDocument(FocusType, Node*);

    // Searches through the given tree scope, starting from start node, for the next/previous selectable element that comes after/before start node.
    // The order followed is as specified in section 17.11.1 of the HTML4 spec, which is elements with tab indexes
    // first (from lowest to highest), and then elements without tab indexes (in document order).
    //
    // @param start The node from which to start searching. The node after this will be focused. May be null.
    //
    // @return The focus node that comes after/before start node.
    //
    // See http://www.w3.org/TR/html4/interact/forms.html#h-17.11.1
    inline Node* findFocusableNode(FocusType, FocusNavigationScope, Node* start);

    Node* nextFocusableNode(FocusNavigationScope, Node* start);
    Node* previousFocusableNode(FocusNavigationScope, Node* start);

    Node* findNodeWithExactTabIndex(Node* start, int tabIndex, FocusType);

    bool advanceFocusDirectionallyInContainer(Node* container, const LayoutRect& startingRect, FocusType);
    void findFocusCandidateInContainer(Node& container, const LayoutRect& startingRect, FocusType, FocusCandidate& closest);

    Page* m_page;
    RefPtr<LocalFrame> m_focusedFrame;
    bool m_isActive;
    bool m_isFocused;
    bool m_isChangingFocusedFrame;
    bool m_containingWindowIsVisible;

};

} // namespace WebCore

#endif // FocusController_h
