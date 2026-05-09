/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <sal/types.h>
#include <tools/long.hxx>
#include <o3tl/hash_combine.hxx>
#include <basegfx/matrix/b2dhommatrix.hxx>
#include <basegfx/range/b2drange.hxx>

#include <o3tl/hash_combine.hxx>
#include <basegfx/matrix/b2dhommatrix.hxx>
#include <basegfx/range/b2drange.hxx>

#include <vcl/dllapi.h>
#include <vcl/mapconvert.hxx>
#include <vcl/mapmod.hxx>
#include <vcl/region.hxx>

#include <MappingCoefficients.hxx>

#include <optional>
#include <atomic>
#include <memory>
#include <concepts>
#include <type_traits>
#include <array>

class LineInfo;

template <typename T>
concept TransformableB2DGeometry = requires(T a, const basegfx::B2DHomMatrix& rMatrix)
{
    a.transform(rMatrix);
};

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

class CoordinateMapper;

// The strict execution instruction set
enum class TransformMode
{
    Identity,
    Translation,
    AffineFallback
};

enum class CoordinateSpace
{
    Logic,
    Window,
    Device
};

struct TransformRequest
{
    CoordinateSpace eFrom = CoordinateSpace::Logic;
    CoordinateSpace eTo = CoordinateSpace::Device;
    bool bApplyMapping = true;
};

// Explicit slots for the O(1) Transform Register File
enum class TransformSlot : size_t
{
    LogicToWindow_Mapped = 0,
    LogicToWindow_Unmapped = 1,
    LogicToDevice_Mapped = 2,
    LogicToDevice_Unmapped = 3,
    WindowToLogic_Mapped = 4,
    WindowToLogic_Unmapped = 5,
    DeviceToLogic_Mapped = 6,
    DeviceToLogic_Unmapped = 7,
    Count = 8
};

enum class GeometryInvariant : size_t
{
    AxisAlignment, // Edges remain parallel to X/Y axes (Critical for Rectangle/Scalar)
    Orthogonality, // Basis vectors remain 90° to each other (Critical for Shear-safety)
    Orientation, // Handedness/Mirroring state (Critical for Size)
    Parallelism, // Parallel lines stay parallel (Always true for Affine)
    Connectivity, // Shapes stay "in one piece" (Always true for Affine)
    COUNT
};

struct TransformContract
{
    std::bitset<static_cast<size_t>(GeometryInvariant::COUNT)> maPreserved;

    bool preserves(GeometryInvariant inv) const
    {
        return maPreserved.test(static_cast<size_t>(inv));
    }
};

struct VCL_DLLPUBLIC CompiledTransform
{
public:
    basegfx::B2DHomMatrix maMatrix;
    uint64_t mnSemanticKey = 0;

    TransformMode meMode = TransformMode::AffineFallback;

    tools::Long mnLogicTx = 0, mnLogicTy = 0;
    tools::Long mnDeviceTx = 0, mnDeviceTy = 0;

    TransformContract maContract;

public:
    CompiledTransform() = default;

    const TransformContract& GetContract() const { return maContract; }
    bool PreservesAxisAlignment() const
    {
        return maContract.preserves(GeometryInvariant::AxisAlignment);
    }
    bool CheckRectilinearContract() const;

    uint64_t GetSemanticKey() const { return mnSemanticKey; }
    TransformMode GetMode() const { return meMode; }

    tools::Long GetLogicTx() const { return mnLogicTx; }
    tools::Long GetLogicTy() const { return mnLogicTy; }
    tools::Long GetDeviceTx() const { return mnDeviceTx; }
    tools::Long GetDeviceTy() const { return mnDeviceTy; }

    const basegfx::B2DHomMatrix& GetMatrix() const { return maMatrix; }

    template <typename T> T Apply(const T& rGeometry) const;

    bool IsIdentity() const { return meMode == TransformMode::Identity; }
    bool IsPureTranslation() const { return meMode == TransformMode::Translation; }

    Size ApplyRectilinear(const Size& rSize) const;
    tools::Rectangle ApplyRectilinear(const tools::Rectangle& rRect) const;
};

