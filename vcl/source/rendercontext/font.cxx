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

#include <rtl/ustrbuf.hxx>
#include <tools/debug.hxx>
#include <tools/fract.hxx>
#include <unotools/configmgr.hxx>

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

bool RenderContext2::InitFont() const
{
    DBG_TESTSOLARMUTEX();

    if (!ImplNewFont())
        return false;

    if (!mpFontInstance)
        return false;

    if (!mpGraphics)
    {
        if (!AcquireGraphics())
            return false;
    }
    else if (!mbInitFont)
    {
        return true;
    }

    assert(mpGraphics);

    mpGraphics->SetFont(mpFontInstance.get(), 0);
    mbInitFont = false;
    return true;
}

void RenderContext2::SetFontOrientation(LogicalFontInstance* const pFontInstance) const
{
    if (pFontInstance->GetFontSelectPattern().mnOrientation
        && !pFontInstance->mxFontMetric->GetOrientation())
    {
        pFontInstance->mnOwnOrientation = pFontInstance->GetFontSelectPattern().mnOrientation;
        pFontInstance->mnOrientation = pFontInstance->mnOwnOrientation;
    }
    else
    {
        pFontInstance->mnOrientation = pFontInstance->mxFontMetric->GetOrientation();
    }
}

bool RenderContext2::ImplNewFont() const
{
    DBG_TESTSOLARMUTEX();

    if (!mbNewFont)
        return true;

    // we need a graphics
    if (!mpGraphics && !AcquireGraphics())
    {
        SAL_WARN("vcl.gdi", "RenderContext2::ImplNewFont(): no Graphics, no Font");
        return false;
    }

    assert(mpGraphics);

    ImplInitFontList();

    // convert to pixel height
    // TODO: replace integer based aSize completely with subpixel accurate type
    float fExactHeight
        = ImplFloatLogicHeightToDevicePixel(static_cast<float>(maFont.GetFontHeight()));
    Size aSize = ImplLogicToDevicePixel(maFont.GetFontSize());
    if (!aSize.Height())
    {
        // use default pixel height only when logical height is zero
        if (maFont.GetFontSize().Height())
            aSize.setHeight(1);
        else
            aSize.setHeight((12 * GetDPIY()) / 72);

        fExactHeight = static_cast<float>(aSize.Height());
    }

    // select the default width only when logical width is zero
    if ((aSize.Width() == 0) && (maFont.GetFontSize().Width() != 0))
        aSize.setWidth(1);

    // decide if antialiasing is appropriate
    bool bNonAntialiased(GetAntialiasing() & AntialiasingFlags::DisableText);
    if (!utl::ConfigManager::IsFuzzing())
    {
        const StyleSettings& rStyleSettings = GetSettings().GetStyleSettings();
        bNonAntialiased |= bool(rStyleSettings.GetDisplayOptions() & DisplayOptions::AADisable);
        bNonAntialiased |= (int(rStyleSettings.GetAntialiasingMinPixelHeight())
                            > maFont.GetFontSize().Height());
    }

    // get font entry
    rtl::Reference<LogicalFontInstance> pOldFontInstance = mpFontInstance;
    mpFontInstance = mxFontCache->GetFontInstance(mxFontCollection.get(), maFont, aSize,
                                                  fExactHeight, bNonAntialiased);
    const bool bNewFontInstance = pOldFontInstance.get() != mpFontInstance.get();
    pOldFontInstance.clear();

    LogicalFontInstance* pFontInstance = mpFontInstance.get();

    if (!pFontInstance)
    {
        SAL_WARN("vcl.gdi", "RenderContext2::ImplNewFont(): no LogicalFontInstance, no Font");
        return false;
    }

    // mark when lower layers need to get involved
    mbNewFont = false;
    if (bNewFontInstance)
        mbInitFont = true;

    // select font when it has not been initialized yet
    if (!pFontInstance->mbInit && InitFont())
    {
        // get metric data from device layers
        pFontInstance->mbInit = true;

        pFontInstance->mxFontMetric->SetOrientation(
            mpFontInstance->GetFontSelectPattern().mnOrientation);
        mpGraphics->GetFontMetric(pFontInstance->mxFontMetric, 0);

        pFontInstance->mxFontMetric->ImplInitTextLineSize(this);
        pFontInstance->mxFontMetric->ImplInitAboveTextLineSize();
        pFontInstance->mxFontMetric->ImplInitFlags(this);

        pFontInstance->mnLineHeight
            = pFontInstance->mxFontMetric->GetAscent() + pFontInstance->mxFontMetric->GetDescent();

        SetFontOrientation(pFontInstance);
    }

    // calculate EmphasisArea
    mnEmphasisAscent = 0;
    mnEmphasisDescent = 0;
    if (maFont.GetEmphasisMark() & FontEmphasisMark::Style)
    {
        FontEmphasisMark nEmphasisMark = ImplGetEmphasisMarkStyle(maFont);
        tools::Long nEmphasisHeight = (pFontInstance->mnLineHeight * 250) / 1000;

        if (nEmphasisHeight < 1)
            nEmphasisHeight = 1;

        if (nEmphasisMark & FontEmphasisMark::PosBelow)
            mnEmphasisDescent = nEmphasisHeight;
        else
            mnEmphasisAscent = nEmphasisHeight;
    }

    // calculate text offset depending on TextAlignment
    TextAlign eAlign = maFont.GetAlignment();
    if (eAlign == ALIGN_BASELINE)
    {
        mnTextOffX = 0;
        mnTextOffY = 0;
    }
    else if (eAlign == ALIGN_TOP)
    {
        mnTextOffX = 0;
        mnTextOffY = +pFontInstance->mxFontMetric->GetAscent() + mnEmphasisAscent;

        if (pFontInstance->mnOrientation)
        {
            Point aOriginPt(0, 0);
            aOriginPt.RotateAround(mnTextOffX, mnTextOffY, pFontInstance->mnOrientation);
        }
    }
    else // eAlign == ALIGN_BOTTOM
    {
        mnTextOffX = 0;
        mnTextOffY = -pFontInstance->mxFontMetric->GetDescent() + mnEmphasisDescent;

        if (pFontInstance->mnOrientation)
        {
            Point aOriginPt(0, 0);
            aOriginPt.RotateAround(mnTextOffX, mnTextOffY, pFontInstance->mnOrientation);
        }
    }

    mbTextLines = ((maFont.GetUnderline() != LINESTYLE_NONE)
                   && (maFont.GetUnderline() != LINESTYLE_DONTKNOW))
                  || ((maFont.GetOverline() != LINESTYLE_NONE)
                      && (maFont.GetOverline() != LINESTYLE_DONTKNOW))
                  || ((maFont.GetStrikeout() != STRIKEOUT_NONE)
                      && (maFont.GetStrikeout() != STRIKEOUT_DONTKNOW));
    mbTextSpecial
        = maFont.IsShadow() || maFont.IsOutline() || (maFont.GetRelief() != FontRelief::NONE);

    bool bRet = true;

    // #95414# fix for OLE objects which use scale factors very creatively
    if (mbMap && !aSize.Width())
    {
        int nOrigWidth = pFontInstance->mxFontMetric->GetWidth();
        float fStretch = static_cast<float>(maMapRes.mnMapScNumX) * maMapRes.mnMapScDenomY;
        fStretch /= static_cast<float>(maMapRes.mnMapScNumY) * maMapRes.mnMapScDenomX;
        int nNewWidth = static_cast<int>(nOrigWidth * fStretch + 0.5);
        if ((nNewWidth != nOrigWidth) && (nNewWidth != 0))
        {
            Size aOrigSize = maFont.GetFontSize();
            const_cast<vcl::Font&>(maFont).SetFontSize(Size(nNewWidth, aSize.Height()));
            mbMap = false;
            mbNewFont = true;
            bRet = ImplNewFont(); // recurse once using stretched width
            mbMap = true;
            const_cast<vcl::Font&>(maFont).SetFontSize(aOrigSize);
        }
    }

    return bRet;
}

