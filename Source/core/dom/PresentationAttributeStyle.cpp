/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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
#include "core/dom/PresentationAttributeStyle.h"

#include "core/css/StylePropertySet.h"
#include "core/dom/Attribute.h"
#include "core/dom/Element.h"
#include "core/html/HTMLInputElement.h"
#include "wtf/HashFunctions.h"
#include "wtf/HashMap.h"
#include "wtf/text/CString.h"

namespace WebCore {

using namespace HTMLNames;

struct PresentationAttributeCacheKey {
    PresentationAttributeCacheKey() : tagName(0) { }
    StringImpl* tagName;
    Vector<std::pair<StringImpl*, AtomicString>, 3> attributesAndValues;
};

static bool operator!=(const PresentationAttributeCacheKey& a, const PresentationAttributeCacheKey& b)
{
    if (a.tagName != b.tagName)
        return true;
    return a.attributesAndValues != b.attributesAndValues;
}

struct PresentationAttributeCacheEntry {
    WTF_MAKE_FAST_ALLOCATED;
public:
    PresentationAttributeCacheKey key;
    RefPtr<StylePropertySet> value;
};

typedef HashMap<unsigned, OwnPtr<PresentationAttributeCacheEntry>, AlreadyHashed> PresentationAttributeCache;
static PresentationAttributeCache& presentationAttributeCache()
{
    DEFINE_STATIC_LOCAL(PresentationAttributeCache, cache, ());
    return cache;
}

class PresentationAttributeCacheCleaner {
    WTF_MAKE_NONCOPYABLE(PresentationAttributeCacheCleaner); WTF_MAKE_FAST_ALLOCATED;
public:
    PresentationAttributeCacheCleaner()
        : m_hitCount(0)
        , m_cleanTimer(this, &PresentationAttributeCacheCleaner::cleanCache)
    {
    }

    void didHitPresentationAttributeCache()
    {
        if (presentationAttributeCache().size() < minimumPresentationAttributeCacheSizeForCleaning)
            return;

        m_hitCount++;

        if (!m_cleanTimer.isActive())
            m_cleanTimer.startOneShot(presentationAttributeCacheCleanTimeInSeconds, FROM_HERE);
    }

private:
    static const unsigned presentationAttributeCacheCleanTimeInSeconds = 60;
    static const unsigned minimumPresentationAttributeCacheSizeForCleaning = 100;
    static const unsigned minimumPresentationAttributeCacheHitCountPerMinute = (100 * presentationAttributeCacheCleanTimeInSeconds) / 60;

    void cleanCache(Timer<PresentationAttributeCacheCleaner>* timer)
    {
        ASSERT_UNUSED(timer, timer == &m_cleanTimer);
        unsigned hitCount = m_hitCount;
        m_hitCount = 0;
        if (hitCount > minimumPresentationAttributeCacheHitCountPerMinute)
            return;
        presentationAttributeCache().clear();
    }

    unsigned m_hitCount;
    Timer<PresentationAttributeCacheCleaner> m_cleanTimer;
};

static bool attributeNameSort(const pair<StringImpl*, AtomicString>& p1, const pair<StringImpl*, AtomicString>& p2)
{
    // Sort based on the attribute name pointers. It doesn't matter what the order is as long as it is always the same.
    return p1.first < p2.first;
}

static void makePresentationAttributeCacheKey(Element& element, PresentationAttributeCacheKey& result)
{
    // FIXME: Enable for SVG.
    if (!element.isHTMLElement())
        return;
    // Interpretation of the size attributes on <input> depends on the type attribute.
    if (isHTMLInputElement(element))
        return;
    unsigned size = element.attributeCount();
    for (unsigned i = 0; i < size; ++i) {
        const Attribute& attribute = element.attributeItem(i);
        if (!element.isPresentationAttribute(attribute.name()))
            continue;
        if (!attribute.namespaceURI().isNull())
            return;
        // FIXME: Background URL may depend on the base URL and can't be shared. Disallow caching.
        if (attribute.name() == backgroundAttr)
            return;
        result.attributesAndValues.append(std::make_pair(attribute.localName().impl(), attribute.value()));
    }
    if (result.attributesAndValues.isEmpty())
        return;
    // Attribute order doesn't matter. Sort for easy equality comparison.
    std::sort(result.attributesAndValues.begin(), result.attributesAndValues.end(), attributeNameSort);
    // The cache key is non-null when the tagName is set.
    result.tagName = element.localName().impl();
}

static unsigned computePresentationAttributeCacheHash(const PresentationAttributeCacheKey& key)
{
    if (!key.tagName)
        return 0;
    ASSERT(key.attributesAndValues.size());
    unsigned attributeHash = StringHasher::hashMemory(key.attributesAndValues.data(), key.attributesAndValues.size() * sizeof(key.attributesAndValues[0]));
    return WTF::pairIntHash(key.tagName->existingHash(), attributeHash);
}

PassRefPtr<StylePropertySet> computePresentationAttributeStyle(Element& element)
{
    DEFINE_STATIC_LOCAL(PresentationAttributeCacheCleaner, cacheCleaner, ());

    ASSERT(element.isStyledElement());

    PresentationAttributeCacheKey cacheKey;
    makePresentationAttributeCacheKey(element, cacheKey);

    unsigned cacheHash = computePresentationAttributeCacheHash(cacheKey);

    PresentationAttributeCache::ValueType* cacheValue;
    if (cacheHash) {
        cacheValue = presentationAttributeCache().add(cacheHash, nullptr).storedValue;
        if (cacheValue->value && cacheValue->value->key != cacheKey)
            cacheHash = 0;
    } else {
        cacheValue = 0;
    }

    RefPtr<StylePropertySet> style;
    if (cacheHash && cacheValue->value) {
        style = cacheValue->value->value;
        cacheCleaner.didHitPresentationAttributeCache();
    } else {
        style = MutableStylePropertySet::create(element.isSVGElement() ? SVGAttributeMode : HTMLAttributeMode);
        unsigned size = element.attributeCount();
        for (unsigned i = 0; i < size; ++i) {
            const Attribute& attribute = element.attributeItem(i);
            element.collectStyleForPresentationAttribute(attribute.name(), attribute.value(), toMutableStylePropertySet(style));
        }
    }

    if (!cacheHash || cacheValue->value)
        return style.release();

    OwnPtr<PresentationAttributeCacheEntry> newEntry = adoptPtr(new PresentationAttributeCacheEntry);
    newEntry->key = cacheKey;
    newEntry->value = style;

    static const unsigned presentationAttributeCacheMaximumSize = 4096;
    if (presentationAttributeCache().size() > presentationAttributeCacheMaximumSize) {
        // FIXME: Discarding the entire cache when it gets too big is probably bad
        // since it creates a perf "cliff". Perhaps we should use an LRU?
        presentationAttributeCache().clear();
        presentationAttributeCache().set(cacheHash, newEntry.release());
    } else {
        cacheValue->value = newEntry.release();
    }

    return style.release();
}

} // namespace WebCore
