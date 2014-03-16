/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#include "config.h"
#include "core/html/HTMLShadowElement.h"

#include "HTMLNames.h"
#include "core/dom/shadow/ShadowRoot.h"

namespace WebCore {

class Document;

inline HTMLShadowElement::HTMLShadowElement(Document& document)
    : InsertionPoint(HTMLNames::shadowTag, document)
{
    ScriptWrappable::init(this);
}

PassRefPtr<HTMLShadowElement> HTMLShadowElement::create(Document& document)
{
    return adoptRef(new HTMLShadowElement(document));
}

HTMLShadowElement::~HTMLShadowElement()
{
}

ShadowRoot* HTMLShadowElement::olderShadowRoot()
{
    ShadowRoot* containingRoot = containingShadowRoot();
    if (!containingRoot)
        return 0;

    document().updateDistributionForNodeIfNeeded(this);

    ShadowRoot* older = containingRoot->olderShadowRoot();
    if (!older || !older->shouldExposeToBindings() || older->shadowInsertionPointOfYoungerShadowRoot() != this)
        return 0;

    ASSERT(older->shouldExposeToBindings());
    return older;
}

Node::InsertionNotificationRequest HTMLShadowElement::insertedInto(ContainerNode* insertionPoint)
{
    if (insertionPoint->inDocument()) {
        // Warn if trying to reproject between user agent and author shadows.
        ShadowRoot* root = containingShadowRoot();
        if (root && root->olderShadowRoot() && root->type() != root->olderShadowRoot()->type()) {
            String message = String::format("<shadow> doesn't work for %s element host.", root->host()->tagName().utf8().data());
            document().addConsoleMessage(RenderingMessageSource, WarningMessageLevel, message);
        }
    }
    return InsertionPoint::insertedInto(insertionPoint);
}

} // namespace WebCore

