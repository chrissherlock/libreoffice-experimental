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

#include <sal/log.hxx>
#include <basegfx/matrix/b2dhommatrix.hxx>
#include <basegfx/range/b2drange.hxx>
#include <basegfx/vector/b2dvector.hxx>
#include <basegfx/range/b2drectangle.hxx>
#include <basegfx/polygon/b2dpolygon.hxx>
#include <basegfx/polygon/b2dpolypolygon.hxx>
#include <tools/bigint.hxx>
#include <tools/debug.hxx>
#include <tools/gen.hxx>
#include <tools/mapunit.hxx>

#include <vcl/lineinfo.hxx>

#include <CoordinateMapper.hxx>
#include <MappingCoefficients.hxx>

#include <cmath>
#include <cassert>
#include <ranges>

static inline tools::Long lcl_RoundToLong(double fVal)
{
    return static_cast<tools::Long>(std::llround(fVal));
}

static double lcl_GetScaledXLength(const basegfx::B2DHomMatrix& m)
{
    basegfx::B2DVector vx(1.0, 0.0);
    vx *= m;
    return vx.getLength();
}

static double lcl_GetScaledYLength(const basegfx::B2DHomMatrix& m)
{
    basegfx::B2DVector vy(0.0, 1.0);
    vy *= m;
    return vy.getLength();
}

static std::pair<MappingCoefficients, MappingCoefficients>
lcl_calcConversionMapRes(const MapMode& rMMSource, const MapMode& rMMDest)
{
    std::pair<MappingCoefficients, MappingCoefficients> result;
    result.first.CalcMapResolution(rMMSource, 72, 72);
    result.second.CalcMapResolution(rMMDest, 72, 72);
    return result;
}

static tools::Long lcl_convertLogicValue(const tools::Long nSourceValue,
                                         const o3tl::Length eSourceUnit,
                                         const o3tl::Length eDestUnit)
{
    if (nSourceValue == 0 || eSourceUnit == o3tl::Length::invalid
        || eDestUnit == o3tl::Length::invalid)
    {
        return 0;
    }

    bool bOverflow;
    const auto nResult = o3tl::convert(nSourceValue, eSourceUnit, eDestUnit, bOverflow);

    if (!bOverflow)
        return nResult;

    const auto[nMultiplier, nDivisor] = o3tl::getConversionMulDiv(eSourceUnit, eDestUnit);
    BigInt aBigValue = nSourceValue;
    aBigValue *= nMultiplier;

    if (aBigValue.IsNeg())
        aBigValue -= nDivisor / 2;
    else
        aBigValue += nDivisor / 2;

    aBigValue /= nDivisor;

    return static_cast<tools::Long>(aBigValue);
}

static void lcl_ApplyEmptyState(tools::Rectangle& rDest, const tools::Rectangle& rSrc)
{
    if (rSrc.IsWidthEmpty())
        rDest.SetWidthEmpty();

    if (rSrc.IsHeightEmpty())
        rDest.SetHeightEmpty();
}

static bool lcl_IsPureTranslation(const basegfx::B2DHomMatrix& matrix)
{
    // Protect against future floating-point noise from complex matrix composition
    constexpr double fEpsilon = 1e-9;
    return std::abs(matrix.get(0, 0) - 1.0) < fEpsilon
           && std::abs(matrix.get(1, 1) - 1.0) < fEpsilon && std::abs(matrix.get(0, 1)) < fEpsilon
           && std::abs(matrix.get(1, 0)) < fEpsilon;
}

// Conceptual Pipeline Separation (Mathematical Invariant):
// Logic -> View: Scaled transformations (Scale * Logic) + Scaled Offsets ((MapOfs + LogicOfs) * Scale)
// View -> Window: Pure translation (WindowOfs)
// Window -> Device: Pure translation (DeviceOfs)
void CoordinateMapper::GetLogicToViewWeights(double& rScaleX, double& rScaleY, double& rTransX,
                                             double& rTransY, bool bMap) const
{
    const basegfx::B2DHomMatrix aMat = GetLogicToWindowMatrix(bMap);

    rScaleX = lcl_GetScaledXLength(aMat);
    rScaleY = lcl_GetScaledYLength(aMat);

    rTransX = aMat.get(0, 2);
    rTransY = aMat.get(1, 2);
}

sal_Int32 CoordinateMapper::GetDPIX() const { return mnDPIX; }

sal_Int32 CoordinateMapper::GetDPIY() const { return mnDPIY; }

void CoordinateMapper::SetDPIX(sal_Int32 nDPIX)
{
    mnDPIX = nDPIX;
    InvalidateViewTransform();
}

void CoordinateMapper::SetDPIY(sal_Int32 nDPIY)
{
    mnDPIY = nDPIY;
    InvalidateViewTransform();
}

sal_Int32 CoordinateMapper::GetDPIScalePercentage() const { return mnDPIScalePercentage; }

void CoordinateMapper::SetDPIScalePercentage(sal_Int32 nPercent)
{
    mnDPIScalePercentage = nPercent;
    InvalidateViewTransform();
}

float CoordinateMapper::GetDPIScaleFactor() const { return mnDPIScalePercentage / 100.0f; }

void CoordinateMapper::SetPixelOffset(const Size& rSize)
{
    mnLogicToAbsoluteOffsetX = rSize.getWidth();
    mnLogicToAbsoluteOffsetY = rSize.getHeight();
    InvalidateViewTransform();
}

void CoordinateMapper::SetWindowToViewOffset(const Size& rWindowPixelOffset)
{
    mnWindowToViewOffsetX = rWindowPixelOffset.Width();
    mnWindowToViewOffsetY = rWindowPixelOffset.Height();
    InvalidateViewTransform();
}

tools::Long CoordinateMapper::GetDeviceToWindowOffsetX() const { return mnDeviceToWindowOffsetX; }

tools::Long CoordinateMapper::GetDeviceToWindowOffsetY() const { return mnDeviceToWindowOffsetY; }

void CoordinateMapper::SetDeviceToWindowOffsetX(tools::Long nDeviceToWindowOffsetX)
{
    mnDeviceToWindowOffsetX = nDeviceToWindowOffsetX;
    InvalidateViewTransform();
}

void CoordinateMapper::SetDeviceToWindowOffsetY(tools::Long nDeviceToWindowOffsetY)
{
    mnDeviceToWindowOffsetY = nDeviceToWindowOffsetY;
    InvalidateViewTransform();
}

Point CoordinateMapper::GetDeviceToWindowOffset() const
{
    return Point(mnDeviceToWindowOffsetX, mnDeviceToWindowOffsetY);
}

Size CoordinateMapper::LogicToViewDistance(const Size& rLogicSize, bool bMap) const
{
    const basegfx::B2DHomMatrix aMat = GetLogicToWindowMatrix(bMap);
    const double sx = lcl_GetScaledXLength(aMat);
    const double sy = lcl_GetScaledYLength(aMat);
    return Size(lcl_RoundToLong(rLogicSize.Width() * sx),
                lcl_RoundToLong(rLogicSize.Height() * sy));
}

vcl::Region CoordinateMapper::ViewToDevice(const vcl::Region& rRegion) const
{
    tools::Long nDeltaX = GetDeviceToViewOffsetX();
    tools::Long nDeltaY = GetDeviceToViewOffsetY();

    if (nDeltaX == 0 && nDeltaY == 0)
        return rRegion;

    vcl::Region aRegion(rRegion);
    aRegion.Move(nDeltaX, nDeltaY);
    return aRegion;
}

tools::Long CoordinateMapper::GetOutputWidthPixel() const { return mnOutWidth; }

tools::Long CoordinateMapper::GetOutputHeightPixel() const { return mnOutHeight; }

void CoordinateMapper::SetOutputWidthPixel(tools::Long nWidth) { mnOutWidth = nWidth; }

void CoordinateMapper::SetOutputHeightPixel(tools::Long nHeight) { mnOutHeight = nHeight; }

void CoordinateMapper::SetLogicToAbsoluteOffset(Size const& rOffset)
{
    mnLogicToAbsoluteOffsetX = rOffset.getWidth();
    mnLogicToAbsoluteOffsetY = rOffset.getHeight();
    InvalidateViewTransform();
}

