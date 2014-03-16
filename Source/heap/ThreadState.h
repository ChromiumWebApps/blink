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

#ifndef ThreadState_h
#define ThreadState_h

#include "heap/AddressSanitizer.h"
#include "heap/HeapExport.h"
#include "wtf/HashSet.h"
#include "wtf/OwnPtr.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/ThreadSpecific.h"
#include "wtf/Threading.h"
#include "wtf/ThreadingPrimitives.h"
#include "wtf/Vector.h"

namespace WebCore {

class BaseHeap;
class BaseHeapPage;
class FinalizedHeapObjectHeader;
class HeapContainsCache;
class HeapObjectHeader;
class PersistentNode;
class Visitor;
class SafePointBarrier;
template<typename Header> class ThreadHeap;
class CallbackStack;

typedef uint8_t* Address;

typedef void (*FinalizationCallback)(void*);
typedef void (*VisitorCallback)(Visitor*, void* self);
typedef VisitorCallback TraceCallback;
typedef VisitorCallback WeakPointerCallback;

// ThreadAffinity indicates which threads objects can be used on. We
// distinguish between objects that can be used on the main thread
// only and objects that can be used on any thread.
//
// For objects that can only be used on the main thread we avoid going
// through thread-local storage to get to the thread state.
//
// FIXME: We should evaluate the performance gain. Having
// ThreadAffinity is complicating the implementation and we should get
// rid of it if it is fast enough to go through thread-local storage
// always.
enum ThreadAffinity {
    AnyThread,
    MainThreadOnly,
};

class Node;
class CSSValue;

template<typename T, bool derivesNodeOrCSSValue = WTF::IsSubclass<T, Node>::value || WTF::IsSubclass<T, CSSValue>::value > struct DefaultThreadingTrait;

template<typename T>
struct DefaultThreadingTrait<T, false> {
    static const ThreadAffinity Affinity = AnyThread;
};

template<typename T>
struct DefaultThreadingTrait<T, true> {
    static const ThreadAffinity Affinity = MainThreadOnly;
};

template<typename T>
struct ThreadingTrait {
    static const ThreadAffinity Affinity = DefaultThreadingTrait<T>::Affinity;
};

// Marks the specified class as being used from multiple threads. When
// a class is used from multiple threads we go through thread local
// storage to get the heap in which to allocate an object of that type
// and when allocating a Persistent handle for an object with that
// type. Notice that marking the base class does not automatically
// mark its descendants and they have to be explicitly marked.
#define USED_FROM_MULTIPLE_THREADS(Class)                 \
    class Class;                                          \
    template<> struct ThreadingTrait<Class> {             \
        static const ThreadAffinity Affinity = AnyThread; \
    }

#define USED_FROM_MULTIPLE_THREADS_NAMESPACE(Namespace, Class)          \
    namespace Namespace {                                               \
        class Class;                                                    \
    }                                                                   \
    namespace WebCore {                                                 \
        template<> struct ThreadingTrait<Namespace::Class> {            \
            static const ThreadAffinity Affinity = AnyThread;           \
        };                                                              \
    }

template<typename U> class ThreadingTrait<const U> : public ThreadingTrait<U> { };

// List of typed heaps. The list is used to generate the implementation
// of typed heap related methods.
//
// To create a new typed heap add a H(<ClassName>) to the
// FOR_EACH_TYPED_HEAP macro below.
// FIXME: When the Node hierarchy has been moved use Node in our
// tests instead of TestTypedHeapClass.
#define FOR_EACH_TYPED_HEAP(H)  \
    H(TestTypedHeapClass)
//    H(Node)

#define TypedHeapEnumName(Type) Type##Heap,

enum TypedHeaps {
    GeneralHeap,
    FOR_EACH_TYPED_HEAP(TypedHeapEnumName)
    NumberOfHeaps
};

// Trait to give an index in the thread state to all the
// type-specialized heaps. The general heap is at index 0 in the
// thread state. The index for other type-specialized heaps are given
// by the TypedHeaps enum above.
template<typename T>
struct HeapTrait {
    static const int index = GeneralHeap;
    typedef ThreadHeap<FinalizedHeapObjectHeader> HeapType;
};

#define DEFINE_HEAP_INDEX_TRAIT(Type)                  \
    class Type;                                        \
    template<>                                         \
    struct HeapTrait<class Type> {                     \
        static const int index = Type##Heap;           \
        typedef ThreadHeap<HeapObjectHeader> HeapType; \
    };

FOR_EACH_TYPED_HEAP(DEFINE_HEAP_INDEX_TRAIT)

// A HeapStats structure keeps track of the amount of memory allocated
// for a Blink heap and how much of that memory is used for actual
// Blink objects. These stats are used in the heuristics to determine
// when to perform garbage collections.
class HeapStats {
public:
    size_t totalObjectSpace() const { return m_totalObjectSpace; }
    size_t totalAllocatedSpace() const { return m_totalAllocatedSpace; }

