/*
 * Copyright (C) 2003, 2004, 2005, 2006, 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "platform/graphics/GraphicsContext.h"

#include "platform/geometry/IntRect.h"
#include "platform/geometry/RoundedRect.h"
#include "platform/graphics/BitmapImage.h"
#include "platform/graphics/DisplayList.h"
#include "platform/graphics/Gradient.h"
#include "platform/graphics/ImageBuffer.h"
#include "platform/text/BidiResolver.h"
#include "platform/text/TextRunIterator.h"
#include "platform/weborigin/KURL.h"
#include "third_party/skia/include/core/SkAnnotation.h"
#include "third_party/skia/include/core/SkColorFilter.h"
#include "third_party/skia/include/core/SkData.h"
#include "third_party/skia/include/core/SkDevice.h"
#include "third_party/skia/include/core/SkPicture.h"
#include "third_party/skia/include/core/SkRRect.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/effects/SkBlurMaskFilter.h"
#include "third_party/skia/include/effects/SkCornerPathEffect.h"
#include "third_party/skia/include/effects/SkLumaColorFilter.h"
#include "third_party/skia/include/gpu/GrRenderTarget.h"
#include "third_party/skia/include/gpu/GrTexture.h"
#include "wtf/Assertions.h"
#include "wtf/MathExtras.h"

#if OS(MACOSX)
#include <ApplicationServices/ApplicationServices.h>
#endif

using namespace std;
using blink::WebBlendMode;

namespace WebCore {

namespace {

class CompatibleImageBufferSurface : public ImageBufferSurface {
    WTF_MAKE_NONCOPYABLE(CompatibleImageBufferSurface); WTF_MAKE_FAST_ALLOCATED;
public:
    CompatibleImageBufferSurface(PassRefPtr<SkSurface> surface, const IntSize& size, OpacityMode opacityMode)
        : ImageBufferSurface(size, opacityMode)
        , m_surface(surface)
    {
    }
    virtual ~CompatibleImageBufferSurface() { }

    virtual SkCanvas* canvas() const OVERRIDE { return m_surface ? m_surface->getCanvas() : 0; }
    virtual bool isValid() const OVERRIDE { return m_surface; }
    virtual bool isAccelerated() const OVERRIDE { return isValid() && m_surface->getCanvas()->getTopDevice()->accessRenderTarget(); }
    virtual Platform3DObject getBackingTexture() const OVERRIDE
    {
        ASSERT(isAccelerated());
        GrRenderTarget* renderTarget = m_surface->getCanvas()->getTopDevice()->accessRenderTarget();
        if (renderTarget) {
            return renderTarget->asTexture()->getTextureHandle();
        }
        return 0;
    };

private:
    RefPtr<SkSurface> m_surface;
};

} // unnamed namespace

struct GraphicsContext::CanvasSaveState {
    CanvasSaveState(unsigned mask, int count) : m_flags(mask), m_restoreCount(count) { }

    unsigned m_flags;
    int m_restoreCount;
};

struct GraphicsContext::RecordingState {
    RecordingState(SkCanvas* currentCanvas, const SkMatrix& currentMatrix, PassRefPtr<DisplayList> displayList)
        : m_savedCanvas(currentCanvas)
        , m_displayList(displayList)
        , m_savedMatrix(currentMatrix)
    {
    }

    SkCanvas* m_savedCanvas;
    RefPtr<DisplayList> m_displayList;
    const SkMatrix m_savedMatrix;
};

GraphicsContext::GraphicsContext(SkCanvas* canvas)
    : m_canvas(canvas)
    , m_paintStateStack()
    , m_paintStateIndex(0)
    , m_canvasSaveFlags(0)
    , m_annotationMode(0)
#if !ASSERT_DISABLED
    , m_annotationCount(0)
    , m_layerCount(0)
#endif
    , m_trackOpaqueRegion(false)
    , m_trackTextRegion(false)
    , m_useHighResMarker(false)
    , m_updatingControlTints(false)
    , m_accelerated(false)
    , m_isCertainlyOpaque(true)
    , m_printing(false)
{
    // FIXME: Do some tests to determine how many states are typically used, and allocate
    // several here.
    m_paintStateStack.append(GraphicsContextState::create());
    m_paintState = m_paintStateStack.last().get();
}

GraphicsContext::~GraphicsContext()
{
    ASSERT(!m_paintStateIndex);
    ASSERT(!m_paintState->saveCount());
    ASSERT(!m_annotationCount);
    ASSERT(!m_layerCount);
    ASSERT(m_recordingStateStack.isEmpty());
}

void GraphicsContext::save()
{
    if (paintingDisabled())
        return;

    m_paintState->incrementSaveCount();

    m_canvasStateStack.append(CanvasSaveState(m_canvasSaveFlags, m_canvas->getSaveCount()));
    m_canvasSaveFlags |= SkCanvas::kMatrixClip_SaveFlag;
}

void GraphicsContext::restore()
{
    if (paintingDisabled())
        return;

    if (!m_paintStateIndex && !m_paintState->saveCount()) {
        WTF_LOG_ERROR("ERROR void GraphicsContext::restore() stack is empty");
        return;
    }

    if (m_paintState->saveCount()) {
        m_paintState->decrementSaveCount();
    } else {
        m_paintStateIndex--;
        m_paintState = m_paintStateStack[m_paintStateIndex].get();
    }

    CanvasSaveState savedState = m_canvasStateStack.last();
    m_canvasStateStack.removeLast();
    m_canvasSaveFlags = savedState.m_flags;
    m_canvas->restoreToCount(savedState.m_restoreCount);
}

void GraphicsContext::saveLayer(const SkRect* bounds, const SkPaint* paint, SkCanvas::SaveFlags saveFlags)
{
    if (paintingDisabled())
        return;

    realizeCanvasSave(SkCanvas::kMatrixClip_SaveFlag);

    m_canvas->saveLayer(bounds, paint, saveFlags);
    if (bounds)
        m_canvas->clipRect(*bounds);
    if (m_trackOpaqueRegion)
        m_opaqueRegion.pushCanvasLayer(paint);
}

void GraphicsContext::restoreLayer()
{
    if (paintingDisabled())
        return;

    m_canvas->restore();
    if (m_trackOpaqueRegion)
        m_opaqueRegion.popCanvasLayer(this);
}

void GraphicsContext::beginAnnotation(const char* rendererName, const char* paintPhase,
    const String& elementId, const String& elementClass, const String& elementTag)
{
    if (paintingDisabled())
        return;

    canvas()->beginCommentGroup("GraphicsContextAnnotation");

    GraphicsContextAnnotation annotation(rendererName, paintPhase, elementId, elementClass, elementTag);
    AnnotationList annotations;
    annotation.asAnnotationList(annotations);

    AnnotationList::const_iterator end = annotations.end();
    for (AnnotationList::const_iterator it = annotations.begin(); it != end; ++it)
        canvas()->addComment(it->first, it->second.ascii().data());

#if !ASSERT_DISABLED
    ++m_annotationCount;
#endif
}

void GraphicsContext::endAnnotation()
{
    if (paintingDisabled())
        return;

    canvas()->endCommentGroup();

    ASSERT(m_annotationCount > 0);
#if !ASSERT_DISABLED
    --m_annotationCount;
#endif
}

void GraphicsContext::setStrokePattern(PassRefPtr<Pattern> pattern)
{
    if (paintingDisabled())
        return;

    ASSERT(pattern);
    if (!pattern) {
        setStrokeColor(Color::black);
        return;
    }
    mutableState()->setStrokePattern(pattern);
}

void GraphicsContext::setStrokeGradient(PassRefPtr<Gradient> gradient)
{
    if (paintingDisabled())
        return;

    ASSERT(gradient);
    if (!gradient) {
        setStrokeColor(Color::black);
        return;
    }
    mutableState()->setStrokeGradient(gradient);
}

void GraphicsContext::setFillPattern(PassRefPtr<Pattern> pattern)
{
    if (paintingDisabled())
        return;

    ASSERT(pattern);
    if (!pattern) {
        setFillColor(Color::black);
        return;
    }

    mutableState()->setFillPattern(pattern);
}

void GraphicsContext::setFillGradient(PassRefPtr<Gradient> gradient)
{
    if (paintingDisabled())
        return;

    ASSERT(gradient);
    if (!gradient) {
        setFillColor(Color::black);
        return;
    }

    mutableState()->setFillGradient(gradient);
}

void GraphicsContext::setShadow(const FloatSize& offset, float blur, const Color& color,
    DrawLooper::ShadowTransformMode shadowTransformMode,
    DrawLooper::ShadowAlphaMode shadowAlphaMode)
{
    if (paintingDisabled())
        return;

    if (!color.alpha() || (!offset.width() && !offset.height() && !blur)) {
        clearShadow();
        return;
    }

    DrawLooper drawLooper;
    drawLooper.addShadow(offset, blur, color, shadowTransformMode, shadowAlphaMode);
    drawLooper.addUnmodifiedContent();
    setDrawLooper(drawLooper);
}

void GraphicsContext::setDrawLooper(const DrawLooper& drawLooper)
{
    if (paintingDisabled())
        return;

    mutableState()->setDrawLooper(drawLooper);
}

void GraphicsContext::clearDrawLooper()
{
    if (paintingDisabled())
        return;

    mutableState()->clearDrawLooper();
}

bool GraphicsContext::hasShadow() const
{
    return !!immutableState()->drawLooper();
}

FloatRect GraphicsContext::getClipBounds() const
{
    if (paintingDisabled())
        return FloatRect();
    SkRect rect;
    if (!m_canvas->getClipBounds(&rect))
        return FloatRect();
    return FloatRect(rect);
}

bool GraphicsContext::getTransformedClipBounds(FloatRect* bounds) const
{
    if (paintingDisabled())
        return false;
    SkIRect skIBounds;
    if (!m_canvas->getClipDeviceBounds(&skIBounds))
        return false;
    SkRect skBounds = SkRect::MakeFromIRect(skIBounds);
    *bounds = FloatRect(skBounds);
    return true;
}

SkMatrix GraphicsContext::getTotalMatrix() const
{
    if (paintingDisabled())
        return SkMatrix::I();

    if (!isRecording())
        return m_canvas->getTotalMatrix();

    const RecordingState& recordingState = m_recordingStateStack.last();
    SkMatrix totalMatrix = recordingState.m_savedMatrix;
    totalMatrix.preConcat(m_canvas->getTotalMatrix());

    return totalMatrix;
}

void GraphicsContext::adjustTextRenderMode(SkPaint* paint)
{
    if (paintingDisabled())
        return;

    if (!paint->isLCDRenderText())
        return;

    paint->setLCDRenderText(couldUseLCDRenderedText());
}

bool GraphicsContext::couldUseLCDRenderedText()
{
    // Our layers only have a single alpha channel. This means that subpixel
    // rendered text cannot be composited correctly when the layer is
    // collapsed. Therefore, subpixel text is disabled when we are drawing
    // onto a layer.
    if (paintingDisabled() || isDrawingToLayer() || !isCertainlyOpaque())
        return false;

    return shouldSmoothFonts();
}

void GraphicsContext::setCompositeOperation(CompositeOperator compositeOperation, WebBlendMode blendMode)
{
    mutableState()->setCompositeOperation(compositeOperation, blendMode);
}

SkColorFilter* GraphicsContext::colorFilter()
{
    return immutableState()->colorFilter();
}

void GraphicsContext::setColorFilter(ColorFilter colorFilter)
{
    GraphicsContextState* stateToSet = mutableState();

    // We only support one active color filter at the moment. If (when) this becomes a problem,
    // we should switch to using color filter chains (Skia work in progress).
    ASSERT(!stateToSet->colorFilter());
    stateToSet->setColorFilter(WebCoreColorFilterToSkiaColorFilter(colorFilter));
}

bool GraphicsContext::readPixels(SkBitmap* bitmap, int x, int y, SkCanvas::Config8888 config8888)
{
    if (paintingDisabled())
        return false;

    return m_canvas->readPixels(bitmap, x, y, config8888);
}

void GraphicsContext::setMatrix(const SkMatrix& matrix)
{
    if (paintingDisabled())
        return;

    realizeCanvasSave(SkCanvas::kMatrix_SaveFlag);

    m_canvas->setMatrix(matrix);
}

void GraphicsContext::concat(const SkMatrix& matrix)
{
    if (paintingDisabled())
        return;

    if (matrix.isIdentity())
        return;

    realizeCanvasSave(SkCanvas::kMatrix_SaveFlag);

    m_canvas->concat(matrix);
}

void GraphicsContext::beginTransparencyLayer(float opacity, const FloatRect* bounds)
{
    beginLayer(opacity, immutableState()->compositeOperator(), bounds);
}

void GraphicsContext::beginLayer(float opacity, CompositeOperator op, const FloatRect* bounds, ColorFilter colorFilter, ImageFilter* imageFilter)
{
    if (paintingDisabled())
        return;

    // We need the "alpha" layer flag here because the base layer is opaque
    // (the surface of the page) but layers on top may have transparent parts.
    // Without explicitly setting the alpha flag, the layer will inherit the
    // opaque setting of the base and some things won't work properly.
    SkCanvas::SaveFlags saveFlags = static_cast<SkCanvas::SaveFlags>(SkCanvas::kHasAlphaLayer_SaveFlag | SkCanvas::kFullColorLayer_SaveFlag);

    SkPaint layerPaint;
    layerPaint.setAlpha(static_cast<unsigned char>(opacity * 255));
    layerPaint.setXfermode(WebCoreCompositeToSkiaComposite(op, m_paintState->blendMode()).get());
    layerPaint.setColorFilter(WebCoreColorFilterToSkiaColorFilter(colorFilter).get());
    layerPaint.setImageFilter(imageFilter);

    // Filters will adjust the clip to accomodate for filter bounds, but
    // need the kClipToLayer_SaveFlag to do so. We also save the clip here, so
    // it is restored back before the filtered layer is drawn in restore().
    // The matrix also needs to be saved, in order to be correctly passed to
    // the filter CTM during restore().
    if (imageFilter)
        saveFlags = SkCanvas::kARGB_ClipLayer_SaveFlag;

    if (bounds) {
        SkRect skBounds = WebCoreFloatRectToSKRect(*bounds);
        saveLayer(&skBounds, &layerPaint, saveFlags);
    } else {
        saveLayer(0, &layerPaint, saveFlags);
    }

#if !ASSERT_DISABLED
    ++m_layerCount;
#endif
}

void GraphicsContext::endLayer()
{
    if (paintingDisabled())
        return;

    restoreLayer();

    ASSERT(m_layerCount > 0);
#if !ASSERT_DISABLED
    --m_layerCount;
#endif
}

void GraphicsContext::beginRecording(const FloatRect& bounds)
{
    RefPtr<DisplayList> displayList = adoptRef(new DisplayList(bounds));

    SkCanvas* savedCanvas = m_canvas;
    SkMatrix savedMatrix = getTotalMatrix();

    IntRect recordingRect = enclosingIntRect(bounds);
    m_canvas = displayList->picture()->beginRecording(recordingRect.width(), recordingRect.height(),
        SkPicture::kUsePathBoundsForClip_RecordingFlag);

    // We want the bounds offset mapped to (0, 0), such that the display list content
    // is fully contained within the SkPictureRecord's bounds.
    if (!toFloatSize(bounds.location()).isZero()) {
        m_canvas->translate(-bounds.x(), -bounds.y());
        // To avoid applying the offset repeatedly in getTotalMatrix(), we pre-apply it here.
        savedMatrix.preTranslate(bounds.x(), bounds.y());
    }

    m_recordingStateStack.append(RecordingState(savedCanvas, savedMatrix, displayList));
}

PassRefPtr<DisplayList> GraphicsContext::endRecording()
{
    ASSERT(!m_recordingStateStack.isEmpty());

    RecordingState recording = m_recordingStateStack.last();
    ASSERT(recording.m_displayList->picture()->getRecordingCanvas());
    recording.m_displayList->picture()->endRecording();

    m_recordingStateStack.removeLast();
    m_canvas = recording.m_savedCanvas;

    return recording.m_displayList.release();
}

bool GraphicsContext::isRecording() const
{
    return !m_recordingStateStack.isEmpty();
}

void GraphicsContext::drawDisplayList(DisplayList* displayList)
{
    ASSERT(displayList);
    ASSERT(!displayList->picture()->getRecordingCanvas());

    if (paintingDisabled() || displayList->bounds().isEmpty())
        return;

    realizeCanvasSave(SkCanvas::kMatrixClip_SaveFlag);

    const FloatRect& bounds = displayList->bounds();
    if (bounds.x() || bounds.y())
        m_canvas->translate(bounds.x(), bounds.y());

    m_canvas->drawPicture(*displayList->picture());

    if (bounds.x() || bounds.y())
        m_canvas->translate(-bounds.x(), -bounds.y());
}

void GraphicsContext::setupPaintForFilling(SkPaint* paint) const
{
    if (paintingDisabled())
        return;

    *paint = immutableState()->fillPaint();
}

void GraphicsContext::setupPaintForStroking(SkPaint* paint) const
{
    if (paintingDisabled())
        return;

    *paint = immutableState()->strokePaint();
}

void GraphicsContext::drawConvexPolygon(size_t numPoints, const FloatPoint* points, bool shouldAntialias)
{
    if (paintingDisabled())
        return;

    if (numPoints <= 1)
        return;

    SkPath path;
    setPathFromConvexPoints(&path, numPoints, points);

    SkPaint paint(immutableState()->fillPaint());
    paint.setAntiAlias(shouldAntialias);
    drawPath(path, paint);

    if (strokeStyle() != NoStroke)
        drawPath(path, immutableState()->strokePaint());
}

// This method is only used to draw the little circles used in lists.
void GraphicsContext::drawEllipse(const IntRect& elipseRect)
{
    if (paintingDisabled())
        return;

    SkRect rect = elipseRect;
    drawOval(rect, immutableState()->fillPaint());

    if (strokeStyle() != NoStroke)
        drawOval(rect, immutableState()->strokePaint());
}

void GraphicsContext::drawFocusRing(const Path& focusRingPath, int width, int offset, const Color& color)
{
    // FIXME: Implement support for offset.
    if (paintingDisabled())
        return;

    SkPaint paint;
    paint.setAntiAlias(true);
    paint.setStyle(SkPaint::kStroke_Style);
    paint.setColor(color.rgb());

    drawOuterPath(focusRingPath.skPath(), paint, width);
    drawInnerPath(focusRingPath.skPath(), paint, width);
}

void GraphicsContext::drawFocusRing(const Vector<IntRect>& rects, int width, int offset, const Color& color)
{
    if (paintingDisabled())
        return;

    unsigned rectCount = rects.size();
    if (!rectCount)
        return;

    SkRegion focusRingRegion;
    const int focusRingOutset = getFocusRingOutset(offset);
    for (unsigned i = 0; i < rectCount; i++) {
        SkIRect r = rects[i];
        r.inset(-focusRingOutset, -focusRingOutset);
        focusRingRegion.op(r, SkRegion::kUnion_Op);
    }

    SkPath path;
    SkPaint paint;
    paint.setAntiAlias(true);
    paint.setStyle(SkPaint::kStroke_Style);

    paint.setColor(color.rgb());
    focusRingRegion.getBoundaryPath(&path);
    drawOuterPath(path, paint, width);
    drawInnerPath(path, paint, width);
}

static inline IntRect areaCastingShadowInHole(const IntRect& holeRect, int shadowBlur, int shadowSpread, const IntSize& shadowOffset)
{
    IntRect bounds(holeRect);

    bounds.inflate(shadowBlur);

    if (shadowSpread < 0)
        bounds.inflate(-shadowSpread);

    IntRect offsetBounds = bounds;
    offsetBounds.move(-shadowOffset);
    return unionRect(bounds, offsetBounds);
}

void GraphicsContext::drawInnerShadow(const RoundedRect& rect, const Color& shadowColor, const IntSize shadowOffset, int shadowBlur, int shadowSpread, Edges clippedEdges)
{
    IntRect holeRect(rect.rect());
    holeRect.inflate(-shadowSpread);

    if (holeRect.isEmpty()) {
        if (rect.isRounded())
            fillRoundedRect(rect, shadowColor);
        else
            fillRect(rect.rect(), shadowColor);
        return;
    }

    if (clippedEdges & LeftEdge) {
        holeRect.move(-max(shadowOffset.width(), 0) - shadowBlur, 0);
        holeRect.setWidth(holeRect.width() + max(shadowOffset.width(), 0) + shadowBlur);
    }
    if (clippedEdges & TopEdge) {
        holeRect.move(0, -max(shadowOffset.height(), 0) - shadowBlur);
        holeRect.setHeight(holeRect.height() + max(shadowOffset.height(), 0) + shadowBlur);
    }
    if (clippedEdges & RightEdge)
        holeRect.setWidth(holeRect.width() - min(shadowOffset.width(), 0) + shadowBlur);
    if (clippedEdges & BottomEdge)
        holeRect.setHeight(holeRect.height() - min(shadowOffset.height(), 0) + shadowBlur);

    Color fillColor(shadowColor.red(), shadowColor.green(), shadowColor.blue(), 255);

    IntRect outerRect = areaCastingShadowInHole(rect.rect(), shadowBlur, shadowSpread, shadowOffset);
    RoundedRect roundedHole(holeRect, rect.radii());

    save();
    if (rect.isRounded()) {
        Path path;
        path.addRoundedRect(rect);
        clipPath(path);
        roundedHole.shrinkRadii(shadowSpread);
    } else {
        clip(rect.rect());
    }

    DrawLooper drawLooper;
    drawLooper.addShadow(shadowOffset, shadowBlur, shadowColor,
        DrawLooper::ShadowRespectsTransforms, DrawLooper::ShadowIgnoresAlpha);
    setDrawLooper(drawLooper);
    fillRectWithRoundedHole(outerRect, roundedHole, fillColor);
    restore();
    clearDrawLooper();
}

void GraphicsContext::drawLine(const IntPoint& point1, const IntPoint& point2)
{
    if (paintingDisabled())
        return;

    StrokeStyle penStyle = strokeStyle();
    if (penStyle == NoStroke)
        return;

    FloatPoint p1 = point1;
    FloatPoint p2 = point2;
    bool isVerticalLine = (p1.x() == p2.x());
    int width = roundf(strokeThickness());

    // We know these are vertical or horizontal lines, so the length will just
    // be the sum of the displacement component vectors give or take 1 -
    // probably worth the speed up of no square root, which also won't be exact.
    FloatSize disp = p2 - p1;
    int length = SkScalarRoundToInt(disp.width() + disp.height());
    SkPaint paint(immutableState()->strokePaint(length));

    if (strokeStyle() == DottedStroke || strokeStyle() == DashedStroke) {
        // Do a rect fill of our endpoints.  This ensures we always have the
        // appearance of being a border.  We then draw the actual dotted/dashed line.
        SkRect r1, r2;
        r1.set(p1.x(), p1.y(), p1.x() + width, p1.y() + width);
        r2.set(p2.x(), p2.y(), p2.x() + width, p2.y() + width);

        if (isVerticalLine) {
            r1.offset(-width / 2, 0);
            r2.offset(-width / 2, -width);
        } else {
            r1.offset(0, -width / 2);
            r2.offset(-width, -width / 2);
        }
        SkPaint fillPaint;
        fillPaint.setColor(paint.getColor());
        drawRect(r1, fillPaint);
        drawRect(r2, fillPaint);
    }

    adjustLineToPixelBoundaries(p1, p2, width, penStyle);
    SkPoint pts[2] = { (SkPoint)p1, (SkPoint)p2 };

    m_canvas->drawPoints(SkCanvas::kLines_PointMode, 2, pts, paint);

    if (m_trackOpaqueRegion)
        m_opaqueRegion.didDrawPoints(this, SkCanvas::kLines_PointMode, 2, pts, paint);
}

void GraphicsContext::drawLineForDocumentMarker(const FloatPoint& pt, float width, DocumentMarkerLineStyle style)
{
    if (paintingDisabled())
        return;

    int deviceScaleFactor = m_useHighResMarker ? 2 : 1;

    // Create the pattern we'll use to draw the underline.
    int index = style == DocumentMarkerGrammarLineStyle ? 1 : 0;
    static SkBitmap* misspellBitmap1x[2] = { 0, 0 };
    static SkBitmap* misspellBitmap2x[2] = { 0, 0 };
    SkBitmap** misspellBitmap = deviceScaleFactor == 2 ? misspellBitmap2x : misspellBitmap1x;
    if (!misspellBitmap[index]) {
#if OS(MACOSX)
        // Match the artwork used by the Mac.
        const int rowPixels = 4 * deviceScaleFactor;
        const int colPixels = 3 * deviceScaleFactor;
        misspellBitmap[index] = new SkBitmap;
        misspellBitmap[index]->setConfig(SkBitmap::kARGB_8888_Config,
                                         rowPixels, colPixels);
        misspellBitmap[index]->allocPixels();

        misspellBitmap[index]->eraseARGB(0, 0, 0, 0);
        const uint32_t transparentColor = 0x00000000;

        if (deviceScaleFactor == 1) {
            const uint32_t colors[2][6] = {
                { 0x2a2a0600, 0x57571000,  0xa8a81b00, 0xbfbf1f00,  0x70701200, 0xe0e02400 },
                { 0x2a0f0f0f, 0x571e1e1e,  0xa83d3d3d, 0xbf454545,  0x70282828, 0xe0515151 }
            };

            // Pattern: a b a   a b a
            //          c d c   c d c
            //          e f e   e f e
            for (int x = 0; x < colPixels; ++x) {
                uint32_t* row = misspellBitmap[index]->getAddr32(0, x);
                row[0] = colors[index][x * 2];
                row[1] = colors[index][x * 2 + 1];
                row[2] = colors[index][x * 2];
                row[3] = transparentColor;
            }
        } else if (deviceScaleFactor == 2) {
            const uint32_t colors[2][18] = {
                { 0x0a090101, 0x33320806, 0x55540f0a,  0x37360906, 0x6e6c120c, 0x6e6c120c,  0x7674140d, 0x8d8b1810, 0x8d8b1810,
                  0x96941a11, 0xb3b01f15, 0xb3b01f15,  0x6d6b130c, 0xd9d62619, 0xd9d62619,  0x19180402, 0x7c7a150e, 0xcecb2418 },
                { 0x0a020202, 0x33141414, 0x55232323,  0x37161616, 0x6e2e2e2e, 0x6e2e2e2e,  0x76313131, 0x8d3a3a3a, 0x8d3a3a3a,
                  0x963e3e3e, 0xb34b4b4b, 0xb34b4b4b,  0x6d2d2d2d, 0xd95b5b5b, 0xd95b5b5b,  0x19090909, 0x7c343434, 0xce575757 }
            };

            // Pattern: a b c c b a
            //          d e f f e d
            //          g h j j h g
            //          k l m m l k
            //          n o p p o n
            //          q r s s r q
            for (int x = 0; x < colPixels; ++x) {
                uint32_t* row = misspellBitmap[index]->getAddr32(0, x);
                row[0] = colors[index][x * 3];
                row[1] = colors[index][x * 3 + 1];
                row[2] = colors[index][x * 3 + 2];
                row[3] = colors[index][x * 3 + 2];
                row[4] = colors[index][x * 3 + 1];
                row[5] = colors[index][x * 3];
                row[6] = transparentColor;
                row[7] = transparentColor;
            }
        } else
            ASSERT_NOT_REACHED();
#else
        // We use a 2-pixel-high misspelling indicator because that seems to be
        // what WebKit is designed for, and how much room there is in a typical
        // page for it.
        const int rowPixels = 32 * deviceScaleFactor; // Must be multiple of 4 for pattern below.
        const int colPixels = 2 * deviceScaleFactor;
        misspellBitmap[index] = new SkBitmap;
        misspellBitmap[index]->setConfig(SkBitmap::kARGB_8888_Config, rowPixels, colPixels);
        misspellBitmap[index]->allocPixels();

        misspellBitmap[index]->eraseARGB(0, 0, 0, 0);
        if (deviceScaleFactor == 1)
            draw1xMarker(misspellBitmap[index], index);
        else if (deviceScaleFactor == 2)
            draw2xMarker(misspellBitmap[index], index);
        else
            ASSERT_NOT_REACHED();
#endif
    }

#if OS(MACOSX)
    SkScalar originX = WebCoreFloatToSkScalar(pt.x()) * deviceScaleFactor;
    SkScalar originY = WebCoreFloatToSkScalar(pt.y()) * deviceScaleFactor;

    // Make sure to draw only complete dots.
    int rowPixels = misspellBitmap[index]->width();
    float widthMod = fmodf(width * deviceScaleFactor, rowPixels);
    if (rowPixels - widthMod > deviceScaleFactor)
        width -= widthMod / deviceScaleFactor;
#else
    SkScalar originX = WebCoreFloatToSkScalar(pt.x());

    // Offset it vertically by 1 so that there's some space under the text.
    SkScalar originY = WebCoreFloatToSkScalar(pt.y()) + 1;
    originX *= deviceScaleFactor;
    originY *= deviceScaleFactor;
#endif

    RefPtr<SkShader> shader = adoptRef(SkShader::CreateBitmapShader(
        *misspellBitmap[index], SkShader::kRepeat_TileMode, SkShader::kRepeat_TileMode));
    SkMatrix matrix;
    matrix.setTranslate(originX, originY);
    shader->setLocalMatrix(matrix);

    SkPaint paint;
    paint.setShader(shader.get());

    SkRect rect;
    rect.set(originX, originY, originX + WebCoreFloatToSkScalar(width) * deviceScaleFactor, originY + SkIntToScalar(misspellBitmap[index]->height()));

    if (deviceScaleFactor == 2) {
        save();
        scale(FloatSize(0.5, 0.5));
    }
    drawRect(rect, paint);
    if (deviceScaleFactor == 2)
        restore();
}

void GraphicsContext::drawLineForText(const FloatPoint& pt, float width, bool printing)
{
    if (paintingDisabled())
        return;

    if (width <= 0)
        return;

    SkPaint paint;
    switch (strokeStyle()) {
    case NoStroke:
    case SolidStroke:
    case DoubleStroke:
    case WavyStroke: {
        int thickness = SkMax32(static_cast<int>(strokeThickness()), 1);
        SkRect r;
        r.fLeft = WebCoreFloatToSkScalar(pt.x());
        // Avoid anti-aliasing lines. Currently, these are always horizontal.
        // Round to nearest pixel to match text and other content.
        r.fTop = WebCoreFloatToSkScalar(floorf(pt.y() + 0.5f));
        r.fRight = r.fLeft + WebCoreFloatToSkScalar(width);
        r.fBottom = r.fTop + SkIntToScalar(thickness);
        paint = immutableState()->fillPaint();
        // Text lines are drawn using the stroke color.
        paint.setColor(effectiveStrokeColor());
        drawRect(r, paint);
        return;
    }
    case DottedStroke:
    case DashedStroke: {
        int y = floorf(pt.y() + std::max<float>(strokeThickness() / 2.0f, 0.5f));
        drawLine(IntPoint(pt.x(), y), IntPoint(pt.x() + width, y));
        return;
    }
    }

    ASSERT_NOT_REACHED();
}

// Draws a filled rectangle with a stroked border.
void GraphicsContext::drawRect(const IntRect& rect)
{
    if (paintingDisabled())
        return;

    ASSERT(!rect.isEmpty());
    if (rect.isEmpty())
        return;

    SkRect skRect = rect;
    int fillcolorNotTransparent = immutableState()->fillColor().rgb() & 0xFF000000;
    if (fillcolorNotTransparent)
        drawRect(skRect, immutableState()->fillPaint());

    if (immutableState()->strokeData().style() != NoStroke
        && immutableState()->strokeData().color().alpha()) {
        // Stroke a width: 1 inset border
        SkPaint paint(immutableState()->fillPaint());
        paint.setColor(effectiveStrokeColor());
        paint.setStyle(SkPaint::kStroke_Style);
        paint.setStrokeWidth(1);

        skRect.inset(0.5f, 0.5f);
        drawRect(skRect, paint);
    }
}

void GraphicsContext::drawText(const Font& font, const TextRunPaintInfo& runInfo, const FloatPoint& point)
{
    if (paintingDisabled())
        return;

    font.drawText(this, runInfo, point);
}

void GraphicsContext::drawEmphasisMarks(const Font& font, const TextRunPaintInfo& runInfo, const AtomicString& mark, const FloatPoint& point)
{
    if (paintingDisabled())
        return;

    font.drawEmphasisMarks(this, runInfo, mark, point);
}

void GraphicsContext::drawBidiText(const Font& font, const TextRunPaintInfo& runInfo, const FloatPoint& point, Font::CustomFontNotReadyAction customFontNotReadyAction)
{
    if (paintingDisabled())
        return;

    // sub-run painting is not supported for Bidi text.
    const TextRun& run = runInfo.run;
    ASSERT((runInfo.from == 0) && (runInfo.to == run.length()));
    BidiResolver<TextRunIterator, BidiCharacterRun> bidiResolver;
    bidiResolver.setStatus(BidiStatus(run.direction(), run.directionalOverride()));
    bidiResolver.setPositionIgnoringNestedIsolates(TextRunIterator(&run, 0));

    // FIXME: This ownership should be reversed. We should pass BidiRunList
    // to BidiResolver in createBidiRunsForLine.
    BidiRunList<BidiCharacterRun>& bidiRuns = bidiResolver.runs();
    bidiResolver.createBidiRunsForLine(TextRunIterator(&run, run.length()));
    if (!bidiRuns.runCount())
        return;

    FloatPoint currPoint = point;
    BidiCharacterRun* bidiRun = bidiRuns.firstRun();
    while (bidiRun) {
        TextRun subrun = run.subRun(bidiRun->start(), bidiRun->stop() - bidiRun->start());
        bool isRTL = bidiRun->level() % 2;
        subrun.setDirection(isRTL ? RTL : LTR);
        subrun.setDirectionalOverride(bidiRun->dirOverride(false));

        TextRunPaintInfo subrunInfo(subrun);
        subrunInfo.bounds = runInfo.bounds;
        font.drawText(this, subrunInfo, currPoint, customFontNotReadyAction);

        bidiRun = bidiRun->next();
        // FIXME: Have Font::drawText return the width of what it drew so that we don't have to re-measure here.
        if (bidiRun)
            currPoint.move(font.width(subrun), 0);
    }

    bidiRuns.deleteRuns();
}

void GraphicsContext::drawHighlightForText(const Font& font, const TextRun& run, const FloatPoint& point, int h, const Color& backgroundColor, int from, int to)
{
    if (paintingDisabled())
        return;

    fillRect(font.selectionRectForText(run, point, h, from, to), backgroundColor);
}

void GraphicsContext::drawImage(Image* image, const IntPoint& p, CompositeOperator op, RespectImageOrientationEnum shouldRespectImageOrientation)
{
    if (!image)
        return;
    drawImage(image, FloatRect(IntRect(p, image->size())), FloatRect(FloatPoint(), FloatSize(image->size())), op, shouldRespectImageOrientation);
}

void GraphicsContext::drawImage(Image* image, const IntRect& r, CompositeOperator op, RespectImageOrientationEnum shouldRespectImageOrientation, bool useLowQualityScale)
{
    if (!image)
        return;
    drawImage(image, FloatRect(r), FloatRect(FloatPoint(), FloatSize(image->size())), op, shouldRespectImageOrientation, useLowQualityScale);
}

void GraphicsContext::drawImage(Image* image, const FloatRect& dest, const FloatRect& src, CompositeOperator op, RespectImageOrientationEnum shouldRespectImageOrientation, bool useLowQualityScale)
{
    drawImage(image, dest, src, op, blink::WebBlendModeNormal, shouldRespectImageOrientation, useLowQualityScale);
}

void GraphicsContext::drawImage(Image* image, const FloatRect& dest)
{
    if (!image)
        return;
    drawImage(image, dest, FloatRect(IntRect(IntPoint(), image->size())));
}

void GraphicsContext::drawImage(Image* image, const FloatRect& dest, const FloatRect& src, CompositeOperator op, WebBlendMode blendMode, RespectImageOrientationEnum shouldRespectImageOrientation, bool useLowQualityScale)
{    if (paintingDisabled() || !image)
        return;

    InterpolationQuality previousInterpolationQuality = InterpolationDefault;

    if (useLowQualityScale) {
        previousInterpolationQuality = imageInterpolationQuality();
        setImageInterpolationQuality(InterpolationLow);
    }

    image->draw(this, dest, src, op, blendMode, shouldRespectImageOrientation);

    if (useLowQualityScale)
        setImageInterpolationQuality(previousInterpolationQuality);
}

void GraphicsContext::drawTiledImage(Image* image, const IntRect& destRect, const IntPoint& srcPoint, const IntSize& tileSize, CompositeOperator op, bool useLowQualityScale, WebBlendMode blendMode, const IntSize& repeatSpacing)
{
    if (paintingDisabled() || !image)
        return;

    if (useLowQualityScale) {
        InterpolationQuality previousInterpolationQuality = imageInterpolationQuality();
        setImageInterpolationQuality(InterpolationLow);
        image->drawTiled(this, destRect, srcPoint, tileSize, op, blendMode, repeatSpacing);
        setImageInterpolationQuality(previousInterpolationQuality);
    } else {
        image->drawTiled(this, destRect, srcPoint, tileSize, op, blendMode, repeatSpacing);
    }
}

void GraphicsContext::drawTiledImage(Image* image, const IntRect& dest, const IntRect& srcRect,
    const FloatSize& tileScaleFactor, Image::TileRule hRule, Image::TileRule vRule, CompositeOperator op, bool useLowQualityScale)
{
    if (paintingDisabled() || !image)
        return;

    if (hRule == Image::StretchTile && vRule == Image::StretchTile) {
        // Just do a scale.
        drawImage(image, dest, srcRect, op);
        return;
    }

    if (useLowQualityScale) {
        InterpolationQuality previousInterpolationQuality = imageInterpolationQuality();
        setImageInterpolationQuality(InterpolationLow);
        image->drawTiled(this, dest, srcRect, tileScaleFactor, hRule, vRule, op);
        setImageInterpolationQuality(previousInterpolationQuality);
    } else {
        image->drawTiled(this, dest, srcRect, tileScaleFactor, hRule, vRule, op);
    }
}

void GraphicsContext::drawImageBuffer(ImageBuffer* image, const IntPoint& p, CompositeOperator op, WebBlendMode blendMode)
{
    if (!image)
        return;
    drawImageBuffer(image, FloatRect(IntRect(p, image->size())), FloatRect(FloatPoint(), FloatSize(image->size())), op, blendMode);
}

void GraphicsContext::drawImageBuffer(ImageBuffer* image, const IntRect& r, CompositeOperator op, WebBlendMode blendMode, bool useLowQualityScale)
{
    if (!image)
        return;
    drawImageBuffer(image, FloatRect(r), FloatRect(FloatPoint(), FloatSize(image->size())), op, blendMode, useLowQualityScale);
}

void GraphicsContext::drawImageBuffer(ImageBuffer* image, const IntPoint& dest, const IntRect& srcRect, CompositeOperator op, WebBlendMode blendMode)
{
    drawImageBuffer(image, FloatRect(IntRect(dest, srcRect.size())), FloatRect(srcRect), op, blendMode);
}

void GraphicsContext::drawImageBuffer(ImageBuffer* image, const IntRect& dest, const IntRect& srcRect, CompositeOperator op, WebBlendMode blendMode, bool useLowQualityScale)
{
    drawImageBuffer(image, FloatRect(dest), FloatRect(srcRect), op, blendMode, useLowQualityScale);
}

void GraphicsContext::drawImageBuffer(ImageBuffer* image, const FloatRect& dest)
{
    if (!image)
        return;
    drawImageBuffer(image, dest, FloatRect(IntRect(IntPoint(), image->size())));
}

void GraphicsContext::drawImageBuffer(ImageBuffer* image, const FloatRect& dest, const FloatRect& src, CompositeOperator op, WebBlendMode blendMode, bool useLowQualityScale)
{
    if (paintingDisabled() || !image)
        return;

    if (useLowQualityScale) {
        InterpolationQuality previousInterpolationQuality = imageInterpolationQuality();
        setImageInterpolationQuality(InterpolationLow);
        image->draw(this, dest, src, op, blendMode, useLowQualityScale);
        setImageInterpolationQuality(previousInterpolationQuality);
    } else {
        image->draw(this, dest, src, op, blendMode, useLowQualityScale);
    }
}

void GraphicsContext::writePixels(const SkImageInfo& info, const void* pixels, size_t rowBytes, int x, int y)
{
    if (paintingDisabled())
        return;

    m_canvas->writePixels(info, pixels, rowBytes, x, y);

    if (m_trackOpaqueRegion) {
        SkRect rect = SkRect::MakeXYWH(x, y, info.width(), info.height());
        SkPaint paint;

        paint.setXfermodeMode(SkXfermode::kSrc_Mode);
        if (kOpaque_SkAlphaType != info.alphaType())
            paint.setAlpha(0x80); // signal to m_opaqueRegion that we are not fully opaque

        m_opaqueRegion.didDrawRect(this, rect, paint, 0);
        // more efficient would be to call markRectAsOpaque or MarkRectAsNonOpaque directly,
        // rather than cons-ing up a paint with an xfermode and alpha
    }
}

void GraphicsContext::writePixels(const SkBitmap& bitmap, int x, int y)
{
    if (!bitmap.getTexture()) {
        SkAutoLockPixels alp(bitmap);
        if (bitmap.getPixels())
            writePixels(bitmap.info(), bitmap.getPixels(), bitmap.rowBytes(), x, y);
    }
}

void GraphicsContext::drawBitmap(const SkBitmap& bitmap, SkScalar left, SkScalar top, const SkPaint* paint)
{
    if (paintingDisabled())
        return;

    m_canvas->drawBitmap(bitmap, left, top, paint);

    if (m_trackOpaqueRegion) {
        SkRect rect = SkRect::MakeXYWH(left, top, bitmap.width(), bitmap.height());
        m_opaqueRegion.didDrawRect(this, rect, *paint, &bitmap);
    }
}

void GraphicsContext::drawBitmapRect(const SkBitmap& bitmap, const SkRect* src,
    const SkRect& dst, const SkPaint* paint)
{
    if (paintingDisabled())
        return;

    SkCanvas::DrawBitmapRectFlags flags =
        immutableState()->shouldClampToSourceRect() ? SkCanvas::kNone_DrawBitmapRectFlag : SkCanvas::kBleed_DrawBitmapRectFlag;

    m_canvas->drawBitmapRectToRect(bitmap, src, dst, paint, flags);

    if (m_trackOpaqueRegion)
        m_opaqueRegion.didDrawRect(this, dst, *paint, &bitmap);
}

void GraphicsContext::drawOval(const SkRect& oval, const SkPaint& paint)
{
    if (paintingDisabled())
        return;

    m_canvas->drawOval(oval, paint);

    if (m_trackOpaqueRegion)
        m_opaqueRegion.didDrawBounded(this, oval, paint);
}

void GraphicsContext::drawPath(const SkPath& path, const SkPaint& paint)
{
    if (paintingDisabled())
        return;

    m_canvas->drawPath(path, paint);

    if (m_trackOpaqueRegion)
        m_opaqueRegion.didDrawPath(this, path, paint);
}

void GraphicsContext::drawRect(const SkRect& rect, const SkPaint& paint)
{
    if (paintingDisabled())
        return;

    m_canvas->drawRect(rect, paint);

    if (m_trackOpaqueRegion)
        m_opaqueRegion.didDrawRect(this, rect, paint, 0);
}

void GraphicsContext::didDrawRect(const SkRect& rect, const SkPaint& paint, const SkBitmap* bitmap)
{
    if (m_trackOpaqueRegion)
        m_opaqueRegion.didDrawRect(this, rect, paint, bitmap);
}

void GraphicsContext::drawPosText(const void* text, size_t byteLength,
    const SkPoint pos[],  const SkRect& textRect, const SkPaint& paint)
{
    if (paintingDisabled())
        return;

    m_canvas->drawPosText(text, byteLength, pos, paint);
    didDrawTextInRect(textRect);

    // FIXME: compute bounds for positioned text.
    if (m_trackOpaqueRegion)
        m_opaqueRegion.didDrawUnbounded(this, paint, OpaqueRegionSkia::FillOrStroke);
}

void GraphicsContext::drawPosTextH(const void* text, size_t byteLength,
    const SkScalar xpos[], SkScalar constY,  const SkRect& textRect, const SkPaint& paint)
{
    if (paintingDisabled())
        return;

    m_canvas->drawPosTextH(text, byteLength, xpos, constY, paint);
    didDrawTextInRect(textRect);

    // FIXME: compute bounds for positioned text.
    if (m_trackOpaqueRegion)
        m_opaqueRegion.didDrawUnbounded(this, paint, OpaqueRegionSkia::FillOrStroke);
}

void GraphicsContext::drawTextOnPath(const void* text, size_t byteLength,
    const SkPath& path,  const SkRect& textRect, const SkMatrix* matrix, const SkPaint& paint)
{
    if (paintingDisabled())
        return;

    m_canvas->drawTextOnPath(text, byteLength, path, matrix, paint);
    didDrawTextInRect(textRect);

    // FIXME: compute bounds for positioned text.
    if (m_trackOpaqueRegion)
        m_opaqueRegion.didDrawUnbounded(this, paint, OpaqueRegionSkia::FillOrStroke);
}

void GraphicsContext::fillPath(const Path& pathToFill)
{
    if (paintingDisabled() || pathToFill.isEmpty())
        return;

    // Use const_cast and temporarily modify the fill type instead of copying the path.
    SkPath& path = const_cast<SkPath&>(pathToFill.skPath());
    SkPath::FillType previousFillType = path.getFillType();

    SkPath::FillType temporaryFillType =
        immutableState()->fillRule() == RULE_EVENODD ? SkPath::kEvenOdd_FillType : SkPath::kWinding_FillType;
    path.setFillType(temporaryFillType);

    drawPath(path, immutableState()->fillPaint());

    path.setFillType(previousFillType);
}

void GraphicsContext::fillRect(const FloatRect& rect)
{
    if (paintingDisabled())
        return;

    SkRect r = rect;

    drawRect(r, immutableState()->fillPaint());
}

void GraphicsContext::fillRect(const FloatRect& rect, const Color& color)
{
    if (paintingDisabled())
        return;

    SkRect r = rect;
    SkPaint paint = immutableState()->fillPaint();
    paint.setColor(color.rgb());
    drawRect(r, paint);
}

void GraphicsContext::fillBetweenRoundedRects(const IntRect& outer, const IntSize& outerTopLeft, const IntSize& outerTopRight, const IntSize& outerBottomLeft, const IntSize& outerBottomRight,
    const IntRect& inner, const IntSize& innerTopLeft, const IntSize& innerTopRight, const IntSize& innerBottomLeft, const IntSize& innerBottomRight, const Color& color) {
    if (paintingDisabled())
        return;

    SkVector outerRadii[4];
    SkVector innerRadii[4];
    setRadii(outerRadii, outerTopLeft, outerTopRight, outerBottomRight, outerBottomLeft);
    setRadii(innerRadii, innerTopLeft, innerTopRight, innerBottomRight, innerBottomLeft);

    SkRRect rrOuter;
    SkRRect rrInner;
    rrOuter.setRectRadii(outer, outerRadii);
    rrInner.setRectRadii(inner, innerRadii);

    SkPaint paint(immutableState()->fillPaint());
    paint.setColor(color.rgb());

    m_canvas->drawDRRect(rrOuter, rrInner, paint);

    if (m_trackOpaqueRegion)
        m_opaqueRegion.didDrawBounded(this, rrOuter.getBounds(), paint);
}

void GraphicsContext::fillBetweenRoundedRects(const RoundedRect& outer, const RoundedRect& inner, const Color& color)
{
    fillBetweenRoundedRects(outer.rect(), outer.radii().topLeft(), outer.radii().topRight(), outer.radii().bottomLeft(), outer.radii().bottomRight(),
        inner.rect(), inner.radii().topLeft(), inner.radii().topRight(), inner.radii().bottomLeft(), inner.radii().bottomRight(), color);
}

void GraphicsContext::fillRoundedRect(const IntRect& rect, const IntSize& topLeft, const IntSize& topRight,
    const IntSize& bottomLeft, const IntSize& bottomRight, const Color& color)
{
    if (paintingDisabled())
        return;

    if (topLeft.width() + topRight.width() > rect.width()
            || bottomLeft.width() + bottomRight.width() > rect.width()
            || topLeft.height() + bottomLeft.height() > rect.height()
            || topRight.height() + bottomRight.height() > rect.height()) {
        // Not all the radii fit, return a rect. This matches the behavior of
        // Path::createRoundedRectangle. Without this we attempt to draw a round
        // shadow for a square box.
        fillRect(rect, color);
        return;
    }

    SkVector radii[4];
    setRadii(radii, topLeft, topRight, bottomRight, bottomLeft);

    SkRRect rr;
    rr.setRectRadii(rect, radii);

    SkPaint paint(immutableState()->fillPaint());
    paint.setColor(color.rgb());

    m_canvas->drawRRect(rr, paint);

    if (m_trackOpaqueRegion)
        m_opaqueRegion.didDrawBounded(this, rr.getBounds(), paint);
}

void GraphicsContext::fillEllipse(const FloatRect& ellipse)
{
    if (paintingDisabled())
        return;

    SkRect rect = ellipse;
    drawOval(rect, immutableState()->fillPaint());
}

void GraphicsContext::strokePath(const Path& pathToStroke)
{
    if (paintingDisabled() || pathToStroke.isEmpty())
        return;

    const SkPath& path = pathToStroke.skPath();
    drawPath(path, immutableState()->strokePaint());
}

void GraphicsContext::strokeRect(const FloatRect& rect, float lineWidth)
{
    if (paintingDisabled())
        return;

    SkPaint paint(immutableState()->strokePaint());
    paint.setStrokeWidth(WebCoreFloatToSkScalar(lineWidth));
    // Reset the dash effect to account for the width
    immutableState()->strokeData().setupPaintDashPathEffect(&paint, 0);
    // strokerect has special rules for CSS when the rect is degenerate:
    // if width==0 && height==0, do nothing
    // if width==0 || height==0, then just draw line for the other dimension
    SkRect r(rect);
    bool validW = r.width() > 0;
    bool validH = r.height() > 0;
    if (validW && validH) {
        drawRect(r, paint);
    } else if (validW || validH) {
        // we are expected to respect the lineJoin, so we can't just call
        // drawLine -- we have to create a path that doubles back on itself.
        SkPath path;
        path.moveTo(r.fLeft, r.fTop);
        path.lineTo(r.fRight, r.fBottom);
        path.close();
        drawPath(path, paint);
    }
}

void GraphicsContext::strokeEllipse(const FloatRect& ellipse)
{
    if (paintingDisabled())
        return;

    drawOval(ellipse, immutableState()->strokePaint());
}

void GraphicsContext::clipRoundedRect(const RoundedRect& rect, SkRegion::Op regionOp)
{
    if (paintingDisabled())
        return;

    if (!rect.isRounded()) {
        clipRect(rect.rect(), NotAntiAliased, regionOp);
        return;
    }

    SkVector radii[4];
    RoundedRect::Radii wkRadii = rect.radii();
    setRadii(radii, wkRadii.topLeft(), wkRadii.topRight(), wkRadii.bottomRight(), wkRadii.bottomLeft());

    SkRRect r;
    r.setRectRadii(rect.rect(), radii);

    clipRRect(r, AntiAliased, regionOp);
}

void GraphicsContext::clipOut(const Path& pathToClip)
{
    if (paintingDisabled())
        return;

    // Use const_cast and temporarily toggle the inverse fill type instead of copying the path.
    SkPath& path = const_cast<SkPath&>(pathToClip.skPath());
    path.toggleInverseFillType();
    clipPath(path, AntiAliased);
    path.toggleInverseFillType();
}

void GraphicsContext::clipPath(const Path& pathToClip, WindRule clipRule)
{
    if (paintingDisabled() || pathToClip.isEmpty())
        return;

    // Use const_cast and temporarily modify the fill type instead of copying the path.
    SkPath& path = const_cast<SkPath&>(pathToClip.skPath());
    SkPath::FillType previousFillType = path.getFillType();

    SkPath::FillType temporaryFillType = clipRule == RULE_EVENODD ? SkPath::kEvenOdd_FillType : SkPath::kWinding_FillType;
    path.setFillType(temporaryFillType);
    clipPath(path, AntiAliased);

    path.setFillType(previousFillType);
}

void GraphicsContext::clipConvexPolygon(size_t numPoints, const FloatPoint* points, bool antialiased)
{
    if (paintingDisabled())
        return;

    if (numPoints <= 1)
        return;

    SkPath path;
    setPathFromConvexPoints(&path, numPoints, points);
    clipPath(path, antialiased ? AntiAliased : NotAntiAliased);
}

void GraphicsContext::clipOutRoundedRect(const RoundedRect& rect)
{
    if (paintingDisabled())
        return;

    clipRoundedRect(rect, SkRegion::kDifference_Op);
}

void GraphicsContext::canvasClip(const Path& pathToClip, WindRule clipRule)
{
    if (paintingDisabled())
        return;

    // Use const_cast and temporarily modify the fill type instead of copying the path.
    SkPath& path = const_cast<SkPath&>(pathToClip.skPath());
    SkPath::FillType previousFillType = path.getFillType();

    SkPath::FillType temporaryFillType = clipRule == RULE_EVENODD ? SkPath::kEvenOdd_FillType : SkPath::kWinding_FillType;
    path.setFillType(temporaryFillType);
    clipPath(path);

    path.setFillType(previousFillType);
}

void GraphicsContext::clipRect(const SkRect& rect, AntiAliasingMode aa, SkRegion::Op op)
{
    if (paintingDisabled())
        return;

    realizeCanvasSave(SkCanvas::kClip_SaveFlag);

    m_canvas->clipRect(rect, op, aa == AntiAliased);
}

void GraphicsContext::clipPath(const SkPath& path, AntiAliasingMode aa, SkRegion::Op op)
{
    if (paintingDisabled())
        return;

    realizeCanvasSave(SkCanvas::kClip_SaveFlag);

    m_canvas->clipPath(path, op, aa == AntiAliased);
}

void GraphicsContext::clipRRect(const SkRRect& rect, AntiAliasingMode aa, SkRegion::Op op)
{
    if (paintingDisabled())
        return;

    realizeCanvasSave(SkCanvas::kClip_SaveFlag);

    m_canvas->clipRRect(rect, op, aa == AntiAliased);
}

void GraphicsContext::rotate(float angleInRadians)
{
    if (paintingDisabled())
        return;

    realizeCanvasSave(SkCanvas::kMatrix_SaveFlag);

    m_canvas->rotate(WebCoreFloatToSkScalar(angleInRadians * (180.0f / 3.14159265f)));
}

void GraphicsContext::translate(float w, float h)
{
    if (paintingDisabled())
        return;

    if (!w && !h)
        return;

    realizeCanvasSave(SkCanvas::kMatrix_SaveFlag);

    m_canvas->translate(WebCoreFloatToSkScalar(w), WebCoreFloatToSkScalar(h));
}

void GraphicsContext::scale(const FloatSize& size)
{
    if (paintingDisabled())
        return;

    if (size.width() == 1.0f && size.height() == 1.0f)
        return;

    realizeCanvasSave(SkCanvas::kMatrix_SaveFlag);

    m_canvas->scale(WebCoreFloatToSkScalar(size.width()), WebCoreFloatToSkScalar(size.height()));
}

void GraphicsContext::setURLForRect(const KURL& link, const IntRect& destRect)
{
    if (paintingDisabled())
        return;

    SkAutoDataUnref url(SkData::NewWithCString(link.string().utf8().data()));
    SkAnnotateRectWithURL(m_canvas, destRect, url.get());
}

void GraphicsContext::setURLFragmentForRect(const String& destName, const IntRect& rect)
{
    if (paintingDisabled())
        return;

    SkAutoDataUnref skDestName(SkData::NewWithCString(destName.utf8().data()));
    SkAnnotateLinkToDestination(m_canvas, rect, skDestName.get());
}

void GraphicsContext::addURLTargetAtPoint(const String& name, const IntPoint& pos)
{
    if (paintingDisabled())
        return;

    SkAutoDataUnref nameData(SkData::NewWithCString(name.utf8().data()));
    SkAnnotateNamedDestination(m_canvas, SkPoint::Make(pos.x(), pos.y()), nameData);
}

AffineTransform GraphicsContext::getCTM(IncludeDeviceScale) const
{
    if (paintingDisabled())
        return AffineTransform();

    SkMatrix m = getTotalMatrix();
    return AffineTransform(SkScalarToDouble(m.getScaleX()),
                           SkScalarToDouble(m.getSkewY()),
                           SkScalarToDouble(m.getSkewX()),
                           SkScalarToDouble(m.getScaleY()),
                           SkScalarToDouble(m.getTranslateX()),
                           SkScalarToDouble(m.getTranslateY()));
}

void GraphicsContext::fillRect(const FloatRect& rect, const Color& color, CompositeOperator op)
{
    if (paintingDisabled())
        return;

    CompositeOperator previousOperator = compositeOperation();
    setCompositeOperation(op);
    fillRect(rect, color);
    setCompositeOperation(previousOperator);
}

void GraphicsContext::fillRoundedRect(const RoundedRect& rect, const Color& color)
{
    if (rect.isRounded())
        fillRoundedRect(rect.rect(), rect.radii().topLeft(), rect.radii().topRight(), rect.radii().bottomLeft(), rect.radii().bottomRight(), color);
    else
        fillRect(rect.rect(), color);
}

void GraphicsContext::fillRectWithRoundedHole(const IntRect& rect, const RoundedRect& roundedHoleRect, const Color& color)
{
    if (paintingDisabled())
        return;

    Path path;
    path.addRect(rect);

    if (!roundedHoleRect.radii().isZero())
        path.addRoundedRect(roundedHoleRect);
    else
        path.addRect(roundedHoleRect.rect());

    WindRule oldFillRule = fillRule();
    Color oldFillColor = fillColor();

    setFillRule(RULE_EVENODD);
    setFillColor(color);

    fillPath(path);

    setFillRule(oldFillRule);
    setFillColor(oldFillColor);
}

void GraphicsContext::clearRect(const FloatRect& rect)
{
    if (paintingDisabled())
        return;

    SkRect r = rect;
    SkPaint paint(immutableState()->fillPaint());
    paint.setXfermodeMode(SkXfermode::kClear_Mode);
    drawRect(r, paint);
}

void GraphicsContext::adjustLineToPixelBoundaries(FloatPoint& p1, FloatPoint& p2, float strokeWidth, StrokeStyle penStyle)
{
    // For odd widths, we add in 0.5 to the appropriate x/y so that the float arithmetic
    // works out.  For example, with a border width of 3, WebKit will pass us (y1+y2)/2, e.g.,
    // (50+53)/2 = 103/2 = 51 when we want 51.5.  It is always true that an even width gave
    // us a perfect position, but an odd width gave us a position that is off by exactly 0.5.
    if (penStyle == DottedStroke || penStyle == DashedStroke) {
        if (p1.x() == p2.x()) {
            p1.setY(p1.y() + strokeWidth);
            p2.setY(p2.y() - strokeWidth);
        } else {
            p1.setX(p1.x() + strokeWidth);
            p2.setX(p2.x() - strokeWidth);
        }
    }

    if (static_cast<int>(strokeWidth) % 2) { //odd
        if (p1.x() == p2.x()) {
            // We're a vertical line.  Adjust our x.
            p1.setX(p1.x() + 0.5f);
            p2.setX(p2.x() + 0.5f);
        } else {
            // We're a horizontal line. Adjust our y.
            p1.setY(p1.y() + 0.5f);
            p2.setY(p2.y() + 0.5f);
        }
    }
}

PassOwnPtr<ImageBuffer> GraphicsContext::createCompatibleBuffer(const IntSize& size, OpacityMode opacityMode) const
{
    // Make the buffer larger if the context's transform is scaling it so we need a higher
    // resolution than one pixel per unit. Also set up a corresponding scale factor on the
    // graphics context.

    AffineTransform transform = getCTM(DefinitelyIncludeDeviceScale);
    IntSize scaledSize(static_cast<int>(ceil(size.width() * transform.xScale())), static_cast<int>(ceil(size.height() * transform.yScale())));

    SkAlphaType alphaType = (opacityMode == Opaque) ? kOpaque_SkAlphaType : kPremul_SkAlphaType;
    SkImageInfo info = SkImageInfo::MakeN32(size.width(), size.height(), alphaType);
    RefPtr<SkSurface> skSurface = adoptRef(m_canvas->newSurface(info));
    if (!skSurface)
        return nullptr;
    OwnPtr<ImageBufferSurface> surface = adoptPtr(new CompatibleImageBufferSurface(skSurface.release(), scaledSize, opacityMode));
    ASSERT(surface->isValid());
    OwnPtr<ImageBuffer> buffer = adoptPtr(new ImageBuffer(surface.release()));

    buffer->context()->scale(FloatSize(static_cast<float>(scaledSize.width()) / size.width(),
        static_cast<float>(scaledSize.height()) / size.height()));

    return buffer.release();
}

void GraphicsContext::setPathFromConvexPoints(SkPath* path, size_t numPoints, const FloatPoint* points)
{
    path->incReserve(numPoints);
    path->moveTo(WebCoreFloatToSkScalar(points[0].x()),
                 WebCoreFloatToSkScalar(points[0].y()));
    for (size_t i = 1; i < numPoints; ++i) {
        path->lineTo(WebCoreFloatToSkScalar(points[i].x()),
                     WebCoreFloatToSkScalar(points[i].y()));
    }

    /*  The code used to just blindly call this
            path->setIsConvex(true);
        But webkit can sometimes send us non-convex 4-point values, so we mark the path's
        convexity as unknown, so it will get computed by skia at draw time.
        See crbug.com 108605
    */
    SkPath::Convexity convexity = SkPath::kConvex_Convexity;
    if (numPoints == 4)
        convexity = SkPath::kUnknown_Convexity;
    path->setConvexity(convexity);
}

