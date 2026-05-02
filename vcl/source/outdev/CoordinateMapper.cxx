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

// Conceptual Pipeline Separation (Mathematical Invariant):
// Logic -> View: Scaled transformations (Scale * Logic) + Scaled Offsets ((MapOfs + LogicOfs) * Scale)
// View -> Window: Pure translation (WindowOfs)
// Window -> Device: Pure translation (DeviceOfs)
void CoordinateMapper::GetLogicToViewWeights(double& rScaleX, double& rScaleY, double& rTransX,
                                             double& rTransY, bool bMap) const
{
    rScaleX = GetLogicToWindowMatrix(bMap).get(0, 0);
    rScaleY = GetLogicToWindowMatrix(bMap).get(1, 1);
    rTransX = GetLogicToWindowMatrix(bMap).get(0, 2);
    rTransY = GetLogicToWindowMatrix(bMap).get(1, 2);
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

    // STRANGLER STEP 2: The math now operates entirely on the firewall struct
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

// ========================================================================
// PIPELINE STAGES (Coordinate Transitions)
// ========================================================================

// Device <-> Window (Screen Origin)
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

// View <-> Absolute Logic (Map Scale & Map Offset)

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
    return Size(lcl_RoundToLong(std::abs(rLogicSize.Width() * mat.get(0, 0))),
                lcl_RoundToLong(std::abs(rLogicSize.Height() * mat.get(1, 1))));
}

static void lcl_ApplyEmptyState(tools::Rectangle& rDest, const tools::Rectangle& rSrc)
{
    if (rSrc.IsWidthEmpty())
        rDest.SetWidthEmpty();

    if (rSrc.IsHeightEmpty())
        rDest.SetHeightEmpty();
}

