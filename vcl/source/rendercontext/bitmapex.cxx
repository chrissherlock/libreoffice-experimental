/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * This file incorporates work covered by the following license notice:
 *
 *   Licensed to the Apache Software Foundation (ASF) under one or more
 *   contributor license agreements. See the NOTICE file distributed
 *   with this work for additional information regarding copyright
 *   ownership. The ASF licenses this file to you under the Apache
 *   License, Version 2.0 (the "License"); you may not use this file
 *   except in compliance with the License. You may obtain a copy of
 *   the License at http://www.apache.org/licenses/LICENSE-2.0 .
 */

#include <basegfx/matrix/b2dhommatrixtools.hxx>

#include <vcl/alpha.hxx>
#include <vcl/bitmapex.hxx>
#include <vcl/canvastools.hxx>
#include <vcl/virdev.hxx>

#include <drawmode.hxx>
#include <salgdi.hxx>

BitmapEx RenderContext2::GetBitmapEx(Point const& rSrcPt, Size const& rSize) const
{
    // #110958# Extract alpha value from VDev, if any
    if (mpAlphaVDev)
    {
        Bitmap aAlphaBitmap(mpAlphaVDev->GetBitmap(rSrcPt, rSize));

        // ensure 8 bit alpha
        if (aAlphaBitmap.getPixelFormat() > vcl::PixelFormat::N8_BPP)
            aAlphaBitmap.Convert(BmpConversion::N8BitNoConversion);

        return BitmapEx(GetBitmap(rSrcPt, rSize), AlphaMask(aAlphaBitmap));
    }
    else
    {
        return BitmapEx(GetBitmap(rSrcPt, rSize));
    }
}

void RenderContext2::DrawBitmapEx(Point const& rDestPt, BitmapEx const& rBitmapEx)
{
    if (rBitmapEx.GetTransparentType() == TransparentType::NONE)
    {
        DrawBitmap(rDestPt, rBitmapEx.GetBitmap());
    }
    else
    {
        const Size aSizePix(rBitmapEx.GetSizePixel());

        if (meRasterOp == RasterOp::Invert)
        {
            DrawRect(tools::Rectangle(rDestPt, PixelToLogic(aSizePix)));
            return;
        }

        DrawBitmapEx2(rDestPt, PixelToLogic(aSizePix), Point(), aSizePix, rBitmapEx);
    }
}

void RenderContext2::DrawBitmapEx(Point const& rDestPt, Size const& rDestSize,
                                  BitmapEx const& rBitmapEx)
{
    if (rBitmapEx.GetTransparentType() == TransparentType::NONE)
    {
        DrawBitmap(rDestPt, rDestSize, rBitmapEx.GetBitmap());
    }
    else
    {
        if (meRasterOp == RasterOp::Invert)
        {
            DrawRect(tools::Rectangle(rDestPt, rDestSize));
            return;
        }

        DrawBitmapEx2(rDestPt, rDestSize, Point(), rBitmapEx.GetSizePixel(), rBitmapEx);
    }
}

void RenderContext2::DrawBitmapEx(Point const& rDestPt, Size const& rDestSize,
                                  Point const& rSrcPtPixel, Size const& rSrcSizePixel,
                                  BitmapEx const& rBitmapEx)
{
    if (rBitmapEx.GetTransparentType() != TransparentType::NONE)
    {
        DrawBitmap(rDestPt, rDestSize, rSrcPtPixel, rSrcSizePixel, rBitmapEx.GetBitmap());
    }
    else
    {
        if (RasterOp::Invert == meRasterOp)
        {
            DrawRect(tools::Rectangle(rDestPt, rDestSize));
            return;
        }

        DrawBitmapEx2(rDestPt, rDestSize, rSrcPtPixel, rSrcSizePixel, rBitmapEx);
    }
}

