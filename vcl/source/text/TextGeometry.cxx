/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <vcl/dllapi.h>

#include <tools/degree.hxx>
#include <basegfx/numeric/ftools.hxx>

#include <vcl/outdev.hxx>
#include <vcl/fntstyle.hxx>
#include <vcl/text/TextGeometry.hxx>
#include <vcl/metric.hxx>

#include <cmath>

namespace vcl::text
{
tools::Rectangle TextGeometry::AlignAndRotateTextRect(const tools::Rectangle& rTargetRect,
                                                      tools::Long nContentWidth,
                                                      tools::Long nContentHeight,
                                                      DrawTextFlags nStyle, Degree10 nOrientation)
{
    tools::Rectangle aRect = rTargetRect;

    // Horizontal Alignment
    if (nStyle & DrawTextFlags::Right)
    {
        aRect.SetLeft(aRect.Right() - nContentWidth + 1);
    }
    else if (nStyle & DrawTextFlags::Center)
    {
        aRect.AdjustLeft((rTargetRect.GetWidth() - nContentWidth) / 2);
        aRect.SetRight(aRect.Left() + nContentWidth - 1);
    }
    else
    {
        aRect.SetRight(aRect.Left() + nContentWidth - 1);
    }

    // Vertical Alignment
    if (nStyle & DrawTextFlags::Bottom)
    {
        aRect.SetTop(aRect.Bottom() - nContentHeight + 1);
    }
    else if (nStyle & DrawTextFlags::VCenter)
    {
        aRect.AdjustTop((aRect.GetHeight() - nContentHeight) / 2);
        aRect.SetBottom(aRect.Top() + nContentHeight - 1);
    }
    else
    {
        aRect.SetBottom(aRect.Top() + nContentHeight - 1);
    }

    // Rounding adjustment (legacy behavior)
    if (nStyle & DrawTextFlags::Right)
        aRect.AdjustLeft(-1);
    else
        aRect.AdjustRight(1);

    // Rotation
    if (nOrientation != 0_deg10)
    {
        tools::Polygon aRotatedPolygon(aRect);
        aRotatedPolygon.Rotate(Point(aRect.GetWidth() / 2, aRect.GetHeight() / 2), nOrientation);
        return aRotatedPolygon.GetBoundRect();
    }

    return aRect;
}

Point TextGeometry::GetRotationOrigin(const Point& rPos, const Size& rTextSize,
                                      Degree10 nOrientation, TextAlign eAlign)
{
    if (nOrientation == 0_deg10)
        return rPos;

    tools::Long nX = rPos.X();
    tools::Long nY = rPos.Y();
    tools::Long nAlignOfs = 0;

    if (eAlign == ALIGN_BOTTOM)
        nAlignOfs = -rTextSize.Height();
    else if (eAlign == ALIGN_TOP)
        nAlignOfs = rTextSize.Height();

    double fRad = toRadians(nOrientation);
    double fCos = cos(fRad);
    double fSin = sin(fRad);

    nX += basegfx::fround<tools::Long>(-nAlignOfs * fSin);
    nY += basegfx::fround<tools::Long>(nAlignOfs * fCos);

    return Point(nX, nY);
}

RotatedGeometry TextGeometry::GetRotatedGeometry(const Point& rBase,
                                                 const tools::Rectangle& rLocalRect,
                                                 Degree10 nOrientation)
{
    RotatedGeometry aGeo;
    aGeo.mbIsPolygon = false;

    // Optimization: Handle orthogonal rotations (0, 90, 180, 270)
    if (nOrientation.get() % 900 == 0)
    {
        tools::Long nX = rLocalRect.Left();
        tools::Long nY = rLocalRect.Top();
        tools::Long nW = rLocalRect.GetWidth();
        tools::Long nH = rLocalRect.GetHeight();

        // Dimension Swap for 90 and 270
        if (nOrientation.get() % 1800 != 0)
            std::swap(nW, nH);

        // Coordinate Transformation
        if (nOrientation == 900_deg10)
        {
            tools::Long nOrigX = nX;
            nX = nY;
            nY = -nOrigX - nH;
        }
        else if (nOrientation == 1800_deg10)
        {
            nX = -nX - nW;
            nY = -nY - nH;
        }
        else if (nOrientation == 2700_deg10)
        {
            tools::Long nOrigX = nX;
            nX = -nY - nW;
            nY = nOrigX;
        }

        // Apply Base Translation
        nX += rBase.X();
        nY += rBase.Y();

        aGeo.maRect = tools::Rectangle(Point(nX, nY), Size(nW, nH));
        return aGeo;
    }

    // Fallback: Arbitrary rotation
    tools::Long nX = rLocalRect.Left() + rBase.X();
    tools::Long nY = rLocalRect.Top() + rBase.Y();

    tools::Rectangle aRotRect(Point(nX, nY),
                              Size(rLocalRect.GetWidth() + 1, rLocalRect.GetHeight() + 1));

    aGeo.maPoly = tools::Polygon(aRotRect);
    aGeo.maPoly.Rotate(rBase, nOrientation);
    aGeo.mbIsPolygon = true;

    return aGeo;
}

Point TextGeometry::GetRotatedImageOrigin(const Point& rBase, const tools::Rectangle& rLocalBounds,
                                          Degree10 nOrientation)
{
    tools::Polygon aPoly(rLocalBounds);
    aPoly.Rotate(Point(0, 0), nOrientation);
    return rBase + aPoly.GetBoundRect().TopLeft();
}

tools::Long TextGeometry::GetMirroredX(const MirroringContext& rCtx)
{
    tools::Long nMirroredX = rCtx.nX;

    if (rCtx.bHasMirroredGraphics)
    {
        nMirroredX = rCtx.nGraphicsWidth - 1 - rCtx.nX;

        if (!rCtx.bIsRTL)
        {
            tools::Long nDevX = rCtx.nGraphicsWidth - rCtx.nOutputWidth - rCtx.nOutOffX;
            nMirroredX = nDevX + (rCtx.nOutputWidth - 1 - (nMirroredX - nDevX));
        }
    }
    else if (rCtx.bIsRTL)
    {
        tools::Long nDevX = rCtx.nOutOffX;
        nMirroredX = rCtx.nOutputWidth - 1 - (rCtx.nX - nDevX) + nDevX;
    }

    return nMirroredX;
}

tools::Long TextGeometry::GetReliefOffset(sal_Int32 nDPIX, FontRelief eRelief)
{
    const tools::Long nOff = 1 + (nDPIX / 300);
    return (eRelief == FontRelief::Engraved) ? -nOff : nOff;
}

tools::Long TextGeometry::GetShadowOffset(tools::Long nLineHeight, bool bIsOutline)
{
    tools::Long nOff = 1 + ((nLineHeight - 24) / 24);
    if (bIsOutline)
        nOff++;
    return nOff;
}

const std::vector<basegfx::B2DPoint>& TextGeometry::GetOutlineOffsets()
{
    static const std::vector<basegfx::B2DPoint> aOffsets{ { -1, -1 }, { +1, +1 }, { -1, 0 },
                                                          { -1, +1 }, { +0, +1 }, { +0, -1 },
                                                          { +1, -1 }, { +1, +0 } };
    return aOffsets;
}

MnemonicGeometry TextGeometry::GetMnemonicGeometry(
    std::function<double(tools::Long)> const& fnLogicWidthToDeviceSubPixel,
    std::function<tools::Long(tools::Long)> const& fnLogicWidthToDevicePixel,
    std::function<Point(const Point&)> const& fnLogicToPixel, const MnemonicDeviceParams& rParams,
    KernArraySpan aDXArray, sal_Int32 nRelPos, const Point& rLinePos, bool bTrailing)
{
    MnemonicGeometry aGeo;

    sal_Int32 lc_x1 = nRelPos ? static_cast<sal_Int32>(aDXArray[nRelPos - 1]) : 0;
    sal_Int32 lc_x2 = static_cast<sal_Int32>(aDXArray[nRelPos]);

    aGeo.nWidth = static_cast<tools::Long>(fnLogicWidthToDeviceSubPixel(std::abs(lc_x1 - lc_x2)));

    Point aTempPos = fnLogicToPixel(rLinePos);

    aGeo.nY = rParams.nOutOffY + aTempPos.Y() + fnLogicWidthToDevicePixel(rParams.nLogicalAscent);

    sal_Int32 nCharOffset = bTrailing ? std::max(lc_x1, lc_x2) : std::min(lc_x1, lc_x2);

    aGeo.nX = rParams.nOutOffX + aTempPos.X() + fnLogicWidthToDevicePixel(nCharOffset);

    return aGeo;
}

Point TextGeometry::CalculateLayoutOrigin(const OutputDevice& rDev, const tools::Rectangle& rRect,
                                          tools::Long nTextWidth, tools::Long nTextHeight,
                                          DrawTextFlags nStyle, TextAlign eAlign)
{
    Point aPos = rRect.TopLeft();
    tools::Long nWidth = rRect.GetWidth();
    tools::Long nHeight = rRect.GetHeight();

    // Horizontal text alignment
    if (nStyle & DrawTextFlags::Right)
        aPos.AdjustX(nWidth - nTextWidth);
    else if (nStyle & DrawTextFlags::Center)
        aPos.AdjustX((nWidth - nTextWidth) / 2);

    // Vertical font alignment
    if (eAlign == ALIGN_BOTTOM)
        aPos.AdjustY(nTextHeight);
    else if (eAlign == ALIGN_BASELINE)
        aPos.AdjustY(rDev.GetFontMetric().GetAscent());

    if (nStyle & DrawTextFlags::Bottom)
        aPos.AdjustY(nHeight - nTextHeight);
    else if (nStyle & DrawTextFlags::VCenter)
        aPos.AdjustY((nHeight - nTextHeight) / 2);

    return aPos;
}

} // namespace vcl::text

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
