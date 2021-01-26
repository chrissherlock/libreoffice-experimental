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

#include <tools/gen.hxx>
#include <tools/helpers.hxx>

#include <LinearScaleContext.hxx>
#include <bitmap/BitmapWriteAccess.hxx>

#include <cassert>

LinearScaleContext::LinearScaleContext(tools::Rectangle const& aDstRect,
                                       tools::Rectangle const& aBitmapRect, Size const& aOutSize,
                                       tools::Long nOffX, tools::Long nOffY)

    : mpMapX(new tools::Long[aDstRect.GetWidth()])
    , mpMapY(new tools::Long[aDstRect.GetHeight()])
    , mpMapXOffset(new tools::Long[aDstRect.GetWidth()])
    , mpMapYOffset(new tools::Long[aDstRect.GetHeight()])
{
    const tools::Long nSrcWidth = aBitmapRect.GetWidth();
    const tools::Long nSrcHeight = aBitmapRect.GetHeight();

    generateSimpleMap(nSrcWidth, aDstRect.GetWidth(), aBitmapRect.Left(), aOutSize.Width(), nOffX,
                      mpMapX.get(), mpMapXOffset.get());

    generateSimpleMap(nSrcHeight, aDstRect.GetHeight(), aBitmapRect.Top(), aOutSize.Height(), nOffY,
                      mpMapY.get(), mpMapYOffset.get());
}

void LinearScaleContext::generateSimpleMap(tools::Long nSrcDimension, tools::Long nDstDimension,
                                           tools::Long nDstLocation, tools::Long nOutDimension,
                                           tools::Long nOffset, tools::Long* pMap,
                                           tools::Long* pMapOffset)
{
    const double fReverseScale = (std::abs(nOutDimension) > 1)
                                     ? (nSrcDimension - 1) / double(std::abs(nOutDimension) - 1)
                                     : 0.0;

    tools::Long nSampleRange = std::max(tools::Long(0), nSrcDimension - 2);

    for (tools::Long i = 0; i < nDstDimension; i++)
    {
        double fTemp = std::abs((nOffset + i) * fReverseScale);

        pMap[i] = MinMax(nDstLocation + tools::Long(fTemp), 0, nSampleRange);
        pMapOffset[i] = static_cast<tools::Long>((fTemp - pMap[i]) * 128.0);
    }
}

bool LinearScaleContext::blendBitmap(Bitmap const& rBitmapSource, Bitmap const& rBitmapDest,
                                     AlphaMask const& rAlpha, Size const& rDstSize)
{
    Bitmap::ScopedReadAccess pSource(const_cast<Bitmap&>(rBitmapSource));
    BitmapScopedWriteAccess pDestination(BitmapScopedWriteAccess(const_cast<Bitmap&>(rBitmapDest)));
    AlphaMask::ScopedReadAccess pSourceAlpha(const_cast<AlphaMask&>(rAlpha));

    assert(pSourceAlpha->GetScanlineFormat() == ScanlineFormat::N8BitPal
           && "non-8bit alpha no longer supported!");

    if (pSource && pSourceAlpha && pDestination)
    {
        ScanlineFormat nSourceFormat = pSource->GetScanlineFormat();
        ScanlineFormat nDestinationFormat = pDestination->GetScanlineFormat();

        switch (nSourceFormat)
        {
            case ScanlineFormat::N24BitTcRgb:
            case ScanlineFormat::N24BitTcBgr:
            {
                const tools::Long nDstWidth = rDstSize.Width();
                const tools::Long nDstHeight = rDstSize.Height();

                if ((nSourceFormat == ScanlineFormat::N24BitTcBgr
                     && nDestinationFormat == ScanlineFormat::N32BitTcBgra)
                    || (nSourceFormat == ScanlineFormat::N24BitTcRgb
                        && nDestinationFormat == ScanlineFormat::N32BitTcRgba))
                {
                    blendBitmap24(pDestination.get(), pSource.get(), pSourceAlpha.get(), nDstWidth,
                                  nDstHeight);
                    return true;
                }
            }
            break;
            default:
                break;
        }
    }
    return false;
}

