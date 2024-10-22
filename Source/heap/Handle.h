/*
 * Copyright (C) 2014 Google Inc. All rights reserved.
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

#ifndef Handle_h
#define Handle_h

#include "heap/Heap.h"
#include "heap/ThreadState.h"
#include "heap/Visitor.h"

#include "wtf/Locker.h"
#include "wtf/RawPtr.h"
#include "wtf/RefCounted.h"

namespace WebCore {

template<typename T> class Member;

class PersistentNode {
public:
    explicit PersistentNode(TraceCallback trace)
        : m_trace(trace)
    {
    }

    bool isAlive() { return m_trace; }

    virtual ~PersistentNode()
    {
        ASSERT(isAlive());
        m_trace = 0;
    }

    // Ideally the trace method should be virtual and automatically dispatch
    // to the most specific implementation. However having a virtual method
    // on PersistentNode leads to too eager template instantiation with MSVC
    // which leads to include cycles.
    // Instead we call the constructor with a TraceCallback which knows the
    // type of the most specific child and calls trace directly. See
    // TraceMethodDelegate in Visitor.h for how this is done.
    void trace(Visitor* visitor)
    {
        m_trace(visitor, this);
    }

protected:
    TraceCallback m_trace;

private:
    PersistentNode* m_next;
    PersistentNode* m_prev;

    template<typename RootsAccessor, typename Owner> friend class PersistentBase;
    friend class PersistentAnchor;
    friend class ThreadState;
};

// RootsAccessor for Persistent that provides access to thread-local list
// of persistent handles. Can only be used to create handles that
// are constructed and destructed on the same thread.
template<ThreadAffinity Affinity>
class ThreadLocalPersistents {
public:
    static PersistentNode* roots() { return state()->roots(); }

    // No locking required. Just check that we are at the right thread.
    class Lock {
    public:
        Lock() { state()->checkThread(); }
    };

private:
    static ThreadState* state() { return ThreadStateFor<Affinity>::state(); }
};

// RootsAccessor for Persistent that provides synchronized access to global
// list of persistent handles. Can be used for persistent handles that are
// passed between threads.
class GlobalPersistents {
public:
    static PersistentNode* roots() { return ThreadState::globalRoots(); }

    class Lock {
    public:
        Lock() : m_locker(ThreadState::globalRootsMutex()) { }
    private:
        MutexLocker m_locker;
    };
};

// Base class for persistent handles. RootsAccessor specifies which list to
// link resulting handle into. Owner specifies the class containing trace
// method.
template<typename RootsAccessor, typename Owner>
class PersistentBase : public PersistentNode {
public:
    ~PersistentBase()
    {
        typename RootsAccessor::Lock lock;
        ASSERT(m_roots == RootsAccessor::roots()); // Check that the thread is using the same roots list.
        ASSERT(isAlive());
        ASSERT(m_next->isAlive());
        ASSERT(m_prev->isAlive());
        m_next->m_prev = m_prev;
        m_prev->m_next = m_next;
    }

protected:
    inline PersistentBase()
        : PersistentNode(TraceMethodDelegate<Owner, &Owner::trace>::trampoline)
#ifndef NDEBUG
        , m_roots(RootsAccessor::roots())
#endif
    {
        typename RootsAccessor::Lock lock;
        m_prev = RootsAccessor::roots();
        m_next = m_prev->m_next;
        m_prev->m_next = this;
        m_next->m_prev = this;
    }

    inline explicit PersistentBase(const PersistentBase& otherref)
        : PersistentNode(otherref.m_trace)
#ifndef NDEBUG
        , m_roots(RootsAccessor::roots())
#endif
    {
        typename RootsAccessor::Lock lock;
        ASSERT(otherref.m_roots == m_roots); // Handles must belong to the same list.
        PersistentBase* other = const_cast<PersistentBase*>(&otherref);
        m_prev = other;
        m_next = other->m_next;
        other->m_next = this;
        m_next->m_prev = this;
    }

    inline PersistentBase& operator=(const PersistentBase& otherref) { return *this; }

#ifndef NDEBUG
private:
    PersistentNode* m_roots;
#endif
};

// A dummy Persistent handle that ensures the list of persistents is never null.
// This removes a test from a hot path.
class PersistentAnchor : public PersistentNode {
public:
    void trace(Visitor* visitor)
    {
        for (PersistentNode* current = m_next; current != this; current = current->m_next)
            current->trace(visitor);
    }

private:
    virtual ~PersistentAnchor()
    {
        // FIXME: oilpan: Ideally we should have no left-over persistents at this point. However currently there is a
        // large number of objects leaked when we tear down the main thread. Since some of these might contain a
        // persistent or e.g. be RefCountedGarbageCollected we cannot guarantee there are no remaining Persistents at
        // this point.
    }
    PersistentAnchor() : PersistentNode(TraceMethodDelegate<PersistentAnchor, &PersistentAnchor::trace>::trampoline)
    {
        m_next = this;
        m_prev = this;
    }

    friend class ThreadState;
};

// Persistent handles are used to store pointers into the
// managed heap. As long as the Persistent handle is alive
// the GC will keep the object pointed to alive. Persistent
// handles can be stored in objects and they are not scoped.
// Persistent handles must not be used to contain pointers
// between objects that are in the managed heap. They are only
// meant to point to managed heap objects from variables/members
// outside the managed heap.
//
// A Persistent is always a GC root from the point of view of
// the garbage collector.
template<typename T, typename RootsAccessor /* = ThreadLocalPersistents<ThreadingTrait<T>::Affinity > */ >
class Persistent : public PersistentBase<RootsAccessor, Persistent<T, RootsAccessor> > {
    WTF_DISALLOW_CONSTRUCTION_FROM_ZERO(Persistent);
    WTF_DISALLOW_ZERO_ASSIGNMENT(Persistent);
public:
    Persistent() : m_raw(0) { }
    Persistent(std::nullptr_t) : m_raw(0) { }

