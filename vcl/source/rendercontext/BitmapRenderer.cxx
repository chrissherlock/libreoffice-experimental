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
#include <vcl/BitmapWriteAccess.hxx>

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

/**
 * Ensures a bitmap has an alpha channel.
 * If the bitmap is currently opaque, it is converted to a Bitmap with
 * a fully opaque alpha mask. This is required for rotations/shears to
 * allow for transparent "gutter" pixels.
 */
static void lcl_EnsureAlphaChannel(Bitmap& rBitmap)
{
    if (rBitmap.HasAlpha())
        return;

    const Bitmap aContent(rBitmap.CreateColorBitmap());
    AlphaMask aMaskBmp(aContent.GetSizePixel());
    aMaskBmp.Erase(0); // Initialize as fully opaque (0)
    rBitmap = Bitmap(aContent, aMaskBmp);
}

/**
 * Decomposes the device transform and normalizes it against the original bitmap size.
 * This ensures the software transformation engine handles the coordinate mapping correctly
 * when the target size differs from the source pixel dimensions.
 */
static void lcl_DecomposeAndNormalize(basegfx::B2DHomMatrix& rDeviceTransform,
                                      const Size& rOriginalSizePixel, basegfx::B2DVector& rScale,
                                      basegfx::B2DVector& rTranslate, double& rRotate,
                                      double& rShearX)
{
    rDeviceTransform.decompose(rScale, rTranslate, rRotate, rShearX);

    if (rScale.getX() > 0 && rScale.getY() > 0 && rOriginalSizePixel.getWidth() > rScale.getX()
        && rOriginalSizePixel.getHeight() > rScale.getY())
    {
        const basegfx::B2DHomMatrix aNormalize = basegfx::utils::createScaleB2DHomMatrix(
            rOriginalSizePixel.getWidth() / rScale.getX(),
            rOriginalSizePixel.getHeight() / rScale.getY());
        rDeviceTransform *= aNormalize;
    }
}

/**
 * Routes the transformation to either the generic getTransformed path (for shear
 * or aspect ratio changes) or the optimized Rotate path (for simple rotations).
 */
static void lcl_TransformBitmap(Bitmap& rBitmap, const basegfx::B2DHomMatrix& rNormalizedTransform,
                                basegfx::B2DRange& rVisibleRange, const basegfx::B2DVector& rScale,
                                double fRotation, double fMaximumArea, bool bSheared)
{
    const Size aSize(rBitmap.GetSizePixel());
    const double fSourceRatio
        = aSize.Height() != 0 ? aSize.Width() / static_cast<double>(aSize.Height()) : 1.0;
    const double fTargetRatio = rScale.getY() != 0 ? rScale.getX() / rScale.getY() : 1.0;

    const bool bAspectRatioKept = rtl::math::approxEqual(fSourceRatio, fTargetRatio);

    if (bSheared || !bAspectRatioKept)
    {
        rBitmap = rBitmap.getTransformed(rNormalizedTransform, rVisibleRange, fMaximumArea);
    }
    else
    {
        // Use the optimized Rotate path for simple rotations
        double fAngle = fmod(fRotation * -1, 2 * M_PI);
        if (fAngle < 0)
            fAngle += 2 * M_PI;

        const Degree10 nAngle10(basegfx::fround(basegfx::rad2deg<10>(fAngle)));
        rBitmap.Rotate(nAngle10, COL_TRANSPARENT);
    }
}

Bitmap BitmapRenderer::GetTransformedBitmapFallback(const Bitmap& rBitmap,
                                                    const basegfx::B2DHomMatrix& rDeviceTransform,
                                                    basegfx::B2DRange& rVisibleRange,
                                                    double fMaximumArea, bool bSheared)
{
    Bitmap aTransformed(rBitmap);
    lcl_EnsureAlphaChannel(aTransformed);

    basegfx::B2DVector aScale, aTranslate;
    double fRotate, fShearX;
    basegfx::B2DHomMatrix aNormalizedTransform(rDeviceTransform);

    lcl_DecomposeAndNormalize(aNormalizedTransform, rBitmap.GetSizePixel(), aScale, aTranslate,
                              fRotate, fShearX);

    lcl_TransformBitmap(aTransformed, aNormalizedTransform, rVisibleRange, aScale, fRotate,
                        fMaximumArea, bSheared);

    return aTransformed;
}

