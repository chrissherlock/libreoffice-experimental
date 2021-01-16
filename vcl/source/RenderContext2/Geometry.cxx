/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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

#include <vcl/Geometry.hxx>
#include <vcl/mapmod.hxx>

#include <cassert>
#include <cmath>

Geometry::Geometry()
    : mbMap(false)
    , mnWidthPx(0)
    , mnHeightPx(0)
    , mnOffsetXpx(0)
    , mnOffsetYpx(0)
    , mnOffsetFromOriginXpx(0)
    , mnOffsetFromOriginYpx(0)
    , mnOffsetFromOriginXInLogicalUnits(0)
    , mnOffsetFromOriginYInLogicalUnits(0)
    , mnDPIX(0)
    , mnDPIY(0)
    , mnDPIScalePercentage(100)
{
    // #i75163#
    mpViewTransformer.reset(new ViewTransformer);
    mpViewTransformer->mpViewTransform = nullptr;
    mpViewTransformer->mpInverseViewTransform = nullptr;
}

Geometry::~Geometry()
{
    // #i75163#
    if (mpViewTransformer)
        mpViewTransformer->InvalidateViewTransform();

    mpViewTransformer.reset();
}

bool Geometry::IsMapModeEnabled() const { return mbMap; }

void Geometry::EnableMapMode() { mbMap = true; }

void Geometry::DisableMapMode() { mbMap = false; }

Size Geometry::GetSizeInPixels() const { return Size(mnWidthPx, mnHeightPx); }

void Geometry::SetSizeInPixels(Size const& rSize)
{
    mnWidthPx = rSize.Width();
    mnHeightPx = rSize.Height();
}

void Geometry::SetWidthInPixels(tools::Long nWidth) { mnWidthPx = nWidth; }
void Geometry::SetHeightInPixels(tools::Long nHeight) { mnHeightPx = nHeight; }

tools::Long Geometry::GetWidthInPixels() const { return mnWidthPx; }
tools::Long Geometry::GetHeightInPixels() const { return mnHeightPx; }

tools::Long Geometry::GetXOffsetInPixels() const { return mnOffsetXpx; }
tools::Long Geometry::GetYOffsetInPixels() const { return mnOffsetYpx; }

void Geometry::SetXOffsetInPixels(tools::Long nOffsetXpx) { mnOffsetXpx = nOffsetXpx; }

void Geometry::SetYOffsetInPixels(tools::Long nOffsetYpx) { mnOffsetYpx = nOffsetYpx; }

sal_Int32 Geometry::GetDPIX() const { return mnDPIX; }
sal_Int32 Geometry::GetDPIY() const { return mnDPIY; }
void Geometry::SetDPIX(sal_Int32 nDPIX) { mnDPIX = nDPIX; }
void Geometry::SetDPIY(sal_Int32 nDPIY) { mnDPIY = nDPIY; }
float Geometry::GetDPIScaleFactor() const { return mnDPIScalePercentage / 100.0f; }
sal_Int32 Geometry::GetDPIScalePercentage() const { return mnDPIScalePercentage; }

void Geometry::SetDPIScalePercentage(sal_Int32 nPercentage) { mnDPIScalePercentage = nPercentage; }

void Geometry::ResetLogicalUnitsOffsetFromOrigin()
{
    mnOffsetFromOriginXInLogicalUnits = mnOffsetFromOriginXpx;
    mnOffsetFromOriginYInLogicalUnits = mnOffsetFromOriginYpx;
}

Size Geometry::GetOffsetFromOriginInPixels() const
{
    return Size(mnOffsetFromOriginXpx, mnOffsetFromOriginYpx);
}

tools::Long Geometry::GetXOffsetFromOriginInPixels() const { return mnOffsetFromOriginXpx; }

tools::Long Geometry::GetYOffsetFromOriginInPixels() const { return mnOffsetFromOriginYpx; }

void Geometry::SetOffsetFromOriginInPixels(Size const& rOffset)
{
    mnOffsetFromOriginXpx = rOffset.Width();
    mnOffsetFromOriginYpx = rOffset.Height();

    SetXOffsetFromOriginInLogicalUnits(Geometry::PixelToLogic(mnOffsetFromOriginXpx, mnDPIX,
                                                              maMappingMetrics.mnMapScNumX,
                                                              maMappingMetrics.mnMapScDenomX));
    SetYOffsetFromOriginInLogicalUnits(Geometry::PixelToLogic(mnOffsetFromOriginYpx, mnDPIY,
                                                              maMappingMetrics.mnMapScNumY,
                                                              maMappingMetrics.mnMapScDenomY));
}

sal_uInt32 Geometry::GetXOffsetFromOriginInLogicalUnits() const
{
    return mnOffsetFromOriginXInLogicalUnits;
}

