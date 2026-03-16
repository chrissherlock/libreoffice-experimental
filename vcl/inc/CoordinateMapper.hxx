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
#include <vcl/salgtype.hxx>
#include <vcl/lineinfo.hxx>
#include <vcl/mapmod.hxx>
#include <vcl/region.hxx>
#include <vcl/rendercontext/ImplMapRes.hxx>

class FontMetric;

namespace vcl::text
{
struct LayoutResources;
}

class VCL_DLLPUBLIC CoordinateMapper
{
private:
    sal_uInt64 mnGenerationID = 1; // Start at 1 to force initial mismatch
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

    /** Calculates the subpixel factor (1 or 64) based on the mapping state. */
    static tools::Long GetSubPixelFactor(const CoordinateMapper& rMapper);

    /** Converts logical widths to layout units, accounting for subpixel scaling. */
    static double GetLayoutPixelWidth(const CoordinateMapper& rMapper, tools::Long nLogicWidth,
                                      tools::Long nSubPixelFactor);

public:
    // Generation ID for Lazy Evaluation
    sal_uInt64 GetGenerationID() const { return mnGenerationID; }
    void IncreaseGenerationID() { mnGenerationID++; }

    bool IsMapModeEnabled() const { return mbMap; }
    void EnableMapMode(bool bEnable = true);
    bool UpdateMapMode(const MapMode& rNewMapMode, sal_Int32 nDPIX, sal_Int32 nDPIY);
    bool ResetToDefault();
    void SetOffset(const Size& rOffset);

    const MapMode& GetMapMode() const { return maMapMode; }
    bool IsDefaultMapMode() const { return maMapMode.IsDefault(); }
    void ResetMapMode();
    void ResetMapMode(const MapMode& rMapMode);
    MapUnit GetMapUnit() const { return maMapMode.GetMapUnit(); }

    const Fraction& GetScaleX() const { return maMapMode.GetScaleX(); }
    const Fraction& GetScaleY() const { return maMapMode.GetScaleY(); }
    void SetScaleX(const Fraction& rScale)
    {
        IncreaseGenerationID();
        maMapMode.SetScaleX(rScale);
    }
    void SetScaleY(const Fraction& rScale)
    {
        IncreaseGenerationID();
        maMapMode.SetScaleY(rScale);
    }

    tools::Long GetMappingXOffset() const { return maMapRes.mnMapOfsX; }
    tools::Long GetMappingYOffset() const { return maMapRes.mnMapOfsY; }
    tools::Long GetMappingXNumerator() const { return maMapRes.mnMapScNumX; }
    tools::Long GetMappingYNumerator() const { return maMapRes.mnMapScNumY; }
    tools::Long GetMappingXDenominator() const { return maMapRes.mnMapScDenomX; }
    tools::Long GetMappingYDenominator() const { return maMapRes.mnMapScDenomY; }

    void SetMappingXOffset(tools::Long nOffset)
    {
        IncreaseGenerationID();
        maMapRes.mnMapOfsX = nOffset;
    }
    void SetMappingYOffset(tools::Long nOffset)
    {
        IncreaseGenerationID();
        maMapRes.mnMapOfsY = nOffset;
    }
    void SetMappingXNumerator(tools::Long nNum)
    {
        IncreaseGenerationID();
        maMapRes.mnMapScNumX = nNum;
    }
    void SetMappingYNumerator(tools::Long nNum)
    {
        IncreaseGenerationID();
        maMapRes.mnMapScNumY = nNum;
    }
    void SetMappingXDenominator(tools::Long nDenom)
    {
        IncreaseGenerationID();
        maMapRes.mnMapScDenomX = nDenom;
    }
    void SetMappingYDenominator(tools::Long nDenom)
    {
        IncreaseGenerationID();
        maMapRes.mnMapScDenomY = nDenom;
    }

    void SetOrigin(const Point& rPt)
    {
        IncreaseGenerationID();
        maMapMode.SetOrigin(rPt);
    }

    sal_Int32 GetDPIX() const;
    sal_Int32 GetDPIY() const;

