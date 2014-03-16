/*
 * Copyright (C) 2007, 2009 Apple Inc. All rights reserved.
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

#ifndef DragController_h
#define DragController_h

#include "core/page/DragActions.h"
#include "platform/geometry/IntPoint.h"
#include "platform/weborigin/KURL.h"
#include "wtf/Forward.h"

namespace WebCore {

    class Clipboard;
    class Document;
    class DragClient;
    class DragData;
    class DragImage;
    struct DragSession;
    class DragState;
    class Element;
    class LocalFrame;
    class FrameSelection;
    class HTMLInputElement;
    class Image;
    class IntRect;
    class Node;
    class Page;
    class PlatformMouseEvent;
    class Range;

    class DragController {
        WTF_MAKE_NONCOPYABLE(DragController); WTF_MAKE_FAST_ALLOCATED;
    public:
        ~DragController();

        static PassOwnPtr<DragController> create(Page*, DragClient*);

        DragSession dragEntered(DragData*);
        void dragExited(DragData*);
        DragSession dragUpdated(DragData*);
        bool performDrag(DragData*);

        enum SelectionDragPolicy {
            ImmediateSelectionDragResolution,
            DelayedSelectionDragResolution,
        };
        Node* draggableNode(const LocalFrame*, Node*, const IntPoint&, SelectionDragPolicy, DragSourceAction&) const;
        void dragEnded();

        bool populateDragClipboard(LocalFrame* src, const DragState&, const IntPoint& dragOrigin);
        bool startDrag(LocalFrame* src, const DragState&, const PlatformMouseEvent& dragEvent, const IntPoint& dragOrigin);

        static const int DragIconRightInset;
        static const int DragIconBottomInset;

    private:
        DragController(Page*, DragClient*);

        bool dispatchTextInputEventFor(LocalFrame*, DragData*);
        bool canProcessDrag(DragData*);
        bool concludeEditDrag(DragData*);
        DragSession dragEnteredOrUpdated(DragData*);
        DragOperation operationForLoad(DragData*);
        bool tryDocumentDrag(DragData*, DragDestinationAction, DragSession&);
        bool tryDHTMLDrag(DragData*, DragOperation&);
        DragOperation dragOperation(DragData*);
        void cancelDrag();
        bool dragIsMove(FrameSelection&, DragData*);
        bool isCopyKeyDown(DragData*);

        void mouseMovedIntoDocument(Document*);

        void doSystemDrag(DragImage*, const IntPoint& dragLocation, const IntPoint& dragOrigin, Clipboard*, LocalFrame*, bool forLink);
        void cleanupAfterSystemDrag();

        Page* m_page;
        DragClient* m_client;

        RefPtr<Document> m_documentUnderMouse; // The document the mouse was last dragged over.
        RefPtr<Document> m_dragInitiator; // The Document (if any) that initiated the drag.
        RefPtr<HTMLInputElement> m_fileInputElementUnderMouse;
        bool m_documentIsHandlingDrag;

        DragDestinationAction m_dragDestinationAction;
        bool m_didInitiateDrag;
    };

}

#endif
