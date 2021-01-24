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

#include <config_features.h>

#include <sal/log.hxx>
#include <tools/debug.hxx>

#include <vcl/gdimtf.hxx>
#include <vcl/metaact.hxx>
#include <vcl/skia/SkiaHelper.hxx>
#include <vcl/virdev.hxx>

#include <bitmap/BitmapWriteAccess.hxx>
#include <drawmode.hxx>
#include <salgdi.hxx>

#include "TradScaleContext.hxx"
#include "LinearScaleContext.hxx"

void OutputDevice::DrawBitmap(const Point& rDestPt, const Bitmap& rBitmap)
{
    assert(!is_double_buffered_window());

    const Size aSizePix(rBitmap.GetSizePixel());
    DrawBitmap(rDestPt, maGeometry.PixelToLogic(aSizePix), Point(), aSizePix, rBitmap,
               MetaActionType::BMP);
}

void OutputDevice::DrawBitmap(const Point& rDestPt, const Size& rDestSize, const Bitmap& rBitmap)
{
    assert(!is_double_buffered_window());

    DrawBitmap(rDestPt, rDestSize, Point(), rBitmap.GetSizePixel(), rBitmap,
               MetaActionType::BMPSCALE);
}

void OutputDevice::DrawBitmap(const Point& rDestPt, const Size& rDestSize, const Point& rSrcPtPixel,
                              const Size& rSrcSizePixel, const Bitmap& rBitmap,
                              const MetaActionType nAction)
{
    assert(!is_double_buffered_window());

    if (ImplIsRecordLayout())
        return;

    if (RasterOp::Invert == meRasterOp)
    {
        DrawRect(tools::Rectangle(rDestPt, rDestSize));
        return;
    }

    if (GetDrawMode() & (DrawModeFlags::BlackBitmap | DrawModeFlags::WhiteBitmap))
    {
        sal_uInt8 cCmpVal;

        if (GetDrawMode() & DrawModeFlags::BlackBitmap)
            cCmpVal = 0;
        else
            cCmpVal = 255;

        Color aCol(cCmpVal, cCmpVal, cCmpVal);
        Push(PushFlags::LINECOLOR | PushFlags::FILLCOLOR);
        SetLineColor(aCol);
        SetFillColor(aCol);
        DrawRect(tools::Rectangle(rDestPt, rDestSize));
        Pop();
        return;
    }

    Bitmap aBmp(GetDrawModeBitmap(rBitmap, GetDrawMode()));

    if (mpMetaFile)
    {
        switch (nAction)
        {
            case MetaActionType::BMP:
                mpMetaFile->AddAction(new MetaBmpAction(rDestPt, aBmp));
                break;

            case MetaActionType::BMPSCALE:
                mpMetaFile->AddAction(new MetaBmpScaleAction(rDestPt, rDestSize, aBmp));
                break;

            case MetaActionType::BMPSCALEPART:
                mpMetaFile->AddAction(new MetaBmpScalePartAction(rDestPt, rDestSize, rSrcPtPixel,
                                                                 rSrcSizePixel, aBmp));
                break;

            default:
                break;
        }
    }

    if (!IsDeviceOutputNecessary())
        return;

    if (!mpGraphics && !AcquireGraphics())
        return;

    if (mbInitClipRegion)
        InitClipRegion();

    if (mbOutputClipped)
        return;

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

void OutputDevice::DrawTransparentAlphaBitmap(const Bitmap& rBmp, const AlphaMask& rAlpha,
                                              const Point& rDestPt, const Size& rDestSize,
                                              const Point& rSrcPtPixel, const Size& rSrcSizePixel)
{
    assert(!is_double_buffered_window());

    Point aOutPt(maGeometry.LogicToPixel(rDestPt));
    Size aOutSz(maGeometry.LogicToPixel(rDestSize));
    tools::Rectangle aDstRect(Point(), GetSizeInPixels());

    const bool bHMirr = aOutSz.Width() < 0;
    const bool bVMirr = aOutSz.Height() < 0;

    ClipToPaintRegion(aDstRect);

    BmpMirrorFlags mirrorFlags = BmpMirrorFlags::NONE;
    if (bHMirr)
    {
        aOutSz.setWidth(-aOutSz.Width());
        aOutPt.AdjustX(-(aOutSz.Width() - 1));
        mirrorFlags |= BmpMirrorFlags::Horizontal;
    }

    if (bVMirr)
    {
        aOutSz.setHeight(-aOutSz.Height());
        aOutPt.AdjustY(-(aOutSz.Height() - 1));
        mirrorFlags |= BmpMirrorFlags::Vertical;
    }

    if (aDstRect.Intersection(tools::Rectangle(aOutPt, aOutSz)).IsEmpty())
        return;

    {
        Point aRelPt = aOutPt + Point(GetXOffsetInPixels(), GetYOffsetInPixels());
        SalTwoRect aTR(rSrcPtPixel.X(), rSrcPtPixel.Y(), rSrcSizePixel.Width(),
                       rSrcSizePixel.Height(), aRelPt.X(), aRelPt.Y(), aOutSz.Width(),
                       aOutSz.Height());

        Bitmap bitmap(rBmp);
        AlphaMask alpha(rAlpha);
        if (bHMirr || bVMirr)
        {
            bitmap.Mirror(mirrorFlags);
            alpha.Mirror(mirrorFlags);
        }
        SalBitmap* pSalSrcBmp = bitmap.ImplGetSalBitmap().get();
        SalBitmap* pSalAlphaBmp = alpha.ImplGetSalBitmap().get();

        // #i83087# Naturally, system alpha blending (SalGraphics::DrawAlphaBitmap) cannot work
        // with separate alpha VDev

        // try to blend the alpha bitmap with the alpha virtual device
        if (mpAlphaVDev)
        {
            Bitmap aAlphaBitmap(mpAlphaVDev->GetBitmap(aRelPt, aOutSz));
            if (SalBitmap* pSalAlphaBmp2 = aAlphaBitmap.ImplGetSalBitmap().get())
            {
                if (mpGraphics->BlendAlphaBitmap(aTR, *pSalSrcBmp, *pSalAlphaBmp, *pSalAlphaBmp2,
                                                 *this))
                {
                    mpAlphaVDev->BlendBitmap(aTR, rAlpha);
                    return;
                }
            }
        }
        else
        {
            if (mpGraphics->DrawAlphaBitmap(aTR, *pSalSrcBmp, *pSalAlphaBmp, *this))
                return;
        }

        // we need to make sure Skia never reaches this slow code path
        assert(!SkiaHelper::isVCLSkiaEnabled());
    }

    tools::Rectangle aBmpRect(Point(), rBmp.GetSizePixel());
    if (!aBmpRect.Intersection(tools::Rectangle(rSrcPtPixel, rSrcSizePixel)).IsEmpty())
    {
        Point auxOutPt(maGeometry.LogicToPixel(rDestPt));
        Size auxOutSz(maGeometry.LogicToPixel(rDestSize));

        // HACK: The function is broken with alpha vdev and mirroring, mirror here.
        Bitmap bitmap(rBmp);
        AlphaMask alpha(rAlpha);
        if (mpAlphaVDev && (bHMirr || bVMirr))
        {
            bitmap.Mirror(mirrorFlags);
            alpha.Mirror(mirrorFlags);
            auxOutPt = aOutPt;
            auxOutSz = aOutSz;
        }
        DrawTransparentAlphaBitmapSlowPath(bitmap, alpha, aDstRect, aBmpRect, auxOutSz, auxOutPt);
    }
}

void OutputDevice::DrawTransparentAlphaBitmapSlowPath(const Bitmap& rBitmap,
                                                      const AlphaMask& rAlpha,
                                                      tools::Rectangle aDstRect,
                                                      tools::Rectangle aBmpRect,
                                                      Size const& aOutSize, Point const& aOutPoint)
{
    assert(!is_double_buffered_window());

    VirtualDevice* pOldVDev = mpAlphaVDev;

    const bool bHMirr = aOutSize.Width() < 0;
    const bool bVMirr = aOutSize.Height() < 0;

    // The scaling in this code path produces really ugly results - it
    // does the most trivial scaling with no smoothing.
    GDIMetaFile* pOldMetaFile = mpMetaFile;
    const bool bOldMap = IsMapModeEnabled();

    mpMetaFile = nullptr; // fdo#55044 reset before GetBitmap!
    DisableMapMode();

    Bitmap aBmp(GetBitmap(aDstRect.TopLeft(), aDstRect.GetSize()));

    // #109044# The generated bitmap need not necessarily be
    // of aDstRect dimensions, it's internally clipped to
    // window bounds. Thus, we correct the dest size here,
    // since we later use it (in nDstWidth/Height) for pixel
    // access)
    // #i38887# reading from screen may sometimes fail
    if (aBmp.ImplGetSalBitmap())
    {
        aDstRect.SetSize(aBmp.GetSizePixel());
    }

    const tools::Long nDstWidth = aDstRect.GetWidth();
    const tools::Long nDstHeight = aDstRect.GetHeight();

    // calculate offset in original bitmap
    // in RTL case this is a little more complicated since the contents of the
    // bitmap is not mirrored (it never is), however the paint region and bmp region
    // are in mirrored coordinates, so the intersection of (aOutPt,aOutSz) with these
    // is content wise somewhere else and needs to take mirroring into account
    const tools::Long nOffX = IsRTLEnabled() ? aOutSize.Width() - aDstRect.GetWidth()
                                                   - (aDstRect.Left() - aOutPoint.X())
                                             : aDstRect.Left() - aOutPoint.X();

    const tools::Long nOffY = aDstRect.Top() - aOutPoint.Y();

    TradScaleContext aTradContext(aDstRect, aBmpRect, aOutSize, nOffX, nOffY);

    Bitmap::ScopedReadAccess pBitmapReadAccess(const_cast<Bitmap&>(rBitmap));
    AlphaMask::ScopedReadAccess pAlphaReadAccess(const_cast<AlphaMask&>(rAlpha));

    DBG_ASSERT(pAlphaReadAccess->GetScanlineFormat() == ScanlineFormat::N8BitPal,
               "OutputDevice::ImplDrawAlpha(): non-8bit alpha no longer supported!");

    // #i38887# reading from screen may sometimes fail
    if (aBmp.ImplGetSalBitmap())
    {
        Bitmap aNewBitmap;

        if (mpAlphaVDev)
        {
            aNewBitmap = BlendBitmapWithAlpha(aBmp, pBitmapReadAccess.get(), pAlphaReadAccess.get(),
                                              aDstRect, nOffY, nDstHeight, nOffX, nDstWidth,
                                              aTradContext.mpMapX.get(), aTradContext.mpMapY.get());
        }
        else
        {
            LinearScaleContext aLinearContext(aDstRect, aBmpRect, aOutSize, nOffX, nOffY);

            if (aLinearContext.blendBitmap(BitmapScopedWriteAccess(aBmp).get(),
                                           pBitmapReadAccess.get(), pAlphaReadAccess.get(),
                                           nDstWidth, nDstHeight))
            {
                aNewBitmap = aBmp;
            }
            else
            {
                aNewBitmap
                    = BlendBitmap(aBmp, pBitmapReadAccess.get(), pAlphaReadAccess.get(), nOffY,
                                  nDstHeight, nOffX, nDstWidth, aBmpRect, aOutSize, bHMirr, bVMirr,
                                  aTradContext.mpMapX.get(), aTradContext.mpMapY.get());
            }
        }

        // #110958# Disable alpha VDev, we're doing the necessary
        // stuff explicitly further below
        if (mpAlphaVDev)
            mpAlphaVDev = nullptr;

        DrawBitmap(aDstRect.TopLeft(), aNewBitmap);

        // #110958# Enable alpha VDev again
        mpAlphaVDev = pOldVDev;
    }

    if (bOldMap)
        EnableMapMode();
    else
        DisableMapMode();

    mpMetaFile = pOldMetaFile;
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
