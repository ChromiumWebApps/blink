/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#ifndef WebRTCICECandidate_h
#define WebRTCICECandidate_h

#include "WebCommon.h"
#include "WebPrivatePtr.h"
#include "WebString.h"

namespace WebCore {
class RTCIceCandidateDescriptor;
}

namespace blink {

class WebString;
class WebRTCICECandidatePrivate;

class WebRTCICECandidate {
public:
    WebRTCICECandidate() { }
    WebRTCICECandidate(const WebRTCICECandidate& other) { assign(other); }
    ~WebRTCICECandidate() { reset(); }

    WebRTCICECandidate& operator=(const WebRTCICECandidate& other)
    {
        assign(other);
        return *this;
    }

    BLINK_PLATFORM_EXPORT void assign(const WebRTCICECandidate&);

    BLINK_PLATFORM_EXPORT void initialize(const WebString& candidate, const WebString& sdpMid, unsigned short sdpMLineIndex);
    BLINK_PLATFORM_EXPORT void reset();
    bool isNull() const { return m_private.isNull(); }

    BLINK_PLATFORM_EXPORT WebString candidate() const;
    BLINK_PLATFORM_EXPORT WebString sdpMid() const;
    BLINK_PLATFORM_EXPORT unsigned short sdpMLineIndex() const;
    BLINK_PLATFORM_EXPORT void setCandidate(WebString);
    BLINK_PLATFORM_EXPORT void setSdpMid(WebString);
    BLINK_PLATFORM_EXPORT void setSdpMLineIndex(unsigned short);

#if INSIDE_BLINK
    WebRTCICECandidate(WebString candidate, WebString sdpMid, unsigned short sdpMLineIndex)
    {
        this->initialize(candidate, sdpMid, sdpMLineIndex);
    }
#endif

private:
    WebPrivatePtr<WebRTCICECandidatePrivate> m_private;
};

} // namespace blink

#endif // WebRTCICECandidate_h