void RenderContext2::DrawBitmapEx2(Point const& rDestPt, Size const& rDestSize,
                                   Point const& rSrcPtPixel, Size const& rSrcSizePixel,
                                   BitmapEx const& rBitmapEx)
{
    BitmapEx aBmpEx(rBitmapEx);

    if (rBitmapEx.GetTransparentType() != TransparentType::NONE)
        aBmpEx = GetDrawModeBitmapEx(rBitmapEx, GetDrawMode());

    if (!IsDeviceOutputNecessary())
        return;

    if (!mpGraphics && !AcquireGraphics())
        return;

    assert(mpGraphics);

    if (mbInitClipRegion)
        InitClipRegion();

    if (mbOutputClipped)
        return;

    DrawDeviceBitmapEx(rDestPt, rDestSize, rSrcPtPixel, rSrcSizePixel, aBmpEx);
}

void RenderContext2::DrawDeviceBitmapEx(Point const& rDestPt, Size const& rDestSize,
                                        Point const& rSrcPtPixel, Size const& rSrcSizePixel,
                                        BitmapEx& rBitmapEx)
{
    assert(!is_double_buffered_window());

    if (rBitmapEx.IsAlpha())
    {
        DrawDeviceAlphaBitmap(rBitmapEx.GetBitmap(), rBitmapEx.GetAlpha(), rDestPt, rDestSize,
                              rSrcPtPixel, rSrcSizePixel);
    }
    else if (!rBitmapEx.IsEmpty())
    {
        SalTwoRect aPosAry(rSrcPtPixel.X(), rSrcPtPixel.Y(), rSrcSizePixel.Width(),
                           rSrcSizePixel.Height(), ImplLogicXToDevicePixel(rDestPt.X()),
                           ImplLogicYToDevicePixel(rDestPt.Y()),
                           ImplLogicWidthToDevicePixel(rDestSize.Width()),
                           ImplLogicHeightToDevicePixel(rDestSize.Height()));

        const BmpMirrorFlags nMirrFlags = AdjustTwoRect(aPosAry, rBitmapEx.GetSizePixel());

        if (aPosAry.mnSrcWidth && aPosAry.mnSrcHeight && aPosAry.mnDestWidth
            && aPosAry.mnDestHeight)
        {
            if (nMirrFlags != BmpMirrorFlags::NONE)
                rBitmapEx.Mirror(nMirrFlags);

            const SalBitmap* pSalSrcBmp = rBitmapEx.ImplGetBitmapSalBitmap().get();
            std::shared_ptr<SalBitmap> xMaskBmp = rBitmapEx.ImplGetMaskSalBitmap();

            if (xMaskBmp)
            {
                bool bTryDirectPaint(pSalSrcBmp);

                if (bTryDirectPaint
                    && mpGraphics->DrawAlphaBitmap(aPosAry, *pSalSrcBmp, *xMaskBmp, *this))
                {
                    // tried to paint as alpha directly. If this worked, we are done (except
                    // alpha, see below)
                }
                else
                {
                    // #4919452# reduce operation area to bounds of
                    // cliprect. since masked transparency involves
                    // creation of a large vdev and copying the screen
                    // content into that (slooow read from framebuffer),
                    // that should considerably increase performance for
                    // large bitmaps and small clippings.

                    // Note that this optimization is a workaround for a
                    // Writer peculiarity, namely, to decompose background
                    // graphics into myriads of disjunct, tiny
                    // rectangles. That otherwise kills us here, since for
                    // transparent output, SAL always prepares the whole
                    // bitmap, if aPosAry contains the whole bitmap (and
                    // it's _not_ to blame for that).

                    // Note the call to ImplPixelToDevicePixel(), since
                    // aPosAry already contains the mnOutOff-offsets, they
                    // also have to be applied to the region
                    tools::Rectangle aClipRegionBounds(
                        ImplPixelToDevicePixel(maRegion).GetBoundRect());

                    // TODO: Also respect scaling (that's a bit tricky,
                    // since the source points have to move fractional
                    // amounts (which is not possible, thus has to be
                    // emulated by increases copy area)
                    // const double nScaleX( aPosAry.mnDestWidth / aPosAry.mnSrcWidth );
                    // const double nScaleY( aPosAry.mnDestHeight / aPosAry.mnSrcHeight );

                    // for now, only identity scales allowed
                    if (!aClipRegionBounds.IsEmpty() && aPosAry.mnDestWidth == aPosAry.mnSrcWidth
                        && aPosAry.mnDestHeight == aPosAry.mnSrcHeight)
                    {
                        // now intersect dest rect with clip region
                        aClipRegionBounds.Intersection(
                            tools::Rectangle(aPosAry.mnDestX, aPosAry.mnDestY,
                                             aPosAry.mnDestX + aPosAry.mnDestWidth - 1,
                                             aPosAry.mnDestY + aPosAry.mnDestHeight - 1));

                        // Note: I could theoretically optimize away the
                        // DrawBitmap below, if the region is empty
                        // here. Unfortunately, cannot rule out that
                        // somebody relies on the side effects.
                        if (!aClipRegionBounds.IsEmpty())
                        {
                            aPosAry.mnSrcX += aClipRegionBounds.Left() - aPosAry.mnDestX;
                            aPosAry.mnSrcY += aClipRegionBounds.Top() - aPosAry.mnDestY;
                            aPosAry.mnSrcWidth = aClipRegionBounds.GetWidth();
                            aPosAry.mnSrcHeight = aClipRegionBounds.GetHeight();

                            aPosAry.mnDestX = aClipRegionBounds.Left();
                            aPosAry.mnDestY = aClipRegionBounds.Top();
                            aPosAry.mnDestWidth = aClipRegionBounds.GetWidth();
                            aPosAry.mnDestHeight = aClipRegionBounds.GetHeight();
                        }
                    }

                    mpGraphics->DrawBitmap(aPosAry, *pSalSrcBmp, *xMaskBmp, *this);
                }

                // #110958# Paint mask to alpha channel. Luckily, the
                // black and white representation of the mask maps to
                // the alpha channel

                // #i25167# Restrict mask painting to _opaque_ areas
                // of the mask, otherwise we spoil areas where no
                // bitmap content was ever visible. Interestingly
                // enough, this can be achieved by taking the mask as
                // the transparency mask of itself
                if (mpAlphaVDev)
                    mpAlphaVDev->DrawBitmapEx(rDestPt, rDestSize,
                                              BitmapEx(rBitmapEx.GetMask(), rBitmapEx.GetMask()));
            }
            else
            {
                mpGraphics->DrawBitmap(aPosAry, *pSalSrcBmp, *this);

                if (mpAlphaVDev)
                {
                    // #i32109#: Make bitmap area opaque
                    mpAlphaVDev->ImplFillOpaqueRectangle(tools::Rectangle(rDestPt, rDestSize));
                }
            }
        }
    }
}