void Geometry::SetXOffsetFromOriginInLogicalUnits(tools::Long nOffsetFromOriginXInLogicalUnits)
{
    mnOffsetFromOriginXInLogicalUnits = nOffsetFromOriginXInLogicalUnits;
}

sal_uInt32 Geometry::GetYOffsetFromOriginInLogicalUnits() const
{
    return mnOffsetFromOriginYInLogicalUnits;
}

void Geometry::SetYOffsetFromOriginInLogicalUnits(tools::Long nOffsetFromOriginYInLogicalUnits)
{
    mnOffsetFromOriginYInLogicalUnits = nOffsetFromOriginYInLogicalUnits;
}

// #i75163#
basegfx::B2DHomMatrix Geometry::GetInverseViewTransformation() const
{
    if (mbMap && mpViewTransformer)
    {
        if (!mpViewTransformer->mpInverseViewTransform)
        {
            GetViewTransformation();
            mpViewTransformer->mpInverseViewTransform
                = new basegfx::B2DHomMatrix(*mpViewTransformer->mpViewTransform);
            mpViewTransformer->mpInverseViewTransform->invert();
        }

        return *mpViewTransformer->mpInverseViewTransform;
    }
    else
    {
        return basegfx::B2DHomMatrix();
    }
}

tools::Long Geometry::GetXMapOffset() const { return maMappingMetrics.mnMapOfsX; }
void Geometry::SetXMapOffset(tools::Long nXOffset) { maMappingMetrics.mnMapOfsX = nXOffset; }

tools::Long Geometry::GetYMapOffset() const { return maMappingMetrics.mnMapOfsY; }
void Geometry::SetYMapOffset(tools::Long nYOffset) { maMappingMetrics.mnMapOfsY = nYOffset; }

tools::Long Geometry::GetXMapNumerator() const { return maMappingMetrics.mnMapScNumX; }

void Geometry::SetXMapNumerator(tools::Long nNumerator)
{
    maMappingMetrics.mnMapScNumX = nNumerator;
}

tools::Long Geometry::GetYMapNumerator() const { return maMappingMetrics.mnMapScNumY; }

void Geometry::SetYMapNumerator(tools::Long nNumerator)
{
    maMappingMetrics.mnMapScNumY = nNumerator;
}

tools::Long Geometry::GetXMapDenominator() const { return maMappingMetrics.mnMapScDenomX; }

void Geometry::SetXMapDenominator(tools::Long nDenomerator)
{
    maMappingMetrics.mnMapScDenomX = nDenomerator;
}

tools::Long Geometry::GetYMapDenominator() const { return maMappingMetrics.mnMapScDenomY; }

void Geometry::SetYMapDenominator(tools::Long nDenomerator)
{
    maMappingMetrics.mnMapScDenomY = nDenomerator;
}

MappingMetrics Geometry::GetMappingMetrics() const { return maMappingMetrics; }

void Geometry::CalculateMappingMetrics(MapMode const& rMapMode, tools::Long nDPIX,
                                       tools::Long nDPIY)
{
    maMappingMetrics.Calculate(rMapMode, nDPIX, nDPIY);
}

// #i75163#
basegfx::B2DHomMatrix Geometry::GetViewTransformation() const
{
    if (mbMap && mpViewTransformer)
    {
        if (!mpViewTransformer->mpViewTransform)
        {
            mpViewTransformer->mpViewTransform = new basegfx::B2DHomMatrix;

            const double fScaleFactorX(static_cast<double>(mnDPIX)
                                       * static_cast<double>(maMappingMetrics.mnMapScNumX)
                                       / static_cast<double>(maMappingMetrics.mnMapScDenomX));
            const double fScaleFactorY(static_cast<double>(mnDPIY)
                                       * static_cast<double>(maMappingMetrics.mnMapScNumY)
                                       / static_cast<double>(maMappingMetrics.mnMapScDenomY));
            const double fZeroPointX(
                (static_cast<double>(maMappingMetrics.mnMapOfsX) * fScaleFactorX)
                + static_cast<double>(mnOffsetFromOriginXpx));
            const double fZeroPointY(
                (static_cast<double>(maMappingMetrics.mnMapOfsY) * fScaleFactorY)
                + static_cast<double>(mnOffsetFromOriginYpx));

            mpViewTransformer->mpViewTransform->set(0, 0, fScaleFactorX);
            mpViewTransformer->mpViewTransform->set(1, 1, fScaleFactorY);
            mpViewTransformer->mpViewTransform->set(0, 2, fZeroPointX);
            mpViewTransformer->mpViewTransform->set(1, 2, fZeroPointY);
        }

        return *mpViewTransformer->mpViewTransform;
    }
    else
    {
        return basegfx::B2DHomMatrix();
    }
}

