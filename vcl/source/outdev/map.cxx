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

#include <sal/log.hxx>
#include <osl/diagnose.h>
#include <tools/bigint.hxx>
#include <tools/debug.hxx>
#include <basegfx/matrix/b2dhommatrix.hxx>
#include <o3tl/enumarray.hxx>

#include <vcl/cursor.hxx>
#include <vcl/gdimtf.hxx>
#include <vcl/lineinfo.hxx>
#include <vcl/metaact.hxx>
#include <vcl/virdev.hxx>
#include <vcl/wrkwin.hxx>

#include <window.h>
#include <svdata.hxx>
#include <ImplOutDevData.hxx>

// we don't actually handle units beyond, hence the zeros in the arrays
const MapUnit s_MaxValidUnit = MapUnit::MapPixel;
const o3tl::enumarray<MapUnit, tools::Long> aImplNumeratorAry
    = { 1, 1, 5, 50, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0 };
const o3tl::enumarray<MapUnit, tools::Long> aImplDenominatorAry
    = { 2540, 254, 127, 127, 1000, 100, 10, 1, 72, 1440, 1, 0, 0, 0 };

/*
Reduces accuracy until it is a fraction (should become
ctor fraction once); we could also do this with BigInts
*/

static Fraction ImplMakeFraction(tools::Long nN1, tools::Long nN2, tools::Long nD1, tools::Long nD2)
{
    if (nD1 == 0
        || nD2 == 0) //under these bad circumstances the following while loop will be endless
    {
        SAL_WARN("vcl.gdi", "Invalid parameter for ImplMakeFraction");
        return Fraction(1, 1);
    }

    tools::Long i = 1;

    if (nN1 < 0)
    {
        i = -i;
        nN1 = -nN1;
    }
    if (nN2 < 0)
    {
        i = -i;
        nN2 = -nN2;
    }
    if (nD1 < 0)
    {
        i = -i;
        nD1 = -nD1;
    }
    if (nD2 < 0)
    {
        i = -i;
        nD2 = -nD2;
    }
    // all positive; i sign

    Fraction aF = Fraction(i * nN1, nD1) * Fraction(nN2, nD2);

    while (!aF.IsValid())
    {
        if (nN1 > nN2)
            nN1 = (nN1 + 1) / 2;
        else
            nN2 = (nN2 + 1) / 2;
        if (nD1 > nD2)
            nD1 = (nD1 + 1) / 2;
        else
            nD2 = (nD2 + 1) / 2;

        aF = Fraction(i * nN1, nD1) * Fraction(nN2, nD2);
    }

    aF.ReduceInaccurate(32);
    return aF;
}

void OutputDevice::EnableMapMode()
{
    RenderContext2::EnableMapMode();

    if (mpAlphaVDev)
        mpAlphaVDev->EnableMapMode();
}

void OutputDevice::DisableMapMode()
{
    RenderContext2::DisableMapMode();

    if (mpAlphaVDev)
        mpAlphaVDev->DisableMapMode();
}

void OutputDevice::SetMapMode()
{
    if (mpMetaFile)
        mpMetaFile->AddAction(new MetaMapModeAction(MapMode()));

    RenderContext2::SetMapMode();

    if (mpAlphaVDev)
        mpAlphaVDev->SetMapMode();
}

void OutputDevice::SetMapMode(MapMode const& rNewMapMode)
{
    if (mpMetaFile)
        mpMetaFile->AddAction(new MetaMapModeAction(rNewMapMode));

    RenderContext2::SetMapMode(rNewMapMode);

    // do nothing if MapMode was not changed
    if (maMapMode == rNewMapMode)
        return;

    if (mpAlphaVDev)
        mpAlphaVDev->SetMapMode(rNewMapMode);
}

void OutputDevice::SetMetafileMapMode(const MapMode& rNewMapMode, bool bIsRecord)
{
    if (bIsRecord)
        SetRelativeMapMode(rNewMapMode);
    else
        SetMapMode(rNewMapMode);
}

