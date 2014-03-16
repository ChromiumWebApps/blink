/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2005, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Graham Dennis (graham.dennis@gmail.com)
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

#ifndef RenderStyle_h
#define RenderStyle_h

#include "CSSPropertyNames.h"
#include "core/animation/css/CSSAnimationDataList.h"
#include "core/css/CSSLineBoxContainValue.h"
#include "core/css/CSSPrimitiveValue.h"
#include "core/rendering/style/BorderValue.h"
#include "core/rendering/style/CounterDirectives.h"
#include "core/rendering/style/DataRef.h"
#include "core/rendering/style/LineClampValue.h"
#include "core/rendering/style/NinePieceImage.h"
#include "core/rendering/style/OutlineValue.h"
#include "core/rendering/style/RenderStyleConstants.h"
#include "core/rendering/style/SVGRenderStyle.h"
#include "core/rendering/style/ShapeValue.h"
#include "core/rendering/style/StyleBackgroundData.h"
#include "core/rendering/style/StyleBoxData.h"
#include "core/rendering/style/StyleDeprecatedFlexibleBoxData.h"
#include "core/rendering/style/StyleFilterData.h"
#include "core/rendering/style/StyleFlexibleBoxData.h"
#include "core/rendering/style/StyleGridData.h"
#include "core/rendering/style/StyleGridItemData.h"
#include "core/rendering/style/StyleMarqueeData.h"
#include "core/rendering/style/StyleMultiColData.h"
#include "core/rendering/style/StyleRareInheritedData.h"
#include "core/rendering/style/StyleRareNonInheritedData.h"
#include "core/rendering/style/StyleReflection.h"
#include "core/rendering/style/StyleSurroundData.h"
#include "core/rendering/style/StyleTransformData.h"
#include "core/rendering/style/StyleVisualData.h"
#include "core/rendering/style/StyleWillChangeData.h"
#include "core/svg/SVGPaint.h"
#include "platform/Length.h"
#include "platform/LengthBox.h"
#include "platform/LengthSize.h"
#include "platform/ThemeTypes.h"
#include "platform/fonts/FontBaseline.h"
#include "platform/fonts/FontDescription.h"
#include "platform/geometry/FloatRoundedRect.h"
#include "platform/geometry/LayoutBoxExtent.h"
#include "platform/geometry/RoundedRect.h"
#include "platform/graphics/Color.h"
#include "platform/graphics/GraphicsTypes.h"
#include "platform/scroll/ScrollableArea.h"
#include "platform/text/TextDirection.h"
#include "platform/text/UnicodeBidi.h"
#include "platform/transforms/TransformOperations.h"
#include "wtf/Forward.h"
#include "wtf/OwnPtr.h"
#include "wtf/RefCounted.h"
#include "wtf/StdLibExtras.h"
#include "wtf/Vector.h"

template<typename T, typename U> inline bool compareEqual(const T& t, const U& u) { return t == static_cast<T>(u); }

#define SET_VAR(group, variable, value) \
    if (!compareEqual(group->variable, value)) \
        group.access()->variable = value

#define SET_BORDERVALUE_COLOR(group, variable, value) \
    if (!compareEqual(group->variable.color(), value)) \
        group.access()->variable.setColor(value)

namespace WebCore {

using std::max;

class FilterOperations;

class BorderData;
class CounterContent;
class CursorList;
class Font;
class FontMetrics;
class IntRect;
class Pair;
class ShadowList;
class StyleImage;
class StyleInheritedData;
class StyleResolver;
class TransformationMatrix;

class ContentData;

typedef Vector<RefPtr<RenderStyle>, 4> PseudoStyleCache;

class RenderStyle: public RefCounted<RenderStyle> {
    friend class AnimatedStyleBuilder; // Used by Web Animations CSS. Sets the color styles
    friend class CSSAnimatableValueFactory; // Used by Web Animations CSS. Gets visited and unvisited colors separately.
    friend class CSSPropertyEquality; // Used by CSS animations. We can't allow them to animate based off visited colors.
    friend class ApplyStyleCommand; // Editing has to only reveal unvisited info.
    friend class EditingStyle; // Editing has to only reveal unvisited info.
    friend class CSSComputedStyleDeclaration; // Ignores visited styles, so needs to be able to see unvisited info.
    friend class PropertyWrapperMaybeInvalidStyleColor; // Used by CSS animations. We can't allow them to animate based off visited colors.
    friend class StyleBuilderFunctions; // Sets color styles
    friend class CachedUAStyle; // Saves Border/Background information for later comparison.

    // FIXME: When we stop resolving currentColor at style time, these can be removed.
    friend class CSSToStyleMap;
    friend class FilterOperationResolver;
    friend class StyleBuilderConverter;
    friend class StyleBuilder;
    friend class StyleResolverState;
    friend class StyleResolver;
protected:

    // non-inherited attributes
    DataRef<StyleBoxData> m_box;
    DataRef<StyleVisualData> visual;
    DataRef<StyleBackgroundData> m_background;
    DataRef<StyleSurroundData> surround;
    DataRef<StyleRareNonInheritedData> rareNonInheritedData;

    // inherited attributes
    DataRef<StyleRareInheritedData> rareInheritedData;
    DataRef<StyleInheritedData> inherited;

    // list of associated pseudo styles
    OwnPtr<PseudoStyleCache> m_cachedPseudoStyles;

    DataRef<SVGRenderStyle> m_svgStyle;

// !START SYNC!: Keep this in sync with the copy constructor in RenderStyle.cpp and implicitlyInherited() in StyleResolver.cpp

    // inherit
    struct InheritedFlags {
        bool operator==(const InheritedFlags& other) const
        {
            return (_empty_cells == other._empty_cells)
                && (_caption_side == other._caption_side)
                && (_list_style_type == other._list_style_type)
                && (_list_style_position == other._list_style_position)
                && (_visibility == other._visibility)
                && (_text_align == other._text_align)
                && (_text_transform == other._text_transform)
                && (_text_decorations == other._text_decorations)
                && (_cursor_style == other._cursor_style)
                && (_direction == other._direction)
                && (_white_space == other._white_space)
                && (_border_collapse == other._border_collapse)
                && (_box_direction == other._box_direction)
                && (m_rtlOrdering == other.m_rtlOrdering)
                && (m_printColorAdjust == other.m_printColorAdjust)
                && (_pointerEvents == other._pointerEvents)
                && (_insideLink == other._insideLink)
                && (m_writingMode == other.m_writingMode);
        }

        bool operator!=(const InheritedFlags& other) const { return !(*this == other); }

        unsigned _empty_cells : 1; // EEmptyCell
        unsigned _caption_side : 2; // ECaptionSide
        unsigned _list_style_type : 7; // EListStyleType
        unsigned _list_style_position : 1; // EListStylePosition
        unsigned _visibility : 2; // EVisibility
        unsigned _text_align : 4; // ETextAlign
        unsigned _text_transform : 2; // ETextTransform
        unsigned _text_decorations : TextDecorationBits;
        unsigned _cursor_style : 6; // ECursor
        unsigned _direction : 1; // TextDirection
        unsigned _white_space : 3; // EWhiteSpace
        // 32 bits
        unsigned _border_collapse : 1; // EBorderCollapse
        unsigned _box_direction : 1; // EBoxDirection (CSS3 box_direction property, flexible box layout module)

        // non CSS2 inherited
        unsigned m_rtlOrdering : 1; // Order
        unsigned m_printColorAdjust : PrintColorAdjustBits;
        unsigned _pointerEvents : 4; // EPointerEvents
        unsigned _insideLink : 2; // EInsideLink
        // 43 bits

        // CSS Text Layout Module Level 3: Vertical writing support
        unsigned m_writingMode : 2; // WritingMode
        // 45 bits
    } inherited_flags;

// don't inherit
    struct NonInheritedFlags {
        bool operator==(const NonInheritedFlags& other) const
        {
            return _effectiveDisplay == other._effectiveDisplay
                && _originalDisplay == other._originalDisplay
                && _overflowX == other._overflowX
                && _overflowY == other._overflowY
                && _vertical_align == other._vertical_align
                && _clear == other._clear
                && _position == other._position
                && _floating == other._floating
                && _table_layout == other._table_layout
                && _page_break_before == other._page_break_before
                && _page_break_after == other._page_break_after
                && _page_break_inside == other._page_break_inside
                && _styleType == other._styleType
                && _affectedByFocus == other._affectedByFocus
                && _affectedByHover == other._affectedByHover
                && _affectedByActive == other._affectedByActive
                && _affectedByDrag == other._affectedByDrag
                && _pseudoBits == other._pseudoBits
                && _unicodeBidi == other._unicodeBidi
                && explicitInheritance == other.explicitInheritance
                && currentColor == other.currentColor
                && unique == other.unique
                && emptyState == other.emptyState
                && firstChildState == other.firstChildState
                && lastChildState == other.lastChildState
                && _isLink == other._isLink;
        }

        bool operator!=(const NonInheritedFlags& other) const { return !(*this == other); }

        unsigned _effectiveDisplay : 5; // EDisplay
        unsigned _originalDisplay : 5; // EDisplay
        unsigned _overflowX : 3; // EOverflow
        unsigned _overflowY : 3; // EOverflow
        unsigned _vertical_align : 4; // EVerticalAlign
        unsigned _clear : 2; // EClear
        unsigned _position : 3; // EPosition
        unsigned _floating : 2; // EFloat
        unsigned _table_layout : 1; // ETableLayout

        unsigned _unicodeBidi : 3; // EUnicodeBidi

        // This is set if we used viewport units when resolving a length.
        // It is mutable so we can pass around const RenderStyles to resolve lengths.
        mutable unsigned hasViewportUnits : 1;

        // 32 bits

        unsigned _page_break_before : 2; // EPageBreak
        unsigned _page_break_after : 2; // EPageBreak
        unsigned _page_break_inside : 2; // EPageBreak

        unsigned _styleType : 6; // PseudoId
        unsigned _pseudoBits : 8;
        unsigned explicitInheritance : 1; // Explicitly inherits a non-inherited property
        unsigned currentColor : 1; // At least one color has the value 'currentColor'
        unsigned unique : 1; // Style can not be shared.
        unsigned emptyState : 1;
        unsigned firstChildState : 1;
        unsigned lastChildState : 1;

        bool affectedByFocus() const { return _affectedByFocus; }
        void setAffectedByFocus(bool value) { _affectedByFocus = value; }
        bool affectedByHover() const { return _affectedByHover; }
        void setAffectedByHover(bool value) { _affectedByHover = value; }
        bool affectedByActive() const { return _affectedByActive; }
        void setAffectedByActive(bool value) { _affectedByActive = value; }
        bool affectedByDrag() const { return _affectedByDrag; }
        void setAffectedByDrag(bool value) { _affectedByDrag = value; }
        bool isLink() const { return _isLink; }
        void setIsLink(bool value) { _isLink = value; }
    private:
        unsigned _affectedByFocus : 1;
        unsigned _affectedByHover : 1;
        unsigned _affectedByActive : 1;
        unsigned _affectedByDrag : 1;
        unsigned _isLink : 1;
        // If you add more style bits here, you will also need to update RenderStyle::copyNonInheritedFrom()
        // 63 bits
    } noninherited_flags;

// !END SYNC!

protected:
    void setBitDefaults()
    {
        inherited_flags._empty_cells = initialEmptyCells();
        inherited_flags._caption_side = initialCaptionSide();
        inherited_flags._list_style_type = initialListStyleType();
        inherited_flags._list_style_position = initialListStylePosition();
        inherited_flags._visibility = initialVisibility();
        inherited_flags._text_align = initialTextAlign();
        inherited_flags._text_transform = initialTextTransform();
        inherited_flags._text_decorations = initialTextDecoration();
        inherited_flags._cursor_style = initialCursor();
        inherited_flags._direction = initialDirection();
        inherited_flags._white_space = initialWhiteSpace();
        inherited_flags._border_collapse = initialBorderCollapse();
        inherited_flags.m_rtlOrdering = initialRTLOrdering();
        inherited_flags._box_direction = initialBoxDirection();
        inherited_flags.m_printColorAdjust = initialPrintColorAdjust();
        inherited_flags._pointerEvents = initialPointerEvents();
        inherited_flags._insideLink = NotInsideLink;
        inherited_flags.m_writingMode = initialWritingMode();

        noninherited_flags._effectiveDisplay = noninherited_flags._originalDisplay = initialDisplay();
        noninherited_flags._overflowX = initialOverflowX();
        noninherited_flags._overflowY = initialOverflowY();
        noninherited_flags._vertical_align = initialVerticalAlign();
        noninherited_flags._clear = initialClear();
        noninherited_flags._position = initialPosition();
        noninherited_flags._floating = initialFloating();
        noninherited_flags._table_layout = initialTableLayout();
        noninherited_flags._unicodeBidi = initialUnicodeBidi();
        noninherited_flags._page_break_before = initialPageBreak();
        noninherited_flags._page_break_after = initialPageBreak();
        noninherited_flags._page_break_inside = initialPageBreak();
        noninherited_flags._styleType = NOPSEUDO;
        noninherited_flags._pseudoBits = 0;
        noninherited_flags.explicitInheritance = false;
        noninherited_flags.currentColor = false;
        noninherited_flags.unique = false;
        noninherited_flags.emptyState = false;
        noninherited_flags.firstChildState = false;
        noninherited_flags.lastChildState = false;
        noninherited_flags.hasViewportUnits = false;
        noninherited_flags.setAffectedByFocus(false);
        noninherited_flags.setAffectedByHover(false);
        noninherited_flags.setAffectedByActive(false);
        noninherited_flags.setAffectedByDrag(false);
        noninherited_flags.setIsLink(false);
    }

private:
    ALWAYS_INLINE RenderStyle();

    enum DefaultStyleTag {
        DefaultStyle
    };
    ALWAYS_INLINE explicit RenderStyle(DefaultStyleTag);
    ALWAYS_INLINE RenderStyle(const RenderStyle&);

public:
    static PassRefPtr<RenderStyle> create();
    static PassRefPtr<RenderStyle> createDefaultStyle();
    static PassRefPtr<RenderStyle> createAnonymousStyleWithDisplay(const RenderStyle* parentStyle, EDisplay);
    static PassRefPtr<RenderStyle> clone(const RenderStyle*);

    static StyleRecalcChange compare(const RenderStyle* oldStyle, const RenderStyle* newStyle);

    enum IsAtShadowBoundary {
        AtShadowBoundary,
        NotAtShadowBoundary,
    };

    void inheritFrom(const RenderStyle* inheritParent, IsAtShadowBoundary = NotAtShadowBoundary);
    void copyNonInheritedFrom(const RenderStyle*);

    PseudoId styleType() const { return static_cast<PseudoId>(noninherited_flags._styleType); }
    void setStyleType(PseudoId styleType) { noninherited_flags._styleType = styleType; }

    RenderStyle* getCachedPseudoStyle(PseudoId) const;
    RenderStyle* addCachedPseudoStyle(PassRefPtr<RenderStyle>);
    void removeCachedPseudoStyle(PseudoId);

    const PseudoStyleCache* cachedPseudoStyles() const { return m_cachedPseudoStyles.get(); }

