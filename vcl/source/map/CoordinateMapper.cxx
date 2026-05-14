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
#include <vcl/CoordinateMapper.hxx>
#include <vcl/GeometryAdapter.hxx>
#include <vcl/TransformRouter.hxx>

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

// ========================================================================
// UNIFIED TEMPLATE IMPLEMENTATIONS & EXPLICIT INSTANTIATIONS
// ========================================================================

template <typename T>
T CoordinateMapper::LogicToDevicePixel(const T& rObj, vcl::MappingPolicy ePolicy) const
{
    return vcl::GeometryAdapter::Apply(
        Compile({ CoordinateSpace::Logic, CoordinateSpace::Device, ePolicy }), rObj);
}

template <typename T>
T CoordinateMapper::DevicePixelToLogic(const T& rObj, vcl::MappingPolicy ePolicy) const
{
    return vcl::GeometryAdapter::Apply(
        Compile({ CoordinateSpace::Device, CoordinateSpace::Logic, ePolicy }), rObj);
}

template <typename T>
T CoordinateMapper::LogicToWindowUnits(const T& rObj, vcl::MappingPolicy ePolicy) const
{
    return vcl::GeometryAdapter::Apply(
        Compile({ CoordinateSpace::Logic, CoordinateSpace::Window, ePolicy }), rObj);
}

template <typename T>
T CoordinateMapper::WindowToLogicUnits(const T& rObj, vcl::MappingPolicy ePolicy) const
{
    return vcl::GeometryAdapter::Apply(
        Compile({ CoordinateSpace::Window, CoordinateSpace::Logic, ePolicy }), rObj);
}

template <typename T>
T CoordinateMapper::LogicToWindowUnits(const T& rObj, const vcl::detail::MapConversion& rConv) const
{
    return vcl::GeometryAdapter::Apply(
        vcl::TransformCompiler::Compile(GetViewTransformation(rConv)), rObj);
}

template <typename T>
T CoordinateMapper::WindowToLogicUnits(const T& rObj, const vcl::detail::MapConversion& rConv) const
{
    return vcl::GeometryAdapter::Apply(
        vcl::TransformCompiler::Compile(GetInverseViewTransformation(rConv)), rObj);
}

basegfx::B2DPoint CoordinateMapper::LogicToDeviceSubPixel(const Point& rPt,
                                                          vcl::MappingPolicy ePolicy) const
{
    basegfx::B2DPoint aPt(rPt.X(), rPt.Y());
    aPt *= Compile({ CoordinateSpace::Logic, CoordinateSpace::Device, ePolicy }).maMatrix;
    return aPt;
}

basegfx::B2DPoint CoordinateMapper::DevicePixelToLogicSubPixel(const Point& rPt,
                                                               vcl::MappingPolicy ePolicy) const
{
    basegfx::B2DPoint aPt(rPt.X(), rPt.Y());
    aPt *= Compile({ CoordinateSpace::Device, CoordinateSpace::Logic, ePolicy }).maMatrix;
    return aPt;
}

Point CoordinateMapper::WindowSubPixelToLogicUnits(const basegfx::B2DPoint& rPt,
                                                   vcl::MappingPolicy ePolicy) const
{
    basegfx::B2DPoint aPt = rPt;
    aPt *= GetWindowToLogicMatrix(ePolicy);
    return Point(vcl::detail::RoundToLong(aPt.getX()), vcl::detail::RoundToLong(aPt.getY()));
}

// ============================================================================
// THE ROUTER DELEGATION & MATRIX BUILDERS
// ============================================================================

const vcl::TransformPlan& CoordinateMapper::Compile(const TransformRequest& rReq) const
{
    return maRouter.Compile(maState, rReq);
}

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
// DISTANCE EXTRACTORS (Vector Magnitudes)
// ========================================================================

tools::Long CoordinateMapper::LogicWidthToDevicePixel(tools::Long nWidth,
                                                      vcl::MappingPolicy ePolicy) const
{
    const auto& rTransform = Compile({ CoordinateSpace::Logic, CoordinateSpace::Device, ePolicy });
    if (!rTransform.PreservesAxisAlignment())
        return vcl::detail::RoundToLong(
            static_cast<double>(nWidth)
            * vcl::detail::GetBasisVectorMagnitudeX(rTransform.maMatrix));
    return vcl::detail::RoundToLong(static_cast<double>(nWidth) * rTransform.maMatrix.get(0, 0));
}