void OutputDevice::SetRelativeMapMode(const MapMode& rNewMapMode)
{
    // do nothing if MapMode did not change
    if (maMapMode == rNewMapMode)
        return;

    MapUnit eOld = maMapMode.GetMapUnit();
    MapUnit eNew = rNewMapMode.GetMapUnit();

    // a?F = rNewMapMode.GetScale?() / maMapMode.GetScale?()
    Fraction aXF = ImplMakeFraction(
        rNewMapMode.GetScaleX().GetNumerator(), maMapMode.GetScaleX().GetDenominator(),
        rNewMapMode.GetScaleX().GetDenominator(), maMapMode.GetScaleX().GetNumerator());
    Fraction aYF = ImplMakeFraction(
        rNewMapMode.GetScaleY().GetNumerator(), maMapMode.GetScaleY().GetDenominator(),
        rNewMapMode.GetScaleY().GetDenominator(), maMapMode.GetScaleY().GetNumerator());

    Point aPt(LogicToLogic(Point(), nullptr, &rNewMapMode));
    if (eNew != eOld)
    {
        if (eOld > MapUnit::MapPixel)
        {
            SAL_WARN("vcl.gdi", "Not implemented MapUnit");
        }
        else if (eNew > MapUnit::MapPixel)
        {
            SAL_WARN("vcl.gdi", "Not implemented MapUnit");
        }
        else
        {
            Fraction aF(aImplNumeratorAry[eNew] * aImplDenominatorAry[eOld],
                        aImplNumeratorAry[eOld] * aImplDenominatorAry[eNew]);

            // a?F =  a?F * aF
            aXF = ImplMakeFraction(aXF.GetNumerator(), aF.GetNumerator(), aXF.GetDenominator(),
                                   aF.GetDenominator());
            aYF = ImplMakeFraction(aYF.GetNumerator(), aF.GetNumerator(), aYF.GetDenominator(),
                                   aF.GetDenominator());
            if (eOld == MapUnit::MapPixel)
            {
                aXF *= Fraction(GetDPIX(), 1);
                aYF *= Fraction(GetDPIY(), 1);
            }
            else if (eNew == MapUnit::MapPixel)
            {
                aXF *= Fraction(1, GetDPIX());
                aYF *= Fraction(1, GetDPIY());
            }
        }
    }

    MapMode aNewMapMode(MapUnit::MapRelative, Point(-aPt.X(), -aPt.Y()), aXF, aYF);
    SetMapMode(aNewMapMode);

    if (eNew != eOld)
        maMapMode = rNewMapMode;

    // #106426# Adapt logical offset when changing MapMode
    SetXOffsetFromOriginInLogicalUnits(Geometry::PixelToLogic(
        GetXOffsetFromOriginInPixels(), GetDPIX(), GetXMapNumerator(), GetXMapDenominator()));
    SetYOffsetFromOriginInLogicalUnits(Geometry::PixelToLogic(
        GetYOffsetFromOriginInPixels(), GetDPIY(), GetYMapNumerator(), GetYMapDenominator()));

    if (mpAlphaVDev)
        mpAlphaVDev->SetRelativeMapMode(rNewMapMode);
}

Point OutputDevice::PixelToLogic(const Point& rDevicePt, const MapMode& rMapMode) const
{
    // calculate nothing if default-MapMode
    if (rMapMode.IsDefault())
        return rDevicePt;

    // calculate MapMode-resolution and convert
    MappingMetrics aMappingMetric(rMapMode, GetDPIX(), GetDPIY());

    return Point(Geometry::PixelToLogic(rDevicePt.X(), GetDPIX(), aMappingMetric.mnMapScNumX,
                                        aMappingMetric.mnMapScDenomX)
                     - aMappingMetric.mnMapOfsX - GetXOffsetFromOriginInLogicalUnits(),
                 Geometry::PixelToLogic(rDevicePt.Y(), GetDPIY(), aMappingMetric.mnMapScNumY,
                                        aMappingMetric.mnMapScDenomY)
                     - aMappingMetric.mnMapOfsY - GetYOffsetFromOriginInLogicalUnits());
}

Size OutputDevice::PixelToLogic(const Size& rDeviceSize, const MapMode& rMapMode) const
{
    // calculate nothing if default-MapMode
    if (rMapMode.IsDefault())
        return rDeviceSize;

    // calculate MapMode-resolution and convert
    MappingMetrics aMappingMetric(rMapMode, GetDPIX(), GetDPIY());

    return Size(Geometry::PixelToLogic(rDeviceSize.Width(), GetDPIX(), aMappingMetric.mnMapScNumX,
                                       aMappingMetric.mnMapScDenomX),
                Geometry::PixelToLogic(rDeviceSize.Height(), GetDPIY(), aMappingMetric.mnMapScNumY,
                                       aMappingMetric.mnMapScDenomY));
}