    void setHasViewportUnits(bool hasViewportUnits = true) const { noninherited_flags.hasViewportUnits = hasViewportUnits; }
    bool hasViewportUnits() const { return noninherited_flags.hasViewportUnits; }

    bool affectedByFocus() const { return noninherited_flags.affectedByFocus(); }
    bool affectedByHover() const { return noninherited_flags.affectedByHover(); }
    bool affectedByActive() const { return noninherited_flags.affectedByActive(); }
    bool affectedByDrag() const { return noninherited_flags.affectedByDrag(); }

    void setAffectedByFocus() { noninherited_flags.setAffectedByFocus(true); }
    void setAffectedByHover() { noninherited_flags.setAffectedByHover(true); }
    void setAffectedByActive() { noninherited_flags.setAffectedByActive(true); }
    void setAffectedByDrag() { noninherited_flags.setAffectedByDrag(true); }

    bool operator==(const RenderStyle& other) const;
    bool operator!=(const RenderStyle& other) const { return !(*this == other); }
    bool isFloating() const { return noninherited_flags._floating != NoFloat; }
    bool hasMargin() const { return surround->margin.nonZero(); }
    bool hasBorder() const { return surround->border.hasBorder(); }
    bool hasPadding() const { return surround->padding.nonZero(); }
    bool hasOffset() const { return surround->offset.nonZero(); }
    bool hasMarginBeforeQuirk() const { return marginBefore().quirk(); }
    bool hasMarginAfterQuirk() const { return marginAfter().quirk(); }

    bool hasBackgroundImage() const { return m_background->background().hasImage(); }
    bool hasFixedBackgroundImage() const { return m_background->background().hasFixedImage(); }

    bool hasEntirelyFixedBackground() const;

    bool hasAppearance() const { return appearance() != NoControlPart; }

    bool hasBackground() const
    {
        Color color = visitedDependentColor(CSSPropertyBackgroundColor);
        if (color.alpha())
            return true;
        return hasBackgroundImage();
    }

    LayoutBoxExtent imageOutsets(const NinePieceImage&) const;
    bool hasBorderImageOutsets() const
    {
        return borderImage().hasImage() && borderImage().outset().nonZero();
    }
    LayoutBoxExtent borderImageOutsets() const
    {
        return imageOutsets(borderImage());
    }

    LayoutBoxExtent maskBoxImageOutsets() const
    {
        return imageOutsets(maskBoxImage());
    }

    bool hasFilterOutsets() const { return hasFilter() && filter().hasOutsets(); }
    FilterOutsets filterOutsets() const { return hasFilter() ? filter().outsets() : FilterOutsets(); }

    Order rtlOrdering() const { return static_cast<Order>(inherited_flags.m_rtlOrdering); }
    void setRTLOrdering(Order o) { inherited_flags.m_rtlOrdering = o; }

    bool isStyleAvailable() const;

    bool hasAnyPublicPseudoStyles() const;
    bool hasPseudoStyle(PseudoId pseudo) const;
    void setHasPseudoStyle(PseudoId pseudo);
    bool hasUniquePseudoStyle() const;
    bool hasPseudoElementStyle() const;

    // attribute getter methods

    EDisplay display() const { return static_cast<EDisplay>(noninherited_flags._effectiveDisplay); }
    EDisplay originalDisplay() const { return static_cast<EDisplay>(noninherited_flags._originalDisplay); }

    const Length& left() const { return surround->offset.left(); }
    const Length& right() const { return surround->offset.right(); }
    const Length& top() const { return surround->offset.top(); }
    const Length& bottom() const { return surround->offset.bottom(); }

    // Accessors for positioned object edges that take into account writing mode.
    const Length& logicalLeft() const { return surround->offset.logicalLeft(writingMode()); }
    const Length& logicalRight() const { return surround->offset.logicalRight(writingMode()); }
    const Length& logicalTop() const { return surround->offset.before(writingMode()); }
    const Length& logicalBottom() const { return surround->offset.after(writingMode()); }

    // Whether or not a positioned element requires normal flow x/y to be computed
    // to determine its position.
    bool hasAutoLeftAndRight() const { return left().isAuto() && right().isAuto(); }
    bool hasAutoTopAndBottom() const { return top().isAuto() && bottom().isAuto(); }
    bool hasStaticInlinePosition(bool horizontal) const { return horizontal ? hasAutoLeftAndRight() : hasAutoTopAndBottom(); }
    bool hasStaticBlockPosition(bool horizontal) const { return horizontal ? hasAutoTopAndBottom() : hasAutoLeftAndRight(); }

    EPosition position() const { return static_cast<EPosition>(noninherited_flags._position); }
    bool hasOutOfFlowPosition() const { return position() == AbsolutePosition || position() == FixedPosition; }
    bool hasInFlowPosition() const { return position() == RelativePosition || position() == StickyPosition; }
    bool hasViewportConstrainedPosition() const { return position() == FixedPosition || position() == StickyPosition; }
    EFloat floating() const { return static_cast<EFloat>(noninherited_flags._floating); }

    const Length& width() const { return m_box->width(); }
    const Length& height() const { return m_box->height(); }
    const Length& minWidth() const { return m_box->minWidth(); }
    const Length& maxWidth() const { return m_box->maxWidth(); }
    const Length& minHeight() const { return m_box->minHeight(); }
    const Length& maxHeight() const { return m_box->maxHeight(); }

    const Length& logicalWidth() const { return isHorizontalWritingMode() ? width() : height(); }
    const Length& logicalHeight() const { return isHorizontalWritingMode() ? height() : width(); }
    const Length& logicalMinWidth() const { return isHorizontalWritingMode() ? minWidth() : minHeight(); }
    const Length& logicalMaxWidth() const { return isHorizontalWritingMode() ? maxWidth() : maxHeight(); }
    const Length& logicalMinHeight() const { return isHorizontalWritingMode() ? minHeight() : minWidth(); }
    const Length& logicalMaxHeight() const { return isHorizontalWritingMode() ? maxHeight() : maxWidth(); }

    const BorderData& border() const { return surround->border; }
    const BorderValue& borderLeft() const { return surround->border.left(); }
    const BorderValue& borderRight() const { return surround->border.right(); }
    const BorderValue& borderTop() const { return surround->border.top(); }
    const BorderValue& borderBottom() const { return surround->border.bottom(); }

    const BorderValue& borderBefore() const;
    const BorderValue& borderAfter() const;
    const BorderValue& borderStart() const;
    const BorderValue& borderEnd() const;

    const NinePieceImage& borderImage() const { return surround->border.image(); }
    StyleImage* borderImageSource() const { return surround->border.image().image(); }
    const LengthBox& borderImageSlices() const { return surround->border.image().imageSlices(); }
    const BorderImageLengthBox& borderImageWidth() const { return surround->border.image().borderSlices(); }
    const BorderImageLengthBox& borderImageOutset() const { return surround->border.image().outset(); }

    const LengthSize& borderTopLeftRadius() const { return surround->border.topLeft(); }
    const LengthSize& borderTopRightRadius() const { return surround->border.topRight(); }
    const LengthSize& borderBottomLeftRadius() const { return surround->border.bottomLeft(); }
    const LengthSize& borderBottomRightRadius() const { return surround->border.bottomRight(); }
    bool hasBorderRadius() const { return surround->border.hasBorderRadius(); }

    unsigned borderLeftWidth() const { return surround->border.borderLeftWidth(); }
    EBorderStyle borderLeftStyle() const { return surround->border.left().style(); }
    bool borderLeftIsTransparent() const { return surround->border.left().isTransparent(); }
    unsigned borderRightWidth() const { return surround->border.borderRightWidth(); }
    EBorderStyle borderRightStyle() const { return surround->border.right().style(); }
    bool borderRightIsTransparent() const { return surround->border.right().isTransparent(); }
    unsigned borderTopWidth() const { return surround->border.borderTopWidth(); }
    EBorderStyle borderTopStyle() const { return surround->border.top().style(); }
    bool borderTopIsTransparent() const { return surround->border.top().isTransparent(); }
    unsigned borderBottomWidth() const { return surround->border.borderBottomWidth(); }
    EBorderStyle borderBottomStyle() const { return surround->border.bottom().style(); }
    bool borderBottomIsTransparent() const { return surround->border.bottom().isTransparent(); }

    unsigned short borderBeforeWidth() const;
    unsigned short borderAfterWidth() const;
    unsigned short borderStartWidth() const;
    unsigned short borderEndWidth() const;

    unsigned short outlineSize() const { return max(0, outlineWidth() + outlineOffset()); }
    unsigned short outlineWidth() const
    {
        if (m_background->outline().style() == BNONE)
            return 0;
        return m_background->outline().width();
    }
    bool hasOutline() const { return outlineWidth() > 0 && outlineStyle() > BHIDDEN; }
    EBorderStyle outlineStyle() const { return m_background->outline().style(); }
    OutlineIsAuto outlineStyleIsAuto() const { return static_cast<OutlineIsAuto>(m_background->outline().isAuto()); }

    EOverflow overflowX() const { return static_cast<EOverflow>(noninherited_flags._overflowX); }
    EOverflow overflowY() const { return static_cast<EOverflow>(noninherited_flags._overflowY); }
    // It's sufficient to just check one direction, since it's illegal to have visible on only one overflow value.
    bool isOverflowVisible() const { ASSERT(overflowX() != OVISIBLE || overflowX() == overflowY()); return overflowX() == OVISIBLE; }
    bool isOverflowPaged() const { return overflowY() == OPAGEDX || overflowY() == OPAGEDY; }

    EVisibility visibility() const { return static_cast<EVisibility>(inherited_flags._visibility); }
    EVerticalAlign verticalAlign() const { return static_cast<EVerticalAlign>(noninherited_flags._vertical_align); }
    const Length& verticalAlignLength() const { return m_box->verticalAlign(); }

    const Length& clipLeft() const { return visual->clip.left(); }
    const Length& clipRight() const { return visual->clip.right(); }
    const Length& clipTop() const { return visual->clip.top(); }
    const Length& clipBottom() const { return visual->clip.bottom(); }
    const LengthBox& clip() const { return visual->clip; }
    bool hasClip() const { return visual->hasClip; }

    EUnicodeBidi unicodeBidi() const { return static_cast<EUnicodeBidi>(noninherited_flags._unicodeBidi); }

    EClear clear() const { return static_cast<EClear>(noninherited_flags._clear); }
    ETableLayout tableLayout() const { return static_cast<ETableLayout>(noninherited_flags._table_layout); }

    const Font& font() const;
    const FontMetrics& fontMetrics() const;
    const FontDescription& fontDescription() const;
    float specifiedFontSize() const;
    float computedFontSize() const;
    int fontSize() const;
    FontWeight fontWeight() const;

    float textAutosizingMultiplier() const { return visual->m_textAutosizingMultiplier; }

    const Length& textIndent() const { return rareInheritedData->indent; }
    TextIndentLine textIndentLine() const { return static_cast<TextIndentLine>(rareInheritedData->m_textIndentLine); }
    ETextAlign textAlign() const { return static_cast<ETextAlign>(inherited_flags._text_align); }
    TextAlignLast textAlignLast() const { return static_cast<TextAlignLast>(rareInheritedData->m_textAlignLast); }
    TextJustify textJustify() const { return static_cast<TextJustify>(rareInheritedData->m_textJustify); }
    ETextTransform textTransform() const { return static_cast<ETextTransform>(inherited_flags._text_transform); }
    TextDecoration textDecorationsInEffect() const { return static_cast<TextDecoration>(inherited_flags._text_decorations); }
    TextDecoration textDecoration() const { return static_cast<TextDecoration>(visual->textDecoration); }
    TextUnderlinePosition textUnderlinePosition() const { return static_cast<TextUnderlinePosition>(rareInheritedData->m_textUnderlinePosition); }
    TextDecorationStyle textDecorationStyle() const { return static_cast<TextDecorationStyle>(rareNonInheritedData->m_textDecorationStyle); }
    float wordSpacing() const;
    float letterSpacing() const;

    float zoom() const { return visual->m_zoom; }
    float effectiveZoom() const { return rareInheritedData->m_effectiveZoom; }

    TextDirection direction() const { return static_cast<TextDirection>(inherited_flags._direction); }
    bool isLeftToRightDirection() const { return direction() == LTR; }

    const Length& specifiedLineHeight() const;
    Length lineHeight() const;
    int computedLineHeight() const;

    EWhiteSpace whiteSpace() const { return static_cast<EWhiteSpace>(inherited_flags._white_space); }
    static bool autoWrap(EWhiteSpace ws)
    {
        // Nowrap and pre don't automatically wrap.
        return ws != NOWRAP && ws != PRE;
    }

    bool autoWrap() const
    {
        return autoWrap(whiteSpace());
    }

    static bool preserveNewline(EWhiteSpace ws)
    {
        // Normal and nowrap do not preserve newlines.
        return ws != NORMAL && ws != NOWRAP;
    }

    bool preserveNewline() const
    {
        return preserveNewline(whiteSpace());
    }

    static bool collapseWhiteSpace(EWhiteSpace ws)
    {
        // Pre and prewrap do not collapse whitespace.
        return ws != PRE && ws != PRE_WRAP;
    }

    bool collapseWhiteSpace() const
    {
        return collapseWhiteSpace(whiteSpace());
    }

    bool isCollapsibleWhiteSpace(UChar c) const
    {
        switch (c) {
            case ' ':
            case '\t':
                return collapseWhiteSpace();
            case '\n':
                return !preserveNewline();
        }
        return false;
    }

    bool breakOnlyAfterWhiteSpace() const
    {
        return whiteSpace() == PRE_WRAP || lineBreak() == LineBreakAfterWhiteSpace;
    }

    bool breakWords() const
    {
        return wordBreak() == BreakWordBreak || overflowWrap() == BreakOverflowWrap;
    }

    EFillRepeat backgroundRepeatX() const { return static_cast<EFillRepeat>(m_background->background().repeatX()); }
    EFillRepeat backgroundRepeatY() const { return static_cast<EFillRepeat>(m_background->background().repeatY()); }
    CompositeOperator backgroundComposite() const { return static_cast<CompositeOperator>(m_background->background().composite()); }
    EFillAttachment backgroundAttachment() const { return static_cast<EFillAttachment>(m_background->background().attachment()); }
    EFillBox backgroundClip() const { return static_cast<EFillBox>(m_background->background().clip()); }
    EFillBox backgroundOrigin() const { return static_cast<EFillBox>(m_background->background().origin()); }
    const Length& backgroundXPosition() const { return m_background->background().xPosition(); }
    const Length& backgroundYPosition() const { return m_background->background().yPosition(); }
    EFillSizeType backgroundSizeType() const { return m_background->background().sizeType(); }
    const LengthSize& backgroundSizeLength() const { return m_background->background().sizeLength(); }
    FillLayer* accessBackgroundLayers() { return &(m_background.access()->m_background); }
    const FillLayer* backgroundLayers() const { return &(m_background->background()); }

