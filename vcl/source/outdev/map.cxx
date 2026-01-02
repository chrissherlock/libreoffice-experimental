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

#include <sal/config.h>

#include <sal/log.hxx>
#include <osl/diagnose.h>
#include <tools/bigint.hxx>
#include <tools/debug.hxx>
#include <tools/mapunit.hxx>

#include <vcl/cursor.hxx>
#include <vcl/lineinfo.hxx>
#include <vcl/metaact.hxx>
#include <vcl/virdev.hxx>
#include <vcl/wrkwin.hxx>

#include <CoordinateMapper.hxx>
#include <ImplOutDevData.hxx>
#include <svdata.hxx>
#include <window.h>

#include <basegfx/matrix/b2dhommatrix.hxx>
#include <tools/UnitConversion.hxx>

sal_Int32 OutputDevice::GetDPIX() const { return mpMapper->GetDPIX(); }

sal_Int32 OutputDevice::GetDPIY() const { return mpMapper->GetDPIY(); }

void OutputDevice::SetDPIX(sal_Int32 nDPIX) { mpMapper->SetDPIX(nDPIX); }

void OutputDevice::SetDPIY(sal_Int32 nDPIY) { mpMapper->SetDPIY(nDPIY); }

float OutputDevice::GetDPIScaleFactor() const { return mpMapper->GetDPIScaleFactor(); }

sal_Int32 OutputDevice::GetDPIScalePercentage() const { return mpMapper->GetDPIScalePercentage(); }

void OutputDevice::SetDPIScalePercentage(float nPercent) { mpMapper->SetDPIScalePercentage(nPercent); }

tools::Long OutputDevice::GetOutputWidthPixel() const { return mpMapper->GetOutputWidthPixel(); }

tools::Long OutputDevice::GetOutputHeightPixel() const { return mpMapper->GetOutputHeightPixel(); }

void OutputDevice::SetOutputWidthPixel(tools::Long nWidth) { mpMapper->SetOutputWidthPixel(nWidth); }

void OutputDevice::SetOutputHeightPixel(tools::Long nHeight) { mpMapper->SetOutputHeightPixel(nHeight); }

Size OutputDevice::GetOutputSizePixel() const { return Size(GetOutputWidthPixel(), GetOutputHeightPixel()); }

tools::Long OutputDevice::GetOutOffXPixel() const { return mpMapper->GetOutOffXPixel(); }

tools::Long OutputDevice::GetOutOffYPixel() const { return mpMapper->GetOutOffYPixel(); }

void OutputDevice::SetOutOffXPixel(tools::Long nOutOffX) { return mpMapper->SetOutOffXPixel(nOutOffX); }

void OutputDevice::SetOutOffYPixel(tools::Long nOutOffY) { return mpMapper->SetOutOffYPixel(nOutOffY); }

Point OutputDevice::GetOutputOffPixel() const { return mpMapper->GetOutputOffPixel(); }

void OutputDevice::EnableMapMode(bool bEnable) { ImplEnableMapMode(bEnable); }

void OutputDevice::SetMapMode()
{
    if (mpMetaFile)
        mpMetaFile->AddAction(new MetaMapModeAction(MapMode()));

    ImplSetMapMode();
}

void OutputDevice::SetMapMode(const MapMode& rNewMapMode)
{
    if (mpMetaFile)
        mpMetaFile->AddAction(new MetaMapModeAction(rNewMapMode));

    ImplSetMapMode(rNewMapMode);
}

void OutputDevice::SetMetafileMapMode(const MapMode& rNewMapMode, bool bIsRecord)
{
    if (bIsRecord)
        SetRelativeMapMode(rNewMapMode);
    else
        SetMapMode(rNewMapMode);
}

void OutputDevice::SetRelativeMapMode(const MapMode& rNewMapMode)
{
    ImplSetRelativeMapMode(rNewMapMode);
}

// #i75163#
basegfx::B2DHomMatrix OutputDevice::GetViewTransformation() const
{
    return ImplGetViewTransformation();
}

// #i75163#
basegfx::B2DHomMatrix OutputDevice::GetInverseViewTransformation() const
{
    return ImplGetInverseViewTransformation();
}

// #i75163#
basegfx::B2DHomMatrix OutputDevice::GetViewTransformation(const MapMode& rMapMode) const
{
    return ImplGetViewTransformation(rMapMode);
}

// #i75163#
basegfx::B2DHomMatrix OutputDevice::GetInverseViewTransformation(const MapMode& rMapMode) const
{
    return ImplGetInverseViewTransformation(rMapMode);
}

tools::Long OutputDevice::LogicHeightToDevicePixel(tools::Long nHeight) const
{
    return ImplLogicHeightToDevicePixel(nHeight);
}

double OutputDevice::LogicHeightToDeviceSubPixel(tools::Long nHeight) const
{
    return ImplLogicHeightToDeviceSubPixel(nHeight);
}

Point OutputDevice::SubPixelToLogic(const basegfx::B2DPoint& rDevicePt) const
{
    return ImplSubPixelToLogic(rDevicePt);
}

void OutputDevice::SetPixelOffset(const Size& rOffset) { ImplSetPixelOffset(rOffset); }

double OutputDevice::LogicWidthToDeviceSubPixel(tools::Long nWidth) const
{
    return ImplLogicWidthToDeviceSubPixel(nWidth);
}

basegfx::B2DPoint OutputDevice::LogicToDeviceSubPixel(const Point& rPoint) const
{
    return ImplLogicToDeviceSubPixel(rPoint);
}

tools::Rectangle OutputDevice::LogicToLogic(const tools::Rectangle& rRectSource,
                                            const MapMode* pMapModeSource,
                                            const MapMode* pMapModeDest) const
{
    return ImplLogicToLogic(rRectSource, pMapModeSource, pMapModeDest);
}

Size OutputDevice::LogicToLogic(const Size& rSzSource, const MapMode* pMapModeSource,
                                const MapMode* pMapModeDest) const
{
    return ImplLogicToLogic(rSzSource, pMapModeSource, pMapModeDest);
}

Point OutputDevice::LogicToLogic(const Point& rPtSource, const MapMode* pMapModeSource,
                                 const MapMode* pMapModeDest) const
{
    return ImplLogicToLogic(rPtSource, pMapModeSource, pMapModeDest);
}

Point OutputDevice::LogicToPixel(const Point& rLogicPt) const { return ImplLogicToPixel(rLogicPt); }

Size OutputDevice::LogicToPixel(const Size& rLogicSize) const
{
    return ImplLogicToPixel(rLogicSize);
}

tools::Rectangle OutputDevice::LogicToPixel(const tools::Rectangle& rLogicRect) const
{
    return ImplLogicToPixel(rLogicRect);
}

tools::Polygon OutputDevice::LogicToPixel(const tools::Polygon& rLogicPoly) const
{
    return ImplLogicToPixel(rLogicPoly);
}

tools::PolyPolygon OutputDevice::LogicToPixel(const tools::PolyPolygon& rLogicPolyPoly) const
{
    return ImplLogicToPixel(rLogicPolyPoly);
}

basegfx::B2DPolyPolygon
OutputDevice::LogicToPixel(const basegfx::B2DPolyPolygon& rLogicPolyPoly) const
{
    return ImplLogicToPixel(rLogicPolyPoly);
}

vcl::Region OutputDevice::LogicToPixel(const vcl::Region& rLogicRegion) const
{
    return ImplLogicToPixel(rLogicRegion);
}

Point OutputDevice::LogicToPixel(const Point& rLogicPt, const MapMode& rMapMode) const
{
    return ImplLogicToPixel(rLogicPt, rMapMode);
}

Size OutputDevice::LogicToPixel(const Size& rLogicSize, const MapMode& rMapMode) const
{
    return ImplLogicToPixel(rLogicSize, rMapMode);
}

tools::Rectangle OutputDevice::LogicToPixel(const tools::Rectangle& rLogicRect,
                                            const MapMode& rMapMode) const
{
    return ImplLogicToPixel(rLogicRect, rMapMode);
}

tools::Polygon OutputDevice::LogicToPixel(const tools::Polygon& rLogicPoly,
                                          const MapMode& rMapMode) const
{
    return ImplLogicToPixel(rLogicPoly, rMapMode);
}

basegfx::B2DPolyPolygon OutputDevice::LogicToPixel(const basegfx::B2DPolyPolygon& rLogicPolyPoly,
                                                   const MapMode& rMapMode) const
{
    return ImplLogicToPixel(rLogicPolyPoly, rMapMode);
}

Point OutputDevice::PixelToLogic(const Point& rDevicePt) const
{
    return ImplPixelToLogic(rDevicePt);
}

Size OutputDevice::PixelToLogic(const Size& rDeviceSize) const
{
    return ImplPixelToLogic(rDeviceSize);
}

tools::Rectangle OutputDevice::PixelToLogic(const tools::Rectangle& rDeviceRect) const
{
    return ImplPixelToLogic(rDeviceRect);
}

tools::Polygon OutputDevice::PixelToLogic(const tools::Polygon& rDevicePoly) const
{
    return ImplPixelToLogic(rDevicePoly);
}

tools::PolyPolygon OutputDevice::PixelToLogic(const tools::PolyPolygon& rDevicePolyPoly) const
{
    return ImplPixelToLogic(rDevicePolyPoly);
}

basegfx::B2DPolyPolygon
OutputDevice::PixelToLogic(const basegfx::B2DPolyPolygon& rPixelPolyPoly) const
{
    return ImplPixelToLogic(rPixelPolyPoly);
}

basegfx::B2DRectangle OutputDevice::PixelToLogic(const basegfx::B2DRectangle& rDeviceRect) const
{
    return ImplPixelToLogic(rDeviceRect);
}

vcl::Region OutputDevice::PixelToLogic(const vcl::Region& rDeviceRegion) const
{
    return ImplPixelToLogic(rDeviceRegion);
}

Point OutputDevice::PixelToLogic(const Point& rDevicePt, const MapMode& rMapMode) const
{
    return ImplPixelToLogic(rDevicePt, rMapMode);
}

Size OutputDevice::PixelToLogic(const Size& rDeviceSize, const MapMode& rMapMode) const
{
    return ImplPixelToLogic(rDeviceSize, rMapMode);
}

tools::Rectangle OutputDevice::PixelToLogic(const tools::Rectangle& rDeviceRect,
                                            const MapMode& rMapMode) const
{
    return ImplPixelToLogic(rDeviceRect, rMapMode);
}

tools::Polygon OutputDevice::PixelToLogic(const tools::Polygon& rDevicePoly,
                                          const MapMode& rMapMode) const
{
    return ImplPixelToLogic(rDevicePoly, rMapMode);
}

basegfx::B2DPolygon OutputDevice::PixelToLogic(const basegfx::B2DPolygon& rPixelPoly,
                                               const MapMode& rMapMode) const
{
    return ImplPixelToLogic(rPixelPoly, rMapMode);
}

