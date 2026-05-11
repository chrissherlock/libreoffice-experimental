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
#include <GeometryAdapter.hxx>
#include <TransformRouter.hxx>
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

sal_Int32 CoordinateMapper::GetDPIX() const { return maState.GetDPIX(); }

sal_Int32 CoordinateMapper::GetDPIY() const { return maState.GetDPIY(); }

void CoordinateMapper::SetDPIX(sal_Int32 nDPIX)
{
    maState.SetDPIX(nDPIX);
    InvalidateViewTransform();
}

void CoordinateMapper::SetDPIY(sal_Int32 nDPIY)
{
    maState.SetDPIY(nDPIY);
    InvalidateViewTransform();
}

sal_Int32 CoordinateMapper::GetDPIScalePercentage() const
{
    return maState.GetDPIScalePercentage();
}

void CoordinateMapper::SetDPIScalePercentage(sal_Int32 nPercent)
{
    maState.SetDPIScalePercentage(nPercent);
    InvalidateViewTransform();
}

float CoordinateMapper::GetDPIScaleFactor() const
{
    return maState.GetDPIScalePercentage() / 100.0f;
}

void CoordinateMapper::SetPixelOffset(const Size& rSize)
{
    maState.SetLogicToAbsoluteOffset(rSize);
    InvalidateViewTransform();
}

void CoordinateMapper::SetWindowToViewOffset(const Size& rWindowPixelOffset)
{
    maState.SetWindowToViewOffset(rWindowPixelOffset);
    InvalidateViewTransform();
}

tools::Long CoordinateMapper::GetDeviceToWindowOffsetX() const
{
    return maState.GetDeviceToWindowOffsetX();
}

tools::Long CoordinateMapper::GetDeviceToWindowOffsetY() const
{
    return maState.GetDeviceToWindowOffsetY();
}

void CoordinateMapper::SetDeviceToWindowOffsetX(tools::Long nDeviceToWindowOffsetX)
{
    maState.SetDeviceToWindowOffset(nDeviceToWindowOffsetX, maState.GetDeviceToWindowOffsetY());
    InvalidateViewTransform();
}

void CoordinateMapper::SetDeviceToWindowOffsetY(tools::Long nDeviceToWindowOffsetY)
{
    maState.SetDeviceToWindowOffset(maState.GetDeviceToWindowOffsetX(), nDeviceToWindowOffsetY);
    InvalidateViewTransform();
}

Point CoordinateMapper::GetDeviceToWindowOffset() const
{
    return Point(maState.GetDeviceToWindowOffsetX(), maState.GetDeviceToWindowOffsetY());
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
    maState.SetLogicToAbsoluteOffset(rOffset);
    InvalidateViewTransform();
}

void CoordinateMapper::CalcMapResolution(const MapMode& rMapMode, tools::Long nDPIX,
                                         tools::Long nDPIY)
{
    maState.SetDPIX(nDPIX);
    maState.SetDPIY(nDPIY);
    maState.UpdateFromMapMode(rMapMode);
    InvalidateViewTransform();
}

MappingCoefficients CoordinateMapper::ResolveMapResRelative(const MapMode* pBaseline,
                                                            const MapMode* pTarget,
                                                            vcl::MappingPolicy ePolicy) const
{
    return maState.ResolveMapResRelative(pTarget, pBaseline, ePolicy);
}