// #i75163#
basegfx::B2DHomMatrix Geometry::GetViewTransformation(MapMode const& rMapMode) const
{
    // #i82615#
    MappingMetrics aMappingMetrics(rMapMode, mnDPIX, mnDPIY);

    basegfx::B2DHomMatrix aTransform;

    const double fScaleFactorX(static_cast<double>(mnDPIX)
                               * static_cast<double>(aMappingMetrics.mnMapScNumX)
                               / static_cast<double>(aMappingMetrics.mnMapScDenomX));
    const double fScaleFactorY(static_cast<double>(mnDPIY)
                               * static_cast<double>(aMappingMetrics.mnMapScNumY)
                               / static_cast<double>(aMappingMetrics.mnMapScDenomY));
    const double fZeroPointX((static_cast<double>(aMappingMetrics.mnMapOfsX) * fScaleFactorX)
                             + static_cast<double>(mnOffsetFromOriginXpx));
    const double fZeroPointY((static_cast<double>(aMappingMetrics.mnMapOfsY) * fScaleFactorY)
                             + static_cast<double>(mnOffsetFromOriginYpx));

    aTransform.set(0, 0, fScaleFactorX);
    aTransform.set(1, 1, fScaleFactorY);
    aTransform.set(0, 2, fZeroPointX);
    aTransform.set(1, 2, fZeroPointY);

    return aTransform;
}

// #i75163#
basegfx::B2DHomMatrix Geometry::GetInverseViewTransformation(MapMode const& rMapMode) const
{
    basegfx::B2DHomMatrix aMatrix(GetViewTransformation(rMapMode));
    aMatrix.invert();
    return aMatrix;
}

basegfx::B2DHomMatrix Geometry::GetDeviceTransformation() const
{
    basegfx::B2DHomMatrix aTransformation = GetViewTransformation();

    // TODO: is it worth to cache the transformed result?
    if (mnOffsetXpx || mnOffsetYpx)
        aTransformation.translate(mnOffsetXpx, mnOffsetYpx);

    return aTransformation;
}

void Geometry::InvalidateViewTransform()
{
    if (mpViewTransformer)
        mpViewTransformer->InvalidateViewTransform();
}

tools::Long Geometry::LogicToPixel(tools::Long n, tools::Long nDPI, tools::Long nMapNum,
                                   tools::Long nMapDenom)
{
    assert(nDPI > 0);
    assert(nMapDenom != 0);
    if constexpr (sizeof(tools::Long) >= 8)
    {
        assert(nMapNum >= 0);
        //detect overflows
        assert(nMapNum == 0
               || std::abs(n) < std::numeric_limits<tools::Long>::max() / nMapNum / nDPI);
    }
    sal_Int64 n64 = n;
    n64 *= nMapNum;
    n64 *= nDPI;
    if (nMapDenom == 1)
        n = static_cast<tools::Long>(n64);
    else
    {
        n64 = 2 * n64 / nMapDenom;
        if (n64 < 0)
            --n64;
        else
            ++n64;
        n = static_cast<tools::Long>(n64 / 2);
    }
    return n;
}

tools::Long Geometry::PixelToLogic(tools::Long n, tools::Long nDPI, tools::Long nMapNum,
                                   tools::Long nMapDenom)
{
    assert(nDPI > 0);
    if (nMapNum == 0)
        return 0;
    sal_Int64 nDenom = nDPI;
    nDenom *= nMapNum;

    sal_Int64 n64 = n;
    n64 *= nMapDenom;
    if (nDenom == 1)
        n = static_cast<tools::Long>(n64);
    else
    {
        n64 = 2 * n64 / nDenom;
        if (n64 < 0)
            --n64;
        else
            ++n64;
        n = static_cast<tools::Long>(n64 / 2);
    }
    return n;
}

tools::Long Geometry::LogicXToDevicePixel(tools::Long nX) const
{
    if (!mbMap)
        return nX + mnOffsetXpx;

    return LogicToPixel(nX + maMappingMetrics.mnMapOfsX, mnDPIX, maMappingMetrics.mnMapScNumX,
                        maMappingMetrics.mnMapScDenomX)
           + mnOffsetXpx + mnOffsetFromOriginXpx;
}

tools::Long Geometry::LogicYToDevicePixel(tools::Long nY) const
{
    if (!mbMap)
        return nY + mnOffsetYpx;

    return LogicToPixel(nY + maMappingMetrics.mnMapOfsY, mnDPIY, maMappingMetrics.mnMapScNumY,
                        maMappingMetrics.mnMapScDenomY)
           + mnOffsetYpx + mnOffsetFromOriginYpx;
}

tools::Long Geometry::LogicWidthToDevicePixel(tools::Long nWidth) const
{
    if (!mbMap)
        return nWidth;

    return LogicToPixel(nWidth, mnDPIX, maMappingMetrics.mnMapScNumX,
                        maMappingMetrics.mnMapScDenomX);
}

tools::Long Geometry::LogicHeightToDevicePixel(tools::Long nHeight) const
{
    if (!mbMap)
        return nHeight;

    return LogicToPixel(nHeight, mnDPIY, maMappingMetrics.mnMapScNumY,
                        maMappingMetrics.mnMapScDenomY);
}