basegfx::B2DPolyPolygon OutputDevice::PixelToLogic(const basegfx::B2DPolyPolygon& rPixelPolyPoly,
                                                   const MapMode& rMapMode) const
{
    return ImplPixelToLogic(rPixelPolyPoly, rMapMode);
}

tools::Long OutputDevice::LogicWidthToDevicePixel(tools::Long nWidth) const
{
    return ImplLogicWidthToDevicePixel(nWidth);
}

Point OutputDevice::LogicToDevicePixel(const Point& rLogicPt) const
{
    return ImplLogicToDevicePixel(rLogicPt);
}

tools::Rectangle OutputDevice::LogicToDevicePixel(const tools::Rectangle& rLogicRect) const
{
    return ImplLogicToDevicePixel(rLogicRect);
}

tools::Long OutputDevice::DevicePixelToLogicWidth(tools::Long nWidth) const
{
    return ImplDevicePixelToLogicWidth(nWidth);
}

tools::Long OutputDevice::DevicePixelToLogicHeight(tools::Long nHeight) const
{
    return ImplDevicePixelToLogicHeight(nHeight);
}

static double lcl_logicToSubPixel(tools::Long n, tools::Long nDPI, tools::Long nMapNum,
                                  tools::Long nMapDenom)
{
    assert(nDPI > 0);
    assert(nMapDenom != 0);
    return static_cast<double>(n) * nMapNum * nDPI / nMapDenom;
}

static tools::Long lcl_subPixelToLogic(double n, tools::Long nDPI, tools::Long nMapNum,
                                       tools::Long nMapDenom)
{
    assert(nDPI > 0);
    assert(nMapNum != 0);

    return std::round(n * nMapDenom / nMapNum / nDPI);
}

static tools::Long lcl_pixelToLogic(tools::Long n, tools::Long nDPI, tools::Long nMapNum,
                                    tools::Long nMapDenom)
{
    assert(nDPI > 0);

    if (nMapNum == 0)
        return 0;

    sal_Int64 nDenom = nDPI * nMapNum;
    sal_Int64 n64 = n * nMapDenom;

    if (nDenom == 1)
        return static_cast<tools::Long>(n64);

    n64 = 2 * n64 / nDenom;

    if (n64 < 0)
        --n64;
    else
        ++n64;

    return static_cast<tools::Long>(n64 / 2);
}

static double lcl_pixelToLogicDouble(double n, tools::Long nDPI, tools::Long nMapNum,
                                     tools::Long nMapDenom)
{
    assert(nDPI > 0);
    if (nMapNum == 0)
        return 0;

    n *= nMapDenom;
    n /= nDPI;
    n /= nMapNum;
    return n;
}

double OutputDevice::ImplLogicHeightToDeviceSubPixel(tools::Long nHeight) const
{
    if (!mbMap)
        return nHeight;

    return lcl_logicToSubPixel(nHeight, GetDPIY(), maMapRes.mnMapScNumY, maMapRes.mnMapScDenomY);
}

static tools::Long lcl_logicToPixel(tools::Long n, tools::Long nDPI, tools::Long nMapNum,
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

    sal_Int64 n64 = n * nMapNum * nDPI;

    if (nMapDenom == 1)
        return static_cast<tools::Long>(n64);

    n64 = 2 * n64 / nMapDenom;

    if (n64 < 0)
        --n64;
    else
        ++n64;

    return static_cast<tools::Long>(n64 / 2);
}

tools::Long OutputDevice::ImplLogicXToDevicePixel(tools::Long nX) const
{
    if (!mbMap)
        return nX + GetOutOffXPixel();

    return lcl_logicToPixel(nX + maMapRes.mnMapOfsX, GetDPIX(), maMapRes.mnMapScNumX,
                            maMapRes.mnMapScDenomX)
           + GetOutOffXPixel() + mnOutOffOrigX;
}

tools::Long OutputDevice::ImplLogicYToDevicePixel(tools::Long nY) const
{
    if (!mbMap)
        return nY + GetOutOffYPixel();

    return lcl_logicToPixel(nY + maMapRes.mnMapOfsY, GetDPIY(), maMapRes.mnMapScNumY,
                            maMapRes.mnMapScDenomY)
           + GetOutOffYPixel() + mnOutOffOrigY;
}

tools::Long OutputDevice::ImplLogicHeightToDevicePixel(tools::Long nHeight) const
{
    if (!mbMap)
        return nHeight;

    return lcl_logicToPixel(nHeight, GetDPIY(), maMapRes.mnMapScNumY, maMapRes.mnMapScDenomY);
}

tools::Long OutputDevice::ImplDevicePixelToLogicWidth(tools::Long nWidth) const
{
    if (!mbMap)
        return nWidth;

    return lcl_pixelToLogic(nWidth, GetDPIX(), maMapRes.mnMapScNumX, maMapRes.mnMapScDenomX);
}

SAL_DLLPRIVATE double OutputDevice::ImplDevicePixelToLogicWidthDouble(double nWidth) const
{
    if (!mbMap)
        return nWidth;

    return lcl_pixelToLogicDouble(nWidth, GetDPIX(), maMapRes.mnMapScNumX, maMapRes.mnMapScDenomX);
}

tools::Long OutputDevice::ImplDevicePixelToLogicHeight(tools::Long nHeight) const
{
    if (!mbMap)
        return nHeight;

    return lcl_pixelToLogic(nHeight, GetDPIY(), maMapRes.mnMapScNumY, maMapRes.mnMapScDenomY);
}

double OutputDevice::ImplDevicePixelToLogicHeightDouble(double nHeight) const
{
    if (!mbMap)
        return nHeight;

    return lcl_pixelToLogicDouble(nHeight, GetDPIY(), maMapRes.mnMapScNumY, maMapRes.mnMapScDenomY);
}

Size OutputDevice::ImplLogicToDevicePixel(const Size& rLogicSize) const
{
    if (!mbMap)
        return rLogicSize;

    return Size(
        lcl_logicToPixel(rLogicSize.Width(), GetDPIX(), maMapRes.mnMapScNumX, maMapRes.mnMapScDenomX),
        lcl_logicToPixel(rLogicSize.Height(), GetDPIY(), maMapRes.mnMapScNumY,
                         maMapRes.mnMapScDenomY));
}

tools::Polygon OutputDevice::ImplLogicToDevicePixel(const tools::Polygon& rLogicPoly) const
{
    if (!mbMap && !GetOutOffXPixel() && !GetOutOffYPixel())
        return rLogicPoly;

    const sal_uInt16 nPoints = rLogicPoly.GetSize();
    tools::Polygon aPoly(rLogicPoly);

    // get pointer to Point-array (copy data)
    const Point* pPointAry = aPoly.GetConstPointAry();

    if (mbMap)
    {
        for (sal_uInt16 i = 0; i < nPoints; i++)
        {
            const Point& rPt = pPointAry[i];
            Point aPt(lcl_logicToPixel(rPt.X() + maMapRes.mnMapOfsX, GetDPIX(), maMapRes.mnMapScNumX,
                                       maMapRes.mnMapScDenomX)
                          + GetOutOffXPixel() + mnOutOffOrigX,
                      lcl_logicToPixel(rPt.Y() + maMapRes.mnMapOfsY, GetDPIY(), maMapRes.mnMapScNumY,
                                       maMapRes.mnMapScDenomY)
                          + GetOutOffYPixel() + mnOutOffOrigY);
            aPoly[i] = aPt;
        }

        return aPoly;
    }

    for (sal_uInt16 i = 0; i < nPoints; i++)
    {
        Point aPt = pPointAry[i];
        aPt.AdjustX(GetOutOffXPixel());
        aPt.AdjustY(GetOutOffYPixel());
        aPoly[i] = aPt;
    }

    return aPoly;
}

basegfx::B2DPolygon
OutputDevice::ImplLogicToDevicePixel(const basegfx::B2DPolygon& rLogicPoly) const
{
    if (!mbMap && !GetOutOffXPixel() && !GetOutOffYPixel())
        return rLogicPoly;

    const sal_uInt32 nPoints = rLogicPoly.count();
    basegfx::B2DPolygon aPoly(rLogicPoly);

    basegfx::B2DPoint aC1;
    basegfx::B2DPoint aC2;

    if (!mbMap)
    {
        for (sal_uInt32 i = 0; i < nPoints; ++i)
        {
            const basegfx::B2DPoint& rPt = aPoly.getB2DPoint(i);
            basegfx::B2DPoint aPt(rPt.getX() + GetOutOffXPixel(), rPt.getY() + GetOutOffYPixel());

            const bool bC1 = aPoly.isPrevControlPointUsed(i);
            if (bC1)
            {
                const basegfx::B2DPoint aB2DC1(aPoly.getPrevControlPoint(i));

                aC1 = basegfx::B2DPoint(aB2DC1.getX() + GetOutOffXPixel(), aB2DC1.getY() + GetOutOffYPixel());
            }

            const bool bC2 = aPoly.isNextControlPointUsed(i);
            if (bC2)
            {
                const basegfx::B2DPoint aB2DC2(aPoly.getNextControlPoint(i));

                aC2 = basegfx::B2DPoint(aB2DC2.getX() + GetOutOffXPixel(), aB2DC2.getY() + GetOutOffYPixel());
            }

            aPoly.setB2DPoint(i, aPt);

            if (bC1)
                aPoly.setPrevControlPoint(i, aC1);

            if (bC2)
                aPoly.setNextControlPoint(i, aC2);
        }

        return aPoly;
    }

    for (sal_uInt32 i = 0; i < nPoints; ++i)
    {
        const basegfx::B2DPoint& rPt = aPoly.getB2DPoint(i);
        basegfx::B2DPoint aPt(lcl_logicToPixel(rPt.getX() + maMapRes.mnMapOfsX, GetDPIX(),
                                               maMapRes.mnMapScNumX, maMapRes.mnMapScDenomX)
                                  + GetOutOffXPixel() + mnOutOffOrigX,
                              lcl_logicToPixel(rPt.getY() + maMapRes.mnMapOfsY, GetDPIY(),
                                               maMapRes.mnMapScNumY, maMapRes.mnMapScDenomY)
                                  + GetOutOffYPixel() + mnOutOffOrigY);

        const bool bC1 = aPoly.isPrevControlPointUsed(i);
        if (bC1)
        {
            const basegfx::B2DPoint aB2DC1(aPoly.getPrevControlPoint(i));

            aC1 = basegfx::B2DPoint(
                lcl_logicToPixel(aB2DC1.getX() + maMapRes.mnMapOfsX, GetDPIX(),
                                 maMapRes.mnMapScNumX, maMapRes.mnMapScDenomX)
                    + GetOutOffXPixel() + mnOutOffOrigX,
                lcl_logicToPixel(aB2DC1.getY() + maMapRes.mnMapOfsY, GetDPIY(),
                                 maMapRes.mnMapScNumY, maMapRes.mnMapScDenomY)
                    + GetOutOffYPixel() + mnOutOffOrigY);
        }

        const bool bC2 = aPoly.isNextControlPointUsed(i);
        if (bC2)
        {
            const basegfx::B2DPoint aB2DC2(aPoly.getNextControlPoint(i));

            aC2 = basegfx::B2DPoint(
                lcl_logicToPixel(aB2DC2.getX() + maMapRes.mnMapOfsX, GetDPIX(),
                                 maMapRes.mnMapScNumX, maMapRes.mnMapScDenomX)
                    + GetOutOffXPixel() + mnOutOffOrigX,
                lcl_logicToPixel(aB2DC2.getY() + maMapRes.mnMapOfsY, GetDPIY(),
                                 maMapRes.mnMapScNumY, maMapRes.mnMapScDenomY)
                    + GetOutOffYPixel() + mnOutOffOrigY);
        }

        aPoly.setB2DPoint(i, aPt);

        if (bC1)
            aPoly.setPrevControlPoint(i, aC1);

        if (bC2)
            aPoly.setNextControlPoint(i, aC2);
    }

    return aPoly;
}

