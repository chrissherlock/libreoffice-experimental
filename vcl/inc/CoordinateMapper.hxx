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
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements. See the NOTICE file distributed
 * with this work for additional information regarding copyright
 * ownership. The ASF licenses this file to you under the Apache
 * License, Version 2.0 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.apache.org/licenses/LICENSE-2.0 .
 */

#pragma once

#include <sal/types.h>
#include <tools/long.hxx>

#include <vcl/dllapi.h>
#include <vcl/mapconvert.hxx>
#include <vcl/mapmod.hxx>
#include <vcl/region.hxx>

#include <optional>
#include <atomic>
#include <memory>
#include <concepts>

class LineInfo;

template <typename T>
concept TransformableB2DGeometry = requires(T a, const basegfx::B2DHomMatrix& rMatrix)
{
    a.transform(rMatrix);
};

/**
 * @class CoordinateMapper
 * @brief Centralized mapping engine for VCL coordinate transformations.
 *
 * The CoordinateMapper decouples pure mathematical mapping and spatial translation
 * logic from the physical OutputDevice. It provides a strict, layered pipeline
 * to convert geometry between physical pixels and mathematical document units.
 *
 * Coordinate Spaces (Conceptual)
 * ------------------------------
 * Conceptually, the mapper manages transitions across four distinct domains:
 * 1. Device Space: Absolute physical pixels on the monitor or printer.
 * 2. Window Space: Client area pixels (Device minus DeviceToWindowOffset).
 * 3. View Space: Scrollable viewport pixels (Window minus WindowToViewOffset).
 * 4. Logic Space: Document coordinates defined by a MapMode and MapOffset.
 * * NOTE: While these spaces exist conceptually, the actual implementation squashes
 * them into a single, unified Affine Transformation Matrix for performance.
 *
 * Architecture & API Groupings
 * ----------------------------
 * - Lock-Free Snapshots (Core): Because coordinate state (DPI, MapModes, offsets)
 * can be mutated concurrently, the mapper uses a lock-free, version-stamped
 * `TransformSnapshot`. All transformations project from an immutable snapshot
 * acquired via `AcquireSnapshot()`.
 *
 * - Single Source of Truth: Basegfx geometry and legacy scalar pipelines are unified.
 * All legacy procedural math acts as a lightweight wrapper directly extracting scale
 * and translation components from the pre-calculated Affine matrix cache.
 *
 * - Distance Scaling: Specialized scalar functions (e.g., LogicToViewDistanceX)
 * apply scaling *without* applying translational offsets. Used strictly for Size.
 *
 * ========================================================================
 * CoordinateMapper Transformation Contract
 * ========================================================================
 *
 * 1. Concurrency and State Coherence
 * Transformation functions must never read mutable state (DPI, offsets) directly.
 * They must acquire a localized, immutable `TransformSnapshot` once per function
 * boundary to guarantee mathematical coherence and avoid torn reads.
 *
 * 2. Subpixel Authority
 * All geometric transformations must be performed in double precision
 * (basegfx::B2DHomMatrix). Conversion to integer occurs at the final API boundary.
 *
 * 3. Fast-Path Semantic Guards
 * If `IsMappingActive()` is false, scalar legacy pipelines must bypass MapMode
 * scaling *and* intermediate View/Window offsets, acting as a pure translation
 * to device space.
 *
 * 4. Distance vs. Position
 * Distance (Size) transformations apply scaling only. Position (Point/Geometry)
 * transformations include both scaling and offsets. Width/Height must not be
 * used to calculate geometric boundaries.
 *
 * 5. Forward/Inverse Symmetry
 * For every transformation A -> B, the inverse B -> A must return the original
 * value within ±0.5 drift for intermediate floating-point values.
 * ========================================================================
 */

namespace vcl::detail
{
template <typename T> concept B2DTransformable = requires(T a, const basegfx::B2DHomMatrix& m)
{
    { a.transform(m) };
};

template <typename T> concept B2DMultipliable = requires(T a, const basegfx::B2DHomMatrix& m)
{
    { a *= m };
};

template <typename T> concept B2DGeometry = B2DTransformable<T> || B2DMultipliable<T>;
}

