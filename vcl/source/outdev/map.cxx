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

void OutputDevice::EnableMapMode(vcl::MappingPolicy ePolicy)
{
    if (meMapMode != ePolicy)
    {
        meMapMode = ePolicy;
        // The version update here is what tells CoordinateMapper
        // that its 8-slot cache is now stale.
        mpMapper->InvalidateViewTransform();
    }
}

vcl::MappingPolicy OutputDevice::IsMapModeEnabled() const
{
    return meMapMode;
}

const MapMode& OutputDevice::GetMapMode() const { return maMapMode; }

void OutputDevice::SetPixelOffset(const Size& rOffset)
{
    mpMapper->SetWindowToViewOffset(rOffset);
}

void OutputDevice::SetMapMode()
{
    if ( mpMetaFile )
        mpMetaFile->AddAction( new MetaMapModeAction( MapMode() ) );

    if ((IsMapModeEnabled() == vcl::MappingPolicy::IgnoreMapMode) && maMapMode.IsDefault())
        return;

    EnableMapMode( vcl::MappingPolicy::IgnoreMapMode );
    ResetMapMode();

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
        mpMetaFile->AddAction( new MetaMapModeAction( rNewMapMode ) );

    // do nothing if MapMode was not changed
    if (maMapMode == rNewMapMode)
        return;

     // if default MapMode calculate nothing
    vcl::MappingPolicy eOldPolicy = IsMapModeEnabled();
    EnableMapMode( !rNewMapMode.IsDefault() ? vcl::MappingPolicy::ApplyMapMode : vcl::MappingPolicy::IgnoreMapMode );
    if ( IsMapModeEnabled() == vcl::MappingPolicy::ApplyMapMode )
    {
        // if only the origin is converted, do not scale new
        if ( (rNewMapMode.GetMapUnit() == maMapMode.GetMapUnit()) &&
             (rNewMapMode.GetScaleX()  == maMapMode.GetScaleX())  &&
             (rNewMapMode.GetScaleY()  == maMapMode.GetScaleY())  &&
             (eOldPolicy                  == IsMapModeEnabled()) )
        {
            // set offset
            Point aOrigin = rNewMapMode.GetOrigin();
            mpMapper->SetMappingXOffset(aOrigin.X());
            mpMapper->SetMappingYOffset(aOrigin.Y());
            ResetMapMode(rNewMapMode);

            // #i75163#
            mpMapper->InvalidateViewTransform();

            return;
        }
        if ( (eOldPolicy == vcl::MappingPolicy::IgnoreMapMode) && bRelMap )
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
        maMapMode.SetScaleX(maMapMode.GetScaleX() * rNewMapMode.GetScaleX());
        maMapMode.SetScaleY(maMapMode.GetScaleY() * rNewMapMode.GetScaleY());
        maMapMode.SetOrigin(Point(mpMapper->GetMappingXOffset(), mpMapper->GetMappingYOffset()));
    }
    else
    {
        ResetMapMode(rNewMapMode);
    }

    // create new objects (clip region are not re-scaled)
    mbNewFont   = true;
    mbInitFont  = true;
    ImplInitMapModeObjects();

    // #106426# Adapt logical offset when changing mapmode
    mpMapper->SetLogicToAbsoluteOffset(mpMapper->WindowToLogicUnits(Size(mpMapper->GetWindowToViewOffsetX(), mpMapper->GetWindowToViewOffsetY()), IsMapModeEnabled()));

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
    if (maMapMode == rNewMapMode)
        return;

    MapUnit eOld = maMapMode.GetMapUnit();
    MapUnit eNew = rNewMapMode.GetMapUnit();

    double fXF = rNewMapMode.GetScaleX() / maMapMode.GetScaleX();
    double fYF = rNewMapMode.GetScaleY() / maMapMode.GetScaleY();

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
        ResetMapMode(rNewMapMode);

    // #106426# Adapt logical offset when changing MapMode
    mpMapper->SetLogicToAbsoluteOffset(mpMapper->WindowToLogicUnits(Size(mpMapper->GetWindowToViewOffsetX(), mpMapper->GetWindowToViewOffsetY()), IsMapModeEnabled()));
}

void OutputDevice::ResetMapMode()
{
    maMapMode = MapMode();
    mpMapper->InvalidateViewTransform();
}

void OutputDevice::ResetMapMode(const MapMode& rMapMode)
{
    maMapMode = rMapMode;
    mpMapper->InvalidateViewTransform();
}

void OutputDevice::SetScaleX(double nX)
{
    maMapMode.SetScaleX(nX);
    mpMapper->InvalidateViewTransform();
}

void OutputDevice::SetScaleY(double nY)
{
    maMapMode.SetScaleY(nY);
    mpMapper->InvalidateViewTransform();
}

void OutputDevice::SetOrigin(const Point& rPt)
{
    maMapMode.SetOrigin(rPt);
    mpMapper->InvalidateViewTransform();
}

