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
#include <vcl/CoordinateMapper.hxx>
#include <vcl/GeometryAdapter.hxx>
#include <vcl/TransformTypes.hxx>

#include <TransformCompiler.hxx>
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

void OutputDevice::SetMappingPolicy(vcl::MappingPolicy ePolicy)
{
    if (meMapMode != ePolicy)
    {
        meMapMode = ePolicy;
        // The version update here is what tells CoordinateMapper
        // that its 8-slot cache is now stale.
        mpMapper->InvalidateViewTransform();
    }
}

vcl::MappingPolicy OutputDevice::GetMappingPolicy() const
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

    if ((GetMappingPolicy() == vcl::MappingPolicy::IgnoreMapMode) && maMapMode.IsDefault())
        return;

    SetMappingPolicy( vcl::MappingPolicy::IgnoreMapMode );
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
    vcl::MappingPolicy eOldPolicy = GetMappingPolicy();
    SetMappingPolicy( !rNewMapMode.IsDefault() ? vcl::MappingPolicy::ApplyMapMode : vcl::MappingPolicy::IgnoreMapMode );
    if ( GetMappingPolicy() == vcl::MappingPolicy::ApplyMapMode )
    {
        // if only the origin is converted, do not scale new
        if ( (rNewMapMode.GetMapUnit() == maMapMode.GetMapUnit()) &&
             (rNewMapMode.GetScaleX()  == maMapMode.GetScaleX())  &&
             (rNewMapMode.GetScaleY()  == maMapMode.GetScaleY())  &&
             (eOldPolicy                  == GetMappingPolicy()) )
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
    mpMapper->SetLogicToAbsoluteOffset(mpMapper->WindowToLogicUnits(Size(mpMapper->GetWindowToViewOffsetX(), mpMapper->GetWindowToViewOffsetY()), GetMappingPolicy()));

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
    mpMapper->SetLogicToAbsoluteOffset(mpMapper->WindowToLogicUnits(Size(mpMapper->GetWindowToViewOffsetX(), mpMapper->GetWindowToViewOffsetY()), GetMappingPolicy()));
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
    return mpMapper->LogicWidthToDevicePixel(nWidth, GetMappingPolicy());
}

tools::Long OutputDevice::LogicHeightToDevicePixel(tools::Long nHeight) const
{
    return mpMapper->LogicHeightToDevicePixel(nHeight, GetMappingPolicy());
}

vcl::WindowPoint OutputDevice::LogicToWindow(const Point& rLogicPt) const
{
    return vcl::WindowPoint(vcl::GeometryAdapter::Apply(mpMapper->Compile({CoordinateSpace::Logic, CoordinateSpace::Window, GetMappingPolicy()}), rLogicPt));
}

vcl::WindowSize OutputDevice::LogicToWindow(const Size& rLogicSize) const
{
    return vcl::WindowSize(vcl::GeometryAdapter::Apply(mpMapper->Compile({CoordinateSpace::Logic, CoordinateSpace::Window, GetMappingPolicy()}), rLogicSize));
}

vcl::WindowRect OutputDevice::LogicToWindow(const tools::Rectangle& rLogicRect) const
{
    return vcl::WindowRect(vcl::GeometryAdapter::Apply(mpMapper->Compile({CoordinateSpace::Logic, CoordinateSpace::Window, GetMappingPolicy()}), rLogicRect));
}

tools::Polygon OutputDevice::LogicToWindow(const tools::Polygon& rLogicPoly) const
{
    return vcl::GeometryAdapter::Apply(mpMapper->Compile({CoordinateSpace::Logic, CoordinateSpace::Window, GetMappingPolicy()}), rLogicPoly);
}

tools::PolyPolygon OutputDevice::LogicToWindow(const tools::PolyPolygon& rLogicPolyPoly) const
{
    return vcl::GeometryAdapter::Apply(mpMapper->Compile({CoordinateSpace::Logic, CoordinateSpace::Window, GetMappingPolicy()}), rLogicPolyPoly);
}