float Geometry::FloatLogicHeightToDevicePixel(float fLogicHeight) const
{
    if (!mbMap)
        return fLogicHeight;
    float fPixelHeight
        = (fLogicHeight * mnDPIY * maMappingMetrics.mnMapScNumY) / maMappingMetrics.mnMapScDenomY;
    return fPixelHeight;
}

tools::Long Geometry::DevicePixelToLogicWidth(tools::Long nWidth) const
{
    if (!mbMap)
        return nWidth;

    return PixelToLogic(nWidth, mnDPIX, maMappingMetrics.mnMapScNumX,
                        maMappingMetrics.mnMapScDenomX);
}

tools::Long Geometry::DevicePixelToLogicHeight(tools::Long nHeight) const
{
    if (!mbMap)
        return nHeight;

    return PixelToLogic(nHeight, mnDPIY, maMappingMetrics.mnMapScNumY,
                        maMappingMetrics.mnMapScDenomY);
}

Point Geometry::LogicToDevicePixel(const Point& rLogicPt) const
{
    if (!mbMap)
        return Point(rLogicPt.X() + mnOffsetXpx, rLogicPt.Y() + mnOffsetYpx);

    return Point(LogicToPixel(rLogicPt.X() + maMappingMetrics.mnMapOfsX, mnDPIX,
                              maMappingMetrics.mnMapScNumX, maMappingMetrics.mnMapScDenomX)
                     + mnOffsetXpx + mnOffsetFromOriginXpx,
                 LogicToPixel(rLogicPt.Y() + maMappingMetrics.mnMapOfsY, mnDPIY,
                              maMappingMetrics.mnMapScNumY, maMappingMetrics.mnMapScDenomY)
                     + mnOffsetYpx + mnOffsetFromOriginYpx);
}

Size Geometry::LogicToDevicePixel(const Size& rLogicSize) const
{
    if (!mbMap)
        return rLogicSize;

    return Size(LogicToPixel(rLogicSize.Width(), mnDPIX, maMappingMetrics.mnMapScNumX,
                             maMappingMetrics.mnMapScDenomX),
                LogicToPixel(rLogicSize.Height(), mnDPIY, maMappingMetrics.mnMapScNumY,
                             maMappingMetrics.mnMapScDenomY));
}

tools::Rectangle Geometry::LogicToDevicePixel(const tools::Rectangle& rLogicRect) const
{
    if (rLogicRect.IsEmpty())
        return rLogicRect;

    if (!mbMap)
    {
        return tools::Rectangle(rLogicRect.Left() + mnOffsetXpx, rLogicRect.Top() + mnOffsetYpx,
                                rLogicRect.Right() + mnOffsetXpx,
                                rLogicRect.Bottom() + mnOffsetYpx);
    }

    return tools::Rectangle(
        LogicToPixel(rLogicRect.Left() + maMappingMetrics.mnMapOfsX, mnDPIX,
                     maMappingMetrics.mnMapScNumX, maMappingMetrics.mnMapScDenomX)
            + mnOffsetXpx + mnOffsetFromOriginXpx,
        LogicToPixel(rLogicRect.Top() + maMappingMetrics.mnMapOfsY, mnDPIY,
                     maMappingMetrics.mnMapScNumY, maMappingMetrics.mnMapScDenomY)
            + mnOffsetYpx + mnOffsetFromOriginYpx,
        LogicToPixel(rLogicRect.Right() + maMappingMetrics.mnMapOfsX, mnDPIX,
                     maMappingMetrics.mnMapScNumX, maMappingMetrics.mnMapScDenomX)
            + mnOffsetXpx + mnOffsetFromOriginXpx,
        LogicToPixel(rLogicRect.Bottom() + maMappingMetrics.mnMapOfsY, mnDPIY,
                     maMappingMetrics.mnMapScNumY, maMappingMetrics.mnMapScDenomY)
            + mnOffsetYpx + mnOffsetFromOriginYpx);
}

tools::Polygon Geometry::LogicToDevicePixel(const tools::Polygon& rLogicPoly) const
{
    if (!mbMap && !mnOffsetXpx && !mnOffsetYpx)
        return rLogicPoly;

    sal_uInt16 i;
    sal_uInt16 nPoints = rLogicPoly.GetSize();
    tools::Polygon aPoly(rLogicPoly);

    // get pointer to Point-array (copy data)
    const Point* pPointAry = aPoly.GetConstPointAry();

    if (mbMap)
    {
        for (i = 0; i < nPoints; i++)
        {
            const Point& rPt = pPointAry[i];
            Point aPt(LogicToPixel(rPt.X() + maMappingMetrics.mnMapOfsX, mnDPIX,
                                   maMappingMetrics.mnMapScNumX, maMappingMetrics.mnMapScDenomX)
                          + mnOffsetXpx + mnOffsetFromOriginXpx,
                      LogicToPixel(rPt.Y() + maMappingMetrics.mnMapOfsY, mnDPIY,
                                   maMappingMetrics.mnMapScNumY, maMappingMetrics.mnMapScDenomY)
                          + mnOffsetYpx + mnOffsetFromOriginYpx);
            aPoly[i] = aPt;
        }
    }
    else
    {
        for (i = 0; i < nPoints; i++)
        {
            Point aPt = pPointAry[i];
            aPt.AdjustX(mnOffsetXpx);
            aPt.AdjustY(mnOffsetYpx);
            aPoly[i] = aPt;
        }
    }

    return aPoly;
}

