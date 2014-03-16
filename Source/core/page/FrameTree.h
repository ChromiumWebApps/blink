/*
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

#ifndef FrameTree_h
#define FrameTree_h

#include "wtf/text/AtomicString.h"

namespace WebCore {

    class LocalFrame;
    class TreeScope;

    class FrameTree {
        WTF_MAKE_NONCOPYABLE(FrameTree);
    public:
        explicit FrameTree(LocalFrame* thisFrame);
        ~FrameTree();

        const AtomicString& name() const { return m_name; }
        const AtomicString& uniqueName() const { return m_uniqueName; }
        void setName(const AtomicString&);

        LocalFrame* parent() const;
        LocalFrame* top() const;
        LocalFrame* previousSibling() const;
        LocalFrame* nextSibling() const;
        LocalFrame* firstChild() const;
        LocalFrame* lastChild() const;

        bool isDescendantOf(const LocalFrame* ancestor) const;
        LocalFrame* traversePreviousWithWrap(bool) const;
        LocalFrame* traverseNext(const LocalFrame* stayWithin = 0) const;
        LocalFrame* traverseNextWithWrap(bool) const;

        LocalFrame* child(const AtomicString& name) const;
        LocalFrame* find(const AtomicString& name) const;
        unsigned childCount() const;

        LocalFrame* scopedChild(unsigned index) const;
        LocalFrame* scopedChild(const AtomicString& name) const;
        unsigned scopedChildCount() const;
        void invalidateScopedChildCount();

    private:
        LocalFrame* deepLastChild() const;
        AtomicString uniqueChildName(const AtomicString& requestedName) const;
        unsigned scopedChildCount(TreeScope*) const;

        LocalFrame* m_thisFrame;

        AtomicString m_name; // The actual frame name (may be empty).
        AtomicString m_uniqueName;

        mutable unsigned m_scopedChildCount;
    };

} // namespace WebCore

#ifndef NDEBUG
// Outside the WebCore namespace for ease of invocation from gdb.
void showFrameTree(const WebCore::LocalFrame*);
#endif

#endif // FrameTree_h
