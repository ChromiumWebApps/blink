// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef RemoteFontFaceSource_h
#define RemoteFontFaceSource_h

#include "core/css/CSSFontFaceSource.h"
#include "core/fetch/FontResource.h"
#include "core/fetch/ResourcePtr.h"

namespace WebCore {

class RemoteFontFaceSource : public CSSFontFaceSource, public FontResourceClient {
public:
    RemoteFontFaceSource(FontResource*);
    virtual ~RemoteFontFaceSource();

    virtual FontResource* resource() OVERRIDE { return m_font.get(); }
    virtual bool isLoading() const OVERRIDE;
    virtual bool isLoaded() const OVERRIDE;
    virtual bool isValid() const OVERRIDE;

    void beginLoadIfNeeded();
    virtual bool ensureFontData();

#if ENABLE(SVG_FONTS)
    virtual bool isSVGFontFaceSource() const { return false; }
#endif

    virtual void didStartFontLoad(FontResource*) OVERRIDE;
    virtual void fontLoaded(FontResource*) OVERRIDE;
    virtual void fontLoadWaitLimitExceeded(FontResource*) OVERRIDE;

    // For UMA reporting
    virtual bool hadBlankText() OVERRIDE { return m_histograms.hadBlankText(); }
    void paintRequested() { m_histograms.fallbackFontPainted(); }

protected:
    virtual PassRefPtr<SimpleFontData> createFontData(const FontDescription&) OVERRIDE;
    PassRefPtr<SimpleFontData> createLoadingFallbackFontData(const FontDescription&);
    void pruneTable();

private:
    class FontLoadHistograms {
    public:
        FontLoadHistograms() : m_loadStartTime(0), m_fallbackPaintTime(0) { }
        void loadStarted();
        void fallbackFontPainted();
        void recordRemoteFont(const FontResource*);
        void recordFallbackTime(const FontResource*);
        bool hadBlankText() { return m_fallbackPaintTime; }
    private:
        const char* histogramName(const FontResource*);
        double m_loadStartTime;
        double m_fallbackPaintTime;
    };

    ResourcePtr<FontResource> m_font;
    FontLoadHistograms m_histograms;
};

} // namespace WebCore

#endif