void RenderContext2::ImplInitFontList() const
{
    if (mxFontCollection->Count())
        return;

    if (!(mpGraphics || AcquireGraphics()))
        return;

    assert(mpGraphics);

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

    assert(mpGraphics);

    return true;
}

std::unique_ptr<SalLayout>
RenderContext2::ImplGlyphFallbackLayout(std::unique_ptr<SalLayout> pSalLayout,
                                        ImplLayoutArgs& rLayoutArgs,
                                        const SalLayoutGlyphs* pGlyphs) const
{
    // This function relies on a valid mpFontInstance, if it doesn't exist bail out
    // - we'd have crashed later on anyway. At least here we can catch the error in debug
    // mode.
    if (!mpFontInstance)
    {
        SAL_WARN("vcl.gdi", "No font entry set in RenderContext2");
        assert(mpFontInstance);
        return nullptr;
    }

    // prepare multi level glyph fallback
    std::unique_ptr<MultiSalLayout> pMultiSalLayout;
    ImplLayoutRuns aLayoutRuns = rLayoutArgs.maRuns;
    rLayoutArgs.PrepareFallback();
    rLayoutArgs.mnFlags |= SalLayoutFlags::ForFallback;

    // get list of code units that need glyph fallback
    int nCharPos = -1;
    bool bRTL = false;
    OUStringBuffer aMissingCodeBuf(512);

    while (rLayoutArgs.GetNextPos(&nCharPos, &bRTL))
    {
        aMissingCodeBuf.append(rLayoutArgs.mrStr[nCharPos]);
    }

    rLayoutArgs.ResetPos();
    OUString aMissingCodes = aMissingCodeBuf.makeStringAndClear();

    FontSelectPattern aFontSelData(mpFontInstance->GetFontSelectPattern());

    // try if fallback fonts support the missing code units
    for (int nFallbackLevel = 1; nFallbackLevel < MAX_FALLBACK; ++nFallbackLevel)
    {
        rtl::Reference<LogicalFontInstance> pFallbackFont;

        if (pGlyphs != nullptr && pGlyphs->Impl(nFallbackLevel) != nullptr)
            pFallbackFont = pGlyphs->Impl(nFallbackLevel)->GetFont();

        // find a font family suited for glyph fallback
        // GetGlyphFallbackFont() needs a valid FontInstance
        // if the system-specific glyph fallback is active
        if (!pFallbackFont)
        {
            pFallbackFont = mxFontCache->GetGlyphFallbackFont(mxFontCollection.get(), aFontSelData,
                                                              mpFontInstance.get(), nFallbackLevel,
                                                              aMissingCodes);
        }

        if (!pFallbackFont)
            break;

        if (nFallbackLevel < MAX_FALLBACK - 1)
        {
            // ignore fallback font if it is the same as the original font
            // unless we are looking for a substitution for 0x202F, in which
            // case we'll just use a normal space
            if (mpFontInstance->GetFontFace() == pFallbackFont->GetFontFace()
                && aMissingCodes.indexOf(0x202F) == -1)
            {
                continue;
            }
        }

        // create and add glyph fallback layout to multilayout
        std::unique_ptr<SalLayout> pFallback
            = getFallbackLayout(pFallbackFont.get(), nFallbackLevel, rLayoutArgs, pGlyphs);

        if (pFallback)
        {
            if (!pMultiSalLayout)
                pMultiSalLayout.reset(new MultiSalLayout(std::move(pSalLayout)));

            pMultiSalLayout->AddFallback(std::move(pFallback), rLayoutArgs.maRuns);

            if (nFallbackLevel == MAX_FALLBACK - 1)
                pMultiSalLayout->SetIncomplete(true);
        }

        // break when this fallback was sufficient
        if (!rLayoutArgs.PrepareFallback())
            break;
    }

    if (pMultiSalLayout) // due to missing glyphs, multilevel layout fallback attempted
    {
        // if it works, use that Layout
        if (pMultiSalLayout->LayoutText(rLayoutArgs, nullptr))
            pSalLayout = std::move(pMultiSalLayout);
        else
        {
            // if it doesn't, give up and restore ownership of the pSalLayout
            // back to its original state
            pSalLayout = pMultiSalLayout->ReleaseBaseLayout();
        }
    }

    // restore orig font settings
    pSalLayout->InitFont();
    rLayoutArgs.maRuns = aLayoutRuns;

    return pSalLayout;
}

std::unique_ptr<SalLayout> RenderContext2::getFallbackLayout(LogicalFontInstance* pLogicalFont,
                                                             int nFallbackLevel,
                                                             ImplLayoutArgs& rLayoutArgs,
                                                             SalLayoutGlyphs const* pGlyphs) const
{
    // we need a graphics
    if (!mpGraphics && !AcquireGraphics())
        return nullptr;

    assert(mpGraphics != nullptr);
    mpGraphics->SetFont(pLogicalFont, nFallbackLevel);

    rLayoutArgs.ResetPos();
    std::unique_ptr<GenericSalLayout> pFallback = mpGraphics->GetTextLayout(nFallbackLevel);

    if (!pFallback)
        return nullptr;

    if (!pFallback->LayoutText(rLayoutArgs, pGlyphs ? pGlyphs->Impl(nFallbackLevel) : nullptr))
    {
        // there is no need for a font that couldn't resolve anything
        return nullptr;
    }

    return pFallback;
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