tools::Long CoordinateMapper::LogicHeightToDevicePixel(tools::Long nHeight,
                                                       vcl::MappingPolicy ePolicy) const
{
    const auto& rTransform = Compile({ CoordinateSpace::Logic, CoordinateSpace::Device, ePolicy });
    if (!rTransform.PreservesAxisAlignment())
        return vcl::detail::RoundToLong(
            static_cast<double>(nHeight)
            * vcl::detail::GetBasisVectorMagnitudeY(rTransform.maMatrix));
    return vcl::detail::RoundToLong(static_cast<double>(nHeight) * rTransform.maMatrix.get(1, 1));
}

tools::Long CoordinateMapper::DevicePixelToLogicWidth(tools::Long nWidth,
                                                      vcl::MappingPolicy ePolicy) const
{
    const auto& rTransform = Compile({ CoordinateSpace::Device, CoordinateSpace::Logic, ePolicy });
    if (!rTransform.PreservesAxisAlignment())
        return vcl::detail::RoundToLong(
            static_cast<double>(nWidth)
            * vcl::detail::GetBasisVectorMagnitudeX(rTransform.maMatrix));
    return vcl::detail::RoundToLong(static_cast<double>(nWidth) * rTransform.maMatrix.get(0, 0));
}

tools::Long CoordinateMapper::DevicePixelToLogicHeight(tools::Long nHeight,
                                                       vcl::MappingPolicy ePolicy) const
{
    const auto& rTransform = Compile({ CoordinateSpace::Device, CoordinateSpace::Logic, ePolicy });
    if (!rTransform.PreservesAxisAlignment())
        return vcl::detail::RoundToLong(
            static_cast<double>(nHeight)
            * vcl::detail::GetBasisVectorMagnitudeY(rTransform.maMatrix));
    return vcl::detail::RoundToLong(static_cast<double>(nHeight) * rTransform.maMatrix.get(1, 1));
}

double CoordinateMapper::LogicWidthToWindowSubPixel(tools::Long nWidth,
                                                    vcl::MappingPolicy ePolicy) const
{
    const auto& rTransform = Compile({ CoordinateSpace::Logic, CoordinateSpace::Window, ePolicy });
    if (!rTransform.PreservesAxisAlignment())
        return static_cast<double>(nWidth)
               * vcl::detail::GetBasisVectorMagnitudeX(rTransform.maMatrix);
    return static_cast<double>(nWidth) * rTransform.maMatrix.get(0, 0);
}

double CoordinateMapper::LogicHeightToWindowSubPixel(tools::Long nHeight,
                                                     vcl::MappingPolicy ePolicy) const
{
    const auto& rTransform = Compile({ CoordinateSpace::Logic, CoordinateSpace::Window, ePolicy });
    if (!rTransform.PreservesAxisAlignment())
        return static_cast<double>(nHeight)
               * vcl::detail::GetBasisVectorMagnitudeY(rTransform.maMatrix);
    return static_cast<double>(nHeight) * rTransform.maMatrix.get(1, 1);
}

double CoordinateMapper::LogicWidthToDeviceSubPixel(tools::Long nWidth,
                                                    vcl::MappingPolicy ePolicy) const
{
    const auto& rTransform = Compile({ CoordinateSpace::Logic, CoordinateSpace::Device, ePolicy });
    if (rTransform.PreservesAxisAlignment())
        return static_cast<double>(nWidth) * rTransform.maMatrix.get(0, 0);
    return static_cast<double>(nWidth) * vcl::detail::GetBasisVectorMagnitudeX(rTransform.maMatrix);
}

// ========================================================================
// EXPLICIT TEMPLATE INSTANTIATIONS
// ========================================================================

// Point
template VCL_DLLPUBLIC Point CoordinateMapper::LogicToDevicePixel<Point>(const Point&,
                                                                         vcl::MappingPolicy) const;
