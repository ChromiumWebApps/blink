/*
 * Copyright (C) 2006, 2007 Rob Buis
 * Copyright (C) 2008 Apple, Inc. All rights reserved.
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
#include "core/dom/StyleElement.h"

#include "core/css/MediaList.h"
#include "core/css/MediaQueryEvaluator.h"
#include "core/css/StyleSheetContents.h"
#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/dom/ScriptableDocumentParser.h"
#include "core/dom/StyleEngine.h"
#include "core/frame/csp/ContentSecurityPolicy.h"
#include "core/html/HTMLStyleElement.h"
#include "platform/TraceEvent.h"
#include "wtf/text/StringBuilder.h"

namespace WebCore {

static bool isCSS(Element* element, const AtomicString& type)
{
    return type.isEmpty() || (element->isHTMLElement() ? equalIgnoringCase(type, "text/css") : (type == "text/css"));
}

StyleElement::StyleElement(Document* document, bool createdByParser)
    : m_createdByParser(createdByParser)
    , m_loading(false)
    , m_startPosition(TextPosition::belowRangePosition())
{
    if (createdByParser && document && document->scriptableDocumentParser() && !document->isInDocumentWrite())
        m_startPosition = document->scriptableDocumentParser()->textPosition();
}

StyleElement::~StyleElement()
{
    if (m_sheet)
        clearSheet();
}

void StyleElement::processStyleSheet(Document& document, Element* element)
{
    TRACE_EVENT0("webkit", "StyleElement::processStyleSheet");
    ASSERT(element);
    document.styleEngine()->addStyleSheetCandidateNode(element, m_createdByParser);
    if (m_createdByParser)
        return;

    process(element);
}

void StyleElement::removedFromDocument(Document& document, Element* element)
{
    removedFromDocument(document, element, 0, document);
}

void StyleElement::removedFromDocument(Document& document, Element* element, ContainerNode* scopingNode, TreeScope& treeScope)
{
    ASSERT(element);
    document.styleEngine()->removeStyleSheetCandidateNode(element, scopingNode, treeScope);

    RefPtr<StyleSheet> removedSheet = m_sheet;

    if (m_sheet)
        clearSheet(element);

    document.removedStyleSheet(removedSheet.get(), RecalcStyleDeferred, AnalyzedStyleUpdate);
}

void StyleElement::clearDocumentData(Document& document, Element* element)
{
    if (m_sheet)
        m_sheet->clearOwnerNode();

    if (element->inDocument()) {
        ContainerNode* scopingNode = isHTMLStyleElement(element) ? toHTMLStyleElement(element)->scopingNode() :  0;
        TreeScope& treeScope = scopingNode ? scopingNode->treeScope() : element->treeScope();
        document.styleEngine()->removeStyleSheetCandidateNode(element, scopingNode, treeScope);
    }
}

void StyleElement::childrenChanged(Element* element)
{
    ASSERT(element);
    if (m_createdByParser)
        return;

    process(element);
}

void StyleElement::finishParsingChildren(Element* element)
{
    ASSERT(element);
    process(element);
    m_createdByParser = false;
}

void StyleElement::process(Element* element)
{
    if (!element || !element->inDocument())
        return;
    createSheet(element, element->textFromChildren());
}

void StyleElement::clearSheet(Element* ownerElement)
{
    ASSERT(m_sheet);

    if (ownerElement && m_sheet->isLoading())
        ownerElement->document().styleEngine()->removePendingSheet(ownerElement);

    m_sheet.release()->clearOwnerNode();
}

void StyleElement::createSheet(Element* e, const String& text)
{
    ASSERT(e);
    ASSERT(e->inDocument());
    Document& document = e->document();
    if (m_sheet)
        clearSheet(e);

    // If type is empty or CSS, this is a CSS style sheet.
    const AtomicString& type = this->type();
    bool passesContentSecurityPolicyChecks = document.contentSecurityPolicy()->allowStyleHash(text) || document.contentSecurityPolicy()->allowStyleNonce(e->fastGetAttribute(HTMLNames::nonceAttr)) || document.contentSecurityPolicy()->allowInlineStyle(e->document().url(), m_startPosition.m_line);
    if (isCSS(e, type) && passesContentSecurityPolicyChecks) {
        RefPtrWillBeRawPtr<MediaQuerySet> mediaQueries = MediaQuerySet::create(media());

        MediaQueryEvaluator screenEval("screen", true);
        MediaQueryEvaluator printEval("print", true);
        if (screenEval.eval(mediaQueries.get()) || printEval.eval(mediaQueries.get())) {
            m_loading = true;
            TextPosition startPosition = m_startPosition == TextPosition::belowRangePosition() ? TextPosition::minimumPosition() : m_startPosition;
            m_sheet = StyleEngine::createSheet(e, text, startPosition, m_createdByParser);
            m_sheet->setMediaQueries(mediaQueries.release());
            m_loading = false;
        }
    }

    if (m_sheet)
        m_sheet->contents()->checkLoaded();
}

bool StyleElement::isLoading() const
{
    if (m_loading)
        return true;
    return m_sheet ? m_sheet->isLoading() : false;
}

bool StyleElement::sheetLoaded(Document& document)
{
    if (isLoading())
        return false;

    document.styleEngine()->removePendingSheet(m_sheet->ownerNode());
    return true;
}

void StyleElement::startLoadingDynamicSheet(Document& document)
{
    document.styleEngine()->addPendingSheet();
}

}
