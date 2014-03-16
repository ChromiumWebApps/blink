/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

// How ownership works
// -------------------
//
// Big oh represents a refcounted relationship: owner O--- ownee
//
// WebView (for the toplevel frame only)
//    O
//    |           WebFrame
//    |              O
//    |              |
//   Page O------- LocalFrame (m_mainFrame) O-------O FrameView
//                   ||
//                   ||
//               FrameLoader
//
// FrameLoader and LocalFrame are formerly one object that was split apart because
// it got too big. They basically have the same lifetime, hence the double line.
//
// From the perspective of the embedder, WebFrame is simply an object that it
// allocates by calling WebFrame::create() and must be freed by calling close().
// Internally, WebFrame is actually refcounted and it holds a reference to its
// corresponding LocalFrame in WebCore.
//
// How frames are destroyed
// ------------------------
//
// The main frame is never destroyed and is re-used. The FrameLoader is re-used
// and a reference to the main frame is kept by the Page.
//
// When frame content is replaced, all subframes are destroyed. This happens
// in FrameLoader::detachFromParent for each subframe in a pre-order depth-first
// traversal. Note that child node order may not match DOM node order!
// detachFromParent() calls FrameLoaderClient::detachedFromParent(), which calls
// WebFrame::frameDetached(). This triggers WebFrame to clear its reference to
// LocalFrame, and also notifies the embedder via WebFrameClient that the frame is
// detached. Most embedders will invoke close() on the WebFrame at this point,
// triggering its deletion unless something else is still retaining a reference.
//
// Thie client is expected to be set whenever the WebFrameImpl is attached to
// the DOM.

#include "config.h"
#include "WebFrameImpl.h"

#include <algorithm>
#include "AssociatedURLLoader.h"
#include "DOMUtilitiesPrivate.h"
#include "EventListenerWrapper.h"
#include "FindInPageCoordinates.h"
#include "HTMLNames.h"
#include "PageOverlay.h"
#include "SharedWorkerRepositoryClientImpl.h"
#include "WebConsoleMessage.h"
#include "WebDOMEvent.h"
#include "WebDOMEventListener.h"
#include "WebDataSourceImpl.h"
#include "WebDevToolsAgentPrivate.h"
#include "WebDocument.h"
#include "WebFindOptions.h"
#include "WebFormElement.h"
#include "WebFrameClient.h"
#include "WebHistoryItem.h"
#include "WebIconURL.h"
#include "WebInputElement.h"
#include "WebNode.h"
#include "WebPerformance.h"
#include "WebPlugin.h"
#include "WebPluginContainerImpl.h"
#include "WebPrintParams.h"
#include "WebRange.h"
#include "WebScriptSource.h"
#include "WebSecurityOrigin.h"
#include "WebSerializedScriptValue.h"
#include "WebViewImpl.h"
#include "bindings/v8/DOMWrapperWorld.h"
#include "bindings/v8/ExceptionState.h"
#include "bindings/v8/ExceptionStatePlaceholder.h"
#include "bindings/v8/ScriptController.h"
#include "bindings/v8/ScriptSourceCode.h"
#include "bindings/v8/ScriptValue.h"
#include "bindings/v8/V8GCController.h"
#include "bindings/v8/V8PerIsolateData.h"
#include "core/dom/Document.h"
#include "core/dom/DocumentMarker.h"
#include "core/dom/DocumentMarkerController.h"
#include "core/dom/IconURL.h"
#include "core/dom/MessagePort.h"
#include "core/dom/Node.h"
#include "core/dom/NodeTraversal.h"
#include "core/dom/shadow/ShadowRoot.h"
#include "core/editing/Editor.h"
#include "core/editing/FrameSelection.h"
#include "core/editing/InputMethodController.h"
#include "core/editing/PlainTextRange.h"
#include "core/editing/SpellChecker.h"
#include "core/editing/TextAffinity.h"
#include "core/editing/TextIterator.h"
#include "core/editing/htmlediting.h"
#include "core/editing/markup.h"
#include "core/frame/Console.h"
#include "core/frame/DOMWindow.h"
#include "core/frame/FrameView.h"
#include "core/html/HTMLCollection.h"
#include "core/html/HTMLFormElement.h"
#include "core/html/HTMLFrameOwnerElement.h"
#include "core/html/HTMLHeadElement.h"
#include "core/html/HTMLInputElement.h"
#include "core/html/HTMLLinkElement.h"
#include "core/html/PluginDocument.h"
#include "core/inspector/InspectorController.h"
#include "core/inspector/ScriptCallStack.h"
#include "core/loader/DocumentLoader.h"
#include "core/loader/FrameLoadRequest.h"
#include "core/loader/FrameLoader.h"
#include "core/loader/HistoryItem.h"
#include "core/loader/SubstituteData.h"
#include "core/page/Chrome.h"
#include "core/page/EventHandler.h"
#include "core/page/FocusController.h"
#include "core/page/FrameTree.h"
#include "core/page/Page.h"
#include "core/page/PrintContext.h"
#include "core/frame/Settings.h"
#include "core/rendering/HitTestResult.h"
#include "core/rendering/RenderBox.h"
#include "core/rendering/RenderFrame.h"
#include "core/rendering/RenderLayer.h"
#include "core/rendering/RenderObject.h"
#include "core/rendering/RenderTreeAsText.h"
#include "core/rendering/RenderView.h"
#include "core/rendering/style/StyleInheritedData.h"
#include "core/timing/Performance.h"
#include "core/xml/DocumentXPathEvaluator.h"
#include "core/xml/XPathResult.h"
#include "platform/TraceEvent.h"
#include "platform/UserGestureIndicator.h"
#include "platform/clipboard/ClipboardUtilities.h"
#include "platform/fonts/FontCache.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/GraphicsLayerClient.h"
#include "platform/graphics/skia/SkiaUtils.h"
#include "platform/network/ResourceRequest.h"
#include "platform/scroll/ScrollbarTheme.h"
#include "platform/scroll/ScrollTypes.h"
#include "platform/weborigin/KURL.h"
#include "platform/weborigin/SchemeRegistry.h"
#include "platform/weborigin/SecurityPolicy.h"
#include "public/platform/Platform.h"
#include "public/platform/WebFloatPoint.h"
#include "public/platform/WebFloatRect.h"
#include "public/platform/WebLayer.h"
#include "public/platform/WebPoint.h"
#include "public/platform/WebRect.h"
#include "public/platform/WebSize.h"
#include "public/platform/WebURLError.h"
#include "public/platform/WebVector.h"
#include "wtf/CurrentTime.h"
#include "wtf/HashMap.h"

using namespace WebCore;

namespace blink {

static int frameCount = 0;

// Key for a StatsCounter tracking how many WebFrames are active.
static const char webFrameActiveCount[] = "WebFrameActiveCount";

static void frameContentAsPlainText(size_t maxChars, LocalFrame* frame, StringBuilder& output)
{
    Document* document = frame->document();
    if (!document)
        return;

    if (!frame->view())
        return;

    // TextIterator iterates over the visual representation of the DOM. As such,
    // it requires you to do a layout before using it (otherwise it'll crash).
    document->updateLayout();

    // Select the document body.
    RefPtr<Range> range(document->createRange());
    TrackExceptionState exceptionState;
    range->selectNodeContents(document->body(), exceptionState);

    if (!exceptionState.hadException()) {
        // The text iterator will walk nodes giving us text. This is similar to
        // the plainText() function in core/editing/TextIterator.h, but we implement the maximum
        // size and also copy the results directly into a wstring, avoiding the
        // string conversion.
        for (TextIterator it(range.get()); !it.atEnd(); it.advance()) {
            it.appendTextToStringBuilder(output, 0, maxChars - output.length());
            if (output.length() >= maxChars)
                return; // Filled up the buffer.
        }
    }

    // The separator between frames when the frames are converted to plain text.
    const LChar frameSeparator[] = { '\n', '\n' };
    const size_t frameSeparatorLength = WTF_ARRAY_LENGTH(frameSeparator);

    // Recursively walk the children.
    const FrameTree& frameTree = frame->tree();
    for (LocalFrame* curChild = frameTree.firstChild(); curChild; curChild = curChild->tree().nextSibling()) {
        // Ignore the text of non-visible frames.
        RenderView* contentRenderer = curChild->contentRenderer();
        RenderPart* ownerRenderer = curChild->ownerRenderer();
        if (!contentRenderer || !contentRenderer->width() || !contentRenderer->height()
            || (contentRenderer->x() + contentRenderer->width() <= 0) || (contentRenderer->y() + contentRenderer->height() <= 0)
            || (ownerRenderer && ownerRenderer->style() && ownerRenderer->style()->visibility() != VISIBLE)) {
            continue;
        }

        // Make sure the frame separator won't fill up the buffer, and give up if
        // it will. The danger is if the separator will make the buffer longer than
        // maxChars. This will cause the computation above:
        //   maxChars - output->size()
        // to be a negative number which will crash when the subframe is added.
        if (output.length() >= maxChars - frameSeparatorLength)
            return;

        output.append(frameSeparator, frameSeparatorLength);
        frameContentAsPlainText(maxChars, curChild, output);
        if (output.length() >= maxChars)
            return; // Filled up the buffer.
    }
}

WebPluginContainerImpl* WebFrameImpl::pluginContainerFromFrame(LocalFrame* frame)
{
    if (!frame)
        return 0;
    if (!frame->document() || !frame->document()->isPluginDocument())
        return 0;
    PluginDocument* pluginDocument = toPluginDocument(frame->document());
    return toWebPluginContainerImpl(pluginDocument->pluginWidget());
}

WebPluginContainerImpl* WebFrameImpl::pluginContainerFromNode(WebCore::LocalFrame* frame, const WebNode& node)
{
    WebPluginContainerImpl* pluginContainer = pluginContainerFromFrame(frame);
    if (pluginContainer)
        return pluginContainer;
    return toWebPluginContainerImpl(node.pluginContainer());
}

// Simple class to override some of PrintContext behavior. Some of the methods
// made virtual so that they can be overridden by ChromePluginPrintContext.
class ChromePrintContext : public PrintContext {
    WTF_MAKE_NONCOPYABLE(ChromePrintContext);
public:
    ChromePrintContext(LocalFrame* frame)
        : PrintContext(frame)
        , m_printedPageWidth(0)
    {
    }

    virtual ~ChromePrintContext() { }

    virtual void begin(float width, float height)
    {
        ASSERT(!m_printedPageWidth);
        m_printedPageWidth = width;
        PrintContext::begin(m_printedPageWidth, height);
    }

    virtual void end()
    {
        PrintContext::end();
    }

    virtual float getPageShrink(int pageNumber) const
    {
        IntRect pageRect = m_pageRects[pageNumber];
        return m_printedPageWidth / pageRect.width();
    }

    // Spools the printed page, a subrect of frame(). Skip the scale step.
    // NativeTheme doesn't play well with scaling. Scaling is done browser side
    // instead. Returns the scale to be applied.
    // On Linux, we don't have the problem with NativeTheme, hence we let WebKit
    // do the scaling and ignore the return value.
    virtual float spoolPage(GraphicsContext& context, int pageNumber)
    {
        IntRect pageRect = m_pageRects[pageNumber];
        float scale = m_printedPageWidth / pageRect.width();

        context.save();
#if OS(POSIX) && !OS(MACOSX)
        context.scale(WebCore::FloatSize(scale, scale));
#endif
        context.translate(static_cast<float>(-pageRect.x()), static_cast<float>(-pageRect.y()));
        context.clip(pageRect);
        frame()->view()->paintContents(&context, pageRect);
        if (context.supportsURLFragments())
            outputLinkedDestinations(context, frame()->document(), pageRect);
        context.restore();
        return scale;
    }

