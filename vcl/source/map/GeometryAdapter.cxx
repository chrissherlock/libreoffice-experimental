/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <basegfx/matrix/b2dhommatrix.hxx>
#include <basegfx/range/b2drange.hxx>
#include <basegfx/vector/b2dvector.hxx>
#include <basegfx/polygon/b2dpolygon.hxx>
#include <basegfx/polygon/b2dpolypolygon.hxx>

#include <tools/gen.hxx>

#include <vcl/region.hxx>
#include <vcl/lineinfo.hxx>
#include <vcl/GeometryAdapter.hxx>

#include "CoordinateMath.hxx"

#include <cmath>
#include <algorithm>

namespace vcl::GeometryAdapter
{
VCL_DLLPUBLIC Point Apply(const TransformPlan& rPlan, const Point& rPt)
{
    // Fast Path 0: Identity
    if (rPlan.meMode == TransformMode::Identity)
        [[likely]] return rPt;

    // Fast Path 1: Pure Integer Translation
    if (rPlan.meMode == TransformMode::Translation)
        return Point(rPt.X() + rPlan.mnDeviceTx, rPt.Y() + rPlan.mnDeviceTy);

    // Fast Path 2: Scale + Translation (No rotation/shear)
    if (rPlan.meMode == TransformMode::AxisAlignedAffine)
    {
        // Optimization: Direct scalar arithmetic to bypass matrix cross-terms.
        const double fX
            = static_cast<double>(rPt.X()) * rPlan.maMatrix.get(0, 0) + rPlan.maMatrix.get(0, 2);
        const double fY
            = static_cast<double>(rPt.Y()) * rPlan.maMatrix.get(1, 1) + rPlan.maMatrix.get(1, 2);
        return Point(vcl::detail::RoundToLong(fX), vcl::detail::RoundToLong(fY));
    }

    // Fallback: Complex Affine (Rotation, Shear)
    basegfx::B2DPoint aPt(rPt.X(), rPt.Y());
    aPt *= rPlan.maMatrix;
    return Point(vcl::detail::RoundToLong(aPt.getX()), vcl::detail::RoundToLong(aPt.getY()));
}

VCL_DLLPUBLIC Size Apply(const TransformPlan& rPlan, const Size& rSize)
{
    // Path A: Rectilinear
    if (rPlan.PreservesAxisAlignment())
    {
        // NOTE: We preserve the sign of diagonal elements (m00, m11) to allow
        // Size to reflect basis orientation/mirroring if the MapMode is flipped.
        const double fWidth = static_cast<double>(rSize.Width()) * rPlan.maMatrix.get(0, 0);
        const double fHeight = static_cast<double>(rSize.Height()) * rPlan.maMatrix.get(1, 1);

        tools::Long nW = vcl::detail::RoundToLong(fWidth);
        tools::Long nH = vcl::detail::RoundToLong(fHeight);

        // Hairline clamp: If it had volume before, it must have volume now
        if (nW == 0 && rSize.Width() != 0)
            nW = (fWidth >= 0.0) ? 1 : -1;
        if (nH == 0 && rSize.Height() != 0)
            nH = (fHeight >= 0.0) ? 1 : -1;

        return Size(nW, nH);
    }

    // Path B: Basis Magnitude Approximation (The Rotation Fix)
    const double fNewWidth = static_cast<double>(rSize.Width())
                             * vcl::detail::GetBasisVectorMagnitudeX(rPlan.maMatrix);
    const double fNewHeight = static_cast<double>(rSize.Height())
                              * vcl::detail::GetBasisVectorMagnitudeY(rPlan.maMatrix);
    return Size(vcl::detail::RoundToLong(fNewWidth), vcl::detail::RoundToLong(fNewHeight));
}

VCL_DLLPUBLIC tools::Rectangle Apply(const TransformPlan& rPlan, const tools::Rectangle& rRect)
{
    // Path A: Rectilinear Explicit Math
    if (rPlan.PreservesAxisAlignment())
    {
        // Map mathematical bounds [Left, Right + 1)
        const double fLeft = static_cast<double>(rRect.Left()) * rPlan.maMatrix.get(0, 0)
                             + rPlan.maMatrix.get(0, 2);
        const double fTop = static_cast<double>(rRect.Top()) * rPlan.maMatrix.get(1, 1)
                            + rPlan.maMatrix.get(1, 2);
        const double fRight = static_cast<double>(rRect.Right() + 1) * rPlan.maMatrix.get(0, 0)
                              + rPlan.maMatrix.get(0, 2);
        const double fBottom = static_cast<double>(rRect.Bottom() + 1) * rPlan.maMatrix.get(1, 1)
                               + rPlan.maMatrix.get(1, 2);

        // Round to physical pixel indices (Standard AABB)
        tools::Long nL = vcl::detail::RoundToLong(std::min(fLeft, fRight));
        tools::Long nT = vcl::detail::RoundToLong(std::min(fTop, fBottom));
        tools::Long nR = vcl::detail::RoundToLong(std::max(fLeft, fRight)) - 1;
        tools::Long nB = vcl::detail::RoundToLong(std::max(fTop, fBottom)) - 1;

        // Hairline & Sub-pixel Boundary Clamp:
        if (nR < nL && !rRect.IsWidthEmpty())
            nR = nL;
        if (nB < nT && !rRect.IsHeightEmpty())
            nB = nT;

        // --- INVARIANT PATCH: Hairline Preservation ---
        // Force 1-unit logical lines to strictly occupy 1 physical pixel
        if (rRect.GetWidth() == 1)
            nR = nL;
        if (rRect.GetHeight() == 1)
            nB = nT;

        tools::Rectangle aRet(nL, nT, nR, nB);
        vcl::ApplyEmptyState(aRet, rRect);
        return aRet;
    }

    // Path B: Conservative AABB for Rotated/Sheared geometry
    basegfx::B2DRange aRange(rRect.Left(), rRect.Top(), rRect.Right() + 1, rRect.Bottom() + 1);
    aRange.transform(rPlan.maMatrix);

    tools::Long nL = vcl::detail::RoundToLong(aRange.getMinX());
    tools::Long nT = vcl::detail::RoundToLong(aRange.getMinY());
    tools::Long nR = vcl::detail::RoundToLong(aRange.getMaxX()) - 1;
    tools::Long nB = vcl::detail::RoundToLong(aRange.getMaxY()) - 1;

    // Apply the exact same safety clamp to the rotation fallback
    if (nR < nL && !rRect.IsWidthEmpty())
        nR = nL;
    if (nB < nT && !rRect.IsHeightEmpty())
        nB = nT;

    // --- INVARIANT PATCH: Hairline Preservation (Rotation) ---
    if (rRect.GetWidth() == 1)
        nR = nL;
    if (rRect.GetHeight() == 1)
        nB = nT;

    tools::Rectangle aRet(nL, nT, nR, nB);
    vcl::ApplyEmptyState(aRet, rRect);
    return aRet;
}

VCL_DLLPUBLIC tools::Polygon Apply(const TransformPlan& rPlan, const tools::Polygon& rPoly)
{
    if (rPlan.meMode == TransformMode::Identity)
        [[likely]] return rPoly;

    tools::Polygon aPoly(rPoly);
    for (sal_uInt16 i = 0; i < aPoly.GetSize(); ++i)
    {
        aPoly[i] = Apply(rPlan, aPoly[i]);
    }
    return aPoly;
}

VCL_DLLPUBLIC tools::PolyPolygon Apply(const TransformPlan& rPlan,
                                       const tools::PolyPolygon& rPolyPoly)
{
    if (rPlan.meMode == TransformMode::Identity)
        [[likely]] return rPolyPoly;

    tools::PolyPolygon aPolyPoly;
    for (sal_uInt16 i = 0; i < rPolyPoly.Count(); ++i)
    {
        aPolyPoly.Insert(Apply(rPlan, rPolyPoly[i]));
    }
    return aPolyPoly;
}

VCL_DLLPUBLIC basegfx::B2DPolygon Apply(const TransformPlan& rPlan,
                                        const basegfx::B2DPolygon& rPoly)
{
    if (rPlan.meMode == TransformMode::Identity)
        [[likely]] return rPoly;

    basegfx::B2DPolygon aRet(rPoly);
    aRet.transform(rPlan.maMatrix);
    return aRet;
}

VCL_DLLPUBLIC basegfx::B2DPolyPolygon Apply(const TransformPlan& rPlan,
                                            const basegfx::B2DPolyPolygon& rPolyPoly)
{
    if (rPlan.meMode == TransformMode::Identity)
        [[likely]] return rPolyPoly;

    basegfx::B2DPolyPolygon aRet(rPolyPoly);
    aRet.transform(rPlan.maMatrix);
    return aRet;
}

VCL_DLLPUBLIC basegfx::B2DRange Apply(const TransformPlan& rPlan, const basegfx::B2DRange& rRange)
{
    if (rPlan.meMode == TransformMode::Identity)
        [[likely]] return rRange;

    basegfx::B2DRange aRet(rRange);
    aRet.transform(rPlan.maMatrix);
    return aRet;
}

VCL_DLLPUBLIC vcl::Region Apply(const TransformPlan& rPlan, const vcl::Region& rRegion)
{
    if (rRegion.IsNull() || rRegion.IsEmpty() || rPlan.meMode == TransformMode::Identity)
        [[likely]] return rRegion;

    if (rPlan.meMode == TransformMode::Translation)
    {
        vcl::Region aRet(rRegion);
        aRet.Move(rPlan.mnDeviceTx, rPlan.mnDeviceTy);
        return aRet;
    }

    // PDF Structural Fix: Use the PolyPolygon bridge ONLY if the region
    // already carries a complex vector curve cache payload.
    if (rRegion.getB2DPolyPolygon())
        return vcl::Region(Apply(rPlan, *rRegion.getB2DPolyPolygon()));

    if (rRegion.getPolyPolygon())
        return vcl::Region(Apply(rPlan, *rRegion.getPolyPolygon()));

    // UI Layout & Scroll Protection:
    // If the region consists of standard UI invalidation rectangles, decompose them
    // and route them through Apply(..., const tools::Rectangle&). This allows sub-pixel
    // clamps to run, shielding them from disappearing inside the polygon scan-converter.
    vcl::Region aRegion;
    RectangleVector aRectangles;
    rRegion.GetRegionRectangles(aRectangles);

    for (const auto& rRect : aRectangles)
    {
        aRegion.Union(Apply(rPlan, rRect));
    }

    // --- EXTREME FALLBACK: Polygon Region Collapse Prevention ---
    // If a complex region completely collapses down to nothing during minification,
    // force it to retain at least a 1x1 physical point footprint to prevent VCL
    // from triggering empty-viewport crashes (e.g., SwVirtFlyDrawObj bug).
    if (aRegion.IsEmpty() && !rRegion.IsEmpty())
    {
        Point aCenter = Apply(rPlan, rRegion.GetBoundRect().Center());
        return vcl::Region(tools::Rectangle(aCenter, Size(1, 1)));
    }

    return aRegion;
}

VCL_DLLPUBLIC LineInfo Apply(const TransformPlan& rPlan, const LineInfo& rLineInfo)
{
    if (rPlan.meMode == TransformMode::Identity || rPlan.meMode == TransformMode::Translation)
        [[likely]] return rLineInfo;

    // LineInfo isn't a geometry type yet, we leave it as an explicit passthrough for now
    // until we fully eradicate the LineInfo wrapper from vcl
    LineInfo aInfo(rLineInfo);
    aInfo.SetWidth(Apply(rPlan, Size(rLineInfo.GetWidth(), 0)).Width());
    aInfo.SetDashLen(Apply(rPlan, Size(rLineInfo.GetDashLen(), 0)).Width());
    aInfo.SetDotLen(Apply(rPlan, Size(rLineInfo.GetDotLen(), 0)).Width());
    aInfo.SetDistance(Apply(rPlan, Size(rLineInfo.GetDistance(), 0)).Width());
    return aInfo;
}

} // namespace vcl::GeometryAdapter

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
