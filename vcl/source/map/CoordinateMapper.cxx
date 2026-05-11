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

#include "CoordinateMath.hxx"

#include <cmath>
#include <cassert>
#include <ranges>

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

    rScaleX = vcl::detail::GetBasisVectorMagnitudeX(aMat);
    rScaleY = vcl::detail::GetBasisVectorMagnitudeY(aMat);

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
    const double sx = vcl::detail::GetBasisVectorMagnitudeX(aMat);
    const double sy = vcl::detail::GetBasisVectorMagnitudeY(aMat);
    return Size(vcl::detail::RoundToLong(rLogicSize.Width() * sx),
                vcl::detail::RoundToLong(rLogicSize.Height() * sy));
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
    DBG_TESTSOLARMUTEX();

    const bool bMapped = (ePolicy == vcl::MappingPolicy::ApplyMapMode);

    // Explicitly resolve the keys for this specific compilation pass
    const TransformKey eL2W
        = bMapped ? TransformKey::LogicToWindow_Mapped : TransformKey::LogicToWindow_Unmapped;
    const TransformKey eW2L
        = bMapped ? TransformKey::WindowToLogic_Mapped : TransformKey::WindowToLogic_Unmapped;
    const TransformKey eL2D
        = bMapped ? TransformKey::LogicToDevice_Mapped : TransformKey::LogicToDevice_Unmapped;
    const TransformKey eD2L
        = bMapped ? TransformKey::DeviceToLogic_Mapped : TransformKey::DeviceToLogic_Unmapped;

    // Phase 1: Logic -> Window (The core mapping)
    basegfx::B2DHomMatrix aLogicToWindow;
    if (bMapped)
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

    // Store Phase 1
    maTransformCache[static_cast<size_t>(eL2W)] = BuildCompiledTransform(aLogicToWindow);

    // Phase 2: Window -> Logic (The Inverse)
    if (aLogicToWindow.isInvertible())
    {
        basegfx::B2DHomMatrix aWindowToLogic = aLogicToWindow;
        aWindowToLogic.invert();
        maTransformCache[static_cast<size_t>(eW2L)] = BuildCompiledTransform(aWindowToLogic);
    }
    else
    {
        SAL_WARN("vcl.gdi", "CoordinateMapper: Singular Matrix. Falling back to Identity.");
        maTransformCache[static_cast<size_t>(eW2L)]
            = BuildCompiledTransform(basegfx::B2DHomMatrix());
    }

    // Phase 3: Logic -> Device (Applying physical OS offsets)
    basegfx::B2DHomMatrix aLogicToDevice = aLogicToWindow;
    aLogicToDevice.translate(static_cast<double>(mnDeviceToWindowOffsetX),
                             static_cast<double>(mnDeviceToWindowOffsetY));

    // Store Phase 3
    maTransformCache[static_cast<size_t>(eL2D)] = BuildCompiledTransform(aLogicToDevice);

    // Phase 4: Device -> Logic (The Inverse)
    if (aLogicToDevice.isInvertible())
    {
        basegfx::B2DHomMatrix aDeviceToLogic = aLogicToDevice;
        aDeviceToLogic.invert();
        maTransformCache[static_cast<size_t>(eD2L)] = BuildCompiledTransform(aDeviceToLogic);
    }
    else
    {
        maTransformCache[static_cast<size_t>(eD2L)]
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

TransformKey CoordinateMapper::ResolveKey(const TransformRequest& rReq) const
{
    const bool bMapped = (rReq.Policy == vcl::MappingPolicy::ApplyMapMode);

    if (rReq.eFrom == CoordinateSpace::Logic && rReq.eTo == CoordinateSpace::Window)
        return bMapped ? TransformKey::LogicToWindow_Mapped : TransformKey::LogicToWindow_Unmapped;

    if (rReq.eFrom == CoordinateSpace::Window && rReq.eTo == CoordinateSpace::Logic)
        return bMapped ? TransformKey::WindowToLogic_Mapped : TransformKey::WindowToLogic_Unmapped;

    if (rReq.eFrom == CoordinateSpace::Logic && rReq.eTo == CoordinateSpace::Device)
        return bMapped ? TransformKey::LogicToDevice_Mapped : TransformKey::LogicToDevice_Unmapped;

    if (rReq.eFrom == CoordinateSpace::Device && rReq.eTo == CoordinateSpace::Logic)
        return bMapped ? TransformKey::DeviceToLogic_Mapped : TransformKey::DeviceToLogic_Unmapped;

    assert(false && "Unsupported TransformRequest routing");
    return TransformKey::LogicToWindow_Unmapped; // Safe fallback
}

const CompiledTransform& CoordinateMapper::Compile(const TransformRequest& rReq) const
{
    // O(1) Cache Version Validation
    uint64_t nCurrentVersion = mnStateVersion.load(std::memory_order_acquire);
    if (mnCacheVersion != nCurrentVersion)
    {
        for (auto& slot : maTransformCache)
        {
            slot.reset();
        }

        mnCacheVersion = nCurrentVersion;
    }

    // Resolve the semantic route to a physical cache key
    TransformKey eKey = ResolveKey(rReq);

    // Cache Miss: Compile the graph for this policy
    if (!maTransformCache[static_cast<size_t>(eKey)])
        UpdateCache(rReq.Policy);

    // Return immutable execution artifact
    return *maTransformCache[static_cast<size_t>(eKey)];
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
        return vcl::detail::RoundToLong(n);
    return vcl::detail::RoundToLong(n / (fScale * static_cast<double>(GetDPIX())));
}

tools::Long CoordinateMapper::ViewSubPixelToLogicDistanceY(double n, double fScale) const
{
    if (fScale == 0.0 || GetDPIY() <= 0)
        return vcl::detail::RoundToLong(n);
    return vcl::detail::RoundToLong(n / (fScale * static_cast<double>(GetDPIY())));
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
        return vcl::detail::RoundToLong(
            static_cast<double>(nWidth)
            * vcl::detail::GetBasisVectorMagnitudeX(rTransform.GetMatrix()));

    return vcl::detail::RoundToLong(static_cast<double>(nWidth) * rTransform.GetMatrix().get(0, 0));
}

tools::Long CoordinateMapper::LogicHeightToDevicePixel(tools::Long nHeight,
                                                       vcl::MappingPolicy ePolicy) const
{
    const auto& rTransform
        = Compile(TransformRequest{ CoordinateSpace::Logic, CoordinateSpace::Device, ePolicy });

    if (!rTransform.CheckRectilinearContract())
        return vcl::detail::RoundToLong(
            static_cast<double>(nHeight)
            * vcl::detail::GetBasisVectorMagnitudeY(rTransform.GetMatrix()));

    return vcl::detail::RoundToLong(static_cast<double>(nHeight)
                                    * rTransform.GetMatrix().get(1, 1));
}

tools::Long CoordinateMapper::DevicePixelToLogicWidth(tools::Long nWidth,
                                                      vcl::MappingPolicy ePolicy) const
{
    const auto& rTransform
        = Compile(TransformRequest{ CoordinateSpace::Device, CoordinateSpace::Logic, ePolicy });

    if (!rTransform.CheckRectilinearContract())
        return vcl::detail::RoundToLong(
            static_cast<double>(nWidth)
            * vcl::detail::GetBasisVectorMagnitudeX(rTransform.GetMatrix()));

    return vcl::detail::RoundToLong(static_cast<double>(nWidth) * rTransform.GetMatrix().get(0, 0));
}

tools::Long CoordinateMapper::DevicePixelToLogicHeight(tools::Long nHeight,
                                                       vcl::MappingPolicy ePolicy) const
{
    const auto& rTransform
        = Compile(TransformRequest{ CoordinateSpace::Device, CoordinateSpace::Logic, ePolicy });

    if (!rTransform.CheckRectilinearContract())
        return vcl::detail::RoundToLong(
            static_cast<double>(nHeight)
            * vcl::detail::GetBasisVectorMagnitudeY(rTransform.GetMatrix()));

    return vcl::detail::RoundToLong(static_cast<double>(nHeight)
                                    * rTransform.GetMatrix().get(1, 1));
}

double CoordinateMapper::LogicWidthToWindowSubPixel(tools::Long nWidth,
                                                    vcl::MappingPolicy ePolicy) const
{
    const auto& rTransform
        = Compile(TransformRequest{ CoordinateSpace::Logic, CoordinateSpace::Window, ePolicy });

    if (!rTransform.CheckRectilinearContract())
        return static_cast<double>(nWidth)
               * vcl::detail::GetBasisVectorMagnitudeX(rTransform.GetMatrix());

    return static_cast<double>(nWidth) * rTransform.GetMatrix().get(0, 0);
}

double CoordinateMapper::LogicHeightToWindowSubPixel(tools::Long nHeight,
                                                     vcl::MappingPolicy ePolicy) const
{
    const auto& rTransform
        = Compile(TransformRequest{ CoordinateSpace::Logic, CoordinateSpace::Window, ePolicy });

    if (!rTransform.CheckRectilinearContract())
        return static_cast<double>(nHeight)
               * vcl::detail::GetBasisVectorMagnitudeY(rTransform.GetMatrix());

    return static_cast<double>(nHeight) * rTransform.GetMatrix().get(1, 1);
}

tools::Long CoordinateMapper::LogicToWindowX(tools::Long nX, vcl::MappingPolicy ePolicy) const
{
    const auto& rTransform
        = Compile(TransformRequest{ CoordinateSpace::Logic, CoordinateSpace::Window, ePolicy });
    const auto& rMat = rTransform.GetMatrix();

    if (!rTransform.PreservesAxisAlignment())
        return vcl::detail::RoundToLong(LogicToWindowSubPixelX(static_cast<double>(nX), ePolicy));

    return vcl::detail::RoundToLong(static_cast<double>(nX) * rMat.get(0, 0) + rMat.get(0, 2));
}

tools::Long CoordinateMapper::LogicToWindowY(tools::Long nY, vcl::MappingPolicy ePolicy) const
{
    const auto& rTransform
        = Compile(TransformRequest{ CoordinateSpace::Logic, CoordinateSpace::Window, ePolicy });
    const auto& rMat = rTransform.GetMatrix();

    if (!rTransform.PreservesAxisAlignment())
        return vcl::detail::RoundToLong(LogicToWindowSubPixelY(static_cast<double>(nY), ePolicy));

    return vcl::detail::RoundToLong(static_cast<double>(nY) * rMat.get(1, 1) + rMat.get(1, 2));
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
    return Point(vcl::detail::RoundToLong(aPt.getX()), vcl::detail::RoundToLong(aPt.getY()));
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
    return Point(vcl::detail::RoundToLong(aPt.getX()), vcl::detail::RoundToLong(aPt.getY()));
}

Size CoordinateMapper::LogicToWindowUnits(const Size& rLogicSize,
                                          const vcl::detail::MapConversion& rConv) const
{
    auto mat = GetViewTransformation(rConv);
    return Size(vcl::detail::RoundToLong(rLogicSize.Width() * std::abs(mat.get(0, 0))),
                vcl::detail::RoundToLong(rLogicSize.Height() * std::abs(mat.get(1, 1))));
}

tools::Rectangle CoordinateMapper::LogicToWindowUnits(const tools::Rectangle& rRect,
                                                      const vcl::detail::MapConversion& rConv) const
{
    basegfx::B2DHomMatrix aMat = GetViewTransformation(rConv);

    // ADAPTER: VCL [Left, Right] -> Math [Min, Max)
    basegfx::B2DRange aRange(rRect.Left(), rRect.Top(), rRect.Right() + 1, rRect.Bottom() + 1);

    aRange.transform(aMat);

    // ADAPTER: Math [Min, Max) -> VCL [Left, Right]
    tools::Rectangle aRetval(vcl::detail::RoundToLong(aRange.getMinX()),
                             vcl::detail::RoundToLong(aRange.getMinY()),
                             vcl::detail::RoundToLong(aRange.getMaxX()) - 1,
                             vcl::detail::RoundToLong(aRange.getMaxY()) - 1);

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
        rPoint = Point(vcl::detail::RoundToLong(aPt.getX()), vcl::detail::RoundToLong(aPt.getY()));
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
    return Point(vcl::detail::RoundToLong(aPt.getX()), vcl::detail::RoundToLong(aPt.getY()));
}

Size CoordinateMapper::WindowToLogicUnits(const Size& rWindowSize,
                                          const vcl::detail::MapConversion& rConv) const
{
    auto mat = GetInverseViewTransformation(rConv);
    return Size(vcl::detail::RoundToLong(rWindowSize.Width() * std::abs(mat.get(0, 0))),
                vcl::detail::RoundToLong(rWindowSize.Height() * std::abs(mat.get(1, 1))));
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
    tools::Rectangle aRetval(vcl::detail::RoundToLong(aRange.getMinX()),
                             vcl::detail::RoundToLong(aRange.getMinY()),
                             vcl::detail::RoundToLong(aRange.getMaxX()) - 1,
                             vcl::detail::RoundToLong(aRange.getMaxY()) - 1);

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

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
