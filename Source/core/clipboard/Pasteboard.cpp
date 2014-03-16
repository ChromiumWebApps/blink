/*
 * Copyright (c) 2008, 2009, Google Inc. All rights reserved.
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
#include "core/clipboard/Pasteboard.h"

#include "core/clipboard/DataObject.h"
#include "platform/clipboard/ClipboardUtilities.h"
#include "platform/graphics/Image.h"
#include "platform/graphics/skia/NativeImageSkia.h"
#include "platform/weborigin/KURL.h"
#include "public/platform/Platform.h"
#include "public/platform/WebDragData.h"
#include "public/platform/WebString.h"
#include "public/platform/WebURL.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefPtr.h"

namespace WebCore {

Pasteboard* Pasteboard::generalPasteboard()
{
    static Pasteboard* pasteboard = new Pasteboard;
    return pasteboard;
}

Pasteboard::Pasteboard()
    : m_buffer(blink::WebClipboard::BufferStandard)
{
}

bool Pasteboard::isSelectionMode() const
{
    return m_buffer == blink::WebClipboard::BufferSelection;
}

void Pasteboard::setSelectionMode(bool selectionMode)
{
    m_buffer = selectionMode ? blink::WebClipboard::BufferSelection : blink::WebClipboard::BufferStandard;
}

void Pasteboard::writePlainText(const String& text, SmartReplaceOption)
{
    // FIXME: add support for smart replace
#if OS(WIN)
    String plainText(text);
    replaceNewlinesWithWindowsStyleNewlines(plainText);
    blink::Platform::current()->clipboard()->writePlainText(plainText);
#else
    blink::Platform::current()->clipboard()->writePlainText(text);
#endif
}

void Pasteboard::writeImage(Image* image, const KURL& url, const String& title)
{
    ASSERT(image);

    RefPtr<NativeImageSkia> bitmap = image->nativeImageForCurrentFrame();
    if (!bitmap)
        return;

    blink::WebImage webImage = bitmap->bitmap();
    blink::Platform::current()->clipboard()->writeImage(webImage, blink::WebURL(url), blink::WebString(title));
}

void Pasteboard::writeDataObject(PassRefPtrWillBeRawPtr<DataObject> dataObject)
{
    blink::Platform::current()->clipboard()->writeDataObject(blink::WebDragData(dataObject));
}

bool Pasteboard::canSmartReplace()
{
    return blink::Platform::current()->clipboard()->isFormatAvailable(blink::WebClipboard::FormatSmartPaste, m_buffer);
}

bool Pasteboard::isHTMLAvailable()
{
    return blink::Platform::current()->clipboard()->isFormatAvailable(blink::WebClipboard::FormatHTML, m_buffer);
}

String Pasteboard::plainText()
{
    return blink::Platform::current()->clipboard()->readPlainText(m_buffer);
}

String Pasteboard::readHTML(KURL& url, unsigned& fragmentStart, unsigned& fragmentEnd)
{
    blink::WebURL webURL;
    blink::WebString markup = blink::Platform::current()->clipboard()->readHTML(m_buffer, &webURL, &fragmentStart, &fragmentEnd);
    if (!markup.isEmpty()) {
        url = webURL;
        // fragmentStart and fragmentEnd are populated by WebClipboard::readHTML.
    } else {
        url = KURL();
        fragmentStart = 0;
        fragmentEnd = 0;
    }
    return markup;
}

void Pasteboard::writeHTML(const String& markup, const KURL& documentURL, const String& plainText, bool canSmartCopyOrDelete)
{
    String text = plainText;
#if OS(WIN)
    replaceNewlinesWithWindowsStyleNewlines(text);
#endif
    replaceNBSPWithSpace(text);

    blink::Platform::current()->clipboard()->writeHTML(markup, documentURL, text, canSmartCopyOrDelete);
}

} // namespace WebCore
