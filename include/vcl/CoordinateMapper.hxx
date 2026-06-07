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

#include <vcl/dllapi.h>
#include <vcl/mapconvert.hxx>
#include <vcl/mapmod.hxx>
#include <vcl/region.hxx>
#include <vcl/MappingPolicy.hxx>
#include <vcl/TransformTypes.hxx>
#include <vcl/CoordinateState.hxx>
#include <vcl/TransformPlan.hxx>
#include <vcl/TransformRouter.hxx>
#include <vcl/GeometryAdapter.hxx>

#include <TransformCompiler.hxx>
#include <MappingCoefficients.hxx>

#include <memory>
#include <concepts>
#include <type_traits>

namespace vcl
{
struct SpaceLogic;
struct SpaceWindow;
struct SpaceDevice;
template <typename Space, typename T> struct TypedGeom;
}

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
}

/**
 * @class CoordinateMapper
 * @brief Centralized mapping orchestrator for VCL coordinate transformations.
 *
 * The CoordinateMapper decouples transformation state from the physical
 * OutputDevice. It acts as the central coordinator between VCL's window state,
 * the stateless vcl::TransformCompiler, and the thread-safe TransformCache.
 *
 * Coordinate Spaces (Conceptual)
 * ------------------------------
 * Conceptually, the mapper manages transitions across four distinct domains:
 * 1. Device Space: Absolute physical pixels on the monitor or printer.
 * 2. Window Space: Client area pixels (Device minus DeviceToWindowOffset).
 * 3. View Space: Scrollable viewport pixels (Window minus WindowToViewOffset).
 * 4. Logic Space: Document coordinates defined by a MapMode and MapOffset.
 * * NOTE: While these spaces exist conceptually, the CoordinateMapper squashes
 * them into unified, cached Affine Transformation Matrices (TransformPlan)
 * for O(1) execution performance.
 *
 * Architecture & Subsystems
 * ----------------------------
 * CoordinateMapper has been structurally decomposed into a pure coordinator:
 * * - The Compiler (vcl::TransformCompiler): CoordinateMapper does not synthesize
 * matrices. It passes its current physical offsets and mapping coefficients
 * to the vcl::TransformCompiler, which returns an immutable TransformPlan
 * artifact annotated with geometric invariants.
 *
 * - The Memory (TransformCache): CoordinateMapper does not manage lock-free
 * versioning. It delegates storage to the TransformCache, which guarantees
 * that threads never read an artifact compiled against stale state.
 *
 * - Legacy Quarantine: CoordinateMapper strictly operates on Affine math.
 * Historical procedural unit conversions (e.g., Twips to 100th MM) are
 * expressly forbidden here and are quarantined in LegacyCoordinateAdapter.
 *
 * ========================================================================
 * CoordinateMapper Transformation Contract
 * ========================================================================
 *
 * 1. Subpixel Authority
 * All geometric transformations must be performed in double precision
 * (basegfx::B2DHomMatrix). Conversion to integer occurs at the final API boundary.
 *
 * 2. Explicit Topology Routing
 * Coordinate requests are routed through a formal topology (TransformKey),
 * eliminating implicit slot math and ensuring rigorous path validation.
 *
 * 3. Distance vs. Position
 * Distance (Size) transformations apply scaling only. Position (Point/Geometry)
 * transformations include both scaling and offsets. Width/Height must not be
 * used to calculate geometric boundaries.
 *
 * 4. Forward/Inverse Symmetry
 * For every transformation A -> B, the inverse B -> A must return the original
 * value within +/-0.5 drift for intermediate floating-point values.
 * ========================================================================
 *
 * THREADING CONTRACT:
 * CoordinateMapper's internal state (DPI, offsets) is mutated concurrently
 * and MUST be protected by external synchronization (e.g., the SolarMutex).
 * Lock-free read safety of the resulting execution artifacts is handled
 * entirely by the underlying TransformCache.
 */

class VCL_DLLPUBLIC CoordinateMapper
{
private:
    vcl::CoordinateState maState;
    vcl::TransformRouter maRouter;

    tools::Long mnOutWidth = 0;
    tools::Long mnOutHeight = 0;

public:
    template <typename T>
    T LogicToDeviceSubPixel(T aObj,
                            vcl::MappingPolicy ePolicy = vcl::MappingPolicy::ApplyMapMode) const
    {
        if constexpr (vcl::detail::B2DTransformable<T>)
            aObj.transform(GetLogicToDeviceMatrix(ePolicy));
        else
            aObj *= GetLogicToDeviceMatrix(ePolicy);
        return aObj;
    }

