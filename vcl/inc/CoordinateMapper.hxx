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

#pragma once

#include <sal/types.h>
#include <tools/long.hxx>

#include <vcl/dllapi.h>
#include <vcl/mapmod.hxx>
#include <vcl/region.hxx>

class LineInfo;

class CoordinateMapper
{
private:
    bool mbMap = false;
    MapMode maMapMode;
    ImplMapRes maMapRes;

    // #i75163#
    mutable basegfx::B2DHomMatrix* mpViewTransform = nullptr;
    mutable basegfx::B2DHomMatrix* mpInverseViewTransform = nullptr;

    sal_Int32 mnDPIX = 0;
    sal_Int32 mnDPIY = 0;
    sal_Int32 mnDPIScalePercentage = 100;

    tools::Long mnDeviceToWindowOffsetX; = 0
    tools::Long mnDeviceToWindowOffsetY = 0;

    /// Additional output pixel offset, applied in LogicToPixel (used by SetPixelOffset/GetPixelOffset)
    tools::Long mnWindowToViewOffsetX = 0;
    /// Additional output pixel offset, applied in LogicToPixel (used by SetPixelOffset/GetPixelOffset)
    tools::Long mnWindowToViewOffsetY = 0;

    /// Additional output offset in _logical_ coordinates, applied in PixelToLogic (used by SetPixelOffset/GetPixelOffset)
    tools::Long mnLogicToAbsoluteOffsetX = 0;
    /// Additional output offset in _logical_ coordinates, applied in PixelToLogic (used by SetPixelOffset/GetPixelOffset)
    tools::Long mnLogicToAbsoluteOffsetY = 0;

    tools::Long mnOutWidth = 0;
    tools::Long mnOutHeight = 0;

public:
    bool IsMapModeEnabled() const { return mbMap; }
    void EnableMapMode(bool bEnable = true) { mbMap = bEnable; }

    sal_Int32 GetDPIX() const;
    sal_Int32 GetDPIY() const;

    void SetDPIX(sal_Int32 nDPIX);
    void SetDPIY(sal_Int32 nDPIY);

    sal_Int32 GetDPIScalePercentage() const;
    void SetDPIScalePercentage(sal_Int32 nPercentage);

    float GetDPIScaleFactor() const;

    void SetPixelOffset(const Size& rSize);

    tools::Long GetDeviceToWindowOffsetX() const;
    tools::Long GetDeviceToWindowOffsetY() const;
    Point GetDeviceToWindowOffset() const;

    tools::Long GetWindowToViewOffsetX() const { return mnWindowToViewOffsetX; }
    tools::Long GetWindowToViewOffsetY() const { return mnWindowToViewOffsetY; }
    Size GetWindowToViewOffset() const
    {
        return Size(mnWindowToViewOffsetX, mnWindowToViewOffsetY);
    }

    tools::Long GetLogicToAbsoluteOffsetX() const
    {
        return mnLogicToAbsoluteOffsetX;
    }
    tools::Long GetLogicToAbsoluteOffsetY() const
    {
        return mnLogicToAbsoluteOffsetY;
    }
    Size GetLogicToAbsoluteOffset() const
    {
        return Size(mnLogicToAbsoluteOffsetX, mnLogicToAbsoluteOffsetY);
    }

    tools::Long GetDeviceToViewOffsetX() const
    {
        return mnDeviceToWindowOffsetX + mnWindowToViewOffsetX;
    }
    tools::Long GetDeviceToViewOffsetY() const
    {
        return mnDeviceToWindowOffsetY + mnWindowToViewOffsetY;
    }
    Size GetDeviceToViewOffset() const
    {
        return Size(GetDeviceToViewOffsetX(), GetDeviceToViewOffsetY());
    }

    void SetDeviceToWindowOffsetX(tools::Long nDeviceToWindowOffsetX);
    void SetDeviceToWindowOffsetY(tools::Long nDeviceToWindowOffsetY);
    void SetWindowToViewOffset(const Size& rSize);
    void SetLogicToAbsoluteOffset(const Size& rSize);

    Size LogicToViewDistance(const Size& rLogicSize) const;

    tools::Long GetOutputWidthPixel() const;
    tools::Long GetOutputHeightPixel() const;
    Size GetOutputSizePixel() const;

    void SetOutputWidthPixel(tools::Long nWidth);
    void SetOutputHeightPixel(tools::Long nHeight);

