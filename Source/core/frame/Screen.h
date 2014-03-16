/*
 * Copyright (C) 2007 Apple Inc.  All rights reserved.
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
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
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


#ifndef Screen_h
#define Screen_h

#include "bindings/v8/ScriptWrappable.h"
#include "core/events/EventTarget.h"
#include "core/frame/DOMWindowProperty.h"
#include "heap/Handle.h"
#include "platform/Supplementable.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefCounted.h"

namespace WebCore {

    class LocalFrame;

    class Screen FINAL : public RefCountedWillBeRefCountedGarbageCollected<Screen>, public ScriptWrappable, public EventTargetWithInlineData, public DOMWindowProperty, public WillBeHeapSupplementable<Screen> {
        WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(Screen);
        DEFINE_EVENT_TARGET_REFCOUNTING(RefCountedWillBeRefCountedGarbageCollected<Screen>);
    public:
        static PassRefPtrWillBeRawPtr<Screen> create(LocalFrame* frame)
        {
            return adoptRefWillBeRefCountedGarbageCollected(new Screen(frame));
        }

        unsigned height() const;
        unsigned width() const;
        unsigned colorDepth() const;
        unsigned pixelDepth() const;
        int availLeft() const;
        int availTop() const;
        unsigned availHeight() const;
        unsigned availWidth() const;

        // EventTarget.
        virtual const AtomicString& interfaceName() const OVERRIDE;
        virtual ExecutionContext* executionContext() const OVERRIDE;

        void trace(Visitor*);

    private:
        explicit Screen(LocalFrame*);
    };

} // namespace WebCore

#endif // Screen_h
