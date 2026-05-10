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

#include <vcl/svapp.hxx>
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

static double lcl_GetBasisVectorMagnitudeX(const basegfx::B2DHomMatrix& m)
{
    basegfx::B2DVector vx(1.0, 0.0);
    vx *= m;
    return vx.getLength();
}

static double lcl_GetBasisVectorMagnitudeY(const basegfx::B2DHomMatrix& m)
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

static bool lcl_IsAxisAligned(const basegfx::B2DHomMatrix& rMat)
{
    constexpr double fEpsilon = 1e-9;
    return std::abs(rMat.get(0, 1)) < fEpsilon && std::abs(rMat.get(1, 0)) < fEpsilon;
}

static tools::Rectangle lcl_RangeToVCLRect(const basegfx::B2DRange& rRange)
{
    return tools::Rectangle(lcl_RoundToLong(rRange.getMinX()), lcl_RoundToLong(rRange.getMinY()),
                            lcl_RoundToLong(rRange.getMaxX()) - 1,
                            lcl_RoundToLong(rRange.getMaxY()) - 1);
}

/**
 * THE CANONICAL AFFINE BUILDER
 *
 * ARCHITECTURAL INVARIANT: Matrix Composition Order
 * basegfx::B2DHomMatrix applies operations via post-multiplication.
 * Therefore, the sequence: translate(A) -> scale(S) -> translate(B)
 * mathematically equates to the transformation:
 *
 *          P' = ((P + A) * S) + B
 *
 * Variables:
 *
 * P  (Point)      = The input coordinate
 * A  (LogicTx)    = Logical offset (applied before scaling)
 * S  (Scale)      = DPI, MapMode, and UI scaling factors
 * B  (PhysicalTx) = Absolute physical offset (applied after scaling)
 *
 * This proof ensures that:
 *
 * 1. Logical offsets (A) grow/shrink with the MapMode zoom level.
 * 2. Physical/Viewport offsets (B) remain constant screen pixels.
 * 3. The algebraic expansion [P*S + A*S + B] is consistent with legacy
 *    VCL manual matrix slot injections.
 */

static basegfx::B2DHomMatrix lcl_BuildAffineMatrix(double fScaleX, double fScaleY, double fLogicTx,
                                                   double fLogicTy, double fPhysicalTx,
                                                   double fPhysicalTy)
{
    basegfx::B2DHomMatrix aMat;
    aMat.translate(fLogicTx, fLogicTy);
    aMat.scale(fScaleX, fScaleY);
    aMat.translate(fPhysicalTx, fPhysicalTy);
    return aMat;
}

