/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 * Copyright (C) 2013 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef Internals_h
#define Internals_h

#include "bindings/v8/ExceptionStatePlaceholder.h"
#include "bindings/v8/ScriptPromise.h"
#include "bindings/v8/ScriptValue.h"
#include "core/css/CSSComputedStyleDeclaration.h"
#include "core/dom/ContextLifecycleObserver.h"
#include "core/dom/NodeList.h"
#include "core/page/scrolling/ScrollingCoordinator.h"
#include "heap/Handle.h"
#include "wtf/ArrayBuffer.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefCounted.h"
#include "wtf/text/WTFString.h"

namespace WebCore {

class ClientRect;
class ClientRectList;
class DOMPoint;
class DOMStringList;
class DOMWindow;
class Document;
class DocumentMarker;
class Element;
class ExceptionState;
class LocalFrame;
class GCObservation;
class InspectorFrontendChannelDummy;
class InternalProfilers;
class InternalRuntimeFlags;
class InternalSettings;
class LayerRectList;
class MallocStatistics;
class Node;
class Page;
class PagePopupController;
class Range;
class ExecutionContext;
class SerializedScriptValue;
class ShadowRoot;
class TypeConversions;

class Internals FINAL : public RefCountedWillBeGarbageCollectedFinalized<Internals>, public ContextLifecycleObserver {
public:
    static PassRefPtrWillBeRawPtr<Internals> create(Document*);
    virtual ~Internals();

    static void resetToConsistentState(Page*);

    String elementRenderTreeAsText(Element*, ExceptionState&);

    String address(Node*);

    PassRefPtrWillBeRawPtr<GCObservation> observeGC(ScriptValue);

    bool isPreloaded(const String& url);
    bool isLoadingFromMemoryCache(const String& url);

    void crash();

    void setStyleResolverStatsEnabled(bool);
    String styleResolverStatsReport(ExceptionState&) const;
    String styleResolverStatsTotalsReport(ExceptionState&) const;

    bool isSharingStyle(Element*, Element*, ExceptionState&) const;

    size_t numberOfScopedHTMLStyleChildren(const Node*, ExceptionState&) const;
    PassRefPtr<CSSComputedStyleDeclaration> computedStyleIncludingVisitedInfo(Node*, ExceptionState&) const;

    ShadowRoot* shadowRoot(Element* host, ExceptionState&);
    ShadowRoot* youngestShadowRoot(Element* host, ExceptionState&);
    ShadowRoot* oldestShadowRoot(Element* host, ExceptionState&);
    ShadowRoot* youngerShadowRoot(Node* shadow, ExceptionState&);
    String shadowRootType(const Node*, ExceptionState&) const;
    bool hasShadowInsertionPoint(const Node*, ExceptionState&) const;
    bool hasContentElement(const Node*, ExceptionState&) const;
    size_t countElementShadow(const Node*, ExceptionState&) const;
    const AtomicString& shadowPseudoId(Element*, ExceptionState&);
    void setShadowPseudoId(Element*, const AtomicString&, ExceptionState&);

    // CSS Animation / Transition testing.
    unsigned numberOfActiveAnimations() const;
    void pauseAnimations(double pauseTime, ExceptionState&);

    bool isValidContentSelect(Element* insertionPoint, ExceptionState&);
    Node* treeScopeRootNode(Node*, ExceptionState&);
    Node* parentTreeScope(Node*, ExceptionState&);
    bool hasSelectorForIdInShadow(Element* host, const AtomicString& idValue, ExceptionState&);
    bool hasSelectorForClassInShadow(Element* host, const AtomicString& className, ExceptionState&);
    bool hasSelectorForAttributeInShadow(Element* host, const AtomicString& attributeName, ExceptionState&);
    bool hasSelectorForPseudoClassInShadow(Element* host, const String& pseudoClass, ExceptionState&);
    unsigned short compareTreeScopePosition(const Node*, const Node*, ExceptionState&) const;