    Persistent(T* raw) : m_raw(raw) { }

    Persistent(const Persistent& other) : m_raw(other) { }

    template<typename U>
    Persistent(const Persistent<U, RootsAccessor>& other) : m_raw(other) { }

    template<typename U>
    Persistent(const Member<U>& other) : m_raw(other) { }

    template<typename U>
    Persistent(const RawPtr<U>& other) : m_raw(other.get()) { }

    template<typename U>
    Persistent& operator=(U* other)
    {
        m_raw = other;
        return *this;
    }

    Persistent& operator=(std::nullptr_t)
    {
        m_raw = 0;
        return *this;
    }

    void clear() { m_raw = 0; }

    virtual ~Persistent()
    {
        m_raw = 0;
    }

    template<typename U>
    U* as() const
    {
        return static_cast<U*>(m_raw);
    }

    void trace(Visitor* visitor) { visitor->mark(m_raw); }

    T* release()
    {
        T* result = m_raw;
        m_raw = 0;
        return result;
    }

    T& operator*() const { return *m_raw; }

    bool operator!() const { return !m_raw; }

    operator T*() const { return m_raw; }
    operator RawPtr<T>() const { return m_raw; }

    T* operator->() const { return *this; }

    Persistent& operator=(const Persistent& other)
    {
        m_raw = other;
        return *this;
    }

    template<typename U>
    Persistent& operator=(const Persistent<U, RootsAccessor>& other)
    {
        m_raw = other;
        return *this;
    }

    template<typename U>
    Persistent& operator=(const Member<U>& other)
    {
        m_raw = other;
        return *this;
    }

    template<typename U>
    Persistent& operator=(const RawPtr<U>& other)
    {
        m_raw = other;
        return *this;
    }

    T* get() const { return m_raw; }

private:
    T* m_raw;
};

// FIXME: derive affinity based on the collection.
template<typename Collection, ThreadAffinity Affinity = AnyThread>
class PersistentHeapCollectionBase
    : public Collection
    , public PersistentBase<ThreadLocalPersistents<Affinity>, PersistentHeapCollectionBase<Collection, Affinity> > {
    // Never allocate these objects with new. Use Persistent<Collection> instead.
    DISALLOW_ALLOCATION();
public:
    void trace(Visitor* visitor) { visitor->trace(*static_cast<Collection*>(this)); }
};

template<
    typename KeyArg,
    typename MappedArg,
    typename HashArg = typename DefaultHash<KeyArg>::Hash,
    typename KeyTraitsArg = HashTraits<KeyArg>,
    typename MappedTraitsArg = HashTraits<MappedArg> >
class PersistentHeapHashMap : public PersistentHeapCollectionBase<HeapHashMap<KeyArg, MappedArg, HashArg, KeyTraitsArg, MappedTraitsArg> > { };

template<
    typename ValueArg,
    typename HashArg = typename DefaultHash<ValueArg>::Hash,
    typename TraitsArg = HashTraits<ValueArg> >
class PersistentHeapHashSet : public PersistentHeapCollectionBase<HeapHashSet<ValueArg, HashArg, TraitsArg> > { };

