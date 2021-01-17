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
#include <vcl/Geometry.hxx>

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

    Point aPt(LogicToLogic(Point(), nullptr, &rNewMapMode, 1));
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

static void verifyUnitSourceDest(MapUnit eUnitSource, MapUnit eUnitDest)
{
    DBG_ASSERT(eUnitSource != MapUnit::MapSysFont && eUnitSource != MapUnit::MapAppFont
                   && eUnitSource != MapUnit::MapRelative,
               "Source MapUnit is not permitted");
    DBG_ASSERT(eUnitDest != MapUnit::MapSysFont && eUnitDest != MapUnit::MapAppFont
                   && eUnitDest != MapUnit::MapRelative,
               "Destination MapUnit is not permitted");
}

Point OutputDevice::LogicToLogic(const Point& rPtSource, const MapMode* pMapModeSource,
                                 const MapMode* pMapModeDest, int) const
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

    return Point(Geometry::fn5(rPtSource.X() + aMappingMetricSource.mnMapOfsX,
                     aMappingMetricSource.mnMapScNumX, aMappingMetricDest.mnMapScDenomX,
                     aMappingMetricSource.mnMapScDenomX, aMappingMetricDest.mnMapScNumX)
                     - aMappingMetricDest.mnMapOfsX,
                 Geometry::fn5(rPtSource.Y() + aMappingMetricSource.mnMapOfsY,
                     aMappingMetricSource.mnMapScNumY, aMappingMetricDest.mnMapScDenomY,
                     aMappingMetricSource.mnMapScDenomY, aMappingMetricDest.mnMapScNumY)
                     - aMappingMetricDest.mnMapOfsY);
}

Size OutputDevice::LogicToLogic(const Size& rSzSource, const MapMode* pMapModeSource,
                                const MapMode* pMapModeDest, int) const
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
        Geometry::fn5(rSzSource.Width(), aMappingMetricSource.mnMapScNumX, aMappingMetricDest.mnMapScDenomX,
            aMappingMetricSource.mnMapScDenomX, aMappingMetricDest.mnMapScNumX),
        Geometry::fn5(rSzSource.Height(), aMappingMetricSource.mnMapScNumY, aMappingMetricDest.mnMapScDenomY,
            aMappingMetricSource.mnMapScDenomY, aMappingMetricDest.mnMapScNumY));
}

