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

#include <tools/fract.hxx>

#include <vcl/RenderContext2.hxx>
#include <vcl/settings.hxx>
#include <vcl/virdev.hxx>
#include <vcl/svapp.hxx>

#include <outdev.h>
#include <PhysicalFontCollection.hxx>
#include <drawmode.hxx>
#include <salgdi.hxx>
#include <svdata.hxx>

#include <strings.hrc>

vcl::Font const& RenderContext2::GetFont() const { return maFont; }

void RenderContext2::SetFont(vcl::Font const& rNewFont)
{
    vcl::Font aFont = GetDrawModeFont(rNewFont, GetDrawMode(), GetSettings().GetStyleSettings());

    // Optimization MT/HDU: COL_TRANSPARENT means SetFont should ignore the font color,
    // because SetTextColor() is used for this.
    // #i28759# maTextColor might have been changed behind our back, commit then, too.
    if (aFont.GetColor() != COL_TRANSPARENT
        && (aFont.GetColor() != maFont.GetColor() || aFont.GetColor() != maTextColor))
    {
        maTextColor = aFont.GetColor();
        mbInitTextColor = true;
    }

    if (maFont.IsSameInstance(aFont))
        return;

    maFont = aFont;
    mbNewFont = true;

    if (!mpAlphaVDev)
        return;

    // #i30463#
    // Since SetFont might change the text color, apply that only
    // selectively to alpha vdev (which normally paints opaque text
    // with COL_BLACK)
    if (aFont.GetColor() != COL_TRANSPARENT)
    {
        mpAlphaVDev->SetTextColor(COL_BLACK);
        aFont.SetColor(COL_TRANSPARENT);
    }

    mpAlphaVDev->SetFont(aFont);
}

bool RenderContext2::IsFontAvailable(OUString const& rFontName) const
{
    ImplInitFontList();
    PhysicalFontFamily* pFound = mxFontCollection->FindFontFamily(rFontName);
    return (pFound != nullptr);
}

void RenderContext2::ImplInitFontList() const
{
    if (mxFontCollection->Count())
        return;

    if (!(mpGraphics || AcquireGraphics()))
        return;

    SAL_INFO("vcl.gdi", "RenderContext2::ImplInitFontList()");
    mpGraphics->GetDevFontList(mxFontCollection.get());

    // There is absolutely no way there should be no fonts available on the device
    if (!mxFontCollection->Count())
    {
        OUString aError("Application error: no fonts and no vcl resource found on your system");
        OUString aResStr(VclResId(SV_ACCESSERROR_NO_FONTS));
        if (!aResStr.isEmpty())
            aError = aResStr;
        Application::Abort(aError);
    }
}

FontMetric RenderContext2::GetDevFont(int nDevFontIndex) const
{
    FontMetric aFontMetric;

    ImplInitFontList();

    int nCount = GetDevFontCount();
    if (nDevFontIndex < nCount)
    {
        const PhysicalFontFace& rData = *mpDeviceFontList->Get(nDevFontIndex);
        aFontMetric.SetFamilyName(rData.GetFamilyName());
        aFontMetric.SetStyleName(rData.GetStyleName());
        aFontMetric.SetCharSet(rData.GetCharSet());
        aFontMetric.SetFamily(rData.GetFamilyType());
        aFontMetric.SetPitch(rData.GetPitch());
        aFontMetric.SetWeight(rData.GetWeight());
        aFontMetric.SetItalic(rData.GetItalic());
        aFontMetric.SetAlignment(TextAlign::ALIGN_TOP);
        aFontMetric.SetWidthType(rData.GetWidthType());
        aFontMetric.SetQuality(rData.GetQuality());
    }

    return aFontMetric;
}

int RenderContext2::GetDevFontCount() const
{
    if (!mpDeviceFontList)
    {
        if (!mxFontCollection)
        {
            return 0;
        }

        mpDeviceFontList = mxFontCollection->GetDeviceFontList();

        if (!mpDeviceFontList->Count())
        {
            mpDeviceFontList.reset();
            return 0;
        }
    }
    return mpDeviceFontList->Count();
}

int RenderContext2::GetDevFontSizeCount(vcl::Font const& rFont) const
{
    mpDeviceFontSizeList.reset();

    ImplInitFontList();
    mpDeviceFontSizeList = mxFontCollection->GetDeviceFontSizeList(rFont.GetFamilyName());
    return mpDeviceFontSizeList->Count();
}

Size RenderContext2::GetDevFontSize(vcl::Font const& rFont, int nSizeIndex) const
{
    // check range
    int nCount = GetDevFontSizeCount(rFont);
    if (nSizeIndex >= nCount)
        return Size();

    // when mapping is enabled round to .5 points
    Size aSize(0, mpDeviceFontSizeList->Get(nSizeIndex));
    if (mbMap)
    {
        aSize.setHeight(aSize.Height() * 10);
        MapMode aMap(MapUnit::Map10thInch, Point(), Fraction(1, 72), Fraction(1, 72));
        aSize = PixelToLogic(aSize, aMap);
        aSize.AdjustHeight(5);
        aSize.setHeight(aSize.Height() / 10);
        tools::Long nRound = aSize.Height() % 5;

        if (nRound >= 3)
            aSize.AdjustHeight(5 - nRound);
        else
            aSize.AdjustHeight(-nRound);

        aSize.setHeight(aSize.Height() * 10);
        aSize = LogicToPixel(aSize, aMap);
        aSize = PixelToLogic(aSize);
        aSize.AdjustHeight(5);
        aSize.setHeight(aSize.Height() / 10);
    }
    return aSize;
}

bool RenderContext2::AddTempDevFont(OUString const& rFileURL, OUString const& rFontName)
{
    ImplInitFontList();

    if (!mpGraphics && !AcquireGraphics())
        return false;

    bool bRC = mpGraphics->AddTempDevFont(mxFontCollection.get(), rFileURL, rFontName);
    if (!bRC)
        return false;

    if (mpAlphaVDev)
        mpAlphaVDev->AddTempDevFont(rFileURL, rFontName);

    return true;
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
