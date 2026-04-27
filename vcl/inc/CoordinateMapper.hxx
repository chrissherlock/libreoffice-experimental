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

#include <MappingCoefficients.hxx>

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

struct MapConversion
{
    double mfScaleX = 1.0;
    double mfScaleY = 1.0;
    tools::Long mnOffsetX = 0;
    tools::Long mnOffsetY = 0;
};
}

class VCL_DLLPUBLIC CoordinateMapper
{
private:
    MapMode maMapMode;
    MappingCoefficients maMapRes;
    vcl::detail::MapConversion maMapConversion;

    // #i75163#
    struct TransformSnapshot
    {
        basegfx::B2DHomMatrix maLogicToDevice;
        basegfx::B2DHomMatrix maDeviceToLogic;
        basegfx::B2DHomMatrix maView;
        basegfx::B2DHomMatrix maInvView;
        uint64_t mnVersion = 0;
    };

    // Separate snapshots for mapped/unmapped coordinate spaces
    // because DPI and window/device offsets diverge significantly.
    mutable std::shared_ptr<const TransformSnapshot> mpSnapshots[2];
    mutable std::atomic<uint64_t> mnStateVersion{ 0 };

    std::shared_ptr<const TransformSnapshot> AcquireSnapshot(bool bMap) const;
    std::shared_ptr<TransformSnapshot> BuildSnapshot(bool bMap) const;

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
    bool IsValidDPI() const { return mnDPIX > 0 && mnDPIY > 0; }

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

    Size LogicToViewDistance(const Size& rLogicSize, bool bMap) const;

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

    tools::Long GetMappingXOffset() const { return maMapRes.mnTranslationX; }
    tools::Long GetMappingYOffset() const { return maMapRes.mnTranslationY; }
    double GetMapResolutionScaleX() const { return maMapRes.mfScaleX; }
    double GetMapResolutionScaleY() const { return maMapRes.mfScaleY; }

    void SetMappingXOffset(tools::Long nOffset)
    {
        maMapRes.mnTranslationX = nOffset;
        maMapConversion.mnOffsetX = nOffset; // <-- SYNC THE FIREWALL
        InvalidateViewTransform();
    }

    void SetMappingYOffset(tools::Long nOffset)
    {
        maMapRes.mnTranslationY = nOffset;
        maMapConversion.mnOffsetY = nOffset; // <-- SYNC THE FIREWALL
        InvalidateViewTransform();
    }

    void SetMapResolutionScaleX(double fX)
    {
        maMapRes.mfScaleX = fX;
        maMapConversion.mfScaleX = fX; // <-- SYNC THE FIREWALL
        InvalidateViewTransform();
    }

    void SetMapResolutionScaleY(double fY)
    {
        maMapRes.mfScaleY = fY;
        maMapConversion.mfScaleY = fY; // <-- SYNC THE FIREWALL
        InvalidateViewTransform();
    }

    void CalcMapResolution(const MapMode& rMapMode, tools::Long nDPIX, tools::Long nDPIY);

    vcl::detail::MapConversion ResolveMap(const MapMode& rMapMode, bool bMap) const;

    /** Invalidate the view transformation.

     @since AOO bug 75163 (OpenOffice.org 2.4.3 - OOH 680 milestone 212)
     */
    void InvalidateViewTransform();
    basegfx::B2DHomMatrix GetViewTransformation(bool bMap) const;
    basegfx::B2DHomMatrix GetViewTransformation(const vcl::detail::MapConversion& rConv) const;
    basegfx::B2DHomMatrix GetViewTransformation(const MapMode& rMapMode, bool bMap) const;

    basegfx::B2DHomMatrix GetInverseViewTransformation(bool bMap) const;
    basegfx::B2DHomMatrix
    GetInverseViewTransformation(const vcl::detail::MapConversion& rConv) const;
    basegfx::B2DHomMatrix GetInverseViewTransformation(const MapMode& rMapMode, bool bMap) const;

    basegfx::B2DHomMatrix GetDeviceTransformation(bool bMap) const;

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
    tools::Long ViewToLogicUnitsX(tools::Long nX, bool bMap) const;
    tools::Long ViewToLogicUnitsY(tools::Long nY, bool bMap) const;
    vcl::Region ViewToDevice(const vcl::Region& rRegion) const;

