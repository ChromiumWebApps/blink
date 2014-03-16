/*
 * Copyright (c) 2006, 2007, 2008, 2009 Google Inc. All rights reserved.
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

#include "SkFontMgr.h"
#include "SkTypeface.h"
#include "platform/NotImplemented.h"
#include "platform/fonts/AlternateFontFamily.h"
#include "platform/fonts/FontCache.h"
#include "platform/fonts/FontDescription.h"
#include "platform/fonts/SimpleFontData.h"
#include "wtf/Assertions.h"
#include "wtf/text/AtomicString.h"
#include "wtf/text/CString.h"
#include <unicode/locid.h>

namespace WebCore {

void FontCache::platformInit()
{
}

#if !OS(WIN) && !OS(ANDROID)
PassRefPtr<SimpleFontData> FontCache::platformFallbackForCharacter(const FontDescription& fontDescription, UChar32 c, const SimpleFontData*)
{
    icu::Locale locale = icu::Locale::getDefault();
    FontCache::SimpleFontFamily family;
    FontCache::getFontFamilyForCharacter(c, locale.getLanguage(), &family);
    if (family.name.isEmpty())
        return nullptr;

    AtomicString atomicFamily(family.name);
    // Changes weight and/or italic of given FontDescription depends on
    // the result of fontconfig so that keeping the correct font mapping
    // of the given character. See http://crbug.com/32109 for details.
    bool shouldSetSyntheticBold = false;
    bool shouldSetSyntheticItalic = false;
    FontDescription description(fontDescription);
    if (family.isBold && description.weight() < FontWeightBold)
        description.setWeight(FontWeightBold);
    if (!family.isBold && description.weight() >= FontWeightBold) {
        shouldSetSyntheticBold = true;
        description.setWeight(FontWeightNormal);
    }
    if (family.isItalic && description.style() == FontStyleNormal)
        description.setStyle(FontStyleItalic);
    if (!family.isItalic && description.style() == FontStyleItalic) {
        shouldSetSyntheticItalic = true;
        description.setStyle(FontStyleNormal);
    }

    FontPlatformData* substitutePlatformData = getFontPlatformData(description, atomicFamily);
    if (!substitutePlatformData)
        return nullptr;
    FontPlatformData platformData = FontPlatformData(*substitutePlatformData);
    platformData.setSyntheticBold(shouldSetSyntheticBold);
    platformData.setSyntheticItalic(shouldSetSyntheticItalic);
    return fontDataFromFontPlatformData(&platformData, DoNotRetain);
}

#endif // !OS(WIN) && !OS(ANDROID)

PassRefPtr<SimpleFontData> FontCache::getLastResortFallbackFont(const FontDescription& description, ShouldRetain shouldRetain)
{
    const AtomicString fallbackFontFamily = getFallbackFontFamily(description);
    const FontPlatformData* fontPlatformData = getFontPlatformData(description, fallbackFontFamily);

    // We should at least have Sans or Arial which is the last resort fallback of SkFontHost ports.
    if (!fontPlatformData) {
        DEFINE_STATIC_LOCAL(const AtomicString, sansStr, ("Sans", AtomicString::ConstructFromLiteral));
        fontPlatformData = getFontPlatformData(description, sansStr);
    }
    if (!fontPlatformData) {
        DEFINE_STATIC_LOCAL(const AtomicString, arialStr, ("Arial", AtomicString::ConstructFromLiteral));
        fontPlatformData = getFontPlatformData(description, arialStr);
    }

    ASSERT(fontPlatformData);
    return fontDataFromFontPlatformData(fontPlatformData, shouldRetain);
}

PassRefPtr<SkTypeface> FontCache::createTypeface(const FontDescription& fontDescription, const AtomicString& family, CString& name)
{
    // If we're creating a fallback font (e.g. "-webkit-monospace"), convert the name into
    // the fallback name (like "monospace") that fontconfig understands.
    if (!family.length() || family.startsWith("-webkit-")) {
        name = getFallbackFontFamily(fontDescription).string().utf8();
    } else {
        // convert the name to utf8
        name = family.utf8();
    }

    int style = SkTypeface::kNormal;
    if (fontDescription.weight() >= FontWeightBold)
        style |= SkTypeface::kBold;
    if (fontDescription.style())
        style |= SkTypeface::kItalic;

    // FIXME: Use SkFontStyle and matchFamilyStyle instead of legacyCreateTypeface.
#if OS(WIN)
    if (m_fontManager)
        return adoptRef(m_fontManager->legacyCreateTypeface(name.data(), style));
#endif

    return adoptRef(SkTypeface::CreateFromName(name.data(), static_cast<SkTypeface::Style>(style)));
}

#if !OS(WIN)
FontPlatformData* FontCache::createFontPlatformData(const FontDescription& fontDescription, const AtomicString& family, float fontSize)
{
    CString name;
    RefPtr<SkTypeface> tf(createTypeface(fontDescription, family, name));
    if (!tf)
        return 0;

    FontPlatformData* result = new FontPlatformData(tf,
        name.data(),
        fontSize,
        (fontDescription.weight() >= FontWeightBold && !tf->isBold()) || fontDescription.isSyntheticBold(),
        (fontDescription.style() && !tf->isItalic()) || fontDescription.isSyntheticItalic(),
        fontDescription.orientation(),
        fontDescription.useSubpixelPositioning());
    return result;
}
#endif // !OS(WIN)

} // namespace WebCore
