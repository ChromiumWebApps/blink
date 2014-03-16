/*
 * Copyright (C) 2006, 2008 Apple Computer, Inc.  All rights reserved.
 * Copyright (C) 2007-2008 Torch Mobile, Inc.
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

#ifndef FontCache_h
#define FontCache_h

#include <limits.h>
#include "platform/PlatformExport.h"
#include "wtf/Forward.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefPtr.h"
#include "wtf/Vector.h"
#include "wtf/text/CString.h"
#include "wtf/text/WTFString.h"
#include "wtf/unicode/Unicode.h"

#if OS(WIN)
#include "SkFontMgr.h"
#include <windows.h>
#include <objidl.h>
#include <mlang.h>
#endif

class SkTypeface;

namespace WebCore {

class FontCacheClient;
class FontPlatformData;
class FontData;
class FontDescription;
class OpenTypeVerticalData;
class SimpleFontData;

enum ShouldRetain { Retain, DoNotRetain };
enum PurgeSeverity { PurgeIfNeeded, ForcePurge };

class PLATFORM_EXPORT FontCache {
    friend class FontCachePurgePreventer;

    WTF_MAKE_NONCOPYABLE(FontCache); WTF_MAKE_FAST_ALLOCATED;
public:
    static FontCache* fontCache();

    void releaseFontData(const SimpleFontData*);

    // This method is implemented by the plaform and used by
    // FontFastPath to lookup the font for a given character.
    PassRefPtr<SimpleFontData> platformFallbackForCharacter(const FontDescription&, UChar32, const SimpleFontData* fontDataToSubstitute);

    // Also implemented by the platform.
    void platformInit();

    PassRefPtr<SimpleFontData> getFontData(const FontDescription&, const AtomicString&, bool checkingAlternateName = false, ShouldRetain = Retain);
    PassRefPtr<SimpleFontData> getLastResortFallbackFont(const FontDescription&, ShouldRetain = Retain);
    SimpleFontData* getNonRetainedLastResortFallbackFont(const FontDescription&);
    bool isPlatformFontAvailable(const FontDescription&, const AtomicString&);

    void addClient(FontCacheClient*);
    void removeClient(FontCacheClient*);

    unsigned short generation();
    void invalidate();

#if OS(WIN)
    PassRefPtr<SimpleFontData> fontDataFromDescriptionAndLogFont(const FontDescription&, ShouldRetain, const LOGFONT&, wchar_t* outFontFamilyName);
#endif

#if OS(WIN)
    bool useSubpixelPositioning() const { return m_useSubpixelPositioning; }
    SkFontMgr* fontManager() { return m_fontManager.get(); }
#endif

#if ENABLE(OPENTYPE_VERTICAL)
    typedef uint32_t FontFileKey;
    PassRefPtr<OpenTypeVerticalData> getVerticalData(const FontFileKey&, const FontPlatformData&);
#endif

#if OS(ANDROID)
    static AtomicString getGenericFamilyNameForScript(const AtomicString& familyName, UScriptCode);
#else
    struct SimpleFontFamily {
        String name;
        bool isBold;
        bool isItalic;
    };
    static void getFontFamilyForCharacter(UChar32, const char* preferredLocale, SimpleFontFamily*);
#endif

private:
    FontCache();
    ~FontCache();

    void purge(PurgeSeverity = PurgeIfNeeded);

    void disablePurging() { m_purgePreventCount++; }
    void enablePurging()
    {
        ASSERT(m_purgePreventCount);
        if (!--m_purgePreventCount)
            purge(PurgeIfNeeded);
    }

    // FIXME: This method should eventually be removed.
    FontPlatformData* getFontPlatformData(const FontDescription&, const AtomicString& family, bool checkingAlternateName = false);

    // These methods are implemented by each platform.
    FontPlatformData* createFontPlatformData(const FontDescription&, const AtomicString& family, float fontSize);

    // Implemented on skia platforms.
    PassRefPtr<SkTypeface> createTypeface(const FontDescription&, const AtomicString& family, CString& name);

    PassRefPtr<SimpleFontData> fontDataFromFontPlatformData(const FontPlatformData*, ShouldRetain = Retain);

    // Don't purge if this count is > 0;
    int m_purgePreventCount;

#if OS(WIN)
    OwnPtr<SkFontMgr> m_fontManager;
    bool m_useSubpixelPositioning;
#endif

#if OS(MACOSX) || OS(ANDROID)
    friend class ComplexTextController;
#endif
    friend class SimpleFontData; // For fontDataFromFontPlatformData
    friend class FontFallbackList;
};

class PLATFORM_EXPORT FontCachePurgePreventer {
public:
    FontCachePurgePreventer() { FontCache::fontCache()->disablePurging(); }
    ~FontCachePurgePreventer() { FontCache::fontCache()->enablePurging(); }
};

}

#endif
