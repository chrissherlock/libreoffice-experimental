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

Size OutputDevice::GetPixelOffset() const { return mpMapper->GetPixelOffset(); }

tools::Long OutputDevice::GetOutputWidthPixel() const { return mpMapper->GetOutputWidthPixel(); }

tools::Long OutputDevice::GetOutputHeightPixel() const { return mpMapper->GetOutputHeightPixel(); }

void OutputDevice::SetOutputWidthPixel(tools::Long nWidth) { mpMapper->SetOutputWidthPixel(nWidth); }

void OutputDevice::SetOutputHeightPixel(tools::Long nHeight) { mpMapper->SetOutputHeightPixel(nHeight); }

Size OutputDevice::GetOutputSizePixel() const { return Size(GetOutputWidthPixel(), GetOutputHeightPixel()); }

tools::Long OutputDevice::GetOutOffXPixel() const { return mpMapper->GetOutOffXPixel(); }

tools::Long OutputDevice::GetOutOffYPixel() const { return mpMapper->GetOutOffYPixel(); }

void OutputDevice::SetDeviceOriginX(tools::Long nOutOffX) { return mpMapper->SetDeviceOriginX(nOutOffX); }

void OutputDevice::SetDeviceOriginY(tools::Long nOutOffY) { return mpMapper->SetDeviceOriginY(nOutOffY); }

Point OutputDevice::GetOutputOffPixel() const { return mpMapper->GetOutputOffPixel(); }

bool OutputDevice::IsMapModeEnabled() const { return mpMapper->IsMapModeEnabled(); }

void OutputDevice::EnableMapMode(bool bEnabled) { mpMapper->EnableMapMode(bEnabled); }

const MapMode& OutputDevice::GetMapMode() const { return mpMapper->GetMapMode(); }

