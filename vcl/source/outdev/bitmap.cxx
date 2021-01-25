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

#include <TradScaleContext.hxx>
#include <LinearScaleContext.hxx>
#include <bitmap/BitmapWriteAccess.hxx>
#include <drawmode.hxx>
#include <salbmp.hxx>
#include <salgdi.hxx>

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

    RenderContext2::DrawBitmap(rDestPt, rDestSize, rSrcPtPixel, rSrcSizePixel, rBitmap, nAction);

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

Bitmap OutputDevice::CreateTransparentAlphaBitmap(const Bitmap& rBitmap,
                                                const AlphaMask& rAlpha,
                                                tools::Rectangle aDstRect,
                                                tools::Rectangle aBmpRect,
                                                Size const& aOutSize, Point const& aOutPoint)
{
    const bool bHMirr = aOutSize.Width() < 0;
    const bool bVMirr = aOutSize.Height() < 0;

    Bitmap aBmp(GetBitmap(aDstRect.TopLeft(), aDstRect.GetSize()));

    // #109044# The generated bitmap need not necessarily be of aDstRect dimensions, it's internally
    // clipped to window bounds. Thus, we correct the dest size here, since we later use it (in
    // nDstWidth/Height) for pixel access).
    // #i38887# reading from screen may sometimes fail
    if (aBmp.ImplGetSalBitmap())
        aDstRect.SetSize(aBmp.GetSizePixel());

    // calculate offset in original bitmap in RTL case this is a little more complicated since the
    // contents of the bitmap is not mirrored (it never is), however the paint region and bmp region
    // are in mirrored coordinates, so the intersection of (aOutPt,aOutSz) with these is content
    // wise somewhere else and needs to take mirroring into account
    tools::Long nOffX = 0;

    if (IsRTLEnabled())
        nOffX = aOutSize.Width() - aDstRect.GetWidth() - (aDstRect.Left() - aOutPoint.X());
    else
        nOffX = aDstRect.Left() - aOutPoint.X();

    const tools::Long nOffY = aDstRect.Top() - aOutPoint.Y();

    // #i38887# reading from screen may sometimes fail

    // #i38887# reading from screen may sometimes fail
    if (mpAlphaVDev && aBmp.ImplGetSalBitmap())
        return mpAlphaVDev->CreateTransparentAlphaBitmap(rBitmap, rAlpha, aDstRect, aBmpRect, aOutSize, aOutPoint);

    Bitmap aNewBitmap;

    if (!mpAlphaVDev && aBmp.ImplGetSalBitmap())
    {
        Bitmap::ScopedReadAccess pBitmapReadAccess(const_cast<Bitmap&>(rBitmap));
        AlphaMask::ScopedReadAccess pAlphaReadAccess(const_cast<AlphaMask&>(rAlpha));

        DBG_ASSERT(pAlphaReadAccess->GetScanlineFormat() == ScanlineFormat::N8BitPal,
                   "non-8bit alpha no longer supported!");

        LinearScaleContext aLinearContext(aDstRect, aBmpRect, aOutSize, nOffX, nOffY);

        if (aLinearContext.blendBitmap(BitmapScopedWriteAccess(aBmp).get(),
                                       pBitmapReadAccess.get(), pAlphaReadAccess.get(),
                                       aDstRect.GetWidth(), aDstRect.GetHeight()))
        {
            aNewBitmap = aBmp;
        }
        else
        {
            TradScaleContext aTradContext(aDstRect, aBmpRect, aOutSize, nOffX, nOffY);

            aNewBitmap
                = BlendBitmap(aBmp, pBitmapReadAccess.get(), pAlphaReadAccess.get(), nOffY,
                              aDstRect.GetHeight(), nOffX, aDstRect.GetWidth(), aBmpRect, aOutSize, bHMirr, bVMirr,
                              aTradContext.mpMapX.get(), aTradContext.mpMapY.get());
        }
    }

    return aNewBitmap;
}

