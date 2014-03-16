/*
 * Copyright (C) 2006, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2007 Nicholas Shanks <webkit@nickshanks.com>
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
#include "platform/fonts/FontCache.h"

#include "FontFamilyNames.h"

#include "RuntimeEnabledFeatures.h"
#include "platform/fonts/AlternateFontFamily.h"
#include "platform/fonts/FontCacheClient.h"
#include "platform/fonts/FontCacheKey.h"
#include "platform/fonts/FontDataCache.h"
#include "platform/fonts/FontDescription.h"
#include "platform/fonts/FontFallbackList.h"
#include "platform/fonts/FontPlatformData.h"
#include "platform/fonts/FontSmoothingMode.h"
#include "platform/fonts/TextRenderingMode.h"
#include "platform/fonts/opentype/OpenTypeVerticalData.h"
#include "wtf/HashMap.h"
#include "wtf/ListHashSet.h"
#include "wtf/StdLibExtras.h"
#include "wtf/text/AtomicStringHash.h"
#include "wtf/text/StringHash.h"

using namespace WTF;

namespace WebCore {

#if !OS(WIN)
FontCache::FontCache()
    : m_purgePreventCount(0)
{
}
#endif // !OS(WIN)

typedef HashMap<FontCacheKey, OwnPtr<FontPlatformData>, FontCacheKeyHash, FontCacheKeyTraits> FontPlatformDataCache;

static FontPlatformDataCache* gFontPlatformDataCache = 0;

FontCache* FontCache::fontCache()
{
    DEFINE_STATIC_LOCAL(FontCache, globalFontCache, ());
    return &globalFontCache;
}

FontPlatformData* FontCache::getFontPlatformData(const FontDescription& fontDescription,
    const AtomicString& passedFamilyName, bool checkingAlternateName)
{
#if OS(WIN) && ENABLE(OPENTYPE_VERTICAL)
    // Leading "@" in the font name enables Windows vertical flow flag for the font.
    // Because we do vertical flow by ourselves, we don't want to use the Windows feature.
    // IE disregards "@" regardless of the orientatoin, so we follow the behavior.
    const AtomicString& familyName = (passedFamilyName.isEmpty() || passedFamilyName[0] != '@') ?
        passedFamilyName : AtomicString(passedFamilyName.impl()->substring(1));
#else
    const AtomicString& familyName = passedFamilyName;
#endif

    if (!gFontPlatformDataCache) {
        gFontPlatformDataCache = new FontPlatformDataCache;
        platformInit();
    }

    FontCacheKey key = fontDescription.cacheKey(familyName);
    FontPlatformData* result = 0;
    bool foundResult;
    FontPlatformDataCache::iterator it = gFontPlatformDataCache->find(key);
    if (it == gFontPlatformDataCache->end()) {
        result = createFontPlatformData(fontDescription, familyName, fontDescription.effectiveFontSize());
        gFontPlatformDataCache->set(key, adoptPtr(result));
        foundResult = result;
    } else {
        result = it->value.get();
        foundResult = true;
    }

    if (!foundResult && !checkingAlternateName) {
        // We were unable to find a font. We have a small set of fonts that we alias to other names,
        // e.g., Arial/Helvetica, Courier/Courier New, etc. Try looking up the font under the aliased name.
        const AtomicString& alternateName = alternateFamilyName(familyName);
        if (!alternateName.isEmpty())
            result = getFontPlatformData(fontDescription, alternateName, true);
        if (result)
            gFontPlatformDataCache->set(key, adoptPtr(new FontPlatformData(*result))); // Cache the result under the old name.
    }

    return result;
}

#if ENABLE(OPENTYPE_VERTICAL)
typedef HashMap<FontCache::FontFileKey, RefPtr<OpenTypeVerticalData>, WTF::IntHash<FontCache::FontFileKey>, WTF::UnsignedWithZeroKeyHashTraits<FontCache::FontFileKey> > FontVerticalDataCache;

FontVerticalDataCache& fontVerticalDataCacheInstance()
{
    DEFINE_STATIC_LOCAL(FontVerticalDataCache, fontVerticalDataCache, ());
    return fontVerticalDataCache;
}

PassRefPtr<OpenTypeVerticalData> FontCache::getVerticalData(const FontFileKey& key, const FontPlatformData& platformData)
{
    FontVerticalDataCache& fontVerticalDataCache = fontVerticalDataCacheInstance();
    FontVerticalDataCache::iterator result = fontVerticalDataCache.find(key);
    if (result != fontVerticalDataCache.end())
        return result.get()->value;

    RefPtr<OpenTypeVerticalData> verticalData = OpenTypeVerticalData::create(platformData);
    if (!verticalData->isOpenType())
        verticalData.clear();
    fontVerticalDataCache.set(key, verticalData);
    return verticalData;
}
#endif

static FontDataCache* gFontDataCache = 0;

PassRefPtr<SimpleFontData> FontCache::getFontData(const FontDescription& fontDescription, const AtomicString& family, bool checkingAlternateName, ShouldRetain shouldRetain)
{
    if (FontPlatformData* platformData = getFontPlatformData(fontDescription, adjustFamilyNameToAvoidUnsupportedFonts(family), checkingAlternateName))
        return fontDataFromFontPlatformData(platformData, shouldRetain);

    return nullptr;
}

PassRefPtr<SimpleFontData> FontCache::fontDataFromFontPlatformData(const FontPlatformData* platformData, ShouldRetain shouldRetain)
{
    if (!gFontDataCache)
        gFontDataCache = new FontDataCache;

#if !ASSERT_DISABLED
    if (shouldRetain == DoNotRetain)
        ASSERT(m_purgePreventCount);
#endif

    return gFontDataCache->get(platformData, shouldRetain);
}

bool FontCache::isPlatformFontAvailable(const FontDescription& fontDescription, const AtomicString& family)
{
    bool checkingAlternateName = true;
    return getFontPlatformData(fontDescription, family, checkingAlternateName);
}

SimpleFontData* FontCache::getNonRetainedLastResortFallbackFont(const FontDescription& fontDescription)
{
    return getLastResortFallbackFont(fontDescription, DoNotRetain).leakRef();
}

void FontCache::releaseFontData(const SimpleFontData* fontData)
{
    ASSERT(gFontDataCache);

    gFontDataCache->release(fontData);
}

static inline void purgePlatformFontDataCache()
{
    if (!gFontPlatformDataCache)
        return;

    Vector<FontCacheKey> keysToRemove;
    keysToRemove.reserveInitialCapacity(gFontPlatformDataCache->size());
    FontPlatformDataCache::iterator platformDataEnd = gFontPlatformDataCache->end();
    for (FontPlatformDataCache::iterator platformData = gFontPlatformDataCache->begin(); platformData != platformDataEnd; ++platformData) {
        if (platformData->value && !gFontDataCache->contains(platformData->value.get()))
            keysToRemove.append(platformData->key);
    }

    size_t keysToRemoveCount = keysToRemove.size();
    for (size_t i = 0; i < keysToRemoveCount; ++i)
        gFontPlatformDataCache->remove(keysToRemove[i]);
}

static inline void purgeFontVerticalDataCache()
{
#if ENABLE(OPENTYPE_VERTICAL)
    FontVerticalDataCache& fontVerticalDataCache = fontVerticalDataCacheInstance();
    if (!fontVerticalDataCache.isEmpty()) {
        // Mark & sweep unused verticalData
        FontVerticalDataCache::iterator verticalDataEnd = fontVerticalDataCache.end();
        for (FontVerticalDataCache::iterator verticalData = fontVerticalDataCache.begin(); verticalData != verticalDataEnd; ++verticalData) {
            if (verticalData->value)
                verticalData->value->setInFontCache(false);
        }

        gFontDataCache->markAllVerticalData();

        Vector<FontCache::FontFileKey> keysToRemove;
        keysToRemove.reserveInitialCapacity(fontVerticalDataCache.size());
        for (FontVerticalDataCache::iterator verticalData = fontVerticalDataCache.begin(); verticalData != verticalDataEnd; ++verticalData) {
            if (!verticalData->value || !verticalData->value->inFontCache())
                keysToRemove.append(verticalData->key);
        }
        for (size_t i = 0, count = keysToRemove.size(); i < count; ++i)
            fontVerticalDataCache.take(keysToRemove[i]);
    }
#endif
}

void FontCache::purge(PurgeSeverity PurgeSeverity)
{
    // We should never be forcing the purge while the FontCachePurgePreventer is in scope.
    ASSERT(!m_purgePreventCount || PurgeSeverity == PurgeIfNeeded);
    if (m_purgePreventCount)
        return;

    if (!gFontDataCache || !gFontDataCache->purge(PurgeSeverity))
        return;

    purgePlatformFontDataCache();
    purgeFontVerticalDataCache();
}

static HashSet<FontCacheClient*>* gClients;

void FontCache::addClient(FontCacheClient* client)
{
    if (!gClients)
        gClients = new HashSet<FontCacheClient*>;

    ASSERT(!gClients->contains(client));
    gClients->add(client);
}

void FontCache::removeClient(FontCacheClient* client)
{
    ASSERT(gClients);
    ASSERT(gClients->contains(client));

    gClients->remove(client);
}

static unsigned short gGeneration = 0;

unsigned short FontCache::generation()
{
    return gGeneration;
}

void FontCache::invalidate()
{
    if (!gClients) {
        ASSERT(!gFontPlatformDataCache);
        return;
    }

    if (gFontPlatformDataCache) {
        delete gFontPlatformDataCache;
        gFontPlatformDataCache = new FontPlatformDataCache;
    }

    gGeneration++;

    Vector<RefPtr<FontCacheClient> > clients;
    size_t numClients = gClients->size();
    clients.reserveInitialCapacity(numClients);
    HashSet<FontCacheClient*>::iterator end = gClients->end();
    for (HashSet<FontCacheClient*>::iterator it = gClients->begin(); it != end; ++it)
        clients.append(*it);

    ASSERT(numClients == clients.size());
    for (size_t i = 0; i < numClients; ++i)
        clients[i]->fontCacheInvalidated();

    purge(ForcePurge);
}

} // namespace WebCore