tools::Rectangle OutputDevice::PixelToLogic(const tools::Rectangle& rDeviceRect,
                                            const MapMode& rMapMode) const
{
    // calculate nothing if default-MapMode
    if (rMapMode.IsDefault() || rDeviceRect.IsEmpty())
        return rDeviceRect;

    // calculate MapMode-resolution and convert
    MappingMetrics aMappingMetric(rMapMode, GetDPIX(), GetDPIY());

    return tools::Rectangle(
        Geometry::PixelToLogic(rDeviceRect.Left(), GetDPIX(), aMappingMetric.mnMapScNumX,
                               aMappingMetric.mnMapScDenomX)
            - aMappingMetric.mnMapOfsX - GetXOffsetFromOriginInLogicalUnits(),
        Geometry::PixelToLogic(rDeviceRect.Top(), GetDPIY(), aMappingMetric.mnMapScNumY,
                               aMappingMetric.mnMapScDenomY)
            - aMappingMetric.mnMapOfsY - GetYOffsetFromOriginInLogicalUnits(),
        Geometry::PixelToLogic(rDeviceRect.Right(), GetDPIX(), aMappingMetric.mnMapScNumX,
                               aMappingMetric.mnMapScDenomX)
            - aMappingMetric.mnMapOfsX - GetXOffsetFromOriginInLogicalUnits(),
        Geometry::PixelToLogic(rDeviceRect.Bottom(), GetDPIY(), aMappingMetric.mnMapScNumY,
                               aMappingMetric.mnMapScDenomY)
            - aMappingMetric.mnMapOfsY - GetYOffsetFromOriginInLogicalUnits());
}

tools::Polygon OutputDevice::PixelToLogic(const tools::Polygon& rDevicePoly,
                                          const MapMode& rMapMode) const
{
    // calculate nothing if default-MapMode
    if (rMapMode.IsDefault())
        return rDevicePoly;

    // calculate MapMode-resolution and convert
    MappingMetrics aMappingMetric(rMapMode, GetDPIX(), GetDPIY());

    sal_uInt16 i;
    sal_uInt16 nPoints = rDevicePoly.GetSize();
    tools::Polygon aPoly(rDevicePoly);

    // get pointer to Point-array (copy data)
    const Point* pPointAry = aPoly.GetConstPointAry();

    for (i = 0; i < nPoints; i++)
    {
        const Point* pPt = &(pPointAry[i]);
        Point aPt;
        aPt.setX(Geometry::PixelToLogic(pPt->X(), GetDPIX(), aMappingMetric.mnMapScNumX,
                                        aMappingMetric.mnMapScDenomX)
                 - aMappingMetric.mnMapOfsX - GetXOffsetFromOriginInLogicalUnits());
        aPt.setY(Geometry::PixelToLogic(pPt->Y(), GetDPIY(), aMappingMetric.mnMapScNumY,
                                        aMappingMetric.mnMapScDenomY)
                 - aMappingMetric.mnMapOfsY - GetYOffsetFromOriginInLogicalUnits());
        aPoly[i] = aPt;
    }

    return aPoly;
}

basegfx::B2DPolygon OutputDevice::PixelToLogic(const basegfx::B2DPolygon& rPixelPoly,
                                               const MapMode& rMapMode) const
{
    basegfx::B2DPolygon aTransformedPoly = rPixelPoly;
    const basegfx::B2DHomMatrix& rTransformationMatrix = GetInverseViewTransformation(rMapMode);
    aTransformedPoly.transform(rTransformationMatrix);
    return aTransformedPoly;
}

basegfx::B2DPolyPolygon OutputDevice::PixelToLogic(const basegfx::B2DPolyPolygon& rPixelPolyPoly,
                                                   const MapMode& rMapMode) const
{
    basegfx::B2DPolyPolygon aTransformedPoly = rPixelPolyPoly;
    const basegfx::B2DHomMatrix& rTransformationMatrix = GetInverseViewTransformation(rMapMode);
    aTransformedPoly.transform(rTransformationMatrix);
    return aTransformedPoly;
}

static void verifyUnitSourceDest(MapUnit eUnitSource, MapUnit eUnitDest)
{
    DBG_ASSERT(eUnitSource != MapUnit::MapSysFont && eUnitSource != MapUnit::MapAppFont
                   && eUnitSource != MapUnit::MapRelative,
               "Source MapUnit is not permitted");
    DBG_ASSERT(eUnitDest != MapUnit::MapSysFont && eUnitDest != MapUnit::MapAppFont
                   && eUnitDest != MapUnit::MapRelative,
               "Destination MapUnit is not permitted");
}

