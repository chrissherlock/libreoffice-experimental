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

#include <tools/debug.hxx>
#include <unotools/configmgr.hxx>

#include <vcl/font/FontCharMap.hxx>
#include <vcl/print.hxx>
#include <vcl/virdev.hxx>

#include <window.h>
#include <font/FeatureCollector.hxx>
#include <font/FontManager.hxx>
#include <font/FontFaceCollection.hxx>
#include <font/FontFaceSizeCollection.hxx>
#include <drawmode.hxx>
#include <salgdi.hxx>
#include <svdata.hxx>

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
    mxFontManager->Init(this);
    return mxFontManager->IsFontAvailable(rFontName);
}

void RenderContext2::SetFontOrientation(FontInstance* const pFontInstance) const
{
    if (pFontInstance->GetFontSelectPattern().mnOrientation && !pFontInstance->GetOrientation())
    {
        pFontInstance->SetOwnOrientation(pFontInstance->GetFontSelectPattern().mnOrientation);
        pFontInstance->SetTextAngle(pFontInstance->GetOwnOrientation());
    }
    else
    {
        pFontInstance->SetTextAngle(pFontInstance->GetOrientation());
    }
}

FontSize RenderContext2::GetFontSize(vcl::Font const& rFont) const
{
    // convert to pixel height
    // TODO: replace integer based aSize completely with subpixel accurate type
    float fExactHeight
        = ImplFloatLogicHeightToDevicePixel(static_cast<float>(rFont.GetFontHeight()));

    Size aSize = ImplLogicToDevicePixel(rFont.GetFontSize());

    if (!aSize.Height())
    {
        // use default pixel height only when logical height is zero
        if (rFont.GetFontSize().Height())
            aSize.setHeight(1);
        else
            aSize.setHeight((12 * GetDPIY()) / 72);

        fExactHeight = static_cast<float>(aSize.Height());
    }

    // select the default width only when logical width is zero
    if ((aSize.Width() == 0) && (rFont.GetFontSize().Width() != 0))
        aSize.setWidth(1);

    return FontSize(aSize.Width(), fExactHeight);
}

Color RenderContext2::GetReadableFontColor(const Color& rFontColor, const Color& rBgColor) const
{
    if (rBgColor.IsDark() && rFontColor.IsDark())
        return COL_WHITE;
    else if (rBgColor.IsBright() && rFontColor.IsBright())
        return COL_BLACK;
    else
        return rFontColor;
}

bool RenderContext2::InitFont()
{
    DBG_TESTSOLARMUTEX();

    if (!InitNewFont())
        return false;

    if (!mpFontInstance)
        return false;

    if (!mpGraphics && !AcquireGraphics())
        return false;

    if (!mbInitFont)
        return true;

    assert(mpGraphics);

    mpGraphics->SetFont(mpFontInstance.get(), 0);
    mbInitFont = false;
    return true;
}

bool RenderContext2::InitNewFont()
{
    DBG_TESTSOLARMUTEX();

    if (!mbNewFont)
        return true;

    if (!mxFontManager->InitFontInstance(this))
        return false;

    mxFontManager->InitTextOffset(this);

    return FixOLEScaleFactors();
}

bool RenderContext2::IsFontUnantialiased() const
{
    bool bNonAntialiased(GetAntialiasing() & AntialiasingFlags::DisableText);

    if (!utl::ConfigManager::IsFuzzing())
    {
        const StyleSettings& rStyleSettings = GetSettings().GetStyleSettings();
        bNonAntialiased |= bool(rStyleSettings.GetDisplayOptions() & DisplayOptions::AADisable);
        bNonAntialiased |= (int(rStyleSettings.GetAntialiasingMinPixelHeight())
                            > maFont.GetFontSize().Height());
    }

    return bNonAntialiased;
}

