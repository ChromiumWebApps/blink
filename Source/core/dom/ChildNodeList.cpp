/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2007, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2014 Samsung Electronics. All rights reserved.
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
#include "core/dom/ChildNodeList.h"

#include "core/dom/Element.h"
#include "core/dom/NodeRareData.h"

namespace WebCore {

ChildNodeList::ChildNodeList(ContainerNode& parent)
    : m_parent(parent)
{
}

Node* ChildNodeList::virtualOwnerNode() const
{
    return &ownerNode();
}

ChildNodeList::~ChildNodeList()
{
    m_parent->nodeLists()->removeChildNodeList(this);
}

Node* ChildNodeList::itemBefore(const Node* previous) const
{
    return LIKELY(!!previous) ? previous->previousSibling() : rootNode().lastChild();
}

Node* ChildNodeList::traverseForwardToOffset(unsigned offset, Node& currentNode, unsigned& currentOffset, const ContainerNode&) const
{
    ASSERT(currentOffset < offset);
    Node* next = &currentNode;
    while ((next = next->nextSibling())) {
        if (++currentOffset == offset)
            return next;
    }
    return 0;
}

} // namespace WebCore