void CoordinateMapper::CalcMapResolution(const MapMode& rMapMode, tools::Long nDPIX,
                                         tools::Long nDPIY)
{
    // Let the legacy accumulator do its complex state math
    maMapRes.CalcMapResolution(rMapMode, nDPIX, nDPIY);

    // Copy the pure math into our firewall struct
    maMapConversion.mfScaleX = maMapRes.mfScaleX;
    maMapConversion.mfScaleY = maMapRes.mfScaleY;
    maMapConversion.mnOffsetX = maMapRes.mnTranslationX;
    maMapConversion.mnOffsetY = maMapRes.mnTranslationY;

    InvalidateViewTransform();
}

MappingCoefficients CoordinateMapper::ResolveMapResRelative(const MapMode* pBaseline,
                                                            const MapMode* pTarget, bool bMap) const
{
    return maMapRes.ResolveMapRes(pTarget, *pBaseline, bMap, mnDPIX, mnDPIY);
}

vcl::detail::MapConversion CoordinateMapper::ResolveMap(const MapMode& rBaseline,
                                                        const MapMode& rTarget, bool bMap) const
{
    // Evaluates a temporary MapMode against the current accumulated state
    MappingCoefficients aRes = maMapRes.ResolveMapRes(&rTarget, rBaseline, bMap, mnDPIX, mnDPIY);
    return { aRes.mfScaleX, aRes.mfScaleY, aRes.mnTranslationX, aRes.mnTranslationY };
}

void CoordinateMapper::InvalidateViewTransform()
{
    mnStateVersion.fetch_add(1, std::memory_order_release);
}

basegfx::B2DHomMatrix CoordinateMapper::GetDeviceTransformation(bool bMap) const
{
    return GetLogicToDeviceMatrix(bMap);
}

basegfx::B2DHomMatrix CoordinateMapper::GetViewTransformation(bool bMap) const
{
    return GetLogicToWindowMatrix(bMap);
}

basegfx::B2DHomMatrix CoordinateMapper::GetInverseViewTransformation(bool bMap) const
{
    return GetWindowToLogicMatrix(bMap);
}

basegfx::B2DHomMatrix
CoordinateMapper::GetViewTransformation(const vcl::detail::MapConversion& rConv) const
{
    basegfx::B2DHomMatrix aTransform;

    const double fScaleFactorX = static_cast<double>(GetDPIX()) * rConv.mfScaleX;
    const double fScaleFactorY = static_cast<double>(GetDPIY()) * rConv.mfScaleY;

    const double fZeroPointX
        = (static_cast<double>(rConv.mnOffsetX) + static_cast<double>(mnLogicToAbsoluteOffsetX))
              * fScaleFactorX
          + static_cast<double>(GetWindowToViewOffsetX());
    const double fZeroPointY
        = (static_cast<double>(rConv.mnOffsetY) + static_cast<double>(mnLogicToAbsoluteOffsetY))
              * fScaleFactorY
          + static_cast<double>(GetWindowToViewOffsetY());

    aTransform.set(0, 0, fScaleFactorX);
    aTransform.set(1, 1, fScaleFactorY);
    aTransform.set(0, 2, fZeroPointX);
    aTransform.set(1, 2, fZeroPointY);

    return aTransform;
}

basegfx::B2DHomMatrix CoordinateMapper::GetViewTransformation(const MapMode& rBaseline,
                                                              const MapMode& rTarget,
                                                              bool bMap) const
{
    return GetViewTransformation(ResolveMap(rBaseline, rTarget, bMap));
}

basegfx::B2DHomMatrix
CoordinateMapper::GetInverseViewTransformation(const vcl::detail::MapConversion& rConv) const
{
    basegfx::B2DHomMatrix aMatrix(GetViewTransformation(rConv));
    aMatrix.invert();
    return aMatrix;
}

basegfx::B2DHomMatrix CoordinateMapper::GetInverseViewTransformation(const MapMode& rBaseline,
                                                                     const MapMode& rTarget,
                                                                     bool bMap) const
{
    return GetInverseViewTransformation(ResolveMap(rBaseline, rTarget, bMap));
}

// ============================================================================
// THE O(1) TRANSFORM REGISTER FILE
// ============================================================================

void CoordinateMapper::UpdateCache(bool bMap) const
{
    // Helper to extract Identity/Translation semantics to preserve legacy fast-paths
    auto optimizeTransform = [](CompiledTransform& transform) {
        if (transform.maMatrix.isIdentity())
        {
            transform.meMode = TransformMode::Identity;
        }
        else if (lcl_IsPureTranslation(transform.maMatrix))
        {
            double translationX = transform.maMatrix.get(0, 2);
            double translationY = transform.maMatrix.get(1, 2);

            // Check if translations are exact integers
            if (std::floor(translationX) == translationX
                && std::floor(translationY) == translationY)
            {
                transform.mnDeviceTx = static_cast<tools::Long>(translationX);
                transform.mnDeviceTy = static_cast<tools::Long>(translationY);
                transform.meMode = TransformMode::Translation;
            }
            else
            {
                transform.meMode = TransformMode::AffineFallback;
            }
        }
        else
        {
            transform.meMode = TransformMode::AffineFallback;
        }
    };

    // Logic -> Window (True Affine Composition)
    CompiledTransform logicToWindowTransform;

    if (bMap)
    {
        const double uiScaleFactor = static_cast<double>(mnDPIScalePercentage) / 100.0;
        const double finalScaleX = maMapRes.mfScaleX * static_cast<double>(mnDPIX) * uiScaleFactor;
        const double finalScaleY = maMapRes.mfScaleY * static_cast<double>(mnDPIY) * uiScaleFactor;

        const double totalLogicalOffsetX
            = static_cast<double>(maMapRes.mnTranslationX + mnLogicToAbsoluteOffsetX);
        const double totalLogicalOffsetY
            = static_cast<double>(maMapRes.mnTranslationY + mnLogicToAbsoluteOffsetY);

        // Phase 1: Apply Logical Offset
        logicToWindowTransform.maMatrix.translate(totalLogicalOffsetX, totalLogicalOffsetY);

        // Phase 2: Apply DPI, MapMode, and UI Scaling
        logicToWindowTransform.maMatrix.scale(finalScaleX, finalScaleY);
    }

    // Phase 3: Apply Viewport Scroll Offsets
    logicToWindowTransform.maMatrix.translate(static_cast<double>(mnWindowToViewOffsetX),
                                              static_cast<double>(mnWindowToViewOffsetY));

    optimizeTransform(logicToWindowTransform);

    // Window -> Logic (Precalculated Inverse)
    CompiledTransform windowToLogicTransform;
    windowToLogicTransform.maMatrix = logicToWindowTransform.maMatrix;
    windowToLogicTransform.maMatrix.invert();

    if (logicToWindowTransform.meMode == TransformMode::Identity)
    {
        windowToLogicTransform.meMode = TransformMode::Identity;
    }
    else if (logicToWindowTransform.meMode == TransformMode::Translation)
    {
        windowToLogicTransform.meMode = TransformMode::Translation;
        windowToLogicTransform.mnDeviceTx = -logicToWindowTransform.mnDeviceTx;
        windowToLogicTransform.mnDeviceTy = -logicToWindowTransform.mnDeviceTy;
    }
    else
    {
        windowToLogicTransform.meMode = TransformMode::AffineFallback;
    }

    // Logic -> Device
    CompiledTransform logicToDeviceTransform;
    logicToDeviceTransform.maMatrix = logicToWindowTransform.maMatrix;

    // Phase 4: Apply OS/Widget physical screen offsets
    logicToDeviceTransform.maMatrix.translate(static_cast<double>(mnDeviceToWindowOffsetX),
                                              static_cast<double>(mnDeviceToWindowOffsetY));
    optimizeTransform(logicToDeviceTransform);

    // Device -> Logic (Precalculated Inverse)
    CompiledTransform deviceToLogicTransform;
    deviceToLogicTransform.maMatrix = logicToDeviceTransform.maMatrix;
    deviceToLogicTransform.maMatrix.invert();

    if (logicToDeviceTransform.meMode == TransformMode::Identity)
    {
        deviceToLogicTransform.meMode = TransformMode::Identity;
    }
    else if (logicToDeviceTransform.meMode == TransformMode::Translation)
    {
        deviceToLogicTransform.meMode = TransformMode::Translation;
        deviceToLogicTransform.mnDeviceTx = -logicToDeviceTransform.mnDeviceTx;
        deviceToLogicTransform.mnDeviceTy = -logicToDeviceTransform.mnDeviceTy;
    }
    else
    {
        deviceToLogicTransform.meMode = TransformMode::AffineFallback;
    }

    // Commit to the Register File
    size_t cacheSlotOffset = bMap ? 0 : 1;
    maTransformCache[static_cast<size_t>(TransformSlot::LogicToWindow_Mapped) + cacheSlotOffset]
        = logicToWindowTransform;
    maTransformCache[static_cast<size_t>(TransformSlot::WindowToLogic_Mapped) + cacheSlotOffset]
        = windowToLogicTransform;
    maTransformCache[static_cast<size_t>(TransformSlot::LogicToDevice_Mapped) + cacheSlotOffset]
        = logicToDeviceTransform;
    maTransformCache[static_cast<size_t>(TransformSlot::DeviceToLogic_Mapped) + cacheSlotOffset]
        = deviceToLogicTransform;
}

