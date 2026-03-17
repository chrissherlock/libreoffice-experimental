/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <tools/color.hxx>
#include <tools/long.hxx>

#include <vcl/dllapi.h>

class SalGraphics;
class Bitmap;
struct SalTwoRect;

namespace basegfx
{
class B2DHomMatrix;
class B2DPoint;
class B2DRange;
}

namespace vcl::rendercontext
{
/**
 * Stateless Compositor for Raster/Bitmap Operations.
 *
 * This class handles the rendering of bitmaps, alpha bitmaps, and transformed
 * images. It acts as the bridge between the stateful OutputDevice and the
 * hardware abstraction layer (SalGraphics).
 *
 * All coordinates passed to these methods MUST be fully composed device pixels.
 */
class VCL_DLLPUBLIC BitmapRenderer
{
public:
    static Bitmap CaptureBitmap(SalGraphics& rGraphics, tools::Long nX, tools::Long nY,
                                tools::Long nWidth, tools::Long nHeight);

    static void MirrorRTLRect(SalTwoRect& rPosAry, tools::Long nFrameWidth);

    static void DrawBitmap(SalGraphics& rGraphics, SalTwoRect& rPosAry, const Bitmap& rBitmap,
                           tools::Long nFrameWidth, bool bRTL, bool bAlphaCapable = true);

    static bool DrawTransformedBitmap(SalGraphics& rGraphics, const basegfx::B2DPoint& rNull,
                                      const basegfx::B2DPoint& rX, const basegfx::B2DPoint& rY,
                                      const Bitmap& rBitmap, double fAlpha = 1.0);

    static void DrawMask(SalGraphics& rGraphics, const SalTwoRect& rPosAry, const Bitmap& rBitmap,
                         const Color& rMaskColor);

    /**
     * Performs high-quality subsampling for downscaling bitmaps.
     * Updates both the bitmap and the SalTwoRect source dimensions.
     */
    static void ApplySubsampling(SalGraphics& rGraphics, SalTwoRect& rPosAry, Bitmap& rBitmap);

    /**
     * Generates a software-transformed version of a bitmap (rotated or sheared).
     * Handles alpha channel preparation and chooses the optimal transformation
     * method (Rotate vs. getTransformed).
     *
     * @param rBitmap       The original source bitmap.
     * @param rDeviceTransform The full transformation matrix in device coordinates.
     * @param rVisibleRange Input/Output: The visible sub-section of the bitmap.
     * @param fMaximumArea  The maximum pixel area allowed for the result.
     * @param bSheared      Hint: true if the transformation includes shearing.
     * @return              The transformed bitmap.
     */
    static Bitmap GetTransformedBitmapFallback(const Bitmap& rBitmap,
                                               const basegfx::B2DHomMatrix& rDeviceTransform,
                                               basegfx::B2DRange& rVisibleRange,
                                               double fMaximumArea, bool bSheared);

    static void BlendAlphaBitmap(Bitmap& rPaint, const Bitmap& rPolyMask, const Color& rFillColor,
                                 sal_uInt8 nAlpha);

    /**
     * Converts a grayscale gradient mask into an AlphaMask, merges it with
     * the content's existing alpha, and returns the final composited Bitmap.
     */
    static Bitmap ApplyGradientAlpha(const Bitmap& rContent, const Bitmap& rGradientMask);

private:
    BitmapRenderer() = delete;
    ~BitmapRenderer() = delete;
};

} // namespace vcl::rendercontext

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