    template <typename T>
    T DevicePixelToLogicSubPixel(T aObj, vcl::MappingPolicy ePolicy
                                         = vcl::MappingPolicy::ApplyMapMode) const
    {
        if constexpr (vcl::detail::B2DTransformable<T>)
            aObj.transform(GetDeviceToLogicMatrix(ePolicy));
        else
            aObj *= GetDeviceToLogicMatrix(ePolicy);
        return aObj;
    }

    basegfx::B2DHomMatrix GetLogicToLogicMatrix(const MapMode& rSrc, const MapMode& rDst) const;

    // Distance Extractors
    tools::Long LogicWidthToDevicePixel(tools::Long nWidth,
                                        vcl::MappingPolicy ePolicy
                                        = vcl::MappingPolicy::ApplyMapMode) const;
    tools::Long LogicHeightToDevicePixel(tools::Long nHeight,
                                         vcl::MappingPolicy ePolicy
                                         = vcl::MappingPolicy::ApplyMapMode) const;
    tools::Long DevicePixelToLogicWidth(tools::Long nWidth,
                                        vcl::MappingPolicy ePolicy
                                        = vcl::MappingPolicy::ApplyMapMode) const;
    tools::Long DevicePixelToLogicHeight(tools::Long nHeight,
                                         vcl::MappingPolicy ePolicy
                                         = vcl::MappingPolicy::ApplyMapMode) const;
    double LogicWidthToWindowSubPixel(tools::Long nWidth, vcl::MappingPolicy ePolicy
                                                          = vcl::MappingPolicy::ApplyMapMode) const;
    double LogicHeightToWindowSubPixel(tools::Long nHeight,
                                       vcl::MappingPolicy ePolicy
                                       = vcl::MappingPolicy::ApplyMapMode) const;
    double LogicWidthToDeviceSubPixel(tools::Long nWidth, vcl::MappingPolicy ePolicy
                                                          = vcl::MappingPolicy::ApplyMapMode) const;

    // Legacy Window Management Helper
    vcl::Region ViewToDevice(const vcl::Region& rRegion) const;

    const vcl::TransformPlan& Compile(const TransformRequest& rReq) const;

    // Legacy bridge
    const vcl::TransformPlan& Compile(vcl::MappingPolicy ePolicy) const
    {
        return Compile({ CoordinateSpace::Logic, CoordinateSpace::Device, ePolicy });
    }

    uint64_t GetSemanticKey(vcl::MappingPolicy ePolicy) const;

    bool IsValidDPI() const { return maState.GetDPIX() > 0 && maState.GetDPIY() > 0; }

    sal_Int32 GetDPIX() const;
    sal_Int32 GetDPIY() const;

    void SetDPIX(sal_Int32 nDPIX);
    void SetDPIY(sal_Int32 nDPIY);

    sal_Int32 GetDPIScalePercentage() const;
    float GetDPIScaleFactor() const;
    void SetDPIScalePercentage(sal_Int32 nPercentage);

    void SetPixelOffset(const Size& rSize);

    tools::Long GetDeviceToWindowOffsetX() const;
    tools::Long GetDeviceToWindowOffsetY() const;
    Point GetDeviceToWindowOffset() const;

    tools::Long GetWindowToViewOffsetX() const { return maState.GetWindowToViewOffsetX(); }
    tools::Long GetWindowToViewOffsetY() const { return maState.GetWindowToViewOffsetY(); }

    Size GetWindowToViewOffset() const
    {
        return Size(maState.GetWindowToViewOffsetX(), maState.GetWindowToViewOffsetY());
    }

    tools::Long GetLogicToAbsoluteOffsetX() const { return maState.GetLogicToAbsoluteOffsetX(); }
    tools::Long GetLogicToAbsoluteOffsetY() const { return maState.GetLogicToAbsoluteOffsetY(); }

    Size GetLogicToAbsoluteOffset() const
    {
        return Size(maState.GetLogicToAbsoluteOffsetX(), maState.GetLogicToAbsoluteOffsetY());
    }