template<typename T, size_t inlineCapacity = 0>
class PersistentHeapVector : public PersistentHeapCollectionBase<HeapVector<T, inlineCapacity> > { };

// Members are used in classes to contain strong pointers to other oilpan heap
// allocated objects.
// All Member fields of a class must be traced in the class' trace method.
// During the mark phase of the GC all live objects are marked as live and
// all Member fields of a live object will be traced marked as live as well.
template<typename T>
class Member {
    WTF_DISALLOW_CONSTRUCTION_FROM_ZERO(Member);
    WTF_DISALLOW_ZERO_ASSIGNMENT(Member);
public:
    Member() : m_raw(0) { }
    Member(std::nullptr_t) : m_raw(0) { }

    Member(T* raw) : m_raw(raw) { }

    Member(WTF::HashTableDeletedValueType) : m_raw(reinterpret_cast<T*>(-1)) { }

    bool isHashTableDeletedValue() const { return m_raw == reinterpret_cast<T*>(-1); }

    template<typename U>
    Member(const Persistent<U>& other) : m_raw(other) { }

    Member(const Member& other) : m_raw(other) { }

    template<typename U>
    Member(const Member<U>& other) : m_raw(other) { }

    T* release()
    {
        T* result = m_raw;
        m_raw = 0;
        return result;
    }

    template<typename U>
    U* as() const
    {
        return static_cast<U*>(m_raw);
    }

    bool operator!() const { return !m_raw; }

    operator T*() const { return m_raw; }

    T* operator->() const { return m_raw; }
    T& operator*() const { return *m_raw; }
    operator RawPtr<T>() const { return m_raw; }

    template<typename U>
    Member& operator=(const Persistent<U>& other)
    {
        m_raw = other;
        return *this;
    }

    template<typename U>
    Member& operator=(const Member<U>& other)
    {
        m_raw = other;
        return *this;
    }

    template<typename U>
    Member& operator=(U* other)
    {
        m_raw = other;
        return *this;
    }

    template<typename U>
    Member& operator=(RawPtr<U> other)
    {
        m_raw = other;
        return *this;
    }

    Member& operator=(std::nullptr_t)
    {
        m_raw = 0;
        return *this;
    }

    void swap(Member<T>& other) { std::swap(m_raw, other.m_raw); }

    T* get() const { return m_raw; }

    void clear() { m_raw = 0; }

protected:
    T* m_raw;

    template<bool x, bool y, bool z, typename U, typename V> friend struct CollectionBackingTraceTrait;
};

template<typename T>
class TraceTrait<Member<T> > {
public:
    static void trace(Visitor* visitor, void* self)
    {
        TraceTrait<T>::mark(visitor, *static_cast<Member<T>*>(self));
    }
};

// TraceTrait to allow compilation of trace method bodies when oilpan is disabled.
// This should never be called, but is needed to compile.
template<typename T>
class TraceTrait<RefPtr<T> > {
public:
    static void trace(Visitor*, void*)
    {
        ASSERT_NOT_REACHED();
    }
};

// WeakMember is similar to Member in that it is used to point to other oilpan
// heap allocated objects.
// However instead of creating a strong pointer to the object, the WeakMember creates
// a weak pointer, which does not keep the pointee alive. Hence if all pointers to
// to a heap allocated object are weak the object will be garbage collected. At the
// time of GC the weak pointers will automatically be set to null.
template<typename T>
class WeakMember : public Member<T> {
    WTF_DISALLOW_CONSTRUCTION_FROM_ZERO(WeakMember);
    WTF_DISALLOW_ZERO_ASSIGNMENT(WeakMember);
public:
    WeakMember() : Member<T>() { }
    WeakMember(std::nullptr_t) : Member<T>(nullptr) { }

    WeakMember(T* raw) : Member<T>(raw) { }

    WeakMember(WTF::HashTableDeletedValueType x) : Member<T>(x) { }

    template<typename U>
    WeakMember(const Persistent<U>& other) : Member<T>(other) { }

    template<typename U>
    WeakMember(const Member<U>& other) : Member<T>(other) { }

    template<typename U>
    WeakMember& operator=(const Persistent<U>& other)
    {
        this->m_raw = other;
        return *this;
    }

    template<typename U>
    WeakMember& operator=(const Member<U>& other)
    {
        this->m_raw = other;
        return *this;
    }

    template<typename U>
    WeakMember& operator=(U* other)
    {
        this->m_raw = other;
        return *this;
    }

