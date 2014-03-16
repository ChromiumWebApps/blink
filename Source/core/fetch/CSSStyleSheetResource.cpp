/*
    Copyright (C) 1998 Lars Knoll (knoll@mpi-hd.mpg.de)
    Copyright (C) 2001 Dirk Mueller (mueller@kde.org)
    Copyright (C) 2002 Waldo Bastian (bastian@kde.org)
    Copyright (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
    Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.

    This class provides all functionality needed for loading images, style sheets and html
    pages from the web. It has a memory cache for these objects.
*/

#include "config.h"
#include "core/fetch/CSSStyleSheetResource.h"

#include "core/css/StyleSheetContents.h"
#include "core/fetch/ResourceClientWalker.h"
#include "core/fetch/StyleSheetResourceClient.h"
#include "core/html/parser/TextResourceDecoder.h"
#include "platform/SharedBuffer.h"
#include "platform/network/HTTPParsers.h"
#include "wtf/CurrentTime.h"
#include "wtf/Vector.h"

namespace WebCore {

CSSStyleSheetResource::CSSStyleSheetResource(const ResourceRequest& resourceRequest, const String& charset)
    : StyleSheetResource(resourceRequest, CSSStyleSheet)
    , m_decoder(TextResourceDecoder::create("text/css", charset))
{
    DEFINE_STATIC_LOCAL(const AtomicString, acceptCSS, ("text/css,*/*;q=0.1", AtomicString::ConstructFromLiteral));

    // Prefer text/css but accept any type (dell.com serves a stylesheet
    // as text/html; see <http://bugs.webkit.org/show_bug.cgi?id=11451>).
    setAccept(acceptCSS);
}

CSSStyleSheetResource::~CSSStyleSheetResource()
{
    if (m_parsedStyleSheetCache)
        m_parsedStyleSheetCache->removedFromMemoryCache();
}

void CSSStyleSheetResource::didAddClient(ResourceClient* c)
{
    ASSERT(c->resourceClientType() == StyleSheetResourceClient::expectedType());
    // Resource::didAddClient() must be before setCSSStyleSheet(),
    // because setCSSStyleSheet() may cause scripts to be executed, which could destroy 'c' if it is an instance of HTMLLinkElement.
    // see the comment of HTMLLinkElement::setCSSStyleSheet.
    Resource::didAddClient(c);

    if (!isLoading())
        static_cast<StyleSheetResourceClient*>(c)->setCSSStyleSheet(m_resourceRequest.url(), m_response.url(), m_decoder->encoding().name(), this);
}

void CSSStyleSheetResource::setEncoding(const String& chs)
{
    m_decoder->setEncoding(chs, TextResourceDecoder::EncodingFromHTTPHeader);
}

String CSSStyleSheetResource::encoding() const
{
    return m_decoder->encoding().name();
}

const String CSSStyleSheetResource::sheetText(bool enforceMIMEType, bool* hasValidMIMEType) const
{
    ASSERT(!isPurgeable());

    if (!m_data || m_data->isEmpty() || !canUseSheet(enforceMIMEType, hasValidMIMEType))
        return String();

    if (!m_decodedSheetText.isNull())
        return m_decodedSheetText;

    // Don't cache the decoded text, regenerating is cheap and it can use quite a bit of memory
    String sheetText = m_decoder->decode(m_data->data(), m_data->size());
    sheetText = sheetText + m_decoder->flush();
    return sheetText;
}

void CSSStyleSheetResource::checkNotify()
{
    // Decode the data to find out the encoding and keep the sheet text around during checkNotify()
    if (m_data) {
        m_decodedSheetText = m_decoder->decode(m_data->data(), m_data->size());
        m_decodedSheetText = m_decodedSheetText + m_decoder->flush();
    }

    ResourceClientWalker<StyleSheetResourceClient> w(m_clients);
    while (StyleSheetResourceClient* c = w.next())
        c->setCSSStyleSheet(m_resourceRequest.url(), m_response.url(), m_decoder->encoding().name(), this);
    // Clear the decoded text as it is unlikely to be needed immediately again and is cheap to regenerate.
    m_decodedSheetText = String();
}

bool CSSStyleSheetResource::isSafeToUnlock() const
{
    return m_data->hasOneRef();
}

void CSSStyleSheetResource::destroyDecodedDataIfPossible()
{
    if (!m_parsedStyleSheetCache)
        return;

    m_parsedStyleSheetCache->removedFromMemoryCache();
    m_parsedStyleSheetCache.clear();

    setDecodedSize(0);
}

bool CSSStyleSheetResource::canUseSheet(bool enforceMIMEType, bool* hasValidMIMEType) const
{
    if (errorOccurred())
        return false;

    if (!enforceMIMEType && !hasValidMIMEType)
        return true;

    // This check exactly matches Firefox. Note that we grab the Content-Type
    // header directly because we want to see what the value is BEFORE content
    // sniffing. Firefox does this by setting a "type hint" on the channel.
    // This implementation should be observationally equivalent.
    //
    // This code defaults to allowing the stylesheet for non-HTTP protocols so
    // folks can use standards mode for local HTML documents.
    const AtomicString& mimeType = extractMIMETypeFromMediaType(response().httpHeaderField("Content-Type"));
    bool typeOK = mimeType.isEmpty() || equalIgnoringCase(mimeType, "text/css") || equalIgnoringCase(mimeType, "application/x-unknown-content-type");
    if (hasValidMIMEType)
        *hasValidMIMEType = typeOK;
    if (!enforceMIMEType)
        return true;
    return typeOK;
}

PassRefPtrWillBeRawPtr<StyleSheetContents> CSSStyleSheetResource::restoreParsedStyleSheet(const CSSParserContext& context)
{
    if (!m_parsedStyleSheetCache)
        return nullptr;
    if (m_parsedStyleSheetCache->hasFailedOrCanceledSubresources()) {
        m_parsedStyleSheetCache->removedFromMemoryCache();
        m_parsedStyleSheetCache.clear();
        return nullptr;
    }

    ASSERT(m_parsedStyleSheetCache->isCacheable());
    ASSERT(m_parsedStyleSheetCache->isInMemoryCache());

    // Contexts must be identical so we know we would get the same exact result if we parsed again.
    if (m_parsedStyleSheetCache->parserContext() != context)
        return nullptr;

    didAccessDecodedData(currentTime());

    return m_parsedStyleSheetCache;
}

void CSSStyleSheetResource::saveParsedStyleSheet(PassRefPtrWillBeRawPtr<StyleSheetContents> sheet)
{
    ASSERT(sheet && sheet->isCacheable());

    if (m_parsedStyleSheetCache)
        m_parsedStyleSheetCache->removedFromMemoryCache();
    m_parsedStyleSheetCache = sheet;
    m_parsedStyleSheetCache->addedToMemoryCache();

    setDecodedSize(m_parsedStyleSheetCache->estimatedSizeInBytes());
}

}