// Declare explicit specializations to prevent implicit instantiation errors
template <> VCL_DLLPUBLIC Point CompiledTransform::Apply<Point>(const Point& rPt) const;
template <> VCL_DLLPUBLIC Size CompiledTransform::Apply<Size>(const Size& rSize) const;
template <>
VCL_DLLPUBLIC tools::Rectangle
CompiledTransform::Apply<tools::Rectangle>(const tools::Rectangle& rRect) const;
template <>
VCL_DLLPUBLIC tools::Polygon
CompiledTransform::Apply<tools::Polygon>(const tools::Polygon& rPoly) const;
template <>
VCL_DLLPUBLIC tools::PolyPolygon
CompiledTransform::Apply<tools::PolyPolygon>(const tools::PolyPolygon& rPolyPoly) const;
template <>
VCL_DLLPUBLIC basegfx::B2DPolygon
CompiledTransform::Apply<basegfx::B2DPolygon>(const basegfx::B2DPolygon& rPoly) const;
template <>
VCL_DLLPUBLIC basegfx::B2DPolyPolygon
CompiledTransform::Apply<basegfx::B2DPolyPolygon>(const basegfx::B2DPolyPolygon& rPolyPoly) const;
template <>
VCL_DLLPUBLIC vcl::Region CompiledTransform::Apply<vcl::Region>(const vcl::Region& rRegion) const;
template <>
VCL_DLLPUBLIC LineInfo CompiledTransform::Apply<LineInfo>(const LineInfo& rLineInfo) const;
template <>
VCL_DLLPUBLIC basegfx::B2DRange
CompiledTransform::Apply<basegfx::B2DRange>(const basegfx::B2DRange& rRange) const;

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
 *
 * THREADING CONTRACT:
 * CoordinateMapper is NOT internally synchronized. The Transform Register File
 * (maTransformCache) and cache versioning rely on thread confinement or
 * external synchronization (e.g., the Solar Mutex).
 */

class VCL_DLLPUBLIC CoordinateMapper
{
private:
    MappingCoefficients maMapRes;
    vcl::detail::MapConversion maMapConversion;

    // The O(1) Transform Register File & Version Tracker
    mutable std::atomic<uint64_t> mnStateVersion{ 0 };
    mutable uint64_t mnCacheVersion{ 0 };
    mutable std::array<std::optional<CompiledTransform>, static_cast<size_t>(TransformSlot::Count)>
        maTransformCache;

    sal_Int32 mnDPIX = 72;
    sal_Int32 mnDPIY = 72;
    sal_Int32 mnDPIScalePercentage = 100;

    tools::Long mnDeviceToWindowOffsetX = 0;
    tools::Long mnDeviceToWindowOffsetY = 0;

    tools::Long mnWindowToViewOffsetX = 0;
    tools::Long mnWindowToViewOffsetY = 0;

    tools::Long mnLogicToAbsoluteOffsetX = 0;
    tools::Long mnLogicToAbsoluteOffsetY = 0;

    tools::Long mnOutWidth = 0;
    tools::Long mnOutHeight = 0;

    void UpdateCache(bool bMap) const;

public:
    const CompiledTransform& Compile(const TransformRequest& rReq) const;

    // Legacy bridge
    const CompiledTransform& Compile(bool bMap) const
    {
        return Compile({ CoordinateSpace::Logic, CoordinateSpace::Device, bMap });
    }

    uint64_t GetSemanticKey(bool bMap) const;

    bool IsValidDPI() const { return mnDPIX > 0 && mnDPIY > 0; }

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
    void SetMapMode(const MapMode& rMapMode) { CalcMapResolution(rMapMode, mnDPIX, mnDPIY); }
    void SetLogicOffset(const Size& rOffset) { SetLogicToAbsoluteOffset(rOffset); }
    void SetWindowOffset(const Size& rOffset) { SetWindowToViewOffset(rOffset); }

    Size LogicToViewDistance(const Size& rLogicSize, bool bMap) const;

    tools::Long GetOutputWidthPixel() const;
    tools::Long GetOutputHeightPixel() const;
    Size GetOutputSizePixel() const;

    void SetOutputWidthPixel(tools::Long nWidth);
    void SetOutputHeightPixel(tools::Long nHeight);

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

