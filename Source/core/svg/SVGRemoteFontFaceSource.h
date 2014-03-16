// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SVGRemoteFontFaceSource_h
#define SVGRemoteFontFaceSource_h

#if ENABLE(SVG_FONTS)

#include "core/css/RemoteFontFaceSource.h"

namespace WebCore {

class SVGFontElement;

class SVGRemoteFontFaceSource : public RemoteFontFaceSource {
public:
    SVGRemoteFontFaceSource(const String& uri, FontResource*);
    ~SVGRemoteFontFaceSource();
    virtual bool isSVGFontFaceSource() const OVERRIDE { return true; }
    virtual bool ensureFontData() OVERRIDE;

private:
    virtual PassRefPtr<SimpleFontData> createFontData(const FontDescription&) OVERRIDE;

    String m_uri;
    RefPtr<SVGFontElement> m_externalSVGFontElement;
};

} // namespace WebCore

#endif // ENABLE(SVG_FONTS)
#endif
