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

#include <vcl/virdev.hxx>

#include <salgdi.hxx>

void RenderContext2::DrawBitmap(Point const& rDestPt, Bitmap const& rBitmap)
{
    if (ProcessBitmapRasterOpInvert(rDestPt, PixelToLogic(rBitmap.GetSizePixel())))
        return;

    if (ProcessBitmapDrawModeBlackWhite(rDestPt, PixelToLogic(rBitmap.GetSizePixel())))
        return;

    DrawBitmap(rDestPt, PixelToLogic(rBitmap.GetSizePixel()), Point(),
               PixelToLogic(rBitmap.GetSizePixel()), rBitmap);
}

static void MirrorBitmap(Bitmap& rBitmap, SalTwoRect& rPosAry)
{
    if (rPosAry.mnSrcWidth && rPosAry.mnSrcHeight && rPosAry.mnDestWidth && rPosAry.mnDestHeight)
    {
        const BmpMirrorFlags nMirrFlags = AdjustTwoRect(rPosAry, rBitmap.GetSizePixel());

        if (nMirrFlags != BmpMirrorFlags::NONE)
            rBitmap.Mirror(nMirrFlags);
    }
}

void RenderContext2::DrawBitmap(Point const& rDestPt, Size const& rDestSize, Bitmap const& rBitmap)
{
    if (ProcessBitmapRasterOpInvert(rDestPt, rDestSize))
        return;

    if (ProcessBitmapDrawModeBlackWhite(rDestPt, rDestSize))
        return;

    if (!IsDeviceOutputNecessary())
        return;

    if (!mpGraphics && !AcquireGraphics())
        return;

    assert(mpGraphics);

    if (mbInitClipRegion)
        InitClipRegion();

    if (mbOutputClipped)
        return;

    if (!rBitmap.IsEmpty())
    {
        Point aSrcPtPixel;
        Size aSrcSizePixel(PixelToLogic(rBitmap.GetSizePixel()));

        SalTwoRect aPosAry(aSrcPtPixel.X(), aSrcPtPixel.Y(), aSrcSizePixel.Width(),
                           aSrcSizePixel.Height(), ImplLogicXToDevicePixel(rDestPt.X()),
                           ImplLogicYToDevicePixel(rDestPt.Y()),
                           ImplLogicWidthToDevicePixel(rDestSize.Width()),
                           ImplLogicHeightToDevicePixel(rDestSize.Height()));

        Bitmap aBmp(rBitmap);

        MirrorBitmap(aBmp, aPosAry);

        if (aPosAry.mnSrcWidth && aPosAry.mnSrcHeight && aPosAry.mnDestWidth
            && aPosAry.mnDestHeight)
        {
            const double nScaleX = aPosAry.mnDestWidth / static_cast<double>(aPosAry.mnSrcWidth);
            const double nScaleY = aPosAry.mnDestHeight / static_cast<double>(aPosAry.mnSrcHeight);

            // If subsampling, use Bitmap::Scale() for subsampling of better quality.
            if (nScaleX < 1.0 || nScaleY < 1.0)
            {
                aBmp.Scale(nScaleX, nScaleY);
                aPosAry.mnSrcWidth = aPosAry.mnDestWidth;
                aPosAry.mnSrcHeight = aPosAry.mnDestHeight;
            }

            mpGraphics->DrawBitmap(aPosAry, *aBmp.ImplGetSalBitmap(), *this);
        }
    }

    if (mpAlphaVDev)
    {
        // #i32109#: Make bitmap area opaque
        mpAlphaVDev->ImplFillOpaqueRectangle(tools::Rectangle(rDestPt, rDestSize));
    }
}

void RenderContext2::DrawBitmap(Point const& rDestPt, Size const& rDestSize,
                                Point const& rSrcPtPixel, Size const& rSrcSizePixel,
                                Bitmap const& rBitmap)
{
    if (ProcessBitmapRasterOpInvert(rDestPt, rDestSize))
        return;

    if (ProcessBitmapDrawModeBlackWhite(rDestPt, rDestSize))
        return;

    if (!IsDeviceOutputNecessary())
        return;

    if (!mpGraphics && !AcquireGraphics())
        return;

    assert(mpGraphics);

    if (mbInitClipRegion)
        InitClipRegion();

    if (mbOutputClipped)
        return;

    Bitmap aBmp(ProcessBitmapDrawModeGray(rBitmap));

    if (!aBmp.IsEmpty())
    {
        SalTwoRect aPosAry(rSrcPtPixel.X(), rSrcPtPixel.Y(), rSrcSizePixel.Width(),
                           rSrcSizePixel.Height(), ImplLogicXToDevicePixel(rDestPt.X()),
                           ImplLogicYToDevicePixel(rDestPt.Y()),
                           ImplLogicWidthToDevicePixel(rDestSize.Width()),
                           ImplLogicHeightToDevicePixel(rDestSize.Height()));

        MirrorBitmap(aBmp, aPosAry);

        if (aPosAry.mnSrcWidth && aPosAry.mnSrcHeight && aPosAry.mnDestWidth
            && aPosAry.mnDestHeight)
        {
            mpGraphics->DrawBitmap(aPosAry, *aBmp.ImplGetSalBitmap(), *this);
        }
    }

    if (mpAlphaVDev)
    {
        // #i32109#: Make bitmap area opaque
        mpAlphaVDev->ImplFillOpaqueRectangle(tools::Rectangle(rDestPt, rDestSize));
    }
}