vcl::detail::MapConversion CoordinateMapper::ResolveMap(const MapMode& rBaseline,
                                                        const MapMode& rTarget,
                                                        vcl::MappingPolicy ePolicy) const
{
    // Evaluates a temporary MapMode against the current accumulated state
    MappingCoefficients aRes = maState.ResolveMapResRelative(&rTarget, &rBaseline, ePolicy);
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

    return vcl::BuildAffineMatrix(
        fScaleFactorX, fScaleFactorY,
        static_cast<double>(rConv.mnOffsetX + maState.GetLogicToAbsoluteOffsetX()),
        static_cast<double>(rConv.mnOffsetY + maState.GetLogicToAbsoluteOffsetY()),
        static_cast<double>(maState.GetWindowToViewOffsetX()),
        static_cast<double>(maState.GetWindowToViewOffsetY()));
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
// THE ROUTER DELEGATION
// ============================================================================

const vcl::TransformPlan& CoordinateMapper::Compile(const TransformRequest& rReq) const
{
    return maRouter.Compile(maState, rReq);
}

// ============================================================================
// SINGLE SOURCE OF TRUTH: MATRIX BUILDERS (NOW ROUTES TO COMPILE)
// ============================================================================

basegfx::B2DHomMatrix CoordinateMapper::GetLogicToDeviceMatrix(vcl::MappingPolicy ePolicy) const
{
    return Compile({ CoordinateSpace::Logic, CoordinateSpace::Device, ePolicy }).maMatrix;
}

basegfx::B2DHomMatrix CoordinateMapper::GetDeviceToLogicMatrix(vcl::MappingPolicy ePolicy) const
{
    return Compile({ CoordinateSpace::Device, CoordinateSpace::Logic, ePolicy }).maMatrix;
}

basegfx::B2DHomMatrix CoordinateMapper::GetLogicToWindowMatrix(vcl::MappingPolicy ePolicy) const
{
    return Compile({ CoordinateSpace::Logic, CoordinateSpace::Window, ePolicy }).maMatrix;
}

basegfx::B2DHomMatrix CoordinateMapper::GetWindowToLogicMatrix(vcl::MappingPolicy ePolicy) const
{
    return Compile({ CoordinateSpace::Window, CoordinateSpace::Logic, ePolicy }).maMatrix;
}

// ========================================================================
// PIPELINE STAGES (Coordinate Transitions)
// ========================================================================

double CoordinateMapper::DeviceToWindowSubPixelX(double fX) const
{
    return fX - static_cast<double>(maState.GetDeviceToWindowOffsetX());
}

double CoordinateMapper::DeviceToWindowSubPixelY(double fY) const
{
    return fY - static_cast<double>(maState.GetDeviceToWindowOffsetY());
}

double CoordinateMapper::WindowToDeviceSubPixelX(double fX) const
{
    return fX + static_cast<double>(maState.GetDeviceToWindowOffsetX());
}

double CoordinateMapper::WindowToDeviceSubPixelY(double fY) const
{
    return fY + static_cast<double>(maState.GetDeviceToWindowOffsetY());
}

tools::Long CoordinateMapper::ViewToWindowUnitsX(tools::Long nX) const
{
    return nX + maState.GetWindowToViewOffsetX();
}

tools::Long CoordinateMapper::ViewToWindowUnitsY(tools::Long nY) const
{
    return nY + maState.GetWindowToViewOffsetY();
}

tools::Long CoordinateMapper::WindowToViewUnitsX(tools::Long nX) const
{
    return nX - maState.GetWindowToViewOffsetX();
}

tools::Long CoordinateMapper::WindowToViewUnitsY(tools::Long nY) const
{
    return nY - maState.GetWindowToViewOffsetY();
}

tools::Long CoordinateMapper::DeviceToWindowUnitsX(tools::Long nX) const
{
    return nX - maState.GetDeviceToWindowOffsetX();
}

tools::Long CoordinateMapper::DeviceToWindowUnitsY(tools::Long nY) const
{
    return nY - maState.GetDeviceToWindowOffsetY();
}

tools::Long CoordinateMapper::WindowToDeviceUnitsX(tools::Long nX) const
{
    return nX + maState.GetDeviceToWindowOffsetX();
}

tools::Long CoordinateMapper::WindowToDeviceUnitsY(tools::Long nY) const
{
    return nY + maState.GetDeviceToWindowOffsetY();
}

tools::Long CoordinateMapper::ViewSubPixelToLogicDistanceX(double n) const
{
    return ViewSubPixelToLogicDistanceX(n, maState.GetMapConversion().mfScaleX);
}

tools::Long CoordinateMapper::ViewSubPixelToLogicDistanceY(double n) const
{
    return ViewSubPixelToLogicDistanceY(n, maState.GetMapConversion().mfScaleY);
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
    return fX - static_cast<double>(maState.GetWindowToViewOffsetX());
}

double CoordinateMapper::WindowToViewSubPixelY(double fY) const
{
    return fY - static_cast<double>(maState.GetWindowToViewOffsetY());
}

double CoordinateMapper::ViewToWindowSubPixelX(double fX) const
{
    return fX + static_cast<double>(maState.GetWindowToViewOffsetX());
}

double CoordinateMapper::ViewToWindowSubPixelY(double fY) const
{
    return fY + static_cast<double>(maState.GetWindowToViewOffsetY());
}

// ========================================================================
// PUBLIC WRAPPERS (Routing into the unified pipeline)
// ========================================================================

// Logic -> Device
Point CoordinateMapper::LogicToDevicePixel(const Point& rPt, vcl::MappingPolicy eP) const
{
    return vcl::GeometryAdapter::Apply(
        Compile({ CoordinateSpace::Logic, CoordinateSpace::Device, eP }), rPt);
}
Size CoordinateMapper::LogicToDevicePixel(const Size& rSz, vcl::MappingPolicy eP) const
{
    return vcl::GeometryAdapter::Apply(
        Compile({ CoordinateSpace::Logic, CoordinateSpace::Device, eP }), rSz);
}
tools::Rectangle CoordinateMapper::LogicToDevicePixel(const tools::Rectangle& rRect,
                                                      vcl::MappingPolicy eP) const
{
    return vcl::GeometryAdapter::Apply(
        Compile({ CoordinateSpace::Logic, CoordinateSpace::Device, eP }), rRect);
}
tools::Polygon CoordinateMapper::LogicToDevicePixel(const tools::Polygon& rPoly,
                                                    vcl::MappingPolicy eP) const
{
    return vcl::GeometryAdapter::Apply(
        Compile({ CoordinateSpace::Logic, CoordinateSpace::Device, eP }), rPoly);
}
tools::PolyPolygon CoordinateMapper::LogicToDevicePixel(const tools::PolyPolygon& rPoly,
                                                        vcl::MappingPolicy eP) const
{
    return vcl::GeometryAdapter::Apply(
        Compile({ CoordinateSpace::Logic, CoordinateSpace::Device, eP }), rPoly);
}
vcl::Region CoordinateMapper::LogicToDevicePixel(const vcl::Region& rReg,
                                                 vcl::MappingPolicy eP) const
{
    return vcl::GeometryAdapter::Apply(
        Compile({ CoordinateSpace::Logic, CoordinateSpace::Device, eP }), rReg);
}
LineInfo CoordinateMapper::LogicToDevicePixel(const LineInfo& rLine, vcl::MappingPolicy eP) const
{
    return vcl::GeometryAdapter::Apply(
        Compile({ CoordinateSpace::Logic, CoordinateSpace::Device, eP }), rLine);
}
basegfx::B2DPolygon CoordinateMapper::LogicToDevicePixel(const basegfx::B2DPolygon& rPoly,
                                                         vcl::MappingPolicy eP) const
{
    return vcl::GeometryAdapter::Apply(
        Compile({ CoordinateSpace::Logic, CoordinateSpace::Device, eP }), rPoly);
}
basegfx::B2DPolyPolygon CoordinateMapper::LogicToDevicePixel(const basegfx::B2DPolyPolygon& rPoly,
                                                             vcl::MappingPolicy eP) const
{
    return vcl::GeometryAdapter::Apply(
        Compile({ CoordinateSpace::Logic, CoordinateSpace::Device, eP }), rPoly);
}

// Device -> Logic
Point CoordinateMapper::DevicePixelToLogic(const Point& rPt, vcl::MappingPolicy eP) const
{
    return vcl::GeometryAdapter::Apply(
        Compile({ CoordinateSpace::Device, CoordinateSpace::Logic, eP }), rPt);
}
Size CoordinateMapper::DevicePixelToLogic(const Size& rSz, vcl::MappingPolicy eP) const
{
    return vcl::GeometryAdapter::Apply(
        Compile({ CoordinateSpace::Device, CoordinateSpace::Logic, eP }), rSz);
}
tools::Rectangle CoordinateMapper::DevicePixelToLogic(const tools::Rectangle& rRect,
                                                      vcl::MappingPolicy eP) const
{
    return vcl::GeometryAdapter::Apply(
        Compile({ CoordinateSpace::Device, CoordinateSpace::Logic, eP }), rRect);
}
tools::Polygon CoordinateMapper::DevicePixelToLogic(const tools::Polygon& rPoly,
                                                    vcl::MappingPolicy eP) const
{
    return vcl::GeometryAdapter::Apply(
        Compile({ CoordinateSpace::Device, CoordinateSpace::Logic, eP }), rPoly);
}
tools::PolyPolygon CoordinateMapper::DevicePixelToLogic(const tools::PolyPolygon& rPoly,
                                                        vcl::MappingPolicy eP) const
{
    return vcl::GeometryAdapter::Apply(
        Compile({ CoordinateSpace::Device, CoordinateSpace::Logic, eP }), rPoly);
}
vcl::Region CoordinateMapper::DevicePixelToLogic(const vcl::Region& rReg,
                                                 vcl::MappingPolicy eP) const
{
    return vcl::GeometryAdapter::Apply(
        Compile({ CoordinateSpace::Device, CoordinateSpace::Logic, eP }), rReg);
}
basegfx::B2DPolygon CoordinateMapper::DevicePixelToLogic(const basegfx::B2DPolygon& rPoly,
                                                         vcl::MappingPolicy eP) const
{
    return vcl::GeometryAdapter::Apply(
        Compile({ CoordinateSpace::Device, CoordinateSpace::Logic, eP }), rPoly);
}
basegfx::B2DPolyPolygon CoordinateMapper::DevicePixelToLogic(const basegfx::B2DPolyPolygon& rPoly,
                                                             vcl::MappingPolicy eP) const
{
    return vcl::GeometryAdapter::Apply(
        Compile({ CoordinateSpace::Device, CoordinateSpace::Logic, eP }), rPoly);
}

// Logic -> Window
Point CoordinateMapper::LogicToWindowUnits(const Point& rPt, vcl::MappingPolicy eP) const
{
    return vcl::GeometryAdapter::Apply(
        Compile({ CoordinateSpace::Logic, CoordinateSpace::Window, eP }), rPt);
}
Size CoordinateMapper::LogicToWindowUnits(const Size& rSz, vcl::MappingPolicy eP) const
{
    return vcl::GeometryAdapter::Apply(
        Compile({ CoordinateSpace::Logic, CoordinateSpace::Window, eP }), rSz);
}
tools::Rectangle CoordinateMapper::LogicToWindowUnits(const tools::Rectangle& rRect,
                                                      vcl::MappingPolicy eP) const
{
    return vcl::GeometryAdapter::Apply(
        Compile({ CoordinateSpace::Logic, CoordinateSpace::Window, eP }), rRect);
}
tools::Polygon CoordinateMapper::LogicToWindowUnits(const tools::Polygon& rPoly,
                                                    vcl::MappingPolicy eP) const
{
    return vcl::GeometryAdapter::Apply(
        Compile({ CoordinateSpace::Logic, CoordinateSpace::Window, eP }), rPoly);
}
tools::PolyPolygon CoordinateMapper::LogicToWindowUnits(const tools::PolyPolygon& rPoly,
                                                        vcl::MappingPolicy eP) const
{
    return vcl::GeometryAdapter::Apply(
        Compile({ CoordinateSpace::Logic, CoordinateSpace::Window, eP }), rPoly);
}
vcl::Region CoordinateMapper::LogicToWindowUnits(const vcl::Region& rReg,
                                                 vcl::MappingPolicy eP) const
{
    return vcl::GeometryAdapter::Apply(
        Compile({ CoordinateSpace::Logic, CoordinateSpace::Window, eP }), rReg);
}

// Window -> Logic
Point CoordinateMapper::WindowToLogicUnits(const Point& rPt, vcl::MappingPolicy eP) const
{
    return vcl::GeometryAdapter::Apply(
        Compile({ CoordinateSpace::Window, CoordinateSpace::Logic, eP }), rPt);
}
Size CoordinateMapper::WindowToLogicUnits(const Size& rSz, vcl::MappingPolicy eP) const
{
    return vcl::GeometryAdapter::Apply(
        Compile({ CoordinateSpace::Window, CoordinateSpace::Logic, eP }), rSz);
}
tools::Rectangle CoordinateMapper::WindowToLogicUnits(const tools::Rectangle& rRect,
                                                      vcl::MappingPolicy eP) const
{
    return vcl::GeometryAdapter::Apply(
        Compile({ CoordinateSpace::Window, CoordinateSpace::Logic, eP }), rRect);
}
tools::Polygon CoordinateMapper::WindowToLogicUnits(const tools::Polygon& rPoly,
                                                    vcl::MappingPolicy eP) const
{
    return vcl::GeometryAdapter::Apply(
        Compile({ CoordinateSpace::Window, CoordinateSpace::Logic, eP }), rPoly);
}
tools::PolyPolygon CoordinateMapper::WindowToLogicUnits(const tools::PolyPolygon& rPoly,
                                                        vcl::MappingPolicy eP) const
{
    return vcl::GeometryAdapter::Apply(
        Compile({ CoordinateSpace::Window, CoordinateSpace::Logic, eP }), rPoly);
}
vcl::Region CoordinateMapper::WindowToLogicUnits(const vcl::Region& rReg,
                                                 vcl::MappingPolicy eP) const
{
    return vcl::GeometryAdapter::Apply(
        Compile({ CoordinateSpace::Window, CoordinateSpace::Logic, eP }), rReg);
}

// ========================================================================
// MapConversion Wrappers (Dynamic Compilation)
// ========================================================================
// NOTE: Instead of duplicating geometry logic, we compile a temporary
// TransformPlan from the MapConversion matrix and route it through the Adapter.

Point CoordinateMapper::LogicToWindowUnits(const Point& rPt,
                                           const vcl::detail::MapConversion& rConv) const
{
    return vcl::GeometryAdapter::Apply(
        vcl::TransformCompiler::Compile(GetViewTransformation(rConv)), rPt);
}
Size CoordinateMapper::LogicToWindowUnits(const Size& rSz,
                                          const vcl::detail::MapConversion& rConv) const
{
    return vcl::GeometryAdapter::Apply(
        vcl::TransformCompiler::Compile(GetViewTransformation(rConv)), rSz);
}
tools::Rectangle CoordinateMapper::LogicToWindowUnits(const tools::Rectangle& rRect,
                                                      const vcl::detail::MapConversion& rConv) const
{
    return vcl::GeometryAdapter::Apply(
        vcl::TransformCompiler::Compile(GetViewTransformation(rConv)), rRect);
}
tools::Polygon CoordinateMapper::LogicToWindowUnits(const tools::Polygon& rPoly,
                                                    const vcl::detail::MapConversion& rConv) const
{
    return vcl::GeometryAdapter::Apply(
        vcl::TransformCompiler::Compile(GetViewTransformation(rConv)), rPoly);
}
tools::PolyPolygon
CoordinateMapper::LogicToWindowUnits(const tools::PolyPolygon& rPoly,
                                     const vcl::detail::MapConversion& rConv) const
{
    return vcl::GeometryAdapter::Apply(
        vcl::TransformCompiler::Compile(GetViewTransformation(rConv)), rPoly);
}

Point CoordinateMapper::WindowToLogicUnits(const Point& rPt,
                                           const vcl::detail::MapConversion& rConv) const
{
    return vcl::GeometryAdapter::Apply(
        vcl::TransformCompiler::Compile(GetInverseViewTransformation(rConv)), rPt);
}
Size CoordinateMapper::WindowToLogicUnits(const Size& rSz,
                                          const vcl::detail::MapConversion& rConv) const
{
    return vcl::GeometryAdapter::Apply(
        vcl::TransformCompiler::Compile(GetInverseViewTransformation(rConv)), rSz);
}
tools::Rectangle CoordinateMapper::WindowToLogicUnits(const tools::Rectangle& rRect,
                                                      const vcl::detail::MapConversion& rConv) const
{
    return vcl::GeometryAdapter::Apply(
        vcl::TransformCompiler::Compile(GetInverseViewTransformation(rConv)), rRect);
}
tools::Polygon CoordinateMapper::WindowToLogicUnits(const tools::Polygon& rPoly,
                                                    const vcl::detail::MapConversion& rConv) const
{
    return vcl::GeometryAdapter::Apply(
        vcl::TransformCompiler::Compile(GetInverseViewTransformation(rConv)), rPoly);
}
tools::PolyPolygon
CoordinateMapper::WindowToLogicUnits(const tools::PolyPolygon& rPoly,
                                     const vcl::detail::MapConversion& rConv) const
{
    return vcl::GeometryAdapter::Apply(
        vcl::TransformCompiler::Compile(GetInverseViewTransformation(rConv)), rPoly);
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

    if (!rTransform.PreservesAxisAlignment())
        return vcl::detail::RoundToLong(
            static_cast<double>(nWidth)
            * vcl::detail::GetBasisVectorMagnitudeX(rTransform.maMatrix));

    return vcl::detail::RoundToLong(static_cast<double>(nWidth) * rTransform.maMatrix.get(0, 0));
}

tools::Long CoordinateMapper::LogicHeightToDevicePixel(tools::Long nHeight,
                                                       vcl::MappingPolicy ePolicy) const
{
    const auto& rTransform
        = Compile(TransformRequest{ CoordinateSpace::Logic, CoordinateSpace::Device, ePolicy });

    if (!rTransform.PreservesAxisAlignment())
        return vcl::detail::RoundToLong(
            static_cast<double>(nHeight)
            * vcl::detail::GetBasisVectorMagnitudeY(rTransform.maMatrix));

    return vcl::detail::RoundToLong(static_cast<double>(nHeight) * rTransform.maMatrix.get(1, 1));
}

tools::Long CoordinateMapper::DevicePixelToLogicWidth(tools::Long nWidth,
                                                      vcl::MappingPolicy ePolicy) const
{
    const auto& rTransform
        = Compile(TransformRequest{ CoordinateSpace::Device, CoordinateSpace::Logic, ePolicy });

    if (!rTransform.PreservesAxisAlignment())
        return vcl::detail::RoundToLong(
            static_cast<double>(nWidth)
            * vcl::detail::GetBasisVectorMagnitudeX(rTransform.maMatrix));

    return vcl::detail::RoundToLong(static_cast<double>(nWidth) * rTransform.maMatrix.get(0, 0));
}

tools::Long CoordinateMapper::DevicePixelToLogicHeight(tools::Long nHeight,
                                                       vcl::MappingPolicy ePolicy) const
{
    const auto& rTransform
        = Compile(TransformRequest{ CoordinateSpace::Device, CoordinateSpace::Logic, ePolicy });

    if (!rTransform.PreservesAxisAlignment())
        return vcl::detail::RoundToLong(
            static_cast<double>(nHeight)
            * vcl::detail::GetBasisVectorMagnitudeY(rTransform.maMatrix));

    return vcl::detail::RoundToLong(static_cast<double>(nHeight) * rTransform.maMatrix.get(1, 1));
}

double CoordinateMapper::LogicWidthToWindowSubPixel(tools::Long nWidth,
                                                    vcl::MappingPolicy ePolicy) const
{
    const auto& rTransform
        = Compile(TransformRequest{ CoordinateSpace::Logic, CoordinateSpace::Window, ePolicy });

    if (!rTransform.PreservesAxisAlignment())
        return static_cast<double>(nWidth)
               * vcl::detail::GetBasisVectorMagnitudeX(rTransform.maMatrix);

    return static_cast<double>(nWidth) * rTransform.maMatrix.get(0, 0);
}

double CoordinateMapper::LogicHeightToWindowSubPixel(tools::Long nHeight,
                                                     vcl::MappingPolicy ePolicy) const
{
    const auto& rTransform
        = Compile(TransformRequest{ CoordinateSpace::Logic, CoordinateSpace::Window, ePolicy });

    if (!rTransform.PreservesAxisAlignment())
        return static_cast<double>(nHeight)
               * vcl::detail::GetBasisVectorMagnitudeY(rTransform.maMatrix);

    return static_cast<double>(nHeight) * rTransform.maMatrix.get(1, 1);
}

double CoordinateMapper::LogicWidthToDeviceSubPixel(tools::Long nWidth,
                                                    vcl::MappingPolicy ePolicy) const
{
    // Acquire the compiled execution plan for this specific route
    const auto& rTransform = Compile({ CoordinateSpace::Logic, CoordinateSpace::Device, ePolicy });
    const auto& rMat = rTransform.maMatrix;

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

// ========================================================================
// B2DGeometry Template Implementations
// ========================================================================

template <TransformableB2DGeometry T>
T CoordinateMapper::LogicToWindowUnits(const T& rLogicGeometry, vcl::MappingPolicy ePolicy) const
{
    return vcl::GeometryAdapter::Apply(
        Compile({ CoordinateSpace::Logic, CoordinateSpace::Window, ePolicy }), rLogicGeometry);
}

template <TransformableB2DGeometry T>
T CoordinateMapper::LogicToWindowUnits(const T& rLogicGeometry,
                                       const vcl::detail::MapConversion& rConv) const
{
    return vcl::GeometryAdapter::Apply(
        vcl::TransformCompiler::Compile(GetViewTransformation(rConv)), rLogicGeometry);
}

template <TransformableB2DGeometry T>
T CoordinateMapper::WindowToLogicUnits(const T& rWindowGeometry, vcl::MappingPolicy ePolicy) const
{
    return vcl::GeometryAdapter::Apply(
        Compile({ CoordinateSpace::Window, CoordinateSpace::Logic, ePolicy }), rWindowGeometry);
}

template <TransformableB2DGeometry T>
T CoordinateMapper::WindowToLogicUnits(const T& rWindowGeometry,
                                       const vcl::detail::MapConversion& rConv) const
{
    return vcl::GeometryAdapter::Apply(
        vcl::TransformCompiler::Compile(GetInverseViewTransformation(rConv)), rWindowGeometry);
}

// Explicit Instantiations
template SAL_DLLPRIVATE basegfx::B2DRectangle
CoordinateMapper::LogicToWindowUnits<basegfx::B2DRectangle>(const basegfx::B2DRectangle&,
                                                            vcl::MappingPolicy) const;
template SAL_DLLPRIVATE basegfx::B2DPolygon
CoordinateMapper::LogicToWindowUnits<basegfx::B2DPolygon>(const basegfx::B2DPolygon&,
                                                          vcl::MappingPolicy) const;
template SAL_DLLPRIVATE basegfx::B2DPolyPolygon
CoordinateMapper::LogicToWindowUnits<basegfx::B2DPolyPolygon>(const basegfx::B2DPolyPolygon&,
                                                              vcl::MappingPolicy) const;

template SAL_DLLPRIVATE basegfx::B2DPolygon
CoordinateMapper::LogicToWindowUnits<basegfx::B2DPolygon>(const basegfx::B2DPolygon&,
                                                          const vcl::detail::MapConversion&) const;
template SAL_DLLPRIVATE basegfx::B2DPolyPolygon
CoordinateMapper::LogicToWindowUnits<basegfx::B2DPolyPolygon>(
    const basegfx::B2DPolyPolygon&, const vcl::detail::MapConversion&) const;

template SAL_DLLPRIVATE basegfx::B2DRectangle
CoordinateMapper::WindowToLogicUnits<basegfx::B2DRectangle>(const basegfx::B2DRectangle&,
                                                            vcl::MappingPolicy) const;
template SAL_DLLPRIVATE basegfx::B2DPolygon
CoordinateMapper::WindowToLogicUnits<basegfx::B2DPolygon>(const basegfx::B2DPolygon&,
                                                          vcl::MappingPolicy) const;
template SAL_DLLPRIVATE basegfx::B2DPolyPolygon
CoordinateMapper::WindowToLogicUnits<basegfx::B2DPolyPolygon>(const basegfx::B2DPolyPolygon&,
                                                              vcl::MappingPolicy) const;

template SAL_DLLPRIVATE basegfx::B2DPolygon
CoordinateMapper::WindowToLogicUnits<basegfx::B2DPolygon>(const basegfx::B2DPolygon&,
                                                          const vcl::detail::MapConversion&) const;
template SAL_DLLPRIVATE basegfx::B2DPolyPolygon
CoordinateMapper::WindowToLogicUnits<basegfx::B2DPolyPolygon>(
    const basegfx::B2DPolyPolygon&, const vcl::detail::MapConversion&) const;

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