// return (n1 * n2 * n3) / (n4 * n5)
static tools::Long fn5(const tools::Long n1, const tools::Long n2, const tools::Long n3,
                       const tools::Long n4, const tools::Long n5)
{
    if (n1 == 0 || n2 == 0 || n3 == 0 || n4 == 0 || n5 == 0)
        return 0;
    if (std::numeric_limits<tools::Long>::max() / std::abs(n2) < std::abs(n3))
    {
        // a6 is skipped
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
        } // of if
        else
        {
            tools::Long n8 = n4 * n5;

            if (a7.IsNeg())
                a7 -= n8 / 2;
            else
                a7 += n8 / 2;

            a7 /= n8;
        } // of else
        return static_cast<tools::Long>(a7);
    } // of if
    else
    {
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
            } // of if
            else
            {
                tools::Long n8 = n4 * n5;

                if (a7.IsNeg())
                    a7 -= n8 / 2;
                else
                    a7 += n8 / 2;

                a7 /= n8;
            } // of else
            return static_cast<tools::Long>(a7);
        } // of if
        else
        {
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
            } // of if
            else
            {
                const tools::Long n8 = n4 * n5;
                const tools::Long n8_2 = n8 / 2;

                if (n7 < 0)
                {
                    if ((n7 - std::numeric_limits<tools::Long>::min()) >= n8_2)
                        n7 -= n8_2;
                }
                else if ((std::numeric_limits<tools::Long>::max() - n7) >= n8_2)
                    n7 += n8_2;

                return n7 / n8;
            } // of else
        } // of else
    } // of else
}

// return (n1 * n2) / n3
static tools::Long fn3(const tools::Long n1, const tools::Long n2, const tools::Long n3)
{
    if (n1 == 0 || n2 == 0 || n3 == 0)
        return 0;
    if (std::numeric_limits<tools::Long>::max() / std::abs(n1) < std::abs(n2))
    {
        BigInt a4 = n1;
        a4 *= n2;

        if (a4.IsNeg())
            a4 -= n3 / 2;
        else
            a4 += n3 / 2;

        a4 /= n3;
        return static_cast<tools::Long>(a4);
    } // of if
    else
    {
        tools::Long n4 = n1 * n2;
        const tools::Long n3_2 = n3 / 2;

        if (n4 < 0)
        {
            if ((n4 - std::numeric_limits<tools::Long>::min()) >= n3_2)
                n4 -= n3_2;
        }
        else if ((std::numeric_limits<tools::Long>::max() - n4) >= n3_2)
            n4 += n3_2;

        return n4 / n3;
    } // of else
}

Point OutputDevice::LogicToLogic(const Point& rPtSource, const MapMode* pMapModeSource,
                                 const MapMode* pMapModeDest) const
{
    if (!pMapModeSource)
        pMapModeSource = &maMapMode;

    if (!pMapModeDest)
        pMapModeDest = &maMapMode;

    if (*pMapModeSource == *pMapModeDest)
        return rPtSource;

    MappingMetrics aMappingMetricSource;
    MappingMetrics aMappingMetricDest;

    if (!IsMapModeEnabled() || pMapModeSource != &maMapMode)
    {
        if (pMapModeSource->GetMapUnit() == MapUnit::MapRelative)
            aMappingMetricSource = maGeometry.GetMappingMetrics();

        aMappingMetricSource.Calculate(*pMapModeSource, GetDPIX(), GetDPIY());
    }
    else
    {
        aMappingMetricSource = maGeometry.GetMappingMetrics();
    }

    if (!IsMapModeEnabled() || pMapModeDest != &maMapMode)
    {
        if (pMapModeDest->GetMapUnit() == MapUnit::MapRelative)
            aMappingMetricDest = maGeometry.GetMappingMetrics();

        aMappingMetricDest.Calculate(*pMapModeDest, GetDPIX(), GetDPIY());
    }
    else
    {
        aMappingMetricDest = maGeometry.GetMappingMetrics();
    }

    return Point(fn5(rPtSource.X() + aMappingMetricSource.mnMapOfsX,
                     aMappingMetricSource.mnMapScNumX, aMappingMetricDest.mnMapScDenomX,
                     aMappingMetricSource.mnMapScDenomX, aMappingMetricDest.mnMapScNumX)
                     - aMappingMetricDest.mnMapOfsX,
                 fn5(rPtSource.Y() + aMappingMetricSource.mnMapOfsY,
                     aMappingMetricSource.mnMapScNumY, aMappingMetricDest.mnMapScDenomY,
                     aMappingMetricSource.mnMapScDenomY, aMappingMetricDest.mnMapScNumY)
                     - aMappingMetricDest.mnMapOfsY);
}

