/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2006, 2007 Apple Inc. All rights reserved.
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
 *
 */

#ifndef LiveNodeList_h
#define LiveNodeList_h

#include "core/dom/LiveNodeListBase.h"
#include "core/dom/NodeList.h"
#include "core/html/CollectionIndexCache.h"
#include "core/html/CollectionType.h"
#include "wtf/PassRefPtr.h"

namespace WebCore {

class Element;

class LiveNodeList : public NodeList, public LiveNodeListBase {
public:
    LiveNodeList(ContainerNode& ownerNode, CollectionType collectionType, NodeListInvalidationType invalidationType, NodeListRootType rootType = NodeListIsRootedAtNode)
        : LiveNodeListBase(ownerNode, rootType, invalidationType,
        collectionType)
    { }

    virtual unsigned length() const OVERRIDE FINAL { return m_collectionIndexCache.nodeCount(*this); }
    virtual Node* item(unsigned offset) const OVERRIDE FINAL { return m_collectionIndexCache.nodeAt(*this, offset); }
    virtual bool elementMatches(const Element&) const = 0;

    virtual void invalidateCache(Document* oldDocument) const OVERRIDE FINAL;
    bool shouldOnlyIncludeDirectChildren() const { return false; }

    // Collection IndexCache API.
    bool canTraverseBackward() const { return true; }
    Element* itemBefore(const Element* previousItem) const;
    Element* traverseToFirstElement(const ContainerNode& root) const;
    Element* traverseForwardToOffset(unsigned offset, Element& currentNode, unsigned& currentOffset, const ContainerNode& root) const;

private:
    virtual Node* virtualOwnerNode() const OVERRIDE FINAL;

    mutable CollectionIndexCache<LiveNodeList, Element> m_collectionIndexCache;
};

} // namespace WebCore

#endif // LiveNodeList_h