basegfx::B2DPolyPolygon OutputDevice::LogicToWindow(const basegfx::B2DPolyPolygon& rLogicPolyPoly) const
{
    return vcl::GeometryAdapter::Apply(mpMapper->Compile({CoordinateSpace::Logic, CoordinateSpace::Window, GetMappingPolicy()}), rLogicPolyPoly);
}

vcl::Region OutputDevice::LogicToWindow(const vcl::Region& rLogicRegion) const
{
    return vcl::GeometryAdapter::Apply(mpMapper->Compile({CoordinateSpace::Logic, CoordinateSpace::Window, GetMappingPolicy()}), rLogicRegion);
}

Point OutputDevice::LogicToWindow(const Point& rLogicPt, const MapMode& rMapMode) const
{
    return vcl::GeometryAdapter::Apply(vcl::TransformCompiler::Compile(mpMapper->GetViewTransformation(maMapMode, rMapMode, GetMappingPolicy())), rLogicPt);
}

Size OutputDevice::LogicToWindow(const Size& rLogicSize, const MapMode& rMapMode) const
{
    return vcl::GeometryAdapter::Apply(vcl::TransformCompiler::Compile(mpMapper->GetViewTransformation(maMapMode, rMapMode, GetMappingPolicy())), rLogicSize);
}

tools::Rectangle OutputDevice::LogicToWindow(const tools::Rectangle& rLogicRect, const MapMode& rMapMode) const
{
    return vcl::GeometryAdapter::Apply(vcl::TransformCompiler::Compile(mpMapper->GetViewTransformation(maMapMode, rMapMode, GetMappingPolicy())), rLogicRect);
}

tools::Polygon OutputDevice::LogicToWindow(const tools::Polygon& rLogicPoly, const MapMode& rMapMode) const
{
    return vcl::GeometryAdapter::Apply(vcl::TransformCompiler::Compile(mpMapper->GetViewTransformation(maMapMode, rMapMode, GetMappingPolicy())), rLogicPoly);
}

basegfx::B2DPolyPolygon OutputDevice::LogicToWindow(const basegfx::B2DPolyPolygon& rLogicPolyPoly,
                                                   const MapMode& rMapMode) const
{
    return vcl::GeometryAdapter::Apply(vcl::TransformCompiler::Compile(mpMapper->GetViewTransformation(maMapMode, rMapMode, GetMappingPolicy())), rLogicPolyPoly);
}

tools::Long OutputDevice::DevicePixelToLogicWidth(tools::Long nWidth) const
{
    return mpMapper->DevicePixelToLogicWidth(nWidth, GetMappingPolicy());
}

tools::Long OutputDevice::DevicePixelToLogicHeight(tools::Long nHeight) const
{
    return mpMapper->DevicePixelToLogicHeight(nHeight, GetMappingPolicy());
}

vcl::Region OutputDevice::WindowToLogic(const vcl::Region& rDeviceRegion) const
{
    return vcl::GeometryAdapter::Apply(mpMapper->Compile({CoordinateSpace::Window, CoordinateSpace::Logic, GetMappingPolicy()}), rDeviceRegion);
}

vcl::LogicPoint OutputDevice::WindowToLogic(const Point& rDevicePt) const
{
    return vcl::LogicPoint(vcl::GeometryAdapter::Apply(mpMapper->Compile({CoordinateSpace::Window, CoordinateSpace::Logic, GetMappingPolicy()}), rDevicePt));
}

Point OutputDevice::SubPixelToLogic(const basegfx::B2DPoint& rDevicePt) const
{
    return mpMapper->WindowSubPixelToLogicUnits(rDevicePt, GetMappingPolicy());
}

vcl::LogicSize OutputDevice::WindowToLogic(const Size& rDeviceSize) const
{
    return vcl::LogicSize(vcl::GeometryAdapter::Apply(mpMapper->Compile({CoordinateSpace::Window, CoordinateSpace::Logic, GetMappingPolicy()}), rDeviceSize));
}