void LinearScaleContext::blendBitmap24(const BitmapWriteAccess* pDestination,
                                       const BitmapReadAccess* pSource,
                                       const BitmapReadAccess* pSourceAlpha,
                                       const tools::Long nDstWidth, const tools::Long nDstHeight)
{
    Scanline pLine0, pLine1;
    Scanline pLineAlpha0, pLineAlpha1;
    Scanline pColorSample1, pColorSample2;
    Scanline pDestScanline;

    tools::Long nColor1Line1, nColor2Line1, nColor3Line1;
    tools::Long nColor1Line2, nColor2Line2, nColor3Line2;
    tools::Long nAlphaLine1, nAlphaLine2;

    sal_uInt8 nColor1, nColor2, nColor3, nAlpha;

    for (tools::Long nY = 0; nY < nDstHeight; nY++)
    {
        const tools::Long nMapY = mpMapY[nY];
        const tools::Long nMapFY = mpMapYOffset[nY];

        pLine0 = pSource->GetScanline(nMapY);
        // tdf#95481 guard nMapY + 1 to be within bounds
        pLine1 = (nMapY + 1 < pSource->Height()) ? pSource->GetScanline(nMapY + 1) : pLine0;

        pLineAlpha0 = pSourceAlpha->GetScanline(nMapY);
        // tdf#95481 guard nMapY + 1 to be within bounds
        pLineAlpha1 = (nMapY + 1 < pSourceAlpha->Height()) ? pSourceAlpha->GetScanline(nMapY + 1)
                                                           : pLineAlpha0;

        pDestScanline = pDestination->GetScanline(nY);

        for (tools::Long nX = 0; nX < nDstWidth; nX++)
        {
            const tools::Long nMapX = mpMapX[nX];
            const tools::Long nMapFX = mpMapXOffset[nX];

            pColorSample1 = pLine0 + 3 * nMapX;
            pColorSample2 = (nMapX + 1 < pSource->Width()) ? pColorSample1 + 3 : pColorSample1;
            nColor1Line1 = (static_cast<tools::Long>(*pColorSample1) << 7)
                           + nMapFX * (static_cast<tools::Long>(*pColorSample2) - *pColorSample1);

            pColorSample1++;
            pColorSample2++;
            nColor2Line1 = (static_cast<tools::Long>(*pColorSample1) << 7)
                           + nMapFX * (static_cast<tools::Long>(*pColorSample2) - *pColorSample1);

            pColorSample1++;
            pColorSample2++;
            nColor3Line1 = (static_cast<tools::Long>(*pColorSample1) << 7)
                           + nMapFX * (static_cast<tools::Long>(*pColorSample2) - *pColorSample1);

            pColorSample1 = pLine1 + 3 * nMapX;
            pColorSample2 = (nMapX + 1 < pSource->Width()) ? pColorSample1 + 3 : pColorSample1;
            nColor1Line2 = (static_cast<tools::Long>(*pColorSample1) << 7)
                           + nMapFX * (static_cast<tools::Long>(*pColorSample2) - *pColorSample1);

            pColorSample1++;
            pColorSample2++;
            nColor2Line2 = (static_cast<tools::Long>(*pColorSample1) << 7)
                           + nMapFX * (static_cast<tools::Long>(*pColorSample2) - *pColorSample1);

            pColorSample1++;
            pColorSample2++;
            nColor3Line2 = (static_cast<tools::Long>(*pColorSample1) << 7)
                           + nMapFX * (static_cast<tools::Long>(*pColorSample2) - *pColorSample1);

            pColorSample1 = pLineAlpha0 + nMapX;
            pColorSample2 = (nMapX + 1 < pSourceAlpha->Width()) ? pColorSample1 + 1 : pColorSample1;
            nAlphaLine1 = (static_cast<tools::Long>(*pColorSample1) << 7)
                          + nMapFX * (static_cast<tools::Long>(*pColorSample2) - *pColorSample1);

            pColorSample1 = pLineAlpha1 + nMapX;
            pColorSample2 = (nMapX + 1 < pSourceAlpha->Width()) ? pColorSample1 + 1 : pColorSample1;
            nAlphaLine2 = (static_cast<tools::Long>(*pColorSample1) << 7)
                          + nMapFX * (static_cast<tools::Long>(*pColorSample2) - *pColorSample1);

            nColor1 = (nColor1Line1 + nMapFY * ((nColor1Line2 >> 7) - (nColor1Line1 >> 7))) >> 7;
            nColor2 = (nColor2Line1 + nMapFY * ((nColor2Line2 >> 7) - (nColor2Line1 >> 7))) >> 7;
            nColor3 = (nColor3Line1 + nMapFY * ((nColor3Line2 >> 7) - (nColor3Line1 >> 7))) >> 7;

            nAlpha = (nAlphaLine1 + nMapFY * ((nAlphaLine2 >> 7) - (nAlphaLine1 >> 7))) >> 7;

            *pDestScanline = color::ColorChannelMerge(*pDestScanline, nColor1, nAlpha);
            pDestScanline++;
            *pDestScanline = color::ColorChannelMerge(*pDestScanline, nColor2, nAlpha);
            pDestScanline++;
            *pDestScanline = color::ColorChannelMerge(*pDestScanline, nColor3, nAlpha);
            pDestScanline++;
            pDestScanline++;
        }
    }
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