    template<typename U>
    WeakMember& operator=(const RawPtr<U>& other)
    {
        this->m_raw = other;
        return *this;
    }

    WeakMember& operator=(std::nullptr_t)
    {
        this->m_raw = 0;
        return *this;
    }

private:
    T** cell() const { return const_cast<T**>(&this->m_raw); }

    friend class Visitor;
};

// Comparison operators between (Weak)Members and Persistents
template<typename T, typename U> inline bool operator==(const Member<T>& a, const Member<U>& b) { return a.get() == b.get(); }
template<typename T, typename U> inline bool operator!=(const Member<T>& a, const Member<U>& b) { return a.get() != b.get(); }
template<typename T, typename U> inline bool operator==(const Member<T>& a, const Persistent<U>& b) { return a.get() == b.get(); }
template<typename T, typename U> inline bool operator!=(const Member<T>& a, const Persistent<U>& b) { return a.get() != b.get(); }
template<typename T, typename U> inline bool operator==(const Persistent<T>& a, const Member<U>& b) { return a.get() == b.get(); }
template<typename T, typename U> inline bool operator!=(const Persistent<T>& a, const Member<U>& b) { return a.get() != b.get(); }
template<typename T, typename U> inline bool operator==(const Persistent<T>& a, const Persistent<U>& b) { return a.get() == b.get(); }
template<typename T, typename U> inline bool operator!=(const Persistent<T>& a, const Persistent<U>& b) { return a.get() != b.get(); }

// CPP-defined type names for the transition period where we want to
// support both reference counting and garbage collection based on a
// compile-time flag.
//
// C++11 template aliases were initially used (with clang only, not
// with GCC nor MSVC.) However, supporting both CPP defines and
// template aliases is problematic from outside a WebCore namespace
// when Oilpan is disabled: e.g.,
// WebCore::RefCountedWillBeGarbageCollected as a template alias would
// uniquely resolve from within any namespace, but if it is backed by
// a CPP #define, it would expand to WebCore::RefCounted, and not the
// required WTF::RefCounted.
//
// Having the CPP expansion instead be fully namespace qualified, and the
// transition type be unqualified, would dually not work for template
// aliases. So, slightly unfortunately, fall back/down to the lowest
// commmon denominator of using CPP macros only.
#if ENABLE(OILPAN)
#define PassRefPtrWillBeRawPtr WTF::RawPtr
#define RefCountedWillBeGarbageCollected WebCore::GarbageCollected
#define RefCountedWillBeGarbageCollectedFinalized WebCore::GarbageCollectedFinalized
#define RefCountedWillBeRefCountedGarbageCollected WebCore::RefCountedGarbageCollected
#define RefPtrWillBePersistent WebCore::Persistent
#define RefPtrWillBeRawPtr WTF::RawPtr
#define RefPtrWillBeMember WebCore::Member
#define RawPtrWillBeMember WebCore::Member
#define RawPtrWillBeWeakMember WebCore::WeakMember
#define OwnPtrWillBeMember WebCore::Member
#define OwnPtrWillBePersistent WebCore::Persistent
#define PassOwnPtrWillBeRawPtr WTF::RawPtr
#define NoBaseWillBeGarbageCollected WebCore::GarbageCollected
#define NoBaseWillBeGarbageCollectedFinalized WebCore::GarbageCollectedFinalized
#define WillBeHeapHashMap WebCore::HeapHashMap
#define WillBePersistentHeapHashMap WebCore::PersistentHeapHashMap
#define WillBeHeapHashSet WebCore::HeapHashSet
#define WillBePersistentHeapHashSet WebCore::PersistentHeapHashSet
#define WillBeHeapVector WebCore::HeapVector
#define WillBePersistentHeapVector WebCore::PersistentHeapVector

template<typename T> PassRefPtrWillBeRawPtr<T> adoptRefWillBeNoop(T* ptr)
{
    static const bool notRefCountedGarbageCollected = !WTF::IsSubclassOfTemplate<T, RefCountedGarbageCollected>::value;
    static const bool notRefCounted = !WTF::IsSubclassOfTemplate<T, RefCounted>::value;
    COMPILE_ASSERT(notRefCountedGarbageCollected, useAdoptRefCountedWillBeRefCountedGarbageCollected);
    COMPILE_ASSERT(notRefCounted, youMustAdopt);
    return PassRefPtrWillBeRawPtr<T>(ptr);
}