    tools::Long LogicUnitsToViewUnitsX(tools::Long nX, bool bMap) const;
    tools::Long LogicUnitsToViewUnitsY(tools::Long nY, bool bMap) const;
    tools::Long LogicUnitsToViewUnitsX(tools::Long nX,
                                       const vcl::detail::MapConversion& rConv) const;
    tools::Long LogicUnitsToViewUnitsY(tools::Long nY,
                                       const vcl::detail::MapConversion& rConv) const;

    // View <-> LogicUnits (Sub-pixel)
    double ViewSubPixelToLogicUnitsX(double fX, bool bMap) const;
    double ViewSubPixelToLogicUnitsY(double fY, bool bMap) const;
    tools::Long ViewSubPixelToLogicUnitsIntX(double fX, bool bMap) const;
    tools::Long ViewSubPixelToLogicUnitsIntY(double fY, bool bMap) const;
    double LogicUnitsToViewSubPixelX(double fX, bool bMap) const;
    double LogicUnitsToViewSubPixelY(double fY, bool bMap) const;

    // ========================================================================
    // MASTER WRAPPERS (Multi-space Positional Transformations)
    // ========================================================================

    // Device <-> Logic (Full journey)
    tools::Long LogicToDevicePixelX(tools::Long nX, bool bMap) const;
    tools::Long LogicToDevicePixelY(tools::Long nY, bool bMap) const;
    tools::Long LogicWidthToDevicePixel(tools::Long nWidth, bool bMap) const;
    double LogicWidthToDeviceSubPixel(tools::Long nWidth, bool bMap) const;
    tools::Long LogicHeightToDevicePixel(tools::Long nHeight, bool bMap) const;
    Point LogicToDevicePixel(const Point& rLogicPt, bool bMap) const;
    Size LogicToDevicePixel(const Size& rLogicSize, bool bMap) const;
    tools::Rectangle LogicToDevicePixel(const tools::Rectangle& rLogicRect, bool bMap) const;
    tools::Polygon LogicToDevicePixel(const tools::Polygon& rLogicPoly, bool bMap) const;
    tools::PolyPolygon LogicToDevicePixel(const tools::PolyPolygon& rLogicPolyPoly,
                                          bool bMap) const;
    LineInfo LogicToDevicePixel(const LineInfo& rLineInfo, bool bMap) const;
    basegfx::B2DPolygon LogicToDevicePixel(const basegfx::B2DPolygon& rLogicPoly, bool bMap) const;
    double LogicToDeviceSubPixelX(double fX, bool bMap) const;
    double LogicToDeviceSubPixelY(double fY, bool bMap) const;
    basegfx::B2DPoint LogicToDeviceSubPixel(const Point& rPoint, bool bMap) const;

    tools::Long DevicePixelToLogicX(tools::Long nX, bool bMap) const;
    tools::Long DevicePixelToLogicY(tools::Long nY, bool bMap) const;
    tools::Long DevicePixelToLogicWidth(tools::Long nWidth, bool bMap) const;
    tools::Long DevicePixelToLogicHeight(tools::Long nHeight, bool bMap) const;
    Point DevicePixelToLogic(const Point& rDevicePt, bool bMap) const;
    Size DevicePixelToLogic(const Size& rDeviceSize, bool bMap) const;
    tools::Rectangle DevicePixelToLogic(const tools::Rectangle& rPixelRect, bool bMap) const;
    double DevicePixelToLogicSubPixelX(double fX, bool bMap) const;
    double DevicePixelToLogicSubPixelY(double fY, bool bMap) const;
    basegfx::B2DPoint DevicePixelToLogicSubPixel(const Point& rDevicePt, bool bMap) const;

    // Window <-> Logic
    tools::Long WindowToLogicX(tools::Long nX, bool bMap) const;
    tools::Long WindowToLogicY(tools::Long nY, bool bMap) const;
    double WindowToLogicSubPixelX(double fX, bool bMap) const;
    double WindowToLogicSubPixelY(double fY, bool bMap) const;

    double LogicToWindowSubPixelX(double fX, bool bMap) const;
    double LogicToWindowSubPixelY(double fY, bool bMap) const;
    tools::Long LogicToWindowX(tools::Long nX, bool bMap) const;
    tools::Long LogicToWindowY(tools::Long nY, bool bMap) const;
    double LogicWidthToWindowSubPixel(tools::Long nWidth, bool bMap) const;
    double LogicHeightToWindowSubPixel(tools::Long nHeight, bool bMap) const;

    // View <-> Absolute Logic (Includes mnLogicToAbsoluteOffsetX/Y)
    tools::Long ViewToLogicX(tools::Long nX, bool bMap) const;
    tools::Long ViewToLogicY(tools::Long nY, bool bMap) const;