tools::PolyPolygon
OutputDevice::ImplLogicToDevicePixel(const tools::PolyPolygon& rLogicPolyPoly) const
{
    if (!mbMap && !GetOutOffXPixel() && !GetOutOffYPixel())
        return rLogicPolyPoly;

    tools::PolyPolygon aPolyPoly(rLogicPolyPoly);
    const sal_uInt16 nPoly = aPolyPoly.Count();

    for (sal_uInt16 i = 0; i < nPoly; i++)
    {
        tools::Polygon& rPoly = aPolyPoly[i];
        rPoly = ImplLogicToDevicePixel(rPoly);
    }
    return aPolyPoly;
}

LineInfo OutputDevice::ImplLogicToDevicePixel(const LineInfo& rLineInfo) const
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

tools::Rectangle OutputDevice::ImplDevicePixelToLogic(const tools::Rectangle& rPixelRect) const
{
    // tdf#141761 see comments above, IsEmpty() removed
    tools::Rectangle aRetval;

    if (!mbMap)
    {
        aRetval
            = tools::Rectangle(rPixelRect.Left() - GetOutOffXPixel(), rPixelRect.Top() - GetOutOffYPixel(),
                               rPixelRect.IsWidthEmpty() ? 0 : rPixelRect.Right() - GetOutOffXPixel(),
                               rPixelRect.IsHeightEmpty() ? 0 : rPixelRect.Bottom() - GetOutOffYPixel());
    }
    else
    {
        aRetval = tools::Rectangle(
            lcl_pixelToLogic(rPixelRect.Left() - GetOutOffXPixel() - mnOutOffOrigX, GetDPIX(),
                             maMapRes.mnMapScNumX, maMapRes.mnMapScDenomX)
                - maMapRes.mnMapOfsX,
            lcl_pixelToLogic(rPixelRect.Top() - GetOutOffYPixel() - mnOutOffOrigY, GetDPIY(),
                             maMapRes.mnMapScNumY, maMapRes.mnMapScDenomY)
                - maMapRes.mnMapOfsY,
            rPixelRect.IsWidthEmpty()
                ? 0
                : lcl_pixelToLogic(rPixelRect.Right() - GetOutOffXPixel() - mnOutOffOrigX, GetDPIX(),
                                   maMapRes.mnMapScNumX, maMapRes.mnMapScDenomX)
                      - maMapRes.mnMapOfsX,
            rPixelRect.IsHeightEmpty()
                ? 0
                : lcl_pixelToLogic(rPixelRect.Bottom() - GetOutOffYPixel() - mnOutOffOrigY, GetDPIY(),
                                   maMapRes.mnMapScNumY, maMapRes.mnMapScDenomY)
                      - maMapRes.mnMapOfsY);
    }

    if (rPixelRect.IsWidthEmpty())
        aRetval.SetWidthEmpty();

    if (rPixelRect.IsHeightEmpty())
        aRetval.SetHeightEmpty();

    return aRetval;
}

vcl::Region OutputDevice::ImplPixelToDevicePixel(const vcl::Region& rRegion) const
{
    if (!GetOutOffXPixel() && !GetOutOffYPixel())
        return rRegion;

    vcl::Region aRegion(rRegion);
    aRegion.Move(GetOutOffXPixel() + mnOutOffOrigX, GetOutOffYPixel() + mnOutOffOrigY);
    return aRegion;
}

void OutputDevice::ImplEnableMapMode(bool bEnable) { mbMap = bEnable; }

void OutputDevice::ImplSetMapMode()
{
    if (!mbMap && maMapMode.IsDefault())
        return;

    mbMap = false;
    maMapMode = MapMode();

    // create new objects (clip region are not re-scaled)
    mbNewFont = true;
    mbInitFont = true;
    ImplInitMapModeObjects();

    // #106426# Adapt logical offset when changing mapmode
    mnOutOffLogicX = mnOutOffOrigX; // no mapping -> equal offsets
    mnOutOffLogicY = mnOutOffOrigY;

    // #i75163#
    ImplInvalidateViewTransform();
}

void OutputDevice::ImplSetMapMode(const MapMode& rNewMapMode)
{
    bool bRelMap = (rNewMapMode.GetMapUnit() == MapUnit::MapRelative);

    // do nothing if MapMode was not changed
    if (maMapMode == rNewMapMode)
        return;

    // if default MapMode calculate nothing
    bool bOldMap = mbMap;
    mbMap = !rNewMapMode.IsDefault();
    if (mbMap)
    {
        // if only the origin is converted, do not scale new
        if ((rNewMapMode.GetMapUnit() == maMapMode.GetMapUnit())
            && (rNewMapMode.GetScaleX() == maMapMode.GetScaleX())
            && (rNewMapMode.GetScaleY() == maMapMode.GetScaleY()) && (bOldMap == mbMap))
        {
            // set offset
            Point aOrigin = rNewMapMode.GetOrigin();
            maMapRes.mnMapOfsX = aOrigin.X();
            maMapRes.mnMapOfsY = aOrigin.Y();
            maMapMode = rNewMapMode;

            // #i75163#
            ImplInvalidateViewTransform();

            return;
        }
        if (!bOldMap && bRelMap)
        {
            maMapRes.mnMapScNumX = 1;
            maMapRes.mnMapScNumY = 1;
            maMapRes.mnMapScDenomX = GetDPIX();
            maMapRes.mnMapScDenomY = GetDPIY();
            maMapRes.mnMapOfsX = 0;
            maMapRes.mnMapOfsY = 0;
        }

        // calculate new MapMode-resolution
        maMapRes.CalcMapResolution(rNewMapMode, GetDPIX(), GetDPIY());
    }

    // set new MapMode
    if (bRelMap)
    {
        maMapMode.SetScaleX(Fraction::MakeFraction(
            maMapMode.GetScaleX().GetNumerator(), rNewMapMode.GetScaleX().GetNumerator(),
            maMapMode.GetScaleX().GetDenominator(), rNewMapMode.GetScaleX().GetDenominator()));

        maMapMode.SetScaleY(Fraction::MakeFraction(
            maMapMode.GetScaleY().GetNumerator(), rNewMapMode.GetScaleY().GetNumerator(),
            maMapMode.GetScaleY().GetDenominator(), rNewMapMode.GetScaleY().GetDenominator()));

        maMapMode.SetOrigin(Point(maMapRes.mnMapOfsX, maMapRes.mnMapOfsY));
    }
    else
    {
        maMapMode = rNewMapMode;
    }

    // create new objects (clip region are not re-scaled)
    mbNewFont = true;
    mbInitFont = true;
    ImplInitMapModeObjects();

    // #106426# Adapt logical offset when changing mapmode
    mnOutOffLogicX
        = lcl_pixelToLogic(mnOutOffOrigX, GetDPIX(), maMapRes.mnMapScNumX, maMapRes.mnMapScDenomX);
    mnOutOffLogicY
        = lcl_pixelToLogic(mnOutOffOrigY, GetDPIY(), maMapRes.mnMapScNumY, maMapRes.mnMapScDenomY);

    // #i75163#
    ImplInvalidateViewTransform();
}

void OutputDevice::ImplInitMapModeObjects() {}

void OutputDevice::ImplSetRelativeMapMode(const MapMode& rNewMapMode)
{
    // do nothing if MapMode did not change
    if (maMapMode == rNewMapMode)
        return;

    MapUnit eOld = maMapMode.GetMapUnit();
    MapUnit eNew = rNewMapMode.GetMapUnit();

    // a?F = rNewMapMode.GetScale?() / maMapMode.GetScale?()
    Fraction aXF = Fraction::MakeFraction(
        rNewMapMode.GetScaleX().GetNumerator(), maMapMode.GetScaleX().GetDenominator(),
        rNewMapMode.GetScaleX().GetDenominator(), maMapMode.GetScaleX().GetNumerator());
    Fraction aYF = Fraction::MakeFraction(
        rNewMapMode.GetScaleY().GetNumerator(), maMapMode.GetScaleY().GetDenominator(),
        rNewMapMode.GetScaleY().GetDenominator(), maMapMode.GetScaleY().GetNumerator());

    Point aPt(LogicToLogic(Point(), nullptr, &rNewMapMode));
    if (eNew != eOld)
    {
        if (eOld > MapUnit::MapPixel)
        {
            SAL_WARN("vcl.gdi", "Not implemented MapUnit");
        }
        else if (eNew > MapUnit::MapPixel)
        {
            SAL_WARN("vcl.gdi", "Not implemented MapUnit");
        }
        else
        {
            const auto eFrom = MapToO3tlLength(eOld, o3tl::Length::in);
            const auto eTo = MapToO3tlLength(eNew, o3tl::Length::in);
            const auto[mul, div] = o3tl::getConversionMulDiv(eFrom, eTo);
            Fraction aF(div, mul);

            // a?F =  a?F * aF
            aXF = Fraction::MakeFraction(aXF.GetNumerator(), aF.GetNumerator(),
                                         aXF.GetDenominator(), aF.GetDenominator());
            aYF = Fraction::MakeFraction(aYF.GetNumerator(), aF.GetNumerator(),
                                         aYF.GetDenominator(), aF.GetDenominator());
            if (eOld == MapUnit::MapPixel)
            {
                aXF *= Fraction(GetDPIX(), 1);
                aYF *= Fraction(GetDPIY(), 1);
            }
            else if (eNew == MapUnit::MapPixel)
            {
                aXF *= Fraction(1, GetDPIX());
                aYF *= Fraction(1, GetDPIY());
            }
        }
    }

    MapMode aNewMapMode(MapUnit::MapRelative, Point(-aPt.X(), -aPt.Y()), aXF, aYF);
    SetMapMode(aNewMapMode);

    if (eNew != eOld)
        maMapMode = rNewMapMode;

    // #106426# Adapt logical offset when changing MapMode
    mnOutOffLogicX
        = lcl_pixelToLogic(mnOutOffOrigX, GetDPIX(), maMapRes.mnMapScNumX, maMapRes.mnMapScDenomX);
    mnOutOffLogicY
        = lcl_pixelToLogic(mnOutOffOrigY, GetDPIY(), maMapRes.mnMapScNumY, maMapRes.mnMapScDenomY);
}

