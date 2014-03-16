/*
 * Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005 Rob Buis <buis@kde.org>
 * Copyright (C) 2005 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2006 Apple Computer, Inc
 * Copyright (C) 2009 Google, Inc.
 * Copyright (C) 2011 Renata Hodovan <reni@webkit.org>
 * Copyright (C) 2011 University of Szeged
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef RenderSVGShape_h
#define RenderSVGShape_h

#include "core/rendering/svg/RenderSVGModelObject.h"
#include "core/rendering/svg/SVGMarkerData.h"
#include "platform/geometry/FloatRect.h"
#include "platform/transforms/AffineTransform.h"
#include "wtf/OwnPtr.h"
#include "wtf/Vector.h"

namespace WebCore {

class FloatPoint;
class GraphicsContextStateSaver;
class PointerEventsHitRules;
class RenderSVGContainer;
class RenderSVGPath;
class RenderSVGResource;
class SVGGraphicsElement;

class RenderSVGShape : public RenderSVGModelObject {
public:
    explicit RenderSVGShape(SVGGraphicsElement*);
    RenderSVGShape(SVGGraphicsElement*, Path*, bool);
    virtual ~RenderSVGShape();

    void setNeedsShapeUpdate() { m_needsShapeUpdate = true; }
    virtual void setNeedsBoundariesUpdate() OVERRIDE FINAL { m_needsBoundariesUpdate = true; }
    virtual void setNeedsTransformUpdate() OVERRIDE FINAL { m_needsTransformUpdate = true; }
    virtual void fillShape(GraphicsContext*) const;
    virtual void strokeShape(GraphicsContext*) const;

    bool nodeAtFloatPointInternal(const HitTestRequest&, const FloatPoint&, PointerEventsHitRules);

    Path& path() const
    {
        ASSERT(m_path);
        return *m_path;
    }

protected:
    virtual void updateShapeFromElement();
    virtual bool isShapeEmpty() const { return path().isEmpty(); }
    virtual bool shapeDependentStrokeContains(const FloatPoint&);
    virtual bool shapeDependentFillContains(const FloatPoint&, const WindRule) const;
    float strokeWidth() const;
    bool hasPath() const { return m_path.get(); }
    bool hasSmoothStroke() const;

    bool hasNonScalingStroke() const { return style()->svgStyle()->vectorEffect() == VE_NON_SCALING_STROKE; }
    AffineTransform nonScalingStrokeTransform() const;
    Path* nonScalingStrokePath(const Path*, const AffineTransform&) const;

    FloatRect m_fillBoundingBox;
    FloatRect m_strokeBoundingBox;

private:
    // Hit-detection separated for the fill and the stroke
    bool fillContains(const FloatPoint&, bool requiresFill = true, const WindRule fillRule = RULE_NONZERO);
    bool strokeContains(const FloatPoint&, bool requiresStroke = true);

    virtual FloatRect repaintRectInLocalCoordinates() const OVERRIDE FINAL { return m_repaintBoundingBox; }
    virtual const AffineTransform& localToParentTransform() const OVERRIDE FINAL { return m_localTransform; }
    virtual AffineTransform localTransform() const OVERRIDE FINAL { return m_localTransform; }

    virtual bool isSVGShape() const OVERRIDE FINAL { return true; }
    virtual const char* renderName() const OVERRIDE { return "RenderSVGShape"; }

    virtual void layout() OVERRIDE FINAL;
    virtual void paint(PaintInfo&, const LayoutPoint&) OVERRIDE FINAL;
    virtual void addFocusRingRects(Vector<IntRect>&, const LayoutPoint& additionalOffset, const RenderLayerModelObject* paintContainer = 0) OVERRIDE FINAL;

    virtual bool nodeAtFloatPoint(const HitTestRequest&, HitTestResult&, const FloatPoint& pointInParent, HitTestAction) OVERRIDE FINAL;

    virtual FloatRect objectBoundingBox() const OVERRIDE FINAL { return m_fillBoundingBox; }
    virtual FloatRect strokeBoundingBox() const OVERRIDE FINAL { return m_strokeBoundingBox; }
    FloatRect calculateObjectBoundingBox() const;
    FloatRect calculateStrokeBoundingBox() const;
    void updateRepaintBoundingBox();

    bool setupNonScalingStrokeContext(AffineTransform&, GraphicsContextStateSaver&);

    bool shouldGenerateMarkerPositions() const;
    FloatRect markerRect(float strokeWidth) const;
    void processMarkerPositions();

    void fillShape(RenderStyle*, GraphicsContext*);
    void strokeShape(RenderStyle*, GraphicsContext*);
    void drawMarkers(PaintInfo&);

private:
    FloatRect m_repaintBoundingBox;
    AffineTransform m_localTransform;
    OwnPtr<Path> m_path;
    Vector<MarkerPosition> m_markerPositions;

    bool m_needsBoundariesUpdate : 1;
    bool m_needsShapeUpdate : 1;
    bool m_needsTransformUpdate : 1;
};

DEFINE_RENDER_OBJECT_TYPE_CASTS(RenderSVGShape, isSVGShape());

}

#endif