template<typename T> PassRefPtrWillBeRawPtr<T> adoptRefCountedWillBeRefCountedGarbageCollected(T* ptr)
{
    static const bool isRefCountedGarbageCollected = WTF::IsSubclassOfTemplate<T, RefCountedGarbageCollected>::value;
    COMPILE_ASSERT(isRefCountedGarbageCollected, useAdoptRefWillBeNoop);
    return PassRefPtrWillBeRawPtr<T>(adoptRefCountedGarbageCollected(ptr));
}

template<typename T> PassOwnPtrWillBeRawPtr<T> adoptPtrWillBeNoop(T* ptr)
{
    static const bool notRefCountedGarbageCollected = !WTF::IsSubclassOfTemplate<T, RefCountedGarbageCollected>::value;
    static const bool notRefCounted = !WTF::IsSubclassOfTemplate<T, RefCounted>::value;
    COMPILE_ASSERT(notRefCountedGarbageCollected, useAdoptRefCountedWillBeRefCountedGarbageCollected);
    COMPILE_ASSERT(notRefCounted, youMustAdopt);
    return PassOwnPtrWillBeRawPtr<T>(ptr);
}

#define WTF_MAKE_FAST_ALLOCATED_WILL_BE_REMOVED // do nothing when oilpan is enabled.

#else // !ENABLE(OILPAN)

template<typename T>
class DummyBase {
public:
    DummyBase() { }
    ~DummyBase() { }
};

#define PassRefPtrWillBeRawPtr WTF::PassRefPtr
#define RefCountedWillBeGarbageCollected WTF::RefCounted
#define RefCountedWillBeGarbageCollectedFinalized WTF::RefCounted
#define RefCountedWillBeRefCountedGarbageCollected WTF::RefCounted
#define RefPtrWillBePersistent WTF::RefPtr
#define RefPtrWillBeRawPtr WTF::RefPtr
#define RefPtrWillBeMember WTF::RefPtr
#define RawPtrWillBeMember WTF::RawPtr
#define RawPtrWillBeWeakMember WTF::RawPtr
#define OwnPtrWillBeMember WTF::OwnPtr
#define OwnPtrWillBePersistent WTF::OwnPtr
#define PassOwnPtrWillBeRawPtr WTF::PassOwnPtr
#define NoBaseWillBeGarbageCollected WebCore::DummyBase
#define NoBaseWillBeGarbageCollectedFinalized WebCore::DummyBase
#define WillBeHeapHashMap WTF::HashMap
#define WillBePersistentHeapHashMap WTF::HashMap
#define WillBeHeapHashSet WTF::HashSet
#define WillBePersistentHeapHashSet WTF::HashSet
#define WillBeHeapVector WTF::Vector
#define WillBePersistentHeapVector WTF::Vector

template<typename T> PassRefPtrWillBeRawPtr<T> adoptRefWillBeNoop(T* ptr) { return adoptRef(ptr); }
template<typename T> PassRefPtrWillBeRawPtr<T> adoptRefCountedWillBeRefCountedGarbageCollected(T* ptr) { return adoptRef(ptr); }
template<typename T> PassOwnPtrWillBeRawPtr<T> adoptPtrWillBeNoop(T* ptr) { return adoptPtr(ptr); }

#define WTF_MAKE_FAST_ALLOCATED_WILL_BE_REMOVED WTF_MAKE_FAST_ALLOCATED

#endif // ENABLE(OILPAN)

} // namespace WebCore

