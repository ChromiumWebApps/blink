/*
 * Copyright (C) 2006, 2007 Apple Computer, Inc.
 * Copyright (c) 2006, 2007, 2008, 2009, Google Inc. All rights reserved.
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

#ifndef FontPlatformDataChromiumWin_h
#define FontPlatformDataChromiumWin_h

#include "SkPaint.h"
#include "SkTypeface.h"
#include "SkTypeface_win.h"
#include "platform/SharedBuffer.h"
#include "platform/fonts/FontOrientation.h"
#include "platform/fonts/opentype/OpenTypeVerticalData.h"
#include "wtf/Forward.h"
#include "wtf/HashTableDeletedValueType.h"
#include "wtf/OwnPtr.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefCounted.h"
#include "wtf/RefPtr.h"
#include "wtf/text/StringImpl.h"

#include <usp10.h>

typedef struct HFONT__ *HFONT;

namespace WebCore {

class FontDescription;
class GraphicsContext;
class HarfBuzzFace;

class PLATFORM_EXPORT FontPlatformData {
public:
    // Used for deleted values in the font cache's hash tables. The hash table
    // will create us with this structure, and it will compare other values
    // to this "Deleted" one. It expects the Deleted one to be differentiable
    // from the NULL one (created with the empty constructor), so we can't just
    // set everything to NULL.
    FontPlatformData(WTF::HashTableDeletedValueType);
    FontPlatformData();
    FontPlatformData(float size, bool bold, bool oblique);
    FontPlatformData(const FontPlatformData&);
    FontPlatformData(const FontPlatformData&, float textSize);
    FontPlatformData(PassRefPtr<SkTypeface>, const char* name, float textSize, bool syntheticBold, bool syntheticItalic, FontOrientation = Horizontal, bool useSubpixelPositioning = defaultUseSubpixelPositioning());

    void setupPaint(SkPaint*, GraphicsContext* = 0) const;

    FontPlatformData& operator=(const FontPlatformData&);

    bool isHashTableDeletedValue() const { return m_isHashTableDeletedValue; }

    ~FontPlatformData();

    bool isFixedPitch() const;
    float size() const { return m_textSize; }
    HarfBuzzFace* harfBuzzFace() const;
    SkTypeface* typeface() const { return m_typeface.get(); }
    SkFontID uniqueID() const { return m_typeface->uniqueID(); }
    int paintTextFlags() const { return m_paintTextFlags; }

    String fontFamilyName() const;

    FontOrientation orientation() const { return m_orientation; }
    void setOrientation(FontOrientation orientation) { m_orientation = orientation; }

    unsigned hash() const;

    bool operator==(const FontPlatformData&) const;

#if ENABLE(OPENTYPE_VERTICAL)
    PassRefPtr<OpenTypeVerticalData> verticalData() const;
    PassRefPtr<SharedBuffer> openTypeTable(uint32_t table) const;
#endif

#ifndef NDEBUG
    String description() const;
#endif

private:
    bool static defaultUseSubpixelPositioning();

    float m_textSize; // Point size of the font in pixels.
    FontOrientation m_orientation;
    bool m_syntheticBold;
    bool m_syntheticItalic;

    RefPtr<SkTypeface> m_typeface;
    int m_paintTextFlags;

    mutable RefPtr<HarfBuzzFace> m_harfBuzzFace;

    bool m_isHashTableDeletedValue;
    bool m_useSubpixelPositioning;
};

} // WebCore

#endif // FontPlatformDataChromiumWin_h
