/*
 * Copyright (C) 2004, 2005, 2006, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005 Rob Buis <buis@kde.org>
 * Copyright (C) 2005 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#ifndef FEGaussianBlur_h
#define FEGaussianBlur_h

#include "platform/graphics/filters/Filter.h"
#include "platform/graphics/filters/FilterEffect.h"

namespace WebCore {

class PLATFORM_EXPORT FEGaussianBlur : public FilterEffect {
public:
    static PassRefPtr<FEGaussianBlur> create(Filter*, float, float);

    float stdDeviationX() const;
    void setStdDeviationX(float);

    float stdDeviationY() const;
    void setStdDeviationY(float);

    virtual FloatRect mapRect(const FloatRect&, bool forward = true) OVERRIDE FINAL;
    virtual FloatRect determineAbsolutePaintRect(const FloatRect& requestedRect) OVERRIDE;
    static void calculateKernelSize(Filter*, unsigned& kernelSizeX, unsigned& kernelSizeY, float stdX, float stdY);
    static void calculateUnscaledKernelSize(unsigned& kernelSizeX, unsigned& kernelSizeY, float stdX, float stdY);

    virtual TextStream& externalRepresentation(TextStream&, int indention) const OVERRIDE;

private:
    static const int s_minimalRectDimension = 100 * 100; // Empirical data limit for parallel jobs

    template<typename Type>
    friend class ParallelJobs;

    struct PlatformApplyParameters {
        FEGaussianBlur* filter;
        RefPtr<Uint8ClampedArray> srcPixelArray;
        RefPtr<Uint8ClampedArray> dstPixelArray;
        int width;
        int height;
        unsigned kernelSizeX;
        unsigned kernelSizeY;
    };

    static void platformApplyWorker(PlatformApplyParameters*);

    FEGaussianBlur(Filter*, float, float);

    virtual void applySoftware() OVERRIDE;
    virtual bool applySkia() OVERRIDE;

    static inline void kernelPosition(int boxBlur, unsigned& std, int& dLeft, int& dRight);
    inline void platformApply(Uint8ClampedArray* srcPixelArray, Uint8ClampedArray* tmpPixelArray, unsigned kernelSizeX, unsigned kernelSizeY, IntSize& paintSize);

    inline void platformApplyGeneric(Uint8ClampedArray* srcPixelArray, Uint8ClampedArray* tmpPixelArray, unsigned kernelSizeX, unsigned kernelSizeY, IntSize& paintSize);
    virtual PassRefPtr<SkImageFilter> createImageFilter(SkiaImageFilterBuilder*) OVERRIDE;

    float m_stdX;
    float m_stdY;
};

inline void FEGaussianBlur::kernelPosition(int boxBlur, unsigned& std, int& dLeft, int& dRight)
{
    // check http://www.w3.org/TR/SVG/filters.html#feGaussianBlurElement for details
    switch (boxBlur) {
    case 0:
        if (!(std % 2)) {
            dLeft = std / 2 - 1;
            dRight = std - dLeft;
        } else {
            dLeft = std / 2;
            dRight = std - dLeft;
        }
        break;
    case 1:
        if (!(std % 2)) {
            dLeft++;
            dRight--;
        }
        break;
    case 2:
        if (!(std % 2)) {
            dRight++;
            std++;
        }
        break;
    }
}

} // namespace WebCore

#endif // FEGaussianBlur_h
