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

static inline tools::Long lcl_RoundToLong(double fVal)
{
    return static_cast<tools::Long>(std::llround(fVal));
}

// Conceptual Pipeline Separation (Mathematical Invariant):
// Logic -> View: Scaled transformations (Scale * Logic) + Scaled Offsets ((MapOfs + LogicOfs) * Scale)
// View -> Window: Pure translation (WindowOfs)
// Window -> Device: Pure translation (DeviceOfs)
void CoordinateMapper::GetLogicToViewWeights(double& rScaleX, double& rScaleY, double& rTransX,
                                             double& rTransY, bool bMap) const
{
    auto snap = AcquireSnapshot(bMap);
    rScaleX = snap->maView.get(0, 0);
    rScaleY = snap->maView.get(1, 1);
    rTransX = snap->maView.get(0, 2);
    rTransY = snap->maView.get(1, 2);
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
    if (!bMap && IsValidDPI())
        return rLogicSize;

    return Size(LogicToViewDistanceX(rLogicSize.Width(), bMap),
                LogicToViewDistanceY(rLogicSize.Height(), bMap));
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
    // 1. Let the legacy accumulator do its complex state math
    maMapRes.CalcMapResolution(rMapMode, nDPIX, nDPIY);

    // 2. Copy the pure math into our firewall struct
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

vcl::detail::MapConversion CoordinateMapper::ResolveMap(const MapMode& rMapMode, bool bMap) const
{
    // Evaluates a temporary MapMode against the current accumulated state
    MappingCoefficients aRes = maMapRes.ResolveMapRes(&rMapMode, maMapMode, bMap, mnDPIX, mnDPIY);
    return { aRes.mfScaleX, aRes.mfScaleY, aRes.mnTranslationX, aRes.mnTranslationY };
}

void CoordinateMapper::InvalidateViewTransform()
{
    mnStateCounter.fetch_add(1, std::memory_order_release);
}

std::shared_ptr<CoordinateMapper::TransformSnapshot>
CoordinateMapper::BuildSnapshot(bool bMap) const
{
    auto pSnap = std::make_shared<TransformSnapshot>();

    if (!bMap && IsValidDPI())
    {
        // Identity fallback: Only screen-space offsets remain
        const double dxView = static_cast<double>(mnWindowToViewOffsetX);
        const double dyView = static_cast<double>(mnWindowToViewOffsetY);

        pSnap->maView.identity();
        pSnap->maView.set(0, 2, dxView);
        pSnap->maView.set(1, 2, dyView);

        const double dxDev = dxView + static_cast<double>(mnDeviceToWindowOffsetX);
        const double dyDev = dyView + static_cast<double>(mnDeviceToWindowOffsetY);

        pSnap->maLogicToDevice.identity();
        pSnap->maLogicToDevice.set(0, 2, dxDev);
        pSnap->maLogicToDevice.set(1, 2, dyDev);
    }
    else
    {
        // Canonical scale (Logic -> Device space)
        // STRANGLER STEP 1: Read scale from the pure math firewall, not the state accumulator
        const double scaleX = static_cast<double>(mnDPIX) * maMapConversion.mfScaleX;
        const double scaleY = static_cast<double>(mnDPIY) * maMapConversion.mfScaleY;

        // Logic-space translation only
        // STRANGLER STEP 1: Read offsets from the pure math firewall, not the state accumulator
        const double logicOffsetX = static_cast<double>(maMapConversion.mnOffsetX)
                                    + static_cast<double>(mnLogicToAbsoluteOffsetX);
        const double logicOffsetY = static_cast<double>(maMapConversion.mnOffsetY)
                                    + static_cast<double>(mnLogicToAbsoluteOffsetY);

        // View Space (Logic -> Window)
        const double txView = (logicOffsetX * scaleX) + static_cast<double>(mnWindowToViewOffsetX);
        const double tyView = (logicOffsetY * scaleY) + static_cast<double>(mnWindowToViewOffsetY);

        pSnap->maView.identity();
        pSnap->maView.set(0, 0, scaleX);
        pSnap->maView.set(1, 1, scaleY);
        pSnap->maView.set(0, 2, txView);
        pSnap->maView.set(1, 2, tyView);

        // Device Space (Window -> Device)
        const double txDev = txView + static_cast<double>(mnDeviceToWindowOffsetX);
        const double tyDev = tyView + static_cast<double>(mnDeviceToWindowOffsetY);

        pSnap->maLogicToDevice.identity();
        pSnap->maLogicToDevice.set(0, 0, scaleX);
        pSnap->maLogicToDevice.set(1, 1, scaleY);
        pSnap->maLogicToDevice.set(0, 2, txDev);
        pSnap->maLogicToDevice.set(1, 2, tyDev);
    }

    // Derived transforms
    pSnap->maDeviceToLogic = pSnap->maLogicToDevice;
    if (!pSnap->maDeviceToLogic.invert())
        pSnap->maDeviceToLogic.identity();

    pSnap->maInvView = pSnap->maView;
    if (!pSnap->maInvView.invert())
        pSnap->maInvView.identity();

    return pSnap;
}

std::shared_ptr<const CoordinateMapper::TransformSnapshot>
CoordinateMapper::AcquireSnapshot(bool bMap) const
{
    while (true)
    {
        // Capture version BEFORE reading snapshot
        const uint64_t nStartVersion = mnStateCounter.load(std::memory_order_acquire);

        auto pSnap = std::atomic_load_explicit(&mpSnapshot, std::memory_order_acquire);

        // Fast path: snapshot is valid
        if (pSnap && pSnap->mnVersion == nStartVersion)
            return pSnap;

        // Build new snapshot (single source of truth)
        auto pNew = BuildSnapshot(bMap);

        // Version check (lock-free consistency guard)
        const uint64_t nEndVersion = mnStateCounter.load(std::memory_order_acquire);

        if (nStartVersion != nEndVersion)
            continue; // state changed mid-build → retry

        pNew->mnVersion = nStartVersion;

        std::atomic_store_explicit(&mpSnapshot,
                                   std::shared_ptr<const TransformSnapshot>(std::move(pNew)),
                                   std::memory_order_release);

        return std::atomic_load_explicit(&mpSnapshot, std::memory_order_acquire);
    }
}

basegfx::B2DHomMatrix CoordinateMapper::GetDeviceTransformation(bool bMap) const
{
    return AcquireSnapshot(bMap)->maLogicToDevice;
}

basegfx::B2DHomMatrix CoordinateMapper::GetViewTransformation(bool bMap) const
{
    return AcquireSnapshot(bMap)->maView;
}

basegfx::B2DHomMatrix CoordinateMapper::GetInverseViewTransformation(bool bMap) const
{
    return AcquireSnapshot(bMap)->maInvView;
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

basegfx::B2DHomMatrix CoordinateMapper::GetViewTransformation(const MapMode& rMapMode,
                                                              bool bMap) const
{
    // STRANGLER STEP 2: The legacy API is now just a routing shim
    if (rMapMode.IsDefault())
        return GetViewTransformation(bMap);
    return GetViewTransformation(ResolveMap(rMapMode, bMap));
}

basegfx::B2DHomMatrix
CoordinateMapper::GetInverseViewTransformation(const vcl::detail::MapConversion& rConv) const
{
    basegfx::B2DHomMatrix aMatrix(GetViewTransformation(rConv));
    aMatrix.invert();
    return aMatrix;
}

basegfx::B2DHomMatrix CoordinateMapper::GetInverseViewTransformation(const MapMode& rMapMode,
                                                                     bool bMap) const
{
    if (rMapMode.IsDefault())
        return GetInverseViewTransformation(bMap);
    return GetInverseViewTransformation(ResolveMap(rMapMode, bMap));
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
double CoordinateMapper::ViewSubPixelToLogicUnitsX(double fX, bool bMap) const
{
    auto snap = AcquireSnapshot(bMap);
    // To get Logic from View, we convert View to Window, then apply InvView (which is Window -> Logic)
    double fWindow = fX + static_cast<double>(mnWindowToViewOffsetX);
    return fWindow * snap->maInvView.get(0, 0) + snap->maInvView.get(0, 2);
}

double CoordinateMapper::ViewSubPixelToLogicUnitsY(double fY, bool bMap) const
{
    auto snap = AcquireSnapshot(bMap);
    double fWindow = fY + static_cast<double>(mnWindowToViewOffsetY);
    return fWindow * snap->maInvView.get(1, 1) + snap->maInvView.get(1, 2);
}

double CoordinateMapper::LogicUnitsToViewSubPixelX(double fX, bool bMap) const
{
    auto snap = AcquireSnapshot(bMap);
    // maView maps Logic -> Window. View is Window - WindowToViewOffset.
    return (fX * snap->maView.get(0, 0) + snap->maView.get(0, 2))
           - static_cast<double>(mnWindowToViewOffsetX);
}

double CoordinateMapper::LogicUnitsToViewSubPixelY(double fY, bool bMap) const
{
    auto snap = AcquireSnapshot(bMap);
    return (fY * snap->maView.get(1, 1) + snap->maView.get(1, 2))
           - static_cast<double>(mnWindowToViewOffsetY);
}

tools::Long CoordinateMapper::ViewToLogicUnitsX(tools::Long nX, bool bMap) const
{
    return lcl_RoundToLong(ViewSubPixelToLogicUnitsX(static_cast<double>(nX), bMap));
}

tools::Long CoordinateMapper::ViewToLogicUnitsY(tools::Long nY, bool bMap) const
{
    return lcl_RoundToLong(ViewSubPixelToLogicUnitsY(static_cast<double>(nY), bMap));
}

tools::Long CoordinateMapper::LogicUnitsToViewUnitsX(tools::Long nX, bool bMap) const
{
    return lcl_RoundToLong(LogicUnitsToViewSubPixelX(static_cast<double>(nX), bMap));
}

tools::Long CoordinateMapper::LogicUnitsToViewUnitsY(tools::Long nY, bool bMap) const
{
    return lcl_RoundToLong(LogicUnitsToViewSubPixelY(static_cast<double>(nY), bMap));
}

tools::Long CoordinateMapper::ViewSubPixelToLogicUnitsIntX(double fX, bool bMap) const
{
    return lcl_RoundToLong(ViewSubPixelToLogicUnitsX(fX, bMap));
}

tools::Long CoordinateMapper::ViewSubPixelToLogicUnitsIntY(double fY, bool bMap) const
{
    return lcl_RoundToLong(ViewSubPixelToLogicUnitsY(fY, bMap));
}

tools::Long CoordinateMapper::LogicUnitsToViewUnitsX(tools::Long nX,
                                                     const vcl::detail::MapConversion& rConv) const
{
    return LogicToViewDistanceX(nX + rConv.mnOffsetX, rConv.mfScaleX);
}

tools::Long CoordinateMapper::LogicUnitsToViewUnitsY(tools::Long nY,
                                                     const vcl::detail::MapConversion& rConv) const
{
    return LogicToViewDistanceY(nY + rConv.mnOffsetY, rConv.mfScaleY);
}

tools::Long CoordinateMapper::LogicToWindowUnitsX(tools::Long nX,
                                                  const vcl::detail::MapConversion& rConv) const
{
    auto mat = GetViewTransformation(rConv);
    return lcl_RoundToLong(static_cast<double>(nX) * mat.get(0, 0) + mat.get(0, 2));
}

tools::Long CoordinateMapper::LogicToWindowUnitsY(tools::Long nY,
                                                  const vcl::detail::MapConversion& rConv) const
{
    auto mat = GetViewTransformation(rConv);
    return lcl_RoundToLong(static_cast<double>(nY) * mat.get(1, 1) + mat.get(1, 2));
}

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
    tools::Rectangle aRetval(
        LogicToWindowUnitsX(rRect.Left(), rConv), LogicToWindowUnitsY(rRect.Top(), rConv),
        rRect.IsWidthEmpty() ? 0 : LogicToWindowUnitsX(rRect.Right(), rConv),
        rRect.IsHeightEmpty() ? 0 : LogicToWindowUnitsY(rRect.Bottom(), rConv));

    lcl_ApplyEmptyState(aRetval, rRect);

    return aRetval;
}

tools::Polygon CoordinateMapper::LogicToWindowUnits(const tools::Polygon& rLogicPoly,
                                                    const vcl::detail::MapConversion& rConv) const
{
    tools::Polygon aPoly(rLogicPoly);

    for (auto& rPoint : aPoly)
    {
        rPoint.setX(LogicToWindowUnitsX(rPoint.X(), rConv));
        rPoint.setY(LogicToWindowUnitsY(rPoint.Y(), rConv));
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

double CoordinateMapper::WindowToLogicSubPixelX(double fX, bool bMap) const
{
    auto snap = AcquireSnapshot(bMap);
    return fX * snap->maInvView.get(0, 0) + snap->maInvView.get(0, 2);
}

double CoordinateMapper::WindowToLogicSubPixelY(double fY, bool bMap) const
{
    auto snap = AcquireSnapshot(bMap);
    return fY * snap->maInvView.get(1, 1) + snap->maInvView.get(1, 2);
}

tools::Long CoordinateMapper::WindowToLogicX(tools::Long nX, bool bMap) const
{
    return lcl_RoundToLong(WindowToLogicSubPixelX(static_cast<double>(nX), bMap));
}

tools::Long CoordinateMapper::WindowToLogicY(tools::Long nY, bool bMap) const
{
    return lcl_RoundToLong(WindowToLogicSubPixelY(static_cast<double>(nY), bMap));
}

Size CoordinateMapper::LogicToWindowUnits(const Size& rLogicSize, bool bMap) const
{
    if (!bMap && IsValidDPI())
        return rLogicSize;

    return Size(LogicToViewDistanceX(rLogicSize.Width(), bMap),
                LogicToViewDistanceY(rLogicSize.Height(), bMap));
}

// --- Sub-Pixel Full Journey ---
double CoordinateMapper::LogicToDeviceSubPixelX(double fX, bool bMap) const
{
    auto snap = AcquireSnapshot(bMap);
    return fX * snap->maLogicToDevice.get(0, 0) + snap->maLogicToDevice.get(0, 2);
}

double CoordinateMapper::LogicToDeviceSubPixelY(double fY, bool bMap) const
{
    auto snap = AcquireSnapshot(bMap);
    return fY * snap->maLogicToDevice.get(1, 1) + snap->maLogicToDevice.get(1, 2);
}

double CoordinateMapper::DevicePixelToLogicSubPixelX(double fX, bool bMap) const
{
    auto snap = AcquireSnapshot(bMap);
    return fX * snap->maDeviceToLogic.get(0, 0) + snap->maDeviceToLogic.get(0, 2);
}

double CoordinateMapper::DevicePixelToLogicSubPixelY(double fY, bool bMap) const
{
    auto snap = AcquireSnapshot(bMap);
    return fY * snap->maDeviceToLogic.get(1, 1) + snap->maDeviceToLogic.get(1, 2);
}

basegfx::B2DPoint CoordinateMapper::LogicToDeviceSubPixel(const Point& rPoint, bool bMap) const
{
    auto snap = AcquireSnapshot(bMap);
    basegfx::B2DPoint aPt(rPoint.X(), rPoint.Y());
    aPt *= snap->maLogicToDevice;
    return aPt;
}

basegfx::B2DPoint CoordinateMapper::DevicePixelToLogicSubPixel(const Point& rPoint, bool bMap) const
{
    auto snap = AcquireSnapshot(bMap);
    basegfx::B2DPoint aPt(rPoint.X(), rPoint.Y());
    aPt *= snap->maDeviceToLogic;
    return aPt;
}

// Integer Boundary (this is the only place rounding occurs)

tools::Long CoordinateMapper::DevicePixelToLogicX(tools::Long nX, bool bMap) const
{
    return lcl_RoundToLong(DevicePixelToLogicSubPixelX(static_cast<double>(nX), bMap));
}

tools::Long CoordinateMapper::DevicePixelToLogicY(tools::Long nY, bool bMap) const
{
    return lcl_RoundToLong(DevicePixelToLogicSubPixelY(static_cast<double>(nY), bMap));
}

Point CoordinateMapper::DevicePixelToLogic(const Point& rDevicePt, bool bMap) const
{
    return Point(DevicePixelToLogicX(rDevicePt.X(), bMap),
                 DevicePixelToLogicY(rDevicePt.Y(), bMap));
}

tools::Rectangle CoordinateMapper::DevicePixelToLogic(const tools::Rectangle& rPixelRect,
                                                      bool bMap) const
{
    tools::Rectangle aRetval(
        DevicePixelToLogicX(rPixelRect.Left(), bMap), DevicePixelToLogicY(rPixelRect.Top(), bMap),
        rPixelRect.IsWidthEmpty() ? 0 : DevicePixelToLogicX(rPixelRect.Right(), bMap),
        rPixelRect.IsHeightEmpty() ? 0 : DevicePixelToLogicY(rPixelRect.Bottom(), bMap));

    if (rPixelRect.IsWidthEmpty())
        aRetval.SetWidthEmpty();

    if (rPixelRect.IsHeightEmpty())
        aRetval.SetHeightEmpty();

    return aRetval;
}

tools::Long CoordinateMapper::LogicToDevicePixelX(tools::Long nX, bool bMap) const
{
    if (!bMap && IsValidDPI())
        return nX + GetDeviceToWindowOffsetX();

    return lcl_RoundToLong(LogicToDeviceSubPixelX(static_cast<double>(nX), bMap));
}

tools::Long CoordinateMapper::LogicToDevicePixelY(tools::Long nY, bool bMap) const
{
    if (!bMap && IsValidDPI())
        return nY + GetDeviceToWindowOffsetY();

    return lcl_RoundToLong(LogicToDeviceSubPixelY(static_cast<double>(nY), bMap));
}

Point CoordinateMapper::LogicToDevicePixel(const Point& rLogicPt, bool bMap) const
{
    if (!bMap && IsValidDPI() && !GetDeviceToWindowOffsetX() && !GetDeviceToWindowOffsetY())
        return rLogicPt;

    auto snap = AcquireSnapshot(bMap);
    basegfx::B2DPoint aPt(rLogicPt.X(), rLogicPt.Y());
    aPt *= snap->maLogicToDevice;

    return Point(lcl_RoundToLong(aPt.getX()), lcl_RoundToLong(aPt.getY()));
}

// Note: Width/Height use Distances, not Positions!
double CoordinateMapper::LogicWidthToDeviceSubPixel(tools::Long nWidth, bool bMap) const
{
    if (!bMap && IsValidDPI())
        return static_cast<double>(nWidth);

    return LogicToViewDistanceSubPixelX(nWidth, bMap);
}

tools::Long CoordinateMapper::LogicWidthToDevicePixel(tools::Long nWidth, bool bMap) const
{
    if (!bMap && IsValidDPI())
        return nWidth;

    return lcl_RoundToLong(std::abs(LogicToViewDistanceSubPixelX(nWidth, bMap)));
}

tools::Long CoordinateMapper::LogicHeightToDevicePixel(tools::Long nHeight, bool bMap) const
{
    if (!bMap && IsValidDPI())
        return nHeight;

    return lcl_RoundToLong(std::abs(LogicToViewDistanceSubPixelY(nHeight, bMap)));
}

Size CoordinateMapper::LogicToDevicePixel(const Size& rLogicSize, bool bMap) const
{
    if (!bMap && IsValidDPI())
        return rLogicSize;

    return Size(LogicWidthToDevicePixel(rLogicSize.Width(), bMap),
                LogicHeightToDevicePixel(rLogicSize.Height(), bMap));
}

tools::Rectangle CoordinateMapper::LogicToDevicePixel(const tools::Rectangle& rLogicRect,
                                                      bool bMap) const
{
    // Fast path: no mapping active, no offsets -> identity
    if (!bMap && IsValidDPI() && !GetDeviceToWindowOffsetX() && !GetDeviceToWindowOffsetY())
        return rLogicRect;

    auto snap = AcquireSnapshot(bMap);

    // Treat rectangle as geometric range (continuous space)
    basegfx::B2DRange aRange(rLogicRect.Left(), rLogicRect.Top(), rLogicRect.Right(),
                             rLogicRect.Bottom());

    // Apply full affine transform
    aRange.transform(snap->maLogicToDevice);

    const tools::Long nL = lcl_RoundToLong(aRange.getMinX());
    const tools::Long nT = lcl_RoundToLong(aRange.getMinY());
    const tools::Long nR = lcl_RoundToLong(aRange.getMaxX());
    const tools::Long nB = lcl_RoundToLong(aRange.getMaxY());

    tools::Rectangle aRetval(nL, nT, nR, nB);

    if (rLogicRect.IsWidthEmpty())
        aRetval.SetWidthEmpty();

    if (rLogicRect.IsHeightEmpty())
        aRetval.SetHeightEmpty();

    return aRetval;
}

tools::Polygon CoordinateMapper::LogicToDevicePixel(const tools::Polygon& rLogicPoly,
                                                    bool bMap) const
{
    if (!bMap && IsValidDPI() && !GetDeviceToWindowOffsetX() && !GetDeviceToWindowOffsetY())
        return rLogicPoly;

    auto snap = AcquireSnapshot(bMap);

    basegfx::B2DPolygon aB2DPoly(rLogicPoly.getB2DPolygon());
    aB2DPoly.transform(snap->maLogicToDevice);

    tools::Polygon aPoly;

    for (sal_uInt32 i = 0; i < aB2DPoly.count(); ++i)
    {
        const auto& p = aB2DPoly.getB2DPoint(i);
        aPoly.Insert(POLY_APPEND, Point(lcl_RoundToLong(p.getX()), lcl_RoundToLong(p.getY())));
    }

    return aPoly;
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

    auto snap = AcquireSnapshot(bMap);

    basegfx::B2DPolygon aPoly(rLogicPoly);
    aPoly.transform(snap->maLogicToDevice);
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

tools::Long CoordinateMapper::ViewToLogicX(tools::Long nX, bool bMap) const
{
    return ViewToLogicUnitsX(nX, bMap) - mnLogicToAbsoluteOffsetX;
}

tools::Long CoordinateMapper::ViewToLogicY(tools::Long nY, bool bMap) const
{
    return ViewToLogicUnitsY(nY, bMap) - mnLogicToAbsoluteOffsetY;
}

tools::Long CoordinateMapper::LogicToWindowUnitsX(tools::Long nX, bool bMap) const
{
    if (!bMap && IsValidDPI())
        return nX;

    return lcl_RoundToLong(LogicToWindowSubPixelX(static_cast<double>(nX), bMap));
}

tools::Long CoordinateMapper::LogicToWindowUnitsY(tools::Long nY, bool bMap) const
{
    if (!bMap && IsValidDPI())
        return nY;

    return lcl_RoundToLong(LogicToWindowSubPixelY(static_cast<double>(nY), bMap));
}

Point CoordinateMapper::LogicToWindowUnits(const Point& rLogicPt, bool bMap) const
{
    if (!bMap && IsValidDPI())
        return rLogicPt;

    return Point(LogicToWindowUnitsX(rLogicPt.X(), bMap), LogicToWindowUnitsY(rLogicPt.Y(), bMap));
}

tools::Rectangle CoordinateMapper::LogicToWindowUnits(const tools::Rectangle& rRect,
                                                      bool bMap) const
{
    if (!bMap && IsValidDPI())
        return rRect;

    tools::Rectangle aRetval(LogicToWindowUnitsX(rRect.Left(), bMap),
                             LogicToWindowUnitsY(rRect.Top(), bMap),
                             rRect.IsWidthEmpty() ? 0 : LogicToWindowUnitsX(rRect.Right(), bMap),
                             rRect.IsHeightEmpty() ? 0 : LogicToWindowUnitsY(rRect.Bottom(), bMap));

    lcl_ApplyEmptyState(aRetval, rRect);

    return aRetval;
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

        for (auto aRectIter = aRectangles.rbegin(); aRectIter != aRectangles.rend(); ++aRectIter)
        {
            aRegion.Union(func(*aRectIter));
        }
    }
    return aRegion;
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

    for (auto& rPoint : aPoly)
    {
        rPoint.setX(LogicToWindowUnitsX(rPoint.X(), bMap));
        rPoint.setY(LogicToWindowUnitsY(rPoint.Y(), bMap));
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
    if (!bMap && IsValidDPI())
        return nWidth;

    return LogicToViewDistanceSubPixelX(nWidth, bMap);
}

double CoordinateMapper::LogicHeightToWindowSubPixel(tools::Long nHeight, bool bMap) const
{
    if (!bMap && IsValidDPI())
        return nHeight;

    return LogicToViewDistanceSubPixelY(nHeight, bMap);
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
    if (!bMap && IsValidDPI())
        return rWindowPt;

    return Point(WindowToLogicX(rWindowPt.X(), bMap), WindowToLogicY(rWindowPt.Y(), bMap));
}

tools::Long CoordinateMapper::WindowSubPixelToLogicIntX(double fX, bool bMap) const
{
    return lcl_RoundToLong(WindowToLogicSubPixelX(fX, bMap));
}

tools::Long CoordinateMapper::WindowSubPixelToLogicIntY(double fY, bool bMap) const
{
    return lcl_RoundToLong(WindowToLogicSubPixelY(fY, bMap));
}

Point CoordinateMapper::WindowSubPixelToLogicUnits(const basegfx::B2DPoint& rWindowPt,
                                                   bool bMap) const
{
    if (!bMap && IsValidDPI())
    {
        assert(std::floor(rWindowPt.getX()) == rWindowPt.getX()
               && std::floor(rWindowPt.getY()) == rWindowPt.getY());
        return Point(rWindowPt.getX(), rWindowPt.getY());
    }

    return Point(WindowSubPixelToLogicIntX(rWindowPt.getX(), bMap),
                 WindowSubPixelToLogicIntY(rWindowPt.getY(), bMap));
}

Size CoordinateMapper::WindowToLogicUnits(const Size& rWindowSize, bool bMap) const
{
    if (!bMap && IsValidDPI())
        return rWindowSize;

    return Size(ViewToLogicDistanceX(rWindowSize.Width(), bMap),
                ViewToLogicDistanceY(rWindowSize.Height(), bMap));
}

tools::Rectangle CoordinateMapper::WindowToLogicUnits(const tools::Rectangle& rWindowRect,
                                                      bool bMap) const
{
    if (!bMap && IsValidDPI())
        return rWindowRect;

    tools::Rectangle aRetval(
        WindowSubPixelToLogicIntX(rWindowRect.Left(), bMap),
        WindowSubPixelToLogicIntY(rWindowRect.Top(), bMap),
        rWindowRect.IsWidthEmpty() ? 0 : WindowSubPixelToLogicIntX(rWindowRect.Right(), bMap),
        rWindowRect.IsHeightEmpty() ? 0 : WindowSubPixelToLogicIntY(rWindowRect.Bottom(), bMap));

    lcl_ApplyEmptyState(aRetval, rWindowRect);

    return aRetval;
}

tools::Polygon CoordinateMapper::WindowToLogicUnits(const tools::Polygon& rWindowPoly,
                                                    bool bMap) const
{
    if (!bMap && IsValidDPI())
        return rWindowPoly;

    tools::Polygon aPoly(rWindowPoly);

    for (auto& rPoint : aPoly)
    {
        rPoint = Point(WindowSubPixelToLogicIntX(rPoint.X(), bMap),
                       WindowSubPixelToLogicIntY(rPoint.Y(), bMap));
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

tools::Long CoordinateMapper::ViewSubPixelToLogicIntX(double fX,
                                                      const vcl::detail::MapConversion& rConv) const
{
    double fLogicDist = ViewToLogicDistanceDoubleX(fX, rConv.mfScaleX);
    return lcl_RoundToLong(fLogicDist - static_cast<double>(rConv.mnOffsetX)
                           - static_cast<double>(mnLogicToAbsoluteOffsetX));
}

tools::Long CoordinateMapper::ViewSubPixelToLogicIntY(double fY,
                                                      const vcl::detail::MapConversion& rConv) const
{
    double fLogicDist = ViewToLogicDistanceDoubleY(fY, rConv.mfScaleY);
    return lcl_RoundToLong(fLogicDist - static_cast<double>(rConv.mnOffsetY)
                           - static_cast<double>(mnLogicToAbsoluteOffsetY));
}

tools::Long
CoordinateMapper::WindowSubPixelToLogicIntX(double fX,
                                            const vcl::detail::MapConversion& rConv) const
{
    auto mat = GetInverseViewTransformation(rConv);
    return lcl_RoundToLong(fX * mat.get(0, 0) + mat.get(0, 2));
}

tools::Long
CoordinateMapper::WindowSubPixelToLogicIntY(double fY,
                                            const vcl::detail::MapConversion& rConv) const
{
    auto mat = GetInverseViewTransformation(rConv);
    return lcl_RoundToLong(fY * mat.get(1, 1) + mat.get(1, 2));
}

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
    tools::Rectangle aRetval(
        WindowSubPixelToLogicIntX(rWindowRect.Left(), rConv),
        WindowSubPixelToLogicIntY(rWindowRect.Top(), rConv),
        rWindowRect.IsWidthEmpty() ? 0 : WindowSubPixelToLogicIntX(rWindowRect.Right(), rConv),
        rWindowRect.IsHeightEmpty() ? 0 : WindowSubPixelToLogicIntY(rWindowRect.Bottom(), rConv));
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

tools::Long CoordinateMapper::LogicToViewDistanceX(tools::Long n, bool bMap) const
{
    return lcl_RoundToLong(LogicToViewDistanceSubPixelX(n, bMap));
}

tools::Long CoordinateMapper::LogicToViewDistanceY(tools::Long n, bool bMap) const
{
    return lcl_RoundToLong(LogicToViewDistanceSubPixelY(n, bMap));
}

tools::Long CoordinateMapper::ViewToLogicDistanceX(tools::Long n, bool bMap) const
{
    return lcl_RoundToLong(ViewToLogicDistanceDoubleX(static_cast<double>(n), bMap));
}

tools::Long CoordinateMapper::ViewToLogicDistanceY(tools::Long n, bool bMap) const
{
    return lcl_RoundToLong(ViewToLogicDistanceDoubleY(static_cast<double>(n), bMap));
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

double CoordinateMapper::LogicToViewDistanceSubPixelX(tools::Long n, bool bMap) const
{
    auto snap = AcquireSnapshot(bMap);
    return static_cast<double>(n) * snap->maView.get(0, 0);
}

double CoordinateMapper::LogicToViewDistanceSubPixelY(tools::Long n, bool bMap) const
{
    auto snap = AcquireSnapshot(bMap);
    return static_cast<double>(n) * snap->maView.get(1, 1);
}

double CoordinateMapper::LogicToViewDistanceSubPixelX(tools::Long n, double fScale) const
{
    assert(GetDPIX() > 0 && "CoordinateMapper: Invalid DPI X, falling back to identity");
    if (GetDPIX() <= 0)
        return static_cast<double>(n);

    return static_cast<double>(n) * fScale * GetDPIX();
}

double CoordinateMapper::LogicToViewDistanceSubPixelY(tools::Long n, double fScale) const
{
    assert(GetDPIY() > 0 && "CoordinateMapper: Invalid DPI Y, falling back to identity");
    if (GetDPIY() <= 0)
        return static_cast<double>(n);

    return static_cast<double>(n) * fScale * GetDPIY();
}

double CoordinateMapper::ViewToLogicDistanceDoubleX(double n, bool bMap) const
{
    auto snap = AcquireSnapshot(bMap);
    return n * snap->maInvView.get(0, 0);
}

double CoordinateMapper::ViewToLogicDistanceDoubleY(double n, bool bMap) const
{
    auto snap = AcquireSnapshot(bMap);
    return n * snap->maInvView.get(1, 1);
}

double CoordinateMapper::ViewToLogicDistanceDoubleX(double n, double fScale) const
{
    assert(fScale != 0.0 && GetDPIX() > 0
           && "CoordinateMapper: Zero scale or invalid DPI X, falling back to identity");
    if (fScale == 0.0 || GetDPIX() <= 0)
        return n;

    return n / fScale / GetDPIX();
}

double CoordinateMapper::ViewToLogicDistanceDoubleY(double n, double fScale) const
{
    assert(fScale != 0.0 && GetDPIY() > 0
           && "CoordinateMapper: Zero scale or invalid DPI Y, falling back to identity");
    if (fScale == 0.0 || GetDPIY() <= 0)
        return n;

    return n / fScale / GetDPIY();
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

tools::Long CoordinateMapper::DevicePixelToLogicWidth(tools::Long nWidth, bool bMap) const
{
    if (!bMap && IsValidDPI())
        return nWidth;

    return ViewToLogicDistanceX(nWidth, bMap);
}

tools::Long CoordinateMapper::DevicePixelToLogicHeight(tools::Long nHeight, bool bMap) const
{
    if (!bMap && IsValidDPI())
        return nHeight;

    return ViewToLogicDistanceY(nHeight, bMap);
}

Size CoordinateMapper::DevicePixelToLogic(const Size& rDeviceSize, bool bMap) const
{
    return Size(DevicePixelToLogicWidth(rDeviceSize.Width(), bMap),
                DevicePixelToLogicHeight(rDeviceSize.Height(), bMap));
}

double CoordinateMapper::LogicToWindowSubPixelX(double fX, bool bMap) const
{
    auto snap = AcquireSnapshot(bMap);
    return fX * snap->maView.get(0, 0) + snap->maView.get(0, 2);
}

double CoordinateMapper::LogicToWindowSubPixelY(double fY, bool bMap) const
{
    auto snap = AcquireSnapshot(bMap);
    return fY * snap->maView.get(1, 1) + snap->maView.get(1, 2);
}

Point CoordinateMapper::LogicToLogic(const Point& rPtSource, const MapMode* pMapModeBaseline,
                                     const MapMode* pMapModeSource, const MapMode* pMapModeDest,
                                     bool bMap) const
{
    const MapMode* pSrc = pMapModeSource ? pMapModeSource : &GetMapMode();
    const MapMode* pDst = pMapModeDest ? pMapModeDest : &GetMapMode();

    if (*pSrc == *pDst)
        return rPtSource;

    MappingCoefficients aMapResSource
        = ResolveMapResRelative(pMapModeBaseline, pMapModeSource, bMap);
    MappingCoefficients aMapResDest = ResolveMapResRelative(pMapModeBaseline, pMapModeDest, bMap);

    return Point(aMapResSource.TransformPointX(rPtSource.X(), aMapResDest),
                 aMapResSource.TransformPointY(rPtSource.Y(), aMapResDest));
}

Size CoordinateMapper::LogicToLogic(const Size& rSzSource, const MapMode* pMapModeBaseline,
                                    const MapMode* pMapModeSource, const MapMode* pMapModeDest,
                                    bool bMap) const
{
    const MapMode* pSrc = pMapModeSource ? pMapModeSource : &GetMapMode();
    const MapMode* pDst = pMapModeDest ? pMapModeDest : &GetMapMode();

    if (*pSrc == *pDst)
        return rSzSource;

    MappingCoefficients aMapResSource
        = ResolveMapResRelative(pMapModeBaseline, pMapModeSource, bMap);
    MappingCoefficients aMapResDest = ResolveMapResRelative(pMapModeBaseline, pMapModeDest, bMap);

    return Size(aMapResSource.ScaleDistanceX(rSzSource.Width(), aMapResDest),
                aMapResSource.ScaleDistanceY(rSzSource.Height(), aMapResDest));
}

tools::Rectangle CoordinateMapper::LogicToLogic(const tools::Rectangle& rRectSource,
                                                const MapMode* pMapModeBaseline,
                                                const MapMode* pMapModeSource,
                                                const MapMode* pMapModeDest, bool bMap) const
{
    const MapMode* pSrc = pMapModeSource ? pMapModeSource : &GetMapMode();
    const MapMode* pDst = pMapModeDest ? pMapModeDest : &GetMapMode();

    if (*pSrc == *pDst)
        return rRectSource;

    MappingCoefficients aMapResSource
        = ResolveMapResRelative(pMapModeBaseline, pMapModeSource, bMap);
    MappingCoefficients aMapResDest = ResolveMapResRelative(pMapModeBaseline, pMapModeDest, bMap);

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