    const MapMode& GetMapMode() const { return maMapMode; }
    bool IsDefaultMapMode() const { return maMapMode.IsDefault(); }
    void ResetMapMode() { maMapMode = MapMode(); }
    void ResetMapMode(const MapMode& rMapMode) { maMapMode = rMapMode; }
    MapUnit GetMapUnit() const { return maMapMode.GetMapUnit(); }

    double GetScaleX() const { return maMapMode.GetScaleX(); }
    double GetScaleY() const { return maMapMode.GetScaleY(); }
    void SetScaleX(double nX) { maMapMode.SetScaleX(nX); }
    void SetScaleY(double nY) { maMapMode.SetScaleY(nY); }

    void SetOrigin(const Point& rPt) { maMapMode.SetOrigin(rPt); }

    tools::Long GetMappingXOffset() const { return maMapRes.mnMapOfsX; }
    tools::Long GetMappingYOffset() const { return maMapRes.mnMapOfsY; }
    double GetMapResolutionScaleX() const { return maMapRes.mfMapScX; }
    double GetMapResolutionScaleY() const { return maMapRes.mfMapScY; }

    void SetMappingXOffset(tools::Long nOffset) { maMapRes.mnMapOfsX = nOffset; }
    void SetMappingYOffset(tools::Long nOffset) { maMapRes.mnMapOfsY = nOffset; }
    void SetMapResolutionScaleX(double fX) { maMapRes.mfMapScX = fX; }
    void SetMapResolutionScaleY(double fY) { maMapRes.mfMapScY = fY; }

    void CalcMapResolution(const MapMode& rMapMode, tools::Long nDPIX,
                                          tools::Long nDPIY);

    ImplMapRes ResolveMapRes(const MapMode* pMode);

    /** Invalidate the view transformation.

     @since AOO bug 75163 (OpenOffice.org 2.4.3 - OOH 680 milestone 212)
     */
    void InvalidateViewTransform();
    basegfx::B2DHomMatrix GetViewTransformation() const;
    basegfx::B2DHomMatrix GetViewTransformation(const MapMode& rMapMode) const;
    basegfx::B2DHomMatrix GetInverseViewTransformation() const;
    basegfx::B2DHomMatrix
    GetInverseViewTransformation(const MapMode& rMapMode) const;
    basegfx::B2DHomMatrix GetDeviceTransformation() const;

    // ========================================================================
    // PIPELINE STAGES (Coordinate Transitions)
    // ========================================================================

    // Device <-> Window (Integer)
    tools::Long DeviceToWindowUnitsX(tools::Long nX) const;
    tools::Long DeviceToWindowUnitsY(tools::Long nY) const;
    tools::Long WindowToDeviceUnitsX(tools::Long nX) const;
    tools::Long WindowToDeviceUnitsY(tools::Long nY) const;

    // Device <-> Window (Sub-pixel)
    double DeviceToWindowSubPixelX(double fX) const;
    double DeviceToWindowSubPixelY(double fY) const;
    double WindowToDeviceSubPixelX(double fX) const;
    double WindowToDeviceSubPixelY(double fY) const;

    // Window <-> View (Integer)
    tools::Long WindowToViewUnitsX(tools::Long nX) const;
    tools::Long WindowToViewUnitsY(tools::Long nY) const;
    tools::Long ViewToWindowUnitsX(tools::Long nX) const;
    tools::Long ViewToWindowUnitsY(tools::Long nY) const;

    // Window <-> View (Sub-pixel)
    double WindowToViewSubPixelX(double fX) const;
    double WindowToViewSubPixelY(double fY) const;
    double ViewToWindowSubPixelX(double fX) const;
    double ViewToWindowSubPixelY(double fY) const;

    // View <-> LogicUnits (Integer)
    tools::Long ViewToLogicUnitsX(tools::Long nX) const;
    tools::Long ViewToLogicUnitsY(tools::Long nY) const;
    vcl::Region ViewToDevice(const vcl::Region& rRegion) const;

    tools::Long LogicUnitsToViewUnitsX(tools::Long nX) const;
    tools::Long LogicUnitsToViewUnitsY(tools::Long nY) const;
    tools::Long LogicUnitsToViewUnitsX(tools::Long nX, const ImplMapRes& rRes) const;
    tools::Long LogicUnitsToViewUnitsY(tools::Long nY, const ImplMapRes& rRes) const;

