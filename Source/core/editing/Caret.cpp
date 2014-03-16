/*
 * Copyright (C) 2004, 2008, 2009, 2010 Apple Inc. All rights reserved.
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

#include "config.h"
#include "core/editing/Caret.h"

#include "core/dom/Document.h"
#include "core/editing/htmlediting.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Settings.h"
#include "core/rendering/RenderBlock.h"
#include "core/rendering/RenderView.h"

namespace WebCore {

CaretBase::CaretBase(CaretVisibility visibility)
    : m_caretRectNeedsUpdate(true)
    , m_caretVisibility(visibility)
{
}

DragCaretController::DragCaretController()
    : CaretBase(Visible)
{
}

PassOwnPtr<DragCaretController> DragCaretController::create()
{
    return adoptPtr(new DragCaretController);
}

bool DragCaretController::isContentRichlyEditable() const
{
    return isRichlyEditablePosition(m_position.deepEquivalent());
}

void DragCaretController::setCaretPosition(const VisiblePosition& position)
{
    if (Node* node = m_position.deepEquivalent().deprecatedNode())
        invalidateCaretRect(node);
    m_position = position;
    setCaretRectNeedsUpdate();
    Document* document = 0;
    if (Node* node = m_position.deepEquivalent().deprecatedNode()) {
        invalidateCaretRect(node);
        document = &node->document();
    }
    if (m_position.isNull() || m_position.isOrphan())
        clearCaretRect();
    else
        updateCaretRect(document, m_position);
}

static bool removingNodeRemovesPosition(Node& node, const Position& position)
{
    if (!position.anchorNode())
        return false;

    if (position.anchorNode() == node)
        return true;

    if (!node.isElementNode())
        return false;

    Element& element = toElement(node);
    return element.containsIncludingShadowDOM(position.anchorNode());
}

void DragCaretController::nodeWillBeRemoved(Node& node)
{
    if (!hasCaret() || !node.inActiveDocument())
        return;

    if (!removingNodeRemovesPosition(node, m_position.deepEquivalent()))
        return;

    m_position.deepEquivalent().document()->renderView()->clearSelection();
    clear();
}

void CaretBase::clearCaretRect()
{
    m_caretLocalRect = LayoutRect();
}

static inline bool caretRendersInsideNode(Node* node)
{
    return node && !isRenderedTable(node) && !editingIgnoresContent(node);
}

RenderObject* CaretBase::caretRenderer(Node* node)
{
    if (!node)
        return 0;

    RenderObject* renderer = node->renderer();
    if (!renderer)
        return 0;

    // if caretNode is a block and caret is inside it then caret should be painted by that block
    bool paintedByBlock = renderer->isRenderBlock() && caretRendersInsideNode(node);
    return paintedByBlock ? renderer : renderer->containingBlock();
}

bool CaretBase::updateCaretRect(Document* document, const VisiblePosition& caretPosition)
{
    document->updateStyleIfNeeded();
    m_caretLocalRect = LayoutRect();

    m_caretRectNeedsUpdate = false;

    if (caretPosition.isNull())
        return false;

    ASSERT(caretPosition.deepEquivalent().deprecatedNode()->renderer());

    // First compute a rect local to the renderer at the selection start.
    RenderObject* renderer;
    LayoutRect localRect = caretPosition.localCaretRect(renderer);

    // Get the renderer that will be responsible for painting the caret
    // (which is either the renderer we just found, or one of its containers).
    RenderObject* caretPainter = caretRenderer(caretPosition.deepEquivalent().deprecatedNode());

    // Compute an offset between the renderer and the caretPainter.
    bool unrooted = false;
    while (renderer != caretPainter) {
        RenderObject* containerObject = renderer->container();
        if (!containerObject) {
            unrooted = true;
            break;
        }
        localRect.move(renderer->offsetFromContainer(containerObject, localRect.location()));
        renderer = containerObject;
    }

    if (!unrooted)
        m_caretLocalRect = localRect;

    return true;
}

RenderObject* DragCaretController::caretRenderer() const
{
    return CaretBase::caretRenderer(m_position.deepEquivalent().deprecatedNode());
}

IntRect CaretBase::absoluteBoundsForLocalRect(Node* node, const LayoutRect& rect) const
{
    RenderObject* caretPainter = caretRenderer(node);
    if (!caretPainter)
        return IntRect();

    LayoutRect localRect(rect);
    if (caretPainter->isBox())
        toRenderBox(caretPainter)->flipForWritingMode(localRect);
    return caretPainter->localToAbsoluteQuad(FloatRect(localRect)).enclosingBoundingBox();
}

void CaretBase::repaintCaretForLocalRect(Node* node, const LayoutRect& rect)
{
    RenderObject* caretPainter = caretRenderer(node);
    if (!caretPainter)
        return;

    // FIXME: Need to over-paint 1 pixel to workaround some rounding problems.
    // https://bugs.webkit.org/show_bug.cgi?id=108283
    LayoutRect inflatedRect = rect;
    inflatedRect.inflate(1);

    caretPainter->repaintRectangle(inflatedRect);
}

bool CaretBase::shouldRepaintCaret(const RenderView* view, bool isContentEditable) const
{
    ASSERT(view);
    bool caretBrowsing = false;
    if (FrameView* frameView = view->frameView()) {
        LocalFrame& frame = frameView->frame(); // The frame where the selection started
        caretBrowsing = frame.settings() && frame.settings()->caretBrowsingEnabled();
    }
    return (caretBrowsing || isContentEditable);
}

void CaretBase::invalidateCaretRect(Node* node, bool caretRectChanged)
{
    // EDIT FIXME: This is an unfortunate hack.
    // Basically, we can't trust this layout position since we
    // can't guarantee that the check to see if we are in unrendered
    // content will work at this point. We may have to wait for
    // a layout and re-render of the document to happen. So, resetting this
    // flag will cause another caret layout to happen the first time
    // that we try to paint the caret after this call. That one will work since
    // it happens after the document has accounted for any editing
    // changes which may have been done.
    // And, we need to leave this layout here so the caret moves right
    // away after clicking.
    m_caretRectNeedsUpdate = true;

    if (caretRectChanged)
        return;

    if (RenderView* view = node->document().renderView()) {
        if (shouldRepaintCaret(view, node->isContentEditable(Node::UserSelectAllIsAlwaysNonEditable)))
            repaintCaretForLocalRect(node, localCaretRectWithoutUpdate());
    }
}

void CaretBase::paintCaret(Node* node, GraphicsContext* context, const LayoutPoint& paintOffset, const LayoutRect& clipRect) const
{
    if (m_caretVisibility == Hidden)
        return;

    LayoutRect drawingRect = localCaretRectWithoutUpdate();
    RenderObject* renderer = caretRenderer(node);
    if (renderer && renderer->isBox())
        toRenderBox(renderer)->flipForWritingMode(drawingRect);
    drawingRect.moveBy(roundedIntPoint(paintOffset));
    LayoutRect caret = intersection(drawingRect, clipRect);
    if (caret.isEmpty())
        return;

    Color caretColor = Color::black;

    Element* element;
    if (node->isElementNode())
        element = toElement(node);
    else
        element = node->parentElement();

    if (element && element->renderer())
        caretColor = element->renderer()->resolveColor(CSSPropertyColor);

    context->fillRect(caret, caretColor);
}

void DragCaretController::paintDragCaret(LocalFrame* frame, GraphicsContext* p, const LayoutPoint& paintOffset, const LayoutRect& clipRect) const
{
    if (m_position.deepEquivalent().deprecatedNode()->document().frame() == frame)
        paintCaret(m_position.deepEquivalent().deprecatedNode(), p, paintOffset, clipRect);
}

}
