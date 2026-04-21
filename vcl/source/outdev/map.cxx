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

const CoordinateMapper& OutputDevice::GetMapper() const { return *mpMapper; }

tools::Long OutputDevice::GetOutputWidthPixel() const { return mpMapper->GetOutputWidthPixel(); }

tools::Long OutputDevice::GetOutputHeightPixel() const { return mpMapper->GetOutputHeightPixel(); }

void OutputDevice::SetOutputWidthPixel(tools::Long nWidth) { mpMapper->SetOutputWidthPixel(nWidth); }

void OutputDevice::SetOutputHeightPixel(tools::Long nHeight) { mpMapper->SetOutputHeightPixel(nHeight); }

Size OutputDevice::GetOutputSizePixel() const { return Size(GetOutputWidthPixel(), GetOutputHeightPixel()); }

tools::Long OutputDevice::GetDeviceOriginX() const { return mpMapper->GetDeviceToWindowOffsetX(); }

tools::Long OutputDevice::GetDeviceOriginY() const { return mpMapper->GetDeviceToWindowOffsetY(); }

void OutputDevice::SetDeviceOriginX(tools::Long nOutOffX) { return mpMapper->SetDeviceToWindowOffsetX(nOutOffX); }

void OutputDevice::SetDeviceOriginY(tools::Long nOutOffY) { return mpMapper->SetDeviceToWindowOffsetY(nOutOffY); }

Point OutputDevice::GetOutputOffPixel() const { return mpMapper->GetDeviceToWindowOffset(); }

Size OutputDevice::GetPixelOffset() const { return mpMapper->GetWindowToViewOffset(); }

bool OutputDevice::IsMapModeEnabled() const { return mpMapper->IsMapModeEnabled(); }

void OutputDevice::EnableMapMode(bool bEnabled) { mpMapper->EnableMapMode(bEnabled); }

const MapMode& OutputDevice::GetMapMode() const { return mpMapper->GetMapMode(); }

void OutputDevice::SetPixelOffset(const Size& rOffset)
{
    mpMapper->SetWindowToViewOffset(rOffset);
}

void OutputDevice::SetMapMode()
{
    if ( mpMetaFile )
        mpMetaFile->AddAction( new MetaMapModeAction( MapMode() ) );

    if (!mpMapper->IsMapModeEnabled() && mpMapper->IsDefaultMapMode())
        return;

    mpMapper->EnableMapMode(false);
    mpMapper->ResetMapMode();

    // create new objects (clip region are not re-scaled)
    mbNewFont   = true;
    mbInitFont  = true;
    ImplInitMapModeObjects();

    // #106426# Adapt logical offset when changing mapmode
    mpMapper->SetLogicToAbsoluteOffset(mpMapper->GetWindowToViewOffset());

    // #i75163#
    mpMapper->InvalidateViewTransform();
}

