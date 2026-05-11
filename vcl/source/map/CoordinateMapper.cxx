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
#include <TransformCompiler.hxx>
#include <MappingCoefficients.hxx>

#include "CoordinateMath.hxx"

#include <cmath>
#include <cassert>
#include <ranges>

/**
 * COORDINATE TRANSFORMATION STRATEGY
 *
 * All transformations in this mapper are orchestrated via vcl::BuildAffineMatrix.
 * We enforce a single-source-of-truth mathematical contract where Logic-to-Device
 * projections are composed as:
 *
 * P' = ((P + LogicOffset) * Scale) + PhysicalOffset
 *
 * This specific ordering is critical for maintaining parity with legacy VCL
 * behavior while benefiting from modern basegfx::B2DHomMatrix performance.
 * For the full algebraic derivation, see CoordinateMath.hxx.
 */

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

    return vcl::BuildAffineMatrix(fScaleFactorX, fScaleFactorY,
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

    const TransformKey eL2W
        = bMapped ? TransformKey::LogicToWindow_Mapped : TransformKey::LogicToWindow_Unmapped;
    const TransformKey eW2L
        = bMapped ? TransformKey::WindowToLogic_Mapped : TransformKey::WindowToLogic_Unmapped;
    const TransformKey eL2D
        = bMapped ? TransformKey::LogicToDevice_Mapped : TransformKey::LogicToDevice_Unmapped;
    const TransformKey eD2L
        = bMapped ? TransformKey::DeviceToLogic_Mapped : TransformKey::DeviceToLogic_Unmapped;

    // Phase 1: Logic -> Window
    basegfx::B2DHomMatrix aLogicToWindow;
    if (bMapped)
    {
        const double fScaleX
            = maMapRes.mfScaleX * static_cast<double>(mnDPIX) * GetDPIScaleFactor();
        const double fScaleY
            = maMapRes.mfScaleY * static_cast<double>(mnDPIY) * GetDPIScaleFactor();

        aLogicToWindow = vcl::BuildAffineMatrix(
            fScaleX, fScaleY,
            static_cast<double>(maMapRes.mnTranslationX + mnLogicToAbsoluteOffsetX),
            static_cast<double>(maMapRes.mnTranslationY + mnLogicToAbsoluteOffsetY),
            static_cast<double>(mnWindowToViewOffsetX), static_cast<double>(mnWindowToViewOffsetY));
    }
    else
    {
        aLogicToWindow.translate(static_cast<double>(mnWindowToViewOffsetX),
                                 static_cast<double>(mnWindowToViewOffsetY));
    }

    maCache.Store(eL2W, vcl::TransformCompiler::Compile(aLogicToWindow));

    // Phase 2: Window -> Logic
    if (aLogicToWindow.isInvertible())
    {
        basegfx::B2DHomMatrix aWindowToLogic = aLogicToWindow;
        aWindowToLogic.invert();
        maCache.Store(eW2L, vcl::TransformCompiler::Compile(aWindowToLogic));
    }
    else
    {
        SAL_WARN("vcl.gdi", "CoordinateMapper: Singular Matrix. Falling back to Identity.");
        maCache.Store(eW2L, vcl::TransformCompiler::Compile(basegfx::B2DHomMatrix()));
    }

    // Phase 3: Logic -> Device
    basegfx::B2DHomMatrix aLogicToDevice = aLogicToWindow;
    aLogicToDevice.translate(static_cast<double>(mnDeviceToWindowOffsetX),
                             static_cast<double>(mnDeviceToWindowOffsetY));

    maCache.Store(eL2D, vcl::TransformCompiler::Compile(aLogicToDevice));

    // Phase 4: Device -> Logic
    if (aLogicToDevice.isInvertible())
    {
        basegfx::B2DHomMatrix aDeviceToLogic = aLogicToDevice;
        aDeviceToLogic.invert();
        maCache.Store(eD2L, vcl::TransformCompiler::Compile(aDeviceToLogic));
    }
    else
    {
        maCache.Store(eD2L, vcl::TransformCompiler::Compile(basegfx::B2DHomMatrix()));
    }
}