    void spoolAllPagesWithBoundaries(GraphicsContext& graphicsContext, const FloatSize& pageSizeInPixels)
    {
        if (!frame()->document() || !frame()->view() || !frame()->document()->renderer())
            return;

        frame()->document()->updateLayout();

        float pageHeight;
        computePageRects(FloatRect(FloatPoint(0, 0), pageSizeInPixels), 0, 0, 1, pageHeight);

        const float pageWidth = pageSizeInPixels.width();
        size_t numPages = pageRects().size();
        int totalHeight = numPages * (pageSizeInPixels.height() + 1) - 1;

        // Fill the whole background by white.
        graphicsContext.setFillColor(Color::white);
        graphicsContext.fillRect(FloatRect(0, 0, pageWidth, totalHeight));

        int currentHeight = 0;
        for (size_t pageIndex = 0; pageIndex < numPages; pageIndex++) {
            // Draw a line for a page boundary if this isn't the first page.
            if (pageIndex > 0) {
                graphicsContext.save();
                graphicsContext.setStrokeColor(Color(0, 0, 255));
                graphicsContext.setFillColor(Color(0, 0, 255));
                graphicsContext.drawLine(IntPoint(0, currentHeight), IntPoint(pageWidth, currentHeight));
                graphicsContext.restore();
            }

            graphicsContext.save();

            graphicsContext.translate(0, currentHeight);
#if OS(WIN) || OS(MACOSX)
            // Account for the disabling of scaling in spoolPage. In the context
            // of spoolAllPagesWithBoundaries the scale HAS NOT been pre-applied.
            float scale = getPageShrink(pageIndex);
            graphicsContext.scale(WebCore::FloatSize(scale, scale));
#endif
            spoolPage(graphicsContext, pageIndex);
            graphicsContext.restore();

            currentHeight += pageSizeInPixels.height() + 1;
        }
    }

    virtual void computePageRects(const FloatRect& printRect, float headerHeight, float footerHeight, float userScaleFactor, float& outPageHeight)
    {
        PrintContext::computePageRects(printRect, headerHeight, footerHeight, userScaleFactor, outPageHeight);
    }

    virtual int pageCount() const
    {
        return PrintContext::pageCount();
    }

private:
    // Set when printing.
    float m_printedPageWidth;
};

// Simple class to override some of PrintContext behavior. This is used when
// the frame hosts a plugin that supports custom printing. In this case, we
// want to delegate all printing related calls to the plugin.
class ChromePluginPrintContext : public ChromePrintContext {
public:
    ChromePluginPrintContext(LocalFrame* frame, WebPluginContainerImpl* plugin, const WebPrintParams& printParams)
        : ChromePrintContext(frame), m_plugin(plugin), m_pageCount(0), m_printParams(printParams)
    {
    }

    virtual ~ChromePluginPrintContext() { }

    virtual void begin(float width, float height)
    {
    }

    virtual void end()
    {
        m_plugin->printEnd();
    }

    virtual float getPageShrink(int pageNumber) const
    {
        // We don't shrink the page (maybe we should ask the widget ??)
        return 1.0;
    }

    virtual void computePageRects(const FloatRect& printRect, float headerHeight, float footerHeight, float userScaleFactor, float& outPageHeight)
    {
        m_printParams.printContentArea = IntRect(printRect);
        m_pageCount = m_plugin->printBegin(m_printParams);
    }

    virtual int pageCount() const
    {
        return m_pageCount;
    }

    // Spools the printed page, a subrect of frame(). Skip the scale step.
    // NativeTheme doesn't play well with scaling. Scaling is done browser side
    // instead. Returns the scale to be applied.
    virtual float spoolPage(GraphicsContext& context, int pageNumber)
    {
        m_plugin->printPage(pageNumber, &context);
        return 1.0;
    }

private:
    // Set when printing.
    WebPluginContainerImpl* m_plugin;
    int m_pageCount;
    WebPrintParams m_printParams;

};

static WebDataSource* DataSourceForDocLoader(DocumentLoader* loader)
{
    return loader ? WebDataSourceImpl::fromDocumentLoader(loader) : 0;
}

WebFrameImpl::FindMatch::FindMatch(PassRefPtr<Range> range, int ordinal)
    : m_range(range)
    , m_ordinal(ordinal)
{
}

class WebFrameImpl::DeferredScopeStringMatches {
public:
    DeferredScopeStringMatches(WebFrameImpl* webFrame, int identifier, const WebString& searchText, const WebFindOptions& options, bool reset)
        : m_timer(this, &DeferredScopeStringMatches::doTimeout)
        , m_webFrame(webFrame)
        , m_identifier(identifier)
        , m_searchText(searchText)
        , m_options(options)
        , m_reset(reset)
    {
        m_timer.startOneShot(0.0, FROM_HERE);
    }

private:
    void doTimeout(Timer<DeferredScopeStringMatches>*)
    {
        m_webFrame->callScopeStringMatches(this, m_identifier, m_searchText, m_options, m_reset);
    }