    StyleImage* maskImage() const { return rareNonInheritedData->m_mask.image(); }
    EFillRepeat maskRepeatX() const { return static_cast<EFillRepeat>(rareNonInheritedData->m_mask.repeatX()); }
    EFillRepeat maskRepeatY() const { return static_cast<EFillRepeat>(rareNonInheritedData->m_mask.repeatY()); }
    CompositeOperator maskComposite() const { return static_cast<CompositeOperator>(rareNonInheritedData->m_mask.composite()); }
    EFillBox maskClip() const { return static_cast<EFillBox>(rareNonInheritedData->m_mask.clip()); }
    EFillBox maskOrigin() const { return static_cast<EFillBox>(rareNonInheritedData->m_mask.origin()); }
    const Length& maskXPosition() const { return rareNonInheritedData->m_mask.xPosition(); }
    const Length& maskYPosition() const { return rareNonInheritedData->m_mask.yPosition(); }
    EFillSizeType maskSizeType() const { return rareNonInheritedData->m_mask.sizeType(); }
    const LengthSize& maskSizeLength() const { return rareNonInheritedData->m_mask.sizeLength(); }
    FillLayer* accessMaskLayers() { return &(rareNonInheritedData.access()->m_mask); }
    const FillLayer* maskLayers() const { return &(rareNonInheritedData->m_mask); }

    const NinePieceImage& maskBoxImage() const { return rareNonInheritedData->m_maskBoxImage; }
    StyleImage* maskBoxImageSource() const { return rareNonInheritedData->m_maskBoxImage.image(); }
    const LengthBox& maskBoxImageSlices() const { return rareNonInheritedData->m_maskBoxImage.imageSlices(); }
    bool maskBoxImageSlicesFill() const { return rareNonInheritedData->m_maskBoxImage.fill(); }
    const BorderImageLengthBox& maskBoxImageWidth() const { return rareNonInheritedData->m_maskBoxImage.borderSlices(); }
    const BorderImageLengthBox& maskBoxImageOutset() const { return rareNonInheritedData->m_maskBoxImage.outset(); }

    EBorderCollapse borderCollapse() const { return static_cast<EBorderCollapse>(inherited_flags._border_collapse); }
    short horizontalBorderSpacing() const;
    short verticalBorderSpacing() const;
    EEmptyCell emptyCells() const { return static_cast<EEmptyCell>(inherited_flags._empty_cells); }
    ECaptionSide captionSide() const { return static_cast<ECaptionSide>(inherited_flags._caption_side); }

    EListStyleType listStyleType() const { return static_cast<EListStyleType>(inherited_flags._list_style_type); }
    StyleImage* listStyleImage() const;
    EListStylePosition listStylePosition() const { return static_cast<EListStylePosition>(inherited_flags._list_style_position); }

    const Length& marginTop() const { return surround->margin.top(); }
    const Length& marginBottom() const { return surround->margin.bottom(); }
    const Length& marginLeft() const { return surround->margin.left(); }
    const Length& marginRight() const { return surround->margin.right(); }
    const Length& marginBefore() const { return surround->margin.before(writingMode()); }
    const Length& marginAfter() const { return surround->margin.after(writingMode()); }
    const Length& marginStart() const { return surround->margin.start(writingMode(), direction()); }
    const Length& marginEnd() const { return surround->margin.end(writingMode(), direction()); }
    const Length& marginStartUsing(const RenderStyle* otherStyle) const { return surround->margin.start(otherStyle->writingMode(), otherStyle->direction()); }
    const Length& marginEndUsing(const RenderStyle* otherStyle) const { return surround->margin.end(otherStyle->writingMode(), otherStyle->direction()); }
    const Length& marginBeforeUsing(const RenderStyle* otherStyle) const { return surround->margin.before(otherStyle->writingMode()); }
    const Length& marginAfterUsing(const RenderStyle* otherStyle) const { return surround->margin.after(otherStyle->writingMode()); }

    const LengthBox& paddingBox() const { return surround->padding; }
    const Length& paddingTop() const { return surround->padding.top(); }
    const Length& paddingBottom() const { return surround->padding.bottom(); }
    const Length& paddingLeft() const { return surround->padding.left(); }
    const Length& paddingRight() const { return surround->padding.right(); }
    const Length& paddingBefore() const { return surround->padding.before(writingMode()); }
    const Length& paddingAfter() const { return surround->padding.after(writingMode()); }
    const Length& paddingStart() const { return surround->padding.start(writingMode(), direction()); }
    const Length& paddingEnd() const { return surround->padding.end(writingMode(), direction()); }

    ECursor cursor() const { return static_cast<ECursor>(inherited_flags._cursor_style); }
    CursorList* cursors() const { return rareInheritedData->cursorData.get(); }

    EInsideLink insideLink() const { return static_cast<EInsideLink>(inherited_flags._insideLink); }
    bool isLink() const { return noninherited_flags.isLink(); }

    short widows() const { return rareInheritedData->widows; }
    short orphans() const { return rareInheritedData->orphans; }
    bool hasAutoWidows() const { return rareInheritedData->m_hasAutoWidows; }
    bool hasAutoOrphans() const { return rareInheritedData->m_hasAutoOrphans; }
    EPageBreak pageBreakInside() const { return static_cast<EPageBreak>(noninherited_flags._page_break_inside); }
    EPageBreak pageBreakBefore() const { return static_cast<EPageBreak>(noninherited_flags._page_break_before); }
    EPageBreak pageBreakAfter() const { return static_cast<EPageBreak>(noninherited_flags._page_break_after); }

    // CSS3 Getter Methods

    int outlineOffset() const
    {
        if (m_background->outline().style() == BNONE)
            return 0;
        return m_background->outline().offset();
    }

    ShadowList* textShadow() const { return rareInheritedData->textShadow.get(); }
    void getTextShadowExtent(LayoutUnit& top, LayoutUnit& right, LayoutUnit& bottom, LayoutUnit& left) const { getShadowExtent(textShadow(), top, right, bottom, left); }
    void getTextShadowHorizontalExtent(LayoutUnit& left, LayoutUnit& right) const { getShadowHorizontalExtent(textShadow(), left, right); }
    void getTextShadowVerticalExtent(LayoutUnit& top, LayoutUnit& bottom) const { getShadowVerticalExtent(textShadow(), top, bottom); }
    void getTextShadowInlineDirectionExtent(LayoutUnit& logicalLeft, LayoutUnit& logicalRight) { getShadowInlineDirectionExtent(textShadow(), logicalLeft, logicalRight); }
    void getTextShadowBlockDirectionExtent(LayoutUnit& logicalTop, LayoutUnit& logicalBottom) { getShadowBlockDirectionExtent(textShadow(), logicalTop, logicalBottom); }

    float textStrokeWidth() const { return rareInheritedData->textStrokeWidth; }
    float opacity() const { return rareNonInheritedData->opacity; }
    ControlPart appearance() const { return static_cast<ControlPart>(rareNonInheritedData->m_appearance); }
    // aspect ratio convenience method
    bool hasAspectRatio() const { return rareNonInheritedData->m_hasAspectRatio; }
    float aspectRatio() const { return aspectRatioNumerator() / aspectRatioDenominator(); }
    float aspectRatioDenominator() const { return rareNonInheritedData->m_aspectRatioDenominator; }
    float aspectRatioNumerator() const { return rareNonInheritedData->m_aspectRatioNumerator; }
    EBoxAlignment boxAlign() const { return static_cast<EBoxAlignment>(rareNonInheritedData->m_deprecatedFlexibleBox->align); }
    EBoxDirection boxDirection() const { return static_cast<EBoxDirection>(inherited_flags._box_direction); }
    float boxFlex() const { return rareNonInheritedData->m_deprecatedFlexibleBox->flex; }
    unsigned int boxFlexGroup() const { return rareNonInheritedData->m_deprecatedFlexibleBox->flex_group; }
    EBoxLines boxLines() const { return static_cast<EBoxLines>(rareNonInheritedData->m_deprecatedFlexibleBox->lines); }
    unsigned int boxOrdinalGroup() const { return rareNonInheritedData->m_deprecatedFlexibleBox->ordinal_group; }
    EBoxOrient boxOrient() const { return static_cast<EBoxOrient>(rareNonInheritedData->m_deprecatedFlexibleBox->orient); }
    EBoxPack boxPack() const { return static_cast<EBoxPack>(rareNonInheritedData->m_deprecatedFlexibleBox->pack); }

    int order() const { return rareNonInheritedData->m_order; }
    const Vector<String>& callbackSelectors() const { return rareNonInheritedData->m_callbackSelectors; }
    float flexGrow() const { return rareNonInheritedData->m_flexibleBox->m_flexGrow; }
    float flexShrink() const { return rareNonInheritedData->m_flexibleBox->m_flexShrink; }
    const Length& flexBasis() const { return rareNonInheritedData->m_flexibleBox->m_flexBasis; }
    EAlignContent alignContent() const { return static_cast<EAlignContent>(rareNonInheritedData->m_alignContent); }
    ItemPosition alignItems() const { return static_cast<ItemPosition>(rareNonInheritedData->m_alignItems); }
    OverflowAlignment alignItemsOverflowAlignment() const { return static_cast<OverflowAlignment>(rareNonInheritedData->m_alignItemsOverflowAlignment); }
    ItemPosition alignSelf() const { return static_cast<ItemPosition>(rareNonInheritedData->m_alignSelf); }
    OverflowAlignment alignSelfOverflowAlignment() const { return static_cast<OverflowAlignment>(rareNonInheritedData->m_alignSelfOverflowAlignment); }
    EFlexDirection flexDirection() const { return static_cast<EFlexDirection>(rareNonInheritedData->m_flexibleBox->m_flexDirection); }
    bool isColumnFlexDirection() const { return flexDirection() == FlowColumn || flexDirection() == FlowColumnReverse; }
    bool isReverseFlexDirection() const { return flexDirection() == FlowRowReverse || flexDirection() == FlowColumnReverse; }
    EFlexWrap flexWrap() const { return static_cast<EFlexWrap>(rareNonInheritedData->m_flexibleBox->m_flexWrap); }
    EJustifyContent justifyContent() const { return static_cast<EJustifyContent>(rareNonInheritedData->m_justifyContent); }
    ItemPosition justifySelf() const { return static_cast<ItemPosition>(rareNonInheritedData->m_justifySelf); }
    OverflowAlignment justifySelfOverflowAlignment() const { return static_cast<OverflowAlignment>(rareNonInheritedData->m_justifySelfOverflowAlignment); }

    const Vector<GridTrackSize>& gridTemplateColumns() const { return rareNonInheritedData->m_grid->m_gridTemplateColumns; }
    const Vector<GridTrackSize>& gridTemplateRows() const { return rareNonInheritedData->m_grid->m_gridTemplateRows; }
    const NamedGridLinesMap& namedGridColumnLines() const { return rareNonInheritedData->m_grid->m_namedGridColumnLines; }
    const NamedGridLinesMap& namedGridRowLines() const { return rareNonInheritedData->m_grid->m_namedGridRowLines; }
    const OrderedNamedGridLines& orderedNamedGridColumnLines() const { return rareNonInheritedData->m_grid->m_orderedNamedGridColumnLines; }
    const OrderedNamedGridLines& orderedNamedGridRowLines() const { return rareNonInheritedData->m_grid->m_orderedNamedGridRowLines; }
    const NamedGridAreaMap& namedGridArea() const { return rareNonInheritedData->m_grid->m_namedGridArea; }
    size_t namedGridAreaRowCount() const { return rareNonInheritedData->m_grid->m_namedGridAreaRowCount; }
    size_t namedGridAreaColumnCount() const { return rareNonInheritedData->m_grid->m_namedGridAreaColumnCount; }
    GridAutoFlow gridAutoFlow() const { return rareNonInheritedData->m_grid->m_gridAutoFlow; }
    const GridTrackSize& gridAutoColumns() const { return rareNonInheritedData->m_grid->m_gridAutoColumns; }
    const GridTrackSize& gridAutoRows() const { return rareNonInheritedData->m_grid->m_gridAutoRows; }

    const GridPosition& gridColumnStart() const { return rareNonInheritedData->m_gridItem->m_gridColumnStart; }
    const GridPosition& gridColumnEnd() const { return rareNonInheritedData->m_gridItem->m_gridColumnEnd; }
    const GridPosition& gridRowStart() const { return rareNonInheritedData->m_gridItem->m_gridRowStart; }
    const GridPosition& gridRowEnd() const { return rareNonInheritedData->m_gridItem->m_gridRowEnd; }

    ShadowList* boxShadow() const { return rareNonInheritedData->m_boxShadow.get(); }
    void getBoxShadowExtent(LayoutUnit& top, LayoutUnit& right, LayoutUnit& bottom, LayoutUnit& left) const { getShadowExtent(boxShadow(), top, right, bottom, left); }
    LayoutBoxExtent getBoxShadowInsetExtent() const { return getShadowInsetExtent(boxShadow()); }
    void getBoxShadowHorizontalExtent(LayoutUnit& left, LayoutUnit& right) const { getShadowHorizontalExtent(boxShadow(), left, right); }
    void getBoxShadowVerticalExtent(LayoutUnit& top, LayoutUnit& bottom) const { getShadowVerticalExtent(boxShadow(), top, bottom); }
    void getBoxShadowInlineDirectionExtent(LayoutUnit& logicalLeft, LayoutUnit& logicalRight) { getShadowInlineDirectionExtent(boxShadow(), logicalLeft, logicalRight); }
    void getBoxShadowBlockDirectionExtent(LayoutUnit& logicalTop, LayoutUnit& logicalBottom) { getShadowBlockDirectionExtent(boxShadow(), logicalTop, logicalBottom); }

    EBoxDecorationBreak boxDecorationBreak() const { return m_box->boxDecorationBreak(); }
    StyleReflection* boxReflect() const { return rareNonInheritedData->m_boxReflect.get(); }
    bool reflectionDataEquivalent(const RenderStyle* otherStyle) const { return rareNonInheritedData->reflectionDataEquivalent(*otherStyle->rareNonInheritedData); }

