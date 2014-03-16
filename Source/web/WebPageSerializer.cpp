/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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
#include "WebPageSerializer.h"

#include "HTMLNames.h"
#include "WebFrame.h"
#include "WebFrameImpl.h"
#include "WebPageSerializerClient.h"
#include "WebPageSerializerImpl.h"
#include "WebView.h"
#include "WebViewImpl.h"
#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/frame/LocalFrame.h"
#include "core/html/HTMLAllCollection.h"
#include "core/html/HTMLFrameOwnerElement.h"
#include "core/html/HTMLInputElement.h"
#include "core/html/HTMLTableElement.h"
#include "core/loader/DocumentLoader.h"
#include "core/page/PageSerializer.h"
#include "platform/SerializedResource.h"
#include "platform/mhtml/MHTMLArchive.h"
#include "platform/weborigin/KURL.h"
#include "public/platform/WebCString.h"
#include "public/platform/WebString.h"
#include "public/platform/WebURL.h"
#include "public/platform/WebVector.h"
#include "wtf/Vector.h"
#include "wtf/text/StringConcatenate.h"

using namespace WebCore;

namespace {

KURL getSubResourceURLFromElement(Element* element)
{
    ASSERT(element);
    const QualifiedName* attributeName = 0;
    if (element->hasTagName(HTMLNames::imgTag) || element->hasTagName(HTMLNames::scriptTag))
        attributeName = &HTMLNames::srcAttr;
    else if (element->hasTagName(HTMLNames::inputTag)) {
        if (toHTMLInputElement(element)->isImageButton())
            attributeName = &HTMLNames::srcAttr;
    } else if (element->hasTagName(HTMLNames::bodyTag)
        || element->hasTagName(HTMLNames::tableTag)
        || element->hasTagName(HTMLNames::trTag)
        || element->hasTagName(HTMLNames::tdTag))
        attributeName = &HTMLNames::backgroundAttr;
    else if (element->hasTagName(HTMLNames::blockquoteTag)
             || element->hasTagName(HTMLNames::qTag)
             || element->hasTagName(HTMLNames::delTag)
             || element->hasTagName(HTMLNames::insTag))
        attributeName = &HTMLNames::citeAttr;
    else if (element->hasTagName(HTMLNames::linkTag)) {
        // If the link element is not css, ignore it.
        if (equalIgnoringCase(element->getAttribute(HTMLNames::typeAttr), "text/css")) {
            // FIXME: Add support for extracting links of sub-resources which
            // are inside style-sheet such as @import, @font-face, url(), etc.
            attributeName = &HTMLNames::hrefAttr;
        }
    } else if (element->hasTagName(HTMLNames::objectTag))
        attributeName = &HTMLNames::dataAttr;
    else if (element->hasTagName(HTMLNames::embedTag))
        attributeName = &HTMLNames::srcAttr;

    if (!attributeName)
        return KURL();

    String value = element->getAttribute(*attributeName);
    // Ignore javascript content.
    if (value.isEmpty() || value.stripWhiteSpace().startsWith("javascript:", false))
        return KURL();

    return element->document().completeURL(value);
}

void retrieveResourcesForElement(Element* element,
                                 Vector<LocalFrame*>* visitedFrames,
                                 Vector<LocalFrame*>* framesToVisit,
                                 Vector<KURL>* frameURLs,
                                 Vector<KURL>* resourceURLs)
{
    // If the node is a frame, we'll process it later in retrieveResourcesForFrame.
    if ((element->hasTagName(HTMLNames::iframeTag) || element->hasTagName(HTMLNames::frameTag)
        || element->hasTagName(HTMLNames::objectTag) || element->hasTagName(HTMLNames::embedTag))
            && element->isFrameOwnerElement()) {
        if (LocalFrame* frame = toHTMLFrameOwnerElement(element)->contentFrame()) {
            if (!visitedFrames->contains(frame))
                framesToVisit->append(frame);
            return;
        }
    }

    KURL url = getSubResourceURLFromElement(element);
    if (url.isEmpty() || !url.isValid())
        return; // No subresource for this node.

    // Ignore URLs that have a non-standard protocols. Since the FTP protocol
    // does no have a cache mechanism, we skip it as well.
    if (!url.protocolIsInHTTPFamily() && !url.isLocalFile())
        return;

    if (!resourceURLs->contains(url))
        resourceURLs->append(url);
}

void retrieveResourcesForFrame(LocalFrame* frame,
                               const blink::WebVector<blink::WebCString>& supportedSchemes,
                               Vector<LocalFrame*>* visitedFrames,
                               Vector<LocalFrame*>* framesToVisit,
                               Vector<KURL>* frameURLs,
                               Vector<KURL>* resourceURLs)
{
    KURL frameURL = frame->loader().documentLoader()->request().url();

    // If the frame's URL is invalid, ignore it, it is not retrievable.
    if (!frameURL.isValid())
        return;

    // Ignore frames from unsupported schemes.
    bool isValidScheme = false;
    for (size_t i = 0; i < supportedSchemes.size(); ++i) {
        if (frameURL.protocolIs(static_cast<CString>(supportedSchemes[i]).data())) {
            isValidScheme = true;
            break;
        }
    }
    if (!isValidScheme)
        return;

    // If we have already seen that frame, ignore it.
    if (visitedFrames->contains(frame))
        return;
    visitedFrames->append(frame);
    if (!frameURLs->contains(frameURL))
        frameURLs->append(frameURL);

    // Now get the resources associated with each node of the document.
    RefPtr<HTMLCollection> allElements = frame->document()->all();
    for (unsigned i = 0; i < allElements->length(); ++i) {
        Element* element = allElements->item(i);
        retrieveResourcesForElement(element,
                                    visitedFrames, framesToVisit,
                                    frameURLs, resourceURLs);
    }
}

} // namespace