void GraphicsContext::drawOuterPath(const SkPath& path, SkPaint& paint, int width)
{
#if OS(MACOSX)
    paint.setAlpha(64);
    paint.setStrokeWidth(width);
    paint.setPathEffect(new SkCornerPathEffect((width - 1) * 0.5f))->unref();
#else
    paint.setStrokeWidth(1);
    paint.setPathEffect(new SkCornerPathEffect(1))->unref();
#endif
    drawPath(path, paint);
}

void GraphicsContext::drawInnerPath(const SkPath& path, SkPaint& paint, int width)
{
#if OS(MACOSX)
    paint.setAlpha(128);
    paint.setStrokeWidth(width * 0.5f);
    drawPath(path, paint);
#endif
}

void GraphicsContext::setRadii(SkVector* radii, IntSize topLeft, IntSize topRight, IntSize bottomRight, IntSize bottomLeft)
{
    radii[SkRRect::kUpperLeft_Corner].set(SkIntToScalar(topLeft.width()),
        SkIntToScalar(topLeft.height()));
    radii[SkRRect::kUpperRight_Corner].set(SkIntToScalar(topRight.width()),
        SkIntToScalar(topRight.height()));
    radii[SkRRect::kLowerRight_Corner].set(SkIntToScalar(bottomRight.width()),
        SkIntToScalar(bottomRight.height()));
    radii[SkRRect::kLowerLeft_Corner].set(SkIntToScalar(bottomLeft.width()),
        SkIntToScalar(bottomLeft.height()));
}

