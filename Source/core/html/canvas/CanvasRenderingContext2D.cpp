/*
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011, 2012 Apple Inc. All rights reserved.
 * Copyright (C) 2008, 2010 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2007 Alp Toker <alp@atoker.com>
 * Copyright (C) 2008 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2008 Dirk Schulze <krit@webkit.org>
 * Copyright (C) 2010 Torch Mobile (Beijing) Co. Ltd. All rights reserved.
 * Copyright (C) 2012, 2013 Intel Corporation. All rights reserved.
 * Copyright (C) 2013 Adobe Systems Incorporated. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "core/html/canvas/CanvasRenderingContext2D.h"

#include "CSSPropertyNames.h"
#include "bindings/v8/ExceptionMessages.h"
#include "bindings/v8/ExceptionState.h"
#include "bindings/v8/ExceptionStatePlaceholder.h"
#include "core/accessibility/AXObjectCache.h"
#include "core/css/CSSFontSelector.h"
#include "core/css/parser/BisonCSSParser.h"
#include "core/css/StylePropertySet.h"
#include "core/css/resolver/StyleResolver.h"
#include "core/dom/ExceptionCode.h"
#include "core/fetch/ImageResource.h"
#include "core/frame/ImageBitmap.h"
#include "core/html/HTMLCanvasElement.h"
#include "core/html/HTMLImageElement.h"
#include "core/html/HTMLMediaElement.h"
#include "core/html/HTMLVideoElement.h"
#include "core/html/ImageData.h"
#include "core/html/TextMetrics.h"
#include "core/html/canvas/CanvasGradient.h"
#include "core/html/canvas/CanvasPattern.h"
#include "core/html/canvas/CanvasStyle.h"
#include "core/html/canvas/Path2D.h"
#include "core/rendering/RenderImage.h"
#include "core/rendering/RenderLayer.h"
#include "core/rendering/RenderTheme.h"
#include "platform/fonts/FontCache.h"
#include "platform/geometry/FloatQuad.h"
#include "platform/graphics/GraphicsContextStateSaver.h"
#include "platform/graphics/DrawLooper.h"
#include "platform/text/TextRun.h"
#include "wtf/CheckedArithmetic.h"
#include "wtf/MathExtras.h"
#include "wtf/OwnPtr.h"
#include "wtf/Uint8ClampedArray.h"
#include "wtf/text/StringBuilder.h"

using namespace std;

namespace WebCore {

static const int defaultFontSize = 10;
static const char defaultFontFamily[] = "sans-serif";
static const char defaultFont[] = "10px sans-serif";

CanvasRenderingContext2D::CanvasRenderingContext2D(HTMLCanvasElement* canvas, const Canvas2DContextAttributes* attrs, bool usesCSSCompatibilityParseMode)
    : CanvasRenderingContext(canvas)
    , m_stateStack(1)
    , m_usesCSSCompatibilityParseMode(usesCSSCompatibilityParseMode)
    , m_hasAlpha(!attrs || attrs->alpha())
{
    ScriptWrappable::init(this);
}

void CanvasRenderingContext2D::unwindStateStack()
{
    // Ensure that the state stack in the ImageBuffer's context
    // is cleared before destruction, to avoid assertions in the
    // GraphicsContext dtor.
    if (size_t stackSize = m_stateStack.size()) {
        if (GraphicsContext* context = canvas()->existingDrawingContext()) {
            while (--stackSize)
                context->restore();
        }
    }
}

CanvasRenderingContext2D::~CanvasRenderingContext2D()
{
#if !ASSERT_DISABLED
    unwindStateStack();
#endif
}

bool CanvasRenderingContext2D::isAccelerated() const
{
    if (!canvas()->hasImageBuffer())
        return false;
    GraphicsContext* context = drawingContext();
    return context && context->isAccelerated();
}

void CanvasRenderingContext2D::reset()
{
    unwindStateStack();
    m_stateStack.resize(1);
    m_stateStack.first() = State();
    m_path.clear();
}

// Important: Several of these properties are also stored in GraphicsContext's
// StrokeData. The default values that StrokeData uses may not the same values
// that the canvas 2d spec specifies. Make sure to sync the initial state of the
// GraphicsContext in HTMLCanvasElement::createImageBuffer()!
CanvasRenderingContext2D::State::State()
    : m_unrealizedSaveCount(0)
    , m_strokeStyle(CanvasStyle::createFromRGBA(Color::black))
    , m_fillStyle(CanvasStyle::createFromRGBA(Color::black))
    , m_lineWidth(1)
    , m_lineCap(ButtCap)
    , m_lineJoin(MiterJoin)
    , m_miterLimit(10)
    , m_shadowBlur(0)
    , m_shadowColor(Color::transparent)
    , m_globalAlpha(1)
    , m_globalComposite(CompositeSourceOver)
    , m_globalBlend(blink::WebBlendModeNormal)
    , m_invertibleCTM(true)
    , m_lineDashOffset(0)
    , m_imageSmoothingEnabled(true)
    , m_textAlign(StartTextAlign)
    , m_textBaseline(AlphabeticTextBaseline)
    , m_unparsedFont(defaultFont)
    , m_realizedFont(false)
{
}

CanvasRenderingContext2D::State::State(const State& other)
    : CSSFontSelectorClient()
    , m_unrealizedSaveCount(other.m_unrealizedSaveCount)
    , m_unparsedStrokeColor(other.m_unparsedStrokeColor)
    , m_unparsedFillColor(other.m_unparsedFillColor)
    , m_strokeStyle(other.m_strokeStyle)
    , m_fillStyle(other.m_fillStyle)
    , m_lineWidth(other.m_lineWidth)
    , m_lineCap(other.m_lineCap)
    , m_lineJoin(other.m_lineJoin)
    , m_miterLimit(other.m_miterLimit)
    , m_shadowOffset(other.m_shadowOffset)
    , m_shadowBlur(other.m_shadowBlur)
    , m_shadowColor(other.m_shadowColor)
    , m_globalAlpha(other.m_globalAlpha)
    , m_globalComposite(other.m_globalComposite)
    , m_globalBlend(other.m_globalBlend)
    , m_transform(other.m_transform)
    , m_invertibleCTM(other.m_invertibleCTM)
    , m_lineDashOffset(other.m_lineDashOffset)
    , m_imageSmoothingEnabled(other.m_imageSmoothingEnabled)
    , m_textAlign(other.m_textAlign)
    , m_textBaseline(other.m_textBaseline)
    , m_unparsedFont(other.m_unparsedFont)
    , m_font(other.m_font)
    , m_realizedFont(other.m_realizedFont)
{
    if (m_realizedFont)
        static_cast<CSSFontSelector*>(m_font.fontSelector())->registerForInvalidationCallbacks(this);
}

CanvasRenderingContext2D::State& CanvasRenderingContext2D::State::operator=(const State& other)
{
    if (this == &other)
        return *this;

    if (m_realizedFont)
        static_cast<CSSFontSelector*>(m_font.fontSelector())->unregisterForInvalidationCallbacks(this);

    m_unrealizedSaveCount = other.m_unrealizedSaveCount;
    m_unparsedStrokeColor = other.m_unparsedStrokeColor;
    m_unparsedFillColor = other.m_unparsedFillColor;
    m_strokeStyle = other.m_strokeStyle;
    m_fillStyle = other.m_fillStyle;
    m_lineWidth = other.m_lineWidth;
    m_lineCap = other.m_lineCap;
    m_lineJoin = other.m_lineJoin;
    m_miterLimit = other.m_miterLimit;
    m_shadowOffset = other.m_shadowOffset;
    m_shadowBlur = other.m_shadowBlur;
    m_shadowColor = other.m_shadowColor;
    m_globalAlpha = other.m_globalAlpha;
    m_globalComposite = other.m_globalComposite;
    m_globalBlend = other.m_globalBlend;
    m_transform = other.m_transform;
    m_invertibleCTM = other.m_invertibleCTM;
    m_imageSmoothingEnabled = other.m_imageSmoothingEnabled;
    m_textAlign = other.m_textAlign;
    m_textBaseline = other.m_textBaseline;
    m_unparsedFont = other.m_unparsedFont;
    m_font = other.m_font;
    m_realizedFont = other.m_realizedFont;

    if (m_realizedFont)
        static_cast<CSSFontSelector*>(m_font.fontSelector())->registerForInvalidationCallbacks(this);

    return *this;
}

CanvasRenderingContext2D::State::~State()
{
    if (m_realizedFont)
        static_cast<CSSFontSelector*>(m_font.fontSelector())->unregisterForInvalidationCallbacks(this);
}

void CanvasRenderingContext2D::State::fontsNeedUpdate(CSSFontSelector* fontSelector)
{
    ASSERT_ARG(fontSelector, fontSelector == m_font.fontSelector());
    ASSERT(m_realizedFont);

    m_font.update(fontSelector);
}

void CanvasRenderingContext2D::realizeSaves()
{
    if (state().m_unrealizedSaveCount) {
        ASSERT(m_stateStack.size() >= 1);
        // Reduce the current state's unrealized count by one now,
        // to reflect the fact we are saving one state.
        m_stateStack.last().m_unrealizedSaveCount--;
        m_stateStack.append(state());
        // Set the new state's unrealized count to 0, because it has no outstanding saves.
        // We need to do this explicitly because the copy constructor and operator= used
        // by the Vector operations copy the unrealized count from the previous state (in
        // turn necessary to support correct resizing and unwinding of the stack).
        m_stateStack.last().m_unrealizedSaveCount = 0;
        GraphicsContext* context = drawingContext();
        if (context)
            context->save();
    }
}

void CanvasRenderingContext2D::restore()
{
    if (state().m_unrealizedSaveCount) {
        // We never realized the save, so just record that it was unnecessary.
        --m_stateStack.last().m_unrealizedSaveCount;
        return;
    }
    ASSERT(m_stateStack.size() >= 1);
    if (m_stateStack.size() <= 1)
        return;
    m_path.transform(state().m_transform);
    m_stateStack.removeLast();
    m_path.transform(state().m_transform.inverse());
    GraphicsContext* c = drawingContext();
    if (c)
        c->restore();
}

CanvasStyle* CanvasRenderingContext2D::strokeStyle() const
{
    return state().m_strokeStyle.get();
}

void CanvasRenderingContext2D::setStrokeStyle(PassRefPtr<CanvasStyle> prpStyle)
{
    RefPtr<CanvasStyle> style = prpStyle;

    if (!style)
        return;

    if (state().m_strokeStyle && state().m_strokeStyle->isEquivalentColor(*style))
        return;

    if (style->isCurrentColor()) {
        if (style->hasOverrideAlpha())
            style = CanvasStyle::createFromRGBA(colorWithOverrideAlpha(currentColor(canvas()), style->overrideAlpha()));
        else
            style = CanvasStyle::createFromRGBA(currentColor(canvas()));
    } else if (canvas()->originClean() && style->canvasPattern() && !style->canvasPattern()->originClean()) {
        canvas()->setOriginTainted();
    }

    realizeSaves();
    modifiableState().m_strokeStyle = style.release();
    GraphicsContext* c = drawingContext();
    if (!c)
        return;
    state().m_strokeStyle->applyStrokeColor(c);
    modifiableState().m_unparsedStrokeColor = String();
}

CanvasStyle* CanvasRenderingContext2D::fillStyle() const
{
    return state().m_fillStyle.get();
}

void CanvasRenderingContext2D::setFillStyle(PassRefPtr<CanvasStyle> prpStyle)
{
    RefPtr<CanvasStyle> style = prpStyle;

    if (!style)
        return;

    if (state().m_fillStyle && state().m_fillStyle->isEquivalentColor(*style))
        return;

    if (style->isCurrentColor()) {
        if (style->hasOverrideAlpha())
            style = CanvasStyle::createFromRGBA(colorWithOverrideAlpha(currentColor(canvas()), style->overrideAlpha()));
        else
            style = CanvasStyle::createFromRGBA(currentColor(canvas()));
    } else if (canvas()->originClean() && style->canvasPattern() && !style->canvasPattern()->originClean()) {
        canvas()->setOriginTainted();
    }

    realizeSaves();
    modifiableState().m_fillStyle = style.release();
    GraphicsContext* c = drawingContext();
    if (!c)
        return;
    state().m_fillStyle->applyFillColor(c);
    modifiableState().m_unparsedFillColor = String();
}

float CanvasRenderingContext2D::lineWidth() const
{
    return state().m_lineWidth;
}

void CanvasRenderingContext2D::setLineWidth(float width)
{
    if (!(std::isfinite(width) && width > 0))
        return;
    if (state().m_lineWidth == width)
        return;
    realizeSaves();
    modifiableState().m_lineWidth = width;
    GraphicsContext* c = drawingContext();
    if (!c)
        return;
    c->setStrokeThickness(width);
}

String CanvasRenderingContext2D::lineCap() const
{
    return lineCapName(state().m_lineCap);
}

void CanvasRenderingContext2D::setLineCap(const String& s)
{
    LineCap cap;
    if (!parseLineCap(s, cap))
        return;
    if (state().m_lineCap == cap)
        return;
    realizeSaves();
    modifiableState().m_lineCap = cap;
    GraphicsContext* c = drawingContext();
    if (!c)
        return;
    c->setLineCap(cap);
}

String CanvasRenderingContext2D::lineJoin() const
{
    return lineJoinName(state().m_lineJoin);
}

void CanvasRenderingContext2D::setLineJoin(const String& s)
{
    LineJoin join;
    if (!parseLineJoin(s, join))
        return;
    if (state().m_lineJoin == join)
        return;
    realizeSaves();
    modifiableState().m_lineJoin = join;
    GraphicsContext* c = drawingContext();
    if (!c)
        return;
    c->setLineJoin(join);
}

float CanvasRenderingContext2D::miterLimit() const
{
    return state().m_miterLimit;
}

void CanvasRenderingContext2D::setMiterLimit(float limit)
{
    if (!(std::isfinite(limit) && limit > 0))
        return;
    if (state().m_miterLimit == limit)
        return;
    realizeSaves();
    modifiableState().m_miterLimit = limit;
    GraphicsContext* c = drawingContext();
    if (!c)
        return;
    c->setMiterLimit(limit);
}

float CanvasRenderingContext2D::shadowOffsetX() const
{
    return state().m_shadowOffset.width();
}

void CanvasRenderingContext2D::setShadowOffsetX(float x)
{
    if (!std::isfinite(x))
        return;
    if (state().m_shadowOffset.width() == x)
        return;
    realizeSaves();
    modifiableState().m_shadowOffset.setWidth(x);
    applyShadow();
}

float CanvasRenderingContext2D::shadowOffsetY() const
{
    return state().m_shadowOffset.height();
}

void CanvasRenderingContext2D::setShadowOffsetY(float y)
{
    if (!std::isfinite(y))
        return;
    if (state().m_shadowOffset.height() == y)
        return;
    realizeSaves();
    modifiableState().m_shadowOffset.setHeight(y);
    applyShadow();
}

float CanvasRenderingContext2D::shadowBlur() const
{
    return state().m_shadowBlur;
}

void CanvasRenderingContext2D::setShadowBlur(float blur)
{
    if (!(std::isfinite(blur) && blur >= 0))
        return;
    if (state().m_shadowBlur == blur)
        return;
    realizeSaves();
    modifiableState().m_shadowBlur = blur;
    applyShadow();
}

String CanvasRenderingContext2D::shadowColor() const
{
    return Color(state().m_shadowColor).serialized();
}

void CanvasRenderingContext2D::setShadowColor(const String& color)
{
    RGBA32 rgba;
    if (!parseColorOrCurrentColor(rgba, color, canvas()))
        return;
    if (state().m_shadowColor == rgba)
        return;
    realizeSaves();
    modifiableState().m_shadowColor = rgba;
    applyShadow();
}

const Vector<float>& CanvasRenderingContext2D::getLineDash() const
{
    return state().m_lineDash;
}

static bool lineDashSequenceIsValid(const Vector<float>& dash)
{
    for (size_t i = 0; i < dash.size(); i++) {
        if (!std::isfinite(dash[i]) || dash[i] < 0)
            return false;
    }
    return true;
}

void CanvasRenderingContext2D::setLineDash(const Vector<float>& dash)
{
    if (!lineDashSequenceIsValid(dash))
        return;

    realizeSaves();
    modifiableState().m_lineDash = dash;
    // Spec requires the concatenation of two copies the dash list when the
    // number of elements is odd
    if (dash.size() % 2)
        modifiableState().m_lineDash.appendVector(dash);

    applyLineDash();
}

float CanvasRenderingContext2D::lineDashOffset() const
{
    return state().m_lineDashOffset;
}

void CanvasRenderingContext2D::setLineDashOffset(float offset)
{
    if (!std::isfinite(offset) || state().m_lineDashOffset == offset)
        return;

    realizeSaves();
    modifiableState().m_lineDashOffset = offset;
    applyLineDash();
}

void CanvasRenderingContext2D::applyLineDash() const
{
    GraphicsContext* c = drawingContext();
    if (!c)
        return;
    DashArray convertedLineDash(state().m_lineDash.size());
    for (size_t i = 0; i < state().m_lineDash.size(); ++i)
        convertedLineDash[i] = static_cast<DashArrayElement>(state().m_lineDash[i]);
    c->setLineDash(convertedLineDash, state().m_lineDashOffset);
}

float CanvasRenderingContext2D::globalAlpha() const
{
    return state().m_globalAlpha;
}

void CanvasRenderingContext2D::setGlobalAlpha(float alpha)
{
    if (!(alpha >= 0 && alpha <= 1))
        return;
    if (state().m_globalAlpha == alpha)
        return;
    realizeSaves();
    modifiableState().m_globalAlpha = alpha;
    GraphicsContext* c = drawingContext();
    if (!c)
        return;
    c->setAlphaAsFloat(alpha);
}

String CanvasRenderingContext2D::globalCompositeOperation() const
{
    return compositeOperatorName(state().m_globalComposite, state().m_globalBlend);
}

void CanvasRenderingContext2D::setGlobalCompositeOperation(const String& operation)
{
    CompositeOperator op = CompositeSourceOver;
    blink::WebBlendMode blendMode = blink::WebBlendModeNormal;
    if (!parseCompositeAndBlendOperator(operation, op, blendMode))
        return;
    if ((state().m_globalComposite == op) && (state().m_globalBlend == blendMode))
        return;
    realizeSaves();
    modifiableState().m_globalComposite = op;
    modifiableState().m_globalBlend = blendMode;
    GraphicsContext* c = drawingContext();
    if (!c)
        return;
    c->setCompositeOperation(op, blendMode);
}

void CanvasRenderingContext2D::setCurrentTransform(PassRefPtr<SVGMatrixTearOff> passMatrixTearOff, ExceptionState& exceptionState)
{
    RefPtr<SVGMatrixTearOff> matrixTearOff = passMatrixTearOff;

    // FIXME: bindings should do null checking and throw: http://crbug.com/321518
    if (!matrixTearOff) {
        exceptionState.throwTypeError("currentTransform only accepts a SVGMatrix.");
        return;
    }

    const AffineTransform& transform = matrixTearOff->value();
    setTransform(transform.a(), transform.b(), transform.c(), transform.d(), transform.e(), transform.f());
}

void CanvasRenderingContext2D::scale(float sx, float sy)
{
    GraphicsContext* c = drawingContext();
    if (!c)
        return;
    if (!state().m_invertibleCTM)
        return;

    if (!std::isfinite(sx) | !std::isfinite(sy))
        return;

    AffineTransform newTransform = state().m_transform;
    newTransform.scaleNonUniform(sx, sy);
    if (state().m_transform == newTransform)
        return;

    realizeSaves();

    if (!newTransform.isInvertible()) {
        modifiableState().m_invertibleCTM = false;
        return;
    }

    modifiableState().m_transform = newTransform;
    c->scale(FloatSize(sx, sy));
    m_path.transform(AffineTransform().scaleNonUniform(1.0 / sx, 1.0 / sy));
}

void CanvasRenderingContext2D::rotate(float angleInRadians)
{
    GraphicsContext* c = drawingContext();
    if (!c)
        return;
    if (!state().m_invertibleCTM)
        return;

    if (!std::isfinite(angleInRadians))
        return;

    AffineTransform newTransform = state().m_transform;
    newTransform.rotateRadians(angleInRadians);
    if (state().m_transform == newTransform)
        return;

    realizeSaves();

    if (!newTransform.isInvertible()) {
        modifiableState().m_invertibleCTM = false;
        return;
    }

    modifiableState().m_transform = newTransform;
    c->rotate(angleInRadians);
    m_path.transform(AffineTransform().rotateRadians(-angleInRadians));
}

void CanvasRenderingContext2D::translate(float tx, float ty)
{
    GraphicsContext* c = drawingContext();
    if (!c)
        return;
    if (!state().m_invertibleCTM)
        return;

    if (!std::isfinite(tx) | !std::isfinite(ty))
        return;

    AffineTransform newTransform = state().m_transform;
    newTransform.translate(tx, ty);
    if (state().m_transform == newTransform)
        return;

    realizeSaves();

    if (!newTransform.isInvertible()) {
        modifiableState().m_invertibleCTM = false;
        return;
    }

    modifiableState().m_transform = newTransform;
    c->translate(tx, ty);
    m_path.transform(AffineTransform().translate(-tx, -ty));
}

void CanvasRenderingContext2D::transform(float m11, float m12, float m21, float m22, float dx, float dy)
{
    GraphicsContext* c = drawingContext();
    if (!c)
        return;
    if (!state().m_invertibleCTM)
        return;

    if (!std::isfinite(m11) | !std::isfinite(m21) | !std::isfinite(dx) | !std::isfinite(m12) | !std::isfinite(m22) | !std::isfinite(dy))
        return;

    AffineTransform transform(m11, m12, m21, m22, dx, dy);
    AffineTransform newTransform = state().m_transform * transform;
    if (state().m_transform == newTransform)
        return;

    realizeSaves();

    modifiableState().m_transform = newTransform;
    if (!newTransform.isInvertible()) {
        modifiableState().m_invertibleCTM = false;
        return;
    }

    c->concatCTM(transform);
    m_path.transform(transform.inverse());
}

void CanvasRenderingContext2D::resetTransform()
{
    GraphicsContext* c = drawingContext();
    if (!c)
        return;

    AffineTransform ctm = state().m_transform;
    bool invertibleCTM = state().m_invertibleCTM;
    // It is possible that CTM is identity while CTM is not invertible.
    // When CTM becomes non-invertible, realizeSaves() can make CTM identity.
    if (ctm.isIdentity() && invertibleCTM)
        return;

    realizeSaves();
    // resetTransform() resolves the non-invertible CTM state.
    modifiableState().m_transform.makeIdentity();
    modifiableState().m_invertibleCTM = true;
    c->setCTM(canvas()->baseTransform());

    if (invertibleCTM)
        m_path.transform(ctm);
    // When else, do nothing because all transform methods didn't update m_path when CTM became non-invertible.
    // It means that resetTransform() restores m_path just before CTM became non-invertible.
}

void CanvasRenderingContext2D::setTransform(float m11, float m12, float m21, float m22, float dx, float dy)
{
    GraphicsContext* c = drawingContext();
    if (!c)
        return;

    if (!std::isfinite(m11) | !std::isfinite(m21) | !std::isfinite(dx) | !std::isfinite(m12) | !std::isfinite(m22) | !std::isfinite(dy))
        return;

    resetTransform();
    transform(m11, m12, m21, m22, dx, dy);
}

void CanvasRenderingContext2D::setStrokeColor(const String& color)
{
    if (color == state().m_unparsedStrokeColor)
        return;
    realizeSaves();
    setStrokeStyle(CanvasStyle::createFromString(color));
    modifiableState().m_unparsedStrokeColor = color;
}

void CanvasRenderingContext2D::setStrokeColor(float grayLevel)
{
    if (state().m_strokeStyle && state().m_strokeStyle->isEquivalentRGBA(grayLevel, grayLevel, grayLevel, 1.0f))
        return;
    setStrokeStyle(CanvasStyle::createFromGrayLevelWithAlpha(grayLevel, 1.0f));
}

void CanvasRenderingContext2D::setStrokeColor(const String& color, float alpha)
{
    setStrokeStyle(CanvasStyle::createFromStringWithOverrideAlpha(color, alpha));
}

void CanvasRenderingContext2D::setStrokeColor(float grayLevel, float alpha)
{
    if (state().m_strokeStyle && state().m_strokeStyle->isEquivalentRGBA(grayLevel, grayLevel, grayLevel, alpha))
        return;
    setStrokeStyle(CanvasStyle::createFromGrayLevelWithAlpha(grayLevel, alpha));
}

void CanvasRenderingContext2D::setStrokeColor(float r, float g, float b, float a)
{
    if (state().m_strokeStyle && state().m_strokeStyle->isEquivalentRGBA(r, g, b, a))
        return;
    setStrokeStyle(CanvasStyle::createFromRGBAChannels(r, g, b, a));
}

void CanvasRenderingContext2D::setStrokeColor(float c, float m, float y, float k, float a)
{
    if (state().m_strokeStyle && state().m_strokeStyle->isEquivalentCMYKA(c, m, y, k, a))
        return;
    setStrokeStyle(CanvasStyle::createFromCMYKAChannels(c, m, y, k, a));
}

void CanvasRenderingContext2D::setFillColor(const String& color)
{
    if (color == state().m_unparsedFillColor)
        return;
    realizeSaves();
    setFillStyle(CanvasStyle::createFromString(color));
    modifiableState().m_unparsedFillColor = color;
}

void CanvasRenderingContext2D::setFillColor(float grayLevel)
{
    if (state().m_fillStyle && state().m_fillStyle->isEquivalentRGBA(grayLevel, grayLevel, grayLevel, 1.0f))
        return;
    setFillStyle(CanvasStyle::createFromGrayLevelWithAlpha(grayLevel, 1.0f));
}

void CanvasRenderingContext2D::setFillColor(const String& color, float alpha)
{
    setFillStyle(CanvasStyle::createFromStringWithOverrideAlpha(color, alpha));
}

void CanvasRenderingContext2D::setFillColor(float grayLevel, float alpha)
{
    if (state().m_fillStyle && state().m_fillStyle->isEquivalentRGBA(grayLevel, grayLevel, grayLevel, alpha))
        return;
    setFillStyle(CanvasStyle::createFromGrayLevelWithAlpha(grayLevel, alpha));
}

void CanvasRenderingContext2D::setFillColor(float r, float g, float b, float a)
{
    if (state().m_fillStyle && state().m_fillStyle->isEquivalentRGBA(r, g, b, a))
        return;
    setFillStyle(CanvasStyle::createFromRGBAChannels(r, g, b, a));
}

void CanvasRenderingContext2D::setFillColor(float c, float m, float y, float k, float a)
{
    if (state().m_fillStyle && state().m_fillStyle->isEquivalentCMYKA(c, m, y, k, a))
        return;
    setFillStyle(CanvasStyle::createFromCMYKAChannels(c, m, y, k, a));
}

void CanvasRenderingContext2D::beginPath()
{
    m_path.clear();
}

PassRefPtr<Path2D> CanvasRenderingContext2D::currentPath()
{
    return Path2D::create(m_path);
}

void CanvasRenderingContext2D::setCurrentPath(Path2D* path)
{
    if (!path)
        return;
    m_path = path->path();
}

static bool validateRectForCanvas(float& x, float& y, float& width, float& height)
{
    if (!std::isfinite(x) | !std::isfinite(y) | !std::isfinite(width) | !std::isfinite(height))
        return false;

    if (!width && !height)
        return false;

    if (width < 0) {
        width = -width;
        x -= width;
    }

    if (height < 0) {
        height = -height;
        y -= height;
    }

    return true;
}

static bool isFullCanvasCompositeMode(CompositeOperator op)
{
    // See 4.8.11.1.3 Compositing
    // CompositeSourceAtop and CompositeDestinationOut are not listed here as the platforms already
    // implement the specification's behavior.
    return op == CompositeSourceIn || op == CompositeSourceOut || op == CompositeDestinationIn || op == CompositeDestinationAtop;
}

static bool parseWinding(const String& windingRuleString, WindRule& windRule)
{
    if (windingRuleString == "nonzero")
        windRule = RULE_NONZERO;
    else if (windingRuleString == "evenodd")
        windRule = RULE_EVENODD;
    else
        return false;

    return true;
}

void CanvasRenderingContext2D::fillInternal(const Path& path, const String& windingRuleString)
{
    if (path.isEmpty()) {
        return;
    }
    GraphicsContext* c = drawingContext();
    if (!c) {
        return;
    }
    if (!state().m_invertibleCTM) {
        return;
    }
    FloatRect clipBounds;
    if (!drawingContext()->getTransformedClipBounds(&clipBounds)) {
        return;
    }

    // If gradient size is zero, then paint nothing.
    Gradient* gradient = c->fillGradient();
    if (gradient && gradient->isZeroSize()) {
        return;
    }

    WindRule windRule = c->fillRule();
    WindRule newWindRule = RULE_NONZERO;
    if (!parseWinding(windingRuleString, newWindRule)) {
        return;
    }
    c->setFillRule(newWindRule);

    if (isFullCanvasCompositeMode(state().m_globalComposite)) {
        fullCanvasCompositedFill(path);
        didDraw(clipBounds);
    } else if (state().m_globalComposite == CompositeCopy) {
        clearCanvas();
        c->fillPath(path);
        didDraw(clipBounds);
    } else {
        FloatRect dirtyRect;
        if (computeDirtyRect(path.boundingRect(), clipBounds, &dirtyRect)) {
            c->fillPath(path);
            didDraw(dirtyRect);
        }
    }

    c->setFillRule(windRule);
}

void CanvasRenderingContext2D::fill(const String& windingRuleString)
{
    fillInternal(m_path, windingRuleString);
}

void CanvasRenderingContext2D::fill(Path2D* domPath, ExceptionState& exceptionState)
{
    fill(domPath, "nonzero", exceptionState);
}

void CanvasRenderingContext2D::fill(Path2D* domPath, const String& windingRuleString, ExceptionState& exceptionState)
{
    if (!domPath) {
        exceptionState.throwDOMException(TypeMismatchError, ExceptionMessages::argumentNullOrIncorrectType(1, "Path"));
        return;
    }

    fillInternal(domPath->path(), windingRuleString);
}

void CanvasRenderingContext2D::strokeInternal(const Path& path)
{
    if (path.isEmpty()) {
        return;
    }
    GraphicsContext* c = drawingContext();
    if (!c) {
        return;
    }
    if (!state().m_invertibleCTM) {
        return;
    }

    // If gradient size is zero, then paint nothing.
    Gradient* gradient = c->strokeGradient();
    if (gradient && gradient->isZeroSize()) {
        return;
    }

    FloatRect bounds = path.boundingRect();
    inflateStrokeRect(bounds);
    FloatRect dirtyRect;
    if (computeDirtyRect(bounds, &dirtyRect)) {
        c->strokePath(path);
        didDraw(dirtyRect);
    }
}

void CanvasRenderingContext2D::stroke()
{
    strokeInternal(m_path);
}

void CanvasRenderingContext2D::stroke(Path2D* domPath, ExceptionState& exceptionState)
{
    if (!domPath) {
        exceptionState.throwDOMException(TypeMismatchError, ExceptionMessages::argumentNullOrIncorrectType(1, "Path"));
        return;
    }

    strokeInternal(domPath->path());
}

void CanvasRenderingContext2D::clipInternal(const Path& path, const String& windingRuleString)
{
    GraphicsContext* c = drawingContext();
    if (!c) {
        return;
    }
    if (!state().m_invertibleCTM) {
        return;
    }

    WindRule newWindRule = RULE_NONZERO;
    if (!parseWinding(windingRuleString, newWindRule)) {
        return;
    }

    realizeSaves();
    c->canvasClip(path, newWindRule);
}

void CanvasRenderingContext2D::clip(const String& windingRuleString)
{
    clipInternal(m_path, windingRuleString);
}

void CanvasRenderingContext2D::clip(Path2D* domPath, ExceptionState& exceptionState)
{
    clip(domPath, "nonzero", exceptionState);
}

void CanvasRenderingContext2D::clip(Path2D* domPath, const String& windingRuleString, ExceptionState& exceptionState)
{
    if (!domPath) {
        exceptionState.throwDOMException(TypeMismatchError, ExceptionMessages::argumentNullOrIncorrectType(1, "Path"));
        return;
    }

    clipInternal(domPath->path(), windingRuleString);
}

bool CanvasRenderingContext2D::isPointInPath(const float x, const float y, const String& windingRuleString)
{
    return isPointInPathInternal(m_path, x, y, windingRuleString);
}

bool CanvasRenderingContext2D::isPointInPath(Path2D* domPath, const float x, const float y, ExceptionState& exceptionState)
{
    return isPointInPath(domPath, x, y, "nonzero", exceptionState);
}

bool CanvasRenderingContext2D::isPointInPath(Path2D* domPath, const float x, const float y, const String& windingRuleString, ExceptionState& exceptionState)
{
    if (!domPath) {
        exceptionState.throwDOMException(TypeMismatchError, ExceptionMessages::argumentNullOrIncorrectType(1, "Path"));
        return false;
    }

    return isPointInPathInternal(domPath->path(), x, y, windingRuleString);
}

bool CanvasRenderingContext2D::isPointInPathInternal(const Path& path, const float x, const float y, const String& windingRuleString)
{
    GraphicsContext* c = drawingContext();
    if (!c)
        return false;
    if (!state().m_invertibleCTM)
        return false;

    FloatPoint point(x, y);
    AffineTransform ctm = state().m_transform;
    FloatPoint transformedPoint = ctm.inverse().mapPoint(point);
    if (!std::isfinite(transformedPoint.x()) || !std::isfinite(transformedPoint.y()))
        return false;

    WindRule windRule = RULE_NONZERO;
    if (!parseWinding(windingRuleString, windRule))
        return false;

    return path.contains(transformedPoint, windRule);
}

bool CanvasRenderingContext2D::isPointInStroke(const float x, const float y)
{
    return isPointInStrokeInternal(m_path, x, y);
}

bool CanvasRenderingContext2D::isPointInStroke(Path2D* domPath, const float x, const float y, ExceptionState& exceptionState)
{
    if (!domPath) {
        exceptionState.throwDOMException(TypeMismatchError, ExceptionMessages::argumentNullOrIncorrectType(1, "Path"));
        return false;
    }

    return isPointInStrokeInternal(domPath->path(), x, y);
}

bool CanvasRenderingContext2D::isPointInStrokeInternal(const Path& path, const float x, const float y)
{
    GraphicsContext* c = drawingContext();
    if (!c)
        return false;
    if (!state().m_invertibleCTM)
        return false;

    FloatPoint point(x, y);
    AffineTransform ctm = state().m_transform;
    FloatPoint transformedPoint = ctm.inverse().mapPoint(point);
    if (!std::isfinite(transformedPoint.x()) || !std::isfinite(transformedPoint.y()))
        return false;

    StrokeData strokeData;
    strokeData.setThickness(lineWidth());
    strokeData.setLineCap(getLineCap());
    strokeData.setLineJoin(getLineJoin());
    strokeData.setMiterLimit(miterLimit());
    strokeData.setLineDash(getLineDash(), lineDashOffset());
    return path.strokeContains(transformedPoint, strokeData);
}

void CanvasRenderingContext2D::clearRect(float x, float y, float width, float height)
{
    if (!validateRectForCanvas(x, y, width, height))
        return;
    GraphicsContext* context = drawingContext();
    if (!context)
        return;
    if (!state().m_invertibleCTM)
        return;
    FloatRect rect(x, y, width, height);

    FloatRect dirtyRect;
    if (!computeDirtyRect(rect, &dirtyRect))
        return;

    bool saved = false;
    if (shouldDrawShadows()) {
        context->save();
        saved = true;
        context->clearShadow();
    }
    if (state().m_globalAlpha != 1) {
        if (!saved) {
            context->save();
            saved = true;
        }
        context->setAlphaAsFloat(1);
    }
    if (state().m_globalComposite != CompositeSourceOver) {
        if (!saved) {
            context->save();
            saved = true;
        }
        context->setCompositeOperation(CompositeSourceOver);
    }
    context->clearRect(rect);
    if (saved)
        context->restore();

    didDraw(dirtyRect);
}

void CanvasRenderingContext2D::fillRect(float x, float y, float width, float height)
{
    if (!validateRectForCanvas(x, y, width, height))
        return;

    GraphicsContext* c = drawingContext();
    if (!c)
        return;
    if (!state().m_invertibleCTM)
        return;
    FloatRect clipBounds;
    if (!drawingContext()->getTransformedClipBounds(&clipBounds))
        return;

    // from the HTML5 Canvas spec:
    // If x0 = x1 and y0 = y1, then the linear gradient must paint nothing
    // If x0 = x1 and y0 = y1 and r0 = r1, then the radial gradient must paint nothing
    Gradient* gradient = c->fillGradient();
    if (gradient && gradient->isZeroSize())
        return;

    FloatRect rect(x, y, width, height);
    if (rectContainsTransformedRect(rect, clipBounds)) {
        c->fillRect(rect);
        didDraw(clipBounds);
    } else if (isFullCanvasCompositeMode(state().m_globalComposite)) {
        fullCanvasCompositedFill(rect);
        didDraw(clipBounds);
    } else if (state().m_globalComposite == CompositeCopy) {
        clearCanvas();
        c->fillRect(rect);
        didDraw(clipBounds);
    } else {
        FloatRect dirtyRect;
        if (computeDirtyRect(rect, clipBounds, &dirtyRect)) {
            c->fillRect(rect);
            didDraw(dirtyRect);
        }
    }
}

void CanvasRenderingContext2D::strokeRect(float x, float y, float width, float height)
{
    if (!validateRectForCanvas(x, y, width, height))
        return;

    if (!(state().m_lineWidth >= 0))
        return;

    GraphicsContext* c = drawingContext();
    if (!c)
        return;
    if (!state().m_invertibleCTM)
        return;

    // If gradient size is zero, then paint nothing.
    Gradient* gradient = c->strokeGradient();
    if (gradient && gradient->isZeroSize())
        return;

    FloatRect rect(x, y, width, height);

    FloatRect boundingRect = rect;
    boundingRect.inflate(state().m_lineWidth / 2);
    FloatRect dirtyRect;
    if (computeDirtyRect(boundingRect, &dirtyRect)) {
        c->strokeRect(rect, state().m_lineWidth);
        didDraw(dirtyRect);
    }
}

void CanvasRenderingContext2D::setShadow(float width, float height, float blur)
{
    setShadow(FloatSize(width, height), blur, Color::transparent);
}

void CanvasRenderingContext2D::setShadow(float width, float height, float blur, const String& color)
{
    RGBA32 rgba;
    if (!parseColorOrCurrentColor(rgba, color, canvas()))
        return;
    setShadow(FloatSize(width, height), blur, rgba);
}

void CanvasRenderingContext2D::setShadow(float width, float height, float blur, float grayLevel)
{
    setShadow(FloatSize(width, height), blur, makeRGBA32FromFloats(grayLevel, grayLevel, grayLevel, 1));
}

void CanvasRenderingContext2D::setShadow(float width, float height, float blur, const String& color, float alpha)
{
    RGBA32 rgba;
    if (!parseColorOrCurrentColor(rgba, color, canvas()))
        return;
    setShadow(FloatSize(width, height), blur, colorWithOverrideAlpha(rgba, alpha));
}

void CanvasRenderingContext2D::setShadow(float width, float height, float blur, float grayLevel, float alpha)
{
    setShadow(FloatSize(width, height), blur, makeRGBA32FromFloats(grayLevel, grayLevel, grayLevel, alpha));
}

void CanvasRenderingContext2D::setShadow(float width, float height, float blur, float r, float g, float b, float a)
{
    setShadow(FloatSize(width, height), blur, makeRGBA32FromFloats(r, g, b, a));
}

void CanvasRenderingContext2D::setShadow(float width, float height, float blur, float c, float m, float y, float k, float a)
{
    setShadow(FloatSize(width, height), blur, makeRGBAFromCMYKA(c, m, y, k, a));
}

void CanvasRenderingContext2D::clearShadow()
{
    setShadow(FloatSize(), 0, Color::transparent);
}

void CanvasRenderingContext2D::setShadow(const FloatSize& offset, float blur, RGBA32 color)
{
    if (state().m_shadowOffset == offset && state().m_shadowBlur == blur && state().m_shadowColor == color)
        return;
    bool wasDrawingShadows = shouldDrawShadows();
    realizeSaves();
    modifiableState().m_shadowOffset = offset;
    modifiableState().m_shadowBlur = blur;
    modifiableState().m_shadowColor = color;
    if (!wasDrawingShadows && !shouldDrawShadows())
        return;
    applyShadow();
}

void CanvasRenderingContext2D::applyShadow()
{
    GraphicsContext* c = drawingContext();
    if (!c)
        return;

    if (shouldDrawShadows()) {
        c->setShadow(state().m_shadowOffset, state().m_shadowBlur, state().m_shadowColor,
            DrawLooper::ShadowIgnoresTransforms);
    } else {
        c->clearShadow();
    }
}

bool CanvasRenderingContext2D::shouldDrawShadows() const
{
    return alphaChannel(state().m_shadowColor) && (state().m_shadowBlur || !state().m_shadowOffset.isZero());
}

enum ImageSizeType {
    ImageSizeAfterDevicePixelRatio,
    ImageSizeBeforeDevicePixelRatio
};

static inline FloatRect normalizeRect(const FloatRect& rect)
{
    return FloatRect(min(rect.x(), rect.maxX()),
        min(rect.y(), rect.maxY()),
        max(rect.width(), -rect.width()),
        max(rect.height(), -rect.height()));
}

static inline void clipRectsToImageRect(const FloatRect& imageRect, FloatRect* srcRect, FloatRect* dstRect)
{
    if (imageRect.contains(*srcRect))
        return;

    // Compute the src to dst transform
    FloatSize scale(dstRect->size().width() / srcRect->size().width(), dstRect->size().height() / srcRect->size().height());
    FloatPoint scaledSrcLocation = srcRect->location();
    scaledSrcLocation.scale(scale.width(), scale.height());
    FloatSize offset = dstRect->location() - scaledSrcLocation;

    srcRect->intersect(imageRect);

    // To clip the destination rectangle in the same proportion, transform the clipped src rect
    *dstRect = *srcRect;
    dstRect->scale(scale.width(), scale.height());
    dstRect->move(offset);
}

static bool checkImageSource(CanvasImageSource* imageSource, ExceptionState& exceptionState)
{
    if (!imageSource) {
        // FIXME: Message should mention ImageBitmap once that feature ships.
        exceptionState.throwDOMException(TypeMismatchError, ExceptionMessages::argumentNullOrIncorrectType(1, String("HTMLImageElement, HTMLCanvasElement or HTMLVideoElement")));
        return false;
    }
    return true;
}

void CanvasRenderingContext2D::drawImage(CanvasImageSource* imageSource, float x, float y, ExceptionState& exceptionState)
{
    if (!checkImageSource(imageSource, exceptionState))
        return;
    FloatSize destRectSize = imageSource->defaultDestinationSize();
    drawImage(imageSource, x, y, destRectSize.width(), destRectSize.height(), exceptionState);
}

void CanvasRenderingContext2D::drawImage(CanvasImageSource* imageSource,
    float x, float y, float width, float height, ExceptionState& exceptionState)
{
    if (!checkImageSource(imageSource, exceptionState))
        return;
    FloatSize sourceRectSize = imageSource->sourceSize();
    drawImage(imageSource, 0, 0, sourceRectSize.width(), sourceRectSize.height(), x, y, width, height, exceptionState);
}

void CanvasRenderingContext2D::drawImage(CanvasImageSource* imageSource,
    float sx, float sy, float sw, float sh,
    float dx, float dy, float dw, float dh, ExceptionState& exceptionState)
{
    GraphicsContext* c = drawingContext(); // Do not exit yet if !c because we may need to throw exceptions first
    CompositeOperator op = c ? c->compositeOperation() : CompositeSourceOver;
    blink::WebBlendMode blendMode = c ? c->blendModeOperation() : blink::WebBlendModeNormal;
    drawImageInternal(imageSource, sx, sy, sw, sh, dx, dy, dw, dh, exceptionState, op, blendMode);
}

void CanvasRenderingContext2D::drawImageInternal(CanvasImageSource* imageSource,
    float sx, float sy, float sw, float sh,
    float dx, float dy, float dw, float dh, ExceptionState& exceptionState,
    CompositeOperator op, blink::WebBlendMode blendMode)
{
    if (!checkImageSource(imageSource, exceptionState))
        return;

    RefPtr<Image> image;
    SourceImageStatus sourceImageStatus;
    if (!imageSource->isVideoElement()) {
        SourceImageMode mode = canvas() == imageSource ? CopySourceImageIfVolatile : DontCopySourceImage; // Thunking for ==
        image = imageSource->getSourceImageForCanvas(mode, &sourceImageStatus);
        if (sourceImageStatus == UndecodableSourceImageStatus)
            exceptionState.throwDOMException(InvalidStateError, "The HTMLImageElement provided is in the 'broken' state.");
        if (!image || !image->width() || !image->height())
            return;
    }

    GraphicsContext* c = drawingContext();
    if (!c)
        return;

    if (!state().m_invertibleCTM)
        return;

    if (!std::isfinite(dx) || !std::isfinite(dy) || !std::isfinite(dw) || !std::isfinite(dh)
        || !std::isfinite(sx) || !std::isfinite(sy) || !std::isfinite(sw) || !std::isfinite(sh)
        || !dw || !dh || !sw || !sh)
        return;

    FloatRect clipBounds;
    if (!c->getTransformedClipBounds(&clipBounds))
        return;

    FloatRect srcRect = normalizeRect(FloatRect(sx, sy, sw, sh));
    FloatRect dstRect = normalizeRect(FloatRect(dx, dy, dw, dh));

    clipRectsToImageRect(FloatRect(FloatPoint(), imageSource->sourceSize()), &srcRect, &dstRect);

    imageSource->adjustDrawRects(&srcRect, &dstRect);

    if (srcRect.isEmpty())
        return;

    FloatRect dirtyRect = clipBounds;
    if (imageSource->isVideoElement()) {
        drawVideo(static_cast<HTMLVideoElement*>(imageSource), srcRect, dstRect);
        computeDirtyRect(dstRect, clipBounds, &dirtyRect);
    } else {
        if (rectContainsTransformedRect(dstRect, clipBounds)) {
            c->drawImage(image.get(), dstRect, srcRect, op, blendMode);
        } else if (isFullCanvasCompositeMode(op)) {
            fullCanvasCompositedDrawImage(image.get(), dstRect, srcRect, op);
        } else if (op == CompositeCopy) {
            clearCanvas();
            c->drawImage(image.get(), dstRect, srcRect, op, blendMode);
        } else {
            FloatRect dirtyRect;
            computeDirtyRect(dstRect, clipBounds, &dirtyRect);
            c->drawImage(image.get(), dstRect, srcRect, op, blendMode);
        }

        if (sourceImageStatus == ExternalSourceImageStatus && isAccelerated() && canvas()->buffer())
            canvas()->buffer()->flush();
    }

    if (canvas()->originClean() && imageSource->wouldTaintOrigin(canvas()->securityOrigin()))
        canvas()->setOriginTainted();

    didDraw(dirtyRect);
}

void CanvasRenderingContext2D::drawVideo(HTMLVideoElement* video, FloatRect srcRect, FloatRect dstRect)
{
    GraphicsContext* c = drawingContext();
    GraphicsContextStateSaver stateSaver(*c);
    c->clip(dstRect);
    c->translate(dstRect.x(), dstRect.y());
    c->scale(FloatSize(dstRect.width() / srcRect.width(), dstRect.height() / srcRect.height()));
    c->translate(-srcRect.x(), -srcRect.y());
    video->paintCurrentFrameInContext(c, IntRect(IntPoint(), IntSize(video->videoWidth(), video->videoHeight())));
    stateSaver.restore();
}

void CanvasRenderingContext2D::drawImageFromRect(HTMLImageElement* image,
    float sx, float sy, float sw, float sh,
    float dx, float dy, float dw, float dh,
    const String& compositeOperation)
{
    CompositeOperator op;
    blink::WebBlendMode blendOp = blink::WebBlendModeNormal;
    if (!parseCompositeAndBlendOperator(compositeOperation, op, blendOp) || blendOp != blink::WebBlendModeNormal)
        op = CompositeSourceOver;

    drawImageInternal(image, sx, sy, sw, sh, dx, dy, dw, dh, IGNORE_EXCEPTION, op, blendOp);
}

void CanvasRenderingContext2D::setAlpha(float alpha)
{
    setGlobalAlpha(alpha);
}

void CanvasRenderingContext2D::setCompositeOperation(const String& operation)
{
    setGlobalCompositeOperation(operation);
}

void CanvasRenderingContext2D::clearCanvas()
{
    FloatRect canvasRect(0, 0, canvas()->width(), canvas()->height());
    GraphicsContext* c = drawingContext();
    if (!c)
        return;

    c->save();
    c->setCTM(canvas()->baseTransform());
    c->clearRect(canvasRect);
    c->restore();
}

bool CanvasRenderingContext2D::rectContainsTransformedRect(const FloatRect& rect, const FloatRect& transformedRect) const
{
    FloatQuad quad(rect);
    FloatQuad transformedQuad(transformedRect);
    return state().m_transform.mapQuad(quad).containsQuad(transformedQuad);
}

static void drawImageToContext(Image* image, GraphicsContext* context, const FloatRect& dest, const FloatRect& src, CompositeOperator op)
{
    context->drawImage(image, dest, src, op);
}

template<class T> void  CanvasRenderingContext2D::fullCanvasCompositedDrawImage(T* image, const FloatRect& dest, const FloatRect& src, CompositeOperator op)
{
    ASSERT(isFullCanvasCompositeMode(op));

    drawingContext()->beginLayer(1, op);
    drawImageToContext(image, drawingContext(), dest, src, CompositeSourceOver);
    drawingContext()->endLayer();
}

static void fillPrimitive(const FloatRect& rect, GraphicsContext* context)
{
    context->fillRect(rect);
}

static void fillPrimitive(const Path& path, GraphicsContext* context)
{
    context->fillPath(path);
}

template<class T> void CanvasRenderingContext2D::fullCanvasCompositedFill(const T& area)
{
    ASSERT(isFullCanvasCompositeMode(state().m_globalComposite));

    GraphicsContext* c = drawingContext();
    ASSERT(c);
    c->beginLayer(1, state().m_globalComposite);
    CompositeOperator previousOperator = c->compositeOperation();
    c->setCompositeOperation(CompositeSourceOver);
    fillPrimitive(area, c);
    c->setCompositeOperation(previousOperator);
    c->endLayer();
}

PassRefPtr<CanvasGradient> CanvasRenderingContext2D::createLinearGradient(float x0, float y0, float x1, float y1, ExceptionState& exceptionState)
{
    if (!std::isfinite(x0))
        exceptionState.throwDOMException(NotSupportedError, ExceptionMessages::notAFiniteNumber(x0, "x0"));
    else if (!std::isfinite(y0))
        exceptionState.throwDOMException(NotSupportedError, ExceptionMessages::notAFiniteNumber(y0, "y0"));
    else if (!std::isfinite(x1))
        exceptionState.throwDOMException(NotSupportedError, ExceptionMessages::notAFiniteNumber(x1, "x1"));
    else if (!std::isfinite(y1))
        exceptionState.throwDOMException(NotSupportedError, ExceptionMessages::notAFiniteNumber(y1, "y1"));

    if (exceptionState.hadException())
        return nullptr;

    RefPtr<CanvasGradient> gradient = CanvasGradient::create(FloatPoint(x0, y0), FloatPoint(x1, y1));
    return gradient.release();
}

PassRefPtr<CanvasGradient> CanvasRenderingContext2D::createRadialGradient(float x0, float y0, float r0, float x1, float y1, float r1, ExceptionState& exceptionState)
{
    if (!std::isfinite(x0))
        exceptionState.throwDOMException(NotSupportedError, ExceptionMessages::notAFiniteNumber(x0, "x0"));
    else if (!std::isfinite(y0))
        exceptionState.throwDOMException(NotSupportedError, ExceptionMessages::notAFiniteNumber(y0, "y0"));
    else if (!std::isfinite(r0))
        exceptionState.throwDOMException(NotSupportedError, ExceptionMessages::notAFiniteNumber(r0, "r0"));
    else if (!std::isfinite(x1))
        exceptionState.throwDOMException(NotSupportedError, ExceptionMessages::notAFiniteNumber(x1, "x1"));
    else if (!std::isfinite(y1))
        exceptionState.throwDOMException(NotSupportedError, ExceptionMessages::notAFiniteNumber(y1, "y1"));
    else if (!std::isfinite(r1))
        exceptionState.throwDOMException(NotSupportedError, ExceptionMessages::notAFiniteNumber(r1, "r1"));
    else if (r0 < 0 || r1 < 0)
        exceptionState.throwDOMException(IndexSizeError, String::format("The %s provided is less than 0.", r0 < 0 ? "r0" : "r1"));

    if (exceptionState.hadException())
        return nullptr;

    RefPtr<CanvasGradient> gradient = CanvasGradient::create(FloatPoint(x0, y0), r0, FloatPoint(x1, y1), r1);
    return gradient.release();
}

PassRefPtr<CanvasPattern> CanvasRenderingContext2D::createPattern(CanvasImageSource* imageSource,
    const String& repetitionType, ExceptionState& exceptionState)
{
    if (!checkImageSource(imageSource, exceptionState))
        return nullptr;
    bool repeatX, repeatY;
    CanvasPattern::parseRepetitionType(repetitionType, repeatX, repeatY, exceptionState);
    if (exceptionState.hadException())
        return nullptr;

    SourceImageStatus status;
    RefPtr<Image> imageForRendering = imageSource->getSourceImageForCanvas(CopySourceImageIfVolatile, &status);

    switch (status) {
    case NormalSourceImageStatus:
        break;
    case ZeroSizeCanvasSourceImageStatus:
        exceptionState.throwDOMException(InvalidStateError, String::format("The canvas %s is 0.", imageSource->sourceSize().width() ? "height" : "width"));
        return nullptr;
    case UndecodableSourceImageStatus:
        exceptionState.throwDOMException(InvalidStateError, "Source image is in the 'broken' state.");
        return nullptr;
    case InvalidSourceImageStatus:
        imageForRendering = Image::nullImage();
        break;
    case IncompleteSourceImageStatus:
        return nullptr;
    default:
    case ExternalSourceImageStatus: // should not happen when mode is CopySourceImageIfVolatile
        ASSERT_NOT_REACHED();
        return nullptr;
    }
    ASSERT(imageForRendering);

    return CanvasPattern::create(imageForRendering.release(), repeatX, repeatY, !imageSource->wouldTaintOrigin(canvas()->securityOrigin()));
}

bool CanvasRenderingContext2D::computeDirtyRect(const FloatRect& localRect, FloatRect* dirtyRect)
{
    FloatRect clipBounds;
    if (!drawingContext()->getTransformedClipBounds(&clipBounds))
        return false;
    return computeDirtyRect(localRect, clipBounds, dirtyRect);
}

bool CanvasRenderingContext2D::computeDirtyRect(const FloatRect& localRect, const FloatRect& transformedClipBounds, FloatRect* dirtyRect)
{
    FloatRect canvasRect = state().m_transform.mapRect(localRect);

    if (alphaChannel(state().m_shadowColor)) {
        FloatRect shadowRect(canvasRect);
        shadowRect.move(state().m_shadowOffset);
        shadowRect.inflate(state().m_shadowBlur);
        canvasRect.unite(shadowRect);
    }

    canvasRect.intersect(transformedClipBounds);
    if (canvasRect.isEmpty())
        return false;

    if (dirtyRect)
        *dirtyRect = canvasRect;

    return true;
}

void CanvasRenderingContext2D::didDraw(const FloatRect& dirtyRect)
{
    if (dirtyRect.isEmpty())
        return;

    // If we are drawing to hardware and we have a composited layer, just call contentChanged().
    if (isAccelerated()) {
        RenderBox* renderBox = canvas()->renderBox();
        if (renderBox && renderBox->hasAcceleratedCompositing()) {
            renderBox->contentChanged(CanvasPixelsChanged);
            canvas()->clearCopiedImage();
            canvas()->notifyObserversCanvasChanged(dirtyRect);
            return;
        }
    }

    canvas()->didDraw(dirtyRect);
}

GraphicsContext* CanvasRenderingContext2D::drawingContext() const
{
    return canvas()->drawingContext();
}

static PassRefPtr<ImageData> createEmptyImageData(const IntSize& size)
{
    if (RefPtr<ImageData> data = ImageData::create(size)) {
        data->data()->zeroFill();
        return data.release();
    }

    return nullptr;
}

PassRefPtr<ImageData> CanvasRenderingContext2D::createImageData(PassRefPtr<ImageData> imageData, ExceptionState& exceptionState) const
{
    if (!imageData) {
        exceptionState.throwDOMException(NotSupportedError, ExceptionMessages::argumentNullOrIncorrectType(1, "ImageData"));
        return nullptr;
    }

    return createEmptyImageData(imageData->size());
}

PassRefPtr<ImageData> CanvasRenderingContext2D::createImageData(float sw, float sh, ExceptionState& exceptionState) const
{
    if (!sw || !sh)
        exceptionState.throwDOMException(IndexSizeError, String::format("The source %s is 0.", sw ? "height" : "width"));
    else if (!std::isfinite(sw))
        exceptionState.throwDOMException(NotSupportedError, ExceptionMessages::notAFiniteNumber(sw, "source width"));
    else if (!std::isfinite(sh))
        exceptionState.throwDOMException(NotSupportedError, ExceptionMessages::notAFiniteNumber(sh, "source height"));

    if (exceptionState.hadException())
        return nullptr;

    FloatSize logicalSize(fabs(sw), fabs(sh));
    if (!logicalSize.isExpressibleAsIntSize())
        return nullptr;

    IntSize size = expandedIntSize(logicalSize);
    if (size.width() < 1)
        size.setWidth(1);
    if (size.height() < 1)
        size.setHeight(1);

    return createEmptyImageData(size);
}

PassRefPtr<ImageData> CanvasRenderingContext2D::webkitGetImageDataHD(float sx, float sy, float sw, float sh, ExceptionState& exceptionState) const
{
    return getImageData(sx, sy, sw, sh, exceptionState);
}

PassRefPtr<ImageData> CanvasRenderingContext2D::getImageData(float sx, float sy, float sw, float sh, ExceptionState& exceptionState) const
{
    if (!canvas()->originClean())
        exceptionState.throwSecurityError("The canvas has been tainted by cross-origin data.");
    else if (!sw || !sh)
        exceptionState.throwDOMException(IndexSizeError, String::format("The source %s is 0.", sw ? "height" : "width"));
    else if (!std::isfinite(sx))
        exceptionState.throwDOMException(NotSupportedError, ExceptionMessages::notAFiniteNumber(sx, "source X"));
    else if (!std::isfinite(sy))
        exceptionState.throwDOMException(NotSupportedError, ExceptionMessages::notAFiniteNumber(sy, "source Y"));
    else if (!std::isfinite(sw))
        exceptionState.throwDOMException(NotSupportedError, ExceptionMessages::notAFiniteNumber(sw, "source width"));
    else if (!std::isfinite(sh))
        exceptionState.throwDOMException(NotSupportedError, ExceptionMessages::notAFiniteNumber(sh, "source height"));

    if (exceptionState.hadException())
        return nullptr;

    if (sw < 0) {
        sx += sw;
        sw = -sw;
    }
    if (sh < 0) {
        sy += sh;
        sh = -sh;
    }

    FloatRect logicalRect(sx, sy, sw, sh);
    if (logicalRect.width() < 1)
        logicalRect.setWidth(1);
    if (logicalRect.height() < 1)
        logicalRect.setHeight(1);
    if (!logicalRect.isExpressibleAsIntRect())
        return nullptr;

    IntRect imageDataRect = enclosingIntRect(logicalRect);
    ImageBuffer* buffer = canvas()->buffer();
    if (!buffer)
        return createEmptyImageData(imageDataRect.size());

    RefPtr<Uint8ClampedArray> byteArray = buffer->getUnmultipliedImageData(imageDataRect);
    if (!byteArray)
        return nullptr;

    return ImageData::create(imageDataRect.size(), byteArray.release());
}

void CanvasRenderingContext2D::putImageData(ImageData* data, float dx, float dy, ExceptionState& exceptionState)
{
    if (!data) {
        exceptionState.throwDOMException(TypeMismatchError, ExceptionMessages::argumentNullOrIncorrectType(1, "ImageData"));
        return;
    }
    putImageData(data, dx, dy, 0, 0, data->width(), data->height(), exceptionState);
}

void CanvasRenderingContext2D::putImageData(ImageData* data, float dx, float dy, float dirtyX, float dirtyY,
    float dirtyWidth, float dirtyHeight, ExceptionState& exceptionState)
{
    if (!data)
        exceptionState.throwDOMException(TypeMismatchError, ExceptionMessages::argumentNullOrIncorrectType(1, "ImageData"));
    else if (!std::isfinite(dx))
        exceptionState.throwDOMException(NotSupportedError, ExceptionMessages::notAFiniteNumber(dx, "dx"));
    else if (!std::isfinite(dy))
        exceptionState.throwDOMException(NotSupportedError, ExceptionMessages::notAFiniteNumber(dy, "dy"));
    else if (!std::isfinite(dirtyX))
        exceptionState.throwDOMException(NotSupportedError, ExceptionMessages::notAFiniteNumber(dirtyX, "dirtyX"));
    else if (!std::isfinite(dirtyY))
        exceptionState.throwDOMException(NotSupportedError, ExceptionMessages::notAFiniteNumber(dirtyY, "dirtyY"));
    else if (!std::isfinite(dirtyWidth))
        exceptionState.throwDOMException(NotSupportedError, ExceptionMessages::notAFiniteNumber(dirtyWidth, "dirtyWidth"));
    else if (!std::isfinite(dirtyHeight))
        exceptionState.throwDOMException(NotSupportedError, ExceptionMessages::notAFiniteNumber(dirtyHeight, "dirtyHeight"));

    if (exceptionState.hadException())
        return;

    ImageBuffer* buffer = canvas()->buffer();
    if (!buffer)
        return;

    if (dirtyWidth < 0) {
        dirtyX += dirtyWidth;
        dirtyWidth = -dirtyWidth;
    }

    if (dirtyHeight < 0) {
        dirtyY += dirtyHeight;
        dirtyHeight = -dirtyHeight;
    }

    FloatRect clipRect(dirtyX, dirtyY, dirtyWidth, dirtyHeight);
    clipRect.intersect(IntRect(0, 0, data->width(), data->height()));
    IntSize destOffset(static_cast<int>(dx), static_cast<int>(dy));
    IntRect destRect = enclosingIntRect(clipRect);
    destRect.move(destOffset);
    destRect.intersect(IntRect(IntPoint(), buffer->size()));
    if (destRect.isEmpty())
        return;
    IntRect sourceRect(destRect);
    sourceRect.move(-destOffset);

    buffer->putByteArray(Unmultiplied, data->data(), IntSize(data->width(), data->height()), sourceRect, IntPoint(destOffset));

    didDraw(destRect);
}

String CanvasRenderingContext2D::font() const
{
    if (!state().m_realizedFont)
        return defaultFont;

    StringBuilder serializedFont;
    const FontDescription& fontDescription = state().m_font.fontDescription();

    if (fontDescription.style() == FontStyleItalic)
        serializedFont.appendLiteral("italic ");
    if (fontDescription.weight() == FontWeightBold)
        serializedFont.appendLiteral("bold ");
    if (fontDescription.variant() == FontVariantSmallCaps)
        serializedFont.appendLiteral("small-caps ");

    serializedFont.appendNumber(fontDescription.computedPixelSize());
    serializedFont.appendLiteral("px");

    const FontFamily& firstFontFamily = fontDescription.family();
    for (const FontFamily* fontFamily = &firstFontFamily; fontFamily; fontFamily = fontFamily->next()) {
        if (fontFamily != &firstFontFamily)
            serializedFont.append(',');

        // FIXME: We should append family directly to serializedFont rather than building a temporary string.
        String family = fontFamily->family();
        if (family.startsWith("-webkit-"))
            family = family.substring(8);
        if (family.contains(' '))
            family = "\"" + family + "\"";

        serializedFont.append(' ');
        serializedFont.append(family);
    }

    return serializedFont.toString();
}

void CanvasRenderingContext2D::setFont(const String& newFont)
{
    // The style resolution required for rendering text is not available in frame-less documents.
    if (!canvas()->document().frame())
        return;

    MutableStylePropertyMap::iterator i = m_fetchedFonts.find(newFont);
    RefPtr<MutableStylePropertySet> parsedStyle = i != m_fetchedFonts.end() ? i->value : nullptr;

    if (!parsedStyle) {
        parsedStyle = MutableStylePropertySet::create();
        CSSParserMode mode = m_usesCSSCompatibilityParseMode ? HTMLQuirksMode : HTMLStandardMode;
        BisonCSSParser::parseValue(parsedStyle.get(), CSSPropertyFont, newFont, true, mode, 0);
        m_fetchedFonts.add(newFont, parsedStyle);
    }
    if (parsedStyle->isEmpty())
        return;

    String fontValue = parsedStyle->getPropertyValue(CSSPropertyFont);

    // According to http://lists.w3.org/Archives/Public/public-html/2009Jul/0947.html,
    // the "inherit" and "initial" values must be ignored.
    if (fontValue == "inherit" || fontValue == "initial")
        return;

    // The parse succeeded.
    String newFontSafeCopy(newFont); // Create a string copy since newFont can be deleted inside realizeSaves.
    realizeSaves();
    modifiableState().m_unparsedFont = newFontSafeCopy;

    // Map the <canvas> font into the text style. If the font uses keywords like larger/smaller, these will work
    // relative to the canvas.
    RefPtr<RenderStyle> newStyle = RenderStyle::create();
    if (RenderStyle* computedStyle = canvas()->computedStyle())
        newStyle->setFontDescription(computedStyle->fontDescription());
    else {
        FontFamily fontFamily;
        fontFamily.setFamily(defaultFontFamily);

        FontDescription defaultFontDescription;
        defaultFontDescription.setFamily(fontFamily);
        defaultFontDescription.setSpecifiedSize(defaultFontSize);
        defaultFontDescription.setComputedSize(defaultFontSize);

        newStyle->setFontDescription(defaultFontDescription);
    }

    newStyle->font().update(newStyle->font().fontSelector());

    // Now map the font property longhands into the style.
    CSSPropertyValue properties[] = {
        CSSPropertyValue(CSSPropertyFontFamily, *parsedStyle),
        CSSPropertyValue(CSSPropertyFontStyle, *parsedStyle),
        CSSPropertyValue(CSSPropertyFontVariant, *parsedStyle),
        CSSPropertyValue(CSSPropertyFontWeight, *parsedStyle),
        CSSPropertyValue(CSSPropertyFontSize, *parsedStyle),
        CSSPropertyValue(CSSPropertyLineHeight, *parsedStyle),
    };

    StyleResolver& styleResolver = canvas()->document().ensureStyleResolver();
    styleResolver.applyPropertiesToStyle(properties, WTF_ARRAY_LENGTH(properties), newStyle.get());

    if (state().m_realizedFont)
        static_cast<CSSFontSelector*>(state().m_font.fontSelector())->unregisterForInvalidationCallbacks(&modifiableState());
    modifiableState().m_font = newStyle->font();
    modifiableState().m_font.update(canvas()->document().styleEngine()->fontSelector());
    modifiableState().m_realizedFont = true;
    canvas()->document().styleEngine()->fontSelector()->registerForInvalidationCallbacks(&modifiableState());
}

String CanvasRenderingContext2D::textAlign() const
{
    return textAlignName(state().m_textAlign);
}

void CanvasRenderingContext2D::setTextAlign(const String& s)
{
    TextAlign align;
    if (!parseTextAlign(s, align))
        return;
    if (state().m_textAlign == align)
        return;
    realizeSaves();
    modifiableState().m_textAlign = align;
}

String CanvasRenderingContext2D::textBaseline() const
{
    return textBaselineName(state().m_textBaseline);
}

void CanvasRenderingContext2D::setTextBaseline(const String& s)
{
    TextBaseline baseline;
    if (!parseTextBaseline(s, baseline))
        return;
    if (state().m_textBaseline == baseline)
        return;
    realizeSaves();
    modifiableState().m_textBaseline = baseline;
}

void CanvasRenderingContext2D::fillText(const String& text, float x, float y)
{
    drawTextInternal(text, x, y, true);
}

void CanvasRenderingContext2D::fillText(const String& text, float x, float y, float maxWidth)
{
    drawTextInternal(text, x, y, true, maxWidth, true);
}

void CanvasRenderingContext2D::strokeText(const String& text, float x, float y)
{
    drawTextInternal(text, x, y, false);
}

void CanvasRenderingContext2D::strokeText(const String& text, float x, float y, float maxWidth)
{
    drawTextInternal(text, x, y, false, maxWidth, true);
}

PassRefPtr<TextMetrics> CanvasRenderingContext2D::measureText(const String& text)
{
    RefPtr<TextMetrics> metrics = TextMetrics::create();

    // The style resolution required for rendering text is not available in frame-less documents.
    if (!canvas()->document().frame())
        return metrics.release();

    FontCachePurgePreventer fontCachePurgePreventer;
    canvas()->document().updateStyleIfNeeded();
    const Font& font = accessFont();
    const TextRun textRun(text);
    FloatRect textBounds = font.selectionRectForText(textRun, FloatPoint(), font.fontDescription().computedSize(), 0, -1, true);

    // x direction
    metrics->setWidth(font.width(textRun));
    metrics->setActualBoundingBoxLeft(-textBounds.x());
    metrics->setActualBoundingBoxRight(textBounds.maxX());

    // y direction
    const FontMetrics& fontMetrics = font.fontMetrics();
    const float ascent = fontMetrics.floatAscent();
    const float descent = fontMetrics.floatDescent();
    const float baselineY = getFontBaseline(fontMetrics);

    metrics->setFontBoundingBoxAscent(ascent - baselineY);
    metrics->setFontBoundingBoxDescent(descent + baselineY);
    metrics->setActualBoundingBoxAscent(-textBounds.y() - baselineY);
    metrics->setActualBoundingBoxDescent(textBounds.maxY() + baselineY);

    // Note : top/bottom and ascend/descend are currently the same, so there's no difference
    //        between the EM box's top and bottom and the font's ascend and descend
    metrics->setEmHeightAscent(0);
    metrics->setEmHeightDescent(0);

    metrics->setHangingBaseline(-0.8f * ascent + baselineY);
    metrics->setAlphabeticBaseline(baselineY);
    metrics->setIdeographicBaseline(descent + baselineY);
    return metrics.release();
}

static void replaceCharacterInString(String& text, WTF::CharacterMatchFunctionPtr matchFunction, const String& replacement)
{
    const size_t replacementLength = replacement.length();
    size_t index = 0;
    while ((index = text.find(matchFunction, index)) != kNotFound) {
        text.replace(index, 1, replacement);
        index += replacementLength;
    }
}

void CanvasRenderingContext2D::drawTextInternal(const String& text, float x, float y, bool fill, float maxWidth, bool useMaxWidth)
{
    // The style resolution required for rendering text is not available in frame-less documents.
    if (!canvas()->document().frame())
        return;

    // accessFont needs the style to be up to date, but updating style can cause script to run,
    // (e.g. due to autofocus) which can free the GraphicsContext, so update style before grabbing
    // the GraphicsContext.
    canvas()->document().updateStyleIfNeeded();

    GraphicsContext* c = drawingContext();
    if (!c)
        return;
    if (!state().m_invertibleCTM)
        return;
    if (!std::isfinite(x) | !std::isfinite(y))
        return;
    if (useMaxWidth && (!std::isfinite(maxWidth) || maxWidth <= 0))
        return;

    // If gradient size is zero, then paint nothing.
    Gradient* gradient = c->strokeGradient();
    if (!fill && gradient && gradient->isZeroSize())
        return;

    gradient = c->fillGradient();
    if (fill && gradient && gradient->isZeroSize())
        return;

    FontCachePurgePreventer fontCachePurgePreventer;

    const Font& font = accessFont();
    const FontMetrics& fontMetrics = font.fontMetrics();
    // According to spec, all the space characters must be replaced with U+0020 SPACE characters.
    String normalizedText = text;
    replaceCharacterInString(normalizedText, isSpaceOrNewline, " ");

    // FIXME: Need to turn off font smoothing.

    RenderStyle* computedStyle = canvas()->computedStyle();
    TextDirection direction = computedStyle ? computedStyle->direction() : LTR;
    bool isRTL = direction == RTL;
    bool override = computedStyle ? isOverride(computedStyle->unicodeBidi()) : false;

    TextRun textRun(normalizedText, 0, 0, TextRun::AllowTrailingExpansion, direction, override, true, TextRun::NoRounding);
    // Draw the item text at the correct point.
    FloatPoint location(x, y + getFontBaseline(fontMetrics));

    float fontWidth = font.width(TextRun(normalizedText, 0, 0, TextRun::AllowTrailingExpansion, direction, override));

    useMaxWidth = (useMaxWidth && maxWidth < fontWidth);
    float width = useMaxWidth ? maxWidth : fontWidth;

    TextAlign align = state().m_textAlign;
    if (align == StartTextAlign)
        align = isRTL ? RightTextAlign : LeftTextAlign;
    else if (align == EndTextAlign)
        align = isRTL ? LeftTextAlign : RightTextAlign;

    switch (align) {
    case CenterTextAlign:
        location.setX(location.x() - width / 2);
        break;
    case RightTextAlign:
        location.setX(location.x() - width);
        break;
    default:
        break;
    }

    // The slop built in to this mask rect matches the heuristic used in FontCGWin.cpp for GDI text.
    TextRunPaintInfo textRunPaintInfo(textRun);
    textRunPaintInfo.bounds = FloatRect(location.x() - fontMetrics.height() / 2,
                                        location.y() - fontMetrics.ascent() - fontMetrics.lineGap(),
                                        width + fontMetrics.height(),
                                        fontMetrics.lineSpacing());
    if (!fill)
        inflateStrokeRect(textRunPaintInfo.bounds);

    FloatRect dirtyRect;
    if (!computeDirtyRect(textRunPaintInfo.bounds, &dirtyRect))
        return;

    c->setTextDrawingMode(fill ? TextModeFill : TextModeStroke);
    if (useMaxWidth) {
        GraphicsContextStateSaver stateSaver(*c);
        c->translate(location.x(), location.y());
        // We draw when fontWidth is 0 so compositing operations (eg, a "copy" op) still work.
        c->scale(FloatSize((fontWidth > 0 ? (width / fontWidth) : 0), 1));
        c->drawBidiText(font, textRunPaintInfo, FloatPoint(0, 0), Font::UseFallbackIfFontNotReady);
    } else
        c->drawBidiText(font, textRunPaintInfo, location, Font::UseFallbackIfFontNotReady);

    didDraw(dirtyRect);
}

void CanvasRenderingContext2D::inflateStrokeRect(FloatRect& rect) const
{
    // Fast approximation of the stroke's bounding rect.
    // This yields a slightly oversized rect but is very fast
    // compared to Path::strokeBoundingRect().
    static const float root2 = sqrtf(2);
    float delta = state().m_lineWidth / 2;
    if (state().m_lineJoin == MiterJoin)
        delta *= state().m_miterLimit;
    else if (state().m_lineCap == SquareCap)
        delta *= root2;

    rect.inflate(delta);
}

const Font& CanvasRenderingContext2D::accessFont()
{
    // This needs style to be up to date, but can't assert so because drawTextInternal
    // can invalidate style before this is called (e.g. drawingContext invalidates style).
    if (!state().m_realizedFont)
        setFont(state().m_unparsedFont);
    return state().m_font;
}

int CanvasRenderingContext2D::getFontBaseline(const FontMetrics& fontMetrics) const
{
    switch (state().m_textBaseline) {
    case TopTextBaseline:
        return fontMetrics.ascent();
    case HangingTextBaseline:
        // According to http://wiki.apache.org/xmlgraphics-fop/LineLayout/AlignmentHandling
        // "FOP (Formatting Objects Processor) puts the hanging baseline at 80% of the ascender height"
        return (fontMetrics.ascent() * 4) / 5;
    case BottomTextBaseline:
    case IdeographicTextBaseline:
        return -fontMetrics.descent();
    case MiddleTextBaseline:
        return -fontMetrics.descent() + fontMetrics.height() / 2;
    case AlphabeticTextBaseline:
    default:
        // Do nothing.
        break;
    }
    return 0;
}

blink::WebLayer* CanvasRenderingContext2D::platformLayer() const
{
    return canvas()->buffer() ? canvas()->buffer()->platformLayer() : 0;
}

bool CanvasRenderingContext2D::imageSmoothingEnabled() const
{
    return state().m_imageSmoothingEnabled;
}

void CanvasRenderingContext2D::setImageSmoothingEnabled(bool enabled)
{
    if (enabled == state().m_imageSmoothingEnabled)
        return;

    realizeSaves();
    modifiableState().m_imageSmoothingEnabled = enabled;
    GraphicsContext* c = drawingContext();
    if (c)
        c->setImageInterpolationQuality(enabled ? DefaultInterpolationQuality : InterpolationNone);
}

PassRefPtr<Canvas2DContextAttributes> CanvasRenderingContext2D::getContextAttributes() const
{
    RefPtr<Canvas2DContextAttributes> attributes = Canvas2DContextAttributes::create();
    attributes->setAlpha(m_hasAlpha);
    return attributes.release();
}

void CanvasRenderingContext2D::drawSystemFocusRing(Element* element)
{
    if (!focusRingCallIsValid(m_path, element))
        return;

    updateFocusRingAccessibility(m_path, element);
    // Note: we need to check document->focusedElement() rather than just calling
    // element->focused(), because element->focused() isn't updated until after
    // focus events fire.
    if (element->document().focusedElement() == element)
        drawFocusRing(m_path);
}

bool CanvasRenderingContext2D::drawCustomFocusRing(Element* element)
{
    if (!focusRingCallIsValid(m_path, element))
        return false;

    updateFocusRingAccessibility(m_path, element);

    // Return true if the application should draw the focus ring. The spec allows us to
    // override this for accessibility, but currently Blink doesn't take advantage of this.
    return element->focused();
}

bool CanvasRenderingContext2D::focusRingCallIsValid(const Path& path, Element* element)
{
    if (!state().m_invertibleCTM)
        return false;
    if (path.isEmpty())
        return false;
    if (!element->isDescendantOf(canvas()))
        return false;

    return true;
}

void CanvasRenderingContext2D::updateFocusRingAccessibility(const Path& path, Element* element)
{
    if (!canvas()->renderer())
        return;

    // If accessibility is already enabled in this frame, associate this path's
    // bounding box with the accessible object. Do this even if the element
    // isn't focused because assistive technology might try to explore the object's
    // location before it gets focus.
    if (AXObjectCache* axObjectCache = element->document().existingAXObjectCache()) {
        if (AXObject* obj = axObjectCache->getOrCreate(element)) {
            // Get the bounding rect and apply transformations.
            FloatRect bounds = m_path.boundingRect();
            AffineTransform ctm = state().m_transform;
            FloatRect transformedBounds = ctm.mapRect(bounds);
            LayoutRect elementRect = LayoutRect(transformedBounds);

            // Offset by the canvas rect and set the bounds of the accessible element.
            IntRect canvasRect = canvas()->renderer()->absoluteBoundingBoxRect();
            elementRect.moveBy(canvasRect.location());
            obj->setElementRect(elementRect);

            // Set the bounds of any ancestor accessible elements, up to the canvas element,
            // otherwise this element will appear to not be within its parent element.
            obj = obj->parentObject();
            while (obj && obj->node() != canvas()) {
                obj->setElementRect(elementRect);
                obj = obj->parentObject();
            }
        }
    }
}

void CanvasRenderingContext2D::drawFocusRing(const Path& path)
{
    GraphicsContext* c = drawingContext();
    if (!c)
        return;

    FloatRect dirtyRect;
    if (!computeDirtyRect(path.boundingRect(), &dirtyRect))
        return;

    c->save();
    c->setAlphaAsFloat(1.0);
    c->clearShadow();
    c->setCompositeOperation(CompositeSourceOver, blink::WebBlendModeNormal);

    // These should match the style defined in html.css.
    Color focusRingColor = RenderTheme::theme().focusRingColor();
    const int focusRingWidth = 5;
    const int focusRingOutline = 0;
    c->drawFocusRing(path, focusRingWidth, focusRingOutline, focusRingColor);

    c->restore();

    didDraw(dirtyRect);
}

} // namespace WebCore
