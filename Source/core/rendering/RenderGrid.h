/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
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

#ifndef RenderGrid_h
#define RenderGrid_h

#include "core/rendering/OrderIterator.h"
#include "core/rendering/RenderBlock.h"

namespace WebCore {

struct GridCoordinate;
struct GridSpan;
class GridTrack;

enum GridTrackSizingDirection {
    ForColumns,
    ForRows
};

class RenderGrid FINAL : public RenderBlock {
public:
    RenderGrid(Element*);
    virtual ~RenderGrid();

    virtual const char* renderName() const OVERRIDE;

    virtual void layoutBlock(bool relayoutChildren) OVERRIDE;

    virtual bool avoidsFloats() const OVERRIDE { return true; }
    virtual bool canCollapseAnonymousBlockChild() const OVERRIDE { return false; }

    void dirtyGrid();

    const Vector<LayoutUnit>& columnPositions() const { return m_columnPositions; }
    const Vector<LayoutUnit>& rowPositions() const { return m_rowPositions; }

private:
    virtual bool isRenderGrid() const OVERRIDE { return true; }
    virtual void computeIntrinsicLogicalWidths(LayoutUnit& minLogicalWidth, LayoutUnit& maxLogicalWidth) const OVERRIDE;
    virtual void computePreferredLogicalWidths() OVERRIDE;

    virtual void addChild(RenderObject* newChild, RenderObject* beforeChild = 0) OVERRIDE;
    virtual void removeChild(RenderObject*) OVERRIDE;

    virtual void styleDidChange(StyleDifference, const RenderStyle*) OVERRIDE;

    bool explicitGridDidResize(const RenderStyle*) const;
    bool namedGridLinesDefinitionDidChange(const RenderStyle*) const;

    class GridIterator;
    struct GridSizingData;
    bool gridElementIsShrinkToFit();
    void computeUsedBreadthOfGridTracks(GridTrackSizingDirection, GridSizingData&);
    void computeUsedBreadthOfGridTracks(GridTrackSizingDirection, GridSizingData&, LayoutUnit& availableLogicalSpace);
    LayoutUnit computeUsedBreadthOfMinLength(GridTrackSizingDirection, const GridLength&) const;
    LayoutUnit computeUsedBreadthOfMaxLength(GridTrackSizingDirection, const GridLength&, LayoutUnit usedBreadth) const;
    LayoutUnit computeUsedBreadthOfSpecifiedLength(GridTrackSizingDirection, const Length&) const;
    void resolveContentBasedTrackSizingFunctions(GridTrackSizingDirection, GridSizingData&, LayoutUnit& availableLogicalSpace);

    void growGrid(GridTrackSizingDirection);
    void insertItemIntoGrid(RenderBox*, size_t rowTrack, size_t columnTrack);
    void insertItemIntoGrid(RenderBox*, const GridCoordinate&);
    void placeItemsOnGrid();
    void populateExplicitGridAndOrderIterator();
    void placeSpecifiedMajorAxisItemsOnGrid(const Vector<RenderBox*>&);
    void placeAutoMajorAxisItemsOnGrid(const Vector<RenderBox*>&);
    void placeAutoMajorAxisItemOnGrid(RenderBox*);
    GridTrackSizingDirection autoPlacementMajorAxisDirection() const;
    GridTrackSizingDirection autoPlacementMinorAxisDirection() const;

    void layoutGridItems();
    void populateGridPositions(const GridSizingData&);

    typedef LayoutUnit (RenderGrid::* SizingFunction)(RenderBox*, GridTrackSizingDirection, Vector<GridTrack>&);
    typedef LayoutUnit (GridTrack::* AccumulatorGetter)() const;
    typedef void (GridTrack::* AccumulatorGrowFunction)(LayoutUnit);
    typedef bool (GridTrackSize::* FilterFunction)() const;
    void resolveContentBasedTrackSizingFunctionsForItems(GridTrackSizingDirection, GridSizingData&, RenderBox*, FilterFunction, SizingFunction, AccumulatorGetter, AccumulatorGrowFunction);
    void distributeSpaceToTracks(Vector<GridTrack*>&, Vector<GridTrack*>* tracksForGrowthAboveMaxBreadth, AccumulatorGetter, AccumulatorGrowFunction, GridSizingData&, LayoutUnit& availableLogicalSpace);

