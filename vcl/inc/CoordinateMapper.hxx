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

#pragma once

#include <sal/types.h>
#include <tools/gen.hxx>
#include <tools/fract.hxx>
#include <tools/long.hxx>
#include <tools/poly.hxx>
#include <basegfx/matrix/b2dhommatrix.hxx>
#include <basegfx/range/b2drectangle.hxx>
#include <basegfx/polygon/b2dpolygon.hxx>

#include <vcl/dllapi.h>
#include <vcl/lineinfo.hxx>
#include <vcl/mapmod.hxx>
#include <vcl/region.hxx>
#include <vcl/rendercontext/ImplMapRes.hxx>

class CoordinateMapper
{
private:
    bool mbMap;
    MapMode maMapMode;
    ImplMapRes maMapRes;

    // #i75163#
    mutable basegfx::B2DHomMatrix* mpViewTransform = nullptr;
    mutable basegfx::B2DHomMatrix* mpInverseViewTransform = nullptr;

    sal_Int32 mnDPIX;
    sal_Int32 mnDPIY;
    sal_Int32 mnDPIScalePercentage = 100;

    /// Additional output pixel offset, applied in LogicToPixel (used by SetPixelOffset/GetPixelOffset)
    tools::Long mnOutOffOrigX;
    /// Additional output pixel offset, applied in LogicToPixel (used by SetPixelOffset/GetPixelOffset)
    tools::Long mnOutOffOrigY;

    tools::Long mnOutWidth;
    tools::Long mnOutHeight;

    /// Output offset for device output in pixel (pseudo window offset within window system's frames)
    tools::Long mnOutOffX;
    /// Output offset for device output in pixel (pseudo window offset within window system's frames)
    tools::Long mnOutOffY;

    /// Additional output offset in _logical_ coordinates, applied in PixelToLogic (used by SetPixelOffset/GetPixelOffset)
    tools::Long mnOutOffLogicX;
    /// Additional output offset in _logical_ coordinates, applied in PixelToLogic (used by SetPixelOffset/GetPixelOffset)
    tools::Long mnOutOffLogicY;

public:
    SAL_DLLPRIVATE bool IsMapModeEnabled() const { return mbMap; }
    SAL_DLLPRIVATE void EnableMapMode(bool bEnable = true) { mbMap = bEnable; }
    SAL_DLLPRIVATE void SetOffset(const Size& rOffset);

    SAL_DLLPRIVATE const MapMode& GetMapMode() const { return maMapMode; }
    SAL_DLLPRIVATE bool IsDefaultMapMode() const { return maMapMode.IsDefault(); }
    SAL_DLLPRIVATE void ResetMapMode() { maMapMode = MapMode(); }
    SAL_DLLPRIVATE void ResetMapMode(const MapMode& rMapMode) { maMapMode = rMapMode; }
    SAL_DLLPRIVATE MapUnit GetMapUnit() const { return maMapMode.GetMapUnit(); }

    SAL_DLLPRIVATE const Fraction& GetScaleX() const { return maMapMode.GetScaleX(); }
    SAL_DLLPRIVATE const Fraction& GetScaleY() const { return maMapMode.GetScaleY(); }
    SAL_DLLPRIVATE void SetScaleX(const Fraction& rScale) { maMapMode.SetScaleX(rScale); }
    SAL_DLLPRIVATE void SetScaleY(const Fraction& rScale) { maMapMode.SetScaleY(rScale); }

    SAL_DLLPRIVATE tools::Long GetMappingXOffset() const { return maMapRes.mnMapOfsX; }
    SAL_DLLPRIVATE tools::Long GetMappingYOffset() const { return maMapRes.mnMapOfsY; }
    SAL_DLLPRIVATE tools::Long GetMappingXNumerator() const { return maMapRes.mnMapScNumX; }
    SAL_DLLPRIVATE tools::Long GetMappingYNumerator() const { return maMapRes.mnMapScNumY; }
    SAL_DLLPRIVATE tools::Long GetMappingXDenominator() const { return maMapRes.mnMapScDenomX; }
    SAL_DLLPRIVATE tools::Long GetMappingYDenominator() const { return maMapRes.mnMapScDenomY; }

    SAL_DLLPRIVATE void SetMappingXOffset(tools::Long nOffset) { maMapRes.mnMapOfsX = nOffset; }
    SAL_DLLPRIVATE void SetMappingYOffset(tools::Long nOffset) { maMapRes.mnMapOfsY = nOffset; }
    SAL_DLLPRIVATE void SetMappingXNumerator(tools::Long nNum) { maMapRes.mnMapScNumX = nNum; }
    SAL_DLLPRIVATE void SetMappingYNumerator(tools::Long nNum) { maMapRes.mnMapScNumY = nNum; }
    SAL_DLLPRIVATE void SetMappingXDenominator(tools::Long nDenom)
    {
        maMapRes.mnMapScDenomX = nDenom;
    }
    SAL_DLLPRIVATE void SetMappingYDenominator(tools::Long nDenom)
    {
        maMapRes.mnMapScDenomY = nDenom;
    }

