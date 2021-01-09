/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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

#include <tools/long.hxx>

#include <vcl/Geometry.hxx>

#include <cassert>
#include <cmath>

static tools::Long LogicToPixel(tools::Long n, tools::Long nDPI, tools::Long nMapNum,
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
    sal_Int64 n64 = n;
    n64 *= nMapNum;
    n64 *= nDPI;
    if (nMapDenom == 1)
        n = static_cast<tools::Long>(n64);
    else
    {
        n64 = 2 * n64 / nMapDenom;
        if (n64 < 0)
            --n64;
        else
            ++n64;
        n = static_cast<tools::Long>(n64 / 2);
    }
    return n;
}

static tools::Long PixelToLogic(tools::Long n, tools::Long nDPI, tools::Long nMapNum,
                                tools::Long nMapDenom)
{
    assert(nDPI > 0);
    if (nMapNum == 0)
        return 0;
    sal_Int64 nDenom = nDPI;
    nDenom *= nMapNum;

    sal_Int64 n64 = n;
    n64 *= nMapDenom;
    if (nDenom == 1)
        n = static_cast<tools::Long>(n64);
    else
    {
        n64 = 2 * n64 / nDenom;
        if (n64 < 0)
            --n64;
        else
            ++n64;
        n = static_cast<tools::Long>(n64 / 2);
    }
    return n;
}

tools::Long Geometry::LogicXToDevicePixel(tools::Long nX, MappingMetrics aMappingMetrics) const
{
    if (!mbMap)
        return nX + mnOffsetXpx;

    return LogicToPixel(nX + aMappingMetrics.mnMapOfsX, mnDPIX, aMappingMetrics.mnMapScNumX,
                        aMappingMetrics.mnMapScDenomX)
           + mnOffsetXpx + mnOffsetFromOriginXpx;
}

tools::Long Geometry::LogicYToDevicePixel(tools::Long nY, MappingMetrics aMappingMetrics) const
{
    if (!mbMap)
        return nY + mnOffsetYpx;

    return LogicToPixel(nY + aMappingMetrics.mnMapOfsY, mnDPIY, aMappingMetrics.mnMapScNumY,
                        aMappingMetrics.mnMapScDenomY)
           + mnOffsetYpx + mnOffsetFromOriginYpx;
}

tools::Long Geometry::LogicWidthToDevicePixel(tools::Long nWidth,
                                              MappingMetrics aMappingMetrics) const
{
    if (!mbMap)
        return nWidth;

    return LogicToPixel(nWidth, mnDPIX, aMappingMetrics.mnMapScNumX, aMappingMetrics.mnMapScDenomX);
}

tools::Long Geometry::LogicHeightToDevicePixel(tools::Long nHeight,
                                               MappingMetrics aMappingMetrics) const
{
    if (!mbMap)
        return nHeight;

    return LogicToPixel(nHeight, mnDPIY, aMappingMetrics.mnMapScNumY,
                        aMappingMetrics.mnMapScDenomY);
}

float Geometry::FloatLogicHeightToDevicePixel(float fLogicHeight,
                                              MappingMetrics aMappingMetrics) const
{
    if (!mbMap)
        return fLogicHeight;
    float fPixelHeight
        = (fLogicHeight * mnDPIY * aMappingMetrics.mnMapScNumY) / aMappingMetrics.mnMapScDenomY;
    return fPixelHeight;
}

tools::Long Geometry::DevicePixelToLogicWidth(tools::Long nWidth,
                                              MappingMetrics aMappingMetrics) const
{
    if (!mbMap)
        return nWidth;

    return PixelToLogic(nWidth, mnDPIX, aMappingMetrics.mnMapScNumX, aMappingMetrics.mnMapScDenomX);
}

tools::Long Geometry::DevicePixelToLogicHeight(tools::Long nHeight,
                                               MappingMetrics aMappingMetrics) const
{
    if (!mbMap)
        return nHeight;

    return PixelToLogic(nHeight, mnDPIY, aMappingMetrics.mnMapScNumY,
                        aMappingMetrics.mnMapScDenomY);
}