// Conceptual Pipeline Separation (Mathematical Invariant):
// Logic -> View: Scaled transformations (Scale * Logic) + Scaled Offsets ((MapOfs + LogicOfs) * Scale)
// View -> Window: Pure translation (WindowOfs)
// Window -> Device: Pure translation (DeviceOfs)
void CoordinateMapper::GetLogicToViewWeights(double& rScaleX, double& rScaleY, double& rTransX,
                                             double& rTransY, vcl::MappingPolicy ePolicy) const
{
    const basegfx::B2DHomMatrix aMat = GetLogicToWindowMatrix(ePolicy);

    rScaleX = lcl_GetBasisVectorMagnitudeX(aMat);
    rScaleY = lcl_GetBasisVectorMagnitudeY(aMat);

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

Size CoordinateMapper::LogicToViewDistance(const Size& rLogicSize, vcl::MappingPolicy ePolicy) const
{
    const basegfx::B2DHomMatrix aMat = GetLogicToWindowMatrix(ePolicy);
    const double sx = lcl_GetBasisVectorMagnitudeX(aMat);
    const double sy = lcl_GetBasisVectorMagnitudeY(aMat);
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
                                                            const MapMode* pTarget,
                                                            vcl::MappingPolicy ePolicy) const
{
    return maMapRes.ResolveMapRes(pTarget, *pBaseline, ePolicy, mnDPIX, mnDPIY);
}

vcl::detail::MapConversion CoordinateMapper::ResolveMap(const MapMode& rBaseline,
                                                        const MapMode& rTarget,
                                                        vcl::MappingPolicy ePolicy) const
{
    // Evaluates a temporary MapMode against the current accumulated state
    MappingCoefficients aRes = maMapRes.ResolveMapRes(&rTarget, rBaseline, ePolicy, mnDPIX, mnDPIY);
    return { aRes.mfScaleX, aRes.mfScaleY, aRes.mnTranslationX, aRes.mnTranslationY };
}

void CoordinateMapper::InvalidateViewTransform()
{
    mnStateVersion.fetch_add(1, std::memory_order_release);
}

basegfx::B2DHomMatrix CoordinateMapper::GetDeviceTransformation(vcl::MappingPolicy ePolicy) const
{
    return GetLogicToDeviceMatrix(ePolicy);
}

basegfx::B2DHomMatrix CoordinateMapper::GetViewTransformation(vcl::MappingPolicy ePolicy) const
{
    return GetLogicToWindowMatrix(ePolicy);
}

basegfx::B2DHomMatrix
CoordinateMapper::GetInverseViewTransformation(vcl::MappingPolicy ePolicy) const
{
    return GetWindowToLogicMatrix(ePolicy);
}

basegfx::B2DHomMatrix
CoordinateMapper::GetViewTransformation(const vcl::detail::MapConversion& rConv) const
{
    // NOTE: This path intentionally bypasses the transform cache because MapConversion
    // represents a temporary, externally resolved mapping state (e.g. for MapMode evaluation).
    //
    // IMPORTANT: The affine composition performed here must remain algebraically consistent
    // with UpdateCache(ePolicy=true), specifically regarding logical-offset scaling semantics
    // and the ordering of physical viewport offsets.
    const double fScaleFactorX = static_cast<double>(GetDPIX()) * rConv.mfScaleX;
    const double fScaleFactorY = static_cast<double>(GetDPIY()) * rConv.mfScaleY;

    return lcl_BuildAffineMatrix(fScaleFactorX, fScaleFactorY,
                                 static_cast<double>(rConv.mnOffsetX + mnLogicToAbsoluteOffsetX),
                                 static_cast<double>(rConv.mnOffsetY + mnLogicToAbsoluteOffsetY),
                                 static_cast<double>(mnWindowToViewOffsetX),
                                 static_cast<double>(mnWindowToViewOffsetY));
}

basegfx::B2DHomMatrix CoordinateMapper::GetViewTransformation(const MapMode& rBaseline,
                                                              const MapMode& rTarget,
                                                              vcl::MappingPolicy ePolicy) const
{
    return GetViewTransformation(ResolveMap(rBaseline, rTarget, ePolicy));
}

basegfx::B2DHomMatrix
CoordinateMapper::GetInverseViewTransformation(const vcl::detail::MapConversion& rConv) const
{
    basegfx::B2DHomMatrix aMatrix(GetViewTransformation(rConv));
    aMatrix.invert();
    return aMatrix;
}

basegfx::B2DHomMatrix
CoordinateMapper::GetInverseViewTransformation(const MapMode& rBaseline, const MapMode& rTarget,
                                               vcl::MappingPolicy ePolicy) const
{
    return GetInverseViewTransformation(ResolveMap(rBaseline, rTarget, ePolicy));
}

// ============================================================================
// THE O(1) TRANSFORM REGISTER FILE
// ============================================================================

void CoordinateMapper::UpdateCache(vcl::MappingPolicy ePolicy) const
{
    // ARCHITECTURAL GUARD: Ensure enum layout is valid for slot arithmetic
    static_assert(static_cast<size_t>(TransformSlot::LogicToWindow_Unmapped)
                  == static_cast<size_t>(TransformSlot::LogicToWindow_Mapped) + 1);
    static_assert(static_cast<size_t>(TransformSlot::LogicToDevice_Unmapped)
                  == static_cast<size_t>(TransformSlot::LogicToDevice_Mapped) + 1);

    DBG_TESTSOLARMUTEX();

    const size_t nSlotOffset = ePolicy == vcl::MappingPolicy::ApplyMapMode ? 0 : 1;

    // Phase 1: Logic -> Window (The core mapping)
    basegfx::B2DHomMatrix aLogicToWindow;
    if (ePolicy == vcl::MappingPolicy::ApplyMapMode)
    {
        const double fScaleX
            = maMapRes.mfScaleX * static_cast<double>(mnDPIX) * GetDPIScaleFactor();
        const double fScaleY
            = maMapRes.mfScaleY * static_cast<double>(mnDPIY) * GetDPIScaleFactor();

        aLogicToWindow = lcl_BuildAffineMatrix(
            fScaleX, fScaleY,
            static_cast<double>(maMapRes.mnTranslationX + mnLogicToAbsoluteOffsetX),
            static_cast<double>(maMapRes.mnTranslationY + mnLogicToAbsoluteOffsetY),
            static_cast<double>(mnWindowToViewOffsetX), static_cast<double>(mnWindowToViewOffsetY));
    }
    else
    {
        // Unmapped case: Pure viewport translation
        aLogicToWindow.translate(static_cast<double>(mnWindowToViewOffsetX),
                                 static_cast<double>(mnWindowToViewOffsetY));
    }

    // Atomic Snapshot for Logic -> Window
    maTransformCache[static_cast<size_t>(TransformSlot::LogicToWindow_Mapped) + nSlotOffset]
        = BuildCompiledTransform(aLogicToWindow);

    // Phase 2: Window -> Logic (The Inverse)
    if (aLogicToWindow.isInvertible())
    {
        basegfx::B2DHomMatrix aWindowToLogic = aLogicToWindow;
        aWindowToLogic.invert();
        maTransformCache[static_cast<size_t>(TransformSlot::WindowToLogic_Mapped) + nSlotOffset]
            = BuildCompiledTransform(aWindowToLogic);
    }
    else
    {
        SAL_WARN("vcl.gdi", "CoordinateMapper: Singular Matrix. Falling back to Identity.");
        maTransformCache[static_cast<size_t>(TransformSlot::WindowToLogic_Mapped) + nSlotOffset]
            = BuildCompiledTransform(basegfx::B2DHomMatrix());
    }

    // Phase 3: Logic -> Device (Applying physical OS offsets)
    basegfx::B2DHomMatrix aLogicToDevice = aLogicToWindow;
    aLogicToDevice.translate(static_cast<double>(mnDeviceToWindowOffsetX),
                             static_cast<double>(mnDeviceToWindowOffsetY));

    maTransformCache[static_cast<size_t>(TransformSlot::LogicToDevice_Mapped) + nSlotOffset]
        = BuildCompiledTransform(aLogicToDevice);

    // Phase 4: Device -> Logic
    if (aLogicToDevice.isInvertible())
    {
        basegfx::B2DHomMatrix aDeviceToLogic = aLogicToDevice;
        aDeviceToLogic.invert();
        maTransformCache[static_cast<size_t>(TransformSlot::DeviceToLogic_Mapped) + nSlotOffset]
            = BuildCompiledTransform(aDeviceToLogic);
    }
    else
    {
        maTransformCache[static_cast<size_t>(TransformSlot::DeviceToLogic_Mapped) + nSlotOffset]
            = BuildCompiledTransform(basegfx::B2DHomMatrix());
    }
}

CompiledTransform CoordinateMapper::BuildCompiledTransform(const basegfx::B2DHomMatrix& rMat) const
{
    CompiledTransform aTransform;
    aTransform.maMatrix = rMat;

    // Structural Invariants: Inherent to the affine model.
    aTransform.maContract.maPreserved.set(static_cast<size_t>(GeometryInvariant::Parallelism));
    aTransform.maContract.maPreserved.set(static_cast<size_t>(GeometryInvariant::Connectivity));

    // Orientation Invariant: Handedness check.
    // det = ad - bc. Epsilon-guarded for numerical stability.
    const double fDet = rMat.get(0, 0) * rMat.get(1, 1) - rMat.get(0, 1) * rMat.get(1, 0);
    if (fDet > 1e-12)
        aTransform.maContract.maPreserved.set(static_cast<size_t>(GeometryInvariant::Orientation));

    // Performance Taxonomy Classification
    if (rMat.isIdentity())
    {
        aTransform.meMode = TransformMode::Identity;
        aTransform.maContract.maPreserved.set(); // All invariants preserved
    }
    else if (lcl_IsAxisAligned(rMat))
    {
        // Rectilinear transforms preserve Axis Alignment and Orthogonality
        aTransform.maContract.maPreserved.set(
            static_cast<size_t>(GeometryInvariant::AxisAlignment));
        aTransform.maContract.maPreserved.set(
            static_cast<size_t>(GeometryInvariant::Orthogonality));

        if (lcl_IsPureTranslation(rMat))
        {
            const double fTx = rMat.get(0, 2);
            const double fTy = rMat.get(1, 2);
            constexpr double fEpsilon = 1e-9;

            // Fast path for integer-only translations (Legacy VCL optimization)
            if (std::abs(fTx - std::round(fTx)) < fEpsilon
                && std::abs(fTy - std::round(fTy)) < fEpsilon)
            {
                aTransform.meMode = TransformMode::Translation;
                aTransform.mnDeviceTx = static_cast<tools::Long>(std::round(fTx));
                aTransform.mnDeviceTy = static_cast<tools::Long>(std::round(fTy));
            }
            else
            {
                // Floating point translation or non-identity scale
                aTransform.meMode = TransformMode::AxisAlignedAffine;
            }
        }
        else
        {
            // Axis-aligned but contains scaling (DPI, MapMode, etc.)
            aTransform.meMode = TransformMode::AxisAlignedAffine;
        }
    }
    else
    {
        // SEMANTIC COLLAPSE: Transform is rotated or sheared.
        aTransform.meMode = TransformMode::AffineFallback;
    }

    return aTransform;
}

const CompiledTransform& CoordinateMapper::Compile(const TransformRequest& rReq) const
{
    // O(1) Cache Version Validation.
    // CoordinateMapper is externally synchronized via SolarMutex.
    // The atomic version counter is used only for cache invalidation visibility,
    // not to provide full internal thread safety.
    uint64_t nCurrentVersion = mnStateVersion.load(std::memory_order_acquire);
    if (mnCacheVersion != nCurrentVersion)
    {
        for (auto& slot : maTransformCache)
            slot.reset();
        mnCacheVersion = nCurrentVersion;
    }

    size_t nOffset = rReq.Policy == vcl::MappingPolicy::ApplyMapMode ? 0 : 1;
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
        UpdateCache(rReq.Policy);
    }

    return *maTransformCache[static_cast<size_t>(eSlot)];
}