template VCL_DLLPUBLIC Point CoordinateMapper::DevicePixelToLogic<Point>(const Point&,
                                                                         vcl::MappingPolicy) const;
template VCL_DLLPUBLIC Point CoordinateMapper::LogicToWindowUnits<Point>(const Point&,
                                                                         vcl::MappingPolicy) const;
template VCL_DLLPUBLIC Point CoordinateMapper::WindowToLogicUnits<Point>(const Point&,
                                                                         vcl::MappingPolicy) const;
template VCL_DLLPUBLIC Point
CoordinateMapper::LogicToWindowUnits<Point>(const Point&, const vcl::detail::MapConversion&) const;
template VCL_DLLPUBLIC Point
CoordinateMapper::WindowToLogicUnits<Point>(const Point&, const vcl::detail::MapConversion&) const;

// Size
template VCL_DLLPUBLIC Size CoordinateMapper::LogicToDevicePixel<Size>(const Size&,
                                                                       vcl::MappingPolicy) const;
template VCL_DLLPUBLIC Size CoordinateMapper::DevicePixelToLogic<Size>(const Size&,
                                                                       vcl::MappingPolicy) const;
template VCL_DLLPUBLIC Size CoordinateMapper::LogicToWindowUnits<Size>(const Size&,
                                                                       vcl::MappingPolicy) const;
template VCL_DLLPUBLIC Size CoordinateMapper::WindowToLogicUnits<Size>(const Size&,
                                                                       vcl::MappingPolicy) const;
template VCL_DLLPUBLIC Size
CoordinateMapper::LogicToWindowUnits<Size>(const Size&, const vcl::detail::MapConversion&) const;
template VCL_DLLPUBLIC Size
CoordinateMapper::WindowToLogicUnits<Size>(const Size&, const vcl::detail::MapConversion&) const;

// tools::Rectangle
template VCL_DLLPUBLIC tools::Rectangle
CoordinateMapper::LogicToDevicePixel<tools::Rectangle>(const tools::Rectangle&,
                                                       vcl::MappingPolicy) const;
template VCL_DLLPUBLIC tools::Rectangle
CoordinateMapper::DevicePixelToLogic<tools::Rectangle>(const tools::Rectangle&,
                                                       vcl::MappingPolicy) const;
template VCL_DLLPUBLIC tools::Rectangle
CoordinateMapper::LogicToWindowUnits<tools::Rectangle>(const tools::Rectangle&,
                                                       vcl::MappingPolicy) const;
template VCL_DLLPUBLIC tools::Rectangle
CoordinateMapper::WindowToLogicUnits<tools::Rectangle>(const tools::Rectangle&,
                                                       vcl::MappingPolicy) const;
template VCL_DLLPUBLIC tools::Rectangle
CoordinateMapper::LogicToWindowUnits<tools::Rectangle>(const tools::Rectangle&,
                                                       const vcl::detail::MapConversion&) const;
template VCL_DLLPUBLIC tools::Rectangle
CoordinateMapper::WindowToLogicUnits<tools::Rectangle>(const tools::Rectangle&,
                                                       const vcl::detail::MapConversion&) const;

// tools::Polygon
template VCL_DLLPUBLIC tools::Polygon
CoordinateMapper::LogicToDevicePixel<tools::Polygon>(const tools::Polygon&,
                                                     vcl::MappingPolicy) const;
template VCL_DLLPUBLIC tools::Polygon
CoordinateMapper::DevicePixelToLogic<tools::Polygon>(const tools::Polygon&,
                                                     vcl::MappingPolicy) const;
template VCL_DLLPUBLIC tools::Polygon
CoordinateMapper::LogicToWindowUnits<tools::Polygon>(const tools::Polygon&,
                                                     vcl::MappingPolicy) const;
template VCL_DLLPUBLIC tools::Polygon
CoordinateMapper::WindowToLogicUnits<tools::Polygon>(const tools::Polygon&,
                                                     vcl::MappingPolicy) const;
template VCL_DLLPUBLIC tools::Polygon
CoordinateMapper::LogicToWindowUnits<tools::Polygon>(const tools::Polygon&,
                                                     const vcl::detail::MapConversion&) const;