Point Geometry::LogicToDevicePixel(const Point& rLogicPt, MappingMetrics aMappingMetrics) const
{
    if (!mbMap)
        return Point(rLogicPt.X() + mnOffsetXpx, rLogicPt.Y() + mnOffsetYpx);

    return Point(LogicToPixel(rLogicPt.X() + aMappingMetrics.mnMapOfsX, mnDPIX,
                              aMappingMetrics.mnMapScNumX, aMappingMetrics.mnMapScDenomX)
                     + mnOffsetXpx + mnOffsetFromOriginXpx,
                 LogicToPixel(rLogicPt.Y() + aMappingMetrics.mnMapOfsY, mnDPIY,
                              aMappingMetrics.mnMapScNumY, aMappingMetrics.mnMapScDenomY)
                     + mnOffsetYpx + mnOffsetFromOriginYpx);
}

Size Geometry::LogicToDevicePixel(const Size& rLogicSize, MappingMetrics aMappingMetrics) const
{
    if (!mbMap)
        return rLogicSize;

    return Size(LogicToPixel(rLogicSize.Width(), mnDPIX, aMappingMetrics.mnMapScNumX,
                             aMappingMetrics.mnMapScDenomX),
                LogicToPixel(rLogicSize.Height(), mnDPIY, aMappingMetrics.mnMapScNumY,
                             aMappingMetrics.mnMapScDenomY));
}

tools::Rectangle Geometry::LogicToDevicePixel(const tools::Rectangle& rLogicRect,
                                              MappingMetrics aMappingMetrics) const
{
    if (rLogicRect.IsEmpty())
        return rLogicRect;

    if (!mbMap)
    {
        return tools::Rectangle(rLogicRect.Left() + mnOffsetXpx, rLogicRect.Top() + mnOffsetYpx,
                                rLogicRect.Right() + mnOffsetXpx,
                                rLogicRect.Bottom() + mnOffsetYpx);
    }

    return tools::Rectangle(LogicToPixel(rLogicRect.Left() + aMappingMetrics.mnMapOfsX, mnDPIX,
                                         aMappingMetrics.mnMapScNumX, aMappingMetrics.mnMapScDenomX)
                                + mnOffsetXpx + mnOffsetFromOriginXpx,
                            LogicToPixel(rLogicRect.Top() + aMappingMetrics.mnMapOfsY, mnDPIY,
                                         aMappingMetrics.mnMapScNumY, aMappingMetrics.mnMapScDenomY)
                                + mnOffsetYpx + mnOffsetFromOriginYpx,
                            LogicToPixel(rLogicRect.Right() + aMappingMetrics.mnMapOfsX, mnDPIX,
                                         aMappingMetrics.mnMapScNumX, aMappingMetrics.mnMapScDenomX)
                                + mnOffsetXpx + mnOffsetFromOriginXpx,
                            LogicToPixel(rLogicRect.Bottom() + aMappingMetrics.mnMapOfsY, mnDPIY,
                                         aMappingMetrics.mnMapScNumY, aMappingMetrics.mnMapScDenomY)
                                + mnOffsetYpx + mnOffsetFromOriginYpx);
}

tools::Polygon Geometry::LogicToDevicePixel(const tools::Polygon& rLogicPoly,
                                            MappingMetrics aMappingMetrics) const
{
    if (!mbMap && !mnOffsetXpx && !mnOffsetYpx)
        return rLogicPoly;

    sal_uInt16 i;
    sal_uInt16 nPoints = rLogicPoly.GetSize();
    tools::Polygon aPoly(rLogicPoly);

    // get pointer to Point-array (copy data)
    const Point* pPointAry = aPoly.GetConstPointAry();

    if (mbMap)
    {
        for (i = 0; i < nPoints; i++)
        {
            const Point& rPt = pPointAry[i];
            Point aPt(LogicToPixel(rPt.X() + aMappingMetrics.mnMapOfsX, mnDPIX,
                                   aMappingMetrics.mnMapScNumX, aMappingMetrics.mnMapScDenomX)
                          + mnOffsetXpx + mnOffsetFromOriginXpx,
                      LogicToPixel(rPt.Y() + aMappingMetrics.mnMapOfsY, mnDPIY,
                                   aMappingMetrics.mnMapScNumY, aMappingMetrics.mnMapScDenomY)
                          + mnOffsetYpx + mnOffsetFromOriginYpx);
            aPoly[i] = aPt;
        }
    }
    else
    {
        for (i = 0; i < nPoints; i++)
        {
            Point aPt = pPointAry[i];
            aPt.AdjustX(mnOffsetXpx);
            aPt.AdjustY(mnOffsetYpx);
            aPoly[i] = aPt;
        }
    }

    return aPoly;
}

