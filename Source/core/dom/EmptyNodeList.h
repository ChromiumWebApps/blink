/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
 * Copyright (C) 2014 Samsung Electronics. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
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

#ifndef EmptyNodeList_h
#define EmptyNodeList_h

#include "core/dom/NodeList.h"
#include "wtf/RefPtr.h"

namespace WebCore {

class EmptyNodeList FINAL : public NodeList {
public:
    static PassRefPtr<EmptyNodeList> create(Node& rootNode)
    {
        return adoptRef(new EmptyNodeList(rootNode));
    }
    virtual ~EmptyNodeList();

    Node& ownerNode() const { return *m_owner; }

private:
    explicit EmptyNodeList(Node& rootNode) : m_owner(rootNode) { }

    virtual unsigned length() const OVERRIDE { return 0; }
    virtual Node* item(unsigned) const OVERRIDE { return 0; }

    virtual bool isEmptyNodeList() const OVERRIDE { return true; }
    virtual Node* virtualOwnerNode() const OVERRIDE;

    RefPtr<Node> m_owner;
};

DEFINE_TYPE_CASTS(EmptyNodeList, NodeList, nodeList, nodeList->isEmptyNodeList(), nodeList.isEmptyNodeList());

} // namespace WebCore

#endif // EmptyNodeList_h