    vcl::detail::MapConversion ResolveMap(const MapMode& rBaseline, const MapMode& rTarget,
                                          bool bMap) const;

    void InvalidateViewTransform();
    basegfx::B2DHomMatrix GetViewTransformation(bool bMap) const;
    basegfx::B2DHomMatrix GetViewTransformation(const vcl::detail::MapConversion& rConv) const;
    basegfx::B2DHomMatrix GetViewTransformation(const MapMode& rBaseline, const MapMode& rTarget,
                                                bool bMap) const;

    basegfx::B2DHomMatrix GetInverseViewTransformation(bool bMap) const;
    basegfx::B2DHomMatrix
    GetInverseViewTransformation(const vcl::detail::MapConversion& rConv) const;
    basegfx::B2DHomMatrix GetInverseViewTransformation(const MapMode& rBaseline,
                                                       const MapMode& rTarget, bool bMap) const;

    basegfx::B2DHomMatrix GetDeviceTransformation(bool bMap) const;

    // --- PIPELINE MATRIX BUILDERS (Now single-source-of-truth projections) ---
    basegfx::B2DHomMatrix GetLogicToWindowMatrix(bool bMap) const;
    basegfx::B2DHomMatrix GetWindowToLogicMatrix(bool bMap) const;
    basegfx::B2DHomMatrix GetLogicToDeviceMatrix(bool bMap) const;
    basegfx::B2DHomMatrix GetDeviceToLogicMatrix(bool bMap) const;

    // ========================================================================
    // PIPELINE STAGES (Coordinate Transitions)
    // ========================================================================

    tools::Long DeviceToWindowUnitsX(tools::Long nX) const;
    tools::Long DeviceToWindowUnitsY(tools::Long nY) const;
    tools::Long WindowToDeviceUnitsX(tools::Long nX) const;
    tools::Long WindowToDeviceUnitsY(tools::Long nY) const;

    double DeviceToWindowSubPixelX(double fX) const;
    double DeviceToWindowSubPixelY(double fY) const;
    double WindowToDeviceSubPixelX(double fX) const;
    double WindowToDeviceSubPixelY(double fY) const;

    tools::Long WindowToViewUnitsX(tools::Long nX) const;
    tools::Long WindowToViewUnitsY(tools::Long nY) const;
    tools::Long ViewToWindowUnitsX(tools::Long nX) const;
    tools::Long ViewToWindowUnitsY(tools::Long nY) const;

    double WindowToViewSubPixelX(double fX) const;
    double WindowToViewSubPixelY(double fY) const;
    double ViewToWindowSubPixelX(double fX) const;
    double ViewToWindowSubPixelY(double fY) const;

    vcl::Region ViewToDevice(const vcl::Region& rRegion) const;
    tools::Long LogicUnitsToViewUnitsX(tools::Long nX,
                                       const vcl::detail::MapConversion& rConv) const;
    tools::Long LogicUnitsToViewUnitsY(tools::Long nY,
                                       const vcl::detail::MapConversion& rConv) const;

    // ========================================================================
    // MASTER WRAPPERS (Multi-space Positional Transformations)
    // ========================================================================

    tools::Long LogicWidthToDevicePixel(tools::Long nWidth, bool bMap) const;
    double LogicWidthToDeviceSubPixel(tools::Long nWidth, bool bMap) const;
    tools::Long LogicHeightToDevicePixel(tools::Long nHeight, bool bMap) const;
    Point LogicToDevicePixel(const Point& rLogicPt, bool bMap = true) const;
    Size LogicToDevicePixel(const Size& rLogicSize, bool bMap = true) const;
    tools::Rectangle LogicToDevicePixel(const tools::Rectangle& rLogicRect, bool bMap = true) const;
    tools::Polygon LogicToDevicePixel(const tools::Polygon& rLogicPoly, bool bMap = true) const;
    tools::PolyPolygon LogicToDevicePixel(const tools::PolyPolygon& rLogicPolyPoly,
                                          bool bMap = true) const;
    LineInfo LogicToDevicePixel(const LineInfo& rLineInfo, bool bMap = true) const;
    basegfx::B2DPolygon LogicToDevicePixel(const basegfx::B2DPolygon& rLogicPoly,
                                           bool bMap = true) const;
    basegfx::B2DPoint LogicToDeviceSubPixel(const Point& rPoint, bool bMap) const;
    basegfx::B2DPolyPolygon LogicToDevicePixel(const basegfx::B2DPolyPolygon& rLogicPolyPoly,
                                               bool bMap = true) const;