// ============================================================================
// SINGLE SOURCE OF TRUTH: MATRIX BUILDERS (NOW ROUTES TO COMPILE)
// ============================================================================

basegfx::B2DHomMatrix CoordinateMapper::GetLogicToDeviceMatrix(vcl::MappingPolicy ePolicy) const
{
    return Compile({ CoordinateSpace::Logic, CoordinateSpace::Device, ePolicy }).GetMatrix();
}

basegfx::B2DHomMatrix CoordinateMapper::GetDeviceToLogicMatrix(vcl::MappingPolicy ePolicy) const
{
    return Compile({ CoordinateSpace::Device, CoordinateSpace::Logic, ePolicy }).GetMatrix();
}

basegfx::B2DHomMatrix CoordinateMapper::GetLogicToWindowMatrix(vcl::MappingPolicy ePolicy) const
{
    return Compile({ CoordinateSpace::Logic, CoordinateSpace::Window, ePolicy }).GetMatrix();
}

basegfx::B2DHomMatrix CoordinateMapper::GetWindowToLogicMatrix(vcl::MappingPolicy ePolicy) const
{
    return Compile({ CoordinateSpace::Window, CoordinateSpace::Logic, ePolicy }).GetMatrix();
}

// ============================================================================
// TEMPLATE SPECIALIZATIONS: THE UNIVERSAL GEOMETRY PIPELINE
// ============================================================================

template <> Point CompiledTransform::Apply<Point>(const Point& rPt) const
{
    switch (meMode)
    {
        case TransformMode::Identity:
            [[likely]] return rPt;

        case TransformMode::Translation:
            return Point(rPt.X() + mnDeviceTx, rPt.Y() + mnDeviceTy);

        case TransformMode::AxisAlignedAffine:
        {
            // Stabilization: Use basegfx logic to ensure rounding consistency
            // with the full affine path, avoiding 1-pixel 'jitter' in unit tests.
            basegfx::B2DPoint aPt(rPt.X(), rPt.Y());

            double fX = aPt.getX() * maMatrix.get(0, 0) + maMatrix.get(0, 2);
            double fY = aPt.getY() * maMatrix.get(1, 1) + maMatrix.get(1, 2);

            return Point(lcl_RoundToLong(fX), lcl_RoundToLong(fY));
        }

        case TransformMode::AffineFallback:
        default:
            // Full 3x3 Affine Transformation
            basegfx::B2DPoint aB2DPt(rPt.X(), rPt.Y());
            aB2DPt *= maMatrix;
            return Point(lcl_RoundToLong(aB2DPt.getX()), lcl_RoundToLong(aB2DPt.getY()));
    }
}

