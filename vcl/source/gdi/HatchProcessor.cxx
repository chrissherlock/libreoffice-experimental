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

namespace vcl
{
void HatchProcessor::calcHatchValues(const tools::Rectangle& rRect, tools::Long nDist,
                                     Degree10 nAngle10, const Point& rRefPoint, Point& rPt1,
                                     Point& rPt2, Size& rInc, Point& rEndPt1)
{
    Degree10 nAngle = nAngle10 % 1800_deg10;
    tools::Long nOffset = 0;

    if (nAngle > 900_deg10)
        nAngle -= 1800_deg10;

    if (0_deg10 == nAngle)
    {
        rInc = Size(0, nDist);
        rPt1 = rRect.TopLeft();
        rPt2 = rRect.TopRight();
        rEndPt1 = rRect.BottomLeft();

        if (rRefPoint.Y() <= rRect.Top())
            nOffset = ((rRect.Top() - rRefPoint.Y()) % nDist);
        else
            nOffset = (nDist - ((rRefPoint.Y() - rRect.Top()) % nDist));

        rPt1.AdjustY(-nOffset);
        rPt2.AdjustY(-nOffset);
    }
    else if (900_deg10 == nAngle)
    {
        rInc = Size(nDist, 0);
        rPt1 = rRect.TopLeft();
        rPt2 = rRect.BottomLeft();
        rEndPt1 = rRect.TopRight();

        if (rRefPoint.X() <= rRect.Left())
            nOffset = (rRect.Left() - rRefPoint.X()) % nDist;
        else
            nOffset = nDist - ((rRefPoint.X() - rRect.Left()) % nDist);

        rPt1.AdjustX(-nOffset);
        rPt2.AdjustX(-nOffset);
    }
    else if (nAngle >= Degree10(-450) && nAngle <= 450_deg10)
    {
        const double fAngle = std::abs(toRadians(nAngle));
        const double fTan = std::tan(fAngle);
        const tools::Long nYOff
            = basegfx::fround<tools::Long>((rRect.Right() - rRect.Left()) * fTan);
        tools::Long nPY;

        nDist = basegfx::fround<tools::Long>(nDist / std::cos(fAngle));
        rInc = Size(0, nDist);

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

        if (nPY <= rPt1.Y())
            nOffset = (rPt1.Y() - nPY) % nDist;
        else
            nOffset = nDist - ((nPY - rPt1.Y()) % nDist);

        rPt1.AdjustY(-nOffset);
        rPt2.AdjustY(-nOffset);
    }
    else
    {
        const double fAngle = std::abs(toRadians(nAngle));
        const double fTan = std::tan(fAngle);
        const tools::Long nXOff = basegfx::fround<tools::Long>(
            (static_cast<double>(rRect.Bottom()) - rRect.Top()) / fTan);
        tools::Long nPX;

        nDist = basegfx::fround<tools::Long>(nDist / std::sin(fAngle));
        rInc = Size(nDist, 0);

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

        if (nPX <= rPt1.X())
            nOffset = (rPt1.X() - nPX) % nDist;
        else
            nOffset = nDist - ((nPX - rPt1.X()) % nDist);

        rPt1.AdjustX(-nOffset);
        rPt2.AdjustX(-nOffset);
    }
}

void HatchProcessor::collectHatchIntersections(const tools::Line& rLine,
                                               const tools::PolyPolygon& rPolyPoly,
                                               std::vector<Point>& rPtBuffer)
{
    double fX, fY;
    rPtBuffer.clear();
    tools::Long nAdd;

    for (tools::Long nPoly = 0, nPolyCount = rPolyPoly.Count(); nPoly < nPolyCount; nPoly++)
    {
        const tools::Polygon& rPoly = rPolyPoly[static_cast<sal_uInt16>(nPoly)];

        if (rPoly.GetSize() > 1)
        {
            tools::Line aCurSegment(rPoly[0], Point());

            for (tools::Long i = 1, nCount = rPoly.GetSize(); i <= nCount; i++)
            {
                aCurSegment.SetEnd(rPoly[static_cast<sal_uInt16>(i % nCount)]);
                nAdd = 0;

                if (rLine.Intersection(aCurSegment, fX, fY))
                {
                    if ((std::abs(fX - aCurSegment.GetStart().X()) <= 0.0000001)
                        && (std::abs(fY - aCurSegment.GetStart().Y()) <= 0.0000001))
                    {
                        const tools::Line aPrevSegment(
                            rPoly[static_cast<sal_uInt16>((i > 1) ? (i - 2) : (nCount - 1))],
                            aCurSegment.GetStart());
                        const double fPrevDistance = rLine.GetDistance(aPrevSegment.GetStart());
                        const double fCurDistance = rLine.GetDistance(aCurSegment.GetEnd());

                        if ((fPrevDistance <= 0.0 && fCurDistance > 0.0)
                            || (fPrevDistance > 0.0 && fCurDistance < 0.0))
                        {
                            nAdd = 1;
                        }
                    }
                    else if ((std::abs(fX - aCurSegment.GetEnd().X()) <= 0.0000001)
                             && (std::abs(fY - aCurSegment.GetEnd().Y()) <= 0.0000001))
                    {
                        const tools::Line aNextSegment(
                            aCurSegment.GetEnd(), rPoly[static_cast<sal_uInt16>((i + 1) % nCount)]);

                        if ((std::abs(rLine.GetDistance(aNextSegment.GetEnd())) <= 0.0000001)
                            && (rLine.GetDistance(aCurSegment.GetStart()) > 0.0))
                        {
                            nAdd = 1;
                        }
                    }
                    else
                        nAdd = 1;

                    if (nAdd)
                    {
                        rPtBuffer.emplace_back(basegfx::fround<tools::Long>(fX),
                                               basegfx::fround<tools::Long>(fY));
                    }
                }

                aCurSegment.SetStart(aCurSegment.GetEnd());
            }
        }
    }

    if (rPtBuffer.size() <= 1)
        return;

    std::sort(rPtBuffer.begin(), rPtBuffer.end(), [](const Point& rA, const Point& rB) {
        if (rA.X() != rB.X())
            return rA.X() < rB.X();
        return rA.Y() < rB.Y();
    });
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
