/*
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
 * Copyright (C) 2006 Apple Computer, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "core/page/FrameTree.h"

#include "core/dom/Document.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/loader/FrameLoaderClient.h"
#include "core/page/Page.h"
#include "wtf/Vector.h"
#include "wtf/text/CString.h"
#include "wtf/text/StringBuilder.h"

using std::swap;

namespace WebCore {

namespace {

const unsigned invalidChildCount = ~0;

} // namespace

FrameTree::FrameTree(LocalFrame* thisFrame)
    : m_thisFrame(thisFrame)
    , m_scopedChildCount(invalidChildCount)
{
}

FrameTree::~FrameTree()
{
    // FIXME: Why is this here? Doesn't this parallel what we already do in ~LocalFrame?
    for (LocalFrame* child = firstChild(); child; child = child->tree().nextSibling())
        child->setView(nullptr);
}

void FrameTree::setName(const AtomicString& name)
{
    m_name = name;
    if (!parent()) {
        m_uniqueName = name;
        return;
    }
    m_uniqueName = AtomicString(); // Remove our old frame name so it's not considered in uniqueChildName.
    m_uniqueName = parent()->tree().uniqueChildName(name);
}

LocalFrame* FrameTree::parent() const
{
    if (!m_thisFrame->loader().client())
        return 0;
    // FIXME: Temporary hack to stage converting locations that really should be Frame.
    return toLocalFrame(m_thisFrame->loader().client()->parent());
}

LocalFrame* FrameTree::top() const
{
    // FIXME: top() should never return null, so here are some hacks to deal
    // with EmptyFrameLoaderClient and cases where the frame is detached
    // already...
    if (!m_thisFrame->loader().client())
        return m_thisFrame;
    // FIXME: Temporary hack to stage converting locations that really should be Frame.
    LocalFrame* candidate = toLocalFrame(m_thisFrame->loader().client()->top());
    return candidate ? candidate : m_thisFrame;
}

LocalFrame* FrameTree::previousSibling() const
{
    if (!m_thisFrame->loader().client())
        return 0;
    // FIXME: Temporary hack to stage converting locations that really should be Frame.
    return toLocalFrame(m_thisFrame->loader().client()->previousSibling());
}

LocalFrame* FrameTree::nextSibling() const
{
    if (!m_thisFrame->loader().client())
        return 0;
    // FIXME: Temporary hack to stage converting locations that really should be Frame.
    return toLocalFrame(m_thisFrame->loader().client()->nextSibling());
}

LocalFrame* FrameTree::firstChild() const
{
    if (!m_thisFrame->loader().client())
        return 0;
    // FIXME: Temporary hack to stage converting locations that really should be Frame.
    return toLocalFrame(m_thisFrame->loader().client()->firstChild());
}

LocalFrame* FrameTree::lastChild() const
{
    if (!m_thisFrame->loader().client())
        return 0;
    // FIXME: Temporary hack to stage converting locations that really should be Frame.
    return toLocalFrame(m_thisFrame->loader().client()->lastChild());
}

AtomicString FrameTree::uniqueChildName(const AtomicString& requestedName) const
{
    if (!requestedName.isEmpty() && !child(requestedName) && requestedName != "_blank")
        return requestedName;

    // Create a repeatable name for a child about to be added to us. The name must be
    // unique within the frame tree. The string we generate includes a "path" of names
    // from the root frame down to us. For this path to be unique, each set of siblings must
    // contribute a unique name to the path, which can't collide with any HTML-assigned names.
    // We generate this path component by index in the child list along with an unlikely
    // frame name that can't be set in HTML because it collides with comment syntax.

    const char framePathPrefix[] = "<!--framePath ";
    const int framePathPrefixLength = 14;
    const int framePathSuffixLength = 3;

    // Find the nearest parent that has a frame with a path in it.
    Vector<LocalFrame*, 16> chain;
    LocalFrame* frame;
    for (frame = m_thisFrame; frame; frame = frame->tree().parent()) {
        if (frame->tree().uniqueName().startsWith(framePathPrefix))
            break;
        chain.append(frame);
    }
    StringBuilder name;
    name.append(framePathPrefix);
    if (frame) {
        name.append(frame->tree().uniqueName().string().substring(framePathPrefixLength,
            frame->tree().uniqueName().length() - framePathPrefixLength - framePathSuffixLength));
    }
    for (int i = chain.size() - 1; i >= 0; --i) {
        frame = chain[i];
        name.append('/');
        name.append(frame->tree().uniqueName());
    }

    name.appendLiteral("/<!--frame");
    name.appendNumber(childCount() - 1);
    name.appendLiteral("-->-->");

    return name.toAtomicString();
}

LocalFrame* FrameTree::scopedChild(unsigned index) const
{
    TreeScope* scope = m_thisFrame->document();
    if (!scope)
        return 0;

    unsigned scopedIndex = 0;
    for (LocalFrame* result = firstChild(); result; result = result->tree().nextSibling()) {
        if (result->inScope(scope)) {
            if (scopedIndex == index)
                return result;
            scopedIndex++;
        }
    }

    return 0;
}

LocalFrame* FrameTree::scopedChild(const AtomicString& name) const
{
    TreeScope* scope = m_thisFrame->document();
    if (!scope)
        return 0;

    for (LocalFrame* child = firstChild(); child; child = child->tree().nextSibling())
        if (child->tree().uniqueName() == name && child->inScope(scope))
            return child;
    return 0;
}

inline unsigned FrameTree::scopedChildCount(TreeScope* scope) const
{
    if (!scope)
        return 0;

    unsigned scopedCount = 0;
    for (LocalFrame* result = firstChild(); result; result = result->tree().nextSibling()) {
        if (result->inScope(scope))
            scopedCount++;
    }

    return scopedCount;
}

unsigned FrameTree::scopedChildCount() const
{
    if (m_scopedChildCount == invalidChildCount)
        m_scopedChildCount = scopedChildCount(m_thisFrame->document());
    return m_scopedChildCount;
}

void FrameTree::invalidateScopedChildCount()
{
    m_scopedChildCount = invalidChildCount;
}

unsigned FrameTree::childCount() const
{
    unsigned count = 0;
    for (LocalFrame* result = firstChild(); result; result = result->tree().nextSibling())
        ++count;
    return count;
}

LocalFrame* FrameTree::child(const AtomicString& name) const
{
    for (LocalFrame* child = firstChild(); child; child = child->tree().nextSibling())
        if (child->tree().uniqueName() == name)
            return child;
    return 0;
}

LocalFrame* FrameTree::find(const AtomicString& name) const
{
    if (name == "_self" || name == "_current" || name.isEmpty())
        return m_thisFrame;

    if (name == "_top")
        return top();

    if (name == "_parent")
        return parent() ? parent() : m_thisFrame;

    // Since "_blank" should never be any frame's name, the following just amounts to an optimization.
    if (name == "_blank")
        return 0;

    // Search subtree starting with this frame first.
    for (LocalFrame* frame = m_thisFrame; frame; frame = frame->tree().traverseNext(m_thisFrame))
        if (frame->tree().uniqueName() == name)
            return frame;

    // Search the entire tree for this page next.
    Page* page = m_thisFrame->page();

    // The frame could have been detached from the page, so check it.
    if (!page)
        return 0;

    for (LocalFrame* frame = page->mainFrame(); frame; frame = frame->tree().traverseNext())
        if (frame->tree().uniqueName() == name)
            return frame;

    // Search the entire tree of each of the other pages in this namespace.
    // FIXME: Is random order OK?
    const HashSet<Page*>& pages = Page::ordinaryPages();
    HashSet<Page*>::const_iterator end = pages.end();
    for (HashSet<Page*>::const_iterator it = pages.begin(); it != end; ++it) {
        Page* otherPage = *it;
        if (otherPage != page) {
            for (LocalFrame* frame = otherPage->mainFrame(); frame; frame = frame->tree().traverseNext()) {
                if (frame->tree().uniqueName() == name)
                    return frame;
            }
        }
    }

    return 0;
}

bool FrameTree::isDescendantOf(const LocalFrame* ancestor) const
{
    if (!ancestor)
        return false;

    if (m_thisFrame->page() != ancestor->page())
        return false;

    for (LocalFrame* frame = m_thisFrame; frame; frame = frame->tree().parent())
        if (frame == ancestor)
            return true;
    return false;
}

LocalFrame* FrameTree::traverseNext(const LocalFrame* stayWithin) const
{
    LocalFrame* child = firstChild();
    if (child) {
        ASSERT(!stayWithin || child->tree().isDescendantOf(stayWithin));
        return child;
    }

    if (m_thisFrame == stayWithin)
        return 0;

    LocalFrame* sibling = nextSibling();
    if (sibling) {
        ASSERT(!stayWithin || sibling->tree().isDescendantOf(stayWithin));
        return sibling;
    }

    LocalFrame* frame = m_thisFrame;
    while (!sibling && (!stayWithin || frame->tree().parent() != stayWithin)) {
        frame = frame->tree().parent();
        if (!frame)
            return 0;
        sibling = frame->tree().nextSibling();
    }

    if (frame) {
        ASSERT(!stayWithin || !sibling || sibling->tree().isDescendantOf(stayWithin));
        return sibling;
    }

    return 0;
}

LocalFrame* FrameTree::traverseNextWithWrap(bool wrap) const
{
    if (LocalFrame* result = traverseNext())
        return result;

    if (wrap)
        return m_thisFrame->page()->mainFrame();

    return 0;
}

LocalFrame* FrameTree::traversePreviousWithWrap(bool wrap) const
{
    // FIXME: besides the wrap feature, this is just the traversePreviousNode algorithm

    if (LocalFrame* prevSibling = previousSibling())
        return prevSibling->tree().deepLastChild();
    if (LocalFrame* parentFrame = parent())
        return parentFrame;

    // no siblings, no parent, self==top
    if (wrap)
        return deepLastChild();

    // top view is always the last one in this ordering, so prev is nil without wrap
    return 0;
}

LocalFrame* FrameTree::deepLastChild() const
{
    LocalFrame* result = m_thisFrame;
    for (LocalFrame* last = lastChild(); last; last = last->tree().lastChild())
        result = last;

    return result;
}

} // namespace WebCore

#ifndef NDEBUG

static void printIndent(int indent)
{
    for (int i = 0; i < indent; ++i)
        printf("    ");
}

static void printFrames(const WebCore::LocalFrame* frame, const WebCore::LocalFrame* targetFrame, int indent)
{
    if (frame == targetFrame) {
        printf("--> ");
        printIndent(indent - 1);
    } else
        printIndent(indent);

    WebCore::FrameView* view = frame->view();
    printf("LocalFrame %p %dx%d\n", frame, view ? view->width() : 0, view ? view->height() : 0);
    printIndent(indent);
    printf("  ownerElement=%p\n", frame->ownerElement());
    printIndent(indent);
    printf("  frameView=%p\n", view);
    printIndent(indent);
    printf("  document=%p\n", frame->document());
    printIndent(indent);
    printf("  uri=%s\n\n", frame->document()->url().string().utf8().data());

    for (WebCore::LocalFrame* child = frame->tree().firstChild(); child; child = child->tree().nextSibling())
        printFrames(child, targetFrame, indent + 1);
}

void showFrameTree(const WebCore::LocalFrame* frame)
{
    if (!frame) {
        printf("Null input frame\n");
        return;
    }

    printFrames(frame->tree().top(), frame, 0);
}

#endif
