/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <sal/log.hxx>
#include <tools/line.hxx>
#include <tools/gen.hxx>
#include <tools/helpers.hxx>
#include <tools/poly.hxx>
#include <comphelper/configuration.hxx>
#include <basegfx/numeric/ftools.hxx>
#include <o3tl/safeint.hxx>

#include <vcl/vclenum.hxx>
#include <vcl/hatch.hxx>

#include <HatchProcessor.hxx>

#include <algorithm>
#include <cmath>
#include <limits>

static void lcl_CalcHorizontalHatch(const tools::Rectangle& rRect, tools::Long nDist,
                                    const Point& rRefPoint, Point& rPt1, Point& rPt2, Size& rInc,
                                    Point& rEndPt1)
{
    rInc = Size(0, nDist);
    rPt1 = rRect.TopLeft();
    rPt2 = rRect.TopRight();
    rEndPt1 = rRect.BottomLeft();

    tools::Long nOffset;
    if (rRefPoint.Y() <= rRect.Top())
        nOffset = ((rRect.Top() - rRefPoint.Y()) % nDist);
    else
        nOffset = (nDist - ((rRefPoint.Y() - rRect.Top()) % nDist));

    rPt1.AdjustY(-nOffset);
    rPt2.AdjustY(-nOffset);
}

static void lcl_CalcVerticalHatch(const tools::Rectangle& rRect, tools::Long nDist,
                                  const Point& rRefPoint, Point& rPt1, Point& rPt2, Size& rInc,
                                  Point& rEndPt1)
{
    rInc = Size(nDist, 0);
    rPt1 = rRect.TopLeft();
    rPt2 = rRect.BottomLeft();
    rEndPt1 = rRect.TopRight();

    tools::Long nOffset;
    if (rRefPoint.X() <= rRect.Left())
        nOffset = (rRect.Left() - rRefPoint.X()) % nDist;
    else
        nOffset = nDist - ((rRefPoint.X() - rRect.Left()) % nDist);

    rPt1.AdjustX(-nOffset);
    rPt2.AdjustX(-nOffset);
}

static void lcl_CalcDiagonalHatchHorizontalScan(const tools::Rectangle& rRect, tools::Long nDist,
                                                Degree10 nAngle, const Point& rRefPoint,
                                                Point& rPt1, Point& rPt2, Size& rInc,
                                                Point& rEndPt1)
{
    // "Horizontal-ish" diagonals
    const double fAngle = std::abs(toRadians(nAngle));
    const double fTan = std::tan(fAngle);
    const tools::Long nYOff = basegfx::fround<tools::Long>((rRect.Right() - rRect.Left()) * fTan);

    nDist = basegfx::fround<tools::Long>(nDist / std::cos(fAngle));
    rInc = Size(0, nDist);

    tools::Long nPY;
    if (nAngle > 0_deg10)
    {
        rPt1 = rRect.TopLeft();
        rPt2 = Point(rRect.Right(), rRect.Top() - nYOff);
        rEndPt1 = Point(rRect.Left(), rRect.Bottom() + nYOff);
        nPY = basegfx::fround<tools::Long>(rRefPoint.Y() - ((rPt1.X() - rRefPoint.X()) * fTan));
    }
    else
    {
        rPt1 = rRect.TopRight();
        rPt2 = Point(rRect.Left(), rRect.Top() - nYOff);
        rEndPt1 = Point(rRect.Right(), rRect.Bottom() + nYOff);
        nPY = basegfx::fround<tools::Long>(rRefPoint.Y() + ((rPt1.X() - rRefPoint.X()) * fTan));
    }

    tools::Long nOffset;
    if (nPY <= rPt1.Y())
        nOffset = (rPt1.Y() - nPY) % nDist;
    else
        nOffset = nDist - ((nPY - rPt1.Y()) % nDist);

    rPt1.AdjustY(-nOffset);
    rPt2.AdjustY(-nOffset);
}

