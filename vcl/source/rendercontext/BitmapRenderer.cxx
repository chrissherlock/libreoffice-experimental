/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <vcl/rendercontext/BitmapRenderer.hxx>
#include <vcl/alpha.hxx>
#include <vcl/bitmap.hxx>

#include <salgdi.hxx>

namespace vcl::rendercontext
{
Bitmap BitmapRenderer::CaptureBitmap(SalGraphics& rGraphics, tools::Long nX, tools::Long nY,
                                     tools::Long nWidth, tools::Long nHeight)
{
    std::shared_ptr<SalBitmap> xSalBmp = rGraphics.getBitmap(nX, nY, nWidth, nHeight);

    if (!xSalBmp)
        return Bitmap();

    return Bitmap(xSalBmp);
}

void BitmapRenderer::MirrorRTLRect(SalTwoRect& rPosAry, tools::Long nFrameWidth)
{
    rPosAry.mnDestX = nFrameWidth - rPosAry.mnDestWidth - rPosAry.mnDestX;
}

void BitmapRenderer::DrawBitmap(SalGraphics& rGraphics, SalTwoRect& rPosAry, const Bitmap& rBitmap,
                                tools::Long nFrameWidth, bool bRTL, bool bAlphaCapable)
{
    if (rBitmap.IsEmpty())
        return;

    if (bRTL)
        MirrorRTLRect(rPosAry, nFrameWidth);

    Bitmap aTargetBmp(rBitmap);

    if (!bAlphaCapable && aTargetBmp.HasAlpha())
    {
        Bitmap aColorBmp = aTargetBmp.CreateColorBitmap();
        aColorBmp.Blend(aTargetBmp.CreateAlphaMask(), COL_WHITE);
        aTargetBmp = aColorBmp;
    }

    std::shared_ptr<SalBitmap> pSalBitmap = aTargetBmp.ImplGetSalBitmap();
    if (!pSalBitmap)
        return;

    if (aTargetBmp.HasAlpha())
        rGraphics.drawAlphaBitmap(rPosAry, *pSalBitmap);
    else
        rGraphics.drawBitmap(rPosAry, *pSalBitmap);
}

bool BitmapRenderer::DrawTransformedBitmap(SalGraphics& rGraphics, const basegfx::B2DPoint& rNull,
                                           const basegfx::B2DPoint& rX, const basegfx::B2DPoint& rY,
                                           const Bitmap& rBitmap, double fAlpha)
{
    if (rBitmap.IsEmpty())
        return true;

    std::shared_ptr<SalBitmap> pSalBitmap = rBitmap.ImplGetSalBitmap();
    if (!pSalBitmap)
        return false;

    return rGraphics.drawTransformedBitmap(rNull, rX, rY, *pSalBitmap, fAlpha);
}

void BitmapRenderer::DrawMask(SalGraphics& rGraphics, const SalTwoRect& rPosAry,
                              const Bitmap& rBitmap, const Color& rMaskColor)
{
    if (rBitmap.IsEmpty())
        return;

    std::shared_ptr<SalBitmap> pSalBitmap = rBitmap.ImplGetSalBitmap();
    if (!pSalBitmap)
        return;

    rGraphics.drawMask(rPosAry, *pSalBitmap, rMaskColor);
}

void BitmapRenderer::ApplySubsampling(SalGraphics& rGraphics, SalTwoRect& rPosAry, Bitmap& rBitmap)
{
    double nScaleX = rPosAry.mnDestWidth / static_cast<double>(rPosAry.mnSrcWidth);
    double nScaleY = rPosAry.mnDestHeight / static_cast<double>(rPosAry.mnSrcHeight);

    // Handle Surface-specific scaling (e.g., HiDPI/Retina surfaces)
    double fSurfaceScale(1.0);
    if (rGraphics.ShouldDownscaleIconsAtSurface(fSurfaceScale))
    {
        nScaleX *= fSurfaceScale;
        nScaleY *= fSurfaceScale;
    }

    // Only apply if we are shrinking the bitmap (downscaling)
    if (nScaleX < 1.0 || nScaleY < 1.0)
    {
        rBitmap.Scale(nScaleX, nScaleY);

        // Update the source rectangle dimensions to match the new bitmap size
        rPosAry.mnSrcWidth = rPosAry.mnDestWidth * fSurfaceScale;
        rPosAry.mnSrcHeight = rPosAry.mnDestHeight * fSurfaceScale;
    }
}

} // namespace vcl::rendercontext

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
