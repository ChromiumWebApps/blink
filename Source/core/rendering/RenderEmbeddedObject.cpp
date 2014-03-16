/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Simon Hausmann <hausmann@kde.org>
 *           (C) 2000 Stefan Schimanski (1Stein@gmx.de)
 * Copyright (C) 2004, 2005, 2006, 2008, 2009, 2010 Apple Inc. All rights reserved.
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
 *
 */

#include "config.h"
#include "core/rendering/RenderEmbeddedObject.h"

#include "CSSValueKeywords.h"
#include "HTMLNames.h"
#include "core/frame/LocalFrame.h"
#include "core/html/HTMLIFrameElement.h"
#include "core/page/Page.h"
#include "core/frame/Settings.h"
#include "core/plugins/PluginView.h"
#include "core/rendering/LayoutRectRecorder.h"
#include "core/rendering/PaintInfo.h"
#include "core/rendering/RenderTheme.h"
#include "core/rendering/RenderView.h"
#include "platform/fonts/Font.h"
#include "platform/fonts/FontSelector.h"
#include "platform/graphics/GraphicsContextStateSaver.h"
#include "platform/graphics/Path.h"
#include "platform/text/PlatformLocale.h"
#include "platform/text/TextRun.h"

namespace WebCore {

using namespace HTMLNames;

static const float replacementTextRoundedRectHeight = 18;
static const float replacementTextRoundedRectLeftRightTextMargin = 6;
static const float replacementTextRoundedRectOpacity = 0.20f;
static const float replacementTextRoundedRectRadius = 5;
static const float replacementTextTextOpacity = 0.55f;

RenderEmbeddedObject::RenderEmbeddedObject(Element* element)
    : RenderPart(element)
    , m_hasFallbackContent(false)
    , m_showsUnavailablePluginIndicator(false)
{
    view()->frameView()->setIsVisuallyNonEmpty();
}

RenderEmbeddedObject::~RenderEmbeddedObject()
{
}

LayerType RenderEmbeddedObject::layerTypeRequired() const
{
    LayerType type = RenderPart::layerTypeRequired();
    if (type != NoLayer)
        return type;

    return allowsAcceleratedCompositing() ? NormalLayer : NoLayer;
}

bool RenderEmbeddedObject::allowsAcceleratedCompositing() const
{
    return widget() && widget()->isPluginView() && toPluginView(widget())->platformLayer();
}

static String unavailablePluginReplacementText(Node* node, RenderEmbeddedObject::PluginUnavailabilityReason pluginUnavailabilityReason)
{
    Locale& locale = node ? toElement(node)->locale() : Locale::defaultLocale();
    switch (pluginUnavailabilityReason) {
    case RenderEmbeddedObject::PluginMissing:
        return locale.queryString(blink::WebLocalizedString::MissingPluginText);
    case RenderEmbeddedObject::PluginBlockedByContentSecurityPolicy:
        return locale.queryString(blink::WebLocalizedString::BlockedPluginText);
    }

    ASSERT_NOT_REACHED();
    return String();
}

void RenderEmbeddedObject::setPluginUnavailabilityReason(PluginUnavailabilityReason pluginUnavailabilityReason)
{
    ASSERT(!m_showsUnavailablePluginIndicator);
    m_showsUnavailablePluginIndicator = true;
    m_pluginUnavailabilityReason = pluginUnavailabilityReason;

    m_unavailablePluginReplacementText = unavailablePluginReplacementText(node(), pluginUnavailabilityReason);
}

bool RenderEmbeddedObject::showsUnavailablePluginIndicator() const
{
    return m_showsUnavailablePluginIndicator;
}

void RenderEmbeddedObject::paintContents(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    Element* element = toElement(node());
    if (!element || !element->isPluginElement())
        return;

    RenderPart::paintContents(paintInfo, paintOffset);
}

void RenderEmbeddedObject::paint(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    if (showsUnavailablePluginIndicator()) {
        RenderReplaced::paint(paintInfo, paintOffset);
        return;
    }

    RenderPart::paint(paintInfo, paintOffset);
}

void RenderEmbeddedObject::paintReplaced(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    if (!showsUnavailablePluginIndicator())
        return;

    if (paintInfo.phase == PaintPhaseSelection)
        return;

    GraphicsContext* context = paintInfo.context;
    if (context->paintingDisabled())
        return;

    FloatRect contentRect;
    Path path;
    FloatRect replacementTextRect;
    Font font;
    TextRun run("");
    float textWidth;
    if (!getReplacementTextGeometry(paintOffset, contentRect, path, replacementTextRect, font, run, textWidth))
        return;

    GraphicsContextStateSaver stateSaver(*context);
    context->clip(contentRect);
    context->setAlphaAsFloat(replacementTextRoundedRectOpacity);
    context->setFillColor(Color::white);
    context->fillPath(path);

    const FontMetrics& fontMetrics = font.fontMetrics();
    float labelX = roundf(replacementTextRect.location().x() + (replacementTextRect.size().width() - textWidth) / 2);
    float labelY = roundf(replacementTextRect.location().y() + (replacementTextRect.size().height() - fontMetrics.height()) / 2 + fontMetrics.ascent());
    TextRunPaintInfo runInfo(run);
    runInfo.bounds = replacementTextRect;
    context->setAlphaAsFloat(replacementTextTextOpacity);
    context->setFillColor(Color::black);
    context->drawBidiText(font, runInfo, FloatPoint(labelX, labelY));
}

bool RenderEmbeddedObject::getReplacementTextGeometry(const LayoutPoint& accumulatedOffset, FloatRect& contentRect, Path& path, FloatRect& replacementTextRect, Font& font, TextRun& run, float& textWidth) const
{
    contentRect = contentBoxRect();
    contentRect.moveBy(roundedIntPoint(accumulatedOffset));

    FontDescription fontDescription;
    RenderTheme::theme().systemFont(CSSValueWebkitSmallControl, fontDescription);
    fontDescription.setWeight(FontWeightBold);
    Settings* settings = document().settings();
    ASSERT(settings);
    if (!settings)
        return false;
    fontDescription.setComputedSize(fontDescription.specifiedSize());
    font = Font(fontDescription);
    font.update(nullptr);

    run = TextRun(m_unavailablePluginReplacementText);
    textWidth = font.width(run);

    replacementTextRect.setSize(FloatSize(textWidth + replacementTextRoundedRectLeftRightTextMargin * 2, replacementTextRoundedRectHeight));
    float x = (contentRect.size().width() / 2 - replacementTextRect.size().width() / 2) + contentRect.location().x();
    float y = (contentRect.size().height() / 2 - replacementTextRect.size().height() / 2) + contentRect.location().y();
    replacementTextRect.setLocation(FloatPoint(x, y));

    path.addRoundedRect(replacementTextRect, FloatSize(replacementTextRoundedRectRadius, replacementTextRoundedRectRadius));

    return true;
}

void RenderEmbeddedObject::layout()
{
    ASSERT(needsLayout());

    LayoutSize oldSize = contentBoxRect().size();
    LayoutRectRecorder recorder(*this);

    updateLogicalWidth();
    updateLogicalHeight();

    RenderPart::layout();

    m_overflow.clear();
    addVisualEffectOverflow();

    updateLayerTransform();

    if (!widget() && frameView())
        frameView()->addWidgetToUpdate(*this);

    clearNeedsLayout();

    if (!canHaveChildren())
        return;

    // This code copied from RenderMedia::layout().
    RenderObject* child = m_children.firstChild();

    if (!child)
        return;

    RenderBox* childBox = toRenderBox(child);

    if (!childBox)
        return;

    LayoutSize newSize = contentBoxRect().size();
    if (newSize == oldSize && !childBox->needsLayout())
        return;

    LayoutStateMaintainer statePusher(*this, locationOffset());

    childBox->setLocation(LayoutPoint(borderLeft(), borderTop()) + LayoutSize(paddingLeft(), paddingTop()));
    childBox->style()->setHeight(Length(newSize.height(), Fixed));
    childBox->style()->setWidth(Length(newSize.width(), Fixed));
    childBox->forceLayout();
    clearNeedsLayout();

    statePusher.pop();
}

bool RenderEmbeddedObject::scroll(ScrollDirection direction, ScrollGranularity granularity, float)
{
    return false;
}

bool RenderEmbeddedObject::canHaveChildren() const
{
    return false;
}

}
