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

#include <comphelper/lok.hxx>

#include <vcl/outdev.hxx>
#include <vcl/canvastools.hxx>

#include <salgdi.hxx>

BitmapEx RenderContext2::GetBitmapEx(Point const& rSrcPt, Size const& rSize) const
{
    return BitmapEx(GetBitmap(rSrcPt, rSize));
}

Point RenderContext2::ShiftPoint(Point const& rDestPt, Point const& rOrigin)
{
    Point aDestPt(rDestPt);

    if (comphelper::LibreOfficeKit::isActive() && GetMapMode().GetMapUnit() != MapUnit::MapPixel)
    {
        aDestPt.Move(rOrigin.getX(), rOrigin.getY());
        DisableMapMode();
    }

    return aDestPt;
}

void RenderContext2::RestoreAfterShift()
{
    if (comphelper::LibreOfficeKit::isActive() && GetMapMode().GetMapUnit() != MapUnit::MapPixel)
    {
        EnableMapMode();
    }
}

bool RenderContext2::DrawTransformBitmapExDirect(basegfx::B2DHomMatrix const& aFullTransform,
                                                 BitmapEx const& rBitmapEx)
{
    SalBitmap* pSalAlphaBmp = rBitmapEx.GetBitmap().ImplGetSalBitmap().get();

    // try to paint directly
    const basegfx::B2DPoint aNull(aFullTransform * basegfx::B2DPoint(0.0, 0.0));
    const basegfx::B2DPoint aTopX(aFullTransform * basegfx::B2DPoint(1.0, 0.0));
    const basegfx::B2DPoint aTopY(aFullTransform * basegfx::B2DPoint(0.0, 1.0));
    SalBitmap* pSalSrcBmp = rBitmapEx.GetBitmap().ImplGetSalBitmap().get();

    OutputDevice const* pOutDev = dynamic_cast<OutputDevice const*>(this);
    return mpGraphics->DrawTransformedBitmap(aNull, aTopX, aTopY, *pSalSrcBmp, pSalAlphaBmp,
                                             *pOutDev);
};