namespace blink {

void WebPageSerializer::serialize(WebView* view, WebVector<WebPageSerializer::Resource>* resourcesParam)
{
    Vector<SerializedResource> resources;
    PageSerializer serializer(&resources);
    serializer.serialize(toWebViewImpl(view)->page());

    Vector<Resource> result;
    for (Vector<SerializedResource>::const_iterator iter = resources.begin(); iter != resources.end(); ++iter) {
        Resource resource;
        resource.url = iter->url;
        resource.mimeType = iter->mimeType.ascii();
        // FIXME: we are copying all the resource data here. Idealy we would have a WebSharedData().
        resource.data = WebCString(iter->data->data(), iter->data->size());
        result.append(resource);
    }

    *resourcesParam = result;
}

static PassRefPtr<SharedBuffer> serializePageToMHTML(Page* page, MHTMLArchive::EncodingPolicy encodingPolicy)
{
    Vector<SerializedResource> resources;
    PageSerializer serializer(&resources);
    serializer.serialize(page);
    Document* document = page->mainFrame()->document();
    return MHTMLArchive::generateMHTMLData(resources, encodingPolicy, document->title(), document->suggestedMIMEType());
}

WebCString WebPageSerializer::serializeToMHTML(WebView* view)
{
    RefPtr<SharedBuffer> mhtml = serializePageToMHTML(toWebViewImpl(view)->page(), MHTMLArchive::UseDefaultEncoding);
    // FIXME: we are copying all the data here. Idealy we would have a WebSharedData().
    return WebCString(mhtml->data(), mhtml->size());
}

WebCString WebPageSerializer::serializeToMHTMLUsingBinaryEncoding(WebView* view)
{
    RefPtr<SharedBuffer> mhtml = serializePageToMHTML(toWebViewImpl(view)->page(), MHTMLArchive::UseBinaryEncoding);
    // FIXME: we are copying all the data here. Idealy we would have a WebSharedData().
    return WebCString(mhtml->data(), mhtml->size());
}

bool WebPageSerializer::serialize(WebFrame* frame,
                                  bool recursive,
                                  WebPageSerializerClient* client,
                                  const WebVector<WebURL>& links,
                                  const WebVector<WebString>& localPaths,
                                  const WebString& localDirectoryName)
{
    WebPageSerializerImpl serializerImpl(
        frame, recursive, client, links, localPaths, localDirectoryName);
    return serializerImpl.serialize();
}

bool WebPageSerializer::retrieveAllResources(WebView* view,
                                             const WebVector<WebCString>& supportedSchemes,
                                             WebVector<WebURL>* resourceURLs,
                                             WebVector<WebURL>* frameURLs) {
    WebFrameImpl* mainFrame = toWebFrameImpl(view->mainFrame());
    if (!mainFrame)
        return false;

    Vector<LocalFrame*> framesToVisit;
    Vector<LocalFrame*> visitedFrames;
    Vector<KURL> frameKURLs;
    Vector<KURL> resourceKURLs;

    // Let's retrieve the resources from every frame in this page.
    framesToVisit.append(mainFrame->frame());
    while (!framesToVisit.isEmpty()) {
        LocalFrame* frame = framesToVisit[0];
        framesToVisit.remove(0);
        retrieveResourcesForFrame(frame, supportedSchemes,
                                  &visitedFrames, &framesToVisit,
                                  &frameKURLs, &resourceKURLs);
    }

    // Converts the results to WebURLs.
    WebVector<WebURL> resultResourceURLs(resourceKURLs.size());
    for (size_t i = 0; i < resourceKURLs.size(); ++i) {
        resultResourceURLs[i] = resourceKURLs[i];
        // A frame's src can point to the same URL as another resource, keep the
        // resource URL only in such cases.
        size_t index = frameKURLs.find(resourceKURLs[i]);
        if (index != kNotFound)
            frameKURLs.remove(index);
    }
    *resourceURLs = resultResourceURLs;
    WebVector<WebURL> resultFrameURLs(frameKURLs.size());
    for (size_t i = 0; i < frameKURLs.size(); ++i)
        resultFrameURLs[i] = frameKURLs[i];
    *frameURLs = resultFrameURLs;

    return true;
}

WebString WebPageSerializer::generateMetaCharsetDeclaration(const WebString& charset)
{
    String charsetString = "<meta http-equiv=\"Content-Type\" content=\"text/html; charset=" + static_cast<const String&>(charset) + "\">";
    return charsetString;
}

WebString WebPageSerializer::generateMarkOfTheWebDeclaration(const WebURL& url)
{
    return String::format("\n<!-- saved from url=(%04d)%s -->\n",
                          static_cast<int>(url.spec().length()),
                          url.spec().data());
}

WebString WebPageSerializer::generateBaseTagDeclaration(const WebString& baseTarget)
{
    if (baseTarget.isEmpty())
        return String("<base href=\".\">");
    String baseString = "<base href=\".\" target=\"" + static_cast<const String&>(baseTarget) + "\">";
    return baseString;
}

} // namespace blink
