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

#ifndef InternalSettings_h
#define InternalSettings_h

#include "InternalSettingsGenerated.h"
#include "core/editing/EditingBehaviorTypes.h"
#include "heap/Handle.h"
#include "platform/geometry/IntSize.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefCounted.h"
#include "wtf/text/WTFString.h"

namespace WebCore {

class Document;
class ExceptionState;
class LocalFrame;
class Page;
class Settings;

class InternalSettings FINAL : public InternalSettingsGenerated {
public:
    class Backup {
    public:
        explicit Backup(Settings*);
        void restoreTo(Settings*);

        bool m_originalCSSExclusionsEnabled;
        bool m_originalAuthorShadowDOMForAnyElementEnabled;
        bool m_originalExperimentalWebSocketEnabled;
        bool m_originalStyleScoped;
        bool m_originalCSP;
        bool m_originalOverlayScrollbarsEnabled;
        EditingBehaviorType m_originalEditingBehavior;
        bool m_originalTextAutosizingEnabled;
        IntSize m_originalTextAutosizingWindowSizeOverride;
        float m_originalAccessibilityFontScaleFactor;
        String m_originalMediaTypeOverride;
        bool m_originalMockScrollbarsEnabled;
        bool m_langAttributeAwareFormControlUIEnabled;
        bool m_imagesEnabled;
        bool m_shouldDisplaySubtitles;
        bool m_shouldDisplayCaptions;
        bool m_shouldDisplayTextDescriptions;
        String m_defaultVideoPosterURL;
        bool m_originalCompositorDrivenAcceleratedScrollEnabled;
        bool m_originalLayerSquashingEnabled;
        bool m_originalPasswordGenerationDecorationEnabled;
    };

    static PassRefPtrWillBeRawPtr<InternalSettings> create(Page& page)
    {
        return adoptRefWillBeNoop(new InternalSettings(page));
    }
    static InternalSettings* from(Page&);
    void hostDestroyed() { m_page = 0; }

    virtual ~InternalSettings();
    void resetToConsistentState();

    void setStandardFontFamily(const AtomicString& family, const String& script, ExceptionState&);
    void setSerifFontFamily(const AtomicString& family, const String& script, ExceptionState&);
    void setSansSerifFontFamily(const AtomicString& family, const String& script, ExceptionState&);
    void setFixedFontFamily(const AtomicString& family, const String& script, ExceptionState&);
    void setCursiveFontFamily(const AtomicString& family, const String& script, ExceptionState&);
    void setFantasyFontFamily(const AtomicString& family, const String& script, ExceptionState&);
    void setPictographFontFamily(const AtomicString& family, const String& script, ExceptionState&);

    void setDefaultVideoPosterURL(const String& url, ExceptionState&);
    void setEditingBehavior(const String&, ExceptionState&);
    void setImagesEnabled(bool, ExceptionState&);
    void setMediaTypeOverride(const String& mediaType, ExceptionState&);
    void setMockScrollbarsEnabled(bool, ExceptionState&);
    void setPasswordGenerationDecorationEnabled(bool, ExceptionState&);
    void setTextAutosizingEnabled(bool, ExceptionState&);
    void setAccessibilityFontScaleFactor(float fontScaleFactor, ExceptionState&);
    void setTextAutosizingWindowSizeOverride(int width, int height, ExceptionState&);
    void setTouchEventEmulationEnabled(bool, ExceptionState&);
    void setViewportEnabled(bool, ExceptionState&);

    // FIXME: This is a temporary flag and should be removed once accelerated
    // overflow scroll is ready (crbug.com/254111).
    void setCompositorDrivenAcceleratedScrollingEnabled(bool, ExceptionState&);

    // FIXME: This is a temporary flag and should be removed once squashing is
    // ready (crbug.com/261605).
    void setLayerSquashingEnabled(bool, ExceptionState&);

    // FIXME: The following are RuntimeEnabledFeatures and likely
    // cannot be changed after process start. These setters should
    // be removed or moved onto internals.runtimeFlags:
    void setAuthorShadowDOMForAnyElementEnabled(bool);
    void setCSSExclusionsEnabled(bool);
    void setExperimentalWebSocketEnabled(bool);
    void setLangAttributeAwareFormControlUIEnabled(bool);
    void setOverlayScrollbarsEnabled(bool);
    void setStyleScopedEnabled(bool);
    void setExperimentalContentSecurityPolicyFeaturesEnabled(bool);

    virtual void trace(Visitor* visitor) OVERRIDE { InternalSettingsGenerated::trace(visitor); }

private:
    explicit InternalSettings(Page&);

    Settings* settings() const;
    Page* page() const { return m_page; }
    static const char* supplementName();

    Page* m_page;
    Backup m_backup;
};

} // namespace WebCore

#endif