Size CompiledTransform::ApplyRectilinear(const Size& rSize) const
{
    // NOTE: We preserve the sign of diagonal elements (m00, m11) to allow
    // Size to reflect basis orientation/mirroring if the MapMode is flipped.
    double fWidth = static_cast<double>(rSize.Width()) * maMatrix.get(0, 0);
    double fHeight = static_cast<double>(rSize.Height()) * maMatrix.get(1, 1);

    return Size(lcl_RoundToLong(fWidth), lcl_RoundToLong(fHeight));
}

tools::Rectangle CompiledTransform::ApplyRectilinear(const tools::Rectangle& rRect) const
{
    if (rRect.IsEmpty())
        return tools::Rectangle();

    double fL = static_cast<double>(rRect.Left()) * maMatrix.get(0, 0) + maMatrix.get(0, 2);
    double fT = static_cast<double>(rRect.Top()) * maMatrix.get(1, 1) + maMatrix.get(1, 2);
    double fW = static_cast<double>(rRect.GetWidth()) * maMatrix.get(0, 0);
    double fH = static_cast<double>(rRect.GetHeight()) * maMatrix.get(1, 1);

    tools::Long nL = lcl_RoundToLong(fL);
    tools::Long nT = lcl_RoundToLong(fT);
    tools::Long nW = lcl_RoundToLong(fW);
    tools::Long nH = lcl_RoundToLong(fH);

    if (nW == 0 && rRect.GetWidth() > 0)
        nW = 1;
    if (nH == 0 && rRect.GetHeight() > 0)
        nH = 1;

    return tools::Rectangle(Point(nL, nT), Size(nW, nH));
}

template <> Size CompiledTransform::Apply<Size>(const Size& rSize) const
{
    if (PreservesAxisAlignment())
        return ApplyRectilinear(rSize);

    // Path B: Basis Magnitude Approximation
    const double fNewWidth
        = static_cast<double>(rSize.Width()) * lcl_GetBasisVectorMagnitudeX(maMatrix);
    const double fNewHeight
        = static_cast<double>(rSize.Height()) * lcl_GetBasisVectorMagnitudeY(maMatrix);

    return Size(lcl_RoundToLong(fNewWidth), lcl_RoundToLong(fNewHeight));
}

template <>
tools::Rectangle CompiledTransform::Apply<tools::Rectangle>(const tools::Rectangle& rRect) const
{
    // Pure query: Silent selection of Path A or Path B
    if (PreservesAxisAlignment())
        return ApplyRectilinear(rRect);

    // Path B: Conservative AABB
    basegfx::B2DRange aRange(rRect.Left(), rRect.Top(), rRect.Right() + 1, rRect.Bottom() + 1);
    aRange.transform(maMatrix);
    tools::Rectangle aRet = lcl_RangeToVCLRect(aRange);

    if (rRect.IsEmpty())
        aRet.SetEmpty();

    return aRet;
}