bool RenderContext2::FixOLEScaleFactors()
{
    bool bRet = true;

    FontSize aSize = GetFontSize(maFont);

    // #95414# fix for OLE objects which use scale factors very creatively
    if (IsMapModeEnabled() && !aSize.GetWidth())
    {
        int nOrigWidth = mpFontInstance->GetWidth();
        float fStretch = static_cast<float>(maMapRes.mnMapScNumX) * maMapRes.mnMapScDenomY;
        fStretch /= static_cast<float>(maMapRes.mnMapScNumY) * maMapRes.mnMapScDenomX;
        int nNewWidth = static_cast<int>(nOrigWidth * fStretch + 0.5);
        if ((nNewWidth != nOrigWidth) && (nNewWidth != 0))
        {
            Size aOrigSize = maFont.GetFontSize();
            const_cast<vcl::Font&>(maFont).SetFontSize(Size(nNewWidth, aSize.GetHeight()));
            EnableMapMode(false);
            mbNewFont = true;
            bRet = InitNewFont(); // recurse once using stretched width
            EnableMapMode();
            const_cast<vcl::Font&>(maFont).SetFontSize(aOrigSize);
        }
    }

    return bRet;
}

FontMetric RenderContext2::GetFontMetric(int nDevFontIndex) const
{
    mxFontManager->Init(this);

    int nCount = GetFontMetricCount();
    if (nDevFontIndex < nCount)
        return mpDeviceFontList->GetFontMetric(nDevFontIndex);

    return FontMetric();
}

int RenderContext2::GetFontMetricCount() const
{
    if (!mpDeviceFontList)
    {
        if (!mxFontManager)
        {
            return 0;
        }

        mpDeviceFontList = mxFontManager->GetDeviceFontList();

        if (!mpDeviceFontList->Count())
        {
            mpDeviceFontList.reset();
            return 0;
        }
    }
    return mpDeviceFontList->Count();
}

int RenderContext2::GetFontFaceSizeCount(vcl::Font const& rFont) const
{
    mpDeviceFontSizeList.reset();

    mxFontManager->Init(this);
    mpDeviceFontSizeList = mxFontManager->GetDeviceFontSizeList(rFont.GetFamilyName());
    return mpDeviceFontSizeList->Count();
}

Size RenderContext2::GetFontFaceSize(vcl::Font const& rFont, int nSizeIndex) const
{
    // check range
    int nCount = GetFontFaceSizeCount(rFont);
    if (nSizeIndex >= nCount)
        return Size();

    // when mapping is enabled round to .5 points
    Size aSize(0, mpDeviceFontSizeList->Get(nSizeIndex));
    if (IsMapModeEnabled())
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
    mxFontManager->Init(this);

    if (!mpGraphics && !AcquireGraphics())
        return false;

    bool bRC = mpGraphics->AddTempDevFont(mxFontManager.get(), rFileURL, rFontName);
    if (!bRC)
        return false;

    if (mpAlphaVDev)
        mpAlphaVDev->AddTempDevFont(rFileURL, rFontName);

    assert(mpGraphics);

    return true;
}

bool RenderContext2::GetFontFeatures(std::vector<vcl::font::Feature>& rFontFeatures) const
{
    RenderContext2* pRC = const_cast<RenderContext2*>(this);
    if (!pRC->InitNewFont())
        return false;

    FontInstance* pFontInstance = mpFontInstance.get();
    if (!pFontInstance)
        return false;

    hb_font_t* pHbFont = pFontInstance->GetHbFont();
    if (!pHbFont)
        return false;

    hb_face_t* pHbFace = hb_font_get_face(pHbFont);
    if (!pHbFace)
        return false;

    const LanguageType eOfficeLanguage
        = Application::GetSettings().GetLanguageTag().getLanguageType();

    vcl::font::FeatureCollector aFeatureCollector(pHbFace, rFontFeatures, eOfficeLanguage);
    aFeatureCollector.collect();

    return true;
}