const CompiledTransform& CoordinateMapper::Compile(const TransformRequest& rReq) const
{
    // O(1) Cache Version Validation
    // Locks are unnecessary due to the atomic state version and thread-local assumption of the handle.
    uint64_t nCurrentVersion = mnStateVersion.load(std::memory_order_acquire);
    if (mnCacheVersion != nCurrentVersion)
    {
        for (auto& slot : maTransformCache)
            slot.reset();
        mnCacheVersion = nCurrentVersion;
    }

    size_t nOffset = rReq.bApplyMapping ? 0 : 1;
    TransformSlot eSlot;

    if (rReq.eFrom == CoordinateSpace::Logic && rReq.eTo == CoordinateSpace::Window)
        eSlot = static_cast<TransformSlot>(static_cast<size_t>(TransformSlot::LogicToWindow_Mapped)
                                           + nOffset);
    else if (rReq.eFrom == CoordinateSpace::Window && rReq.eTo == CoordinateSpace::Logic)
        eSlot = static_cast<TransformSlot>(static_cast<size_t>(TransformSlot::WindowToLogic_Mapped)
                                           + nOffset);
    else if (rReq.eFrom == CoordinateSpace::Logic && rReq.eTo == CoordinateSpace::Device)
        eSlot = static_cast<TransformSlot>(static_cast<size_t>(TransformSlot::LogicToDevice_Mapped)
                                           + nOffset);
    else if (rReq.eFrom == CoordinateSpace::Device && rReq.eTo == CoordinateSpace::Logic)
        eSlot = static_cast<TransformSlot>(static_cast<size_t>(TransformSlot::DeviceToLogic_Mapped)
                                           + nOffset);
    else
        assert(false && "Unsupported TransformRequest routing");

    if (!maTransformCache[static_cast<size_t>(eSlot)])
    {
        UpdateCache(rReq.bApplyMapping);
    }

    return *maTransformCache[static_cast<size_t>(eSlot)];
}

// ============================================================================
// SINGLE SOURCE OF TRUTH: MATRIX BUILDERS (NOW ROUTES TO COMPILE)
// ============================================================================

basegfx::B2DHomMatrix CoordinateMapper::GetLogicToDeviceMatrix(bool bMap) const
{
    return Compile({ CoordinateSpace::Logic, CoordinateSpace::Device, bMap }).GetMatrix();
}

basegfx::B2DHomMatrix CoordinateMapper::GetDeviceToLogicMatrix(bool bMap) const
{
    return Compile({ CoordinateSpace::Device, CoordinateSpace::Logic, bMap }).GetMatrix();
}

basegfx::B2DHomMatrix CoordinateMapper::GetLogicToWindowMatrix(bool bMap) const
{
    return Compile({ CoordinateSpace::Logic, CoordinateSpace::Window, bMap }).GetMatrix();
}

basegfx::B2DHomMatrix CoordinateMapper::GetWindowToLogicMatrix(bool bMap) const
{
    return Compile({ CoordinateSpace::Window, CoordinateSpace::Logic, bMap }).GetMatrix();
}

// ============================================================================
// TEMPLATE SPECIALIZATIONS: THE UNIVERSAL GEOMETRY PIPELINE
// ============================================================================

template <> Point CompiledTransform::Apply<Point>(const Point& rPt) const
{
    if (meMode == TransformMode::Identity)
        [[likely]] return rPt;

    if (meMode == TransformMode::Translation)
        return Point(rPt.X() + GetDeviceTx(), rPt.Y() + GetDeviceTy());

    basegfx::B2DPoint aB2DPt(rPt.X(), rPt.Y());
    aB2DPt *= maMatrix;
    return Point(lcl_RoundToLong(aB2DPt.getX()), lcl_RoundToLong(aB2DPt.getY()));
}

template <> Size CompiledTransform::Apply<Size>(const Size& rSize) const
{
    if (meMode == TransformMode::Identity || meMode == TransformMode::Translation)
        [[likely]] return rSize;

    // CRITICAL FIX: Preserve VCL negative sizes for mirroring
    // Treat Size as an axis-aligned Extent for legacy compatibility,
    // rather than a full vector that can flip components under rotation.
    basegfx::B2DVector vx(1.0, 0.0);
    basegfx::B2DVector vy(0.0, 1.0);
    vx *= maMatrix;
    vy *= maMatrix;

    return Size(lcl_RoundToLong(rSize.Width() * vx.getLength()),
                lcl_RoundToLong(rSize.Height() * vy.getLength()));
}

template <>
tools::Rectangle CompiledTransform::Apply<tools::Rectangle>(const tools::Rectangle& rRect) const
{
    if (meMode == TransformMode::Identity)
        [[likely]] return rRect;

    if (meMode == TransformMode::Translation)
    {
        tools::Rectangle aRetval(rRect.Left() + GetDeviceTx(), rRect.Top() + GetDeviceTy(),
                                 rRect.Right() + GetDeviceTx(), rRect.Bottom() + GetDeviceTy());
        lcl_ApplyEmptyState(aRetval, rRect);
        return aRetval;
    }

    // ADAPTER: VCL [Left, Right] -> Math [Min, Max)
    basegfx::B2DRange aRange(rRect.Left(), rRect.Top(), rRect.Right() + 1, rRect.Bottom() + 1);

    aRange.transform(maMatrix);

    // ADAPTER: Math [Min, Max) -> VCL [Left, Right]
    // A B2DRange mathematically guarantees Min <= Max, so this safely
    // extracts the Axis-Aligned Bounding Box (AABB) of the transformed geometry.
    tools::Rectangle aRetval(lcl_RoundToLong(aRange.getMinX()), lcl_RoundToLong(aRange.getMinY()),
                             lcl_RoundToLong(aRange.getMaxX()) - 1,
                             lcl_RoundToLong(aRange.getMaxY()) - 1);

    lcl_ApplyEmptyState(aRetval, rRect);
    return aRetval;
}

template <>
tools::Polygon CompiledTransform::Apply<tools::Polygon>(const tools::Polygon& rPoly) const
{
    if (meMode == TransformMode::Identity)
        [[likely]] return rPoly;

    tools::Polygon aPoly(rPoly);
    for (sal_uInt16 i = 0; i < aPoly.GetSize(); ++i)
        aPoly[i] = Apply(aPoly[i]);
    return aPoly;
}

template <>
tools::PolyPolygon
CompiledTransform::Apply<tools::PolyPolygon>(const tools::PolyPolygon& rPolyPoly) const
{
    if (meMode == TransformMode::Identity)
        [[likely]] return rPolyPoly;

    tools::PolyPolygon aPolyPoly;
    for (sal_uInt16 i = 0; i < rPolyPoly.Count(); ++i)
        aPolyPoly.Insert(Apply(rPolyPoly[i]));
    return aPolyPoly;
}

