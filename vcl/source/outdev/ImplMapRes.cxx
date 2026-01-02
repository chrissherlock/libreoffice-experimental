
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

#include <osl/diagnose.h>
#include <tools/bigint.hxx>
#include <tools/fract.hxx>
#include <tools/long.hxx>
#include <tools/mapunit.hxx>

#include <vcl/rendercontext/ImplMapRes.hxx>
#include <vcl/mapmod.hxx>
#include <vcl/vclptr.hxx>
#include <vcl/virdev.hxx>
#include <vcl/wrkwin.hxx>

#include <svdata.hxx>

void ImplMapRes::SetMapRes(const o3tl::Length eUnit)
{
    const auto[nNum, nDen] = o3tl::getConversionMulDiv(eUnit, o3tl::Length::in);

    mnMapScNumX = nNum;
    mnMapScNumY = nNum;
    mnMapScDenomX = nDen;
    mnMapScDenomY = nDen;
};

void ImplMapRes::CalcMapResolution(const MapMode& rMapMode, tools::Long nDPIX, tools::Long nDPIY)
{
    switch (rMapMode.GetMapUnit())
    {
        case MapUnit::MapRelative:
            break;
        case MapUnit::Map100thMM:
            SetMapRes(o3tl::Length::mm100);
            break;
        case MapUnit::Map10thMM:
            SetMapRes(o3tl::Length::mm10);
            break;
        case MapUnit::MapMM:
            SetMapRes(o3tl::Length::mm);
            break;
        case MapUnit::MapCM:
            SetMapRes(o3tl::Length::cm);
            break;
        case MapUnit::Map1000thInch:
            SetMapRes(o3tl::Length::in1000);
            break;
        case MapUnit::Map100thInch:
            SetMapRes(o3tl::Length::in100);
            break;
        case MapUnit::Map10thInch:
            SetMapRes(o3tl::Length::in10);
            break;
        case MapUnit::MapInch:
            SetMapRes(o3tl::Length::in);
            break;
        case MapUnit::MapPoint:
            SetMapRes(o3tl::Length::pt);
            break;
        case MapUnit::MapTwip:
            SetMapRes(o3tl::Length::twip);
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
        auto funcCalcOffset
            = [](const Fraction& rScale, tools::Long& rnMapOffset, tools::Long nOrigin) {
                  auto nNumerator = rScale.GetNumerator();
                  assert(nNumerator != 0);

                  BigInt aX(rnMapOffset);
                  aX *= BigInt(rScale.GetDenominator());
                  if (rnMapOffset >= 0)
                  {
                      if (nNumerator >= 0)
                          aX += BigInt(nNumerator / 2);
                      else
                          aX -= BigInt((nNumerator + 1) / 2);
                  }
                  else
                  {
                      if (nNumerator >= 0)
                          aX -= BigInt((nNumerator - 1) / 2);
                      else
                          aX += BigInt(nNumerator / 2);
                  }
                  aX /= BigInt(nNumerator);
                  rnMapOffset = static_cast<tools::Long>(aX) + nOrigin;
              };

        funcCalcOffset(aScaleX, mnMapOfsX, aOrigin.X());
        funcCalcOffset(aScaleY, mnMapOfsY, aOrigin.Y());
    }

    // calculate scaling factor according to MapMode
    // aTemp? = rMapRes.mnMapSc? * aScale?
    Fraction aTempX = Fraction::MakeFraction(mnMapScNumX, aScaleX.GetNumerator(), mnMapScDenomX,
                                             aScaleX.GetDenominator());
    Fraction aTempY = Fraction::MakeFraction(mnMapScNumY, aScaleY.GetNumerator(), mnMapScDenomY,
                                             aScaleY.GetDenominator());
    mnMapScNumX = aTempX.GetNumerator();
    mnMapScDenomX = aTempX.GetDenominator();
    mnMapScNumY = aTempY.GetNumerator();
    mnMapScDenomY = aTempY.GetDenominator();
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
