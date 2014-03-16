/*
 * Copyright (C) 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
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

#ifndef HistoryController_h
#define HistoryController_h

#include "core/loader/FrameLoaderTypes.h"
#include "core/loader/HistoryItem.h"
#include "platform/network/ResourceRequest.h"
#include "wtf/HashMap.h"
#include "wtf/Noncopyable.h"
#include "wtf/RefPtr.h"
#include "wtf/text/WTFString.h"

namespace WebCore {

class LocalFrame;
class HistoryEntry;
class Page;


// A guide to history state in Blink:
//
// HistoryController: Owned by Page, is the entry point for interacting with history.
//     Handles most of the operations to modify history state, navigate to an existing
//     back/forward entry, etc.
// HistoryEntry: Represents a single entry in the back/forward list, encapsulating
//     all frames in the page it represents. It provides access to each frame's
//     state via lookups by frame id or frame name.
// HistoryNode: Represents a single frame in a HistoryEntry. Owned by a HistoryEntry. HistoryNodes
// form a tree that mirrors the FrameTree in the corresponding page. HistoryNodes represent
// the structure of the page, but don't hold any per-frame state except a list of child frames.
// HistoryItem (lives in a separate file): The state for a given frame. Can persist across
//     navigations. HistoryItem is reference counted, and each HistoryNode holds a reference
//     to its single corresponding HistoryItem. Can be referenced by multiple HistoryNodes and
//     can therefore exist in multiple HistoryEntry instances.
//
// Suppose we have the following page, foo.com, which embeds foo.com/a in an iframe:
//
// HistoryEntry 0:
//     HistoryNode 0_0 (HistoryItem A (url: foo.com))
//         HistoryNode 0_1: (HistoryItem B (url: foo.com/a))
//
// Now we navigation the top frame to bar.com, which embeds bar.com/b and bar.com/c in iframes,
// and bar.com/b in turn embeds bar.com/d. We will create a new HistoryEntry with a tree
// containing 4 new HistoryNodes. The state will be:
//
// HistoryEntry 1:
//     HistoryNode 1_0 (HistoryItem C (url: bar.com))
//         HistoryNode 1_1: (HistoryItem D (url: bar.com/b))
//             HistoryNode 1_3: (HistoryItem F (url: bar.com/d))
//         HistoryNode 1_2: (HistoryItem E (url: bar.com/c))
//
//
// Finally, we navigate the first subframe from bar.com/b to bar.com/e, which embeds bar.com/f.
// We will create a new HistoryEntry and new HistoryNode for each frame. Any frame that
// navigates (bar.com/e and its child, bar.com/f) will receive a new HistoryItem. However,
// 2 frames were not navigated (bar.com and bar.com/c), so those two frames will reuse the
// existing HistoryItem:
//
// HistoryEntry 2:
//     HistoryNode 2_0 (HistoryItem C (url: bar.com))  *REUSED*
//         HistoryNode 2_1: (HistoryItem G (url: bar.com/e))
//            HistoryNode 2_3: (HistoryItem H (url: bar.com/f))
//         HistoryNode 2_2: (HistoryItem E (url: bar.com/c)) *REUSED*
//

class HistoryNode {
public:
    static PassOwnPtr<HistoryNode> create(HistoryEntry*, HistoryItem*, int64_t frameID);
    ~HistoryNode() { }

    HistoryNode* addChild(PassRefPtr<HistoryItem>, int64_t frameID);
    PassOwnPtr<HistoryNode> cloneAndReplace(HistoryEntry*, HistoryItem* newItem, bool clipAtTarget, LocalFrame* targetFrame, LocalFrame* currentFrame);
    HistoryItem* value() { return m_value.get(); }
    void updateValue(PassRefPtr<HistoryItem> item) { m_value = item; }
    const Vector<OwnPtr<HistoryNode> >& children() const { return m_children; }
    void removeChildren();

private:
    HistoryNode(HistoryEntry*, HistoryItem*, int64_t frameID);

    HistoryEntry* m_entry;
    Vector<OwnPtr<HistoryNode> > m_children;
    RefPtr<HistoryItem> m_value;

};

class HistoryEntry {
public:
    static PassOwnPtr<HistoryEntry> create(HistoryItem* root, int64_t frameID);
    PassOwnPtr<HistoryEntry> cloneAndReplace(HistoryItem* newItem, bool clipAtTarget, LocalFrame* targetFrame, Page*);

    HistoryNode* historyNodeForFrame(LocalFrame*);
    HistoryItem* itemForFrame(LocalFrame*);
    HistoryItem* root() const { return m_root->value(); }
    HistoryNode* rootHistoryNode() const { return m_root.get(); }

private:
    friend class HistoryNode;

    HistoryEntry() { }
    explicit HistoryEntry(HistoryItem* root, int64_t frameID);

    OwnPtr<HistoryNode> m_root;
    HashMap<uint64_t, HistoryNode*> m_framesToItems;
    HashMap<String, HistoryNode*> m_uniqueNamesToItems;
};

class HistoryController {
    WTF_MAKE_NONCOPYABLE(HistoryController);
public:
    explicit HistoryController(Page*);
    ~HistoryController();

    // Should only be called by embedder. To request a back/forward
    // navigation, call FrameLoaderClient::navigateBackForward().
    void goToItem(HistoryItem*, ResourceRequestCachePolicy);

    void updateBackForwardListForFragmentScroll(LocalFrame*, HistoryItem*);
    void updateForCommit(LocalFrame*, HistoryItem*, HistoryCommitType);

    PassRefPtr<HistoryItem> currentItemForExport();
    PassRefPtr<HistoryItem> previousItemForExport();
    HistoryItem* itemForNewChildFrame(LocalFrame*) const;
    void removeChildrenForRedirect(LocalFrame*);

private:
    void goToEntry(PassOwnPtr<HistoryEntry>, ResourceRequestCachePolicy);
    typedef HashMap<RefPtr<LocalFrame>, RefPtr<HistoryItem> > HistoryFrameLoadSet;
    void recursiveGoToEntry(LocalFrame*, HistoryFrameLoadSet& sameDocumentLoads, HistoryFrameLoadSet& differentDocumentLoads);

    void updateForInitialLoadInChildFrame(LocalFrame*, HistoryItem*);
    void createNewBackForwardItem(LocalFrame*, HistoryItem*, bool doClip);

    Page* m_page;

    OwnPtr<HistoryEntry> m_currentEntry;
    OwnPtr<HistoryEntry> m_previousEntry;
    OwnPtr<HistoryEntry> m_provisionalEntry;
};

} // namespace WebCore

#endif // HistoryController_h