template <>
basegfx::B2DPolygon
CompiledTransform::Apply<basegfx::B2DPolygon>(const basegfx::B2DPolygon& rPoly) const
{
    if (meMode == TransformMode::Identity)
        [[likely]] return rPoly;

    basegfx::B2DPolygon aRet(rPoly);
    aRet.transform(maMatrix);
    return aRet;
}

template <>
basegfx::B2DPolyPolygon
CompiledTransform::Apply<basegfx::B2DPolyPolygon>(const basegfx::B2DPolyPolygon& rPolyPoly) const
{
    if (meMode == TransformMode::Identity)
        [[likely]] return rPolyPoly;

    basegfx::B2DPolyPolygon aRet(rPolyPoly);
    aRet.transform(maMatrix);
    return aRet;
}

template <>
basegfx::B2DRange CompiledTransform::Apply<basegfx::B2DRange>(const basegfx::B2DRange& rRange) const
{
    if (meMode == TransformMode::Identity)
        [[likely]] return rRange;

    basegfx::B2DRange aRet(rRange);
    aRet.transform(maMatrix);
    return aRet;
}

template <> vcl::Region CompiledTransform::Apply<vcl::Region>(const vcl::Region& rRegion) const
{
    if (rRegion.IsNull() || rRegion.IsEmpty() || meMode == TransformMode::Identity)
        return rRegion;

    if (meMode == TransformMode::Translation)
    {
        vcl::Region aRet(rRegion);
        aRet.Move(mnDeviceTx, mnDeviceTy);
        return aRet;
    }

    if (rRegion.getB2DPolyPolygon())
        return vcl::Region(Apply(*rRegion.getB2DPolyPolygon()));

    if (rRegion.getPolyPolygon())
        return vcl::Region(Apply(*rRegion.getPolyPolygon()));

    // Base case: RegionBand composition via AABB aggregation
    vcl::Region aRegion;
    RectangleVector aRectangles;
    rRegion.GetRegionRectangles(aRectangles);

    for (const auto& rRect : aRectangles | std::views::reverse)
        aRegion.Union(Apply(rRect));

    return aRegion;
}

template <> LineInfo CompiledTransform::Apply<LineInfo>(const LineInfo& rLineInfo) const
{
    if (meMode == TransformMode::Identity || meMode == TransformMode::Translation)
        [[likely]] return rLineInfo;

    // LineInfo isn't a geometry type yet, we leave it as an explicit passthrough for now
    // until we fully eradicate the LineInfo wrapper from vcl
    LineInfo aInfo(rLineInfo);
    aInfo.SetWidth(Apply(Size(rLineInfo.GetWidth(), 0)).Width());
    aInfo.SetDashLen(Apply(Size(rLineInfo.GetDashLen(), 0)).Width());
    aInfo.SetDotLen(Apply(Size(rLineInfo.GetDotLen(), 0)).Width());
    aInfo.SetDistance(Apply(Size(rLineInfo.GetDistance(), 0)).Width());
    return aInfo;
}

// ========================================================================
// PIPELINE STAGES (Coordinate Transitions)
// ========================================================================

double CoordinateMapper::DeviceToWindowSubPixelX(double fX) const
{
    return fX - static_cast<double>(mnDeviceToWindowOffsetX);
}

double CoordinateMapper::DeviceToWindowSubPixelY(double fY) const
{
    return fY - static_cast<double>(mnDeviceToWindowOffsetY);
}

double CoordinateMapper::WindowToDeviceSubPixelX(double fX) const
{
    return fX + static_cast<double>(mnDeviceToWindowOffsetX);
}

double CoordinateMapper::WindowToDeviceSubPixelY(double fY) const
{
    return fY + static_cast<double>(mnDeviceToWindowOffsetY);
}

tools::Long CoordinateMapper::ViewToWindowUnitsX(tools::Long nX) const
{
    return nX + mnWindowToViewOffsetX;
}

tools::Long CoordinateMapper::ViewToWindowUnitsY(tools::Long nY) const
{
    return nY + mnWindowToViewOffsetY;
}

tools::Long CoordinateMapper::WindowToViewUnitsX(tools::Long nX) const
{
    return nX - mnWindowToViewOffsetX;
}

tools::Long CoordinateMapper::WindowToViewUnitsY(tools::Long nY) const
{
    return nY - mnWindowToViewOffsetY;
}

tools::Long CoordinateMapper::DeviceToWindowUnitsX(tools::Long nX) const
{
    return nX - mnDeviceToWindowOffsetX;
}

tools::Long CoordinateMapper::DeviceToWindowUnitsY(tools::Long nY) const
{
    return nY - mnDeviceToWindowOffsetY;
}

tools::Long CoordinateMapper::WindowToDeviceUnitsX(tools::Long nX) const
{
    return nX + mnDeviceToWindowOffsetX;
}

tools::Long CoordinateMapper::WindowToDeviceUnitsY(tools::Long nY) const
{
    return nY + mnDeviceToWindowOffsetY;
}

tools::Long CoordinateMapper::ViewSubPixelToLogicDistanceX(double n) const
{
    return ViewSubPixelToLogicDistanceX(n, maMapConversion.mfScaleX);
}

tools::Long CoordinateMapper::ViewSubPixelToLogicDistanceY(double n) const
{
    return ViewSubPixelToLogicDistanceY(n, maMapConversion.mfScaleY);
}

tools::Long CoordinateMapper::ViewSubPixelToLogicDistanceX(double n, double fScale) const
{
    if (fScale == 0.0 || GetDPIX() <= 0)
        return lcl_RoundToLong(n);
    return lcl_RoundToLong(n / (fScale * static_cast<double>(GetDPIX())));
}

tools::Long CoordinateMapper::ViewSubPixelToLogicDistanceY(double n, double fScale) const
{
    if (fScale == 0.0 || GetDPIY() <= 0)
        return lcl_RoundToLong(n);
    return lcl_RoundToLong(n / (fScale * static_cast<double>(GetDPIY())));
}

double CoordinateMapper::WindowToViewSubPixelX(double fX) const
{
    return fX - static_cast<double>(mnWindowToViewOffsetX);
}

double CoordinateMapper::WindowToViewSubPixelY(double fY) const
{
    return fY - static_cast<double>(mnWindowToViewOffsetY);
}

double CoordinateMapper::ViewToWindowSubPixelX(double fX) const
{
    return fX + static_cast<double>(mnWindowToViewOffsetX);
}

double CoordinateMapper::ViewToWindowSubPixelY(double fY) const
{
    return fY + static_cast<double>(mnWindowToViewOffsetY);
}

double CoordinateMapper::LogicToWindowSubPixelX(double fX, bool bMap) const
{
    return fX * GetLogicToWindowMatrix(bMap).get(0, 0) + GetLogicToWindowMatrix(bMap).get(0, 2);
}

double CoordinateMapper::LogicToWindowSubPixelY(double fY, bool bMap) const
{
    return fY * GetLogicToWindowMatrix(bMap).get(1, 1) + GetLogicToWindowMatrix(bMap).get(1, 2);
}

// ========================================================================
// PUBLIC WRAPPERS (Routing into the unified pipeline)
// ========================================================================

Point CoordinateMapper::LogicToDevicePixel(const Point& rLogicPt, bool bMap) const
{
    return Compile({ CoordinateSpace::Logic, CoordinateSpace::Device, bMap }).Apply(rLogicPt);
}

tools::Rectangle CoordinateMapper::LogicToDevicePixel(const tools::Rectangle& rLogicRect,
                                                      bool bMap) const
{
    return Compile({ CoordinateSpace::Logic, CoordinateSpace::Device, bMap }).Apply(rLogicRect);
}

tools::Polygon CoordinateMapper::LogicToDevicePixel(const tools::Polygon& rLogicPoly,
                                                    bool bMap) const
{
    CompiledTransform t = Compile({ CoordinateSpace::Logic, CoordinateSpace::Device, bMap });
    if (t.GetMode() == TransformMode::AffineFallback)
    {
        // Legacy Note: We currently don't AdaptiveSubdivide inside Apply<Polygon> because
        // subdivision relies on global VCL tools settings which breaks abstraction. We
        // leave it here temporarily.
        tools::Polygon aSubdivided;
        rLogicPoly.AdaptiveSubdivide(aSubdivided);
        return t.Apply(aSubdivided);
    }
    return t.Apply(rLogicPoly);
}

