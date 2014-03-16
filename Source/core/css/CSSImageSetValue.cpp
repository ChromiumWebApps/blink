/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "core/css/CSSImageSetValue.h"

#include "FetchInitiatorTypeNames.h"
#include "core/css/CSSImageValue.h"
#include "core/css/CSSPrimitiveValue.h"
#include "core/dom/Document.h"
#include "core/fetch/FetchRequest.h"
#include "core/fetch/ImageResource.h"
#include "core/fetch/ResourceFetcher.h"
#include "core/rendering/style/StyleFetchedImageSet.h"
#include "core/rendering/style/StylePendingImage.h"
#include "wtf/text/StringBuilder.h"

namespace WebCore {

CSSImageSetValue::CSSImageSetValue()
    : CSSValueList(ImageSetClass, CommaSeparator)
    , m_accessedBestFitImage(false)
    , m_scaleFactor(1)
{
}

CSSImageSetValue::~CSSImageSetValue()
{
    if (m_imageSet && m_imageSet->isImageResourceSet())
        toStyleFetchedImageSet(m_imageSet)->clearImageSetValue();
}

void CSSImageSetValue::fillImageSet()
{
    size_t length = this->length();
    size_t i = 0;
    while (i < length) {
        CSSValue* imageValue = item(i);
        String imageURL = toCSSImageValue(imageValue)->url();

        ++i;
        ASSERT_WITH_SECURITY_IMPLICATION(i < length);
        CSSValue* scaleFactorValue = item(i);
        float scaleFactor = toCSSPrimitiveValue(scaleFactorValue)->getFloatValue();

        ImageWithScale image;
        image.imageURL = imageURL;
        image.scaleFactor = scaleFactor;
        m_imagesInSet.append(image);
        ++i;
    }

    // Sort the images so that they are stored in order from lowest resolution to highest.
    std::sort(m_imagesInSet.begin(), m_imagesInSet.end(), CSSImageSetValue::compareByScaleFactor);
}

CSSImageSetValue::ImageWithScale CSSImageSetValue::bestImageForScaleFactor()
{
    ImageWithScale image;
    size_t numberOfImages = m_imagesInSet.size();
    for (size_t i = 0; i < numberOfImages; ++i) {
        image = m_imagesInSet.at(i);
        if (image.scaleFactor >= m_scaleFactor)
            return image;
    }
    return image;
}

StyleFetchedImageSet* CSSImageSetValue::cachedImageSet(ResourceFetcher* loader, float deviceScaleFactor, const ResourceLoaderOptions& options)
{
    ASSERT(loader);

    m_scaleFactor = deviceScaleFactor;

    if (!m_imagesInSet.size())
        fillImageSet();

    if (!m_accessedBestFitImage) {
        // FIXME: In the future, we want to take much more than deviceScaleFactor into acount here.
        // All forms of scale should be included: Page::pageScaleFactor(), LocalFrame::pageZoomFactor(),
        // and any CSS transforms. https://bugs.webkit.org/show_bug.cgi?id=81698
        ImageWithScale image = bestImageForScaleFactor();
        if (Document* document = loader->document()) {
            FetchRequest request(ResourceRequest(document->completeURL(image.imageURL)), FetchInitiatorTypeNames::css, options);

            if (options.corsEnabled == IsCORSEnabled)
                request.setCrossOriginAccessControl(loader->document()->securityOrigin(), options.allowCredentials);

            if (ResourcePtr<ImageResource> cachedImage = loader->fetchImage(request)) {
                m_imageSet = StyleFetchedImageSet::create(cachedImage.get(), image.scaleFactor, this);
                m_accessedBestFitImage = true;
            }
        }
    }

    return (m_imageSet && m_imageSet->isImageResourceSet()) ? toStyleFetchedImageSet(m_imageSet) : 0;
}

StyleFetchedImageSet* CSSImageSetValue::cachedImageSet(ResourceFetcher* fetcher, float deviceScaleFactor)
{
    return cachedImageSet(fetcher, deviceScaleFactor, ResourceFetcher::defaultResourceOptions());
}

StyleImage* CSSImageSetValue::cachedOrPendingImageSet(float deviceScaleFactor)
{
    if (!m_imageSet) {
        m_imageSet = StylePendingImage::create(this);
    } else if (!m_imageSet->isPendingImage()) {
        // If the deviceScaleFactor has changed, we may not have the best image loaded, so we have to re-assess.
        if (deviceScaleFactor != m_scaleFactor) {
            m_accessedBestFitImage = false;
            m_imageSet = StylePendingImage::create(this);
        }
    }

    return m_imageSet.get();
}

String CSSImageSetValue::customCSSText() const
{
    StringBuilder result;
    result.append("-webkit-image-set(");

    size_t length = this->length();
    size_t i = 0;
    while (i < length) {
        if (i > 0)
            result.append(", ");

        const CSSValue* imageValue = item(i);
        result.append(imageValue->cssText());
        result.append(' ');

        ++i;
        ASSERT_WITH_SECURITY_IMPLICATION(i < length);
        const CSSValue* scaleFactorValue = item(i);
        result.append(scaleFactorValue->cssText());
        // FIXME: Eventually the scale factor should contain it's own unit http://wkb.ug/100120.
        // For now 'x' is hard-coded in the parser, so we hard-code it here too.
        result.append('x');

        ++i;
    }

    result.append(")");
    return result.toString();
}

bool CSSImageSetValue::hasFailedOrCanceledSubresources() const
{
    if (!m_imageSet || !m_imageSet->isImageResourceSet())
        return false;
    if (Resource* cachedResource = toStyleFetchedImageSet(m_imageSet)->cachedImage())
        return cachedResource->loadFailedOrCanceled();
    return true;
}

CSSImageSetValue::CSSImageSetValue(const CSSImageSetValue& cloneFrom)
    : CSSValueList(cloneFrom)
    , m_accessedBestFitImage(false)
    , m_scaleFactor(1)
{
    // Non-CSSValueList data is not accessible through CSS OM, no need to clone.
}

PassRefPtrWillBeRawPtr<CSSImageSetValue> CSSImageSetValue::cloneForCSSOM() const
{
    return adoptRefWillBeRefCountedGarbageCollected(new CSSImageSetValue(*this));
}

} // namespace WebCore