bool RenderContext2::TransformAndReduceBitmapExToTargetRange(
    const basegfx::B2DHomMatrix& aFullTransform, basegfx::B2DRange& aVisibleRange,
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
    basegfx::B2DRange aOutPixel(0.0, 0.0, GetSizeInPixels().Width(), GetSizeInPixels().Height());

    if (IsClipRegion())
    {
        tools::Rectangle aRegionRectangle(GetActiveClipRegion().GetBoundRect());

        // caution! Range from rectangle, one too much (!)
        aRegionRectangle.AdjustRight(-1);
        aRegionRectangle.AdjustBottom(-1);
        aOutPixel.intersect(vcl::unotools::b2DRectangleFromRectangle(aRegionRectangle));
    }

    if (aOutPixel.isEmpty())
    {
        // no active output area
        return false;
    }

    // if aFullPixelRange is not completely inside of aOutPixel,
    // reduction of target pixels is possible
    basegfx::B2DRange aVisiblePixelRange(aFullPixelRange);

    if (!aOutPixel.isInside(aFullPixelRange))
    {
        aVisiblePixelRange.intersect(aOutPixel);

        if (aVisiblePixelRange.isEmpty())
        {
            // nothing in visible part, reduces to nothing
            return false;
        }

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

    // for pixel devices, do *not* limit size, else RenderContext2::DrawTransparentAlphaBitmap
    // will create another, badly scaled bitmap to do the job. Nonetheless, do a
    // maximum clipping of something big (1600x1280x2). Add 1.0 to avoid rounding
    // errors in rough estimations
    const double fNewMaxArea(aVisiblePixelRange.getWidth() * aVisiblePixelRange.getHeight());

    fMaximumArea = std::min(4096000.0, fNewMaxArea + 1.0);

    return true;
}

bool RenderContext2::DrawMaskedAlphaBitmapEx(Point const& rDestPt, Size const& rDestSize,
                                             Point const& rSrcPtPixel, Size const& rSrcSizePixel,
                                             BitmapEx const& rBitmapEx)
{
    BitmapEx aBmpEx(rBitmapEx);

    SalTwoRect aPosAry(rSrcPtPixel.X(), rSrcPtPixel.Y(), rSrcSizePixel.Width(),
                       rSrcSizePixel.Height(), maGeometry.LogicXToDevicePixel(rDestPt.X()),
                       maGeometry.LogicYToDevicePixel(rDestPt.Y()),
                       maGeometry.LogicWidthToDevicePixel(rDestSize.Width()),
                       maGeometry.LogicHeightToDevicePixel(rDestSize.Height()));

    const SalBitmap* pSalSrcBmp = aBmpEx.ImplGetBitmapSalBitmap().get();
    std::shared_ptr<SalBitmap> xMaskBmp = aBmpEx.ImplGetMaskSalBitmap();

    if (!xMaskBmp)
        return false;

    assert(pSalSrcBmp);

    // temporary till I migrate over to RenderContext2
    OutputDevice const* pOutDev = dynamic_cast<OutputDevice const*>(this);

    // try to paint as alpha directly. If this worked, we are done (except alpha, see below)
    if (!mpGraphics->DrawAlphaBitmap(aPosAry, *pSalSrcBmp, *xMaskBmp, *pOutDev))
    {
        // #4919452# reduce operation area to bounds of cliprect. since masked transparency involves
        // creation of a large vdev and copying the screen content into that (slooow read from
        // framebuffer), that should considerably increase performance for large bitmaps and small
        // clippings.

        // Note that this optimization is a workaround for a Writer peculiarity, namely, to decompose
        // background graphics into myriads of disjunct, tiny rectangles. That otherwise kills us
        // here, since for transparent output, SAL always prepares the whole bitmap, if aPosAry
        // contains the whole bitmap (and it's _not_ to blame for that).

        // Note the call to ImplPixelToDevicePixel(), since aPosAry already contains the
        // mnOutOff-offsets, they also have to be applied to the region
        tools::Rectangle aClipRegionBounds(maGeometry.PixelToDevicePixel(maRegion).GetBoundRect());

        // TODO: Also respect scaling (that's a bit tricky, since the source points have to move
        // fractional amounts (which is not possible, thus has to be emulated by increases of the
        // copy area)

        // for now, only identity scales allowed
        if (!aClipRegionBounds.IsEmpty() && aPosAry.mnDestWidth == aPosAry.mnSrcWidth
            && aPosAry.mnDestHeight == aPosAry.mnSrcHeight)
        {
            // now intersect dest rect with clip region
            aClipRegionBounds.Intersection(tools::Rectangle(
                aPosAry.mnDestX, aPosAry.mnDestY, aPosAry.mnDestX + aPosAry.mnDestWidth - 1,
                aPosAry.mnDestY + aPosAry.mnDestHeight - 1));

            // Note: I could theoretically optimize away the DrawBitmap below, if the region is empty
            // here. Unfortunately, cannot rule out that somebody relies on the side effects.
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

        mpGraphics->DrawBitmap(aPosAry, *pSalSrcBmp, *xMaskBmp, *pOutDev);
    }

    return true;
}

void RenderContext2::DrawAlphaBitmapEx(Point const& rDestPt, Size const& rDestSize,
                                       Point const& rSrcPtPixel, Size const& rSrcSizePixel,
                                       BitmapEx const& rBitmapEx)
{
    BitmapEx aBmpEx(rBitmapEx);

    SalTwoRect aPosAry(rSrcPtPixel.X(), rSrcPtPixel.Y(), rSrcSizePixel.Width(),
                       rSrcSizePixel.Height(), maGeometry.LogicXToDevicePixel(rDestPt.X()),
                       maGeometry.LogicYToDevicePixel(rDestPt.Y()),
                       maGeometry.LogicWidthToDevicePixel(rDestSize.Width()),
                       maGeometry.LogicHeightToDevicePixel(rDestSize.Height()));

    const BmpMirrorFlags nMirrFlags = AdjustTwoRect(aPosAry, aBmpEx.GetSizePixel());

    if (aPosAry.mnSrcWidth && aPosAry.mnSrcHeight && aPosAry.mnDestWidth && aPosAry.mnDestHeight)
    {
        if (nMirrFlags != BmpMirrorFlags::NONE)
            aBmpEx.Mirror(nMirrFlags);

        if (DrawMaskedAlphaBitmapEx(rDestPt, rDestSize, rSrcPtPixel, rSrcSizePixel, aBmpEx))
            return;

        const SalBitmap* pSalSrcBmp = aBmpEx.ImplGetBitmapSalBitmap().get();

        // temporary till I migrate over to RenderContext2
        OutputDevice const* pOutDev = dynamic_cast<OutputDevice const*>(this);
        mpGraphics->DrawBitmap(aPosAry, *pSalSrcBmp, *pOutDev);
    }
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