void OutputDevice::DrawTransparentAlphaBitmapSlowPath(const Bitmap& rBitmap,
                                                      const AlphaMask& rAlpha,
                                                      tools::Rectangle aDstRect,
                                                      tools::Rectangle aBmpRect,
                                                      Size const& aOutSize, Point const& aOutPoint)
{
    assert(!is_double_buffered_window());

    // The scaling in this code path produces really ugly results - it
    // does the most trivial scaling with no smoothing.
    GDIMetaFile* pOldMetaFile = mpMetaFile;
    const bool bOldMap = IsMapModeEnabled();

    mpMetaFile = nullptr; // fdo#55044 reset before GetBitmap!
    DisableMapMode();

    Bitmap aNewBitmap(CreateTransparentAlphaBitmap(rBitmap, rAlpha, aDstRect, aBmpRect, aOutSize, aOutPoint));

    // #110958# Disable alpha VDev, we're doing the necessary
    // stuff explicitly further below
    VirtualDevice* pOldVDev = mpAlphaVDev;

    if (mpAlphaVDev)
        mpAlphaVDev = nullptr;

    DrawBitmap(aDstRect.TopLeft(), aNewBitmap);

    // #110958# Enable alpha VDev again
    mpAlphaVDev = pOldVDev;

    if (bOldMap)
        EnableMapMode();
    else
        DisableMapMode();

    mpMetaFile = pOldMetaFile;
}

// Co = Cs + Cd*(1-As) premultiplied alpha -or-
// Co = (AsCs + AdCd*(1-As)) / Ao
static sal_uInt8 CalcColor(const sal_uInt8 nSourceColor, const sal_uInt8 nSourceAlpha,
                    const sal_uInt8 nDstAlpha, const sal_uInt8 nResAlpha,
                    const sal_uInt8 nDestColor)
{
    int c = nResAlpha ? (static_cast<int>(nSourceAlpha) * nSourceColor
                         + static_cast<int>(nDstAlpha) * nDestColor
                         - static_cast<int>(nDstAlpha) * nDestColor * nSourceAlpha / 255)
                            / static_cast<int>(nResAlpha)
                      : 0;
    return sal_uInt8(c);
}

static BitmapColor AlphaBlend(int nX, int nY, const tools::Long nMapX, const tools::Long nMapY,
                       BitmapReadAccess const* pP, BitmapReadAccess const* pA,
                       BitmapReadAccess const* pB, BitmapWriteAccess const* pAlphaW,
                       sal_uInt8& nResAlpha)
{
    BitmapColor aDstCol, aSrcCol;
    aSrcCol = pP->GetColor(nMapY, nMapX);
    aDstCol = pB->GetColor(nY, nX);

    // vcl stores transparency, not alpha - invert it
    const sal_uInt8 nSrcAlpha = 255 - pA->GetPixelIndex(nMapY, nMapX);
    const sal_uInt8 nDstAlpha = 255 - pAlphaW->GetPixelIndex(nY, nX);

    // Perform porter-duff compositing 'over' operation

    // Co = Cs + Cd*(1-As)
    // Ad = As + Ad*(1-As)
    nResAlpha = static_cast<int>(nSrcAlpha) + static_cast<int>(nDstAlpha)
                - static_cast<int>(nDstAlpha) * nSrcAlpha / 255;

    aDstCol.SetRed(CalcColor(aSrcCol.GetRed(), nSrcAlpha, nDstAlpha, nResAlpha, aDstCol.GetRed()));
    aDstCol.SetBlue(
        CalcColor(aSrcCol.GetBlue(), nSrcAlpha, nDstAlpha, nResAlpha, aDstCol.GetBlue()));
    aDstCol.SetGreen(
        CalcColor(aSrcCol.GetGreen(), nSrcAlpha, nDstAlpha, nResAlpha, aDstCol.GetGreen()));

    return aDstCol;
}

Bitmap OutputDevice::BlendBitmapWithAlpha(Bitmap& aBmp, BitmapReadAccess const* pP,
                                          BitmapReadAccess const* pA,
                                          const tools::Rectangle& aDstRect, const sal_Int32 nOffY,
                                          const sal_Int32 nDstHeight, const sal_Int32 nOffX,
                                          const sal_Int32 nDstWidth, const tools::Long* pMapX,
                                          const tools::Long* pMapY)