    // To resolve the return-type conflict, these now return double
    double ViewSubPixelToLogicX(double fX, bool bMap) const;
    double ViewSubPixelToLogicY(double fY, bool bMap) const;
    // View (Sub-pixel) -> Absolute Logic (Integer)
    // Note: This rounds the distance before stripping offsets to satisfy legacy test parity.
    tools::Long ViewSubPixelToLogicIntX(double fX) const;
    tools::Long ViewSubPixelToLogicIntY(double fY) const;
    tools::Long ViewSubPixelToLogicIntX(double fX, const vcl::detail::MapConversion& rConv) const;
    tools::Long ViewSubPixelToLogicIntY(double fY, const vcl::detail::MapConversion& rConv) const;
    double LogicToViewSubPixelX(double fX) const;
    double LogicToViewSubPixelY(double fY) const;

    // Logic -> Window units (Commonly used in OutputDevice::LogicToPixel)
    tools::Long LogicToWindowUnitsX(tools::Long nX, bool bMap) const;
    tools::Long LogicToWindowUnitsY(tools::Long nY, bool bMap) const;
    tools::Long LogicToWindowUnitsX(tools::Long nX, const vcl::detail::MapConversion& rConv) const;
    tools::Long LogicToWindowUnitsY(tools::Long nY, const vcl::detail::MapConversion& rConv) const;
    Point LogicToWindowUnits(const Point& rLogicPt, bool bMap) const;
    Point LogicToWindowUnits(const Point& rLogicPt, const vcl::detail::MapConversion& rConv) const;
    Size LogicToWindowUnits(const Size& rLogicSize, const vcl::detail::MapConversion& rConv) const;
    tools::Rectangle LogicToWindowUnits(const tools::Rectangle& rRect,
                                        const vcl::detail::MapConversion& rConv) const;
    tools::Rectangle LogicToWindowUnits(const tools::Rectangle& rRect, bool bMap) const;
    Size LogicToWindowUnits(const Size& rLogicSize, bool bMap) const;
    vcl::Region LogicToWindowUnits(const vcl::Region& rRegion, bool bMap) const;
    tools::Polygon LogicToWindowUnits(const tools::Polygon& rPoly, bool bMap) const;
    tools::Polygon LogicToWindowUnits(const tools::Polygon& rPoly,
                                      const vcl::detail::MapConversion& rConv) const;
    tools::PolyPolygon LogicToWindowUnits(const tools::PolyPolygon& rPoly, bool bMap) const;
    tools::PolyPolygon LogicToWindowUnits(const tools::PolyPolygon& rPoly,
                                          const vcl::detail::MapConversion& rConv) const;

    template <TransformableB2DGeometry T>
    T LogicToWindowUnits(const T& rLogicGeometry, bool bMap) const;

    template <TransformableB2DGeometry T>
    T LogicToWindowUnits(const T& rLogicGeometry, const vcl::detail::MapConversion& rConv) const;

    tools::Long WindowSubPixelToLogicIntX(double fX, bool bMap) const;
    tools::Long WindowSubPixelToLogicIntY(double fY, bool bMap) const;
    tools::Long WindowSubPixelToLogicIntX(double fX, const vcl::detail::MapConversion& rConv) const;
    tools::Long WindowSubPixelToLogicIntY(double fY, const vcl::detail::MapConversion& rConv) const;
    vcl::Region WindowToLogicUnits(const vcl::Region& rWindowRegion, bool bmap) const;
    Point WindowToLogicUnits(const Point& rWindowPt, bool bMap) const;
    Point WindowToLogicUnits(const Point& rWindowPt, const vcl::detail::MapConversion& rConv) const;

    tools::Rectangle WindowToLogicUnits(const tools::Rectangle& rWindowRect, bool bMap) const;
    tools::Rectangle WindowToLogicUnits(const tools::Rectangle& rWindowRect,
                                        const vcl::detail::MapConversion& rConv) const;

    tools::Polygon WindowToLogicUnits(const tools::Polygon& rWindowPoly, bool bMap) const;
    tools::Polygon WindowToLogicUnits(const tools::Polygon& rWindowPoly,
                                      const vcl::detail::MapConversion& rConv) const;

    tools::PolyPolygon WindowToLogicUnits(const tools::PolyPolygon& rWindowPolyPoly,
                                          bool bMap) const;
    Point WindowSubPixelToLogicUnits(const basegfx::B2DPoint& rWindowPt, bool bMap) const;

