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

const o3tl::enumarray<MapUnit, tools::Long> aImplNumeratorAry
    = { 1, 1, 5, 50, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0 };
const o3tl::enumarray<MapUnit, tools::Long> aImplDenominatorAry
    = { 2540, 254, 127, 127, 1000, 100, 10, 1, 72, 1440, 1, 0, 0, 0 };

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

    Point aPt(rNewMapMode.MapTo(GetMapMode(), Point(), GetGeometry()));
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

void OutputDevice::SetOffsetFromOriginInPixels(Size const& rOffset)
{
    RenderContext2::SetOffsetFromOriginInPixels(rOffset);

    if (mpAlphaVDev)
        mpAlphaVDev->SetOffsetFromOriginInPixels(rOffset);
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