tools::PolyPolygon CoordinateMapper::LogicToDevicePixel(const tools::PolyPolygon& rLogicPolyPoly,
                                                        bool bMap) const
{
    return Compile({ CoordinateSpace::Logic, CoordinateSpace::Device, bMap }).Apply(rLogicPolyPoly);
}

LineInfo CoordinateMapper::LogicToDevicePixel(const LineInfo& rLineInfo, bool bMap) const
{
    return Compile({ CoordinateSpace::Logic, CoordinateSpace::Device, bMap }).Apply(rLineInfo);
}

basegfx::B2DPolygon CoordinateMapper::LogicToDevicePixel(const basegfx::B2DPolygon& rLogicPoly,
                                                         bool bMap) const
{
    return Compile({ CoordinateSpace::Logic, CoordinateSpace::Device, bMap }).Apply(rLogicPoly);
}

basegfx::B2DPolyPolygon
CoordinateMapper::LogicToDevicePixel(const basegfx::B2DPolyPolygon& rLogicPolyPoly, bool bMap) const
{
    return Compile({ CoordinateSpace::Logic, CoordinateSpace::Device, bMap }).Apply(rLogicPolyPoly);
}

vcl::Region CoordinateMapper::LogicToDevicePixel(const vcl::Region& rRegion, bool bMap) const
{
    return Compile({ CoordinateSpace::Logic, CoordinateSpace::Device, bMap }).Apply(rRegion);
}

Size CoordinateMapper::LogicToDevicePixel(const Size& rLogicSize, bool bMap) const
{
    return Compile({ CoordinateSpace::Logic, CoordinateSpace::Device, bMap }).Apply(rLogicSize);
}

Point CoordinateMapper::DevicePixelToLogic(const Point& rDevicePt, bool bMap) const
{
    return Compile({ CoordinateSpace::Device, CoordinateSpace::Logic, bMap }).Apply(rDevicePt);
}

tools::Rectangle CoordinateMapper::DevicePixelToLogic(const tools::Rectangle& rPixelRect,
                                                      bool bMap) const
{
    return Compile({ CoordinateSpace::Device, CoordinateSpace::Logic, bMap }).Apply(rPixelRect);
}

tools::Polygon CoordinateMapper::DevicePixelToLogic(const tools::Polygon& rPixelPoly,
                                                    bool bMap) const
{
    return Compile({ CoordinateSpace::Device, CoordinateSpace::Logic, bMap }).Apply(rPixelPoly);
}

tools::PolyPolygon CoordinateMapper::DevicePixelToLogic(const tools::PolyPolygon& rPixelPolyPoly,
                                                        bool bMap) const
{
    return Compile({ CoordinateSpace::Device, CoordinateSpace::Logic, bMap }).Apply(rPixelPolyPoly);
}

basegfx::B2DPolygon CoordinateMapper::DevicePixelToLogic(const basegfx::B2DPolygon& rPixelPoly,
                                                         bool bMap) const
{
    return Compile({ CoordinateSpace::Device, CoordinateSpace::Logic, bMap }).Apply(rPixelPoly);
}

basegfx::B2DPolyPolygon
CoordinateMapper::DevicePixelToLogic(const basegfx::B2DPolyPolygon& rPixelPolyPoly, bool bMap) const
{
    return Compile({ CoordinateSpace::Device, CoordinateSpace::Logic, bMap }).Apply(rPixelPolyPoly);
}

vcl::Region CoordinateMapper::DevicePixelToLogic(const vcl::Region& rRegion, bool bMap) const
{
    return Compile({ CoordinateSpace::Device, CoordinateSpace::Logic, bMap }).Apply(rRegion);
}

Size CoordinateMapper::DevicePixelToLogic(const Size& rDeviceSize, bool bMap) const
{
    return Compile({ CoordinateSpace::Device, CoordinateSpace::Logic, bMap }).Apply(rDeviceSize);
}

Point CoordinateMapper::LogicToWindowUnits(const Point& rLogicPt, bool bMap) const
{
    return Compile({ CoordinateSpace::Logic, CoordinateSpace::Window, bMap }).Apply(rLogicPt);
}

tools::Rectangle CoordinateMapper::LogicToWindowUnits(const tools::Rectangle& rRect,
                                                      bool bMap) const
{
    return Compile({ CoordinateSpace::Logic, CoordinateSpace::Window, bMap }).Apply(rRect);
}

tools::Polygon CoordinateMapper::LogicToWindowUnits(const tools::Polygon& rPoly, bool bMap) const
{
    return Compile({ CoordinateSpace::Logic, CoordinateSpace::Window, bMap }).Apply(rPoly);
}

tools::PolyPolygon CoordinateMapper::LogicToWindowUnits(const tools::PolyPolygon& rPolyPoly,
                                                        bool bMap) const
{
    return Compile({ CoordinateSpace::Logic, CoordinateSpace::Window, bMap }).Apply(rPolyPoly);
}

vcl::Region CoordinateMapper::LogicToWindowUnits(const vcl::Region& rRegion, bool bMap) const
{
    return Compile({ CoordinateSpace::Logic, CoordinateSpace::Window, bMap }).Apply(rRegion);
}

Size CoordinateMapper::LogicToWindowUnits(const Size& rLogicSize, bool bMap) const
{
    return Compile({ CoordinateSpace::Logic, CoordinateSpace::Window, bMap }).Apply(rLogicSize);
}

Point CoordinateMapper::WindowToLogicUnits(const Point& rWindowPt, bool bMap) const
{
    return Compile({ CoordinateSpace::Window, CoordinateSpace::Logic, bMap }).Apply(rWindowPt);
}

tools::Rectangle CoordinateMapper::WindowToLogicUnits(const tools::Rectangle& rWindowRect,
                                                      bool bMap) const
{
    return Compile({ CoordinateSpace::Window, CoordinateSpace::Logic, bMap }).Apply(rWindowRect);
}

tools::Polygon CoordinateMapper::WindowToLogicUnits(const tools::Polygon& rWindowPoly,
                                                    bool bMap) const
{
    return Compile({ CoordinateSpace::Window, CoordinateSpace::Logic, bMap }).Apply(rWindowPoly);
}

tools::PolyPolygon CoordinateMapper::WindowToLogicUnits(const tools::PolyPolygon& rWindowPolyPoly,
                                                        bool bMap) const
{
    return Compile({ CoordinateSpace::Window, CoordinateSpace::Logic, bMap })
        .Apply(rWindowPolyPoly);
}

vcl::Region CoordinateMapper::WindowToLogicUnits(const vcl::Region& rRegion, bool bMap) const
{
    return Compile({ CoordinateSpace::Window, CoordinateSpace::Logic, bMap }).Apply(rRegion);
}

Size CoordinateMapper::WindowToLogicUnits(const Size& rWindowSize, bool bMap) const
{
    return Compile({ CoordinateSpace::Window, CoordinateSpace::Logic, bMap }).Apply(rWindowSize);
}

// ========================================================================
// DISTANCE EXTRACTORS (Vector Magnitudes)
// ========================================================================

/**
 * IMPORTANT ARCHITECTURAL NOTE: Distance Semantics under Affine Transforms
 * * Functions like LogicWidthToDevicePixel() now implicitly define "Width" as
 * the Euclidean magnitude of the transformed basis vector (lcl_GetScaledXLength).
 * * Historically, VCL assumed strictly orthogonal axes, meaning Width was treated
 * as an axis-aligned projected extent. Under shear or rotation, Euclidean length
 * differs from the AABB width. This preserves VCL's legacy vector mirroring
 * semantics without exploding extents under rotation.
 */

double CoordinateMapper::LogicWidthToDeviceSubPixel(tools::Long nWidth, bool bMap) const
{
    const basegfx::B2DHomMatrix aMat = GetLogicToDeviceMatrix(bMap);
    return static_cast<double>(nWidth) * lcl_GetScaledXLength(aMat);
}

tools::Long CoordinateMapper::LogicWidthToDevicePixel(tools::Long nWidth, bool bMap) const
{
    const basegfx::B2DHomMatrix aMat = GetLogicToDeviceMatrix(bMap);
    return lcl_RoundToLong(nWidth * lcl_GetScaledXLength(aMat));
}