tools::Rectangle CoordinateMapper::LogicToWindowUnits(const tools::Rectangle& rRect,
                                                      const vcl::detail::MapConversion& rConv) const
{
    basegfx::B2DHomMatrix aMat = GetViewTransformation(rConv);
    basegfx::B2DRange aRange(rRect.Left(), rRect.Top(), rRect.Right(), rRect.Bottom());
    aRange.transform(aMat);
    tools::Rectangle aRetval(lcl_RoundToLong(aRange.getMinX()), lcl_RoundToLong(aRange.getMinY()),
                             lcl_RoundToLong(aRange.getMaxX()), lcl_RoundToLong(aRange.getMaxY()));
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

// ========================================================================
// MASTER WRAPPERS (Multi-space Positional Transformations)
// ========================================================================

Size CoordinateMapper::LogicToWindowUnits(const Size& rLogicSize, bool bMap) const
{
    const basegfx::B2DHomMatrix aMat = GetLogicToWindowMatrix(bMap);
    const double sx = lcl_GetScaledXLength(aMat);
    const double sy = lcl_GetScaledYLength(aMat);
    return Size(lcl_RoundToLong(rLogicSize.Width() * sx),
                lcl_RoundToLong(rLogicSize.Height() * sy));
}

// --- Sub-Pixel Full Journey ---

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

// Integer Boundary (this is the only place rounding occurs)

Point CoordinateMapper::DevicePixelToLogic(const Point& rDevicePt, bool bMap) const
{
    basegfx::B2DHomMatrix aMat = GetDeviceToLogicMatrix(bMap);
    basegfx::B2DPoint aPt(rDevicePt.X(), rDevicePt.Y());
    aPt *= aMat;
    return Point(lcl_RoundToLong(aPt.getX()), lcl_RoundToLong(aPt.getY()));
}

tools::Rectangle CoordinateMapper::DevicePixelToLogic(const tools::Rectangle& rPixelRect,
                                                      bool bMap) const
{
    basegfx::B2DHomMatrix aMat = GetDeviceToLogicMatrix(bMap);
    basegfx::B2DRange aRange(rPixelRect.Left(), rPixelRect.Top(), rPixelRect.Right(),
                             rPixelRect.Bottom());
    aRange.transform(aMat);
    return tools::Rectangle(lcl_RoundToLong(aRange.getMinX()), lcl_RoundToLong(aRange.getMinY()),
                            lcl_RoundToLong(aRange.getMaxX()), lcl_RoundToLong(aRange.getMaxY()));
}

// Note: Width/Height use Distances, not Positions!
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

Size CoordinateMapper::LogicToDevicePixel(const Size& rLogicSize, bool bMap) const
{
    const basegfx::B2DHomMatrix aMat = GetLogicToDeviceMatrix(bMap);
    const double sx = lcl_GetScaledXLength(aMat);
    const double sy = lcl_GetScaledYLength(aMat);
    return Size(lcl_RoundToLong(rLogicSize.Width() * sx),
                lcl_RoundToLong(rLogicSize.Height() * sy));
}

template <typename TransformFunc>
static vcl::Region lcl_TransformRegion(const vcl::Region& rRegion, TransformFunc&& func)
{
    if (rRegion.IsNull() || rRegion.IsEmpty())
        return rRegion;

    vcl::Region aRegion;
    if (rRegion.getB2DPolyPolygon())
    {
        aRegion = vcl::Region(func(*rRegion.getB2DPolyPolygon()));
    }
    else if (rRegion.getPolyPolygon())
    {
        aRegion = vcl::Region(func(*rRegion.getPolyPolygon()));
    }
    else if (rRegion.getRegionBand())
    {
        RectangleVector aRectangles;
        rRegion.GetRegionRectangles(aRectangles);

        for (const auto& rRect : aRectangles | std::views::reverse)
        {
            aRegion.Union(func(rRect));
        }
    }
    return aRegion;
}

tools::PolyPolygon CoordinateMapper::LogicToDevicePixel(const tools::PolyPolygon& rLogicPolyPoly,
                                                        bool bMap) const
{
    if (!bMap && IsValidDPI() && !GetDeviceToWindowOffsetX() && !GetDeviceToWindowOffsetY())
        return rLogicPolyPoly;

    tools::PolyPolygon aPolyPoly(rLogicPolyPoly);

    for (auto& rPoly : aPolyPoly)
    {
        rPoly = LogicToDevicePixel(rPoly, bMap);
    }

    return aPolyPoly;
}

LineInfo CoordinateMapper::LogicToDevicePixel(const LineInfo& rLineInfo, bool bMap) const
{
    LineInfo aInfo(rLineInfo);

    if (aInfo.GetStyle() == LineStyle::Dash)
    {
        if (aInfo.GetDotCount() && aInfo.GetDotLen())
            aInfo.SetDotLen(
                std::max(LogicWidthToDevicePixel(aInfo.GetDotLen(), bMap), tools::Long(1)));
        else
            aInfo.SetDotCount(0);

        if (aInfo.GetDashCount() && aInfo.GetDashLen())
            aInfo.SetDashLen(
                std::max(LogicWidthToDevicePixel(aInfo.GetDashLen(), bMap), tools::Long(1)));
        else
            aInfo.SetDashCount(0);

        aInfo.SetDistance(LogicWidthToDevicePixel(aInfo.GetDistance(), bMap));

        if ((!aInfo.GetDashCount() && !aInfo.GetDotCount()) || !aInfo.GetDistance())
            aInfo.SetStyle(LineStyle::Solid);
    }

    aInfo.SetWidth(LogicWidthToDevicePixel(aInfo.GetWidth(), bMap));

    return aInfo;
}

basegfx::B2DPolygon CoordinateMapper::LogicToDevicePixel(const basegfx::B2DPolygon& rLogicPoly,
                                                         bool bMap) const
{
    if (!bMap && IsValidDPI() && !GetDeviceToWindowOffsetX() && !GetDeviceToWindowOffsetY())
        return rLogicPoly;

    basegfx::B2DPolygon aPoly(rLogicPoly);
    aPoly.transform(GetLogicToDeviceMatrix(bMap));
    return aPoly;
}

basegfx::B2DPolyPolygon
CoordinateMapper::LogicToDevicePixel(const basegfx::B2DPolyPolygon& rLogicPolyPoly, bool bMap) const
{
    if (!bMap && IsValidDPI() && !GetDeviceToWindowOffsetX() && !GetDeviceToWindowOffsetY())
        return rLogicPolyPoly;

    basegfx::B2DPolyPolygon aPoly(rLogicPolyPoly);
    aPoly.transform(GetLogicToDeviceMatrix(bMap));
    return aPoly;
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

Point CoordinateMapper::LogicToWindowUnits(const Point& rLogicPt, bool bMap) const
{
    basegfx::B2DHomMatrix aMat = GetLogicToWindowMatrix(bMap);
    basegfx::B2DPoint aPt(rLogicPt.X(), rLogicPt.Y());
    aPt *= aMat;
    return Point(lcl_RoundToLong(aPt.getX()), lcl_RoundToLong(aPt.getY()));
}

tools::Rectangle CoordinateMapper::LogicToWindowUnits(const tools::Rectangle& rRect,
                                                      bool bMap) const
{
    basegfx::B2DHomMatrix aMat = GetLogicToWindowMatrix(bMap);
    basegfx::B2DRange aRange(rRect.Left(), rRect.Top(), rRect.Right(), rRect.Bottom());
    aRange.transform(aMat);
    return tools::Rectangle(lcl_RoundToLong(aRange.getMinX()), lcl_RoundToLong(aRange.getMinY()),
                            lcl_RoundToLong(aRange.getMaxX()), lcl_RoundToLong(aRange.getMaxY()));
}

vcl::Region CoordinateMapper::LogicToWindowUnits(const vcl::Region& rLogicRegion, bool bMap) const
{
    if (!bMap && IsValidDPI())
        return rLogicRegion;

    return lcl_TransformRegion(
        rLogicRegion, [bMap, this](const auto& obj) { return LogicToWindowUnits(obj, bMap); });
}

tools::Polygon CoordinateMapper::LogicToWindowUnits(const tools::Polygon& rPoly, bool bMap) const
{
    if (!bMap && IsValidDPI())
        return rPoly;

    tools::Polygon aPoly(rPoly);
    basegfx::B2DHomMatrix aMat = GetLogicToWindowMatrix(bMap);
    for (auto& rPoint : aPoly)
    {
        basegfx::B2DPoint aPt(rPoint.X(), rPoint.Y());
        aPt *= aMat;
        rPoint = Point(lcl_RoundToLong(aPt.getX()), lcl_RoundToLong(aPt.getY()));
    }

    return aPoly;
}

tools::PolyPolygon CoordinateMapper::LogicToWindowUnits(const tools::PolyPolygon& rPolyPoly,
                                                        bool bMap) const
{
    if (!bMap && IsValidDPI())
        return rPolyPoly;

    tools::PolyPolygon aPolyPoly(rPolyPoly);

    for (auto& rPoly : aPolyPoly)
    {
        rPoly = LogicToWindowUnits(rPoly, bMap);
    }

    return aPolyPoly;
}

template <TransformableB2DGeometry T>
T CoordinateMapper::LogicToWindowUnits(const T& rLogicGeometry, bool bMap) const
{
    T aTransformedGeometry = rLogicGeometry;
    aTransformedGeometry.transform(GetViewTransformation(bMap));
    return aTransformedGeometry;
}

template SAL_DLLPRIVATE basegfx::B2DRectangle
CoordinateMapper::LogicToWindowUnits<basegfx::B2DRectangle>(const basegfx::B2DRectangle&,
                                                            bool) const;

template SAL_DLLPRIVATE basegfx::B2DPolygon
CoordinateMapper::LogicToWindowUnits<basegfx::B2DPolygon>(const basegfx::B2DPolygon&, bool) const;

template SAL_DLLPRIVATE basegfx::B2DPolyPolygon
CoordinateMapper::LogicToWindowUnits<basegfx::B2DPolyPolygon>(const basegfx::B2DPolyPolygon&,
                                                              bool) const;

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

vcl::Region CoordinateMapper::WindowToLogicUnits(const vcl::Region& rWindowRegion, bool bMap) const
{
    if (!bMap && IsValidDPI())
        return rWindowRegion;

    return lcl_TransformRegion(
        rWindowRegion, [this, bMap](const auto& obj) { return WindowToLogicUnits(obj, bMap); });
}

Point CoordinateMapper::WindowToLogicUnits(const Point& rWindowPt, bool bMap) const
{
    basegfx::B2DHomMatrix aMat = GetWindowToLogicMatrix(bMap);
    basegfx::B2DPoint aPt(rWindowPt.X(), rWindowPt.Y());
    aPt *= aMat;
    return Point(lcl_RoundToLong(aPt.getX()), lcl_RoundToLong(aPt.getY()));
}

Point CoordinateMapper::WindowSubPixelToLogicUnits(const basegfx::B2DPoint& rWindowPt,
                                                   bool bMap) const
{
    basegfx::B2DHomMatrix aMat = GetWindowToLogicMatrix(bMap);
    basegfx::B2DPoint aPt(rWindowPt);
    aPt *= aMat;
    return Point(lcl_RoundToLong(aPt.getX()), lcl_RoundToLong(aPt.getY()));
}

Size CoordinateMapper::WindowToLogicUnits(const Size& rWindowSize, bool bMap) const
{
    const basegfx::B2DHomMatrix aMat = GetWindowToLogicMatrix(bMap);
    const double sx = lcl_GetScaledXLength(aMat);
    const double sy = lcl_GetScaledYLength(aMat);
    return Size(lcl_RoundToLong(rWindowSize.Width() * sx),
                lcl_RoundToLong(rWindowSize.Height() * sy));
}

tools::Rectangle CoordinateMapper::WindowToLogicUnits(const tools::Rectangle& rWindowRect,
                                                      bool bMap) const
{
    basegfx::B2DHomMatrix aMat = GetWindowToLogicMatrix(bMap);
    basegfx::B2DRange aRange(rWindowRect.Left(), rWindowRect.Top(), rWindowRect.Right(),
                             rWindowRect.Bottom());
    aRange.transform(aMat);
    return tools::Rectangle(lcl_RoundToLong(aRange.getMinX()), lcl_RoundToLong(aRange.getMinY()),
                            lcl_RoundToLong(aRange.getMaxX()), lcl_RoundToLong(aRange.getMaxY()));
}

tools::Polygon CoordinateMapper::WindowToLogicUnits(const tools::Polygon& rWindowPoly,
                                                    bool bMap) const
{
    if (!bMap && IsValidDPI())
        return rWindowPoly;

    tools::Polygon aPoly(rWindowPoly);
    basegfx::B2DHomMatrix aMat = GetWindowToLogicMatrix(bMap);
    for (auto& rPoint : aPoly)
    {
        basegfx::B2DPoint aPt(rPoint.X(), rPoint.Y());
        aPt *= aMat;
        rPoint = Point(lcl_RoundToLong(aPt.getX()), lcl_RoundToLong(aPt.getY()));
    }

    return aPoly;
}

tools::PolyPolygon CoordinateMapper::WindowToLogicUnits(const tools::PolyPolygon& rWindowPolyPoly,
                                                        bool bMap) const
{
    if (!bMap && IsValidDPI())
        return rWindowPolyPoly;

    tools::PolyPolygon aPolyPoly(rWindowPolyPoly);

    for (auto& rPoly : aPolyPoly)
    {
        rPoly = WindowToLogicUnits(rPoly, bMap);
    }

    return aPolyPoly;
}

template <TransformableB2DGeometry T>
T CoordinateMapper::WindowToLogicUnits(const T& rWindowGeometry, bool bMap) const
{
    if (!bMap && IsValidDPI())
        return rWindowGeometry;

    T aTransformedGeometry = rWindowGeometry;
    aTransformedGeometry.transform(GetInverseViewTransformation(bMap));
    return aTransformedGeometry;
}

template SAL_DLLPRIVATE basegfx::B2DRectangle
CoordinateMapper::WindowToLogicUnits<basegfx::B2DRectangle>(const basegfx::B2DRectangle&,
                                                            bool) const;

template SAL_DLLPRIVATE basegfx::B2DPolyPolygon
CoordinateMapper::WindowToLogicUnits<basegfx::B2DPolyPolygon>(const basegfx::B2DPolyPolygon&,
                                                              bool) const;

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
    return Size(lcl_RoundToLong(std::abs(rWindowSize.Width() * mat.get(0, 0))),
                lcl_RoundToLong(std::abs(rWindowSize.Height() * mat.get(1, 1))));
}

tools::Rectangle CoordinateMapper::WindowToLogicUnits(const tools::Rectangle& rWindowRect,
                                                      const vcl::detail::MapConversion& rConv) const
{
    basegfx::B2DHomMatrix aMat = GetInverseViewTransformation(rConv);
    basegfx::B2DRange aRange(rWindowRect.Left(), rWindowRect.Top(), rWindowRect.Right(),
                             rWindowRect.Bottom());
    aRange.transform(aMat);
    tools::Rectangle aRetval(lcl_RoundToLong(aRange.getMinX()), lcl_RoundToLong(aRange.getMinY()),
                             lcl_RoundToLong(aRange.getMaxX()), lcl_RoundToLong(aRange.getMaxY()));
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

// ========================================================================
// DISTANCE SCALING (Raw Scalar Conversion)
// ========================================================================

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

Size CoordinateMapper::DevicePixelToLogic(const Size& rDeviceSize, bool bMap) const
{
    const basegfx::B2DHomMatrix aMat = GetDeviceToLogicMatrix(bMap);
    const double sx = lcl_GetScaledXLength(aMat);
    const double sy = lcl_GetScaledYLength(aMat);
    return Size(lcl_RoundToLong(rDeviceSize.Width() * sx),
                lcl_RoundToLong(rDeviceSize.Height() * sy));
}

vcl::Region CoordinateMapper::DevicePixelToLogic(const vcl::Region& rPixelRegion, bool bMap) const
{
    if (!bMap && IsValidDPI() && !GetDeviceToWindowOffsetX() && !GetDeviceToWindowOffsetY())
        return rPixelRegion;

    return lcl_TransformRegion(
        rPixelRegion, [this, bMap](const auto& obj) { return DevicePixelToLogic(obj, bMap); });
}

tools::Polygon CoordinateMapper::DevicePixelToLogic(const tools::Polygon& rPixelPoly,
                                                    bool bMap) const
{
    if (!bMap && IsValidDPI() && !GetDeviceToWindowOffsetX() && !GetDeviceToWindowOffsetY())
        return rPixelPoly;

    tools::Polygon aPoly(rPixelPoly);
    basegfx::B2DHomMatrix aMat = GetDeviceToLogicMatrix(bMap);
    for (auto& rPoint : aPoly)
    {
        basegfx::B2DPoint aPt(rPoint.X(), rPoint.Y());
        aPt *= aMat;
        rPoint = Point(lcl_RoundToLong(aPt.getX()), lcl_RoundToLong(aPt.getY()));
    }
    return aPoly;
}

tools::PolyPolygon CoordinateMapper::DevicePixelToLogic(const tools::PolyPolygon& rPixelPolyPoly,
                                                        bool bMap) const
{
    if (!bMap && IsValidDPI() && !GetDeviceToWindowOffsetX() && !GetDeviceToWindowOffsetY())
        return rPixelPolyPoly;

    tools::PolyPolygon aPolyPoly(rPixelPolyPoly);
    for (auto& rPoly : aPolyPoly)
        rPoly = DevicePixelToLogic(rPoly, bMap);
    return aPolyPoly;
}

basegfx::B2DPolygon CoordinateMapper::DevicePixelToLogic(const basegfx::B2DPolygon& rPixelPoly,
                                                         bool bMap) const
{
    if (!bMap && IsValidDPI() && !GetDeviceToWindowOffsetX() && !GetDeviceToWindowOffsetY())
        return rPixelPoly;

    basegfx::B2DPolygon aPoly(rPixelPoly);
    aPoly.transform(GetDeviceToLogicMatrix(bMap));
    return aPoly;
}

basegfx::B2DPolyPolygon
CoordinateMapper::DevicePixelToLogic(const basegfx::B2DPolyPolygon& rPixelPolyPoly, bool bMap) const
{
    if (!bMap && IsValidDPI() && !GetDeviceToWindowOffsetX() && !GetDeviceToWindowOffsetY())
        return rPixelPolyPoly;

    basegfx::B2DPolyPolygon aPoly(rPixelPolyPoly);
    aPoly.transform(GetDeviceToLogicMatrix(bMap));
    return aPoly;
}

double CoordinateMapper::LogicToWindowSubPixelX(double fX, bool bMap) const
{
    return fX * GetLogicToWindowMatrix(bMap).get(0, 0) + GetLogicToWindowMatrix(bMap).get(0, 2);
}

double CoordinateMapper::LogicToWindowSubPixelY(double fY, bool bMap) const
{
    return fY * GetLogicToWindowMatrix(bMap).get(1, 1) + GetLogicToWindowMatrix(bMap).get(1, 2);
}

Point CoordinateMapper::LogicToLogic(const Point& rPtSource, const MapMode* pMapModeBaseline,
                                     const MapMode* pMapModeSource, const MapMode* pMapModeDest,
                                     bool bMap) const
{
    const MapMode* pSrc = pMapModeSource ? pMapModeSource : pMapModeBaseline;
    const MapMode* pDst = pMapModeDest ? pMapModeDest : pMapModeBaseline;

    // Safety check to prevent null dereference, then check equivalence
    if (!pSrc || !pDst || *pSrc == *pDst)
        return rPtSource;

    // Pass the resolved non-null pointers down the pipeline
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
        // Here 72 PPI is assumed for MapPixel
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

        // tdf#141761 see comments above, IsEmpty() removed
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

        // tdf#141761 see comments above, IsEmpty() removed
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
    {
        return rPolySource;
    }

    const basegfx::B2DHomMatrix aTransform(::LogicToLogic(rMapModeSource, rMapModeDest));
    basegfx::B2DPolygon aPoly(rPolySource);

    aPoly.transform(aTransform);
    return aPoly;
}

basegfx::B2DHomMatrix LogicToLogic(const MapMode& rMapModeSource, const MapMode& rMapModeDest)
{
    basegfx::B2DHomMatrix aTransform;

    if (rMapModeSource == rMapModeDest)
    {
        return aTransform;
    }

    MapUnit eUnitSource = rMapModeSource.GetMapUnit();
    MapUnit eUnitDest = rMapModeDest.GetMapUnit();
    lcl_verifyUnitSourceDest(eUnitSource, eUnitDest);

    if (rMapModeSource.IsSimple() && rMapModeDest.IsSimple())
    {
        const auto[eFrom, eTo] = lcl_getCorrectedUnit(eUnitSource, eUnitDest);

        if (eFrom == o3tl::Length::invalid || eTo == o3tl::Length::invalid)
        {
            SAL_WARN("vcl.gdi", "CoordinateMapper: Invalid MapUnit conversion requested. Falling "
                                "back to identity matrix to prevent NaN poisoning.");
            return aTransform;
        }

        const double fScaleFactor = o3tl::convert(1.0, eFrom, eTo);
        aTransform.set(0, 0, fScaleFactor);
        aTransform.set(1, 1, fScaleFactor);

        return aTransform;
    }

    const auto[aMapResSource, aMapResDest] = lcl_calcConversionMapRes(rMapModeSource, rMapModeDest);

    // Guard against division by zero if MapResDest has an invalid scale
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

uint64_t CoordinateMapper::GetSemanticKey(bool bMap) const
{
    uint64_t epoch = mnStateVersion.load(std::memory_order_acquire);

    std::size_t h = 0;
    o3tl::hash_combine(h, epoch);
    o3tl::hash_combine(h, bMap);
    o3tl::hash_combine(h, mnDPIX);
    o3tl::hash_combine(h, mnDPIY);
    o3tl::hash_combine(h, mnDPIScalePercentage);

    return static_cast<uint64_t>(h);
}

CompiledTransform CoordinateMapper::Compile(bool bMap) const
{
    CompiledTransform t;
    t.mnSemanticKey = GetSemanticKey(bMap);

    const double fUIScale = static_cast<double>(mnDPIScalePercentage) / 100.0;
    const double scaleX = bMap ? (maMapRes.mfScaleX * static_cast<double>(mnDPIX) * fUIScale) : 1.0;
    const double scaleY = bMap ? (maMapRes.mfScaleY * static_cast<double>(mnDPIY) * fUIScale) : 1.0;

    const double logicOffsetX
        = bMap ? static_cast<double>(maMapRes.mnTranslationX + mnLogicToAbsoluteOffsetX) : 0.0;
    const double logicOffsetY
        = bMap ? static_cast<double>(maMapRes.mnTranslationY + mnLogicToAbsoluteOffsetY) : 0.0;

    const double txView = (logicOffsetX * scaleX) + static_cast<double>(mnWindowToViewOffsetX);
    const double tyView = (logicOffsetY * scaleY) + static_cast<double>(mnWindowToViewOffsetY);

    const double txDev = txView + static_cast<double>(mnDeviceToWindowOffsetX);
    const double tyDev = tyView + static_cast<double>(mnDeviceToWindowOffsetY);

    t.maMatrix.set(0, 0, scaleX);
    t.maMatrix.set(1, 1, scaleY);
    t.maMatrix.set(0, 2, txDev);
    t.maMatrix.set(1, 2, tyDev);

    if (t.maMatrix.isIdentity())
    {
        t.meMode = TransformMode::Identity;
        return t;
    }

    const bool bNoScaleOrShear
        = (maMapRes.mfScaleX == 1.0 && maMapRes.mfScaleY == 1.0 && mnDPIScalePercentage == 100);

    if (bNoScaleOrShear)
    {
        double dTx = t.maMatrix.get(0, 2);
        double dTy = t.maMatrix.get(1, 2);

        if (std::floor(dTx) == dTx && std::floor(dTy) == dTy)
        {
            t.mnDeviceTx = static_cast<tools::Long>(dTx);
            t.mnDeviceTy = static_cast<tools::Long>(dTy);

            if (t.mnDeviceTx == 0 && t.mnDeviceTy == 0)
            {
                t.meMode = TransformMode::Identity;
            }
            else
            {
                t.meMode = TransformMode::Translation;
            }
            return t;
        }
    }

    // Rational scaling extraction requires MapUnit which CoordinateMapper does not hold natively.
    // Matrix fallback elegantly guarantees exactness for scaled coordinates.
    t.meMode = TransformMode::AffineFallback;
    return t;
}

static inline Point ApplyTransform(const CompiledTransform& t, const Point& rPt)
{
#ifndef DBG_UTIL
    if (t.IsIdentity())
        [[likely]] return rPt;

    if (t.IsPureTranslation())
        [[likely]] return Point(rPt.X() + t.GetDeviceTx(), rPt.Y() + t.GetDeviceTy());

    basegfx::B2DPoint aB2DPt(rPt.X(), rPt.Y());
    aB2DPt *= t.GetMatrix();
    return Point(lcl_RoundToLong(aB2DPt.getX()), lcl_RoundToLong(aB2DPt.getY()));
#else
    basegfx::B2DPoint aB2DPt(rPt.X(), rPt.Y());
    aB2DPt *= t.GetMatrix();
    Point aSlow(lcl_RoundToLong(aB2DPt.getX()), lcl_RoundToLong(aB2DPt.getY()));

    Point aFast = aSlow;
    if (t.IsIdentity())
        aFast = rPt;
    else if (t.IsPureTranslation())
        aFast = Point(rPt.X() + t.GetDeviceTx(), rPt.Y() + t.GetDeviceTy());

    assert(aFast == aSlow && "CoordinateMapper FATAL: Fast path diverged from matrix truth!");
    return aFast;
#endif
}

basegfx::B2DHomMatrix CoordinateMapper::GetLogicToDeviceMatrix(bool bMap) const
{
    return Compile(bMap).GetMatrix();
}

basegfx::B2DHomMatrix CoordinateMapper::GetDeviceToLogicMatrix(bool bMap) const
{
    basegfx::B2DHomMatrix aMat = Compile(bMap).GetMatrix();
    aMat.invert();
    return aMat;
}

basegfx::B2DHomMatrix CoordinateMapper::GetLogicToWindowMatrix(bool bMap) const
{
    const double fUIScale = static_cast<double>(mnDPIScalePercentage) / 100.0;
    const double scaleX = bMap ? (maMapRes.mfScaleX * static_cast<double>(mnDPIX) * fUIScale) : 1.0;
    const double scaleY = bMap ? (maMapRes.mfScaleY * static_cast<double>(mnDPIY) * fUIScale) : 1.0;
    const double logicOffsetX
        = bMap ? static_cast<double>(maMapRes.mnTranslationX + mnLogicToAbsoluteOffsetX) : 0.0;
    const double logicOffsetY
        = bMap ? static_cast<double>(maMapRes.mnTranslationY + mnLogicToAbsoluteOffsetY) : 0.0;

    basegfx::B2DHomMatrix aMat;
    aMat.set(0, 0, scaleX);
    aMat.set(1, 1, scaleY);
    aMat.set(0, 2, (logicOffsetX * scaleX) + mnWindowToViewOffsetX);
    aMat.set(1, 2, (logicOffsetY * scaleY) + mnWindowToViewOffsetY);
    return aMat;
}

basegfx::B2DHomMatrix CoordinateMapper::GetWindowToLogicMatrix(bool bMap) const
{
    basegfx::B2DHomMatrix aMat = GetLogicToWindowMatrix(bMap);
    aMat.invert();
    return aMat;
}

Point CoordinateMapper::LogicToDevicePixel(const Point& rLogicPt, bool bMap) const
{
    return ApplyTransform(Compile(bMap), rLogicPt);
}

tools::Rectangle CoordinateMapper::LogicToDevicePixel(const tools::Rectangle& rLogicRect,
                                                      bool bMap) const
{
    CompiledTransform t = Compile(bMap);
    return tools::Rectangle(ApplyTransform(t, rLogicRect.TopLeft()),
                            ApplyTransform(t, rLogicRect.BottomRight()));
}

tools::Polygon CoordinateMapper::LogicToDevicePixel(const tools::Polygon& rLogicPoly,
                                                    bool bMap) const
{
    CompiledTransform t = Compile(bMap);

    if (!t.IsPureTranslation() && !t.IsIdentity())
    {
        basegfx::B2DPolygon aB2DPoly(rLogicPoly.getB2DPolygon());
        aB2DPoly.transform(t.GetMatrix());
        return tools::Polygon(aB2DPoly);
    }

    tools::Polygon aPoly(rLogicPoly);
    for (sal_uInt16 i = 0; i < aPoly.GetSize(); ++i)
    {
        aPoly[i] = ApplyTransform(t, aPoly[i]);
    }

    return aPoly;
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