tools::Long OutputDevice::LogicWidthToDevicePixel(tools::Long nWidth) const
{
    return mpMapper->LogicWidthToDevicePixel(nWidth, IsMapModeEnabled());
}

tools::Long OutputDevice::LogicHeightToDevicePixel(tools::Long nHeight) const
{
    return mpMapper->LogicHeightToDevicePixel(nHeight, IsMapModeEnabled());
}

Point OutputDevice::LogicToPixel(const Point& rLogicPt) const
{
    return mpMapper->LogicToWindowUnits(rLogicPt, IsMapModeEnabled());
}

Size OutputDevice::LogicToPixel(const Size& rLogicSize) const
{
    return mpMapper->LogicToWindowUnits(rLogicSize, IsMapModeEnabled());
}

tools::Rectangle OutputDevice::LogicToPixel(const tools::Rectangle& rLogicRect) const
{
    return mpMapper->LogicToWindowUnits(rLogicRect, IsMapModeEnabled());
}

tools::Polygon OutputDevice::LogicToPixel(const tools::Polygon& rLogicPoly) const
{
    return mpMapper->LogicToWindowUnits(rLogicPoly, IsMapModeEnabled());
}

tools::PolyPolygon OutputDevice::LogicToPixel(const tools::PolyPolygon& rLogicPolyPoly) const
{
    return mpMapper->LogicToWindowUnits(rLogicPolyPoly, IsMapModeEnabled());
}

basegfx::B2DPolyPolygon OutputDevice::LogicToPixel(const basegfx::B2DPolyPolygon& rLogicPolyPoly) const
{
    return mpMapper->LogicToWindowUnits(rLogicPolyPoly, IsMapModeEnabled());
}

vcl::Region OutputDevice::LogicToPixel(const vcl::Region& rLogicRegion) const
{
    return mpMapper->LogicToWindowUnits(rLogicRegion, IsMapModeEnabled());
}

Point OutputDevice::LogicToPixel(const Point& rLogicPt, const MapMode& rMapMode) const
{
    return mpMapper->LogicToWindowUnits(rLogicPt, mpMapper->ResolveMap(maMapMode, rMapMode, IsMapModeEnabled()));
}

Size OutputDevice::LogicToPixel(const Size& rLogicSize, const MapMode& rMapMode) const
{
    return mpMapper->LogicToWindowUnits(rLogicSize, mpMapper->ResolveMap(maMapMode, rMapMode, IsMapModeEnabled()));
}

tools::Rectangle OutputDevice::LogicToPixel(const tools::Rectangle& rLogicRect, const MapMode& rMapMode) const
{
    return mpMapper->LogicToWindowUnits(rLogicRect, mpMapper->ResolveMap(maMapMode, rMapMode, IsMapModeEnabled()));
}

tools::Polygon OutputDevice::LogicToPixel(const tools::Polygon& rLogicPoly, const MapMode& rMapMode) const
{
    return mpMapper->LogicToWindowUnits(rLogicPoly, mpMapper->ResolveMap(maMapMode, rMapMode, IsMapModeEnabled()));
}

basegfx::B2DPolyPolygon OutputDevice::LogicToPixel(const basegfx::B2DPolyPolygon& rLogicPolyPoly,
                                                   const MapMode& rMapMode) const
{
    return mpMapper->LogicToWindowUnits(rLogicPolyPoly, mpMapper->ResolveMap(maMapMode, rMapMode, IsMapModeEnabled()));
}

tools::Long OutputDevice::DevicePixelToLogicWidth(tools::Long nWidth) const
{
    return mpMapper->DevicePixelToLogicWidth(nWidth, IsMapModeEnabled());
}

tools::Long OutputDevice::DevicePixelToLogicHeight(tools::Long nHeight) const
{
    return mpMapper->DevicePixelToLogicHeight(nHeight, IsMapModeEnabled());
}

vcl::Region OutputDevice::PixelToLogic(const vcl::Region& rDeviceRegion) const
{
    return mpMapper->WindowToLogicUnits(rDeviceRegion, IsMapModeEnabled());
}

Point OutputDevice::PixelToLogic(const Point& rDevicePt) const
{
    return mpMapper->WindowToLogicUnits(rDevicePt, IsMapModeEnabled());
}

Point OutputDevice::SubPixelToLogic(const basegfx::B2DPoint& rDevicePt) const
{
    return mpMapper->WindowSubPixelToLogicUnits(rDevicePt, IsMapModeEnabled());
}

Size OutputDevice::PixelToLogic(const Size& rDeviceSize) const
{
    return mpMapper->WindowToLogicUnits(rDeviceSize, IsMapModeEnabled());
}

tools::Rectangle OutputDevice::PixelToLogic(const tools::Rectangle& rDeviceRect) const
{
    return mpMapper->WindowToLogicUnits(rDeviceRect, IsMapModeEnabled());
}

