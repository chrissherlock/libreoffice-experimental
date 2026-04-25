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
#include <basegfx/vector/b2dvector.hxx>
#include <basegfx/range/b2drectangle.hxx>
#include <basegfx/polygon/b2dpolygon.hxx>
#include <basegfx/polygon/b2dpolypolygon.hxx>
#include <tools/bigint.hxx>
#include <tools/debug.hxx>
#include <tools/gen.hxx>
#include <tools/mapunit.hxx>

#include <vcl/lineinfo.hxx>
#include <vcl/rendercontext/ImplMapRes.hxx>

#include <CoordinateMapper.hxx>

#include <cmath>
#include <cassert>

// Conceptual Pipeline Separation (Mathematical Invariant):
// Logic -> View: Scaled transformations (Scale * Logic) + Scaled Offsets ((MapOfs + LogicOfs) * Scale)
// View -> Window: Pure translation (WindowOfs)
// Window -> Device: Pure translation (DeviceOfs)
// We explicitly isolate the scaled offsets (fTrans) from unscaled offsets here.

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

Size CoordinateMapper::LogicToViewDistance(const Size& rLogicSize) const
{
    if (!IsMappingActive())
        return rLogicSize;

    return Size(LogicToViewDistanceX(rLogicSize.Width()),
                LogicToViewDistanceY(rLogicSize.Height()));
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
    maMapRes.CalcMapResolution(rMapMode, nDPIX, nDPIY);
    InvalidateViewTransform();
}

ImplMapRes CoordinateMapper::ResolveMapRes(const MapMode* pMode) const
{
    return maMapRes.ResolveMapRes(pMode, maMapMode, mbMap, mnDPIX, mnDPIY);
}

// #i75163#
void CoordinateMapper::InvalidateViewTransform()
{
    maLogicToDevice.reset();
    maDeviceToLogic.reset();
}

basegfx::B2DHomMatrix CoordinateMapper::GetDeviceTransformation() const
{
    UpdateTransforms();
    return *maLogicToDevice;
}

void CoordinateMapper::UpdateTransforms() const
{
    if (maLogicToDevice)
        return; // Already cached

    basegfx::B2DHomMatrix aTransform;

    if (IsMappingActive())
    {
        // 1. Scale
        double fScaleX = static_cast<double>(mnDPIX) * maMapRes.mfMapScX;
        double fScaleY = static_cast<double>(mnDPIY) * maMapRes.mfMapScY;

        // 2. Translation (Combine all offsets into a single translation vector)
        // Note: Logical offsets are scaled. Physical offsets are not.
        double fTransX = (static_cast<double>(maMapRes.mnMapOfsX)
                          + static_cast<double>(mnLogicToAbsoluteOffsetX))
                             * fScaleX
                         + static_cast<double>(mnWindowToViewOffsetX)
                         + static_cast<double>(mnDeviceToWindowOffsetX);

        double fTransY = (static_cast<double>(maMapRes.mnMapOfsY)
                          + static_cast<double>(mnLogicToAbsoluteOffsetY))
                             * fScaleY
                         + static_cast<double>(mnWindowToViewOffsetY)
                         + static_cast<double>(mnDeviceToWindowOffsetY);

        aTransform.set(0, 0, fScaleX);
        aTransform.set(1, 1, fScaleY);
        aTransform.set(0, 2, fTransX);
        aTransform.set(1, 2, fTransY);
    }
    else
    {
        // Identity fallback for non-mapped, but screen offsets ALWAYS apply
        aTransform.translate(static_cast<double>(mnWindowToViewOffsetX + mnDeviceToWindowOffsetX),
                             static_cast<double>(mnWindowToViewOffsetY + mnDeviceToWindowOffsetY));
    }

    maLogicToDevice = aTransform;

    basegfx::B2DHomMatrix aInverse = aTransform;
    aInverse.invert();
    maDeviceToLogic = aInverse;
}

basegfx::B2DHomMatrix CoordinateMapper::GetViewTransformation() const
{
    // Maintain legacy behavior: inactive mapping returns a pure identity matrix
    if (!IsMappingActive())
        return basegfx::B2DHomMatrix();

    // Ensure our Single Source of Truth is built
    UpdateTransforms();
    basegfx::B2DHomMatrix aTransform = *maLogicToDevice;

    // maLogicToDevice includes physical screen offsets (Device space).
    // View space conceptually does not, so we subtract them back out.
    if (mnDeviceToWindowOffsetX != 0 || mnDeviceToWindowOffsetY != 0)
    {
        aTransform.translate(-static_cast<double>(mnDeviceToWindowOffsetX),
                             -static_cast<double>(mnDeviceToWindowOffsetY));
    }

    return aTransform;
}

basegfx::B2DHomMatrix CoordinateMapper::GetInverseViewTransformation() const
{
    basegfx::B2DHomMatrix aInverse(GetViewTransformation());
    aInverse.invert();
    return aInverse;
}

basegfx::B2DHomMatrix CoordinateMapper::GetViewTransformation(const MapMode& rMapMode) const
{
    ImplMapRes aMapRes(rMapMode, GetDPIX(), GetDPIY());
    basegfx::B2DHomMatrix aTransform;

    const double fScaleFactorX = static_cast<double>(GetDPIX()) * aMapRes.mfMapScX;
    const double fScaleFactorY = static_cast<double>(GetDPIY()) * aMapRes.mfMapScY;

    const double fZeroPointX
        = (static_cast<double>(aMapRes.mnMapOfsX) + static_cast<double>(mnLogicToAbsoluteOffsetX))
              * fScaleFactorX
          + static_cast<double>(GetWindowToViewOffsetX());
    const double fZeroPointY
        = (static_cast<double>(aMapRes.mnMapOfsY) + static_cast<double>(mnLogicToAbsoluteOffsetY))
              * fScaleFactorY
          + static_cast<double>(GetWindowToViewOffsetY());

    aTransform.set(0, 0, fScaleFactorX);
    aTransform.set(1, 1, fScaleFactorY);
    aTransform.set(0, 2, fZeroPointX);
    aTransform.set(1, 2, fZeroPointY);

    return aTransform;
}

basegfx::B2DHomMatrix CoordinateMapper::GetInverseViewTransformation(const MapMode& rMapMode) const
{
    basegfx::B2DHomMatrix aMatrix(GetViewTransformation(rMapMode));
    aMatrix.invert();
    return aMatrix;
}

// ========================================================================
// PIPELINE STAGES (Coordinate Transitions)
// ========================================================================

static inline tools::Long lcl_RoundToLong(double fVal)
{
    return static_cast<tools::Long>(std::llround(fVal));
}

