/*
 * Copyright (C) 2012 Adobe Systems Incorporated. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef BasicShapes_h
#define BasicShapes_h

#include "core/rendering/style/RenderStyleConstants.h"
#include "platform/Length.h"
#include "platform/LengthSize.h"
#include "platform/graphics/WindRule.h"
#include "wtf/RefCounted.h"
#include "wtf/RefPtr.h"
#include "wtf/Vector.h"

namespace WebCore {

class FloatRect;
class FloatSize;
class Path;

class BasicShape : public RefCounted<BasicShape> {
public:
    virtual ~BasicShape() { }

    enum Type {
        BasicShapeRectangleType,
        DeprecatedBasicShapeCircleType,
        DeprecatedBasicShapeEllipseType,
        BasicShapeEllipseType,
        BasicShapePolygonType,
        BasicShapeInsetRectangleType,
        BasicShapeCircleType,
        BasicShapeInsetType
    };

    bool canBlend(const BasicShape*) const;
    bool isSameType(const BasicShape& other) const { return type() == other.type(); }

    virtual void path(Path&, const FloatRect&) = 0;
    virtual WindRule windRule() const { return RULE_NONZERO; }
    virtual PassRefPtr<BasicShape> blend(const BasicShape*, double) const = 0;
    virtual bool operator==(const BasicShape&) const = 0;

    virtual Type type() const = 0;

protected:
    BasicShape()
    {
    }

};

#define DEFINE_BASICSHAPE_TYPE_CASTS(thisType) \
    DEFINE_TYPE_CASTS(thisType, BasicShape, value, value->type() == BasicShape::thisType##Type, value.type() == BasicShape::thisType##Type)

class BasicShapeRectangle FINAL : public BasicShape {
public:
    static PassRefPtr<BasicShapeRectangle> create() { return adoptRef(new BasicShapeRectangle); }

    Length x() const { return m_x; }
    Length y() const { return m_y; }
    Length width() const { return m_width; }
    Length height() const { return m_height; }
    Length cornerRadiusX() const { return m_cornerRadiusX; }
    Length cornerRadiusY() const { return m_cornerRadiusY; }

    void setX(Length x) { m_x = x; }
    void setY(Length y) { m_y = y; }
    void setWidth(Length width) { m_width = width; }
    void setHeight(Length height) { m_height = height; }
    void setCornerRadiusX(Length radiusX)
    {
        m_cornerRadiusX = radiusX;
    }
    void setCornerRadiusY(Length radiusY)
    {
        m_cornerRadiusY = radiusY;
    }

    virtual void path(Path&, const FloatRect&) OVERRIDE;
    virtual PassRefPtr<BasicShape> blend(const BasicShape*, double) const OVERRIDE;
    virtual bool operator==(const BasicShape&) const OVERRIDE;

    virtual Type type() const OVERRIDE { return BasicShapeRectangleType; }
private:
    BasicShapeRectangle() { }

    Length m_y;
    Length m_x;
    Length m_width;
    Length m_height;
    Length m_cornerRadiusX;
    Length m_cornerRadiusY;
};

DEFINE_BASICSHAPE_TYPE_CASTS(BasicShapeRectangle);

class BasicShapeCenterCoordinate {
public:
    enum Direction {
        TopLeft,
        BottomRight
    };
    BasicShapeCenterCoordinate()
        : m_direction(TopLeft)
        , m_length(Undefined)
    {
        updateComputedLength();
    }

    BasicShapeCenterCoordinate(Direction direction, Length length)
        : m_direction(direction)
        , m_length(length)
    {
        updateComputedLength();
    }

    BasicShapeCenterCoordinate(const BasicShapeCenterCoordinate& other)
        : m_direction(other.direction())
        , m_length(other.length())
        , m_computedLength(other.m_computedLength)
    {
    }

    bool operator==(const BasicShapeCenterCoordinate& other) const { return m_direction == other.m_direction && m_length == other.m_length && m_computedLength == other.m_computedLength; }

    Direction direction() const { return m_direction; }
    const Length& length() const { return m_length; }
    const Length& computedLength() const { return m_computedLength; }

    BasicShapeCenterCoordinate blend(const BasicShapeCenterCoordinate& other, double progress) const
    {
        return BasicShapeCenterCoordinate(TopLeft, m_computedLength.blend(other.m_computedLength, progress, ValueRangeAll));
    }

private:
    Direction m_direction;
    Length m_length;
    Length m_computedLength;

    void updateComputedLength();
};

class BasicShapeRadius {
public:
    enum Type {
        Value,
        ClosestSide,
        FarthestSide
    };
    BasicShapeRadius() : m_value(Undefined), m_type(ClosestSide) { }
    explicit BasicShapeRadius(Length v) : m_value(v), m_type(Value) { }
    explicit BasicShapeRadius(Type t) : m_value(Undefined), m_type(t) { }
    BasicShapeRadius(const BasicShapeRadius& other) : m_value(other.value()), m_type(other.type()) { }
    bool operator==(const BasicShapeRadius& other) const { return m_type == other.m_type && m_value == other.m_value; }

    const Length& value() const { return m_value; }
    Type type() const { return m_type; }

    bool canBlend(const BasicShapeRadius& other) const
    {
        // FIXME determine how to interpolate between keywords. See issue 330248.
        return m_type == Value && other.type() == Value;
    }

    BasicShapeRadius blend(const BasicShapeRadius& other, double progress) const
    {
        if (m_type != Value || other.type() != Value)
            return BasicShapeRadius(other);

        return BasicShapeRadius(m_value.blend(other.value(), progress, ValueRangeAll));
    }

private:
    Length m_value;
    Type m_type;

};

class BasicShapeCircle FINAL : public BasicShape {
public:
    static PassRefPtr<BasicShapeCircle> create() { return adoptRef(new BasicShapeCircle); }

    const BasicShapeCenterCoordinate& centerX() const { return m_centerX; }
    const BasicShapeCenterCoordinate& centerY() const { return m_centerY; }
    const BasicShapeRadius& radius() const { return m_radius; }

    float floatValueForRadiusInBox(FloatSize) const;
    void setCenterX(BasicShapeCenterCoordinate centerX) { m_centerX = centerX; }
    void setCenterY(BasicShapeCenterCoordinate centerY) { m_centerY = centerY; }
    void setRadius(BasicShapeRadius radius) { m_radius = radius; }

    virtual void path(Path&, const FloatRect&) OVERRIDE;
    virtual PassRefPtr<BasicShape> blend(const BasicShape*, double) const OVERRIDE;
    virtual bool operator==(const BasicShape&) const OVERRIDE;

    virtual Type type() const OVERRIDE { return BasicShapeCircleType; }
private:
    BasicShapeCircle() { }

    BasicShapeCenterCoordinate m_centerX;
    BasicShapeCenterCoordinate m_centerY;
    BasicShapeRadius m_radius;
};

DEFINE_BASICSHAPE_TYPE_CASTS(BasicShapeCircle);

class DeprecatedBasicShapeCircle FINAL : public BasicShape {
public:
    static PassRefPtr<DeprecatedBasicShapeCircle> create() { return adoptRef(new DeprecatedBasicShapeCircle); }

    Length centerX() const { return m_centerX; }
    Length centerY() const { return m_centerY; }
    Length radius() const { return m_radius; }

    void setCenterX(Length centerX) { m_centerX = centerX; }
    void setCenterY(Length centerY) { m_centerY = centerY; }
    void setRadius(Length radius) { m_radius = radius; }

    virtual void path(Path&, const FloatRect&) OVERRIDE;
    virtual PassRefPtr<BasicShape> blend(const BasicShape*, double) const OVERRIDE;
    virtual bool operator==(const BasicShape&) const OVERRIDE;

    virtual Type type() const OVERRIDE { return DeprecatedBasicShapeCircleType; }
private:
    DeprecatedBasicShapeCircle() { }

    Length m_centerX;
    Length m_centerY;
    Length m_radius;
};

DEFINE_BASICSHAPE_TYPE_CASTS(DeprecatedBasicShapeCircle);

class BasicShapeEllipse FINAL : public BasicShape {
public:
    static PassRefPtr<BasicShapeEllipse> create() { return adoptRef(new BasicShapeEllipse); }

    const BasicShapeCenterCoordinate& centerX() const { return m_centerX; }
    const BasicShapeCenterCoordinate& centerY() const { return m_centerY; }
    const BasicShapeRadius& radiusX() const { return m_radiusX; }
    const BasicShapeRadius& radiusY() const { return m_radiusY; }
    float floatValueForRadiusInBox(const BasicShapeRadius&, float center, float boxWidthOrHeight) const;

    void setCenterX(BasicShapeCenterCoordinate centerX) { m_centerX = centerX; }
    void setCenterY(BasicShapeCenterCoordinate centerY) { m_centerY = centerY; }
    void setRadiusX(BasicShapeRadius radiusX) { m_radiusX = radiusX; }
    void setRadiusY(BasicShapeRadius radiusY) { m_radiusY = radiusY; }

    virtual void path(Path&, const FloatRect&) OVERRIDE;
    virtual PassRefPtr<BasicShape> blend(const BasicShape*, double) const OVERRIDE;
    virtual bool operator==(const BasicShape&) const OVERRIDE;

    virtual Type type() const OVERRIDE { return BasicShapeEllipseType; }
private:
    BasicShapeEllipse() { }

    BasicShapeCenterCoordinate m_centerX;
    BasicShapeCenterCoordinate m_centerY;
    BasicShapeRadius m_radiusX;
    BasicShapeRadius m_radiusY;
};

DEFINE_BASICSHAPE_TYPE_CASTS(BasicShapeEllipse);

class DeprecatedBasicShapeEllipse FINAL : public BasicShape {
public:
    static PassRefPtr<DeprecatedBasicShapeEllipse> create() { return adoptRef(new DeprecatedBasicShapeEllipse); }

    Length centerX() const { return m_centerX; }
    Length centerY() const { return m_centerY; }
    Length radiusX() const { return m_radiusX; }
    Length radiusY() const { return m_radiusY; }

    void setCenterX(Length centerX) { m_centerX = centerX; }
    void setCenterY(Length centerY) { m_centerY = centerY; }
    void setRadiusX(Length radiusX) { m_radiusX = radiusX; }
    void setRadiusY(Length radiusY) { m_radiusY = radiusY; }

    virtual void path(Path&, const FloatRect&) OVERRIDE;
    virtual PassRefPtr<BasicShape> blend(const BasicShape*, double) const OVERRIDE;
    virtual bool operator==(const BasicShape&) const OVERRIDE;

    virtual Type type() const OVERRIDE { return DeprecatedBasicShapeEllipseType; }
private:
    DeprecatedBasicShapeEllipse() { }

    Length m_centerX;
    Length m_centerY;
    Length m_radiusX;
    Length m_radiusY;
};

DEFINE_BASICSHAPE_TYPE_CASTS(DeprecatedBasicShapeEllipse);

class BasicShapePolygon FINAL : public BasicShape {
public:
    static PassRefPtr<BasicShapePolygon> create() { return adoptRef(new BasicShapePolygon); }

    const Vector<Length>& values() const { return m_values; }
    Length getXAt(unsigned i) const { return m_values.at(2 * i); }
    Length getYAt(unsigned i) const { return m_values.at(2 * i + 1); }

    void setWindRule(WindRule windRule) { m_windRule = windRule; }
    void appendPoint(Length x, Length y) { m_values.append(x); m_values.append(y); }

    virtual void path(Path&, const FloatRect&) OVERRIDE;
    virtual PassRefPtr<BasicShape> blend(const BasicShape*, double) const OVERRIDE;
    virtual bool operator==(const BasicShape&) const OVERRIDE;

    virtual WindRule windRule() const OVERRIDE { return m_windRule; }

    virtual Type type() const OVERRIDE { return BasicShapePolygonType; }
private:
    BasicShapePolygon()
        : m_windRule(RULE_NONZERO)
    { }

    WindRule m_windRule;
    Vector<Length> m_values;
};

DEFINE_BASICSHAPE_TYPE_CASTS(BasicShapePolygon);

class BasicShapeInsetRectangle FINAL : public BasicShape {
public:
    static PassRefPtr<BasicShapeInsetRectangle> create() { return adoptRef(new BasicShapeInsetRectangle); }

    Length top() const { return m_top; }
    Length right() const { return m_right; }
    Length bottom() const { return m_bottom; }
    Length left() const { return m_left; }
    Length cornerRadiusX() const { return m_cornerRadiusX; }
    Length cornerRadiusY() const { return m_cornerRadiusY; }

    void setTop(Length top) { m_top = top; }
    void setRight(Length right) { m_right = right; }
    void setBottom(Length bottom) { m_bottom = bottom; }
    void setLeft(Length left) { m_left = left; }
    void setCornerRadiusX(Length radiusX)
    {
        m_cornerRadiusX = radiusX;
    }
    void setCornerRadiusY(Length radiusY)
    {
        m_cornerRadiusY = radiusY;
    }

    virtual void path(Path&, const FloatRect&) OVERRIDE;
    virtual PassRefPtr<BasicShape> blend(const BasicShape*, double) const OVERRIDE;
    virtual bool operator==(const BasicShape&) const OVERRIDE;

    virtual Type type() const OVERRIDE { return BasicShapeInsetRectangleType; }
private:
    BasicShapeInsetRectangle() { }

    Length m_right;
    Length m_top;
    Length m_bottom;
    Length m_left;
    Length m_cornerRadiusX;
    Length m_cornerRadiusY;
};

DEFINE_BASICSHAPE_TYPE_CASTS(BasicShapeInsetRectangle);

class BasicShapeInset : public BasicShape {
public:
    static PassRefPtr<BasicShapeInset> create() { return adoptRef(new BasicShapeInset); }

    const Length& top() const { return m_top; }
    const Length& right() const { return m_right; }
    const Length& bottom() const { return m_bottom; }
    const Length& left() const { return m_left; }

    const LengthSize& topLeftRadius() const { return m_topLeftRadius; }
    const LengthSize& topRightRadius() const { return m_topRightRadius; }
    const LengthSize& bottomRightRadius() const { return m_bottomRightRadius; }
    const LengthSize& bottomLeftRadius() const { return m_bottomLeftRadius; }

    void setTop(Length top) { m_top = top; }
    void setRight(Length right) { m_right = right; }
    void setBottom(Length bottom) { m_bottom = bottom; }
    void setLeft(Length left) { m_left = left; }

    void setTopLeftRadius(LengthSize radius) { m_topLeftRadius = radius; }
    void setTopRightRadius(LengthSize radius) { m_topRightRadius = radius; }
    void setBottomRightRadius(LengthSize radius) { m_bottomRightRadius = radius; }
    void setBottomLeftRadius(LengthSize radius) { m_bottomLeftRadius = radius; }

    virtual void path(Path&, const FloatRect&) OVERRIDE;
    virtual PassRefPtr<BasicShape> blend(const BasicShape*, double) const OVERRIDE;
    virtual bool operator==(const BasicShape&) const OVERRIDE;

    virtual Type type() const OVERRIDE { return BasicShapeInsetType; }
private:
    BasicShapeInset() { }

    Length m_right;
    Length m_top;
    Length m_bottom;
    Length m_left;

    LengthSize m_topLeftRadius;
    LengthSize m_topRightRadius;
    LengthSize m_bottomRightRadius;
    LengthSize m_bottomLeftRadius;
};

DEFINE_BASICSHAPE_TYPE_CASTS(BasicShapeInset);

}
#endif