static void lcl_MaskedPaletteBlend(Bitmap& rPaint, const Bitmap& rPolyMask, const Color& rFillColor,
                                   sal_uInt8 nAlpha)
{
    BitmapScopedWriteAccess pW(rPaint);
    BitmapScopedReadAccess pR(rPolyMask);

    if (!pW || !pR)
        return;

    const BitmapColor aFillCol(rFillColor);
    const BitmapColor aBlack(pR->GetBestMatchingColor(COL_BLACK));
    const tools::Long nWidth = pW->Width();
    const tools::Long nHeight = pW->Height();

    const BitmapPalette& rPal = pW->GetPalette();
    const sal_uInt16 nCount = rPal.GetEntryCount();

    // Pre-calculate the blended results for every palette entry
    std::vector<BitmapColor> aMap(nCount);
    for (sal_uInt16 i = 0; i < nCount; i++)
    {
        BitmapColor aCol(rPal[i]);
        aCol.Merge(aFillCol, nAlpha);
        // Store the index of the closest match in the existing palette
        aMap[i] = BitmapColor(static_cast<sal_uInt8>(rPal.GetBestIndex(aCol)));
    }

    for (tools::Long nY = 0; nY < nHeight; nY++)
    {
        Scanline pScanline = pW->GetScanline(nY);
        Scanline pScanlineRead = pR->GetScanline(nY);
        for (tools::Long nX = 0; nX < nWidth; nX++)
        {
            if (pR->GetPixelFromData(pScanlineRead, nX) == aBlack)
            {
                // Fast index lookup from our pre-calculated map
                pW->SetPixelOnData(pScanline, nX, aMap[pW->GetIndexFromData(pScanline, nX)]);
            }
        }
    }
}

static void lcl_MaskedBlend(Bitmap& rPaint, const Bitmap& rPolyMask, const Color& rFillColor,
                            sal_uInt8 nAlpha)
{
    BitmapScopedWriteAccess pW(rPaint);
    BitmapScopedReadAccess pR(rPolyMask);

    if (!pW || !pR)
        return;

    const BitmapColor aFillCol(rFillColor);
    const BitmapColor aBlack(pR->GetBestMatchingColor(COL_BLACK));
    const tools::Long nWidth = pW->Width();
    const tools::Long nHeight = pW->Height();

    for (tools::Long nY = 0; nY < nHeight; nY++)
    {
        Scanline pScanline = pW->GetScanline(nY);
        Scanline pScanlineRead = pR->GetScanline(nY);
        for (tools::Long nX = 0; nX < nWidth; nX++)
        {
            if (pR->GetPixelFromData(pScanlineRead, nX) == aBlack)
            {
                BitmapColor aPixCol = pW->GetColor(nY, nX);
                aPixCol.Merge(aFillCol, nAlpha);
                pW->SetPixelOnData(pScanline, nX, aPixCol);
            }
        }
    }
}

/**
 * Manually blends a fill color into a bitmap using a mask to define the
 * area of effect and a transparency value for the blend strength.
 */
void BitmapRenderer::BlendAlphaBitmap(Bitmap& rPaint, const Bitmap& rPolyMask,
                                      const Color& rFillColor, sal_uInt8 nAlpha)
{
    if (vcl::isPalettePixelFormat(rPaint.getPixelFormat()))
        lcl_MaskedPaletteBlend(rPaint, rPolyMask, rFillColor, nAlpha);
    else
        lcl_MaskedBlend(rPaint, rPolyMask, rFillColor, nAlpha);
}

Bitmap BitmapRenderer::ApplyGradientAlpha(const Bitmap& rContent, const Bitmap& rGradientMask)
{
    AlphaMask aAlpha(rGradientMask);
    const AlphaMask aContentAlpha(rContent.CreateAlphaMask());

    // The metafile mask is drawn as a grayscale gradient (Black to White).
    // VCL AlphaMasks expect the inverse, so we invert it before blending.
    aAlpha.Invert();
    aAlpha.BlendWith(aContentAlpha);

    // Combine the original color channels with the newly calculated alpha
    return Bitmap(rContent.CreateColorBitmap(), aAlpha);
}

} // namespace vcl::rendercontext

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