    Size GetPixelOffset() const { return Size(mnOutOffOrigX, mnOutOffOrigY); }
    void SetPixelOffset(const Size& rSize);
    tools::Long GetPixelXOffset() const { return mnOutOffOrigX; }
    tools::Long GetPixelYOffset() const { return mnOutOffOrigY; }

    void SetLogicalOffset(const Size& rSize);
    tools::Long GetLogicalXOffset() const { return mnOutOffLogicX; }
    tools::Long GetLogicalYOffset() const { return mnOutOffLogicY; }

    tools::Long GetOutputWidthPixel() const;
    tools::Long GetOutputHeightPixel() const;
    Size GetOutputSizePixel() const;

    void SetOutputWidthPixel(tools::Long nWidth);
    void SetOutputHeightPixel(tools::Long nHeight);

    void SetDPIX(sal_Int32 nDPIX);
    void SetDPIY(sal_Int32 nDPIY);

    sal_Int32 GetDPIScalePercentage() const;
    void SetDPIScalePercentage(sal_Int32 nPercentage);

    float GetDPIScaleFactor() const;

    tools::Long GetOutOffXPixel() const;
    tools::Long GetOutOffYPixel() const;

    void SetDeviceOriginX(tools::Long nX);
    void SetDeviceOriginY(tools::Long nY);

    Point GetOutputOffPixel() const;

    void CalcMapResolution(const MapMode& rMapMode, tools::Long nDPIX, tools::Long nDPIY);
    ImplMapRes ResolveMapRes(const MapMode* pMode, const MapMode& rDefaultMapMode, bool bMap,
                             tools::Long nDPIX, tools::Long nDPIY) const;

    /** Invalidate the view transformation.

     @since AOO bug 75163 (OpenOffice.org 2.4.3 - OOH 680 milestone 212)
     */
    void InvalidateViewTransform();
    basegfx::B2DHomMatrix GetViewTransformation() const;
    basegfx::B2DHomMatrix GetViewTransformation(const MapMode& rMapMode) const;
    basegfx::B2DHomMatrix GetInverseViewTransformation() const;
    basegfx::B2DHomMatrix GetInverseViewTransformation(const MapMode& rMapMode) const;
    basegfx::B2DHomMatrix GetDeviceTransformation() const;

    tools::Long LogicHeightToDevicePixel(tools::Long nHeight) const;
    double LogicHeightToDeviceSubPixel(tools::Long nHeight) const;
    Point SubPixelToLogic(const basegfx::B2DPoint& rDevicePt) const;
    tools::Long DevicePixelToLogicWidth(tools::Long nWidth) const;
    tools::Long DevicePixelToLogicHeight(tools::Long nHeight) const;