    tools::Long DevicePixelToLogicWidth(tools::Long nWidth, bool bMap) const;
    tools::Long DevicePixelToLogicHeight(tools::Long nHeight, bool bMap) const;
    Point DevicePixelToLogic(const Point& rDevicePt, bool bMap = true) const;
    Size DevicePixelToLogic(const Size& rDeviceSize, bool bMap = true) const;
    tools::Rectangle DevicePixelToLogic(const tools::Rectangle& rPixelRect, bool bMap = true) const;
    basegfx::B2DPoint DevicePixelToLogicSubPixel(const Point& rDevicePt, bool bMap) const;

    tools::Polygon DevicePixelToLogic(const tools::Polygon& rPixelPoly, bool bMap = true) const;
    tools::PolyPolygon DevicePixelToLogic(const tools::PolyPolygon& rPixelPolyPoly,
                                          bool bMap = true) const;
    basegfx::B2DPolygon DevicePixelToLogic(const basegfx::B2DPolygon& rPixelPoly,
                                           bool bMap = true) const;
    basegfx::B2DPolyPolygon DevicePixelToLogic(const basegfx::B2DPolyPolygon& rPixelPolyPoly,
                                               bool bMap = true) const;

    vcl::Region LogicToDevicePixel(const vcl::Region& rLogicRegion, bool bMap = true) const;
    vcl::Region DevicePixelToLogic(const vcl::Region& rPixelRegion, bool bMap = true) const;

    // Window <-> Logic

    double LogicToWindowSubPixelX(double fX, bool bMap) const;
    double LogicToWindowSubPixelY(double fY, bool bMap) const;
    tools::Long LogicToWindowX(tools::Long nX, bool bMap = true) const;
    tools::Long LogicToWindowY(tools::Long nY, bool bMap = true) const;
    double LogicWidthToWindowSubPixel(tools::Long nWidth, bool bMap) const;
    double LogicHeightToWindowSubPixel(tools::Long nHeight, bool bMap) const;

    double ViewSubPixelToLogicX(double fX, bool bMap) const;
    double ViewSubPixelToLogicY(double fY, bool bMap) const;
    double LogicToViewSubPixelX(double fX) const;
    double LogicToViewSubPixelY(double fY) const;

    Point LogicToWindowUnits(const Point& rLogicPt, bool bMap = true) const;
    Point LogicToWindowUnits(const Point& rLogicPt, const vcl::detail::MapConversion& rConv) const;
    Size LogicToWindowUnits(const Size& rLogicSize, const vcl::detail::MapConversion& rConv) const;
    tools::Rectangle LogicToWindowUnits(const tools::Rectangle& rRect,
                                        const vcl::detail::MapConversion& rConv) const;
    tools::Rectangle LogicToWindowUnits(const tools::Rectangle& rRect, bool bMap = true) const;
    Size LogicToWindowUnits(const Size& rLogicSize, bool bMap = true) const;
    vcl::Region LogicToWindowUnits(const vcl::Region& rRegion, bool bMap = true) const;
    tools::Polygon LogicToWindowUnits(const tools::Polygon& rPoly, bool bMap = true) const;
    tools::Polygon LogicToWindowUnits(const tools::Polygon& rPoly,
                                      const vcl::detail::MapConversion& rConv) const;
    tools::PolyPolygon LogicToWindowUnits(const tools::PolyPolygon& rPoly, bool bMap = true) const;
    tools::PolyPolygon LogicToWindowUnits(const tools::PolyPolygon& rPoly,
                                          const vcl::detail::MapConversion& rConv) const;

    template <TransformableB2DGeometry T>
    T LogicToWindowUnits(const T& rLogicGeometry, bool bMap = true) const;