    void add(HeapStats* other)
    {
        m_totalObjectSpace += other->m_totalObjectSpace;
        m_totalAllocatedSpace += other->m_totalAllocatedSpace;
    }

    void inline increaseObjectSpace(size_t newObjectSpace)
    {
        m_totalObjectSpace += newObjectSpace;
    }

    void inline decreaseObjectSpace(size_t deadObjectSpace)
    {
        m_totalObjectSpace -= deadObjectSpace;
    }

    void inline increaseAllocatedSpace(size_t newAllocatedSpace)
    {
        m_totalAllocatedSpace += newAllocatedSpace;
    }

    void inline decreaseAllocatedSpace(size_t deadAllocatedSpace)
    {
        m_totalAllocatedSpace -= deadAllocatedSpace;
    }

    void clear()
    {
        m_totalObjectSpace = 0;
        m_totalAllocatedSpace = 0;
    }

    bool operator==(const HeapStats& other)
    {
        return m_totalAllocatedSpace == other.m_totalAllocatedSpace
            && m_totalObjectSpace == other.m_totalObjectSpace;
    }

private:
    size_t m_totalObjectSpace; // Actually contains objects that may be live, not including headers.
    size_t m_totalAllocatedSpace; // Allocated from the OS.

    friend class HeapTester;
};

class HEAP_EXPORT ThreadState {
    WTF_MAKE_NONCOPYABLE(ThreadState);
public:
    // When garbage collecting we need to know whether or not there
    // can be pointers to Blink GC managed objects on the stack for
    // each thread. When threads reach a safe point they record
    // whether or not they have pointers on the stack.
    enum StackState {
        NoHeapPointersOnStack,
        HeapPointersOnStack
    };

    // The set of ThreadStates for all threads attached to the Blink
    // garbage collector.
    typedef HashSet<ThreadState*> AttachedThreadStateSet;
    static AttachedThreadStateSet& attachedThreads();

    // Initialize threading infrastructure. Should be called from the main
    // thread.
    static void init();
    static void shutdown();

    // Trace all GC roots, called when marking the managed heap objects.
    static void visitRoots(Visitor*);

    // Associate ThreadState object with the current thread. After this
    // call thread can start using the garbage collected heap infrastructure.
    // It also has to periodically check for safepoints.
    static void attach();

    // Disassociate attached ThreadState from the current thread. The thread
    // can no longer use the garbage collected heap after this call.
    static void detach();

    static ThreadState* current() { return **s_threadSpecific; }
    static ThreadState* mainThreadState()
    {
        return reinterpret_cast<ThreadState*>(s_mainThreadStateStorage);
    }

    static bool isMainThread() { return current() == mainThreadState(); }

    inline bool checkThread() const
    {
        ASSERT(m_thread == currentThread());
        return true;
    }

    // shouldGC and shouldForceConservativeGC implement the heuristics
    // that are used to determine when to collect garbage. If
    // shouldForceConservativeGC returns true, we force the garbage
    // collection immediately. Otherwise, if shouldGC returns true, we
    // record that we should garbage collect the next time we return
    // to the event loop. If both return false, we don't need to
    // collect garbage at this point.
    bool shouldGC();
    bool shouldForceConservativeGC();

    // If gcRequested returns true when a thread returns to its event
    // loop the thread will initiate a garbage collection.
    bool gcRequested();
    void setGCRequested();
    void clearGCRequested();

    bool sweepRequested();
    void setSweepRequested();
    void clearSweepRequested();
    void performPendingSweep();

    // Support for disallowing allocation. Mainly used for sanity
    // checks asserts.
    bool isAllocationAllowed() const { return !isAtSafePoint() && !m_noAllocationCount; }
    void enterNoAllocationScope() { m_noAllocationCount++; }
    void leaveNoAllocationScope() { m_noAllocationCount--; }

    // Before performing GC the thread-specific heap state should be
    // made consistent for garbage collection.
    bool isConsistentForGC();
    void makeConsistentForGC();

    // Is the thread corresponding to this thread state currently
    // performing GC?
    bool isInGC() const { return m_inGC; }

    // Is any of the threads registered with the blink garbage collection
    // infrastructure currently perform GC?
    static bool isAnyThreadInGC() { return s_inGC; }