basegfx::B2DHomMatrix OutputDevice::ImplGetViewTransformation() const
{
    if (!mbMap || !mpOutDevData)
        return basegfx::B2DHomMatrix();

    if (mpOutDevData->mpViewTransform)
        return *mpOutDevData->mpViewTransform;

    mpOutDevData->mpViewTransform = new basegfx::B2DHomMatrix;

    const double fScaleFactorX(static_cast<double>(GetDPIX())
                               * static_cast<double>(maMapRes.mnMapScNumX)
                               / static_cast<double>(maMapRes.mnMapScDenomX));
    const double fScaleFactorY(static_cast<double>(GetDPIY())
                               * static_cast<double>(maMapRes.mnMapScNumY)
                               / static_cast<double>(maMapRes.mnMapScDenomY));
    const double fZeroPointX((static_cast<double>(maMapRes.mnMapOfsX) * fScaleFactorX)
                             + static_cast<double>(mnOutOffOrigX));
    const double fZeroPointY((static_cast<double>(maMapRes.mnMapOfsY) * fScaleFactorY)
                             + static_cast<double>(mnOutOffOrigY));

    mpOutDevData->mpViewTransform->set(0, 0, fScaleFactorX);
    mpOutDevData->mpViewTransform->set(1, 1, fScaleFactorY);
    mpOutDevData->mpViewTransform->set(0, 2, fZeroPointX);
    mpOutDevData->mpViewTransform->set(1, 2, fZeroPointY);

    return *mpOutDevData->mpViewTransform;
}

basegfx::B2DHomMatrix OutputDevice::ImplGetInverseViewTransformation() const
{
    if (!mbMap || !mpOutDevData)
        return basegfx::B2DHomMatrix();

    if (mpOutDevData->mpInverseViewTransform)
        return *mpOutDevData->mpInverseViewTransform;

    GetViewTransformation();

    mpOutDevData->mpInverseViewTransform
        = new basegfx::B2DHomMatrix(*mpOutDevData->mpViewTransform);
    mpOutDevData->mpInverseViewTransform->invert();

    return *mpOutDevData->mpInverseViewTransform;
}

basegfx::B2DHomMatrix OutputDevice::ImplGetViewTransformation(const MapMode& rMapMode) const
{
    // #i82615#
    ImplMapRes aMapRes;
    aMapRes.CalcMapResolution(rMapMode, GetDPIX(), GetDPIY());

    basegfx::B2DHomMatrix aTransform;

    const double fScaleFactorX(static_cast<double>(GetDPIX())
                               * static_cast<double>(aMapRes.mnMapScNumX)
                               / static_cast<double>(aMapRes.mnMapScDenomX));
    const double fScaleFactorY(static_cast<double>(GetDPIY())
                               * static_cast<double>(aMapRes.mnMapScNumY)
                               / static_cast<double>(aMapRes.mnMapScDenomY));
    const double fZeroPointX((static_cast<double>(aMapRes.mnMapOfsX) * fScaleFactorX)
                             + static_cast<double>(mnOutOffOrigX));
    const double fZeroPointY((static_cast<double>(aMapRes.mnMapOfsY) * fScaleFactorY)
                             + static_cast<double>(mnOutOffOrigY));

    aTransform.set(0, 0, fScaleFactorX);
    aTransform.set(1, 1, fScaleFactorY);
    aTransform.set(0, 2, fZeroPointX);
    aTransform.set(1, 2, fZeroPointY);

    return aTransform;
}

basegfx::B2DHomMatrix OutputDevice::ImplGetInverseViewTransformation(const MapMode& rMapMode) const
{
    basegfx::B2DHomMatrix aMatrix(GetViewTransformation(rMapMode));
    aMatrix.invert();
    return aMatrix;
}

basegfx::B2DHomMatrix OutputDevice::ImplGetDeviceTransformation() const
{
    basegfx::B2DHomMatrix aTransformation = GetViewTransformation();
    // TODO: is it worth to cache the transformed result?
    if (GetOutOffXPixel() || GetOutOffYPixel())
        aTransformation.translate(GetOutOffXPixel(), GetOutOffYPixel());
    return aTransformation;
}

Point OutputDevice::ImplSubPixelToLogic(const basegfx::B2DPoint& rDevicePt) const
{
    if (!mbMap)
    {
        assert(floor(rDevicePt.getX() == rDevicePt.getX())
               && floor(rDevicePt.getY() == rDevicePt.getY()));
        return Point(rDevicePt.getX(), rDevicePt.getY());
    }

    return Point(
        lcl_subPixelToLogic(rDevicePt.getX(), GetDPIX(), maMapRes.mnMapScNumX, maMapRes.mnMapScDenomX)
            - maMapRes.mnMapOfsX - mnOutOffLogicX,
        lcl_subPixelToLogic(rDevicePt.getY(), GetDPIY(), maMapRes.mnMapScNumY, maMapRes.mnMapScDenomY)
            - maMapRes.mnMapOfsY - mnOutOffLogicY);
}

basegfx::B2DPoint OutputDevice::ImplLogicToDeviceSubPixel(const Point& rPoint) const
{
    if (!mbMap)
        return basegfx::B2DPoint(rPoint.X() + GetOutOffXPixel(), rPoint.Y() + GetOutOffYPixel());

    return basegfx::B2DPoint(lcl_logicToSubPixel(rPoint.X() + maMapRes.mnMapOfsX, GetDPIX(),
                                                 maMapRes.mnMapScNumX, maMapRes.mnMapScDenomX)
                                 + GetOutOffXPixel() + mnOutOffOrigX,
                             lcl_logicToSubPixel(rPoint.Y() + maMapRes.mnMapOfsY, GetDPIY(),
                                                 maMapRes.mnMapScNumY, maMapRes.mnMapScDenomY)
                                 + GetOutOffYPixel() + mnOutOffOrigY);
}

double OutputDevice::ImplLogicWidthToDeviceSubPixel(tools::Long nWidth) const
{
    if (!mbMap)
        return nWidth;

    return lcl_logicToSubPixel(nWidth, GetDPIX(), maMapRes.mnMapScNumX, maMapRes.mnMapScDenomX);
}

void OutputDevice::ImplSetPixelOffset(const Size& rOffset)
{
    mnOutOffOrigX = rOffset.Width();
    mnOutOffOrigY = rOffset.Height();

    mnOutOffLogicX
        = lcl_pixelToLogic(mnOutOffOrigX, GetDPIX(), maMapRes.mnMapScNumX, maMapRes.mnMapScDenomX);
    mnOutOffLogicY
        = lcl_pixelToLogic(mnOutOffOrigY, GetDPIY(), maMapRes.mnMapScNumY, maMapRes.mnMapScDenomY);
}

static void lcl_verifyUnitSourceDest(MapUnit eUnitSource, MapUnit eUnitDest)
{
    DBG_ASSERT(eUnitSource != MapUnit::MapSysFont && eUnitSource != MapUnit::MapAppFont
                   && eUnitSource != MapUnit::MapRelative,
               "Source MapUnit is not permitted");
    DBG_ASSERT(eUnitDest != MapUnit::MapSysFont && eUnitDest != MapUnit::MapAppFont
                   && eUnitDest != MapUnit::MapRelative,
               "Destination MapUnit is not permitted");
}

static auto lcl_getCorrectedUnit(MapUnit eMapSrc, MapUnit eMapDst)
{
    o3tl::Length eSrc = o3tl::Length::invalid;
    o3tl::Length eDst = o3tl::Length::invalid;
    if (eMapSrc > MapUnit::MapPixel)
        SAL_WARN("vcl.gdi", "Invalid source map unit");
    else if (eMapDst > MapUnit::MapPixel)
        SAL_WARN("vcl.gdi", "Invalid destination map unit");
    else if (eMapSrc != eMapDst)
    {
        // Here 72 PPI is assumed for MapPixel
        eSrc = MapToO3tlLength(eMapSrc, o3tl::Length::pt);
        eDst = MapToO3tlLength(eMapDst, o3tl::Length::pt);
    }
    return std::make_pair(eSrc, eDst);
}

static tools::Long lcl_convertLogicValue(const tools::Long n1, const o3tl::Length eFrom,
                                         const o3tl::Length eTo)
{
    if (n1 == 0 || eFrom == o3tl::Length::invalid || eTo == o3tl::Length::invalid)
        return 0;

    bool bOverflow;
    const auto nResult = o3tl::convert(n1, eFrom, eTo, bOverflow);

    if (!bOverflow)
        return nResult;

    const auto[n2, n3] = o3tl::getConversionMulDiv(eFrom, eTo);

    BigInt a4 = n1;
    a4 *= n2;

    if (a4.IsNeg())
        a4 -= n3 / 2;
    else
        a4 += n3 / 2;

    a4 /= n3;

    return static_cast<tools::Long>(a4);
}

tools::Long LogicToLogic(tools::Long nLongSource, MapUnit eUnitSource, MapUnit eUnitDest)
{
    if (eUnitSource == eUnitDest)
        return nLongSource;

    lcl_verifyUnitSourceDest(eUnitSource, eUnitDest);
    const auto[eFrom, eTo] = lcl_getCorrectedUnit(eUnitSource, eUnitDest);
    return lcl_convertLogicValue(nLongSource, eFrom, eTo);
}

static std::pair<ImplMapRes, ImplMapRes> lcl_calcConversionMapRes(const MapMode& rMMSource,
                                                                  const MapMode& rMMDest)
{
    std::pair<ImplMapRes, ImplMapRes> result;
    result.first.CalcMapResolution(rMMSource, 72, 72);
    result.second.CalcMapResolution(rMMDest, 72, 72);
    return result;
}