    EBoxSizing boxSizing() const { return m_box->boxSizing(); }
    const Length& marqueeIncrement() const { return rareNonInheritedData->m_marquee->increment; }
    int marqueeSpeed() const { return rareNonInheritedData->m_marquee->speed; }
    int marqueeLoopCount() const { return rareNonInheritedData->m_marquee->loops; }
    EMarqueeBehavior marqueeBehavior() const { return static_cast<EMarqueeBehavior>(rareNonInheritedData->m_marquee->behavior); }
    EMarqueeDirection marqueeDirection() const { return static_cast<EMarqueeDirection>(rareNonInheritedData->m_marquee->direction); }
    EUserModify userModify() const { return static_cast<EUserModify>(rareInheritedData->userModify); }
    EUserDrag userDrag() const { return static_cast<EUserDrag>(rareNonInheritedData->userDrag); }
    EUserSelect userSelect() const { return static_cast<EUserSelect>(rareInheritedData->userSelect); }
    TextOverflow textOverflow() const { return static_cast<TextOverflow>(rareNonInheritedData->textOverflow); }
    EMarginCollapse marginBeforeCollapse() const { return static_cast<EMarginCollapse>(rareNonInheritedData->marginBeforeCollapse); }
    EMarginCollapse marginAfterCollapse() const { return static_cast<EMarginCollapse>(rareNonInheritedData->marginAfterCollapse); }
    EWordBreak wordBreak() const { return static_cast<EWordBreak>(rareInheritedData->wordBreak); }
    EOverflowWrap overflowWrap() const { return static_cast<EOverflowWrap>(rareInheritedData->overflowWrap); }
    LineBreak lineBreak() const { return static_cast<LineBreak>(rareInheritedData->lineBreak); }
    const AtomicString& highlight() const { return rareInheritedData->highlight; }
    const AtomicString& hyphenationString() const { return rareInheritedData->hyphenationString; }
    const AtomicString& locale() const { return rareInheritedData->locale; }
    EBorderFit borderFit() const { return static_cast<EBorderFit>(rareNonInheritedData->m_borderFit); }
    EResize resize() const { return static_cast<EResize>(rareInheritedData->resize); }
    ColumnAxis columnAxis() const { return static_cast<ColumnAxis>(rareNonInheritedData->m_multiCol->m_axis); }
    bool hasInlineColumnAxis() const {
        ColumnAxis axis = columnAxis();
        return axis == AutoColumnAxis || isHorizontalWritingMode() == (axis == HorizontalColumnAxis);
    }
    bool hasInlinePaginationAxis() const
    {
        // If the pagination axis is parallel with the writing mode inline axis, columns may be laid
        // out along the inline axis, just like for regular multicol. Otherwise, we need to lay out
        // along the block axis.
        if (isOverflowPaged())
            return (overflowY() == OPAGEDX) == isHorizontalWritingMode();
        return false;
    }
    ColumnProgression columnProgression() const { return static_cast<ColumnProgression>(rareNonInheritedData->m_multiCol->m_progression); }
    float columnWidth() const { return rareNonInheritedData->m_multiCol->m_width; }
    bool hasAutoColumnWidth() const { return rareNonInheritedData->m_multiCol->m_autoWidth; }
    unsigned short columnCount() const { return rareNonInheritedData->m_multiCol->m_count; }
    bool hasAutoColumnCount() const { return rareNonInheritedData->m_multiCol->m_autoCount; }
    bool specifiesAutoColumns() const { return hasAutoColumnCount() && hasAutoColumnWidth(); }
    bool specifiesColumns() const { return !hasAutoColumnCount() || !hasAutoColumnWidth() || !hasInlineColumnAxis(); }
    ColumnFill columnFill() const { return static_cast<ColumnFill>(rareNonInheritedData->m_multiCol->m_fill); }
    float columnGap() const { return rareNonInheritedData->m_multiCol->m_gap; }
    bool hasNormalColumnGap() const { return rareNonInheritedData->m_multiCol->m_normalGap; }
    EBorderStyle columnRuleStyle() const { return rareNonInheritedData->m_multiCol->m_rule.style(); }
    unsigned short columnRuleWidth() const { return rareNonInheritedData->m_multiCol->ruleWidth(); }
    bool columnRuleIsTransparent() const { return rareNonInheritedData->m_multiCol->m_rule.isTransparent(); }
    ColumnSpan columnSpan() const { return static_cast<ColumnSpan>(rareNonInheritedData->m_multiCol->m_columnSpan); }
    EPageBreak columnBreakBefore() const { return static_cast<EPageBreak>(rareNonInheritedData->m_multiCol->m_breakBefore); }
    EPageBreak columnBreakInside() const { return static_cast<EPageBreak>(rareNonInheritedData->m_multiCol->m_breakInside); }
    EPageBreak columnBreakAfter() const { return static_cast<EPageBreak>(rareNonInheritedData->m_multiCol->m_breakAfter); }
    const TransformOperations& transform() const { return rareNonInheritedData->m_transform->m_operations; }
    const Length& transformOriginX() const { return rareNonInheritedData->m_transform->m_x; }
    const Length& transformOriginY() const { return rareNonInheritedData->m_transform->m_y; }
    float transformOriginZ() const { return rareNonInheritedData->m_transform->m_z; }
    bool hasTransform() const { return !rareNonInheritedData->m_transform->m_operations.operations().isEmpty(); }
    bool transformDataEquivalent(const RenderStyle* otherStyle) const { return rareNonInheritedData->m_transform == otherStyle->rareNonInheritedData->m_transform; }

    TextEmphasisFill textEmphasisFill() const { return static_cast<TextEmphasisFill>(rareInheritedData->textEmphasisFill); }
    TextEmphasisMark textEmphasisMark() const;
    const AtomicString& textEmphasisCustomMark() const { return rareInheritedData->textEmphasisCustomMark; }
    TextEmphasisPosition textEmphasisPosition() const { return static_cast<TextEmphasisPosition>(rareInheritedData->textEmphasisPosition); }
    const AtomicString& textEmphasisMarkString() const;

    RubyPosition rubyPosition() const { return static_cast<RubyPosition>(rareInheritedData->m_rubyPosition); }

    TextOrientation textOrientation() const { return static_cast<TextOrientation>(rareInheritedData->m_textOrientation); }

    ObjectFit objectFit() const { return static_cast<ObjectFit>(rareNonInheritedData->m_objectFit); }
    LengthPoint objectPosition() const { return rareNonInheritedData->m_objectPosition; }

    // Return true if any transform related property (currently transform, transformStyle3D or perspective)
    // indicates that we are transforming
    bool hasTransformRelatedProperty() const { return hasTransform() || preserves3D() || hasPerspective(); }

    enum ApplyTransformOrigin { IncludeTransformOrigin, ExcludeTransformOrigin };
    void applyTransform(TransformationMatrix&, const LayoutSize& borderBoxSize, ApplyTransformOrigin = IncludeTransformOrigin) const;
    void applyTransform(TransformationMatrix&, const FloatRect& boundingBox, ApplyTransformOrigin = IncludeTransformOrigin) const;

    bool hasMask() const { return rareNonInheritedData->m_mask.hasImage() || rareNonInheritedData->m_maskBoxImage.hasImage(); }

    TextCombine textCombine() const { return static_cast<TextCombine>(rareNonInheritedData->m_textCombine); }
    bool hasTextCombine() const { return textCombine() != TextCombineNone; }

    unsigned tabSize() const { return rareInheritedData->m_tabSize; }

    // End CSS3 Getters

    WrapFlow wrapFlow() const { return static_cast<WrapFlow>(rareNonInheritedData->m_wrapFlow); }
    WrapThrough wrapThrough() const { return static_cast<WrapThrough>(rareNonInheritedData->m_wrapThrough); }

    // Apple-specific property getter methods
    EPointerEvents pointerEvents() const { return static_cast<EPointerEvents>(inherited_flags._pointerEvents); }
    const CSSAnimationDataList* animations() const { return rareNonInheritedData->m_animations.get(); }
    const CSSAnimationDataList* transitions() const { return rareNonInheritedData->m_transitions.get(); }

    CSSAnimationDataList* accessAnimations();
    CSSAnimationDataList* accessTransitions();

    bool hasAnimations() const { return rareNonInheritedData->m_animations && rareNonInheritedData->m_animations->size() > 0; }

    // return the first found Animation (including 'all' transitions)
    const CSSAnimationData* transitionForProperty(CSSPropertyID) const;

    ETransformStyle3D transformStyle3D() const { return static_cast<ETransformStyle3D>(rareNonInheritedData->m_transformStyle3D); }
    bool preserves3D() const { return rareNonInheritedData->m_transformStyle3D == TransformStyle3DPreserve3D; }

    EBackfaceVisibility backfaceVisibility() const { return static_cast<EBackfaceVisibility>(rareNonInheritedData->m_backfaceVisibility); }
    float perspective() const { return rareNonInheritedData->m_perspective; }
    bool hasPerspective() const { return rareNonInheritedData->m_perspective > 0; }
    const Length& perspectiveOriginX() const { return rareNonInheritedData->m_perspectiveOriginX; }
    const Length& perspectiveOriginY() const { return rareNonInheritedData->m_perspectiveOriginY; }
    const LengthSize& pageSize() const { return rareNonInheritedData->m_pageSize; }
    PageSizeType pageSizeType() const { return static_cast<PageSizeType>(rareNonInheritedData->m_pageSizeType); }

    // When set, this ensures that styles compare as different. Used during accelerated animations.
    bool isRunningAcceleratedAnimation() const { return rareNonInheritedData->m_runningAcceleratedAnimation; }

    LineBoxContain lineBoxContain() const { return rareInheritedData->m_lineBoxContain; }
    const LineClampValue& lineClamp() const { return rareNonInheritedData->lineClamp; }
    Color tapHighlightColor() const { return rareInheritedData->tapHighlightColor; }
    ETextSecurity textSecurity() const { return static_cast<ETextSecurity>(rareInheritedData->textSecurity); }

    WritingMode writingMode() const { return static_cast<WritingMode>(inherited_flags.m_writingMode); }
    bool isHorizontalWritingMode() const { return WebCore::isHorizontalWritingMode(writingMode()); }
    bool isFlippedLinesWritingMode() const { return WebCore::isFlippedLinesWritingMode(writingMode()); }
    bool isFlippedBlocksWritingMode() const { return WebCore::isFlippedBlocksWritingMode(writingMode()); }

    EImageRendering imageRendering() const { return static_cast<EImageRendering>(rareInheritedData->m_imageRendering); }

    ESpeak speak() const { return static_cast<ESpeak>(rareInheritedData->speak); }

    FilterOperations& mutableFilter() { return rareNonInheritedData.access()->m_filter.access()->m_operations; }
    const FilterOperations& filter() const { return rareNonInheritedData->m_filter->m_operations; }
    bool hasFilter() const { return !rareNonInheritedData->m_filter->m_operations.operations().isEmpty(); }

    blink::WebBlendMode blendMode() const;
    void setBlendMode(blink::WebBlendMode v);
    bool hasBlendMode() const;

    EIsolation isolation() const;
    void setIsolation(EIsolation v);
    bool hasIsolation() const;

    bool shouldPlaceBlockDirectionScrollbarOnLogicalLeft() const { return !isLeftToRightDirection() && isHorizontalWritingMode(); }

    TouchAction touchAction() const { return static_cast<TouchAction>(rareNonInheritedData->m_touchAction); }
    TouchActionDelay touchActionDelay() const { return static_cast<TouchActionDelay>(rareInheritedData->m_touchActionDelay); }

    ScrollBehavior scrollBehavior() const { return static_cast<ScrollBehavior>(rareNonInheritedData->m_scrollBehavior); }

    const Vector<CSSPropertyID>& willChangeProperties() const { return rareNonInheritedData->m_willChange->m_properties; }
    bool willChangeContents() const { return rareNonInheritedData->m_willChange->m_contents; }
    bool willChangeScrollPosition() const { return rareNonInheritedData->m_willChange->m_scrollPosition; }
    bool hasWillChangeCompositingHint() const;
    bool hasWillChangeGpuRasterizationHint() const;

// attribute setter methods

    void setDisplay(EDisplay v) { noninherited_flags._effectiveDisplay = v; }
    void setOriginalDisplay(EDisplay v) { noninherited_flags._originalDisplay = v; }
    void setPosition(EPosition v) { noninherited_flags._position = v; }
    void setFloating(EFloat v) { noninherited_flags._floating = v; }

    void setLeft(Length v) { SET_VAR(surround, offset.m_left, v); }
    void setRight(Length v) { SET_VAR(surround, offset.m_right, v); }
    void setTop(Length v) { SET_VAR(surround, offset.m_top, v); }
    void setBottom(Length v) { SET_VAR(surround, offset.m_bottom, v); }

    void setWidth(Length v) { SET_VAR(m_box, m_width, v); }
    void setHeight(Length v) { SET_VAR(m_box, m_height, v); }

    void setLogicalWidth(Length v)
    {
        if (isHorizontalWritingMode()) {
            SET_VAR(m_box, m_width, v);
        } else {
            SET_VAR(m_box, m_height, v);
        }
    }

    void setLogicalHeight(Length v)
    {
        if (isHorizontalWritingMode()) {
            SET_VAR(m_box, m_height, v);
        } else {
            SET_VAR(m_box, m_width, v);
        }
    }

    void setMinWidth(Length v) { SET_VAR(m_box, m_minWidth, v); }
    void setMaxWidth(Length v) { SET_VAR(m_box, m_maxWidth, v); }
    void setMinHeight(Length v) { SET_VAR(m_box, m_minHeight, v); }
    void setMaxHeight(Length v) { SET_VAR(m_box, m_maxHeight, v); }

    DraggableRegionMode getDraggableRegionMode() const { return rareNonInheritedData->m_draggableRegionMode; }
    void setDraggableRegionMode(DraggableRegionMode v) { SET_VAR(rareNonInheritedData, m_draggableRegionMode, v); }

    void resetBorder() { resetBorderImage(); resetBorderTop(); resetBorderRight(); resetBorderBottom(); resetBorderLeft(); resetBorderRadius(); }
    void resetBorderTop() { SET_VAR(surround, border.m_top, BorderValue()); }
    void resetBorderRight() { SET_VAR(surround, border.m_right, BorderValue()); }
    void resetBorderBottom() { SET_VAR(surround, border.m_bottom, BorderValue()); }
    void resetBorderLeft() { SET_VAR(surround, border.m_left, BorderValue()); }
    void resetBorderImage() { SET_VAR(surround, border.m_image, NinePieceImage()); }
    void resetBorderRadius() { resetBorderTopLeftRadius(); resetBorderTopRightRadius(); resetBorderBottomLeftRadius(); resetBorderBottomRightRadius(); }
    void resetBorderTopLeftRadius() { SET_VAR(surround, border.m_topLeft, initialBorderRadius()); }
    void resetBorderTopRightRadius() { SET_VAR(surround, border.m_topRight, initialBorderRadius()); }
    void resetBorderBottomLeftRadius() { SET_VAR(surround, border.m_bottomLeft, initialBorderRadius()); }
    void resetBorderBottomRightRadius() { SET_VAR(surround, border.m_bottomRight, initialBorderRadius()); }

    void setBackgroundColor(const StyleColor& v) { SET_VAR(m_background, m_color, v); }

    void setBackgroundXPosition(Length length) { SET_VAR(m_background, m_background.m_xPosition, length); }
    void setBackgroundYPosition(Length length) { SET_VAR(m_background, m_background.m_yPosition, length); }
    void setBackgroundSize(EFillSizeType b) { SET_VAR(m_background, m_background.m_sizeType, b); }
    void setBackgroundSizeLength(LengthSize s) { SET_VAR(m_background, m_background.m_sizeLength, s); }

    void setBorderImage(const NinePieceImage& b) { SET_VAR(surround, border.m_image, b); }
    void setBorderImageSource(PassRefPtr<StyleImage>);
    void setBorderImageSlices(LengthBox);
    void setBorderImageWidth(const BorderImageLengthBox&);
    void setBorderImageOutset(const BorderImageLengthBox&);

    void setBorderTopLeftRadius(LengthSize s) { SET_VAR(surround, border.m_topLeft, s); }
    void setBorderTopRightRadius(LengthSize s) { SET_VAR(surround, border.m_topRight, s); }
    void setBorderBottomLeftRadius(LengthSize s) { SET_VAR(surround, border.m_bottomLeft, s); }
    void setBorderBottomRightRadius(LengthSize s) { SET_VAR(surround, border.m_bottomRight, s); }

    void setBorderRadius(LengthSize s)
    {
        setBorderTopLeftRadius(s);
        setBorderTopRightRadius(s);
        setBorderBottomLeftRadius(s);
        setBorderBottomRightRadius(s);
    }
    void setBorderRadius(const IntSize& s)
    {
        setBorderRadius(LengthSize(Length(s.width(), Fixed), Length(s.height(), Fixed)));
    }