Size OutputDevice::LogicToLogic(const Size& rSzSource, const MapMode* pMapModeSource,
                                const MapMode* pMapModeDest) const
{
    if (!pMapModeSource)
        pMapModeSource = &maMapMode;

    if (!pMapModeDest)
        pMapModeDest = &maMapMode;

    if (*pMapModeSource == *pMapModeDest)
        return rSzSource;

    MappingMetrics aMappingMetricSource;
    MappingMetrics aMappingMetricDest;

    if (!IsMapModeEnabled() || pMapModeSource != &maMapMode)
    {
        if (pMapModeSource->GetMapUnit() == MapUnit::MapRelative)
            aMappingMetricSource = maGeometry.GetMappingMetrics();

        aMappingMetricSource.Calculate(*pMapModeSource, GetDPIX(), GetDPIY());
    }
    else
    {
        aMappingMetricSource = maGeometry.GetMappingMetrics();
    }

    if (!IsMapModeEnabled() || pMapModeDest != &maMapMode)
    {
        if (pMapModeDest->GetMapUnit() == MapUnit::MapRelative)
            aMappingMetricDest = maGeometry.GetMappingMetrics();

        aMappingMetricDest.Calculate(*pMapModeDest, GetDPIX(), GetDPIY());
    }
    else
    {
        aMappingMetricDest = maGeometry.GetMappingMetrics();
    }

    return Size(
        fn5(rSzSource.Width(), aMappingMetricSource.mnMapScNumX, aMappingMetricDest.mnMapScDenomX,
            aMappingMetricSource.mnMapScDenomX, aMappingMetricDest.mnMapScNumX),
        fn5(rSzSource.Height(), aMappingMetricSource.mnMapScNumY, aMappingMetricDest.mnMapScDenomY,
            aMappingMetricSource.mnMapScDenomY, aMappingMetricDest.mnMapScNumY));
}

tools::Rectangle OutputDevice::LogicToLogic(const tools::Rectangle& rRectSource,
                                            const MapMode* pMapModeSource,
                                            const MapMode* pMapModeDest) const
{
    if (!pMapModeSource)
        pMapModeSource = &maMapMode;

    if (!pMapModeDest)
        pMapModeDest = &maMapMode;

    if (*pMapModeSource == *pMapModeDest)
        return rRectSource;

    MappingMetrics aMappingMetricSource;
    MappingMetrics aMappingMetricDest;

    if (!IsMapModeEnabled() || pMapModeSource != &maMapMode)
    {
        if (pMapModeSource->GetMapUnit() == MapUnit::MapRelative)
            aMappingMetricSource = maGeometry.GetMappingMetrics();

        aMappingMetricSource.Calculate(*pMapModeSource, GetDPIX(), GetDPIY());
    }
    else
    {
        aMappingMetricSource = maGeometry.GetMappingMetrics();
    }

    if (!IsMapModeEnabled() || pMapModeDest != &maMapMode)
    {
        if (pMapModeDest->GetMapUnit() == MapUnit::MapRelative)
            aMappingMetricDest = maGeometry.GetMappingMetrics();

        aMappingMetricDest.Calculate(*pMapModeDest, GetDPIX(), GetDPIY());
    }
    else
    {
        aMappingMetricDest = maGeometry.GetMappingMetrics();
    }

    return tools::Rectangle(fn5(rRectSource.Left() + aMappingMetricSource.mnMapOfsX,
                                aMappingMetricSource.mnMapScNumX, aMappingMetricDest.mnMapScDenomX,
                                aMappingMetricSource.mnMapScDenomX, aMappingMetricDest.mnMapScNumX)
                                - aMappingMetricDest.mnMapOfsX,
                            fn5(rRectSource.Top() + aMappingMetricSource.mnMapOfsY,
                                aMappingMetricSource.mnMapScNumY, aMappingMetricDest.mnMapScDenomY,
                                aMappingMetricSource.mnMapScDenomY, aMappingMetricDest.mnMapScNumY)
                                - aMappingMetricDest.mnMapOfsY,
                            fn5(rRectSource.Right() + aMappingMetricSource.mnMapOfsX,
                                aMappingMetricSource.mnMapScNumX, aMappingMetricDest.mnMapScDenomX,
                                aMappingMetricSource.mnMapScDenomX, aMappingMetricDest.mnMapScNumX)
                                - aMappingMetricDest.mnMapOfsX,
                            fn5(rRectSource.Bottom() + aMappingMetricSource.mnMapOfsY,
                                aMappingMetricSource.mnMapScNumY, aMappingMetricDest.mnMapScDenomY,
                                aMappingMetricSource.mnMapScDenomY, aMappingMetricDest.mnMapScNumY)
                                - aMappingMetricDest.mnMapOfsY);
}

