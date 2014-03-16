/*
 * Copyright (c) 2009, Google Inc.  All rights reserved.
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

#ifndef ApplicationCacheHost_h
#define ApplicationCacheHost_h

#include "platform/weborigin/KURL.h"
#include "public/platform/WebApplicationCacheHostClient.h"
#include "wtf/Deque.h"
#include "wtf/OwnPtr.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefPtr.h"
#include "wtf/Vector.h"

namespace WebCore {
    class ApplicationCache;
    class DocumentLoader;
    class LocalFrame;
    class ResourceLoader;
    class ResourceError;
    class ResourceRequest;
    class ResourceResponse;
    class SubstituteData;

    class ApplicationCacheHost : public blink::WebApplicationCacheHostClient {
        WTF_MAKE_NONCOPYABLE(ApplicationCacheHost);
    public:
        // The Status numeric values are specified in the HTML5 spec.
        enum Status {
            UNCACHED = 0,
            IDLE = 1,
            CHECKING = 2,
            DOWNLOADING = 3,
            UPDATEREADY = 4,
            OBSOLETE = 5
        };

        enum EventID {
            CHECKING_EVENT = 0,
            ERROR_EVENT,
            NOUPDATE_EVENT,
            DOWNLOADING_EVENT,
            PROGRESS_EVENT,
            UPDATEREADY_EVENT,
            CACHED_EVENT,
            OBSOLETE_EVENT  // Must remain the last value, this is used to size arrays.
        };

        struct CacheInfo {
            CacheInfo(const KURL& manifest, double creationTime, double updateTime, long long size)
                : m_manifest(manifest)
                , m_creationTime(creationTime)
                , m_updateTime(updateTime)
                , m_size(size) { }
            KURL m_manifest;
            double m_creationTime;
            double m_updateTime;
            long long m_size;
        };

        struct ResourceInfo {
            ResourceInfo(const KURL& resource, bool isMaster, bool isManifest, bool isFallback, bool isForeign, bool isExplicit, long long size)
                : m_resource(resource)
                , m_isMaster(isMaster)
                , m_isManifest(isManifest)
                , m_isFallback(isFallback)
                , m_isForeign(isForeign)
                , m_isExplicit(isExplicit)
                , m_size(size) { }
            KURL m_resource;
            bool m_isMaster;
            bool m_isManifest;
            bool m_isFallback;
            bool m_isForeign;
            bool m_isExplicit;
            long long m_size;
        };

        typedef Vector<ResourceInfo> ResourceInfoList;

        explicit ApplicationCacheHost(DocumentLoader*);
        virtual ~ApplicationCacheHost();

        void selectCacheWithoutManifest();
        void selectCacheWithManifest(const KURL& manifestURL);

        void willStartLoadingMainResource(ResourceRequest&);
        void didReceiveResponseForMainResource(const ResourceResponse&);
        void mainResourceDataReceived(const char* data, int length);
        void finishedLoadingMainResource();
        void failedLoadingMainResource();

        void willStartLoadingResource(ResourceRequest&);

        Status status() const;
        bool update();
        bool swapCache();
        void abort();

        void setApplicationCache(ApplicationCache*);
        void notifyApplicationCache(EventID, int progressTotal, int progressDone);

        void stopDeferringEvents(); // Also raises the events that have been queued up.

        void fillResourceList(ResourceInfoList*);
        CacheInfo applicationCacheInfo();

    private:
        // WebApplicationCacheHostClient implementation
        virtual void didChangeCacheAssociation() OVERRIDE FINAL;
        virtual void notifyEventListener(blink::WebApplicationCacheHost::EventID) OVERRIDE FINAL;
        virtual void notifyProgressEventListener(const blink::WebURL&, int progressTotal, int progressDone) OVERRIDE FINAL;

        bool isApplicationCacheEnabled();
        DocumentLoader* documentLoader() const { return m_documentLoader; }

        struct DeferredEvent {
            EventID eventID;
            int progressTotal;
            int progressDone;
            DeferredEvent(EventID id, int total, int done) : eventID(id), progressTotal(total), progressDone(done) { }
        };

        ApplicationCache* m_domApplicationCache;
        DocumentLoader* m_documentLoader;
        bool m_defersEvents; // Events are deferred until after document onload.
        Vector<DeferredEvent> m_deferredEvents;

        void dispatchDOMEvent(EventID, int progressTotal, int progressDone);

        OwnPtr<blink::WebApplicationCacheHost> m_host;
    };

}  // namespace WebCore

#endif  // ApplicationCacheHost_h
