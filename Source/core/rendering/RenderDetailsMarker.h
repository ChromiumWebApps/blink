/*
 * Copyright (C) 2010, 2011 Nokia Corporation and/or its subsidiary(-ies)
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

#ifndef RenderDetailsMarker_h
#define RenderDetailsMarker_h

#include "core/rendering/RenderBlockFlow.h"

namespace WebCore {

class RenderDetailsMarker FINAL : public RenderBlockFlow {
public:
    RenderDetailsMarker(Element*);

    enum Orientation { Up, Down, Left, Right };

    Orientation orientation() const;

private:
    virtual const char* renderName() const OVERRIDE { return "RenderDetailsMarker"; }
    virtual bool isDetailsMarker() const OVERRIDE { return true; }
    virtual void paint(PaintInfo&, const LayoutPoint&) OVERRIDE;

    bool isOpen() const;
    Path getCanonicalPath() const;
    Path getPath(const LayoutPoint& origin) const;
};

DEFINE_RENDER_OBJECT_TYPE_CASTS(RenderDetailsMarker, isDetailsMarker());

}

#endif // RenderDetailsMarker_h