template <>
tools::Polygon CompiledTransform::Apply<tools::Polygon>(const tools::Polygon& rPoly) const
{
    if (meMode == TransformMode::Identity)
        [[likely]] return rPoly;

    tools::Polygon aPoly(rPoly);
    for (sal_uInt16 i = 0; i < aPoly.GetSize(); ++i)
    {
        aPoly[i] = Apply(aPoly[i]);
    }

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
    {
        aPolyPoly.Insert(Apply(rPolyPoly[i]));
    }

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
        [[likely]] return rRegion;

    if (meMode == TransformMode::Translation)
    {
        vcl::Region aRet(rRegion);
        aRet.Move(mnDeviceTx, mnDeviceTy);
        return aRet;
    }

    // PDF Structural Fix: Use the PolyPolygon bridge ONLY if the region
    // is already a complex polygon.
    if (rRegion.getB2DPolyPolygon())
        return vcl::Region(Apply(*rRegion.getB2DPolyPolygon()));

    if (rRegion.getPolyPolygon())
        return vcl::Region(Apply(*rRegion.getPolyPolygon()));

    // UI Paint Rectangles MUST fallback to AABB aggregation so they
    // hit ApplyRectilinear() and benefit from the nW=1 / nH=1 footprint safeguards,
    // avoiding destructive scan-conversion.
    vcl::Region aRegion;
    RectangleVector aRectangles;
    rRegion.GetRegionRectangles(aRectangles);

    for (const auto& rRect : aRectangles)
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

double CoordinateMapper::LogicToWindowSubPixelX(double fX, vcl::MappingPolicy ePolicy) const
{
    const auto& rTransform = Compile({ CoordinateSpace::Logic, CoordinateSpace::Window, ePolicy });

    const auto& rMat = rTransform.GetMatrix();

    DBG_ASSERT(lcl_IsAxisAligned(rMat), "LogicToWindowSubPixelX requires axis-aligned transform");

    // NOTE:
    // This helper assumes an axis-aligned transform (no shear/rotation).
    // VCL MapModes cannot currently generate shear, so m00 fully defines
    // the X scale factor.
    return fX * rMat.get(0, 0) + rMat.get(0, 2);
}

double CoordinateMapper::LogicToWindowSubPixelY(double fY, vcl::MappingPolicy ePolicy) const
{
    const auto& rTransform = Compile({ CoordinateSpace::Logic, CoordinateSpace::Window, ePolicy });

    const auto& rMat = rTransform.GetMatrix();

    DBG_ASSERT(lcl_IsAxisAligned(rMat), "LogicToWindowSubPixelY requires axis-aligned transform");

    // NOTE:
    // This helper assumes an axis-aligned transform (no shear/rotation).
    // VCL MapModes cannot currently generate shear, so m11 fully defines
    // the Y scale factor.
    return fY * rMat.get(1, 1) + rMat.get(1, 2);
}

// ========================================================================
// PUBLIC WRAPPERS (Routing into the unified pipeline)
// ========================================================================

Point CoordinateMapper::LogicToDevicePixel(const Point& rLogicPt, vcl::MappingPolicy ePolicy) const
{
    return Compile({ CoordinateSpace::Logic, CoordinateSpace::Device, ePolicy }).Apply(rLogicPt);
}

tools::Rectangle CoordinateMapper::LogicToDevicePixel(const tools::Rectangle& rLogicRect,
                                                      vcl::MappingPolicy ePolicy) const
{
    return Compile({ CoordinateSpace::Logic, CoordinateSpace::Device, ePolicy }).Apply(rLogicRect);
}

tools::Polygon CoordinateMapper::LogicToDevicePixel(const tools::Polygon& rLogicPoly,
                                                    vcl::MappingPolicy ePolicy) const
{
    CompiledTransform t = Compile({ CoordinateSpace::Logic, CoordinateSpace::Device, ePolicy });
    if (t.GetMode() == TransformMode::AffineFallback)
    {
        // TODO: Legacy Note: We currently don't AdaptiveSubdivide inside Apply<Polygon> because
        // subdivision relies on global VCL tools settings which breaks abstraction. We
        // leave it here temporarily.
        tools::Polygon aSubdivided;
        rLogicPoly.AdaptiveSubdivide(aSubdivided);
        return t.Apply(aSubdivided);
    }
    return t.Apply(rLogicPoly);
}

tools::PolyPolygon CoordinateMapper::LogicToDevicePixel(const tools::PolyPolygon& rLogicPolyPoly,
                                                        vcl::MappingPolicy ePolicy) const
{
    return Compile({ CoordinateSpace::Logic, CoordinateSpace::Device, ePolicy })
        .Apply(rLogicPolyPoly);
}

LineInfo CoordinateMapper::LogicToDevicePixel(const LineInfo& rLineInfo,
                                              vcl::MappingPolicy ePolicy) const
{
    return Compile({ CoordinateSpace::Logic, CoordinateSpace::Device, ePolicy }).Apply(rLineInfo);
}

basegfx::B2DPolygon CoordinateMapper::LogicToDevicePixel(const basegfx::B2DPolygon& rLogicPoly,
                                                         vcl::MappingPolicy ePolicy) const
{
    return Compile({ CoordinateSpace::Logic, CoordinateSpace::Device, ePolicy }).Apply(rLogicPoly);
}

basegfx::B2DPolyPolygon
CoordinateMapper::LogicToDevicePixel(const basegfx::B2DPolyPolygon& rLogicPolyPoly,
                                     vcl::MappingPolicy ePolicy) const
{
    return Compile({ CoordinateSpace::Logic, CoordinateSpace::Device, ePolicy })
        .Apply(rLogicPolyPoly);
}

vcl::Region CoordinateMapper::LogicToDevicePixel(const vcl::Region& rRegion,
                                                 vcl::MappingPolicy ePolicy) const
{
    return Compile({ CoordinateSpace::Logic, CoordinateSpace::Device, ePolicy }).Apply(rRegion);
}

Size CoordinateMapper::LogicToDevicePixel(const Size& rLogicSize, vcl::MappingPolicy ePolicy) const
{
    return Compile({ CoordinateSpace::Logic, CoordinateSpace::Device, ePolicy }).Apply(rLogicSize);
}

Point CoordinateMapper::DevicePixelToLogic(const Point& rDevicePt, vcl::MappingPolicy ePolicy) const
{
    return Compile({ CoordinateSpace::Device, CoordinateSpace::Logic, ePolicy }).Apply(rDevicePt);
}

tools::Rectangle CoordinateMapper::DevicePixelToLogic(const tools::Rectangle& rPixelRect,
                                                      vcl::MappingPolicy ePolicy) const
{
    return Compile({ CoordinateSpace::Device, CoordinateSpace::Logic, ePolicy }).Apply(rPixelRect);
}

tools::Polygon CoordinateMapper::DevicePixelToLogic(const tools::Polygon& rPixelPoly,
                                                    vcl::MappingPolicy ePolicy) const
{
    return Compile({ CoordinateSpace::Device, CoordinateSpace::Logic, ePolicy }).Apply(rPixelPoly);
}

tools::PolyPolygon CoordinateMapper::DevicePixelToLogic(const tools::PolyPolygon& rPixelPolyPoly,
                                                        vcl::MappingPolicy ePolicy) const
{
    return Compile({ CoordinateSpace::Device, CoordinateSpace::Logic, ePolicy })
        .Apply(rPixelPolyPoly);
}

basegfx::B2DPolygon CoordinateMapper::DevicePixelToLogic(const basegfx::B2DPolygon& rPixelPoly,
                                                         vcl::MappingPolicy ePolicy) const
{
    return Compile({ CoordinateSpace::Device, CoordinateSpace::Logic, ePolicy }).Apply(rPixelPoly);
}

basegfx::B2DPolyPolygon
CoordinateMapper::DevicePixelToLogic(const basegfx::B2DPolyPolygon& rPixelPolyPoly,
                                     vcl::MappingPolicy ePolicy) const
{
    return Compile({ CoordinateSpace::Device, CoordinateSpace::Logic, ePolicy })
        .Apply(rPixelPolyPoly);
}

vcl::Region CoordinateMapper::DevicePixelToLogic(const vcl::Region& rRegion,
                                                 vcl::MappingPolicy ePolicy) const
{
    return Compile({ CoordinateSpace::Device, CoordinateSpace::Logic, ePolicy }).Apply(rRegion);
}

Size CoordinateMapper::DevicePixelToLogic(const Size& rDeviceSize, vcl::MappingPolicy ePolicy) const
{
    return Compile({ CoordinateSpace::Device, CoordinateSpace::Logic, ePolicy }).Apply(rDeviceSize);
}

Point CoordinateMapper::LogicToWindowUnits(const Point& rLogicPt, vcl::MappingPolicy ePolicy) const
{
    return Compile({ CoordinateSpace::Logic, CoordinateSpace::Window, ePolicy }).Apply(rLogicPt);
}

tools::Rectangle CoordinateMapper::LogicToWindowUnits(const tools::Rectangle& rRect,
                                                      vcl::MappingPolicy ePolicy) const
{
    return Compile({ CoordinateSpace::Logic, CoordinateSpace::Window, ePolicy }).Apply(rRect);
}

tools::Polygon CoordinateMapper::LogicToWindowUnits(const tools::Polygon& rPoly,
                                                    vcl::MappingPolicy ePolicy) const
{
    return Compile({ CoordinateSpace::Logic, CoordinateSpace::Window, ePolicy }).Apply(rPoly);
}

tools::PolyPolygon CoordinateMapper::LogicToWindowUnits(const tools::PolyPolygon& rPolyPoly,
                                                        vcl::MappingPolicy ePolicy) const
{
    return Compile({ CoordinateSpace::Logic, CoordinateSpace::Window, ePolicy }).Apply(rPolyPoly);
}

vcl::Region CoordinateMapper::LogicToWindowUnits(const vcl::Region& rRegion,
                                                 vcl::MappingPolicy ePolicy) const
{
    return Compile({ CoordinateSpace::Logic, CoordinateSpace::Window, ePolicy }).Apply(rRegion);
}

Size CoordinateMapper::LogicToWindowUnits(const Size& rLogicSize, vcl::MappingPolicy ePolicy) const
{
    return Compile({ CoordinateSpace::Logic, CoordinateSpace::Window, ePolicy }).Apply(rLogicSize);
}

Point CoordinateMapper::WindowToLogicUnits(const Point& rWindowPt, vcl::MappingPolicy ePolicy) const
{
    return Compile({ CoordinateSpace::Window, CoordinateSpace::Logic, ePolicy }).Apply(rWindowPt);
}

tools::Rectangle CoordinateMapper::WindowToLogicUnits(const tools::Rectangle& rWindowRect,
                                                      vcl::MappingPolicy ePolicy) const
{
    return Compile({ CoordinateSpace::Window, CoordinateSpace::Logic, ePolicy }).Apply(rWindowRect);
}

tools::Polygon CoordinateMapper::WindowToLogicUnits(const tools::Polygon& rWindowPoly,
                                                    vcl::MappingPolicy ePolicy) const
{
    return Compile({ CoordinateSpace::Window, CoordinateSpace::Logic, ePolicy }).Apply(rWindowPoly);
}

tools::PolyPolygon CoordinateMapper::WindowToLogicUnits(const tools::PolyPolygon& rWindowPolyPoly,
                                                        vcl::MappingPolicy ePolicy) const
{
    return Compile({ CoordinateSpace::Window, CoordinateSpace::Logic, ePolicy })
        .Apply(rWindowPolyPoly);
}

vcl::Region CoordinateMapper::WindowToLogicUnits(const vcl::Region& rRegion,
                                                 vcl::MappingPolicy ePolicy) const
{
    return Compile({ CoordinateSpace::Window, CoordinateSpace::Logic, ePolicy }).Apply(rRegion);
}

Size CoordinateMapper::WindowToLogicUnits(const Size& rWindowSize, vcl::MappingPolicy ePolicy) const
{
    return Compile({ CoordinateSpace::Window, CoordinateSpace::Logic, ePolicy }).Apply(rWindowSize);
}

// ========================================================================
// DISTANCE EXTRACTORS (Vector Magnitudes)
// ========================================================================

/**
 * NOTE: Under non-axis-aligned transforms (Rotation/Shear), 'Width' is not
 * well-defined as a scalar. This API returns the Euclidean magnitude of
 * the transformed X-basis vector to preserve legacy distance scaling
 * expectations (e.g. for line widths or font heights).
 */
tools::Long CoordinateMapper::LogicWidthToDevicePixel(tools::Long nWidth,
                                                      vcl::MappingPolicy ePolicy) const
{
    const auto& rTransform
        = Compile(TransformRequest{ CoordinateSpace::Logic, CoordinateSpace::Device, ePolicy });

    if (!rTransform.CheckRectilinearContract())
        return lcl_RoundToLong(static_cast<double>(nWidth)
                               * lcl_GetBasisVectorMagnitudeX(rTransform.GetMatrix()));

    return lcl_RoundToLong(static_cast<double>(nWidth) * rTransform.GetMatrix().get(0, 0));
}

tools::Long CoordinateMapper::LogicHeightToDevicePixel(tools::Long nHeight,
                                                       vcl::MappingPolicy ePolicy) const
{
    const auto& rTransform
        = Compile(TransformRequest{ CoordinateSpace::Logic, CoordinateSpace::Device, ePolicy });

    if (!rTransform.CheckRectilinearContract())
        return lcl_RoundToLong(static_cast<double>(nHeight)
                               * lcl_GetBasisVectorMagnitudeY(rTransform.GetMatrix()));

    return lcl_RoundToLong(static_cast<double>(nHeight) * rTransform.GetMatrix().get(1, 1));
}

tools::Long CoordinateMapper::DevicePixelToLogicWidth(tools::Long nWidth,
                                                      vcl::MappingPolicy ePolicy) const
{
    const auto& rTransform
        = Compile(TransformRequest{ CoordinateSpace::Device, CoordinateSpace::Logic, ePolicy });

    if (!rTransform.CheckRectilinearContract())
        return lcl_RoundToLong(static_cast<double>(nWidth)
                               * lcl_GetBasisVectorMagnitudeX(rTransform.GetMatrix()));

    return lcl_RoundToLong(static_cast<double>(nWidth) * rTransform.GetMatrix().get(0, 0));
}

tools::Long CoordinateMapper::DevicePixelToLogicHeight(tools::Long nHeight,
                                                       vcl::MappingPolicy ePolicy) const
{
    const auto& rTransform
        = Compile(TransformRequest{ CoordinateSpace::Device, CoordinateSpace::Logic, ePolicy });

    if (!rTransform.CheckRectilinearContract())
        return lcl_RoundToLong(static_cast<double>(nHeight)
                               * lcl_GetBasisVectorMagnitudeY(rTransform.GetMatrix()));

    return lcl_RoundToLong(static_cast<double>(nHeight) * rTransform.GetMatrix().get(1, 1));
}

double CoordinateMapper::LogicWidthToWindowSubPixel(tools::Long nWidth,
                                                    vcl::MappingPolicy ePolicy) const
{
    const auto& rTransform
        = Compile(TransformRequest{ CoordinateSpace::Logic, CoordinateSpace::Window, ePolicy });

    if (!rTransform.CheckRectilinearContract())
        return static_cast<double>(nWidth) * lcl_GetBasisVectorMagnitudeX(rTransform.GetMatrix());

    return static_cast<double>(nWidth) * rTransform.GetMatrix().get(0, 0);
}

double CoordinateMapper::LogicHeightToWindowSubPixel(tools::Long nHeight,
                                                     vcl::MappingPolicy ePolicy) const
{
    const auto& rTransform
        = Compile(TransformRequest{ CoordinateSpace::Logic, CoordinateSpace::Window, ePolicy });

    if (!rTransform.CheckRectilinearContract())
        return static_cast<double>(nHeight) * lcl_GetBasisVectorMagnitudeY(rTransform.GetMatrix());

    return static_cast<double>(nHeight) * rTransform.GetMatrix().get(1, 1);
}

tools::Long CoordinateMapper::LogicToWindowX(tools::Long nX, vcl::MappingPolicy ePolicy) const
{
    const auto& rTransform
        = Compile(TransformRequest{ CoordinateSpace::Logic, CoordinateSpace::Window, ePolicy });
    const auto& rMat = rTransform.GetMatrix();

    if (!rTransform.PreservesAxisAlignment())
        return lcl_RoundToLong(LogicToWindowSubPixelX(static_cast<double>(nX), ePolicy));

    return lcl_RoundToLong(static_cast<double>(nX) * rMat.get(0, 0) + rMat.get(0, 2));
}

tools::Long CoordinateMapper::LogicToWindowY(tools::Long nY, vcl::MappingPolicy ePolicy) const
{
    const auto& rTransform
        = Compile(TransformRequest{ CoordinateSpace::Logic, CoordinateSpace::Window, ePolicy });
    const auto& rMat = rTransform.GetMatrix();

    if (!rTransform.PreservesAxisAlignment())
        return lcl_RoundToLong(LogicToWindowSubPixelY(static_cast<double>(nY), ePolicy));

    return lcl_RoundToLong(static_cast<double>(nY) * rMat.get(1, 1) + rMat.get(1, 2));
}

basegfx::B2DPoint CoordinateMapper::LogicToDeviceSubPixel(const Point& rPoint,
                                                          vcl::MappingPolicy ePolicy) const
{
    basegfx::B2DHomMatrix aMat = GetLogicToDeviceMatrix(ePolicy);
    basegfx::B2DPoint aPt(rPoint.X(), rPoint.Y());
    aPt *= aMat;
    return aPt;
}

basegfx::B2DPoint CoordinateMapper::DevicePixelToLogicSubPixel(const Point& rDevicePt,
                                                               vcl::MappingPolicy ePolicy) const
{
    basegfx::B2DPoint aPt(rDevicePt.X(), rDevicePt.Y());
    aPt *= GetDeviceToLogicMatrix(ePolicy);
    return aPt;
}

Point CoordinateMapper::WindowSubPixelToLogicUnits(const basegfx::B2DPoint& rWindowPt,
                                                   vcl::MappingPolicy ePolicy) const
{
    basegfx::B2DHomMatrix aMat = GetWindowToLogicMatrix(ePolicy);
    basegfx::B2DPoint aPt(rWindowPt);
    aPt *= aMat;
    return Point(lcl_RoundToLong(aPt.getX()), lcl_RoundToLong(aPt.getY()));
}

template <TransformableB2DGeometry T>
T CoordinateMapper::LogicToWindowUnits(const T& rLogicGeometry, vcl::MappingPolicy ePolicy) const
{
    return Compile({ CoordinateSpace::Logic, CoordinateSpace::Window, ePolicy })
        .Apply(rLogicGeometry);
}

template SAL_DLLPRIVATE basegfx::B2DRectangle
CoordinateMapper::LogicToWindowUnits<basegfx::B2DRectangle>(const basegfx::B2DRectangle&,
                                                            vcl::MappingPolicy) const;

template SAL_DLLPRIVATE basegfx::B2DPolygon
CoordinateMapper::LogicToWindowUnits<basegfx::B2DPolygon>(const basegfx::B2DPolygon&,
                                                          vcl::MappingPolicy) const;

template SAL_DLLPRIVATE basegfx::B2DPolyPolygon
CoordinateMapper::LogicToWindowUnits<basegfx::B2DPolyPolygon>(const basegfx::B2DPolyPolygon&,
                                                              vcl::MappingPolicy) const;

template <TransformableB2DGeometry T>
T CoordinateMapper::WindowToLogicUnits(const T& rWindowGeometry, vcl::MappingPolicy ePolicy) const
{
    return Compile({ CoordinateSpace::Window, CoordinateSpace::Logic, ePolicy })
        .Apply(rWindowGeometry);
}

template SAL_DLLPRIVATE basegfx::B2DRectangle
CoordinateMapper::WindowToLogicUnits<basegfx::B2DRectangle>(const basegfx::B2DRectangle&,
                                                            vcl::MappingPolicy) const;

template SAL_DLLPRIVATE basegfx::B2DPolyPolygon
CoordinateMapper::WindowToLogicUnits<basegfx::B2DPolyPolygon>(const basegfx::B2DPolyPolygon&,
                                                              vcl::MappingPolicy) const;

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

/**
 * IMPORTANT ARCHITECTURAL NOTE: Conservative Inverse Bounds
 * * Because forward transformations project Rectangles into Axis-Aligned
 *   Bounding Boxes (AABBs), inverse transformations of Rectangles are NOT
 *   geometrically symmetrical under rotation or shear.
 * * inverse(transform(rect)) will yield a mathematically inflated AABB
 *   that strictly subsumes the original geometry.
 * * Callers MUST treat inverse-mapped Rectangles as 'Conservative Invalidation
 *   Bounds', NOT as exact hit-testing boundaries. For exact hit-testing under
 *   rotation, map the point forward, or map a basegfx::B2DPolygon backward.
 */
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
                                     vcl::MappingPolicy ePolicy) const
{
    const MapMode* pSrc = pMapModeSource ? pMapModeSource : pMapModeBaseline;
    const MapMode* pDst = pMapModeDest ? pMapModeDest : pMapModeBaseline;

    if (!pSrc || !pDst || *pSrc == *pDst)
        return rPtSource;

    MappingCoefficients aMapResSource = ResolveMapResRelative(pMapModeBaseline, pSrc, ePolicy);
    MappingCoefficients aMapResDest = ResolveMapResRelative(pMapModeBaseline, pDst, ePolicy);

    return Point(aMapResSource.TransformPointX(rPtSource.X(), aMapResDest),
                 aMapResSource.TransformPointY(rPtSource.Y(), aMapResDest));
}