tools::Long CoordinateMapper::LogicHeightToDevicePixel(tools::Long nHeight, bool bMap) const
{
    const basegfx::B2DHomMatrix aMat = GetLogicToDeviceMatrix(bMap);
    return lcl_RoundToLong(nHeight * lcl_GetScaledYLength(aMat));
}

tools::Long CoordinateMapper::DevicePixelToLogicWidth(tools::Long nWidth, bool bMap) const
{
    const basegfx::B2DHomMatrix aMat = GetDeviceToLogicMatrix(bMap);
    return lcl_RoundToLong(nWidth * lcl_GetScaledXLength(aMat));
}

tools::Long CoordinateMapper::DevicePixelToLogicHeight(tools::Long nHeight, bool bMap) const
{
    const basegfx::B2DHomMatrix aMat = GetDeviceToLogicMatrix(bMap);
    return lcl_RoundToLong(nHeight * lcl_GetScaledYLength(aMat));
}

double CoordinateMapper::LogicWidthToWindowSubPixel(tools::Long nWidth, bool bMap) const
{
    const basegfx::B2DHomMatrix aMat = GetLogicToWindowMatrix(bMap);
    return static_cast<double>(nWidth) * lcl_GetScaledXLength(aMat);
}

double CoordinateMapper::LogicHeightToWindowSubPixel(tools::Long nHeight, bool bMap) const
{
    const basegfx::B2DHomMatrix aMat = GetLogicToWindowMatrix(bMap);
    return static_cast<double>(nHeight) * lcl_GetScaledYLength(aMat);
}

tools::Long CoordinateMapper::LogicToWindowX(tools::Long nX, bool bMap) const
{
    if (!bMap && IsValidDPI())
        return nX;
    return lcl_RoundToLong(LogicToWindowSubPixelX(static_cast<double>(nX), bMap));
}

tools::Long CoordinateMapper::LogicToWindowY(tools::Long nY, bool bMap) const
{
    if (!bMap && IsValidDPI())
        return nY;
    return lcl_RoundToLong(LogicToWindowSubPixelY(static_cast<double>(nY), bMap));
}

basegfx::B2DPoint CoordinateMapper::LogicToDeviceSubPixel(const Point& rPoint, bool bMap) const
{
    basegfx::B2DHomMatrix aMat = GetLogicToDeviceMatrix(bMap);
    basegfx::B2DPoint aPt(rPoint.X(), rPoint.Y());
    aPt *= aMat;
    return aPt;
}

basegfx::B2DPoint CoordinateMapper::DevicePixelToLogicSubPixel(const Point& rDevicePt,
                                                               bool bMap) const
{
    basegfx::B2DPoint aPt(rDevicePt.X(), rDevicePt.Y());
    aPt *= GetDeviceToLogicMatrix(bMap);
    return aPt;
}

Point CoordinateMapper::WindowSubPixelToLogicUnits(const basegfx::B2DPoint& rWindowPt,
                                                   bool bMap) const
{
    basegfx::B2DHomMatrix aMat = GetWindowToLogicMatrix(bMap);
    basegfx::B2DPoint aPt(rWindowPt);
    aPt *= aMat;
    return Point(lcl_RoundToLong(aPt.getX()), lcl_RoundToLong(aPt.getY()));
}

template <TransformableB2DGeometry T>
T CoordinateMapper::LogicToWindowUnits(const T& rLogicGeometry, bool bMap) const
{
    return Compile({ CoordinateSpace::Logic, CoordinateSpace::Window, bMap }).Apply(rLogicGeometry);
}

template SAL_DLLPRIVATE basegfx::B2DRectangle
CoordinateMapper::LogicToWindowUnits<basegfx::B2DRectangle>(const basegfx::B2DRectangle&,
                                                            bool) const;

template SAL_DLLPRIVATE basegfx::B2DPolygon
CoordinateMapper::LogicToWindowUnits<basegfx::B2DPolygon>(const basegfx::B2DPolygon&, bool) const;

template SAL_DLLPRIVATE basegfx::B2DPolyPolygon
CoordinateMapper::LogicToWindowUnits<basegfx::B2DPolyPolygon>(const basegfx::B2DPolyPolygon&,
                                                              bool) const;

template <TransformableB2DGeometry T>
T CoordinateMapper::WindowToLogicUnits(const T& rWindowGeometry, bool bMap) const
{
    return Compile({ CoordinateSpace::Window, CoordinateSpace::Logic, bMap })
        .Apply(rWindowGeometry);
}

template SAL_DLLPRIVATE basegfx::B2DRectangle
CoordinateMapper::WindowToLogicUnits<basegfx::B2DRectangle>(const basegfx::B2DRectangle&,
                                                            bool) const;

template SAL_DLLPRIVATE basegfx::B2DPolyPolygon
CoordinateMapper::WindowToLogicUnits<basegfx::B2DPolyPolygon>(const basegfx::B2DPolyPolygon&,
                                                              bool) const;

// ========================================================================
// MapConversion Wrappers
// ========================================================================

Point CoordinateMapper::LogicToWindowUnits(const Point& rLogicPt,
                                           const vcl::detail::MapConversion& rConv) const
{
    basegfx::B2DPoint aPt(rLogicPt.X(), rLogicPt.Y());
    aPt *= GetViewTransformation(rConv);
    return Point(lcl_RoundToLong(aPt.getX()), lcl_RoundToLong(aPt.getY()));
}

Size CoordinateMapper::LogicToWindowUnits(const Size& rLogicSize,
                                          const vcl::detail::MapConversion& rConv) const
{
    auto mat = GetViewTransformation(rConv);
    return Size(lcl_RoundToLong(rLogicSize.Width() * std::abs(mat.get(0, 0))),
                lcl_RoundToLong(rLogicSize.Height() * std::abs(mat.get(1, 1))));
}

tools::Rectangle CoordinateMapper::LogicToWindowUnits(const tools::Rectangle& rRect,
                                                      const vcl::detail::MapConversion& rConv) const
{
    basegfx::B2DHomMatrix aMat = GetViewTransformation(rConv);

    // ADAPTER: VCL [Left, Right] -> Math [Min, Max)
    basegfx::B2DRange aRange(rRect.Left(), rRect.Top(), rRect.Right() + 1, rRect.Bottom() + 1);

    aRange.transform(aMat);

    // ADAPTER: Math [Min, Max) -> VCL [Left, Right]
    tools::Rectangle aRetval(lcl_RoundToLong(aRange.getMinX()), lcl_RoundToLong(aRange.getMinY()),
                             lcl_RoundToLong(aRange.getMaxX()) - 1,
                             lcl_RoundToLong(aRange.getMaxY()) - 1);

    lcl_ApplyEmptyState(aRetval, rRect);
    return aRetval;
}

tools::Polygon CoordinateMapper::LogicToWindowUnits(const tools::Polygon& rLogicPoly,
                                                    const vcl::detail::MapConversion& rConv) const
{
    tools::Polygon aPoly(rLogicPoly);
    basegfx::B2DHomMatrix aMat = GetViewTransformation(rConv);
    for (auto& rPoint : aPoly)
    {
        basegfx::B2DPoint aPt(rPoint.X(), rPoint.Y());
        aPt *= aMat;
        rPoint = Point(lcl_RoundToLong(aPt.getX()), lcl_RoundToLong(aPt.getY()));
    }
    return aPoly;
}

tools::PolyPolygon
CoordinateMapper::LogicToWindowUnits(const tools::PolyPolygon& rPolyPoly,
                                     const vcl::detail::MapConversion& rConv) const
{
    tools::PolyPolygon aPolyPoly(rPolyPoly);
    for (auto& rPoly : aPolyPoly)
    {
        rPoly = LogicToWindowUnits(rPoly, rConv);
    }
    return aPolyPoly;
}

template <TransformableB2DGeometry T>
T CoordinateMapper::LogicToWindowUnits(const T& rLogicGeometry,
                                       const vcl::detail::MapConversion& rConv) const
{
    T aTransformedGeometry = rLogicGeometry;
    aTransformedGeometry.transform(GetViewTransformation(rConv));
    return aTransformedGeometry;
}