FontMetric RenderContext2::GetFontMetric() const
{
    FontMetric aMetric;

    RenderContext2* pRC = const_cast<RenderContext2*>(this);
    if (!pRC->InitNewFont())
        return aMetric;

    FontInstance* pFontInstance = mpFontInstance.get();
    ImplFontMetricDataRef xFontMetric = pFontInstance->GetFontMetricData();

    // prepare metric
    aMetric = maFont;

    // set aMetric with info from font
    aMetric.SetFamilyName(maFont.GetFamilyName());
    aMetric.SetStyleName(xFontMetric->GetStyleName());
    aMetric.SetFontSize(PixelToLogic(
        Size(xFontMetric->GetWidth(), xFontMetric->GetAscent() + xFontMetric->GetDescent()
                                          - xFontMetric->GetInternalLeading())));
    aMetric.SetCharSet(xFontMetric->IsSymbolFont() ? RTL_TEXTENCODING_SYMBOL
                                                   : RTL_TEXTENCODING_UNICODE);
    aMetric.SetFamily(xFontMetric->GetFamilyType());
    aMetric.SetPitch(xFontMetric->GetPitch());
    aMetric.SetWeight(xFontMetric->GetWeight());
    aMetric.SetItalic(xFontMetric->GetItalic());
    aMetric.SetAlignment(TextAlign::ALIGN_TOP);
    aMetric.SetWidthType(xFontMetric->GetWidthType());
    if (pFontInstance->GetOwnOrientation())
        aMetric.SetOrientation(pFontInstance->GetOwnOrientation());
    else
        aMetric.SetOrientation(xFontMetric->GetOrientation());

    // set remaining metric fields
    aMetric.SetFullstopCenteredFlag(xFontMetric->IsFullstopCentered());
    aMetric.SetBulletOffset(xFontMetric->GetBulletOffset());
    aMetric.SetAscent(ImplDevicePixelToLogicHeight(xFontMetric->GetAscent()
                                                   + pFontInstance->GetEmphasisAscent()));
    aMetric.SetDescent(ImplDevicePixelToLogicHeight(xFontMetric->GetDescent()
                                                    + pFontInstance->GetEmphasisDescent()));
    aMetric.SetInternalLeading(ImplDevicePixelToLogicHeight(xFontMetric->GetInternalLeading()
                                                            + pFontInstance->GetEmphasisAscent()));
    // RenderContext2 has its own external leading function due to #i60945#
    aMetric.SetExternalLeading(ImplDevicePixelToLogicHeight(GetFontExtLeading()));
    aMetric.SetLineHeight(ImplDevicePixelToLogicHeight(
        xFontMetric->GetAscent() + xFontMetric->GetDescent() + pFontInstance->GetEmphasisAscent()
        + pFontInstance->GetEmphasisDescent()));
    aMetric.SetSlant(ImplDevicePixelToLogicHeight(xFontMetric->GetSlant()));

    // get miscellaneous data
    aMetric.SetQuality(xFontMetric->GetQuality());

    SAL_INFO("vcl.gdi.fontmetric", "RenderContext2::GetFontMetric:" << aMetric);

    xFontMetric = nullptr;

    return aMetric;
}

FontMetric RenderContext2::GetFontMetric(vcl::Font const& rFont) const
{
    // select font, query metrics, select original font again
    vcl::Font aOldFont = GetFont();
    const_cast<RenderContext2*>(this)->SetFont(rFont);
    FontMetric aMetric(GetFontMetric());
    const_cast<RenderContext2*>(this)->SetFont(aOldFont);
    return aMetric;
}

bool RenderContext2::GetFontCharMap(FontCharMapRef& rxFontCharMap) const
{
    RenderContext2* pRC = const_cast<RenderContext2*>(this);
    if (!pRC->InitFont())
        return false;

    FontCharMapRef xFontCharMap(mpGraphics->GetFontCharMap());
    if (!xFontCharMap.is())
    {
        FontCharMapRef xDefaultMap(new FontCharMap());
        rxFontCharMap = xDefaultMap;
    }
    else
        rxFontCharMap = xFontCharMap;

    return !rxFontCharMap->IsDefaultMap();
}

bool RenderContext2::GetFontCapabilities(vcl::FontCapabilities& rFontCapabilities) const
{
    RenderContext2* pRC = const_cast<RenderContext2*>(this);
    if (!pRC->InitFont())
        return false;

    return mpGraphics->GetFontCapabilities(rFontCapabilities);
}

tools::Long RenderContext2::GetFontExtLeading() const
{
    return mpFontInstance->GetExternalLeading();
}