// Centralized rounding policy for final device pixel boundaries
static inline tools::Long lcl_ToDevicePixel(double fVal)
{
    return static_cast<tools::Long>(std::llround(fVal));
}

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
double CoordinateMapper::ViewSubPixelToLogicUnitsX(double fX) const
{
    if (!IsMappingActive() || maMapRes.mfMapScX == 0.0)
        return fX;

    return (fX / (maMapRes.mfMapScX * mnDPIX)) - static_cast<double>(maMapRes.mnMapOfsX);
}

double CoordinateMapper::ViewSubPixelToLogicUnitsY(double fY) const
{
    if (!IsMappingActive() || maMapRes.mfMapScY == 0.0)
        return fY;

    return (fY / (maMapRes.mfMapScY * mnDPIY)) - static_cast<double>(maMapRes.mnMapOfsY);
}

double CoordinateMapper::LogicUnitsToViewSubPixelX(double fX) const
{
    if (!IsMappingActive())
        return fX;

    return (fX + static_cast<double>(maMapRes.mnMapOfsX)) * maMapRes.mfMapScX * mnDPIX;
}

double CoordinateMapper::LogicUnitsToViewSubPixelY(double fY) const
{
    if (!IsMappingActive())
        return fY;

    return (fY + static_cast<double>(maMapRes.mnMapOfsY)) * maMapRes.mfMapScY * mnDPIY;
}

// View <-> LogicUnits (Scale and Mapping Offset: mnMapOfsX/Y)
tools::Long CoordinateMapper::ViewToLogicUnitsX(tools::Long nX) const
{
    return ViewToLogicDistanceX(nX) - maMapRes.mnMapOfsX;
}

tools::Long CoordinateMapper::ViewToLogicUnitsY(tools::Long nY) const
{
    return ViewToLogicDistanceY(nY) - maMapRes.mnMapOfsY;
}

tools::Long CoordinateMapper::LogicUnitsToViewUnitsX(tools::Long nX) const
{
    return LogicToViewDistanceX(nX + maMapRes.mnMapOfsX);
}

tools::Long CoordinateMapper::LogicUnitsToViewUnitsY(tools::Long nY) const
{
    return LogicToViewDistanceY(nY + maMapRes.mnMapOfsY);
}

tools::Long CoordinateMapper::LogicUnitsToViewUnitsX(tools::Long nX, const ImplMapRes& rRes) const
{
    return LogicToViewDistanceX(nX + rRes.mnMapOfsX, rRes.mfMapScX);
}

tools::Long CoordinateMapper::LogicUnitsToViewUnitsY(tools::Long nY, const ImplMapRes& rRes) const
{
    return LogicToViewDistanceY(nY + rRes.mnMapOfsY, rRes.mfMapScY);
}

tools::Long CoordinateMapper::ViewSubPixelToLogicUnitsIntX(double fX) const
{
    // Fix: Round only at the end of the full pipeline stage
    return lcl_RoundToLong(ViewSubPixelToLogicUnitsX(fX));
}

tools::Long CoordinateMapper::ViewSubPixelToLogicUnitsIntY(double fY) const
{
    // Fix: Round only at the end of the full pipeline stage
    return lcl_RoundToLong(ViewSubPixelToLogicUnitsY(fY));
}

tools::Long CoordinateMapper::ViewSubPixelToLogicIntX(double fX, const ImplMapRes& rRes) const
{
    // Use pure double distance, apply offsets, and round the final result
    double fLogicDist = ViewToLogicDistanceDoubleX(fX, rRes.mfMapScX);
    return lcl_RoundToLong(fLogicDist - static_cast<double>(rRes.mnMapOfsX)
                           - static_cast<double>(mnLogicToAbsoluteOffsetX));
}

tools::Long CoordinateMapper::ViewSubPixelToLogicIntY(double fY, const ImplMapRes& rRes) const
{
    // Use pure double distance, apply offsets, and round the final result
    double fLogicDist = ViewToLogicDistanceDoubleY(fY, rRes.mfMapScY);
    return lcl_RoundToLong(fLogicDist - static_cast<double>(rRes.mnMapOfsY)
                           - static_cast<double>(mnLogicToAbsoluteOffsetY));
}

// ========================================================================
// MASTER WRAPPERS (Multi-space Positional Transformations)
// ========================================================================

double CoordinateMapper::WindowToLogicSubPixelX(double fX) const
{
    if (!IsMappingActive())
        return fX;

    double fView = WindowToViewSubPixelX(fX);

    return ViewSubPixelToLogicUnitsX(fView) - static_cast<double>(mnLogicToAbsoluteOffsetX);
}

double CoordinateMapper::WindowToLogicSubPixelY(double fY) const
{
    if (!IsMappingActive())
        return fY;

    double fView = WindowToViewSubPixelY(fY);

    return ViewSubPixelToLogicUnitsY(fView) - static_cast<double>(mnLogicToAbsoluteOffsetY);
}

tools::Long CoordinateMapper::WindowToLogicX(tools::Long nX) const
{
    return lcl_RoundToLong(WindowToLogicSubPixelX(static_cast<double>(nX)));
}

tools::Long CoordinateMapper::WindowToLogicY(tools::Long nY) const
{
    return lcl_RoundToLong(WindowToLogicSubPixelY(static_cast<double>(nY)));
}

Size CoordinateMapper::LogicToWindowUnits(const Size& rLogicSize) const
{
    if (!IsMappingActive())
        return rLogicSize;

    // Distances ignore offsets, so Window == View
    return Size(LogicToViewDistanceX(rLogicSize.Width()),
                LogicToViewDistanceY(rLogicSize.Height()));
}

// --- Sub-Pixel Full Journey ---
double CoordinateMapper::LogicToDeviceSubPixelX(double fX) const
{
    UpdateTransforms();
    return (fX * maLogicToDevice->get(0, 0)) + maLogicToDevice->get(0, 2);
}

double CoordinateMapper::LogicToDeviceSubPixelY(double fY) const
{
    UpdateTransforms();
    return (fY * maLogicToDevice->get(1, 1)) + maLogicToDevice->get(1, 2);
}

double CoordinateMapper::DevicePixelToLogicSubPixelX(double fX) const
{
    UpdateTransforms();
    return (fX * maDeviceToLogic->get(0, 0)) + maDeviceToLogic->get(0, 2);
}

double CoordinateMapper::DevicePixelToLogicSubPixelY(double fY) const
{
    UpdateTransforms();
    return (fY * maDeviceToLogic->get(1, 1)) + maDeviceToLogic->get(1, 2);
}