class VCL_DLLPUBLIC CoordinateMapper
{
private:
    bool mbMap = false;
    MapMode maMapMode;
    ImplMapRes maMapRes;

    // #i75163#
    struct TransformSnapshot
    {
        basegfx::B2DHomMatrix maLogicToDevice;
        basegfx::B2DHomMatrix maDeviceToLogic;
        basegfx::B2DHomMatrix maView;
        basegfx::B2DHomMatrix maInvView;
        uint64_t mnVersion = 0;
    };

    mutable std::shared_ptr<const TransformSnapshot> mpSnapshot;
    mutable std::atomic<uint64_t> mnStateCounter{ 0 };

    std::shared_ptr<const TransformSnapshot> AcquireSnapshot() const;
    std::shared_ptr<TransformSnapshot> BuildSnapshot() const;

    sal_Int32 mnDPIX = 72;
    sal_Int32 mnDPIY = 72;
    sal_Int32 mnDPIScalePercentage = 100;

    tools::Long mnDeviceToWindowOffsetX = 0;
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
    bool IsMappingActive() const { return mbMap && mnDPIX > 0 && mnDPIY > 0; }
    void EnableMapMode(bool bEnable = true)
    {
        mbMap = bEnable;
        InvalidateViewTransform();
    }

    sal_Int32 GetDPIX() const;
    sal_Int32 GetDPIY() const;

    void SetDPIX(sal_Int32 nDPIX);
    void SetDPIY(sal_Int32 nDPIY);

    sal_Int32 GetDPIScalePercentage() const;
    float GetDPIScaleFactor() const;
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

    tools::Long GetLogicToAbsoluteOffsetX() const { return mnLogicToAbsoluteOffsetX; }
    tools::Long GetLogicToAbsoluteOffsetY() const { return mnLogicToAbsoluteOffsetY; }

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
    void SetWindowToViewOffset(const Size& rWindowPixelOffset);
    void SetLogicToAbsoluteOffset(const Size& rSize);

    Size LogicToViewDistance(const Size& rLogicSize) const;

    tools::Long GetOutputWidthPixel() const;
    tools::Long GetOutputHeightPixel() const;
    Size GetOutputSizePixel() const;

    void SetOutputWidthPixel(tools::Long nWidth);
    void SetOutputHeightPixel(tools::Long nHeight);

    const MapMode& GetMapMode() const { return maMapMode; }
    bool IsDefaultMapMode() const { return maMapMode.IsDefault(); }

    void ResetMapMode()
    {
        maMapMode = MapMode();
        InvalidateViewTransform();
    }

    void ResetMapMode(const MapMode& rMapMode)
    {
        maMapMode = rMapMode;
        InvalidateViewTransform();
    }

    MapUnit GetMapUnit() const { return maMapMode.GetMapUnit(); }

    double GetScaleX() const { return maMapMode.GetScaleX(); }
    double GetScaleY() const { return maMapMode.GetScaleY(); }

    void SetScaleX(double nX)
    {
        maMapMode.SetScaleX(nX);
        InvalidateViewTransform();
    }

    void SetScaleY(double nY)
    {
        maMapMode.SetScaleY(nY);
        InvalidateViewTransform();
    }

    void SetOrigin(const Point& rPt)
    {
        maMapMode.SetOrigin(rPt);
        InvalidateViewTransform();
    }

    tools::Long GetMappingXOffset() const { return maMapRes.mnMapOfsX; }
    tools::Long GetMappingYOffset() const { return maMapRes.mnMapOfsY; }
    double GetMapResolutionScaleX() const { return maMapRes.mfMapScX; }
    double GetMapResolutionScaleY() const { return maMapRes.mfMapScY; }

    void SetMappingXOffset(tools::Long nOffset)
    {
        maMapRes.mnMapOfsX = nOffset;
        InvalidateViewTransform();
    }

    void SetMappingYOffset(tools::Long nOffset)
    {
        maMapRes.mnMapOfsY = nOffset;
        InvalidateViewTransform();
    }