template SAL_DLLPRIVATE basegfx::B2DPolygon
CoordinateMapper::LogicToWindowUnits<basegfx::B2DPolygon>(const basegfx::B2DPolygon&,
                                                          const vcl::detail::MapConversion&) const;

template SAL_DLLPRIVATE basegfx::B2DPolyPolygon
CoordinateMapper::LogicToWindowUnits<basegfx::B2DPolyPolygon>(
    const basegfx::B2DPolyPolygon&, const vcl::detail::MapConversion&) const;

Point CoordinateMapper::WindowToLogicUnits(const Point& rWindowPt,
                                           const vcl::detail::MapConversion& rConv) const
{
    basegfx::B2DPoint aPt(rWindowPt.X(), rWindowPt.Y());
    aPt *= GetInverseViewTransformation(rConv);
    return Point(lcl_RoundToLong(aPt.getX()), lcl_RoundToLong(aPt.getY()));
}

Size CoordinateMapper::WindowToLogicUnits(const Size& rWindowSize,
                                          const vcl::detail::MapConversion& rConv) const
{
    auto mat = GetInverseViewTransformation(rConv);
    return Size(lcl_RoundToLong(rWindowSize.Width() * std::abs(mat.get(0, 0))),
                lcl_RoundToLong(rWindowSize.Height() * std::abs(mat.get(1, 1))));
}

tools::Rectangle CoordinateMapper::WindowToLogicUnits(const tools::Rectangle& rWindowRect,
                                                      const vcl::detail::MapConversion& rConv) const
{
    basegfx::B2DHomMatrix aMat = GetInverseViewTransformation(rConv);

    // ADAPTER: VCL [Left, Right] -> Math [Min, Max)
    basegfx::B2DRange aRange(rWindowRect.Left(), rWindowRect.Top(), rWindowRect.Right() + 1,
                             rWindowRect.Bottom() + 1);

    aRange.transform(aMat);

    // ADAPTER: Math [Min, Max) -> VCL [Left, Right]
    tools::Rectangle aRetval(lcl_RoundToLong(aRange.getMinX()), lcl_RoundToLong(aRange.getMinY()),
                             lcl_RoundToLong(aRange.getMaxX()) - 1,
                             lcl_RoundToLong(aRange.getMaxY()) - 1);

    lcl_ApplyEmptyState(aRetval, rWindowRect);
    return aRetval;
}

tools::Polygon CoordinateMapper::WindowToLogicUnits(const tools::Polygon& rWindowPoly,
                                                    const vcl::detail::MapConversion& rConv) const
{
    tools::Polygon aPoly(rWindowPoly);
    for (auto& rPoint : aPoly)
    {
        rPoint = WindowToLogicUnits(rPoint, rConv);
    }
    return aPoly;
}

template <TransformableB2DGeometry T>
T CoordinateMapper::WindowToLogicUnits(const T& rWindowGeometry,
                                       const vcl::detail::MapConversion& rConv) const
{
    T aTransformedGeometry = rWindowGeometry;
    aTransformedGeometry.transform(GetInverseViewTransformation(rConv));
    return aTransformedGeometry;
}

template SAL_DLLPRIVATE basegfx::B2DPolygon
CoordinateMapper::WindowToLogicUnits<basegfx::B2DPolygon>(const basegfx::B2DPolygon&,
                                                          const vcl::detail::MapConversion&) const;

template SAL_DLLPRIVATE basegfx::B2DPolyPolygon
CoordinateMapper::WindowToLogicUnits<basegfx::B2DPolyPolygon>(
    const basegfx::B2DPolyPolygon&, const vcl::detail::MapConversion&) const;

// ========================================================================
// LOGIC-TO-LOGIC CONVERSIONS
// ========================================================================

Point CoordinateMapper::LogicToLogic(const Point& rPtSource, const MapMode* pMapModeBaseline,
                                     const MapMode* pMapModeSource, const MapMode* pMapModeDest,
                                     bool bMap) const
{
    const MapMode* pSrc = pMapModeSource ? pMapModeSource : pMapModeBaseline;
    const MapMode* pDst = pMapModeDest ? pMapModeDest : pMapModeBaseline;

    if (!pSrc || !pDst || *pSrc == *pDst)
        return rPtSource;

    MappingCoefficients aMapResSource = ResolveMapResRelative(pMapModeBaseline, pSrc, bMap);
    MappingCoefficients aMapResDest = ResolveMapResRelative(pMapModeBaseline, pDst, bMap);

    return Point(aMapResSource.TransformPointX(rPtSource.X(), aMapResDest),
                 aMapResSource.TransformPointY(rPtSource.Y(), aMapResDest));
}

Size CoordinateMapper::LogicToLogic(const Size& rSzSource, const MapMode* pMapModeBaseline,
                                    const MapMode* pMapModeSource, const MapMode* pMapModeDest,
                                    bool bMap) const
{
    const MapMode* pSrc = pMapModeSource ? pMapModeSource : pMapModeBaseline;
    const MapMode* pDst = pMapModeDest ? pMapModeDest : pMapModeBaseline;

    if (!pSrc || !pDst || *pSrc == *pDst)
        return rSzSource;

    MappingCoefficients aMapResSource = ResolveMapResRelative(pMapModeBaseline, pSrc, bMap);
    MappingCoefficients aMapResDest = ResolveMapResRelative(pMapModeBaseline, pDst, bMap);

    return Size(aMapResSource.ScaleDistanceX(rSzSource.Width(), aMapResDest),
                aMapResSource.ScaleDistanceY(rSzSource.Height(), aMapResDest));
}

/**
 * @brief Transforms a rectangle directly between two arbitrary MapModes.
 * * IMPORTANT ARCHITECTURAL NOTE:
 * Unlike LogicToDevicePixel() or LogicToWindowUnits(), this function does NOT
 * use the basegfx::B2DRange AABB adapter (+1/-1 correction) during transformation.
 * * Why? Because this function does not cross the affine CompiledTransform()
 * boundary. It strictly bypasses the matrix engine and uses the legacy
 * MappingCoefficients integer pipeline, which was explicitly designed to
 * scale VCL's inclusive [Left, Right] boundaries correctly without
 * requiring continuous half-open [Min, Max) interval adjustments.
 */
tools::Rectangle CoordinateMapper::LogicToLogic(const tools::Rectangle& rRectSource,
                                                const MapMode* pMapModeBaseline,
                                                const MapMode* pMapModeSource,
                                                const MapMode* pMapModeDest, bool bMap) const
{
    const MapMode* pSrc = pMapModeSource ? pMapModeSource : pMapModeBaseline;
    const MapMode* pDst = pMapModeDest ? pMapModeDest : pMapModeBaseline;

    if (!pSrc || !pDst || *pSrc == *pDst)
        return rRectSource;

    MappingCoefficients aMapResSource = ResolveMapResRelative(pMapModeBaseline, pSrc, bMap);
    MappingCoefficients aMapResDest = ResolveMapResRelative(pMapModeBaseline, pDst, bMap);

    return tools::Rectangle(aMapResSource.TransformPointX(rRectSource.Left(), aMapResDest),
                            aMapResSource.TransformPointY(rRectSource.Top(), aMapResDest),
                            aMapResSource.TransformPointX(rRectSource.Right(), aMapResDest),
                            aMapResSource.TransformPointY(rRectSource.Bottom(), aMapResDest));
}

static void lcl_verifyUnitSourceDest(MapUnit eUnitSource, MapUnit eUnitDest)
{
    DBG_ASSERT(eUnitSource != MapUnit::MapSysFont && eUnitSource != MapUnit::MapAppFont
                   && eUnitSource != MapUnit::MapRelative,
               "Source MapUnit is not permitted");
    DBG_ASSERT(eUnitDest != MapUnit::MapSysFont && eUnitDest != MapUnit::MapAppFont
                   && eUnitDest != MapUnit::MapRelative,
               "Destination MapUnit is not permitted");
}