    RoundedRect getRoundedBorderFor(const LayoutRect& borderRect, bool includeLogicalLeftEdge = true, bool includeLogicalRightEdge = true) const;
    RoundedRect getRoundedInnerBorderFor(const LayoutRect& borderRect, bool includeLogicalLeftEdge = true, bool includeLogicalRightEdge = true) const;

    RoundedRect getRoundedInnerBorderFor(const LayoutRect& borderRect,
        int topWidth, int bottomWidth, int leftWidth, int rightWidth, bool includeLogicalLeftEdge, bool includeLogicalRightEdge) const;

    void setBorderLeftWidth(unsigned v) { SET_VAR(surround, border.m_left.m_width, v); }
    void setBorderLeftStyle(EBorderStyle v) { SET_VAR(surround, border.m_left.m_style, v); }
    void setBorderLeftColor(const StyleColor& v) { SET_BORDERVALUE_COLOR(surround, border.m_left, v); }
    void setBorderRightWidth(unsigned v) { SET_VAR(surround, border.m_right.m_width, v); }
    void setBorderRightStyle(EBorderStyle v) { SET_VAR(surround, border.m_right.m_style, v); }
    void setBorderRightColor(const StyleColor& v) { SET_BORDERVALUE_COLOR(surround, border.m_right, v); }
    void setBorderTopWidth(unsigned v) { SET_VAR(surround, border.m_top.m_width, v); }
    void setBorderTopStyle(EBorderStyle v) { SET_VAR(surround, border.m_top.m_style, v); }
    void setBorderTopColor(const StyleColor& v) { SET_BORDERVALUE_COLOR(surround, border.m_top, v); }
    void setBorderBottomWidth(unsigned v) { SET_VAR(surround, border.m_bottom.m_width, v); }
    void setBorderBottomStyle(EBorderStyle v) { SET_VAR(surround, border.m_bottom.m_style, v); }
    void setBorderBottomColor(const StyleColor& v) { SET_BORDERVALUE_COLOR(surround, border.m_bottom, v); }

    void setOutlineWidth(unsigned short v) { SET_VAR(m_background, m_outline.m_width, v); }
    void setOutlineStyleIsAuto(OutlineIsAuto isAuto) { SET_VAR(m_background, m_outline.m_isAuto, isAuto); }
    void setOutlineStyle(EBorderStyle v) { SET_VAR(m_background, m_outline.m_style, v); }
    void setOutlineColor(const StyleColor& v) { SET_BORDERVALUE_COLOR(m_background, m_outline, v); }

    void setOverflowX(EOverflow v) { noninherited_flags._overflowX = v; }
    void setOverflowY(EOverflow v) { noninherited_flags._overflowY = v; }
    void setVisibility(EVisibility v) { inherited_flags._visibility = v; }
    void setVerticalAlign(EVerticalAlign v) { noninherited_flags._vertical_align = v; }
    void setVerticalAlignLength(Length length) { setVerticalAlign(LENGTH); SET_VAR(m_box, m_verticalAlign, length); }

    void setHasClip(bool b = true) { SET_VAR(visual, hasClip, b); }
    void setClipLeft(Length v) { SET_VAR(visual, clip.m_left, v); }
    void setClipRight(Length v) { SET_VAR(visual, clip.m_right, v); }
    void setClipTop(Length v) { SET_VAR(visual, clip.m_top, v); }
    void setClipBottom(Length v) { SET_VAR(visual, clip.m_bottom, v); }
    void setClip(Length top, Length right, Length bottom, Length left);
    void setClip(LengthBox box) { SET_VAR(visual, clip, box); }

    void setUnicodeBidi(EUnicodeBidi b) { noninherited_flags._unicodeBidi = b; }

    void setClear(EClear v) { noninherited_flags._clear = v; }
    void setTableLayout(ETableLayout v) { noninherited_flags._table_layout = v; }

    bool setFontDescription(const FontDescription&);
    // Only used for blending font sizes when animating and for text autosizing.
    void setFontSize(float);
    void setFontWeight(FontWeight);

    void setTextAutosizingMultiplier(float v)
    {
        SET_VAR(visual, m_textAutosizingMultiplier, v);
        setFontSize(fontDescription().specifiedSize());
    }

    void setColor(const Color&);
    void setTextIndent(Length v) { SET_VAR(rareInheritedData, indent, v); }
    void setTextIndentLine(TextIndentLine v) { SET_VAR(rareInheritedData, m_textIndentLine, v); }
    void setTextAlign(ETextAlign v) { inherited_flags._text_align = v; }
    void setTextAlignLast(TextAlignLast v) { SET_VAR(rareInheritedData, m_textAlignLast, v); }
    void setTextJustify(TextJustify v) { SET_VAR(rareInheritedData, m_textJustify, v); }
    void setTextTransform(ETextTransform v) { inherited_flags._text_transform = v; }
    void addToTextDecorationsInEffect(TextDecoration v) { inherited_flags._text_decorations |= v; }
    void setTextDecorationsInEffect(TextDecoration v) { inherited_flags._text_decorations = v; }
    void setTextDecoration(TextDecoration v) { SET_VAR(visual, textDecoration, v); }
    void setTextUnderlinePosition(TextUnderlinePosition v) { SET_VAR(rareInheritedData, m_textUnderlinePosition, v); }
    void setTextDecorationStyle(TextDecorationStyle v) { SET_VAR(rareNonInheritedData, m_textDecorationStyle, v); }
    void setDirection(TextDirection v) { inherited_flags._direction = v; }
    void setLineHeight(Length specifiedLineHeight);
    bool setZoom(float);
    void setZoomWithoutReturnValue(float f) { setZoom(f); }
    bool setEffectiveZoom(float);

    void setImageRendering(EImageRendering v) { SET_VAR(rareInheritedData, m_imageRendering, v); }

    void setWhiteSpace(EWhiteSpace v) { inherited_flags._white_space = v; }

    // FIXME: Remove these two and replace them with respective FontBuilder calls.
    void setWordSpacing(float);
    void setLetterSpacing(float);

    void clearBackgroundLayers() { m_background.access()->m_background = FillLayer(BackgroundFillLayer); }
    void inheritBackgroundLayers(const FillLayer& parent) { m_background.access()->m_background = parent; }

    void adjustBackgroundLayers()
    {
        if (backgroundLayers()->next()) {
            accessBackgroundLayers()->cullEmptyLayers();
            accessBackgroundLayers()->fillUnsetProperties();
        }
    }

    void clearMaskLayers() { rareNonInheritedData.access()->m_mask = FillLayer(MaskFillLayer); }
    void inheritMaskLayers(const FillLayer& parent) { rareNonInheritedData.access()->m_mask = parent; }

    void adjustMaskLayers()
    {
        if (maskLayers()->next()) {
            accessMaskLayers()->cullEmptyLayers();
            accessMaskLayers()->fillUnsetProperties();
        }
    }

    void setMaskImage(PassRefPtr<StyleImage> v) { rareNonInheritedData.access()->m_mask.setImage(v); }

    void setMaskBoxImage(const NinePieceImage& b) { SET_VAR(rareNonInheritedData, m_maskBoxImage, b); }
    void setMaskBoxImageSource(PassRefPtr<StyleImage> v) { rareNonInheritedData.access()->m_maskBoxImage.setImage(v); }
    void setMaskBoxImageSlices(LengthBox slices)
    {
        rareNonInheritedData.access()->m_maskBoxImage.setImageSlices(slices);
    }
    void setMaskBoxImageSlicesFill(bool fill)
    {
        rareNonInheritedData.access()->m_maskBoxImage.setFill(fill);
    }
    void setMaskBoxImageWidth(const BorderImageLengthBox& slices)
    {
        rareNonInheritedData.access()->m_maskBoxImage.setBorderSlices(slices);
    }
    void setMaskBoxImageOutset(const BorderImageLengthBox& outset)
    {
        rareNonInheritedData.access()->m_maskBoxImage.setOutset(outset);
    }
    void setMaskXPosition(Length length) { SET_VAR(rareNonInheritedData, m_mask.m_xPosition, length); }
    void setMaskYPosition(Length length) { SET_VAR(rareNonInheritedData, m_mask.m_yPosition, length); }
    void setMaskSize(LengthSize s) { SET_VAR(rareNonInheritedData, m_mask.m_sizeLength, s); }

    void setBorderCollapse(EBorderCollapse collapse) { inherited_flags._border_collapse = collapse; }
    void setHorizontalBorderSpacing(short);
    void setVerticalBorderSpacing(short);
    void setEmptyCells(EEmptyCell v) { inherited_flags._empty_cells = v; }
    void setCaptionSide(ECaptionSide v) { inherited_flags._caption_side = v; }

    void setHasAspectRatio(bool b) { SET_VAR(rareNonInheritedData, m_hasAspectRatio, b); }
    void setAspectRatioDenominator(float v) { SET_VAR(rareNonInheritedData, m_aspectRatioDenominator, v); }
    void setAspectRatioNumerator(float v) { SET_VAR(rareNonInheritedData, m_aspectRatioNumerator, v); }

    void setListStyleType(EListStyleType v) { inherited_flags._list_style_type = v; }
    void setListStyleImage(PassRefPtr<StyleImage>);
    void setListStylePosition(EListStylePosition v) { inherited_flags._list_style_position = v; }

    void resetMargin() { SET_VAR(surround, margin, LengthBox(Fixed)); }
    void setMarginTop(Length v) { SET_VAR(surround, margin.m_top, v); }
    void setMarginBottom(Length v) { SET_VAR(surround, margin.m_bottom, v); }
    void setMarginLeft(Length v) { SET_VAR(surround, margin.m_left, v); }
    void setMarginRight(Length v) { SET_VAR(surround, margin.m_right, v); }
    void setMarginStart(Length);
    void setMarginEnd(Length);

    void resetPadding() { SET_VAR(surround, padding, LengthBox(Auto)); }
    void setPaddingBox(const LengthBox& b) { SET_VAR(surround, padding, b); }
    void setPaddingTop(Length v) { SET_VAR(surround, padding.m_top, v); }
    void setPaddingBottom(Length v) { SET_VAR(surround, padding.m_bottom, v); }
    void setPaddingLeft(Length v) { SET_VAR(surround, padding.m_left, v); }
    void setPaddingRight(Length v) { SET_VAR(surround, padding.m_right, v); }

    void setCursor(ECursor c) { inherited_flags._cursor_style = c; }
    void addCursor(PassRefPtr<StyleImage>, const IntPoint& hotSpot = IntPoint());
    void setCursorList(PassRefPtr<CursorList>);
    void clearCursorList();

    void setInsideLink(EInsideLink insideLink) { inherited_flags._insideLink = insideLink; }
    void setIsLink(bool b) { noninherited_flags.setIsLink(b); }

    PrintColorAdjust printColorAdjust() const { return static_cast<PrintColorAdjust>(inherited_flags.m_printColorAdjust); }
    void setPrintColorAdjust(PrintColorAdjust value) { inherited_flags.m_printColorAdjust = value; }

    bool hasAutoZIndex() const { return m_box->hasAutoZIndex(); }
    void setHasAutoZIndex() { SET_VAR(m_box, m_hasAutoZIndex, true); SET_VAR(m_box, m_zIndex, 0); }
    int zIndex() const { return m_box->zIndex(); }
    void setZIndex(int v) { SET_VAR(m_box, m_hasAutoZIndex, false); SET_VAR(m_box, m_zIndex, v); }

    void setHasAutoWidows() { SET_VAR(rareInheritedData, m_hasAutoWidows, true); SET_VAR(rareInheritedData, widows, initialWidows()); }
    void setWidows(short w) { SET_VAR(rareInheritedData, m_hasAutoWidows, false); SET_VAR(rareInheritedData, widows, w); }

    void setHasAutoOrphans() { SET_VAR(rareInheritedData, m_hasAutoOrphans, true); SET_VAR(rareInheritedData, orphans, initialOrphans()); }
    void setOrphans(short o) { SET_VAR(rareInheritedData, m_hasAutoOrphans, false); SET_VAR(rareInheritedData, orphans, o); }

    // For valid values of page-break-inside see http://www.w3.org/TR/CSS21/page.html#page-break-props
    void setPageBreakInside(EPageBreak b) { ASSERT(b == PBAUTO || b == PBAVOID); noninherited_flags._page_break_inside = b; }
    void setPageBreakBefore(EPageBreak b) { noninherited_flags._page_break_before = b; }
    void setPageBreakAfter(EPageBreak b) { noninherited_flags._page_break_after = b; }