PassRefPtr<SkColorFilter> GraphicsContext::WebCoreColorFilterToSkiaColorFilter(ColorFilter colorFilter)
{
    switch (colorFilter) {
    case ColorFilterLuminanceToAlpha:
        return adoptRef(SkLumaColorFilter::Create());
    case ColorFilterLinearRGBToSRGB:
        return ImageBuffer::createColorSpaceFilter(ColorSpaceLinearRGB, ColorSpaceDeviceRGB);
    case ColorFilterSRGBToLinearRGB:
        return ImageBuffer::createColorSpaceFilter(ColorSpaceDeviceRGB, ColorSpaceLinearRGB);
    case ColorFilterNone:
        break;
    default:
        ASSERT_NOT_REACHED();
        break;
    }

    return nullptr;
}

#if OS(MACOSX)
CGColorSpaceRef PLATFORM_EXPORT deviceRGBColorSpaceRef()
{
    static CGColorSpaceRef deviceSpace = CGColorSpaceCreateDeviceRGB();
    return deviceSpace;
}
#else
void GraphicsContext::draw2xMarker(SkBitmap* bitmap, int index)
{
    const SkPMColor lineColor = lineColors(index);
    const SkPMColor antiColor1 = antiColors1(index);
    const SkPMColor antiColor2 = antiColors2(index);

    uint32_t* row1 = bitmap->getAddr32(0, 0);
    uint32_t* row2 = bitmap->getAddr32(0, 1);
    uint32_t* row3 = bitmap->getAddr32(0, 2);
    uint32_t* row4 = bitmap->getAddr32(0, 3);

    // Pattern: X0o   o0X0o   o0
    //          XX0o o0XXX0o o0X
    //           o0XXX0o o0XXX0o
    //            o0X0o   o0X0o
    const SkPMColor row1Color[] = { lineColor, antiColor1, antiColor2, 0,          0,         0,          antiColor2, antiColor1 };
    const SkPMColor row2Color[] = { lineColor, lineColor,  antiColor1, antiColor2, 0,         antiColor2, antiColor1, lineColor };
    const SkPMColor row3Color[] = { 0,         antiColor2, antiColor1, lineColor,  lineColor, lineColor,  antiColor1, antiColor2 };
    const SkPMColor row4Color[] = { 0,         0,          antiColor2, antiColor1, lineColor, antiColor1, antiColor2, 0 };

    for (int x = 0; x < bitmap->width() + 8; x += 8) {
        int count = std::min(bitmap->width() - x, 8);
        if (count > 0) {
            memcpy(row1 + x, row1Color, count * sizeof(SkPMColor));
            memcpy(row2 + x, row2Color, count * sizeof(SkPMColor));
            memcpy(row3 + x, row3Color, count * sizeof(SkPMColor));
            memcpy(row4 + x, row4Color, count * sizeof(SkPMColor));
        }
    }
}

