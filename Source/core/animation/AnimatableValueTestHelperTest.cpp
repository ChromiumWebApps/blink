/*
 * Copyright (c) 2013, Google Inc. All rights reserved.
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

#include "core/animation/AnimatableValueTestHelper.h"

#include "core/rendering/ClipPathOperation.h"
#include "core/rendering/style/BasicShapes.h"
#include "core/svg/SVGLengthContext.h"
#include "platform/transforms/ScaleTransformOperation.h"
#include "platform/transforms/TranslateTransformOperation.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <sstream>
#include <string>


using namespace WebCore;

namespace {

class AnimationAnimatableValueTestHelperTest : public ::testing::Test {
protected:
    ::std::string PrintToString(PassRefPtr<AnimatableValue> animValue)
    {
        return PrintToString(animValue.get());
    }

    ::std::string PrintToString(const AnimatableValue* animValue)
    {
        return ::testing::PrintToString(*animValue);
    }
};

TEST_F(AnimationAnimatableValueTestHelperTest, PrintTo)
{
    EXPECT_THAT(
        PrintToString(AnimatableClipPathOperation::create(ShapeClipPathOperation::create(BasicShapeCircle::create().get()).get())),
        testing::StartsWith("AnimatableClipPathOperation")
        );

    EXPECT_EQ(
        ::std::string("AnimatableColor(rgba(0, 0, 0, 0), #ff0000)"),
        PrintToString(AnimatableColor::create(Color(0x000000FF), Color(0xFFFF0000))));

    EXPECT_EQ(
        ::std::string("AnimatableDouble(1)"),
        PrintToString(AnimatableDouble::create(1.0)));

    EXPECT_EQ(
        ::std::string("AnimatableLength(5px)"),
        PrintToString(AnimatableLength::create(CSSPrimitiveValue::create(5, CSSPrimitiveValue::CSS_PX).get())));

    EXPECT_EQ(
        ::std::string("AnimatableLengthBox(AnimatableLength(1px), AnimatableLength(2em), AnimatableLength(3rem), AnimatableLength(4pt))"),
        PrintToString(AnimatableLengthBox::create(
            AnimatableLength::create(CSSPrimitiveValue::create(1, CSSPrimitiveValue::CSS_PX).get()),
            AnimatableLength::create(CSSPrimitiveValue::create(2, CSSPrimitiveValue::CSS_EMS).get()),
            AnimatableLength::create(CSSPrimitiveValue::create(3, CSSPrimitiveValue::CSS_REMS).get()),
            AnimatableLength::create(CSSPrimitiveValue::create(4, CSSPrimitiveValue::CSS_PT).get())
            )));

    EXPECT_EQ(
        ::std::string("AnimatableLengthPoint(AnimatableLength(5%), AnimatableLength(6px))"),
        PrintToString(AnimatableLengthPoint::create(
            AnimatableLength::create(CSSPrimitiveValue::create(5, CSSPrimitiveValue::CSS_PERCENTAGE).get()),
            AnimatableLength::create(CSSPrimitiveValue::create(6, CSSPrimitiveValue::CSS_PX).get())
            )));

    EXPECT_EQ(
        ::std::string("AnimatableLengthSize(AnimatableLength(3rem), AnimatableLength(4pt))"),
        PrintToString(AnimatableLengthSize::create(
            AnimatableLength::create(CSSPrimitiveValue::create(3, CSSPrimitiveValue::CSS_REMS).get()),
            AnimatableLength::create(CSSPrimitiveValue::create(4, CSSPrimitiveValue::CSS_PT).get())
            )));

    EXPECT_THAT(
        PrintToString(AnimatableValue::neutralValue()),
        testing::StartsWith("AnimatableNeutral@"));

    Vector<RefPtr<AnimatableValue> > v1;
    v1.append(AnimatableLength::create(CSSPrimitiveValue::create(3, CSSPrimitiveValue::CSS_REMS).get()));
    v1.append(AnimatableLength::create(CSSPrimitiveValue::create(4, CSSPrimitiveValue::CSS_PT).get()));
    EXPECT_EQ(
        ::std::string("AnimatableRepeatable(AnimatableLength(3rem), AnimatableLength(4pt))"),
        PrintToString(AnimatableRepeatable::create(v1)));

    RefPtr<SVGLength> length1cm = SVGLength::create(LengthModeOther);
    RefPtr<SVGLength> length2cm = SVGLength::create(LengthModeOther);
    length1cm->setValueAsString("1cm", ASSERT_NO_EXCEPTION);
    length2cm->setValueAsString("2cm", ASSERT_NO_EXCEPTION);

    EXPECT_EQ(
        ::std::string("AnimatableSVGLength(1cm)"),
        PrintToString(AnimatableSVGLength::create(length1cm)));

    EXPECT_EQ(
        ::std::string("AnimatableSVGPaint(#ff0000)"),
        PrintToString(AnimatableSVGPaint::create(SVGPaint::SVG_PAINTTYPE_RGBCOLOR, Color(0xFFFF0000), "")));

    EXPECT_EQ(
        ::std::string("AnimatableSVGPaint(url(abc))"),
        PrintToString(AnimatableSVGPaint::create(SVGPaint::SVG_PAINTTYPE_URI, Color(0xFFFF0000), "abc")));

    EXPECT_THAT(
        PrintToString(AnimatableShapeValue::create(ShapeValue::createShapeValue(BasicShapeCircle::create().get(), ContentBox).get())),
        testing::StartsWith("AnimatableShapeValue@"));

    RefPtr<SVGLengthList> l2 = SVGLengthList::create();
    l2->append(length1cm);
    l2->append(length2cm);
    EXPECT_EQ(
        ::std::string("AnimatableStrokeDasharrayList(1cm, 2cm)"),
        PrintToString(AnimatableStrokeDasharrayList::create(l2)));

    TransformOperations operations1;
    operations1.operations().append(TranslateTransformOperation::create(Length(2, WebCore::Fixed), Length(0, WebCore::Fixed), TransformOperation::TranslateX));
    EXPECT_EQ(
        ::std::string("AnimatableTransform([1 0 0 1 2 0])"),
        PrintToString(AnimatableTransform::create(operations1)));

    TransformOperations operations2;
    operations2.operations().append(ScaleTransformOperation::create(1, 1, 1, TransformOperation::Scale3D));
    EXPECT_EQ(
        ::std::string("AnimatableTransform([1 0 0 1 0 0])"),
        PrintToString(AnimatableTransform::create(operations2)));

    EXPECT_EQ(
        ::std::string("AnimatableUnknown(none)"),
        PrintToString(AnimatableUnknown::create(CSSPrimitiveValue::createIdentifier(CSSValueNone).get())));

    EXPECT_EQ(
        ::std::string("AnimatableVisibility(VISIBLE)"),
        PrintToString(AnimatableVisibility::create(VISIBLE)));
}

} // namespace