    // CSS3 Setters
    void setOutlineOffset(int v) { SET_VAR(m_background, m_outline.m_offset, v); }
    void setTextShadow(PassRefPtr<ShadowList>);
    void setTextStrokeColor(const StyleColor& c) { SET_VAR(rareInheritedData, textStrokeColor, c); }
    void setTextStrokeWidth(float w) { SET_VAR(rareInheritedData, textStrokeWidth, w); }
    void setTextFillColor(const StyleColor& c) { SET_VAR(rareInheritedData, textFillColor, c); }
    void setOpacity(float f) { float v = clampTo<float>(f, 0, 1); SET_VAR(rareNonInheritedData, opacity, v); }
    void setAppearance(ControlPart a) { SET_VAR(rareNonInheritedData, m_appearance, a); }
    // For valid values of box-align see http://www.w3.org/TR/2009/WD-css3-flexbox-20090723/#alignment
    void setBoxAlign(EBoxAlignment a) { SET_VAR(rareNonInheritedData.access()->m_deprecatedFlexibleBox, align, a); }
    void setBoxDecorationBreak(EBoxDecorationBreak b) { SET_VAR(m_box, m_boxDecorationBreak, b); }
    void setBoxDirection(EBoxDirection d) { inherited_flags._box_direction = d; }
    void setBoxFlex(float f) { SET_VAR(rareNonInheritedData.access()->m_deprecatedFlexibleBox, flex, f); }
    void setBoxFlexGroup(unsigned int fg) { SET_VAR(rareNonInheritedData.access()->m_deprecatedFlexibleBox, flex_group, fg); }
    void setBoxLines(EBoxLines l) { SET_VAR(rareNonInheritedData.access()->m_deprecatedFlexibleBox, lines, l); }
    void setBoxOrdinalGroup(unsigned int og) { SET_VAR(rareNonInheritedData.access()->m_deprecatedFlexibleBox, ordinal_group, og); }
    void setBoxOrient(EBoxOrient o) { SET_VAR(rareNonInheritedData.access()->m_deprecatedFlexibleBox, orient, o); }
    void setBoxPack(EBoxPack p) { SET_VAR(rareNonInheritedData.access()->m_deprecatedFlexibleBox, pack, p); }
    void setBoxShadow(PassRefPtr<ShadowList>);
    void setBoxReflect(PassRefPtr<StyleReflection> reflect) { if (rareNonInheritedData->m_boxReflect != reflect) rareNonInheritedData.access()->m_boxReflect = reflect; }
    void setBoxSizing(EBoxSizing s) { SET_VAR(m_box, m_boxSizing, s); }
    void setFlexGrow(float f) { SET_VAR(rareNonInheritedData.access()->m_flexibleBox, m_flexGrow, f); }
    void setFlexShrink(float f) { SET_VAR(rareNonInheritedData.access()->m_flexibleBox, m_flexShrink, f); }
    void setFlexBasis(Length length) { SET_VAR(rareNonInheritedData.access()->m_flexibleBox, m_flexBasis, length); }
    // We restrict the smallest value to int min + 2 because we use int min and int min + 1 as special values in a hash set.
    void setOrder(int o) { SET_VAR(rareNonInheritedData, m_order, max(std::numeric_limits<int>::min() + 2, o)); }
    void addCallbackSelector(const String& selector);
    void setAlignContent(EAlignContent p) { SET_VAR(rareNonInheritedData, m_alignContent, p); }
    void setAlignItems(ItemPosition a) { SET_VAR(rareNonInheritedData, m_alignItems, a); }
    void setAlignItemsOverflowAlignment(OverflowAlignment overflowAlignment) { SET_VAR(rareNonInheritedData, m_alignItemsOverflowAlignment, overflowAlignment); }
    void setAlignSelf(ItemPosition a) { SET_VAR(rareNonInheritedData, m_alignSelf, a); }
    void setAlignSelfOverflowAlignment(OverflowAlignment overflowAlignment) { SET_VAR(rareNonInheritedData, m_alignSelfOverflowAlignment, overflowAlignment); }
    void setFlexDirection(EFlexDirection direction) { SET_VAR(rareNonInheritedData.access()->m_flexibleBox, m_flexDirection, direction); }
    void setFlexWrap(EFlexWrap w) { SET_VAR(rareNonInheritedData.access()->m_flexibleBox, m_flexWrap, w); }
    void setJustifyContent(EJustifyContent p) { SET_VAR(rareNonInheritedData, m_justifyContent, p); }
    void setJustifySelf(ItemPosition justifySelf) { SET_VAR(rareNonInheritedData, m_justifySelf, justifySelf); }
    void setJustifySelfOverflowAlignment(OverflowAlignment overflowAlignment) { SET_VAR(rareNonInheritedData, m_justifySelfOverflowAlignment, overflowAlignment); }
    void setGridAutoColumns(const GridTrackSize& length) { SET_VAR(rareNonInheritedData.access()->m_grid, m_gridAutoColumns, length); }
    void setGridAutoRows(const GridTrackSize& length) { SET_VAR(rareNonInheritedData.access()->m_grid, m_gridAutoRows, length); }
    void setGridTemplateColumns(const Vector<GridTrackSize>& lengths) { SET_VAR(rareNonInheritedData.access()->m_grid, m_gridTemplateColumns, lengths); }
    void setGridTemplateRows(const Vector<GridTrackSize>& lengths) { SET_VAR(rareNonInheritedData.access()->m_grid, m_gridTemplateRows, lengths); }
    void setNamedGridColumnLines(const NamedGridLinesMap& namedGridColumnLines) { SET_VAR(rareNonInheritedData.access()->m_grid, m_namedGridColumnLines, namedGridColumnLines); }
    void setNamedGridRowLines(const NamedGridLinesMap& namedGridRowLines) { SET_VAR(rareNonInheritedData.access()->m_grid, m_namedGridRowLines, namedGridRowLines); }
    void setOrderedNamedGridColumnLines(const OrderedNamedGridLines& orderedNamedGridColumnLines) { SET_VAR(rareNonInheritedData.access()->m_grid, m_orderedNamedGridColumnLines, orderedNamedGridColumnLines); }
    void setOrderedNamedGridRowLines(const OrderedNamedGridLines& orderedNamedGridRowLines) { SET_VAR(rareNonInheritedData.access()->m_grid, m_orderedNamedGridRowLines, orderedNamedGridRowLines); }
    void setNamedGridArea(const NamedGridAreaMap& namedGridArea) { SET_VAR(rareNonInheritedData.access()->m_grid, m_namedGridArea, namedGridArea); }
    void setNamedGridAreaRowCount(size_t rowCount) { SET_VAR(rareNonInheritedData.access()->m_grid, m_namedGridAreaRowCount, rowCount); }
    void setNamedGridAreaColumnCount(size_t columnCount) { SET_VAR(rareNonInheritedData.access()->m_grid, m_namedGridAreaColumnCount, columnCount); }
    void setGridAutoFlow(GridAutoFlow flow) { SET_VAR(rareNonInheritedData.access()->m_grid, m_gridAutoFlow, flow); }

    void setGridColumnStart(const GridPosition& columnStartPosition) { SET_VAR(rareNonInheritedData.access()->m_gridItem, m_gridColumnStart, columnStartPosition); }
    void setGridColumnEnd(const GridPosition& columnEndPosition) { SET_VAR(rareNonInheritedData.access()->m_gridItem, m_gridColumnEnd, columnEndPosition); }
    void setGridRowStart(const GridPosition& rowStartPosition) { SET_VAR(rareNonInheritedData.access()->m_gridItem, m_gridRowStart, rowStartPosition); }
    void setGridRowEnd(const GridPosition& rowEndPosition) { SET_VAR(rareNonInheritedData.access()->m_gridItem, m_gridRowEnd, rowEndPosition); }

    void setMarqueeIncrement(Length f) { SET_VAR(rareNonInheritedData.access()->m_marquee, increment, f); }
    void setMarqueeSpeed(int f) { SET_VAR(rareNonInheritedData.access()->m_marquee, speed, f); }
    void setMarqueeDirection(EMarqueeDirection d) { SET_VAR(rareNonInheritedData.access()->m_marquee, direction, d); }
    void setMarqueeBehavior(EMarqueeBehavior b) { SET_VAR(rareNonInheritedData.access()->m_marquee, behavior, b); }
    void setMarqueeLoopCount(int i) { SET_VAR(rareNonInheritedData.access()->m_marquee, loops, i); }
    void setUserModify(EUserModify u) { SET_VAR(rareInheritedData, userModify, u); }
    void setUserDrag(EUserDrag d) { SET_VAR(rareNonInheritedData, userDrag, d); }
    void setUserSelect(EUserSelect s) { SET_VAR(rareInheritedData, userSelect, s); }
    void setTextOverflow(TextOverflow overflow) { SET_VAR(rareNonInheritedData, textOverflow, overflow); }
    void setMarginBeforeCollapse(EMarginCollapse c) { SET_VAR(rareNonInheritedData, marginBeforeCollapse, c); }
    void setMarginAfterCollapse(EMarginCollapse c) { SET_VAR(rareNonInheritedData, marginAfterCollapse, c); }
    void setWordBreak(EWordBreak b) { SET_VAR(rareInheritedData, wordBreak, b); }
    void setOverflowWrap(EOverflowWrap b) { SET_VAR(rareInheritedData, overflowWrap, b); }
    void setLineBreak(LineBreak b) { SET_VAR(rareInheritedData, lineBreak, b); }
    void setHighlight(const AtomicString& h) { SET_VAR(rareInheritedData, highlight, h); }
    void setHyphens(Hyphens h) { SET_VAR(rareInheritedData, hyphens, h); }
    void setHyphenationString(const AtomicString& h) { SET_VAR(rareInheritedData, hyphenationString, h); }
    void setLocale(const AtomicString& locale) { SET_VAR(rareInheritedData, locale, locale); }
    void setBorderFit(EBorderFit b) { SET_VAR(rareNonInheritedData, m_borderFit, b); }
    void setResize(EResize r) { SET_VAR(rareInheritedData, resize, r); }
    void setColumnAxis(ColumnAxis axis) { SET_VAR(rareNonInheritedData.access()->m_multiCol, m_axis, axis); }
    void setColumnProgression(ColumnProgression progression) { SET_VAR(rareNonInheritedData.access()->m_multiCol, m_progression, progression); }
    void setColumnWidth(float f) { SET_VAR(rareNonInheritedData.access()->m_multiCol, m_autoWidth, false); SET_VAR(rareNonInheritedData.access()->m_multiCol, m_width, f); }
    void setHasAutoColumnWidth() { SET_VAR(rareNonInheritedData.access()->m_multiCol, m_autoWidth, true); SET_VAR(rareNonInheritedData.access()->m_multiCol, m_width, 0); }
    void setColumnCount(unsigned short c) { SET_VAR(rareNonInheritedData.access()->m_multiCol, m_autoCount, false); SET_VAR(rareNonInheritedData.access()->m_multiCol, m_count, c); }
    void setHasAutoColumnCount() { SET_VAR(rareNonInheritedData.access()->m_multiCol, m_autoCount, true); SET_VAR(rareNonInheritedData.access()->m_multiCol, m_count, 0); }
    void setColumnFill(ColumnFill columnFill) { SET_VAR(rareNonInheritedData.access()->m_multiCol, m_fill, columnFill); }
    void setColumnGap(float f) { SET_VAR(rareNonInheritedData.access()->m_multiCol, m_normalGap, false); SET_VAR(rareNonInheritedData.access()->m_multiCol, m_gap, f); }
    void setHasNormalColumnGap() { SET_VAR(rareNonInheritedData.access()->m_multiCol, m_normalGap, true); SET_VAR(rareNonInheritedData.access()->m_multiCol, m_gap, 0); }
    void setColumnRuleColor(const StyleColor& c) { SET_BORDERVALUE_COLOR(rareNonInheritedData.access()->m_multiCol, m_rule, c); }
    void setColumnRuleStyle(EBorderStyle b) { SET_VAR(rareNonInheritedData.access()->m_multiCol, m_rule.m_style, b); }
    void setColumnRuleWidth(unsigned short w) { SET_VAR(rareNonInheritedData.access()->m_multiCol, m_rule.m_width, w); }
    void resetColumnRule() { SET_VAR(rareNonInheritedData.access()->m_multiCol, m_rule, BorderValue()); }
    void setColumnSpan(ColumnSpan columnSpan) { SET_VAR(rareNonInheritedData.access()->m_multiCol, m_columnSpan, columnSpan); }
    void setColumnBreakBefore(EPageBreak p) { SET_VAR(rareNonInheritedData.access()->m_multiCol, m_breakBefore, p); }
    // For valid values of column-break-inside see http://www.w3.org/TR/css3-multicol/#break-before-break-after-break-inside
    void setColumnBreakInside(EPageBreak p) { ASSERT(p == PBAUTO || p == PBAVOID); SET_VAR(rareNonInheritedData.access()->m_multiCol, m_breakInside, p); }
    void setColumnBreakAfter(EPageBreak p) { SET_VAR(rareNonInheritedData.access()->m_multiCol, m_breakAfter, p); }
    void inheritColumnPropertiesFrom(RenderStyle* parent) { rareNonInheritedData.access()->m_multiCol = parent->rareNonInheritedData->m_multiCol; }
    void setTransform(const TransformOperations& ops) { SET_VAR(rareNonInheritedData.access()->m_transform, m_operations, ops); }
    void setTransformOriginX(Length l) { SET_VAR(rareNonInheritedData.access()->m_transform, m_x, l); }
    void setTransformOriginY(Length l) { SET_VAR(rareNonInheritedData.access()->m_transform, m_y, l); }
    void setTransformOriginZ(float f) { SET_VAR(rareNonInheritedData.access()->m_transform, m_z, f); }
    void setSpeak(ESpeak s) { SET_VAR(rareInheritedData, speak, s); }
    void setTextCombine(TextCombine v) { SET_VAR(rareNonInheritedData, m_textCombine, v); }
    void setTextDecorationColor(const StyleColor& c) { SET_VAR(rareNonInheritedData, m_textDecorationColor, c); }
    void setTextEmphasisColor(const StyleColor& c) { SET_VAR(rareInheritedData, textEmphasisColor, c); }
    void setTextEmphasisFill(TextEmphasisFill fill) { SET_VAR(rareInheritedData, textEmphasisFill, fill); }
    void setTextEmphasisMark(TextEmphasisMark mark) { SET_VAR(rareInheritedData, textEmphasisMark, mark); }
    void setTextEmphasisCustomMark(const AtomicString& mark) { SET_VAR(rareInheritedData, textEmphasisCustomMark, mark); }
    void setTextEmphasisPosition(TextEmphasisPosition position) { SET_VAR(rareInheritedData, textEmphasisPosition, position); }
    bool setTextOrientation(TextOrientation);

    void setObjectFit(ObjectFit f) { SET_VAR(rareNonInheritedData, m_objectFit, f); }
    void setObjectPosition(LengthPoint position) { SET_VAR(rareNonInheritedData, m_objectPosition, position); }

    void setRubyPosition(RubyPosition position) { SET_VAR(rareInheritedData, m_rubyPosition, position); }

    void setFilter(const FilterOperations& ops) { SET_VAR(rareNonInheritedData.access()->m_filter, m_operations, ops); }

    void setTabSize(unsigned size) { SET_VAR(rareInheritedData, m_tabSize, size); }

    // End CSS3 Setters

    void setWrapFlow(WrapFlow wrapFlow) { SET_VAR(rareNonInheritedData, m_wrapFlow, wrapFlow); }
    void setWrapThrough(WrapThrough wrapThrough) { SET_VAR(rareNonInheritedData, m_wrapThrough, wrapThrough); }

    // Apple-specific property setters
    void setPointerEvents(EPointerEvents p) { inherited_flags._pointerEvents = p; }

    void clearAnimations()
    {
        rareNonInheritedData.access()->m_animations.clear();
    }

    void clearTransitions()
    {
        rareNonInheritedData.access()->m_transitions.clear();
    }

    void adjustAnimations();
    void adjustTransitions();

    void setTransformStyle3D(ETransformStyle3D b) { SET_VAR(rareNonInheritedData, m_transformStyle3D, b); }
    void setBackfaceVisibility(EBackfaceVisibility b) { SET_VAR(rareNonInheritedData, m_backfaceVisibility, b); }
    void setPerspective(float p) { SET_VAR(rareNonInheritedData, m_perspective, p); }
    void setPerspectiveOriginX(Length l) { SET_VAR(rareNonInheritedData, m_perspectiveOriginX, l); }
    void setPerspectiveOriginY(Length l) { SET_VAR(rareNonInheritedData, m_perspectiveOriginY, l); }
    void setPageSize(LengthSize s) { SET_VAR(rareNonInheritedData, m_pageSize, s); }
    void setPageSizeType(PageSizeType t) { SET_VAR(rareNonInheritedData, m_pageSizeType, t); }
    void resetPageSizeType() { SET_VAR(rareNonInheritedData, m_pageSizeType, PAGE_SIZE_AUTO); }

    void setIsRunningAcceleratedAnimation(bool b = true) { SET_VAR(rareNonInheritedData, m_runningAcceleratedAnimation, b); }

    void setLineBoxContain(LineBoxContain c) { SET_VAR(rareInheritedData, m_lineBoxContain, c); }
    void setLineClamp(LineClampValue c) { SET_VAR(rareNonInheritedData, lineClamp, c); }
    void setTapHighlightColor(const Color& c) { SET_VAR(rareInheritedData, tapHighlightColor, c); }
    void setTextSecurity(ETextSecurity aTextSecurity) { SET_VAR(rareInheritedData, textSecurity, aTextSecurity); }
    void setTouchAction(TouchAction t) { SET_VAR(rareNonInheritedData, m_touchAction, t); }
    void setTouchActionDelay(TouchActionDelay t) { SET_VAR(rareInheritedData, m_touchActionDelay, t); }