    // View <-> LogicUnits (Sub-pixel)
    double ViewSubPixelToLogicUnitsX(double fX) const;
    double ViewSubPixelToLogicUnitsY(double fY) const;
    tools::Long ViewSubPixelToLogicUnitsIntX(double fX) const;
    tools::Long ViewSubPixelToLogicUnitsIntY(double fY) const;
    double LogicUnitsToViewSubPixelX(double fX) const;
    double LogicUnitsToViewSubPixelY(double fY) const;

    // ========================================================================
    // MASTER WRAPPERS (Multi-space Positional Transformations)
    // ========================================================================

    // Device <-> Logic (Full journey)
    tools::Long LogicToDevicePixelX(tools::Long nX) const;
    tools::Long LogicToDevicePixelY(tools::Long nY) const;
    tools::Long LogicWidthToDevicePixel(tools::Long nWidth) const;
    tools::Long LogicHeightToDevicePixel(tools::Long nHeight) const;
    Point LogicToDevicePixel(const Point& rLogicPt) const;
    Size LogicToDevicePixel(const Size& rLogicSize) const;
    tools::Rectangle LogicToDevicePixel(const tools::Rectangle& rLogicRect) const;
    tools::Polygon LogicToDevicePixel(const tools::Polygon& rLogicPoly) const;
    tools::PolyPolygon
    LogicToDevicePixel(const tools::PolyPolygon& rLogicPolyPoly) const;
    LineInfo LogicToDevicePixel(const LineInfo& rLineInfo) const;
    basegfx::B2DPolygon
    LogicToDevicePixel(const basegfx::B2DPolygon& rLogicPoly) const;
    double LogicToDeviceSubPixelX(double fX) const;
    double LogicToDeviceSubPixelY(double fY) const;

    tools::Long DevicePixelToLogicX(tools::Long nX) const;
    tools::Long DevicePixelToLogicY(tools::Long nY) const;
    tools::Long DevicePixelToLogicWidth(tools::Long nWidth) const;
    tools::Long DevicePixelToLogicHeight(tools::Long nHeight) const;
    Point DevicePixelToLogic(const Point& rDevicePt) const;
    Size DevicePixelToLogic(const Size& rDeviceSize) const;
    tools::Rectangle DevicePixelToLogic(const tools::Rectangle& rPixelRect) const;
    double DevicePixelToLogicSubPixelX(double fX) const;
    double DevicePixelToLogicSubPixelY(double fY) const;

    // Window <-> Logic
    tools::Long WindowToLogicX(tools::Long nX) const;
    tools::Long WindowToLogicY(tools::Long nY) const;
    double WindowToLogicSubPixelX(double fX) const;
    double WindowToLogicSubPixelY(double fY) const;

    double LogicToWindowSubPixelX(double fX) const;
    double LogicToWindowSubPixelY(double fY) const;
    tools::Long LogicToWindowX(tools::Long nX) const;
    tools::Long LogicToWindowY(tools::Long nY) const;
    Size LogicToWindowUnits(const Size& rLogicSize) const;

    // View <-> Absolute Logic (Includes mnLogicToAbsoluteOffsetX/Y)
    tools::Long ViewToLogicX(tools::Long nX) const;
    tools::Long ViewToLogicY(tools::Long nY) const;

    // To resolve the return-type conflict, these now return double
    double ViewSubPixelToLogicX(double fX) const;
    double ViewSubPixelToLogicY(double fY) const;
    // View (Sub-pixel) -> Absolute Logic (Integer)
    // Note: This rounds the distance before stripping offsets to satisfy legacy test parity.
    tools::Long ViewSubPixelToLogicIntX(double fX) const;
    tools::Long ViewSubPixelToLogicIntY(double fY) const;
    tools::Long ViewSubPixelToLogicIntX(double fX, const ImplMapRes& rRes) const;
    tools::Long ViewSubPixelToLogicIntY(double fY, const ImplMapRes& rRes) const;
    double LogicToViewSubPixelX(double fX) const;
    double LogicToViewSubPixelY(double fY) const;