basegfx::B2DPoint CoordinateMapper::LogicToDeviceSubPixel(const Point& rPoint) const
{
    UpdateTransforms();
    basegfx::B2DPoint aPt(rPoint.X(), rPoint.Y());
    aPt *= *maLogicToDevice;
    return aPt;
}
basegfx::B2DPoint CoordinateMapper::DevicePixelToLogicSubPixel(const Point& rPoint) const
{
    UpdateTransforms();
    basegfx::B2DPoint aPt(rPoint.X(), rPoint.Y());
    aPt *= *maDeviceToLogic;
    return aPt;
}

// Integer Boundary (this is the only place rounding occurs)

tools::Long CoordinateMapper::DevicePixelToLogicX(tools::Long nX) const
{
    return lcl_RoundToLong(DevicePixelToLogicSubPixelX(static_cast<double>(nX)));
}

tools::Long CoordinateMapper::DevicePixelToLogicY(tools::Long nY) const
{
    return lcl_RoundToLong(DevicePixelToLogicSubPixelY(static_cast<double>(nY)));
}

Point CoordinateMapper::DevicePixelToLogic(const Point& rDevicePt) const
{
    return Point(DevicePixelToLogicX(rDevicePt.X()), DevicePixelToLogicY(rDevicePt.Y()));
}

tools::Rectangle CoordinateMapper::DevicePixelToLogic(const tools::Rectangle& rPixelRect) const
{
    tools::Rectangle aRetval(
        DevicePixelToLogicX(rPixelRect.Left()), DevicePixelToLogicY(rPixelRect.Top()),
        rPixelRect.IsWidthEmpty() ? 0 : DevicePixelToLogicX(rPixelRect.Right()),
        rPixelRect.IsHeightEmpty() ? 0 : DevicePixelToLogicY(rPixelRect.Bottom()));

    if (rPixelRect.IsWidthEmpty())
        aRetval.SetWidthEmpty();
    if (rPixelRect.IsHeightEmpty())
        aRetval.SetHeightEmpty();
    return aRetval;
}

tools::Long CoordinateMapper::LogicToDevicePixelX(tools::Long nX) const
{
    if (!IsMappingActive())
        return nX + GetDeviceToWindowOffsetX();

    UpdateTransforms();
    double fX = (static_cast<double>(nX) * maLogicToDevice->get(0, 0)) + maLogicToDevice->get(0, 2);

    tools::Long nRes = lcl_ToDevicePixel(fX);

    return nRes;
}

tools::Long CoordinateMapper::LogicToDevicePixelY(tools::Long nY) const
{
    return lcl_RoundToLong(LogicToDeviceSubPixelY(static_cast<double>(nY)));
}

Point CoordinateMapper::LogicToDevicePixel(const Point& rLogicPt) const
{
    if (!IsMappingActive() && !GetDeviceToWindowOffsetX() && !GetDeviceToWindowOffsetY())
        return rLogicPt;

    UpdateTransforms();
    basegfx::B2DPoint aPt(rLogicPt.X(), rLogicPt.Y());
    aPt *= *maLogicToDevice;
    return Point(lcl_ToDevicePixel(aPt.getX()), lcl_ToDevicePixel(aPt.getY()));
}

// Note: Width/Height use Distances, not Positions!
double CoordinateMapper::LogicWidthToDeviceSubPixel(tools::Long nWidth) const
{
    if (!IsMappingActive())
        return static_cast<double>(nWidth);
    return LogicToViewDistanceSubPixelX(nWidth);
}

tools::Long CoordinateMapper::LogicWidthToDevicePixel(tools::Long nWidth) const
{
    if (!IsMappingActive())
        return nWidth;

    UpdateTransforms();
    double fW = static_cast<double>(nWidth) * maLogicToDevice->get(0, 0);
    tools::Long nRes = lcl_ToDevicePixel(std::abs(fW));
    return nRes;
}

tools::Long CoordinateMapper::LogicHeightToDevicePixel(tools::Long nHeight) const
{
    if (!IsMappingActive())
        return nHeight;
    return LogicToViewDistanceY(nHeight);
}

Size CoordinateMapper::LogicToDevicePixel(const Size& rLogicSize) const
{
    if (!IsMappingActive())
        return rLogicSize;

    UpdateTransforms();
    basegfx::B2DVector aVec(rLogicSize.Width(), rLogicSize.Height());
    aVec *= *maLogicToDevice;
    return Size(lcl_ToDevicePixel(aVec.getX()), lcl_ToDevicePixel(aVec.getY()));
}

tools::Rectangle CoordinateMapper::LogicToDevicePixel(const tools::Rectangle& rLogicRect) const
{
    // Fast path: no mapping active, no offsets → identity
    if (!IsMappingActive() && !GetDeviceToWindowOffsetX() && !GetDeviceToWindowOffsetY())
        return rLogicRect;

    UpdateTransforms();

    // Treat rectangle as geometric range (continuous space)
    basegfx::B2DRange aRange(rLogicRect.Left(), rLogicRect.Top(), rLogicRect.Right(),
                             rLogicRect.Bottom());

    // Apply full affine transform
    aRange.transform(*maLogicToDevice);

    // Extract transformed bounding box
    const double fMinX = aRange.getMinX();
    const double fMinY = aRange.getMinY();
    const double fMaxX = aRange.getMaxX();
    const double fMaxY = aRange.getMaxY();

    // Single, consistent rounding policy at the final boundary
    const tools::Long nL = lcl_RoundToLong(fMinX);
    const tools::Long nT = lcl_RoundToLong(fMinY);
    const tools::Long nR = lcl_RoundToLong(fMaxX);
    const tools::Long nB = lcl_RoundToLong(fMaxY);

    tools::Rectangle aRetval(nL, nT, nR, nB);

    // Preserve semantic flags
    if (rLogicRect.IsWidthEmpty())
        aRetval.SetWidthEmpty();

    if (rLogicRect.IsHeightEmpty())
        aRetval.SetHeightEmpty();

    return aRetval;
}

tools::Polygon CoordinateMapper::LogicToDevicePixel(const tools::Polygon& rLogicPoly) const
{
    if (!IsMappingActive() && !GetDeviceToWindowOffsetX() && !GetDeviceToWindowOffsetY())
        return rLogicPoly;

    UpdateTransforms();
    const basegfx::B2DHomMatrix& rMat = *maLogicToDevice;

    tools::Polygon aPoly(rLogicPoly);

    // Transform points directly to avoid hidden recursive coupling with the Point overload
    for (auto& rPoint : aPoly)
    {
        basegfx::B2DPoint aPt(rPoint.X(), rPoint.Y());
        aPt *= rMat;
        rPoint = Point(lcl_ToDevicePixel(aPt.getX()), lcl_ToDevicePixel(aPt.getY()));
    }

    return aPoly;
}