tools::PolyPolygon Geometry::LogicToDevicePixel(const tools::PolyPolygon& rLogicPolyPoly) const
{
    if (!mbMap && !mnOffsetXpx && !mnOffsetYpx)
        return rLogicPolyPoly;

    tools::PolyPolygon aPolyPoly(rLogicPolyPoly);
    sal_uInt16 nPoly = aPolyPoly.Count();
    for (sal_uInt16 i = 0; i < nPoly; i++)
    {
        tools::Polygon& rPoly = aPolyPoly[i];
        rPoly = LogicToDevicePixel(rPoly);
    }
    return aPolyPoly;
}

LineInfo Geometry::LogicToDevicePixel(const LineInfo& rLineInfo) const
{
    LineInfo aInfo(rLineInfo);

    if (aInfo.GetStyle() == LineStyle::Dash)
    {
        if (aInfo.GetDotCount() && aInfo.GetDotLen())
            aInfo.SetDotLen(std::max(LogicWidthToDevicePixel(aInfo.GetDotLen()), tools::Long(1)));
        else
            aInfo.SetDotCount(0);

        if (aInfo.GetDashCount() && aInfo.GetDashLen())
            aInfo.SetDashLen(std::max(LogicWidthToDevicePixel(aInfo.GetDashLen()), tools::Long(1)));
        else
            aInfo.SetDashCount(0);

        aInfo.SetDistance(LogicWidthToDevicePixel(aInfo.GetDistance()));

        if ((!aInfo.GetDashCount() && !aInfo.GetDotCount()) || !aInfo.GetDistance())
            aInfo.SetStyle(LineStyle::Solid);
    }

    aInfo.SetWidth(LogicWidthToDevicePixel(aInfo.GetWidth()));

    return aInfo;
}

tools::Rectangle Geometry::DevicePixelToLogic(const tools::Rectangle& rPixelRect) const
{
    if (rPixelRect.IsEmpty())
        return rPixelRect;

    if (!mbMap)
    {
        return tools::Rectangle(rPixelRect.Left() - mnOffsetXpx, rPixelRect.Top() - mnOffsetYpx,
                                rPixelRect.Right() - mnOffsetXpx,
                                rPixelRect.Bottom() - mnOffsetYpx);
    }

    return tools::Rectangle(
        PixelToLogic(rPixelRect.Left() - mnOffsetXpx - mnOffsetFromOriginXpx, mnDPIX,
                     maMappingMetrics.mnMapScNumX, maMappingMetrics.mnMapScDenomX)
            - maMappingMetrics.mnMapOfsX,
        PixelToLogic(rPixelRect.Top() - mnOffsetYpx - mnOffsetFromOriginYpx, mnDPIY,
                     maMappingMetrics.mnMapScNumY, maMappingMetrics.mnMapScDenomY)
            - maMappingMetrics.mnMapOfsY,
        PixelToLogic(rPixelRect.Right() - mnOffsetXpx - mnOffsetFromOriginXpx, mnDPIX,
                     maMappingMetrics.mnMapScNumX, maMappingMetrics.mnMapScDenomX)
            - maMappingMetrics.mnMapOfsX,
        PixelToLogic(rPixelRect.Bottom() - mnOffsetYpx - mnOffsetFromOriginYpx, mnDPIY,
                     maMappingMetrics.mnMapScNumY, maMappingMetrics.mnMapScDenomY)
            - maMappingMetrics.mnMapOfsY);
}

vcl::Region Geometry::PixelToDevicePixel(const vcl::Region& rRegion) const
{
    if (!mnOffsetXpx && !mnOffsetYpx)
        return rRegion;

    vcl::Region aRegion(rRegion);
    aRegion.Move(mnOffsetXpx + mnOffsetFromOriginXpx, mnOffsetYpx + mnOffsetFromOriginYpx);
    return aRegion;
}

DeviceCoordinate Geometry::LogicWidthToDeviceCoordinate(tools::Long nWidth) const
{
    if (!mbMap)
        return static_cast<DeviceCoordinate>(nWidth);

#if VCL_FLOAT_DEVICE_PIXEL
    return (double)nWidth * mmaMappingMetric.mfScaleX * mnDPIX;
#else

    return LogicToPixel(nWidth, mnDPIX, maMappingMetrics.mnMapScNumY,
                        maMappingMetrics.mnMapScDenomY);
#endif
}

