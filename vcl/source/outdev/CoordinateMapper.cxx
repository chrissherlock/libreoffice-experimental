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

#include <basegfx/matrix/b2dhommatrix.hxx>
#include <tools/bigint.hxx>
#include <tools/debug.hxx>
#include <tools/mapunit.hxx>
#include <tools/poly.hxx>

#include <vcl/rendercontext/ImplMapRes.hxx>
#include <vcl/lineinfo.hxx>
#include <vcl/region.hxx>
#include <vcl/outdev.hxx>
#include <vcl/text/LayoutResources.hxx>

#include <CoordinateMapper.hxx>

sal_Int32 CoordinateMapper::GetDPIX() const { return mnDPIX; }

sal_Int32 CoordinateMapper::GetDPIY() const { return mnDPIY; }

void CoordinateMapper::SetDPIX(sal_Int32 nDPIX) { mnDPIX = nDPIX; }

void CoordinateMapper::SetDPIY(sal_Int32 nDPIY) { mnDPIY = nDPIY; }

sal_Int32 CoordinateMapper::GetDPIScalePercentage() const { return mnDPIScalePercentage; }

void CoordinateMapper::SetDPIScalePercentage(sal_Int32 nPercent)
{
    mnDPIScalePercentage = nPercent;
}

float CoordinateMapper::GetDPIScaleFactor() const { return mnDPIScalePercentage / 100.0f; }

tools::Long CoordinateMapper::GetOutputWidthPixel() const { return mnOutWidth; }

tools::Long CoordinateMapper::GetOutputHeightPixel() const { return mnOutHeight; }

void CoordinateMapper::SetOutputWidthPixel(tools::Long nWidth) { mnOutWidth = nWidth; }

void CoordinateMapper::SetOutputHeightPixel(tools::Long nHeight) { mnOutHeight = nHeight; }

static tools::Long lcl_pixelToLogic(tools::Long n, tools::Long nDPI, tools::Long nMapNum,
                                    tools::Long nMapDenom)
{
    assert(nDPI > 0);

    if (nMapNum == 0)
        return 0;

    sal_Int64 nDenom = nDPI * nMapNum;
    sal_Int64 n64 = n * nMapDenom;

    if (nDenom == 1)
        return static_cast<tools::Long>(n64);

    n64 = 2 * n64 / nDenom;

    if (n64 < 0)
        --n64;
    else
        ++n64;

    return static_cast<tools::Long>(n64 / 2);
}

void CoordinateMapper::SetPixelOffset(const Size& rSize)
{
    mnOutOffOrigX = rSize.Width();
    mnOutOffOrigY = rSize.Height();
}

void CoordinateMapper::SetLogicalOffset(const Size& rSize)
{
    mnOutOffLogicX = rSize.Width();
    mnOutOffLogicY = rSize.Height();
}

void CoordinateMapper::SetOffset(const Size& rOffset)
{
    SetPixelOffset(rOffset);
    // Recalculate logical offset based on the new pixel offset
    SetLogicalOffset(Size(lcl_pixelToLogic(GetPixelXOffset(), GetDPIX(), GetMappingXNumerator(),
                                           GetMappingXDenominator()),
                          lcl_pixelToLogic(GetPixelYOffset(), GetDPIY(), GetMappingYNumerator(),
                                           GetMappingYDenominator())));
}

tools::Long CoordinateMapper::GetOutOffXPixel() const { return mnOutOffX; }

tools::Long CoordinateMapper::GetOutOffYPixel() const { return mnOutOffY; }

void CoordinateMapper::SetDeviceOriginX(tools::Long nX) { mnOutOffX = nX; }

void CoordinateMapper::SetDeviceOriginY(tools::Long nY) { mnOutOffY = nY; }

Point CoordinateMapper::GetOutputOffPixel() const { return Point(mnOutOffX, mnOutOffY); }

void CoordinateMapper::CalcMapResolution(const MapMode& rMapMode, tools::Long nDPIX,
                                         tools::Long nDPIY)
{
    maMapRes.CalcMapResolution(rMapMode, nDPIX, nDPIY);
}

ImplMapRes CoordinateMapper::ResolveMapRes(const MapMode* pMode, const MapMode& rDefaultMapMode,
                                           bool bMap, tools::Long nDPIX, tools::Long nDPIY) const
{
    return maMapRes.ResolveMapRes(pMode, rDefaultMapMode, bMap, nDPIX, nDPIY);
}

// #i75163#
void CoordinateMapper::InvalidateViewTransform()
{
    if (mpViewTransform)
    {
        delete mpViewTransform;
        mpViewTransform = nullptr;
    }

    if (mpInverseViewTransform)
    {
        delete mpInverseViewTransform;
        mpInverseViewTransform = nullptr;
    }
}

basegfx::B2DHomMatrix CoordinateMapper::GetDeviceTransformation() const
{
    basegfx::B2DHomMatrix aTransformation = GetViewTransformation();

    // TODO: is it worth to cache the transformed result?
    if (mnOutOffX || mnOutOffY)
        aTransformation.translate(mnOutOffX, mnOutOffY);

    return aTransformation;
}

basegfx::B2DHomMatrix CoordinateMapper::GetViewTransformation() const
{
    if (!IsMapModeEnabled())
        return basegfx::B2DHomMatrix();

    if (mpViewTransform)
        return *mpViewTransform;

    mpViewTransform = new basegfx::B2DHomMatrix;

    const double fScaleFactorX(static_cast<double>(GetDPIX())
                               * static_cast<double>(GetMappingXNumerator())
                               / static_cast<double>(GetMappingXDenominator()));
    const double fScaleFactorY(static_cast<double>(GetDPIY())
                               * static_cast<double>(GetMappingYNumerator())
                               / static_cast<double>(GetMappingYDenominator()));
    const double fZeroPointX((static_cast<double>(GetMappingXOffset()) * fScaleFactorX)
                             + static_cast<double>(GetPixelXOffset()));
    const double fZeroPointY((static_cast<double>(GetMappingYOffset()) * fScaleFactorY)
                             + static_cast<double>(GetPixelYOffset()));

    mpViewTransform->set(0, 0, fScaleFactorX);
    mpViewTransform->set(1, 1, fScaleFactorY);
    mpViewTransform->set(0, 2, fZeroPointX);
    mpViewTransform->set(1, 2, fZeroPointY);

    return *mpViewTransform;
}

basegfx::B2DHomMatrix CoordinateMapper::GetInverseViewTransformation() const
{
    if (!IsMapModeEnabled())
        return basegfx::B2DHomMatrix();

    if (mpInverseViewTransform)
        return *mpInverseViewTransform;

    GetViewTransformation();

    mpInverseViewTransform = new basegfx::B2DHomMatrix(*mpViewTransform);
    mpInverseViewTransform->invert();

    return *mpInverseViewTransform;
}

