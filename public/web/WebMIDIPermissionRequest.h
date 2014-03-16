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

#ifndef WebMIDIPermissionRequest_h
#define WebMIDIPermissionRequest_h

#include "../platform/WebCommon.h"
#include "../platform/WebPrivatePtr.h"

namespace WebCore {
class MIDIAccess;
}

namespace blink {

class WebSecurityOrigin;

// WebMIDIPermissionRequest encapsulates a WebCore MIDIAccess object and represents
// a request from WebCore for permissions.
// The underlying MIDIAccess object is guaranteed to be valid until the invocation of
// either WebMIDIPermissionRequest::setIsAllowed (request complete) or
// WebMIDIClient::cancelPermissionRequest (request canceled).
class WebMIDIPermissionRequest {
public:
    WebMIDIPermissionRequest(const WebMIDIPermissionRequest& o) { assign(o); }
    ~WebMIDIPermissionRequest() { reset(); };

    BLINK_EXPORT WebSecurityOrigin securityOrigin() const;
    BLINK_EXPORT void setIsAllowed(bool);

    BLINK_EXPORT void reset();
    BLINK_EXPORT void assign(const WebMIDIPermissionRequest&);
    BLINK_EXPORT bool equals(const WebMIDIPermissionRequest&) const;

#if BLINK_IMPLEMENTATION
    explicit WebMIDIPermissionRequest(const PassRefPtrWillBeRawPtr<WebCore::MIDIAccess>&);

    WebCore::MIDIAccess* midiAccess() const { return m_private.get(); }
#endif

private:
    WebPrivatePtr<WebCore::MIDIAccess> m_private;
};

inline bool operator==(const WebMIDIPermissionRequest& a, const WebMIDIPermissionRequest& b)
{
    return a.equals(b);
}

inline bool operator!=(const WebMIDIPermissionRequest& a, const WebMIDIPermissionRequest& b)
{
    return !(a == b);
}

} // namespace blink

#endif // WebMIDIPermissionRequest_h