namespace WTF {

template <typename T> struct VectorTraits<WebCore::Member<T> > : VectorTraitsBase<WebCore::Member<T> > {
    static const bool needsDestruction = false;
    static const bool canInitializeWithMemset = true;
    static const bool canMoveWithMemcpy = true;
};

template <typename T> struct VectorTraits<WebCore::WeakMember<T> > : VectorTraitsBase<WebCore::WeakMember<T> > {
    static const bool needsDestruction = false;
    static const bool canInitializeWithMemset = true;
    static const bool canMoveWithMemcpy = true;
};

template<typename T> struct HashTraits<WebCore::Member<T> > : SimpleClassHashTraits<WebCore::Member<T> > {
    static const bool needsDestruction = false;
    // FIXME: The distinction between PeekInType and PassInType is there for
    // the sake of the reference counting handles. When they are gone the two
    // types can be merged into PassInType.
    // FIXME: Implement proper const'ness for iterator types. Requires support
    // in the marking Visitor.
    typedef RawPtr<T> PeekInType;
    typedef RawPtr<T> PassInType;
    typedef WebCore::Member<T>* IteratorGetType;
    typedef const WebCore::Member<T>* IteratorConstGetType;
    typedef T* IteratorReferenceType;
    typedef T* IteratorConstReferenceType;
    static IteratorConstGetType getToConstGetConversion(const WebCore::Member<T>* x) { return x->get(); }
    static IteratorReferenceType getToReferenceConversion(IteratorGetType x) { return x->get(); }
    static IteratorConstReferenceType getToReferenceConstConversion(IteratorConstGetType x) { return x->get(); }
    // FIXME: Similarly, there is no need for a distinction between PeekOutType
    // and PassOutType without reference counting.
    typedef T* PeekOutType;
    typedef T* PassOutType;

    template<typename U>
    static void store(const U& value, WebCore::Member<T>& storage) { storage = value; }

    static PeekOutType peek(const WebCore::Member<T>& value) { return value; }
    static PassOutType passOut(const WebCore::Member<T>& value) { return value; }
};

template<typename T> struct HashTraits<WebCore::WeakMember<T> > : SimpleClassHashTraits<WebCore::WeakMember<T> > {
    static const bool needsDestruction = false;
    // FIXME: The distinction between PeekInType and PassInType is there for
    // the sake of the reference counting handles. When they are gone the two
    // types can be merged into PassInType.
    // FIXME: Implement proper const'ness for iterator types. Requires support
    // in the marking Visitor.
    typedef RawPtr<T> PeekInType;
    typedef RawPtr<T> PassInType;
    typedef WebCore::WeakMember<T>* IteratorGetType;
    typedef const WebCore::WeakMember<T>* IteratorConstGetType;
    typedef T* IteratorReferenceType;
    typedef T* IteratorConstReferenceType;
    static IteratorConstGetType getToConstGetConversion(const WebCore::WeakMember<T>* x) { return x->get(); }
    static IteratorReferenceType getToReferenceConversion(IteratorGetType x) { return x->get(); }
    static IteratorConstReferenceType getToReferenceConstConversion(IteratorConstGetType x) { return x->get(); }
    // FIXME: Similarly, there is no need for a distinction between PeekOutType
    // and PassOutType without reference counting.
    typedef T* PeekOutType;
    typedef T* PassOutType;

    template<typename U>
    static void store(const U& value, WebCore::WeakMember<T>& storage) { storage = value; }

    static PeekOutType peek(const WebCore::WeakMember<T>& value) { return value; }
    static PassOutType passOut(const WebCore::WeakMember<T>& value) { return value; }
};

template<typename T> struct PtrHash<WebCore::Member<T> > : PtrHash<T*> {
    template<typename U>
    static unsigned hash(const U& key) { return PtrHash<T*>::hash(key); }
    static bool equal(T* a, const WebCore::Member<T>& b) { return a == b; }
    static bool equal(const WebCore::Member<T>& a, T* b) { return a == b; }
    template<typename U, typename V>
    static bool equal(const U& a, const V& b) { return a == b; }
};

template<typename T> struct PtrHash<WebCore::WeakMember<T> > : PtrHash<WebCore::Member<T> > {
};

// PtrHash is the default hash for hash tables with members.
template<typename T> struct DefaultHash<WebCore::Member<T> > {
    typedef PtrHash<WebCore::Member<T> > Hash;
};

template<typename T> struct DefaultHash<WebCore::WeakMember<T> > {
    typedef PtrHash<WebCore::WeakMember<T> > Hash;
};

template<typename T>
struct NeedsTracing<WebCore::Member<T> > {
    static const bool value = true;
};

template<typename T>
struct IsWeak<WebCore::WeakMember<T> > {
    static const bool value = true;
};

template<typename T, size_t inlineCapacity>
struct NeedsTracing<WebCore::HeapVector<T, inlineCapacity> > {
    static const bool value = true;
};

template<typename T, typename U, typename V>
struct NeedsTracing<WebCore::HeapHashSet<T, U, V> > {
    static const bool value = true;
};

template<typename T, typename U, typename V, typename W, typename X>
struct NeedsTracing<WebCore::HeapHashMap<T, U, V, W, X> > {
    static const bool value = true;
};

template<typename Key, typename Value, typename Extractor, typename Traits, typename KeyTraits>
struct IsWeak<WebCore::HeapHashTableBacking<Key, Value, Extractor, Traits, KeyTraits> > {
    static const bool value = Traits::isWeak;
};

template<typename T> inline T* getPtr(const WebCore::Member<T>& p)
{
    return p.get();
}

} // namespace WTF

#endif