void GraphicsContext::draw1xMarker(SkBitmap* bitmap, int index)
{
    const uint32_t lineColor = lineColors(index);
    const uint32_t antiColor = antiColors2(index);

    // Pattern: X o   o X o   o X
    //            o X o   o X o
    uint32_t* row1 = bitmap->getAddr32(0, 0);
    uint32_t* row2 = bitmap->getAddr32(0, 1);
    for (int x = 0; x < bitmap->width(); x++) {
        switch (x % 4) {
        case 0:
            row1[x] = lineColor;
            break;
        case 1:
            row1[x] = antiColor;
            row2[x] = antiColor;
            break;
        case 2:
            row2[x] = lineColor;
            break;
        case 3:
            row1[x] = antiColor;
            row2[x] = antiColor;
            break;
        }
    }
}

const SkPMColor GraphicsContext::lineColors(int index)
{
    static const SkPMColor colors[] = {
        SkPreMultiplyARGB(0xFF, 0xFF, 0x00, 0x00), // Opaque red.
        SkPreMultiplyARGB(0xFF, 0xC0, 0xC0, 0xC0) // Opaque gray.
    };

    return colors[index];
}

const SkPMColor GraphicsContext::antiColors1(int index)
{
    static const SkPMColor colors[] = {
        SkPreMultiplyARGB(0xB0, 0xFF, 0x00, 0x00), // Semitransparent red.
        SkPreMultiplyARGB(0xB0, 0xC0, 0xC0, 0xC0)  // Semitransparent gray.
    };

    return colors[index];
}

const SkPMColor GraphicsContext::antiColors2(int index)
{
    static const SkPMColor colors[] = {
        SkPreMultiplyARGB(0x60, 0xFF, 0x00, 0x00), // More transparent red
        SkPreMultiplyARGB(0x60, 0xC0, 0xC0, 0xC0)  // More transparent gray
    };

    return colors[index];
}
#endif

void GraphicsContext::didDrawTextInRect(const SkRect& textRect)
{
    if (m_trackTextRegion) {
        TRACE_EVENT0("skia", "PlatformContextSkia::trackTextRegion");
        m_textRegion.join(textRect);
    }
}

}
