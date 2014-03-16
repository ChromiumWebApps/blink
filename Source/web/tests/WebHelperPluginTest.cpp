// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "WebHelperPlugin.h"

#include "FakeWebPlugin.h"
#include "FrameTestHelpers.h"
#include "WebFrameClient.h"
#include <gtest/gtest.h>

namespace blink {

namespace {

class FakePlaceholderWebPlugin : public FakeWebPlugin {
public:
    FakePlaceholderWebPlugin(blink::WebFrame* frame, const blink::WebPluginParams& params)
        : FakeWebPlugin(frame, params)
    {
    }
    virtual ~FakePlaceholderWebPlugin() { }

    virtual bool isPlaceholder() OVERRIDE { return true; }
};

class WebHelperPluginFrameClient : public FrameTestHelpers::TestWebFrameClient {
public:
    WebHelperPluginFrameClient() : m_createPlaceholder(false) { }
    virtual ~WebHelperPluginFrameClient() { }

    virtual WebPlugin* createPlugin(WebFrame* frame, const WebPluginParams& params) OVERRIDE
    {
        return m_createPlaceholder ? new FakePlaceholderWebPlugin(frame, params) : new FakeWebPlugin(frame, params);
    }

    void setCreatePlaceholder(bool createPlaceholder) { m_createPlaceholder = createPlaceholder; }

private:
    bool m_createPlaceholder;
};

class WebHelperPluginTest : public testing::Test {
protected:
    virtual void SetUp() OVERRIDE
    {
        m_helper.initializeAndLoad("about:blank", false, &m_frameClient);
    }


    void destroyHelperPlugin()
    {
        m_plugin.clear();
        // WebHelperPlugin is destroyed by a task posted to the message loop.
        FrameTestHelpers::runPendingTasks();
    }

    FrameTestHelpers::WebViewHelper m_helper;
    WebHelperPluginFrameClient m_frameClient;
    OwnPtr<WebHelperPlugin> m_plugin;
};

TEST_F(WebHelperPluginTest, CreateAndDestroyAfterWebViewDestruction)
{
    m_plugin = adoptPtr(WebHelperPlugin::create("hello", m_helper.webView()->mainFrame()));
    EXPECT_TRUE(m_plugin);
    EXPECT_TRUE(m_plugin->getPlugin());

    m_helper.reset();
    destroyHelperPlugin();
}

TEST_F(WebHelperPluginTest, CreateAndDestroyBeforeWebViewDestruction)
{
    m_plugin = adoptPtr(WebHelperPlugin::create("hello", m_helper.webView()->mainFrame()));
    EXPECT_TRUE(m_plugin);
    EXPECT_TRUE(m_plugin->getPlugin());

    destroyHelperPlugin();
    m_helper.reset();
}

TEST_F(WebHelperPluginTest, CreateFailsWithPlaceholder)
{
    m_frameClient.setCreatePlaceholder(true);

    m_plugin = adoptPtr(WebHelperPlugin::create("hello", m_helper.webView()->mainFrame()));
    EXPECT_EQ(0, m_plugin.get());
}

} // namespace

} // namespace
