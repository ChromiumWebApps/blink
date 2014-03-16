/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 *           (C) 2006 Alexey Proskuryakov (ap@webkit.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2012 Apple Inc. All rights reserved.
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2013 Google Inc. All rights reserved.
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
#include "core/dom/DocumentStyleSheetCollection.h"

#include "HTMLNames.h"
#include "RuntimeEnabledFeatures.h"
#include "SVGNames.h"
#include "core/css/CSSStyleSheet.h"
#include "core/css/resolver/StyleResolver.h"
#include "core/dom/Document.h"
#include "core/dom/DocumentStyleSheetCollector.h"
#include "core/dom/Element.h"
#include "core/dom/ProcessingInstruction.h"
#include "core/dom/StyleEngine.h"
#include "core/dom/StyleSheetCandidate.h"
#include "core/html/HTMLIFrameElement.h"
#include "core/html/HTMLLinkElement.h"
#include "core/html/HTMLStyleElement.h"
#include "core/frame/Settings.h"
#include "core/svg/SVGStyleElement.h"

namespace WebCore {

using namespace HTMLNames;

DocumentStyleSheetCollection::DocumentStyleSheetCollection(TreeScope& treeScope)
    : TreeScopeStyleSheetCollection(treeScope)
{
    ASSERT(treeScope.rootNode() == treeScope.rootNode().document());
}

void DocumentStyleSheetCollection::collectStyleSheetsFromCandidates(StyleEngine* engine, DocumentStyleSheetCollector& collector)
{
    DocumentOrderedList::iterator begin = m_styleSheetCandidateNodes.begin();
    DocumentOrderedList::iterator end = m_styleSheetCandidateNodes.end();
    for (DocumentOrderedList::iterator it = begin; it != end; ++it) {
        Node* n = *it;
        StyleSheetCandidate candidate(*n);

        if (candidate.isXSL()) {
            // Processing instruction (XML documents only).
            // We don't support linking to embedded CSS stylesheets, see <https://bugs.webkit.org/show_bug.cgi?id=49281> for discussion.
            // Don't apply XSL transforms to already transformed documents -- <rdar://problem/4132806>
            if (RuntimeEnabledFeatures::xsltEnabled() && !document().transformSourceDocument()) {
                ProcessingInstruction* pi = toProcessingInstruction(n);
                // Don't apply XSL transforms until loading is finished.
                if (!document().parsing() && !pi->isLoading())
                    document().applyXSLTransform(pi);
                return;
            }

            continue;
        }

        if (candidate.isImport()) {
            Document* document = candidate.importedDocument();
            if (!document)
                continue;
            if (collector.hasVisited(document))
                continue;
            collector.willVisit(document);
            document->styleEngine()->updateStyleSheetsInImport(collector);
            continue;
        }

        if (candidate.isEnabledAndLoading()) {
            // it is loading but we should still decide which style sheet set to use
            if (candidate.hasPreferrableName(engine->preferredStylesheetSetName()))
                engine->selectStylesheetSetName(candidate.title());
            continue;
        }

        StyleSheet* sheet = candidate.sheet();
        if (!sheet)
            continue;

        if (candidate.hasPreferrableName(engine->preferredStylesheetSetName()))
            engine->selectStylesheetSetName(candidate.title());
        collector.appendSheetForList(sheet);
        if (candidate.canBeActivated(engine->preferredStylesheetSetName()))
            collector.appendActiveStyleSheet(toCSSStyleSheet(sheet));
    }
}

void DocumentStyleSheetCollection::collectStyleSheets(StyleEngine* engine, DocumentStyleSheetCollector& collector)
{
    ASSERT(document().styleEngine() == engine);
    collector.appendActiveStyleSheets(engine->injectedAuthorStyleSheets());
    collector.appendActiveStyleSheets(engine->documentAuthorStyleSheets());
    collectStyleSheetsFromCandidates(engine, collector);
}

bool DocumentStyleSheetCollection::updateActiveStyleSheets(StyleEngine* engine, StyleResolverUpdateMode updateMode)
{
    StyleSheetCollection collection;
    ActiveDocumentStyleSheetCollector collector(collection);
    collectStyleSheets(engine, collector);

    StyleSheetChange change;
    analyzeStyleSheetChange(updateMode, collection, change);

    if (change.styleResolverUpdateType == Reconstruct) {
        engine->clearMasterResolver();
        // FIMXE: The following depends on whether StyleRuleFontFace was modified or not.
        // No need to always-clear font cache.
        engine->clearFontCache();
    } else if (StyleResolver* styleResolver = engine->resolver()) {
        // FIXME: We might have already had styles in child treescope. In this case, we cannot use buildScopedStyleTreeInDocumentOrder.
        // Need to change "false" to some valid condition.
        styleResolver->setBuildScopedStyleTreeInDocumentOrder(false);
        if (change.styleResolverUpdateType != Additive) {
            ASSERT(change.styleResolverUpdateType == Reset);
            resetAllRuleSetsInTreeScope(styleResolver);
            engine->removeFontFaceRules(change.fontFaceRulesToRemove);
            styleResolver->removePendingAuthorStyleSheets(m_activeAuthorStyleSheets);
            styleResolver->lazyAppendAuthorStyleSheets(0, collection.activeAuthorStyleSheets());
        } else {
            styleResolver->lazyAppendAuthorStyleSheets(m_activeAuthorStyleSheets.size(), collection.activeAuthorStyleSheets());
        }
    }
    m_scopingNodesForStyleScoped.didRemoveScopingNodes();

    collection.swap(*this);

    updateUsesRemUnits();

    return change.requiresFullStyleRecalc;
}

}