basegfx::B2DHomMatrix CoordinateMapper::GetViewTransformation(const MapMode& rMapMode) const
{
    // #i82615#
    ImplMapRes aMapRes;
    aMapRes.CalcMapResolution(rMapMode, GetDPIX(), GetDPIY());

    basegfx::B2DHomMatrix aTransform;

    const double fScaleFactorX(static_cast<double>(GetDPIX())
                               * static_cast<double>(aMapRes.mnMapScNumX)
                               / static_cast<double>(aMapRes.mnMapScDenomX));
    const double fScaleFactorY(static_cast<double>(GetDPIY())
                               * static_cast<double>(aMapRes.mnMapScNumY)
                               / static_cast<double>(aMapRes.mnMapScDenomY));
    const double fZeroPointX((static_cast<double>(aMapRes.mnMapOfsX) * fScaleFactorX)
                             + static_cast<double>(GetPixelXOffset()));
    const double fZeroPointY((static_cast<double>(aMapRes.mnMapOfsY) * fScaleFactorY)
                             + static_cast<double>(GetPixelYOffset()));

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

static double lcl_logicToSubPixel(tools::Long n, tools::Long nDPI, tools::Long nMapNum,
                                  tools::Long nMapDenom)
{
    assert(nDPI > 0);
    assert(nMapDenom != 0);
    return static_cast<double>(n) * nMapNum * nDPI / nMapDenom;
}

static double lcl_pixelToLogicDouble(double n, tools::Long nDPI, tools::Long nMapNum,
                                     tools::Long nMapDenom)
{
    assert(nDPI > 0);
    if (nMapNum == 0)
        return 0;

    n *= nMapDenom;
    n /= nDPI;
    n /= nMapNum;
    return n;
}

double CoordinateMapper::LogicHeightToDeviceSubPixel(tools::Long nHeight) const
{
    if (!IsMapModeEnabled())
        return nHeight;

    return lcl_logicToSubPixel(nHeight, GetDPIY(), GetMappingYNumerator(),
                               GetMappingYDenominator());
}

double CoordinateMapper::LogicWidthToDeviceSubPixel(tools::Long nWidth) const
{
    if (!IsMapModeEnabled())
        return nWidth;

    return lcl_logicToSubPixel(nWidth, GetDPIX(), GetMappingXNumerator(), GetMappingXDenominator());
}

static tools::Long lcl_logicToPixel(tools::Long n, tools::Long nDPI, tools::Long nMapNum,
                                    tools::Long nMapDenom)
{
    assert(nDPI > 0);
    assert(nMapDenom != 0);

    if constexpr (sizeof(tools::Long) >= 8)
    {
        assert(nMapNum >= 0);
        //detect overflows
        assert(nMapNum == 0
               || std::abs(n) < std::numeric_limits<tools::Long>::max() / nMapNum / nDPI);
    }

    sal_Int64 n64 = n * nMapNum * nDPI;

    if (nMapDenom == 1)
        return static_cast<tools::Long>(n64);

    n64 = 2 * n64 / nMapDenom;

    if (n64 < 0)
        --n64;
    else
        ++n64;

    return static_cast<tools::Long>(n64 / 2);
}

tools::Long CoordinateMapper::LogicXToDevicePixel(tools::Long nX) const
{
    if (!IsMapModeEnabled())
        return nX + GetOutOffXPixel();

    return lcl_logicToPixel(nX + GetMappingXOffset(), GetDPIX(), GetMappingXNumerator(),
                            GetMappingXDenominator())
           + GetOutOffXPixel() + GetPixelXOffset();
}

tools::Long CoordinateMapper::LogicYToDevicePixel(tools::Long nY) const
{
    if (!IsMapModeEnabled())
        return nY + GetOutOffYPixel();

    return lcl_logicToPixel(nY + GetMappingYOffset(), GetDPIY(), GetMappingYNumerator(),
                            GetMappingYDenominator())
           + GetOutOffYPixel() + GetPixelYOffset();
}

tools::Long CoordinateMapper::LogicHeightToDevicePixel(tools::Long nHeight) const
{
    if (!IsMapModeEnabled())
        return nHeight;

    return lcl_logicToPixel(nHeight, GetDPIY(), GetMappingYNumerator(), GetMappingYDenominator());
}

tools::Long CoordinateMapper::LogicWidthToDevicePixel(tools::Long nWidth) const
{
    if (!IsMapModeEnabled())
        return nWidth;

    return lcl_logicToPixel(nWidth, GetDPIX(), GetMappingXNumerator(), GetMappingXDenominator());
}

tools::Long CoordinateMapper::DevicePixelToLogicWidth(tools::Long nWidth) const
{
    if (!IsMapModeEnabled())
        return nWidth;

    return lcl_pixelToLogic(nWidth, GetDPIX(), GetMappingXNumerator(), GetMappingXDenominator());
}

SAL_DLLPRIVATE double CoordinateMapper::DevicePixelToLogicWidthDouble(double nWidth) const
{
    if (!IsMapModeEnabled())
        return nWidth;

    return lcl_pixelToLogicDouble(nWidth, GetDPIX(), GetMappingXNumerator(),
                                  GetMappingXDenominator());
}

tools::Long CoordinateMapper::DevicePixelToLogicHeight(tools::Long nHeight) const
{
    if (!IsMapModeEnabled())
        return nHeight;

    return lcl_pixelToLogic(nHeight, GetDPIY(), GetMappingYNumerator(), GetMappingYDenominator());
}

double CoordinateMapper::DevicePixelToLogicHeightDouble(double nHeight) const
{
    if (!IsMapModeEnabled())
        return nHeight;

    return lcl_pixelToLogicDouble(nHeight, GetDPIY(), GetMappingYNumerator(),
                                  GetMappingYDenominator());
}

Point CoordinateMapper::LogicToDevicePixel(const Point& rLogicPt) const
{
    if (!IsMapModeEnabled())
        return Point(rLogicPt.X() + GetOutOffXPixel(), rLogicPt.Y() + GetOutOffYPixel());

    return Point(lcl_logicToPixel(rLogicPt.X() + GetMappingXOffset(), GetDPIX(),
                                  GetMappingXNumerator(), GetMappingXDenominator())
                     + GetOutOffXPixel() + GetPixelXOffset(),
                 lcl_logicToPixel(rLogicPt.Y() + GetMappingYOffset(), GetDPIY(),
                                  GetMappingYNumerator(), GetMappingYDenominator())
                     + GetOutOffYPixel() + GetPixelYOffset());
}

Size CoordinateMapper::LogicToDevicePixel(const Size& rLogicSize) const
{
    if (!IsMapModeEnabled())
        return rLogicSize;

    return Size(lcl_logicToPixel(rLogicSize.Width(), GetDPIX(), GetMappingXNumerator(),
                                 GetMappingXDenominator()),
                lcl_logicToPixel(rLogicSize.Height(), GetDPIY(), GetMappingYNumerator(),
                                 GetMappingYDenominator()));
}

tools::Rectangle CoordinateMapper::LogicToDevicePixel(const tools::Rectangle& rLogicRect) const
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
    tools::Rectangle aRetval;

    if (!IsMapModeEnabled())
    {
        aRetval = tools::Rectangle(
            rLogicRect.Left() + GetOutOffXPixel(), rLogicRect.Top() + GetOutOffYPixel(),
            rLogicRect.IsWidthEmpty() ? 0 : rLogicRect.Right() + GetOutOffXPixel(),
            rLogicRect.IsHeightEmpty() ? 0 : rLogicRect.Bottom() + GetOutOffYPixel());
    }
    else
    {
        aRetval = tools::Rectangle(
            lcl_logicToPixel(rLogicRect.Left() + GetMappingXOffset(), GetDPIX(),
                             GetMappingXNumerator(), GetMappingXDenominator())
                + GetOutOffXPixel() + GetPixelXOffset(),
            lcl_logicToPixel(rLogicRect.Top() + GetMappingYOffset(), GetDPIY(),
                             GetMappingYNumerator(), GetMappingYDenominator())
                + GetOutOffYPixel() + GetPixelYOffset(),
            rLogicRect.IsWidthEmpty()
                ? 0
                : lcl_logicToPixel(rLogicRect.Right() + GetMappingXOffset(), GetDPIX(),
                                   GetMappingXNumerator(), GetMappingXDenominator())
                      + GetOutOffXPixel() + GetPixelXOffset(),
            rLogicRect.IsHeightEmpty()
                ? 0
                : lcl_logicToPixel(rLogicRect.Bottom() + GetMappingYOffset(), GetDPIY(),
                                   GetMappingYNumerator(), GetMappingYDenominator())
                      + GetOutOffYPixel() + GetPixelYOffset());
    }

    if (rLogicRect.IsWidthEmpty())
        aRetval.SetWidthEmpty();

    if (rLogicRect.IsHeightEmpty())
        aRetval.SetHeightEmpty();

    return aRetval;
}

tools::Polygon CoordinateMapper::LogicToDevicePixel(const tools::Polygon& rLogicPoly) const
{
    if (!IsMapModeEnabled() && !GetOutOffXPixel() && !GetOutOffYPixel())
        return rLogicPoly;

    const sal_uInt16 nPoints = rLogicPoly.GetSize();
    tools::Polygon aPoly(rLogicPoly);

    // get pointer to Point-array (copy data)
    const Point* pPointAry = aPoly.GetConstPointAry();

    if (IsMapModeEnabled())
    {
        for (sal_uInt16 i = 0; i < nPoints; i++)
        {
            const Point& rPt = pPointAry[i];
            Point aPt(lcl_logicToPixel(rPt.X() + GetMappingXOffset(), GetDPIX(),
                                       GetMappingXNumerator(), GetMappingXDenominator())
                          + GetOutOffXPixel() + GetPixelXOffset(),
                      lcl_logicToPixel(rPt.Y() + GetMappingYOffset(), GetDPIY(),
                                       GetMappingYNumerator(), GetMappingYDenominator())
                          + GetOutOffYPixel() + GetPixelYOffset());
            aPoly[i] = aPt;
        }

        return aPoly;
    }

    for (sal_uInt16 i = 0; i < nPoints; i++)
    {
        Point aPt = pPointAry[i];
        aPt.AdjustX(GetOutOffXPixel());
        aPt.AdjustY(GetOutOffYPixel());
        aPoly[i] = aPt;
    }

    return aPoly;
}