tools::Polygon OutputDevice::PixelToLogic(const tools::Polygon& rDevicePoly) const
{
    return mpMapper->WindowToLogicUnits(rDevicePoly, IsMapModeEnabled());
}

tools::PolyPolygon OutputDevice::PixelToLogic(const tools::PolyPolygon& rDevicePolyPoly) const
{
    return mpMapper->WindowToLogicUnits(rDevicePolyPoly, IsMapModeEnabled());
}

basegfx::B2DRectangle OutputDevice::PixelToLogic(const basegfx::B2DRectangle& rDeviceRect) const
{
    return mpMapper->WindowToLogicUnits(rDeviceRect, IsMapModeEnabled());
}

basegfx::B2DPolyPolygon OutputDevice::PixelToLogic(const basegfx::B2DPolyPolygon& rPixelPolyPoly) const
{
    return mpMapper->WindowToLogicUnits(rPixelPolyPoly, IsMapModeEnabled());
}

Point OutputDevice::PixelToLogic(const Point& rDevicePt, const MapMode& rMapMode) const
{
    return mpMapper->WindowToLogicUnits(rDevicePt, mpMapper->ResolveMap(maMapMode, rMapMode, IsMapModeEnabled()));
}

Size OutputDevice::PixelToLogic(const Size& rDeviceSize, const MapMode& rMapMode) const
{
    return mpMapper->WindowToLogicUnits(rDeviceSize, mpMapper->ResolveMap(maMapMode, rMapMode, IsMapModeEnabled()));
}

tools::Rectangle OutputDevice::PixelToLogic(const tools::Rectangle& rDeviceRect, const MapMode& rMapMode) const
{
    return mpMapper->WindowToLogicUnits(rDeviceRect, mpMapper->ResolveMap(maMapMode, rMapMode, IsMapModeEnabled()));
}

tools::Polygon OutputDevice::PixelToLogic(const tools::Polygon& rDevicePoly, const MapMode& rMapMode) const
{
    return mpMapper->WindowToLogicUnits(rDevicePoly, mpMapper->ResolveMap(maMapMode, rMapMode, IsMapModeEnabled()));
}

basegfx::B2DPolygon OutputDevice::PixelToLogic(const basegfx::B2DPolygon& rPixelPoly, const MapMode& rMapMode) const
{
    return mpMapper->WindowToLogicUnits(rPixelPoly, mpMapper->ResolveMap(maMapMode, rMapMode, IsMapModeEnabled()));
}

basegfx::B2DPolyPolygon OutputDevice::PixelToLogic(const basegfx::B2DPolyPolygon& rPixelPolyPoly, const MapMode& rMapMode) const
{
    return mpMapper->WindowToLogicUnits(rPixelPolyPoly, mpMapper->ResolveMap(maMapMode, rMapMode, IsMapModeEnabled()));
}

Point OutputDevice::LogicToLogic(const Point& rPtSource,
                                 const MapMode* pMapModeSource,
                                 const MapMode* pMapModeDest) const
{
    return mpMapper->LogicToLogic(rPtSource, &GetMapMode(), pMapModeSource, pMapModeDest, IsMapModeEnabled());
}

Size OutputDevice::LogicToLogic(const Size& rSzSource,
                                const MapMode* pMapModeSource,
                                const MapMode* pMapModeDest) const
{
    return mpMapper->LogicToLogic(rSzSource, &GetMapMode(), pMapModeSource, pMapModeDest, IsMapModeEnabled());
}

tools::Rectangle OutputDevice::LogicToLogic(const tools::Rectangle& rRectSource,
                                            const MapMode* pMapModeSource,
                                            const MapMode* pMapModeDest) const
{
    return mpMapper->LogicToLogic(rRectSource, &GetMapMode(), pMapModeSource, pMapModeDest, IsMapModeEnabled());
}

double OutputDevice::LogicWidthToDeviceSubPixel(tools::Long nWidth) const
{
    return mpMapper->LogicWidthToWindowSubPixel(nWidth, IsMapModeEnabled());
}

double OutputDevice::LogicHeightToDeviceSubPixel(tools::Long nHeight) const
{
    return mpMapper->LogicHeightToWindowSubPixel(nHeight, IsMapModeEnabled());
}

basegfx::B2DHomMatrix OutputDevice::GetViewTransformation() const
{
    return mpMapper->GetViewTransformation(IsMapModeEnabled());
}

basegfx::B2DHomMatrix OutputDevice::GetViewTransformation(const MapMode& rMapMode) const
{
    return mpMapper->GetViewTransformation(maMapMode, rMapMode, IsMapModeEnabled());
}

basegfx::B2DHomMatrix OutputDevice::GetInverseViewTransformation() const
{
    return mpMapper->GetInverseViewTransformation(IsMapModeEnabled());
}

basegfx::B2DHomMatrix OutputDevice::GetInverseViewTransformation(const MapMode& rMapMode) const
{
    return mpMapper->GetInverseViewTransformation(maMapMode, rMapMode, IsMapModeEnabled());
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