    Size WindowToLogicUnits(const Size& rWindowSize, bool bMap) const;
    Size WindowToLogicUnits(const Size& rWindowSize, const vcl::detail::MapConversion& rConv) const;

    template <TransformableB2DGeometry T>
    T WindowToLogicUnits(const T& rWindowGeometry, bool bMap) const;

    template <TransformableB2DGeometry T>
    T WindowToLogicUnits(const T& rWindowGeometry, const vcl::detail::MapConversion& rConv) const;

    // ========================================================================
    // MASTER STAGES / LOGIC-TO-LOGIC CONVERSIONS
    // ========================================================================
    // Pure mathematical transformations between arbitrary MapModes.
    // These do not traverse the VCL device pipeline.

    Point LogicToLogic(const Point& rPtSource, const MapMode* pMapModeBaseline,
                       const MapMode* pMapModeSource, const MapMode* pMapModeDest, bool bMap) const;
    Size LogicToLogic(const Size& rSzSource, const MapMode* pMapModeBaseline,
                      const MapMode* pMapModeSource, const MapMode* pMapModeDest, bool bMap) const;
    tools::Rectangle LogicToLogic(const tools::Rectangle& rRectSource,
                                  const MapMode* pMapModeBaseline, const MapMode* pMapModeSource,
                                  const MapMode* pMapModeDest, bool bMap) const;

    // ========================================================================
    // DISTANCE SCALING (Raw Scalar Conversion, NO offsets applied)
    // ========================================================================

    // Integer Distances
    tools::Long LogicToViewDistanceX(tools::Long n, bool bMap) const;
    tools::Long LogicToViewDistanceY(tools::Long n, bool bMap) const;
    tools::Long LogicToViewDistanceX(tools::Long n, double fScale) const;
    tools::Long LogicToViewDistanceY(tools::Long n, double fScale) const;
    tools::Long ViewToLogicDistanceX(tools::Long n, bool bMap) const;
    tools::Long ViewToLogicDistanceY(tools::Long n, bool bMap) const;
    tools::Long ViewToLogicDistanceX(tools::Long n, double fScale) const;
    tools::Long ViewToLogicDistanceY(tools::Long n, double fScale) const;

    // Double/Sub-Pixel Distances
    double LogicToViewDistanceSubPixelX(tools::Long n, bool bMap) const;
    double LogicToViewDistanceSubPixelY(tools::Long n, bool bMap) const;
    double LogicToViewDistanceSubPixelX(tools::Long n, double fScale) const;
    double LogicToViewDistanceSubPixelY(tools::Long n, double fScale) const;
    double ViewToLogicDistanceDoubleX(double n, bool bMap) const;
    double ViewToLogicDistanceDoubleY(double n, bool bMap) const;
    double ViewToLogicDistanceDoubleX(double n, double fScale) const;
    double ViewToLogicDistanceDoubleY(double n, double fScale) const;
    tools::Long ViewSubPixelToLogicDistanceX(double n) const;
    tools::Long ViewSubPixelToLogicDistanceY(double n) const;
    tools::Long ViewSubPixelToLogicDistanceX(double n, double fScale) const;
    tools::Long ViewSubPixelToLogicDistanceY(double n, double fScale) const;

    // Universal basegfx pipeline
    template <vcl::detail::B2DGeometry T> T LogicToDeviceSubPixel(T aObj, bool bMap) const
    {
        auto snap = AcquireSnapshot(bMap);
        if constexpr (vcl::detail::B2DTransformable<T>)
            aObj.transform(snap->maLogicToDevice);
        else
            aObj *= snap->maLogicToDevice;
        return aObj;
    }

    // Universal inverse basegfx pipeline
    template <vcl::detail::B2DGeometry T> T DevicePixelToLogicSubPixel(T aObj, bool bMap) const
    {
        auto snap = AcquireSnapshot(bMap);
        if constexpr (vcl::detail::B2DTransformable<T>)
            aObj.transform(snap->maDeviceToLogic);
        else
            aObj *= snap->maDeviceToLogic;
        return aObj;
    }

private:
    MappingCoefficients ResolveMapResRelative(const MapMode* pBaseline, const MapMode* pTarget,
                                              bool bMap) const;
    void UpdateTransforms() const;
    void GetLogicToViewWeights(double& rScaleX, double& rScaleY, double& rTransX, double& rTransY,
                               bool bMap) const;
};

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