{
    BitmapColor aDstCol;
    Bitmap res;
    int nX, nY;
    sal_uInt8 nResAlpha;

    SAL_WARN_IF(!mpAlphaVDev, "vcl.gdi",
                "BlendBitmapWithAlpha(): call me only with valid alpha VirtualDevice!");

    bool bOldMapMode(mpAlphaVDev->IsMapModeEnabled());
    mpAlphaVDev->DisableMapMode();

    Bitmap aAlphaBitmap(mpAlphaVDev->GetBitmap(aDstRect.TopLeft(), aDstRect.GetSize()));
    BitmapScopedWriteAccess pAlphaW(aAlphaBitmap);

    if (GetBitCount() <= 8)
    {
        Bitmap aDither(aBmp.GetSizePixel(), 8);
        BitmapColor aIndex(0);
        Bitmap::ScopedReadAccess pB(aBmp);
        BitmapScopedWriteAccess pW(aDither);

        if (pB && pP && pA && pW && pAlphaW)
        {
            int nOutY;

            for (nY = 0, nOutY = nOffY; nY < nDstHeight; nY++, nOutY++)
            {
                const tools::Long nMapY = pMapY[nY];
                const tools::Long nModY = (nOutY & 0x0FL) << 4;
                int nOutX;

                Scanline pScanline = pW->GetScanline(nY);
                Scanline pScanlineAlpha = pAlphaW->GetScanline(nY);
                for (nX = 0, nOutX = nOffX; nX < nDstWidth; nX++, nOutX++)
                {
                    const tools::Long nMapX = pMapX[nX];
                    const sal_uLong nD = nVCLDitherLut[nModY | (nOutX & 0x0FL)];

                    aDstCol = AlphaBlend(nX, nY, nMapX, nMapY, pP, pA, pB.get(), pAlphaW.get(),
                                         nResAlpha);

                    aIndex.SetIndex(static_cast<sal_uInt8>(
                        nVCLRLut[(nVCLLut[aDstCol.GetRed()] + nD) >> 16]
                        + nVCLGLut[(nVCLLut[aDstCol.GetGreen()] + nD) >> 16]
                        + nVCLBLut[(nVCLLut[aDstCol.GetBlue()] + nD) >> 16]));
                    pW->SetPixelOnData(pScanline, nX, aIndex);

                    aIndex.SetIndex(
                        static_cast<sal_uInt8>(nVCLRLut[(nVCLLut[255 - nResAlpha] + nD) >> 16]
                                               + nVCLGLut[(nVCLLut[255 - nResAlpha] + nD) >> 16]
                                               + nVCLBLut[(nVCLLut[255 - nResAlpha] + nD) >> 16]));
                    pAlphaW->SetPixelOnData(pScanlineAlpha, nX, aIndex);
                }
            }
        }
        pB.reset();
        pW.reset();
        res = aDither;
    }
    else
    {
        BitmapScopedWriteAccess pB(aBmp);
        if (pB && pP && pA && pAlphaW)
        {
            for (nY = 0; nY < nDstHeight; nY++)
            {
                const tools::Long nMapY = pMapY[nY];
                Scanline pScanlineB = pB->GetScanline(nY);
                Scanline pScanlineAlpha = pAlphaW->GetScanline(nY);

                for (nX = 0; nX < nDstWidth; nX++)
                {
                    const tools::Long nMapX = pMapX[nX];
                    aDstCol = AlphaBlend(nX, nY, nMapX, nMapY, pP, pA, pB.get(), pAlphaW.get(),
                                         nResAlpha);

                    pB->SetPixelOnData(pScanlineB, nX, pB->GetBestMatchingColor(aDstCol));
                    pAlphaW->SetPixelOnData(
                        pScanlineAlpha, nX,
                        pB->GetBestMatchingColor(
                            Color(255L - nResAlpha, 255L - nResAlpha, 255L - nResAlpha)));
                }
            }
        }
        pB.reset();
        res = aBmp;
    }

    pAlphaW.reset();
    mpAlphaVDev->DrawBitmap(aDstRect.TopLeft(), aAlphaBitmap);

    if (bOldMapMode)
        mpAlphaVDev->EnableMapMode();
    else
        mpAlphaVDev->DisableMapMode();

    return res;
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