void RenderContext2::ImplClearFontData(const bool bNewFontLists)
{
    // the currently selected logical font is no longer needed
    mpFontInstance.clear();

    mbInitFont = true;
    mbNewFont = true;

    if (bNewFontLists)
    {
        mpDeviceFontList.reset();
        mpDeviceFontSizeList.reset();

        // release all physically selected fonts on this device
        if (AcquireGraphics())
            mpGraphics->ReleaseFonts();
    }

    ImplSVData* pSVData = ImplGetSVData();

    if (mxFontManager && mxFontManager != pSVData->maGDIData.mxScreenFontManager)
        mxFontManager->Invalidate();

    if (bNewFontLists && AcquireGraphics())
    {
        if (mxFontManager && mxFontManager != pSVData->maGDIData.mxScreenFontManager)
            mxFontManager->Clear();
    }
}

void RenderContext2::RefreshFontData(const bool bNewFontLists)
{
    ImplRefreshFontData(bNewFontLists);
}

void RenderContext2::ImplRefreshFontData(const bool bNewFontLists)
{
    if (bNewFontLists && AcquireGraphics())
        mpGraphics->GetDevFontList(mxFontManager.get());
}

void RenderContext2::ImplUpdateFontData()
{
    ImplClearFontData(true /*bNewFontLists*/);
    ImplRefreshFontData(true /*bNewFontLists*/);
}

void RenderContext2::ImplClearAllFontData(bool bNewFontLists)
{
    ImplSVData* pSVData = ImplGetSVData();

    ImplUpdateFontDataForAllFrames(&RenderContext2::ImplClearFontData, bNewFontLists);

    // clear global font lists to have them updated
    pSVData->maGDIData.mxScreenFontManager->Invalidate();
    if (!bNewFontLists)
        return;

    pSVData->maGDIData.mxScreenFontManager->Clear();
    vcl::Window* pFrame = pSVData->maFrameData.mpFirstFrame;
    if (pFrame)
    {
        if (pFrame->AcquireGraphics())
        {
            RenderContext2* pDevice = pFrame;
            pDevice->mpGraphics->ClearDevFontCache();
            pDevice->mpGraphics->GetDevFontList(
                pFrame->mpWindowImpl->mpFrameData->mxFontManager.get());
        }
    }
}

void RenderContext2::ImplRefreshAllFontData(bool bNewFontLists)
{
    ImplUpdateFontDataForAllFrames(&RenderContext2::ImplRefreshFontData, bNewFontLists);
}

void RenderContext2::ImplUpdateAllFontData(bool bNewFontLists)
{
    RenderContext2::ImplClearAllFontData(bNewFontLists);
    RenderContext2::ImplRefreshAllFontData(bNewFontLists);
}

void RenderContext2::ImplUpdateFontDataForAllFrames(const FontUpdateHandler_t pHdl,
                                                    const bool bNewFontLists)
{
    ImplSVData* const pSVData = ImplGetSVData();

    // update all windows
    vcl::Window* pFrame = pSVData->maFrameData.mpFirstFrame;
    while (pFrame)
    {
        (pFrame->*pHdl)(bNewFontLists);

        vcl::Window* pSysWin = pFrame->mpWindowImpl->mpFrameData->mpFirstOverlap;
        while (pSysWin)
        {
            (pSysWin->*pHdl)(bNewFontLists);
            pSysWin = pSysWin->mpWindowImpl->mpNextOverlap;
        }

        pFrame = pFrame->mpWindowImpl->mpFrameData->mpNextFrame;
    }

    // update all virtual devices
    VirtualDevice* pVirDev = pSVData->maGDIData.mpFirstVirDev;
    while (pVirDev)
    {
        (pVirDev->*pHdl)(bNewFontLists);
        pVirDev = pVirDev->mpNext;
    }

    // update all printers
    Printer* pPrinter = pSVData->maGDIData.mpFirstPrinter;
    while (pPrinter)
    {
        (pPrinter->*pHdl)(bNewFontLists);
        pPrinter = pPrinter->mpNext;
    }
}

