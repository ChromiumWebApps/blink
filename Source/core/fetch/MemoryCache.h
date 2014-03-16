/*
    Copyright (C) 1998 Lars Knoll (knoll@mpi-hd.mpg.de)
    Copyright (C) 2001 Dirk Mueller <mueller@kde.org>
    Copyright (C) 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.

    This class provides all functionality needed for loading images, style sheets and html
    pages from the web. It has a memory cache for these objects.
*/

#ifndef MemoryCache_h
#define MemoryCache_h

#include "core/fetch/Resource.h"
#include "core/fetch/ResourcePtr.h"
#include "public/platform/WebThread.h"
#include "wtf/HashMap.h"
#include "wtf/Noncopyable.h"
#include "wtf/Vector.h"
#include "wtf/text/StringHash.h"
#include "wtf/text/WTFString.h"

namespace WebCore  {

class CSSStyleSheetResource;
class Resource;
class ResourceFetcher;
class KURL;
class ExecutionContext;
class SecurityOrigin;
struct SecurityOriginHash;

// This cache holds subresources used by Web pages: images, scripts, stylesheets, etc.

// The cache keeps a flexible but bounded window of dead resources that grows/shrinks
// depending on the live resource load. Here's an example of cache growth over time,
// with a min dead resource capacity of 25% and a max dead resource capacity of 50%:

//        |-----|                              Dead: -
//        |----------|                         Live: +
//      --|----------|                         Cache boundary: | (objects outside this mark have been evicted)
//      --|----------++++++++++|
// -------|-----+++++++++++++++|
// -------|-----+++++++++++++++|+++++

// Enable this macro to periodically log information about the memory cache.
#undef MEMORY_CACHE_STATS

class MemoryCache FINAL : public blink::WebThread::TaskObserver {
    WTF_MAKE_NONCOPYABLE(MemoryCache); WTF_MAKE_FAST_ALLOCATED;
public:
    MemoryCache();
    virtual ~MemoryCache();

    class MemoryCacheEntry {
    public:
        static PassOwnPtr<MemoryCacheEntry> create(Resource* resource) { return adoptPtr(new MemoryCacheEntry(resource)); }

        ResourcePtr<Resource> m_resource;
        bool m_inLiveDecodedResourcesList;

        MemoryCacheEntry* m_previousInLiveResourcesList;
        MemoryCacheEntry* m_nextInLiveResourcesList;
        MemoryCacheEntry* m_previousInAllResourcesList;
        MemoryCacheEntry* m_nextInAllResourcesList;

    private:
        MemoryCacheEntry(Resource* resource)
            : m_resource(resource)
            , m_inLiveDecodedResourcesList(false)
            , m_previousInLiveResourcesList(0)
            , m_nextInLiveResourcesList(0)
            , m_previousInAllResourcesList(0)
            , m_nextInAllResourcesList(0)
        {
        }
    };

    struct LRUList {
        MemoryCacheEntry* m_head;
        MemoryCacheEntry* m_tail;
        LRUList() : m_head(0), m_tail(0) { }
    };

    struct TypeStatistic {
        int count;
        int size;
        int liveSize;
        int decodedSize;
        int encodedSize;
        int encodedSizeDuplicatedInDataURLs;
        int purgeableSize;
        int purgedSize;

        TypeStatistic()
            : count(0)
            , size(0)
            , liveSize(0)
            , decodedSize(0)
            , encodedSize(0)
            , encodedSizeDuplicatedInDataURLs(0)
            , purgeableSize(0)
            , purgedSize(0)
        {
        }

        void addResource(Resource*);
    };

    struct Statistics {
        TypeStatistic images;
        TypeStatistic cssStyleSheets;
        TypeStatistic scripts;
        TypeStatistic xslStyleSheets;
        TypeStatistic fonts;
        TypeStatistic other;
    };

    Resource* resourceForURL(const KURL&);

    void add(Resource*);
    void replace(Resource* newResource, Resource* oldResource);
    void remove(Resource* resource) { evict(resource); }

    static KURL removeFragmentIdentifierIfNeeded(const KURL& originalURL);