void OutputDevice::SetMapMode( const MapMode& rNewMapMode )
{

    bool bRelMap = (rNewMapMode.GetMapUnit() == MapUnit::MapRelative);

    if ( mpMetaFile )
    {
        mpMetaFile->AddAction( new MetaMapModeAction( rNewMapMode ) );
    }

    // do nothing if MapMode was not changed
    if (mpMapper->GetMapMode() == rNewMapMode)
        return;

     // if default MapMode calculate nothing
    bool bOldMap = mpMapper->IsMapModeEnabled();
    mpMapper->EnableMapMode(!rNewMapMode.IsDefault());
    if ( mpMapper->IsMapModeEnabled() )
    {
        // if only the origin is converted, do not scale new
        if ( (rNewMapMode.GetMapUnit() == mpMapper->GetMapUnit()) &&
             (rNewMapMode.GetScaleX()  == mpMapper->GetScaleX())  &&
             (rNewMapMode.GetScaleY()  == mpMapper->GetScaleY())  &&
             (bOldMap                  == mpMapper->IsMapModeEnabled()) )
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
        if ( !bOldMap && bRelMap )
        {
            mpMapper->SetMapResolutionScaleX(1.0 / mpMapper->GetDPIX());
            mpMapper->SetMapResolutionScaleY(1.0 / mpMapper->GetDPIY());
            mpMapper->SetMappingXOffset(0);
            mpMapper->SetMappingYOffset(0);
        }

        // calculate new MapMode-resolution
        mpMapper->CalcMapResolution(rNewMapMode, mpMapper->GetDPIX(), mpMapper->GetDPIY());
    }

    // set new MapMode
    if (bRelMap)
    {
        mpMapper->SetScaleX(mpMapper->GetScaleX() * rNewMapMode.GetScaleX());
        mpMapper->SetScaleY(mpMapper->GetScaleY() * rNewMapMode.GetScaleY());
        mpMapper->SetOrigin(Point(mpMapper->GetMappingXOffset(), mpMapper->GetMappingYOffset()));
    }
    else
    {
        mpMapper->ResetMapMode(rNewMapMode);
    }

    // create new objects (clip region are not re-scaled)
    mbNewFont   = true;
    mbInitFont  = true;
    ImplInitMapModeObjects();

    // #106426# Adapt logical offset when changing mapmode
    mpMapper->SetLogicToAbsoluteOffset(Size(mpMapper->ViewToLogicDistanceX(mpMapper->GetWindowToViewOffsetX()),
                                    mpMapper->ViewToLogicDistanceY(mpMapper->GetWindowToViewOffsetY())));

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

void OutputDevice::ImplInitMapModeObjects() {}

void OutputDevice::SetRelativeMapMode( const MapMode& rNewMapMode )
{
    // do nothing if MapMode did not change
    if (mpMapper->GetMapMode() == rNewMapMode)
        return;

    MapUnit eOld = mpMapper->GetMapUnit();
    MapUnit eNew = rNewMapMode.GetMapUnit();

    double fXF = rNewMapMode.GetScaleX() / mpMapper->GetScaleX();
    double fYF = rNewMapMode.GetScaleY() / mpMapper->GetScaleY();

    Point aPt( LogicToLogic( Point(), nullptr, &rNewMapMode ) );
    if ( eNew != eOld )
    {
        if ( eOld > MapUnit::MapPixel )
        {
            SAL_WARN( "vcl.gdi", "Not implemented MapUnit" );
        }
        else if ( eNew > MapUnit::MapPixel )
        {
            SAL_WARN( "vcl.gdi", "Not implemented MapUnit" );
        }
        else
        {
            const auto eFrom = MapToO3tlLength(eOld, o3tl::Length::in);
            const auto eTo = MapToO3tlLength(eNew, o3tl::Length::in);
            const auto [mul, div] = o3tl::getConversionMulDiv(eFrom, eTo);
            double aF = double(div) / mul;

            // a?F =  a?F * aF
            fXF = fXF * aF;
            fYF = fYF * aF;
            if ( eOld == MapUnit::MapPixel )
            {
                fXF *= mpMapper->GetDPIX();
                fYF *= mpMapper->GetDPIY();
            }
            else if ( eNew == MapUnit::MapPixel )
            {
                fXF /= mpMapper->GetDPIX();
                fYF /= mpMapper->GetDPIY();
            }
        }
    }

    MapMode aNewMapMode( MapUnit::MapRelative, Point( -aPt.X(), -aPt.Y() ), fXF, fYF );
    SetMapMode( aNewMapMode );

    if ( eNew != eOld )
        mpMapper->ResetMapMode(rNewMapMode);

    // #106426# Adapt logical offset when changing MapMode
    mpMapper->SetLogicToAbsoluteOffset(Size(mpMapper->ViewToLogicDistanceX(mpMapper->GetWindowToViewOffsetX()),
                                    mpMapper->ViewToLogicDistanceY(mpMapper->GetWindowToViewOffsetY())));
}

tools::Long OutputDevice::LogicWidthToDevicePixel(tools::Long nWidth) const
{
    return mpMapper->LogicWidthToDevicePixel(nWidth);
}

tools::Long OutputDevice::LogicHeightToDevicePixel(tools::Long nHeight) const
{
    return mpMapper->LogicHeightToDevicePixel(nHeight);
}

Point OutputDevice::LogicToPixel(const Point& rLogicPt) const
{
    return mpMapper->LogicToWindowUnits(rLogicPt);
}

Size OutputDevice::LogicToPixel(const Size& rLogicSize) const
{
    return mpMapper->LogicToWindowUnits(rLogicSize);
}

tools::Rectangle OutputDevice::LogicToPixel(const tools::Rectangle& rLogicRect) const
{
    return mpMapper->LogicToWindowUnits(rLogicRect);
}

tools::Polygon OutputDevice::LogicToPixel(const tools::Polygon& rLogicPoly) const
{
    return mpMapper->LogicToWindowUnits(rLogicPoly);
}

tools::PolyPolygon OutputDevice::LogicToPixel(const tools::PolyPolygon& rLogicPolyPoly) const
{
    return mpMapper->LogicToWindowUnits(rLogicPolyPoly);
}

basegfx::B2DPolyPolygon OutputDevice::LogicToPixel(const basegfx::B2DPolyPolygon& rLogicPolyPoly) const
{
    return mpMapper->LogicToWindowUnits(rLogicPolyPoly);
}

vcl::Region OutputDevice::LogicToPixel(const vcl::Region& rLogicRegion) const
{
    return mpMapper->LogicToWindowUnits(rLogicRegion);
}

Point OutputDevice::LogicToPixel(const Point& rLogicPt, const MapMode& rMapMode) const
{
    return mpMapper->LogicToWindowUnits(rLogicPt, rMapMode);
}

Size OutputDevice::LogicToPixel(const Size& rLogicSize, const MapMode& rMapMode) const
{
    return mpMapper->LogicToWindowUnits(rLogicSize, rMapMode);
}

tools::Rectangle OutputDevice::LogicToPixel(const tools::Rectangle& rLogicRect, const MapMode& rMapMode) const
{
    return mpMapper->LogicToWindowUnits(rLogicRect, rMapMode);
}

tools::Polygon OutputDevice::LogicToPixel(const tools::Polygon& rLogicPoly, const MapMode& rMapMode) const
{
    return mpMapper->LogicToWindowUnits(rLogicPoly, rMapMode);
}

basegfx::B2DPolyPolygon OutputDevice::LogicToPixel(const basegfx::B2DPolyPolygon& rLogicPolyPoly,
                                                   const MapMode& rMapMode) const
{
    return mpMapper->LogicToWindowUnits(rLogicPolyPoly, rMapMode);
}

tools::Long OutputDevice::DevicePixelToLogicWidth(tools::Long nWidth) const
{
    return mpMapper->DevicePixelToLogicWidth(nWidth);
}

tools::Long OutputDevice::DevicePixelToLogicHeight(tools::Long nHeight) const
{
    return mpMapper->DevicePixelToLogicHeight(nHeight);
}

vcl::Region OutputDevice::PixelToLogic(const vcl::Region& rDeviceRegion) const
{
    return mpMapper->WindowToLogicUnits(rDeviceRegion);
}

Point OutputDevice::PixelToLogic(const Point& rDevicePt) const
{
    return mpMapper->WindowToLogicUnits(rDevicePt);
}

Point OutputDevice::SubPixelToLogic(const basegfx::B2DPoint& rDevicePt) const
{
    return mpMapper->WindowSubPixelToLogicUnits(rDevicePt);
}

Size OutputDevice::PixelToLogic(const Size& rDeviceSize) const
{
    return mpMapper->WindowToLogicUnits(rDeviceSize);
}

tools::Rectangle OutputDevice::PixelToLogic(const tools::Rectangle& rDeviceRect) const
{
    return mpMapper->WindowToLogicUnits(rDeviceRect);
}

tools::Polygon OutputDevice::PixelToLogic(const tools::Polygon& rDevicePoly) const
{
    return mpMapper->WindowToLogicUnits(rDevicePoly);
}

tools::PolyPolygon OutputDevice::PixelToLogic(const tools::PolyPolygon& rDevicePolyPoly) const
{
    return mpMapper->WindowToLogicUnits(rDevicePolyPoly);
}

basegfx::B2DRectangle OutputDevice::PixelToLogic(const basegfx::B2DRectangle& rDeviceRect) const
{
    return mpMapper->WindowToLogicUnits(rDeviceRect);
}

basegfx::B2DPolyPolygon OutputDevice::PixelToLogic(const basegfx::B2DPolyPolygon& rPixelPolyPoly) const
{
    return mpMapper->WindowToLogicUnits(rPixelPolyPoly);
}

Point OutputDevice::PixelToLogic(const Point& rDevicePt, const MapMode& rMapMode) const
{
    return mpMapper->WindowToLogicUnits(rDevicePt, rMapMode);
}

Size OutputDevice::PixelToLogic(const Size& rDeviceSize, const MapMode& rMapMode) const
{
    return mpMapper->WindowToLogicUnits(rDeviceSize, rMapMode);
}

tools::Rectangle OutputDevice::PixelToLogic(const tools::Rectangle& rDeviceRect, const MapMode& rMapMode) const
{
    return mpMapper->WindowToLogicUnits(rDeviceRect, rMapMode);
}

tools::Polygon OutputDevice::PixelToLogic(const tools::Polygon& rDevicePoly, const MapMode& rMapMode) const
{
    return mpMapper->WindowToLogicUnits(rDevicePoly, rMapMode);
}

basegfx::B2DPolygon OutputDevice::PixelToLogic(const basegfx::B2DPolygon& rPixelPoly, const MapMode& rMapMode) const
{
    return mpMapper->WindowToLogicUnits(rPixelPoly, rMapMode);
}

basegfx::B2DPolyPolygon OutputDevice::PixelToLogic(const basegfx::B2DPolyPolygon& rPixelPolyPoly, const MapMode& rMapMode) const
{
    return mpMapper->WindowToLogicUnits(rPixelPolyPoly, rMapMode);
}

Point OutputDevice::LogicToLogic(const Point& rPtSource,
                                 const MapMode* pMapModeSource,
                                 const MapMode* pMapModeDest) const
{
    return mpMapper->LogicToLogic(rPtSource, pMapModeSource, pMapModeDest);
}

Size OutputDevice::LogicToLogic(const Size& rSzSource,
                                const MapMode* pMapModeSource,
                                const MapMode* pMapModeDest) const
{
    return mpMapper->LogicToLogic(rSzSource, pMapModeSource, pMapModeDest);
}

tools::Rectangle OutputDevice::LogicToLogic(const tools::Rectangle& rRectSource,
                                            const MapMode* pMapModeSource,
                                            const MapMode* pMapModeDest) const
{
    return mpMapper->LogicToLogic(rRectSource, pMapModeSource, pMapModeDest);
}

double OutputDevice::LogicWidthToDeviceSubPixel(tools::Long nWidth) const
{
    if (!mpMapper->IsMapModeEnabled())
        return nWidth;

    return mpMapper->LogicToViewDistanceSubPixelX(nWidth);
}

double OutputDevice::LogicHeightToDeviceSubPixel(tools::Long nHeight) const
{
    if (!mpMapper->IsMapModeEnabled())
        return nHeight;

    return mpMapper->LogicToViewDistanceSubPixelY(nHeight);
}

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