Point OutputDevice::LogicToLogic(const Point& rPtSource, const MapMode& rMapModeSource,
                                 const MapMode& rMapModeDest)
{
    if (rMapModeSource == rMapModeDest)
        return rPtSource;

    MapUnit eUnitSource = rMapModeSource.GetMapUnit();
    MapUnit eUnitDest = rMapModeDest.GetMapUnit();
    verifyUnitSourceDest(eUnitSource, eUnitDest);

    if (rMapModeSource.IsSimple() && rMapModeDest.IsSimple())
    {
        tools::Long nNumerator = 1;
        tools::Long nDenominator = 1;
        SAL_WARN_IF(eUnitSource > s_MaxValidUnit, "vcl.gdi", "Invalid source map unit");
        SAL_WARN_IF(eUnitDest > s_MaxValidUnit, "vcl.gdi", "Invalid destination map unit");

        if ((eUnitSource <= s_MaxValidUnit) && (eUnitDest <= s_MaxValidUnit))
        {
            nNumerator = aImplNumeratorAry[eUnitSource] * aImplDenominatorAry[eUnitDest];
            nDenominator = aImplNumeratorAry[eUnitDest] * aImplDenominatorAry[eUnitSource];
        }

        if (eUnitSource == MapUnit::MapPixel)
            nDenominator *= 72;
        else if (eUnitDest == MapUnit::MapPixel)
            nNumerator *= 72;

        return Point(fn3(rPtSource.X(), nNumerator, nDenominator),
                     fn3(rPtSource.Y(), nNumerator, nDenominator));
    }
    else
    {
        MappingMetrics aMappingMetricSource(rMapModeSource, 72, 72);
        MappingMetrics aMappingMetricDest(rMapModeDest, 72, 72);

        return Point(fn5(rPtSource.X() + aMappingMetricSource.mnMapOfsX,
                         aMappingMetricSource.mnMapScNumX, aMappingMetricDest.mnMapScDenomX,
                         aMappingMetricSource.mnMapScDenomX, aMappingMetricDest.mnMapScNumX)
                         - aMappingMetricDest.mnMapOfsX,
                     fn5(rPtSource.Y() + aMappingMetricSource.mnMapOfsY,
                         aMappingMetricSource.mnMapScNumY, aMappingMetricDest.mnMapScDenomY,
                         aMappingMetricSource.mnMapScDenomY, aMappingMetricDest.mnMapScNumY)
                         - aMappingMetricDest.mnMapOfsY);
    }
}

Size OutputDevice::LogicToLogic(const Size& rSzSource, const MapMode& rMapModeSource,
                                const MapMode& rMapModeDest)
{
    if (rMapModeSource == rMapModeDest)
        return rSzSource;

    MapUnit eUnitSource = rMapModeSource.GetMapUnit();
    MapUnit eUnitDest = rMapModeDest.GetMapUnit();
    verifyUnitSourceDest(eUnitSource, eUnitDest);

    if (rMapModeSource.IsSimple() && rMapModeDest.IsSimple())
    {
        tools::Long nNumerator = 1;
        tools::Long nDenominator = 1;
        SAL_WARN_IF(eUnitSource > s_MaxValidUnit, "vcl.gdi", "Invalid source map unit");
        SAL_WARN_IF(eUnitDest > s_MaxValidUnit, "vcl.gdi", "Invalid destination map unit");

        if ((eUnitSource <= s_MaxValidUnit) && (eUnitDest <= s_MaxValidUnit))
        {
            nNumerator = aImplNumeratorAry[eUnitSource] * aImplDenominatorAry[eUnitDest];
            nDenominator = aImplNumeratorAry[eUnitDest] * aImplDenominatorAry[eUnitSource];
        }

        if (eUnitSource == MapUnit::MapPixel)
            nDenominator *= 72;
        else if (eUnitDest == MapUnit::MapPixel)
            nNumerator *= 72;

        return Size(fn3(rSzSource.Width(), nNumerator, nDenominator),
                    fn3(rSzSource.Height(), nNumerator, nDenominator));
    }
    else
    {
        MappingMetrics aMappingMetricSource(rMapModeSource, 72, 72);
        MappingMetrics aMappingMetricDest(rMapModeDest, 72, 72);

        return Size(fn5(rSzSource.Width(), aMappingMetricSource.mnMapScNumX,
                        aMappingMetricDest.mnMapScDenomX, aMappingMetricSource.mnMapScDenomX,
                        aMappingMetricDest.mnMapScNumX),
                    fn5(rSzSource.Height(), aMappingMetricSource.mnMapScNumY,
                        aMappingMetricDest.mnMapScDenomY, aMappingMetricSource.mnMapScDenomY,
                        aMappingMetricDest.mnMapScNumY));
    }
}