// return (n1 * n2 * n3) / (n4 * n5)
static tools::Long lcl_scaleLogicValue(const tools::Long n1, const tools::Long n2,
                                       const tools::Long n3, const tools::Long n4,
                                       const tools::Long n5)
{
    if (n1 == 0 || n2 == 0 || n3 == 0 || n4 == 0 || n5 == 0)
        return 0;

    if (std::numeric_limits<tools::Long>::max() / std::abs(n2) < std::abs(n3))
    {
        // a6 is skipped
        BigInt a7 = n2;
        a7 *= n3;
        a7 *= n1;

        if (std::numeric_limits<tools::Long>::max() / std::abs(n4) < std::abs(n5))
        {
            BigInt a8 = n4;
            a8 *= n5;

            BigInt a9 = a8;
            a9 /= 2;
            if (a7.IsNeg())
                a7 -= a9;
            else
                a7 += a9;

            a7 /= a8;
        } // of if
        else
        {
            tools::Long n8 = n4 * n5;

            if (a7.IsNeg())
                a7 -= n8 / 2;
            else
                a7 += n8 / 2;

            a7 /= n8;
        } // of else
        return static_cast<tools::Long>(a7);
    }

    tools::Long n6 = n2 * n3;

    if (std::numeric_limits<tools::Long>::max() / std::abs(n1) < std::abs(n6))
    {
        BigInt a7 = n1;
        a7 *= n6;

        if (std::numeric_limits<tools::Long>::max() / std::abs(n4) < std::abs(n5))
        {
            BigInt a8 = n4;
            a8 *= n5;

            BigInt a9 = a8;
            a9 /= 2;
            if (a7.IsNeg())
                a7 -= a9;
            else
                a7 += a9;

            a7 /= a8;
        } // of if
        else
        {
            tools::Long n8 = n4 * n5;

            if (a7.IsNeg())
                a7 -= n8 / 2;
            else
                a7 += n8 / 2;

            a7 /= n8;
        }

        return static_cast<tools::Long>(a7);
    }

    tools::Long n7 = n1 * n6;

    if (std::numeric_limits<tools::Long>::max() / std::abs(n4) < std::abs(n5))
    {
        BigInt a7 = n7;
        BigInt a8 = n4;
        a8 *= n5;

        BigInt a9 = a8;
        a9 /= 2;
        if (a7.IsNeg())
            a7 -= a9;
        else
            a7 += a9;

        a7 /= a8;
        return static_cast<tools::Long>(a7);
    }

    const tools::Long n8 = n4 * n5;
    const tools::Long n8_2 = n8 / 2;

    if (n7 < 0)
    {
        if ((n7 - std::numeric_limits<tools::Long>::min()) >= n8_2)
            n7 -= n8_2;
    }
    else if ((std::numeric_limits<tools::Long>::max() - n7) >= n8_2)
    {
        n7 += n8_2;
    }

    return n7 / n8;
}

tools::Rectangle LogicToLogic(const tools::Rectangle& rRectSource, const MapMode& rMapModeSource,
                              const MapMode& rMapModeDest)
{
    if (rMapModeSource == rMapModeDest)
        return rRectSource;

    MapUnit eUnitSource = rMapModeSource.GetMapUnit();
    MapUnit eUnitDest = rMapModeDest.GetMapUnit();
    lcl_verifyUnitSourceDest(eUnitSource, eUnitDest);

    tools::Rectangle aRetval;

    if (rMapModeSource.IsSimple() && rMapModeDest.IsSimple())
    {
        const auto[eFrom, eTo] = lcl_getCorrectedUnit(eUnitSource, eUnitDest);

        auto left = lcl_convertLogicValue(rRectSource.Left(), eFrom, eTo);
        auto top = lcl_convertLogicValue(rRectSource.Top(), eFrom, eTo);

        // tdf#141761 see comments above, IsEmpty() removed
        auto right = rRectSource.IsWidthEmpty()
                         ? 0
                         : lcl_convertLogicValue(rRectSource.Right(), eFrom, eTo);
        auto bottom = rRectSource.IsHeightEmpty()
                          ? 0
                          : lcl_convertLogicValue(rRectSource.Bottom(), eFrom, eTo);

        aRetval = tools::Rectangle(left, top, right, bottom);
    }
    else
    {
        const auto[aMapResSource, aMapResDest]
            = lcl_calcConversionMapRes(rMapModeSource, rMapModeDest);

        auto left = lcl_scaleLogicValue(rRectSource.Left() + aMapResSource.mnMapOfsX,
                                        aMapResSource.mnMapScNumX, aMapResDest.mnMapScDenomX,
                                        aMapResSource.mnMapScDenomX, aMapResDest.mnMapScNumX)
                    - aMapResDest.mnMapOfsX;
        auto top = lcl_scaleLogicValue(rRectSource.Top() + aMapResSource.mnMapOfsY,
                                       aMapResSource.mnMapScNumY, aMapResDest.mnMapScDenomY,
                                       aMapResSource.mnMapScDenomY, aMapResDest.mnMapScNumY)
                   - aMapResDest.mnMapOfsY;

        // tdf#141761 see comments above, IsEmpty() removed
        auto right = rRectSource.IsWidthEmpty()
                         ? 0
                         : lcl_scaleLogicValue(rRectSource.Right() + aMapResSource.mnMapOfsX,
                                               aMapResSource.mnMapScNumX, aMapResDest.mnMapScDenomX,
                                               aMapResSource.mnMapScDenomX, aMapResDest.mnMapScNumX)
                               - aMapResDest.mnMapOfsX;
        auto bottom
            = rRectSource.IsHeightEmpty()
                  ? 0
                  : lcl_scaleLogicValue(rRectSource.Bottom() + aMapResSource.mnMapOfsY,
                                        aMapResSource.mnMapScNumY, aMapResDest.mnMapScDenomY,
                                        aMapResSource.mnMapScDenomY, aMapResDest.mnMapScNumY)
                        - aMapResDest.mnMapOfsY;

        aRetval = tools::Rectangle(left, top, right, bottom);
    }

    if (rRectSource.IsWidthEmpty())
        aRetval.SetWidthEmpty();

    if (rRectSource.IsHeightEmpty())
        aRetval.SetHeightEmpty();

    return aRetval;
}

basegfx::B2DHomMatrix LogicToLogic(const MapMode& rMapModeSource, const MapMode& rMapModeDest)
{
    basegfx::B2DHomMatrix aTransform;

    if (rMapModeSource == rMapModeDest)
    {
        return aTransform;
    }

    MapUnit eUnitSource = rMapModeSource.GetMapUnit();
    MapUnit eUnitDest = rMapModeDest.GetMapUnit();
    lcl_verifyUnitSourceDest(eUnitSource, eUnitDest);

    if (rMapModeSource.IsSimple() && rMapModeDest.IsSimple())
    {
        const auto[eFrom, eTo] = lcl_getCorrectedUnit(eUnitSource, eUnitDest);
        const double fScaleFactor(eFrom == o3tl::Length::invalid || eTo == o3tl::Length::invalid
                                      ? std::numeric_limits<double>::quiet_NaN()
                                      : o3tl::convert(1.0, eFrom, eTo));
        aTransform.set(0, 0, fScaleFactor);
        aTransform.set(1, 1, fScaleFactor);

        return aTransform;
    }

    const auto[aMapResSource, aMapResDest]
        = lcl_calcConversionMapRes(rMapModeSource, rMapModeDest);

    const double fScaleFactorX(
        (double(aMapResSource.mnMapScNumX) * double(aMapResDest.mnMapScDenomX))
        / (double(aMapResSource.mnMapScDenomX) * double(aMapResDest.mnMapScNumX)));
    const double fScaleFactorY(
        (double(aMapResSource.mnMapScNumY) * double(aMapResDest.mnMapScDenomY))
        / (double(aMapResSource.mnMapScDenomY) * double(aMapResDest.mnMapScNumY)));
    const double fZeroPointX(double(aMapResSource.mnMapOfsX) * fScaleFactorX
                             - double(aMapResDest.mnMapOfsX));
    const double fZeroPointY(double(aMapResSource.mnMapOfsY) * fScaleFactorY
                             - double(aMapResDest.mnMapOfsY));

    aTransform.set(0, 0, fScaleFactorX);
    aTransform.set(1, 1, fScaleFactorY);
    aTransform.set(0, 2, fZeroPointX);
    aTransform.set(1, 2, fZeroPointY);

    return aTransform;
}

basegfx::B2DPolygon LogicToLogic(const basegfx::B2DPolygon& rPolySource,
                                 const MapMode& rMapModeSource, const MapMode& rMapModeDest)
{
    if (rMapModeSource == rMapModeDest)
        return rPolySource;

    const basegfx::B2DHomMatrix aTransform(LogicToLogic(rMapModeSource, rMapModeDest));
    basegfx::B2DPolygon aPoly(rPolySource);

    aPoly.transform(aTransform);
    return aPoly;
}

Size LogicToLogic(const Size& rSzSource, const MapMode& rMapModeSource, const MapMode& rMapModeDest)
{
    if (rMapModeSource == rMapModeDest)
        return rSzSource;

    MapUnit eUnitSource = rMapModeSource.GetMapUnit();
    MapUnit eUnitDest = rMapModeDest.GetMapUnit();
    lcl_verifyUnitSourceDest(eUnitSource, eUnitDest);

    if (rMapModeSource.IsSimple() && rMapModeDest.IsSimple())
    {
        const auto[eFrom, eTo] = lcl_getCorrectedUnit(eUnitSource, eUnitDest);
        return Size(lcl_convertLogicValue(rSzSource.Width(), eFrom, eTo),
                    lcl_convertLogicValue(rSzSource.Height(), eFrom, eTo));
    }

    const auto[aMapResSource, aMapResDest]
        = lcl_calcConversionMapRes(rMapModeSource, rMapModeDest);

    return Size(lcl_scaleLogicValue(rSzSource.Width(), aMapResSource.mnMapScNumX,
                                    aMapResDest.mnMapScDenomX, aMapResSource.mnMapScDenomX,
                                    aMapResDest.mnMapScNumX),
                lcl_scaleLogicValue(rSzSource.Height(), aMapResSource.mnMapScNumY,
                                    aMapResDest.mnMapScDenomY, aMapResSource.mnMapScDenomY,
                                    aMapResDest.mnMapScNumY));
}

Point LogicToLogic(const Point& rPtSource, const MapMode& rMapModeSource,
                   const MapMode& rMapModeDest)
{
    if (rMapModeSource == rMapModeDest)
        return rPtSource;

    MapUnit eUnitSource = rMapModeSource.GetMapUnit();
    MapUnit eUnitDest = rMapModeDest.GetMapUnit();
    lcl_verifyUnitSourceDest(eUnitSource, eUnitDest);

    if (rMapModeSource.IsSimple() && rMapModeDest.IsSimple())
    {
        const auto[eFrom, eTo] = lcl_getCorrectedUnit(eUnitSource, eUnitDest);

        return Point(lcl_convertLogicValue(rPtSource.X(), eFrom, eTo),
                     lcl_convertLogicValue(rPtSource.Y(), eFrom, eTo));
    }

    const auto[aMapResSource, aMapResDest]
        = lcl_calcConversionMapRes(rMapModeSource, rMapModeDest);

    /* HISTORICAL NOTE & WARNING:
     * --------------------------
     * You might expect the formula below to be: (Point - Origin) * Scale.
     * * In standard graphics API terminology (like MS GDI):
     * - "Window Origin" implies shifting the logical coordinate system.
     * Mathematical effect: Subtraction (Point - Origin).
     * - "Viewport Origin" implies shifting the destination device output.
     * Mathematical effect: Addition (Point + Origin).
     * * VCL's MapMode::SetOrigin() uses the naming convention of a Window Origin,
     * but historically implements the behavior of a Translation/Viewport Offset.
     * * Therefore, the formula used throughout VCL (see lcl_logicToPixel) is:
     * (Point + Origin) * Scale
     * * We MUST use Addition (+) here to maintain consistency. Changing this to
     * Subtraction (-) will cause "LogicToLogic" to drift relative to "LogicToPixel",
     * resulting in printing misalignments and hit-testing errors.
     */

    // rPtSource.X() + aMapResSource.mnMapOfsX  <-- KEEP AS ADDITION
    return Point(lcl_scaleLogicValue(rPtSource.X() + aMapResSource.mnMapOfsX,
                                     aMapResSource.mnMapScNumX, aMapResDest.mnMapScDenomX,
                                     aMapResSource.mnMapScDenomX, aMapResDest.mnMapScNumX)
                     - aMapResDest.mnMapOfsX,
                 lcl_scaleLogicValue(rPtSource.Y() + aMapResSource.mnMapOfsY,
                                     aMapResSource.mnMapScNumY, aMapResDest.mnMapScDenomY,
                                     aMapResSource.mnMapScDenomY, aMapResDest.mnMapScNumY)
                     - aMapResDest.mnMapOfsY);
}