static auto lcl_getCorrectedUnit(MapUnit eMapSrc, MapUnit eMapDst)
{
    o3tl::Length eSrc = o3tl::Length::invalid;
    o3tl::Length eDst = o3tl::Length::invalid;

    if (eMapSrc > MapUnit::MapPixel)
    {
        SAL_WARN("vcl.gdi", "Invalid source map unit");
    }
    else if (eMapDst > MapUnit::MapPixel)
    {
        SAL_WARN("vcl.gdi", "Invalid destination map unit");
    }
    else if (eMapSrc != eMapDst)
    {
        eSrc = MapToO3tlLength(eMapSrc, o3tl::Length::pt);
        eDst = MapToO3tlLength(eMapDst, o3tl::Length::pt);
    }

    return std::make_pair(eSrc, eDst);
}

Point LogicToLogic(const Point& rPtSource, const MapMode& rMapModeSource,
                   const MapMode& rMapModeDest)
{
    if (rMapModeSource == rMapModeDest)
        return rPtSource;

    MapUnit eUnitSource = rMapModeSource.GetMapUnit();
    MapUnit eUnitDest = rMapModeDest.GetMapUnit();
    lcl_verifyUnitSourceDest(eUnitSource, eUnitDest);

    if (rMapModeSource.IsSimple() && rMapModeDest.IsSimple())
    {
        const auto[eFrom, eTo] = lcl_getCorrectedUnit(eUnitSource, eUnitDest);
        return Point(lcl_convertLogicValue(rPtSource.X(), eFrom, eTo),
                     lcl_convertLogicValue(rPtSource.Y(), eFrom, eTo));
    }

    const auto[aMapResSource, aMapResDest] = lcl_calcConversionMapRes(rMapModeSource, rMapModeDest);
    return Point(aMapResSource.TransformPointX(rPtSource.X(), aMapResDest),
                 aMapResSource.TransformPointY(rPtSource.Y(), aMapResDest));
}

Size LogicToLogic(const Size& rSzSource, const MapMode& rMapModeSource, const MapMode& rMapModeDest)
{
    if (rMapModeSource == rMapModeDest)
        return rSzSource;

    MapUnit eUnitSource = rMapModeSource.GetMapUnit();
    MapUnit eUnitDest = rMapModeDest.GetMapUnit();
    lcl_verifyUnitSourceDest(eUnitSource, eUnitDest);

    if (rMapModeSource.IsSimple() && rMapModeDest.IsSimple())
    {
        const auto[eFrom, eTo] = lcl_getCorrectedUnit(eUnitSource, eUnitDest);
        return Size(lcl_convertLogicValue(rSzSource.Width(), eFrom, eTo),
                    lcl_convertLogicValue(rSzSource.Height(), eFrom, eTo));
    }

    const auto[aMapResSource, aMapResDest] = lcl_calcConversionMapRes(rMapModeSource, rMapModeDest);
    return Size(aMapResSource.ScaleDistanceX(rSzSource.Width(), aMapResDest),
                aMapResSource.ScaleDistanceY(rSzSource.Height(), aMapResDest));
}

tools::Rectangle LogicToLogic(const tools::Rectangle& rRectSource, const MapMode& rMapModeSource,
                              const MapMode& rMapModeDest)
{
    if (rMapModeSource == rMapModeDest)
        return rRectSource;

    MapUnit eUnitSource = rMapModeSource.GetMapUnit();
    MapUnit eUnitDest = rMapModeDest.GetMapUnit();
    lcl_verifyUnitSourceDest(eUnitSource, eUnitDest);

    tools::Rectangle aRetval;

    if (rMapModeSource.IsSimple() && rMapModeDest.IsSimple())
    {
        const auto[eFrom, eTo] = lcl_getCorrectedUnit(eUnitSource, eUnitDest);

        const auto left = lcl_convertLogicValue(rRectSource.Left(), eFrom, eTo);
        const auto top = lcl_convertLogicValue(rRectSource.Top(), eFrom, eTo);
        const auto right = rRectSource.IsWidthEmpty()
                               ? 0
                               : lcl_convertLogicValue(rRectSource.Right(), eFrom, eTo);
        const auto bottom = rRectSource.IsHeightEmpty()
                                ? 0
                                : lcl_convertLogicValue(rRectSource.Bottom(), eFrom, eTo);

        aRetval = tools::Rectangle(left, top, right, bottom);
    }
    else
    {
        const auto[aMapResSource, aMapResDest]
            = lcl_calcConversionMapRes(rMapModeSource, rMapModeDest);

        const auto left = aMapResSource.TransformPointX(rRectSource.Left(), aMapResDest);
        const auto top = aMapResSource.TransformPointY(rRectSource.Top(), aMapResDest);
        const auto right = rRectSource.IsWidthEmpty()
                               ? 0
                               : aMapResSource.TransformPointX(rRectSource.Right(), aMapResDest);
        const auto bottom = rRectSource.IsHeightEmpty()
                                ? 0
                                : aMapResSource.TransformPointY(rRectSource.Bottom(), aMapResDest);

        aRetval = tools::Rectangle(left, top, right, bottom);
    }

    lcl_ApplyEmptyState(aRetval, rRectSource);
    return aRetval;
}

tools::Long LogicToLogic(tools::Long nLongSource, MapUnit eUnitSource, MapUnit eUnitDest)
{
    if (eUnitSource == eUnitDest)
        return nLongSource;

    lcl_verifyUnitSourceDest(eUnitSource, eUnitDest);
    const auto[eFrom, eTo] = lcl_getCorrectedUnit(eUnitSource, eUnitDest);
    return lcl_convertLogicValue(nLongSource, eFrom, eTo);
}

basegfx::B2DPolygon LogicToLogic(const basegfx::B2DPolygon& rPolySource,
                                 const MapMode& rMapModeSource, const MapMode& rMapModeDest)
{
    if (rMapModeSource == rMapModeDest)
        return rPolySource;

    const basegfx::B2DHomMatrix aTransform(::LogicToLogic(rMapModeSource, rMapModeDest));
    basegfx::B2DPolygon aPoly(rPolySource);
    aPoly.transform(aTransform);
    return aPoly;
}

basegfx::B2DHomMatrix LogicToLogic(const MapMode& rMapModeSource, const MapMode& rMapModeDest)
{
    basegfx::B2DHomMatrix aTransform;

    if (rMapModeSource == rMapModeDest)
        return aTransform;

    MapUnit eUnitSource = rMapModeSource.GetMapUnit();
    MapUnit eUnitDest = rMapModeDest.GetMapUnit();
    lcl_verifyUnitSourceDest(eUnitSource, eUnitDest);

    if (rMapModeSource.IsSimple() && rMapModeDest.IsSimple())
    {
        const auto[eFrom, eTo] = lcl_getCorrectedUnit(eUnitSource, eUnitDest);
        if (eFrom == o3tl::Length::invalid || eTo == o3tl::Length::invalid)
        {
            SAL_WARN("vcl.gdi", "CoordinateMapper: Invalid MapUnit conversion requested. Falling "
                                "back to identity matrix.");
            return aTransform;
        }

        const double fScaleFactor = o3tl::convert(1.0, eFrom, eTo);
        aTransform.set(0, 0, fScaleFactor);
        aTransform.set(1, 1, fScaleFactor);
        return aTransform;
    }

    const auto[aMapResSource, aMapResDest] = lcl_calcConversionMapRes(rMapModeSource, rMapModeDest);

    const double fDestScX = (aMapResDest.mfScaleX != 0.0) ? aMapResDest.mfScaleX : 1.0;
    const double fDestScY = (aMapResDest.mfScaleY != 0.0) ? aMapResDest.mfScaleY : 1.0;

    const double fScaleFactorX(aMapResSource.mfScaleX / fDestScX);
    const double fScaleFactorY(aMapResSource.mfScaleY / fDestScY);
    const double fZeroPointX(double(aMapResSource.mnTranslationX) * fScaleFactorX
                             - double(aMapResDest.mnTranslationX));
    const double fZeroPointY(double(aMapResSource.mnTranslationY) * fScaleFactorY
                             - double(aMapResDest.mnTranslationY));

    aTransform.set(0, 0, fScaleFactorX);
    aTransform.set(1, 1, fScaleFactorY);
    aTransform.set(0, 2, fZeroPointX);
    aTransform.set(1, 2, fZeroPointY);

    return aTransform;
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