template VCL_DLLPUBLIC tools::Polygon
CoordinateMapper::WindowToLogicUnits<tools::Polygon>(const tools::Polygon&,
                                                     const vcl::detail::MapConversion&) const;

// tools::PolyPolygon
template VCL_DLLPUBLIC tools::PolyPolygon
CoordinateMapper::LogicToDevicePixel<tools::PolyPolygon>(const tools::PolyPolygon&,
                                                         vcl::MappingPolicy) const;
template VCL_DLLPUBLIC tools::PolyPolygon
CoordinateMapper::DevicePixelToLogic<tools::PolyPolygon>(const tools::PolyPolygon&,
                                                         vcl::MappingPolicy) const;
template VCL_DLLPUBLIC tools::PolyPolygon
CoordinateMapper::LogicToWindowUnits<tools::PolyPolygon>(const tools::PolyPolygon&,
                                                         vcl::MappingPolicy) const;
template VCL_DLLPUBLIC tools::PolyPolygon
CoordinateMapper::WindowToLogicUnits<tools::PolyPolygon>(const tools::PolyPolygon&,
                                                         vcl::MappingPolicy) const;
template VCL_DLLPUBLIC tools::PolyPolygon
CoordinateMapper::LogicToWindowUnits<tools::PolyPolygon>(const tools::PolyPolygon&,
                                                         const vcl::detail::MapConversion&) const;
template VCL_DLLPUBLIC tools::PolyPolygon
CoordinateMapper::WindowToLogicUnits<tools::PolyPolygon>(const tools::PolyPolygon&,
                                                         const vcl::detail::MapConversion&) const;

// vcl::Region
template VCL_DLLPUBLIC vcl::Region
CoordinateMapper::LogicToDevicePixel<vcl::Region>(const vcl::Region&, vcl::MappingPolicy) const;
template VCL_DLLPUBLIC vcl::Region
CoordinateMapper::DevicePixelToLogic<vcl::Region>(const vcl::Region&, vcl::MappingPolicy) const;
template VCL_DLLPUBLIC vcl::Region
CoordinateMapper::LogicToWindowUnits<vcl::Region>(const vcl::Region&, vcl::MappingPolicy) const;
template VCL_DLLPUBLIC vcl::Region
CoordinateMapper::WindowToLogicUnits<vcl::Region>(const vcl::Region&, vcl::MappingPolicy) const;
template VCL_DLLPUBLIC vcl::Region
CoordinateMapper::LogicToWindowUnits<vcl::Region>(const vcl::Region&,
                                                  const vcl::detail::MapConversion&) const;
template VCL_DLLPUBLIC vcl::Region
CoordinateMapper::WindowToLogicUnits<vcl::Region>(const vcl::Region&,
                                                  const vcl::detail::MapConversion&) const;

// LineInfo
template VCL_DLLPUBLIC LineInfo
CoordinateMapper::LogicToDevicePixel<LineInfo>(const LineInfo&, vcl::MappingPolicy) const;
template VCL_DLLPUBLIC LineInfo
CoordinateMapper::DevicePixelToLogic<LineInfo>(const LineInfo&, vcl::MappingPolicy) const;
template VCL_DLLPUBLIC LineInfo
CoordinateMapper::LogicToWindowUnits<LineInfo>(const LineInfo&, vcl::MappingPolicy) const;
template VCL_DLLPUBLIC LineInfo
CoordinateMapper::WindowToLogicUnits<LineInfo>(const LineInfo&, vcl::MappingPolicy) const;
template VCL_DLLPUBLIC LineInfo CoordinateMapper::LogicToWindowUnits<LineInfo>(
    const LineInfo&, const vcl::detail::MapConversion&) const;
template VCL_DLLPUBLIC LineInfo CoordinateMapper::WindowToLogicUnits<LineInfo>(
    const LineInfo&, const vcl::detail::MapConversion&) const;

// basegfx::B2DPolygon
template VCL_DLLPUBLIC basegfx::B2DPolygon
CoordinateMapper::LogicToDevicePixel<basegfx::B2DPolygon>(const basegfx::B2DPolygon&,
                                                          vcl::MappingPolicy) const;