basegfx::B2DPolygon OutputDevice::LogicToLogic(const basegfx::B2DPolygon& rPolySource,
                                               const MapMode& rMapModeSource,
                                               const MapMode& rMapModeDest)
{
    if (rMapModeSource == rMapModeDest)
    {
        return rPolySource;
    }

    const basegfx::B2DHomMatrix aTransform(LogicToLogic(rMapModeSource, rMapModeDest));
    basegfx::B2DPolygon aPoly(rPolySource);

    aPoly.transform(aTransform);
    return aPoly;
}

basegfx::B2DHomMatrix OutputDevice::LogicToLogic(const MapMode& rMapModeSource,
                                                 const MapMode& rMapModeDest)
{
    basegfx::B2DHomMatrix aTransform;

    if (rMapModeSource == rMapModeDest)
    {
        return aTransform;
    }

    MapUnit eUnitSource = rMapModeSource.GetMapUnit();
    MapUnit eUnitDest = rMapModeDest.GetMapUnit();
    verifyUnitSourceDest(eUnitSource, eUnitDest);

    if (rMapModeSource.IsSimple() && rMapModeDest.IsSimple())
    {
        tools::Long nNumerator = 1;
        tools::Long nDenominator = 1;
        SAL_WARN_IF(eUnitSource > s_MaxValidUnit, "vcl.gdi", "Invalid source map unit");
        SAL_WARN_IF(eUnitDest > s_MaxValidUnit, "vcl.gdi", "Invalid destination map unit");

        if ((eUnitSource <= s_MaxValidUnit) && (eUnitDest <= s_MaxValidUnit))
        {
            nNumerator = aImplNumeratorAry[eUnitSource] * aImplDenominatorAry[eUnitDest];
            nDenominator = aImplNumeratorAry[eUnitDest] * aImplDenominatorAry[eUnitSource];
        }

        if (eUnitSource == MapUnit::MapPixel)
            nDenominator *= 72;
        else if (eUnitDest == MapUnit::MapPixel)
            nNumerator *= 72;

        const double fScaleFactor(static_cast<double>(nNumerator)
                                  / static_cast<double>(nDenominator));
        aTransform.set(0, 0, fScaleFactor);
        aTransform.set(1, 1, fScaleFactor);
    }
    else
    {
        MappingMetrics aMappingMetricSource(rMapModeSource, 72, 72);
        MappingMetrics aMappingMetricDest(rMapModeDest, 72, 72);

        const double fScaleFactorX(
            (double(aMappingMetricSource.mnMapScNumX) * double(aMappingMetricDest.mnMapScDenomX))
            / (double(aMappingMetricSource.mnMapScDenomX)
               * double(aMappingMetricDest.mnMapScNumX)));
        const double fScaleFactorY(
            (double(aMappingMetricSource.mnMapScNumY) * double(aMappingMetricDest.mnMapScDenomY))
            / (double(aMappingMetricSource.mnMapScDenomY)
               * double(aMappingMetricDest.mnMapScNumY)));
        const double fZeroPointX(double(aMappingMetricSource.mnMapOfsX) * fScaleFactorX
                                 - double(aMappingMetricDest.mnMapOfsX));
        const double fZeroPointY(double(aMappingMetricSource.mnMapOfsY) * fScaleFactorY
                                 - double(aMappingMetricDest.mnMapOfsY));

        aTransform.set(0, 0, fScaleFactorX);
        aTransform.set(1, 1, fScaleFactorY);
        aTransform.set(0, 2, fZeroPointX);
        aTransform.set(1, 2, fZeroPointY);
    }

    return aTransform;
}