static void lcl_CalcDiagonalHatchVerticalScan(const tools::Rectangle& rRect, tools::Long nDist,
                                              Degree10 nAngle, const Point& rRefPoint, Point& rPt1,
                                              Point& rPt2, Size& rInc, Point& rEndPt1)
{
    // "Vertical-ish" diagonals
    const double fAngle = std::abs(toRadians(nAngle));
    const double fTan = std::tan(fAngle);
    const tools::Long nXOff
        = basegfx::fround<tools::Long>((static_cast<double>(rRect.Bottom()) - rRect.Top()) / fTan);

    nDist = basegfx::fround<tools::Long>(nDist / std::sin(fAngle));
    rInc = Size(nDist, 0);

    tools::Long nPX;
    if (nAngle > 0_deg10)
    {
        rPt1 = rRect.TopLeft();
        rPt2 = Point(rRect.Left() - nXOff, rRect.Bottom());
        rEndPt1 = Point(rRect.Right() + nXOff, rRect.Top());
        nPX = basegfx::fround<tools::Long>(
            rRefPoint.X() - ((static_cast<double>(rPt1.Y()) - rRefPoint.Y()) / fTan));
    }
    else
    {
        rPt1 = rRect.BottomLeft();
        rPt2 = Point(rRect.Left() - nXOff, rRect.Top());
        rEndPt1 = Point(rRect.Right() + nXOff, rRect.Bottom());
        nPX = basegfx::fround<tools::Long>(
            rRefPoint.X() + ((static_cast<double>(rPt1.Y()) - rRefPoint.Y()) / fTan));
    }

    tools::Long nOffset;
    if (nPX <= rPt1.X())
        nOffset = (rPt1.X() - nPX) % nDist;
    else
        nOffset = nDist - ((nPX - rPt1.X()) % nDist);

    rPt1.AdjustX(-nOffset);
    rPt2.AdjustX(-nOffset);
}

static void lcl_CalcDiagonalHatch(const tools::Rectangle& rRect, tools::Long nDist, Degree10 nAngle,
                                  const Point& rRefPoint, Point& rPt1, Point& rPt2, Size& rInc,
                                  Point& rEndPt1)
{
    if (nAngle >= Degree10(-450) && nAngle <= 450_deg10)
        lcl_CalcDiagonalHatchHorizontalScan(rRect, nDist, nAngle, rRefPoint, rPt1, rPt2, rInc,
                                            rEndPt1);
    else
        lcl_CalcDiagonalHatchVerticalScan(rRect, nDist, nAngle, rRefPoint, rPt1, rPt2, rInc,
                                          rEndPt1);
}

static bool lcl_IsPointAtVertex(double fX, double fY, const Point& rPt)
{
    return (std::abs(fX - rPt.X()) <= 0.0000001) && (std::abs(fY - rPt.Y()) <= 0.0000001);
}

static bool lcl_IsCrossingAtStart(const tools::Line& rHatchLine, const tools::Line& rSegment,
                                  const tools::Polygon& rPoly, tools::Long nIndex,
                                  tools::Long nCount)
{
    // Check previous segment to see if we are crossing the boundary or just grazing it
    const tools::Line aPrevSegment(
        rPoly[static_cast<sal_uInt16>((nIndex > 1) ? (nIndex - 2) : (nCount - 1))],
        rSegment.GetStart());
    const double fPrevDistance = rHatchLine.GetDistance(aPrevSegment.GetStart());
    const double fCurDistance = rHatchLine.GetDistance(rSegment.GetEnd());

    // Valid if the polygon sides are on opposite sides of the hatch line
    return (fPrevDistance <= 0.0 && fCurDistance > 0.0)
           || (fPrevDistance > 0.0 && fCurDistance < 0.0);
}

static bool lcl_IsCrossingAtEnd(const tools::Line& rHatchLine, const tools::Line& rSegment,
                                const tools::Polygon& rPoly, tools::Long nIndex, tools::Long nCount)
{
    // Check next segment
    const tools::Line aNextSegment(rSegment.GetEnd(),
                                   rPoly[static_cast<sal_uInt16>((nIndex + 1) % nCount)]);

    return (std::abs(rHatchLine.GetDistance(aNextSegment.GetEnd())) <= 0.0000001)
           && (rHatchLine.GetDistance(rSegment.GetStart()) > 0.0);
}

static bool lcl_IsValidIntersection(const tools::Line& rHatchLine, const tools::Line& rSegment,
                                    const tools::Polygon& rPoly, tools::Long nIndex,
                                    tools::Long nCount, double fX, double fY)
{
    if (lcl_IsPointAtVertex(fX, fY, rSegment.GetStart()))
        return lcl_IsCrossingAtStart(rHatchLine, rSegment, rPoly, nIndex, nCount);

    if (lcl_IsPointAtVertex(fX, fY, rSegment.GetEnd()))
        return lcl_IsCrossingAtEnd(rHatchLine, rSegment, rPoly, nIndex, nCount);

    return true;
}