Size CoordinateMapper::LogicToLogic(const Size& rSzSource, const MapMode* pMapModeBaseline,
                                    const MapMode* pMapModeSource, const MapMode* pMapModeDest,
                                    vcl::MappingPolicy ePolicy) const
{
    const MapMode* pSrc = pMapModeSource ? pMapModeSource : pMapModeBaseline;
    const MapMode* pDst = pMapModeDest ? pMapModeDest : pMapModeBaseline;

    if (!pSrc || !pDst || *pSrc == *pDst)
        return rSzSource;

    MappingCoefficients aMapResSource = ResolveMapResRelative(pMapModeBaseline, pSrc, ePolicy);
    MappingCoefficients aMapResDest = ResolveMapResRelative(pMapModeBaseline, pDst, ePolicy);

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
                                                const MapMode* pMapModeDest,
                                                vcl::MappingPolicy ePolicy) const
{
    const MapMode* pSrc = pMapModeSource ? pMapModeSource : pMapModeBaseline;
    const MapMode* pDst = pMapModeDest ? pMapModeDest : pMapModeBaseline;

    if (!pSrc || !pDst || *pSrc == *pDst)
        return rRectSource;

    MappingCoefficients aMapResSource = ResolveMapResRelative(pMapModeBaseline, pSrc, ePolicy);
    MappingCoefficients aMapResDest = ResolveMapResRelative(pMapModeBaseline, pDst, ePolicy);

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
    if (rMapModeSource == rMapModeDest)
        return basegfx::B2DHomMatrix(); // Returns pure Identity matrix

    MapUnit eUnitSource = rMapModeSource.GetMapUnit();
    MapUnit eUnitDest = rMapModeDest.GetMapUnit();
    lcl_verifyUnitSourceDest(eUnitSource, eUnitDest);

    // Path 1: Simple MapModes (Pure Scaling, No Offsets)
    if (rMapModeSource.IsSimple() && rMapModeDest.IsSimple())
    {
        const auto[eFrom, eTo] = lcl_getCorrectedUnit(eUnitSource, eUnitDest);
        if (eFrom == o3tl::Length::invalid || eTo == o3tl::Length::invalid)
        {
            SAL_WARN("vcl.gdi", "CoordinateMapper: Invalid MapUnit conversion requested. Falling "
                                "back to identity matrix.");
            return basegfx::B2DHomMatrix();
        }

        const double fScaleFactor = o3tl::convert(1.0, eFrom, eTo);

        // Simple scaling uses no offsets
        return lcl_BuildAffineMatrix(fScaleFactor, fScaleFactor, 0.0, 0.0, 0.0, 0.0);
    }

    // Path 2: Complex MapModes (Scaling + Origin Offsets)
    const auto[aMapResSource, aMapResDest] = lcl_calcConversionMapRes(rMapModeSource, rMapModeDest);

    const double fDestScX = (aMapResDest.mfScaleX != 0.0) ? aMapResDest.mfScaleX : 1.0;
    const double fDestScY = (aMapResDest.mfScaleY != 0.0) ? aMapResDest.mfScaleY : 1.0;

    const double fScaleFactorX = aMapResSource.mfScaleX / fDestScX;
    const double fScaleFactorY = aMapResSource.mfScaleY / fDestScY;

    // By passing the source as the "Logical" offset and the negative dest as the "Physical" offset,
    // the canonical builder perfectly scales the source offset before subtracting the dest offset.
    return lcl_BuildAffineMatrix(fScaleFactorX, fScaleFactorY,
                                 static_cast<double>(aMapResSource.mnTranslationX),
                                 static_cast<double>(aMapResSource.mnTranslationY),
                                 static_cast<double>(-aMapResDest.mnTranslationX),
                                 static_cast<double>(-aMapResDest.mnTranslationY));
}

bool CompiledTransform::CheckRectilinearContract() const
{
    const bool bSafe = PreservesAxisAlignment();

    // Developer Stop
    assert(bSafe
           && "CoordinateMapper Contract Violation: Rectilinear API requires Axis Alignment!");

    // Production Audit
    SAL_WARN_IF(
        !bSafe, "vcl.gdi",
        "CoordinateMapper: Scalar/Rect extraction on non-aligned transform - fallback used.");

    return bSafe;
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