tools::PolyPolygon Geometry::LogicToDevicePixel(const tools::PolyPolygon& rLogicPolyPoly,
                                                MappingMetrics aMappingMetrics) const
{
    if (!mbMap && !mnOffsetXpx && !mnOffsetYpx)
        return rLogicPolyPoly;

    tools::PolyPolygon aPolyPoly(rLogicPolyPoly);
    sal_uInt16 nPoly = aPolyPoly.Count();
    for (sal_uInt16 i = 0; i < nPoly; i++)
    {
        tools::Polygon& rPoly = aPolyPoly[i];
        rPoly = LogicToDevicePixel(rPoly, aMappingMetrics);
    }
    return aPolyPoly;
}

LineInfo Geometry::LogicToDevicePixel(const LineInfo& rLineInfo,
                                      MappingMetrics aMappingMetrics) const
{
    LineInfo aInfo(rLineInfo);

    if (aInfo.GetStyle() == LineStyle::Dash)
    {
        if (aInfo.GetDotCount() && aInfo.GetDotLen())
            aInfo.SetDotLen(std::max(LogicWidthToDevicePixel(aInfo.GetDotLen(), aMappingMetrics),
                                     tools::Long(1)));
        else
            aInfo.SetDotCount(0);

        if (aInfo.GetDashCount() && aInfo.GetDashLen())
            aInfo.SetDashLen(std::max(LogicWidthToDevicePixel(aInfo.GetDashLen(), aMappingMetrics),
                                      tools::Long(1)));
        else
            aInfo.SetDashCount(0);

        aInfo.SetDistance(LogicWidthToDevicePixel(aInfo.GetDistance(), aMappingMetrics));

        if ((!aInfo.GetDashCount() && !aInfo.GetDotCount()) || !aInfo.GetDistance())
            aInfo.SetStyle(LineStyle::Solid);
    }

    aInfo.SetWidth(LogicWidthToDevicePixel(aInfo.GetWidth(), aMappingMetrics));

    return aInfo;
}

tools::Rectangle Geometry::DevicePixelToLogic(const tools::Rectangle& rPixelRect,
                                              MappingMetrics aMappingMetrics) const
{
    if (rPixelRect.IsEmpty())
        return rPixelRect;

    if (!mbMap)
    {
        return tools::Rectangle(rPixelRect.Left() - mnOffsetXpx, rPixelRect.Top() - mnOffsetYpx,
                                rPixelRect.Right() - mnOffsetXpx,
                                rPixelRect.Bottom() - mnOffsetYpx);
    }

    return tools::Rectangle(
        PixelToLogic(rPixelRect.Left() - mnOffsetXpx - mnOffsetFromOriginXpx, mnDPIX,
                     aMappingMetrics.mnMapScNumX, aMappingMetrics.mnMapScDenomX)
            - aMappingMetrics.mnMapOfsX,
        PixelToLogic(rPixelRect.Top() - mnOffsetYpx - mnOffsetFromOriginYpx, mnDPIY,
                     aMappingMetrics.mnMapScNumY, aMappingMetrics.mnMapScDenomY)
            - aMappingMetrics.mnMapOfsY,
        PixelToLogic(rPixelRect.Right() - mnOffsetXpx - mnOffsetFromOriginXpx, mnDPIX,
                     aMappingMetrics.mnMapScNumX, aMappingMetrics.mnMapScDenomX)
            - aMappingMetrics.mnMapOfsX,
        PixelToLogic(rPixelRect.Bottom() - mnOffsetYpx - mnOffsetFromOriginYpx, mnDPIY,
                     aMappingMetrics.mnMapScNumY, aMappingMetrics.mnMapScDenomY)
            - aMappingMetrics.mnMapOfsY);
}

vcl::Region Geometry::PixelToDevicePixel(const vcl::Region& rRegion) const
{
    if (!mnOffsetXpx && !mnOffsetYpx)
        return rRegion;

    vcl::Region aRegion(rRegion);
    aRegion.Move(mnOffsetXpx + mnOffsetFromOriginXpx, mnOffsetYpx + mnOffsetFromOriginYpx);
    return aRegion;
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