    // Logic -> Window units (Commonly used in OutputDevice::LogicToPixel)
    tools::Long LogicToWindowUnitsX(tools::Long nX) const;
    tools::Long LogicToWindowUnitsY(tools::Long nY) const;
    tools::Long LogicToWindowUnitsX(tools::Long nX, const ImplMapRes& rRes) const;
    tools::Long LogicToWindowUnitsY(tools::Long nY, const ImplMapRes& rRes) const;
    Point LogicToWindowUnits(const Point& rLogicPt) const;
    Point LogicToWindowUnits(const Point& rLogicPt, const MapMode& rMapMode) const;
    Point LogicToWindowUnits(const Point& rLogicPt, const ImplMapRes& rRes) const;
    Size LogicToWindowUnits(const Size& rLogicSize, const MapMode& rMapMode) const;
    Size LogicToWindowUnits(const Size& rLogicSize, const ImplMapRes& rRes) const;
    tools::Rectangle LogicToWindowUnits(const tools::Rectangle& rRect,
                                                       const MapMode& rMapMode) const;
    tools::Rectangle LogicToWindowUnits(const tools::Rectangle& rRect,
                                                       const ImplMapRes& rRes) const;
    tools::Rectangle LogicToWindowUnits(const tools::Rectangle& rRect) const;
    vcl::Region LogicToWindowUnits(const vcl::Region& rRegion) const;
    tools::Polygon LogicToWindowUnits(const tools::Polygon& rPoly) const;
    tools::Polygon LogicToWindowUnits(const tools::Polygon& rPoly,
                                                     const MapMode& rMapMode) const;
    tools::Polygon LogicToWindowUnits(const tools::Polygon& rPoly,
                                                     const ImplMapRes& rRes) const;
    tools::PolyPolygon LogicToWindowUnits(const tools::PolyPolygon& rPoly) const;
    basegfx::B2DPolyPolygon
    LogicToWindowUnits(const basegfx::B2DPolyPolygon& rLogicPolyPoly) const;
    SAL_DLLPRIVATE basegfx::B2DPolyPolygon
    LogicToWindowUnits(const basegfx::B2DPolyPolygon& rLogicPolyPoly,
                       const MapMode& rMapMode) const;

    SAL_DLLPRIVATE vcl::Region WindowToLogicUnits(const vcl::Region& rWindowRegion) const;
    SAL_DLLPRIVATE tools::Polygon WindowToLogicUnits(const tools::Polygon& rWindowPoly) const;
    SAL_DLLPRIVATE tools::Rectangle WindowToLogicUnits(const tools::Rectangle& rRect) const;
    SAL_DLLPRIVATE tools::PolyPolygon WindowToLogicUnits(const tools::PolyPolygon& rPolyPoly) const;
    SAL_DLLPRIVATE basegfx::B2DPolyPolygon
    WindowToLogicUnits(const basegfx::B2DPolyPolygon& rPolyPoly) const;

    // ========================================================================
    // DISTANCE SCALING (Raw Scalar Conversion, NO offsets applied)
    // ========================================================================

    // Integer Distances
    tools::Long LogicToViewDistanceX(tools::Long n) const;
    tools::Long LogicToViewDistanceY(tools::Long n) const;
    tools::Long LogicToViewDistanceX(tools::Long n, double fScale) const;
    tools::Long LogicToViewDistanceY(tools::Long n, double fScale) const;
    tools::Long ViewToLogicDistanceX(tools::Long n) const;
    tools::Long ViewToLogicDistanceY(tools::Long n) const;
    tools::Long ViewToLogicDistanceX(tools::Long n, double fScale) const;
    tools::Long ViewToLogicDistanceY(tools::Long n, double fScale) const;

    // Double/Sub-Pixel Distances
    double LogicToViewDistanceSubPixelX(tools::Long n) const;
    double LogicToViewDistanceSubPixelY(tools::Long n) const;
    double LogicToViewDistanceSubPixelX(tools::Long n, double fScale) const;
    double LogicToViewDistanceSubPixelY(tools::Long n, double fScale) const;
    double ViewToLogicDistanceDoubleX(double n) const;
    double ViewToLogicDistanceDoubleY(double n) const;
    double ViewToLogicDistanceDoubleX(double n, double fScale) const;
    double ViewToLogicDistanceDoubleY(double n, double fScale) const;
    tools::Long ViewSubPixelToLogicDistanceX(double n) const;
    tools::Long ViewSubPixelToLogicDistanceY(double n) const;
    tools::Long ViewSubPixelToLogicDistanceX(double n, double fScale) const;
    tools::Long ViewSubPixelToLogicDistanceY(double n, double fScale) const;

private:
    tools::Long ImplCalcDevicePixelX(tools::Long nX) const;
    tools::Long ImplCalcDevicePixelY(tools::Long nY) const;
};

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