tools::Rectangle OutputDevice::LogicToLogic(const tools::Rectangle& rRectSource,
                                            const MapMode& rMapModeSource,
                                            const MapMode& rMapModeDest)
{
    if (rMapModeSource == rMapModeDest)
        return rRectSource;

    MapUnit eUnitSource = rMapModeSource.GetMapUnit();
    MapUnit eUnitDest = rMapModeDest.GetMapUnit();
    verifyUnitSourceDest(eUnitSource, eUnitDest);

    if (rMapModeSource.IsSimple() && rMapModeDest.IsSimple())
    {
        tools::Long nNumerator = 1;
        tools::Long nDenominator = 1;
        SAL_WARN_IF(eUnitSource > s_MaxValidUnit, "vcl.gdi", "Invalid source map unit");
        SAL_WARN_IF(eUnitDest > s_MaxValidUnit, "vcl.gdi", "Invalid destination map unit");

        if ((eUnitSource <= s_MaxValidUnit) && (eUnitDest <= s_MaxValidUnit))
        {
            nNumerator = aImplNumeratorAry[eUnitSource] * aImplDenominatorAry[eUnitDest];
            nDenominator = aImplNumeratorAry[eUnitDest] * aImplDenominatorAry[eUnitSource];
        }

        if (eUnitSource == MapUnit::MapPixel)
            nDenominator *= 72;
        else if (eUnitDest == MapUnit::MapPixel)
            nNumerator *= 72;

        auto left = fn3(rRectSource.Left(), nNumerator, nDenominator);
        auto top = fn3(rRectSource.Top(), nNumerator, nDenominator);
        if (rRectSource.IsEmpty())
            return tools::Rectangle(left, top);

        auto right = fn3(rRectSource.Right(), nNumerator, nDenominator);
        auto bottom = fn3(rRectSource.Bottom(), nNumerator, nDenominator);
        return tools::Rectangle(left, top, right, bottom);
    }
    else
    {
        MappingMetrics aMappingMetricSource(rMapModeSource, 72, 72);
        MappingMetrics aMappingMetricDest(rMapModeDest, 72, 72);

        auto left = fn5(rRectSource.Left() + aMappingMetricSource.mnMapOfsX,
                        aMappingMetricSource.mnMapScNumX, aMappingMetricDest.mnMapScDenomX,
                        aMappingMetricSource.mnMapScDenomX, aMappingMetricDest.mnMapScNumX)
                    - aMappingMetricDest.mnMapOfsX;
        auto top = fn5(rRectSource.Top() + aMappingMetricSource.mnMapOfsY,
                       aMappingMetricSource.mnMapScNumY, aMappingMetricDest.mnMapScDenomY,
                       aMappingMetricSource.mnMapScDenomY, aMappingMetricDest.mnMapScNumY)
                   - aMappingMetricDest.mnMapOfsY;
        if (rRectSource.IsEmpty())
            return tools::Rectangle(left, top);

        auto right = fn5(rRectSource.Right() + aMappingMetricSource.mnMapOfsX,
                         aMappingMetricSource.mnMapScNumX, aMappingMetricDest.mnMapScDenomX,
                         aMappingMetricSource.mnMapScDenomX, aMappingMetricDest.mnMapScNumX)
                     - aMappingMetricDest.mnMapOfsX;
        auto bottom = fn5(rRectSource.Bottom() + aMappingMetricSource.mnMapOfsY,
                          aMappingMetricSource.mnMapScNumY, aMappingMetricDest.mnMapScDenomY,
                          aMappingMetricSource.mnMapScDenomY, aMappingMetricDest.mnMapScNumY)
                      - aMappingMetricDest.mnMapOfsY;
        return tools::Rectangle(left, top, right, bottom);
    }
}

tools::Long OutputDevice::LogicToLogic(tools::Long nLongSource, MapUnit eUnitSource,
                                       MapUnit eUnitDest)
{
    if (eUnitSource == eUnitDest)
        return nLongSource;

    verifyUnitSourceDest(eUnitSource, eUnitDest);

    tools::Long nNumerator = 1;
    tools::Long nDenominator = 1;
    SAL_WARN_IF(eUnitSource > s_MaxValidUnit, "vcl.gdi", "Invalid source map unit");
    SAL_WARN_IF(eUnitDest > s_MaxValidUnit, "vcl.gdi", "Invalid destination map unit");

    if ((eUnitSource <= s_MaxValidUnit) && (eUnitDest <= s_MaxValidUnit))
    {
        nNumerator = aImplNumeratorAry[eUnitSource] * aImplDenominatorAry[eUnitDest];
        nDenominator = aImplNumeratorAry[eUnitDest] * aImplDenominatorAry[eUnitSource];
    }

    if (eUnitSource == MapUnit::MapPixel)
        nDenominator *= 72;
    else if (eUnitDest == MapUnit::MapPixel)
        nNumerator *= 72;

    return fn3(nLongSource, nNumerator, nDenominator);
}

void OutputDevice::SetOffsetFromOriginInPixels(Size const& rOffset)
{
    RenderContext2::SetOffsetFromOriginInPixels(rOffset);

    if (mpAlphaVDev)
        mpAlphaVDev->SetOffsetFromOriginInPixels(rOffset);
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
