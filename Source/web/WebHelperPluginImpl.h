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

#ifndef WebHelperPluginImpl_h
#define WebHelperPluginImpl_h

#include "WebHelperPlugin.h"
#include "platform/Timer.h"
#include "wtf/FastAllocBase.h"
#include "wtf/Noncopyable.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/RefPtr.h"
#include "wtf/text/WTFString.h"

namespace WebCore {
class HTMLObjectElement;
} // namespace WebCore

namespace blink {

class WebFrameImpl;
class WebPluginContainerImpl;

// Utility class to host helper plugins for media. Internally, it creates a detached
// HTMLPluginElement to host the plugin and uses FrameLoaderClient::createPlugin() to instantiate
// the requested plugin.
class WebHelperPluginImpl FINAL : public WebHelperPlugin {
    WTF_MAKE_NONCOPYABLE(WebHelperPluginImpl);
    WTF_MAKE_FAST_ALLOCATED;
public:
    // WebHelperPlugin methods:
    virtual WebPlugin* getPlugin() OVERRIDE;
    virtual void destroy() OVERRIDE;

private:
    friend class WebHelperPlugin;

    WebHelperPluginImpl();
    virtual ~WebHelperPluginImpl();

    bool initialize(const String& pluginType, WebFrameImpl*);
    void reallyDestroy(WebCore::Timer<WebHelperPluginImpl>*);

    WebCore::Timer<WebHelperPluginImpl> m_destructionTimer;
    RefPtr<WebCore::HTMLObjectElement> m_objectElement;
    RefPtr<WebPluginContainerImpl> m_pluginContainer;
};

} // namespace blink

#endif // WebHelperPluginImpl_h