Point OutputDevice::ImplPixelToLogic(const Point& rDevicePt) const
{
    if (!mbMap)
        return rDevicePt;

    return Point(
        lcl_pixelToLogic(rDevicePt.X(), GetDPIX(), maMapRes.mnMapScNumX, maMapRes.mnMapScDenomX)
            - maMapRes.mnMapOfsX - mnOutOffLogicX,
        lcl_pixelToLogic(rDevicePt.Y(), GetDPIY(), maMapRes.mnMapScNumY, maMapRes.mnMapScDenomY)
            - maMapRes.mnMapOfsY - mnOutOffLogicY);
}

Size OutputDevice::ImplPixelToLogic(const Size& rDeviceSize) const
{
    if (!mbMap)
        return rDeviceSize;

    return Size(
        lcl_pixelToLogic(rDeviceSize.Width(), GetDPIX(), maMapRes.mnMapScNumX, maMapRes.mnMapScDenomX),
        lcl_pixelToLogic(rDeviceSize.Height(), GetDPIY(), maMapRes.mnMapScNumY,
                         maMapRes.mnMapScDenomY));
}

tools::Rectangle OutputDevice::ImplPixelToLogic(const tools::Rectangle& rDeviceRect) const
{
    // tdf#141761 see comments above, IsEmpty() removed
    if (!mbMap)
        return rDeviceRect;

    tools::Rectangle aRetval(
        lcl_pixelToLogic(rDeviceRect.Left(), GetDPIX(), maMapRes.mnMapScNumX, maMapRes.mnMapScDenomX)
            - maMapRes.mnMapOfsX - mnOutOffLogicX,
        lcl_pixelToLogic(rDeviceRect.Top(), GetDPIY(), maMapRes.mnMapScNumY, maMapRes.mnMapScDenomY)
            - maMapRes.mnMapOfsY - mnOutOffLogicY,
        rDeviceRect.IsWidthEmpty() ? 0
                                   : lcl_pixelToLogic(rDeviceRect.Right(), GetDPIX(),
                                                      maMapRes.mnMapScNumX, maMapRes.mnMapScDenomX)
                                         - maMapRes.mnMapOfsX - mnOutOffLogicX,
        rDeviceRect.IsHeightEmpty() ? 0
                                    : lcl_pixelToLogic(rDeviceRect.Bottom(), GetDPIY(),
                                                       maMapRes.mnMapScNumY, maMapRes.mnMapScDenomY)
                                          - maMapRes.mnMapOfsY - mnOutOffLogicY);

    if (rDeviceRect.IsWidthEmpty())
        aRetval.SetWidthEmpty();

    if (rDeviceRect.IsHeightEmpty())
        aRetval.SetHeightEmpty();

    return aRetval;
}

tools::Polygon OutputDevice::ImplPixelToLogic(const tools::Polygon& rDevicePoly) const
{
    if (!mbMap)
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
        aPt.setX(lcl_pixelToLogic(pPt->X(), GetDPIX(), maMapRes.mnMapScNumX, maMapRes.mnMapScDenomX)
                 - maMapRes.mnMapOfsX - mnOutOffLogicX);
        aPt.setY(lcl_pixelToLogic(pPt->Y(), GetDPIY(), maMapRes.mnMapScNumY, maMapRes.mnMapScDenomY)
                 - maMapRes.mnMapOfsY - mnOutOffLogicY);
        aPoly[i] = aPt;
    }

    return aPoly;
}

