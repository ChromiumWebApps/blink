// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EffectInput_h
#define EffectInput_h

#include "core/animation/AnimationEffect.h"
#include "wtf/Vector.h"

namespace WebCore {

class AnimationEffect;
class Dictionary;
class Element;

class EffectInput {
public:
    static PassRefPtrWillBeRawPtr<AnimationEffect> convert(Element*, const Vector<Dictionary>& keyframeDictionaryVector, bool unsafe = false);
};

} // namespace WebCore

#endif
