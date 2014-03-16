/*
 * Copyright (C) 2008, 2009, 2010, 2011 Apple Inc. All Rights Reserved.
 * Copyright (C) 2009 Torch Mobile, Inc.
 * Copyright 2010, The Android Open Source Project
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "modules/geolocation/Geolocation.h"

#include "core/dom/Document.h"
#include "wtf/CurrentTime.h"

#include "modules/geolocation/Coordinates.h"
#include "modules/geolocation/GeolocationController.h"
#include "modules/geolocation/GeolocationError.h"
#include "modules/geolocation/GeolocationPosition.h"

namespace WebCore {

static const char permissionDeniedErrorMessage[] = "User denied Geolocation";
static const char failedToStartServiceErrorMessage[] = "Failed to start Geolocation service";
static const char framelessDocumentErrorMessage[] = "Geolocation cannot be used in frameless documents";

static PassRefPtrWillBeRawPtr<Geoposition> createGeoposition(GeolocationPosition* position)
{
    if (!position)
        return nullptr;

    RefPtrWillBeRawPtr<Coordinates> coordinates = Coordinates::create(
        position->latitude(),
        position->longitude(),
        position->canProvideAltitude(),
        position->altitude(),
        position->accuracy(),
        position->canProvideAltitudeAccuracy(),
        position->altitudeAccuracy(),
        position->canProvideHeading(),
        position->heading(),
        position->canProvideSpeed(),
        position->speed());
    return Geoposition::create(coordinates.release(), convertSecondsToDOMTimeStamp(position->timestamp()));
}

static PassRefPtrWillBeRawPtr<PositionError> createPositionError(GeolocationError* error)
{
    PositionError::ErrorCode code = PositionError::POSITION_UNAVAILABLE;
    switch (error->code()) {
    case GeolocationError::PermissionDenied:
        code = PositionError::PERMISSION_DENIED;
        break;
    case GeolocationError::PositionUnavailable:
        code = PositionError::POSITION_UNAVAILABLE;
        break;
    }

    return PositionError::create(code, error->message());
}

Geolocation::GeoNotifier::GeoNotifier(Geolocation* geolocation, PassOwnPtr<PositionCallback> successCallback, PassOwnPtr<PositionErrorCallback> errorCallback, PassRefPtrWillBeRawPtr<PositionOptions> options)
    : m_geolocation(geolocation)
    , m_successCallback(successCallback)
    , m_errorCallback(errorCallback)
    , m_options(options)
    , m_timer(this, &Geolocation::GeoNotifier::timerFired)
    , m_useCachedPosition(false)
{
    ASSERT(m_geolocation);
    ASSERT(m_successCallback);
    // If no options were supplied from JS, we should have created a default set
    // of options in JSGeolocationCustom.cpp.
    ASSERT(m_options);
}

void Geolocation::GeoNotifier::trace(Visitor* visitor)
{
    visitor->trace(m_geolocation);
    visitor->trace(m_options);
    visitor->trace(m_fatalError);
}

void Geolocation::GeoNotifier::setFatalError(PassRefPtrWillBeRawPtr<PositionError> error)
{
    // If a fatal error has already been set, stick with it. This makes sure that
    // when permission is denied, this is the error reported, as required by the
    // spec.
    if (m_fatalError)
        return;

    m_fatalError = error;
    // An existing timer may not have a zero timeout.
    m_timer.stop();
    m_timer.startOneShot(0, FROM_HERE);
}

void Geolocation::GeoNotifier::setUseCachedPosition()
{
    m_useCachedPosition = true;
    m_timer.startOneShot(0, FROM_HERE);
}

bool Geolocation::GeoNotifier::hasZeroTimeout() const
{
    return m_options->hasTimeout() && m_options->timeout() == 0;
}

void Geolocation::GeoNotifier::runSuccessCallback(Geoposition* position)
{
    // If we are here and the Geolocation permission is not approved, something has
    // gone horribly wrong.
    if (!m_geolocation->isAllowed())
        CRASH();

    m_successCallback->handleEvent(position);
}

void Geolocation::GeoNotifier::runErrorCallback(PositionError* error)
{
    if (m_errorCallback)
        m_errorCallback->handleEvent(error);
}

void Geolocation::GeoNotifier::startTimerIfNeeded()
{
    if (m_options->hasTimeout())
        m_timer.startOneShot(m_options->timeout() / 1000.0, FROM_HERE);
}

void Geolocation::GeoNotifier::stopTimer()
{
    m_timer.stop();
}

void Geolocation::GeoNotifier::timerFired(Timer<GeoNotifier>*)
{
    m_timer.stop();

    // Protect this GeoNotifier object, since it
    // could be deleted by a call to clearWatch in a callback.
    RefPtrWillBeRawPtr<GeoNotifier> protect(this);

    // Test for fatal error first. This is required for the case where the LocalFrame is
    // disconnected and requests are cancelled.
    if (m_fatalError) {
        runErrorCallback(m_fatalError.get());
        // This will cause this notifier to be deleted.
        m_geolocation->fatalErrorOccurred(this);
        return;
    }

    if (m_useCachedPosition) {
        // Clear the cached position flag in case this is a watch request, which
        // will continue to run.
        m_useCachedPosition = false;
        m_geolocation->requestUsesCachedPosition(this);
        return;
    }

    if (m_errorCallback) {
        RefPtrWillBeRawPtr<PositionError> error = PositionError::create(PositionError::TIMEOUT, "Timeout expired");
        m_errorCallback->handleEvent(error.get());
    }
    m_geolocation->requestTimedOut(this);
}

void Geolocation::Watchers::trace(Visitor* visitor)
{
    visitor->trace(m_idToNotifierMap);
    visitor->trace(m_notifierToIdMap);
}

bool Geolocation::Watchers::add(int id, PassRefPtrWillBeRawPtr<GeoNotifier> prpNotifier)
{
    ASSERT(id > 0);
    RefPtrWillBeRawPtr<GeoNotifier> notifier = prpNotifier;

    if (!m_idToNotifierMap.add(id, notifier.get()).isNewEntry)
        return false;
    m_notifierToIdMap.set(notifier.release(), id);
    return true;
}

Geolocation::GeoNotifier* Geolocation::Watchers::find(int id)
{
    ASSERT(id > 0);
    IdToNotifierMap::iterator iter = m_idToNotifierMap.find(id);
    if (iter == m_idToNotifierMap.end())
        return 0;
    return iter->value.get();
}

void Geolocation::Watchers::remove(int id)
{
    ASSERT(id > 0);
    IdToNotifierMap::iterator iter = m_idToNotifierMap.find(id);
    if (iter == m_idToNotifierMap.end())
        return;
    m_notifierToIdMap.remove(iter->value);
    m_idToNotifierMap.remove(iter);
}

void Geolocation::Watchers::remove(GeoNotifier* notifier)
{
    NotifierToIdMap::iterator iter = m_notifierToIdMap.find(notifier);
    if (iter == m_notifierToIdMap.end())
        return;
    m_idToNotifierMap.remove(iter->value);
    m_notifierToIdMap.remove(iter);
}

bool Geolocation::Watchers::contains(GeoNotifier* notifier) const
{
    return m_notifierToIdMap.contains(notifier);
}

void Geolocation::Watchers::clear()
{
    m_idToNotifierMap.clear();
    m_notifierToIdMap.clear();
}

bool Geolocation::Watchers::isEmpty() const
{
    return m_idToNotifierMap.isEmpty();
}

void Geolocation::Watchers::getNotifiersVector(GeoNotifierVector& copy) const
{
    copyValuesToVector(m_idToNotifierMap, copy);
}

PassRefPtrWillBeRawPtr<Geolocation> Geolocation::create(ExecutionContext* context)
{
    RefPtrWillBeRawPtr<Geolocation> geolocation = adoptRefWillBeNoop(new Geolocation(context));
    geolocation->suspendIfNeeded();
    return geolocation.release();
}

Geolocation::Geolocation(ExecutionContext* context)
    : ActiveDOMObject(context)
    , m_allowGeolocation(Unknown)
{
    ScriptWrappable::init(this);
}

Geolocation::~Geolocation()
{
    ASSERT(m_allowGeolocation != InProgress);
}

void Geolocation::trace(Visitor* visitor)
{
    visitor->trace(m_oneShots);
    visitor->trace(m_watchers);
    visitor->trace(m_pendingForPermissionNotifiers);
    visitor->trace(m_lastPosition);
    visitor->trace(m_requestsAwaitingCachedPosition);
}

Document* Geolocation::document() const
{
    return toDocument(executionContext());
}

LocalFrame* Geolocation::frame() const
{
    return document() ? document()->frame() : 0;
}

Page* Geolocation::page() const
{
    return document() ? document()->page() : 0;
}

void Geolocation::stop()
{
    Page* page = this->page();
    if (page && m_allowGeolocation == InProgress)
        GeolocationController::from(page)->cancelPermissionRequest(this);
    // The frame may be moving to a new page and we want to get the permissions from the new page's client.
    m_allowGeolocation = Unknown;
    cancelAllRequests();
    stopUpdating();
    m_pendingForPermissionNotifiers.clear();
}

Geoposition* Geolocation::lastPosition()
{
    Page* page = this->page();
    if (!page)
        return 0;

    m_lastPosition = createGeoposition(GeolocationController::from(page)->lastPosition());

    return m_lastPosition.get();
}

void Geolocation::getCurrentPosition(PassOwnPtr<PositionCallback> successCallback, PassOwnPtr<PositionErrorCallback> errorCallback, PassRefPtrWillBeRawPtr<PositionOptions> options)
{
    if (!frame())
        return;

    RefPtrWillBeRawPtr<GeoNotifier> notifier = GeoNotifier::create(this, successCallback, errorCallback, options);
    startRequest(notifier.get());

    m_oneShots.add(notifier);
}

int Geolocation::watchPosition(PassOwnPtr<PositionCallback> successCallback, PassOwnPtr<PositionErrorCallback> errorCallback, PassRefPtrWillBeRawPtr<PositionOptions> options)
{
    if (!frame())
        return 0;

    RefPtrWillBeRawPtr<GeoNotifier> notifier = GeoNotifier::create(this, successCallback, errorCallback, options);
    startRequest(notifier.get());

    int watchID;
    // Keep asking for the next id until we're given one that we don't already have.
    do {
        watchID = executionContext()->circularSequentialID();
    } while (!m_watchers.add(watchID, notifier));
    return watchID;
}

void Geolocation::startRequest(GeoNotifier *notifier)
{
    // Check whether permissions have already been denied. Note that if this is the case,
    // the permission state can not change again in the lifetime of this page.
    if (isDenied())
        notifier->setFatalError(PositionError::create(PositionError::PERMISSION_DENIED, permissionDeniedErrorMessage));
    else if (haveSuitableCachedPosition(notifier->options()))
        notifier->setUseCachedPosition();
    else if (notifier->hasZeroTimeout())
        notifier->startTimerIfNeeded();
    else if (!isAllowed()) {
        // if we don't yet have permission, request for permission before calling startUpdating()
        m_pendingForPermissionNotifiers.add(notifier);
        requestPermission();
    } else if (startUpdating(notifier))
        notifier->startTimerIfNeeded();
    else
        notifier->setFatalError(PositionError::create(PositionError::POSITION_UNAVAILABLE, failedToStartServiceErrorMessage));
}

void Geolocation::fatalErrorOccurred(Geolocation::GeoNotifier* notifier)
{
    // This request has failed fatally. Remove it from our lists.
    m_oneShots.remove(notifier);
    m_watchers.remove(notifier);

    if (!hasListeners())
        stopUpdating();
}

void Geolocation::requestUsesCachedPosition(GeoNotifier* notifier)
{
    // This is called asynchronously, so the permissions could have been denied
    // since we last checked in startRequest.
    if (isDenied()) {
        notifier->setFatalError(PositionError::create(PositionError::PERMISSION_DENIED, permissionDeniedErrorMessage));
        return;
    }

    m_requestsAwaitingCachedPosition.add(notifier);

    // If permissions are allowed, make the callback
    if (isAllowed()) {
        makeCachedPositionCallbacks();
        return;
    }

    // Request permissions, which may be synchronous or asynchronous.
    requestPermission();
}

void Geolocation::makeCachedPositionCallbacks()
{
    // All modifications to m_requestsAwaitingCachedPosition are done
    // asynchronously, so we don't need to worry about it being modified from
    // the callbacks.
    GeoNotifierSet::const_iterator end = m_requestsAwaitingCachedPosition.end();
    for (GeoNotifierSet::const_iterator iter = m_requestsAwaitingCachedPosition.begin(); iter != end; ++iter) {
        GeoNotifier* notifier = iter->get();
        notifier->runSuccessCallback(lastPosition());

        // If this is a one-shot request, stop it. Otherwise, if the watch still
        // exists, start the service to get updates.
        if (m_oneShots.contains(notifier))
            m_oneShots.remove(notifier);
        else if (m_watchers.contains(notifier)) {
            if (notifier->hasZeroTimeout() || startUpdating(notifier))
                notifier->startTimerIfNeeded();
            else
                notifier->setFatalError(PositionError::create(PositionError::POSITION_UNAVAILABLE, failedToStartServiceErrorMessage));
        }
    }

    m_requestsAwaitingCachedPosition.clear();

    if (!hasListeners())
        stopUpdating();
}

void Geolocation::requestTimedOut(GeoNotifier* notifier)
{
    // If this is a one-shot request, stop it.
    m_oneShots.remove(notifier);

    if (!hasListeners())
        stopUpdating();
}

bool Geolocation::haveSuitableCachedPosition(PositionOptions* options)
{
    Geoposition* cachedPosition = lastPosition();
    if (!cachedPosition)
        return false;
    if (!options->hasMaximumAge())
        return true;
    if (!options->maximumAge())
        return false;
    DOMTimeStamp currentTimeMillis = convertSecondsToDOMTimeStamp(currentTime());
    return cachedPosition->timestamp() > currentTimeMillis - options->maximumAge();
}

void Geolocation::clearWatch(int watchID)
{
    if (watchID <= 0)
        return;

    if (GeoNotifier* notifier = m_watchers.find(watchID))
        m_pendingForPermissionNotifiers.remove(notifier);
    m_watchers.remove(watchID);

    if (!hasListeners())
        stopUpdating();
}

void Geolocation::setIsAllowed(bool allowed)
{
    // Protect the Geolocation object from garbage collection during a callback.
    RefPtrWillBeRawPtr<Geolocation> protect(this);

    // This may be due to either a new position from the service, or a cached
    // position.
    m_allowGeolocation = allowed ? Yes : No;

    // Permission request was made during the startRequest process
    if (!m_pendingForPermissionNotifiers.isEmpty()) {
        handlePendingPermissionNotifiers();
        m_pendingForPermissionNotifiers.clear();
        return;
    }

    if (!isAllowed()) {
        RefPtrWillBeRawPtr<PositionError> error = PositionError::create(PositionError::PERMISSION_DENIED, permissionDeniedErrorMessage);
        error->setIsFatal(true);
        handleError(error.get());
        m_requestsAwaitingCachedPosition.clear();
        return;
    }

    // If the service has a last position, use it to call back for all requests.
    // If any of the requests are waiting for permission for a cached position,
    // the position from the service will be at least as fresh.
    if (lastPosition())
        makeSuccessCallbacks();
    else
        makeCachedPositionCallbacks();
}

void Geolocation::sendError(GeoNotifierVector& notifiers, PositionError* error)
{
    GeoNotifierVector::const_iterator end = notifiers.end();
    for (GeoNotifierVector::const_iterator it = notifiers.begin(); it != end; ++it)
        (*it)->runErrorCallback(error);
}

void Geolocation::sendPosition(GeoNotifierVector& notifiers, Geoposition* position)
{
    GeoNotifierVector::const_iterator end = notifiers.end();
    for (GeoNotifierVector::const_iterator it = notifiers.begin(); it != end; ++it)
        (*it)->runSuccessCallback(position);
}

void Geolocation::stopTimer(GeoNotifierVector& notifiers)
{
    GeoNotifierVector::const_iterator end = notifiers.end();
    for (GeoNotifierVector::const_iterator it = notifiers.begin(); it != end; ++it)
        (*it)->stopTimer();
}

void Geolocation::stopTimersForOneShots()
{
    GeoNotifierVector copy;
    copyToVector(m_oneShots, copy);

    stopTimer(copy);
}

void Geolocation::stopTimersForWatchers()
{
    GeoNotifierVector copy;
    m_watchers.getNotifiersVector(copy);

    stopTimer(copy);
}

void Geolocation::stopTimers()
{
    stopTimersForOneShots();
    stopTimersForWatchers();
}

void Geolocation::cancelRequests(GeoNotifierVector& notifiers)
{
    GeoNotifierVector::const_iterator end = notifiers.end();
    for (GeoNotifierVector::const_iterator it = notifiers.begin(); it != end; ++it)
        (*it)->setFatalError(PositionError::create(PositionError::POSITION_UNAVAILABLE, framelessDocumentErrorMessage));
}

void Geolocation::cancelAllRequests()
{
    GeoNotifierVector copy;
    copyToVector(m_oneShots, copy);
    cancelRequests(copy);
    m_watchers.getNotifiersVector(copy);
    cancelRequests(copy);
}

void Geolocation::extractNotifiersWithCachedPosition(GeoNotifierVector& notifiers, GeoNotifierVector* cached)
{
    GeoNotifierVector nonCached;
    GeoNotifierVector::iterator end = notifiers.end();
    for (GeoNotifierVector::const_iterator it = notifiers.begin(); it != end; ++it) {
        GeoNotifier* notifier = it->get();
        if (notifier->useCachedPosition()) {
            if (cached)
                cached->append(notifier);
        } else
            nonCached.append(notifier);
    }
    notifiers.swap(nonCached);
}

void Geolocation::copyToSet(const GeoNotifierVector& src, GeoNotifierSet& dest)
{
     GeoNotifierVector::const_iterator end = src.end();
     for (GeoNotifierVector::const_iterator it = src.begin(); it != end; ++it) {
         GeoNotifier* notifier = it->get();
         dest.add(notifier);
     }
}

void Geolocation::handleError(PositionError* error)
{
    ASSERT(error);

    GeoNotifierVector oneShotsCopy;
    copyToVector(m_oneShots, oneShotsCopy);

    GeoNotifierVector watchersCopy;
    m_watchers.getNotifiersVector(watchersCopy);

    // Clear the lists before we make the callbacks, to avoid clearing notifiers
    // added by calls to Geolocation methods from the callbacks, and to prevent
    // further callbacks to these notifiers.
    GeoNotifierVector oneShotsWithCachedPosition;
    m_oneShots.clear();
    if (error->isFatal())
        m_watchers.clear();
    else {
        // Don't send non-fatal errors to notifiers due to receive a cached position.
        extractNotifiersWithCachedPosition(oneShotsCopy, &oneShotsWithCachedPosition);
        extractNotifiersWithCachedPosition(watchersCopy, 0);
    }

    sendError(oneShotsCopy, error);
    sendError(watchersCopy, error);

    // hasListeners() doesn't distinguish between notifiers due to receive a
    // cached position and those requiring a fresh position. Perform the check
    // before restoring the notifiers below.
    if (!hasListeners())
        stopUpdating();

    // Maintain a reference to the cached notifiers until their timer fires.
    copyToSet(oneShotsWithCachedPosition, m_oneShots);
}

void Geolocation::requestPermission()
{
    if (m_allowGeolocation > Unknown)
        return;

    Page* page = this->page();
    if (!page)
        return;

    m_allowGeolocation = InProgress;

    // Ask the embedder: it maintains the geolocation challenge policy itself.
    GeolocationController::from(page)->requestPermission(this);
}

void Geolocation::makeSuccessCallbacks()
{
    ASSERT(lastPosition());
    ASSERT(isAllowed());

    GeoNotifierVector oneShotsCopy;
    copyToVector(m_oneShots, oneShotsCopy);

    GeoNotifierVector watchersCopy;
    m_watchers.getNotifiersVector(watchersCopy);

    // Clear the lists before we make the callbacks, to avoid clearing notifiers
    // added by calls to Geolocation methods from the callbacks, and to prevent
    // further callbacks to these notifiers.
    m_oneShots.clear();

    // Also clear the set of notifiers waiting for a cached position. All the
    // oneshots and watchers will receive a position now, and if they happen to
    // be lingering in that set, avoid this bug: http://crbug.com/311876 .
    m_requestsAwaitingCachedPosition.clear();

    sendPosition(oneShotsCopy, lastPosition());
    sendPosition(watchersCopy, lastPosition());

    if (!hasListeners())
        stopUpdating();
}

void Geolocation::positionChanged()
{
    ASSERT(isAllowed());

    // Stop all currently running timers.
    stopTimers();

    makeSuccessCallbacks();
}

void Geolocation::setError(GeolocationError* error)
{
    RefPtrWillBeRawPtr<PositionError> positionError = createPositionError(error);
    handleError(positionError.get());
}

bool Geolocation::startUpdating(GeoNotifier* notifier)
{
    Page* page = this->page();
    if (!page)
        return false;

    GeolocationController::from(page)->addObserver(this, notifier->options()->enableHighAccuracy());
    return true;
}

void Geolocation::stopUpdating()
{
    Page* page = this->page();
    if (!page)
        return;

    GeolocationController::from(page)->removeObserver(this);
}

void Geolocation::handlePendingPermissionNotifiers()
{
    // While we iterate through the list, we need not worry about list being modified as the permission
    // is already set to Yes/No and no new listeners will be added to the pending list
    GeoNotifierSet::const_iterator end = m_pendingForPermissionNotifiers.end();
    for (GeoNotifierSet::const_iterator iter = m_pendingForPermissionNotifiers.begin(); iter != end; ++iter) {
        GeoNotifier* notifier = iter->get();

        if (isAllowed()) {
            // start all pending notification requests as permission granted.
            // The notifier is always ref'ed by m_oneShots or m_watchers.
            if (startUpdating(notifier))
                notifier->startTimerIfNeeded();
            else
                notifier->setFatalError(PositionError::create(PositionError::POSITION_UNAVAILABLE, failedToStartServiceErrorMessage));
        } else {
            notifier->setFatalError(PositionError::create(PositionError::PERMISSION_DENIED, permissionDeniedErrorMessage));
        }
    }
}

} // namespace WebCore
