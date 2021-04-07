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

#include <vcl/skia/SkiaHelper.hxx>
#include <vcl/vclptr.hxx>
#include <vcl/virdev.hxx>

#include <bitmap/BitmapWriteAccess.hxx>
#include <salgdi.hxx>

void RenderContext2::DrawBitmap(Point const& rDestPt, Bitmap const& rBitmap)
{
    assert(!is_double_buffered_window());

    if (ImplIsRecordLayout())
        return;

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
    assert(!is_double_buffered_window());

    if (ImplIsRecordLayout())
        return;

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
    assert(!is_double_buffered_window());

    if (ImplIsRecordLayout())
        return;

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

    if (ImplIsRecordLayout())
        return;

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

Bitmap RenderContext2::GetBitmap(Point const& rSrcPt, Size const& rSize) const
{
    Bitmap aBmp;
    tools::Long nX = ImplLogicXToDevicePixel(rSrcPt.X());
    tools::Long nY = ImplLogicYToDevicePixel(rSrcPt.Y());
    tools::Long nWidth = ImplLogicWidthToDevicePixel(rSize.Width());
    tools::Long nHeight = ImplLogicHeightToDevicePixel(rSize.Height());

    if (mpGraphics || AcquireGraphics())
    {
        assert(mpGraphics);

        if (nWidth > 0 && nHeight > 0
            && nX <= (maGeometry.GetWidthInPixels() + maGeometry.GetXFrameOffset())
            && nY <= (maGeometry.GetHeightInPixels() + maGeometry.GetYFrameOffset()))
        {
            tools::Rectangle aRect(Point(nX, nY), Size(nWidth, nHeight));
            bool bClipped = false;

            // X-Coordinate outside of draw area?
            if (nX < maGeometry.GetXFrameOffset())
            {
                nWidth -= (maGeometry.GetXFrameOffset() - nX);
                nX = maGeometry.GetXFrameOffset();
                bClipped = true;
            }

            // Y-Coordinate outside of draw area?
            if (nY < maGeometry.GetYFrameOffset())
            {
                nHeight -= (maGeometry.GetYFrameOffset() - nY);
                nY = maGeometry.GetYFrameOffset();
                bClipped = true;
            }

            // Width outside of draw area?
            if ((nWidth + nX) > (maGeometry.GetWidthInPixels() + maGeometry.GetXFrameOffset()))
            {
                nWidth = maGeometry.GetXFrameOffset() + maGeometry.GetWidthInPixels() - nX;
                bClipped = true;
            }

            // Height outside of draw area?
            if ((nHeight + nY) > (maGeometry.GetHeightInPixels() + maGeometry.GetYFrameOffset()))
            {
                nHeight = maGeometry.GetYFrameOffset() + maGeometry.GetHeightInPixels() - nY;
                bClipped = true;
            }

            if (bClipped)
            {
                // If the visible part has been clipped, we have to create a
                // Bitmap with the correct size in which we copy the clipped
                // Bitmap to the correct position.
                ScopedVclPtrInstance<VirtualDevice> aVDev(*this);

                if (aVDev->SetOutputSizePixel(aRect.GetSize()))
                {
                    if (aVDev->mpGraphics || aVDev->AcquireGraphics())
                    {
                        if ((nWidth > 0) && (nHeight > 0))
                        {
                            SalTwoRect aPosAry(nX, nY, nWidth, nHeight,
                                               (aRect.Left() < maGeometry.GetXFrameOffset())
                                                   ? (maGeometry.GetXFrameOffset() - aRect.Left())
                                                   : 0L,
                                               (aRect.Top() < maGeometry.GetYFrameOffset())
                                                   ? (maGeometry.GetYFrameOffset() - aRect.Top())
                                                   : 0L,
                                               nWidth, nHeight);
                            aVDev->mpGraphics->CopyBits(aPosAry, *mpGraphics, *this, *this);
                        }

                        SAL_WARN_IF((nWidth <= 0) || (nHeight <= 0), "vcl.gdi",
                                    "zero or negative width or height");

                        aBmp = aVDev->GetBitmap(Point(), aVDev->GetSize());
                    }
                    else
                    {
                        bClipped = false;
                    }
                }
                else
                {
                    bClipped = false;
                }
            }

            if (!bClipped)
            {
                std::shared_ptr<SalBitmap> pSalBmp
                    = mpGraphics->GetBitmap(nX, nY, nWidth, nHeight, *this);

                if (pSalBmp)
                    aBmp.ImplSetSalBitmap(pSalBmp);
            }
        }
    }

    return aBmp;
}

namespace
{
struct LinearScaleContext
{
    std::unique_ptr<tools::Long[]> mpMapX;
    std::unique_ptr<tools::Long[]> mpMapY;

    std::unique_ptr<tools::Long[]> mpMapXOffset;
    std::unique_ptr<tools::Long[]> mpMapYOffset;

    LinearScaleContext(tools::Rectangle const& aDstRect, tools::Rectangle const& aBitmapRect,
                       Size const& aOutSize, tools::Long nOffX, tools::Long nOffY)

        : mpMapX(new tools::Long[aDstRect.GetWidth()])
        , mpMapY(new tools::Long[aDstRect.GetHeight()])
        , mpMapXOffset(new tools::Long[aDstRect.GetWidth()])
        , mpMapYOffset(new tools::Long[aDstRect.GetHeight()])
    {
        const tools::Long nSrcWidth = aBitmapRect.GetWidth();
        const tools::Long nSrcHeight = aBitmapRect.GetHeight();

        generateSimpleMap(nSrcWidth, aDstRect.GetWidth(), aBitmapRect.Left(), aOutSize.Width(),
                          nOffX, mpMapX.get(), mpMapXOffset.get());

        generateSimpleMap(nSrcHeight, aDstRect.GetHeight(), aBitmapRect.Top(), aOutSize.Height(),
                          nOffY, mpMapY.get(), mpMapYOffset.get());
    }

private:
    static void generateSimpleMap(tools::Long nSrcDimension, tools::Long nDstDimension,
                                  tools::Long nDstLocation, tools::Long nOutDimension,
                                  tools::Long nOffset, tools::Long* pMap, tools::Long* pMapOffset)
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

public:
    bool blendBitmap(const BitmapWriteAccess* pDestination, const BitmapReadAccess* pSource,
                     const BitmapReadAccess* pSourceAlpha, const tools::Long nDstWidth,
                     const tools::Long nDstHeight)
    {
        if (pSource && pSourceAlpha && pDestination)
        {
            ScanlineFormat nSourceFormat = pSource->GetScanlineFormat();
            ScanlineFormat nDestinationFormat = pDestination->GetScanlineFormat();

            switch (nSourceFormat)
            {
                case ScanlineFormat::N24BitTcRgb:
                case ScanlineFormat::N24BitTcBgr:
                {
                    if ((nSourceFormat == ScanlineFormat::N24BitTcBgr
                         && nDestinationFormat == ScanlineFormat::N32BitTcBgra)
                        || (nSourceFormat == ScanlineFormat::N24BitTcRgb
                            && nDestinationFormat == ScanlineFormat::N32BitTcRgba))
                    {
                        blendBitmap24(pDestination, pSource, pSourceAlpha, nDstWidth, nDstHeight);
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

    void blendBitmap24(const BitmapWriteAccess* pDestination, const BitmapReadAccess* pSource,
                       const BitmapReadAccess* pSourceAlpha, const tools::Long nDstWidth,
                       const tools::Long nDstHeight)
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
            pLineAlpha1 = (nMapY + 1 < pSourceAlpha->Height())
                              ? pSourceAlpha->GetScanline(nMapY + 1)
                              : pLineAlpha0;

            pDestScanline = pDestination->GetScanline(nY);

            for (tools::Long nX = 0; nX < nDstWidth; nX++)
            {
                const tools::Long nMapX = mpMapX[nX];
                const tools::Long nMapFX = mpMapXOffset[nX];

                pColorSample1 = pLine0 + 3 * nMapX;
                pColorSample2 = (nMapX + 1 < pSource->Width()) ? pColorSample1 + 3 : pColorSample1;
                nColor1Line1
                    = (static_cast<tools::Long>(*pColorSample1) << 7)
                      + nMapFX * (static_cast<tools::Long>(*pColorSample2) - *pColorSample1);

                pColorSample1++;
                pColorSample2++;
                nColor2Line1
                    = (static_cast<tools::Long>(*pColorSample1) << 7)
                      + nMapFX * (static_cast<tools::Long>(*pColorSample2) - *pColorSample1);

                pColorSample1++;
                pColorSample2++;
                nColor3Line1
                    = (static_cast<tools::Long>(*pColorSample1) << 7)
                      + nMapFX * (static_cast<tools::Long>(*pColorSample2) - *pColorSample1);

                pColorSample1 = pLine1 + 3 * nMapX;
                pColorSample2 = (nMapX + 1 < pSource->Width()) ? pColorSample1 + 3 : pColorSample1;
                nColor1Line2
                    = (static_cast<tools::Long>(*pColorSample1) << 7)
                      + nMapFX * (static_cast<tools::Long>(*pColorSample2) - *pColorSample1);

                pColorSample1++;
                pColorSample2++;
                nColor2Line2
                    = (static_cast<tools::Long>(*pColorSample1) << 7)
                      + nMapFX * (static_cast<tools::Long>(*pColorSample2) - *pColorSample1);

                pColorSample1++;
                pColorSample2++;
                nColor3Line2
                    = (static_cast<tools::Long>(*pColorSample1) << 7)
                      + nMapFX * (static_cast<tools::Long>(*pColorSample2) - *pColorSample1);

                pColorSample1 = pLineAlpha0 + nMapX;
                pColorSample2
                    = (nMapX + 1 < pSourceAlpha->Width()) ? pColorSample1 + 1 : pColorSample1;
                nAlphaLine1
                    = (static_cast<tools::Long>(*pColorSample1) << 7)
                      + nMapFX * (static_cast<tools::Long>(*pColorSample2) - *pColorSample1);

                pColorSample1 = pLineAlpha1 + nMapX;
                pColorSample2
                    = (nMapX + 1 < pSourceAlpha->Width()) ? pColorSample1 + 1 : pColorSample1;
                nAlphaLine2
                    = (static_cast<tools::Long>(*pColorSample1) << 7)
                      + nMapFX * (static_cast<tools::Long>(*pColorSample2) - *pColorSample1);

                nColor1
                    = (nColor1Line1 + nMapFY * ((nColor1Line2 >> 7) - (nColor1Line1 >> 7))) >> 7;
                nColor2
                    = (nColor2Line1 + nMapFY * ((nColor2Line2 >> 7) - (nColor2Line1 >> 7))) >> 7;
                nColor3
                    = (nColor3Line1 + nMapFY * ((nColor3Line2 >> 7) - (nColor3Line1 >> 7))) >> 7;

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
};

struct TradScaleContext
{
    std::unique_ptr<tools::Long[]> mpMapX;
    std::unique_ptr<tools::Long[]> mpMapY;

    TradScaleContext(tools::Rectangle const& aDstRect, tools::Rectangle const& aBitmapRect,
                     Size const& aOutSize, tools::Long nOffX, tools::Long nOffY)

        : mpMapX(new tools::Long[aDstRect.GetWidth()])
        , mpMapY(new tools::Long[aDstRect.GetHeight()])
    {
        const tools::Long nSrcWidth = aBitmapRect.GetWidth();
        const tools::Long nSrcHeight = aBitmapRect.GetHeight();

        const bool bHMirr = aOutSize.Width() < 0;
        const bool bVMirr = aOutSize.Height() < 0;

        generateSimpleMap(nSrcWidth, aDstRect.GetWidth(), aBitmapRect.Left(), aOutSize.Width(),
                          nOffX, bHMirr, mpMapX.get());

        generateSimpleMap(nSrcHeight, aDstRect.GetHeight(), aBitmapRect.Top(), aOutSize.Height(),
                          nOffY, bVMirr, mpMapY.get());
    }

private:
    static void generateSimpleMap(tools::Long nSrcDimension, tools::Long nDstDimension,
                                  tools::Long nDstLocation, tools::Long nOutDimension,
                                  tools::Long nOffset, bool bMirror, tools::Long* pMap)
    {
        tools::Long nMirrorOffset = 0;

        if (bMirror)
            nMirrorOffset = (nDstLocation << 1) + nSrcDimension - 1;

        for (tools::Long i = 0; i < nDstDimension; ++i, ++nOffset)
        {
            pMap[i] = nDstLocation + nOffset * nSrcDimension / nOutDimension;
            if (bMirror)
                pMap[i] = nMirrorOffset - pMap[i];
        }
    }
};

} // end anonymous namespace

void RenderContext2::DrawDeviceAlphaBitmapSlowPath(Bitmap const& rBitmap, AlphaMask const& rAlpha,
                                                   tools::Rectangle aDstRect,
                                                   tools::Rectangle aBmpRect, Size const& aOutSize,
                                                   Point const& aOutPoint)
{
    VirtualDevice* pOldVDev = mpAlphaVDev;

    const bool bHMirr = aOutSize.Width() < 0;
    const bool bVMirr = aOutSize.Height() < 0;

    // The scaling in this code path produces really ugly results - it
    // does the most trivial scaling with no smoothing.
    const bool bOldMap = IsMapModeEnabled();
    EnableMapMode(false);

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

    assert(pAlphaReadAccess->GetScanlineFormat() == ScanlineFormat::N8BitPal
           && "RenderContext2::ImplDrawAlpha(): non-8bit alpha no longer supported!");

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

    EnableMapMode(bOldMap);
}

void RenderContext2::DrawDeviceAlphaBitmap(Bitmap const& rBmp, AlphaMask const& rAlpha,
                                           Point const& rDestPt, Size const& rDestSize,
                                           Point const& rSrcPtPixel, Size const& rSrcSizePixel)
{
    assert(!is_double_buffered_window());

    Point aOutPt(LogicToPixel(rDestPt));
    Size aOutSz(LogicToPixel(rDestSize));
    tools::Rectangle aDstRect(Point(), GetSize());

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
        Point aRelPt = aOutPt + Point(maGeometry.GetXFrameOffset(), maGeometry.GetYFrameOffset());
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
        Point auxOutPt(LogicToPixel(rDestPt));
        Size auxOutSz(LogicToPixel(rDestSize));

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
        DrawDeviceAlphaBitmapSlowPath(bitmap, alpha, aDstRect, aBmpRect, auxOutSz, auxOutPt);
    }
}

bool RenderContext2::HasFastDrawTransformedBitmap() const
{
    if (ImplIsRecordLayout())
        return false;

    if (!mpGraphics && !AcquireGraphics())
        return false;

    return mpGraphics->HasFastDrawTransformedBitmap();
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
