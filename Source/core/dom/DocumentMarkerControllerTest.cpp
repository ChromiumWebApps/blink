/*
 * Copyright (c) 2013, Google Inc. All rights reserved.
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
#include "core/dom/DocumentMarkerController.h"

#include "bindings/v8/ExceptionStatePlaceholder.h"
#include "core/dom/Document.h"
#include "core/dom/Range.h"
#include "core/dom/Text.h"
#include "core/html/HTMLElement.h"
#include "core/testing/DummyPageHolder.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefPtr.h"
#include "wtf/testing/WTFTestHelpers.h"
#include <gtest/gtest.h>

using namespace WebCore;

namespace {

class DocumentMarkerControllerTest : public ::testing::Test {
protected:
    virtual void SetUp() OVERRIDE;

    Document& document() const { return *m_document; }
    DocumentMarkerController& markerController() const { return m_document->markers(); }

    PassRefPtr<Text> createTextNode(const char*);
    void markNodeContents(PassRefPtr<Node>);
    void setBodyInnerHTML(const char*);

private:
    OwnPtr<DummyPageHolder> m_dummyPageHolder;
    Document* m_document;
};

void DocumentMarkerControllerTest::SetUp()
{
    m_dummyPageHolder = DummyPageHolder::create(IntSize(800, 600));
    m_document = &m_dummyPageHolder->document();
    ASSERT(m_document);
}

PassRefPtr<Text> DocumentMarkerControllerTest::createTextNode(const char* textContents)
{
    return document().createTextNode(String::fromUTF8(textContents));
}

void DocumentMarkerControllerTest::markNodeContents(PassRefPtr<Node> node)
{
    // Force renderers to be created; TextIterator, which is used in
    // DocumentMarkerControllerTest::addMarker(), needs them.
    document().updateLayout();
    RefPtr<Range> range = rangeOfContents(node.get());
    markerController().addMarker(range.get(), DocumentMarker::Spelling);
}

void DocumentMarkerControllerTest::setBodyInnerHTML(const char* bodyContent)
{
    document().body()->setInnerHTML(String::fromUTF8(bodyContent), ASSERT_NO_EXCEPTION);
}

TEST_F(DocumentMarkerControllerTest, NodeWillBeRemovedMarkedByNormalize)
{
    setBodyInnerHTML("<b><i>foo</i></b>");
    {
        RefPtr<Element> parent = toElement(document().body()->firstChild()->firstChild());
        parent->appendChild(createTextNode("bar").get());
        markNodeContents(parent.get());
        EXPECT_EQ(2u, markerController().markers().size());
        parent->normalize();
    }
    // No more reference to marked node.
    EXPECT_EQ(1u, markerController().markers().size());
}

TEST_F(DocumentMarkerControllerTest, NodeWillBeRemovedMarkedByRemoveChildren)
{
    setBodyInnerHTML("<b><i>foo</i></b>");
    RefPtr<Element> parent = toElement(document().body()->firstChild()->firstChild());
    markNodeContents(parent.get());
    EXPECT_EQ(1u, markerController().markers().size());
    parent->removeChildren();
    // No more reference to marked node.
    EXPECT_EQ(0u, markerController().markers().size());
}

TEST_F(DocumentMarkerControllerTest, NodeWillBeRemovedByRemoveMarked)
{
    setBodyInnerHTML("<b><i>foo</i></b>");
    {
        RefPtr<Element> parent = toElement(document().body()->firstChild()->firstChild());
        markNodeContents(parent);
        EXPECT_EQ(1u, markerController().markers().size());
        parent->removeChild(parent->firstChild());
    }
    // No more reference to marked node.
    EXPECT_EQ(0u, markerController().markers().size());
}

TEST_F(DocumentMarkerControllerTest, NodeWillBeRemovedMarkedByRemoveAncestor)
{
    setBodyInnerHTML("<b><i>foo</i></b>");
    {
        RefPtr<Element> parent = toElement(document().body()->firstChild()->firstChild());
        markNodeContents(parent);
        EXPECT_EQ(1u, markerController().markers().size());
        parent->parentNode()->parentNode()->removeChild(parent->parentNode());
    }
    // No more reference to marked node.
    EXPECT_EQ(0u, markerController().markers().size());
}

TEST_F(DocumentMarkerControllerTest, NodeWillBeRemovedMarkedByRemoveParent)
{
    setBodyInnerHTML("<b><i>foo</i></b>");
    {
        RefPtr<Element> parent = toElement(document().body()->firstChild()->firstChild());
        markNodeContents(parent);
        EXPECT_EQ(1u, markerController().markers().size());
        parent->parentNode()->removeChild(parent.get());
    }
    // No more reference to marked node.
    EXPECT_EQ(0u, markerController().markers().size());
}

TEST_F(DocumentMarkerControllerTest, NodeWillBeRemovedMarkedByReplaceChild)
{
    setBodyInnerHTML("<b><i>foo</i></b>");
    {
        RefPtr<Element> parent = toElement(document().body()->firstChild()->firstChild());
        markNodeContents(parent.get());
        EXPECT_EQ(1u, markerController().markers().size());
        parent->replaceChild(createTextNode("bar").get(), parent->firstChild());
    }
    // No more reference to marked node.
    EXPECT_EQ(0u, markerController().markers().size());
}

TEST_F(DocumentMarkerControllerTest, NodeWillBeRemovedBySetInnerHTML)
{
    setBodyInnerHTML("<b><i>foo</i></b>");
    {
        RefPtr<Element> parent = toElement(document().body()->firstChild()->firstChild());
        markNodeContents(parent);
        EXPECT_EQ(1u, markerController().markers().size());
        setBodyInnerHTML("");
    }
    // No more reference to marked node.
    EXPECT_EQ(0u, markerController().markers().size());
}

}