basegfx::B2DPolygon
CoordinateMapper::LogicToDevicePixel(const basegfx::B2DPolygon& rLogicPoly) const
{
    if (!IsMapModeEnabled() && !GetOutOffXPixel() && !GetOutOffYPixel())
        return rLogicPoly;

    const sal_uInt32 nPoints = rLogicPoly.count();
    basegfx::B2DPolygon aPoly(rLogicPoly);

    basegfx::B2DPoint aC1;
    basegfx::B2DPoint aC2;

    if (!IsMapModeEnabled())
    {
        for (sal_uInt32 i = 0; i < nPoints; ++i)
        {
            const basegfx::B2DPoint& rPt = aPoly.getB2DPoint(i);
            basegfx::B2DPoint aPt(rPt.getX() + GetOutOffXPixel(), rPt.getY() + GetOutOffYPixel());

            const bool bC1 = aPoly.isPrevControlPointUsed(i);
            if (bC1)
            {
                const basegfx::B2DPoint aB2DC1(aPoly.getPrevControlPoint(i));

                aC1 = basegfx::B2DPoint(aB2DC1.getX() + GetOutOffXPixel(),
                                        aB2DC1.getY() + GetOutOffYPixel());
            }

            const bool bC2 = aPoly.isNextControlPointUsed(i);
            if (bC2)
            {
                const basegfx::B2DPoint aB2DC2(aPoly.getNextControlPoint(i));

                aC1 = basegfx::B2DPoint(aB2DC2.getX() + GetOutOffXPixel(),
                                        aB2DC2.getY() + GetOutOffYPixel());
            }

            aPoly.setB2DPoint(i, aPt);

            if (bC1)
                aPoly.setPrevControlPoint(i, aC1);

            if (bC2)
                aPoly.setNextControlPoint(i, aC2);
        }

        return aPoly;
    }

    for (sal_uInt32 i = 0; i < nPoints; ++i)
    {
        const basegfx::B2DPoint& rPt = aPoly.getB2DPoint(i);
        basegfx::B2DPoint aPt(lcl_logicToPixel(rPt.getX() + GetMappingXOffset(), GetDPIX(),
                                               GetMappingXNumerator(), GetMappingXDenominator())
                                  + GetOutOffXPixel() + GetPixelXOffset(),
                              lcl_logicToPixel(rPt.getY() + GetMappingYOffset(), GetDPIY(),
                                               GetMappingYNumerator(), GetMappingYDenominator())
                                  + GetOutOffYPixel() + GetPixelYOffset());

        const bool bC1 = aPoly.isPrevControlPointUsed(i);
        if (bC1)
        {
            const basegfx::B2DPoint aB2DC1(aPoly.getPrevControlPoint(i));

            aC1 = basegfx::B2DPoint(
                lcl_logicToPixel(aB2DC1.getX() + GetMappingXOffset(), GetDPIX(),
                                 GetMappingXNumerator(), GetMappingXDenominator())
                    + GetOutOffXPixel() + GetPixelXOffset(),
                lcl_logicToPixel(aB2DC1.getY() + GetMappingYOffset(), GetDPIY(),
                                 GetMappingYNumerator(), GetMappingYDenominator())
                    + GetOutOffYPixel() + GetPixelYOffset());
        }

        const bool bC2 = aPoly.isNextControlPointUsed(i);
        if (bC2)
        {
            const basegfx::B2DPoint aB2DC2(aPoly.getNextControlPoint(i));

            aC2 = basegfx::B2DPoint(
                lcl_logicToPixel(aB2DC2.getX() + GetMappingXOffset(), GetDPIX(),
                                 GetMappingXNumerator(), GetMappingXDenominator())
                    + GetOutOffXPixel() + GetPixelXOffset(),
                lcl_logicToPixel(aB2DC2.getY() + GetMappingYOffset(), GetDPIY(),
                                 GetMappingYNumerator(), GetMappingYDenominator())
                    + GetOutOffYPixel() + GetPixelYOffset());
        }

        aPoly.setB2DPoint(i, aPt);

        if (bC1)
            aPoly.setPrevControlPoint(i, aC1);

        if (bC2)
            aPoly.setNextControlPoint(i, aC2);
    }

    return aPoly;
}

