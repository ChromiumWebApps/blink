/*
 * Copyright (C) 2010, 2011 Nokia Corporation and/or its subsidiary(-ies)
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

#include "config.h"
#include "core/html/HTMLDetailsElement.h"

#include "CSSPropertyNames.h"
#include "CSSValueKeywords.h"
#include "HTMLNames.h"
#include "bindings/v8/ExceptionStatePlaceholder.h"
#include "core/dom/Text.h"
#include "core/dom/shadow/ShadowRoot.h"
#include "core/html/HTMLContentElement.h"
#include "core/html/HTMLDivElement.h"
#include "core/html/HTMLSummaryElement.h"
#include "core/html/shadow/ShadowElementNames.h"
#include "core/rendering/RenderBlockFlow.h"
#include "platform/text/PlatformLocale.h"

namespace WebCore {

using namespace HTMLNames;

PassRefPtr<HTMLDetailsElement> HTMLDetailsElement::create(Document& document)
{
    RefPtr<HTMLDetailsElement> details = adoptRef(new HTMLDetailsElement(document));
    details->ensureUserAgentShadowRoot();
    return details.release();
}

HTMLDetailsElement::HTMLDetailsElement(Document& document)
    : HTMLElement(detailsTag, document)
    , m_isOpen(false)
{
    ScriptWrappable::init(this);
}

RenderObject* HTMLDetailsElement::createRenderer(RenderStyle*)
{
    return new RenderBlockFlow(this);
}

void HTMLDetailsElement::didAddUserAgentShadowRoot(ShadowRoot& root)
{
    DEFINE_STATIC_LOCAL(const AtomicString, summarySelector, ("summary:first-of-type", AtomicString::ConstructFromLiteral));

    RefPtr<HTMLSummaryElement> defaultSummary = HTMLSummaryElement::create(document());
    defaultSummary->appendChild(Text::create(document(), locale().queryString(blink::WebLocalizedString::DetailsLabel)));

    RefPtr<HTMLContentElement> summary = HTMLContentElement::create(document());
    summary->setIdAttribute(ShadowElementNames::detailsSummary());
    summary->setAttribute(selectAttr, summarySelector);
    summary->appendChild(defaultSummary);
    root.appendChild(summary.release());

    RefPtr<HTMLDivElement> content = HTMLDivElement::create(document());
    content->setIdAttribute(ShadowElementNames::detailsContent());
    content->appendChild(HTMLContentElement::create(document()));
    content->setInlineStyleProperty(CSSPropertyDisplay, CSSValueNone);
    root.appendChild(content.release());
}

Element* HTMLDetailsElement::findMainSummary() const
{
    if (HTMLSummaryElement* summary = Traversal<HTMLSummaryElement>::firstChild(*this))
        return summary;

    HTMLContentElement* content = toHTMLContentElement(userAgentShadowRoot()->firstChild());
    ASSERT(content->firstChild() && isHTMLSummaryElement(*content->firstChild()));
    return toElement(content->firstChild());
}

void HTMLDetailsElement::parseAttribute(const QualifiedName& name, const AtomicString& value)
{
    if (name == openAttr) {
        bool oldValue = m_isOpen;
        m_isOpen = !value.isNull();
        if (m_isOpen == oldValue)
            return;
        Element* content = ensureUserAgentShadowRoot().getElementById(ShadowElementNames::detailsContent());
        ASSERT(content);
        if (m_isOpen)
            content->removeInlineStyleProperty(CSSPropertyDisplay);
        else
            content->setInlineStyleProperty(CSSPropertyDisplay, CSSValueNone);
        Element* summary = ensureUserAgentShadowRoot().getElementById(ShadowElementNames::detailsSummary());
        ASSERT(summary);
        // FIXME: DetailsMarkerControl's RenderDetailsMarker has no concept of being updated
        // without recreating it causing a repaint. Instead we should change it so we can tell
        // it to toggle the open/closed triangle state and avoid reattaching the entire summary.
        summary->lazyReattachIfAttached();
        return;
    }
    HTMLElement::parseAttribute(name, value);
}

void HTMLDetailsElement::toggleOpen()
{
    setAttribute(openAttr, m_isOpen ? nullAtom : emptyAtom);
}

bool HTMLDetailsElement::isInteractiveContent() const
{
    return true;
}

}