    // FIXME: Rename these functions if walker is prefered.
    Node* nextSiblingByWalker(Node*, ExceptionState&);
    Node* firstChildByWalker(Node*, ExceptionState&);
    Node* lastChildByWalker(Node*, ExceptionState&);
    Node* nextNodeByWalker(Node*, ExceptionState&);
    Node* previousNodeByWalker(Node*, ExceptionState&);

    unsigned updateStyleAndReturnAffectedElementCount(ExceptionState&) const;
    unsigned needsLayoutCount(ExceptionState&) const;

    String visiblePlaceholder(Element*);
    void selectColorInColorChooser(Element*, const String& colorValue);
    bool hasAutofocusRequest(Document*);
    bool hasAutofocusRequest();
    Vector<String> formControlStateOfHistoryItem(ExceptionState&);
    void setFormControlStateOfHistoryItem(const Vector<String>&, ExceptionState&);
    void setEnableMockPagePopup(bool, ExceptionState&);
    PassRefPtrWillBeRawPtr<PagePopupController> pagePopupController();

    PassRefPtr<ClientRect> unscaledViewportRect(ExceptionState&);

    PassRefPtr<ClientRect> absoluteCaretBounds(ExceptionState&);

    PassRefPtr<ClientRect> boundingBox(Element*, ExceptionState&);

    PassRefPtr<ClientRectList> inspectorHighlightRects(Document*, ExceptionState&);

    unsigned markerCountForNode(Node*, const String&, ExceptionState&);
    unsigned activeMarkerCountForNode(Node*, ExceptionState&);
    PassRefPtr<Range> markerRangeForNode(Node*, const String& markerType, unsigned index, ExceptionState&);
    String markerDescriptionForNode(Node*, const String& markerType, unsigned index, ExceptionState&);
    void addTextMatchMarker(const Range*, bool isActive);
    void setMarkersActive(Node*, unsigned startOffset, unsigned endOffset, bool, ExceptionState&);
    void setMarkedTextMatchesAreHighlighted(Document*, bool, ExceptionState&);

    void setScrollViewPosition(Document*, long x, long y, ExceptionState&);
    String viewportAsText(Document*, float devicePixelRatio, int availableWidth, int availableHeight, ExceptionState&);

    bool wasLastChangeUserEdit(Element* textField, ExceptionState&);
    bool elementShouldAutoComplete(Element* inputElement, ExceptionState&);
    String suggestedValue(Element*, ExceptionState&);
    void setSuggestedValue(Element*, const String&, ExceptionState&);
    void setEditingValue(Element* inputElement, const String&, ExceptionState&);
    void setAutofilled(Element*, bool enabled, ExceptionState&);
    void scrollElementToRect(Element*, long x, long y, long w, long h, ExceptionState&);

    PassRefPtr<Range> rangeFromLocationAndLength(Element* scope, int rangeLocation, int rangeLength, ExceptionState&);
    unsigned locationFromRange(Element* scope, const Range*, ExceptionState&);
    unsigned lengthFromRange(Element* scope, const Range*, ExceptionState&);
    String rangeAsText(const Range*, ExceptionState&);

    PassRefPtr<DOMPoint> touchPositionAdjustedToBestClickableNode(long x, long y, long width, long height, Document*, ExceptionState&);
    Node* touchNodeAdjustedToBestClickableNode(long x, long y, long width, long height, Document*, ExceptionState&);
    PassRefPtr<DOMPoint> touchPositionAdjustedToBestContextMenuNode(long x, long y, long width, long height, Document*, ExceptionState&);
    Node* touchNodeAdjustedToBestContextMenuNode(long x, long y, long width, long height, Document*, ExceptionState&);
    PassRefPtr<ClientRect> bestZoomableAreaForTouchPoint(long x, long y, long width, long height, Document*, ExceptionState&);

