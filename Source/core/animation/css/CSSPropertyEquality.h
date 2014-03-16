// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSPropertyEquality_h
#define CSSPropertyEquality_h

#include "CSSPropertyNames.h"

namespace WebCore {

class RenderStyle;

class CSSPropertyEquality {
public:
    static bool propertiesEqual(CSSPropertyID, const RenderStyle&, const RenderStyle&);
};

} // namespace WebCore

#endif // CSSPropertyEquality_h