    SAL_DLLPRIVATE void SetOrigin(const Point& rPt) { maMapMode.SetOrigin(rPt); }

    SAL_DLLPRIVATE sal_Int32 GetDPIX() const;
    SAL_DLLPRIVATE sal_Int32 GetDPIY() const;

    SAL_DLLPRIVATE Size GetPixelOffset() const { return Size(mnOutOffOrigX, mnOutOffOrigY); }
    SAL_DLLPRIVATE void SetPixelOffset(const Size& rSize);
    SAL_DLLPRIVATE tools::Long GetPixelXOffset() const { return mnOutOffOrigX; }
    SAL_DLLPRIVATE tools::Long GetPixelYOffset() const { return mnOutOffOrigY; }

    SAL_DLLPRIVATE void SetLogicalOffset(const Size& rSize);
    SAL_DLLPRIVATE tools::Long GetLogicalXOffset() const { return mnOutOffLogicX; }
    SAL_DLLPRIVATE tools::Long GetLogicalYOffset() const { return mnOutOffLogicY; }

    SAL_DLLPRIVATE tools::Long GetOutputWidthPixel() const;
    SAL_DLLPRIVATE tools::Long GetOutputHeightPixel() const;
    SAL_DLLPRIVATE Size GetOutputSizePixel() const;

    SAL_DLLPRIVATE void SetOutputWidthPixel(tools::Long nWidth);
    SAL_DLLPRIVATE void SetOutputHeightPixel(tools::Long nHeight);

    SAL_DLLPRIVATE void SetDPIX(sal_Int32 nDPIX);
    SAL_DLLPRIVATE void SetDPIY(sal_Int32 nDPIY);

    SAL_DLLPRIVATE sal_Int32 GetDPIScalePercentage() const;
    SAL_DLLPRIVATE void SetDPIScalePercentage(sal_Int32 nPercentage);

    SAL_DLLPRIVATE float GetDPIScaleFactor() const;

    SAL_DLLPRIVATE tools::Long GetOutOffXPixel() const;
    SAL_DLLPRIVATE tools::Long GetOutOffYPixel() const;

    SAL_DLLPRIVATE void SetDeviceOriginX(tools::Long nX);
    SAL_DLLPRIVATE void SetDeviceOriginY(tools::Long nY);

    SAL_DLLPRIVATE Point GetOutputOffPixel() const;

    SAL_DLLPRIVATE void CalcMapResolution(const MapMode& rMapMode, tools::Long nDPIX,
                                          tools::Long nDPIY);
    SAL_DLLPRIVATE ImplMapRes ResolveMapRes(const MapMode* pMode, const MapMode& rDefaultMapMode,
                                            bool bMap, tools::Long nDPIX, tools::Long nDPIY) const;

    /** Invalidate the view transformation.

     @since AOO bug 75163 (OpenOffice.org 2.4.3 - OOH 680 milestone 212)
     */
    SAL_DLLPRIVATE void InvalidateViewTransform();
    SAL_DLLPRIVATE basegfx::B2DHomMatrix GetViewTransformation() const;
    SAL_DLLPRIVATE basegfx::B2DHomMatrix GetViewTransformation(const MapMode& rMapMode) const;
    SAL_DLLPRIVATE basegfx::B2DHomMatrix GetInverseViewTransformation() const;
    SAL_DLLPRIVATE basegfx::B2DHomMatrix
    GetInverseViewTransformation(const MapMode& rMapMode) const;
    SAL_DLLPRIVATE basegfx::B2DHomMatrix GetDeviceTransformation() const;

    SAL_DLLPRIVATE tools::Long LogicHeightToDevicePixel(tools::Long nHeight) const;
    SAL_DLLPRIVATE double LogicHeightToDeviceSubPixel(tools::Long nHeight) const;
    SAL_DLLPRIVATE Point SubPixelToLogic(const basegfx::B2DPoint& rDevicePt) const;
    SAL_DLLPRIVATE tools::Long DevicePixelToLogicWidth(tools::Long nWidth) const;
    SAL_DLLPRIVATE tools::Long DevicePixelToLogicHeight(tools::Long nHeight) const;