    void enterGC()
    {
        ASSERT(!m_inGC);
        ASSERT(!s_inGC);
        m_inGC = true;
        s_inGC = true;
    }

    void leaveGC()
    {
        m_inGC = false;
        s_inGC = false;
    }

    // Is the thread corresponding to this thread state currently
    // sweeping?
    bool isSweepInProgress() const { return m_sweepInProgress; }

    void prepareForGC();

    // Safepoint related functionality.
    //
    // When a thread attempts to perform GC it needs to stop all other threads
    // that use the heap or at least guarantee that they will not touch any
    // heap allocated object until GC is complete.
    //
    // We say that a thread is at a safepoint if this thread is guaranteed to
    // not touch any heap allocated object or any heap related functionality until
    // it leaves the safepoint.
    //
    // Notice that a thread does not have to be paused if it is at safepoint it
    // can continue to run and perform tasks that do not require interaction
    // with the heap. It will be paused if it attempts to leave the safepoint and
    // there is a GC in progress.
    //
    // Each thread that has ThreadState attached must:
    //   - periodically check if GC is requested from another thread by calling a safePoint() method;
    //   - use SafePointScope around long running loops that have no safePoint() invocation inside,
    //     such loops must not touch any heap object;
    //   - register an Interruptor that can interrupt long running loops that have no calls to safePoint and
    //     are not wrapped in a SafePointScope (e.g. Interruptor for JavaScript code)
    //

    // Request all other threads to stop. Must only be called if the current thread is at safepoint.
    static void stopThreads();
    static void resumeThreads();

    // Check if GC is requested by another thread and pause this thread if this is the case.
    // Can only be called when current thread is in a consistent state.
    void safePoint(StackState);

    // Mark current thread as running inside safepoint.
    void enterSafePointWithoutPointers() { enterSafePoint(NoHeapPointersOnStack, 0); }
    void enterSafePointWithPointers(void* scopeMarker) { enterSafePoint(HeapPointersOnStack, scopeMarker); }
    void leaveSafePoint();
    bool isAtSafePoint() const { return m_atSafePoint; }

    class SafePointScope {
    public:
        enum ScopeNesting {
            NoNesting,
            AllowNesting
        };

        explicit SafePointScope(StackState stackState, ScopeNesting nesting = NoNesting)
            : m_state(ThreadState::current())
        {
            if (m_state->isAtSafePoint()) {
                RELEASE_ASSERT(nesting == AllowNesting);
                // We can ignore stackState because there should be no heap object
                // pointers manipulation after outermost safepoint was entered.
                m_state = 0;
            } else {
                m_state->enterSafePoint(stackState, this);
            }
        }

        ~SafePointScope()
        {
            if (m_state)
                m_state->leaveSafePoint();
        }

    private:
        ThreadState* m_state;
    };

    // If attached thread enters long running loop that can call back
    // into Blink and leaving and reentering safepoint at every
    // transition between this loop and Blink is deemed too expensive
    // then instead of marking this loop as a GC safepoint thread
    // can provide an interruptor object which would allow GC
    // to temporarily interrupt and pause this long running loop at
    // an arbitrary moment creating a safepoint for a GC.
    class HEAP_EXPORT Interruptor {
    public:
        virtual ~Interruptor() { }

        // Request the interruptor to interrupt the thread and
        // call onInterrupted on that thread once interruption
        // succeeds.
        virtual void requestInterrupt() = 0;

        // Clear previous interrupt request.
        virtual void clearInterrupt() = 0;

    protected:
        // This method is called on the interrupted thread to
        // create a safepoint for a GC.
        void onInterrupted();
    };

    void addInterruptor(Interruptor*);
    void removeInterruptor(Interruptor*);

    // CleanupTasks are executed when ThreadState performs
    // cleanup before detaching.
    class CleanupTask {
    public:
        virtual ~CleanupTask() { }

        // Executed before the final GC.
        virtual void preCleanup() { }

        // Executed after the final GC. Thread heap is empty at this point.
        virtual void postCleanup() { }
    };

    void addCleanupTask(PassOwnPtr<CleanupTask> cleanupTask)
    {
        m_cleanupTasks.append(cleanupTask);
    }

    // Should only be called under protection of threadAttachMutex().
    const Vector<Interruptor*>& interruptors() const { return m_interruptors; }

    void recordStackEnd(intptr_t* endOfStack)
    {
        m_endOfStack = endOfStack;
    }