static void lcl_SortIntersections(std::vector<Point>& rPtBuffer)
{
    if (rPtBuffer.size() <= 1)
        return;

    std::sort(rPtBuffer.begin(), rPtBuffer.end(), [](const Point& rA, const Point& rB) {
        if (rA.X() != rB.X())
            return rA.X() < rB.X();
        return rA.Y() < rB.Y();
    });
}

static void lcl_CollectPolygonIntersections(const tools::Line& rHatchLine,
                                            const tools::Polygon& rPoly,
                                            std::vector<Point>& rPtBuffer)
{
    if (rPoly.GetSize() <= 1)
        return;

    double fX, fY;
    tools::Line aCurSegment(rPoly[0], Point());

    for (tools::Long i = 1, nCount = rPoly.GetSize(); i <= nCount; i++)
    {
        aCurSegment.SetEnd(rPoly[static_cast<sal_uInt16>(i % nCount)]);

        if (rHatchLine.Intersection(aCurSegment, fX, fY))
        {
            if (lcl_IsValidIntersection(rHatchLine, aCurSegment, rPoly, i, nCount, fX, fY))
            {
                rPtBuffer.emplace_back(basegfx::fround<tools::Long>(fX),
                                       basegfx::fround<tools::Long>(fY));
            }
        }

        aCurSegment.SetStart(aCurSegment.GetEnd());
    }
}