template VCL_DLLPUBLIC basegfx::B2DPolygon
CoordinateMapper::DevicePixelToLogic<basegfx::B2DPolygon>(const basegfx::B2DPolygon&,
                                                          vcl::MappingPolicy) const;
template VCL_DLLPUBLIC basegfx::B2DPolygon
CoordinateMapper::LogicToWindowUnits<basegfx::B2DPolygon>(const basegfx::B2DPolygon&,
                                                          vcl::MappingPolicy) const;
template VCL_DLLPUBLIC basegfx::B2DPolygon
CoordinateMapper::WindowToLogicUnits<basegfx::B2DPolygon>(const basegfx::B2DPolygon&,
                                                          vcl::MappingPolicy) const;
template VCL_DLLPUBLIC basegfx::B2DPolygon
CoordinateMapper::LogicToWindowUnits<basegfx::B2DPolygon>(const basegfx::B2DPolygon&,
                                                          const vcl::detail::MapConversion&) const;
template VCL_DLLPUBLIC basegfx::B2DPolygon
CoordinateMapper::WindowToLogicUnits<basegfx::B2DPolygon>(const basegfx::B2DPolygon&,
                                                          const vcl::detail::MapConversion&) const;

// basegfx::B2DPolyPolygon
template VCL_DLLPUBLIC basegfx::B2DPolyPolygon
CoordinateMapper::LogicToDevicePixel<basegfx::B2DPolyPolygon>(const basegfx::B2DPolyPolygon&,
                                                              vcl::MappingPolicy) const;
template VCL_DLLPUBLIC basegfx::B2DPolyPolygon
CoordinateMapper::DevicePixelToLogic<basegfx::B2DPolyPolygon>(const basegfx::B2DPolyPolygon&,
                                                              vcl::MappingPolicy) const;
template VCL_DLLPUBLIC basegfx::B2DPolyPolygon
CoordinateMapper::LogicToWindowUnits<basegfx::B2DPolyPolygon>(const basegfx::B2DPolyPolygon&,
                                                              vcl::MappingPolicy) const;
template VCL_DLLPUBLIC basegfx::B2DPolyPolygon
CoordinateMapper::WindowToLogicUnits<basegfx::B2DPolyPolygon>(const basegfx::B2DPolyPolygon&,
                                                              vcl::MappingPolicy) const;
template VCL_DLLPUBLIC basegfx::B2DPolyPolygon
CoordinateMapper::LogicToWindowUnits<basegfx::B2DPolyPolygon>(
    const basegfx::B2DPolyPolygon&, const vcl::detail::MapConversion&) const;
template VCL_DLLPUBLIC basegfx::B2DPolyPolygon
CoordinateMapper::WindowToLogicUnits<basegfx::B2DPolyPolygon>(
    const basegfx::B2DPolyPolygon&, const vcl::detail::MapConversion&) const;

// basegfx::B2DRange
template VCL_DLLPUBLIC basegfx::B2DRange
CoordinateMapper::LogicToDevicePixel<basegfx::B2DRange>(const basegfx::B2DRange&,
                                                        vcl::MappingPolicy) const;
template VCL_DLLPUBLIC basegfx::B2DRange
CoordinateMapper::DevicePixelToLogic<basegfx::B2DRange>(const basegfx::B2DRange&,
                                                        vcl::MappingPolicy) const;
template VCL_DLLPUBLIC basegfx::B2DRange
CoordinateMapper::LogicToWindowUnits<basegfx::B2DRange>(const basegfx::B2DRange&,
                                                        vcl::MappingPolicy) const;
template VCL_DLLPUBLIC basegfx::B2DRange
CoordinateMapper::WindowToLogicUnits<basegfx::B2DRange>(const basegfx::B2DRange&,
                                                        vcl::MappingPolicy) const;
template VCL_DLLPUBLIC basegfx::B2DRange
CoordinateMapper::LogicToWindowUnits<basegfx::B2DRange>(const basegfx::B2DRange&,
                                                        const vcl::detail::MapConversion&) const;
template VCL_DLLPUBLIC basegfx::B2DRange
CoordinateMapper::WindowToLogicUnits<basegfx::B2DRange>(const basegfx::B2DRange&,
                                                        const vcl::detail::MapConversion&) const;

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
