/*
 * Copyright (C) 2003, 2006 Apple Computer, Inc.  All rights reserved.
 * Copyright (C) 2006 Samuel Weinig <sam.weinig@gmail.com>
 * Copyright (C) 2009, 2012 Google Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef ResourceRequest_h
#define ResourceRequest_h

#include "platform/network/FormData.h"
#include "platform/network/HTTPHeaderMap.h"
#include "platform/network/ResourceLoadPriority.h"
#include "platform/weborigin/KURL.h"
#include "platform/weborigin/Referrer.h"
#include "wtf/OwnPtr.h"

namespace WebCore {

enum ResourceRequestCachePolicy {
    UseProtocolCachePolicy, // normal load
    ReloadIgnoringCacheData, // reload
    ReturnCacheDataElseLoad, // back/forward or encoding change - allow stale data
    ReturnCacheDataDontLoad  // results of a post - allow stale data and only use cache
};

struct CrossThreadResourceRequestData;

class PLATFORM_EXPORT ResourceRequest {
    WTF_MAKE_FAST_ALLOCATED;
public:
    // The type of this ResourceRequest, based on how the resource will be used.
    enum TargetType {
        TargetIsMainFrame,
        TargetIsSubframe,
        TargetIsSubresource, // Resource is a generic subresource. (Generally a specific type should be specified)
        TargetIsStyleSheet,
        TargetIsScript,
        TargetIsFont,
        TargetIsImage,
        TargetIsObject,
        TargetIsMedia,
        TargetIsWorker,
        TargetIsSharedWorker,
        TargetIsPrefetch,
        TargetIsFavicon,
        TargetIsXHR,
        TargetIsTextTrack,
        TargetIsPing,
        TargetIsServiceWorker,
        TargetIsUnspecified,
    };

    class ExtraData : public RefCounted<ExtraData> {
    public:
        virtual ~ExtraData() { }
    };

    ResourceRequest()
    {
        initialize(KURL(), UseProtocolCachePolicy);
    }

    ResourceRequest(const String& urlString)
    {
        initialize(KURL(ParsedURLString, urlString), UseProtocolCachePolicy);
    }

    ResourceRequest(const KURL& url)
    {
        initialize(url, UseProtocolCachePolicy);
    }

    ResourceRequest(const KURL& url, const Referrer& referrer, ResourceRequestCachePolicy cachePolicy = UseProtocolCachePolicy)
    {
        initialize(url, cachePolicy);
        setHTTPReferrer(referrer);
    }

    static PassOwnPtr<ResourceRequest> adopt(PassOwnPtr<CrossThreadResourceRequestData>);

    // Gets a copy of the data suitable for passing to another thread.
    PassOwnPtr<CrossThreadResourceRequestData> copyData() const;

    bool isNull() const;
    bool isEmpty() const;

    const KURL& url() const;
    void setURL(const KURL& url);

    void removeCredentials();

    ResourceRequestCachePolicy cachePolicy() const;
    void setCachePolicy(ResourceRequestCachePolicy cachePolicy);

    double timeoutInterval() const; // May return 0 when using platform default.
    void setTimeoutInterval(double timeoutInterval);

    const KURL& firstPartyForCookies() const;
    void setFirstPartyForCookies(const KURL& firstPartyForCookies);

    const AtomicString& httpMethod() const;
    void setHTTPMethod(const AtomicString&);

    const HTTPHeaderMap& httpHeaderFields() const;
    const AtomicString& httpHeaderField(const AtomicString& name) const;
    const AtomicString& httpHeaderField(const char* name) const;
    void setHTTPHeaderField(const AtomicString& name, const AtomicString& value);
    void setHTTPHeaderField(const char* name, const AtomicString& value);
    void addHTTPHeaderField(const AtomicString& name, const AtomicString& value);
    void addHTTPHeaderFields(const HTTPHeaderMap& headerFields);
    void clearHTTPHeaderField(const AtomicString& name);

    void clearHTTPAuthorization();

    const AtomicString& httpContentType() const { return httpHeaderField("Content-Type");  }
    void setHTTPContentType(const AtomicString& httpContentType) { setHTTPHeaderField("Content-Type", httpContentType); }
    void clearHTTPContentType();

    const AtomicString& httpReferrer() const { return httpHeaderField("Referer"); }
    ReferrerPolicy referrerPolicy() const { return m_referrerPolicy; }
    void setHTTPReferrer(const Referrer& httpReferrer) { setHTTPHeaderField("Referer", httpReferrer.referrer); m_referrerPolicy = httpReferrer.referrerPolicy; }
    void clearHTTPReferrer();

    const AtomicString& httpOrigin() const { return httpHeaderField("Origin"); }
    void setHTTPOrigin(const AtomicString& httpOrigin) { setHTTPHeaderField("Origin", httpOrigin); }
    void clearHTTPOrigin();

    const AtomicString& httpUserAgent() const { return httpHeaderField("User-Agent"); }
    void setHTTPUserAgent(const AtomicString& httpUserAgent) { setHTTPHeaderField("User-Agent", httpUserAgent); }
    void clearHTTPUserAgent();

    const AtomicString& httpAccept() const { return httpHeaderField("Accept"); }
    void setHTTPAccept(const AtomicString& httpAccept) { setHTTPHeaderField("Accept", httpAccept); }
    void clearHTTPAccept();

    FormData* httpBody() const;
    void setHTTPBody(PassRefPtr<FormData> httpBody);

    bool allowStoredCredentials() const;
    void setAllowStoredCredentials(bool allowCredentials);

    ResourceLoadPriority priority() const;
    void setPriority(ResourceLoadPriority);

    bool isConditional() const;

    // Whether the associated ResourceHandleClient needs to be notified of
    // upload progress made for that resource.
    bool reportUploadProgress() const { return m_reportUploadProgress; }
    void setReportUploadProgress(bool reportUploadProgress) { m_reportUploadProgress = reportUploadProgress; }

    // Whether the timing information should be collected for the request.
    bool reportLoadTiming() const { return m_reportLoadTiming; }
    void setReportLoadTiming(bool reportLoadTiming) { m_reportLoadTiming = reportLoadTiming; }

    // Whether actual headers being sent/received should be collected and reported for the request.
    bool reportRawHeaders() const { return m_reportRawHeaders; }
    void setReportRawHeaders(bool reportRawHeaders) { m_reportRawHeaders = reportRawHeaders; }

    // Allows the request to be matched up with its requestor.
    int requestorID() const { return m_requestorID; }
    void setRequestorID(int requestorID) { m_requestorID = requestorID; }

    // The process id of the process from which this request originated. In
    // the case of out-of-process plugins, this allows to link back the
    // request to the plugin process (as it is processed through a render
    // view process).
    int requestorProcessID() const { return m_requestorProcessID; }
    void setRequestorProcessID(int requestorProcessID) { m_requestorProcessID = requestorProcessID; }

    // Allows the request to be matched up with its app cache host.
    int appCacheHostID() const { return m_appCacheHostID; }
    void setAppCacheHostID(int id) { m_appCacheHostID = id; }

    // True if request was user initiated.
    bool hasUserGesture() const { return m_hasUserGesture; }
    void setHasUserGesture(bool hasUserGesture) { m_hasUserGesture = hasUserGesture; }

    // True if request should be downloaded to file.
    bool downloadToFile() const { return m_downloadToFile; }
    void setDownloadToFile(bool downloadToFile) { m_downloadToFile = downloadToFile; }

    // Extra data associated with this request.
    ExtraData* extraData() const { return m_extraData.get(); }
    void setExtraData(PassRefPtr<ExtraData> extraData) { m_extraData = extraData; }

    // What this request is for.
    TargetType targetType() const { return m_targetType; }
    void setTargetType(TargetType type) { m_targetType = type; }

    static double defaultTimeoutInterval(); // May return 0 when using platform default.
    static void setDefaultTimeoutInterval(double);

    static bool compare(const ResourceRequest&, const ResourceRequest&);

private:
    void initialize(const KURL& url, ResourceRequestCachePolicy cachePolicy);

    KURL m_url;
    ResourceRequestCachePolicy m_cachePolicy;
    double m_timeoutInterval; // 0 is a magic value for platform default on platforms that have one.
    KURL m_firstPartyForCookies;
    AtomicString m_httpMethod;
    HTTPHeaderMap m_httpHeaderFields;
    RefPtr<FormData> m_httpBody;
    bool m_allowStoredCredentials : 1;
    bool m_reportUploadProgress : 1;
    bool m_reportLoadTiming : 1;
    bool m_reportRawHeaders : 1;
    bool m_hasUserGesture : 1;
    bool m_downloadToFile : 1;
    ResourceLoadPriority m_priority;
    int m_requestorID;
    int m_requestorProcessID;
    int m_appCacheHostID;
    RefPtr<ExtraData> m_extraData;
    TargetType m_targetType;
    ReferrerPolicy m_referrerPolicy;

    static double s_defaultTimeoutInterval;
};

bool equalIgnoringHeaderFields(const ResourceRequest&, const ResourceRequest&);

inline bool operator==(const ResourceRequest& a, const ResourceRequest& b) { return ResourceRequest::compare(a, b); }
inline bool operator!=(ResourceRequest& a, const ResourceRequest& b) { return !(a == b); }

struct CrossThreadResourceRequestData {
    WTF_MAKE_NONCOPYABLE(CrossThreadResourceRequestData); WTF_MAKE_FAST_ALLOCATED;
public:
    CrossThreadResourceRequestData() { }
    KURL m_url;

    ResourceRequestCachePolicy m_cachePolicy;
    double m_timeoutInterval;
    KURL m_firstPartyForCookies;

    String m_httpMethod;
    OwnPtr<CrossThreadHTTPHeaderMapData> m_httpHeaders;
    RefPtr<FormData> m_httpBody;
    bool m_allowStoredCredentials;
    bool m_reportUploadProgress;
    bool m_hasUserGesture;
    bool m_downloadToFile;
    ResourceLoadPriority m_priority;
    int m_requestorID;
    int m_requestorProcessID;
    int m_appCacheHostID;
    ResourceRequest::TargetType m_targetType;
    ReferrerPolicy m_referrerPolicy;
};

unsigned initializeMaximumHTTPConnectionCountPerHost();

} // namespace WebCore

#endif // ResourceRequest_h