namespace vcl
{
void HatchProcessor::calcHatchValues(const tools::Rectangle& rRect, tools::Long nDist,
                                     Degree10 nAngle10, const Point& rRefPoint, Point& rPt1,
                                     Point& rPt2, Size& rInc, Point& rEndPt1)
{
    Degree10 nAngle = nAngle10 % 1800_deg10;

    if (nAngle > 900_deg10)
        nAngle -= 1800_deg10;

    if (nAngle == 0_deg10)
        lcl_CalcHorizontalHatch(rRect, nDist, rRefPoint, rPt1, rPt2, rInc, rEndPt1);
    else if (nAngle == 900_deg10)
        lcl_CalcVerticalHatch(rRect, nDist, rRefPoint, rPt1, rPt2, rInc, rEndPt1);
    else
        lcl_CalcDiagonalHatch(rRect, nDist, nAngle, rRefPoint, rPt1, rPt2, rInc, rEndPt1);
}

void HatchProcessor::collectHatchIntersections(const tools::Line& rLine,
                                               const tools::PolyPolygon& rPolyPoly,
                                               std::vector<Point>& rPtBuffer)
{
    rPtBuffer.clear();

    for (tools::Long nPoly = 0, nPolyCount = rPolyPoly.Count(); nPoly < nPolyCount; nPoly++)
    {
        lcl_CollectPolygonIntersections(rLine, rPolyPoly[static_cast<sal_uInt16>(nPoly)],
                                        rPtBuffer);
    }

    lcl_SortIntersections(rPtBuffer);
}

bool HatchProcessor::hasSaneNSteps(const Point& rPt1, const Point& rEndPt1, const Size& rInc)
{
    tools::Long nVertSteps = -1;
    if (rInc.Height())
    {
        bool bFail = o3tl::checked_sub(rEndPt1.Y(), rPt1.Y(), nVertSteps);
        if (bFail)
            nVertSteps = std::numeric_limits<tools::Long>::max();
        else
            nVertSteps = nVertSteps / rInc.Height();
    }
    tools::Long nHorzSteps = -1;
    if (rInc.Width())
    {
        bool bFail = o3tl::checked_sub(rEndPt1.X(), rPt1.X(), nHorzSteps);
        if (bFail)
            nHorzSteps = std::numeric_limits<tools::Long>::max();
        else
            nHorzSteps = nHorzSteps / rInc.Width();
    }
    auto nSteps = std::max(nVertSteps, nHorzSteps);
    if (nSteps > 1024)
    {
        SAL_WARN("vcl.gdi", "skipping slow hatch with " << nSteps << " steps");
        return false;
    }
    return true;
}

void HatchProcessor::Process(const tools::PolyPolygon& rPolyPoly, const Hatch& rHatch,
                             const tools::Rectangle& rRect, const Point& rRefPoint,
                             tools::Long nLogPixelWidth, tools::Long nWidth, Callback callback)
{
    // #i115630# DecomposeHatch does not work with beziers included in the polypolygon
    bool bIsCurve = false;
    for (sal_uInt16 a(0); !bIsCurve && a < rPolyPoly.Count(); a++)
    {
        if (rPolyPoly[a].HasFlags())
            bIsCurve = true;
    }

    if (bIsCurve)
    {
        tools::PolyPolygon aPolyPoly;
        rPolyPoly.AdaptiveSubdivide(aPolyPoly);
        Process(aPolyPoly, rHatch, rRect, rRefPoint, nLogPixelWidth, nWidth, callback);
        return;
    }

    // Single hatch
    tools::Rectangle aRect(rRect);
    aRect.AdjustLeft(-nLogPixelWidth);
    aRect.AdjustTop(-nLogPixelWidth);
    aRect.AdjustRight(nLogPixelWidth);
    aRect.AdjustBottom(nLogPixelWidth);

    Point aPt1, aPt2, aEndPt1;
    Size aInc;
    std::vector<Point> aPtBuffer;
    aPtBuffer.reserve(1024);

    calcHatchValues(aRect, nWidth, rHatch.GetAngle(), rRefPoint, aPt1, aPt2, aInc, aEndPt1);
    if (comphelper::IsFuzzing() && !hasSaneNSteps(aPt1, aEndPt1, aInc))
        return;

    if (aInc.Width() <= 0 && aInc.Height() <= 0)
        SAL_WARN("vcl.gdi", "invalid increment");
    else
    {
        do
        {
            collectHatchIntersections(tools::Line(aPt1, aPt2), rPolyPoly, aPtBuffer);

            if (aPtBuffer.size() > 1)
            {
                tools::Long nSize = static_cast<tools::Long>(aPtBuffer.size());
                if (nSize & 1)
                    nSize--;

                for (tools::Long i = 0; i < nSize; i += 2)
                    callback(aPtBuffer[i], aPtBuffer[i + 1]);
            }

            aPt1.AdjustX(aInc.Width());
            aPt1.AdjustY(aInc.Height());
            aPt2.AdjustX(aInc.Width());
            aPt2.AdjustY(aInc.Height());
        } while ((aPt1.X() <= aEndPt1.X()) && (aPt1.Y() <= aEndPt1.Y()));
    }

    if (rHatch.GetStyle() != HatchStyle::Double && rHatch.GetStyle() != HatchStyle::Triple)
        return;

    // Double hatch
    calcHatchValues(aRect, nWidth, rHatch.GetAngle() + 900_deg10, rRefPoint, aPt1, aPt2, aInc,
                    aEndPt1);
    if (comphelper::IsFuzzing() && !hasSaneNSteps(aPt1, aEndPt1, aInc))
        return;

    do
    {
        collectHatchIntersections(tools::Line(aPt1, aPt2), rPolyPoly, aPtBuffer);

        if (aPtBuffer.size() > 1)
        {
            tools::Long nSize = static_cast<tools::Long>(aPtBuffer.size());
            if (nSize & 1)
                nSize--;

            for (tools::Long i = 0; i < nSize; i += 2)
                callback(aPtBuffer[i], aPtBuffer[i + 1]);
        }

        aPt1.AdjustX(aInc.Width());
        aPt1.AdjustY(aInc.Height());
        aPt2.AdjustX(aInc.Width());
        aPt2.AdjustY(aInc.Height());
    } while ((aPt1.X() <= aEndPt1.X()) && (aPt1.Y() <= aEndPt1.Y()));

    if (rHatch.GetStyle() == HatchStyle::Triple)
    {
        // Triple hatch
        calcHatchValues(aRect, nWidth, rHatch.GetAngle() + 450_deg10, rRefPoint, aPt1, aPt2, aInc,
                        aEndPt1);
        if (comphelper::IsFuzzing() && !hasSaneNSteps(aPt1, aEndPt1, aInc))
            return;

        do
        {
            collectHatchIntersections(tools::Line(aPt1, aPt2), rPolyPoly, aPtBuffer);

            if (aPtBuffer.size() > 1)
            {
                tools::Long nSize = static_cast<tools::Long>(aPtBuffer.size());
                if (nSize & 1)
                    nSize--;

                for (tools::Long i = 0; i < nSize; i += 2)
                    callback(aPtBuffer[i], aPtBuffer[i + 1]);
            }

            aPt1.AdjustX(aInc.Width());
            aPt1.AdjustY(aInc.Height());
            aPt2.AdjustX(aInc.Width());
            aPt2.AdjustY(aInc.Height());
        } while ((aPt1.X() <= aEndPt1.X()) && (aPt1.Y() <= aEndPt1.Y()));
    }
}

} // namespace vcl

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
