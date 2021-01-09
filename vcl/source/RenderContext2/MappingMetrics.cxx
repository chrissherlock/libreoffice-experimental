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

#include <osl/diagnose.h>
#include <tools/bigint.hxx>
#include <tools/debug.hxx>
#include <tools/fract.hxx>

#include <vcl/MappingMetrics.hxx>
#include <vcl/wrkwin.hxx>

#include <svdata.hxx>

MappingMetrics::MappingMetrics()
    : mnMapOfsX(0)
    , mnMapOfsY(0)
    , mnMapScNumX(1)
    , mnMapScNumY(1)
    , mnMapScDenomX(1)
    , mnMapScDenomY(1)
{
}

MappingMetrics::MappingMetrics(MapMode const& rMapMode, tools::Long nDPIX, tools::Long nDPIY)
{
    Calculate(rMapMode, nDPIX, nDPIY);
}

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

void MappingMetrics::Calculate(MapMode const& rMapMode, tools::Long nDPIX, tools::Long nDPIY)
{
    MappingMetrics aMappingMetrics;

    switch (rMapMode.GetMapUnit())
    {
        case MapUnit::MapRelative:
            break;
        case MapUnit::Map100thMM:
            mnMapScNumX = 1;
            mnMapScDenomX = 2540;
            mnMapScNumY = 1;
            mnMapScDenomY = 2540;
            break;
        case MapUnit::Map10thMM:
            mnMapScNumX = 1;
            mnMapScDenomX = 254;
            mnMapScNumY = 1;
            mnMapScDenomY = 254;
            break;
        case MapUnit::MapMM:
            mnMapScNumX = 5; // 10
            mnMapScDenomX = 127; // 254
            mnMapScNumY = 5; // 10
            mnMapScDenomY = 127; // 254
            break;
        case MapUnit::MapCM:
            mnMapScNumX = 50; // 100
            mnMapScDenomX = 127; // 254
            mnMapScNumY = 50; // 100
            mnMapScDenomY = 127; // 254
            break;
        case MapUnit::Map1000thInch:
            mnMapScNumX = 1;
            mnMapScDenomX = 1000;
            mnMapScNumY = 1;
            mnMapScDenomY = 1000;
            break;
        case MapUnit::Map100thInch:
            mnMapScNumX = 1;
            mnMapScDenomX = 100;
            mnMapScNumY = 1;
            mnMapScDenomY = 100;
            break;
        case MapUnit::Map10thInch:
            mnMapScNumX = 1;
            mnMapScDenomX = 10;
            mnMapScNumY = 1;
            mnMapScDenomY = 10;
            break;
        case MapUnit::MapInch:
            mnMapScNumX = 1;
            mnMapScDenomX = 1;
            mnMapScNumY = 1;
            mnMapScDenomY = 1;
            break;
        case MapUnit::MapPoint:
            mnMapScNumX = 1;
            mnMapScDenomX = 72;
            mnMapScNumY = 1;
            mnMapScDenomY = 72;
            break;
        case MapUnit::MapTwip:
            mnMapScNumX = 1;
            mnMapScDenomX = 1440;
            mnMapScNumY = 1;
            mnMapScDenomY = 1440;
            break;
        case MapUnit::MapPixel:
            mnMapScNumX = 1;
            mnMapScDenomX = nDPIX;
            mnMapScNumY = 1;
            mnMapScDenomY = nDPIY;
            break;
        case MapUnit::MapSysFont:
        case MapUnit::MapAppFont:
        {
            ImplSVData* pSVData = ImplGetSVData();
            if (!pSVData->maGDIData.mnAppFontX)
            {
                if (pSVData->maFrameData.mpFirstFrame)
                    vcl::Window::ImplInitAppFontData(pSVData->maFrameData.mpFirstFrame);
                else
                {
                    ScopedVclPtrInstance<WorkWindow> pWin(nullptr, 0);
                    vcl::Window::ImplInitAppFontData(pWin);
                }
            }
            mnMapScNumX = pSVData->maGDIData.mnAppFontX;
            mnMapScDenomX = nDPIX * 40;
            mnMapScNumY = pSVData->maGDIData.mnAppFontY;
            mnMapScDenomY = nDPIY * 80;
        }
        break;
        default:
            OSL_FAIL("unhandled MapUnit");
            break;
    }

    const Fraction& aScaleX = rMapMode.GetScaleX();
    const Fraction& aScaleY = rMapMode.GetScaleY();

    // set offset according to MapMode
    Point aOrigin = rMapMode.GetOrigin();
    if (rMapMode.GetMapUnit() != MapUnit::MapRelative)
    {
        mnMapOfsX = aOrigin.X();
        mnMapOfsY = aOrigin.Y();
    }
    else
    {
        auto nXNumerator = aScaleX.GetNumerator();
        auto nYNumerator = aScaleY.GetNumerator();
        assert(nXNumerator != 0 && nYNumerator != 0);

        BigInt aX(mnMapOfsX);
        aX *= BigInt(aScaleX.GetDenominator());
        if (mnMapOfsX >= 0)
        {
            if (nXNumerator >= 0)
                aX += BigInt(nXNumerator / 2);
            else
                aX -= BigInt((nXNumerator + 1) / 2);
        }
        else
        {
            if (nXNumerator >= 0)
                aX -= BigInt((nXNumerator - 1) / 2);
            else
                aX += BigInt(nXNumerator / 2);
        }
        aX /= BigInt(nXNumerator);
        mnMapOfsX = static_cast<tools::Long>(aX) + aOrigin.X();
        BigInt aY(mnMapOfsY);
        aY *= BigInt(aScaleY.GetDenominator());
        if (mnMapOfsY >= 0)
        {
            if (nYNumerator >= 0)
                aY += BigInt(nYNumerator / 2);
            else
                aY -= BigInt((nYNumerator + 1) / 2);
        }
        else
        {
            if (nYNumerator >= 0)
                aY -= BigInt((nYNumerator - 1) / 2);
            else
                aY += BigInt(nYNumerator / 2);
        }
        aY /= BigInt(nYNumerator);
        mnMapOfsY = static_cast<tools::Long>(aY) + aOrigin.Y();
    }

    // calculate scaling factor according to MapMode
    // aTemp? = mnMapSc? * aScale?
    Fraction aTempX = ImplMakeFraction(mnMapScNumX, aScaleX.GetNumerator(), mnMapScDenomX,
                                       aScaleX.GetDenominator());
    Fraction aTempY = ImplMakeFraction(mnMapScNumY, aScaleY.GetNumerator(), mnMapScDenomY,
                                       aScaleY.GetDenominator());
    mnMapScNumX = aTempX.GetNumerator();
    mnMapScDenomX = aTempX.GetDenominator();
    mnMapScNumY = aTempY.GetNumerator();
    mnMapScDenomY = aTempY.GetDenominator();
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