tools::PolyPolygon
CoordinateMapper::LogicToDevicePixel(const tools::PolyPolygon& rLogicPolyPoly) const
{
    if (!IsMappingActive() && !GetDeviceToWindowOffsetX() && !GetDeviceToWindowOffsetY())
        return rLogicPolyPoly;

    tools::PolyPolygon aPolyPoly(rLogicPolyPoly);

    for (auto& rPoly : aPolyPoly)
    {
        rPoly = LogicToDevicePixel(rPoly);
    }

    return aPolyPoly;
}

LineInfo CoordinateMapper::LogicToDevicePixel(const LineInfo& rLineInfo) const
{
    LineInfo aInfo(rLineInfo);

    if (aInfo.GetStyle() == LineStyle::Dash)
    {
        if (aInfo.GetDotCount() && aInfo.GetDotLen())
            aInfo.SetDotLen(std::max(LogicWidthToDevicePixel(aInfo.GetDotLen()), tools::Long(1)));
        else
            aInfo.SetDotCount(0);

        if (aInfo.GetDashCount() && aInfo.GetDashLen())
            aInfo.SetDashLen(std::max(LogicWidthToDevicePixel(aInfo.GetDashLen()), tools::Long(1)));
        else
            aInfo.SetDashCount(0);

        aInfo.SetDistance(LogicWidthToDevicePixel(aInfo.GetDistance()));

        if ((!aInfo.GetDashCount() && !aInfo.GetDotCount()) || !aInfo.GetDistance())
            aInfo.SetStyle(LineStyle::Solid);
    }

    aInfo.SetWidth(LogicWidthToDevicePixel(aInfo.GetWidth()));

    return aInfo;
}

basegfx::B2DPolygon
CoordinateMapper::LogicToDevicePixel(const basegfx::B2DPolygon& rLogicPoly) const
{
    if (!IsMappingActive() && !GetDeviceToWindowOffsetX() && !GetDeviceToWindowOffsetY())
        return rLogicPoly;

    const sal_uInt32 nPoints = rLogicPoly.count();
    basegfx::B2DPolygon aPoly(rLogicPoly);

    for (sal_uInt32 i = 0; i < nPoints; ++i)
    {
        const basegfx::B2DPoint& rPt = aPoly.getB2DPoint(i);

        // FIX: Use SubPixel methods to prevent double -> Long -> double truncation!
        aPoly.setB2DPoint(i, basegfx::B2DPoint(LogicToDeviceSubPixelX(rPt.getX()),
                                               LogicToDeviceSubPixelY(rPt.getY())));

        if (aPoly.isPrevControlPointUsed(i))
        {
            const basegfx::B2DPoint aB2DC1(aPoly.getPrevControlPoint(i));
            aPoly.setPrevControlPoint(i, basegfx::B2DPoint(LogicToDeviceSubPixelX(aB2DC1.getX()),
                                                           LogicToDeviceSubPixelY(aB2DC1.getY())));
        }

        if (aPoly.isNextControlPointUsed(i))
        {
            const basegfx::B2DPoint aB2DC2(aPoly.getNextControlPoint(i));
            aPoly.setNextControlPoint(i, basegfx::B2DPoint(LogicToDeviceSubPixelX(aB2DC2.getX()),
                                                           LogicToDeviceSubPixelY(aB2DC2.getY())));
        }
    }

    return aPoly;
}

tools::Long CoordinateMapper::LogicToWindowX(tools::Long nX) const
{
    if (!IsMappingActive())
        return nX;

    return ViewToWindowUnitsX(LogicUnitsToViewUnitsX(nX + mnLogicToAbsoluteOffsetX));
}

tools::Long CoordinateMapper::LogicToWindowY(tools::Long nY) const
{
    if (!IsMappingActive())
        return nY;

    return ViewToWindowUnitsY(LogicUnitsToViewUnitsY(nY + mnLogicToAbsoluteOffsetY));
}

basegfx::B2DPolyPolygon
CoordinateMapper::LogicToWindowUnits(const basegfx::B2DPolyPolygon& rLogicPolyPoly) const
{
    basegfx::B2DPolyPolygon aTransformedPoly = rLogicPolyPoly;
    // This encapsulates the GetViewTransformation() logic inside the mapper
    aTransformedPoly.transform(GetViewTransformation());
    return aTransformedPoly;
}

tools::Long CoordinateMapper::ViewToLogicX(tools::Long nX) const
{
    return ViewToLogicUnitsX(nX) - mnLogicToAbsoluteOffsetX;
}

tools::Long CoordinateMapper::ViewToLogicY(tools::Long nY) const
{
    return ViewToLogicUnitsY(nY) - mnLogicToAbsoluteOffsetY;
}

tools::Long CoordinateMapper::LogicToWindowUnitsX(tools::Long nX) const
{
    if (!IsMappingActive())
        return nX;
    return ViewToWindowUnitsX(LogicUnitsToViewUnitsX(nX));
}

tools::Long CoordinateMapper::LogicToWindowUnitsY(tools::Long nY) const
{
    if (!IsMappingActive())
        return nY;
    return ViewToWindowUnitsY(LogicUnitsToViewUnitsY(nY));
}

tools::Long CoordinateMapper::LogicToWindowUnitsX(tools::Long nX, const ImplMapRes& rRes) const
{
    return ViewToWindowUnitsX(LogicUnitsToViewUnitsX(nX, rRes));
}

tools::Long CoordinateMapper::LogicToWindowUnitsY(tools::Long nY, const ImplMapRes& rRes) const
{
    return ViewToWindowUnitsY(LogicUnitsToViewUnitsY(nY, rRes));
}

Point CoordinateMapper::LogicToWindowUnits(const Point& rLogicPt) const
{
    if (!IsMappingActive())
        return rLogicPt;

    return Point(LogicToWindowUnitsX(rLogicPt.X()), LogicToWindowUnitsY(rLogicPt.Y()));
}

Point CoordinateMapper::LogicToWindowUnits(const Point& rLogicPt, const MapMode& rMapMode) const
{
    if (rMapMode.IsDefault())
        return rLogicPt;

    ImplMapRes aMapRes(rMapMode, GetDPIX(), GetDPIY());
    return LogicToWindowUnits(rLogicPt, aMapRes);
}

Point CoordinateMapper::LogicToWindowUnits(const Point& rLogicPt, const ImplMapRes& rRes) const
{
    return Point(LogicToWindowUnitsX(rLogicPt.X(), rRes), LogicToWindowUnitsY(rLogicPt.Y(), rRes));
}

