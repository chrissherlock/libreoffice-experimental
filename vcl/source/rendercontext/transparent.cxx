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

#include <tools/helpers.hxx>
#include <tools/poly.hxx>

#include <vcl/virdev.hxx>

#include <bitmap/BitmapWriteAccess.hxx>
#include <salgdi.hxx>

void RenderContext2::DrawInvisiblePolygon(tools::PolyPolygon const& rPolyPoly)
{
    assert(!is_double_buffered_window());

    // short circuit if the polygon border is invisible too
    if (!mbLineColor)
        return;

    // we assume that the border is NOT to be drawn transparently???
    Push(PushFlags::FILLCOLOR);
    SetFillColor();
    DrawPolyPolygon(rPolyPoly);
    Pop();
}

void RenderContext2::EmulateDrawTransparent(tools::PolyPolygon const& rPolyPoly,
                                            sal_uInt16 nTransparencePercent)
{
    // #110958# Disable alpha VDev, we perform the necessary
    VirtualDevice* pOldAlphaVDev = mpAlphaVDev;

    // operation explicitly further below.
    if (mpAlphaVDev)
        mpAlphaVDev = nullptr;

    tools::PolyPolygon aPolyPoly(LogicToPixel(rPolyPoly));
    tools::Rectangle aPolyRect(aPolyPoly.GetBoundRect());
    tools::Rectangle aDstRect(Point(), GetOutputSizePixel());

    aDstRect.Intersection(aPolyRect);

    ClipToPaintRegion(aDstRect);

    if (!aDstRect.IsEmpty())
    {
        bool bDrawn = false;

        // #i66849# Added fast path for exactly rectangular
        // polygons
        // #i83087# Naturally, system alpha blending cannot
        // work with separate alpha VDev
        if (!mpAlphaVDev && aPolyPoly.IsRect())
        {
            // setup Graphics only here (other cases delegate
            // to basic OutDev methods)
            if (mbInitClipRegion)
                InitClipRegion();

            if (mbInitLineColor)
                InitLineColor();

            if (mbInitFillColor)
                InitFillColor();

            tools::Rectangle aLogicPolyRect(rPolyPoly.GetBoundRect());
            tools::Rectangle aPixelRect(ImplLogicToDevicePixel(aLogicPolyRect));

            if (!mbOutputClipped)
            {
                bDrawn = mpGraphics->DrawAlphaRect(
                    aPixelRect.Left(), aPixelRect.Top(),
                    // #i98405# use methods with small g, else one pixel too much will be painted.
                    // This is because the source is a polygon which when painted would not paint
                    // the rightmost and lowest pixel line(s), so use one pixel less for the
                    // rectangle, too.
                    aPixelRect.getWidth(), aPixelRect.getHeight(),
                    sal::static_int_cast<sal_uInt8>(nTransparencePercent), *this);
            }
            else
            {
                bDrawn = true;
            }
        }

        if (!bDrawn)
        {
            ScopedVclPtrInstance<VirtualDevice> aVDev(*this, DeviceFormat::BITMASK);
            const Size aDstSz(aDstRect.GetSize());
            const sal_uInt8 cTrans
                = static_cast<sal_uInt8>(MinMax(FRound(nTransparencePercent * 2.55), 0, 255));

            if (aDstRect.Left() || aDstRect.Top())
                aPolyPoly.Move(-aDstRect.Left(), -aDstRect.Top());

            if (aVDev->SetOutputSizePixel(aDstSz))
            {
                const bool bOldMap = mbMap;

                EnableMapMode(false);

                aVDev->SetLineColor(COL_BLACK);
                aVDev->SetFillColor(COL_BLACK);
                aVDev->DrawPolyPolygon(aPolyPoly);

                Bitmap aPaint(GetBitmap(aDstRect.TopLeft(), aDstSz));
                Bitmap aPolyMask(aVDev->GetBitmap(Point(), aDstSz));

                // #107766# check for non-empty bitmaps before accessing them
                if (!aPaint.IsEmpty() && !aPolyMask.IsEmpty())
                {
                    BitmapScopedWriteAccess pW(aPaint);
                    Bitmap::ScopedReadAccess pR(aPolyMask);

                    if (pW && pR)
                    {
                        BitmapColor aPixCol;
                        const BitmapColor aFillCol(GetFillColor());
                        const BitmapColor aBlack(pR->GetBestMatchingColor(COL_BLACK));
                        const tools::Long nWidth = pW->Width();
                        const tools::Long nHeight = pW->Height();
                        const tools::Long nR = aFillCol.GetRed();
                        const tools::Long nG = aFillCol.GetGreen();
                        const tools::Long nB = aFillCol.GetBlue();
                        tools::Long nX, nY;

                        if (vcl::isPalettePixelFormat(aPaint.getPixelFormat()))
                        {
                            const BitmapPalette& rPal = pW->GetPalette();
                            const sal_uInt16 nCount = rPal.GetEntryCount();
                            std::unique_ptr<sal_uInt8[]> xMap(
                                new sal_uInt8[nCount * sizeof(BitmapColor)]);
                            BitmapColor* pMap = reinterpret_cast<BitmapColor*>(xMap.get());

                            for (sal_uInt16 i = 0; i < nCount; i++)
                            {
                                BitmapColor aCol(rPal[i]);
                                aCol.Merge(aFillCol, cTrans);
                                pMap[i]
                                    = BitmapColor(static_cast<sal_uInt8>(rPal.GetBestIndex(aCol)));
                            }

                            if (pR->GetScanlineFormat() == ScanlineFormat::N1BitMsbPal
                                && pW->GetScanlineFormat() == ScanlineFormat::N8BitPal)
                            {
                                const sal_uInt8 cBlack = aBlack.GetIndex();

                                for (nY = 0; nY < nHeight; nY++)
                                {
                                    Scanline pWScan = pW->GetScanline(nY);
                                    Scanline pRScan = pR->GetScanline(nY);
                                    sal_uInt8 cBit = 128;

                                    for (nX = 0; nX < nWidth; nX++, cBit >>= 1, pWScan++)
                                    {
                                        if (!cBit)
                                        {
                                            cBit = 128;
                                            pRScan += 1;
                                        }
                                        if ((*pRScan & cBit) == cBlack)
                                        {
                                            *pWScan = pMap[*pWScan].GetIndex();
                                        }
                                    }
                                }
                            }
                            else
                            {
                                for (nY = 0; nY < nHeight; nY++)
                                {
                                    Scanline pScanline = pW->GetScanline(nY);
                                    Scanline pScanlineRead = pR->GetScanline(nY);
                                    for (nX = 0; nX < nWidth; nX++)
                                    {
                                        if (pR->GetPixelFromData(pScanlineRead, nX) == aBlack)
                                        {
                                            pW->SetPixelOnData(
                                                pScanline, nX,
                                                pMap[pW->GetIndexFromData(pScanline, nX)]);
                                        }
                                    }
                                }
                            }
                        }
                        else
                        {
                            if (pR->GetScanlineFormat() == ScanlineFormat::N1BitMsbPal
                                && pW->GetScanlineFormat() == ScanlineFormat::N24BitTcBgr)
                            {
                                const sal_uInt8 cBlack = aBlack.GetIndex();

                                for (nY = 0; nY < nHeight; nY++)
                                {
                                    Scanline pWScan = pW->GetScanline(nY);
                                    Scanline pRScan = pR->GetScanline(nY);
                                    sal_uInt8 cBit = 128;

                                    for (nX = 0; nX < nWidth; nX++, cBit >>= 1, pWScan += 3)
                                    {
                                        if (!cBit)
                                        {
                                            cBit = 128;
                                            pRScan += 1;
                                        }
                                        if ((*pRScan & cBit) == cBlack)
                                        {
                                            pWScan[0]
                                                = color::ColorChannelMerge(pWScan[0], nB, cTrans);
                                            pWScan[1]
                                                = color::ColorChannelMerge(pWScan[1], nG, cTrans);
                                            pWScan[2]
                                                = color::ColorChannelMerge(pWScan[2], nR, cTrans);
                                        }
                                    }
                                }
                            }
                            else
                            {
                                for (nY = 0; nY < nHeight; nY++)
                                {
                                    Scanline pScanline = pW->GetScanline(nY);
                                    Scanline pScanlineRead = pR->GetScanline(nY);
                                    for (nX = 0; nX < nWidth; nX++)
                                    {
                                        if (pR->GetPixelFromData(pScanlineRead, nX) == aBlack)
                                        {
                                            aPixCol = pW->GetColor(nY, nX);
                                            aPixCol.Merge(aFillCol, cTrans);
                                            pW->SetPixelOnData(pScanline, nX, aPixCol);
                                        }
                                    }
                                }
                            }
                        }
                    }

                    pR.reset();
                    pW.reset();

                    DrawBitmap(aDstRect.TopLeft(), aPaint);

                    EnableMapMode(bOldMap);

                    if (mbLineColor)
                    {
                        Push(PushFlags::FILLCOLOR);
                        SetFillColor();
                        DrawPolyPolygon(rPolyPoly);
                        Pop();
                    }
                }
            }
            else
            {
                DrawPolyPolygon(rPolyPoly);
            }
        }
    }

    // #110958# Restore disabled alpha VDev
    mpAlphaVDev = pOldAlphaVDev;
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