    void setScrollBehavior(ScrollBehavior b) { SET_VAR(rareNonInheritedData, m_scrollBehavior, b); }

    void setWillChangeProperties(const Vector<CSSPropertyID>& properties) { SET_VAR(rareNonInheritedData.access()->m_willChange, m_properties, properties); }
    void setWillChangeContents(bool b) { SET_VAR(rareNonInheritedData.access()->m_willChange, m_contents, b); }
    void setWillChangeScrollPosition(bool b) { SET_VAR(rareNonInheritedData.access()->m_willChange, m_scrollPosition, b); }

    const SVGRenderStyle* svgStyle() const { return m_svgStyle.get(); }
    SVGRenderStyle* accessSVGStyle() { return m_svgStyle.access(); }

    const SVGPaint::SVGPaintType& fillPaintType() const { return svgStyle()->fillPaintType(); }
    Color fillPaintColor() const { return svgStyle()->fillPaintColor(); }
    void setFillPaintColor(const Color& c) { accessSVGStyle()->setFillPaint(SVGPaint::SVG_PAINTTYPE_RGBCOLOR, c, ""); }
    float fillOpacity() const { return svgStyle()->fillOpacity(); }
    void setFillOpacity(float f) { accessSVGStyle()->setFillOpacity(f); }

    const SVGPaint::SVGPaintType& strokePaintType() const { return svgStyle()->strokePaintType(); }
    Color strokePaintColor() const { return svgStyle()->strokePaintColor(); }
    void setStrokePaintColor(const Color& c) { accessSVGStyle()->setStrokePaint(SVGPaint::SVG_PAINTTYPE_RGBCOLOR, c, ""); }
    float strokeOpacity() const { return svgStyle()->strokeOpacity(); }
    void setStrokeOpacity(float f) { accessSVGStyle()->setStrokeOpacity(f); }
    PassRefPtr<SVGLength> strokeWidth() const { return svgStyle()->strokeWidth(); }
    void setStrokeWidth(PassRefPtr<SVGLength> w) { accessSVGStyle()->setStrokeWidth(w); }
    PassRefPtr<SVGLengthList> strokeDashArray() const { return svgStyle()->strokeDashArray(); }
    void setStrokeDashArray(PassRefPtr<SVGLengthList> array) { accessSVGStyle()->setStrokeDashArray(array); }
    PassRefPtr<SVGLength> strokeDashOffset() const { return svgStyle()->strokeDashOffset(); }
    void setStrokeDashOffset(PassRefPtr<SVGLength> d) { accessSVGStyle()->setStrokeDashOffset(d); }
    float strokeMiterLimit() const { return svgStyle()->strokeMiterLimit(); }
    void setStrokeMiterLimit(float f) { accessSVGStyle()->setStrokeMiterLimit(f); }

    float floodOpacity() const { return svgStyle()->floodOpacity(); }
    void setFloodOpacity(float f) { accessSVGStyle()->setFloodOpacity(f); }

    float stopOpacity() const { return svgStyle()->stopOpacity(); }
    void setStopOpacity(float f) { accessSVGStyle()->setStopOpacity(f); }

    void setStopColor(const Color& c) { accessSVGStyle()->setStopColor(c); }
    void setFloodColor(const Color& c) { accessSVGStyle()->setFloodColor(c); }
    void setLightingColor(const Color& c) { accessSVGStyle()->setLightingColor(c); }

    PassRefPtr<SVGLength> baselineShiftValue() const { return svgStyle()->baselineShiftValue(); }
    void setBaselineShiftValue(PassRefPtr<SVGLength> s) { accessSVGStyle()->setBaselineShiftValue(s); }
    PassRefPtr<SVGLength> kerning() const { return svgStyle()->kerning(); }
    void setKerning(PassRefPtr<SVGLength> k) { accessSVGStyle()->setKerning(k); }

    void setShapeInside(PassRefPtr<ShapeValue> value)
    {
        if (rareNonInheritedData->m_shapeInside == value)
            return;
        rareNonInheritedData.access()->m_shapeInside = value;
    }
    ShapeValue* shapeInside() const { return rareNonInheritedData->m_shapeInside.get(); }
    ShapeValue* resolvedShapeInside() const
    {
        ShapeValue* shapeInside = this->shapeInside();
        if (shapeInside && shapeInside->type() == ShapeValue::Outside)
            return shapeOutside();
        return shapeInside;
    }

    void setShapeOutside(PassRefPtr<ShapeValue> value)
    {
        if (rareNonInheritedData->m_shapeOutside == value)
            return;
        rareNonInheritedData.access()->m_shapeOutside = value;
    }
    ShapeValue* shapeOutside() const { return rareNonInheritedData->m_shapeOutside.get(); }

    static ShapeValue* initialShapeInside() { return 0; }
    static ShapeValue* initialShapeOutside() { return 0; }

    void setClipPath(PassRefPtr<ClipPathOperation> operation)
    {
        if (rareNonInheritedData->m_clipPath != operation)
            rareNonInheritedData.access()->m_clipPath = operation;
    }
    ClipPathOperation* clipPath() const { return rareNonInheritedData->m_clipPath.get(); }

    static ClipPathOperation* initialClipPath() { return 0; }

    const Length& shapePadding() const { return rareNonInheritedData->m_shapePadding; }
    void setShapePadding(Length shapePadding) { SET_VAR(rareNonInheritedData, m_shapePadding, shapePadding); }
    static Length initialShapePadding() { return Length(0, Fixed); }

    const Length& shapeMargin() const { return rareNonInheritedData->m_shapeMargin; }
    void setShapeMargin(Length shapeMargin) { SET_VAR(rareNonInheritedData, m_shapeMargin, shapeMargin); }
    static Length initialShapeMargin() { return Length(0, Fixed); }

    float shapeImageThreshold() const { return rareNonInheritedData->m_shapeImageThreshold; }
    void setShapeImageThreshold(float shapeImageThreshold)
    {
        float clampedShapeImageThreshold = clampTo<float>(shapeImageThreshold, 0, 1);
        SET_VAR(rareNonInheritedData, m_shapeImageThreshold, clampedShapeImageThreshold);
    }
    static float initialShapeImageThreshold() { return 0; }

    bool hasContent() const { return contentData(); }
    const ContentData* contentData() const { return rareNonInheritedData->m_content.get(); }
    bool contentDataEquivalent(const RenderStyle* otherStyle) const { return const_cast<RenderStyle*>(this)->rareNonInheritedData->contentDataEquivalent(*const_cast<RenderStyle*>(otherStyle)->rareNonInheritedData); }
    void clearContent();
    void setContent(const String&, bool add = false);
    void setContent(PassRefPtr<StyleImage>, bool add = false);
    void setContent(PassOwnPtr<CounterContent>, bool add = false);
    void setContent(QuoteType, bool add = false);

    const CounterDirectiveMap* counterDirectives() const;
    CounterDirectiveMap& accessCounterDirectives();
    const CounterDirectives getCounterDirectives(const AtomicString& identifier) const;

    QuotesData* quotes() const { return rareInheritedData->quotes.get(); }
    void setQuotes(PassRefPtr<QuotesData>);

    const AtomicString& hyphenString() const;

    bool inheritedNotEqual(const RenderStyle*) const;
    bool inheritedDataShared(const RenderStyle*) const;

    StyleDifference diff(const RenderStyle*, unsigned& changedContextSensitiveProperties) const;

    bool isDisplayReplacedType() const { return isDisplayReplacedType(display()); }
    bool isDisplayInlineType() const { return isDisplayInlineType(display()); }
    bool isOriginalDisplayInlineType() const { return isDisplayInlineType(originalDisplay()); }

    bool setWritingMode(WritingMode v)
    {
        if (v == writingMode())
            return false;

        inherited_flags.m_writingMode = v;
        return true;
    }

    // A unique style is one that has matches something that makes it impossible to share.
    bool unique() const { return noninherited_flags.unique; }
    void setUnique() { noninherited_flags.unique = true; }

    bool isSharable() const;

    bool emptyState() const { return noninherited_flags.emptyState; }
    void setEmptyState(bool b) { setUnique(); noninherited_flags.emptyState = b; }
    bool firstChildState() const { return noninherited_flags.firstChildState; }
    void setFirstChildState() { setUnique(); noninherited_flags.firstChildState = true; }
    bool lastChildState() const { return noninherited_flags.lastChildState; }
    void setLastChildState() { setUnique(); noninherited_flags.lastChildState = true; }

    StyleColor visitedDependentDecorationColor() const;
    Color visitedDependentColor(int colorProperty) const;

    void setHasExplicitlyInheritedProperties() { noninherited_flags.explicitInheritance = true; }
    bool hasExplicitlyInheritedProperties() const { return noninherited_flags.explicitInheritance; }

    void setHasCurrentColor() { noninherited_flags.currentColor = true; }
    bool hasCurrentColor() const { return noninherited_flags.currentColor; }

    // Initial values for all the properties
    static EBorderCollapse initialBorderCollapse() { return BSEPARATE; }
    static EBorderStyle initialBorderStyle() { return BNONE; }
    static OutlineIsAuto initialOutlineStyleIsAuto() { return AUTO_OFF; }
    static NinePieceImage initialNinePieceImage() { return NinePieceImage(); }
    static LengthSize initialBorderRadius() { return LengthSize(Length(0, Fixed), Length(0, Fixed)); }
    static ECaptionSide initialCaptionSide() { return CAPTOP; }
    static EClear initialClear() { return CNONE; }
    static ColumnAxis initialColumnAxis() { return AutoColumnAxis; }
    static ColumnProgression initialColumnProgression() { return NormalColumnProgression; }
    static TextDirection initialDirection() { return LTR; }
    static WritingMode initialWritingMode() { return TopToBottomWritingMode; }
    static TextCombine initialTextCombine() { return TextCombineNone; }
    static TextOrientation initialTextOrientation() { return TextOrientationVerticalRight; }
    static ObjectFit initialObjectFit() { return ObjectFitFill; }
    static LengthPoint initialObjectPosition() { return LengthPoint(Length(50.0, Percent), Length(50.0, Percent)); }
    static EDisplay initialDisplay() { return INLINE; }
    static EEmptyCell initialEmptyCells() { return SHOW; }
    static EFloat initialFloating() { return NoFloat; }
    static EListStylePosition initialListStylePosition() { return OUTSIDE; }
    static EListStyleType initialListStyleType() { return Disc; }
    static EOverflow initialOverflowX() { return OVISIBLE; }
    static EOverflow initialOverflowY() { return OVISIBLE; }
    static EPageBreak initialPageBreak() { return PBAUTO; }
    static EPosition initialPosition() { return StaticPosition; }
    static ETableLayout initialTableLayout() { return TAUTO; }
    static EUnicodeBidi initialUnicodeBidi() { return UBNormal; }
    static ETextTransform initialTextTransform() { return TTNONE; }
    static EVisibility initialVisibility() { return VISIBLE; }
    static EWhiteSpace initialWhiteSpace() { return NORMAL; }
    static short initialHorizontalBorderSpacing() { return 0; }
    static short initialVerticalBorderSpacing() { return 0; }
    static ECursor initialCursor() { return CURSOR_AUTO; }
    static Color initialColor() { return Color::black; }
    static StyleImage* initialListStyleImage() { return 0; }
    static unsigned initialBorderWidth() { return 3; }
    static unsigned short initialColumnRuleWidth() { return 3; }
    static unsigned short initialOutlineWidth() { return 3; }
    static float initialLetterWordSpacing() { return 0.0f; }
    static Length initialSize() { return Length(); }
    static Length initialMinSize() { return Length(Fixed); }
    static Length initialMaxSize() { return Length(Undefined); }
    static Length initialOffset() { return Length(); }
    static Length initialMargin() { return Length(Fixed); }
    static Length initialPadding() { return Length(Fixed); }
    static Length initialTextIndent() { return Length(Fixed); }
    static TextIndentLine initialTextIndentLine() { return TextIndentFirstLine; }
    static EVerticalAlign initialVerticalAlign() { return BASELINE; }
    static short initialWidows() { return 2; }
    static short initialOrphans() { return 2; }
    static Length initialLineHeight() { return Length(-100.0, Percent); }
    static ETextAlign initialTextAlign() { return TASTART; }
    static TextAlignLast initialTextAlignLast() { return TextAlignLastAuto; }
    static TextJustify initialTextJustify() { return TextJustifyAuto; }
    static TextDecoration initialTextDecoration() { return TextDecorationNone; }
    static TextUnderlinePosition initialTextUnderlinePosition() { return TextUnderlinePositionAuto; }
    static TextDecorationStyle initialTextDecorationStyle() { return TextDecorationStyleSolid; }
    static float initialZoom() { return 1.0f; }
    static int initialOutlineOffset() { return 0; }
    static float initialOpacity() { return 1.0f; }
    static EBoxAlignment initialBoxAlign() { return BSTRETCH; }
    static EBoxDecorationBreak initialBoxDecorationBreak() { return DSLICE; }
    static EBoxDirection initialBoxDirection() { return BNORMAL; }
    static EBoxLines initialBoxLines() { return SINGLE; }
    static EBoxOrient initialBoxOrient() { return HORIZONTAL; }
    static EBoxPack initialBoxPack() { return Start; }
    static float initialBoxFlex() { return 0.0f; }
    static unsigned int initialBoxFlexGroup() { return 1; }
    static unsigned int initialBoxOrdinalGroup() { return 1; }
    static EBoxSizing initialBoxSizing() { return CONTENT_BOX; }
    static StyleReflection* initialBoxReflect() { return 0; }
    static float initialFlexGrow() { return 0; }
    static float initialFlexShrink() { return 1; }
    static Length initialFlexBasis() { return Length(Auto); }
    static int initialOrder() { return 0; }
    static EAlignContent initialAlignContent() { return AlignContentStretch; }
    static ItemPosition initialAlignItems() { return ItemPositionStretch; }
    static OverflowAlignment initialAlignItemsOverflowAlignment() { return OverflowAlignmentDefault; }
    static ItemPosition initialAlignSelf() { return ItemPositionAuto; }
    static OverflowAlignment initialAlignSelfOverflowAlignment() { return OverflowAlignmentDefault; }
    static EFlexDirection initialFlexDirection() { return FlowRow; }
    static EFlexWrap initialFlexWrap() { return FlexNoWrap; }
    static EJustifyContent initialJustifyContent() { return JustifyFlexStart; }
    static ItemPosition initialJustifySelf() { return ItemPositionAuto; }
    static OverflowAlignment initialJustifySelfOverflowAlignment() { return OverflowAlignmentDefault; }
    static int initialMarqueeLoopCount() { return -1; }
    static int initialMarqueeSpeed() { return 85; }
    static Length initialMarqueeIncrement() { return Length(6, Fixed); }
    static EMarqueeBehavior initialMarqueeBehavior() { return MSCROLL; }
    static EMarqueeDirection initialMarqueeDirection() { return MAUTO; }
    static EUserModify initialUserModify() { return READ_ONLY; }
    static EUserDrag initialUserDrag() { return DRAG_AUTO; }
    static EUserSelect initialUserSelect() { return SELECT_TEXT; }
    static TextOverflow initialTextOverflow() { return TextOverflowClip; }
    static EMarginCollapse initialMarginBeforeCollapse() { return MCOLLAPSE; }
    static EMarginCollapse initialMarginAfterCollapse() { return MCOLLAPSE; }
    static EWordBreak initialWordBreak() { return NormalWordBreak; }
    static EOverflowWrap initialOverflowWrap() { return NormalOverflowWrap; }
    static LineBreak initialLineBreak() { return LineBreakAuto; }
    static const AtomicString& initialHighlight() { return nullAtom; }
    static ESpeak initialSpeak() { return SpeakNormal; }
    static const AtomicString& initialHyphenationString() { return nullAtom; }
    static const AtomicString& initialLocale() { return nullAtom; }
    static EBorderFit initialBorderFit() { return BorderFitBorder; }
    static EResize initialResize() { return RESIZE_NONE; }
    static ControlPart initialAppearance() { return NoControlPart; }
    static bool initialHasAspectRatio() { return false; }
    static float initialAspectRatioDenominator() { return 1; }
    static float initialAspectRatioNumerator() { return 1; }
    static Order initialRTLOrdering() { return LogicalOrder; }
    static float initialTextStrokeWidth() { return 0; }
    static unsigned short initialColumnCount() { return 1; }
    static ColumnFill initialColumnFill() { return ColumnFillBalance; }
    static ColumnSpan initialColumnSpan() { return ColumnSpanNone; }
    static const TransformOperations& initialTransform() { DEFINE_STATIC_LOCAL(TransformOperations, ops, ()); return ops; }
    static Length initialTransformOriginX() { return Length(50.0, Percent); }
    static Length initialTransformOriginY() { return Length(50.0, Percent); }
    static EPointerEvents initialPointerEvents() { return PE_AUTO; }
    static float initialTransformOriginZ() { return 0; }
    static ETransformStyle3D initialTransformStyle3D() { return TransformStyle3DFlat; }
    static EBackfaceVisibility initialBackfaceVisibility() { return BackfaceVisibilityVisible; }
    static float initialPerspective() { return 0; }
    static Length initialPerspectiveOriginX() { return Length(50.0, Percent); }
    static Length initialPerspectiveOriginY() { return Length(50.0, Percent); }
    static Color initialBackgroundColor() { return Color::transparent; }
    static Color initialTextEmphasisColor() { return TextEmphasisFillFilled; }
    static TextEmphasisFill initialTextEmphasisFill() { return TextEmphasisFillFilled; }
    static TextEmphasisMark initialTextEmphasisMark() { return TextEmphasisMarkNone; }
    static const AtomicString& initialTextEmphasisCustomMark() { return nullAtom; }
    static TextEmphasisPosition initialTextEmphasisPosition() { return TextEmphasisPositionOver; }
    static RubyPosition initialRubyPosition() { return RubyPositionBefore; }
    static LineBoxContain initialLineBoxContain() { return LineBoxContainBlock | LineBoxContainInline | LineBoxContainReplaced; }
    static ImageOrientationEnum initialImageOrientation() { return OriginTopLeft; }
    static EImageRendering initialImageRendering() { return ImageRenderingAuto; }
    static ImageResolutionSource initialImageResolutionSource() { return ImageResolutionSpecified; }
    static ImageResolutionSnap initialImageResolutionSnap() { return ImageResolutionNoSnap; }
    static float initialImageResolution() { return 1; }
    static StyleImage* initialBorderImageSource() { return 0; }
    static StyleImage* initialMaskBoxImageSource() { return 0; }
    static PrintColorAdjust initialPrintColorAdjust() { return PrintColorAdjustEconomy; }
    static TouchAction initialTouchAction() { return TouchActionAuto; }
    static TouchActionDelay initialTouchActionDelay() { return TouchActionDelayScript; }
    static ShadowList* initialBoxShadow() { return 0; }
    static ShadowList* initialTextShadow() { return 0; }
    static ScrollBehavior initialScrollBehavior() { return ScrollBehaviorInstant; }