    double computeNormalizedFractionBreadth(Vector<GridTrack>&, const GridSpan& tracksSpan, GridTrackSizingDirection, LayoutUnit availableLogicalSpace) const;

    const GridTrackSize& gridTrackSize(GridTrackSizingDirection, size_t) const;
    size_t explicitGridColumnCount() const;
    size_t explicitGridRowCount() const;
    size_t explicitGridSizeForSide(GridPositionSide) const;

    LayoutUnit logicalContentHeightForChild(RenderBox*, Vector<GridTrack>&);
    LayoutUnit minContentForChild(RenderBox*, GridTrackSizingDirection, Vector<GridTrack>& columnTracks);
    LayoutUnit maxContentForChild(RenderBox*, GridTrackSizingDirection, Vector<GridTrack>& columnTracks);
    LayoutUnit startOfColumnForChild(const RenderBox* child) const;
    LayoutUnit endOfColumnForChild(const RenderBox* child) const;
    LayoutUnit columnPositionAlignedWithGridContainerStart(const RenderBox*) const;
    LayoutUnit columnPositionAlignedWithGridContainerEnd(const RenderBox*) const;
    LayoutUnit centeredColumnPositionForChild(const RenderBox*) const;
    LayoutUnit columnPositionForChild(const RenderBox*) const;
    LayoutUnit rowPositionForChild(const RenderBox*) const;
    LayoutPoint findChildLogicalPosition(const RenderBox*) const;
    GridCoordinate cachedGridCoordinate(const RenderBox*) const;

    GridSpan resolveGridPositionsFromAutoPlacementPosition(const RenderBox*, GridTrackSizingDirection, size_t) const;
    PassOwnPtr<GridSpan> resolveGridPositionsFromStyle(const RenderBox*, GridTrackSizingDirection) const;
    size_t resolveNamedGridLinePositionFromStyle(const GridPosition&, GridPositionSide) const;
    size_t resolveGridPositionFromStyle(const GridPosition&, GridPositionSide) const;
    PassOwnPtr<GridSpan> resolveGridPositionAgainstOppositePosition(size_t resolvedOppositePosition, const GridPosition&, GridPositionSide) const;
    PassOwnPtr<GridSpan> resolveNamedGridLinePositionAgainstOppositePosition(size_t resolvedOppositePosition, const GridPosition&, GridPositionSide) const;
    PassOwnPtr<GridSpan> resolveBeforeStartNamedGridLinePositionAgainstOppositePosition(size_t resolvedOppositePosition, const GridPosition&, const Vector<size_t>&) const;
    PassOwnPtr<GridSpan> resolveAfterEndNamedGridLinePositionAgainstOppositePosition(size_t resolvedOppositePosition, const GridPosition&, const Vector<size_t>&) const;

    LayoutUnit gridAreaBreadthForChild(const RenderBox* child, GridTrackSizingDirection, const Vector<GridTrack>&) const;

    virtual void paintChildren(PaintInfo&, const LayoutPoint&) OVERRIDE;

    bool gridIsDirty() const { return m_gridIsDirty; }

#ifndef NDEBUG
    bool tracksAreWiderThanMinTrackBreadth(GridTrackSizingDirection, const Vector<GridTrack>&);
#endif

    size_t gridColumnCount() const
    {
        ASSERT(!gridIsDirty());
        return m_grid[0].size();
    }
    size_t gridRowCount() const
    {
        ASSERT(!gridIsDirty());
        return m_grid.size();
    }

    typedef Vector<RenderBox*, 1> GridCell;
    typedef Vector<Vector<GridCell> > GridRepresentation;
    GridRepresentation m_grid;
    bool m_gridIsDirty;
    Vector<LayoutUnit> m_rowPositions;
    Vector<LayoutUnit> m_columnPositions;
    HashMap<const RenderBox*, GridCoordinate> m_gridItemCoordinate;
    OrderIterator m_orderIterator;
    Vector<RenderBox*> m_gridItemsOverflowingGridArea;

    friend class GridCoordinateSorter;
};

DEFINE_RENDER_OBJECT_TYPE_CASTS(RenderGrid, isRenderGrid());

} // namespace WebCore

#endif // RenderGrid_h
