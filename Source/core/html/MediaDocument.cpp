/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include "core/html/MediaDocument.h"

#include "HTMLNames.h"
#include "bindings/v8/ExceptionStatePlaceholder.h"
#include "core/dom/ElementTraversal.h"
#include "core/dom/RawDataDocumentParser.h"
#include "core/events/KeyboardEvent.h"
#include "core/events/ThreadLocalEventNames.h"
#include "core/frame/LocalFrame.h"
#include "core/html/HTMLBodyElement.h"
#include "core/html/HTMLHeadElement.h"
#include "core/html/HTMLHtmlElement.h"
#include "core/html/HTMLMetaElement.h"
#include "core/html/HTMLSourceElement.h"
#include "core/html/HTMLVideoElement.h"
#include "core/loader/DocumentLoader.h"
#include "core/loader/FrameLoader.h"
#include "platform/KeyboardCodes.h"

namespace WebCore {

using namespace HTMLNames;

// FIXME: Share more code with PluginDocumentParser.
class MediaDocumentParser : public RawDataDocumentParser {
public:
    static PassRefPtr<MediaDocumentParser> create(MediaDocument* document)
    {
        return adoptRef(new MediaDocumentParser(document));
    }

private:
    explicit MediaDocumentParser(Document* document)
        : RawDataDocumentParser(document)
        , m_didBuildDocumentStructure(false)
    {
    }

    virtual void appendBytes(const char*, size_t) OVERRIDE;

    void createDocumentStructure();

    bool m_didBuildDocumentStructure;
};

void MediaDocumentParser::createDocumentStructure()
{
    ASSERT(document());
    RefPtr<HTMLHtmlElement> rootElement = HTMLHtmlElement::create(*document());
    rootElement->insertedByParser();
    document()->appendChild(rootElement);

    if (document()->frame())
        document()->frame()->loader().dispatchDocumentElementAvailable();

    RefPtr<HTMLHeadElement> head = HTMLHeadElement::create(*document());
    RefPtr<HTMLMetaElement> meta = HTMLMetaElement::create(*document());
    meta->setAttribute(nameAttr, "viewport");
    meta->setAttribute(contentAttr, "width=device-width");
    head->appendChild(meta.release());

    RefPtr<HTMLVideoElement> media = HTMLVideoElement::create(*document());
    media->setAttribute(controlsAttr, "");
    media->setAttribute(autoplayAttr, "");
    media->setAttribute(nameAttr, "media");

    RefPtr<HTMLSourceElement> source = HTMLSourceElement::create(*document());
    source->setSrc(document()->url());

    if (DocumentLoader* loader = document()->loader())
        source->setType(loader->responseMIMEType());

    media->appendChild(source.release());

    RefPtr<HTMLBodyElement> body = HTMLBodyElement::create(*document());
    body->appendChild(media.release());

    rootElement->appendChild(head.release());
    rootElement->appendChild(body.release());

    m_didBuildDocumentStructure = true;
}

void MediaDocumentParser::appendBytes(const char*, size_t)
{
    if (m_didBuildDocumentStructure)
        return;

    createDocumentStructure();
    finish();
}

MediaDocument::MediaDocument(const DocumentInit& initializer)
    : HTMLDocument(initializer, MediaDocumentClass)
{
    setCompatibilityMode(QuirksMode);
    lockCompatibilityMode();
}

PassRefPtr<DocumentParser> MediaDocument::createParser()
{
    return MediaDocumentParser::create(this);
}

void MediaDocument::defaultEventHandler(Event* event)
{
    Node* targetNode = event->target()->toNode();
    if (!targetNode)
        return;

    if (event->type() == EventTypeNames::keydown && event->isKeyboardEvent()) {
        HTMLVideoElement* video = Traversal<HTMLVideoElement>::firstWithin(*targetNode);
        if (!video)
            return;

        KeyboardEvent* keyboardEvent = toKeyboardEvent(event);
        if (keyboardEvent->keyIdentifier() == "U+0020" || keyboardEvent->keyCode() == VKEY_MEDIA_PLAY_PAUSE) {
            // space or media key (play/pause)
            if (video->paused()) {
                if (video->canPlay())
                    video->play();
            } else
                video->pause();
            event->setDefaultHandled();
        }
    }
}

}