bool RenderContext2::DrawTransformBitmapExDirect(basegfx::B2DHomMatrix const& aFullTransform,
                                                 BitmapEx const& rBitmapEx, double fAlpha)
{
    bool bDone = false;

    // try to paint directly
    const basegfx::B2DPoint aNull(aFullTransform * basegfx::B2DPoint(0.0, 0.0));
    const basegfx::B2DPoint aTopX(aFullTransform * basegfx::B2DPoint(1.0, 0.0));
    const basegfx::B2DPoint aTopY(aFullTransform * basegfx::B2DPoint(0.0, 1.0));
    SalBitmap* pSalSrcBmp = rBitmapEx.GetBitmap().ImplGetSalBitmap().get();
    Bitmap aAlphaBitmap;

    if (rBitmapEx.IsTransparent())
    {
        if (rBitmapEx.IsAlpha())
        {
            aAlphaBitmap = rBitmapEx.GetAlpha();
        }
        else
        {
            aAlphaBitmap = rBitmapEx.GetMask();
        }
    }
    else if (mpAlphaVDev)
    {
        aAlphaBitmap = AlphaMask(rBitmapEx.GetSizePixel());
        aAlphaBitmap.Erase(COL_BLACK); // opaque
    }

    SalBitmap* pSalAlphaBmp = aAlphaBitmap.ImplGetSalBitmap().get();

    bDone = mpGraphics->DrawTransformedBitmap(aNull, aTopX, aTopY, *pSalSrcBmp, pSalAlphaBmp,
                                              fAlpha, *this);

    if (mpAlphaVDev)
    {
        // Merge bitmap alpha to alpha device
        AlphaMask aAlpha(rBitmapEx.GetSizePixel());
        aAlpha.Erase((1 - fAlpha) * 255);
        mpAlphaVDev->DrawTransformBitmapExDirect(aFullTransform, BitmapEx(aAlpha, aAlphaBitmap));
    }

    return bDone;
};