Size CoordinateMapper::LogicToWindowUnits(const Size& rLogicSize, const MapMode& rMapMode) const
{
    if (rMapMode.IsDefault())
        return rLogicSize;

    ImplMapRes aMapRes(rMapMode, GetDPIX(), GetDPIY());
    return LogicToWindowUnits(rLogicSize, aMapRes);
}

Size CoordinateMapper::LogicToWindowUnits(const Size& rLogicSize, const ImplMapRes& rRes) const
{
    return Size(LogicToViewDistanceX(rLogicSize.Width(), rRes.mfMapScX),
                LogicToViewDistanceY(rLogicSize.Height(), rRes.mfMapScY));
}

static void lcl_ApplyEmptyState(tools::Rectangle& rDest, const tools::Rectangle& rSrc)
{
    // tdf#141761 IsEmpty() removed
    // Even if rLogicRect.IsEmpty(), transform of the Position contained
    // in the Rectangle is necessary. Due to Rectangle::Right() returning
    // Left() when IsEmpty(), the code *could* stay unchanged (same for Bottom),
    // but:
    // The Rectangle constructor used with the four tools::Long values does not
    // check for IsEmpty(), so to keep that state correct there are two possibilities:
    // (1) Add a test to the Rectangle constructor in question
    // (2) Do it by hand here
    // I have tried (1) first, but test Test::test_rectangle() claims that for
    //  tools::Rectangle aRect(1, 1, 1, 1);
    //    tools::Long(1) == aRect.GetWidth()
    //    tools::Long(0) == aRect.getWidth()
    // (remember: this means Left == Right == 1 -> GetWidth => 1, getWidth == 0)
    // so indeed the 1's have to go uncommented/unchecked into the data body
    // of rectangle. Switching to (2) *is* needed, doing so

    if (rSrc.IsWidthEmpty())
        rDest.SetWidthEmpty();

    if (rSrc.IsHeightEmpty())
        rDest.SetHeightEmpty();
}

tools::Rectangle CoordinateMapper::LogicToWindowUnits(const tools::Rectangle& rRect) const
{
    if (!IsMappingActive())
        return rRect;

    tools::Rectangle aRetval(LogicToWindowUnitsX(rRect.Left()), LogicToWindowUnitsY(rRect.Top()),
                             rRect.IsWidthEmpty() ? 0 : LogicToWindowUnitsX(rRect.Right()),
                             rRect.IsHeightEmpty() ? 0 : LogicToWindowUnitsY(rRect.Bottom()));

    lcl_ApplyEmptyState(aRetval, rRect);

    return aRetval;
}

tools::Rectangle CoordinateMapper::LogicToWindowUnits(const tools::Rectangle& rLogicRect,
                                                      const MapMode& rMapMode) const
{
    if (rMapMode.IsDefault())
        return rLogicRect;

    ImplMapRes aMapRes(rMapMode, GetDPIX(), GetDPIY());
    return LogicToWindowUnits(rLogicRect, aMapRes);
}

tools::Rectangle CoordinateMapper::LogicToWindowUnits(const tools::Rectangle& rLogicRect,
                                                      const ImplMapRes& rRes) const
{
    tools::Rectangle aRetval(
        LogicToWindowUnitsX(rLogicRect.Left(), rRes), LogicToWindowUnitsY(rLogicRect.Top(), rRes),
        rLogicRect.IsWidthEmpty() ? 0 : LogicToWindowUnitsX(rLogicRect.Right(), rRes),
        rLogicRect.IsHeightEmpty() ? 0 : LogicToWindowUnitsY(rLogicRect.Bottom(), rRes));

    lcl_ApplyEmptyState(aRetval, rLogicRect);

    return aRetval;
}

tools::Polygon CoordinateMapper::LogicToWindowUnits(const tools::Polygon& rLogicPoly,
                                                    const MapMode& rMapMode) const
{
    if (rMapMode.IsDefault())
        return rLogicPoly;

    ImplMapRes aMapRes(rMapMode, GetDPIX(), GetDPIY());
    return LogicToWindowUnits(rLogicPoly, aMapRes);
}

tools::Polygon CoordinateMapper::LogicToWindowUnits(const tools::Polygon& rLogicPoly,
                                                    const ImplMapRes& rRes) const
{
    tools::Polygon aPoly(rLogicPoly);

    for (auto& rPoint : aPoly)
    {
        rPoint.setX(LogicToWindowUnitsX(rPoint.X(), rRes));
        rPoint.setY(LogicToWindowUnitsY(rPoint.Y(), rRes));
    }

    return aPoly;
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

        // Reverse run to fill new region bottom-up for speed
        for (auto aRectIter = aRectangles.rbegin(); aRectIter != aRectangles.rend(); ++aRectIter)
        {
            aRegion.Union(func(*aRectIter));
        }
    }
    return aRegion;
}

vcl::Region CoordinateMapper::LogicToWindowUnits(const vcl::Region& rLogicRegion) const
{
    if (!IsMappingActive())
        return rLogicRegion;

    return lcl_TransformRegion(rLogicRegion,
                               [this](const auto& obj) { return LogicToWindowUnits(obj); });
}

tools::Polygon CoordinateMapper::LogicToWindowUnits(const tools::Polygon& rPoly) const
{
    if (!IsMappingActive())
        return rPoly;

    tools::Polygon aPoly(rPoly);

    for (auto& rPoint : aPoly)
    {
        rPoint.setX(LogicToWindowUnitsX(rPoint.X()));
        rPoint.setY(LogicToWindowUnitsY(rPoint.Y()));
    }

    return aPoly;
}

tools::PolyPolygon CoordinateMapper::LogicToWindowUnits(const tools::PolyPolygon& rPolyPoly) const
{
    if (!IsMappingActive())
        return rPolyPoly;

    tools::PolyPolygon aPolyPoly(rPolyPoly);

    for (auto& rPoly : aPolyPoly)
    {
        rPoly = LogicToWindowUnits(rPoly);
    }

    return aPolyPoly;
}

double CoordinateMapper::LogicWidthToWindowSubPixel(tools::Long nWidth) const
{
    if (!IsMappingActive())
        return nWidth;

    // View distance == Window distance, so this is perfectly safe
    return LogicToViewDistanceSubPixelX(nWidth);
}

double CoordinateMapper::LogicHeightToWindowSubPixel(tools::Long nHeight) const
{
    if (!IsMappingActive())
        return nHeight;

    return LogicToViewDistanceSubPixelY(nHeight);
}