//hidpi TODO: This routine has hard-coded font-sizes that break places such as DialControl
vcl::Font RenderContext2::GetDefaultFont(DefaultFontType nType, LanguageType eLang,
                                         GetDefaultFontFlags nFlags, RenderContext2 const* pOutDev)
{
    if (!pOutDev && !utl::ConfigManager::IsFuzzing()) // default is NULL
        pOutDev = Application::GetDefaultDevice();

    OUString aSearch;
    if (!utl::ConfigManager::IsFuzzing())
    {
        LanguageTag aLanguageTag(
            (eLang == LANGUAGE_NONE || eLang == LANGUAGE_SYSTEM || eLang == LANGUAGE_DONTKNOW)
                ? Application::GetSettings().GetUILanguageTag()
                : LanguageTag(eLang));

        utl::DefaultFontConfiguration& rDefaults = utl::DefaultFontConfiguration::get();
        OUString aDefault = rDefaults.getDefaultFont(aLanguageTag, nType);

        if (!aDefault.isEmpty())
            aSearch = aDefault;
        else
            aSearch = rDefaults.getUserInterfaceFont(aLanguageTag); // use the UI font as a fallback
    }
    else
        aSearch = "Liberation Serif";

    vcl::Font aFont;
    aFont.SetPitch(PITCH_VARIABLE);

    switch (nType)
    {
        case DefaultFontType::SANS_UNICODE:
        case DefaultFontType::UI_SANS:
            aFont.SetFamily(tools::FAMILY_SWISS);
            break;

        case DefaultFontType::SANS:
        case DefaultFontType::LATIN_HEADING:
        case DefaultFontType::LATIN_SPREADSHEET:
        case DefaultFontType::LATIN_DISPLAY:
            aFont.SetFamily(tools::FAMILY_SWISS);
            break;

        case DefaultFontType::SERIF:
        case DefaultFontType::LATIN_TEXT:
        case DefaultFontType::LATIN_PRESENTATION:
            aFont.SetFamily(tools::FAMILY_ROMAN);
            break;

        case DefaultFontType::FIXED:
        case DefaultFontType::LATIN_FIXED:
        case DefaultFontType::UI_FIXED:
            aFont.SetPitch(PITCH_FIXED);
            aFont.SetFamily(tools::FAMILY_MODERN);
            break;

        case DefaultFontType::SYMBOL:
            aFont.SetCharSet(RTL_TEXTENCODING_SYMBOL);
            break;

        case DefaultFontType::CJK_TEXT:
        case DefaultFontType::CJK_PRESENTATION:
        case DefaultFontType::CJK_SPREADSHEET:
        case DefaultFontType::CJK_HEADING:
        case DefaultFontType::CJK_DISPLAY:
            aFont.SetFamily(
                tools::FAMILY_SYSTEM); // don't care, but don't use font subst config later...
            break;

        case DefaultFontType::CTL_TEXT:
        case DefaultFontType::CTL_PRESENTATION:
        case DefaultFontType::CTL_SPREADSHEET:
        case DefaultFontType::CTL_HEADING:
        case DefaultFontType::CTL_DISPLAY:
            aFont.SetFamily(
                tools::FAMILY_SYSTEM); // don't care, but don't use font subst config later...
            break;
    }

    if (!aSearch.isEmpty())
    {
        aFont.SetFontHeight(12); // corresponds to nDefaultHeight
        aFont.SetWeight(WEIGHT_NORMAL);
        aFont.SetLanguage(eLang);

        if (aFont.GetCharSet() == RTL_TEXTENCODING_DONTKNOW)
            aFont.SetCharSet(osl_getThreadTextEncoding());

        // Should we only return available fonts on the given device
        if (pOutDev)
        {
            pOutDev->mxFontManager->Init(pOutDev);

            // Search Font in the FontList
            OUString aName;
            sal_Int32 nIndex = 0;
            do
            {
                FontFamily* pFontFamily
                    = pOutDev->mxFontManager->FindFontFamily(GetNextFontToken(aSearch, nIndex));
                if (pFontFamily)
                {
                    AddTokenFontName(aName, pFontFamily->GetFamilyName());
                    if (nFlags & GetDefaultFontFlags::OnlyOne)
                        break;
                }
            } while (nIndex != -1);
            aFont.SetFamilyName(aName);
        }

        // No Name, then set all names
        if (aFont.GetFamilyName().isEmpty())
        {
            if (nFlags & GetDefaultFontFlags::OnlyOne)
            {
                if (!pOutDev)
                {
                    SAL_WARN_IF(!utl::ConfigManager::IsFuzzing(), "vcl.gdi",
                                "No default window has been set for the application - we really "
                                "shouldn't be able to get here");
                    aFont.SetFamilyName(aSearch.getToken(0, ';'));
                }
                else
                {
                    pOutDev->mxFontManager->Init(pOutDev);

                    aFont.SetFamilyName(aSearch);

                    // convert to pixel height
                    FontSize aSize = pOutDev->GetFontSize(aFont);

                    // get the name of the first available font
                    rtl::Reference<FontInstance> pFontInstance
                        = pOutDev->mxFontManager->GetFontInstance(aFont, aSize);

                    if (pFontInstance)
                    {
                        assert(pFontInstance->GetFontFace());
                        aFont.SetFamilyName(pFontInstance->GetFontFace()->GetFamilyName());
                    }
                }
            }
            else
            {
                aFont.SetFamilyName(aSearch);
            }
        }
    }

#if OSL_DEBUG_LEVEL > 2
    const char* s = "SANS_UNKNOWN";
    switch (nType)
    {
        case DefaultFontType::SANS_UNICODE:
            s = "SANS_UNICODE";
            break;
        case DefaultFontType::UI_SANS:
            s = "UI_SANS";
            break;

        case DefaultFontType::SANS:
            s = "SANS";
            break;
        case DefaultFontType::LATIN_HEADING:
            s = "LATIN_HEADING";
            break;
        case DefaultFontType::LATIN_SPREADSHEET:
            s = "LATIN_SPREADSHEET";
            break;
        case DefaultFontType::LATIN_DISPLAY:
            s = "LATIN_DISPLAY";
            break;

        case DefaultFontType::SERIF:
            s = "SERIF";
            break;
        case DefaultFontType::LATIN_TEXT:
            s = "LATIN_TEXT";
            break;
        case DefaultFontType::LATIN_PRESENTATION:
            s = "LATIN_PRESENTATION";
            break;

        case DefaultFontType::FIXED:
            s = "FIXED";
            break;
        case DefaultFontType::LATIN_FIXED:
            s = "LATIN_FIXED";
            break;
        case DefaultFontType::UI_FIXED:
            s = "UI_FIXED";
            break;

        case DefaultFontType::SYMBOL:
            s = "SYMBOL";
            break;

        case DefaultFontType::CJK_TEXT:
            s = "CJK_TEXT";
            break;
        case DefaultFontType::CJK_PRESENTATION:
            s = "CJK_PRESENTATION";
            break;
        case DefaultFontType::CJK_SPREADSHEET:
            s = "CJK_SPREADSHEET";
            break;
        case DefaultFontType::CJK_HEADING:
            s = "CJK_HEADING";
            break;
        case DefaultFontType::CJK_DISPLAY:
            s = "CJK_DISPLAY";
            break;

        case DefaultFontType::CTL_TEXT:
            s = "CTL_TEXT";
            break;
        case DefaultFontType::CTL_PRESENTATION:
            s = "CTL_PRESENTATION";
            break;
        case DefaultFontType::CTL_SPREADSHEET:
            s = "CTL_SPREADSHEET";
            break;
        case DefaultFontType::CTL_HEADING:
            s = "CTL_HEADING";
            break;
        case DefaultFontType::CTL_DISPLAY:
            s = "CTL_DISPLAY";
            break;
    }
    SAL_INFO("vcl.gdi", "RenderContext2::GetDefaultFont() Type="
                            << s << " lang=" << eLang << " flags=" << static_cast<int>(nFlags)
                            << " family=\"" << aFont.GetFamilyName() << "\"");
#endif

    return aFont;
}