    SAL_DLLPRIVATE basegfx::B2DPoint LogicToDeviceSubPixel(const Point& rLogicPt) const;
    SAL_DLLPRIVATE double LogicWidthToDeviceSubPixel(tools::Long nWidth) const;
    SAL_DLLPRIVATE static tools::Long LogicToLogic(tools::Long nLongSource, MapUnit eUnitSource,
                                                   MapUnit eUnitDest);
    SAL_DLLPRIVATE static tools::Rectangle LogicToLogic(const tools::Rectangle& rRectSource,
                                                        const MapMode& rMapModeSource,
                                                        const MapMode& rMapModeDest);
    SAL_DLLPRIVATE static basegfx::B2DHomMatrix LogicToLogic(const MapMode& rMapModeSource,
                                                             const MapMode& rMapModeDest);
    SAL_DLLPRIVATE static basegfx::B2DPolygon LogicToLogic(const basegfx::B2DPolygon& rPolySource,
                                                           const MapMode& rMapModeSource,
                                                           const MapMode& rMapModeDest);
    SAL_DLLPRIVATE static Size LogicToLogic(const Size& rSzSource, const MapMode& rMapModeSource,
                                            const MapMode& rMapModeDest);
    SAL_DLLPRIVATE static Point LogicToLogic(const Point& rPtSource, const MapMode& rMapModeSource,
                                             const MapMode& rMapModeDest);
    SAL_DLLPRIVATE tools::Rectangle LogicToLogic(const tools::Rectangle& rRectSource,
                                                 const MapMode* pMapModeSource,
                                                 const MapMode* pMapModeDest) const;
    SAL_DLLPRIVATE Size LogicToLogic(const Size& rSzSource, const MapMode* pMapModeSource,
                                     const MapMode* pMapModeDest) const;
    SAL_DLLPRIVATE Point LogicToLogic(const Point& rPtSource, const MapMode* pMapModeSource,
                                      const MapMode* pMapModeDest) const;
    SAL_DLLPRIVATE Point LogicToPixel(const Point& rLogicPt) const;
    SAL_DLLPRIVATE Size LogicToPixel(const Size& rLogicSize) const;
    SAL_DLLPRIVATE tools::Rectangle LogicToPixel(const tools::Rectangle& rLogicRect) const;
    SAL_DLLPRIVATE tools::Polygon LogicToPixel(const tools::Polygon& rLogicPoly) const;
    SAL_DLLPRIVATE tools::PolyPolygon LogicToPixel(const tools::PolyPolygon& rLogicPolyPoly) const;
    SAL_DLLPRIVATE basegfx::B2DPolyPolygon
    LogicToPixel(const basegfx::B2DPolyPolygon& rLogicPolyPoly) const;
    SAL_DLLPRIVATE vcl::Region LogicToPixel(const vcl::Region& rLogicRegion) const;
    SAL_DLLPRIVATE Point LogicToPixel(const Point& rLogicPt, const MapMode& rMapMode) const;
    SAL_DLLPRIVATE Size LogicToPixel(const Size& rLogicSize, const MapMode& rMapMode) const;
    SAL_DLLPRIVATE tools::Rectangle LogicToPixel(const tools::Rectangle& rLogicRect,
                                                 const MapMode& rMapMode) const;
    SAL_DLLPRIVATE tools::Polygon LogicToPixel(const tools::Polygon& rLogicPoly,
                                               const MapMode& rMapMode) const;
    SAL_DLLPRIVATE basegfx::B2DPolyPolygon
    LogicToPixel(const basegfx::B2DPolyPolygon& rLogicPolyPoly, const MapMode& rMapMode) const;
    SAL_DLLPRIVATE Point PixelToLogic(const Point& rDevicePt) const;
    SAL_DLLPRIVATE Size PixelToLogic(const Size& rDeviceSize) const;
    SAL_DLLPRIVATE tools::Rectangle PixelToLogic(const tools::Rectangle& rDeviceRect) const;
    SAL_DLLPRIVATE tools::Polygon PixelToLogic(const tools::Polygon& rDevicePoly) const;
    SAL_DLLPRIVATE tools::PolyPolygon PixelToLogic(const tools::PolyPolygon& rDevicePolyPoly) const;
    SAL_DLLPRIVATE basegfx::B2DPolyPolygon
    PixelToLogic(const basegfx::B2DPolyPolygon& rPixelPolyPoly) const;
    SAL_DLLPRIVATE basegfx::B2DRectangle
    PixelToLogic(const basegfx::B2DRectangle& rDeviceRect) const;
    SAL_DLLPRIVATE vcl::Region PixelToLogic(const vcl::Region& rDeviceRegion) const;
    SAL_DLLPRIVATE Point PixelToLogic(const Point& rDevicePt, const MapMode& rMapMode) const;
    SAL_DLLPRIVATE Size PixelToLogic(const Size& rDeviceSize, const MapMode& rMapMode) const;
    SAL_DLLPRIVATE tools::Rectangle PixelToLogic(const tools::Rectangle& rDeviceRect,
                                                 const MapMode& rMapMode) const;
    SAL_DLLPRIVATE tools::Polygon PixelToLogic(const tools::Polygon& rDevicePoly,
                                               const MapMode& rMapMode) const;
    SAL_DLLPRIVATE basegfx::B2DPolygon PixelToLogic(const basegfx::B2DPolygon& rPixelPoly,
                                                    const MapMode& rMapMode) const;
    SAL_DLLPRIVATE basegfx::B2DPolyPolygon
    PixelToLogic(const basegfx::B2DPolyPolygon& rPixelPolyPoly, const MapMode& rMapMode) const;
    SAL_DLLPRIVATE tools::Long LogicWidthToDevicePixel(tools::Long nWidth) const;
    SAL_DLLPRIVATE Point LogicToDevicePixel(const Point& rLogicPt) const;
    SAL_DLLPRIVATE tools::Rectangle LogicToDevicePixel(const tools::Rectangle& rLogicRect) const;

