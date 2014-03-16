/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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
#include "core/dom/MutationObserver.h"

#include <algorithm>
#include "bindings/v8/Dictionary.h"
#include "bindings/v8/ExceptionState.h"
#include "core/dom/Document.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/Microtask.h"
#include "core/dom/MutationCallback.h"
#include "core/dom/MutationObserverRegistration.h"
#include "core/dom/MutationRecord.h"
#include "core/dom/Node.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "wtf/MainThread.h"

namespace WebCore {

static unsigned s_observerPriority = 0;

struct MutationObserver::ObserverLessThan {
    bool operator()(const RefPtr<MutationObserver>& lhs, const RefPtr<MutationObserver>& rhs)
    {
        return lhs->m_priority < rhs->m_priority;
    }
};

PassRefPtr<MutationObserver> MutationObserver::create(PassOwnPtr<MutationCallback> callback)
{
    ASSERT(isMainThread());
    return adoptRef(new MutationObserver(callback));
}

MutationObserver::MutationObserver(PassOwnPtr<MutationCallback> callback)
    : m_callback(callback)
    , m_priority(s_observerPriority++)
{
    ScriptWrappable::init(this);
}

MutationObserver::~MutationObserver()
{
    ASSERT(m_registrations.isEmpty());
    if (!m_records.isEmpty())
        InspectorInstrumentation::didClearAllMutationRecords(m_callback->executionContext(), this);
}

void MutationObserver::observe(Node* node, const Dictionary& optionsDictionary, ExceptionState& exceptionState)
{
    if (!node) {
        exceptionState.throwDOMException(NotFoundError, "The provided node was null.");
        return;
    }

    MutationObserverOptions options = 0;

    bool attributeOldValue = false;
    bool attributeOldValuePresent = optionsDictionary.get("attributeOldValue", attributeOldValue);
    if (attributeOldValue)
        options |= AttributeOldValue;

    HashSet<AtomicString> attributeFilter;
    bool attributeFilterPresent = optionsDictionary.get("attributeFilter", attributeFilter);
    if (attributeFilterPresent)
        options |= AttributeFilter;

    bool attributes = false;
    bool attributesPresent = optionsDictionary.get("attributes", attributes);
    if (attributes || (!attributesPresent && (attributeOldValuePresent || attributeFilterPresent)))
        options |= Attributes;

    bool characterDataOldValue = false;
    bool characterDataOldValuePresent = optionsDictionary.get("characterDataOldValue", characterDataOldValue);
    if (characterDataOldValue)
        options |= CharacterDataOldValue;

    bool characterData = false;
    bool characterDataPresent = optionsDictionary.get("characterData", characterData);
    if (characterData || (!characterDataPresent && characterDataOldValuePresent))
        options |= CharacterData;

    bool childListValue = false;
    if (optionsDictionary.get("childList", childListValue) && childListValue)
        options |= ChildList;

    bool subtreeValue = false;
    if (optionsDictionary.get("subtree", subtreeValue) && subtreeValue)
        options |= Subtree;

    if (!(options & Attributes)) {
        if (options & AttributeOldValue) {
            exceptionState.throwTypeError("The options object may only set 'attributeOldValue' to true when 'attributes' is true or not present.");
            return;
        }
        if (options & AttributeFilter) {
            exceptionState.throwTypeError("The options object may only set 'attributeFilter' when 'attributes' is true or not present.");
            return;
        }
    }
    if (!((options & CharacterData) || !(options & CharacterDataOldValue))) {
        exceptionState.throwTypeError("The options object may only set 'characterDataOldValue' to true when 'characterData' is true or not present.");
        return;
    }

    if (!(options & (Attributes | CharacterData | ChildList))) {
        exceptionState.throwTypeError("The options object must set at least one of 'attributes', 'characterData', or 'childList' to true.");
        return;
    }

    node->registerMutationObserver(*this, options, attributeFilter);
}

Vector<RefPtr<MutationRecord> > MutationObserver::takeRecords()
{
    Vector<RefPtr<MutationRecord> > records;
    records.swap(m_records);
    InspectorInstrumentation::didClearAllMutationRecords(m_callback->executionContext(), this);
    return records;
}

void MutationObserver::disconnect()
{
    m_records.clear();
    InspectorInstrumentation::didClearAllMutationRecords(m_callback->executionContext(), this);
    HashSet<MutationObserverRegistration*> registrations(m_registrations);
    for (HashSet<MutationObserverRegistration*>::iterator iter = registrations.begin(); iter != registrations.end(); ++iter)
        (*iter)->unregister();
}

void MutationObserver::observationStarted(MutationObserverRegistration* registration)
{
    ASSERT(!m_registrations.contains(registration));
    m_registrations.add(registration);
}

void MutationObserver::observationEnded(MutationObserverRegistration* registration)
{
    ASSERT(m_registrations.contains(registration));
    m_registrations.remove(registration);
}

typedef HashSet<RefPtr<MutationObserver> > MutationObserverSet;

static MutationObserverSet& activeMutationObservers()
{
    DEFINE_STATIC_LOCAL(MutationObserverSet, activeObservers, ());
    return activeObservers;
}

static MutationObserverSet& suspendedMutationObservers()
{
    DEFINE_STATIC_LOCAL(MutationObserverSet, suspendedObservers, ());
    return suspendedObservers;
}

static void activateObserver(PassRefPtr<MutationObserver> observer)
{
    if (activeMutationObservers().isEmpty())
        Microtask::enqueueMicrotask(&MutationObserver::deliverMutations);

    activeMutationObservers().add(observer);
}

void MutationObserver::enqueueMutationRecord(PassRefPtr<MutationRecord> mutation)
{
    ASSERT(isMainThread());
    m_records.append(mutation);
    activateObserver(this);
    InspectorInstrumentation::didEnqueueMutationRecord(m_callback->executionContext(), this);
}

void MutationObserver::setHasTransientRegistration()
{
    ASSERT(isMainThread());
    activateObserver(this);
}

HashSet<Node*> MutationObserver::getObservedNodes() const
{
    HashSet<Node*> observedNodes;
    for (HashSet<MutationObserverRegistration*>::const_iterator iter = m_registrations.begin(); iter != m_registrations.end(); ++iter)
        (*iter)->addRegistrationNodesToSet(observedNodes);
    return observedNodes;
}

bool MutationObserver::canDeliver()
{
    return !m_callback->executionContext()->activeDOMObjectsAreSuspended();
}

void MutationObserver::deliver()
{
    ASSERT(canDeliver());

    // Calling clearTransientRegistrations() can modify m_registrations, so it's necessary
    // to make a copy of the transient registrations before operating on them.
    Vector<MutationObserverRegistration*, 1> transientRegistrations;
    for (HashSet<MutationObserverRegistration*>::iterator iter = m_registrations.begin(); iter != m_registrations.end(); ++iter) {
        if ((*iter)->hasTransientRegistrations())
            transientRegistrations.append(*iter);
    }
    for (size_t i = 0; i < transientRegistrations.size(); ++i)
        transientRegistrations[i]->clearTransientRegistrations();

    if (m_records.isEmpty())
        return;

    Vector<RefPtr<MutationRecord> > records;
    records.swap(m_records);

    InspectorInstrumentation::willDeliverMutationRecords(m_callback->executionContext(), this);
    m_callback->call(records, this);
    InspectorInstrumentation::didDeliverMutationRecords(m_callback->executionContext());
}

void MutationObserver::resumeSuspendedObservers()
{
    ASSERT(isMainThread());
    if (suspendedMutationObservers().isEmpty())
        return;

    Vector<RefPtr<MutationObserver> > suspended;
    copyToVector(suspendedMutationObservers(), suspended);
    for (size_t i = 0; i < suspended.size(); ++i) {
        if (suspended[i]->canDeliver()) {
            suspendedMutationObservers().remove(suspended[i]);
            activateObserver(suspended[i]);
        }
    }
}

void MutationObserver::deliverMutations()
{
    ASSERT(isMainThread());
    Vector<RefPtr<MutationObserver> > observers;
    copyToVector(activeMutationObservers(), observers);
    activeMutationObservers().clear();
    std::sort(observers.begin(), observers.end(), ObserverLessThan());
    for (size_t i = 0; i < observers.size(); ++i) {
        if (observers[i]->canDeliver())
            observers[i]->deliver();
        else
            suspendedMutationObservers().add(observers[i]);
    }
}

} // namespace WebCore