    void SetMapResolutionScaleX(double fX)
    {
        maMapRes.mfMapScX = fX;
        InvalidateViewTransform();
    }

    void SetMapResolutionScaleY(double fY)
    {
        maMapRes.mfMapScY = fY;
        InvalidateViewTransform();
    }

    void CalcMapResolution(const MapMode& rMapMode, tools::Long nDPIX, tools::Long nDPIY);

    ImplMapRes ResolveMapRes(const MapMode* pMode) const;

    /** Invalidate the view transformation.

     @since AOO bug 75163 (OpenOffice.org 2.4.3 - OOH 680 milestone 212)
     */
    void InvalidateViewTransform();
    basegfx::B2DHomMatrix GetViewTransformation() const;
    basegfx::B2DHomMatrix GetViewTransformation(const MapMode& rMapMode) const;
    basegfx::B2DHomMatrix GetInverseViewTransformation() const;
    basegfx::B2DHomMatrix GetInverseViewTransformation(const MapMode& rMapMode) const;
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
    double LogicWidthToDeviceSubPixel(tools::Long nWidth) const;
    tools::Long LogicHeightToDevicePixel(tools::Long nHeight) const;
    Point LogicToDevicePixel(const Point& rLogicPt) const;
    Size LogicToDevicePixel(const Size& rLogicSize) const;
    tools::Rectangle LogicToDevicePixel(const tools::Rectangle& rLogicRect) const;
    tools::Polygon LogicToDevicePixel(const tools::Polygon& rLogicPoly) const;
    tools::PolyPolygon LogicToDevicePixel(const tools::PolyPolygon& rLogicPolyPoly) const;
    LineInfo LogicToDevicePixel(const LineInfo& rLineInfo) const;
    basegfx::B2DPolygon LogicToDevicePixel(const basegfx::B2DPolygon& rLogicPoly) const;
    double LogicToDeviceSubPixelX(double fX) const;
    double LogicToDeviceSubPixelY(double fY) const;
    basegfx::B2DPoint LogicToDeviceSubPixel(const Point& rPoint) const;

    tools::Long DevicePixelToLogicX(tools::Long nX) const;
    tools::Long DevicePixelToLogicY(tools::Long nY) const;
    tools::Long DevicePixelToLogicWidth(tools::Long nWidth) const;
    tools::Long DevicePixelToLogicHeight(tools::Long nHeight) const;
    Point DevicePixelToLogic(const Point& rDevicePt) const;
    Size DevicePixelToLogic(const Size& rDeviceSize) const;
    tools::Rectangle DevicePixelToLogic(const tools::Rectangle& rPixelRect) const;
    double DevicePixelToLogicSubPixelX(double fX) const;
    double DevicePixelToLogicSubPixelY(double fY) const;
    basegfx::B2DPoint DevicePixelToLogicSubPixel(const Point& rDevicePt) const;

    // Window <-> Logic
    tools::Long WindowToLogicX(tools::Long nX) const;
    tools::Long WindowToLogicY(tools::Long nY) const;
    double WindowToLogicSubPixelX(double fX) const;
    double WindowToLogicSubPixelY(double fY) const;

    double LogicToWindowSubPixelX(double fX) const;
    double LogicToWindowSubPixelY(double fY) const;
    tools::Long LogicToWindowX(tools::Long nX) const;
    tools::Long LogicToWindowY(tools::Long nY) const;
    double LogicWidthToWindowSubPixel(tools::Long nWidth) const;
    double LogicHeightToWindowSubPixel(tools::Long nHeight) const;
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
    tools::Polygon LogicToWindowUnits(const tools::Polygon& rPoly, const MapMode& rMapMode) const;
    tools::Polygon LogicToWindowUnits(const tools::Polygon& rPoly, const ImplMapRes& rRes) const;
    tools::PolyPolygon LogicToWindowUnits(const tools::PolyPolygon& rPoly) const;
    basegfx::B2DPolyPolygon LogicToWindowUnits(const basegfx::B2DPolyPolygon& rLogicPolyPoly) const;
    basegfx::B2DPolyPolygon LogicToWindowUnits(const basegfx::B2DPolyPolygon& rLogicPolyPoly,
                                               const MapMode& rMapMode) const;

