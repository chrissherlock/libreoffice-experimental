/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <basegfx/matrix/b2dhommatrixtools.hxx>

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

Bitmap BitmapRenderer::GetTransformedBitmapFallback(const Bitmap& rBitmap,
                                                    const basegfx::B2DHomMatrix& rDeviceTransform,
                                                    basegfx::B2DRange& rVisibleRange,
                                                    double fMaximumArea, bool bSheared)
{
    Bitmap aTransformed(rBitmap);

    // Ensure Alpha for "Gutter" pixels
    if (!aTransformed.HasAlpha())
    {
        const Bitmap aContent(aTransformed.CreateColorBitmap());
        AlphaMask aMaskBmp(aContent.GetSizePixel());
        aMaskBmp.Erase(0);
        aTransformed = Bitmap(aContent, aMaskBmp);
    }

    // Normalize and Decompose
    basegfx::B2DVector aFullScale, aFullTranslate;
    double fFullRotate, fFullShearX;
    basegfx::B2DHomMatrix aDeviceTransform(rDeviceTransform);
    aDeviceTransform.decompose(aFullScale, aFullTranslate, fFullRotate, fFullShearX);

    const Size aOriginalSizePixel(rBitmap.GetSizePixel());
    if (aFullScale.getX() > 0 && aFullScale.getY() > 0
        && aOriginalSizePixel.getWidth() > aFullScale.getX()
        && aOriginalSizePixel.getHeight() > aFullScale.getY())
    {
        basegfx::B2DHomMatrix aNormalize = basegfx::utils::createScaleB2DHomMatrix(
            aOriginalSizePixel.getWidth() / aFullScale.getX(),
            aOriginalSizePixel.getHeight() / aFullScale.getY());
        aDeviceTransform *= aNormalize;
    }

    // Transformation Routing
    double fSourceRatio
        = aOriginalSizePixel.getHeight() != 0
              ? aOriginalSizePixel.getWidth() / static_cast<double>(aOriginalSizePixel.getHeight())
              : 1.0;
    double fTargetRatio = aFullScale.getY() != 0 ? aFullScale.getX() / aFullScale.getY() : 1.0;

    bool bAspectRatioKept = rtl::math::approxEqual(fSourceRatio, fTargetRatio);
    if (bSheared || !bAspectRatioKept)
    {
        aTransformed = aTransformed.getTransformed(aDeviceTransform, rVisibleRange, fMaximumArea);
    }
    else
    {
        fFullRotate = fmod(fFullRotate * -1, 2 * M_PI);
        if (fFullRotate < 0)
            fFullRotate += 2 * M_PI;

        Degree10 nAngle10(basegfx::fround(basegfx::rad2deg<10>(fFullRotate)));
        aTransformed.Rotate(nAngle10, COL_TRANSPARENT);
    }

    return aTransformed;
}

} // namespace vcl::rendercontext

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
