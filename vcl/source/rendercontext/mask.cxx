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

#include <vcl/gdimtf.hxx>
#include <vcl/metaact.hxx>
#include <vcl/virdev.hxx>

#include <salgdi.hxx>
#include <salbmp.hxx>

void RenderContext2::DrawMask(Point const& rDestPt, Bitmap const& rBitmap, Color const& rMaskColor)
{
    assert(!is_double_buffered_window());

    DrawMask(rDestPt, PixelToLogic(rBitmap.GetSizePixel()), Point(), rBitmap.GetSizePixel(),
             rBitmap, rMaskColor);
}

void RenderContext2::DrawMask(Point const& rDestPt, Size const& rDestSize, Bitmap const& rBitmap,
                              Color const& rMaskColor)
{
    assert(!is_double_buffered_window());

    DrawMask(rDestPt, rDestSize, Point(), rBitmap.GetSizePixel(), rBitmap, rMaskColor);
}

void RenderContext2::DrawMask(Point const& rDestPt, Size const& rDestSize, Point const& rSrcPtPixel,
                              Size const& rSrcSizePixel, Bitmap const& rBitmap,
                              Color const& rMaskColor)
{
    assert(!is_double_buffered_window());

    if (ImplIsRecordLayout())
        return;

    if (meRasterOp == RasterOp::Invert)
    {
        DrawRect(tools::Rectangle(rDestPt, rDestSize));
        return;
    }

    if (!IsDeviceOutputNecessary())
        return;

    if (!mpGraphics && !AcquireGraphics())
        return;

    if (mbInitClipRegion)
        InitClipRegion();

    if (mbOutputClipped)
        return;

    DrawDeviceMask(rBitmap, rMaskColor, rDestPt, rDestSize, rSrcPtPixel, rSrcSizePixel);
}

void RenderContext2::DrawDeviceMask(Bitmap const& rMask, Color const& rMaskColor,
                                    Point const& rDestPt, Size const& rDestSize,
                                    Point const& rSrcPtPixel, Size const& rSrcSizePixel)
{
    assert(!is_double_buffered_window());

    const std::shared_ptr<SalBitmap>& xImpBmp = rMask.ImplGetSalBitmap();
    if (xImpBmp)
    {
        SalTwoRect aPosAry(rSrcPtPixel.X(), rSrcPtPixel.Y(), rSrcSizePixel.Width(),
                           rSrcSizePixel.Height(), ImplLogicXToDevicePixel(rDestPt.X()),
                           ImplLogicYToDevicePixel(rDestPt.Y()),
                           ImplLogicWidthToDevicePixel(rDestSize.Width()),
                           ImplLogicHeightToDevicePixel(rDestSize.Height()));

        // we don't want to mirror via coordinates
        const BmpMirrorFlags nMirrFlags = AdjustTwoRect(aPosAry, xImpBmp->GetSize());

        // check if output is necessary
        if (aPosAry.mnSrcWidth && aPosAry.mnSrcHeight && aPosAry.mnDestWidth
            && aPosAry.mnDestHeight)
        {
            if (nMirrFlags != BmpMirrorFlags::NONE)
            {
                Bitmap aTmp(rMask);
                aTmp.Mirror(nMirrFlags);
                mpGraphics->DrawMask(aPosAry, *aTmp.ImplGetSalBitmap(), rMaskColor, *this);
            }
            else
            {
                mpGraphics->DrawMask(aPosAry, *xImpBmp, rMaskColor, *this);
            }
        }
    }

    // TODO: Use mask here
    if (!mpAlphaVDev)
        return;

    const Bitmap& rAlphaMask(rMask.CreateMask(rMaskColor));

    // #i25167# Restrict mask painting to _opaque_ areas
    // of the mask, otherwise we spoil areas where no
    // bitmap content was ever visible. Interestingly
    // enough, this can be achieved by taking the mask as
    // the transparency mask of itself
    mpAlphaVDev->DrawBitmapEx(rDestPt, rDestSize, rSrcPtPixel, rSrcSizePixel,
                              BitmapEx(rAlphaMask, rMask));
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