    // Sets the cache's memory capacities, in bytes. These will hold only approximately,
    // since the decoded cost of resources like scripts and stylesheets is not known.
    //  - minDeadBytes: The maximum number of bytes that dead resources should consume when the cache is under pressure.
    //  - maxDeadBytes: The maximum number of bytes that dead resources should consume when the cache is not under pressure.
    //  - totalBytes: The maximum number of bytes that the cache should consume overall.
    void setCapacities(size_t minDeadBytes, size_t maxDeadBytes, size_t totalBytes);
    void setDelayBeforeLiveDecodedPrune(double seconds) { m_delayBeforeLiveDecodedPrune = seconds; }
    void setMaxPruneDeferralDelay(double seconds) { m_maxPruneDeferralDelay = seconds; }

    void evictResources();

    void prune(Resource* justReleasedResource = 0);

    // Calls to put the cached resource into and out of LRU lists.
    void insertInLRUList(Resource*);
    void removeFromLRUList(Resource*);

    // Called to adjust the cache totals when a resource changes size.
    void adjustSize(bool live, ptrdiff_t delta);

    // Track decoded resources that are in the cache and referenced by a Web page.
    void insertInLiveDecodedResourcesList(Resource*);
    void removeFromLiveDecodedResourcesList(Resource*);
    bool isInLiveDecodedResourcesList(Resource*);

    void addToLiveResourcesSize(Resource*);
    void removeFromLiveResourcesSize(Resource*);

    static void removeURLFromCache(ExecutionContext*, const KURL&);

    Statistics getStatistics();

    size_t minDeadCapacity() const { return m_minDeadCapacity; }
    size_t maxDeadCapacity() const { return m_maxDeadCapacity; }
    size_t capacity() const { return m_capacity; }
    size_t liveSize() const { return m_liveSize; }
    size_t deadSize() const { return m_deadSize; }

    // TaskObserver implementation
    virtual void willProcessTask() OVERRIDE;
    virtual void didProcessTask() OVERRIDE;

private:
    LRUList* lruListFor(MemoryCacheEntry*);

#ifdef MEMORY_CACHE_STATS
    void dumpStats(Timer<MemoryCache>*);
    void dumpLRULists(bool includeLive) const;
#endif

    size_t liveCapacity() const;
    size_t deadCapacity() const;

    // pruneDeadResources() - Flush decoded and encoded data from resources not referenced by Web pages.
    // pruneLiveResources() - Flush decoded data from resources still referenced by Web pages.
    void pruneDeadResources(); // Automatically decide how much to prune.
    void pruneLiveResources();
    void pruneNow(double currentTime);

    bool evict(Resource*);

    static void removeURLFromCacheInternal(ExecutionContext*, const KURL&);

    bool m_inPruneResources;
    bool m_prunePending;
    double m_maxPruneDeferralDelay;
    double m_pruneTimeStamp;
    double m_pruneFrameTimeStamp;

    size_t m_capacity;
    size_t m_minDeadCapacity;
    size_t m_maxDeadCapacity;
    size_t m_maxDeferredPruneDeadCapacity;
    double m_delayBeforeLiveDecodedPrune;

    size_t m_liveSize; // The number of bytes currently consumed by "live" resources in the cache.
    size_t m_deadSize; // The number of bytes currently consumed by "dead" resources in the cache.

    // Size-adjusted and popularity-aware LRU list collection for cache objects. This collection can hold
    // more resources than the cached resource map, since it can also hold "stale" multiple versions of objects that are
    // waiting to die when the clients referencing them go away.
    Vector<LRUList, 32> m_allResources;

    // Lists just for live resources with decoded data. Access to this list is based off of painting the resource.
    // The lists are ordered by decode priority, with higher indices having higher priorities.
    LRUList m_liveDecodedResources[Resource::CacheLiveResourcePriorityHigh + 1];

    // A URL-based map of all resources that are in the cache (including the freshest version of objects that are currently being
    // referenced by a Web page).
    typedef HashMap<String, OwnPtr<MemoryCacheEntry> > ResourceMap;
    ResourceMap m_resources;

    friend class MemoryCacheTest;
#ifdef MEMORY_CACHE_STATS
    Timer<MemoryCache> m_statsTimer;
#endif
};

// Returns the global cache.
MemoryCache* memoryCache();

// Sets the global cache, used to swap in a test instance.
void setMemoryCacheForTesting(MemoryCache*);

}

#endif