tools::PolyPolygon
CoordinateMapper::LogicToDevicePixel(const tools::PolyPolygon& rLogicPolyPoly) const
{
    if (!IsMapModeEnabled() && !GetOutOffXPixel() && !GetOutOffYPixel())
        return rLogicPolyPoly;

    tools::PolyPolygon aPolyPoly(rLogicPolyPoly);
    const sal_uInt16 nPoly = aPolyPoly.Count();

    for (sal_uInt16 i = 0; i < nPoly; i++)
    {
        tools::Polygon& rPoly = aPolyPoly[i];
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

tools::Rectangle CoordinateMapper::DevicePixelToLogic(const tools::Rectangle& rPixelRect) const
{
    // tdf#141761 see comments above, IsEmpty() removed
    tools::Rectangle aRetval;

    if (!IsMapModeEnabled())
    {
        aRetval = tools::Rectangle(
            rPixelRect.Left() - GetOutOffXPixel(), rPixelRect.Top() - GetOutOffYPixel(),
            rPixelRect.IsWidthEmpty() ? 0 : rPixelRect.Right() - GetOutOffXPixel(),
            rPixelRect.IsHeightEmpty() ? 0 : rPixelRect.Bottom() - GetOutOffYPixel());
    }
    else
    {
        aRetval = tools::Rectangle(
            lcl_pixelToLogic(rPixelRect.Left() - GetOutOffXPixel() - GetPixelXOffset(), GetDPIX(),
                             GetMappingXNumerator(), GetMappingXDenominator())
                - GetMappingXOffset(),
            lcl_pixelToLogic(rPixelRect.Top() - GetOutOffYPixel() - GetPixelYOffset(), GetDPIY(),
                             GetMappingYNumerator(), GetMappingYDenominator())
                - GetMappingYOffset(),
            rPixelRect.IsWidthEmpty()
                ? 0
                : lcl_pixelToLogic(rPixelRect.Right() - GetOutOffXPixel() - GetPixelXOffset(),
                                   GetDPIX(), GetMappingXNumerator(), GetMappingXDenominator())
                      - GetMappingXOffset(),
            rPixelRect.IsHeightEmpty()
                ? 0
                : lcl_pixelToLogic(rPixelRect.Bottom() - GetOutOffYPixel() - GetPixelYOffset(),
                                   GetDPIY(), GetMappingYNumerator(), GetMappingYDenominator())
                      - GetMappingYOffset());
    }

    if (rPixelRect.IsWidthEmpty())
        aRetval.SetWidthEmpty();

    if (rPixelRect.IsHeightEmpty())
        aRetval.SetHeightEmpty();

    return aRetval;
}

vcl::Region CoordinateMapper::PixelToDevicePixel(const vcl::Region& rRegion) const
{
    if (!GetOutOffXPixel() && !GetOutOffYPixel())
        return rRegion;

    vcl::Region aRegion(rRegion);
    aRegion.Move(GetOutOffXPixel() + GetPixelXOffset(), GetOutOffYPixel() + GetPixelYOffset());
    return aRegion;
}

Point CoordinateMapper::LogicToPixel(const Point& rLogicPt) const
{
    if (!IsMapModeEnabled())
        return rLogicPt;

    return Point(lcl_logicToPixel(rLogicPt.X() + GetMappingXOffset(), GetDPIX(),
                                  GetMappingXNumerator(), GetMappingXDenominator())
                     + GetPixelXOffset(),
                 lcl_logicToPixel(rLogicPt.Y() + GetMappingYOffset(), GetDPIY(),
                                  GetMappingYNumerator(), GetMappingYDenominator())
                     + GetPixelYOffset());
}

Size CoordinateMapper::LogicToPixel(const Size& rLogicSize) const
{
    if (!IsMapModeEnabled())
        return rLogicSize;

    return Size(lcl_logicToPixel(rLogicSize.Width(), GetDPIX(), GetMappingXNumerator(),
                                 GetMappingXDenominator()),
                lcl_logicToPixel(rLogicSize.Height(), GetDPIY(), GetMappingYNumerator(),
                                 GetMappingYDenominator()));
}

tools::Rectangle CoordinateMapper::LogicToPixel(const tools::Rectangle& rLogicRect) const
{
    if (!IsMapModeEnabled())
        return rLogicRect;

    tools::Rectangle aRetval(
        lcl_logicToPixel(rLogicRect.Left() + GetMappingXOffset(), GetDPIX(), GetMappingXNumerator(),
                         GetMappingXDenominator())
            + GetPixelXOffset(),
        lcl_logicToPixel(rLogicRect.Top() + GetMappingYOffset(), GetDPIY(), GetMappingYNumerator(),
                         GetMappingYDenominator())
            + GetPixelYOffset(),
        rLogicRect.IsWidthEmpty()
            ? 0
            : lcl_logicToPixel(rLogicRect.Right() + GetMappingXOffset(), GetDPIX(),
                               GetMappingXNumerator(), GetMappingXDenominator())
                  + GetPixelXOffset(),
        rLogicRect.IsHeightEmpty()
            ? 0
            : lcl_logicToPixel(rLogicRect.Bottom() + GetMappingYOffset(), GetDPIY(),
                               GetMappingYNumerator(), GetMappingYDenominator())
                  + GetPixelYOffset());

    if (rLogicRect.IsWidthEmpty())
        aRetval.SetWidthEmpty();

    if (rLogicRect.IsHeightEmpty())
        aRetval.SetHeightEmpty();

    return aRetval;
}

tools::Polygon CoordinateMapper::LogicToPixel(const tools::Polygon& rLogicPoly) const
{
    if (!IsMapModeEnabled())
        return rLogicPoly;

    sal_uInt16 i;
    sal_uInt16 nPoints = rLogicPoly.GetSize();
    tools::Polygon aPoly(rLogicPoly);

    // get pointer to Point-array (copy data)
    const Point* pPointAry = aPoly.GetConstPointAry();

    for (i = 0; i < nPoints; i++)
    {
        const Point* pPt = &(pPointAry[i]);
        Point aPt;
        aPt.setX(lcl_logicToPixel(pPt->X() + GetMappingXOffset(), GetDPIX(), GetMappingXNumerator(),
                                  GetMappingXDenominator())
                 + GetPixelXOffset());
        aPt.setY(lcl_logicToPixel(pPt->Y() + GetMappingYOffset(), GetDPIY(), GetMappingYNumerator(),
                                  GetMappingYDenominator())
                 + GetPixelYOffset());
        aPoly[i] = aPt;
    }

    return aPoly;
}

tools::PolyPolygon CoordinateMapper::LogicToPixel(const tools::PolyPolygon& rLogicPolyPoly) const
{
    if (!IsMapModeEnabled())
        return rLogicPolyPoly;

    tools::PolyPolygon aPolyPoly(rLogicPolyPoly);
    sal_uInt16 nPoly = aPolyPoly.Count();
    for (sal_uInt16 i = 0; i < nPoly; i++)
    {
        tools::Polygon& rPoly = aPolyPoly[i];
        rPoly = LogicToPixel(rPoly);
    }
    return aPolyPoly;
}

basegfx::B2DPolyPolygon
CoordinateMapper::LogicToPixel(const basegfx::B2DPolyPolygon& rLogicPolyPoly) const
{
    basegfx::B2DPolyPolygon aTransformedPoly = rLogicPolyPoly;
    const basegfx::B2DHomMatrix aTransformationMatrix = GetViewTransformation();
    aTransformedPoly.transform(aTransformationMatrix);
    return aTransformedPoly;
}

vcl::Region CoordinateMapper::LogicToPixel(const vcl::Region& rLogicRegion) const
{
    if (!IsMapModeEnabled() || rLogicRegion.IsNull() || rLogicRegion.IsEmpty())
        return rLogicRegion;

    if (rLogicRegion.getB2DPolyPolygon())
        return vcl::Region(LogicToPixel(*rLogicRegion.getB2DPolyPolygon()));

    if (rLogicRegion.getPolyPolygon())
        return vcl::Region(LogicToPixel(*rLogicRegion.getPolyPolygon()));

    if (!rLogicRegion.getRegionBand())
        return vcl::Region();

    RectangleVector aRectangles;
    rLogicRegion.GetRegionRectangles(aRectangles);
    const RectangleVector& rRectangles(aRectangles); // needed to make the '!=' work

    vcl::Region aRegion;

    for (RectangleVector::const_reverse_iterator aRectIter(rRectangles.rbegin());
         aRectIter != rRectangles.rend(); ++aRectIter)
    {
        aRegion.Union(LogicToPixel(*aRectIter));
    }

    return aRegion;
}

Point CoordinateMapper::LogicToPixel(const Point& rLogicPt, const MapMode& rMapMode) const
{
    if (rMapMode.IsDefault())
        return rLogicPt;

    ImplMapRes aMapRes;
    aMapRes.CalcMapResolution(rMapMode, GetDPIX(), GetDPIY());

    return Point(lcl_logicToPixel(rLogicPt.X() + aMapRes.mnMapOfsX, GetDPIX(), aMapRes.mnMapScNumX,
                                  aMapRes.mnMapScDenomX)
                     + GetPixelXOffset(),
                 lcl_logicToPixel(rLogicPt.Y() + aMapRes.mnMapOfsY, GetDPIY(), aMapRes.mnMapScNumY,
                                  aMapRes.mnMapScDenomY)
                     + GetPixelYOffset());
}

Size CoordinateMapper::LogicToPixel(const Size& rLogicSize, const MapMode& rMapMode) const
{
    if (rMapMode.IsDefault())
        return rLogicSize;

    ImplMapRes aMapRes;
    aMapRes.CalcMapResolution(rMapMode, GetDPIX(), GetDPIY());

    return Size(
        lcl_logicToPixel(rLogicSize.Width(), GetDPIX(), aMapRes.mnMapScNumX, aMapRes.mnMapScDenomX),
        lcl_logicToPixel(rLogicSize.Height(), GetDPIY(), aMapRes.mnMapScNumY,
                         aMapRes.mnMapScDenomY));
}

tools::Rectangle CoordinateMapper::LogicToPixel(const tools::Rectangle& rLogicRect,
                                                const MapMode& rMapMode) const
{
    if (rMapMode.IsDefault())
        return rLogicRect;

    ImplMapRes aMapRes;
    aMapRes.CalcMapResolution(rMapMode, GetDPIX(), GetDPIY());

    tools::Rectangle aRetval(
        lcl_logicToPixel(rLogicRect.Left() + aMapRes.mnMapOfsX, GetDPIX(), aMapRes.mnMapScNumX,
                         aMapRes.mnMapScDenomX)
            + GetPixelXOffset(),
        lcl_logicToPixel(rLogicRect.Top() + aMapRes.mnMapOfsY, GetDPIY(), aMapRes.mnMapScNumY,
                         aMapRes.mnMapScDenomY)
            + GetPixelYOffset(),
        rLogicRect.IsWidthEmpty()
            ? 0
            : lcl_logicToPixel(rLogicRect.Right() + aMapRes.mnMapOfsX, GetDPIX(),
                               aMapRes.mnMapScNumX, aMapRes.mnMapScDenomX)
                  + GetPixelXOffset(),
        rLogicRect.IsHeightEmpty()
            ? 0
            : lcl_logicToPixel(rLogicRect.Bottom() + aMapRes.mnMapOfsY, GetDPIY(),
                               aMapRes.mnMapScNumY, aMapRes.mnMapScDenomY)
                  + GetPixelYOffset());

    if (rLogicRect.IsWidthEmpty())
        aRetval.SetWidthEmpty();

    if (rLogicRect.IsHeightEmpty())
        aRetval.SetHeightEmpty();

    return aRetval;
}

tools::Polygon CoordinateMapper::LogicToPixel(const tools::Polygon& rLogicPoly,
                                              const MapMode& rMapMode) const
{
    if (rMapMode.IsDefault())
        return rLogicPoly;

    ImplMapRes aMapRes;
    aMapRes.CalcMapResolution(rMapMode, GetDPIX(), GetDPIY());

    sal_uInt16 i;
    sal_uInt16 nPoints = rLogicPoly.GetSize();
    tools::Polygon aPoly(rLogicPoly);

    const Point* pPointAry = aPoly.GetConstPointAry();

    for (i = 0; i < nPoints; i++)
    {
        const Point* pPt = &(pPointAry[i]);
        Point aPt;
        aPt.setX(lcl_logicToPixel(pPt->X() + aMapRes.mnMapOfsX, GetDPIX(), aMapRes.mnMapScNumX,
                                  aMapRes.mnMapScDenomX)
                 + GetPixelXOffset());
        aPt.setY(lcl_logicToPixel(pPt->Y() + aMapRes.mnMapOfsY, GetDPIY(), aMapRes.mnMapScNumY,
                                  aMapRes.mnMapScDenomY)
                 + GetPixelYOffset());
        aPoly[i] = aPt;
    }

    return aPoly;
}

basegfx::B2DPolyPolygon
CoordinateMapper::LogicToPixel(const basegfx::B2DPolyPolygon& rLogicPolyPoly,
                               const MapMode& rMapMode) const
{
    basegfx::B2DPolyPolygon aTransformedPoly = rLogicPolyPoly;
    const basegfx::B2DHomMatrix aTransformationMatrix = GetViewTransformation(rMapMode);
    aTransformedPoly.transform(aTransformationMatrix);
    return aTransformedPoly;
}

Point CoordinateMapper::PixelToLogic(const Point& rDevicePt) const
{
    if (!IsMapModeEnabled())
        return rDevicePt;

    return Point(
        lcl_pixelToLogic(rDevicePt.X(), GetDPIX(), GetMappingXNumerator(), GetMappingXDenominator())
            - GetMappingXOffset() - GetLogicalXOffset(),
        lcl_pixelToLogic(rDevicePt.Y(), GetDPIY(), GetMappingYNumerator(), GetMappingYDenominator())
            - GetMappingYOffset() - GetLogicalYOffset());
}

Size CoordinateMapper::PixelToLogic(const Size& rDeviceSize) const
{
    if (!IsMapModeEnabled())
        return rDeviceSize;

    return Size(lcl_pixelToLogic(rDeviceSize.Width(), GetDPIX(), GetMappingXNumerator(),
                                 GetMappingXDenominator()),
                lcl_pixelToLogic(rDeviceSize.Height(), GetDPIY(), GetMappingYNumerator(),
                                 GetMappingYDenominator()));
}

tools::Rectangle CoordinateMapper::PixelToLogic(const tools::Rectangle& rDeviceRect) const
{
    if (!IsMapModeEnabled())
        return rDeviceRect;

    tools::Rectangle aRetval(
        lcl_pixelToLogic(rDeviceRect.Left(), GetDPIX(), GetMappingXNumerator(),
                         GetMappingXDenominator())
            - GetMappingXOffset() - GetLogicalXOffset(),
        lcl_pixelToLogic(rDeviceRect.Top(), GetDPIY(), GetMappingYNumerator(),
                         GetMappingYDenominator())
            - GetMappingYOffset() - GetLogicalYOffset(),
        rDeviceRect.IsWidthEmpty()
            ? 0
            : lcl_pixelToLogic(rDeviceRect.Right(), GetDPIX(), GetMappingXNumerator(),
                               GetMappingXDenominator())
                  - GetMappingXOffset() - GetLogicalXOffset(),
        rDeviceRect.IsHeightEmpty()
            ? 0
            : lcl_pixelToLogic(rDeviceRect.Bottom(), GetDPIY(), GetMappingYNumerator(),
                               GetMappingYDenominator())
                  - GetMappingYOffset() - GetLogicalYOffset());

    if (rDeviceRect.IsWidthEmpty())
        aRetval.SetWidthEmpty();

    if (rDeviceRect.IsHeightEmpty())
        aRetval.SetHeightEmpty();

    return aRetval;
}

tools::Polygon CoordinateMapper::PixelToLogic(const tools::Polygon& rDevicePoly) const
{
    if (!IsMapModeEnabled())
        return rDevicePoly;

    sal_uInt16 i;
    sal_uInt16 nPoints = rDevicePoly.GetSize();
    tools::Polygon aPoly(rDevicePoly);

    const Point* pPointAry = aPoly.GetConstPointAry();

    for (i = 0; i < nPoints; i++)
    {
        const Point* pPt = &(pPointAry[i]);
        Point aPt;
        aPt.setX(
            lcl_pixelToLogic(pPt->X(), GetDPIX(), GetMappingXNumerator(), GetMappingXDenominator())
            - GetMappingXOffset() - GetLogicalXOffset());
        aPt.setY(
            lcl_pixelToLogic(pPt->Y(), GetDPIY(), GetMappingYNumerator(), GetMappingYDenominator())
            - GetMappingYOffset() - GetLogicalYOffset());
        aPoly[i] = aPt;
    }

    return aPoly;
}

tools::PolyPolygon CoordinateMapper::PixelToLogic(const tools::PolyPolygon& rDevicePolyPoly) const
{
    if (!IsMapModeEnabled())
        return rDevicePolyPoly;

    tools::PolyPolygon aPolyPoly(rDevicePolyPoly);
    sal_uInt16 nPoly = aPolyPoly.Count();
    for (sal_uInt16 i = 0; i < nPoly; i++)
    {
        tools::Polygon& rPoly = aPolyPoly[i];
        rPoly = PixelToLogic(rPoly);
    }
    return aPolyPoly;
}

basegfx::B2DPolyPolygon
CoordinateMapper::PixelToLogic(const basegfx::B2DPolyPolygon& rPixelPolyPoly) const
{
    basegfx::B2DPolyPolygon aTransformedPoly = rPixelPolyPoly;
    const basegfx::B2DHomMatrix aTransformationMatrix = GetInverseViewTransformation();
    aTransformedPoly.transform(aTransformationMatrix);
    return aTransformedPoly;
}

basegfx::B2DRectangle CoordinateMapper::PixelToLogic(const basegfx::B2DRectangle& rDeviceRect) const
{
    basegfx::B2DRectangle aTransformedRect = rDeviceRect;
    const basegfx::B2DHomMatrix aTransformationMatrix = GetInverseViewTransformation();
    aTransformedRect.transform(aTransformationMatrix);
    return aTransformedRect;
}

vcl::Region CoordinateMapper::PixelToLogic(const vcl::Region& rDeviceRegion) const
{
    if (!IsMapModeEnabled() || rDeviceRegion.IsNull() || rDeviceRegion.IsEmpty())
        return rDeviceRegion;

    if (rDeviceRegion.getB2DPolyPolygon())
        return vcl::Region(PixelToLogic(*rDeviceRegion.getB2DPolyPolygon()));

    if (rDeviceRegion.getPolyPolygon())
        return vcl::Region(PixelToLogic(*rDeviceRegion.getPolyPolygon()));

    vcl::Region aRegion;

    if (rDeviceRegion.getRegionBand())
    {
        RectangleVector aRectangles;
        rDeviceRegion.GetRegionRectangles(aRectangles);
        const RectangleVector& rRectangles(aRectangles);

        for (RectangleVector::const_reverse_iterator aRectIter(rRectangles.rbegin());
             aRectIter != rRectangles.rend(); ++aRectIter)
        {
            aRegion.Union(PixelToLogic(*aRectIter));
        }
    }

    return aRegion;
}

Point CoordinateMapper::PixelToLogic(const Point& rDevicePt, const MapMode& rMapMode) const
{
    if (rMapMode.IsDefault())
        return rDevicePt;

    ImplMapRes aMapRes;
    aMapRes.CalcMapResolution(rMapMode, GetDPIX(), GetDPIY());

    return Point(
        lcl_pixelToLogic(rDevicePt.X(), GetDPIX(), aMapRes.mnMapScNumX, aMapRes.mnMapScDenomX)
            - aMapRes.mnMapOfsX - GetLogicalXOffset(),
        lcl_pixelToLogic(rDevicePt.Y(), GetDPIY(), aMapRes.mnMapScNumY, aMapRes.mnMapScDenomY)
            - aMapRes.mnMapOfsY - GetLogicalYOffset());
}

Size CoordinateMapper::PixelToLogic(const Size& rDeviceSize, const MapMode& rMapMode) const
{
    if (rMapMode.IsDefault())
        return rDeviceSize;

    ImplMapRes aMapRes;
    aMapRes.CalcMapResolution(rMapMode, GetDPIX(), GetDPIY());

    return Size(lcl_pixelToLogic(rDeviceSize.Width(), GetDPIX(), aMapRes.mnMapScNumX,
                                 aMapRes.mnMapScDenomX),
                lcl_pixelToLogic(rDeviceSize.Height(), GetDPIY(), aMapRes.mnMapScNumY,
                                 aMapRes.mnMapScDenomY));
}

tools::Rectangle CoordinateMapper::PixelToLogic(const tools::Rectangle& rDeviceRect,
                                                const MapMode& rMapMode) const
{
    if (rMapMode.IsDefault())
        return rDeviceRect;

    ImplMapRes aMapRes;
    aMapRes.CalcMapResolution(rMapMode, GetDPIX(), GetDPIY());

    tools::Rectangle aRetval(
        lcl_pixelToLogic(rDeviceRect.Left(), GetDPIX(), aMapRes.mnMapScNumX, aMapRes.mnMapScDenomX)
            - aMapRes.mnMapOfsX - GetLogicalXOffset(),
        lcl_pixelToLogic(rDeviceRect.Top(), GetDPIY(), aMapRes.mnMapScNumY, aMapRes.mnMapScDenomY)
            - aMapRes.mnMapOfsY - GetLogicalYOffset(),
        rDeviceRect.IsWidthEmpty() ? 0
                                   : lcl_pixelToLogic(rDeviceRect.Right(), GetDPIX(),
                                                      aMapRes.mnMapScNumX, aMapRes.mnMapScDenomX)
                                         - aMapRes.mnMapOfsX - GetLogicalXOffset(),
        rDeviceRect.IsHeightEmpty() ? 0
                                    : lcl_pixelToLogic(rDeviceRect.Bottom(), GetDPIY(),
                                                       aMapRes.mnMapScNumY, aMapRes.mnMapScDenomY)
                                          - aMapRes.mnMapOfsY - GetLogicalYOffset());

    if (rDeviceRect.IsWidthEmpty())
        aRetval.SetWidthEmpty();

    if (rDeviceRect.IsHeightEmpty())
        aRetval.SetHeightEmpty();

    return aRetval;
}

tools::Polygon CoordinateMapper::PixelToLogic(const tools::Polygon& rDevicePoly,
                                              const MapMode& rMapMode) const
{
    if (rMapMode.IsDefault())
        return rDevicePoly;

    ImplMapRes aMapRes;
    aMapRes.CalcMapResolution(rMapMode, GetDPIX(), GetDPIY());

    const sal_uInt16 nPoints = rDevicePoly.GetSize();
    tools::Polygon aPoly(rDevicePoly);

    const Point* pPointAry = aPoly.GetConstPointAry();

    for (sal_uInt16 i = 0; i < nPoints; i++)
    {
        const Point* pPt = &(pPointAry[i]);
        Point aPt;
        aPt.setX(lcl_pixelToLogic(pPt->X(), GetDPIX(), aMapRes.mnMapScNumX, aMapRes.mnMapScDenomX)
                 - aMapRes.mnMapOfsX - GetLogicalXOffset());
        aPt.setY(lcl_pixelToLogic(pPt->Y(), GetDPIY(), aMapRes.mnMapScNumY, aMapRes.mnMapScDenomY)
                 - aMapRes.mnMapOfsY - GetLogicalYOffset());
        aPoly[i] = aPt;
    }

    return aPoly;
}

basegfx::B2DPolygon CoordinateMapper::PixelToLogic(const basegfx::B2DPolygon& rPixelPoly,
                                                   const MapMode& rMapMode) const
{
    basegfx::B2DPolygon aTransformedPoly = rPixelPoly;
    const basegfx::B2DHomMatrix aTransformationMatrix = GetInverseViewTransformation(rMapMode);
    aTransformedPoly.transform(aTransformationMatrix);
    return aTransformedPoly;
}

basegfx::B2DPolyPolygon
CoordinateMapper::PixelToLogic(const basegfx::B2DPolyPolygon& rPixelPolyPoly,
                               const MapMode& rMapMode) const
{
    basegfx::B2DPolyPolygon aTransformedPoly = rPixelPolyPoly;
    const basegfx::B2DHomMatrix aTransformationMatrix = GetInverseViewTransformation(rMapMode);
    aTransformedPoly.transform(aTransformationMatrix);
    return aTransformedPoly;
}

// LogicToLogic - return (n1 * n2 * n3) / (n4 * n5)
static tools::Long lcl_scaleLogicValue(const tools::Long n1, const tools::Long n2,
                                       const tools::Long n3, const tools::Long n4,
                                       const tools::Long n5)
{
    if (n1 == 0 || n2 == 0 || n3 == 0 || n4 == 0 || n5 == 0)
        return 0;

    if (std::numeric_limits<tools::Long>::max() / std::abs(n2) < std::abs(n3))
    {
        BigInt a7 = n2;
        a7 *= n3;
        a7 *= n1;

        if (std::numeric_limits<tools::Long>::max() / std::abs(n4) < std::abs(n5))
        {
            BigInt a8 = n4;
            a8 *= n5;

            BigInt a9 = a8;
            a9 /= 2;
            if (a7.IsNeg())
                a7 -= a9;
            else
                a7 += a9;

            a7 /= a8;

            return static_cast<tools::Long>(a7);
        }
        tools::Long n8 = n4 * n5;

        if (a7.IsNeg())
            a7 -= n8 / 2;
        else
            a7 += n8 / 2;

        a7 /= n8;

        return static_cast<tools::Long>(a7);
    }

    tools::Long n6 = n2 * n3;

    if (std::numeric_limits<tools::Long>::max() / std::abs(n1) < std::abs(n6))
    {
        BigInt a7 = n1;
        a7 *= n6;

        if (std::numeric_limits<tools::Long>::max() / std::abs(n4) < std::abs(n5))
        {
            BigInt a8 = n4;
            a8 *= n5;

            BigInt a9 = a8;
            a9 /= 2;
            if (a7.IsNeg())
                a7 -= a9;
            else
                a7 += a9;

            a7 /= a8;

            return static_cast<tools::Long>(a7);
        }

        tools::Long n8 = n4 * n5;

        if (a7.IsNeg())
            a7 -= n8 / 2;
        else
            a7 += n8 / 2;

        a7 /= n8;

        return static_cast<tools::Long>(a7);
    }

    tools::Long n7 = n1 * n6;

    if (std::numeric_limits<tools::Long>::max() / std::abs(n4) < std::abs(n5))
    {
        BigInt a7 = n7;
        BigInt a8 = n4;
        a8 *= n5;

        BigInt a9 = a8;
        a9 /= 2;
        if (a7.IsNeg())
            a7 -= a9;
        else
            a7 += a9;

        a7 /= a8;

        return static_cast<tools::Long>(a7);
    }

    const tools::Long n8 = n4 * n5;
    const tools::Long n8_2 = n8 / 2;

    if (n7 < 0)
    {
        if ((n7 - std::numeric_limits<tools::Long>::min()) >= n8_2)
            n7 -= n8_2;
    }
    else if ((std::numeric_limits<tools::Long>::max() - n7) >= n8_2)
    {
        n7 += n8_2;
    }

    return n7 / n8;
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
        SAL_WARN("vcl.gdi", "Invalid source map unit");
    else if (eMapDst > MapUnit::MapPixel)
        SAL_WARN("vcl.gdi", "Invalid destination map unit");
    else if (eMapSrc != eMapDst)
    {
        // Here 72 PPI is assumed for MapPixel
        eSrc = MapToO3tlLength(eMapSrc, o3tl::Length::pt);
        eDst = MapToO3tlLength(eMapDst, o3tl::Length::pt);
    }
    return std::make_pair(eSrc, eDst);
}

static tools::Long lcl_convertLogicValue(const tools::Long n1, const o3tl::Length eFrom,
                                         const o3tl::Length eTo)
{
    if (n1 == 0 || eFrom == o3tl::Length::invalid || eTo == o3tl::Length::invalid)
        return 0;

    bool bOverflow;
    const auto nResult = o3tl::convert(n1, eFrom, eTo, bOverflow);

    if (!bOverflow)
        return nResult;

    const auto[n2, n3] = o3tl::getConversionMulDiv(eFrom, eTo);

    BigInt a4 = n1;
    a4 *= n2;

    if (a4.IsNeg())
        a4 -= n3 / 2;
    else
        a4 += n3 / 2;

    a4 /= n3;

    return static_cast<tools::Long>(a4);
}

static std::pair<ImplMapRes, ImplMapRes> lcl_calcConversionMapRes(const MapMode& rMMSource,
                                                                  const MapMode& rMMDest)
{
    std::pair<ImplMapRes, ImplMapRes> result;
    result.first.CalcMapResolution(rMMSource, 72, 72);
    result.second.CalcMapResolution(rMMDest, 72, 72);
    return result;
}

Point CoordinateMapper::LogicToLogic(const Point& rPtSource, const MapMode* pMapModeSource,
                                     const MapMode* pMapModeDest) const
{
    const MapMode* pSrc = pMapModeSource ? pMapModeSource : &GetMapMode();
    const MapMode* pDst = pMapModeDest ? pMapModeDest : &GetMapMode();

    if (*pSrc == *pDst)
        return rPtSource;

    MapUnit eUnitSource = pSrc->GetMapUnit();
    MapUnit eUnitDest = pDst->GetMapUnit();
    lcl_verifyUnitSourceDest(eUnitSource, eUnitDest);

    if (pSrc->IsSimple() && pDst->IsSimple())
    {
        const auto[eFrom, eTo] = lcl_getCorrectedUnit(eUnitSource, eUnitDest);

        return Point(lcl_convertLogicValue(rPtSource.X(), eFrom, eTo),
                     lcl_convertLogicValue(rPtSource.Y(), eFrom, eTo));
    }

    ImplMapRes aMapResSource
        = ResolveMapRes(pMapModeSource, GetMapMode(), IsMapModeEnabled(), GetDPIX(), GetDPIY());
    ImplMapRes aMapResDest
        = ResolveMapRes(pMapModeDest, GetMapMode(), IsMapModeEnabled(), GetDPIX(), GetDPIY());

    return Point(lcl_scaleLogicValue(rPtSource.X() + aMapResSource.mnMapOfsX,
                                     aMapResSource.mnMapScNumX, aMapResDest.mnMapScDenomX,
                                     aMapResSource.mnMapScDenomX, aMapResDest.mnMapScNumX)
                     - aMapResDest.mnMapOfsX,
                 lcl_scaleLogicValue(rPtSource.Y() + aMapResSource.mnMapOfsY,
                                     aMapResSource.mnMapScNumY, aMapResDest.mnMapScDenomY,
                                     aMapResSource.mnMapScDenomY, aMapResDest.mnMapScNumY)
                     - aMapResDest.mnMapOfsY);
}

Size CoordinateMapper::LogicToLogic(const Size& rSzSource, const MapMode* pMapModeSource,
                                    const MapMode* pMapModeDest) const
{
    const MapMode* pSrc = pMapModeSource ? pMapModeSource : &GetMapMode();
    const MapMode* pDst = pMapModeDest ? pMapModeDest : &GetMapMode();

    if (*pSrc == *pDst)
        return rSzSource;

    MapUnit eUnitSource = pSrc->GetMapUnit();
    MapUnit eUnitDest = pDst->GetMapUnit();
    lcl_verifyUnitSourceDest(eUnitSource, eUnitDest);

    if (pSrc->IsSimple() && pDst->IsSimple())
    {
        const auto[eFrom, eTo] = lcl_getCorrectedUnit(eUnitSource, eUnitDest);
        return Size(lcl_convertLogicValue(rSzSource.Width(), eFrom, eTo),
                    lcl_convertLogicValue(rSzSource.Height(), eFrom, eTo));
    }

    ImplMapRes aMapResSource
        = ResolveMapRes(pMapModeSource, GetMapMode(), IsMapModeEnabled(), GetDPIX(), GetDPIY());
    ImplMapRes aMapResDest
        = ResolveMapRes(pMapModeDest, GetMapMode(), IsMapModeEnabled(), GetDPIX(), GetDPIY());

    return Size(lcl_scaleLogicValue(rSzSource.Width(), aMapResSource.mnMapScNumX,
                                    aMapResDest.mnMapScDenomX, aMapResSource.mnMapScDenomX,
                                    aMapResDest.mnMapScNumX),
                lcl_scaleLogicValue(rSzSource.Height(), aMapResSource.mnMapScNumY,
                                    aMapResDest.mnMapScDenomY, aMapResSource.mnMapScDenomY,
                                    aMapResDest.mnMapScNumY));
}

tools::Rectangle CoordinateMapper::LogicToLogic(const tools::Rectangle& rRectSource,
                                                const MapMode* pMapModeSource,
                                                const MapMode* pMapModeDest) const
{
    const MapMode* pSrc = pMapModeSource ? pMapModeSource : &GetMapMode();
    const MapMode* pDst = pMapModeDest ? pMapModeDest : &GetMapMode();

    if (*pSrc == *pDst)
        return rRectSource;

    MapUnit eUnitSource = pSrc->GetMapUnit();
    MapUnit eUnitDest = pDst->GetMapUnit();
    lcl_verifyUnitSourceDest(eUnitSource, eUnitDest);

    if (pSrc->IsSimple() && pDst->IsSimple())
    {
        const auto[eFrom, eTo] = lcl_getCorrectedUnit(eUnitSource, eUnitDest);

        auto left = lcl_convertLogicValue(rRectSource.Left(), eFrom, eTo);
        auto top = lcl_convertLogicValue(rRectSource.Top(), eFrom, eTo);
        auto right = rRectSource.IsWidthEmpty()
                         ? 0
                         : lcl_convertLogicValue(rRectSource.Right(), eFrom, eTo);
        auto bottom = rRectSource.IsHeightEmpty()
                          ? 0
                          : lcl_convertLogicValue(rRectSource.Bottom(), eFrom, eTo);

        tools::Rectangle aRetval(left, top, right, bottom);
        if (rRectSource.IsWidthEmpty())
            aRetval.SetWidthEmpty();
        if (rRectSource.IsHeightEmpty())
            aRetval.SetHeightEmpty();
        return aRetval;
    }

    ImplMapRes aMapResSource
        = ResolveMapRes(pMapModeSource, GetMapMode(), IsMapModeEnabled(), GetDPIX(), GetDPIY());
    ImplMapRes aMapResDest
        = ResolveMapRes(pMapModeDest, GetMapMode(), IsMapModeEnabled(), GetDPIX(), GetDPIY());

    tools::Rectangle aRetval(
        lcl_scaleLogicValue(rRectSource.Left() + aMapResSource.mnMapOfsX, aMapResSource.mnMapScNumX,
                            aMapResDest.mnMapScDenomX, aMapResSource.mnMapScDenomX,
                            aMapResDest.mnMapScNumX)
            - aMapResDest.mnMapOfsX,
        lcl_scaleLogicValue(rRectSource.Top() + aMapResSource.mnMapOfsY, aMapResSource.mnMapScNumY,
                            aMapResDest.mnMapScDenomY, aMapResSource.mnMapScDenomY,
                            aMapResDest.mnMapScNumY)
            - aMapResDest.mnMapOfsY,
        rRectSource.IsWidthEmpty()
            ? 0
            : lcl_scaleLogicValue(rRectSource.Right() + aMapResSource.mnMapOfsX,
                                  aMapResSource.mnMapScNumX, aMapResDest.mnMapScDenomX,
                                  aMapResSource.mnMapScDenomX, aMapResDest.mnMapScNumX)
                  - aMapResDest.mnMapOfsX,
        rRectSource.IsHeightEmpty()
            ? 0
            : lcl_scaleLogicValue(rRectSource.Bottom() + aMapResSource.mnMapOfsY,
                                  aMapResSource.mnMapScNumY, aMapResDest.mnMapScDenomY,
                                  aMapResSource.mnMapScDenomY, aMapResDest.mnMapScNumY)
                  - aMapResDest.mnMapOfsY);

    if (rRectSource.IsWidthEmpty())
        aRetval.SetWidthEmpty();

    if (rRectSource.IsHeightEmpty())
        aRetval.SetHeightEmpty();

    return aRetval;
}

// Keep the global/static overloads for external users that don't go through an instance,
// or if they are exported symbols from the library.
tools::Long LogicToLogic(tools::Long nLongSource, MapUnit eUnitSource, MapUnit eUnitDest)
{
    if (eUnitSource == eUnitDest)
        return nLongSource;

    lcl_verifyUnitSourceDest(eUnitSource, eUnitDest);
    const auto[eFrom, eTo] = lcl_getCorrectedUnit(eUnitSource, eUnitDest);
    return lcl_convertLogicValue(nLongSource, eFrom, eTo);
}

tools::Rectangle LogicToLogic(const tools::Rectangle& rRectSource, const MapMode& rMapModeSource,
                              const MapMode& rMapModeDest)
{
    if (rMapModeSource == rMapModeDest)
        return rRectSource;

    MapUnit eUnitSource = rMapModeSource.GetMapUnit();
    MapUnit eUnitDest = rMapModeDest.GetMapUnit();
    lcl_verifyUnitSourceDest(eUnitSource, eUnitDest);

    if (rMapModeSource.IsSimple() && rMapModeDest.IsSimple())
    {
        const auto[eFrom, eTo] = lcl_getCorrectedUnit(eUnitSource, eUnitDest);
        auto left = lcl_convertLogicValue(rRectSource.Left(), eFrom, eTo);
        auto top = lcl_convertLogicValue(rRectSource.Top(), eFrom, eTo);
        auto right = rRectSource.IsWidthEmpty()
                         ? 0
                         : lcl_convertLogicValue(rRectSource.Right(), eFrom, eTo);
        auto bottom = rRectSource.IsHeightEmpty()
                          ? 0
                          : lcl_convertLogicValue(rRectSource.Bottom(), eFrom, eTo);

        tools::Rectangle aRetval(left, top, right, bottom);
        if (rRectSource.IsWidthEmpty())
            aRetval.SetWidthEmpty();
        if (rRectSource.IsHeightEmpty())
            aRetval.SetHeightEmpty();
        return aRetval;
    }

    const auto[aMapResSource, aMapResDest] = lcl_calcConversionMapRes(rMapModeSource, rMapModeDest);

    tools::Rectangle aRetval(
        lcl_scaleLogicValue(rRectSource.Left() + aMapResSource.mnMapOfsX, aMapResSource.mnMapScNumX,
                            aMapResDest.mnMapScDenomX, aMapResSource.mnMapScDenomX,
                            aMapResDest.mnMapScNumX)
            - aMapResDest.mnMapOfsX,
        lcl_scaleLogicValue(rRectSource.Top() + aMapResSource.mnMapOfsY, aMapResSource.mnMapScNumY,
                            aMapResDest.mnMapScDenomY, aMapResSource.mnMapScDenomY,
                            aMapResDest.mnMapScNumY)
            - aMapResDest.mnMapOfsY,
        rRectSource.IsWidthEmpty()
            ? 0
            : lcl_scaleLogicValue(rRectSource.Right() + aMapResSource.mnMapOfsX,
                                  aMapResSource.mnMapScNumX, aMapResDest.mnMapScDenomX,
                                  aMapResSource.mnMapScDenomX, aMapResDest.mnMapScNumX)
                  - aMapResDest.mnMapOfsX,
        rRectSource.IsHeightEmpty()
            ? 0
            : lcl_scaleLogicValue(rRectSource.Bottom() + aMapResSource.mnMapOfsY,
                                  aMapResSource.mnMapScNumY, aMapResDest.mnMapScDenomY,
                                  aMapResSource.mnMapScDenomY, aMapResDest.mnMapScNumY)
                  - aMapResDest.mnMapOfsY);

    if (rRectSource.IsWidthEmpty())
        aRetval.SetWidthEmpty();
    if (rRectSource.IsHeightEmpty())
        aRetval.SetHeightEmpty();
    return aRetval;
}

basegfx::B2DHomMatrix LogicToLogic(const MapMode& rMapModeSource, const MapMode& rMapModeDest)
{
    basegfx::B2DHomMatrix aTransform;

    if (rMapModeSource == rMapModeDest)
        return aTransform;

    MapUnit eUnitSource = rMapModeSource.GetMapUnit();
    MapUnit eUnitDest = rMapModeDest.GetMapUnit();
    lcl_verifyUnitSourceDest(eUnitSource, eUnitDest);

    if (rMapModeSource.IsSimple() && rMapModeDest.IsSimple())
    {
        const auto[eFrom, eTo] = lcl_getCorrectedUnit(eUnitSource, eUnitDest);
        const double fScaleFactor(eFrom == o3tl::Length::invalid || eTo == o3tl::Length::invalid
                                      ? std::numeric_limits<double>::quiet_NaN()
                                      : o3tl::convert(1.0, eFrom, eTo));
        aTransform.set(0, 0, fScaleFactor);
        aTransform.set(1, 1, fScaleFactor);
        return aTransform;
    }

    const auto[aMapResSource, aMapResDest] = lcl_calcConversionMapRes(rMapModeSource, rMapModeDest);

    const double fScaleFactorX(
        (double(aMapResSource.mnMapScNumX) * double(aMapResDest.mnMapScDenomX))
        / (double(aMapResSource.mnMapScDenomX) * double(aMapResDest.mnMapScNumX)));
    const double fScaleFactorY(
        (double(aMapResSource.mnMapScNumY) * double(aMapResDest.mnMapScDenomY))
        / (double(aMapResSource.mnMapScDenomY) * double(aMapResDest.mnMapScNumY)));
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

basegfx::B2DPolygon LogicToLogic(const basegfx::B2DPolygon& rPolySource,
                                 const MapMode& rMapModeSource, const MapMode& rMapModeDest)
{
    if (rMapModeSource == rMapModeDest)
        return rPolySource;

    const basegfx::B2DHomMatrix aTransform(LogicToLogic(rMapModeSource, rMapModeDest));
    basegfx::B2DPolygon aPoly(rPolySource);
    aPoly.transform(aTransform);
    return aPoly;
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

    return Size(lcl_scaleLogicValue(rSzSource.Width(), aMapResSource.mnMapScNumX,
                                    aMapResDest.mnMapScDenomX, aMapResSource.mnMapScDenomX,
                                    aMapResDest.mnMapScNumX),
                lcl_scaleLogicValue(rSzSource.Height(), aMapResSource.mnMapScNumY,
                                    aMapResDest.mnMapScDenomY, aMapResSource.mnMapScDenomY,
                                    aMapResDest.mnMapScNumY));
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

    return Point(lcl_scaleLogicValue(rPtSource.X() + aMapResSource.mnMapOfsX,
                                     aMapResSource.mnMapScNumX, aMapResDest.mnMapScDenomX,
                                     aMapResSource.mnMapScDenomX, aMapResDest.mnMapScNumX)
                     - aMapResDest.mnMapOfsX,
                 lcl_scaleLogicValue(rPtSource.Y() + aMapResSource.mnMapOfsY,
                                     aMapResSource.mnMapScNumY, aMapResDest.mnMapScDenomY,
                                     aMapResSource.mnMapScDenomY, aMapResDest.mnMapScNumY)
                     - aMapResDest.mnMapOfsY);
}

basegfx::B2DPoint CoordinateMapper::LogicToDeviceSubPixel(const Point& rPoint) const
{
    if (!IsMapModeEnabled())
        return basegfx::B2DPoint(rPoint.X() + GetOutOffXPixel(), rPoint.Y() + GetOutOffYPixel());

    return basegfx::B2DPoint(lcl_logicToSubPixel(rPoint.X() + GetMappingXOffset(), GetDPIX(),
                                                 GetMappingXNumerator(), GetMappingXDenominator())
                                 + GetOutOffXPixel() + GetPixelXOffset(),
                             lcl_logicToSubPixel(rPoint.Y() + GetMappingYOffset(), GetDPIY(),
                                                 GetMappingYNumerator(), GetMappingYDenominator())
                                 + GetOutOffYPixel() + GetPixelYOffset());
}

static tools::Long lcl_subPixelToLogic(double n, tools::Long nDPI, tools::Long nMapNum,
                                       tools::Long nMapDenom)
{
    assert(nDPI > 0);
    assert(nMapNum != 0);

    return std::round(n * nMapDenom / nMapNum / nDPI);
}

Point CoordinateMapper::SubPixelToLogic(const basegfx::B2DPoint& rDevicePt) const
{
    if (!IsMapModeEnabled())
    {
        assert(floor(rDevicePt.getX() == rDevicePt.getX())
               && floor(rDevicePt.getY() == rDevicePt.getY()));
        return Point(rDevicePt.getX(), rDevicePt.getY());
    }

    return Point(lcl_subPixelToLogic(rDevicePt.getX(), GetDPIX(), GetMappingXNumerator(),
                                     GetMappingXDenominator())
                     - GetMappingXOffset() - GetLogicalXOffset(),
                 lcl_subPixelToLogic(rDevicePt.getY(), GetDPIY(), GetMappingYNumerator(),
                                     GetMappingYDenominator())
                     - GetMappingYOffset() - GetLogicalYOffset());
}

double CoordinateMapper::CalculateLayoutWidth(const vcl::text::LayoutResources& rRes,
                                              tools::Long nLogicWidth)
{
    if (nLogicWidth && rRes.rMapper.IsMapModeEnabled())
        return rRes.rMapper.LogicWidthToDeviceSubPixel(nLogicWidth);

    return static_cast<double>(nLogicWidth);
}

tools::Long CoordinateMapper::GetSubPixelFactor(const CoordinateMapper& rMapper)
{
    // Use 64 as a factor when MapMode is disabled to maintain subpixel granularity
    return rMapper.IsMapModeEnabled() ? 1 : 64;
}

double CoordinateMapper::GetLayoutPixelWidth(const CoordinateMapper& rMapper,
                                             tools::Long nLogicWidth, tools::Long nSubPixelFactor)
{
    // High-precision conversion from logical units to device subpixels
    return rMapper.LogicWidthToDeviceSubPixel(nLogicWidth * nSubPixelFactor);
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
