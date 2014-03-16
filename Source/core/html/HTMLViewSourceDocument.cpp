/*
 * Copyright (C) 2006, 2008, 2009, 2010 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "core/html/HTMLViewSourceDocument.h"

#include "HTMLNames.h"
#include "core/dom/StyleEngine.h"
#include "core/dom/Text.h"
#include "core/html/HTMLAnchorElement.h"
#include "core/html/HTMLBRElement.h"
#include "core/html/HTMLBaseElement.h"
#include "core/html/HTMLBodyElement.h"
#include "core/html/HTMLDivElement.h"
#include "core/html/HTMLHeadElement.h"
#include "core/html/HTMLHtmlElement.h"
#include "core/html/HTMLSpanElement.h"
#include "core/html/HTMLTableCellElement.h"
#include "core/html/HTMLTableElement.h"
#include "core/html/HTMLTableRowElement.h"
#include "core/html/HTMLTableSectionElement.h"
#include "core/html/parser/HTMLToken.h"
#include "core/html/parser/HTMLViewSourceParser.h"

namespace WebCore {

using namespace HTMLNames;

HTMLViewSourceDocument::HTMLViewSourceDocument(const DocumentInit& initializer, const String& mimeType)
    : HTMLDocument(initializer)
    , m_type(mimeType)
{
    setIsViewSource(true);

    // FIXME: Why do view-source pages need to load in quirks mode?
    setCompatibilityMode(QuirksMode);
    lockCompatibilityMode();
}

PassRefPtr<DocumentParser> HTMLViewSourceDocument::createParser()
{
    return HTMLViewSourceParser::create(this, m_type);
}

void HTMLViewSourceDocument::createContainingTable()
{
    RefPtr<HTMLHtmlElement> html = HTMLHtmlElement::create(*this);
    parserAppendChild(html);
    RefPtr<HTMLHeadElement> head = HTMLHeadElement::create(*this);
    html->parserAppendChild(head);
    RefPtr<HTMLBodyElement> body = HTMLBodyElement::create(*this);
    html->parserAppendChild(body);

    // Create a line gutter div that can be used to make sure the gutter extends down the height of the whole
    // document.
    RefPtr<HTMLDivElement> div = HTMLDivElement::create(*this);
    div->setAttribute(classAttr, "webkit-line-gutter-backdrop");
    body->parserAppendChild(div);

    RefPtr<HTMLTableElement> table = HTMLTableElement::create(*this);
    body->parserAppendChild(table);
    m_tbody = HTMLTableSectionElement::create(tbodyTag, *this);
    table->parserAppendChild(m_tbody);
    m_current = m_tbody;
    m_lineNumber = 0;
}

void HTMLViewSourceDocument::addSource(const String& source, HTMLToken& token)
{
    if (!m_current)
        createContainingTable();

    switch (token.type()) {
    case HTMLToken::Uninitialized:
        ASSERT_NOT_REACHED();
        break;
    case HTMLToken::DOCTYPE:
        processDoctypeToken(source, token);
        break;
    case HTMLToken::EndOfFile:
        processEndOfFileToken(source, token);
        break;
    case HTMLToken::StartTag:
    case HTMLToken::EndTag:
        processTagToken(source, token);
        break;
    case HTMLToken::Comment:
        processCommentToken(source, token);
        break;
    case HTMLToken::Character:
        processCharacterToken(source, token);
        break;
    }
}

void HTMLViewSourceDocument::processDoctypeToken(const String& source, HTMLToken&)
{
    m_current = addSpanWithClassName("webkit-html-doctype");
    addText(source, "webkit-html-doctype");
    m_current = m_td;
}

void HTMLViewSourceDocument::processEndOfFileToken(const String& source, HTMLToken&)
{
    m_current = addSpanWithClassName("webkit-html-end-of-file");
    addText(source, "webkit-html-end-of-file");
    m_current = m_td;
}

void HTMLViewSourceDocument::processTagToken(const String& source, HTMLToken& token)
{
    m_current = addSpanWithClassName("webkit-html-tag");

    AtomicString tagName(token.name());

    unsigned index = 0;
    HTMLToken::AttributeList::const_iterator iter = token.attributes().begin();
    while (index < source.length()) {
        if (iter == token.attributes().end()) {
            // We want to show the remaining characters in the token.
            index = addRange(source, index, source.length(), emptyAtom);
            ASSERT(index == source.length());
            break;
        }

        AtomicString name(iter->name);
        AtomicString value(StringImpl::create8BitIfPossible(iter->value));

        index = addRange(source, index, iter->nameRange.start - token.startIndex(), emptyAtom);
        index = addRange(source, index, iter->nameRange.end - token.startIndex(), "webkit-html-attribute-name");

        if (tagName == baseTag && name == hrefAttr)
            addBase(value);

        index = addRange(source, index, iter->valueRange.start - token.startIndex(), emptyAtom);

        bool isLink = name == srcAttr || name == hrefAttr;
        index = addRange(source, index, iter->valueRange.end - token.startIndex(), "webkit-html-attribute-value", isLink, tagName == aTag, value);

        ++iter;
    }
    m_current = m_td;
}

void HTMLViewSourceDocument::processCommentToken(const String& source, HTMLToken&)
{
    m_current = addSpanWithClassName("webkit-html-comment");
    addText(source, "webkit-html-comment");
    m_current = m_td;
}

void HTMLViewSourceDocument::processCharacterToken(const String& source, HTMLToken&)
{
    addText(source, "");
}

PassRefPtr<Element> HTMLViewSourceDocument::addSpanWithClassName(const AtomicString& className)
{
    if (m_current == m_tbody) {
        addLine(className);
        return m_current;
    }

    RefPtr<HTMLSpanElement> span = HTMLSpanElement::create(*this);
    span->setAttribute(classAttr, className);
    m_current->parserAppendChild(span);
    return span.release();
}

void HTMLViewSourceDocument::addLine(const AtomicString& className)
{
    // Create a table row.
    RefPtr<HTMLTableRowElement> trow = HTMLTableRowElement::create(*this);
    m_tbody->parserAppendChild(trow);

    // Create a cell that will hold the line number (it is generated in the stylesheet using counters).
    RefPtr<HTMLTableCellElement> td = HTMLTableCellElement::create(tdTag, *this);
    td->setAttribute(classAttr, "webkit-line-number");
    td->setIntegralAttribute(valueAttr, ++m_lineNumber);
    trow->parserAppendChild(td);

    // Create a second cell for the line contents
    td = HTMLTableCellElement::create(tdTag, *this);
    td->setAttribute(classAttr, "webkit-line-content");
    trow->parserAppendChild(td);
    m_current = m_td = td;

    // Open up the needed spans.
    if (!className.isEmpty()) {
        if (className == "webkit-html-attribute-name" || className == "webkit-html-attribute-value")
            m_current = addSpanWithClassName("webkit-html-tag");
        m_current = addSpanWithClassName(className);
    }
}

void HTMLViewSourceDocument::finishLine()
{
    if (!m_current->hasChildren()) {
        RefPtr<HTMLBRElement> br = HTMLBRElement::create(*this);
        m_current->parserAppendChild(br);
    }
    m_current = m_tbody;
}

void HTMLViewSourceDocument::addText(const String& text, const AtomicString& className)
{
    if (text.isEmpty())
        return;

    // Add in the content, splitting on newlines.
    Vector<String> lines;
    text.split('\n', true, lines);
    unsigned size = lines.size();
    for (unsigned i = 0; i < size; i++) {
        String substring = lines[i];
        if (m_current == m_tbody)
            addLine(className);
        if (substring.isEmpty()) {
            if (i == size - 1)
                break;
            finishLine();
            continue;
        }
        RefPtr<Text> t = Text::create(*this, substring);
        m_current->parserAppendChild(t);
        if (i < size - 1)
            finishLine();
    }
}

int HTMLViewSourceDocument::addRange(const String& source, int start, int end, const AtomicString& className, bool isLink, bool isAnchor, const AtomicString& link)
{
    ASSERT(start <= end);
    if (start == end)
        return start;

    String text = source.substring(start, end - start);
    if (!className.isEmpty()) {
        if (isLink)
            m_current = addLink(link, isAnchor);
        else
            m_current = addSpanWithClassName(className);
    }
    addText(text, className);
    if (!className.isEmpty() && m_current != m_tbody)
        m_current = toElement(m_current->parentNode());
    return end;
}

PassRefPtr<Element> HTMLViewSourceDocument::addBase(const AtomicString& href)
{
    RefPtr<HTMLBaseElement> base = HTMLBaseElement::create(*this);
    base->setAttribute(hrefAttr, href);
    m_current->parserAppendChild(base);
    return base.release();
}

PassRefPtr<Element> HTMLViewSourceDocument::addLink(const AtomicString& url, bool isAnchor)
{
    if (m_current == m_tbody)
        addLine("webkit-html-tag");

    // Now create a link for the attribute value instead of a span.
    RefPtr<HTMLAnchorElement> anchor = HTMLAnchorElement::create(*this);
    const char* classValue;
    if (isAnchor)
        classValue = "webkit-html-attribute-value webkit-html-external-link";
    else
        classValue = "webkit-html-attribute-value webkit-html-resource-link";
    anchor->setAttribute(classAttr, classValue);
    anchor->setAttribute(targetAttr, "_blank");
    anchor->setAttribute(hrefAttr, url);
    m_current->parserAppendChild(anchor);
    return anchor.release();
}

}
