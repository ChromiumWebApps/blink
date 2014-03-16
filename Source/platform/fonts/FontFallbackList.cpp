/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "platform/fonts/FontFallbackList.h"

#include "FontFamilyNames.h"
#include "platform/fonts/FontCache.h"
#include "platform/fonts/FontDescription.h"
#include "platform/fonts/FontFamily.h"
#include "platform/fonts/SegmentedFontData.h"

namespace WebCore {

FontFallbackList::FontFallbackList()
    : m_pageZero(0)
    , m_cachedPrimarySimpleFontData(0)
    , m_fontSelector(nullptr)
    , m_fontSelectorVersion(0)
    , m_familyIndex(0)
    , m_generation(FontCache::fontCache()->generation())
    , m_pitch(UnknownPitch)
    , m_hasLoadingFallback(false)
{
}

void FontFallbackList::invalidate(PassRefPtr<FontSelector> fontSelector)
{
    releaseFontData();
    m_fontList.clear();
    m_pageZero = 0;
    m_pages.clear();
    m_cachedPrimarySimpleFontData = 0;
    m_familyIndex = 0;
    m_pitch = UnknownPitch;
    m_hasLoadingFallback = false;
    m_fontSelector = fontSelector;
    m_fontSelectorVersion = m_fontSelector ? m_fontSelector->version() : 0;
    m_generation = FontCache::fontCache()->generation();
    m_widthCache.clear();
}

void FontFallbackList::releaseFontData()
{
    unsigned numFonts = m_fontList.size();
    for (unsigned i = 0; i < numFonts; ++i) {
        if (!m_fontList[i]->isCustomFont()) {
            ASSERT(!m_fontList[i]->isSegmented());
            FontCache::fontCache()->releaseFontData(static_cast<const SimpleFontData*>(m_fontList[i].get()));
        }
    }
}

void FontFallbackList::determinePitch(const FontDescription& fontDescription) const
{
    const FontData* fontData = primaryFontData(fontDescription);
    if (!fontData->isSegmented())
        m_pitch = static_cast<const SimpleFontData*>(fontData)->pitch();
    else {
        const SegmentedFontData* segmentedFontData = static_cast<const SegmentedFontData*>(fontData);
        unsigned numRanges = segmentedFontData->numRanges();
        if (numRanges == 1 && segmentedFontData->rangeAt(0).isEntireRange())
            m_pitch = segmentedFontData->rangeAt(0).fontData()->pitch();
        else
            m_pitch = VariablePitch;
    }
}

bool FontFallbackList::loadingCustomFonts() const
{
    if (!m_hasLoadingFallback)
        return false;

    unsigned numFonts = m_fontList.size();
    for (unsigned i = 0; i < numFonts; ++i) {
        if (m_fontList[i]->isLoading())
            return true;
    }
    return false;
}

bool FontFallbackList::shouldSkipDrawing() const
{
    if (!m_hasLoadingFallback)
        return false;

    unsigned numFonts = m_fontList.size();
    for (unsigned i = 0; i < numFonts; ++i) {
        if (m_fontList[i]->shouldSkipDrawing())
            return true;
    }
    return false;
}

const FontData* FontFallbackList::primaryFontData(const FontDescription& fontDescription) const
{
    bool shouldLoadCustomFont = true;

    for (unsigned fontIndex = 0; ; ++fontIndex) {
        const FontData* fontData = fontDataAt(fontDescription, fontIndex);
        if (!fontData) {
            // All fonts are custom fonts and are loading. Return the first FontData.
            fontData = fontDataAt(fontDescription, 0);
            if (!fontData)
                fontData = FontCache::fontCache()->getLastResortFallbackFont(fontDescription).get();
            ASSERT(fontData);
            return fontData;
        }

        if (fontData->isSegmented() && !toSegmentedFontData(fontData)->containsCharacter(' '))
            continue;

        // When a custom font is loading, we should use the correct fallback font to layout the text.
        // Here skip the temporary font for the loading custom font which may not act as the correct fallback font.
        if (!fontData->isLoadingFallback())
            return fontData;

        // Begin to load the first custom font if needed.
        if (shouldLoadCustomFont) {
            shouldLoadCustomFont = false;
            const SimpleFontData* simpleFontData = fontData->fontDataForCharacter(' ');
            if (simpleFontData && simpleFontData->customFontData())
                simpleFontData->customFontData()->beginLoadIfNeeded();
        }
    }
}

PassRefPtr<FontData> FontFallbackList::getFontData(const FontDescription& fontDescription, int& familyIndex) const
{
    RefPtr<FontData> result;

    int startIndex = familyIndex;
    const FontFamily* startFamily = &fontDescription.family();
    for (int i = 0; startFamily && i < startIndex; i++)
        startFamily = startFamily->next();
    const FontFamily* currFamily = startFamily;
    while (currFamily && !result) {
        familyIndex++;
        if (currFamily->family().length()) {
            if (m_fontSelector)
                result = m_fontSelector->getFontData(fontDescription, currFamily->family());

            if (!result)
                result = FontCache::fontCache()->getFontData(fontDescription, currFamily->family());
        }
        currFamily = currFamily->next();
    }

    if (!currFamily)
        familyIndex = cAllFamiliesScanned;

    if (result || startIndex)
        return result.release();

    // If it's the primary font that we couldn't find, we try the following. In all other cases, we will
    // just use per-character system fallback.

    if (m_fontSelector) {
        // Try the user's preferred standard font.
        if (RefPtr<FontData> data = m_fontSelector->getFontData(fontDescription, FontFamilyNames::webkit_standard))
            return data.release();
    }

    // Still no result. Hand back our last resort fallback font.
    return FontCache::fontCache()->getLastResortFallbackFont(fontDescription);
}


const FontData* FontFallbackList::fontDataAt(const FontDescription& fontDescription, unsigned realizedFontIndex) const
{
    if (realizedFontIndex < m_fontList.size())
        return m_fontList[realizedFontIndex].get(); // This fallback font is already in our list.

    // Make sure we're not passing in some crazy value here.
    ASSERT(realizedFontIndex == m_fontList.size());

    if (m_familyIndex == cAllFamiliesScanned)
        return 0;

    // Ask the font cache for the font data.
    // We are obtaining this font for the first time.  We keep track of the families we've looked at before
    // in |m_familyIndex|, so that we never scan the same spot in the list twice.  getFontData will adjust our
    // |m_familyIndex| as it scans for the right font to make.
    ASSERT(FontCache::fontCache()->generation() == m_generation);
    RefPtr<FontData> result = getFontData(fontDescription, m_familyIndex);
    if (result) {
        m_fontList.append(result);
        if (result->isLoadingFallback())
            m_hasLoadingFallback = true;
    }
    return result.get();
}

}