tools::PolyPolygon OutputDevice::ImplPixelToLogic(const tools::PolyPolygon& rDevicePolyPoly) const
{
    if (!mbMap)
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

basegfx::B2DPolyPolygon
OutputDevice::ImplPixelToLogic(const basegfx::B2DPolyPolygon& rPixelPolyPoly) const
{
    basegfx::B2DPolyPolygon aTransformedPoly = rPixelPolyPoly;
    const basegfx::B2DHomMatrix aTransformationMatrix = GetInverseViewTransformation();
    aTransformedPoly.transform(aTransformationMatrix);
    return aTransformedPoly;
}

basegfx::B2DRectangle OutputDevice::ImplPixelToLogic(const basegfx::B2DRectangle& rDeviceRect) const
{
    basegfx::B2DRectangle aTransformedRect = rDeviceRect;
    const basegfx::B2DHomMatrix aTransformationMatrix = GetInverseViewTransformation();
    aTransformedRect.transform(aTransformationMatrix);
    return aTransformedRect;
}

vcl::Region OutputDevice::ImplPixelToLogic(const vcl::Region& rDeviceRegion) const
{
    if (!mbMap || rDeviceRegion.IsNull() || rDeviceRegion.IsEmpty())
        return rDeviceRegion;

    if (rDeviceRegion.getB2DPolyPolygon())
        return vcl::Region(PixelToLogic(*rDeviceRegion.getB2DPolyPolygon()));

    if (rDeviceRegion.getPolyPolygon())
        return vcl::Region(PixelToLogic(*rDeviceRegion.getPolyPolygon()));

    vcl::Region aRegion;

    if (rDeviceRegion.getRegionBand())
    {
        RectangleVector aRectangles;
        rDeviceRegion.GetRegionRectangles(aRectangles);
        const RectangleVector& rRectangles(aRectangles); // needed to make the '!=' work

        // make reverse run to fill new region bottom-up, this will speed it up due to the used data structuring
        for (RectangleVector::const_reverse_iterator aRectIter(rRectangles.rbegin());
             aRectIter != rRectangles.rend(); ++aRectIter)
        {
            aRegion.Union(PixelToLogic(*aRectIter));
        }
    }

    return aRegion;
}

Point OutputDevice::ImplPixelToLogic(const Point& rDevicePt, const MapMode& rMapMode) const
{
    // calculate nothing if default-MapMode
    if (rMapMode.IsDefault())
        return rDevicePt;

    // calculate MapMode-resolution and convert
    ImplMapRes aMapRes;
    aMapRes.CalcMapResolution(rMapMode, GetDPIX(), GetDPIY());

    return Point(lcl_pixelToLogic(rDevicePt.X(), GetDPIX(), aMapRes.mnMapScNumX, aMapRes.mnMapScDenomX)
                     - aMapRes.mnMapOfsX - mnOutOffLogicX,
                 lcl_pixelToLogic(rDevicePt.Y(), GetDPIY(), aMapRes.mnMapScNumY, aMapRes.mnMapScDenomY)
                     - aMapRes.mnMapOfsY - mnOutOffLogicY);
}

Size OutputDevice::ImplPixelToLogic(const Size& rDeviceSize, const MapMode& rMapMode) const
{
    // calculate nothing if default-MapMode
    if (rMapMode.IsDefault())
        return rDeviceSize;

    // calculate MapMode-resolution and convert
    ImplMapRes aMapRes;
    aMapRes.CalcMapResolution(rMapMode, GetDPIX(), GetDPIY());

    return Size(
        lcl_pixelToLogic(rDeviceSize.Width(), GetDPIX(), aMapRes.mnMapScNumX, aMapRes.mnMapScDenomX),
        lcl_pixelToLogic(rDeviceSize.Height(), GetDPIY(), aMapRes.mnMapScNumY, aMapRes.mnMapScDenomY));
}

tools::Rectangle OutputDevice::ImplPixelToLogic(const tools::Rectangle& rDeviceRect,
                                                const MapMode& rMapMode) const
{
    // calculate nothing if default-MapMode
    // tdf#141761 see comments above, IsEmpty() removed
    if (rMapMode.IsDefault())
        return rDeviceRect;

    // calculate MapMode-resolution and convert
    ImplMapRes aMapRes;
    aMapRes.CalcMapResolution(rMapMode, GetDPIX(), GetDPIY());

    tools::Rectangle aRetval(
        lcl_pixelToLogic(rDeviceRect.Left(), GetDPIX(), aMapRes.mnMapScNumX, aMapRes.mnMapScDenomX)
            - aMapRes.mnMapOfsX - mnOutOffLogicX,
        lcl_pixelToLogic(rDeviceRect.Top(), GetDPIY(), aMapRes.mnMapScNumY, aMapRes.mnMapScDenomY)
            - aMapRes.mnMapOfsY - mnOutOffLogicY,
        rDeviceRect.IsWidthEmpty() ? 0
                                   : lcl_pixelToLogic(rDeviceRect.Right(), GetDPIX(),
                                                      aMapRes.mnMapScNumX, aMapRes.mnMapScDenomX)
                                         - aMapRes.mnMapOfsX - mnOutOffLogicX,
        rDeviceRect.IsHeightEmpty() ? 0
                                    : lcl_pixelToLogic(rDeviceRect.Bottom(), GetDPIY(),
                                                       aMapRes.mnMapScNumY, aMapRes.mnMapScDenomY)
                                          - aMapRes.mnMapOfsY - mnOutOffLogicY);

    if (rDeviceRect.IsWidthEmpty())
        aRetval.SetWidthEmpty();

    if (rDeviceRect.IsHeightEmpty())
        aRetval.SetHeightEmpty();

    return aRetval;
}

tools::Polygon OutputDevice::ImplPixelToLogic(const tools::Polygon& rDevicePoly,
                                              const MapMode& rMapMode) const
{
    // calculate nothing if default-MapMode
    if (rMapMode.IsDefault())
        return rDevicePoly;

    // calculate MapMode-resolution and convert
    ImplMapRes aMapRes;
    aMapRes.CalcMapResolution(rMapMode, GetDPIX(), GetDPIY());

    sal_uInt16 i;
    sal_uInt16 nPoints = rDevicePoly.GetSize();
    tools::Polygon aPoly(rDevicePoly);

    // get pointer to Point-array (copy data)
    const Point* pPointAry = aPoly.GetConstPointAry();

    for (i = 0; i < nPoints; i++)
    {
        const Point* pPt = &(pPointAry[i]);
        Point aPt;
        aPt.setX(lcl_pixelToLogic(pPt->X(), GetDPIX(), aMapRes.mnMapScNumX, aMapRes.mnMapScDenomX)
                 - aMapRes.mnMapOfsX - mnOutOffLogicX);
        aPt.setY(lcl_pixelToLogic(pPt->Y(), GetDPIY(), aMapRes.mnMapScNumY, aMapRes.mnMapScDenomY)
                 - aMapRes.mnMapOfsY - mnOutOffLogicY);
        aPoly[i] = aPt;
    }

    return aPoly;
}

basegfx::B2DPolygon OutputDevice::ImplPixelToLogic(const basegfx::B2DPolygon& rPixelPoly,
                                                   const MapMode& rMapMode) const
{
    basegfx::B2DPolygon aTransformedPoly = rPixelPoly;
    const basegfx::B2DHomMatrix aTransformationMatrix = GetInverseViewTransformation(rMapMode);
    aTransformedPoly.transform(aTransformationMatrix);
    return aTransformedPoly;
}

basegfx::B2DPolyPolygon
OutputDevice::ImplPixelToLogic(const basegfx::B2DPolyPolygon& rPixelPolyPoly,
                               const MapMode& rMapMode) const
{
    basegfx::B2DPolyPolygon aTransformedPoly = rPixelPolyPoly;
    const basegfx::B2DHomMatrix aTransformationMatrix = GetInverseViewTransformation(rMapMode);
    aTransformedPoly.transform(aTransformationMatrix);
    return aTransformedPoly;
}

static ImplMapRes lcl_resolveMapRes(const MapMode* pMode, const MapMode& rDefaultMapMode,
                                    const ImplMapRes& rDefaultMapRes, bool bMap, tools::Long nDPIX,
                                    tools::Long nDPIY)
{
    const MapMode* pEffectiveMode = pMode ? pMode : &rDefaultMapMode;

    if (!bMap || pEffectiveMode != &rDefaultMapMode)
    {
        if (pEffectiveMode->GetMapUnit() == MapUnit::MapRelative)
            return rDefaultMapRes;

        ImplMapRes aRes;
        aRes.CalcMapResolution(*pEffectiveMode, nDPIX, nDPIY);
        return aRes;
    }

    return rDefaultMapRes;
}

tools::Long OutputDevice::ImplLogicWidthToDevicePixel(tools::Long nWidth) const
{
    if (!mbMap)
        return nWidth;

    return lcl_logicToPixel(nWidth, GetDPIX(), maMapRes.mnMapScNumX, maMapRes.mnMapScDenomX);
}

Point OutputDevice::ImplLogicToDevicePixel(const Point& rLogicPt) const
{
    if (!mbMap)
        return Point(rLogicPt.X() + GetOutOffXPixel(), rLogicPt.Y() + GetOutOffYPixel());

    return Point(lcl_logicToPixel(rLogicPt.X() + maMapRes.mnMapOfsX, GetDPIX(), maMapRes.mnMapScNumX,
                                  maMapRes.mnMapScDenomX)
                     + GetOutOffXPixel() + mnOutOffOrigX,
                 lcl_logicToPixel(rLogicPt.Y() + maMapRes.mnMapOfsY, GetDPIY(), maMapRes.mnMapScNumY,
                                  maMapRes.mnMapScDenomY)
                     + GetOutOffYPixel() + mnOutOffOrigY);
}

tools::Rectangle OutputDevice::ImplLogicToDevicePixel(const tools::Rectangle& rLogicRect) const
{
    // tdf#141761 IsEmpty() removed
    // Even if rLogicRect.IsEmpty(), transform of the Position contained
    // in the Rectangle is necessary. Due to Rectangle::Right() returning
    // Left() when IsEmpty(), the code *could* stay unchanged (same for Bottom),
    // but:
    // The Rectangle constructor used with the four tools::Long values does not
    // check for IsEmpty(), so to keep that state correct there are two possibilities:
    // (1) Add a test to the Rectangle constructor in question
    // (2) Do it by hand here
    // I have tried (1) first, but test Test::test_rectangle() claims that for
    //  tools::Rectangle aRect(1, 1, 1, 1);
    //    tools::Long(1) == aRect.GetWidth()
    //    tools::Long(0) == aRect.getWidth()
    // (remember: this means Left == Right == 1 -> GetWidth => 1, getWidth == 0)
    // so indeed the 1's have to go uncommented/unchecked into the data body
    // of rectangle. Switching to (2) *is* needed, doing so
    tools::Rectangle aRetval;

    if (!mbMap)
    {
        aRetval
            = tools::Rectangle(rLogicRect.Left() + GetOutOffXPixel(), rLogicRect.Top() + GetOutOffYPixel(),
                               rLogicRect.IsWidthEmpty() ? 0 : rLogicRect.Right() + GetOutOffXPixel(),
                               rLogicRect.IsHeightEmpty() ? 0 : rLogicRect.Bottom() + GetOutOffYPixel());
    }
    else
    {
        aRetval = tools::Rectangle(
            lcl_logicToPixel(rLogicRect.Left() + maMapRes.mnMapOfsX, GetDPIX(), maMapRes.mnMapScNumX,
                             maMapRes.mnMapScDenomX)
                + GetOutOffXPixel() + mnOutOffOrigX,
            lcl_logicToPixel(rLogicRect.Top() + maMapRes.mnMapOfsY, GetDPIY(), maMapRes.mnMapScNumY,
                             maMapRes.mnMapScDenomY)
                + GetOutOffYPixel() + mnOutOffOrigY,
            rLogicRect.IsWidthEmpty()
                ? 0
                : lcl_logicToPixel(rLogicRect.Right() + maMapRes.mnMapOfsX, GetDPIX(),
                                   maMapRes.mnMapScNumX, maMapRes.mnMapScDenomX)
                      + GetOutOffXPixel() + mnOutOffOrigX,
            rLogicRect.IsHeightEmpty()
                ? 0
                : lcl_logicToPixel(rLogicRect.Bottom() + maMapRes.mnMapOfsY, GetDPIY(),
                                   maMapRes.mnMapScNumY, maMapRes.mnMapScDenomY)
                      + GetOutOffYPixel() + mnOutOffOrigY);
    }

    if (rLogicRect.IsWidthEmpty())
        aRetval.SetWidthEmpty();

    if (rLogicRect.IsHeightEmpty())
        aRetval.SetHeightEmpty();

    return aRetval;
}

Point OutputDevice::ImplLogicToPixel(const Point& rLogicPt) const
{
    if (!mbMap)
        return rLogicPt;

    return Point(lcl_logicToPixel(rLogicPt.X() + maMapRes.mnMapOfsX, GetDPIX(), maMapRes.mnMapScNumX,
                                  maMapRes.mnMapScDenomX)
                     + mnOutOffOrigX,
                 lcl_logicToPixel(rLogicPt.Y() + maMapRes.mnMapOfsY, GetDPIY(), maMapRes.mnMapScNumY,
                                  maMapRes.mnMapScDenomY)
                     + mnOutOffOrigY);
}

Size OutputDevice::ImplLogicToPixel(const Size& rLogicSize) const
{
    if (!mbMap)
        return rLogicSize;

    return Size(
        lcl_logicToPixel(rLogicSize.Width(), GetDPIX(), maMapRes.mnMapScNumX, maMapRes.mnMapScDenomX),
        lcl_logicToPixel(rLogicSize.Height(), GetDPIY(), maMapRes.mnMapScNumY,
                         maMapRes.mnMapScDenomY));
}

tools::Rectangle OutputDevice::ImplLogicToPixel(const tools::Rectangle& rLogicRect) const
{
    // tdf#141761 see comments above, IsEmpty() removed
    if (!mbMap)
        return rLogicRect;

    tools::Rectangle aRetval(lcl_logicToPixel(rLogicRect.Left() + maMapRes.mnMapOfsX, GetDPIX(),
                                              maMapRes.mnMapScNumX, maMapRes.mnMapScDenomX)
                                 + mnOutOffOrigX,
                             lcl_logicToPixel(rLogicRect.Top() + maMapRes.mnMapOfsY, GetDPIY(),
                                              maMapRes.mnMapScNumY, maMapRes.mnMapScDenomY)
                                 + mnOutOffOrigY,
                             rLogicRect.IsWidthEmpty()
                                 ? 0
                                 : lcl_logicToPixel(rLogicRect.Right() + maMapRes.mnMapOfsX, GetDPIX(),
                                                    maMapRes.mnMapScNumX, maMapRes.mnMapScDenomX)
                                       + mnOutOffOrigX,
                             rLogicRect.IsHeightEmpty()
                                 ? 0
                                 : lcl_logicToPixel(rLogicRect.Bottom() + maMapRes.mnMapOfsY,
                                                    GetDPIY(), maMapRes.mnMapScNumY,
                                                    maMapRes.mnMapScDenomY)
                                       + mnOutOffOrigY);

    if (rLogicRect.IsWidthEmpty())
        aRetval.SetWidthEmpty();

    if (rLogicRect.IsHeightEmpty())
        aRetval.SetHeightEmpty();

    return aRetval;
}

tools::Polygon OutputDevice::ImplLogicToPixel(const tools::Polygon& rLogicPoly) const
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
        aPt.setX(lcl_logicToPixel(pPt->X() + maMapRes.mnMapOfsX, GetDPIX(), maMapRes.mnMapScNumX,
                                  maMapRes.mnMapScDenomX)
                 + mnOutOffOrigX);
        aPt.setY(lcl_logicToPixel(pPt->Y() + maMapRes.mnMapOfsY, GetDPIY(), maMapRes.mnMapScNumY,
                                  maMapRes.mnMapScDenomY)
                 + mnOutOffOrigY);
        aPoly[i] = aPt;
    }

    return aPoly;
}

tools::PolyPolygon OutputDevice::ImplLogicToPixel(const tools::PolyPolygon& rLogicPolyPoly) const
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

basegfx::B2DPolyPolygon
OutputDevice::ImplLogicToPixel(const basegfx::B2DPolyPolygon& rLogicPolyPoly) const
{
    basegfx::B2DPolyPolygon aTransformedPoly = rLogicPolyPoly;
    const basegfx::B2DHomMatrix aTransformationMatrix = GetViewTransformation();
    aTransformedPoly.transform(aTransformationMatrix);
    return aTransformedPoly;
}

