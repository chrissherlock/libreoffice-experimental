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
#include <basegfx/matrix/b2dhommatrix.hxx>

#include <vcl/devicecoordinate.hxx>
#include <vcl/lineinfo.hxx>
#include <vcl/region.hxx>
#include <vcl/MappingMetrics.hxx>
#include <vcl/ViewTransformer.hxx>

#include <memory>

struct ViewTransformer;
class MapMode;

class VCL_DLLPUBLIC Geometry
{
public:
    Geometry();
    ~Geometry();

    bool IsMapModeEnabled() const;
    void EnableMapMode();
    void DisableMapMode();

    Size GetSizeInPixels() const;
    tools::Long GetWidthInPixels() const;
    tools::Long GetHeightInPixels() const;
    void SetSizeInPixels(Size const& rSize);
    void SetWidthInPixels(tools::Long nWidth);
    void SetHeightInPixels(tools::Long nHeight);

    tools::Long GetXOffsetInPixels() const;
    tools::Long GetYOffsetInPixels() const;
    void SetXOffsetInPixels(tools::Long nOffsetXpx);
    void SetYOffsetInPixels(tools::Long nOffsetYpx);

    Size GetOffsetFromOriginInPixels() const;
    tools::Long GetXOffsetFromOriginInPixels() const;
    tools::Long GetYOffsetFromOriginInPixels() const;
    void SetOffsetFromOriginInPixels(Size const& rOffset);

    void ResetLogicalUnitsOffsetFromOrigin();
    sal_uInt32 GetXOffsetFromOriginInLogicalUnits() const;
    void SetXOffsetFromOriginInLogicalUnits(tools::Long nOffsetFromOriginXInLogicalUnits);
    sal_uInt32 GetYOffsetFromOriginInLogicalUnits() const;
    void SetYOffsetFromOriginInLogicalUnits(tools::Long nOffsetFromOriginYInLogicalUnits);

    tools::Long GetXMapOffset() const;
    void SetXMapOffset(tools::Long);
    tools::Long GetYMapOffset() const;
    void SetYMapOffset(tools::Long);
    tools::Long GetXMapNumerator() const;
    void SetXMapNumerator(tools::Long nNumerator);
    tools::Long GetYMapNumerator() const;
    void SetYMapNumerator(tools::Long nNumerator);
    tools::Long GetXMapDenominator() const;
    void SetXMapDenominator(tools::Long nDenominator);
    tools::Long GetYMapDenominator() const;
    void SetYMapDenominator(tools::Long nDenominator);

    sal_Int32 GetDPIX() const;
    sal_Int32 GetDPIY() const;
    void SetDPIX(sal_Int32 nDPIX);
    void SetDPIY(sal_Int32 nDPIY);
    float GetDPIScaleFactor() const;
    sal_Int32 GetDPIScalePercentage() const;
    void SetDPIScalePercentage(sal_Int32 nPercentage);

    basegfx::B2DHomMatrix GetViewTransformation() const;
    basegfx::B2DHomMatrix GetViewTransformation(MapMode const& rMapMode) const;
    basegfx::B2DHomMatrix GetInverseViewTransformation() const;
    basegfx::B2DHomMatrix GetInverseViewTransformation(MapMode const& rMapMode) const;

    void InvalidateViewTransform();

    static tools::Long LogicToPixel(tools::Long n, tools::Long nDPI, tools::Long nMapNum,
                                    tools::Long nMapDenom);
    static tools::Long PixelToLogic(tools::Long n, tools::Long nDPI, tools::Long nMapNum,
                                    tools::Long nMapDenom);

    tools::Long LogicXToDevicePixel(tools::Long nX) const;
    tools::Long LogicYToDevicePixel(tools::Long nY) const;
    tools::Long LogicWidthToDevicePixel(tools::Long nWidth) const;
    tools::Long LogicHeightToDevicePixel(tools::Long nHeight) const;
    float FloatLogicHeightToDevicePixel(float fLogicHeight) const;
    Point LogicToDevicePixel(const Point& rLogicPt) const;
    Size LogicToDevicePixel(const Size& rLogicSize) const;
    tools::Rectangle LogicToDevicePixel(const tools::Rectangle& rLogicRect) const;
    tools::Polygon LogicToDevicePixel(const tools::Polygon& rLogicPoly) const;
    tools::PolyPolygon LogicToDevicePixel(const tools::PolyPolygon& rLogicPolyPoly) const;
    LineInfo LogicToDevicePixel(LineInfo const& rLineInfos) const;

    tools::Long DevicePixelToLogicWidth(tools::Long nWidth) const;
    tools::Long DevicePixelToLogicHeight(tools::Long nHeight) const;
    tools::Rectangle DevicePixelToLogic(const tools::Rectangle& rPixelRect) const;
    vcl::Region PixelToDevicePixel(const vcl::Region& rRegion) const;

    DeviceCoordinate LogicWidthToDeviceCoordinate(tools::Long nWidth) const;

    Point LogicToPixel(Point const& rLogicPt) const;
    Size LogicToPixel(Size const& rLogicSize) const;
    vcl::Region LogicToPixel(vcl::Region const& rLogicRegion) const;
    tools::Rectangle LogicToPixel(tools::Rectangle const& rLogicRect) const;
    tools::Polygon LogicToPixel(tools::Polygon const& rLogicPoly) const;
    tools::PolyPolygon LogicToPixel(tools::PolyPolygon const& rLogicPolyPoly) const;
    basegfx::B2DPolyPolygon LogicToPixel(basegfx::B2DPolyPolygon const& rLogicPolyPoly) const;

    Point LogicToPixel(Point const& rLogicPt, MapMode const& rMapMode) const;
    Size LogicToPixel(Size const& rLogicSize, MapMode const& rMapMode) const;
    tools::Rectangle LogicToPixel(tools::Rectangle const& rLogicRect,
                                  MapMode const& rMapMode) const;
    tools::Polygon LogicToPixel(tools::Polygon const& rLogicPoly, MapMode const& rMapMode) const;
    basegfx::B2DPolyPolygon LogicToPixel(basegfx::B2DPolyPolygon const& rLogicPolyPoly,
                                         MapMode const& rMapMode) const;

    /** Get device transformation.

        @since AOO bug 75163 (OpenOffice.org 2.4.3 - OOH 680 milestone 212)
     */
    basegfx::B2DHomMatrix GetDeviceTransformation() const; // TODO make private

    MappingMetrics GetMappingMetrics() const;
    void CalculateMappingMetrics(MapMode const& rMapMode, tools::Long nDPIX, tools::Long nDPIY);

private:
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

    std::unique_ptr<ViewTransformer> mpViewTransformer;

    MappingMetrics maMappingMetrics;
};

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