    /** Convert a logical size to the size on the physical device.

     @param         rLogicSize  Const reference to a size in logical units

     @returns Physical size on the device.
     */
    SAL_DLLPRIVATE Size LogicToDevicePixel(const Size& rLogicSize) const;

    /** Convert a rectangle in physical pixel units to a rectangle in physical pixel units and coords.

     @param         rPixelRect  Const reference to rectangle in logical units and coords.

     @returns Rectangle based on logical coordinates and units.
     */
    SAL_DLLPRIVATE tools::Rectangle DevicePixelToLogic(const tools::Rectangle& rPixelRect) const;

    /** Convert a logical polygon to a polygon in physical device pixel units.

     @param         rLogicPoly  Const reference to a polygon in logical units

     @returns Polygon based on physical device pixel coordinates and units.
     */
    SAL_DLLPRIVATE tools::Polygon LogicToDevicePixel(const tools::Polygon& rLogicPoly) const;

    /** Convert a logical B2DPolygon to a B2DPolygon in physical device pixel units.

     @param         rLogicSize  Const reference to a B2DPolygon in logical units

     @returns B2DPolyPolygon based on physical device pixel coordinates and units.
     */
    SAL_DLLPRIVATE ::basegfx::B2DPolygon
    LogicToDevicePixel(const ::basegfx::B2DPolygon& rLogicPoly) const;

    /** Convert a logical polypolygon to a polypolygon in physical device pixel units.

     @param         rLogicPolyPoly  Const reference to a polypolygon in logical units

     @returns Polypolygon based on physical device pixel coordinates and units.
     */
    SAL_DLLPRIVATE tools::PolyPolygon
    LogicToDevicePixel(const tools::PolyPolygon& rLogicPolyPoly) const;

    /** Convert a line in logical units to a line in physical device pixel units.

     @param         rLineInfo   Const reference to a line in logical units

     @returns Line based on physical device pixel coordinates and units.
     */
    SAL_DLLPRIVATE LineInfo LogicToDevicePixel(const LineInfo& rLineInfo) const;

    /** Convert a region in pixel units to a region in device pixel units and coords.

     @param         rRegion  Const reference to region.

     @returns vcl::Region based on device pixel coordinates and units.
     */
    SAL_DLLPRIVATE vcl::Region PixelToDevicePixel(const vcl::Region& rRegion) const;

    /** Convert a logical X coordinate to a device pixel's X coordinate.

     To get the device's X coordinate, it must calculate the mapping offset
     coordinate X position (if there is one - if not then it just adds
     the pseudo-window offset to the logical X coordinate), the X-DPI of
     the device and the mapping's X scaling factor.

     @param         nX          Logical X coordinate

     @returns Device's X pixel coordinate
     */
    SAL_DLLPRIVATE tools::Long LogicXToDevicePixel(tools::Long nX) const;

    /** Convert a logical Y coordinate to a device pixel's Y coordinate.

     To get the device's Y coordinate, it must calculate the mapping offset
     coordinate Y position (if there is one - if not then it just adds
     the pseudo-window offset to the logical Y coordinate), the Y-DPI of
     the device and the mapping's Y scaling factor.

     @param         nY          Logical Y coordinate

     @returns Device's Y pixel coordinate
     */
    SAL_DLLPRIVATE tools::Long LogicYToDevicePixel(tools::Long nY) const;

    SAL_DLLPRIVATE double DevicePixelToLogicWidthDouble(double nWidth) const;
    SAL_DLLPRIVATE double DevicePixelToLogicHeightDouble(double nHeight) const;
};

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