tools::Rectangle OutputDevice::LogicToLogic(const tools::Rectangle& rRectSource,
                                            const MapMode* pMapModeSource,
                                            const MapMode* pMapModeDest, int) const
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

    return tools::Rectangle(Geometry::fn5(rRectSource.Left() + aMappingMetricSource.mnMapOfsX,
                                aMappingMetricSource.mnMapScNumX, aMappingMetricDest.mnMapScDenomX,
                                aMappingMetricSource.mnMapScDenomX, aMappingMetricDest.mnMapScNumX)
                                - aMappingMetricDest.mnMapOfsX,
                            Geometry::fn5(rRectSource.Top() + aMappingMetricSource.mnMapOfsY,
                                aMappingMetricSource.mnMapScNumY, aMappingMetricDest.mnMapScDenomY,
                                aMappingMetricSource.mnMapScDenomY, aMappingMetricDest.mnMapScNumY)
                                - aMappingMetricDest.mnMapOfsY,
                            Geometry::fn5(rRectSource.Right() + aMappingMetricSource.mnMapOfsX,
                                aMappingMetricSource.mnMapScNumX, aMappingMetricDest.mnMapScDenomX,
                                aMappingMetricSource.mnMapScDenomX, aMappingMetricDest.mnMapScNumX)
                                - aMappingMetricDest.mnMapOfsX,
                            Geometry::fn5(rRectSource.Bottom() + aMappingMetricSource.mnMapOfsY,
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

        return Point(Geometry::fn3(rPtSource.X(), nNumerator, nDenominator),
                     Geometry::fn3(rPtSource.Y(), nNumerator, nDenominator));
    }
    else
    {
        MappingMetrics aMappingMetricSource(rMapModeSource, 72, 72);
        MappingMetrics aMappingMetricDest(rMapModeDest, 72, 72);

        return Point(Geometry::fn5(rPtSource.X() + aMappingMetricSource.mnMapOfsX,
                         aMappingMetricSource.mnMapScNumX, aMappingMetricDest.mnMapScDenomX,
                         aMappingMetricSource.mnMapScDenomX, aMappingMetricDest.mnMapScNumX)
                         - aMappingMetricDest.mnMapOfsX,
                     Geometry::fn5(rPtSource.Y() + aMappingMetricSource.mnMapOfsY,
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

        return Size(Geometry::fn3(rSzSource.Width(), nNumerator, nDenominator),
                    Geometry::fn3(rSzSource.Height(), nNumerator, nDenominator));
    }
    else
    {
        MappingMetrics aMappingMetricSource(rMapModeSource, 72, 72);
        MappingMetrics aMappingMetricDest(rMapModeDest, 72, 72);

        return Size(Geometry::fn5(rSzSource.Width(), aMappingMetricSource.mnMapScNumX,
                        aMappingMetricDest.mnMapScDenomX, aMappingMetricSource.mnMapScDenomX,
                        aMappingMetricDest.mnMapScNumX),
                    Geometry::fn5(rSzSource.Height(), aMappingMetricSource.mnMapScNumY,
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

        auto left = Geometry::fn3(rRectSource.Left(), nNumerator, nDenominator);
        auto top = Geometry::fn3(rRectSource.Top(), nNumerator, nDenominator);
        if (rRectSource.IsEmpty())
            return tools::Rectangle(left, top);

        auto right = Geometry::fn3(rRectSource.Right(), nNumerator, nDenominator);
        auto bottom = Geometry::fn3(rRectSource.Bottom(), nNumerator, nDenominator);
        return tools::Rectangle(left, top, right, bottom);
    }
    else
    {
        MappingMetrics aMappingMetricSource(rMapModeSource, 72, 72);
        MappingMetrics aMappingMetricDest(rMapModeDest, 72, 72);

        auto left = Geometry::fn5(rRectSource.Left() + aMappingMetricSource.mnMapOfsX,
                        aMappingMetricSource.mnMapScNumX, aMappingMetricDest.mnMapScDenomX,
                        aMappingMetricSource.mnMapScDenomX, aMappingMetricDest.mnMapScNumX)
                    - aMappingMetricDest.mnMapOfsX;
        auto top = Geometry::fn5(rRectSource.Top() + aMappingMetricSource.mnMapOfsY,
                       aMappingMetricSource.mnMapScNumY, aMappingMetricDest.mnMapScDenomY,
                       aMappingMetricSource.mnMapScDenomY, aMappingMetricDest.mnMapScNumY)
                   - aMappingMetricDest.mnMapOfsY;
        if (rRectSource.IsEmpty())
            return tools::Rectangle(left, top);

        auto right = Geometry::fn5(rRectSource.Right() + aMappingMetricSource.mnMapOfsX,
                         aMappingMetricSource.mnMapScNumX, aMappingMetricDest.mnMapScDenomX,
                         aMappingMetricSource.mnMapScDenomX, aMappingMetricDest.mnMapScNumX)
                     - aMappingMetricDest.mnMapOfsX;
        auto bottom = Geometry::fn5(rRectSource.Bottom() + aMappingMetricSource.mnMapOfsY,
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

    return Geometry::fn3(nLongSource, nNumerator, nDenominator);
}

void OutputDevice::SetOffsetFromOriginInPixels(Size const& rOffset)
{
    RenderContext2::SetOffsetFromOriginInPixels(rOffset);

    if (mpAlphaVDev)
        mpAlphaVDev->SetOffsetFromOriginInPixels(rOffset);
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