basegfx::B2DPolyPolygon
CoordinateMapper::LogicToWindowUnits(const basegfx::B2DPolyPolygon& rLogicPolyPoly,
                                     const MapMode& rMapMode) const
{
    basegfx::B2DPolyPolygon aTransformedPoly = rLogicPolyPoly;
    const basegfx::B2DHomMatrix aTransformationMatrix = GetViewTransformation(rMapMode);
    aTransformedPoly.transform(aTransformationMatrix);
    return aTransformedPoly;
}

vcl::Region CoordinateMapper::WindowToLogicUnits(const vcl::Region& rWindowRegion) const
{
    if (!IsMappingActive())
        return rWindowRegion;

    return lcl_TransformRegion(rWindowRegion,
                               [this](const auto& obj) { return WindowToLogicUnits(obj); });
}

Point CoordinateMapper::WindowToLogicUnits(const Point& rWindowPt) const
{
    if (!IsMappingActive())
        return rWindowPt;

    return Point(WindowToLogicX(rWindowPt.X()), WindowToLogicY(rWindowPt.Y()));
}

tools::Long CoordinateMapper::WindowSubPixelToLogicIntX(double fX) const
{
    return lcl_RoundToLong(WindowToLogicSubPixelX(fX));
}

tools::Long CoordinateMapper::WindowSubPixelToLogicIntY(double fY) const
{
    return lcl_RoundToLong(WindowToLogicSubPixelY(fY));
}

tools::Long CoordinateMapper::WindowSubPixelToLogicIntX(double fX, const ImplMapRes& rMapRes) const
{
    return ViewSubPixelToLogicIntX(WindowToViewSubPixelX(fX), rMapRes);
}

tools::Long CoordinateMapper::WindowSubPixelToLogicIntY(double fY, const ImplMapRes& rMapRes) const
{
    return ViewSubPixelToLogicIntY(WindowToViewSubPixelY(fY), rMapRes);
}

Point CoordinateMapper::WindowSubPixelToLogicUnits(const basegfx::B2DPoint& rWindowPt) const
{
    if (!IsMappingActive())
    {
        assert(std::floor(rWindowPt.getX()) == rWindowPt.getX()
               && std::floor(rWindowPt.getY()) == rWindowPt.getY());
        return Point(rWindowPt.getX(), rWindowPt.getY());
    }

    return Point(WindowSubPixelToLogicIntX(rWindowPt.getX()),
                 WindowSubPixelToLogicIntY(rWindowPt.getY()));
}

tools::Rectangle CoordinateMapper::WindowToLogicUnits(const tools::Rectangle& rWindowRect) const
{
    if (!IsMappingActive())
        return rWindowRect;

    tools::Rectangle aRetval(
        WindowSubPixelToLogicIntX(rWindowRect.Left()), WindowSubPixelToLogicIntY(rWindowRect.Top()),
        rWindowRect.IsWidthEmpty() ? 0 : WindowSubPixelToLogicIntX(rWindowRect.Right()),
        rWindowRect.IsHeightEmpty() ? 0 : WindowSubPixelToLogicIntY(rWindowRect.Bottom()));

    lcl_ApplyEmptyState(aRetval, rWindowRect);

    return aRetval;
}

tools::Polygon CoordinateMapper::WindowToLogicUnits(const tools::Polygon& rWindowPoly) const
{
    if (!IsMappingActive())
        return rWindowPoly;

    tools::Polygon aPoly(rWindowPoly);

    for (auto& rPoint : aPoly)
    {
        rPoint
            = Point(WindowSubPixelToLogicIntX(rPoint.X()), WindowSubPixelToLogicIntY(rPoint.Y()));
    }

    return aPoly;
}

tools::Polygon CoordinateMapper::WindowToLogicUnits(const tools::Polygon& rWindowPoly,
                                                    const ImplMapRes& rMapRes) const
{
    tools::Polygon aPoly(rWindowPoly);

    for (auto& rPoint : aPoly)
    {
        rPoint = WindowToLogicUnits(rPoint, rMapRes);
    }

    return aPoly;
}

tools::Polygon CoordinateMapper::WindowToLogicUnits(const tools::Polygon& rWindowPoly,
                                                    const MapMode& rMapMode) const
{
    if (rMapMode.IsDefault())
        return rWindowPoly;

    ImplMapRes aMapRes(rMapMode, GetDPIX(), GetDPIY());

    return WindowToLogicUnits(rWindowPoly, aMapRes);
}

tools::PolyPolygon
CoordinateMapper::WindowToLogicUnits(const tools::PolyPolygon& rWindowPolyPoly) const
{
    if (!IsMappingActive())
        return rWindowPolyPoly;

    tools::PolyPolygon aPolyPoly(rWindowPolyPoly);

    for (auto& rPoly : aPolyPoly)
    {
        rPoly = WindowToLogicUnits(rPoly);
    }

    return aPolyPoly;
}

template <TransformableB2DGeometry T>
T CoordinateMapper::WindowToLogicUnits(const T& rWindowGeometry) const
{
    if (!IsMappingActive())
        return rWindowGeometry;

    T aTransformedGeometry = rWindowGeometry;
    aTransformedGeometry.transform(GetInverseViewTransformation());
    return aTransformedGeometry;
}

template SAL_DLLPRIVATE basegfx::B2DRectangle
CoordinateMapper::WindowToLogicUnits<basegfx::B2DRectangle>(const basegfx::B2DRectangle&) const;

template SAL_DLLPRIVATE basegfx::B2DPolyPolygon
CoordinateMapper::WindowToLogicUnits<basegfx::B2DPolyPolygon>(const basegfx::B2DPolyPolygon&) const;

template <TransformableB2DGeometry T>
T CoordinateMapper::WindowToLogicUnits(const T& rWindowGeometry, const MapMode& rMapMode) const
{
    // Fast-path: Route to the optimized default-mode template
    if (rMapMode.IsDefault())
        return WindowToLogicUnits(rWindowGeometry);

    T aTransformedGeometry = rWindowGeometry;
    aTransformedGeometry.transform(GetInverseViewTransformation(rMapMode));
    return aTransformedGeometry;
}

template SAL_DLLPRIVATE basegfx::B2DPolygon
CoordinateMapper::WindowToLogicUnits<basegfx::B2DPolygon>(const basegfx::B2DPolygon&,
                                                          const MapMode&) const;

template SAL_DLLPRIVATE basegfx::B2DPolyPolygon
CoordinateMapper::WindowToLogicUnits<basegfx::B2DPolyPolygon>(const basegfx::B2DPolyPolygon&,
                                                              const MapMode&) const;