void RenderContext2::DrawScaledBitmap(Point const& rDestPt, Size const& rDestSize,
                                        Point const& rSrcPtPixel, Size const& rSrcSizePixel,
                                        Bitmap const& rBitmap)
{
    assert(!is_double_buffered_window());

    if (ProcessBitmapRasterOpInvert(rDestPt, rDestSize))
        return;

    if (ProcessBitmapDrawModeBlackWhite(rDestPt, rDestSize))
        return;

    Bitmap aBmp(ProcessBitmapDrawModeGray(rBitmap));

    if (!IsDeviceOutputNecessary())
        return;

    if (!mpGraphics && !AcquireGraphics())
        return;

    assert(mpGraphics);

    if (mbInitClipRegion)
        InitClipRegion();

    if (mbOutputClipped)
        return;

    if (!aBmp.IsEmpty())
    {
        SalTwoRect aPosAry(rSrcPtPixel.X(), rSrcPtPixel.Y(), rSrcSizePixel.Width(),
                           rSrcSizePixel.Height(), ImplLogicXToDevicePixel(rDestPt.X()),
                           ImplLogicYToDevicePixel(rDestPt.Y()),
                           ImplLogicWidthToDevicePixel(rDestSize.Width()),
                           ImplLogicHeightToDevicePixel(rDestSize.Height()));

        if (aPosAry.mnSrcWidth && aPosAry.mnSrcHeight && aPosAry.mnDestWidth
            && aPosAry.mnDestHeight)
        {
            const BmpMirrorFlags nMirrFlags = AdjustTwoRect(aPosAry, aBmp.GetSizePixel());

            if (nMirrFlags != BmpMirrorFlags::NONE)
                aBmp.Mirror(nMirrFlags);

            if (aPosAry.mnSrcWidth && aPosAry.mnSrcHeight && aPosAry.mnDestWidth
                && aPosAry.mnDestHeight)
            {
                if (CanSubsampleBitmap())
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

                mpGraphics->DrawBitmap(aPosAry, *aBmp.ImplGetSalBitmap(), *this);
            }
        }
    }

    if (mpAlphaVDev)
    {
        // #i32109#: Make bitmap area opaque
        mpAlphaVDev->ImplFillOpaqueRectangle(tools::Rectangle(rDestPt, rDestSize));
    }
}

bool RenderContext2::ProcessBitmapRasterOpInvert(Point const& rDestPt, Size const& rDestSize)
{
    if (meRasterOp == RasterOp::Invert)
    {
        DrawRect(tools::Rectangle(rDestPt, rDestSize));
        return true;
    }

    return false;
}

bool RenderContext2::ProcessBitmapDrawModeBlackWhite(Point const& rDestPt, Size const& rDestSize)
{
    if (mnDrawMode & (DrawModeFlags::BlackBitmap | DrawModeFlags::WhiteBitmap))
    {
        sal_uInt8 cCmpVal;

        if (mnDrawMode & DrawModeFlags::BlackBitmap)
            cCmpVal = 0;
        else
            cCmpVal = 255;

        Color aCol(cCmpVal, cCmpVal, cCmpVal);
        Push(PushFlags::LINECOLOR | PushFlags::FILLCOLOR);
        SetLineColor(aCol);
        SetFillColor(aCol);
        DrawRect(tools::Rectangle(rDestPt, rDestSize));
        Pop();
        return true;
    }

    return false;
}

Bitmap RenderContext2::ProcessBitmapDrawModeGray(Bitmap const& rBitmap)
{
    Bitmap aBmp(rBitmap);

    if ((mnDrawMode & DrawModeFlags::GrayBitmap) && !aBmp.IsEmpty())
        aBmp.Convert(BmpConversion::N8BitGreys);

    return aBmp;
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