Point Geometry::LogicToPixel(const Point& rLogicPt) const
{
    if (!mbMap)
        return rLogicPt;

    return Point(LogicToPixel(rLogicPt.X() + maMappingMetrics.mnMapOfsX, mnDPIX,
                              maMappingMetrics.mnMapScNumX, maMappingMetrics.mnMapScDenomX)
                     + mnOffsetFromOriginXpx,
                 LogicToPixel(rLogicPt.Y() + maMappingMetrics.mnMapOfsY, mnDPIY,
                              maMappingMetrics.mnMapScNumY, maMappingMetrics.mnMapScDenomY)
                     + mnOffsetFromOriginYpx);
}

Size Geometry::LogicToPixel(Size const& rLogicSize) const
{
    if (!mbMap)
        return rLogicSize;

    return Size(LogicToPixel(rLogicSize.Width(), mnDPIX, maMappingMetrics.mnMapScNumX,
                             maMappingMetrics.mnMapScDenomX),
                LogicToPixel(rLogicSize.Height(), mnDPIY, maMappingMetrics.mnMapScNumY,
                             maMappingMetrics.mnMapScDenomY));
}

vcl::Region Geometry::LogicToPixel(vcl::Region const& rLogicRegion) const
{
    if (!mbMap || rLogicRegion.IsNull() || rLogicRegion.IsEmpty())
        return rLogicRegion;

    vcl::Region aRegion;

    if (rLogicRegion.getB2DPolyPolygon())
    {
        aRegion = vcl::Region(LogicToPixel(*rLogicRegion.getB2DPolyPolygon()));
    }
    else if (rLogicRegion.getPolyPolygon())
    {
        aRegion = vcl::Region(LogicToPixel(*rLogicRegion.getPolyPolygon()));
    }
    else if (rLogicRegion.getRegionBand())
    {
        std::vector<tools::Rectangle> aRectangles;
        rLogicRegion.GetRegionRectangles(aRectangles);
        const std::vector<tools::Rectangle>& rRectangles(
            aRectangles); // needed to make the '!=' work

        // make reverse run to fill new region bottom-up, this will speed it up due to the used data structuring
        for (std::vector<tools::Rectangle>::const_reverse_iterator aRectIter(rRectangles.rbegin());
             aRectIter != rRectangles.rend(); ++aRectIter)
        {
            aRegion.Union(LogicToPixel(*aRectIter));
        }
    }

    return aRegion;
}

tools::Rectangle Geometry::LogicToPixel(tools::Rectangle const& rLogicRect) const
{
    if (!mbMap || rLogicRect.IsEmpty())
        return rLogicRect;

    return tools::Rectangle(
        LogicToPixel(rLogicRect.Left() + maMappingMetrics.mnMapOfsX, mnDPIX,
                     maMappingMetrics.mnMapScNumX, maMappingMetrics.mnMapScDenomX)
            + mnOffsetFromOriginXpx,
        LogicToPixel(rLogicRect.Top() + maMappingMetrics.mnMapOfsY, mnDPIY,
                     maMappingMetrics.mnMapScNumY, maMappingMetrics.mnMapScDenomY)
            + mnOffsetFromOriginYpx,
        LogicToPixel(rLogicRect.Right() + maMappingMetrics.mnMapOfsX, mnDPIX,
                     maMappingMetrics.mnMapScNumX, maMappingMetrics.mnMapScDenomX)
            + mnOffsetFromOriginXpx,
        LogicToPixel(rLogicRect.Bottom() + maMappingMetrics.mnMapOfsY, mnDPIY,
                     maMappingMetrics.mnMapScNumY, maMappingMetrics.mnMapScDenomY)
            + mnOffsetFromOriginYpx);
}

tools::Polygon Geometry::LogicToPixel(tools::Polygon const& rLogicPoly) const
{
    if (!mbMap)
        return rLogicPoly;

    sal_uInt16 i;
    sal_uInt16 nPoints = rLogicPoly.GetSize();
    tools::Polygon aPoly(rLogicPoly);

    // get pointer to Point-array (copy data)
    const Point* pPointAry = aPoly.GetConstPointAry();

    for (i = 0; i < nPoints; i++)
    {
        const Point* pPt = &(pPointAry[i]);
        Point aPt;
        aPt.setX(Geometry::LogicToPixel(pPt->X() + maMappingMetrics.mnMapOfsX, mnDPIX,
                                        maMappingMetrics.mnMapScNumX,
                                        maMappingMetrics.mnMapScDenomX)
                 + mnOffsetFromOriginXpx);
        aPt.setY(Geometry::LogicToPixel(pPt->Y() + maMappingMetrics.mnMapOfsY, mnDPIY,
                                        maMappingMetrics.mnMapScNumY,
                                        maMappingMetrics.mnMapScDenomY)
                 + mnOffsetFromOriginYpx);
        aPoly[i] = aPt;
    }

    return aPoly;
}