const FontInstance* RenderContext2::GetFontInstance() const
{
    RenderContext2* pRC = const_cast<RenderContext2*>(this);
    if (!pRC->InitFont())
        return nullptr;

    return mpFontInstance.get();
}

tools::Long RenderContext2::GetMinKashida() const
{
    RenderContext2* pRC = const_cast<RenderContext2*>(this);
    if (!pRC->InitNewFont())
        return 0;

    return ImplDevicePixelToLogicWidth(mpFontInstance->GetMinKashida());
}

sal_Int32 RenderContext2::ValidateKashidas(const OUString& rTxt, sal_Int32 nIdx, sal_Int32 nLen,
                                           sal_Int32 nKashCount, const sal_Int32* pKashidaPos,
                                           sal_Int32* pKashidaPosDropped) const
{
    // do layout
    std::unique_ptr<SalLayout> pSalLayout = ImplLayout(rTxt, nIdx, nLen);
    if (!pSalLayout)
        return 0;
    sal_Int32 nDropped = 0;
    for (int i = 0; i < nKashCount; ++i)
    {
        if (!pSalLayout->IsKashidaPosValid(pKashidaPos[i]))
        {
            pKashidaPosDropped[nDropped] = pKashidaPos[i];
            ++nDropped;
        }
    }
    return nDropped;
}