    tools::Long GetDeviceToViewOffsetX() const
    {
        return maState.GetDeviceToWindowOffsetX() + maState.GetWindowToViewOffsetX();
    }

    tools::Long GetDeviceToViewOffsetY() const
    {
        return maState.GetDeviceToWindowOffsetY() + maState.GetWindowToViewOffsetY();
    }

    Size GetDeviceToViewOffset() const
    {
        return Size(GetDeviceToViewOffsetX(), GetDeviceToViewOffsetY());
    }

    void SetDeviceToWindowOffsetX(tools::Long nDeviceToWindowOffsetX);
    void SetDeviceToWindowOffsetY(tools::Long nDeviceToWindowOffsetY);
    void SetWindowToViewOffset(const Size& rWindowPixelOffset);
    void SetLogicToAbsoluteOffset(const Size& rSize);
    void SetMapMode(const MapMode& rMapMode)
    {
        CalcMapResolution(rMapMode, maState.GetDPIX(), maState.GetDPIY());
    }
    void SetLogicOffset(const Size& rOffset) { SetLogicToAbsoluteOffset(rOffset); }
    void SetWindowOffset(const Size& rOffset) { SetWindowToViewOffset(rOffset); }

    Size LogicToViewDistance(const Size& rLogicSize, vcl::MappingPolicy ePolicy) const;

    tools::Long GetOutputWidthPixel() const;
    tools::Long GetOutputHeightPixel() const;
    Size GetOutputSizePixel() const;

    void SetOutputWidthPixel(tools::Long nWidth);
    void SetOutputHeightPixel(tools::Long nHeight);

    const MapMode& GetMapMode() const { return maState.GetMapMode(); }
    bool IsDefaultMapMode() const { return maState.GetMapMode().IsDefault(); }
    MapUnit GetMapUnit() const { return maState.GetMapMode().GetMapUnit(); }
    double GetScaleX() const { return maState.GetMapMode().GetScaleX(); }
    double GetScaleY() const { return maState.GetMapMode().GetScaleY(); }

    void SetStateMapMode(MapMode aMapMode) { maState.SetMapMode(aMapMode); }
    void SetScaleX(double nX) { maState.SetScaleX(nX); }
    void SetScaleY(double nY) { maState.SetScaleY(nY); }
    void SetOrigin(const Point& rLogicOrig) { maState.SetOrigin(rLogicOrig); }

    tools::Long GetMappingXOffset() const { return maState.GetMapRes().mnTranslationX; }
    tools::Long GetMappingYOffset() const { return maState.GetMapRes().mnTranslationY; }
    double GetMapResolutionScaleX() const { return maState.GetMapRes().mfScaleX; }
    double GetMapResolutionScaleY() const { return maState.GetMapRes().mfScaleY; }

    void SetMappingXOffset(tools::Long nOffset)
    {
        maState.SetMappingXOffset(nOffset);
        InvalidateViewTransform();
    }

    void SetMappingYOffset(tools::Long nOffset)
    {
        maState.SetMappingYOffset(nOffset);
        InvalidateViewTransform();
    }

    void SetMapResolutionScaleX(double fX)
    {
        maState.SetMapResolutionScaleX(fX);
        InvalidateViewTransform();
    }

    void SetMapResolutionScaleY(double fY)
    {
        maState.SetMapResolutionScaleY(fY);
        InvalidateViewTransform();
    }

    void CalcMapResolution(const MapMode& rMapMode, tools::Long nDPIX, tools::Long nDPIY);

    MappingCoefficients ResolveMapResRelative(const MapMode* pBaseline, const MapMode* pTarget,
                                              vcl::MappingPolicy ePolicy) const;

    vcl::detail::MapConversion ResolveMap(const MapMode& rBaseline, const MapMode& rTarget,
                                          vcl::MappingPolicy ePolicy) const;

    void InvalidateViewTransform() { maRouter.Invalidate(); }
    basegfx::B2DHomMatrix GetViewTransformation(vcl::MappingPolicy ePolicy) const;
    basegfx::B2DHomMatrix GetViewTransformation(const vcl::detail::MapConversion& rConv) const;
    basegfx::B2DHomMatrix GetViewTransformation(const MapMode& rBaseline, const MapMode& rTarget,
                                                vcl::MappingPolicy ePolicy) const;

