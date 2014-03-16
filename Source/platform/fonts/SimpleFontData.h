/*
 * This file is part of the internal font implementation.
 *
 * Copyright (C) 2006, 2008, 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2007-2008 Torch Mobile, Inc.
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

#ifndef SimpleFontData_h
#define SimpleFontData_h

#include "platform/PlatformExport.h"
#include "platform/fonts/CustomFontData.h"
#include "platform/fonts/FontBaseline.h"
#include "platform/fonts/FontData.h"
#include "platform/fonts/FontMetrics.h"
#include "platform/fonts/FontPlatformData.h"
#include "platform/fonts/GlyphBuffer.h"
#include "platform/fonts/GlyphMetricsMap.h"
#include "platform/fonts/GlyphPageTreeNode.h"
#include "platform/fonts/TypesettingFeatures.h"
#include "platform/fonts/opentype/OpenTypeVerticalData.h"
#include "platform/geometry/FloatRect.h"
#include "wtf/OwnPtr.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/text/StringHash.h"

#if OS(MACOSX)
#include "wtf/RetainPtr.h"
#endif

namespace WebCore {

class CSSFontFaceSource;
class FontDescription;
class SharedBuffer;
struct WidthIterator;

enum FontDataVariant { AutoVariant, NormalVariant, SmallCapsVariant, EmphasisMarkVariant, BrokenIdeographVariant };
enum Pitch { UnknownPitch, FixedPitch, VariablePitch };

class PLATFORM_EXPORT SimpleFontData : public FontData {
public:
    // Used to create platform fonts.
    static PassRefPtr<SimpleFontData> create(const FontPlatformData& platformData, PassRefPtr<CustomFontData> customData = nullptr, bool isTextOrientationFallback = false)
    {
        return adoptRef(new SimpleFontData(platformData, customData, isTextOrientationFallback));
    }

    // Used to create SVG Fonts.
    static PassRefPtr<SimpleFontData> create(PassRefPtr<CustomFontData> customData, float fontSize, bool syntheticBold, bool syntheticItalic)
    {
        return adoptRef(new SimpleFontData(customData, fontSize, syntheticBold, syntheticItalic));
    }

    virtual ~SimpleFontData();

    static const SimpleFontData* systemFallback() { return reinterpret_cast<const SimpleFontData*>(-1); }

    const FontPlatformData& platformData() const { return m_platformData; }
#if ENABLE(OPENTYPE_VERTICAL)
    const OpenTypeVerticalData* verticalData() const { return m_verticalData.get(); }
#endif

    PassRefPtr<SimpleFontData> smallCapsFontData(const FontDescription&) const;
    PassRefPtr<SimpleFontData> emphasisMarkFontData(const FontDescription&) const;
    PassRefPtr<SimpleFontData> brokenIdeographFontData() const;

    PassRefPtr<SimpleFontData> variantFontData(const FontDescription& description, FontDataVariant variant) const
    {
        switch (variant) {
        case SmallCapsVariant:
            return smallCapsFontData(description);
        case EmphasisMarkVariant:
            return emphasisMarkFontData(description);
        case BrokenIdeographVariant:
            return brokenIdeographFontData();
        case AutoVariant:
        case NormalVariant:
            break;
        }
        ASSERT_NOT_REACHED();
        return const_cast<SimpleFontData*>(this);
    }

    PassRefPtr<SimpleFontData> verticalRightOrientationFontData() const;
    PassRefPtr<SimpleFontData> uprightOrientationFontData() const;

    bool hasVerticalGlyphs() const { return m_hasVerticalGlyphs; }
    bool isTextOrientationFallback() const { return m_isTextOrientationFallback; }

    FontMetrics& fontMetrics() { return m_fontMetrics; }
    const FontMetrics& fontMetrics() const { return m_fontMetrics; }
    float sizePerUnit() const { return platformData().size() / (fontMetrics().unitsPerEm() ? fontMetrics().unitsPerEm() : 1); }

    float maxCharWidth() const { return m_maxCharWidth; }
    void setMaxCharWidth(float maxCharWidth) { m_maxCharWidth = maxCharWidth; }

    float avgCharWidth() const { return m_avgCharWidth; }
    void setAvgCharWidth(float avgCharWidth) { m_avgCharWidth = avgCharWidth; }

    FloatRect boundsForGlyph(Glyph) const;
    float widthForGlyph(Glyph glyph) const;
    FloatRect platformBoundsForGlyph(Glyph) const;
    float platformWidthForGlyph(Glyph) const;

    float spaceWidth() const { return m_spaceWidth; }
    float adjustedSpaceWidth() const { return m_adjustedSpaceWidth; }
    void setSpaceWidth(float spaceWidth) { m_spaceWidth = spaceWidth; }

#if OS(MACOSX)
    float syntheticBoldOffset() const { return m_syntheticBoldOffset; }
#endif

    Glyph spaceGlyph() const { return m_spaceGlyph; }
    void setSpaceGlyph(Glyph spaceGlyph) { m_spaceGlyph = spaceGlyph; }
    Glyph zeroWidthSpaceGlyph() const { return m_zeroWidthSpaceGlyph; }
    void setZeroWidthSpaceGlyph(Glyph spaceGlyph) { m_zeroWidthSpaceGlyph = spaceGlyph; }
    bool isZeroWidthSpaceGlyph(Glyph glyph) const { return glyph == m_zeroWidthSpaceGlyph && glyph; }
    Glyph zeroGlyph() const { return m_zeroGlyph; }
    void setZeroGlyph(Glyph zeroGlyph) { m_zeroGlyph = zeroGlyph; }

    virtual const SimpleFontData* fontDataForCharacter(UChar32) const OVERRIDE;

    Glyph glyphForCharacter(UChar32) const;

    void determinePitch();
    Pitch pitch() const { return m_treatAsFixedPitch ? FixedPitch : VariablePitch; }

    bool isSVGFont() const { return m_customFontData && m_customFontData->isSVGFont(); }
    virtual bool isCustomFont() const OVERRIDE { return m_customFontData; }
    virtual bool isLoading() const OVERRIDE { return m_customFontData ? m_customFontData->isLoading() : false; }
    virtual bool isLoadingFallback() const OVERRIDE { return m_customFontData ? m_customFontData->isLoadingFallback() : false; }
    virtual bool isSegmented() const OVERRIDE;
    virtual bool shouldSkipDrawing() const OVERRIDE { return m_customFontData && m_customFontData->shouldSkipDrawing(); }

    const GlyphData& missingGlyphData() const { return m_missingGlyphData; }
    void setMissingGlyphData(const GlyphData& glyphData) { m_missingGlyphData = glyphData; }

#ifndef NDEBUG
    virtual String description() const OVERRIDE;
#endif

#if OS(MACOSX)
    const SimpleFontData* getCompositeFontReferenceFontData(NSFont *key) const;
    NSFont* getNSFont() const { return m_platformData.font(); }
#endif

#if OS(MACOSX)
    CFDictionaryRef getCFStringAttributes(TypesettingFeatures, FontOrientation) const;
#endif

#if OS(MACOSX) || USE(HARFBUZZ)
    bool canRenderCombiningCharacterSequence(const UChar*, size_t) const;
#endif

    bool applyTransforms(GlyphBufferGlyph*, GlyphBufferAdvance*, size_t, TypesettingFeatures) const
    {
        return false;
    }

    PassRefPtr<CustomFontData> customFontData() const { return m_customFontData; }

private:
    SimpleFontData(const FontPlatformData&, PassRefPtr<CustomFontData> customData, bool isTextOrientationFallback = false);

    SimpleFontData(PassRefPtr<CustomFontData> customData, float fontSize, bool syntheticBold, bool syntheticItalic);

    void platformInit();
    void platformGlyphInit();
    void platformCharWidthInit();
    void platformDestroy();

    void initCharWidths();

    PassRefPtr<SimpleFontData> createScaledFontData(const FontDescription&, float scaleFactor) const;
    PassRefPtr<SimpleFontData> platformCreateScaledFontData(const FontDescription&, float scaleFactor) const;

    FontMetrics m_fontMetrics;
    float m_maxCharWidth;
    float m_avgCharWidth;

    FontPlatformData m_platformData;

    mutable OwnPtr<GlyphMetricsMap<FloatRect> > m_glyphToBoundsMap;
    mutable GlyphMetricsMap<float> m_glyphToWidthMap;

    bool m_treatAsFixedPitch;

    bool m_isTextOrientationFallback;
    bool m_isBrokenIdeographFallback;
#if ENABLE(OPENTYPE_VERTICAL)
    RefPtr<OpenTypeVerticalData> m_verticalData;
#endif
    bool m_hasVerticalGlyphs;

    Glyph m_spaceGlyph;
    float m_spaceWidth;
    Glyph m_zeroGlyph;
    float m_adjustedSpaceWidth;

    Glyph m_zeroWidthSpaceGlyph;

    GlyphData m_missingGlyphData;

    struct DerivedFontData {
        static PassOwnPtr<DerivedFontData> create(bool forCustomFont);
        ~DerivedFontData();

        bool forCustomFont;
        RefPtr<SimpleFontData> smallCaps;
        RefPtr<SimpleFontData> emphasisMark;
        RefPtr<SimpleFontData> brokenIdeograph;
        RefPtr<SimpleFontData> verticalRightOrientation;
        RefPtr<SimpleFontData> uprightOrientation;
#if OS(MACOSX)
        mutable RetainPtr<CFMutableDictionaryRef> compositeFontReferences;
#endif

    private:
        DerivedFontData(bool custom)
            : forCustomFont(custom)
        {
        }
    };

    mutable OwnPtr<DerivedFontData> m_derivedFontData;

    RefPtr<CustomFontData> m_customFontData;

#if OS(MACOSX)
    float m_syntheticBoldOffset;

    mutable HashMap<unsigned, RetainPtr<CFDictionaryRef> > m_CFStringAttributes;
#endif

#if OS(MACOSX) || USE(HARFBUZZ)
    mutable OwnPtr<HashMap<String, bool> > m_combiningCharacterSequenceSupport;
#endif
};

ALWAYS_INLINE FloatRect SimpleFontData::boundsForGlyph(Glyph glyph) const
{
    if (isZeroWidthSpaceGlyph(glyph))
        return FloatRect();

    FloatRect bounds;
    if (m_glyphToBoundsMap) {
        bounds = m_glyphToBoundsMap->metricsForGlyph(glyph);
        if (bounds.width() != cGlyphSizeUnknown)
            return bounds;
    }

    bounds = platformBoundsForGlyph(glyph);
    if (!m_glyphToBoundsMap)
        m_glyphToBoundsMap = adoptPtr(new GlyphMetricsMap<FloatRect>);
    m_glyphToBoundsMap->setMetricsForGlyph(glyph, bounds);
    return bounds;
}

ALWAYS_INLINE float SimpleFontData::widthForGlyph(Glyph glyph) const
{
    if (isZeroWidthSpaceGlyph(glyph))
        return 0;

    float width = m_glyphToWidthMap.metricsForGlyph(glyph);
    if (width != cGlyphSizeUnknown)
        return width;

    if (isSVGFont())
        width = m_customFontData->widthForSVGGlyph(glyph, m_platformData.size());
#if ENABLE(OPENTYPE_VERTICAL)
    else if (m_verticalData)
#if OS(MACOSX)
        width = m_verticalData->advanceHeight(this, glyph) + m_syntheticBoldOffset;
#else
        width = m_verticalData->advanceHeight(this, glyph);
#endif
#endif
    else
        width = platformWidthForGlyph(glyph);

    m_glyphToWidthMap.setMetricsForGlyph(glyph, width);
    return width;
}

} // namespace WebCore
#endif // SimpleFontData_h
