/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#ifndef LayerRect_h
#define LayerRect_h

#include "core/dom/ClientRect.h"

#include "heap/Handle.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefCounted.h"
#include "wtf/RefPtr.h"
#include "wtf/text/WTFString.h"

namespace WebCore {

class Node;

class LayerRect : public RefCountedWillBeGarbageCollectedFinalized<LayerRect> {
public:
    static PassRefPtrWillBeRawPtr<LayerRect> create(PassRefPtr<Node> node, const String& layerType, PassRefPtr<ClientRect> rect)
    {
        return adoptRefWillBeNoop(new LayerRect(node, layerType, rect));
    }

    Node* layerRootNode() const { return m_layerRootNode.get(); }
    String layerType() const { return m_layerType; }
    ClientRect* layerRelativeRect() const { return m_rect.get(); }

    void trace(Visitor*) { }

private:
    LayerRect(PassRefPtr<Node> node, const String& layerName, PassRefPtr<ClientRect> rect)
        : m_layerRootNode(node)
        , m_layerType(layerName)
        , m_rect(rect)
    {
    }

    RefPtr<Node> m_layerRootNode;
    String m_layerType;
    RefPtr<ClientRect> m_rect;
};

} // namespace WebCore

#endif