    basegfx::B2DHomMatrix GetInverseViewTransformation(vcl::MappingPolicy ePolicy) const;
    basegfx::B2DHomMatrix
    GetInverseViewTransformation(const vcl::detail::MapConversion& rConv) const;
    basegfx::B2DHomMatrix GetInverseViewTransformation(const MapMode& rBaseline,
                                                       const MapMode& rTarget,
                                                       vcl::MappingPolicy ePolicy) const;

    basegfx::B2DHomMatrix GetDeviceTransformation(vcl::MappingPolicy ePolicy) const;

    // --- PIPELINE MATRIX BUILDERS (Now single-source-of-truth projections) ---
    basegfx::B2DHomMatrix GetLogicToWindowMatrix(vcl::MappingPolicy ePolicy) const;
    basegfx::B2DHomMatrix GetWindowToLogicMatrix(vcl::MappingPolicy ePolicy) const;
    basegfx::B2DHomMatrix GetLogicToDeviceMatrix(vcl::MappingPolicy ePolicy) const;
    basegfx::B2DHomMatrix GetDeviceToLogicMatrix(vcl::MappingPolicy ePolicy) const;

    // ========================================================================
    // UNIFIED DELEGATES (Bridging to GeometryAdapter)
    // ========================================================================
    template <typename T>
    T LogicToDevicePixel(const T& rObj,
                         vcl::MappingPolicy ePolicy = vcl::MappingPolicy::ApplyMapMode) const;
    template <typename T>
    T DevicePixelToLogic(const T& rObj,
                         vcl::MappingPolicy ePolicy = vcl::MappingPolicy::ApplyMapMode) const;
    template <typename T>
    T LogicToWindowUnits(const T& rObj,
                         vcl::MappingPolicy ePolicy = vcl::MappingPolicy::ApplyMapMode) const;
    template <typename T>
    T WindowToLogicUnits(const T& rObj,
                         vcl::MappingPolicy ePolicy = vcl::MappingPolicy::ApplyMapMode) const;

    template <typename T>
    T LogicToWindowUnits(const T& rObj, const vcl::detail::MapConversion& rConv) const;
    template <typename T>
    T WindowToLogicUnits(const T& rObj, const vcl::detail::MapConversion& rConv) const;

    basegfx::B2DPoint LogicToDeviceSubPixel(const Point& rPt,
                                            vcl::MappingPolicy ePolicy
                                            = vcl::MappingPolicy::ApplyMapMode) const;
    basegfx::B2DPoint DevicePixelToLogicSubPixel(const Point& rPt,
                                                 vcl::MappingPolicy ePolicy
                                                 = vcl::MappingPolicy::ApplyMapMode) const;
    Point WindowSubPixelToLogicUnits(const basegfx::B2DPoint& rPt,
                                     vcl::MappingPolicy ePolicy
                                     = vcl::MappingPolicy::ApplyMapMode) const;

    // Window <-> Logic

    double ViewSubPixelToLogicX(double fX, vcl::MappingPolicy ePolicy) const;
    double ViewSubPixelToLogicY(double fY, vcl::MappingPolicy ePolicy) const;
    double LogicToViewSubPixelX(double fX) const;
    double LogicToViewSubPixelY(double fY) const;

    // ========================================================================
    // DISTANCE SCALING (Raw Scalar Conversion, NO offsets applied)
    // ========================================================================

    // Double/Sub-Pixel Distances
    tools::Long ViewSubPixelToLogicDistanceX(double n) const;
    tools::Long ViewSubPixelToLogicDistanceY(double n) const;
    tools::Long ViewSubPixelToLogicDistanceX(double n, double fScale) const;
    tools::Long ViewSubPixelToLogicDistanceY(double n, double fScale) const;

    template <typename Geom>
    vcl::TypedGeom<vcl::SpaceWindow, Geom>
    MapToWindow(const vcl::TypedGeom<vcl::SpaceLogic, Geom>& rLogicGeom,
                const MapMode& rCustomMapMode) const;

    template <typename Geom>
    vcl::TypedGeom<vcl::SpaceDevice, Geom>
    MapToDevice(const vcl::TypedGeom<vcl::SpaceLogic, Geom>& rLogicGeom,
                const MapMode& rCustomMapMode) const;

private:
    void GetLogicToViewWeights(double& rScaleX, double& rScaleY, double& rTransX, double& rTransY,
                               vcl::MappingPolicy ePolicy) const;
};

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
