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

#include <bitmap/BitmapWriteAccess.hxx>
#include <bitmap/bmpfast.hxx>
#include <salgdi.hxx>
#include <salbmp.hxx>

void RenderContext2::BlendBitmap(const SalTwoRect& rPosAry, const Bitmap& rBmp)
{
    OutputDevice const* pOutDev = dynamic_cast<OutputDevice const*>(this);
    mpGraphics->BlendBitmap(rPosAry, *rBmp.ImplGetSalBitmap(), *pOutDev);
}

Bitmap RenderContext2::BlendBitmap(Bitmap& aBmp, BitmapReadAccess const* pP,
                                   BitmapReadAccess const* pA, const sal_Int32 nOffY,
                                   const sal_Int32 nDstHeight, const sal_Int32 nOffX,
                                   const sal_Int32 nDstWidth, const tools::Rectangle& aBmpRect,
                                   const Size& aOutSz, const bool bHMirr, const bool bVMirr,
                                   const tools::Long* pMapX, const tools::Long* pMapY)
{
    BitmapColor aDstCol;
    Bitmap res;
    int nX, nY;

    if (GetBitCount() <= 8)
    {
        Bitmap aDither(aBmp.GetSizePixel(), 8);
        BitmapColor aIndex(0);
        Bitmap::ScopedReadAccess pB(aBmp);
        BitmapScopedWriteAccess pW(aDither);

        if (pB && pP && pA && pW)
        {
            int nOutY;

            for (nY = 0, nOutY = nOffY; nY < nDstHeight; nY++, nOutY++)
            {
                tools::Long nMapY = pMapY[nY];
                if (bVMirr)
                {
                    nMapY = aBmpRect.Bottom() - nMapY;
                }
                const tools::Long nModY = (nOutY & 0x0FL) << 4;
                int nOutX;

                Scanline pScanline = pW->GetScanline(nY);
                Scanline pScanlineAlpha = pA->GetScanline(nMapY);
                for (nX = 0, nOutX = nOffX; nX < nDstWidth; nX++, nOutX++)
                {
                    tools::Long nMapX = pMapX[nX];
                    if (bHMirr)
                    {
                        nMapX = aBmpRect.Right() - nMapX;
                    }
                    const sal_uLong nD = nVCLDitherLut[nModY | (nOutX & 0x0FL)];

                    aDstCol = pB->GetColor(nY, nX);
                    aDstCol.Merge(pP->GetColor(nMapY, nMapX),
                                  pA->GetIndexFromData(pScanlineAlpha, nMapX));
                    aIndex.SetIndex(static_cast<sal_uInt8>(
                        nVCLRLut[(nVCLLut[aDstCol.GetRed()] + nD) >> 16]
                        + nVCLGLut[(nVCLLut[aDstCol.GetGreen()] + nD) >> 16]
                        + nVCLBLut[(nVCLLut[aDstCol.GetBlue()] + nD) >> 16]));
                    pW->SetPixelOnData(pScanline, nX, aIndex);
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

        bool bFastBlend = false;
        if (pP && pA && pB && !bHMirr && !bVMirr)
        {
            SalTwoRect aTR(aBmpRect.Left(), aBmpRect.Top(), aBmpRect.GetWidth(),
                           aBmpRect.GetHeight(), nOffX, nOffY, aOutSz.Width(), aOutSz.Height());

            bFastBlend = ImplFastBitmapBlending(*pB, *pP, *pA, aTR);
        }

        if (pP && pA && pB && !bFastBlend)
        {
            switch (pP->GetScanlineFormat())
            {
                case ScanlineFormat::N8BitPal:
                {
                    for (nY = 0; nY < nDstHeight; nY++)
                    {
                        tools::Long nMapY = pMapY[nY];
                        if (bVMirr)
                        {
                            nMapY = aBmpRect.Bottom() - nMapY;
                        }
                        Scanline pPScan = pP->GetScanline(nMapY);
                        Scanline pAScan = pA->GetScanline(nMapY);
                        Scanline pBScan = pB->GetScanline(nY);

                        for (nX = 0; nX < nDstWidth; nX++)
                        {
                            tools::Long nMapX = pMapX[nX];

                            if (bHMirr)
                            {
                                nMapX = aBmpRect.Right() - nMapX;
                            }
                            aDstCol = pB->GetPixelFromData(pBScan, nX);
                            aDstCol.Merge(pP->GetPaletteColor(pPScan[nMapX]), pAScan[nMapX]);
                            pB->SetPixelOnData(pBScan, nX, aDstCol);
                        }
                    }
                }
                break;

                default:
                {
                    for (nY = 0; nY < nDstHeight; nY++)
                    {
                        tools::Long nMapY = pMapY[nY];

                        if (bVMirr)
                        {
                            nMapY = aBmpRect.Bottom() - nMapY;
                        }
                        Scanline pAScan = pA->GetScanline(nMapY);
                        Scanline pBScan = pB->GetScanline(nY);
                        for (nX = 0; nX < nDstWidth; nX++)
                        {
                            tools::Long nMapX = pMapX[nX];

                            if (bHMirr)
                            {
                                nMapX = aBmpRect.Right() - nMapX;
                            }
                            aDstCol = pB->GetPixelFromData(pBScan, nX);
                            aDstCol.Merge(pP->GetColor(nMapY, nMapX), pAScan[nMapX]);
                            pB->SetPixelOnData(pBScan, nX, aDstCol);
                        }
                    }
                }
                break;
            }
        }

        pB.reset();
        res = aBmp;
    }

    return res;
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
