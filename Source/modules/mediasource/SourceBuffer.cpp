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
#include "modules/mediasource/SourceBuffer.h"

#include "bindings/v8/ExceptionMessages.h"
#include "bindings/v8/ExceptionState.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/ExecutionContext.h"
#include "core/events/Event.h"
#include "core/events/GenericEventQueue.h"
#include "core/fileapi/FileReaderLoader.h"
#include "core/fileapi/Stream.h"
#include "core/html/TimeRanges.h"
#include "modules/mediasource/MediaSource.h"
#include "platform/Logging.h"
#include "platform/TraceEvent.h"
#include "public/platform/WebSourceBuffer.h"
#include "wtf/ArrayBuffer.h"
#include "wtf/ArrayBufferView.h"
#include "wtf/MathExtras.h"

#include <limits>

using blink::WebSourceBuffer;

namespace WebCore {

namespace {

static bool throwExceptionIfRemovedOrUpdating(bool isRemoved, bool isUpdating, ExceptionState& exceptionState)
{
    if (isRemoved) {
        exceptionState.throwDOMException(InvalidStateError, "This SourceBuffer has been removed from the parent media source.");
        return true;
    }
    if (isUpdating) {
        exceptionState.throwDOMException(InvalidStateError, "This SourceBuffer is still processing an 'appendBuffer', 'appendStream', or 'remove' operation.");
        return true;
    }

    return false;
}

} // namespace

PassRefPtrWillBeRawPtr<SourceBuffer> SourceBuffer::create(PassOwnPtr<WebSourceBuffer> webSourceBuffer, MediaSource* source, GenericEventQueue* asyncEventQueue)
{
    RefPtrWillBeRawPtr<SourceBuffer> sourceBuffer(adoptRefWillBeRefCountedGarbageCollected(new SourceBuffer(webSourceBuffer, source, asyncEventQueue)));
    sourceBuffer->suspendIfNeeded();
    return sourceBuffer.release();
}

SourceBuffer::SourceBuffer(PassOwnPtr<WebSourceBuffer> webSourceBuffer, MediaSource* source, GenericEventQueue* asyncEventQueue)
    : ActiveDOMObject(source->executionContext())
    , m_webSourceBuffer(webSourceBuffer)
    , m_source(source)
    , m_asyncEventQueue(asyncEventQueue)
    , m_mode(segmentsKeyword())
    , m_updating(false)
    , m_timestampOffset(0)
    , m_appendWindowStart(0)
    , m_appendWindowEnd(std::numeric_limits<double>::infinity())
    , m_appendBufferAsyncPartRunner(this, &SourceBuffer::appendBufferAsyncPart)
    , m_pendingRemoveStart(-1)
    , m_pendingRemoveEnd(-1)
    , m_removeAsyncPartRunner(this, &SourceBuffer::removeAsyncPart)
    , m_streamMaxSizeValid(false)
    , m_streamMaxSize(0)
    , m_appendStreamAsyncPartRunner(this, &SourceBuffer::appendStreamAsyncPart)
{
    ASSERT(m_webSourceBuffer);
    ASSERT(m_source);
    ScriptWrappable::init(this);
}

SourceBuffer::~SourceBuffer()
{
    ASSERT(isRemoved());
    ASSERT(!m_loader);
    ASSERT(!m_stream);
}

const AtomicString& SourceBuffer::segmentsKeyword()
{
    DEFINE_STATIC_LOCAL(const AtomicString, segments, ("segments", AtomicString::ConstructFromLiteral));
    return segments;
}

const AtomicString& SourceBuffer::sequenceKeyword()
{
    DEFINE_STATIC_LOCAL(const AtomicString, sequence, ("sequence", AtomicString::ConstructFromLiteral));
    return sequence;
}

void SourceBuffer::setMode(const AtomicString& newMode, ExceptionState& exceptionState)
{
    // Section 3.1 On setting mode attribute steps.
    // 1. Let new mode equal the new value being assigned to this attribute.
    // 2. If new mode does not equal "segments" or "sequence", then throw an INVALID_ACCESS_ERR exception and abort
    //    these steps.
    //    Step 2 is unnecessary: IDL enforcement prevents this case and should just return immediately to script
    //    without calling this method in this case.
    // 3. If this object has been removed from the sourceBuffers attribute of the parent media source, then throw
    //    an INVALID_STATE_ERR exception and abort these steps.
    // 4. If the updating attribute equals true, then throw an INVALID_STATE_ERR exception and abort these steps.
    if (throwExceptionIfRemovedOrUpdating(isRemoved(), m_updating, exceptionState))
        return;

    // 5. If the readyState attribute of the parent media source is in the "ended" state then run the following steps:
    // 5.1 Set the readyState attribute of the parent media source to "open"
    // 5.2 Queue a task to fire a simple event named sourceopen at the parent media source.
    m_source->openIfInEndedState();

    // 6. If the append state equals PARSING_MEDIA_SEGMENT, then throw an INVALID_STATE_ERR and abort these steps.
    // 7. If the new mode equals "sequence", then set the group start timestamp to the highest presentation end timestamp.
    WebSourceBuffer::AppendMode appendMode = WebSourceBuffer::AppendModeSegments;
    if (newMode == sequenceKeyword())
        appendMode = WebSourceBuffer::AppendModeSequence;
    if (!m_webSourceBuffer->setMode(appendMode)) {
        exceptionState.throwDOMException(InvalidStateError, "The mode may not be set while the SourceBuffer's append state is 'PARSING_MEDIA_SEGMENT'.");
        return;
    }

    // 8. Update the attribute to new mode.
    m_mode = newMode;
}

PassRefPtr<TimeRanges> SourceBuffer::buffered(ExceptionState& exceptionState) const
{
    // Section 3.1 buffered attribute steps.
    // 1. If this object has been removed from the sourceBuffers attribute of the parent media source then throw an
    //    InvalidStateError exception and abort these steps.
    if (isRemoved()) {
        exceptionState.throwDOMException(InvalidStateError, "This SourceBuffer has been removed from the parent media source.");
        return nullptr;
    }

    // 2. Return a new static normalized TimeRanges object for the media segments buffered.
    return TimeRanges::create(m_webSourceBuffer->buffered());
}

double SourceBuffer::timestampOffset() const
{
    return m_timestampOffset;
}

void SourceBuffer::setTimestampOffset(double offset, ExceptionState& exceptionState)
{
    // Section 3.1 timestampOffset attribute setter steps.
    // 1. Let new timestamp offset equal the new value being assigned to this attribute.
    // 2. If this object has been removed from the sourceBuffers attribute of the parent media source, then throw an
    //    InvalidStateError exception and abort these steps.
    // 3. If the updating attribute equals true, then throw an InvalidStateError exception and abort these steps.
    if (throwExceptionIfRemovedOrUpdating(isRemoved(), m_updating, exceptionState))
        return;

    // 4. If the readyState attribute of the parent media source is in the "ended" state then run the following steps:
    // 4.1 Set the readyState attribute of the parent media source to "open"
    // 4.2 Queue a task to fire a simple event named sourceopen at the parent media source.
    m_source->openIfInEndedState();

    // 5. If the append state equals PARSING_MEDIA_SEGMENT, then throw an INVALID_STATE_ERR and abort these steps.
    // 6. If the mode attribute equals "sequence", then set the group start timestamp to new timestamp offset.
    if (!m_webSourceBuffer->setTimestampOffset(offset)) {
        exceptionState.throwDOMException(InvalidStateError, "The timestamp offset may not be set while the SourceBuffer's append state is 'PARSING_MEDIA_SEGMENT'.");
        return;
    }

    // 7. Update the attribute to new timestamp offset.
    m_timestampOffset = offset;
}

double SourceBuffer::appendWindowStart() const
{
    return m_appendWindowStart;
}

void SourceBuffer::setAppendWindowStart(double start, ExceptionState& exceptionState)
{
    // Enforce throwing an exception on restricted double values.
    if (std::isnan(start)
        || start == std::numeric_limits<double>::infinity()
        || start == -std::numeric_limits<double>::infinity()) {
        exceptionState.throwDOMException(TypeMismatchError, ExceptionMessages::notAFiniteNumber(start));
        return;
    }

    // Section 3.1 appendWindowStart attribute setter steps.
    // 1. If this object has been removed from the sourceBuffers attribute of the parent media source then throw an
    //    InvalidStateError exception and abort these steps.
    // 2. If the updating attribute equals true, then throw an InvalidStateError exception and abort these steps.
    if (throwExceptionIfRemovedOrUpdating(isRemoved(), m_updating, exceptionState))
        return;

    // 3. If the new value is less than 0 or greater than or equal to appendWindowEnd then throw an InvalidAccessError
    //    exception and abort these steps.
    if (start < 0 || start >= m_appendWindowEnd) {
        exceptionState.throwDOMException(InvalidAccessError, ExceptionMessages::indexOutsideRange("value", start, 0.0, ExceptionMessages::ExclusiveBound, m_appendWindowEnd, ExceptionMessages::InclusiveBound));
        return;
    }

    m_webSourceBuffer->setAppendWindowStart(start);

    // 4. Update the attribute to the new value.
    m_appendWindowStart = start;
}

double SourceBuffer::appendWindowEnd() const
{
    return m_appendWindowEnd;
}

void SourceBuffer::setAppendWindowEnd(double end, ExceptionState& exceptionState)
{
    // Section 3.1 appendWindowEnd attribute setter steps.
    // 1. If this object has been removed from the sourceBuffers attribute of the parent media source then throw an
    //    InvalidStateError exception and abort these steps.
    // 2. If the updating attribute equals true, then throw an InvalidStateError exception and abort these steps.
    if (throwExceptionIfRemovedOrUpdating(isRemoved(), m_updating, exceptionState))
        return;

    // 3. If the new value equals NaN, then throw an InvalidAccessError and abort these steps.
    // 4. If the new value is less than or equal to appendWindowStart then throw an InvalidAccessError
    //    exception and abort these steps.
    if (std::isnan(end)) {
        exceptionState.throwDOMException(InvalidAccessError, ExceptionMessages::notAFiniteNumber(end));
        return;
    }
    if (end <= m_appendWindowStart) {
        // FIXME: Use ExceptionState::indexExceedsMinimumBound() once it lands.
        exceptionState.throwDOMException(InvalidAccessError, "The value provided ('" + String::number(end) + "') is less than or equal to the minimum value (" + String::number(m_appendWindowStart) + ").");
        return;
    }

    m_webSourceBuffer->setAppendWindowEnd(end);

    // 5. Update the attribute to the new value.
    m_appendWindowEnd = end;
}

void SourceBuffer::appendBuffer(PassRefPtr<ArrayBuffer> data, ExceptionState& exceptionState)
{
    // Section 3.2 appendBuffer()
    // https://dvcs.w3.org/hg/html-media/raw-file/default/media-source/media-source.html#widl-SourceBuffer-appendBuffer-void-ArrayBufferView-data
    // 1. If data is null then throw an InvalidAccessError exception and abort these steps.
    if (!data) {
        exceptionState.throwDOMException(InvalidAccessError, "The ArrayBuffer provided is invalid.");
        return;
    }

    appendBufferInternal(static_cast<const unsigned char*>(data->data()), data->byteLength(), exceptionState);
}

void SourceBuffer::appendBuffer(PassRefPtr<ArrayBufferView> data, ExceptionState& exceptionState)
{
    // Section 3.2 appendBuffer()
    // https://dvcs.w3.org/hg/html-media/raw-file/default/media-source/media-source.html#widl-SourceBuffer-appendBuffer-void-ArrayBufferView-data
    // 1. If data is null then throw an InvalidAccessError exception and abort these steps.
    if (!data) {
        exceptionState.throwDOMException(InvalidAccessError, "The ArrayBuffer provided is invalid.");
        return;
    }

    appendBufferInternal(static_cast<const unsigned char*>(data->baseAddress()), data->byteLength(), exceptionState);
}

void SourceBuffer::appendStream(PassRefPtrWillBeRawPtr<Stream> stream, ExceptionState& exceptionState)
{
    m_streamMaxSizeValid = false;
    appendStreamInternal(stream, exceptionState);
}

void SourceBuffer::appendStream(PassRefPtrWillBeRawPtr<Stream> stream, unsigned long long maxSize, ExceptionState& exceptionState)
{
    m_streamMaxSizeValid = maxSize > 0;
    if (m_streamMaxSizeValid)
        m_streamMaxSize = maxSize;
    appendStreamInternal(stream, exceptionState);
}

void SourceBuffer::abort(ExceptionState& exceptionState)
{
    // Section 3.2 abort() method steps.
    // https://dvcs.w3.org/hg/html-media/raw-file/default/media-source/media-source.html#widl-SourceBuffer-abort-void
    // 1. If this object has been removed from the sourceBuffers attribute of the parent media source
    //    then throw an InvalidStateError exception and abort these steps.
    // 2. If the readyState attribute of the parent media source is not in the "open" state
    //    then throw an InvalidStateError exception and abort these steps.
    if (isRemoved()) {
        exceptionState.throwDOMException(InvalidStateError, "This SourceBuffer has been removed from the parent media source.");
        return;
    }
    if (!m_source->isOpen()) {
        exceptionState.throwDOMException(InvalidStateError, "The parent media source's readyState is not 'open'.");
        return;
    }

    // 3. If the sourceBuffer.updating attribute equals true, then run the following steps: ...
    abortIfUpdating();

    // 4. Run the reset parser state algorithm.
    m_webSourceBuffer->abort();

    // 5. Set appendWindowStart to 0.
    setAppendWindowStart(0, exceptionState);

    // 6. Set appendWindowEnd to positive Infinity.
    setAppendWindowEnd(std::numeric_limits<double>::infinity(), exceptionState);
}

void SourceBuffer::remove(double start, double end, ExceptionState& exceptionState)
{
    // Section 3.2 remove() method steps.
    // 1. If start is negative or greater than duration, then throw an InvalidAccessError exception and abort these steps.
    // 2. If end is less than or equal to start, then throw an InvalidAccessError exception and abort these steps.

    if (start < 0 || (m_source && (std::isnan(m_source->duration()) || start > m_source->duration()))) {
        exceptionState.throwDOMException(InvalidAccessError, ExceptionMessages::indexOutsideRange("start", start, 0.0, ExceptionMessages::ExclusiveBound, !m_source || std::isnan(m_source->duration()) ? 0 : m_source->duration(), ExceptionMessages::ExclusiveBound));
        return;
    }
    if (end <= start) {
        exceptionState.throwDOMException(InvalidAccessError, "The end value provided (" + String::number(end) + ") must be greater than the start value provided (" + String::number(start) + ").");
        return;
    }

    // 3. If this object has been removed from the sourceBuffers attribute of the parent media source then throw an
    //    InvalidStateError exception and abort these steps.
    // 4. If the updating attribute equals true, then throw an InvalidStateError exception and abort these steps.
    if (throwExceptionIfRemovedOrUpdating(isRemoved(), m_updating, exceptionState))
        return;

    TRACE_EVENT_ASYNC_BEGIN0("media", "SourceBuffer::remove", this);

    // 5. If the readyState attribute of the parent media source is in the "ended" state then run the following steps:
    // 5.1. Set the readyState attribute of the parent media source to "open"
    // 5.2. Queue a task to fire a simple event named sourceopen at the parent media source .
    m_source->openIfInEndedState();

    // 6. Set the updating attribute to true.
    m_updating = true;

    // 7. Queue a task to fire a simple event named updatestart at this SourceBuffer object.
    scheduleEvent(EventTypeNames::updatestart);

    // 8. Return control to the caller and run the rest of the steps asynchronously.
    m_pendingRemoveStart = start;
    m_pendingRemoveEnd = end;
    m_removeAsyncPartRunner.runAsync();
}

void SourceBuffer::abortIfUpdating()
{
    // Section 3.2 abort() method step 3 substeps.
    // https://dvcs.w3.org/hg/html-media/raw-file/default/media-source/media-source.html#widl-SourceBuffer-abort-void

    if (!m_updating)
        return;

    const char* traceEventName = 0;
    if (!m_pendingAppendData.isEmpty()) {
        traceEventName = "SourceBuffer::appendBuffer";
    } else if (m_stream) {
        traceEventName = "SourceBuffer::appendStream";
    } else if (m_pendingRemoveStart != -1) {
        traceEventName = "SourceBuffer::remove";
    } else {
        ASSERT_NOT_REACHED();
    }

    // 3.1. Abort the buffer append and stream append loop algorithms if they are running.
    m_appendBufferAsyncPartRunner.stop();
    m_pendingAppendData.clear();

    m_removeAsyncPartRunner.stop();
    m_pendingRemoveStart = -1;
    m_pendingRemoveEnd = -1;

    m_appendStreamAsyncPartRunner.stop();
    clearAppendStreamState();

    // 3.2. Set the updating attribute to false.
    m_updating = false;

    // 3.3. Queue a task to fire a simple event named abort at this SourceBuffer object.
    scheduleEvent(EventTypeNames::abort);

    // 3.4. Queue a task to fire a simple event named updateend at this SourceBuffer object.
    scheduleEvent(EventTypeNames::updateend);

    TRACE_EVENT_ASYNC_END0("media", traceEventName, this);
}

void SourceBuffer::removedFromMediaSource()
{
    if (isRemoved())
        return;

    abortIfUpdating();

    m_webSourceBuffer->removedFromMediaSource();
    m_webSourceBuffer.clear();
    m_source = nullptr;
    m_asyncEventQueue = 0;
}

bool SourceBuffer::hasPendingActivity() const
{
    return m_source;
}

void SourceBuffer::suspend()
{
    m_appendBufferAsyncPartRunner.suspend();
    m_removeAsyncPartRunner.suspend();
    m_appendStreamAsyncPartRunner.suspend();
}

void SourceBuffer::resume()
{
    m_appendBufferAsyncPartRunner.resume();
    m_removeAsyncPartRunner.resume();
    m_appendStreamAsyncPartRunner.resume();
}

void SourceBuffer::stop()
{
    m_appendBufferAsyncPartRunner.stop();
    m_removeAsyncPartRunner.stop();
    m_appendStreamAsyncPartRunner.stop();
}

ExecutionContext* SourceBuffer::executionContext() const
{
    return ActiveDOMObject::executionContext();
}

const AtomicString& SourceBuffer::interfaceName() const
{
    return EventTargetNames::SourceBuffer;
}

bool SourceBuffer::isRemoved() const
{
    return !m_source;
}

void SourceBuffer::scheduleEvent(const AtomicString& eventName)
{
    ASSERT(m_asyncEventQueue);

    RefPtr<Event> event = Event::create(eventName);
    event->setTarget(this);

    m_asyncEventQueue->enqueueEvent(event.release());
}

void SourceBuffer::appendBufferInternal(const unsigned char* data, unsigned size, ExceptionState& exceptionState)
{
    // Section 3.2 appendBuffer()
    // https://dvcs.w3.org/hg/html-media/raw-file/default/media-source/media-source.html#widl-SourceBuffer-appendBuffer-void-ArrayBufferView-data

    // Step 1 is enforced by the caller.
    // 2. If this object has been removed from the sourceBuffers attribute of the parent media source then throw an InvalidStateError exception and abort these steps.
    // 3. If the updating attribute equals true, then throw an InvalidStateError exception and abort these steps.
    if (throwExceptionIfRemovedOrUpdating(isRemoved(), m_updating, exceptionState))
        return;

    TRACE_EVENT_ASYNC_BEGIN0("media", "SourceBuffer::appendBuffer", this);

    // 4. If the readyState attribute of the parent media source is in the "ended" state then run the following steps: ...
    m_source->openIfInEndedState();

    // Steps 5-6

    // 7. Add data to the end of the input buffer.
    m_pendingAppendData.append(data, size);

    // 8. Set the updating attribute to true.
    m_updating = true;

    // 9. Queue a task to fire a simple event named updatestart at this SourceBuffer object.
    scheduleEvent(EventTypeNames::updatestart);

    // 10. Asynchronously run the buffer append algorithm.
    m_appendBufferAsyncPartRunner.runAsync();

    TRACE_EVENT_ASYNC_STEP_INTO0("media", "SourceBuffer::appendBuffer", this, "waiting");
}

void SourceBuffer::appendBufferAsyncPart()
{
    ASSERT(m_updating);

    TRACE_EVENT_ASYNC_STEP_INTO0("media", "SourceBuffer::appendBuffer", this, "appending");

    // Section 3.5.4 Buffer Append Algorithm
    // https://dvcs.w3.org/hg/html-media/raw-file/default/media-source/media-source.html#sourcebuffer-buffer-append

    // 1. Run the segment parser loop algorithm.
    // Step 2 doesn't apply since we run Step 1 synchronously here.
    size_t appendSize = m_pendingAppendData.size();
    if (!appendSize) {
        // Resize buffer for 0 byte appends so we always have a valid pointer.
        // We need to convey all appends, even 0 byte ones to |m_webSourceBuffer|
        // so that it can clear its end of stream state if necessary.
        m_pendingAppendData.resize(1);
    }
    m_webSourceBuffer->append(m_pendingAppendData.data(), appendSize, &m_timestampOffset);

    // 3. Set the updating attribute to false.
    m_updating = false;
    m_pendingAppendData.clear();

    // 4. Queue a task to fire a simple event named update at this SourceBuffer object.
    scheduleEvent(EventTypeNames::update);

    // 5. Queue a task to fire a simple event named updateend at this SourceBuffer object.
    scheduleEvent(EventTypeNames::updateend);
    TRACE_EVENT_ASYNC_END0("media", "SourceBuffer::appendBuffer", this);
}

void SourceBuffer::removeAsyncPart()
{
    ASSERT(m_updating);
    ASSERT(m_pendingRemoveStart >= 0);
    ASSERT(m_pendingRemoveStart < m_pendingRemoveEnd);

    // Section 3.2 remove() method steps
    // https://dvcs.w3.org/hg/html-media/raw-file/default/media-source/media-source.html#widl-SourceBuffer-remove-void-double-start-double-end

    // 9. Run the coded frame removal algorithm with start and end as the start and end of the removal range.
    m_webSourceBuffer->remove(m_pendingRemoveStart, m_pendingRemoveEnd);

    // 10. Set the updating attribute to false.
    m_updating = false;
    m_pendingRemoveStart = -1;
    m_pendingRemoveEnd = -1;

    // 11. Queue a task to fire a simple event named update at this SourceBuffer object.
    scheduleEvent(EventTypeNames::update);

    // 12. Queue a task to fire a simple event named updateend at this SourceBuffer object.
    scheduleEvent(EventTypeNames::updateend);
}

void SourceBuffer::appendStreamInternal(PassRefPtrWillBeRawPtr<Stream> stream, ExceptionState& exceptionState)
{
    // Section 3.2 appendStream()
    // https://dvcs.w3.org/hg/html-media/raw-file/default/media-source/media-source.html#widl-SourceBuffer-appendStream-void-Stream-stream-unsigned-long-long-maxSize
    // 1. If stream is null then throw an InvalidAccessError exception and abort these steps.
    if (!stream || stream->isNeutered()) {
        exceptionState.throwDOMException(InvalidAccessError, stream ? "The stream provided has been neutered." : "The stream provided is invalid.");
        return;
    }

    // 2. Run the prepare append algorithm.
    //  Section 3.5.4 Prepare Append Algorithm.
    //  https://dvcs.w3.org/hg/html-media/raw-file/default/media-source/media-source.html#sourcebuffer-prepare-append
    //  1. If this object has been removed from the sourceBuffers attribute of the parent media source then throw an InvalidStateError exception and abort these steps.
    //  2. If the updating attribute equals true, then throw an InvalidStateError exception and abort these steps.
    if (throwExceptionIfRemovedOrUpdating(isRemoved(), m_updating, exceptionState))
        return;

    TRACE_EVENT_ASYNC_BEGIN0("media", "SourceBuffer::appendStream", this);

    //  3. If the readyState attribute of the parent media source is in the "ended" state then run the following steps: ...
    m_source->openIfInEndedState();

    // Steps 4-5 of the prepare append algorithm are handled by m_webSourceBuffer.

    // 3. Set the updating attribute to true.
    m_updating = true;

    // 4. Queue a task to fire a simple event named updatestart at this SourceBuffer object.
    scheduleEvent(EventTypeNames::updatestart);

    // 5. Asynchronously run the stream append loop algorithm with stream and maxSize.

    stream->neuter();
    m_loader = adoptPtr(new FileReaderLoader(FileReaderLoader::ReadByClient, this));
    m_stream = stream;
    m_appendStreamAsyncPartRunner.runAsync();
}

void SourceBuffer::appendStreamAsyncPart()
{
    ASSERT(m_updating);
    ASSERT(m_loader);
    ASSERT(m_stream);

    // Section 3.5.6 Stream Append Loop
    // https://dvcs.w3.org/hg/html-media/raw-file/default/media-source/media-source.html#sourcebuffer-stream-append-loop

    // 1. If maxSize is set, then let bytesLeft equal maxSize.
    // 2. Loop Top: If maxSize is set and bytesLeft equals 0, then jump to the loop done step below.
    if (m_streamMaxSizeValid && !m_streamMaxSize) {
        appendStreamDone(true);
        return;
    }

    // Steps 3-11 are handled by m_loader.
    // Note: Passing 0 here signals that maxSize was not set. (i.e. Read all the data in the stream).
    m_loader->start(executionContext(), *m_stream, m_streamMaxSizeValid ? m_streamMaxSize : 0);
}

void SourceBuffer::appendStreamDone(bool success)
{
    ASSERT(m_updating);
    ASSERT(m_loader);
    ASSERT(m_stream);

    clearAppendStreamState();

    if (!success) {
        // Section 3.5.3 Append Error Algorithm
        // https://dvcs.w3.org/hg/html-media/raw-file/default/media-source/media-source.html#sourcebuffer-append-error
        //
        // 1. Run the reset parser state algorithm. (Handled by caller)
        // 2. Set the updating attribute to false.
        m_updating = false;

        // 3. Queue a task to fire a simple event named error at this SourceBuffer object.
        scheduleEvent(EventTypeNames::error);

        // 4. Queue a task to fire a simple event named updateend at this SourceBuffer object.
        scheduleEvent(EventTypeNames::updateend);
        TRACE_EVENT_ASYNC_END0("media", "SourceBuffer::appendStream", this);
        return;
    }

    // Section 3.5.6 Stream Append Loop
    // Steps 1-11 are handled by appendStreamAsyncPart(), |m_loader|, and |m_webSourceBuffer|.
    // 12. Loop Done: Set the updating attribute to false.
    m_updating = false;

    // 13. Queue a task to fire a simple event named update at this SourceBuffer object.
    scheduleEvent(EventTypeNames::update);

    // 14. Queue a task to fire a simple event named updateend at this SourceBuffer object.
    scheduleEvent(EventTypeNames::updateend);
    TRACE_EVENT_ASYNC_END0("media", "SourceBuffer::appendStream", this);
}

void SourceBuffer::clearAppendStreamState()
{
    m_streamMaxSizeValid = false;
    m_streamMaxSize = 0;
    m_loader.clear();
    m_stream = nullptr;
}

void SourceBuffer::didStartLoading()
{
    WTF_LOG(Media, "SourceBuffer::didStartLoading() %p", this);
}

void SourceBuffer::didReceiveDataForClient(const char* data, unsigned dataLength)
{
    WTF_LOG(Media, "SourceBuffer::didReceiveDataForClient(%d) %p", dataLength, this);
    ASSERT(m_updating);
    ASSERT(m_loader);
    m_webSourceBuffer->append(reinterpret_cast<const unsigned char*>(data), dataLength, &m_timestampOffset);
}

void SourceBuffer::didFinishLoading()
{
    WTF_LOG(Media, "SourceBuffer::didFinishLoading() %p", this);
    appendStreamDone(true);
}

void SourceBuffer::didFail(FileError::ErrorCode errorCode)
{
    WTF_LOG(Media, "SourceBuffer::didFail(%d) %p", errorCode, this);
    appendStreamDone(false);
}

void SourceBuffer::trace(Visitor* visitor)
{
    visitor->trace(m_source);
    visitor->trace(m_stream);
}

} // namespace WebCore
