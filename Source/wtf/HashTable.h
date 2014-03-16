/*
 * Copyright (C) 2005, 2006, 2007, 2008, 2011, 2012 Apple Inc. All rights reserved.
 * Copyright (C) 2008 David Levin <levin@chromium.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef WTF_HashTable_h
#define WTF_HashTable_h

#include "wtf/Alignment.h"
#include "wtf/Assertions.h"
#include "wtf/DefaultAllocator.h"
#include "wtf/HashTraits.h"
#include "wtf/WTF.h"

#define DUMP_HASHTABLE_STATS 0
#define DUMP_HASHTABLE_STATS_PER_TABLE 0

#if DUMP_HASHTABLE_STATS_PER_TABLE
#include "wtf/DataLog.h"
#endif

namespace WTF {

#if DUMP_HASHTABLE_STATS

    struct HashTableStats {
        // The following variables are all atomically incremented when modified.
        static int numAccesses;
        static int numRehashes;
        static int numRemoves;
        static int numReinserts;

        // The following variables are only modified in the recordCollisionAtCount method within a mutex.
        static int maxCollisions;
        static int numCollisions;
        static int collisionGraph[4096];

        static void recordCollisionAtCount(int count);
        static void dumpStats();
    };

#endif

    template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename Allocator>
    class HashTable;
    template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename Allocator>
    class HashTableIterator;
    template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename Allocator>
    class HashTableConstIterator;
    template<bool x, typename T, typename U, typename V, typename W, typename X, typename Y, typename Z>
    struct WeakProcessingHashTableHelper;

    typedef enum { HashItemKnownGood } HashItemKnownGoodTag;

    template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename Allocator>
    class HashTableConstIterator {
    private:
        typedef HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, Allocator> HashTableType;
        typedef HashTableIterator<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, Allocator> iterator;
        typedef HashTableConstIterator<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, Allocator> const_iterator;
        typedef Value ValueType;
        typedef typename Traits::IteratorConstGetType GetType;
        typedef const ValueType* PointerType;

        friend class HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, Allocator>;
        friend class HashTableIterator<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, Allocator>;

        void skipEmptyBuckets()
        {
            while (m_position != m_endPosition && HashTableType::isEmptyOrDeletedBucket(*m_position))
                ++m_position;
        }

        HashTableConstIterator(PointerType position, PointerType endPosition)
            : m_position(position), m_endPosition(endPosition)
        {
            skipEmptyBuckets();
        }

        HashTableConstIterator(PointerType position, PointerType endPosition, HashItemKnownGoodTag)
            : m_position(position), m_endPosition(endPosition)
        {
        }

    public:
        HashTableConstIterator()
        {
        }

        GetType get() const
        {
            return m_position;
        }
        typename Traits::IteratorConstReferenceType operator*() const { return Traits::getToReferenceConstConversion(get()); }
        GetType operator->() const { return get(); }

        const_iterator& operator++()
        {
            ASSERT(m_position != m_endPosition);
            ++m_position;
            skipEmptyBuckets();
            return *this;
        }

        // postfix ++ intentionally omitted

        // Comparison.
        bool operator==(const const_iterator& other) const
        {
            return m_position == other.m_position;
        }
        bool operator!=(const const_iterator& other) const
        {
            return m_position != other.m_position;
        }
        bool operator==(const iterator& other) const
        {
            return *this == static_cast<const_iterator>(other);
        }
        bool operator!=(const iterator& other) const
        {
            return *this != static_cast<const_iterator>(other);
        }

    private:
        PointerType m_position;
        PointerType m_endPosition;
    };

    template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename Allocator>
    class HashTableIterator {
    private:
        typedef HashTableIterator<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, Allocator> iterator;
        typedef HashTableConstIterator<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, Allocator> const_iterator;
        typedef Value ValueType;
        typedef typename Traits::IteratorGetType GetType;
        typedef ValueType* PointerType;

        friend class HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, Allocator>;

        HashTableIterator(PointerType pos, PointerType end) : m_iterator(pos, end) { }
        HashTableIterator(PointerType pos, PointerType end, HashItemKnownGoodTag tag) : m_iterator(pos, end, tag) { }

    public:
        HashTableIterator() { }

        // default copy, assignment and destructor are OK

        GetType get() const { return const_cast<GetType>(m_iterator.get()); }
        typename Traits::IteratorReferenceType operator*() const { return Traits::getToReferenceConversion(get()); }
        GetType operator->() const { return get(); }

        iterator& operator++() { ++m_iterator; return *this; }

        // postfix ++ intentionally omitted

        // Comparison.
        bool operator==(const iterator& other) const { return m_iterator == other.m_iterator; }
        bool operator!=(const iterator& other) const { return m_iterator != other.m_iterator; }
        bool operator==(const const_iterator& other) const { return m_iterator == other; }
        bool operator!=(const const_iterator& other) const { return m_iterator != other; }

        operator const_iterator() const { return m_iterator; }

    private:
        const_iterator m_iterator;
    };

    using std::swap;

    // Work around MSVC's standard library, whose swap for pairs does not swap by component.
    template<typename T> inline void hashTableSwap(T& a, T& b)
    {
        swap(a, b);
    }

    template<typename T, typename U> inline void hashTableSwap(KeyValuePair<T, U>& a, KeyValuePair<T, U>& b)
    {
        swap(a.key, b.key);
        swap(a.value, b.value);
    }

    template<typename T, bool useSwap> struct Mover;
    template<typename T> struct Mover<T, true> { static void move(T& from, T& to) { hashTableSwap(from, to); } };
    template<typename T> struct Mover<T, false> { static void move(T& from, T& to) { to = from; } };

    template<typename HashFunctions> class IdentityHashTranslator {
    public:
        template<typename T> static unsigned hash(const T& key) { return HashFunctions::hash(key); }
        template<typename T, typename U> static bool equal(const T& a, const U& b) { return HashFunctions::equal(a, b); }
        template<typename T, typename U, typename V> static void translate(T& location, const U&, const V& value) { location = value; }
    };

    template<typename ValueType> struct HashTableAddResult {
        HashTableAddResult(ValueType* storedValue, bool isNewEntry) : storedValue(storedValue), isNewEntry(isNewEntry) { }
        ValueType* storedValue;
        bool isNewEntry;
    };

    template<typename Value, typename Extractor, typename KeyTraits>
    struct HashTableHelper {
        static bool isEmptyBucket(const Value& value) { return isHashTraitsEmptyValue<KeyTraits>(Extractor::extract(value)); }
        static bool isDeletedBucket(const Value& value) { return KeyTraits::isDeletedValue(Extractor::extract(value)); }
        static bool isEmptyOrDeletedBucket(const Value& value) { return isEmptyBucket(value) || isDeletedBucket(value); }
    };

    // Don't declare a destructor for HeapAllocated hash tables.
    template<typename Derived, bool isGarbageCollected>
    class HashTableDestructorBase;

    template<typename Derived>
    class HashTableDestructorBase<Derived, true> { };

    template<typename Derived>
    class HashTableDestructorBase<Derived, false> {
    public:
        ~HashTableDestructorBase() { static_cast<Derived*>(this)->finalize(); }
    };

    template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename Allocator>
    class HashTable : public HashTableDestructorBase<HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, Allocator>, Allocator::isGarbageCollected> {
    public:
        typedef HashTableIterator<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, Allocator> iterator;
        typedef HashTableConstIterator<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, Allocator> const_iterator;
        typedef Traits ValueTraits;
        typedef Key KeyType;
        typedef typename KeyTraits::PeekInType KeyPeekInType;
        typedef typename KeyTraits::PassInType KeyPassInType;
        typedef Value ValueType;
        typedef typename Traits::PeekInType ValuePeekInType;
        typedef IdentityHashTranslator<HashFunctions> IdentityTranslatorType;
        typedef HashTableAddResult<ValueType> AddResult;

#if DUMP_HASHTABLE_STATS_PER_TABLE
        struct Stats {
            Stats()
                : numAccesses(0)
                , numRehashes(0)
                , numRemoves(0)
                , numReinserts(0)
                , maxCollisions(0)
                , numCollisions(0)
                , collisionGraph()
            {
            }

            int numAccesses;
            int numRehashes;
            int numRemoves;
            int numReinserts;

            int maxCollisions;
            int numCollisions;
            int collisionGraph[4096];

            void recordCollisionAtCount(int count)
            {
                if (count > maxCollisions)
                    maxCollisions = count;
                numCollisions++;
                collisionGraph[count]++;
            }

            void dumpStats()
            {
                dataLogF("\nWTF::HashTable::Stats dump\n\n");
                dataLogF("%d accesses\n", numAccesses);
                dataLogF("%d total collisions, average %.2f probes per access\n", numCollisions, 1.0 * (numAccesses + numCollisions) / numAccesses);
                dataLogF("longest collision chain: %d\n", maxCollisions);
                for (int i = 1; i <= maxCollisions; i++) {
                    dataLogF("  %d lookups with exactly %d collisions (%.2f%% , %.2f%% with this many or more)\n", collisionGraph[i], i, 100.0 * (collisionGraph[i] - collisionGraph[i+1]) / numAccesses, 100.0 * collisionGraph[i] / numAccesses);
                }
                dataLogF("%d rehashes\n", numRehashes);
                dataLogF("%d reinserts\n", numReinserts);
            }
        };
#endif

        HashTable();
        void finalize()
        {
            ASSERT(!Allocator::isGarbageCollected);
            if (LIKELY(!m_table))
                return;
            deallocateTable(m_table, m_tableSize);
            m_table = 0;
        }

        HashTable(const HashTable&);
        void swap(HashTable&);
        HashTable& operator=(const HashTable&);

        // When the hash table is empty, just return the same iterator for end as for begin.
        // This is more efficient because we don't have to skip all the empty and deleted
        // buckets, and iterating an empty table is a common case that's worth optimizing.
        iterator begin() { return isEmpty() ? end() : makeIterator(m_table); }
        iterator end() { return makeKnownGoodIterator(m_table + m_tableSize); }
        const_iterator begin() const { return isEmpty() ? end() : makeConstIterator(m_table); }
        const_iterator end() const { return makeKnownGoodConstIterator(m_table + m_tableSize); }

        unsigned size() const { return m_keyCount; }
        unsigned capacity() const { return m_tableSize; }
        bool isEmpty() const { return !m_keyCount; }

        AddResult add(ValuePeekInType value)
        {
            return add<IdentityTranslatorType>(Extractor::extract(value), value);
        }

        // A special version of add() that finds the object by hashing and comparing
        // with some other type, to avoid the cost of type conversion if the object is already
        // in the table.
        template<typename HashTranslator, typename T, typename Extra> AddResult add(const T& key, const Extra&);
        template<typename HashTranslator, typename T, typename Extra> AddResult addPassingHashCode(const T& key, const Extra&);

        iterator find(KeyPeekInType key) { return find<IdentityTranslatorType>(key); }
        const_iterator find(KeyPeekInType key) const { return find<IdentityTranslatorType>(key); }
        bool contains(KeyPeekInType key) const { return contains<IdentityTranslatorType>(key); }

        template<typename HashTranslator, typename T> iterator find(const T&);
        template<typename HashTranslator, typename T> const_iterator find(const T&) const;
        template<typename HashTranslator, typename T> bool contains(const T&) const;

        void remove(KeyPeekInType);
        void remove(iterator);
        void remove(const_iterator);
        void clear();

        static bool isEmptyBucket(const ValueType& value) { return isHashTraitsEmptyValue<KeyTraits>(Extractor::extract(value)); }
        static bool isDeletedBucket(const ValueType& value) { return KeyTraits::isDeletedValue(Extractor::extract(value)); }
        static bool isEmptyOrDeletedBucket(const ValueType& value) { return HashTableHelper<ValueType, Extractor, KeyTraits>:: isEmptyOrDeletedBucket(value); }

        ValueType* lookup(KeyPeekInType key) { return lookup<IdentityTranslatorType>(key); }
        template<typename HashTranslator, typename T> ValueType* lookup(const T&);

        void trace(typename Allocator::Visitor*);

    private:
        static ValueType* allocateTable(unsigned size);
        static void deallocateTable(ValueType* table, unsigned size);

        typedef std::pair<ValueType*, bool> LookupType;
        typedef std::pair<LookupType, unsigned> FullLookupType;

        LookupType lookupForWriting(const Key& key) { return lookupForWriting<IdentityTranslatorType>(key); };
        template<typename HashTranslator, typename T> FullLookupType fullLookupForWriting(const T&);
        template<typename HashTranslator, typename T> LookupType lookupForWriting(const T&);

        void remove(ValueType*);

        bool shouldExpand() const { return (m_keyCount + m_deletedCount) * m_maxLoad >= m_tableSize; }
        bool mustRehashInPlace() const { return m_keyCount * m_minLoad < m_tableSize * 2; }
        bool shouldShrink() const { return m_keyCount * m_minLoad < m_tableSize && m_tableSize > KeyTraits::minimumTableSize; }
        void expand();
        void shrink() { rehash(m_tableSize / 2); }

        void rehash(unsigned newTableSize);
        void reinsert(ValueType&);

        static void initializeBucket(ValueType& bucket);
        static void deleteBucket(ValueType& bucket) { bucket.~ValueType(); Traits::constructDeletedValue(bucket); }

        FullLookupType makeLookupResult(ValueType* position, bool found, unsigned hash)
            { return FullLookupType(LookupType(position, found), hash); }

        iterator makeIterator(ValueType* pos) { return iterator(pos, m_table + m_tableSize); }
        const_iterator makeConstIterator(ValueType* pos) const { return const_iterator(pos, m_table + m_tableSize); }
        iterator makeKnownGoodIterator(ValueType* pos) { return iterator(pos, m_table + m_tableSize, HashItemKnownGood); }
        const_iterator makeKnownGoodConstIterator(ValueType* pos) const { return const_iterator(pos, m_table + m_tableSize, HashItemKnownGood); }

        static const unsigned m_maxLoad = 2;
        static const unsigned m_minLoad = 6;

        ValueType* m_table;
        unsigned m_tableSize;
        unsigned m_tableSizeMask;
        unsigned m_keyCount;
        unsigned m_deletedCount;

#if DUMP_HASHTABLE_STATS_PER_TABLE
    public:
        mutable OwnPtr<Stats> m_stats;
#endif

        template<bool x, typename T, typename U, typename V, typename W, typename X, typename Y, typename Z> friend struct WeakProcessingHashTableHelper;
    };

    // Set all the bits to one after the most significant bit: 00110101010 -> 00111111111.
    template<unsigned size> struct OneifyLowBits;
    template<>
    struct OneifyLowBits<0> {
        static const unsigned value = 0;
    };
    template<unsigned number>
    struct OneifyLowBits {
        static const unsigned value = number | OneifyLowBits<(number >> 1)>::value;
    };
    // Compute the first power of two integer that is an upper bound of the parameter 'number'.
    template<unsigned number>
    struct UpperPowerOfTwoBound {
        static const unsigned value = (OneifyLowBits<number - 1>::value + 1) * 2;
    };

    // Because power of two numbers are the limit of maxLoad, their capacity is twice the
    // UpperPowerOfTwoBound, or 4 times their values.
    template<unsigned size, bool isPowerOfTwo> struct HashTableCapacityForSizeSplitter;
    template<unsigned size>
    struct HashTableCapacityForSizeSplitter<size, true> {
        static const unsigned value = size * 4;
    };
    template<unsigned size>
    struct HashTableCapacityForSizeSplitter<size, false> {
        static const unsigned value = UpperPowerOfTwoBound<size>::value;
    };

    // HashTableCapacityForSize computes the upper power of two capacity to hold the size parameter.
    // This is done at compile time to initialize the HashTraits.
    template<unsigned size>
    struct HashTableCapacityForSize {
        static const unsigned value = HashTableCapacityForSizeSplitter<size, !(size & (size - 1))>::value;
        COMPILE_ASSERT(size > 0, HashTableNonZeroMinimumCapacity);
        COMPILE_ASSERT(!static_cast<int>(value >> 31), HashTableNoCapacityOverflow);
        COMPILE_ASSERT(value > (2 * size), HashTableCapacityHoldsContentSize);
    };

    template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename Allocator>
    inline HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, Allocator>::HashTable()
        : m_table(0)
        , m_tableSize(0)
        , m_tableSizeMask(0)
        , m_keyCount(0)
        , m_deletedCount(0)
#if DUMP_HASHTABLE_STATS_PER_TABLE
        , m_stats(adoptPtr(new Stats))
#endif
    {
    }

    inline unsigned doubleHash(unsigned key)
    {
        key = ~key + (key >> 23);
        key ^= (key << 12);
        key ^= (key >> 7);
        key ^= (key << 2);
        key ^= (key >> 20);
        return key;
    }

    template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename Allocator>
    template<typename HashTranslator, typename T>
    inline Value* HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, Allocator>::lookup(const T& key)
    {
        ValueType* table = m_table;
        if (!table)
            return 0;

        size_t k = 0;
        size_t sizeMask = m_tableSizeMask;
        unsigned h = HashTranslator::hash(key);
        size_t i = h & sizeMask;

#if DUMP_HASHTABLE_STATS
        atomicIncrement(&HashTableStats::numAccesses);
        int probeCount = 0;
#endif

#if DUMP_HASHTABLE_STATS_PER_TABLE
        ++m_stats->numAccesses;
        int perTableProbeCount = 0;
#endif

        while (1) {
            ValueType* entry = table + i;

            // we count on the compiler to optimize out this branch
            if (HashFunctions::safeToCompareToEmptyOrDeleted) {
                if (HashTranslator::equal(Extractor::extract(*entry), key))
                    return entry;

                if (isEmptyBucket(*entry))
                    return 0;
            } else {
                if (isEmptyBucket(*entry))
                    return 0;

                if (!isDeletedBucket(*entry) && HashTranslator::equal(Extractor::extract(*entry), key))
                    return entry;
            }
#if DUMP_HASHTABLE_STATS
            ++probeCount;
            HashTableStats::recordCollisionAtCount(probeCount);
#endif

#if DUMP_HASHTABLE_STATS_PER_TABLE
            ++perTableProbeCount;
            m_stats->recordCollisionAtCount(perTableProbeCount);
#endif

            if (!k)
                k = 1 | doubleHash(h);
            i = (i + k) & sizeMask;
        }
    }

    template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename Allocator>
    template<typename HashTranslator, typename T>
    inline typename HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, Allocator>::LookupType HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, Allocator>::lookupForWriting(const T& key)
    {
        ASSERT(m_table);

        size_t k = 0;
        ValueType* table = m_table;
        size_t sizeMask = m_tableSizeMask;
        unsigned h = HashTranslator::hash(key);
        size_t i = h & sizeMask;

#if DUMP_HASHTABLE_STATS
        atomicIncrement(&HashTableStats::numAccesses);
        int probeCount = 0;
#endif

#if DUMP_HASHTABLE_STATS_PER_TABLE
        ++m_stats->numAccesses;
        int perTableProbeCount = 0;
#endif

        ValueType* deletedEntry = 0;

        while (1) {
            ValueType* entry = table + i;

            // we count on the compiler to optimize out this branch
            if (HashFunctions::safeToCompareToEmptyOrDeleted) {
                if (isEmptyBucket(*entry))
                    return LookupType(deletedEntry ? deletedEntry : entry, false);

                if (HashTranslator::equal(Extractor::extract(*entry), key))
                    return LookupType(entry, true);

                if (isDeletedBucket(*entry))
                    deletedEntry = entry;
            } else {
                if (isEmptyBucket(*entry))
                    return LookupType(deletedEntry ? deletedEntry : entry, false);

                if (isDeletedBucket(*entry))
                    deletedEntry = entry;
                else if (HashTranslator::equal(Extractor::extract(*entry), key))
                    return LookupType(entry, true);
            }
#if DUMP_HASHTABLE_STATS
            ++probeCount;
            HashTableStats::recordCollisionAtCount(probeCount);
#endif

#if DUMP_HASHTABLE_STATS_PER_TABLE
            ++perTableProbeCount;
            m_stats->recordCollisionAtCount(perTableProbeCount);
#endif

            if (!k)
                k = 1 | doubleHash(h);
            i = (i + k) & sizeMask;
        }
    }

    template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename Allocator>
    template<typename HashTranslator, typename T>
    inline typename HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, Allocator>::FullLookupType HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, Allocator>::fullLookupForWriting(const T& key)
    {
        ASSERT(m_table);

        size_t k = 0;
        ValueType* table = m_table;
        size_t sizeMask = m_tableSizeMask;
        unsigned h = HashTranslator::hash(key);
        size_t i = h & sizeMask;

#if DUMP_HASHTABLE_STATS
        atomicIncrement(&HashTableStats::numAccesses);
        int probeCount = 0;
#endif

#if DUMP_HASHTABLE_STATS_PER_TABLE
        ++m_stats->numAccesses;
        int perTableProbeCount = 0;
#endif

        ValueType* deletedEntry = 0;

        while (1) {
            ValueType* entry = table + i;

            // we count on the compiler to optimize out this branch
            if (HashFunctions::safeToCompareToEmptyOrDeleted) {
                if (isEmptyBucket(*entry))
                    return makeLookupResult(deletedEntry ? deletedEntry : entry, false, h);

                if (HashTranslator::equal(Extractor::extract(*entry), key))
                    return makeLookupResult(entry, true, h);

                if (isDeletedBucket(*entry))
                    deletedEntry = entry;
            } else {
                if (isEmptyBucket(*entry))
                    return makeLookupResult(deletedEntry ? deletedEntry : entry, false, h);

                if (isDeletedBucket(*entry))
                    deletedEntry = entry;
                else if (HashTranslator::equal(Extractor::extract(*entry), key))
                    return makeLookupResult(entry, true, h);
            }
#if DUMP_HASHTABLE_STATS
            ++probeCount;
            HashTableStats::recordCollisionAtCount(probeCount);
#endif

#if DUMP_HASHTABLE_STATS_PER_TABLE
            ++perTableProbeCount;
            m_stats->recordCollisionAtCount(perTableProbeCount);
#endif

            if (!k)
                k = 1 | doubleHash(h);
            i = (i + k) & sizeMask;
        }
    }

    template<bool emptyValueIsZero> struct HashTableBucketInitializer;

    template<> struct HashTableBucketInitializer<false> {
        template<typename Traits, typename Value> static void initialize(Value& bucket)
        {
            new (NotNull, &bucket) Value(Traits::emptyValue());
        }
    };

    template<> struct HashTableBucketInitializer<true> {
        template<typename Traits, typename Value> static void initialize(Value& bucket)
        {
            // This initializes the bucket without copying the empty value.
            // That makes it possible to use this with types that don't support copying.
            // The memset to 0 looks like a slow operation but is optimized by the compilers.
            memset(&bucket, 0, sizeof(bucket));
        }
    };

    template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename Allocator>
    inline void HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, Allocator>::initializeBucket(ValueType& bucket)
    {
        HashTableBucketInitializer<Traits::emptyValueIsZero>::template initialize<Traits>(bucket);
    }

    template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename Allocator>
    template<typename HashTranslator, typename T, typename Extra>
    typename HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, Allocator>::AddResult HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, Allocator>::add(const T& key, const Extra& extra)
    {
        if (!m_table)
            expand();

        ASSERT(m_table);

        size_t k = 0;
        ValueType* table = m_table;
        size_t sizeMask = m_tableSizeMask;
        unsigned h = HashTranslator::hash(key);
        size_t i = h & sizeMask;

#if DUMP_HASHTABLE_STATS
        atomicIncrement(&HashTableStats::numAccesses);
        int probeCount = 0;
#endif

#if DUMP_HASHTABLE_STATS_PER_TABLE
        ++m_stats->numAccesses;
        int perTableProbeCount = 0;
#endif

        ValueType* deletedEntry = 0;
        ValueType* entry;
        while (1) {
            entry = table + i;

            // we count on the compiler to optimize out this branch
            if (HashFunctions::safeToCompareToEmptyOrDeleted) {
                if (isEmptyBucket(*entry))
                    break;

                if (HashTranslator::equal(Extractor::extract(*entry), key))
                    return AddResult(entry, false);

                if (isDeletedBucket(*entry))
                    deletedEntry = entry;
            } else {
                if (isEmptyBucket(*entry))
                    break;

                if (isDeletedBucket(*entry))
                    deletedEntry = entry;
                else if (HashTranslator::equal(Extractor::extract(*entry), key))
                    return AddResult(entry, false);
            }
#if DUMP_HASHTABLE_STATS
            ++probeCount;
            HashTableStats::recordCollisionAtCount(probeCount);
#endif

#if DUMP_HASHTABLE_STATS_PER_TABLE
            ++perTableProbeCount;
            m_stats->recordCollisionAtCount(perTableProbeCount);
#endif

            if (!k)
                k = 1 | doubleHash(h);
            i = (i + k) & sizeMask;
        }

        if (deletedEntry) {
            initializeBucket(*deletedEntry);
            entry = deletedEntry;
            --m_deletedCount;
        }

        HashTranslator::translate(*entry, key, extra);

        ++m_keyCount;

        if (shouldExpand()) {
            // FIXME: This makes an extra copy on expand. Probably not that bad since
            // expand is rare, but would be better to have a version of expand that can
            // follow a pivot entry and return the new position.
            typename WTF::RemoveReference<KeyPassInType>::Type enteredKey = Extractor::extract(*entry);
            expand();
            iterator findResult = find(enteredKey);
            ASSERT(findResult != end());
            ValueType* resultValue = findResult.get();
            AddResult result(resultValue, true);
            return result;
        }

        return AddResult(entry, true);
    }

    template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename Allocator>
    template<typename HashTranslator, typename T, typename Extra>
    typename HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, Allocator>::AddResult HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, Allocator>::addPassingHashCode(const T& key, const Extra& extra)
    {
        if (!m_table)
            expand();

        FullLookupType lookupResult = fullLookupForWriting<HashTranslator>(key);

        ValueType* entry = lookupResult.first.first;
        bool found = lookupResult.first.second;
        unsigned h = lookupResult.second;

        if (found)
            return AddResult(entry, false);

        if (isDeletedBucket(*entry)) {
            initializeBucket(*entry);
            --m_deletedCount;
        }

        HashTranslator::translate(*entry, key, extra, h);
        ++m_keyCount;
        if (shouldExpand()) {
            // FIXME: This makes an extra copy on expand. Probably not that bad since
            // expand is rare, but would be better to have a version of expand that can
            // follow a pivot entry and return the new position.
            typename WTF::RemoveReference<KeyPassInType>::Type enteredKey = Extractor::extract(*entry);
            expand();
            iterator findResult = find(enteredKey);
            ASSERT(findResult != end());
            ValueType* resultValue = findResult.get();
            AddResult result(resultValue, true);
            return result;
        }

        return AddResult(entry, true);
    }

    template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename Allocator>
    inline void HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, Allocator>::reinsert(ValueType& entry)
    {
        ASSERT(m_table);
        ASSERT(!lookupForWriting(Extractor::extract(entry)).second);
        ASSERT(!isDeletedBucket(*(lookupForWriting(Extractor::extract(entry)).first)));
#if DUMP_HASHTABLE_STATS
        atomicIncrement(&HashTableStats::numReinserts);
#endif
#if DUMP_HASHTABLE_STATS_PER_TABLE
        ++m_stats->numReinserts;
#endif

        Mover<ValueType, Traits::needsDestruction>::move(entry, *lookupForWriting(Extractor::extract(entry)).first);
    }

    template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename Allocator>
    template <typename HashTranslator, typename T>
    inline typename HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, Allocator>::iterator HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, Allocator>::find(const T& key)
    {
        ValueType* entry = lookup<HashTranslator>(key);
        if (!entry)
            return end();

        return makeKnownGoodIterator(entry);
    }

    template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename Allocator>
    template <typename HashTranslator, typename T>
    inline typename HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, Allocator>::const_iterator HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, Allocator>::find(const T& key) const
    {
        ValueType* entry = const_cast<HashTable*>(this)->lookup<HashTranslator>(key);
        if (!entry)
            return end();

        return makeKnownGoodConstIterator(entry);
    }

    template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename Allocator>
    template <typename HashTranslator, typename T>
    bool HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, Allocator>::contains(const T& key) const
    {
        return const_cast<HashTable*>(this)->lookup<HashTranslator>(key);
    }

    template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename Allocator>
    void HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, Allocator>::remove(ValueType* pos)
    {
#if DUMP_HASHTABLE_STATS
        atomicIncrement(&HashTableStats::numRemoves);
#endif
#if DUMP_HASHTABLE_STATS_PER_TABLE
        ++m_stats->numRemoves;
#endif

        deleteBucket(*pos);
        ++m_deletedCount;
        --m_keyCount;

        if (shouldShrink())
            shrink();
    }

    template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename Allocator>
    inline void HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, Allocator>::remove(iterator it)
    {
        if (it == end())
            return;

        remove(const_cast<ValueType*>(it.m_iterator.m_position));
    }

    template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename Allocator>
    inline void HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, Allocator>::remove(const_iterator it)
    {
        if (it == end())
            return;

        remove(const_cast<ValueType*>(it.m_position));
    }

    template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename Allocator>
    inline void HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, Allocator>::remove(KeyPeekInType key)
    {
        remove(find(key));
    }

    template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename Allocator>
    Value* HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, Allocator>::allocateTable(unsigned size)
    {
        typedef typename Allocator::template HashTableBackingHelper<Key, Value, Extractor, Traits, KeyTraits>::Type HashTableBacking;

        size_t allocSize = size * sizeof(ValueType);
        ValueType* result;
        if (Traits::emptyValueIsZero) {
            result = Allocator::template zeroedBackingMalloc<ValueType*, HashTableBacking>(allocSize);
        } else {
            result = Allocator::template backingMalloc<ValueType*, HashTableBacking>(allocSize);
            for (unsigned i = 0; i < size; i++)
                initializeBucket(result[i]);
        }
        return result;
    }

    template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename Allocator>
    void HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, Allocator>::deallocateTable(ValueType* table, unsigned size)
    {
        if (Allocator::isGarbageCollected)
            return;
        if (Traits::needsDestruction) {
            for (unsigned i = 0; i < size; ++i) {
                if (!isDeletedBucket(table[i]))
                    table[i].~ValueType();
            }
        }
        Allocator::backingFree(table);
    }

    template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename Allocator>
    void HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, Allocator>::expand()
    {
        unsigned newSize;
        if (!m_tableSize) {
            newSize = KeyTraits::minimumTableSize;
        } else if (mustRehashInPlace()) {
            newSize = m_tableSize;
        } else {
            newSize = m_tableSize * 2;
            RELEASE_ASSERT(newSize > m_tableSize);
        }

        rehash(newSize);
    }

    template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename Allocator>
    void HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, Allocator>::rehash(unsigned newTableSize)
    {
        unsigned oldTableSize = m_tableSize;
        ValueType* oldTable = m_table;

#if DUMP_HASHTABLE_STATS
        if (oldTableSize != 0)
            atomicIncrement(&HashTableStats::numRehashes);
#endif

#if DUMP_HASHTABLE_STATS_PER_TABLE
        if (oldTableSize != 0)
            ++m_stats->numRehashes;
#endif

        m_table = allocateTable(newTableSize);
        m_tableSize = newTableSize;
        m_tableSizeMask = newTableSize - 1;

        for (unsigned i = 0; i != oldTableSize; ++i)
            if (!isEmptyOrDeletedBucket(oldTable[i]))
                reinsert(oldTable[i]);

        m_deletedCount = 0;

        deallocateTable(oldTable, oldTableSize);
    }

    template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename Allocator>
    void HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, Allocator>::clear()
    {
        if (!m_table)
            return;

        deallocateTable(m_table, m_tableSize);
        m_table = 0;
        m_tableSize = 0;
        m_tableSizeMask = 0;
        m_keyCount = 0;
    }

    template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename Allocator>
    HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, Allocator>::HashTable(const HashTable& other)
        : m_table(0)
        , m_tableSize(0)
        , m_tableSizeMask(0)
        , m_keyCount(0)
        , m_deletedCount(0)
#if DUMP_HASHTABLE_STATS_PER_TABLE
        , m_stats(adoptPtr(new Stats(*other.m_stats)))
#endif
    {
        // Copy the hash table the dumb way, by adding each element to the new table.
        // It might be more efficient to copy the table slots, but it's not clear that efficiency is needed.
        const_iterator end = other.end();
        for (const_iterator it = other.begin(); it != end; ++it)
            add(*it);
    }

    template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename Allocator>
    void HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, Allocator>::swap(HashTable& other)
    {
        ValueType* tmpTable = m_table;
        m_table = other.m_table;
        other.m_table = tmpTable;

        size_t tmpTableSize = m_tableSize;
        m_tableSize = other.m_tableSize;
        other.m_tableSize = tmpTableSize;

        size_t tmpTableSizeMask = m_tableSizeMask;
        m_tableSizeMask = other.m_tableSizeMask;
        other.m_tableSizeMask = tmpTableSizeMask;

        size_t tmpKeyCount = m_keyCount;
        m_keyCount = other.m_keyCount;
        other.m_keyCount = tmpKeyCount;

        size_t tmpDeletedCount = m_deletedCount;
        m_deletedCount = other.m_deletedCount;
        other.m_deletedCount = tmpDeletedCount;

#if DUMP_HASHTABLE_STATS_PER_TABLE
        m_stats.swap(other.m_stats);
#endif
    }

    template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename Allocator>
    HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, Allocator>& HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, Allocator>::operator=(const HashTable& other)
    {
        HashTable tmp(other);
        swap(tmp);
        return *this;
    }

    template<bool isWeak, typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename Allocator>
    struct WeakProcessingHashTableHelper;

    template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename Allocator>
    struct WeakProcessingHashTableHelper<false, Key, Value, Extractor, HashFunctions, Traits, KeyTraits, Allocator> {
        static void process(typename Allocator::Visitor* visitor, void* closure) { }
    };

    template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename Allocator>
    struct WeakProcessingHashTableHelper<true, Key, Value, Extractor, HashFunctions, Traits, KeyTraits, Allocator> {
        static void process(typename Allocator::Visitor* visitor, void* closure)
        {
            typedef HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, Allocator> HashTableType;
            HashTableType* table = reinterpret_cast<HashTableType*>(closure);
            if (table->m_table) {
                // This just marks it live and does not push anything onto the
                // marking stack.
                Allocator::markNoTracing(visitor, table->m_table);
                // Now perform weak processing (this is a no-op if the backing
                // was accessible through an iterator and was already marked
                // strongly).
                for (typename HashTableType::ValueType* element = table->m_table + table->m_tableSize - 1; element >= table->m_table; element--) {
                    if (!HashTableType::isEmptyOrDeletedBucket(*element)) {
                        if (Allocator::hasDeadMember(visitor, *element)) {
                            HashTableType::deleteBucket(*element); // Also calls the destructor.
                            table->m_deletedCount++;
                            table->m_keyCount--;
                            // We don't rehash the backing until the next add
                            // or delete, because that would cause allocation
                            // during GC.
                        }
                    }
                }
            }
        }
    };

    template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename Allocator>
    void HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, Allocator>::trace(typename Allocator::Visitor* visitor)
    {
        // If someone else already marked the backing and queued up the trace
        // and/or weak callback then we are done.
        if (!m_table || visitor->isAlive(m_table))
            return;
        // Normally, we mark the backing store without performing trace. This
        // means it is marked live, but the pointers inside it are not marked.
        // Instead we will mark the pointers below. However, for backing
        // stores that contain weak pointers the handling is rather different.
        // We don't mark the backing store here, so the marking GC will leave
        // the backing unmarked. If the backing is found in any other way than
        // through its HashTable (ie from an iterator) then the mark bit will
        // be set and the pointers will be marked strongly, avoiding problems
        // with iterating over things that disappear due to weak processing
        // while we are iterating over them. The weakProcessing callback will
        // mark the backing as a void pointer, and will perform weak processing
        // if needed.
        if (!Traits::isWeak)
            Allocator::markNoTracing(visitor, m_table);
        else
            Allocator::registerWeakMembers(visitor, this, m_table, WeakProcessingHashTableHelper<Traits::isWeak, Key, Value, Extractor, HashFunctions, Traits, KeyTraits, Allocator>::process);
        if (ShouldBeTraced<Traits>::value) {
            for (ValueType* element = m_table + m_tableSize - 1; element >= m_table; element--) {
                if (!isEmptyOrDeletedBucket(*element))
                    Allocator::template trace<ValueType, Traits>(visitor, *element);
            }
        }
    }

    // iterator adapters

    template<typename HashTableType, typename Traits> struct HashTableConstIteratorAdapter {
        HashTableConstIteratorAdapter() {}
        HashTableConstIteratorAdapter(const typename HashTableType::const_iterator& impl) : m_impl(impl) {}
        typedef typename Traits::IteratorConstGetType GetType;
        typedef typename HashTableType::ValueTraits::IteratorConstGetType SourceGetType;

        GetType get() const { return const_cast<GetType>(SourceGetType(m_impl.get())); }
        typename Traits::IteratorConstReferenceType operator*() const { return Traits::getToReferenceConstConversion(get()); }
        GetType operator->() const { return get(); }

        HashTableConstIteratorAdapter& operator++() { ++m_impl; return *this; }
        // postfix ++ intentionally omitted

        typename HashTableType::const_iterator m_impl;
    };

    template<typename HashTableType, typename Traits> struct HashTableIteratorAdapter {
        typedef typename Traits::IteratorGetType GetType;
        typedef typename HashTableType::ValueTraits::IteratorGetType SourceGetType;

        HashTableIteratorAdapter() {}
        HashTableIteratorAdapter(const typename HashTableType::iterator& impl) : m_impl(impl) {}

        GetType get() const { return const_cast<GetType>(SourceGetType(m_impl.get())); }
        typename Traits::IteratorReferenceType operator*() const { return Traits::getToReferenceConversion(get()); }
        GetType operator->() const { return get(); }

        HashTableIteratorAdapter& operator++() { ++m_impl; return *this; }
        // postfix ++ intentionally omitted

        operator HashTableConstIteratorAdapter<HashTableType, Traits>()
        {
            typename HashTableType::const_iterator i = m_impl;
            return i;
        }

        typename HashTableType::iterator m_impl;
    };

    template<typename T, typename U>
    inline bool operator==(const HashTableConstIteratorAdapter<T, U>& a, const HashTableConstIteratorAdapter<T, U>& b)
    {
        return a.m_impl == b.m_impl;
    }

    template<typename T, typename U>
    inline bool operator!=(const HashTableConstIteratorAdapter<T, U>& a, const HashTableConstIteratorAdapter<T, U>& b)
    {
        return a.m_impl != b.m_impl;
    }

    template<typename T, typename U>
    inline bool operator==(const HashTableIteratorAdapter<T, U>& a, const HashTableIteratorAdapter<T, U>& b)
    {
        return a.m_impl == b.m_impl;
    }

    template<typename T, typename U>
    inline bool operator!=(const HashTableIteratorAdapter<T, U>& a, const HashTableIteratorAdapter<T, U>& b)
    {
        return a.m_impl != b.m_impl;
    }

    // All 4 combinations of ==, != and Const,non const.
    template<typename T, typename U>
    inline bool operator==(const HashTableConstIteratorAdapter<T, U>& a, const HashTableIteratorAdapter<T, U>& b)
    {
        return a.m_impl == b.m_impl;
    }

    template<typename T, typename U>
    inline bool operator!=(const HashTableConstIteratorAdapter<T, U>& a, const HashTableIteratorAdapter<T, U>& b)
    {
        return a.m_impl != b.m_impl;
    }

    template<typename T, typename U>
    inline bool operator==(const HashTableIteratorAdapter<T, U>& a, const HashTableConstIteratorAdapter<T, U>& b)
    {
        return a.m_impl == b.m_impl;
    }

    template<typename T, typename U>
    inline bool operator!=(const HashTableIteratorAdapter<T, U>& a, const HashTableConstIteratorAdapter<T, U>& b)
    {
        return a.m_impl != b.m_impl;
    }

} // namespace WTF

#include "wtf/HashIterators.h"

#endif // WTF_HashTable_h