    // Get one of the heap structures for this thread.
    //
    // The heap is split into multiple heap parts based on object
    // types. To get the index for a given type, use
    // HeapTrait<Type>::index.
    BaseHeap* heap(int index) const { return m_heaps[index]; }

    // Infrastructure to determine if an address is within one of the
    // address ranges for the Blink heap. If the address is in the Blink
    // heap the containing heap page is returned.
    HeapContainsCache* heapContainsCache() { return m_heapContainsCache.get(); }
    BaseHeapPage* contains(Address);
    BaseHeapPage* contains(void* pointer) { return contains(reinterpret_cast<Address>(pointer)); }
    BaseHeapPage* contains(const void* pointer) { return contains(const_cast<void*>(pointer)); }

    // List of persistent roots allocated on the given thread.
    PersistentNode* roots() const { return m_persistents.get(); }

    // List of global persistent roots not owned by any particular thread.
    // globalRootsMutex must be acquired before any modifications.
    static PersistentNode* globalRoots();
    static Mutex& globalRootsMutex();

    // Visit local thread stack and trace all pointers conservatively.
    void visitStack(Visitor*);

    // Visit all persistents allocated on this thread.
    void visitPersistents(Visitor*);

    // Checks a given address and if a pointer into the oilpan heap marks
    // the object to which it points.
    bool checkAndMarkPointer(Visitor*, Address);

    void pushWeakObjectPointerCallback(void*, WeakPointerCallback);
    bool popAndInvokeWeakPointerCallback(Visitor*);

    void getStats(HeapStats&);
    HeapStats& stats() { return m_stats; }
    HeapStats& statsAfterLastGC() { return m_statsAfterLastGC; }

private:
    explicit ThreadState();
    ~ThreadState();

    friend class SafePointBarrier;

    void enterSafePoint(StackState, void*);
    NO_SANITIZE_ADDRESS void copyStackUntilSafePointScope();
    void clearSafePointScopeMarker()
    {
        m_safePointStackCopy.clear();
        m_safePointScopeMarker = 0;
    }

    // Finds the Blink HeapPage in this thread-specific heap
    // corresponding to a given address. Return 0 if the address is
    // not contained in any of the pages. This does not consider
    // large objects.
    BaseHeapPage* heapPageFromAddress(Address);

    // When ThreadState is detaching from non-main thread its
    // heap is expected to be empty (because it is going away).
    // Perform registered cleanup tasks and garbage collection
    // to sweep away any objects that are left on this heap.
    // We assert that nothing must remain after this cleanup.
    // If assertion does not hold we crash as we are potentially
    // in the dangling pointer situation.
    void cleanup();

    static WTF::ThreadSpecific<ThreadState*>* s_threadSpecific;
    static SafePointBarrier* s_safePointBarrier;

    // This variable is flipped to true after all threads are stoped
    // and outermost GC has started.
    static bool s_inGC;

    // We can't create a static member of type ThreadState here
    // because it will introduce global constructor and destructor.
    // We would like to manage lifetime of the ThreadState attached
    // to the main thread explicitly instead and still use normal
    // constructor and destructor for the ThreadState class.
    // For this we reserve static storage for the main ThreadState
    // and lazily construct ThreadState in it using placement new.
    static uint8_t s_mainThreadStateStorage[];

    void trace(Visitor*);

    ThreadIdentifier m_thread;
    OwnPtr<PersistentNode> m_persistents;
    StackState m_stackState;
    intptr_t* m_startOfStack;
    intptr_t* m_endOfStack;
    void* m_safePointScopeMarker;
    Vector<Address> m_safePointStackCopy;
    bool m_atSafePoint;
    Vector<Interruptor*> m_interruptors;
    bool m_gcRequested;
    volatile int m_sweepRequested;
    bool m_sweepInProgress;
    size_t m_noAllocationCount;
    bool m_inGC;
    BaseHeap* m_heaps[NumberOfHeaps];
    OwnPtr<HeapContainsCache> m_heapContainsCache;
    HeapStats m_stats;
    HeapStats m_statsAfterLastGC;

    Vector<OwnPtr<CleanupTask> > m_cleanupTasks;
    bool m_isCleaningUp;

    CallbackStack* m_weakCallbackStack;
};

template<ThreadAffinity affinity> class ThreadStateFor;

template<> class ThreadStateFor<MainThreadOnly> {
public:
    static ThreadState* state()
    {
        // This specialization must only be used from the main thread.
        ASSERT(ThreadState::isMainThread());
        return ThreadState::mainThreadState();
    }
};

template<> class ThreadStateFor<AnyThread> {
public:
    static ThreadState* state() { return ThreadState::current(); }
};

}

#endif // ThreadState_h
