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

#ifndef LinkImport_h
#define LinkImport_h

#include "core/html/LinkResource.h"
#include "core/html/imports/HTMLImportChildClient.h"
#include "wtf/FastAllocBase.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/RefPtr.h"

namespace WebCore {

class Document;
class HTMLImportChild;

//
// A LinkResource subclasss used for @rel=import.
//
class LinkImport FINAL : public LinkResource, public HTMLImportChildClient {
    WTF_MAKE_FAST_ALLOCATED;
public:

    static PassOwnPtr<LinkImport> create(HTMLLinkElement* owner);

    explicit LinkImport(HTMLLinkElement* owner);
    virtual ~LinkImport();

    // LinkResource
    virtual void process() OVERRIDE;
    virtual Type type() const OVERRIDE { return Import; }
    virtual bool hasLoaded() const OVERRIDE;

    // HTMLImportChildClient
    virtual void didFinish() OVERRIDE;
    virtual void importChildWasDestroyed(HTMLImportChild*) OVERRIDE;
    virtual bool isSync() const OVERRIDE;
    virtual HTMLLinkElement* link() OVERRIDE;

    Document* importedDocument() const;
    bool ownsLoader() const;

private:
    void clear();

    HTMLImportChild* m_child;
};

} // namespace WebCore

#endif // LinkImport_h