    int lastSpellCheckRequestSequence(Document*, ExceptionState&);
    int lastSpellCheckProcessedSequence(Document*, ExceptionState&);

    Vector<AtomicString> userPreferredLanguages() const;
    void setUserPreferredLanguages(const Vector<String>&);

    unsigned activeDOMObjectCount(Document*, ExceptionState&);
    unsigned wheelEventHandlerCount(Document*, ExceptionState&);
    unsigned touchEventHandlerCount(Document*, ExceptionState&);
    PassRefPtrWillBeRawPtr<LayerRectList> touchEventTargetLayerRects(Document*, ExceptionState&);

    // This is used to test rect based hit testing like what's done on touch screens.
    PassRefPtr<NodeList> nodesFromRect(Document*, int x, int y, unsigned topPadding, unsigned rightPadding,
        unsigned bottomPadding, unsigned leftPadding, bool ignoreClipping, bool allowShadowContent, bool allowChildFrameContent, ExceptionState&) const;

    void emitInspectorDidBeginFrame(int frameId = 0);
    void emitInspectorDidCancelFrame();

    bool hasSpellingMarker(Document*, int from, int length, ExceptionState&);
    bool hasGrammarMarker(Document*, int from, int length, ExceptionState&);
    void setContinuousSpellCheckingEnabled(bool enabled, ExceptionState&);

    bool isOverwriteModeEnabled(Document*, ExceptionState&);
    void toggleOverwriteModeEnabled(Document*, ExceptionState&);

    unsigned numberOfScrollableAreas(Document*, ExceptionState&);

    bool isPageBoxVisible(Document*, int pageNumber, ExceptionState&);

    static const char* internalsId;

    InternalSettings* settings() const;
    InternalRuntimeFlags* runtimeFlags() const;
    InternalProfilers* profilers();
    unsigned workerThreadCount() const;

    void setDeviceProximity(Document*, const String& eventType, double value, double min, double max, ExceptionState&);

    String layerTreeAsText(Document*, unsigned flags, ExceptionState&) const;
    String layerTreeAsText(Document*, ExceptionState&) const;
    String elementLayerTreeAsText(Element*, unsigned flags, ExceptionState&) const;
    String elementLayerTreeAsText(Element*, ExceptionState&) const;

    PassRefPtr<NodeList> paintOrderListBeforePromote(Element*, ExceptionState&);
    PassRefPtr<NodeList> paintOrderListAfterPromote(Element*, ExceptionState&);

    bool scrollsWithRespectTo(Element*, Element*, ExceptionState&);
    bool isUnclippedDescendant(Element*, ExceptionState&);
    bool needsCompositedScrolling(Element*, ExceptionState&);

    void setNeedsCompositedScrolling(Element*, unsigned value, ExceptionState&);

    String repaintRectsAsText(Document*, ExceptionState&) const;
    PassRefPtr<ClientRectList> repaintRects(Element*, ExceptionState&) const;

    String scrollingStateTreeAsText(Document*, ExceptionState&) const;
    String mainThreadScrollingReasons(Document*, ExceptionState&) const;
    PassRefPtr<ClientRectList> nonFastScrollableRects(Document*, ExceptionState&) const;

    void garbageCollectDocumentResources(Document*, ExceptionState&) const;
    void evictAllResources() const;

    void allowRoundingHacks() const;

    unsigned numberOfLiveNodes() const;
    unsigned numberOfLiveDocuments() const;
    String dumpRefCountedInstanceCounts() const;
    Vector<String> consoleMessageArgumentCounts(Document*) const;
    PassRefPtr<DOMWindow> openDummyInspectorFrontend(const String& url);
    void closeDummyInspectorFrontend();
    Vector<unsigned long> setMemoryCacheCapacities(unsigned long minDeadBytes, unsigned long maxDeadBytes, unsigned long totalBytes);
    void setInspectorResourcesDataSizeLimits(int maximumResourcesContentSize, int maximumSingleResourceContentSize, ExceptionState&);