vcl::LogicRect OutputDevice::WindowToLogic(const tools::Rectangle& rDeviceRect) const
{
    return vcl::LogicRect(vcl::GeometryAdapter::Apply(mpMapper->Compile({CoordinateSpace::Window, CoordinateSpace::Logic, GetMappingPolicy()}), rDeviceRect));
}

tools::Polygon OutputDevice::WindowToLogic(const tools::Polygon& rDevicePoly) const
{
    return vcl::GeometryAdapter::Apply(mpMapper->Compile({CoordinateSpace::Window, CoordinateSpace::Logic, GetMappingPolicy()}), rDevicePoly);
}

tools::PolyPolygon OutputDevice::WindowToLogic(const tools::PolyPolygon& rDevicePolyPoly) const
{
    return vcl::GeometryAdapter::Apply(mpMapper->Compile({CoordinateSpace::Window, CoordinateSpace::Logic, GetMappingPolicy()}), rDevicePolyPoly);
}

basegfx::B2DRectangle OutputDevice::WindowToLogic(const basegfx::B2DRectangle& rDeviceRect) const
{
    return vcl::GeometryAdapter::Apply(mpMapper->Compile({CoordinateSpace::Window, CoordinateSpace::Logic, GetMappingPolicy()}), rDeviceRect);
}

basegfx::B2DPolyPolygon OutputDevice::WindowToLogic(const basegfx::B2DPolyPolygon& rPixelPolyPoly) const
{
    return vcl::GeometryAdapter::Apply(mpMapper->Compile({CoordinateSpace::Window, CoordinateSpace::Logic, GetMappingPolicy()}), rPixelPolyPoly);
}

Point OutputDevice::WindowToLogic(const Point& rDevicePt, const MapMode& rMapMode) const
{
    return vcl::GeometryAdapter::Apply(vcl::TransformCompiler::Compile(mpMapper->GetInverseViewTransformation(maMapMode, rMapMode, GetMappingPolicy())), rDevicePt);
}

Size OutputDevice::WindowToLogic(const Size& rDeviceSize, const MapMode& rMapMode) const
{
    return vcl::GeometryAdapter::Apply(vcl::TransformCompiler::Compile(mpMapper->GetInverseViewTransformation(maMapMode, rMapMode, GetMappingPolicy())), rDeviceSize);
}

tools::Rectangle OutputDevice::WindowToLogic(const tools::Rectangle& rDeviceRect, const MapMode& rMapMode) const
{
    return vcl::GeometryAdapter::Apply(vcl::TransformCompiler::Compile(mpMapper->GetInverseViewTransformation(maMapMode, rMapMode, GetMappingPolicy())), rDeviceRect);
}

tools::Polygon OutputDevice::WindowToLogic(const tools::Polygon& rDevicePoly, const MapMode& rMapMode) const
{
    return vcl::GeometryAdapter::Apply(vcl::TransformCompiler::Compile(mpMapper->GetInverseViewTransformation(maMapMode, rMapMode, GetMappingPolicy())), rDevicePoly);
}

basegfx::B2DPolygon OutputDevice::WindowToLogic(const basegfx::B2DPolygon& rPixelPoly, const MapMode& rMapMode) const
{
    return vcl::GeometryAdapter::Apply(vcl::TransformCompiler::Compile(mpMapper->GetInverseViewTransformation(maMapMode, rMapMode, GetMappingPolicy())), rPixelPoly);
}

basegfx::B2DPolyPolygon OutputDevice::WindowToLogic(const basegfx::B2DPolyPolygon& rPixelPolyPoly, const MapMode& rMapMode) const
{
    return vcl::GeometryAdapter::Apply(vcl::TransformCompiler::Compile(mpMapper->GetInverseViewTransformation(maMapMode, rMapMode, GetMappingPolicy())), rPixelPolyPoly);
}

