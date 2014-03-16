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

#include "config.h"
#include "core/html/imports/LinkImport.h"

#include "core/dom/Document.h"
#include "core/fetch/CrossOriginAccessControl.h"
#include "core/html/HTMLLinkElement.h"
#include "core/html/imports/HTMLImportChild.h"
#include "core/html/imports/HTMLImportsController.h"


namespace WebCore {

PassOwnPtr<LinkImport> LinkImport::create(HTMLLinkElement* owner)
{
    return adoptPtr(new LinkImport(owner));
}

LinkImport::LinkImport(HTMLLinkElement* owner)
    : LinkResource(owner)
    , m_child(0)
{
}

LinkImport::~LinkImport()
{
    clear();
}

Document* LinkImport::importedDocument() const
{
    if (!m_child || !m_owner || !m_owner->inDocument())
        return 0;
    return m_child->importedDocument();
}

void LinkImport::process()
{
    if (m_child)
        return;
    if (!m_owner)
        return;
    if (!shouldLoadResource())
        return;

    if (!m_owner->document().import()) {
        ASSERT(m_owner->document().frame()); // The document should be the master.
        HTMLImportsController::provideTo(m_owner->document());
    }

    LinkRequestBuilder builder(m_owner);
    if (!builder.isValid()) {
        didFinish();
        return;
    }

    HTMLImport* parent = m_owner->document().import();
    HTMLImportsController* controller = parent->controller();
    m_child = controller->load(parent, this, builder.build(true));
    if (!m_child) {
        didFinish();
        return;
    }
}

void LinkImport::clear()
{
    m_owner = 0;
    if (m_child) {
        m_child->clearClient();
        m_child = 0;
    }
}

void LinkImport::didFinish()
{
    if (!m_owner || !m_owner->inDocument())
        return;
    // Because didFinish() is called from import's own scheduler in HTMLImportsController,
    // we don't need to scheduleEvent() here.
    m_owner->dispatchEventImmediately();
}

void LinkImport::importChildWasDestroyed(HTMLImportChild* child)
{
    ASSERT(m_child == child);
    m_child = 0;
    clear();
}

bool LinkImport::isSync() const
{
    return m_owner && m_owner->isCreatedByParser() && !m_owner->async();
}

HTMLLinkElement* LinkImport::link()
{
    return m_owner;
}

bool LinkImport::hasLoaded() const
{
    return m_child && m_child->isDone() && !m_child->loaderHasError();
}

bool LinkImport::ownsLoader() const
{
    return m_child && m_child->hasLoader() && m_child->ownsLoader();
}

} // namespace WebCore