    String counterValue(Element*);

    int pageNumber(Element*, float pageWidth = 800, float pageHeight = 600);
    Vector<String> shortcutIconURLs(Document*) const;
    Vector<String> allIconURLs(Document*) const;

    int numberOfPages(float pageWidthInPixels = 800, float pageHeightInPixels = 600);
    String pageProperty(String, int, ExceptionState& = ASSERT_NO_EXCEPTION) const;
    String pageSizeAndMarginsInPixels(int, int, int, int, int, int, int, ExceptionState& = ASSERT_NO_EXCEPTION) const;

    void setDeviceScaleFactor(float scaleFactor, ExceptionState&);

    void setIsCursorVisible(Document*, bool, ExceptionState&);

    void webkitWillEnterFullScreenForElement(Document*, Element*);
    void webkitDidEnterFullScreenForElement(Document*, Element*);
    void webkitWillExitFullScreenForElement(Document*, Element*);
    void webkitDidExitFullScreenForElement(Document*, Element*);

    void registerURLSchemeAsBypassingContentSecurityPolicy(const String& scheme);
    void removeURLSchemeRegisteredAsBypassingContentSecurityPolicy(const String& scheme);

    PassRefPtrWillBeRawPtr<MallocStatistics> mallocStatistics() const;
    PassRefPtrWillBeRawPtr<TypeConversions> typeConversions() const;

    Vector<String> getReferencedFilePaths() const;

    void startTrackingRepaints(Document*, ExceptionState&);
    void stopTrackingRepaints(Document*, ExceptionState&);
    void updateLayoutIgnorePendingStylesheetsAndRunPostLayoutTasks(ExceptionState&);
    void updateLayoutIgnorePendingStylesheetsAndRunPostLayoutTasks(Node*, ExceptionState&);

    PassRefPtr<ClientRectList> draggableRegions(Document*, ExceptionState&);
    PassRefPtr<ClientRectList> nonDraggableRegions(Document*, ExceptionState&);

    PassRefPtr<ArrayBuffer> serializeObject(PassRefPtr<SerializedScriptValue>) const;
    PassRefPtr<SerializedScriptValue> deserializeBuffer(PassRefPtr<ArrayBuffer>) const;

    String getCurrentCursorInfo(Document*, ExceptionState&);

    String markerTextForListItem(Element*, ExceptionState&);

    void forceReload(bool endToEnd);

    String getImageSourceURL(Element*, ExceptionState&);

    bool isSelectPopupVisible(Node*);

    PassRefPtr<ClientRect> selectionBounds(ExceptionState&);
    String baseURL(Document*, ExceptionState&);

    bool loseSharedGraphicsContext3D();

    void forceCompositingUpdate(Document*, ExceptionState&);

    bool isCompositorFramePending(Document*, ExceptionState&);

    void setZoomFactor(float);

    void setShouldRevealPassword(Element*, bool, ExceptionState&);

    ScriptPromise addOneToPromise(ExecutionContext*, ScriptPromise);

    void trace(Visitor*);

    void startSpeechInput(Element*);
    void setValueForUser(Element*, const String&);

    String textSurroundingNode(Node*, int x, int y, unsigned long maxLength);

private:
    explicit Internals(Document*);
    Document* contextDocument() const;
    LocalFrame* frame() const;
    Vector<String> iconURLs(Document*, int iconTypesMask) const;
    PassRefPtr<ClientRectList> annotatedRegions(Document*, bool draggable, ExceptionState&);

    DocumentMarker* markerAt(Node*, const String& markerType, unsigned index, ExceptionState&);
    RefPtr<DOMWindow> m_frontendWindow;
    OwnPtr<InspectorFrontendChannelDummy> m_frontendChannel;
    RefPtrWillBeMember<InternalRuntimeFlags> m_runtimeFlags;
    RefPtrWillBeMember<InternalProfilers> m_profilers;
};

} // namespace WebCore

#endif