    // The initial value is 'none' for grid tracks.
    static Vector<GridTrackSize> initialGridTemplateColumns() { return Vector<GridTrackSize>(); }
    static Vector<GridTrackSize> initialGridTemplateRows() { return Vector<GridTrackSize>(); }

    static GridAutoFlow initialGridAutoFlow() { return AutoFlowNone; }

    static GridTrackSize initialGridAutoColumns() { return GridTrackSize(Length(Auto)); }
    static GridTrackSize initialGridAutoRows() { return GridTrackSize(Length(Auto)); }

    static NamedGridLinesMap initialNamedGridColumnLines() { return NamedGridLinesMap(); }
    static NamedGridLinesMap initialNamedGridRowLines() { return NamedGridLinesMap(); }

    static OrderedNamedGridLines initialOrderedNamedGridColumnLines() { return OrderedNamedGridLines(); }
    static OrderedNamedGridLines initialOrderedNamedGridRowLines() { return OrderedNamedGridLines(); }

    static NamedGridAreaMap initialNamedGridArea() { return NamedGridAreaMap(); }
    static size_t initialNamedGridAreaCount() { return 0; }

    // 'auto' is the default.
    static GridPosition initialGridColumnStart() { return GridPosition(); }
    static GridPosition initialGridColumnEnd() { return GridPosition(); }
    static GridPosition initialGridRowStart() { return GridPosition(); }
    static GridPosition initialGridRowEnd() { return GridPosition(); }

    static unsigned initialTabSize() { return 8; }

    static WrapFlow initialWrapFlow() { return WrapFlowAuto; }
    static WrapThrough initialWrapThrough() { return WrapThroughWrap; }

    static QuotesData* initialQuotes() { return 0; }

    // Keep these at the end.
    // FIXME: Why? Seems these should all be one big sorted list.
    static LineClampValue initialLineClamp() { return LineClampValue(); }
    static ETextSecurity initialTextSecurity() { return TSNONE; }
    static Color initialTapHighlightColor();
    static const FilterOperations& initialFilter() { DEFINE_STATIC_LOCAL(FilterOperations, ops, ()); return ops; }
    static blink::WebBlendMode initialBlendMode() { return blink::WebBlendModeNormal; }
    static EIsolation initialIsolation() { return IsolationAuto; }
private:
    void setVisitedLinkColor(const Color&);
    void setVisitedLinkBackgroundColor(const StyleColor& v) { SET_VAR(rareNonInheritedData, m_visitedLinkBackgroundColor, v); }
    void setVisitedLinkBorderLeftColor(const StyleColor& v) { SET_VAR(rareNonInheritedData, m_visitedLinkBorderLeftColor, v); }
    void setVisitedLinkBorderRightColor(const StyleColor& v) { SET_VAR(rareNonInheritedData, m_visitedLinkBorderRightColor, v); }
    void setVisitedLinkBorderBottomColor(const StyleColor& v) { SET_VAR(rareNonInheritedData, m_visitedLinkBorderBottomColor, v); }
    void setVisitedLinkBorderTopColor(const StyleColor& v) { SET_VAR(rareNonInheritedData, m_visitedLinkBorderTopColor, v); }
    void setVisitedLinkOutlineColor(const StyleColor& v) { SET_VAR(rareNonInheritedData, m_visitedLinkOutlineColor, v); }
    void setVisitedLinkColumnRuleColor(const StyleColor& v) { SET_VAR(rareNonInheritedData.access()->m_multiCol, m_visitedLinkColumnRuleColor, v); }
    void setVisitedLinkTextDecorationColor(const StyleColor& v) { SET_VAR(rareNonInheritedData, m_visitedLinkTextDecorationColor, v); }
    void setVisitedLinkTextEmphasisColor(const StyleColor& v) { SET_VAR(rareInheritedData, visitedLinkTextEmphasisColor, v); }
    void setVisitedLinkTextFillColor(const StyleColor& v) { SET_VAR(rareInheritedData, visitedLinkTextFillColor, v); }
    void setVisitedLinkTextStrokeColor(const StyleColor& v) { SET_VAR(rareInheritedData, visitedLinkTextStrokeColor, v); }

    void inheritUnicodeBidiFrom(const RenderStyle* parent) { noninherited_flags._unicodeBidi = parent->noninherited_flags._unicodeBidi; }
    void getShadowExtent(const ShadowList*, LayoutUnit& top, LayoutUnit& right, LayoutUnit& bottom, LayoutUnit& left) const;
    LayoutBoxExtent getShadowInsetExtent(const ShadowList*) const;
    void getShadowHorizontalExtent(const ShadowList*, LayoutUnit& left, LayoutUnit& right) const;
    void getShadowVerticalExtent(const ShadowList*, LayoutUnit& top, LayoutUnit& bottom) const;
    void getShadowInlineDirectionExtent(const ShadowList* shadow, LayoutUnit& logicalLeft, LayoutUnit& logicalRight) const
    {
        return isHorizontalWritingMode() ? getShadowHorizontalExtent(shadow, logicalLeft, logicalRight) : getShadowVerticalExtent(shadow, logicalLeft, logicalRight);
    }
    void getShadowBlockDirectionExtent(const ShadowList* shadow, LayoutUnit& logicalTop, LayoutUnit& logicalBottom) const
    {
        return isHorizontalWritingMode() ? getShadowVerticalExtent(shadow, logicalTop, logicalBottom) : getShadowHorizontalExtent(shadow, logicalTop, logicalBottom);
    }

    bool isDisplayReplacedType(EDisplay display) const
    {
        return display == INLINE_BLOCK || display == INLINE_BOX || display == INLINE_FLEX
            || display == INLINE_TABLE || display == INLINE_GRID;
    }

    bool isDisplayInlineType(EDisplay display) const
    {
        return display == INLINE || isDisplayReplacedType(display);
    }

    // Color accessors are all private to make sure callers use visitedDependentColor instead to access them.
    StyleColor borderLeftColor() const { return surround->border.left().color(); }
    StyleColor borderRightColor() const { return surround->border.right().color(); }
    StyleColor borderTopColor() const { return surround->border.top().color(); }
    StyleColor borderBottomColor() const { return surround->border.bottom().color(); }
    StyleColor backgroundColor() const { return m_background->color(); }
    Color color() const;
    StyleColor columnRuleColor() const { return rareNonInheritedData->m_multiCol->m_rule.color(); }
    StyleColor outlineColor() const { return m_background->outline().color(); }
    StyleColor textEmphasisColor() const { return rareInheritedData->textEmphasisColor; }
    StyleColor textFillColor() const { return rareInheritedData->textFillColor; }
    StyleColor textStrokeColor() const { return rareInheritedData->textStrokeColor; }
    Color visitedLinkColor() const;
    StyleColor visitedLinkBackgroundColor() const { return rareNonInheritedData->m_visitedLinkBackgroundColor; }
    StyleColor visitedLinkBorderLeftColor() const { return rareNonInheritedData->m_visitedLinkBorderLeftColor; }
    StyleColor visitedLinkBorderRightColor() const { return rareNonInheritedData->m_visitedLinkBorderRightColor; }
    StyleColor visitedLinkBorderBottomColor() const { return rareNonInheritedData->m_visitedLinkBorderBottomColor; }
    StyleColor visitedLinkBorderTopColor() const { return rareNonInheritedData->m_visitedLinkBorderTopColor; }
    StyleColor visitedLinkOutlineColor() const { return rareNonInheritedData->m_visitedLinkOutlineColor; }
    StyleColor visitedLinkColumnRuleColor() const { return rareNonInheritedData->m_multiCol->m_visitedLinkColumnRuleColor; }
    StyleColor textDecorationColor() const { return rareNonInheritedData->m_textDecorationColor; }
    StyleColor visitedLinkTextDecorationColor() const { return rareNonInheritedData->m_visitedLinkTextDecorationColor; }
    StyleColor visitedLinkTextEmphasisColor() const { return rareInheritedData->visitedLinkTextEmphasisColor; }
    StyleColor visitedLinkTextFillColor() const { return rareInheritedData->visitedLinkTextFillColor; }
    StyleColor visitedLinkTextStrokeColor() const { return rareInheritedData->visitedLinkTextStrokeColor; }

    Color colorIncludingFallback(int colorProperty, bool visitedLink) const;

    Color stopColor() const { return svgStyle()->stopColor(); }
    Color floodColor() const { return svgStyle()->floodColor(); }
    Color lightingColor() const { return svgStyle()->lightingColor(); }

    void appendContent(PassOwnPtr<ContentData>);
    StyleDifference repaintOnlyDiff(const RenderStyle* other, unsigned& changedContextSensitiveProperties) const;
};

inline int adjustForAbsoluteZoom(int value, float zoomFactor)
{
    if (zoomFactor == 1)
        return value;
    // Needed because computeLengthInt truncates (rather than rounds) when scaling up.
    if (zoomFactor > 1) {
        if (value < 0)
            value--;
        else
            value++;
    }

    return roundForImpreciseConversion<int>(value / zoomFactor);
}

inline int adjustForAbsoluteZoom(int value, const RenderStyle* style)
{
    return adjustForAbsoluteZoom(value, style->effectiveZoom());
}

inline float adjustFloatForAbsoluteZoom(float value, const RenderStyle& style)
{
    return value / style.effectiveZoom();
}

inline LayoutUnit adjustLayoutUnitForAbsoluteZoom(LayoutUnit value, const RenderStyle& style)
{
    return value / style.effectiveZoom();
}

inline bool RenderStyle::setZoom(float f)
{
    if (compareEqual(visual->m_zoom, f))
        return false;
    visual.access()->m_zoom = f;
    setEffectiveZoom(effectiveZoom() * zoom());
    return true;
}

inline bool RenderStyle::setEffectiveZoom(float f)
{
    if (compareEqual(rareInheritedData->m_effectiveZoom, f))
        return false;
    rareInheritedData.access()->m_effectiveZoom = f;
    return true;
}

inline bool RenderStyle::isSharable() const
{
    if (unique())
        return false;
    if (hasUniquePseudoStyle())
        return false;
    if (transitions() || animations())
        return false;
    return true;
}

inline bool RenderStyle::setTextOrientation(TextOrientation textOrientation)
{
    if (compareEqual(rareInheritedData->m_textOrientation, textOrientation))
        return false;

    rareInheritedData.access()->m_textOrientation = textOrientation;
    return true;
}

inline bool RenderStyle::hasAnyPublicPseudoStyles() const
{
    return PUBLIC_PSEUDOID_MASK & noninherited_flags._pseudoBits;
}

inline bool RenderStyle::hasPseudoStyle(PseudoId pseudo) const
{
    ASSERT(pseudo > NOPSEUDO);
    ASSERT(pseudo < FIRST_INTERNAL_PSEUDOID);
    return (1 << (pseudo - 1)) & noninherited_flags._pseudoBits;
}

inline void RenderStyle::setHasPseudoStyle(PseudoId pseudo)
{
    ASSERT(pseudo > NOPSEUDO);
    ASSERT(pseudo < FIRST_INTERNAL_PSEUDOID);
    noninherited_flags._pseudoBits |= 1 << (pseudo - 1);
}

inline bool RenderStyle::hasPseudoElementStyle() const
{
    return noninherited_flags._pseudoBits & PSEUDO_ELEMENT_MASK;
}

float calcBorderRadiiConstraintScaleFor(const FloatRect&, const FloatRoundedRect::Radii&);

} // namespace WebCore

#endif // RenderStyle_h