    template <TransformableB2DGeometry T>
    T LogicToWindowUnits(const T& rLogicGeometry, const vcl::detail::MapConversion& rConv) const;
    vcl::Region WindowToLogicUnits(const vcl::Region& rWindowRegion, bool bmap) const;
    Point WindowToLogicUnits(const Point& rWindowPt, bool bMap = true) const;
    Point WindowToLogicUnits(const Point& rWindowPt, const vcl::detail::MapConversion& rConv) const;

    tools::Rectangle WindowToLogicUnits(const tools::Rectangle& rWindowRect,
                                        bool bMap = true) const;
    tools::Rectangle WindowToLogicUnits(const tools::Rectangle& rWindowRect,
                                        const vcl::detail::MapConversion& rConv) const;

    tools::Polygon WindowToLogicUnits(const tools::Polygon& rWindowPoly, bool bMap = true) const;
    tools::Polygon WindowToLogicUnits(const tools::Polygon& rWindowPoly,
                                      const vcl::detail::MapConversion& rConv) const;

    tools::PolyPolygon WindowToLogicUnits(const tools::PolyPolygon& rWindowPolyPoly,
                                          bool bMap) const;
    Point WindowSubPixelToLogicUnits(const basegfx::B2DPoint& rWindowPt, bool bMap) const;

    Size WindowToLogicUnits(const Size& rWindowSize, bool bMap = true) const;
    Size WindowToLogicUnits(const Size& rWindowSize, const vcl::detail::MapConversion& rConv) const;

    template <TransformableB2DGeometry T>
    T WindowToLogicUnits(const T& rWindowGeometry, bool bMap = true) const;

    template <TransformableB2DGeometry T>
    T WindowToLogicUnits(const T& rWindowGeometry, const vcl::detail::MapConversion& rConv) const;

    // ========================================================================
    // MASTER STAGES / LOGIC-TO-LOGIC CONVERSIONS
    // ========================================================================

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

    // Double/Sub-Pixel Distances
    tools::Long ViewSubPixelToLogicDistanceX(double n) const;
    tools::Long ViewSubPixelToLogicDistanceY(double n) const;
    tools::Long ViewSubPixelToLogicDistanceX(double n, double fScale) const;
    tools::Long ViewSubPixelToLogicDistanceY(double n, double fScale) const;

    // Universal basegfx pipeline
    template <vcl::detail::B2DGeometry T> T LogicToDeviceSubPixel(T aObj, bool bMap) const
    {
        if constexpr (vcl::detail::B2DTransformable<T>)
            aObj.transform(GetLogicToDeviceMatrix(bMap));
        else
            aObj *= GetLogicToDeviceMatrix(bMap);
        return aObj;
    }

    // Universal inverse basegfx pipeline
    template <vcl::detail::B2DGeometry T> T DevicePixelToLogicSubPixel(T aObj, bool bMap) const
    {
        if constexpr (vcl::detail::B2DTransformable<T>)
            aObj.transform(GetDeviceToLogicMatrix(bMap));
        else
            aObj *= GetDeviceToLogicMatrix(bMap);
        return aObj;
    }

private:
    MappingCoefficients ResolveMapResRelative(const MapMode* pBaseline, const MapMode* pTarget,
                                              bool bMap) const;
    void GetLogicToViewWeights(double& rScaleX, double& rScaleY, double& rTransX, double& rTransY,
                               bool bMap) const;

    CompiledTransform BuildCompiledTransform(const basegfx::B2DHomMatrix& rMat) const;
};

Point LogicToLogic(const Point& rPtSource, const MapMode& rMapModeSource,
                   const MapMode& rMapModeDest);
Size LogicToLogic(const Size& rSzSource, const MapMode& rMapModeSource,
                  const MapMode& rMapModeDest);
tools::Rectangle LogicToLogic(const tools::Rectangle& rRectSource, const MapMode& rMapModeSource,
                              const MapMode& rMapModeDest);
tools::Long LogicToLogic(tools::Long nLongSource, MapUnit eUnitSource, MapUnit eUnitDest);
basegfx::B2DPolygon LogicToLogic(const basegfx::B2DPolygon& rPolySource,
                                 const MapMode& rMapModeSource, const MapMode& rMapModeDest);
basegfx::B2DHomMatrix LogicToLogic(const MapMode& rMapModeSource, const MapMode& rMapModeDest);

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
