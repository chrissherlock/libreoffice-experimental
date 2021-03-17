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
        return BitmapEx(GetBitmap(rSrcPt, rSize));
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
    basegfx::B2DHomMatrix const& aFullTransform, basegfx::B2DRange& aVisibleRange,
    double& fMaximumArea)
{
    // limit TargetRange to existing pixels (if pixel device)
    // first get discrete range of object
    basegfx::B2DRange aFullPixelRange(aVisibleRange);

    aFullPixelRange.transform(aFullTransform);

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

        aVisibleRange = aVisiblePixelRange;
        aMakeVisibleRangeRelative.translate(-aFullPixelRange.getMinX(), -aFullPixelRange.getMinY());
        aMakeVisibleRangeRelative.scale(1.0 / aFullPixelRange.getWidth(),
                                        1.0 / aFullPixelRange.getHeight());
        aVisibleRange.transform(aMakeVisibleRangeRelative);
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