Point OutputDevice::LogicToLogic(const Point& rPtSource,
                                 const MapMode* pMapModeSource,
                                 const MapMode* pMapModeDest) const
{
    const MapMode* pBaseline = &GetMapMode();
    const MapMode* pSrc = pMapModeSource ? pMapModeSource : pBaseline;
    const MapMode* pDst = pMapModeDest ? pMapModeDest : pBaseline;

    if (!pSrc || !pDst || *pSrc == *pDst)
        return rPtSource;

    MappingCoefficients aMapResSource = mpMapper->ResolveMapResRelative(pBaseline, pSrc, GetMappingPolicy());
    MappingCoefficients aMapResDest = mpMapper->ResolveMapResRelative(pBaseline, pDst, GetMappingPolicy());

    return Point(aMapResSource.TransformPointX(rPtSource.X(), aMapResDest),
                 aMapResSource.TransformPointY(rPtSource.Y(), aMapResDest));
}

Size OutputDevice::LogicToLogic(const Size& rSzSource,
                                const MapMode* pMapModeSource,
                                const MapMode* pMapModeDest) const
{
    const MapMode* pBaseline = &GetMapMode();
    const MapMode* pSrc = pMapModeSource ? pMapModeSource : pBaseline;
    const MapMode* pDst = pMapModeDest ? pMapModeDest : pBaseline;

    if (!pSrc || !pDst || *pSrc == *pDst)
        return rSzSource;

    MappingCoefficients aMapResSource = mpMapper->ResolveMapResRelative(pBaseline, pSrc, GetMappingPolicy());
    MappingCoefficients aMapResDest = mpMapper->ResolveMapResRelative(pBaseline, pDst, GetMappingPolicy());

    return Size(aMapResSource.ScaleDistanceX(rSzSource.Width(), aMapResDest),
                aMapResSource.ScaleDistanceY(rSzSource.Height(), aMapResDest));
}

tools::Rectangle OutputDevice::LogicToLogic(const tools::Rectangle& rRectSource,
                                        const MapMode* pMapModeSource,
                                        const MapMode* pMapModeDest) const
{
    const MapMode* pBaseline = &GetMapMode();
    const MapMode* pSrc = pMapModeSource ? pMapModeSource : pBaseline;
    const MapMode* pDst = pMapModeDest ? pMapModeDest : pBaseline;

    if (!pSrc || !pDst || *pSrc == *pDst)
        return rRectSource;

    MappingCoefficients aMapResSource = mpMapper->ResolveMapResRelative(pBaseline, pSrc, GetMappingPolicy());
    MappingCoefficients aMapResDest = mpMapper->ResolveMapResRelative(pBaseline, pDst, GetMappingPolicy());

    return tools::Rectangle(aMapResSource.TransformPointX(rRectSource.Left(), aMapResDest),
                            aMapResSource.TransformPointY(rRectSource.Top(), aMapResDest),
                            aMapResSource.TransformPointX(rRectSource.Right(), aMapResDest),
                            aMapResSource.TransformPointY(rRectSource.Bottom(), aMapResDest));
}

double OutputDevice::LogicWidthToDeviceSubPixel(tools::Long nWidth) const
{
    return mpMapper->LogicWidthToWindowSubPixel(nWidth, GetMappingPolicy());
}

double OutputDevice::LogicHeightToDeviceSubPixel(tools::Long nHeight) const
{
    return mpMapper->LogicHeightToWindowSubPixel(nHeight, GetMappingPolicy());
}

basegfx::B2DHomMatrix OutputDevice::GetViewTransformation() const
{
    return mpMapper->GetViewTransformation(GetMappingPolicy());
}

basegfx::B2DHomMatrix OutputDevice::GetViewTransformation(const MapMode& rMapMode) const
{
    return mpMapper->GetViewTransformation(maMapMode, rMapMode, GetMappingPolicy());
}

basegfx::B2DHomMatrix OutputDevice::GetInverseViewTransformation() const
{
    return mpMapper->GetInverseViewTransformation(GetMappingPolicy());
}

basegfx::B2DHomMatrix OutputDevice::GetInverseViewTransformation(const MapMode& rMapMode) const
{
    return mpMapper->GetInverseViewTransformation(maMapMode, rMapMode, GetMappingPolicy());
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
