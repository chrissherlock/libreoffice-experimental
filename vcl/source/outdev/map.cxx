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

#include <vcl/deviceconcepts.hxx>
#include <vcl/cursor.hxx>
#include <vcl/lineinfo.hxx>
#include <vcl/metafile/MetaAction.hxx>
#include <vcl/metafile/MetafileRecorder.hxx>
#include <vcl/print.hxx>
#include <vcl/virdev.hxx>
#include <vcl/wrkwin.hxx>

#include <CoordinateMapper.hxx>
#include <GraphicsState.hxx>
#include <devicedispatcher.hxx>
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
    maRecorder.RecordMapMode(MapMode());

    // CoordinateMapper handles its own "is change necessary" logic
    if (mpMapper->ResetToDefault())
    {
        // Only trigger side effects if the state actually changed
        ImplInitMapModeObjects();
    }
}

void OutputDevice::SetMapMode(const MapMode& rNewMapMode)
{
    maRecorder.RecordMapMode(rNewMapMode);

    // One call to handle the math, scaling, and offset adaptation
    if (mpMapper->UpdateMapMode(rNewMapMode, GetDPIX(), GetDPIY()))
    {
        // Create new objects/Update Cursor (The side effects)
        ImplInitMapModeObjects();
    }
}

void OutputDevice::SetMetafileMapMode(const MapMode& rNewMapMode, bool bIsRecord)
{
    MapMode aFinalMap = rNewMapMode;

    Point aHWOffset;
    vcl::DispatchDevice(*this, [&aHWOffset](auto& rDev) {
        using DevType = std::decay_t<decltype(rDev)>;

        // We evaluate against the C++20 Concept.
        // This guarantees both the intent (Trait) and the capability (GetPageOffset).
        if constexpr (vcl::PageDevice<DevType>)
        {
            aHWOffset = rDev.GetPageOffset();
        }
    });

    if (aHWOffset != Point())
    {
        Point aOrigin = aFinalMap.GetOrigin();
        aOrigin += aHWOffset;
        aFinalMap.SetOrigin(aOrigin);
    }

    if (bIsRecord)
        SetRelativeMapMode(aFinalMap);
    else
        SetMapMode(aFinalMap);
}

void OutputDevice::SetRelativeMapMode(const MapMode& rNewMapMode)
{
    if (mpMapper->GetMapMode() == rNewMapMode)
        return;

    MapMode aRelMode = mpMapper->GetRelativeMapMode(rNewMapMode);

    SetMapMode(aRelMode);

    if (rNewMapMode.GetMapUnit() != MapUnit::MapRelative)
        mpMapper->ResetMapMode(rNewMapMode);

    ImplInitMapModeObjects();
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

void OutputDevice::ImplInitMapModeObjects()
{
    vcl::DispatchDevice(*this, [](const auto& rConcrete) {
        // Compile-time trait check!
        // OutputDevice never needs to know what a Window or Cursor is.
        if constexpr (requires { rConcrete.UpdateCursorOnMapModeChange(); })
        {
            rConcrete.UpdateCursorOnMapModeChange();
        }
    });
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

const Point& OutputDevice::GetRefPoint() const { return mpGraphicsState->maRefPoint; }

bool OutputDevice::IsRefPoint() const { return mpGraphicsState->mbRefPoint; }

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