    basegfx::B2DPoint LogicToDeviceSubPixel(const Point& rLogicPt) const;
    double LogicWidthToDeviceSubPixel(tools::Long nWidth) const;
    static tools::Long LogicToLogic(tools::Long nLongSource, MapUnit eUnitSource,
                                    MapUnit eUnitDest);
    static tools::Rectangle LogicToLogic(const tools::Rectangle& rRectSource,
                                         const MapMode& rMapModeSource,
                                         const MapMode& rMapModeDest);
    static basegfx::B2DHomMatrix LogicToLogic(const MapMode& rMapModeSource,
                                              const MapMode& rMapModeDest);
    static basegfx::B2DPolygon LogicToLogic(const basegfx::B2DPolygon& rPolySource,
                                            const MapMode& rMapModeSource,
                                            const MapMode& rMapModeDest);
    static Size LogicToLogic(const Size& rSzSource, const MapMode& rMapModeSource,
                             const MapMode& rMapModeDest);
    static Point LogicToLogic(const Point& rPtSource, const MapMode& rMapModeSource,
                              const MapMode& rMapModeDest);
    tools::Rectangle LogicToLogic(const tools::Rectangle& rRectSource,
                                  const MapMode* pMapModeSource, const MapMode* pMapModeDest) const;
    Size LogicToLogic(const Size& rSzSource, const MapMode* pMapModeSource,
                      const MapMode* pMapModeDest) const;
    Point LogicToLogic(const Point& rPtSource, const MapMode* pMapModeSource,
                       const MapMode* pMapModeDest) const;
    Point LogicToPixel(const Point& rLogicPt) const;
    Size LogicToPixel(const Size& rLogicSize) const;
    tools::Rectangle LogicToPixel(const tools::Rectangle& rLogicRect) const;
    tools::Polygon LogicToPixel(const tools::Polygon& rLogicPoly) const;
    tools::PolyPolygon LogicToPixel(const tools::PolyPolygon& rLogicPolyPoly) const;
    basegfx::B2DPolyPolygon LogicToPixel(const basegfx::B2DPolyPolygon& rLogicPolyPoly) const;
    vcl::Region LogicToPixel(const vcl::Region& rLogicRegion) const;
    Point LogicToPixel(const Point& rLogicPt, const MapMode& rMapMode) const;
    Size LogicToPixel(const Size& rLogicSize, const MapMode& rMapMode) const;
    tools::Rectangle LogicToPixel(const tools::Rectangle& rLogicRect,
                                  const MapMode& rMapMode) const;
    tools::Polygon LogicToPixel(const tools::Polygon& rLogicPoly, const MapMode& rMapMode) const;
    basegfx::B2DPolyPolygon LogicToPixel(const basegfx::B2DPolyPolygon& rLogicPolyPoly,
                                         const MapMode& rMapMode) const;
    Point PixelToLogic(const Point& rDevicePt) const;
    Size PixelToLogic(const Size& rDeviceSize) const;
    tools::Rectangle PixelToLogic(const tools::Rectangle& rDeviceRect) const;
    tools::Polygon PixelToLogic(const tools::Polygon& rDevicePoly) const;
    tools::PolyPolygon PixelToLogic(const tools::PolyPolygon& rDevicePolyPoly) const;
    basegfx::B2DPolyPolygon PixelToLogic(const basegfx::B2DPolyPolygon& rPixelPolyPoly) const;
    basegfx::B2DRectangle PixelToLogic(const basegfx::B2DRectangle& rDeviceRect) const;
    vcl::Region PixelToLogic(const vcl::Region& rDeviceRegion) const;
    Point PixelToLogic(const Point& rDevicePt, const MapMode& rMapMode) const;
    Size PixelToLogic(const Size& rDeviceSize, const MapMode& rMapMode) const;
    tools::Rectangle PixelToLogic(const tools::Rectangle& rDeviceRect,
                                  const MapMode& rMapMode) const;
    tools::Polygon PixelToLogic(const tools::Polygon& rDevicePoly, const MapMode& rMapMode) const;
    basegfx::B2DPolygon PixelToLogic(const basegfx::B2DPolygon& rPixelPoly,
                                     const MapMode& rMapMode) const;
    basegfx::B2DPolyPolygon PixelToLogic(const basegfx::B2DPolyPolygon& rPixelPolyPoly,
                                         const MapMode& rMapMode) const;
    tools::Long LogicWidthToDevicePixel(tools::Long nWidth) const;
    void MirrorDevicePixelPolygon(tools::Polygon& rPoly, tools::Long nFrameWidth, bool bRTL,
                                  bool bAntiparallel) const;
    void MirrorDevicePixelPolyPolygon(tools::PolyPolygon& rPolyPoly, tools::Long nFrameWidth,
                                      bool bRTL, bool bAntiparallel) const;

    /** Mirrors a device pixel rectangle into physical frame coordinates based on RTL/Antiparallel rules */
    void MirrorDevicePixelRect(tools::Rectangle& rRect, tools::Long nFrameWidth, bool bRTL,
                               bool bAntiparallel) const;

    /** Mirrors a device pixel point into physical frame coordinates based on RTL/Antiparallel rules */
    void MirrorDevicePixelPoint(Point& rPt, tools::Long nFrameWidth, bool bRTL,
                                bool bAntiparallel) const;