Size CoordinateMapper::WindowToLogicUnits(const Size& rWindowSize) const
{
    if (!IsMappingActive())
        return rWindowSize;

    return Size(ViewToLogicDistanceX(rWindowSize.Width()),
                ViewToLogicDistanceY(rWindowSize.Height()));
}

Point CoordinateMapper::WindowToLogicUnits(const Point& rWindowPt, const ImplMapRes& rMapRes) const
{
    return Point(WindowSubPixelToLogicIntX(rWindowPt.X(), rMapRes),
                 WindowSubPixelToLogicIntY(rWindowPt.Y(), rMapRes));
}

Point CoordinateMapper::WindowToLogicUnits(const Point& rWindowPt, const MapMode& rMapMode) const
{
    if (rMapMode.IsDefault())
        return rWindowPt;

    // Calculate MapMode-resolution once
    ImplMapRes aMapRes(rMapMode, GetDPIX(), GetDPIY());

    // Pass the pre-calculated resolution down the chain
    return WindowToLogicUnits(rWindowPt, aMapRes);
}

Size CoordinateMapper::WindowToLogicUnits(const Size& rWindowSize, const MapMode& rMapMode) const
{
    if (rMapMode.IsDefault())
        return rWindowSize;

    // Calculate MapMode-resolution once
    ImplMapRes aMapRes(rMapMode, GetDPIX(), GetDPIY());

    // Pass the pre-calculated resolution down the chain
    return WindowToLogicUnits(rWindowSize, aMapRes);
}

Size CoordinateMapper::WindowToLogicUnits(const Size& rWindowSize, const ImplMapRes& rMapRes) const
{
    // Note: Sizes (Distances) ignore translational offsets.
    // Therefore, Window Distance == View Distance.
    return Size(ViewToLogicDistanceX(rWindowSize.Width(), rMapRes.mfMapScX),
                ViewToLogicDistanceY(rWindowSize.Height(), rMapRes.mfMapScY));
}

tools::Rectangle CoordinateMapper::WindowToLogicUnits(const tools::Rectangle& rWindowRect,
                                                      const ImplMapRes& rMapRes) const
{
    tools::Rectangle aRetval(
        // Fix: Use the new Window-level scalar wrappers to ensure WindowToView offsets are applied!
        WindowSubPixelToLogicIntX(rWindowRect.Left(), rMapRes),
        WindowSubPixelToLogicIntY(rWindowRect.Top(), rMapRes),
        rWindowRect.IsWidthEmpty() ? 0 : WindowSubPixelToLogicIntX(rWindowRect.Right(), rMapRes),
        rWindowRect.IsHeightEmpty() ? 0 : WindowSubPixelToLogicIntY(rWindowRect.Bottom(), rMapRes));

    lcl_ApplyEmptyState(aRetval, rWindowRect);

    return aRetval;
}

tools::Rectangle CoordinateMapper::WindowToLogicUnits(const tools::Rectangle& rWindowRect,
                                                      const MapMode& rMapMode) const
{
    // calculate nothing if default-MapMode
    // tdf#141761 see comments above, IsEmpty() removed
    if (rMapMode.IsDefault())
        return rWindowRect;

    // Calculate MapMode-resolution once
    ImplMapRes aMapRes(rMapMode, GetDPIX(), GetDPIY());

    // Pass the pre-calculated resolution down the chain
    return WindowToLogicUnits(rWindowRect, aMapRes);
}

// ========================================================================
// DISTANCE SCALING (Raw Scalar Conversion)
// ========================================================================

tools::Long CoordinateMapper::LogicToViewDistanceX(tools::Long n, double fScale) const
{
    return lcl_RoundToLong(LogicToViewDistanceSubPixelX(n, fScale));
}

tools::Long CoordinateMapper::LogicToViewDistanceY(tools::Long n, double fScale) const
{
    return lcl_RoundToLong(LogicToViewDistanceSubPixelY(n, fScale));
}

tools::Long CoordinateMapper::ViewToLogicDistanceX(tools::Long n, double fScale) const
{
    return lcl_RoundToLong(ViewToLogicDistanceDoubleX(static_cast<double>(n), fScale));
}

tools::Long CoordinateMapper::ViewToLogicDistanceY(tools::Long n, double fScale) const
{
    return lcl_RoundToLong(ViewToLogicDistanceDoubleY(static_cast<double>(n), fScale));
}

tools::Long CoordinateMapper::LogicToViewDistanceX(tools::Long n) const
{
    return LogicToViewDistanceX(n, maMapRes.mfMapScX);
}

tools::Long CoordinateMapper::LogicToViewDistanceY(tools::Long n) const
{
    return LogicToViewDistanceY(n, maMapRes.mfMapScY);
}

tools::Long CoordinateMapper::ViewToLogicDistanceX(tools::Long n) const
{
    return ViewToLogicDistanceX(n, maMapRes.mfMapScX);
}