tools::PolyPolygon Geometry::LogicToPixel(tools::PolyPolygon const& rLogicPolyPoly) const
{
    if (!mbMap)
        return rLogicPolyPoly;

    tools::PolyPolygon aPolyPoly(rLogicPolyPoly);
    sal_uInt16 nPoly = aPolyPoly.Count();
    for (sal_uInt16 i = 0; i < nPoly; i++)
    {
        tools::Polygon& rPoly = aPolyPoly[i];
        rPoly = LogicToPixel(rPoly);
    }
    return aPolyPoly;
}

basegfx::B2DPolyPolygon Geometry::LogicToPixel(basegfx::B2DPolyPolygon const& rLogicPolyPoly) const
{
    basegfx::B2DPolyPolygon aTransformedPoly = rLogicPolyPoly;
    const basegfx::B2DHomMatrix& rTransformationMatrix = GetViewTransformation();
    aTransformedPoly.transform(rTransformationMatrix);

    return aTransformedPoly;
}

Point Geometry::LogicToPixel(Point const& rLogicPt, MapMode const& rMapMode) const
{
    if (rMapMode.IsDefault())
        return rLogicPt;

    // convert MapMode resolution and convert
    MappingMetrics aMappingMetric(rMapMode, GetDPIX(), GetDPIY());

    return Point(LogicToPixel(rLogicPt.X() + aMappingMetric.mnMapOfsX, GetDPIX(),
                              aMappingMetric.mnMapScNumX, aMappingMetric.mnMapScDenomX)
                     + GetXOffsetFromOriginInPixels(),
                 LogicToPixel(rLogicPt.Y() + aMappingMetric.mnMapOfsY, GetDPIY(),
                              aMappingMetric.mnMapScNumY, aMappingMetric.mnMapScDenomY)
                     + GetYOffsetFromOriginInPixels());
}

Size Geometry::LogicToPixel(Size const& rLogicSize, MapMode const& rMapMode) const
{
    if (rMapMode.IsDefault())
        return rLogicSize;

    // convert MapMode resolution and convert
    MappingMetrics aMappingMetric(rMapMode, GetDPIX(), GetDPIY());

    return Size(LogicToPixel(rLogicSize.Width(), GetDPIX(), aMappingMetric.mnMapScNumX,
                             aMappingMetric.mnMapScDenomX),
                LogicToPixel(rLogicSize.Height(), GetDPIY(), aMappingMetric.mnMapScNumY,
                             aMappingMetric.mnMapScDenomY));
}

tools::Rectangle Geometry::LogicToPixel(tools::Rectangle const& rLogicRect,
                                        MapMode const& rMapMode) const
{
    if (rMapMode.IsDefault() || rLogicRect.IsEmpty())
        return rLogicRect;

    // convert MapMode resolution and convert
    MappingMetrics aMappingMetric(rMapMode, GetDPIX(), GetDPIY());

    return tools::Rectangle(LogicToPixel(rLogicRect.Left() + aMappingMetric.mnMapOfsX, GetDPIX(),
                                         aMappingMetric.mnMapScNumX, aMappingMetric.mnMapScDenomX)
                                + GetXOffsetFromOriginInPixels(),
                            LogicToPixel(rLogicRect.Top() + aMappingMetric.mnMapOfsY, GetDPIY(),
                                         aMappingMetric.mnMapScNumY, aMappingMetric.mnMapScDenomY)
                                + GetYOffsetFromOriginInPixels(),
                            LogicToPixel(rLogicRect.Right() + aMappingMetric.mnMapOfsX, GetDPIX(),
                                         aMappingMetric.mnMapScNumX, aMappingMetric.mnMapScDenomX)
                                + GetXOffsetFromOriginInPixels(),
                            LogicToPixel(rLogicRect.Bottom() + aMappingMetric.mnMapOfsY, GetDPIY(),
                                         aMappingMetric.mnMapScNumY, aMappingMetric.mnMapScDenomY)
                                + GetYOffsetFromOriginInPixels());
}

tools::Polygon Geometry::LogicToPixel(tools::Polygon const& rLogicPoly,
                                      MapMode const& rMapMode) const
{
    if (rMapMode.IsDefault())
        return rLogicPoly;

    // convert MapMode resolution and convert
    MappingMetrics aMappingMetric(rMapMode, GetDPIX(), GetDPIY());

    sal_uInt16 nPoints = rLogicPoly.GetSize();
    tools::Polygon aPoly(rLogicPoly);

    // get pointer to Point-array (copy data)
    const Point* pPointAry = aPoly.GetConstPointAry();

    for (sal_uInt16 i = 0; i < nPoints; i++)
    {
        const Point* pPt = &(pPointAry[i]);
        Point aPt;
        aPt.setX(LogicToPixel(pPt->X() + aMappingMetric.mnMapOfsX, GetDPIX(),
                              aMappingMetric.mnMapScNumX, aMappingMetric.mnMapScDenomX)
                 + GetXOffsetFromOriginInPixels());
        aPt.setY(LogicToPixel(pPt->Y() + aMappingMetric.mnMapOfsY, GetDPIY(),
                              aMappingMetric.mnMapScNumY, aMappingMetric.mnMapScDenomY)
                 + GetYOffsetFromOriginInPixels());
        aPoly[i] = aPt;
    }

    return aPoly;
}