vcl::Region OutputDevice::ImplLogicToPixel(const vcl::Region& rLogicRegion) const
{
    if (!mbMap || rLogicRegion.IsNull() || rLogicRegion.IsEmpty())
    {
        return rLogicRegion;
    }

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
        RectangleVector aRectangles;
        rLogicRegion.GetRegionRectangles(aRectangles);
        const RectangleVector& rRectangles(aRectangles); // needed to make the '!=' work

        // make reverse run to fill new region bottom-up, this will speed it up due to the used data structuring
        for (RectangleVector::const_reverse_iterator aRectIter(rRectangles.rbegin());
             aRectIter != rRectangles.rend(); ++aRectIter)
        {
            aRegion.Union(LogicToPixel(*aRectIter));
        }
    }

    return aRegion;
}

Point OutputDevice::ImplLogicToPixel(const Point& rLogicPt, const MapMode& rMapMode) const
{
    if (rMapMode.IsDefault())
        return rLogicPt;

    // convert MapMode resolution and convert
    ImplMapRes aMapRes;
    aMapRes.CalcMapResolution(rMapMode, GetDPIX(), GetDPIY());

    return Point(lcl_logicToPixel(rLogicPt.X() + aMapRes.mnMapOfsX, GetDPIX(), aMapRes.mnMapScNumX,
                                  aMapRes.mnMapScDenomX)
                     + mnOutOffOrigX,
                 lcl_logicToPixel(rLogicPt.Y() + aMapRes.mnMapOfsY, GetDPIY(), aMapRes.mnMapScNumY,
                                  aMapRes.mnMapScDenomY)
                     + mnOutOffOrigY);
}

Size OutputDevice::ImplLogicToPixel(const Size& rLogicSize, const MapMode& rMapMode) const
{
    if (rMapMode.IsDefault())
        return rLogicSize;

    // convert MapMode resolution and convert
    ImplMapRes aMapRes;
    aMapRes.CalcMapResolution(rMapMode, GetDPIX(), GetDPIY());

    return Size(
        lcl_logicToPixel(rLogicSize.Width(), GetDPIX(), aMapRes.mnMapScNumX, aMapRes.mnMapScDenomX),
        lcl_logicToPixel(rLogicSize.Height(), GetDPIY(), aMapRes.mnMapScNumY, aMapRes.mnMapScDenomY));
}

tools::Rectangle OutputDevice::ImplLogicToPixel(const tools::Rectangle& rLogicRect,
                                                const MapMode& rMapMode) const
{
    // tdf#141761 see comments above, IsEmpty() removed
    if (rMapMode.IsDefault())
        return rLogicRect;

    // convert MapMode resolution and convert
    ImplMapRes aMapRes;
    aMapRes.CalcMapResolution(rMapMode, GetDPIX(), GetDPIY());

    tools::Rectangle aRetval(lcl_logicToPixel(rLogicRect.Left() + aMapRes.mnMapOfsX, GetDPIX(),
                                              aMapRes.mnMapScNumX, aMapRes.mnMapScDenomX)
                                 + mnOutOffOrigX,
                             lcl_logicToPixel(rLogicRect.Top() + aMapRes.mnMapOfsY, GetDPIY(),
                                              aMapRes.mnMapScNumY, aMapRes.mnMapScDenomY)
                                 + mnOutOffOrigY,
                             rLogicRect.IsWidthEmpty()
                                 ? 0
                                 : lcl_logicToPixel(rLogicRect.Right() + aMapRes.mnMapOfsX, GetDPIX(),
                                                    aMapRes.mnMapScNumX, aMapRes.mnMapScDenomX)
                                       + mnOutOffOrigX,
                             rLogicRect.IsHeightEmpty()
                                 ? 0
                                 : lcl_logicToPixel(rLogicRect.Bottom() + aMapRes.mnMapOfsY, GetDPIY(),
                                                    aMapRes.mnMapScNumY, aMapRes.mnMapScDenomY)
                                       + mnOutOffOrigY);

    if (rLogicRect.IsWidthEmpty())
        aRetval.SetWidthEmpty();

    if (rLogicRect.IsHeightEmpty())
        aRetval.SetHeightEmpty();

    return aRetval;
}

tools::Polygon OutputDevice::ImplLogicToPixel(const tools::Polygon& rLogicPoly,
                                              const MapMode& rMapMode) const
{
    if (rMapMode.IsDefault())
        return rLogicPoly;

    // convert MapMode resolution and convert
    ImplMapRes aMapRes;
    aMapRes.CalcMapResolution(rMapMode, GetDPIX(), GetDPIY());

    sal_uInt16 i;
    sal_uInt16 nPoints = rLogicPoly.GetSize();
    tools::Polygon aPoly(rLogicPoly);

    // get pointer to Point-array (copy data)
    const Point* pPointAry = aPoly.GetConstPointAry();

    for (i = 0; i < nPoints; i++)
    {
        const Point* pPt = &(pPointAry[i]);
        Point aPt;
        aPt.setX(lcl_logicToPixel(pPt->X() + aMapRes.mnMapOfsX, GetDPIX(), aMapRes.mnMapScNumX,
                                  aMapRes.mnMapScDenomX)
                 + mnOutOffOrigX);
        aPt.setY(lcl_logicToPixel(pPt->Y() + aMapRes.mnMapOfsY, GetDPIY(), aMapRes.mnMapScNumY,
                                  aMapRes.mnMapScDenomY)
                 + mnOutOffOrigY);
        aPoly[i] = aPt;
    }

    return aPoly;
}

basegfx::B2DPolyPolygon
OutputDevice::ImplLogicToPixel(const basegfx::B2DPolyPolygon& rLogicPolyPoly,
                               const MapMode& rMapMode) const
{
    basegfx::B2DPolyPolygon aTransformedPoly = rLogicPolyPoly;
    const basegfx::B2DHomMatrix aTransformationMatrix = GetViewTransformation(rMapMode);
    aTransformedPoly.transform(aTransformationMatrix);
    return aTransformedPoly;
}

Point OutputDevice::ImplLogicToLogic(const Point& rPtSource, const MapMode* pMapModeSource,
                                     const MapMode* pMapModeDest) const
{
    const MapMode* pSrc = pMapModeSource ? pMapModeSource : &maMapMode;
    const MapMode* pDst = pMapModeDest ? pMapModeDest : &maMapMode;

    if (*pSrc == *pDst)
        return rPtSource;

    ImplMapRes aMapResSource
        = lcl_resolveMapRes(pMapModeSource, maMapMode, maMapRes, mbMap, GetDPIX(), GetDPIY());
    ImplMapRes aMapResDest
        = lcl_resolveMapRes(pMapModeDest, maMapMode, maMapRes, mbMap, GetDPIX(), GetDPIY());

    return Point(lcl_scaleLogicValue(rPtSource.X() + aMapResSource.mnMapOfsX,
                                     aMapResSource.mnMapScNumX, aMapResDest.mnMapScDenomX,
                                     aMapResSource.mnMapScDenomX, aMapResDest.mnMapScNumX)
                     - aMapResDest.mnMapOfsX,
                 lcl_scaleLogicValue(rPtSource.Y() + aMapResSource.mnMapOfsY,
                                     aMapResSource.mnMapScNumY, aMapResDest.mnMapScDenomY,
                                     aMapResSource.mnMapScDenomY, aMapResDest.mnMapScNumY)
                     - aMapResDest.mnMapOfsY);
}

Size OutputDevice::ImplLogicToLogic(const Size& rSzSource, const MapMode* pMapModeSource,
                                    const MapMode* pMapModeDest) const
{
    const MapMode* pSrc = pMapModeSource ? pMapModeSource : &maMapMode;
    const MapMode* pDst = pMapModeDest ? pMapModeDest : &maMapMode;

    if (*pSrc == *pDst)
        return rSzSource;

    ImplMapRes aMapResSource
        = lcl_resolveMapRes(pMapModeSource, maMapMode, maMapRes, mbMap, GetDPIX(), GetDPIY());
    ImplMapRes aMapResDest
        = lcl_resolveMapRes(pMapModeDest, maMapMode, maMapRes, mbMap, GetDPIX(), GetDPIY());

    return Size(lcl_scaleLogicValue(rSzSource.Width(), aMapResSource.mnMapScNumX,
                                    aMapResDest.mnMapScDenomX, aMapResSource.mnMapScDenomX,
                                    aMapResDest.mnMapScNumX),
                lcl_scaleLogicValue(rSzSource.Height(), aMapResSource.mnMapScNumY,
                                    aMapResDest.mnMapScDenomY, aMapResSource.mnMapScDenomY,
                                    aMapResDest.mnMapScNumY));
}

tools::Rectangle OutputDevice::ImplLogicToLogic(const tools::Rectangle& rRectSource,
                                                const MapMode* pMapModeSource,
                                                const MapMode* pMapModeDest) const
{
    const MapMode* pSrc = pMapModeSource ? pMapModeSource : &maMapMode;
    const MapMode* pDst = pMapModeDest ? pMapModeDest : &maMapMode;

    if (*pSrc == *pDst)
        return rRectSource;

    ImplMapRes aMapResSource
        = lcl_resolveMapRes(pMapModeSource, maMapMode, maMapRes, mbMap, GetDPIX(), GetDPIY());
    ImplMapRes aMapResDest
        = lcl_resolveMapRes(pMapModeDest, maMapMode, maMapRes, mbMap, GetDPIX(), GetDPIY());

    return tools::Rectangle(
        lcl_scaleLogicValue(rRectSource.Left() + aMapResSource.mnMapOfsX, aMapResSource.mnMapScNumX,
                            aMapResDest.mnMapScDenomX, aMapResSource.mnMapScDenomX,
                            aMapResDest.mnMapScNumX)
            - aMapResDest.mnMapOfsX,
        lcl_scaleLogicValue(rRectSource.Top() + aMapResSource.mnMapOfsY, aMapResSource.mnMapScNumY,
                            aMapResDest.mnMapScDenomY, aMapResSource.mnMapScDenomY,
                            aMapResDest.mnMapScNumY)
            - aMapResDest.mnMapOfsY,
        lcl_scaleLogicValue(rRectSource.Right() + aMapResSource.mnMapOfsX,
                            aMapResSource.mnMapScNumX, aMapResDest.mnMapScDenomX,
                            aMapResSource.mnMapScDenomX, aMapResDest.mnMapScNumX)
            - aMapResDest.mnMapOfsX,
        lcl_scaleLogicValue(rRectSource.Bottom() + aMapResSource.mnMapOfsY,
                            aMapResSource.mnMapScNumY, aMapResDest.mnMapScDenomY,
                            aMapResSource.mnMapScDenomY, aMapResDest.mnMapScNumY)
            - aMapResDest.mnMapOfsY);
}

// #i75163#
void OutputDevice::ImplInvalidateViewTransform()
{
    if (!mpOutDevData)
        return;

    if (mpOutDevData->mpViewTransform)
    {
        delete mpOutDevData->mpViewTransform;
        mpOutDevData->mpViewTransform = nullptr;
    }

    if (mpOutDevData->mpInverseViewTransform)
    {
        delete mpOutDevData->mpInverseViewTransform;
        mpOutDevData->mpInverseViewTransform = nullptr;
    }
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
