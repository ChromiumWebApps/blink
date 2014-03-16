/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#include "config.h"
#include "core/animation/EffectInput.h"

#include "bindings/v8/Dictionary.h"
#include "core/animation/AnimationHelpers.h"
#include "core/animation/css/CSSAnimations.h"
#include "core/css/parser/BisonCSSParser.h"
#include "core/css/resolver/StyleResolver.h"
#include "core/dom/Element.h"

namespace WebCore {

static bool checkDocumentAndRenderer(Element* element)
{
    if (!element->inActiveDocument())
        return false;
    element->document().updateStyleIfNeeded();
    return element->renderer();
}

PassRefPtrWillBeRawPtr<AnimationEffect> EffectInput::convert(Element* element, const Vector<Dictionary>& keyframeDictionaryVector, bool unsafe)
{
    // FIXME: This test will not be neccessary once resolution of keyframe values occurs at
    // animation application time.
    if (!unsafe && !checkDocumentAndRenderer(element))
        return nullptr;

    // FIXME: Move this code into KeyframeEffectModel, it will be used by the IDL constructor for that class.
    KeyframeEffectModel::KeyframeVector keyframes;
    Vector<RefPtr<MutableStylePropertySet> > propertySetVector;

    for (size_t i = 0; i < keyframeDictionaryVector.size(); ++i) {
        RefPtr<MutableStylePropertySet> propertySet = MutableStylePropertySet::create();
        propertySetVector.append(propertySet);

        RefPtrWillBeRawPtr<Keyframe> keyframe = Keyframe::create();
        keyframes.append(keyframe);

        double offset;
        if (keyframeDictionaryVector[i].get("offset", offset))
            keyframe->setOffset(offset);

        String compositeString;
        keyframeDictionaryVector[i].get("composite", compositeString);
        if (compositeString == "add")
            keyframe->setComposite(AnimationEffect::CompositeAdd);

        String timingFunctionString;
        if (keyframeDictionaryVector[i].get("easing", timingFunctionString)) {
            RefPtrWillBeRawPtr<CSSValue> timingFunctionValue = BisonCSSParser::parseAnimationTimingFunctionValue(timingFunctionString);
            if (timingFunctionValue)
                keyframe->setEasing(CSSToStyleMap::animationTimingFunction(timingFunctionValue.get(), false));
        }

        Vector<String> keyframeProperties;
        keyframeDictionaryVector[i].getOwnPropertyNames(keyframeProperties);

        for (size_t j = 0; j < keyframeProperties.size(); ++j) {
            String property = keyframeProperties[j];
            CSSPropertyID id = camelCaseCSSPropertyNameToID(property);

            // FIXME: There is no way to store invalid properties or invalid values
            // in a Keyframe object, so for now I just skip over them. Eventually we
            // will need to support getFrames(), which should return exactly the
            // keyframes that were input through the API. We will add a layer to wrap
            // KeyframeEffectModel, store input keyframes and implement getFrames.
            if (id == CSSPropertyInvalid || !CSSAnimations::isAnimatableProperty(id))
                continue;

            String value;
            keyframeDictionaryVector[i].get(property, value);
            propertySet->setProperty(id, value);
        }
    }

    // FIXME: Replace this with code that just parses, when that code is available.
    return StyleResolver::createKeyframeEffectModel(*element, propertySetVector, keyframes);
}

} // namespace WebCore