bool RenderContext2::GetGlyphBoundRects(const Point& rOrigin, const OUString& rStr, int nIndex,
                                        int nLen, std::vector<tools::Rectangle>& rVector)
{
    rVector.clear();

    if (nIndex >= rStr.getLength())
        return false;

    if (nLen < 0 || nIndex + nLen >= rStr.getLength())
    {
        nLen = rStr.getLength() - nIndex;
    }

    tools::Rectangle aRect;
    for (int i = 0; i < nLen; i++)
    {
        if (!GetTextBoundRect(aRect, rStr, nIndex, nIndex + i, 1))
            break;
        aRect.Move(rOrigin.X(), rOrigin.Y());
        rVector.push_back(aRect);
    }

    return (nLen == static_cast<int>(rVector.size()));
}

sal_Int32 RenderContext2::HasGlyphs(const vcl::Font& rTempFont, const OUString& rStr,
                                    sal_Int32 nIndex, sal_Int32 nLen) const
{
    if (nIndex >= rStr.getLength())
        return nIndex;
    sal_Int32 nEnd;
    if (nLen == -1)
        nEnd = rStr.getLength();
    else
        nEnd = std::min(rStr.getLength(), nIndex + nLen);

    SAL_WARN_IF(nIndex >= nEnd, "vcl.gdi", "StartPos >= EndPos?");
    SAL_WARN_IF(nEnd > rStr.getLength(), "vcl.gdi", "String too short");

    // to get the map temporarily set font
    const vcl::Font aOrigFont = GetFont();
    const_cast<RenderContext2&>(*this).SetFont(rTempFont);
    FontCharMapRef xFontCharMap;
    bool bRet = GetFontCharMap(xFontCharMap);
    const_cast<RenderContext2&>(*this).SetFont(aOrigFont);

    // if fontmap is unknown assume it doesn't have the glyphs
    if (!bRet)
        return nIndex;

    for (sal_Int32 i = nIndex; nIndex < nEnd; ++i, ++nIndex)
        if (!xFontCharMap->HasChar(rStr[i]))
            return nIndex;

    return -1;
}

void RenderContext2::ReleaseFontCollection() { mxFontManager.reset(); }

void RenderContext2::CloneFontManager(FontManager const* const pFontManager)
{
    mxFontManager = pFontManager->Clone();
}

void RenderContext2::ResetNewFontCache() { mxFontManager = std::make_shared<FontManager>(); }

void RenderContext2::ImplReleaseFonts()
{
    mpGraphics->ReleaseFonts();

    mbNewFont = true;
    mbInitFont = true;

    mpFontInstance.clear();
    mpDeviceFontList.reset();
    mpDeviceFontSizeList.reset();
}

Point RenderContext2::GetTextOffset() const { return mxFontManager->GetTextOffset(); }

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
