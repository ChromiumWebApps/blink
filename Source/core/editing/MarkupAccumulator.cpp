/*
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2012 Apple Inc. All rights reserved.
 * Copyright (C) 2009, 2010 Google Inc. All rights reserved.
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
#include "core/editing/MarkupAccumulator.h"

#include "HTMLNames.h"
#include "XLinkNames.h"
#include "XMLNSNames.h"
#include "XMLNames.h"
#include "core/dom/CDATASection.h"
#include "core/dom/Comment.h"
#include "core/dom/DocumentFragment.h"
#include "core/dom/DocumentType.h"
#include "core/dom/ProcessingInstruction.h"
#include "core/editing/Editor.h"
#include "core/html/HTMLElement.h"
#include "core/html/HTMLTemplateElement.h"
#include "platform/weborigin/KURL.h"
#include "wtf/unicode/CharacterNames.h"

namespace WebCore {

using namespace HTMLNames;

struct EntityDescription {
    UChar entity;
    const CString& reference;
    EntityMask mask;
};

template <typename CharType>
static inline void appendCharactersReplacingEntitiesInternal(StringBuilder& result, CharType* text, unsigned length, const EntityDescription entityMaps[], unsigned entityMapsCount, EntityMask entityMask)
{
    unsigned positionAfterLastEntity = 0;
    for (unsigned i = 0; i < length; ++i) {
        for (unsigned entityIndex = 0; entityIndex < entityMapsCount; ++entityIndex) {
            if (text[i] == entityMaps[entityIndex].entity && entityMaps[entityIndex].mask & entityMask) {
                result.append(text + positionAfterLastEntity, i - positionAfterLastEntity);
                const CString& replacement = entityMaps[entityIndex].reference;
                result.append(replacement.data(), replacement.length());
                positionAfterLastEntity = i + 1;
                break;
            }
        }
    }
    result.append(text + positionAfterLastEntity, length - positionAfterLastEntity);
}

void MarkupAccumulator::appendCharactersReplacingEntities(StringBuilder& result, const String& source, unsigned offset, unsigned length, EntityMask entityMask)
{
    DEFINE_STATIC_LOCAL(const CString, ampReference, ("&amp;"));
    DEFINE_STATIC_LOCAL(const CString, ltReference, ("&lt;"));
    DEFINE_STATIC_LOCAL(const CString, gtReference, ("&gt;"));
    DEFINE_STATIC_LOCAL(const CString, quotReference, ("&quot;"));
    DEFINE_STATIC_LOCAL(const CString, nbspReference, ("&nbsp;"));

    static const EntityDescription entityMaps[] = {
        { '&', ampReference, EntityAmp },
        { '<', ltReference, EntityLt },
        { '>', gtReference, EntityGt },
        { '"', quotReference, EntityQuot },
        { noBreakSpace, nbspReference, EntityNbsp },
    };

    if (!(offset + length))
        return;

    ASSERT(offset + length <= source.length());
    if (source.is8Bit())
        appendCharactersReplacingEntitiesInternal(result, source.characters8() + offset, length, entityMaps, WTF_ARRAY_LENGTH(entityMaps), entityMask);
    else
        appendCharactersReplacingEntitiesInternal(result, source.characters16() + offset, length, entityMaps, WTF_ARRAY_LENGTH(entityMaps), entityMask);
}

MarkupAccumulator::MarkupAccumulator(Vector<Node*>* nodes, EAbsoluteURLs resolveUrlsMethod, const Range* range, SerializationType serializationType)
    : m_nodes(nodes)
    , m_range(range)
    , m_resolveURLsMethod(resolveUrlsMethod)
    , m_serializationType(serializationType)
{
}

MarkupAccumulator::~MarkupAccumulator()
{
}

String MarkupAccumulator::serializeNodes(Node& targetNode, EChildrenOnly childrenOnly, Vector<QualifiedName>* tagNamesToSkip)
{
    Namespaces* namespaces = 0;
    Namespaces namespaceHash;
    if (!serializeAsHTMLDocument(targetNode)) {
        // Add pre-bound namespaces for XML fragments.
        namespaceHash.set(xmlAtom.impl(), XMLNames::xmlNamespaceURI.impl());
        namespaces = &namespaceHash;
    }

    serializeNodesWithNamespaces(targetNode, childrenOnly, namespaces, tagNamesToSkip);
    return m_markup.toString();
}

void MarkupAccumulator::serializeNodesWithNamespaces(Node& targetNode, EChildrenOnly childrenOnly, const Namespaces* namespaces, Vector<QualifiedName>* tagNamesToSkip)
{
    if (tagNamesToSkip) {
        for (size_t i = 0; i < tagNamesToSkip->size(); ++i) {
            if (targetNode.hasTagName(tagNamesToSkip->at(i)))
                return;
        }
    }

    Namespaces namespaceHash;
    if (namespaces)
        namespaceHash = *namespaces;

    if (!childrenOnly)
        appendStartTag(targetNode, &namespaceHash);

    if (!(serializeAsHTMLDocument(targetNode) && elementCannotHaveEndTag(targetNode))) {
        Node* current = isHTMLTemplateElement(targetNode) ? toHTMLTemplateElement(targetNode).content()->firstChild() : targetNode.firstChild();
        for ( ; current; current = current->nextSibling())
            serializeNodesWithNamespaces(*current, IncludeNode, &namespaceHash, tagNamesToSkip);
    }

    if (!childrenOnly)
        appendEndTag(targetNode);
}

String MarkupAccumulator::resolveURLIfNeeded(const Element& element, const String& urlString) const
{
    switch (m_resolveURLsMethod) {
    case ResolveAllURLs:
        return element.document().completeURL(urlString).string();

    case ResolveNonLocalURLs:
        if (!element.document().url().isLocalFile())
            return element.document().completeURL(urlString).string();
        break;

    case DoNotResolveURLs:
        break;
    }
    return urlString;
}

void MarkupAccumulator::appendString(const String& string)
{
    m_markup.append(string);
}

void MarkupAccumulator::appendStartTag(Node& node, Namespaces* namespaces)
{
    appendStartMarkup(m_markup, node, namespaces);
    if (m_nodes)
        m_nodes->append(&node);
}

void MarkupAccumulator::appendEndTag(const Node& node)
{
    appendEndMarkup(m_markup, node);
}

size_t MarkupAccumulator::totalLength(const Vector<String>& strings)
{
    size_t length = 0;
    for (size_t i = 0; i < strings.size(); ++i)
        length += strings[i].length();
    return length;
}

void MarkupAccumulator::concatenateMarkup(StringBuilder& result)
{
    result.append(m_markup);
}

void MarkupAccumulator::appendAttributeValue(StringBuilder& result, const String& attribute, bool documentIsHTML)
{
    appendCharactersReplacingEntities(result, attribute, 0, attribute.length(),
        documentIsHTML ? EntityMaskInHTMLAttributeValue : EntityMaskInAttributeValue);
}

void MarkupAccumulator::appendCustomAttributes(StringBuilder&, const Element&, Namespaces*)
{
}

void MarkupAccumulator::appendQuotedURLAttributeValue(StringBuilder& result, const Element& element, const Attribute& attribute)
{
    ASSERT(element.isURLAttribute(attribute));
    const String resolvedURLString = resolveURLIfNeeded(element, attribute.value());
    UChar quoteChar = '"';
    String strippedURLString = resolvedURLString.stripWhiteSpace();
    if (protocolIsJavaScript(strippedURLString)) {
        // minimal escaping for javascript urls
        if (strippedURLString.contains('"')) {
            if (strippedURLString.contains('\''))
                strippedURLString.replaceWithLiteral('"', "&quot;");
            else
                quoteChar = '\'';
        }
        result.append(quoteChar);
        result.append(strippedURLString);
        result.append(quoteChar);
        return;
    }

    // FIXME: This does not fully match other browsers. Firefox percent-escapes non-ASCII characters for innerHTML.
    result.append(quoteChar);
    appendAttributeValue(result, resolvedURLString, false);
    result.append(quoteChar);
}

bool MarkupAccumulator::shouldAddNamespaceElement(const Element& element, Namespaces& namespaces)
{
    // Don't add namespace attribute if it is already defined for this elem.
    const AtomicString& prefix = element.prefix();
    if (prefix.isEmpty()) {
        if (element.hasAttribute(xmlnsAtom)) {
            namespaces.set(emptyAtom.impl(), element.namespaceURI().impl());
            return false;
        }
        return true;
    }

    DEFINE_STATIC_LOCAL(String, xmlnsWithColon, ("xmlns:"));
    return !element.hasAttribute(xmlnsWithColon + prefix);
}

bool MarkupAccumulator::shouldAddNamespaceAttribute(const Attribute& attribute, Namespaces& namespaces)
{
    // Don't add namespace attributes twice
    if (attribute.name() == XMLNSNames::xmlnsAttr) {
        namespaces.set(emptyAtom.impl(), attribute.value().impl());
        return false;
    }

    QualifiedName xmlnsPrefixAttr(xmlnsAtom, attribute.localName(), XMLNSNames::xmlnsNamespaceURI);
    if (attribute.name() == xmlnsPrefixAttr) {
        namespaces.set(attribute.localName().impl(), attribute.value().impl());
        return false;
    }

    return true;
}

void MarkupAccumulator::appendNamespace(StringBuilder& result, const AtomicString& prefix, const AtomicString& namespaceURI, Namespaces& namespaces)
{
    if (namespaceURI.isEmpty())
        return;

    // Use emptyAtoms's impl() for both null and empty strings since the HashMap can't handle 0 as a key
    StringImpl* pre = prefix.isEmpty() ? emptyAtom.impl() : prefix.impl();
    StringImpl* foundNS = namespaces.get(pre);
    if (foundNS != namespaceURI.impl()) {
        namespaces.set(pre, namespaceURI.impl());
        result.append(' ');
        result.append(xmlnsAtom.string());
        if (!prefix.isEmpty()) {
            result.append(':');
            result.append(prefix);
        }

        result.appendLiteral("=\"");
        appendAttributeValue(result, namespaceURI, false);
        result.append('"');
    }
}

EntityMask MarkupAccumulator::entityMaskForText(const Text& text) const
{
    if (!serializeAsHTMLDocument(text))
        return EntityMaskInPCDATA;

    const QualifiedName* parentName = 0;
    if (text.parentElement())
        parentName = &(text.parentElement())->tagQName();

    if (parentName && (*parentName == scriptTag || *parentName == styleTag || *parentName == xmpTag))
        return EntityMaskInCDATA;
    return EntityMaskInHTMLPCDATA;
}

void MarkupAccumulator::appendText(StringBuilder& result, Text& text)
{
    const String& str = text.data();
    unsigned length = str.length();
    unsigned start = 0;

    if (m_range) {
        if (text == m_range->endContainer())
            length = m_range->endOffset();
        if (text == m_range->startContainer()) {
            start = m_range->startOffset();
            length -= start;
        }
    }
    appendCharactersReplacingEntities(result, str, start, length, entityMaskForText(text));
}

void MarkupAccumulator::appendComment(StringBuilder& result, const String& comment)
{
    // FIXME: Comment content is not escaped, but XMLSerializer (and possibly other callers) should raise an exception if it includes "-->".
    result.appendLiteral("<!--");
    result.append(comment);
    result.appendLiteral("-->");
}

void MarkupAccumulator::appendXMLDeclaration(StringBuilder& result, const Document& document)
{
    if (!document.hasXMLDeclaration())
        return;

    result.appendLiteral("<?xml version=\"");
    result.append(document.xmlVersion());
    const String& encoding = document.xmlEncoding();
    if (!encoding.isEmpty()) {
        result.appendLiteral("\" encoding=\"");
        result.append(encoding);
    }
    if (document.xmlStandaloneStatus() != Document::StandaloneUnspecified) {
        result.appendLiteral("\" standalone=\"");
        if (document.xmlStandalone())
            result.appendLiteral("yes");
        else
            result.appendLiteral("no");
    }

    result.appendLiteral("\"?>");
}

void MarkupAccumulator::appendDocumentType(StringBuilder& result, const DocumentType& n)
{
    if (n.name().isEmpty())
        return;

    result.appendLiteral("<!DOCTYPE ");
    result.append(n.name());
    if (!n.publicId().isEmpty()) {
        result.appendLiteral(" PUBLIC \"");
        result.append(n.publicId());
        result.append('"');
        if (!n.systemId().isEmpty()) {
            result.appendLiteral(" \"");
            result.append(n.systemId());
            result.append('"');
        }
    } else if (!n.systemId().isEmpty()) {
        result.appendLiteral(" SYSTEM \"");
        result.append(n.systemId());
        result.append('"');
    }
    if (!n.internalSubset().isEmpty()) {
        result.appendLiteral(" [");
        result.append(n.internalSubset());
        result.append(']');
    }
    result.append('>');
}

void MarkupAccumulator::appendProcessingInstruction(StringBuilder& result, const String& target, const String& data)
{
    // FIXME: PI data is not escaped, but XMLSerializer (and possibly other callers) this should raise an exception if it includes "?>".
    result.appendLiteral("<?");
    result.append(target);
    result.append(' ');
    result.append(data);
    result.appendLiteral("?>");
}

void MarkupAccumulator::appendElement(StringBuilder& result, Element& element, Namespaces* namespaces)
{
    appendOpenTag(result, element, namespaces);

    if (element.hasAttributes()) {
        unsigned length = element.attributeCount();
        for (unsigned int i = 0; i < length; i++)
            appendAttribute(result, element, element.attributeItem(i), namespaces);
    }

    // Give an opportunity to subclasses to add their own attributes.
    appendCustomAttributes(result, element, namespaces);

    appendCloseTag(result, element);
}

static String nodeNamePreservingCase(const Element& element)
{
    return element.tagQName().toString();
}

void MarkupAccumulator::appendOpenTag(StringBuilder& result, const Element& element, Namespaces* namespaces)
{
    result.append('<');
    result.append(nodeNamePreservingCase(element));
    if (!serializeAsHTMLDocument(element) && namespaces && shouldAddNamespaceElement(element, *namespaces))
        appendNamespace(result, element.prefix(), element.namespaceURI(), *namespaces);
}

void MarkupAccumulator::appendCloseTag(StringBuilder& result, const Element& element)
{
    if (shouldSelfClose(element)) {
        if (element.isHTMLElement())
            result.append(' '); // XHTML 1.0 <-> HTML compatibility.
        result.append('/');
    }
    result.append('>');
}

static inline bool attributeIsInSerializedNamespace(const Attribute& attribute)
{
    return attribute.namespaceURI() == XMLNames::xmlNamespaceURI
        || attribute.namespaceURI() == XLinkNames::xlinkNamespaceURI
        || attribute.namespaceURI() == XMLNSNames::xmlnsNamespaceURI;
}

void MarkupAccumulator::appendAttribute(StringBuilder& result, const Element& element, const Attribute& attribute, Namespaces* namespaces)
{
    bool documentIsHTML = serializeAsHTMLDocument(element);

    result.append(' ');

    QualifiedName prefixedName = attribute.name();
    if (documentIsHTML && !attributeIsInSerializedNamespace(attribute)) {
        result.append(attribute.name().localName());
    } else {
        if (attribute.namespaceURI() == XLinkNames::xlinkNamespaceURI) {
            if (!attribute.prefix())
                prefixedName.setPrefix(xlinkAtom);
        } else if (attribute.namespaceURI() == XMLNames::xmlNamespaceURI) {
            if (!attribute.prefix())
                prefixedName.setPrefix(xmlAtom);
        } else if (attribute.namespaceURI() == XMLNSNames::xmlnsNamespaceURI) {
            if (attribute.name() != XMLNSNames::xmlnsAttr && !attribute.prefix())
                prefixedName.setPrefix(xmlnsAtom);
        }
        result.append(prefixedName.toString());
    }

    result.append('=');

    if (element.isURLAttribute(attribute)) {
        appendQuotedURLAttributeValue(result, element, attribute);
    } else {
        result.append('"');
        appendAttributeValue(result, attribute.value(), documentIsHTML);
        result.append('"');
    }

    if (!documentIsHTML && namespaces && shouldAddNamespaceAttribute(attribute, *namespaces))
        appendNamespace(result, prefixedName.prefix(), prefixedName.namespaceURI(), *namespaces);
}

void MarkupAccumulator::appendCDATASection(StringBuilder& result, const String& section)
{
    // FIXME: CDATA content is not escaped, but XMLSerializer (and possibly other callers) should raise an exception if it includes "]]>".
    result.appendLiteral("<![CDATA[");
    result.append(section);
    result.appendLiteral("]]>");
}

void MarkupAccumulator::appendStartMarkup(StringBuilder& result, Node& node, Namespaces* namespaces)
{
    switch (node.nodeType()) {
    case Node::TEXT_NODE:
        appendText(result, toText(node));
        break;
    case Node::COMMENT_NODE:
        appendComment(result, toComment(node).data());
        break;
    case Node::DOCUMENT_NODE:
        appendXMLDeclaration(result, toDocument(node));
        break;
    case Node::DOCUMENT_FRAGMENT_NODE:
        break;
    case Node::DOCUMENT_TYPE_NODE:
        appendDocumentType(result, toDocumentType(node));
        break;
    case Node::PROCESSING_INSTRUCTION_NODE:
        appendProcessingInstruction(result, toProcessingInstruction(node).target(), toProcessingInstruction(node).data());
        break;
    case Node::ELEMENT_NODE:
        appendElement(result, toElement(node), namespaces);
        break;
    case Node::CDATA_SECTION_NODE:
        appendCDATASection(result, toCDATASection(node).data());
        break;
    case Node::ATTRIBUTE_NODE:
        ASSERT_NOT_REACHED();
        break;
    }
}

// Rules of self-closure
// 1. No elements in HTML documents use the self-closing syntax.
// 2. Elements w/ children never self-close because they use a separate end tag.
// 3. HTML elements which do not have a "forbidden" end tag will close with a separate end tag.
// 4. Other elements self-close.
bool MarkupAccumulator::shouldSelfClose(const Node& node)
{
    if (serializeAsHTMLDocument(node))
        return false;
    if (node.hasChildren())
        return false;
    if (node.isHTMLElement() && !elementCannotHaveEndTag(node))
        return false;
    return true;
}

bool MarkupAccumulator::elementCannotHaveEndTag(const Node& node)
{
    if (!node.isHTMLElement())
        return false;

    // FIXME: ieForbidsInsertHTML may not be the right function to call here
    // ieForbidsInsertHTML is used to disallow setting innerHTML/outerHTML
    // or createContextualFragment.  It does not necessarily align with
    // which elements should be serialized w/o end tags.
    return toHTMLElement(node).ieForbidsInsertHTML();
}

void MarkupAccumulator::appendEndMarkup(StringBuilder& result, const Node& node)
{
    if (!node.isElementNode() || shouldSelfClose(node) || (!node.hasChildren() && elementCannotHaveEndTag(node)))
        return;

    result.appendLiteral("</");
    result.append(nodeNamePreservingCase(toElement(node)));
    result.append('>');
}

bool MarkupAccumulator::serializeAsHTMLDocument(const Node& node) const
{
    if (m_serializationType == ForcedXML)
        return false;
    return node.document().isHTMLDocument();
}

}
