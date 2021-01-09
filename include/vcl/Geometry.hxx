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

#include <tools/gen.hxx>
#include <tools/long.hxx>
#include <tools/poly.hxx>

#include <vcl/lineinfo.hxx>
#include <vcl/region.hxx>
#include <vcl/MappingMetrics.hxx>

struct Geometry
{
    Geometry()
        : mbMap(false)
        , mnWidthPx(0)
        , mnHeightPx(0)
        , mnOffsetXpx(0)
        , mnOffsetYpx(0)
        , mnOffsetFromOriginXpx(0)
        , mnOffsetFromOriginYpx(0)
        , mnOffsetFromOriginXInLogicalUnits(0)
        , mnOffsetFromOriginYInLogicalUnits(0)
        , mnDPIX(0)
        , mnDPIY(0)
        , mnDPIScalePercentage(100)
    {
    }

    tools::Long ImplLogicXToDevicePixel(tools::Long nX, MappingMetrics aMappingMetric) const;
    tools::Long ImplLogicYToDevicePixel(tools::Long nY, MappingMetrics aMappingMetric) const;
    tools::Long ImplLogicWidthToDevicePixel(tools::Long nWidth,
                                            MappingMetrics aMappingMetric) const;
    tools::Long ImplLogicHeightToDevicePixel(tools::Long nHeight,
                                             MappingMetrics aMappingMetric) const;
    float ImplFloatLogicHeightToDevicePixel(float fLogicHeight,
                                            MappingMetrics aMappingMetric) const;
    Point ImplLogicToDevicePixel(const Point& rLogicPt, MappingMetrics aMappingMetrics) const;
    Size ImplLogicToDevicePixel(const Size& rLogicSize, MappingMetrics aMappingMetrics) const;
    tools::Rectangle ImplLogicToDevicePixel(const tools::Rectangle& rLogicRect,
                                            MappingMetrics aMappingMetrics) const;
    tools::Polygon ImplLogicToDevicePixel(const tools::Polygon& rLogicPoly,
                                          MappingMetrics aMappingMetrics) const;
    tools::PolyPolygon ImplLogicToDevicePixel(const tools::PolyPolygon& rLogicPolyPoly,
                                              MappingMetrics aMappingMetrics) const;
    LineInfo ImplLogicToDevicePixel(LineInfo const& rLineInfo,
                                    MappingMetrics aMappingMetrics) const;
    tools::Long ImplDevicePixelToLogicWidth(tools::Long nWidth,
                                            MappingMetrics aMappingMetric) const;
    tools::Long ImplDevicePixelToLogicHeight(tools::Long nHeight,
                                             MappingMetrics aMappingMetrics) const;
    tools::Rectangle ImplDevicePixelToLogic(const tools::Rectangle& rPixelRect,
                                            MappingMetrics aMappingMetrics) const;
    vcl::Region ImplPixelToDevicePixel(const vcl::Region& rRegion) const;

    bool mbMap;

    tools::Long mnWidthPx;
    tools::Long mnHeightPx;

    tools::Long
        mnOffsetXpx; ///< Output X offset for device output in pixel (pseudo window offset within window system's frames)
    tools::Long
        mnOffsetYpx; ///< Output Y offset for device output in pixel (pseudo window offset within window system's frames)
    tools::Long
        mnOffsetFromOriginXpx; ///< Additional output pixel offset, applied in LogicToPixel (used by SetPixelOffset/GetPixelOffset)
    tools::Long
        mnOffsetFromOriginYpx; ///< Additional output pixel offset, applied in LogicToPixel (used by SetPixelOffset/GetPixelOffset)
    tools::Long
        mnOffsetFromOriginXInLogicalUnits; ///< Additional output offset in _logical_ coordinates, applied in PixelToLogic (used by SetPixelOffset/GetPixelOffset)
    tools::Long
        mnOffsetFromOriginYInLogicalUnits; ///< Additional output offset in _logical_ coordinates, applied in PixelToLogic (used by SetPixelOffset/GetPixelOffset)

    sal_Int32 mnDPIX;
    sal_Int32 mnDPIY;
    sal_Int32
        mnDPIScalePercentage; ///< For HiDPI displays, we want to draw elements for a percentage larger
};

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
