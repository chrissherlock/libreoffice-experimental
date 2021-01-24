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

#include <vcl/RenderContext2.hxx>
#include <vcl/virdev.hxx>

#include <drawmode.hxx>
#include <salbmp.hxx>
#include <salgdi.hxx>

bool RenderContext2::CanSubsampleBitmap() const { return true; }

static std::tuple<Point, Size, tools::Rectangle, bool>
GetBitmapGeometry(Point const& rSrcPt, Size const& rSize, Geometry const& rGeom)
{
    tools::Long nX = rSrcPt.X();
    tools::Long nY = rSrcPt.Y();
    tools::Long nWidth = rSize.Width();
    tools::Long nHeight = rSize.Height();

    bool bClipped = false;

    tools::Rectangle aRect(Point(nX, nY), Size(nWidth, nHeight));

    if (nWidth > 0 && nHeight > 0 && nX <= (rGeom.GetWidthInPixels() + rGeom.GetXOffsetInPixels())
        && nY <= (rGeom.GetHeightInPixels() + rGeom.GetYOffsetInPixels()))
    {
        // X-Coordinate outside of draw area?
        if (nX < rGeom.GetXOffsetInPixels())
        {
            nWidth -= (rGeom.GetXOffsetInPixels() - nX);
            nX = rGeom.GetXOffsetInPixels();
            bClipped = true;
        }

        // Y-Coordinate outside of draw area?
        if (nY < rGeom.GetYOffsetInPixels())
        {
            nHeight -= (rGeom.GetYOffsetInPixels() - nY);
            nY = rGeom.GetYOffsetInPixels();
            bClipped = true;
        }

        // Width outside of draw area?
        if ((nWidth + nX) > (rGeom.GetWidthInPixels() + rGeom.GetXOffsetInPixels()))
        {
            nWidth = rGeom.GetXOffsetInPixels() + rGeom.GetWidthInPixels() - nX;
            bClipped = true;
        }

        // Height outside of draw area?
        if ((nHeight + nY) > (rGeom.GetHeightInPixels() + rGeom.GetYOffsetInPixels()))
        {
            nHeight = rGeom.GetYOffsetInPixels() + rGeom.GetHeightInPixels() - nY;
            bClipped = true;
        }
    }

    return std::make_tuple(Point(nX, nY), Size(nWidth, nHeight), aRect, bClipped);
}

Bitmap RenderContext2::GetBitmap(const Point& rSrcPt, const Size& rSize) const
{
    Bitmap aBmp;

    tools::Long nX = maGeometry.LogicXToDevicePixel(rSrcPt.X());
    tools::Long nY = maGeometry.LogicYToDevicePixel(rSrcPt.Y());
    tools::Long nWidth = maGeometry.LogicWidthToDevicePixel(rSize.Width());
    tools::Long nHeight = maGeometry.LogicHeightToDevicePixel(rSize.Height());

    Point aBmpPt(nX, nY);
    Size aBmpSize(nWidth, nHeight);
    tools::Rectangle aRect;
    bool bClipped;

    std::tie(aBmpPt, aBmpSize, aRect, bClipped) = GetBitmapGeometry(rSrcPt, rSize, maGeometry);

    if (aBmpSize.Width() > 0 && aBmpSize.Height() > 0
        && aBmpPt.X() <= (GetWidthInPixels() + GetXOffsetInPixels())
        && aBmpPt.Y() <= (GetHeightInPixels() + GetYOffsetInPixels()))
    {
        const OutputDevice* pOutDev = dynamic_cast<const OutputDevice*>(this);

        if (bClipped && (mpGraphics || AcquireGraphics()))
        {
            assert(mpGraphics);

            // If the visible part has been clipped, we have to create a
            // Bitmap with the correct size in which we copy the clipped
            // Bitmap to the correct position.
            ScopedVclPtrInstance<VirtualDevice> aVDev(*pOutDev);

            aBmp = aVDev->ClipBitmap(aBmpPt, aBmpSize, aRect);

            if (!aBmp.IsEmpty())
                bClipped = true;
        }

        if (!bClipped && (mpGraphics || AcquireGraphics()))
        {
            assert(mpGraphics);

            std::shared_ptr<SalBitmap> pSalBmp = mpGraphics->GetBitmap(
                aBmpPt.X(), aBmpPt.Y(), aBmpSize.Width(), aBmpSize.Height(), *pOutDev);

            if (pSalBmp)
                aBmp.ImplSetSalBitmap(pSalBmp);
        }
    }

    return aBmp;
}

void RenderContext2::DrawBitmap(Point const& rDestPt, Size const& rDestSize,
                                Point const& rSrcPtPixel, Size const& rSrcSizePixel,
                                Bitmap const& rBitmap, const MetaActionType nAction)
{
    if (!IsDeviceOutputNecessary())
        return;

    if (!mpGraphics && !AcquireGraphics())
        return;

    if (IsInitClipped())
        InitClipRegion();

    if (maRegion.IsEmpty())
        return;

    Bitmap aBmp(GetDrawModeBitmap(rBitmap, GetDrawMode()));

    if (!aBmp.IsEmpty())
    {
        SalTwoRect aPosAry(rSrcPtPixel.X(), rSrcPtPixel.Y(), rSrcSizePixel.Width(),
                           rSrcSizePixel.Height(), maGeometry.LogicXToDevicePixel(rDestPt.X()),
                           maGeometry.LogicYToDevicePixel(rDestPt.Y()),
                           maGeometry.LogicWidthToDevicePixel(rDestSize.Width()),
                           maGeometry.LogicHeightToDevicePixel(rDestSize.Height()));

        if (aPosAry.mnSrcWidth && aPosAry.mnSrcHeight && aPosAry.mnDestWidth
            && aPosAry.mnDestHeight)
        {
            const BmpMirrorFlags nMirrFlags = AdjustTwoRect(aPosAry, aBmp.GetSizePixel());

            if (nMirrFlags != BmpMirrorFlags::NONE)
                aBmp.Mirror(nMirrFlags);

            if (aPosAry.mnSrcWidth && aPosAry.mnSrcHeight && aPosAry.mnDestWidth
                && aPosAry.mnDestHeight)
            {
                if (nAction == MetaActionType::BMPSCALE && CanSubsampleBitmap())
                {
                    const double nScaleX
                        = aPosAry.mnDestWidth / static_cast<double>(aPosAry.mnSrcWidth);
                    const double nScaleY
                        = aPosAry.mnDestHeight / static_cast<double>(aPosAry.mnSrcHeight);

                    // If subsampling, use Bitmap::Scale() for subsampling of better quality.
                    if (nScaleX < 1.0 || nScaleY < 1.0)
                    {
                        aBmp.Scale(nScaleX, nScaleY);
                        aPosAry.mnSrcWidth = aPosAry.mnDestWidth;
                        aPosAry.mnSrcHeight = aPosAry.mnDestHeight;
                    }
                }

                const OutputDevice* pOutDev = dynamic_cast<const OutputDevice*>(this);
                mpGraphics->DrawBitmap(aPosAry, *aBmp.ImplGetSalBitmap(), *pOutDev);
            }
        }
    }
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