bool RenderContext2::TransformAndReduceBitmapExToTargetRange(
    basegfx::B2DHomMatrix const& rFullTransform, basegfx::B2DRange& rVisibleRange,
    double& fMaximumArea)
{
    // limit TargetRange to existing pixels (if pixel device)
    // first get discrete range of object
    basegfx::B2DRange aFullPixelRange(rVisibleRange);

    aFullPixelRange.transform(rFullTransform);

    if (basegfx::fTools::equalZero(aFullPixelRange.getWidth())
        || basegfx::fTools::equalZero(aFullPixelRange.getHeight()))
    {
        // object is outside of visible area
        return false;
    }

    // now get discrete target pixels; start with OutDev pixel size and evtl.
    // intersect with active clipping area
    basegfx::B2DRange aOutPixel(0.0, 0.0, GetOutputSizePixel().Width(),
                                GetOutputSizePixel().Height());

    if (IsClipRegion())
    {
        tools::Rectangle aRegionRectangle(GetActiveClipRegion().GetBoundRect());

        // caution! Range from rectangle, one too much (!)
        aRegionRectangle.AdjustRight(-1);
        aRegionRectangle.AdjustBottom(-1);
        aOutPixel.intersect(vcl::unotools::b2DRectangleFromRectangle(aRegionRectangle));
    }

    if (aOutPixel.isEmpty())
        return false; // no active output area

    // if aFullPixelRange is not completely inside of aOutPixel,
    // reduction of target pixels is possible
    basegfx::B2DRange aVisiblePixelRange(aFullPixelRange);

    if (!aOutPixel.isInside(aFullPixelRange))
    {
        aVisiblePixelRange.intersect(aOutPixel);

        if (aVisiblePixelRange.isEmpty())
            return false; // nothing in visible part, reduces to nothing

        // aVisiblePixelRange contains the reduced output area in
        // discrete coordinates. To make it useful everywhere, make it relative to
        // the object range
        basegfx::B2DHomMatrix aMakeVisibleRangeRelative;

        rVisibleRange = aVisiblePixelRange;
        aMakeVisibleRangeRelative.translate(-aFullPixelRange.getMinX(), -aFullPixelRange.getMinY());
        aMakeVisibleRangeRelative.scale(1.0 / aFullPixelRange.getWidth(),
                                        1.0 / aFullPixelRange.getHeight());
        rVisibleRange.transform(aMakeVisibleRangeRelative);
    }

    // for pixel devices, do *not* limit size, else RenderContext2::DrawDeviceAlphaBitmap
    // will create another, badly scaled bitmap to do the job. Nonetheless, do a
    // maximum clipping of something big (1600x1280x2). Add 1.0 to avoid rounding
    // errors in rough estimations
    const double fNewMaxArea(aVisiblePixelRange.getWidth() * aVisiblePixelRange.getHeight());

    fMaximumArea = std::min(4096000.0, fNewMaxArea + 1.0);

    return true;
}
/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
