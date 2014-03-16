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

#include "config.h"
#include "core/xml/parser/SharedBufferReader.h"

#include "platform/SharedBuffer.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefPtr.h"

#include <algorithm>
#include <cstring>

namespace WebCore {

SharedBufferReader::SharedBufferReader(PassRefPtr<SharedBuffer> buffer)
    : m_buffer(buffer)
    , m_currentOffset(0)
{
}

SharedBufferReader::~SharedBufferReader()
{
}

int SharedBufferReader::readData(char* outputBuffer, unsigned askedToRead)
{
    if (!m_buffer || m_currentOffset > m_buffer->size())
        return 0;

    unsigned bytesCopied = 0;
    unsigned bytesLeft = m_buffer->size() - m_currentOffset;
    unsigned lenToCopy = std::min(askedToRead, bytesLeft);

    while (bytesCopied < lenToCopy) {
        const char* data;
        unsigned segmentSize = m_buffer->getSomeData(data, m_currentOffset);
        if (!segmentSize)
            break;

        segmentSize = std::min(segmentSize, lenToCopy - bytesCopied);
        memcpy(outputBuffer + bytesCopied, data, segmentSize);
        bytesCopied += segmentSize;
        m_currentOffset += segmentSize;
    }

    return bytesCopied;
}

} // namespace WebCore