    tools::Long WindowSubPixelToLogicIntX(double fX) const;
    tools::Long WindowSubPixelToLogicIntY(double fY) const;
    tools::Long WindowSubPixelToLogicIntX(double fX, const ImplMapRes& rMapRes) const;
    tools::Long WindowSubPixelToLogicIntY(double fY, const ImplMapRes& rMapRes) const;
    vcl::Region WindowToLogicUnits(const vcl::Region& rWindowRegion) const;
    Point WindowToLogicUnits(const Point& rWindowPt) const;
    Point WindowToLogicUnits(const Point& rWindowPt, const ImplMapRes& rMapRes) const;
    Point WindowToLogicUnits(const Point& rWindowPt, const MapMode& rMapMode) const;

    tools::Rectangle WindowToLogicUnits(const tools::Rectangle& rWindowRect) const;
    tools::Rectangle WindowToLogicUnits(const tools::Rectangle& rWindowRect,
                                        const ImplMapRes& rMapRes) const;
    tools::Rectangle WindowToLogicUnits(const tools::Rectangle& rWindowRect,
                                        const MapMode& rMapMode) const;

    tools::Polygon WindowToLogicUnits(const tools::Polygon& rWindowPoly) const;
    tools::Polygon WindowToLogicUnits(const tools::Polygon& rWindowPoly,
                                      const ImplMapRes& rMapRes) const;
    tools::Polygon WindowToLogicUnits(const tools::Polygon& rWindowPoly,
                                      const MapMode& rMapMode) const;

    tools::PolyPolygon WindowToLogicUnits(const tools::PolyPolygon& rWindowPolyPoly) const;
    Point WindowSubPixelToLogicUnits(const basegfx::B2DPoint& rWindowPt) const;

    Size WindowToLogicUnits(const Size& rWindowSize) const;
    Size WindowToLogicUnits(const Size& rWindowSize, const ImplMapRes& rMapRes) const;
    Size WindowToLogicUnits(const Size& rWindowSize, const MapMode& rMapMode) const;

    template <TransformableB2DGeometry T> T WindowToLogicUnits(const T& rWindowGeometry) const;

    template <TransformableB2DGeometry T>
    T WindowToLogicUnits(const T& rWindowGeometry, const MapMode& rMapMode) const;

    // ========================================================================
    // MASTER STAGES / LOGIC-TO-LOGIC CONVERSIONS
    // ========================================================================
    // Pure mathematical transformations between arbitrary MapModes.
    // These do not traverse the VCL device pipeline.

    Point LogicToLogic(const Point& rPtSource, const MapMode* pMapModeSource,
                       const MapMode* pMapModeDest) const;
    Size LogicToLogic(const Size& rSzSource, const MapMode* pMapModeSource,
                      const MapMode* pMapModeDest) const;
    tools::Rectangle LogicToLogic(const tools::Rectangle& rRectSource,
                                  const MapMode* pMapModeSource, const MapMode* pMapModeDest) const;

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

    // Universal basegfx pipeline
    template <vcl::detail::B2DGeometry T> T LogicToDeviceSubPixel(T aObj) const
    {
        auto snap = AcquireSnapshot();
        if constexpr (vcl::detail::B2DTransformable<T>)
            aObj.transform(snap->maLogicToDevice);
        else
            aObj *= snap->maLogicToDevice;
        return aObj;
    }

    // Universal inverse basegfx pipeline
    template <vcl::detail::B2DGeometry T> T DevicePixelToLogicSubPixel(T aObj) const
    {
        auto snap = AcquireSnapshot();
        if constexpr (vcl::detail::B2DTransformable<T>)
            aObj.transform(snap->maDeviceToLogic);
        else
            aObj *= snap->maDeviceToLogic;
        return aObj;
    }

private:
    void UpdateTransforms() const;
    void GetLogicToViewWeights(double& rScaleX, double& rScaleY, double& rTransX,
                               double& rTransY) const;
};

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