    Timer<DeferredScopeStringMatches> m_timer;
    RefPtr<WebFrameImpl> m_webFrame;
    int m_identifier;
    WebString m_searchText;
    WebFindOptions m_options;
    bool m_reset;
};

// WebFrame -------------------------------------------------------------------

int WebFrame::instanceCount()
{
    return frameCount;
}

WebFrame* WebFrame::frameForCurrentContext()
{
    v8::Handle<v8::Context> context = v8::Isolate::GetCurrent()->GetCurrentContext();
    if (context.IsEmpty())
        return 0;
    return frameForContext(context);
}

WebFrame* WebFrame::frameForContext(v8::Handle<v8::Context> context)
{
   return WebFrameImpl::fromFrame(toFrameIfNotDetached(context));
}

WebFrame* WebFrame::fromFrameOwnerElement(const WebElement& element)
{
    return WebFrameImpl::fromFrameOwnerElement(PassRefPtr<Element>(element).get());
}

void WebFrameImpl::close()
{
    m_client = 0;
    deref(); // Balances ref() acquired in WebFrame::create
}

WebString WebFrameImpl::uniqueName() const
{
    return frame()->tree().uniqueName();
}

WebString WebFrameImpl::assignedName() const
{
    return frame()->tree().name();
}

void WebFrameImpl::setName(const WebString& name)
{
    frame()->tree().setName(name);
}

WebVector<WebIconURL> WebFrameImpl::iconURLs(int iconTypesMask) const
{
    // The URL to the icon may be in the header. As such, only
    // ask the loader for the icon if it's finished loading.
    if (frame()->loader().state() == FrameStateComplete)
        return frame()->document()->iconURLs(iconTypesMask);
    return WebVector<WebIconURL>();
}

void WebFrameImpl::setIsRemote(bool isRemote)
{
    m_isRemote = isRemote;
    if (isRemote)
        client()->initializeChildFrame(frame()->view()->frameRect(), frame()->view()->visibleContentScaleFactor());
}

void WebFrameImpl::setRemoteWebLayer(WebLayer* webLayer)
{
    if (!frame())
        return;

    if (frame()->remotePlatformLayer())
        GraphicsLayer::unregisterContentsLayer(frame()->remotePlatformLayer());
    if (webLayer)
        GraphicsLayer::registerContentsLayer(webLayer);
    frame()->setRemotePlatformLayer(webLayer);
    frame()->ownerElement()->setNeedsStyleRecalc(WebCore::SubtreeStyleChange, WebCore::StyleChangeFromRenderer);
}

void WebFrameImpl::setPermissionClient(WebPermissionClient* permissionClient)
{
    m_permissionClient = permissionClient;
}

void WebFrameImpl::setSharedWorkerRepositoryClient(WebSharedWorkerRepositoryClient* client)
{
    m_sharedWorkerRepositoryClient = SharedWorkerRepositoryClientImpl::create(client);
}

WebSize WebFrameImpl::scrollOffset() const
{
    FrameView* view = frameView();
    if (!view)
        return WebSize();
    return view->scrollOffset();
}

WebSize WebFrameImpl::minimumScrollOffset() const
{
    FrameView* view = frameView();
    if (!view)
        return WebSize();
    return toIntSize(view->minimumScrollPosition());
}

WebSize WebFrameImpl::maximumScrollOffset() const
{
    FrameView* view = frameView();
    if (!view)
        return WebSize();
    return toIntSize(view->maximumScrollPosition());
}

void WebFrameImpl::setScrollOffset(const WebSize& offset)
{
    if (FrameView* view = frameView())
        view->setScrollOffset(IntPoint(offset.width, offset.height));
}

WebSize WebFrameImpl::contentsSize() const
{
    return frame()->view()->contentsSize();
}

bool WebFrameImpl::hasVisibleContent() const
{
    return frame()->view()->visibleWidth() > 0 && frame()->view()->visibleHeight() > 0;
}

WebRect WebFrameImpl::visibleContentRect() const
{
    return frame()->view()->visibleContentRect();
}

bool WebFrameImpl::hasHorizontalScrollbar() const
{
    return frame() && frame()->view() && frame()->view()->horizontalScrollbar();
}

bool WebFrameImpl::hasVerticalScrollbar() const
{
    return frame() && frame()->view() && frame()->view()->verticalScrollbar();
}

WebView* WebFrameImpl::view() const
{
    return viewImpl();
}

WebFrame* WebFrameImpl::opener() const
{
    return m_opener;
}

void WebFrameImpl::setOpener(WebFrame* opener)
{
    WebFrameImpl* openerImpl = toWebFrameImpl(opener);
    if (m_opener && !openerImpl && m_client)
        m_client->didDisownOpener(this);

    if (m_opener)
        m_opener->m_openedFrames.remove(this);
    if (openerImpl)
        openerImpl->m_openedFrames.add(this);
    m_opener = openerImpl;

    ASSERT(m_frame);
    if (m_frame && m_frame->document())
        m_frame->document()->initSecurityContext();
}

void WebFrameImpl::appendChild(WebFrame* child)
{
    // FIXME: Original code asserts that the frames have the same Page. We
    // should add an equivalent check... figure out what.
    WebFrameImpl* childImpl = toWebFrameImpl(child);
    childImpl->m_parent = this;
    WebFrameImpl* oldLast = m_lastChild;
    m_lastChild = childImpl;

    if (oldLast) {
        childImpl->m_previousSibling = oldLast;
        oldLast->m_nextSibling = childImpl;
    } else {
        m_firstChild = childImpl;
    }
    // FIXME: Not sure if this is a legitimate assert.
    ASSERT(frame());
    frame()->tree().invalidateScopedChildCount();
}

void WebFrameImpl::removeChild(WebFrame* child)
{
    WebFrameImpl* childImpl = toWebFrameImpl(child);
    childImpl->m_parent = 0;

    if (m_firstChild == childImpl)
        m_firstChild = childImpl->m_nextSibling;
    else
        childImpl->m_previousSibling->m_nextSibling = childImpl->m_nextSibling;

    if (m_lastChild == childImpl)
        m_lastChild = childImpl->m_previousSibling;
    else
        childImpl->m_nextSibling->m_previousSibling = childImpl->m_previousSibling;

    childImpl->m_previousSibling = childImpl->m_nextSibling = 0;
    // FIXME: Not sure if this is a legitimate assert.
    ASSERT(frame());
    frame()->tree().invalidateScopedChildCount();
}

WebFrame* WebFrameImpl::parent() const
{
    return m_parent;
}

WebFrame* WebFrameImpl::top() const
{
    WebFrameImpl* frame = const_cast<WebFrameImpl*>(this);
    for (WebFrameImpl* parent = frame; parent; parent = parent->m_parent)
        frame = parent;
    return frame;
}

WebFrame* WebFrameImpl::previousSibling() const
{
    return m_previousSibling;
}

WebFrame* WebFrameImpl::nextSibling() const
{
    return m_nextSibling;
}

WebFrame* WebFrameImpl::firstChild() const
{
    return m_firstChild;
}

WebFrame* WebFrameImpl::lastChild() const
{
    return m_lastChild;
}

WebFrame* WebFrameImpl::traversePrevious(bool wrap) const
{
    if (!frame())
        return 0;
    return fromFrame(frame()->tree().traversePreviousWithWrap(wrap));
}

WebFrame* WebFrameImpl::traverseNext(bool wrap) const
{
    if (!frame())
        return 0;
    return fromFrame(frame()->tree().traverseNextWithWrap(wrap));
}

WebFrame* WebFrameImpl::findChildByName(const WebString& name) const
{
    if (!frame())
        return 0;
    return fromFrame(frame()->tree().child(name));
}

WebFrame* WebFrameImpl::findChildByExpression(const WebString& xpath) const
{
    if (xpath.isEmpty())
        return 0;

    Document* document = frame()->document();
    ASSERT(document);

    RefPtrWillBeRawPtr<XPathResult> xpathResult = DocumentXPathEvaluator::evaluate(*document, xpath, document, nullptr, XPathResult::ORDERED_NODE_ITERATOR_TYPE, 0, IGNORE_EXCEPTION);
    if (!xpathResult)
        return 0;

    Node* node = xpathResult->iterateNext(IGNORE_EXCEPTION);
    if (!node || !node->isFrameOwnerElement())
        return 0;
    return fromFrame(toHTMLFrameOwnerElement(node)->contentFrame());
}

WebDocument WebFrameImpl::document() const
{
    if (!frame() || !frame()->document())
        return WebDocument();
    return WebDocument(frame()->document());
}

WebPerformance WebFrameImpl::performance() const
{
    if (!frame())
        return WebPerformance();
    return WebPerformance(&frame()->domWindow()->performance());
}

NPObject* WebFrameImpl::windowObject() const
{
    if (!frame())
        return 0;
    return frame()->script().windowScriptNPObject();
}

void WebFrameImpl::bindToWindowObject(const WebString& name, NPObject* object)
{
    bindToWindowObject(name, object, 0);
}

void WebFrameImpl::bindToWindowObject(const WebString& name, NPObject* object, void*)
{
    if (!frame() || !frame()->script().canExecuteScripts(NotAboutToExecuteScript))
        return;
    frame()->script().bindToWindowObject(frame(), String(name), object);
}

void WebFrameImpl::executeScript(const WebScriptSource& source)
{
    ASSERT(frame());
    TextPosition position(OrdinalNumber::fromOneBasedInt(source.startLine), OrdinalNumber::first());
    frame()->script().executeScriptInMainWorld(ScriptSourceCode(source.code, source.url, position));
}

void WebFrameImpl::executeScriptInIsolatedWorld(int worldID, const WebScriptSource* sourcesIn, unsigned numSources, int extensionGroup)
{
    ASSERT(frame());
    RELEASE_ASSERT(worldID > 0);
    RELEASE_ASSERT(worldID < EmbedderWorldIdLimit);

    Vector<ScriptSourceCode> sources;
    for (unsigned i = 0; i < numSources; ++i) {
        TextPosition position(OrdinalNumber::fromOneBasedInt(sourcesIn[i].startLine), OrdinalNumber::first());
        sources.append(ScriptSourceCode(sourcesIn[i].code, sourcesIn[i].url, position));
    }

    frame()->script().executeScriptInIsolatedWorld(worldID, sources, extensionGroup, 0);
}

void WebFrameImpl::setIsolatedWorldSecurityOrigin(int worldID, const WebSecurityOrigin& securityOrigin)
{
    ASSERT(frame());
    DOMWrapperWorld::setIsolatedWorldSecurityOrigin(worldID, securityOrigin.get());
}

void WebFrameImpl::setIsolatedWorldContentSecurityPolicy(int worldID, const WebString& policy)
{
    ASSERT(frame());
    DOMWrapperWorld::setIsolatedWorldContentSecurityPolicy(worldID, policy);
}

void WebFrameImpl::addMessageToConsole(const WebConsoleMessage& message)
{
    ASSERT(frame());

    MessageLevel webCoreMessageLevel;
    switch (message.level) {
    case WebConsoleMessage::LevelDebug:
        webCoreMessageLevel = DebugMessageLevel;
        break;
    case WebConsoleMessage::LevelLog:
        webCoreMessageLevel = LogMessageLevel;
        break;
    case WebConsoleMessage::LevelWarning:
        webCoreMessageLevel = WarningMessageLevel;
        break;
    case WebConsoleMessage::LevelError:
        webCoreMessageLevel = ErrorMessageLevel;
        break;
    default:
        ASSERT_NOT_REACHED();
        return;
    }

    frame()->document()->addConsoleMessage(OtherMessageSource, webCoreMessageLevel, message.text);
}

void WebFrameImpl::collectGarbage()
{
    if (!frame())
        return;
    if (!frame()->settings()->scriptEnabled())
        return;
    V8GCController::collectGarbage(v8::Isolate::GetCurrent());
}

bool WebFrameImpl::checkIfRunInsecureContent(const WebURL& url) const
{
    ASSERT(frame());
    return frame()->loader().mixedContentChecker()->canRunInsecureContent(frame()->document()->securityOrigin(), url);
}

v8::Handle<v8::Value> WebFrameImpl::executeScriptAndReturnValue(const WebScriptSource& source)
{
    ASSERT(frame());

    // FIXME: This fake user gesture is required to make a bunch of pyauto
    // tests pass. If this isn't needed in non-test situations, we should
    // consider removing this code and changing the tests.
    // http://code.google.com/p/chromium/issues/detail?id=86397
    UserGestureIndicator gestureIndicator(DefinitelyProcessingNewUserGesture);

    TextPosition position(OrdinalNumber::fromOneBasedInt(source.startLine), OrdinalNumber::first());
    return frame()->script().executeScriptInMainWorldAndReturnValue(ScriptSourceCode(source.code, source.url, position)).v8Value();
}

void WebFrameImpl::executeScriptInIsolatedWorld(int worldID, const WebScriptSource* sourcesIn, unsigned numSources, int extensionGroup, WebVector<v8::Local<v8::Value> >* results)
{
    ASSERT(frame());
    RELEASE_ASSERT(worldID > 0);
    RELEASE_ASSERT(worldID < EmbedderWorldIdLimit);

    Vector<ScriptSourceCode> sources;

    for (unsigned i = 0; i < numSources; ++i) {
        TextPosition position(OrdinalNumber::fromOneBasedInt(sourcesIn[i].startLine), OrdinalNumber::first());
        sources.append(ScriptSourceCode(sourcesIn[i].code, sourcesIn[i].url, position));
    }

    if (results) {
        Vector<ScriptValue> scriptResults;
        frame()->script().executeScriptInIsolatedWorld(worldID, sources, extensionGroup, &scriptResults);
        WebVector<v8::Local<v8::Value> > v8Results(scriptResults.size());
        for (unsigned i = 0; i < scriptResults.size(); i++)
            v8Results[i] = v8::Local<v8::Value>::New(toIsolate(frame()), scriptResults[i].v8Value());
        results->swap(v8Results);
    } else {
        frame()->script().executeScriptInIsolatedWorld(worldID, sources, extensionGroup, 0);
    }
}

v8::Handle<v8::Value> WebFrameImpl::callFunctionEvenIfScriptDisabled(v8::Handle<v8::Function> function, v8::Handle<v8::Value> receiver, int argc, v8::Handle<v8::Value> argv[])
{
    ASSERT(frame());
    return frame()->script().callFunction(function, receiver, argc, argv);
}

v8::Local<v8::Context> WebFrameImpl::mainWorldScriptContext() const
{
    return toV8Context(V8PerIsolateData::mainThreadIsolate(), frame(), DOMWrapperWorld::mainWorld());
}

void WebFrameImpl::reload(bool ignoreCache)
{
    ASSERT(frame());
    frame()->loader().reload(ignoreCache ? EndToEndReload : NormalReload);
}

void WebFrameImpl::reloadWithOverrideURL(const WebURL& overrideUrl, bool ignoreCache)
{
    ASSERT(frame());
    frame()->loader().reload(ignoreCache ? EndToEndReload : NormalReload, overrideUrl);
}

void WebFrameImpl::loadRequest(const WebURLRequest& request)
{
    ASSERT(frame());
    ASSERT(!request.isNull());
    const ResourceRequest& resourceRequest = request.toResourceRequest();

    if (resourceRequest.url().protocolIs("javascript")) {
        loadJavaScriptURL(resourceRequest.url());
        return;
    }

    frame()->loader().load(FrameLoadRequest(0, resourceRequest));
}

void WebFrameImpl::loadHistoryItem(const WebHistoryItem& item, WebURLRequest::CachePolicy cachePolicy)
{
    ASSERT(frame());
    RefPtr<HistoryItem> historyItem = PassRefPtr<HistoryItem>(item);
    ASSERT(historyItem);
    frame()->page()->historyController().goToItem(historyItem.get(), static_cast<ResourceRequestCachePolicy>(cachePolicy));
}

void WebFrameImpl::loadData(const WebData& data, const WebString& mimeType, const WebString& textEncoding, const WebURL& baseURL, const WebURL& unreachableURL, bool replace)
{
    ASSERT(frame());

    // If we are loading substitute data to replace an existing load, then
    // inherit all of the properties of that original request.  This way,
    // reload will re-attempt the original request.  It is essential that
    // we only do this when there is an unreachableURL since a non-empty
    // unreachableURL informs FrameLoader::reload to load unreachableURL
    // instead of the currently loaded URL.
    ResourceRequest request;
    if (replace && !unreachableURL.isEmpty() && frame()->loader().provisionalDocumentLoader())
        request = frame()->loader().provisionalDocumentLoader()->originalRequest();
    request.setURL(baseURL);

    FrameLoadRequest frameRequest(0, request, SubstituteData(data, mimeType, textEncoding, unreachableURL));
    ASSERT(frameRequest.substituteData().isValid());
    frameRequest.setLockBackForwardList(replace);
    frame()->loader().load(frameRequest);
}

void WebFrameImpl::loadHTMLString(const WebData& data, const WebURL& baseURL, const WebURL& unreachableURL, bool replace)
{
    ASSERT(frame());
    loadData(data, WebString::fromUTF8("text/html"), WebString::fromUTF8("UTF-8"), baseURL, unreachableURL, replace);
}

bool WebFrameImpl::isLoading() const
{
    if (!frame())
        return false;
    return frame()->loader().isLoading();
}

void WebFrameImpl::stopLoading()
{
    if (!frame())
        return;
    // FIXME: Figure out what we should really do here.  It seems like a bug
    // that FrameLoader::stopLoading doesn't call stopAllLoaders.
    frame()->loader().stopAllLoaders();
}

WebDataSource* WebFrameImpl::provisionalDataSource() const
{
    ASSERT(frame());

    // We regard the policy document loader as still provisional.
    DocumentLoader* documentLoader = frame()->loader().provisionalDocumentLoader();
    if (!documentLoader)
        documentLoader = frame()->loader().policyDocumentLoader();

    return DataSourceForDocLoader(documentLoader);
}

WebDataSource* WebFrameImpl::dataSource() const
{
    ASSERT(frame());
    return DataSourceForDocLoader(frame()->loader().documentLoader());
}

WebHistoryItem WebFrameImpl::previousHistoryItem() const
{
    ASSERT(frame());
    // We use the previous item here because documentState (filled-out forms)
    // only get saved to history when it becomes the previous item.  The caller
    // is expected to query the history item after a navigation occurs, after
    // the desired history item has become the previous entry.
    return WebHistoryItem(frame()->page()->historyController().previousItemForExport());
}

WebHistoryItem WebFrameImpl::currentHistoryItem() const
{
    ASSERT(frame());

    // We're shutting down.
    if (!frame()->loader().documentLoader())
        return WebHistoryItem();

    // Lazily update the document state if it was dirtied. Doing it here
    // avoids synchronously serializing forms as they're changing.
    frame()->loader().saveDocumentState();

    return WebHistoryItem(frame()->page()->historyController().currentItemForExport());
}

void WebFrameImpl::enableViewSourceMode(bool enable)
{
    if (frame())
        frame()->setInViewSourceMode(enable);
}

bool WebFrameImpl::isViewSourceModeEnabled() const
{
    if (!frame())
        return false;
    return frame()->inViewSourceMode();
}

void WebFrameImpl::setReferrerForRequest(WebURLRequest& request, const WebURL& referrerURL)
{
    String referrer = referrerURL.isEmpty() ? frame()->document()->outgoingReferrer() : String(referrerURL.spec().utf16());
    referrer = SecurityPolicy::generateReferrerHeader(frame()->document()->referrerPolicy(), request.url(), referrer);
    if (referrer.isEmpty())
        return;
    request.setHTTPReferrer(referrer, static_cast<WebReferrerPolicy>(frame()->document()->referrerPolicy()));
}

void WebFrameImpl::dispatchWillSendRequest(WebURLRequest& request)
{
    ResourceResponse response;
    frame()->loader().client()->dispatchWillSendRequest(0, 0, request.toMutableResourceRequest(), response);
}

WebURLLoader* WebFrameImpl::createAssociatedURLLoader(const WebURLLoaderOptions& options)
{
    return new AssociatedURLLoader(this, options);
}

unsigned WebFrameImpl::unloadListenerCount() const
{
    return frame()->domWindow()->pendingUnloadEventListeners();
}

void WebFrameImpl::replaceSelection(const WebString& text)
{
    bool selectReplacement = false;
    bool smartReplace = true;
    frame()->editor().replaceSelectionWithText(text, selectReplacement, smartReplace);
}

void WebFrameImpl::insertText(const WebString& text)
{
    if (frame()->inputMethodController().hasComposition())
        frame()->inputMethodController().confirmComposition(text);
    else
        frame()->editor().insertText(text, 0);
}

void WebFrameImpl::setMarkedText(const WebString& text, unsigned location, unsigned length)
{
    Vector<CompositionUnderline> decorations;
    frame()->inputMethodController().setComposition(text, decorations, location, length);
}

void WebFrameImpl::unmarkText()
{
    frame()->inputMethodController().cancelComposition();
}

bool WebFrameImpl::hasMarkedText() const
{
    return frame()->inputMethodController().hasComposition();
}

WebRange WebFrameImpl::markedRange() const
{
    return frame()->inputMethodController().compositionRange();
}

bool WebFrameImpl::firstRectForCharacterRange(unsigned location, unsigned length, WebRect& rect) const
{
    if ((location + length < location) && (location + length))
        length = 0;

    Element* editable = frame()->selection().rootEditableElementOrDocumentElement();
    ASSERT(editable);
    RefPtr<Range> range = PlainTextRange(location, location + length).createRange(*editable);
    if (!range)
        return false;
    IntRect intRect = frame()->editor().firstRectForRange(range.get());
    rect = WebRect(intRect);
    rect = frame()->view()->contentsToWindow(rect);
    return true;
}

size_t WebFrameImpl::characterIndexForPoint(const WebPoint& webPoint) const
{
    if (!frame())
        return kNotFound;

    IntPoint point = frame()->view()->windowToContents(webPoint);
    HitTestResult result = frame()->eventHandler().hitTestResultAtPoint(point, HitTestRequest::ReadOnly | HitTestRequest::Active | HitTestRequest::ConfusingAndOftenMisusedDisallowShadowContent);
    RefPtr<Range> range = frame()->rangeForPoint(result.roundedPointInInnerNodeFrame());
    if (!range)
        return kNotFound;
    Element* editable = frame()->selection().rootEditableElementOrDocumentElement();
    ASSERT(editable);
    return PlainTextRange::create(*editable, *range.get()).start();
}

bool WebFrameImpl::executeCommand(const WebString& name, const WebNode& node)
{
    ASSERT(frame());

    if (name.length() <= 2)
        return false;

    // Since we don't have NSControl, we will convert the format of command
    // string and call the function on Editor directly.
    String command = name;

    // Make sure the first letter is upper case.
    command.replace(0, 1, command.substring(0, 1).upper());

    // Remove the trailing ':' if existing.
    if (command[command.length() - 1] == UChar(':'))
        command = command.substring(0, command.length() - 1);

    WebPluginContainerImpl* pluginContainer = pluginContainerFromNode(frame(), node);
    if (pluginContainer && pluginContainer->executeEditCommand(name))
        return true;

    bool result = true;

    // Specially handling commands that Editor::execCommand does not directly
    // support.
    if (command == "DeleteToEndOfParagraph") {
        if (!frame()->editor().deleteWithDirection(DirectionForward, ParagraphBoundary, true, false))
            frame()->editor().deleteWithDirection(DirectionForward, CharacterGranularity, true, false);
    } else if (command == "Indent") {
        frame()->editor().indent();
    } else if (command == "Outdent") {
        frame()->editor().outdent();
    } else if (command == "DeleteBackward") {
        result = frame()->editor().command(AtomicString("BackwardDelete")).execute();
    } else if (command == "DeleteForward") {
        result = frame()->editor().command(AtomicString("ForwardDelete")).execute();
    } else if (command == "AdvanceToNextMisspelling") {
        // Wee need to pass false here or else the currently selected word will never be skipped.
        frame()->spellChecker().advanceToNextMisspelling(false);
    } else if (command == "ToggleSpellPanel") {
        frame()->spellChecker().showSpellingGuessPanel();
    } else {
        result = frame()->editor().command(command).execute();
    }
    return result;
}

bool WebFrameImpl::executeCommand(const WebString& name, const WebString& value, const WebNode& node)
{
    ASSERT(frame());
    String webName = name;

    WebPluginContainerImpl* pluginContainer = pluginContainerFromNode(frame(), node);
    if (pluginContainer && pluginContainer->executeEditCommand(name, value))
        return true;

    // moveToBeginningOfDocument and moveToEndfDocument are only handled by WebKit for editable nodes.
    if (!frame()->editor().canEdit() && webName == "moveToBeginningOfDocument")
        return viewImpl()->bubblingScroll(ScrollUp, ScrollByDocument);

    if (!frame()->editor().canEdit() && webName == "moveToEndOfDocument")
        return viewImpl()->bubblingScroll(ScrollDown, ScrollByDocument);

    if (webName == "showGuessPanel") {
        frame()->spellChecker().showSpellingGuessPanel();
        return true;
    }

    return frame()->editor().command(webName).execute(value);
}

bool WebFrameImpl::isCommandEnabled(const WebString& name) const
{
    ASSERT(frame());
    return frame()->editor().command(name).isEnabled();
}

void WebFrameImpl::enableContinuousSpellChecking(bool enable)
{
    if (enable == isContinuousSpellCheckingEnabled())
        return;
    frame()->spellChecker().toggleContinuousSpellChecking();
}

bool WebFrameImpl::isContinuousSpellCheckingEnabled() const
{
    return frame()->spellChecker().isContinuousSpellCheckingEnabled();
}

void WebFrameImpl::requestTextChecking(const WebElement& webElement)
{
    if (webElement.isNull())
        return;
    frame()->spellChecker().requestTextChecking(*webElement.constUnwrap<Element>());
}

void WebFrameImpl::replaceMisspelledRange(const WebString& text)
{
    // If this caret selection has two or more markers, this function replace the range covered by the first marker with the specified word as Microsoft Word does.
    if (pluginContainerFromFrame(frame()))
        return;
    RefPtr<Range> caretRange = frame()->selection().toNormalizedRange();
    if (!caretRange)
        return;
    Vector<DocumentMarker*> markers = frame()->document()->markers().markersInRange(caretRange.get(), DocumentMarker::MisspellingMarkers());
    if (markers.size() < 1 || markers[0]->startOffset() >= markers[0]->endOffset())
        return;
    RefPtr<Range> markerRange = Range::create(caretRange->ownerDocument(), caretRange->startContainer(), markers[0]->startOffset(), caretRange->endContainer(), markers[0]->endOffset());
    if (!markerRange)
        return;
    frame()->selection().setSelection(VisibleSelection(markerRange.get()), CharacterGranularity);
    frame()->editor().replaceSelectionWithText(text, false, false);
}

void WebFrameImpl::removeSpellingMarkers()
{
    frame()->document()->markers().removeMarkers(DocumentMarker::MisspellingMarkers());
}

bool WebFrameImpl::hasSelection() const
{
    WebPluginContainerImpl* pluginContainer = pluginContainerFromFrame(frame());
    if (pluginContainer)
        return pluginContainer->plugin()->hasSelection();

    // frame()->selection()->isNone() never returns true.
    return frame()->selection().start() != frame()->selection().end();
}

WebRange WebFrameImpl::selectionRange() const
{
    return frame()->selection().toNormalizedRange();
}

WebString WebFrameImpl::selectionAsText() const
{
    WebPluginContainerImpl* pluginContainer = pluginContainerFromFrame(frame());
    if (pluginContainer)
        return pluginContainer->plugin()->selectionAsText();

    RefPtr<Range> range = frame()->selection().toNormalizedRange();
    if (!range)
        return WebString();

    String text = range->text();
#if OS(WIN)
    replaceNewlinesWithWindowsStyleNewlines(text);
#endif
    replaceNBSPWithSpace(text);
    return text;
}

WebString WebFrameImpl::selectionAsMarkup() const
{
    WebPluginContainerImpl* pluginContainer = pluginContainerFromFrame(frame());
    if (pluginContainer)
        return pluginContainer->plugin()->selectionAsMarkup();

    RefPtr<Range> range = frame()->selection().toNormalizedRange();
    if (!range)
        return WebString();

    return createMarkup(range.get(), 0, AnnotateForInterchange, false, ResolveNonLocalURLs);
}

void WebFrameImpl::selectWordAroundPosition(LocalFrame* frame, VisiblePosition position)
{
    VisibleSelection selection(position);
    selection.expandUsingGranularity(WordGranularity);

    TextGranularity granularity = selection.isRange() ? WordGranularity : CharacterGranularity;
    frame->selection().setSelection(selection, granularity);
}

bool WebFrameImpl::selectWordAroundCaret()
{
    FrameSelection& selection = frame()->selection();
    ASSERT(!selection.isNone());
    if (selection.isNone() || selection.isRange())
        return false;
    selectWordAroundPosition(frame(), selection.selection().visibleStart());
    return true;
}

void WebFrameImpl::selectRange(const WebPoint& base, const WebPoint& extent)
{
    moveRangeSelection(base, extent);
}

void WebFrameImpl::selectRange(const WebRange& webRange)
{
    if (RefPtr<Range> range = static_cast<PassRefPtr<Range> >(webRange))
        frame()->selection().setSelectedRange(range.get(), WebCore::VP_DEFAULT_AFFINITY, false);
}

void WebFrameImpl::moveRangeSelection(const WebPoint& base, const WebPoint& extent)
{
    VisiblePosition basePosition = visiblePositionForWindowPoint(base);
    VisiblePosition extentPosition = visiblePositionForWindowPoint(extent);
    VisibleSelection newSelection = VisibleSelection(basePosition, extentPosition);
    frame()->selection().setSelection(newSelection, CharacterGranularity);
}

void WebFrameImpl::moveCaretSelection(const WebPoint& point)
{
    Element* editable = frame()->selection().rootEditableElement();
    if (!editable)
        return;

    VisiblePosition position = visiblePositionForWindowPoint(point);
    frame()->selection().moveTo(position, UserTriggered);
}

void WebFrameImpl::setCaretVisible(bool visible)
{
    frame()->selection().setCaretVisible(visible);
}

VisiblePosition WebFrameImpl::visiblePositionForWindowPoint(const WebPoint& point)
{
    FloatPoint unscaledPoint(point);
    unscaledPoint.scale(1 / view()->pageScaleFactor(), 1 / view()->pageScaleFactor());

    HitTestRequest request = HitTestRequest::Move | HitTestRequest::ReadOnly | HitTestRequest::Active | HitTestRequest::IgnoreClipping | HitTestRequest::ConfusingAndOftenMisusedDisallowShadowContent;
    HitTestResult result(frame()->view()->windowToContents(roundedIntPoint(unscaledPoint)));
    frame()->document()->renderView()->layer()->hitTest(request, result);

    if (Node* node = result.targetNode())
        return frame()->selection().selection().visiblePositionRespectingEditingBoundary(result.localPoint(), node);
    return VisiblePosition();
}

int WebFrameImpl::printBegin(const WebPrintParams& printParams, const WebNode& constrainToNode)
{
    ASSERT(!frame()->document()->isFrameSet());
    WebPluginContainerImpl* pluginContainer = 0;
    if (constrainToNode.isNull()) {
        // If this is a plugin document, check if the plugin supports its own
        // printing. If it does, we will delegate all printing to that.
        pluginContainer = pluginContainerFromFrame(frame());
    } else {
        // We only support printing plugin nodes for now.
        pluginContainer = toWebPluginContainerImpl(constrainToNode.pluginContainer());
    }

    if (pluginContainer && pluginContainer->supportsPaginatedPrint())
        m_printContext = adoptPtr(new ChromePluginPrintContext(frame(), pluginContainer, printParams));
    else
        m_printContext = adoptPtr(new ChromePrintContext(frame()));

    FloatRect rect(0, 0, static_cast<float>(printParams.printContentArea.width), static_cast<float>(printParams.printContentArea.height));
    m_printContext->begin(rect.width(), rect.height());
    float pageHeight;
    // We ignore the overlays calculation for now since they are generated in the
    // browser. pageHeight is actually an output parameter.
    m_printContext->computePageRects(rect, 0, 0, 1.0, pageHeight);

    return m_printContext->pageCount();
}

float WebFrameImpl::getPrintPageShrink(int page)
{
    ASSERT(m_printContext && page >= 0);
    return m_printContext->getPageShrink(page);
}

float WebFrameImpl::printPage(int page, WebCanvas* canvas)
{
#if ENABLE(PRINTING)
    ASSERT(m_printContext && page >= 0 && frame() && frame()->document());

    GraphicsContext graphicsContext(canvas);
    graphicsContext.setPrinting(true);
    return m_printContext->spoolPage(graphicsContext, page);
#else
    return 0;
#endif
}

void WebFrameImpl::printEnd()
{
    ASSERT(m_printContext);
    m_printContext->end();
    m_printContext.clear();
}

bool WebFrameImpl::isPrintScalingDisabledForPlugin(const WebNode& node)
{
    WebPluginContainerImpl* pluginContainer =  node.isNull() ? pluginContainerFromFrame(frame()) : toWebPluginContainerImpl(node.pluginContainer());

    if (!pluginContainer || !pluginContainer->supportsPaginatedPrint())
        return false;

    return pluginContainer->isPrintScalingDisabled();
}

bool WebFrameImpl::hasCustomPageSizeStyle(int pageIndex)
{
    return frame()->document()->styleForPage(pageIndex)->pageSizeType() != PAGE_SIZE_AUTO;
}

bool WebFrameImpl::isPageBoxVisible(int pageIndex)
{
    return frame()->document()->isPageBoxVisible(pageIndex);
}

void WebFrameImpl::pageSizeAndMarginsInPixels(int pageIndex, WebSize& pageSize, int& marginTop, int& marginRight, int& marginBottom, int& marginLeft)
{
    IntSize size = pageSize;
    frame()->document()->pageSizeAndMarginsInPixels(pageIndex, size, marginTop, marginRight, marginBottom, marginLeft);
    pageSize = size;
}

WebString WebFrameImpl::pageProperty(const WebString& propertyName, int pageIndex)
{
    ASSERT(m_printContext);
    return m_printContext->pageProperty(frame(), propertyName.utf8().data(), pageIndex);
}

bool WebFrameImpl::find(int identifier, const WebString& searchText, const WebFindOptions& options, bool wrapWithinFrame, WebRect* selectionRect)
{
    if (!frame() || !frame()->page())
        return false;

    WebFrameImpl* mainFrameImpl = viewImpl()->mainFrameImpl();

    if (!options.findNext)
        frame()->page()->unmarkAllTextMatches();
    else
        setMarkerActive(m_activeMatch.get(), false);

    if (m_activeMatch && &m_activeMatch->ownerDocument() != frame()->document())
        m_activeMatch = nullptr;

    // If the user has selected something since the last Find operation we want
    // to start from there. Otherwise, we start searching from where the last Find
    // operation left off (either a Find or a FindNext operation).
    VisibleSelection selection(frame()->selection().selection());
    bool activeSelection = !selection.isNone();
    if (activeSelection) {
        m_activeMatch = selection.firstRange().get();
        frame()->selection().clear();
    }

    ASSERT(frame() && frame()->view());
    const FindOptions findOptions = (options.forward ? 0 : Backwards)
        | (options.matchCase ? 0 : CaseInsensitive)
        | (wrapWithinFrame ? WrapAround : 0)
        | (options.wordStart ? AtWordStarts : 0)
        | (options.medialCapitalAsWordStart ? TreatMedialCapitalAsWordStart : 0)
        | (options.findNext ? 0 : StartInSelection);
    m_activeMatch = frame()->editor().findStringAndScrollToVisible(searchText, m_activeMatch.get(), findOptions);

    if (!m_activeMatch) {
        // If we're finding next the next active match might not be in the current frame.
        // In this case we don't want to clear the matches cache.
        if (!options.findNext)
            clearFindMatchesCache();
        invalidateArea(InvalidateAll);
        return false;
    }

#if OS(ANDROID)
    viewImpl()->zoomToFindInPageRect(frameView()->contentsToWindow(enclosingIntRect(RenderObject::absoluteBoundingBoxRectForRange(m_activeMatch.get()))));
#endif

    setMarkerActive(m_activeMatch.get(), true);
    WebFrameImpl* oldActiveFrame = mainFrameImpl->m_currentActiveMatchFrame;
    mainFrameImpl->m_currentActiveMatchFrame = this;

    // Make sure no node is focused. See http://crbug.com/38700.
    frame()->document()->setFocusedElement(nullptr);

    if (!options.findNext || activeSelection) {
        // This is either a Find operation or a Find-next from a new start point
        // due to a selection, so we set the flag to ask the scoping effort
        // to find the active rect for us and report it back to the UI.
        m_locatingActiveRect = true;
    } else {
        if (oldActiveFrame != this) {
            if (options.forward)
                m_activeMatchIndexInCurrentFrame = 0;
            else
                m_activeMatchIndexInCurrentFrame = m_lastMatchCount - 1;
        } else {
            if (options.forward)
                ++m_activeMatchIndexInCurrentFrame;
            else
                --m_activeMatchIndexInCurrentFrame;

            if (m_activeMatchIndexInCurrentFrame + 1 > m_lastMatchCount)
                m_activeMatchIndexInCurrentFrame = 0;
            if (m_activeMatchIndexInCurrentFrame == -1)
                m_activeMatchIndexInCurrentFrame = m_lastMatchCount - 1;
        }
        if (selectionRect) {
            *selectionRect = frameView()->contentsToWindow(m_activeMatch->boundingBox());
            reportFindInPageSelection(*selectionRect, m_activeMatchIndexInCurrentFrame + 1, identifier);
        }
    }

    return true;
}

void WebFrameImpl::stopFinding(bool clearSelection)
{
    if (!clearSelection)
        setFindEndstateFocusAndSelection();
    cancelPendingScopingEffort();

    // Remove all markers for matches found and turn off the highlighting.
    frame()->document()->markers().removeMarkers(DocumentMarker::TextMatch);
    frame()->editor().setMarkedTextMatchesAreHighlighted(false);
    clearFindMatchesCache();

    // Let the frame know that we don't want tickmarks or highlighting anymore.
    invalidateArea(InvalidateAll);
}

void WebFrameImpl::scopeStringMatches(int identifier, const WebString& searchText, const WebFindOptions& options, bool reset)
{
    if (reset) {
        // This is a brand new search, so we need to reset everything.
        // Scoping is just about to begin.
        m_scopingInProgress = true;

        // Need to keep the current identifier locally in order to finish the
        // request in case the frame is detached during the process.
        m_findRequestIdentifier = identifier;

        // Clear highlighting for this frame.
        if (frame() && frame()->page() && frame()->editor().markedTextMatchesAreHighlighted())
            frame()->page()->unmarkAllTextMatches();

        // Clear the tickmarks and results cache.
        clearFindMatchesCache();

        // Clear the counters from last operation.
        m_lastMatchCount = 0;
        m_nextInvalidateAfter = 0;

        m_resumeScopingFromRange = nullptr;

        // The view might be null on detached frames.
        if (frame() && frame()->page())
            viewImpl()->mainFrameImpl()->m_framesScopingCount++;

        // Now, defer scoping until later to allow find operation to finish quickly.
        scopeStringMatchesSoon(identifier, searchText, options, false); // false means just reset, so don't do it again.
        return;
    }

    if (!shouldScopeMatches(searchText)) {
        // Note that we want to defer the final update when resetting even if shouldScopeMatches returns false.
        // This is done in order to prevent sending a final message based only on the results of the first frame
        // since m_framesScopingCount would be 0 as other frames have yet to reset.
        finishCurrentScopingEffort(identifier);
        return;
    }

    WebFrameImpl* mainFrameImpl = viewImpl()->mainFrameImpl();
    RefPtr<Range> searchRange(rangeOfContents(frame()->document()));

    Node* originalEndContainer = searchRange->endContainer();
    int originalEndOffset = searchRange->endOffset();

    TrackExceptionState exceptionState, exceptionState2;
    if (m_resumeScopingFromRange) {
        // This is a continuation of a scoping operation that timed out and didn't
        // complete last time around, so we should start from where we left off.
        searchRange->setStart(m_resumeScopingFromRange->startContainer(), m_resumeScopingFromRange->startOffset(exceptionState2) + 1, exceptionState);
        if (exceptionState.hadException() || exceptionState2.hadException()) {
            if (exceptionState2.hadException()) // A non-zero |exceptionState| happens when navigating during search.
                ASSERT_NOT_REACHED();
            return;
        }
    }

    // This timeout controls how long we scope before releasing control.  This
    // value does not prevent us from running for longer than this, but it is
    // periodically checked to see if we have exceeded our allocated time.
    const double maxScopingDuration = 0.1; // seconds

    int matchCount = 0;
    bool timedOut = false;
    double startTime = currentTime();
    do {
        // Find next occurrence of the search string.
        // FIXME: (http://b/1088245) This WebKit operation may run for longer
        // than the timeout value, and is not interruptible as it is currently
        // written. We may need to rewrite it with interruptibility in mind, or
        // find an alternative.
        RefPtr<Range> resultRange(findPlainText(searchRange.get(),
                                                searchText,
                                                options.matchCase ? 0 : CaseInsensitive));
        if (resultRange->collapsed(exceptionState)) {
            if (!resultRange->startContainer()->isInShadowTree())
                break;

            searchRange->setStartAfter(
                resultRange->startContainer()->deprecatedShadowAncestorNode(), exceptionState);
            searchRange->setEnd(originalEndContainer, originalEndOffset, exceptionState);
            continue;
        }

        ++matchCount;

        // Catch a special case where Find found something but doesn't know what
        // the bounding box for it is. In this case we set the first match we find
        // as the active rect.
        IntRect resultBounds = resultRange->boundingBox();
        IntRect activeSelectionRect;
        if (m_locatingActiveRect) {
            activeSelectionRect = m_activeMatch.get() ?
                m_activeMatch->boundingBox() : resultBounds;
        }

        // If the Find function found a match it will have stored where the
        // match was found in m_activeSelectionRect on the current frame. If we
        // find this rect during scoping it means we have found the active
        // tickmark.
        bool foundActiveMatch = false;
        if (m_locatingActiveRect && (activeSelectionRect == resultBounds)) {
            // We have found the active tickmark frame.
            mainFrameImpl->m_currentActiveMatchFrame = this;
            foundActiveMatch = true;
            // We also know which tickmark is active now.
            m_activeMatchIndexInCurrentFrame = matchCount - 1;
            // To stop looking for the active tickmark, we set this flag.
            m_locatingActiveRect = false;

            // Notify browser of new location for the selected rectangle.
            reportFindInPageSelection(
                frameView()->contentsToWindow(resultBounds),
                m_activeMatchIndexInCurrentFrame + 1,
                identifier);
        }

        addMarker(resultRange.get(), foundActiveMatch);

        m_findMatchesCache.append(FindMatch(resultRange.get(), m_lastMatchCount + matchCount));

        // Set the new start for the search range to be the end of the previous
        // result range. There is no need to use a VisiblePosition here,
        // since findPlainText will use a TextIterator to go over the visible
        // text nodes.
        searchRange->setStart(resultRange->endContainer(exceptionState), resultRange->endOffset(exceptionState), exceptionState);

        Node* shadowTreeRoot = searchRange->shadowRoot();
        if (searchRange->collapsed(exceptionState) && shadowTreeRoot)
            searchRange->setEnd(shadowTreeRoot, shadowTreeRoot->countChildren(), exceptionState);

        m_resumeScopingFromRange = resultRange;
        timedOut = (currentTime() - startTime) >= maxScopingDuration;
    } while (!timedOut);

    // Remember what we search for last time, so we can skip searching if more
    // letters are added to the search string (and last outcome was 0).
    m_lastSearchString = searchText;

    if (matchCount > 0) {
        frame()->editor().setMarkedTextMatchesAreHighlighted(true);

        m_lastMatchCount += matchCount;

        // Let the mainframe know how much we found during this pass.
        mainFrameImpl->increaseMatchCount(matchCount, identifier);
    }

    if (timedOut) {
        // If we found anything during this pass, we should redraw. However, we
        // don't want to spam too much if the page is extremely long, so if we
        // reach a certain point we start throttling the redraw requests.
        if (matchCount > 0)
            invalidateIfNecessary();

        // Scoping effort ran out of time, lets ask for another time-slice.
        scopeStringMatchesSoon(
            identifier,
            searchText,
            options,
            false); // don't reset.
        return; // Done for now, resume work later.
    }

    finishCurrentScopingEffort(identifier);
}

void WebFrameImpl::flushCurrentScopingEffort(int identifier)
{
    if (!frame() || !frame()->page())
        return;

    WebFrameImpl* mainFrameImpl = viewImpl()->mainFrameImpl();

    // This frame has no further scoping left, so it is done. Other frames might,
    // of course, continue to scope matches.
    mainFrameImpl->m_framesScopingCount--;

    // If this is the last frame to finish scoping we need to trigger the final
    // update to be sent.
    if (!mainFrameImpl->m_framesScopingCount)
        mainFrameImpl->increaseMatchCount(0, identifier);
}

void WebFrameImpl::finishCurrentScopingEffort(int identifier)
{
    flushCurrentScopingEffort(identifier);

    m_scopingInProgress = false;
    m_lastFindRequestCompletedWithNoMatches = !m_lastMatchCount;

    // This frame is done, so show any scrollbar tickmarks we haven't drawn yet.
    invalidateArea(InvalidateScrollbar);
}

void WebFrameImpl::cancelPendingScopingEffort()
{
    deleteAllValues(m_deferredScopingWork);
    m_deferredScopingWork.clear();

    m_activeMatchIndexInCurrentFrame = -1;

    // Last request didn't complete.
    if (m_scopingInProgress)
        m_lastFindRequestCompletedWithNoMatches = false;

    m_scopingInProgress = false;
}

void WebFrameImpl::increaseMatchCount(int count, int identifier)
{
    // This function should only be called on the mainframe.
    ASSERT(!parent());

    if (count)
        ++m_findMatchMarkersVersion;

    m_totalMatchCount += count;

    // Update the UI with the latest findings.
    if (client())
        client()->reportFindInPageMatchCount(identifier, m_totalMatchCount, !m_framesScopingCount);
}

void WebFrameImpl::reportFindInPageSelection(const WebRect& selectionRect, int activeMatchOrdinal, int identifier)
{
    // Update the UI with the latest selection rect.
    if (client())
        client()->reportFindInPageSelection(identifier, ordinalOfFirstMatchForFrame(this) + activeMatchOrdinal, selectionRect);
}

void WebFrameImpl::resetMatchCount()
{
    if (m_totalMatchCount > 0)
        ++m_findMatchMarkersVersion;

    m_totalMatchCount = 0;
    m_framesScopingCount = 0;
}

void WebFrameImpl::sendOrientationChangeEvent(int orientation)
{
    if (frame())
        frame()->sendOrientationChangeEvent(orientation);
}

void WebFrameImpl::dispatchMessageEventWithOriginCheck(const WebSecurityOrigin& intendedTargetOrigin, const WebDOMEvent& event)
{
    ASSERT(!event.isNull());
    frame()->domWindow()->dispatchMessageEventWithOriginCheck(intendedTargetOrigin.get(), event, nullptr);
}

int WebFrameImpl::findMatchMarkersVersion() const
{
    ASSERT(!parent());
    return m_findMatchMarkersVersion;
}

void WebFrameImpl::clearFindMatchesCache()
{
    if (!m_findMatchesCache.isEmpty())
        viewImpl()->mainFrameImpl()->m_findMatchMarkersVersion++;

    m_findMatchesCache.clear();
    m_findMatchRectsAreValid = false;
}

bool WebFrameImpl::isActiveMatchFrameValid() const
{
    WebFrameImpl* mainFrameImpl = viewImpl()->mainFrameImpl();
    WebFrameImpl* activeMatchFrame = mainFrameImpl->activeMatchFrame();
    return activeMatchFrame && activeMatchFrame->m_activeMatch && activeMatchFrame->frame()->tree().isDescendantOf(mainFrameImpl->frame());
}

void WebFrameImpl::updateFindMatchRects()
{
    IntSize currentContentsSize = contentsSize();
    if (m_contentsSizeForCurrentFindMatchRects != currentContentsSize) {
        m_contentsSizeForCurrentFindMatchRects = currentContentsSize;
        m_findMatchRectsAreValid = false;
    }

    size_t deadMatches = 0;
    for (Vector<FindMatch>::iterator it = m_findMatchesCache.begin(); it != m_findMatchesCache.end(); ++it) {
        if (!it->m_range->boundaryPointsValid() || !it->m_range->startContainer()->inDocument())
            it->m_rect = FloatRect();
        else if (!m_findMatchRectsAreValid)
            it->m_rect = findInPageRectFromRange(it->m_range.get());

        if (it->m_rect.isEmpty())
            ++deadMatches;
    }

    // Remove any invalid matches from the cache.
    if (deadMatches) {
        Vector<FindMatch> filteredMatches;
        filteredMatches.reserveCapacity(m_findMatchesCache.size() - deadMatches);

        for (Vector<FindMatch>::const_iterator it = m_findMatchesCache.begin(); it != m_findMatchesCache.end(); ++it)
            if (!it->m_rect.isEmpty())
                filteredMatches.append(*it);

        m_findMatchesCache.swap(filteredMatches);
    }

    // Invalidate the rects in child frames. Will be updated later during traversal.
    if (!m_findMatchRectsAreValid)
        for (WebFrame* child = firstChild(); child; child = child->nextSibling())
            toWebFrameImpl(child)->m_findMatchRectsAreValid = false;

    m_findMatchRectsAreValid = true;
}

WebFloatRect WebFrameImpl::activeFindMatchRect()
{
    ASSERT(!parent());

    if (!isActiveMatchFrameValid())
        return WebFloatRect();

    return WebFloatRect(findInPageRectFromRange(m_currentActiveMatchFrame->m_activeMatch.get()));
}

void WebFrameImpl::findMatchRects(WebVector<WebFloatRect>& outputRects)
{
    ASSERT(!parent());

    Vector<WebFloatRect> matchRects;
    for (WebFrameImpl* frame = this; frame; frame = toWebFrameImpl(frame->traverseNext(false)))
        frame->appendFindMatchRects(matchRects);

    outputRects = matchRects;
}

void WebFrameImpl::appendFindMatchRects(Vector<WebFloatRect>& frameRects)
{
    updateFindMatchRects();
    frameRects.reserveCapacity(frameRects.size() + m_findMatchesCache.size());
    for (Vector<FindMatch>::const_iterator it = m_findMatchesCache.begin(); it != m_findMatchesCache.end(); ++it) {
        ASSERT(!it->m_rect.isEmpty());
        frameRects.append(it->m_rect);
    }
}

int WebFrameImpl::selectNearestFindMatch(const WebFloatPoint& point, WebRect* selectionRect)
{
    ASSERT(!parent());

    WebFrameImpl* bestFrame = 0;
    int indexInBestFrame = -1;
    float distanceInBestFrame = FLT_MAX;

    for (WebFrameImpl* frame = this; frame; frame = toWebFrameImpl(frame->traverseNext(false))) {
        float distanceInFrame;
        int indexInFrame = frame->nearestFindMatch(point, distanceInFrame);
        if (distanceInFrame < distanceInBestFrame) {
            bestFrame = frame;
            indexInBestFrame = indexInFrame;
            distanceInBestFrame = distanceInFrame;
        }
    }

    if (indexInBestFrame != -1)
        return bestFrame->selectFindMatch(static_cast<unsigned>(indexInBestFrame), selectionRect);

    return -1;
}

int WebFrameImpl::nearestFindMatch(const FloatPoint& point, float& distanceSquared)
{
    updateFindMatchRects();

    int nearest = -1;
    distanceSquared = FLT_MAX;
    for (size_t i = 0; i < m_findMatchesCache.size(); ++i) {
        ASSERT(!m_findMatchesCache[i].m_rect.isEmpty());
        FloatSize offset = point - m_findMatchesCache[i].m_rect.center();
        float width = offset.width();
        float height = offset.height();
        float currentDistanceSquared = width * width + height * height;
        if (currentDistanceSquared < distanceSquared) {
            nearest = i;
            distanceSquared = currentDistanceSquared;
        }
    }
    return nearest;
}

int WebFrameImpl::selectFindMatch(unsigned index, WebRect* selectionRect)
{
    ASSERT_WITH_SECURITY_IMPLICATION(index < m_findMatchesCache.size());

    RefPtr<Range> range = m_findMatchesCache[index].m_range;
    if (!range->boundaryPointsValid() || !range->startContainer()->inDocument())
        return -1;

    // Check if the match is already selected.
    WebFrameImpl* activeMatchFrame = viewImpl()->mainFrameImpl()->m_currentActiveMatchFrame;
    if (this != activeMatchFrame || !m_activeMatch || !areRangesEqual(m_activeMatch.get(), range.get())) {
        if (isActiveMatchFrameValid())
            activeMatchFrame->setMarkerActive(activeMatchFrame->m_activeMatch.get(), false);

        m_activeMatchIndexInCurrentFrame = m_findMatchesCache[index].m_ordinal - 1;

        // Set this frame as the active frame (the one with the active highlight).
        viewImpl()->mainFrameImpl()->m_currentActiveMatchFrame = this;
        viewImpl()->setFocusedFrame(this);

        m_activeMatch = range.release();
        setMarkerActive(m_activeMatch.get(), true);

        // Clear any user selection, to make sure Find Next continues on from the match we just activated.
        frame()->selection().clear();

        // Make sure no node is focused. See http://crbug.com/38700.
        frame()->document()->setFocusedElement(nullptr);
    }

    IntRect activeMatchRect;
    IntRect activeMatchBoundingBox = enclosingIntRect(RenderObject::absoluteBoundingBoxRectForRange(m_activeMatch.get()));

    if (!activeMatchBoundingBox.isEmpty()) {
        if (m_activeMatch->firstNode() && m_activeMatch->firstNode()->renderer())
            m_activeMatch->firstNode()->renderer()->scrollRectToVisible(activeMatchBoundingBox,
                    ScrollAlignment::alignCenterIfNeeded, ScrollAlignment::alignCenterIfNeeded);

        // Zoom to the active match.
        activeMatchRect = frameView()->contentsToWindow(activeMatchBoundingBox);
        viewImpl()->zoomToFindInPageRect(activeMatchRect);
    }

    if (selectionRect)
        *selectionRect = activeMatchRect;

    return ordinalOfFirstMatchForFrame(this) + m_activeMatchIndexInCurrentFrame + 1;
}

WebString WebFrameImpl::contentAsText(size_t maxChars) const
{
    if (!frame())
        return WebString();
    StringBuilder text;
    frameContentAsPlainText(maxChars, frame(), text);
    return text.toString();
}

WebString WebFrameImpl::contentAsMarkup() const
{
    if (!frame())
        return WebString();
    return createFullMarkup(frame()->document());
}

WebString WebFrameImpl::renderTreeAsText(RenderAsTextControls toShow) const
{
    RenderAsTextBehavior behavior = RenderAsTextBehaviorNormal;

    if (toShow & RenderAsTextDebug)
        behavior |= RenderAsTextShowCompositedLayers | RenderAsTextShowAddresses | RenderAsTextShowIDAndClass | RenderAsTextShowLayerNesting;

    if (toShow & RenderAsTextPrinting)
        behavior |= RenderAsTextPrintingMode;

    return externalRepresentation(frame(), behavior);
}

WebString WebFrameImpl::markerTextForListItem(const WebElement& webElement) const
{
    return WebCore::markerTextForListItem(const_cast<Element*>(webElement.constUnwrap<Element>()));
}

void WebFrameImpl::printPagesWithBoundaries(WebCanvas* canvas, const WebSize& pageSizeInPixels)
{
    ASSERT(m_printContext);

    GraphicsContext graphicsContext(canvas);
    graphicsContext.setPrinting(true);

    m_printContext->spoolAllPagesWithBoundaries(graphicsContext, FloatSize(pageSizeInPixels.width, pageSizeInPixels.height));
}

WebRect WebFrameImpl::selectionBoundsRect() const
{
    return hasSelection() ? WebRect(IntRect(frame()->selection().bounds(false))) : WebRect();
}

bool WebFrameImpl::selectionStartHasSpellingMarkerFor(int from, int length) const
{
    if (!frame())
        return false;
    return frame()->spellChecker().selectionStartHasMarkerFor(DocumentMarker::Spelling, from, length);
}

WebString WebFrameImpl::layerTreeAsText(bool showDebugInfo) const
{
    if (!frame())
        return WebString();

    return WebString(frame()->layerTreeAsText(showDebugInfo ? LayerTreeIncludesDebugInfo : LayerTreeNormal));
}

// WebFrameImpl public ---------------------------------------------------------

WebFrame* WebFrame::create(WebFrameClient* client)
{
    return WebFrameImpl::create(client);
}

WebFrameImpl* WebFrameImpl::create(WebFrameClient* client)
{
    return adoptRef(new WebFrameImpl(client)).leakRef();
}

WebFrameImpl::WebFrameImpl(WebFrameClient* client)
    : m_frameLoaderClientImpl(this)
    , m_parent(0)
    , m_previousSibling(0)
    , m_nextSibling(0)
    , m_firstChild(0)
    , m_lastChild(0)
    , m_opener(0)
    , m_client(client)
    , m_permissionClient(0)
    , m_currentActiveMatchFrame(0)
    , m_activeMatchIndexInCurrentFrame(-1)
    , m_locatingActiveRect(false)
    , m_resumeScopingFromRange(nullptr)
    , m_lastMatchCount(-1)
    , m_totalMatchCount(-1)
    , m_framesScopingCount(-1)
    , m_findRequestIdentifier(-1)
    , m_scopingInProgress(false)
    , m_lastFindRequestCompletedWithNoMatches(false)
    , m_nextInvalidateAfter(0)
    , m_findMatchMarkersVersion(0)
    , m_findMatchRectsAreValid(false)
    , m_inputEventsScaleFactorForEmulation(1)
{
    blink::Platform::current()->incrementStatsCounter(webFrameActiveCount);
    frameCount++;
}

WebFrameImpl::~WebFrameImpl()
{
    HashSet<WebFrameImpl*>::iterator end = m_openedFrames.end();
    for (HashSet<WebFrameImpl*>::iterator it = m_openedFrames.begin(); it != end; ++it)
        (*it)->m_opener = 0;

    blink::Platform::current()->decrementStatsCounter(webFrameActiveCount);
    frameCount--;

    cancelPendingScopingEffort();
}

void WebFrameImpl::setWebCoreFrame(PassRefPtr<WebCore::LocalFrame> frame)
{
    m_frame = frame;
}

void WebFrameImpl::initializeAsMainFrame(WebCore::Page* page)
{
    setWebCoreFrame(LocalFrame::create(&m_frameLoaderClientImpl, &page->frameHost(), 0));

    // We must call init() after m_frame is assigned because it is referenced
    // during init().
    m_frame->init();
}

PassRefPtr<LocalFrame> WebFrameImpl::createChildFrame(const FrameLoadRequest& request, HTMLFrameOwnerElement* ownerElement)
{
    ASSERT(m_client);
    WebFrameImpl* webframe = toWebFrameImpl(m_client->createChildFrame(this, request.frameName()));
    if (!webframe)
        return nullptr;

    RefPtr<LocalFrame> childFrame = LocalFrame::create(&webframe->m_frameLoaderClientImpl, frame()->host(), ownerElement);
    webframe->setWebCoreFrame(childFrame);

    childFrame->tree().setName(request.frameName());

    // FIXME: This comment is not quite accurate anymore.
    // LocalFrame::init() can trigger onload event in the parent frame,
    // which may detach this frame and trigger a null-pointer access
    // in FrameTree::removeChild. Move init() after appendChild call
    // so that webframe->mFrame is in the tree before triggering
    // onload event handler.
    // Because the event handler may set webframe->mFrame to null,
    // it is necessary to check the value after calling init() and
    // return without loading URL.
    // NOTE: m_client will be null if this frame has been detached.
    // (b:791612)
    childFrame->init(); // create an empty document
    if (!childFrame->tree().parent())
        return nullptr;

    // If we're moving in the back/forward list, we might want to replace the content
    // of this child frame with whatever was there at that point.
    HistoryItem* childItem = 0;
    if (isBackForwardLoadType(frame()->loader().loadType()) && !frame()->document()->loadEventFinished())
        childItem = frame()->page()->historyController().itemForNewChildFrame(childFrame.get());

    if (childItem)
        childFrame->loader().loadHistoryItem(childItem);
    else
        childFrame->loader().load(FrameLoadRequest(0, request.resourceRequest(), "_self"));

    // A synchronous navigation (about:blank) would have already processed
    // onload, so it is possible for the frame to have already been destroyed by
    // script in the page.
    // NOTE: m_client will be null if this frame has been detached.
    if (!childFrame->tree().parent())
        return nullptr;

    return childFrame.release();
}

void WebFrameImpl::didChangeContentsSize(const IntSize& size)
{
    // This is only possible on the main frame.
    if (m_totalMatchCount > 0) {
        ASSERT(!parent());
        ++m_findMatchMarkersVersion;
    }
}

void WebFrameImpl::createFrameView()
{
    TRACE_EVENT0("webkit", "WebFrameImpl::createFrameView");

    ASSERT(frame()); // If frame() doesn't exist, we probably didn't init properly.

    WebViewImpl* webView = viewImpl();
    bool isMainFrame = webView->mainFrameImpl()->frame() == frame();
    if (isMainFrame)
        webView->suppressInvalidations(true);

    frame()->createView(webView->size(), webView->baseBackgroundColor(), webView->isTransparent());
    if (webView->shouldAutoResize() && isMainFrame)
        frame()->view()->enableAutoSizeMode(true, webView->minAutoSize(), webView->maxAutoSize());

    frame()->view()->setInputEventsTransformForEmulation(m_inputEventsOffsetForEmulation, m_inputEventsScaleFactorForEmulation);

    if (isMainFrame)
        webView->suppressInvalidations(false);
}

WebFrameImpl* WebFrameImpl::fromFrame(LocalFrame* frame)
{
    if (!frame)
        return 0;
    FrameLoaderClient* client = frame->loader().client();
    if (!client || !client->isFrameLoaderClientImpl())
        return 0;
    return toFrameLoaderClientImpl(client)->webFrame();
}

WebFrameImpl* WebFrameImpl::fromFrameOwnerElement(Element* element)
{
    // FIXME: Why do we check specifically for <iframe> and <frame> here? Why can't we get the WebFrameImpl from an <object> element, for example.
    if (!element || !element->isFrameOwnerElement() || (!element->hasTagName(HTMLNames::iframeTag) && !element->hasTagName(HTMLNames::frameTag)))
        return 0;
    return fromFrame(toHTMLFrameOwnerElement(element)->contentFrame());
}

WebViewImpl* WebFrameImpl::viewImpl() const
{
    if (!frame())
        return 0;
    return WebViewImpl::fromPage(frame()->page());
}

WebDataSourceImpl* WebFrameImpl::dataSourceImpl() const
{
    return static_cast<WebDataSourceImpl*>(dataSource());
}

WebDataSourceImpl* WebFrameImpl::provisionalDataSourceImpl() const
{
    return static_cast<WebDataSourceImpl*>(provisionalDataSource());
}

void WebFrameImpl::setFindEndstateFocusAndSelection()
{
    WebFrameImpl* mainFrameImpl = viewImpl()->mainFrameImpl();

    if (this == mainFrameImpl->activeMatchFrame() && m_activeMatch.get()) {
        // If the user has set the selection since the match was found, we
        // don't focus anything.
        VisibleSelection selection(frame()->selection().selection());
        if (!selection.isNone())
            return;

        // Try to find the first focusable node up the chain, which will, for
        // example, focus links if we have found text within the link.
        Node* node = m_activeMatch->firstNode();
        if (node && node->isInShadowTree()) {
            Node* host = node->deprecatedShadowAncestorNode();
            if (host->hasTagName(HTMLNames::inputTag) || host->hasTagName(HTMLNames::textareaTag))
                node = host;
        }
        for (; node; node = node->parentNode()) {
            if (!node->isElementNode())
                continue;
            Element* element = toElement(node);
            if (element->isFocusable()) {
                // Found a focusable parent node. Set the active match as the
                // selection and focus to the focusable node.
                frame()->selection().setSelection(VisibleSelection(m_activeMatch.get()));
                frame()->document()->setFocusedElement(element);
                return;
            }
        }

        // Iterate over all the nodes in the range until we find a focusable node.
        // This, for example, sets focus to the first link if you search for
        // text and text that is within one or more links.
        node = m_activeMatch->firstNode();
        for (; node && node != m_activeMatch->pastLastNode(); node = NodeTraversal::next(*node)) {
            if (!node->isElementNode())
                continue;
            Element* element = toElement(node);
            if (element->isFocusable()) {
                frame()->document()->setFocusedElement(element);
                return;
            }
        }

        // No node related to the active match was focusable, so set the
        // active match as the selection (so that when you end the Find session,
        // you'll have the last thing you found highlighted) and make sure that
        // we have nothing focused (otherwise you might have text selected but
        // a link focused, which is weird).
        frame()->selection().setSelection(VisibleSelection(m_activeMatch.get()));
        frame()->document()->setFocusedElement(nullptr);

        // Finally clear the active match, for two reasons:
        // We just finished the find 'session' and we don't want future (potentially
        // unrelated) find 'sessions' operations to start at the same place.
        // The WebFrameImpl could get reused and the m_activeMatch could end up pointing
        // to a document that is no longer valid. Keeping an invalid reference around
        // is just asking for trouble.
        m_activeMatch = nullptr;
    }
}

void WebFrameImpl::didFail(const ResourceError& error, bool wasProvisional)
{
    if (!client())
        return;
    WebURLError webError = error;
    if (wasProvisional)
        client()->didFailProvisionalLoad(this, webError);
    else
        client()->didFailLoad(this, webError);
}

void WebFrameImpl::setCanHaveScrollbars(bool canHaveScrollbars)
{
    frame()->view()->setCanHaveScrollbars(canHaveScrollbars);
}

void WebFrameImpl::setInputEventsTransformForEmulation(const IntSize& offset, float contentScaleFactor)
{
    m_inputEventsOffsetForEmulation = offset;
    m_inputEventsScaleFactorForEmulation = contentScaleFactor;
    if (frame()->view())
        frame()->view()->setInputEventsTransformForEmulation(m_inputEventsOffsetForEmulation, m_inputEventsScaleFactorForEmulation);
}

void WebFrameImpl::invalidateArea(AreaToInvalidate area)
{
    ASSERT(frame() && frame()->view());
    FrameView* view = frame()->view();

    if ((area & InvalidateAll) == InvalidateAll)
        view->invalidateRect(view->frameRect());
    else {
        if ((area & InvalidateContentArea) == InvalidateContentArea) {
            IntRect contentArea(
                view->x(), view->y(), view->visibleWidth(), view->visibleHeight());
            IntRect frameRect = view->frameRect();
            contentArea.move(-frameRect.x(), -frameRect.y());
            view->invalidateRect(contentArea);
        }
    }

    if ((area & InvalidateScrollbar) == InvalidateScrollbar) {
        // Invalidate the vertical scroll bar region for the view.
        Scrollbar* scrollbar = view->verticalScrollbar();
        if (scrollbar)
            scrollbar->invalidate();
    }
}

void WebFrameImpl::addMarker(Range* range, bool activeMatch)
{
    frame()->document()->markers().addTextMatchMarker(range, activeMatch);
}

void WebFrameImpl::setMarkerActive(Range* range, bool active)
{
    if (!range || range->collapsed(IGNORE_EXCEPTION))
        return;
    frame()->document()->markers().setMarkersActive(range, active);
}

int WebFrameImpl::ordinalOfFirstMatchForFrame(WebFrameImpl* frame) const
{
    int ordinal = 0;
    WebFrameImpl* mainFrameImpl = viewImpl()->mainFrameImpl();
    // Iterate from the main frame up to (but not including) |frame| and
    // add up the number of matches found so far.
    for (WebFrameImpl* it = mainFrameImpl; it != frame; it = toWebFrameImpl(it->traverseNext(true))) {
        if (it->m_lastMatchCount > 0)
            ordinal += it->m_lastMatchCount;
    }
    return ordinal;
}

bool WebFrameImpl::shouldScopeMatches(const String& searchText)
{
    // Don't scope if we can't find a frame or a view.
    // The user may have closed the tab/application, so abort.
    // Also ignore detached frames, as many find operations report to the main frame.
    if (!frame() || !frame()->view() || !frame()->page() || !hasVisibleContent())
        return false;

    ASSERT(frame()->document() && frame()->view());

    // If the frame completed the scoping operation and found 0 matches the last
    // time it was searched, then we don't have to search it again if the user is
    // just adding to the search string or sending the same search string again.
    if (m_lastFindRequestCompletedWithNoMatches && !m_lastSearchString.isEmpty()) {
        // Check to see if the search string prefixes match.
        String previousSearchPrefix =
            searchText.substring(0, m_lastSearchString.length());

        if (previousSearchPrefix == m_lastSearchString)
            return false; // Don't search this frame, it will be fruitless.
    }

    return true;
}

void WebFrameImpl::scopeStringMatchesSoon(int identifier, const WebString& searchText, const WebFindOptions& options, bool reset)
{
    m_deferredScopingWork.append(new DeferredScopeStringMatches(this, identifier, searchText, options, reset));
}

void WebFrameImpl::callScopeStringMatches(DeferredScopeStringMatches* caller, int identifier, const WebString& searchText, const WebFindOptions& options, bool reset)
{
    m_deferredScopingWork.remove(m_deferredScopingWork.find(caller));
    scopeStringMatches(identifier, searchText, options, reset);

    // This needs to happen last since searchText is passed by reference.
    delete caller;
}

void WebFrameImpl::invalidateIfNecessary()
{
    if (m_lastMatchCount <= m_nextInvalidateAfter)
        return;

    // FIXME: (http://b/1088165) Optimize the drawing of the tickmarks and
    // remove this. This calculation sets a milestone for when next to
    // invalidate the scrollbar and the content area. We do this so that we
    // don't spend too much time drawing the scrollbar over and over again.
    // Basically, up until the first 500 matches there is no throttle.
    // After the first 500 matches, we set set the milestone further and
    // further out (750, 1125, 1688, 2K, 3K).
    static const int startSlowingDownAfter = 500;
    static const int slowdown = 750;

    int i = m_lastMatchCount / startSlowingDownAfter;
    m_nextInvalidateAfter += i * slowdown;
    invalidateArea(InvalidateScrollbar);
}

void WebFrameImpl::loadJavaScriptURL(const KURL& url)
{
    // This is copied from ScriptController::executeScriptIfJavaScriptURL.
    // Unfortunately, we cannot just use that method since it is private, and
    // it also doesn't quite behave as we require it to for bookmarklets.  The
    // key difference is that we need to suppress loading the string result
    // from evaluating the JS URL if executing the JS URL resulted in a
    // location change.  We also allow a JS URL to be loaded even if scripts on
    // the page are otherwise disabled.

    if (!frame()->document() || !frame()->page())
        return;

    RefPtr<Document> ownerDocument(frame()->document());

    // Protect privileged pages against bookmarklets and other javascript manipulations.
    if (SchemeRegistry::shouldTreatURLSchemeAsNotAllowingJavascriptURLs(frame()->document()->url().protocol()))
        return;

    String script = decodeURLEscapeSequences(url.string().substring(strlen("javascript:")));
    UserGestureIndicator gestureIndicator(DefinitelyProcessingNewUserGesture);
    ScriptValue result = frame()->script().executeScriptInMainWorldAndReturnValue(ScriptSourceCode(script));

    String scriptResult;
    if (!result.getString(scriptResult))
        return;

    if (!frame()->navigationScheduler().locationChangePending())
        frame()->document()->loader()->replaceDocument(scriptResult, ownerDocument.get());
}

void WebFrameImpl::willDetachParent()
{
    // Do not expect string scoping results from any frames that got detached
    // in the middle of the operation.
    if (m_scopingInProgress) {

        // There is a possibility that the frame being detached was the only
        // pending one. We need to make sure final replies can be sent.
        flushCurrentScopingEffort(m_findRequestIdentifier);

        cancelPendingScopingEffort();
    }
}

} // namespace blink
