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

#ifndef HTMLImport_h
#define HTMLImport_h

#include "core/html/imports/HTMLImportState.h"
#include "wtf/TreeNode.h"
#include "wtf/Vector.h"

namespace WebCore {

class CustomElementMicrotaskImportStep;
class Document;
class LocalFrame;
class HTMLImportChild;
class HTMLImportRoot;
class HTMLImportsController;
class KURL;

//
// # Basic Data Structure and Algorithms of HTML Imports implemenation.
//
// ## The Import Tree
//
// HTML Imports form a tree:
//
// * The root of the tree is HTMLImportsController, which is owned by the master
//   document as a DocumentSupplement. HTMLImportsController has an abstract class called
//   HTMLImportRoot to deal with cycler dependency.
//
// * The non-root nodes are HTMLImportChild, which is owned by LinkStyle, that is owned by HTMLLinkElement.
//   LinkStyle is wired into HTMLImportChild by implementing HTMLImportChildClient interface
//
// * Both HTMLImportsController and HTMLImportChild are derived from HTMLImport superclass
//   that models the tree data structure using WTF::TreeNode and provides a set of
//   virtual functions.
//
// HTMLImportsController also owns all loaders in the tree and manages their lifetime through it.
// One assumption is that the tree is append-only and nodes are never inserted in the middle of the tree nor removed.
//
//
//    HTMLImport <|- HTMLImportRoot <|- HTMLImportsController <- Document
//                                      *
//                                      |
//               <|-                    HTMLImportChild <- LinkStyle <- HTMLLinkElement
//
//
// # Import Sharing and HTMLImportLoader
//
// The HTML Imports spec calls for de-dup mechanism to share already loaded imports.
// To implement this, the actual loading machinery is split out from HTMLImportChild to
// HTMLImportLoader, and each loader shares HTMLImportLoader with other loader if the URL is same.
// Check around HTMLImportsController::findLink() for more detail.
//
// Note that HTMLImportLoader provides HTMLImportLoaderClient to hook it up.
// As it can be shared, HTMLImportLoader supports multiple clients.
//
//    HTMLImportChild (1)-->(*) HTMLImportLoader
//
//
// # Script Blocking
//
// - An import blocks the HTML parser of its own imported document from running <script>
//   until all of its children are loaded.
//   Note that dynamically added import won't block the parser.
//
// - An import under loading also blocks imported documents that follow from being created.
//   This is because an import can include another import that has same URLs of following ones.
//   In such case, the preceding import should be loaded and following ones should be de-duped.
//

// The superclass of HTMLImportsController and HTMLImportChild
// This represents the import tree data structure.
class HTMLImport : public TreeNode<HTMLImport> {
public:
    static bool isMaster(Document*);

    virtual ~HTMLImport() { }

    LocalFrame* frame();
    Document* master();
    HTMLImportsController* controller();
    bool isRoot() const { return !isChild(); }
    bool isSync() const { return m_sync; }
    const HTMLImportState& state() const { return m_state; }

    void appendChild(HTMLImport*);

    virtual bool isChild() const { return false; }
    virtual HTMLImportRoot* root() = 0;
    virtual Document* document() const = 0;
    virtual void wasDetachedFromDocument() = 0;
    virtual void didFinishParsing() { };
    virtual void didRemoveAllPendingStylesheet() { }
    virtual bool isDone() const = 0; // FIXME: Should be renamed to haveFinishedLoading()
    virtual bool hasLoader() const = 0;
    virtual bool ownsLoader() const { return false; }
    virtual CustomElementMicrotaskImportStep* customElementMicrotaskStep() const { return 0; }
    virtual void stateDidChange();

protected:
    // Stating from most conservative state.
    // It will be corrected through state update flow.
    explicit HTMLImport(bool sync = false)
        : m_sync(sync)
    { }

    void stateWillChange();
    static void recalcTreeState(HTMLImport* root);

#if !defined(NDEBUG)
    void show();
    void showTree(HTMLImport* highlight, unsigned depth);
    virtual void showThis();
#endif

private:
    HTMLImportState m_state;
    bool m_sync : 1;
};

// An abstract class to decouple its sublcass HTMLImportsController.
class HTMLImportRoot : public HTMLImport {
public:
    HTMLImportRoot() { }

    virtual void scheduleRecalcState() = 0;
    virtual HTMLImportsController* toController() = 0;
    virtual HTMLImportChild* findLinkFor(const KURL&, HTMLImport* excluding = 0) const = 0;
};

} // namespace WebCore

#endif // HTMLImport_h