    Point LogicToDevicePixel(const Point& rLogicPt) const;
    tools::Rectangle LogicToDevicePixel(const tools::Rectangle& rLogicRect) const;

    /** Convert a logical size to the size on the physical device.

     @param         rLogicSize  Const reference to a size in logical units

     @returns Physical size on the device.
     */
    Size LogicToDevicePixel(const Size& rLogicSize) const;

    /** Convert a rectangle in physical pixel units to a rectangle in physical pixel units and coords.

     @param         rPixelRect  Const reference to rectangle in logical units and coords.

     @returns Rectangle based on logical coordinates and units.
     */
    tools::Rectangle DevicePixelToLogic(const tools::Rectangle& rPixelRect) const;

    /** Convert a logical polygon to a polygon in physical device pixel units.

     @param         rLogicPoly  Const reference to a polygon in logical units

     @returns Polygon based on physical device pixel coordinates and units.
     */
    tools::Polygon LogicToDevicePixel(const tools::Polygon& rLogicPoly) const;

    /** Convert a logical B2DPolygon to a B2DPolygon in physical device pixel units.

     @param         rLogicSize  Const reference to a B2DPolygon in logical units

     @returns B2DPolyPolygon based on physical device pixel coordinates and units.
     */
    ::basegfx::B2DPolygon LogicToDevicePixel(const ::basegfx::B2DPolygon& rLogicPoly) const;

    /** Convert a logical polypolygon to a polypolygon in physical device pixel units.

     @param         rLogicPolyPoly  Const reference to a polypolygon in logical units

     @returns Polypolygon based on physical device pixel coordinates and units.
     */
    tools::PolyPolygon LogicToDevicePixel(const tools::PolyPolygon& rLogicPolyPoly) const;

    /** Convert a line in logical units to a line in physical device pixel units.

     @param         rLineInfo   Const reference to a line in logical units

     @returns Line based on physical device pixel coordinates and units.
     */
    LineInfo LogicToDevicePixel(const LineInfo& rLineInfo) const;

    /** Convert a region in pixel units to a region in device pixel units and coords.

     @param         rRegion  Const reference to region.

     @returns vcl::Region based on device pixel coordinates and units.
     */
    vcl::Region PixelToDevicePixel(const vcl::Region& rRegion) const;

    /** Convert a logical X coordinate to a device pixel's X coordinate.

     To get the device's X coordinate, it must calculate the mapping offset
     coordinate X position (if there is one - if not then it just adds
     the pseudo-window offset to the logical X coordinate), the X-DPI of
     the device and the mapping's X scaling factor.

     @param         nX          Logical X coordinate

     @returns Device's X pixel coordinate
     */
    tools::Long LogicXToDevicePixel(tools::Long nX) const;

    /** Convert a logical Y coordinate to a device pixel's Y coordinate.

     To get the device's Y coordinate, it must calculate the mapping offset
     coordinate Y position (if there is one - if not then it just adds
     the pseudo-window offset to the logical Y coordinate), the Y-DPI of
     the device and the mapping's Y scaling factor.

     @param         nY          Logical Y coordinate

     @returns Device's Y pixel coordinate
     */
    tools::Long LogicYToDevicePixel(tools::Long nY) const;

    double DevicePixelToLogicWidthDouble(double nWidth) const;
    double DevicePixelToLogicHeightDouble(double nHeight) const;

    /** Calculates the subpixel layout width from logical units. */
    static double CalculateLayoutWidth(const vcl::text::LayoutResources& rRes,
                                       tools::Long nLogicWidth);

    /**
     * Robustly rounds a floating-point range to integer device pixels.
     * Uses endpoint-rounding to ensure gapless tiling between adjacent objects.
     */
    tools::Rectangle RoundDeviceRect(const basegfx::B2DRange& rRange) const;

    SalTwoRect ToDeviceRect(const Point& rDestPt, const Size& rDestSize, const Point& rSrcPtPixel,
                            const Size& rSrcSizePixel) const;

    void PixelToLogic(FontMetric& rMetric, tools::Long nExternalLeadingOverride) const;
};

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