tools::Long CoordinateMapper::ViewToLogicDistanceY(tools::Long n) const
{
    return ViewToLogicDistanceY(n, maMapRes.mfMapScY);
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

double CoordinateMapper::LogicToViewDistanceSubPixelX(tools::Long n) const
{
    return LogicToViewDistanceSubPixelX(n, maMapRes.mfMapScX);
}

double CoordinateMapper::LogicToViewDistanceSubPixelY(tools::Long n) const
{
    return LogicToViewDistanceSubPixelY(n, maMapRes.mfMapScY);
}

double CoordinateMapper::LogicToViewDistanceSubPixelX(tools::Long n, double fScale) const
{
    assert(GetDPIX() > 0 && "CoordinateMapper: Invalid DPI X, falling back to identity");
    if (GetDPIX() <= 0)
        return static_cast<double>(n); // Identity fallback, not 0.0

    return static_cast<double>(n) * fScale * GetDPIX();
}

double CoordinateMapper::LogicToViewDistanceSubPixelY(tools::Long n, double fScale) const
{
    assert(GetDPIY() > 0 && "CoordinateMapper: Invalid DPI Y, falling back to identity");
    if (GetDPIY() <= 0)
        return static_cast<double>(n); // Identity fallback, not 0.0

    return static_cast<double>(n) * fScale * GetDPIY();
}

double CoordinateMapper::ViewToLogicDistanceDoubleX(double n) const
{
    return ViewToLogicDistanceDoubleX(n, maMapRes.mfMapScX);
}

double CoordinateMapper::ViewToLogicDistanceDoubleY(double n) const
{
    return ViewToLogicDistanceDoubleY(n, maMapRes.mfMapScY);
}

double CoordinateMapper::ViewToLogicDistanceDoubleX(double n, double fScale) const
{
    assert(fScale != 0.0 && GetDPIX() > 0
           && "CoordinateMapper: Zero scale or invalid DPI X, falling back to identity");
    if (fScale == 0.0 || GetDPIX() <= 0)
        return n; // Identity fallback, not 0.0

    return n / fScale / GetDPIX();
}

double CoordinateMapper::ViewToLogicDistanceDoubleY(double n, double fScale) const
{
    assert(fScale != 0.0 && GetDPIY() > 0
           && "CoordinateMapper: Zero scale or invalid DPI Y, falling back to identity");
    if (fScale == 0.0 || GetDPIY() <= 0)
        return n; // Identity fallback, not 0.0

    return n / fScale / GetDPIY();
}

tools::Long CoordinateMapper::ViewSubPixelToLogicDistanceX(double n) const
{
    return ViewSubPixelToLogicDistanceX(n, maMapRes.mfMapScX);
}

tools::Long CoordinateMapper::ViewSubPixelToLogicDistanceY(double n) const
{
    return ViewSubPixelToLogicDistanceY(n, maMapRes.mfMapScY);
}

tools::Long CoordinateMapper::ViewSubPixelToLogicDistanceX(double n, double fScale) const
{
    return lcl_RoundToLong(ViewToLogicDistanceDoubleX(n, fScale));
}

tools::Long CoordinateMapper::ViewSubPixelToLogicDistanceY(double n, double fScale) const
{
    return lcl_RoundToLong(ViewToLogicDistanceDoubleY(n, fScale));
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

tools::Long CoordinateMapper::DevicePixelToLogicWidth(tools::Long nWidth) const
{
    if (!IsMappingActive())
        return nWidth;

    return ViewToLogicDistanceX(nWidth);
}

tools::Long CoordinateMapper::DevicePixelToLogicHeight(tools::Long nHeight) const
{
    if (!IsMappingActive())
        return nHeight;

    return ViewToLogicDistanceY(nHeight);
}

Size CoordinateMapper::DevicePixelToLogic(const Size& rDeviceSize) const
{
    return Size(DevicePixelToLogicWidth(rDeviceSize.Width()),
                DevicePixelToLogicHeight(rDeviceSize.Height()));
}

Point CoordinateMapper::LogicToLogic(const Point& rPtSource, const MapMode* pMapModeSource,
                                     const MapMode* pMapModeDest) const
{
    const MapMode* pSrc = pMapModeSource ? pMapModeSource : &GetMapMode();
    const MapMode* pDst = pMapModeDest ? pMapModeDest : &GetMapMode();

    if (*pSrc == *pDst)
        return rPtSource;

    ImplMapRes aMapResSource = ResolveMapRes(pMapModeSource);
    ImplMapRes aMapResDest = ResolveMapRes(pMapModeDest);

    return Point(aMapResSource.TransformPointX(rPtSource.X(), aMapResDest),
                 aMapResSource.TransformPointY(rPtSource.Y(), aMapResDest));
}

Size CoordinateMapper::LogicToLogic(const Size& rSzSource, const MapMode* pMapModeSource,
                                    const MapMode* pMapModeDest) const
{
    const MapMode* pSrc = pMapModeSource ? pMapModeSource : &GetMapMode();
    const MapMode* pDst = pMapModeDest ? pMapModeDest : &GetMapMode();

    if (*pSrc == *pDst)
        return rSzSource;

    ImplMapRes aMapResSource = ResolveMapRes(pMapModeSource);
    ImplMapRes aMapResDest = ResolveMapRes(pMapModeDest);

    return Size(aMapResSource.ScaleDistanceX(rSzSource.Width(), aMapResDest),
                aMapResSource.ScaleDistanceY(rSzSource.Height(), aMapResDest));
}

tools::Rectangle CoordinateMapper::LogicToLogic(const tools::Rectangle& rRectSource,
                                                const MapMode* pMapModeSource,
                                                const MapMode* pMapModeDest) const
{
    const MapMode* pSrc = pMapModeSource ? pMapModeSource : &GetMapMode();
    const MapMode* pDst = pMapModeDest ? pMapModeDest : &GetMapMode();

    if (*pSrc == *pDst)
        return rRectSource;

    ImplMapRes aMapResSource = ResolveMapRes(pMapModeSource);
    ImplMapRes aMapResDest = ResolveMapRes(pMapModeDest);

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

static std::pair<ImplMapRes, ImplMapRes> lcl_calcConversionMapRes(const MapMode& rMMSource,
                                                                  const MapMode& rMMDest)
{
    std::pair<ImplMapRes, ImplMapRes> result;
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

    // Fast path: conversion succeeded without integer overflow
    if (!bOverflow)
        return nResult;

    // Fallback: Use BigInt to prevent overflow during intermediate multiplication
    const auto[nMultiplier, nDivisor] = o3tl::getConversionMulDiv(eSourceUnit, eDestUnit);
    BigInt aBigValue = nSourceValue;
    aBigValue *= nMultiplier;

    // Manual rounding: standard integer division truncates towards zero.
    // We add or subtract half the divisor before dividing to achieve round-to-nearest.
    if (aBigValue.IsNeg())
        aBigValue -= nDivisor / 2;
    else
        aBigValue += nDivisor / 2;

    aBigValue /= nDivisor;

    return static_cast<tools::Long>(aBigValue);
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
            return aTransform; // Return default identity matrix
        }

        const double fScaleFactor = o3tl::convert(1.0, eFrom, eTo);
        aTransform.set(0, 0, fScaleFactor);
        aTransform.set(1, 1, fScaleFactor);

        return aTransform;
    }

    const auto[aMapResSource, aMapResDest] = lcl_calcConversionMapRes(rMapModeSource, rMapModeDest);

    // Guard against division by zero if MapResDest has an invalid scale
    const double fDestScX = (aMapResDest.mfMapScX != 0.0) ? aMapResDest.mfMapScX : 1.0;
    const double fDestScY = (aMapResDest.mfMapScY != 0.0) ? aMapResDest.mfMapScY : 1.0;

    const double fScaleFactorX(aMapResSource.mfMapScX / fDestScX);
    const double fScaleFactorY(aMapResSource.mfMapScY / fDestScY);
    const double fZeroPointX(double(aMapResSource.mnMapOfsX) * fScaleFactorX
                             - double(aMapResDest.mnMapOfsX));
    const double fZeroPointY(double(aMapResSource.mnMapOfsY) * fScaleFactorY
                             - double(aMapResDest.mnMapOfsY));

    aTransform.set(0, 0, fScaleFactorX);
    aTransform.set(1, 1, fScaleFactorY);
    aTransform.set(0, 2, fZeroPointX);
    aTransform.set(1, 2, fZeroPointY);

    return aTransform;
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
