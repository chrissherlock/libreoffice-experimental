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

#include <CompiledTransform.hxx>
#include <MappingCoefficients.hxx>

#include "CoordinateMath.hxx"

#include <cmath>
#include <cassert>
#include <ranges>

// ============================================================================
// TEMPLATE SPECIALIZATIONS: THE UNIVERSAL GEOMETRY PIPELINE
// ============================================================================

template <> Point CompiledTransform::Apply<Point>(const Point& rPt) const
{
    // Fast Path 1: Pure Integer Translation
    if (meMode == TransformMode::Translation)
        return Point(rPt.X() + mnDeviceTx, rPt.Y() + mnDeviceTy);

    // Fast Path 2: Scale + Translation (No rotation/shear)
    if (meMode == TransformMode::AxisAlignedAffine)
    {
        // Optimization: Use direct scalar arithmetic to bypass matrix cross-terms.
        // This is mathematically equivalent to a full matrix multiply for axis-aligned transforms.
        const double fX = static_cast<double>(rPt.X()) * maMatrix.get(0, 0) + maMatrix.get(0, 2);
        const double fY = static_cast<double>(rPt.Y()) * maMatrix.get(1, 1) + maMatrix.get(1, 2);

        return Point(vcl::detail::RoundToLong(fX), vcl::detail::RoundToLong(fY));
    }

    // Fallback: Complex Affine (Rotation, Shear, or Identity)
    // Stabilization: Use basegfx logic to ensure rounding consistency across complex projections.
    basegfx::B2DPoint aPt(rPt.X(), rPt.Y());
    aPt *= maMatrix;

    return Point(vcl::detail::RoundToLong(aPt.getX()), vcl::detail::RoundToLong(aPt.getY()));
}

Size CompiledTransform::ApplyRectilinear(const Size& rSize) const
{
    // NOTE: We preserve the sign of diagonal elements (m00, m11) to allow
    // Size to reflect basis orientation/mirroring if the MapMode is flipped.
    double fWidth = static_cast<double>(rSize.Width()) * maMatrix.get(0, 0);
    double fHeight = static_cast<double>(rSize.Height()) * maMatrix.get(1, 1);

    return Size(vcl::detail::RoundToLong(fWidth), vcl::detail::RoundToLong(fHeight));
}

tools::Rectangle CompiledTransform::ApplyRectilinear(const tools::Rectangle& rRect) const
{
    if (rRect.IsEmpty())
        return tools::Rectangle();

    // Map mathematical bounds [Left, Right + 1)
    const double fLeft
        = static_cast<double>(rRect.Left()) * maMatrix.get(0, 0) + maMatrix.get(0, 2);
    const double fTop = static_cast<double>(rRect.Top()) * maMatrix.get(1, 1) + maMatrix.get(1, 2);
    const double fRight
        = static_cast<double>(rRect.Right() + 1) * maMatrix.get(0, 0) + maMatrix.get(0, 2);
    const double fBottom
        = static_cast<double>(rRect.Bottom() + 1) * maMatrix.get(1, 1) + maMatrix.get(1, 2);

    // Determine physical extents
    const double fWidth = std::abs(fRight - fLeft);
    const double fHeight = std::abs(fBottom - fTop);

    // If the scaled width or height is less than 0.5 pixels, it cannot be rendered
    // meaningfully. We return a formally Empty rectangle to prevent "ghost" lines.
    if (fWidth < 0.5 && fHeight < 0.5)
    {
        tools::Rectangle aEmpty;
        aEmpty.SetEmpty();
        return aEmpty;
    }

    // Round to physical pixel indices
    tools::Long nL = vcl::detail::RoundToLong(std::min(fLeft, fRight));
    tools::Long nT = vcl::detail::RoundToLong(std::min(fTop, fBottom));
    tools::Long nR = vcl::detail::RoundToLong(std::max(fLeft, fRight)) - 1;
    tools::Long nB = vcl::detail::RoundToLong(std::max(fTop, fBottom)) - 1;

    tools::Rectangle aRet(nL, nT, nR, nB);

    // Final safety check: preserve original empty state flags (e.g. from Logic)
    vcl::ApplyEmptyState(aRet, rRect);

    return aRet;
}

template <> Size CompiledTransform::Apply<Size>(const Size& rSize) const
{
    if (PreservesAxisAlignment())
        return ApplyRectilinear(rSize);

    // Path B: Basis Magnitude Approximation
    const double fNewWidth
        = static_cast<double>(rSize.Width()) * vcl::detail::GetBasisVectorMagnitudeX(maMatrix);
    const double fNewHeight
        = static_cast<double>(rSize.Height()) * vcl::detail::GetBasisVectorMagnitudeY(maMatrix);

    return Size(vcl::detail::RoundToLong(fNewWidth), vcl::detail::RoundToLong(fNewHeight));
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
    tools::Rectangle aRet = vcl::detail::RangeToVCLRect(aRange);

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

    // PDF Structural Fix: Use the PolyPolygon bridge.
    // basegfx handles the transformation math using our optimized matrix.
    // vcl::Region then handles the scan-conversion. This ensures that
    // the rounding behavior matches the legacy path PDF export relies on.
    if (rRegion.getB2DPolyPolygon())
        return vcl::Region(Apply(*rRegion.getB2DPolyPolygon()));

    if (rRegion.getPolyPolygon())
        return vcl::Region(Apply(*rRegion.getPolyPolygon()));

    return vcl::Region(Apply(rRegion.GetAsPolyPolygon()));
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