void OutputDevice::SetMapMode()
{
    if (mpMetaFile)
        mpMetaFile->AddAction(new MetaMapModeAction(MapMode()));

    if (!mpMapper->IsMapModeEnabled() && mpMapper->IsDefaultMapMode())
        return;

    mpMapper->EnableMapMode(false);
    mpMapper->ResetMapMode();

    // create new objects (clip region are not re-scaled)
    mbNewFont = true;
    mbInitFont = true;
    ImplInitMapModeObjects();

    // #106426# Adapt logical offset when changing mapmode
    mpMapper->SetLogicalOffset(mpMapper->GetPixelOffset());

    // #i75163#
    mpMapper->InvalidateViewTransform();
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

void OutputDevice::SetMapMode(const MapMode& rNewMapMode)
{
    if (mpMetaFile)
        mpMetaFile->AddAction(new MetaMapModeAction(rNewMapMode));

    bool bRelMap = (rNewMapMode.GetMapUnit() == MapUnit::MapRelative);

    // do nothing if MapMode was not changed
    if (mpMapper->GetMapMode() == rNewMapMode)
        return;

    // if default MapMode calculate nothing
    bool bOldMap = mpMapper->IsMapModeEnabled();
    mpMapper->EnableMapMode(!rNewMapMode.IsDefault());
    if (mpMapper->IsMapModeEnabled())
    {
        // if only the origin is converted, do not scale new
        if ((rNewMapMode.GetMapUnit() == mpMapper->GetMapUnit())
            && (rNewMapMode.GetScaleX() == mpMapper->GetScaleX())
            && (rNewMapMode.GetScaleY() == mpMapper->GetScaleY()) && (bOldMap == mpMapper->IsMapModeEnabled()))
        {
            // set offset
            Point aOrigin = rNewMapMode.GetOrigin();
            mpMapper->SetMappingXOffset(aOrigin.X());
            mpMapper->SetMappingYOffset(aOrigin.Y());
            mpMapper->ResetMapMode(rNewMapMode);

            // #i75163#
            mpMapper->InvalidateViewTransform();

            return;
        }
        if (!bOldMap && bRelMap)
        {
            mpMapper->SetMappingXNumerator(1);
            mpMapper->SetMappingYNumerator(1);
            mpMapper->SetMappingXDenominator(GetDPIX());
            mpMapper->SetMappingYDenominator(GetDPIY());
            mpMapper->SetMappingXOffset(0);
            mpMapper->SetMappingYOffset(0);
        }

        // calculate new MapMode-resolution
        mpMapper->CalcMapResolution(rNewMapMode, GetDPIX(), GetDPIY());
    }

    // set new MapMode
    if (bRelMap)
    {
        mpMapper->SetScaleX(Fraction::MakeFraction(
            mpMapper->GetScaleX().GetNumerator(), rNewMapMode.GetScaleX().GetNumerator(),
            mpMapper->GetScaleX().GetDenominator(), rNewMapMode.GetScaleX().GetDenominator()));

        mpMapper->SetScaleY(Fraction::MakeFraction(
            mpMapper->GetScaleY().GetNumerator(), rNewMapMode.GetScaleY().GetNumerator(),
            mpMapper->GetScaleY().GetDenominator(), rNewMapMode.GetScaleY().GetDenominator()));

        mpMapper->SetOrigin(Point(mpMapper->GetMappingXOffset(), mpMapper->GetMappingYOffset()));
    }
    else
    {
        mpMapper->ResetMapMode(rNewMapMode);
    }

    // create new objects (clip region are not re-scaled)
    mbNewFont = true;
    mbInitFont = true;
    ImplInitMapModeObjects();

    // #106426# Adapt logical offset when changing mapmode
    mpMapper->SetLogicalOffset(Size(lcl_pixelToLogic(mpMapper->GetPixelXOffset(), GetDPIX(), mpMapper->GetMappingXNumerator(), mpMapper->GetMappingXDenominator()),
                                    lcl_pixelToLogic(mpMapper->GetPixelYOffset(), GetDPIY(), mpMapper->GetMappingYNumerator(), mpMapper->GetMappingYDenominator())));

    // #i75163#
    mpMapper->InvalidateViewTransform();
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
    // do nothing if MapMode did not change
    if (mpMapper->GetMapMode() == rNewMapMode)
        return;

    MapUnit eOld = mpMapper->GetMapUnit();
    MapUnit eNew = rNewMapMode.GetMapUnit();

    // a?F = rNewMapMode.GetScale?() / mpMapper->GetMapMode().GetScale?()
    Fraction aXF = Fraction::MakeFraction(
        rNewMapMode.GetScaleX().GetNumerator(), mpMapper->GetScaleX().GetDenominator(),
        rNewMapMode.GetScaleX().GetDenominator(), mpMapper->GetScaleX().GetNumerator());
    Fraction aYF = Fraction::MakeFraction(
        rNewMapMode.GetScaleY().GetNumerator(), mpMapper->GetScaleY().GetDenominator(),
        rNewMapMode.GetScaleY().GetDenominator(), mpMapper->GetScaleY().GetNumerator());

    Point aPt(mpMapper->LogicToLogic(Point(), nullptr, &rNewMapMode));
    if (eNew != eOld)
    {
        SAL_WARN_IF(eOld > MapUnit::MapPixel, "vcl.gdi", "Not implemented MapUnit");
        SAL_WARN_IF(eNew > MapUnit::MapPixel, "vcl.gdi", "Not implemented MapUnit");

        if (eOld <= MapUnit::MapPixel && eNew <= MapUnit::MapPixel)
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
        mpMapper->ResetMapMode(rNewMapMode);

    // #106426# Adapt logical offset when changing MapMode
    mpMapper->SetLogicalOffset(Size(lcl_pixelToLogic(mpMapper->GetPixelXOffset(), GetDPIX(), mpMapper->GetMappingXNumerator(), mpMapper->GetMappingXDenominator()),
                                    lcl_pixelToLogic(mpMapper->GetPixelYOffset(), GetDPIY(), mpMapper->GetMappingYNumerator(), mpMapper->GetMappingYDenominator())));
}

tools::Long OutputDevice::LogicHeightToDevicePixel(tools::Long nHeight) const
{
    return mpMapper->LogicHeightToDevicePixel(nHeight);
}

double OutputDevice::LogicHeightToDeviceSubPixel(tools::Long nHeight) const
{
    return mpMapper->LogicHeightToDeviceSubPixel(nHeight);
}

Point OutputDevice::SubPixelToLogic(const basegfx::B2DPoint& rDevicePt) const
{
    return mpMapper->SubPixelToLogic(rDevicePt);
}

void OutputDevice::SetOffset(const Size& rOffset)
{
    mpMapper->SetOffset(rOffset);
}

double OutputDevice::LogicWidthToDeviceSubPixel(tools::Long nWidth) const
{
    return mpMapper->LogicWidthToDeviceSubPixel(nWidth);
}

basegfx::B2DPoint OutputDevice::LogicToDeviceSubPixel(const Point& rPoint) const
{
    return mpMapper->LogicToDeviceSubPixel(rPoint);
}

tools::Long OutputDevice::LogicXToDevicePixel(tools::Long nX) const
{
    return mpMapper->LogicXToDevicePixel(nX);
}

tools::Long OutputDevice::LogicYToDevicePixel(tools::Long nX) const
{
    return mpMapper->LogicYToDevicePixel(nX);
}

tools::Rectangle OutputDevice::LogicToLogic(const tools::Rectangle& rRectSource,
                                            const MapMode* pMapModeSource,
                                            const MapMode* pMapModeDest) const
{
    return mpMapper->LogicToLogic(rRectSource, pMapModeSource, pMapModeDest);
}

Size OutputDevice::LogicToLogic(const Size& rSzSource, const MapMode* pMapModeSource,
                                const MapMode* pMapModeDest) const
{
    return mpMapper->LogicToLogic(rSzSource, pMapModeSource, pMapModeDest);
}

Point OutputDevice::LogicToLogic(const Point& rPtSource, const MapMode* pMapModeSource,
                                 const MapMode* pMapModeDest) const
{
    return mpMapper->LogicToLogic(rPtSource, pMapModeSource, pMapModeDest);
}

Point OutputDevice::LogicToPixel(const Point& rLogicPt) const { return mpMapper->LogicToPixel(rLogicPt); }

Size OutputDevice::LogicToPixel(const Size& rLogicSize) const
{
    return mpMapper->LogicToPixel(rLogicSize);
}

tools::Rectangle OutputDevice::LogicToPixel(const tools::Rectangle& rLogicRect) const
{
    return mpMapper->LogicToPixel(rLogicRect);
}

tools::Polygon OutputDevice::LogicToPixel(const tools::Polygon& rLogicPoly) const
{
    return mpMapper->LogicToPixel(rLogicPoly);
}

tools::PolyPolygon OutputDevice::LogicToPixel(const tools::PolyPolygon& rLogicPolyPoly) const
{
    return mpMapper->LogicToPixel(rLogicPolyPoly);
}

basegfx::B2DPolyPolygon
OutputDevice::LogicToPixel(const basegfx::B2DPolyPolygon& rLogicPolyPoly) const
{
    return mpMapper->LogicToPixel(rLogicPolyPoly);
}

vcl::Region OutputDevice::LogicToPixel(const vcl::Region& rLogicRegion) const
{
    return mpMapper->LogicToPixel(rLogicRegion);
}

Point OutputDevice::LogicToPixel(const Point& rLogicPt, const MapMode& rMapMode) const
{
    return mpMapper->LogicToPixel(rLogicPt, rMapMode);
}

Size OutputDevice::LogicToPixel(const Size& rLogicSize, const MapMode& rMapMode) const
{
    return mpMapper->LogicToPixel(rLogicSize, rMapMode);
}

tools::Rectangle OutputDevice::LogicToPixel(const tools::Rectangle& rLogicRect,
                                            const MapMode& rMapMode) const
{
    return mpMapper->LogicToPixel(rLogicRect, rMapMode);
}

tools::Polygon OutputDevice::LogicToPixel(const tools::Polygon& rLogicPoly,
                                          const MapMode& rMapMode) const
{
    return mpMapper->LogicToPixel(rLogicPoly, rMapMode);
}

basegfx::B2DPolyPolygon OutputDevice::LogicToPixel(const basegfx::B2DPolyPolygon& rLogicPolyPoly,
                                                   const MapMode& rMapMode) const
{
    return mpMapper->LogicToPixel(rLogicPolyPoly, rMapMode);
}

Point OutputDevice::PixelToLogic(const Point& rDevicePt) const
{
    return mpMapper->PixelToLogic(rDevicePt);
}

Size OutputDevice::PixelToLogic(const Size& rDeviceSize) const
{
    return mpMapper->PixelToLogic(rDeviceSize);
}

tools::Rectangle OutputDevice::PixelToLogic(const tools::Rectangle& rDeviceRect) const
{
    return mpMapper->PixelToLogic(rDeviceRect);
}

tools::Polygon OutputDevice::PixelToLogic(const tools::Polygon& rDevicePoly) const
{
    return mpMapper->PixelToLogic(rDevicePoly);
}

tools::PolyPolygon OutputDevice::PixelToLogic(const tools::PolyPolygon& rDevicePolyPoly) const
{
    return mpMapper->PixelToLogic(rDevicePolyPoly);
}

basegfx::B2DPolyPolygon
OutputDevice::PixelToLogic(const basegfx::B2DPolyPolygon& rPixelPolyPoly) const
{
    return mpMapper->PixelToLogic(rPixelPolyPoly);
}

basegfx::B2DRectangle OutputDevice::PixelToLogic(const basegfx::B2DRectangle& rDeviceRect) const
{
    return mpMapper->PixelToLogic(rDeviceRect);
}

vcl::Region OutputDevice::PixelToLogic(const vcl::Region& rDeviceRegion) const
{
    return mpMapper->PixelToLogic(rDeviceRegion);
}

Point OutputDevice::PixelToLogic(const Point& rDevicePt, const MapMode& rMapMode) const
{
    return mpMapper->PixelToLogic(rDevicePt, rMapMode);
}

Size OutputDevice::PixelToLogic(const Size& rDeviceSize, const MapMode& rMapMode) const
{
    return mpMapper->PixelToLogic(rDeviceSize, rMapMode);
}

tools::Rectangle OutputDevice::PixelToLogic(const tools::Rectangle& rDeviceRect,
                                            const MapMode& rMapMode) const
{
    return mpMapper->PixelToLogic(rDeviceRect, rMapMode);
}

tools::Polygon OutputDevice::PixelToLogic(const tools::Polygon& rDevicePoly,
                                          const MapMode& rMapMode) const
{
    return mpMapper->PixelToLogic(rDevicePoly, rMapMode);
}

basegfx::B2DPolygon OutputDevice::PixelToLogic(const basegfx::B2DPolygon& rPixelPoly,
                                               const MapMode& rMapMode) const
{
    return mpMapper->PixelToLogic(rPixelPoly, rMapMode);
}

basegfx::B2DPolyPolygon OutputDevice::PixelToLogic(const basegfx::B2DPolyPolygon& rPixelPolyPoly,
                                                   const MapMode& rMapMode) const
{
    return mpMapper->PixelToLogic(rPixelPolyPoly, rMapMode);
}

tools::Long OutputDevice::LogicWidthToDevicePixel(tools::Long nWidth) const
{
    return mpMapper->LogicWidthToDevicePixel(nWidth);
}

Point OutputDevice::LogicToDevicePixel(const Point& rLogicPt) const
{
    return mpMapper->LogicToDevicePixel(rLogicPt);
}

tools::Rectangle OutputDevice::LogicToDevicePixel(const tools::Rectangle& rLogicRect) const
{
    return mpMapper->LogicToDevicePixel(rLogicRect);
}

tools::Long OutputDevice::DevicePixelToLogicWidth(tools::Long nWidth) const
{
    return mpMapper->DevicePixelToLogicWidth(nWidth);
}

tools::Long OutputDevice::DevicePixelToLogicHeight(tools::Long nHeight) const
{
    return mpMapper->DevicePixelToLogicHeight(nHeight);
}

tools::Rectangle OutputDevice::DevicePixelToLogic(const tools::Rectangle& rPixelRect) const
{
    return mpMapper->DevicePixelToLogic(rPixelRect);
}

vcl::Region OutputDevice::PixelToDevicePixel(const vcl::Region& rRegion) const
{
    return mpMapper->PixelToDevicePixel(rRegion);
}

Size OutputDevice::LogicToDevicePixel(const Size& rLogicSize) const
{
    return mpMapper->LogicToDevicePixel(rLogicSize);
}

void OutputDevice::ImplInitMapModeObjects() {}

basegfx::B2DHomMatrix OutputDevice::GetViewTransformation() const
{
    return mpMapper->GetViewTransformation();
}

basegfx::B2DHomMatrix OutputDevice::GetViewTransformation(const MapMode& rMapMode) const
{
    return mpMapper->GetViewTransformation(rMapMode);
}

basegfx::B2DHomMatrix OutputDevice::GetInverseViewTransformation() const
{
    return mpMapper->GetInverseViewTransformation();
}

basegfx::B2DHomMatrix OutputDevice::GetInverseViewTransformation(const MapMode& rMapMode) const
{
    return mpMapper->GetInverseViewTransformation(rMapMode);
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