TransformKey CoordinateMapper::ResolveKey(const TransformRequest& rReq) const
{
    const bool bMapped = (rReq.Policy == vcl::MappingPolicy::ApplyMapMode);

    // Logic <-> Window
    if (rReq.eFrom == CoordinateSpace::Logic && rReq.eTo == CoordinateSpace::Window)
        return bMapped ? TransformKey::LogicToWindow_Mapped : TransformKey::LogicToWindow_Unmapped;

    if (rReq.eFrom == CoordinateSpace::Window && rReq.eTo == CoordinateSpace::Logic)
        return bMapped ? TransformKey::WindowToLogic_Mapped : TransformKey::WindowToLogic_Unmapped;

    // Logic <-> Device
    if (rReq.eFrom == CoordinateSpace::Logic && rReq.eTo == CoordinateSpace::Device)
        return bMapped ? TransformKey::LogicToDevice_Mapped : TransformKey::LogicToDevice_Unmapped;

    if (rReq.eFrom == CoordinateSpace::Device && rReq.eTo == CoordinateSpace::Logic)
        return bMapped ? TransformKey::DeviceToLogic_Mapped : TransformKey::DeviceToLogic_Unmapped;

    // Device <-> Window (Direct viewport offsets, rarely used but supported)
    if (rReq.eFrom == CoordinateSpace::Device && rReq.eTo == CoordinateSpace::Window)
        return TransformKey::DeviceToWindow;

    if (rReq.eFrom == CoordinateSpace::Window && rReq.eTo == CoordinateSpace::Device)
        return TransformKey::WindowToDevice;

    // CRITICAL: If we reach here, a caller has requested a coordinate transition
    // that the engine does not formally support or hasn't indexed.
    // We abort here because returning a "default" key would result in
    // silent coordinate corruption across the rendering pipeline.
    SAL_WARN("vcl.gdi", "Unsupported TransformRequest routing: " << static_cast<int>(rReq.eFrom)
                                                                 << " to "
                                                                 << static_cast<int>(rReq.eTo));

    std::abort();
}

const CompiledTransform& CoordinateMapper::Compile(const TransformRequest& rReq) const
{
    // Resolve the semantic route to a physical cache key
    TransformKey eKey = ResolveKey(rReq);

    // Check Memory (Handles version validation internally)
    if (const CompiledTransform* pCached = maCache.Get(eKey))
        return *pCached;

    // Cache Miss: Compile the graph for this policy
    UpdateCache(rReq.Policy);

    // Return immutable execution artifact
    return *maCache.Get(eKey);
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

    // Use the cached invariant flag instead of re-calculating alignment
    DBG_ASSERT(rTransform.PreservesAxisAlignment(),
               "LogicToWindowSubPixelX requires axis-aligned transform");

    // P' = P * M00 + M02 (The horizontal scale + translation)
    return fX * rMat.get(0, 0) + rMat.get(0, 2);
}

double CoordinateMapper::LogicToWindowSubPixelY(double fY, vcl::MappingPolicy ePolicy) const
{
    const auto& rTransform = Compile({ CoordinateSpace::Logic, CoordinateSpace::Window, ePolicy });
    const auto& rMat = rTransform.GetMatrix();

    // Use the cached invariant flag
    DBG_ASSERT(rTransform.PreservesAxisAlignment(),
               "LogicToWindowSubPixelY requires axis-aligned transform");

    // P' = P * M11 + M12 (The vertical scale + translation)
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
    const auto& rTransform = Compile({ CoordinateSpace::Logic, CoordinateSpace::Window, ePolicy });
    const auto& rMat = rTransform.GetMatrix();

    // If the transform is complex (rotation/shear), we must use the full sub-pixel
    // path, but we do the math here to avoid a redundant Compile() call.
    if (!rTransform.PreservesAxisAlignment())
    {
        basegfx::B2DPoint aPt(static_cast<double>(nX), 0.0);
        aPt *= rMat;
        return vcl::detail::RoundToLong(aPt.getX());
    }

    // Fast path: Pure rectilinear math
    return vcl::detail::RoundToLong(static_cast<double>(nX) * rMat.get(0, 0) + rMat.get(0, 2));
}

tools::Long CoordinateMapper::LogicToWindowY(tools::Long nY, vcl::MappingPolicy ePolicy) const
{
    const auto& rTransform = Compile({ CoordinateSpace::Logic, CoordinateSpace::Window, ePolicy });
    const auto& rMat = rTransform.GetMatrix();

    if (!rTransform.PreservesAxisAlignment())
    {
        basegfx::B2DPoint aPt(0.0, static_cast<double>(nY));
        aPt *= rMat;
        return vcl::detail::RoundToLong(aPt.getY());
    }

    // Fast path: Pure rectilinear math
    return vcl::detail::RoundToLong(static_cast<double>(nY) * rMat.get(1, 1) + rMat.get(1, 2));
}

double CoordinateMapper::LogicWidthToDeviceSubPixel(tools::Long nWidth,
                                                    vcl::MappingPolicy ePolicy) const
{
    // Acquire the compiled execution plan for this specific route
    const auto& rTransform = Compile({ CoordinateSpace::Logic, CoordinateSpace::Device, ePolicy });
    const auto& rMat = rTransform.GetMatrix();

    // Fast Path: Rectilinear (No rotation/shear)
    // If the transform is axis-aligned, the width scale is simply M00.
    if (rTransform.PreservesAxisAlignment())
    {
        return static_cast<double>(nWidth) * rMat.get(0, 0);
    }

    // Fallback: Complex Affine (Rotation/Shear)
    // If the coordinate system is rotated, 'Width' is no longer a simple scalar along X.
    // We must measure the magnitude of the transformed X-basis vector.
    // This uses the Pythagorean helper from CoordinateMath: sqrt(M00^2 + M01^2)
    return static_cast<double>(nWidth) * vcl::detail::GetBasisVectorMagnitudeX(rMat);
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

    vcl::ApplyEmptyState(aRetval, rRect);
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

    vcl::ApplyEmptyState(aRetval, rWindowRect);
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
