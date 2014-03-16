/*
 * Copyright (C) 2006, 2007, 2009, 2010, 2011, 2012 Apple Inc. All rights reserved.
 * Copyright (C) 2012, 2013 Adobe Systems Incorporated. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
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

#ifndef CanvasPathMethods_h
#define CanvasPathMethods_h

#include "platform/graphics/Path.h"

namespace WebCore {

class ExceptionState;
class FloatRect;

class CanvasPathMethods {
public:
    virtual ~CanvasPathMethods() { }

    void closePath();
    void moveTo(float x, float y);
    void lineTo(float x, float y);
    void quadraticCurveTo(float cpx, float cpy, float x, float y);
    void bezierCurveTo(float cp1x, float cp1y, float cp2x, float cp2y, float x, float y);
    void arcTo(float x0, float y0, float x1, float y1, float radius, ExceptionState&);
    void arc(float x, float y, float radius, float startAngle, float endAngle, bool anticlockwise, ExceptionState&);
    void ellipse(float x, float y, float radiusX, float radiusY, float rotation, float startAngle, float endAngle, bool anticlockwise, ExceptionState&);
    void rect(float x, float y, float width, float height);

    virtual bool isTransformInvertible() const { return true; }

    // CanvasPathMethods JS API.
    static void closePath(CanvasPathMethods& object)
        { object.closePath(); }
    static void moveTo(CanvasPathMethods& object, float x, float y)
        { object.moveTo(x, y); }
    static void lineTo(CanvasPathMethods& object, float x, float y)
        { object.lineTo(x, y); }
    static void quadraticCurveTo(CanvasPathMethods& object, float cpx, float cpy, float x, float y)
        { object.quadraticCurveTo(cpx, cpy, x, y); }
    static void bezierCurveTo(CanvasPathMethods& object, float cp1x, float cp1y, float cp2x, float cp2y, float x, float y)
        { object.bezierCurveTo(cp1x, cp1y, cp2x, cp2y, x, y); }
    static void arcTo(CanvasPathMethods& object, float x0, float y0, float x1, float y1, float radius, ExceptionState& es)
        { object.arcTo(x0, y0, x1, y1, radius, es); }
    static void arc(CanvasPathMethods& object, float x, float y, float radius, float startAngle, float endAngle, bool anticlockwise, ExceptionState& es)
        { object.arc(x, y, radius, startAngle, endAngle, anticlockwise, es); }
    static void ellipse(CanvasPathMethods& object, float x, float y, float radiusX, float radiusY, float rotation, float startAngle, float endAngle, bool anticlockwise, ExceptionState& es)
        { object.ellipse(x, y, radiusX, radiusY, rotation, startAngle, endAngle, anticlockwise, es); }
    static void rect(CanvasPathMethods& object, float x, float y, float width, float height)
        { object.rect(x, y, width, height); }

protected:
    CanvasPathMethods() { }
    CanvasPathMethods(const Path& path) : m_path(path) { }
    Path m_path;
};
}

#endif