basegfx::B2DPolyPolygon Geometry::LogicToPixel(basegfx::B2DPolyPolygon const& rLogicPolyPoly,
                                               MapMode const& rMapMode) const
{
    basegfx::B2DPolyPolygon aTransformedPoly = rLogicPolyPoly;
    const basegfx::B2DHomMatrix& rTransformationMatrix = GetViewTransformation(rMapMode);
    aTransformedPoly.transform(rTransformationMatrix);
    return aTransformedPoly;
}

Point Geometry::PixelToLogic(Point const& rDevicePt) const
{
    if (!IsMapModeEnabled())
        return rDevicePt;

    return Point(PixelToLogic(rDevicePt.X(), GetDPIX(), GetXMapNumerator(), GetXMapDenominator())
                     - GetXMapOffset() - GetXOffsetFromOriginInLogicalUnits(),
                 PixelToLogic(rDevicePt.Y(), GetDPIY(), GetYMapNumerator(), GetYMapDenominator())
                     - GetYMapOffset() - GetYOffsetFromOriginInLogicalUnits());
}

Size Geometry::PixelToLogic(Size const& rDeviceSize) const
{
    if (!IsMapModeEnabled())
        return rDeviceSize;

    return Size(
        PixelToLogic(rDeviceSize.Width(), GetDPIX(), GetXMapNumerator(), GetXMapDenominator()),
        PixelToLogic(rDeviceSize.Height(), GetDPIY(), GetYMapNumerator(), GetYMapDenominator()));
}

tools::Rectangle Geometry::PixelToLogic(tools::Rectangle const& rDeviceRect) const
{
    if (!IsMapModeEnabled() || rDeviceRect.IsEmpty())
        return rDeviceRect;

    return tools::Rectangle(
        PixelToLogic(rDeviceRect.Left(), GetDPIX(), GetXMapNumerator(), GetXMapDenominator())
            - GetXMapOffset() - GetXOffsetFromOriginInLogicalUnits(),
        PixelToLogic(rDeviceRect.Top(), GetDPIY(), GetYMapNumerator(), GetYMapDenominator())
            - GetYMapOffset() - GetYOffsetFromOriginInLogicalUnits(),
        PixelToLogic(rDeviceRect.Right(), GetDPIX(), GetXMapNumerator(), GetXMapDenominator())
            - GetXMapOffset() - GetXOffsetFromOriginInLogicalUnits(),
        PixelToLogic(rDeviceRect.Bottom(), GetDPIY(), GetYMapNumerator(), GetYMapDenominator())
            - GetYMapOffset() - GetYOffsetFromOriginInLogicalUnits());
}

tools::Polygon Geometry::PixelToLogic(tools::Polygon const& rDevicePoly) const
{
    if (!IsMapModeEnabled())
        return rDevicePoly;

    sal_uInt16 i;
    sal_uInt16 nPoints = rDevicePoly.GetSize();
    tools::Polygon aPoly(rDevicePoly);

    // get pointer to Point-array (copy data)
    const Point* pPointAry = aPoly.GetConstPointAry();

    for (i = 0; i < nPoints; i++)
    {
        const Point* pPt = &(pPointAry[i]);
        Point aPt;
        aPt.setX(PixelToLogic(pPt->X(), GetDPIX(), GetXMapNumerator(), GetXMapDenominator())
                 - GetXMapOffset() - GetXOffsetFromOriginInLogicalUnits());
        aPt.setY(PixelToLogic(pPt->Y(), GetDPIY(), GetYMapNumerator(), GetYMapDenominator())
                 - GetYMapOffset() - GetYOffsetFromOriginInLogicalUnits());
        aPoly[i] = aPt;
    }

    return aPoly;
}

tools::PolyPolygon Geometry::PixelToLogic(tools::PolyPolygon const& rDevicePolyPoly) const
{
    if (!IsMapModeEnabled())
        return rDevicePolyPoly;

    tools::PolyPolygon aPolyPoly(rDevicePolyPoly);
    sal_uInt16 nPoly = aPolyPoly.Count();

    for (sal_uInt16 i = 0; i < nPoly; i++)
    {
        tools::Polygon& rPoly = aPolyPoly[i];
        rPoly = PixelToLogic(rPoly);
    }

    return aPolyPoly;
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
