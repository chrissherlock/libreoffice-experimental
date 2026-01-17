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

#include <sal/config.h>

#include <rtl/ustrbuf.hxx>
#include <sal/log.hxx>
#include <tools/debug.hxx>
#include <tools/mapunit.hxx>
#include <i18nlangtag/mslangid.hxx>
#include <i18nlangtag/lang.h>
#include <comphelper/configuration.hxx>

#include <vcl/event.hxx>
#include <vcl/fontcharmap.hxx>
#include <vcl/fntstyle.hxx>
#include <vcl/glyphitem.hxx>
#include <vcl/metaact.hxx>
#include <vcl/metric.hxx>
#include <vcl/print.hxx>
#include <vcl/rendercontext/AntialiasingFlags.hxx>
#include <vcl/rendercontext/GetDefaultFontFlags.hxx>
#include <vcl/sysdata.hxx>
#include <vcl/virdev.hxx>

#include <window.h>
#include <font/EmphasisMark.hxx>

#include <CoordinateMapper.hxx>
#include <FontController.hxx>
#include <GraphicsState.hxx>
#include <ImplLayoutArgs.hxx>
#include <drawmode.hxx>
#include <impfontcache.hxx>
#include <font/DirectFontSubstitution.hxx>
#include <font/PhysicalFontFaceCollection.hxx>
#include <font/PhysicalFontCollection.hxx>
#include <font/FeatureCollector.hxx>
#include <impglyphitem.hxx>
#include <sallayout.hxx>
#include <salgdi.hxx>
#include <svdata.hxx>

#include <unicode/uchar.h>

#include <strings.hrc>

const vcl::Font& OutputDevice::GetFont() const { return mpGraphicsState->maFont; }

void OutputDevice::SetFont(const vcl::Font& rNewFont)
{
    vcl::Font aFont
        = vcl::drawmode::GetFont(rNewFont, GetDrawMode(), GetSettings().GetStyleSettings());

    if (mpMetaFile)
    {
        mpMetaFile->AddAction(new MetaFontAction(aFont));
        // the color and alignment actions don't belong here
        // TODO: get rid of them without breaking anything...
        mpMetaFile->AddAction(new MetaTextAlignAction(aFont.GetAlignment()));
        mpMetaFile->AddAction(
            new MetaTextFillColorAction(aFont.GetFillColor(), !aFont.IsTransparent()));
    }

    if (mpGraphicsState->maFont.IsSameInstance(aFont))
        return;

    // Optimization MT/HDU: COL_TRANSPARENT means SetFont should ignore the font color,
    // because SetTextColor() is used for this.
    // #i28759# mpGraphicsState->maTextColor might have been changed behind our back, commit then, too.
    if (aFont.GetColor() != COL_TRANSPARENT
        && (aFont.GetColor() != mpGraphicsState->maFont.GetColor()
            || aFont.GetColor() != mpGraphicsState->maTextColor))
    {
        mpGraphicsState->maTextColor = aFont.GetColor();
        mbInitTextColor = true;
        if (mpMetaFile)
            mpMetaFile->AddAction(new MetaTextColorAction(aFont.GetColor()));
    }
    mpGraphicsState->maFont = aFont;
    mbNewFont = true;
}

void OutputDevice::SetFontCollection(const std::shared_ptr<vcl::font::PhysicalFontCollection>& pPFC)
{
    if (!mpFontController)
        mpFontController = std::make_unique<vcl::font::FontController>();
    mpFontController->SetFontCollection(pPFC);
}



vcl::font::PhysicalFontCollection* OutputDevice::GetFontCollection() const
{
    return mpFontController ? mpFontController->GetFontCollection() : nullptr;
}




const std::shared_ptr<vcl::font::PhysicalFontCollection>&
OutputDevice::GetSharedFontCollection() const
{
    static const std::shared_ptr<vcl::font::PhysicalFontCollection> pEmpty;
    if (ImplGetSVData()->mbDeInit)
        return pEmpty;

    if (!mpFontController)
        const_cast<OutputDevice*>(this)->mpFontController = std::make_unique<vcl::font::FontController>();
    return mpFontController->GetSharedFontCollection();
}

FontMetric OutputDevice::GetFontMetricFromCollection(int nDevFontIndex) const
{
    ImplInitFontList();

    if (nDevFontIndex < GetFontFaceCollectionCount())
        return FontMetric(*mpFontFaceCollection->Get(nDevFontIndex));

    return FontMetric();
}

int OutputDevice::GetFontFaceCollectionCount() const
{
    if (!mpFontFaceCollection)
    {
        if (!GetSharedFontCollection())
        {
            return 0;
        }

        mpFontFaceCollection = GetSharedFontCollection()->GetFontFaceCollection();

        if (!mpFontFaceCollection->Count())
        {
            mpFontFaceCollection.reset();
            return 0;
        }
    }
    return mpFontFaceCollection->Count();
}

bool OutputDevice::IsFontAvailable(std::u16string_view rFontName) const
{
    ImplInitFontList();
    vcl::font::PhysicalFontFamily* pFound = GetSharedFontCollection()->FindFontFamily(rFontName);
    return (pFound != nullptr);
}

bool OutputDevice::AddTempDevFont(const OUString& rFileURL, const OUString& rFontName) const
{
    ImplInitFontList();

    if (!mpGraphics && !AcquireGraphics())
        return false;
    assert(mpGraphics);

    bool bRC = mpGraphics->AddTempDevFont(GetFontCollection(), rFileURL, rFontName);
    if (!bRC)
        return false;

    return true;
}

bool OutputDevice::RemoveTempDevFont(const OUString& rFileURL, const OUString& rFontName)
{
    if (!mpGraphics && !AcquireGraphics())
        return true; // No graphics -> no fonts used
    assert(mpGraphics);

    return mpGraphics->RemoveTempDevFont(rFileURL, rFontName);
}

bool OutputDevice::GetFontFeatures(std::vector<vcl::font::Feature>& rFontFeatures) const
{
    if (!ImplNewFont())
        return false;

    // [Step 9] Refactor: Use FontRealization
    if (mpFontRealization && mpFontRealization->mxFont)
    {
        mpFontController->GetFontFeatures(mpFontRealization->mxFont.get(), rFontFeatures);
        return true;
    }
    return false;
}

FontMetric OutputDevice::GetFontMetric() const
{
    FontMetric aMetric;
    if (!ImplNewFont())
        return aMetric;

    // Initialize with device-specific logical font and alignment
    aMetric = mpGraphicsState->maFont;
    aMetric.SetAlignment(TextAlign::ALIGN_TOP);

    // Delegate the complex data mapping to the controller
    mpFontController->PopulateFontMetric(
        aMetric, mpGraphicsState->maFont, mpFontRealization->mxFont.get(),
        mpFontRealization->nEmphasisAscent, mpFontRealization->nEmphasisDescent);

    // Convert metrics from pixels to logical units
    aMetric.SetFontSize(PixelToLogic(aMetric.GetFontSize()));
    aMetric.SetAscent(DevicePixelToLogicHeight(aMetric.GetAscent()));
    aMetric.SetDescent(DevicePixelToLogicHeight(aMetric.GetDescent()));
    aMetric.SetInternalLeading(DevicePixelToLogicHeight(aMetric.GetInternalLeading()));
    aMetric.SetLineHeight(DevicePixelToLogicHeight(aMetric.GetLineHeight()));
    aMetric.SetSlant(DevicePixelToLogicHeight(aMetric.GetSlant()));
    aMetric.SetHangingBaseline(DevicePixelToLogicHeight(aMetric.GetHangingBaseline()));

    aMetric.SetUnitEm(DevicePixelToLogicWidth(aMetric.GetUnitEm()));
    aMetric.SetHorCJKAdvance(DevicePixelToLogicWidth(aMetric.GetHorCJKAdvance()));
    aMetric.SetVertCJKAdvance(DevicePixelToLogicHeight(aMetric.GetVertCJKAdvance()));

    // OutputDevice manages external leading separately due to legacy #i60945#
    aMetric.SetExternalLeading(DevicePixelToLogicHeight(GetFontExtLeading()));

    SAL_INFO("vcl.gdi.fontmetric", "OutputDevice::GetFontMetric:" << aMetric);
    return aMetric;
}

FontMetric OutputDevice::GetFontMetric(const vcl::Font& rFont) const
{
    // select font, query metrics, select original font again
    vcl::Font aOldFont = GetFont();
    const_cast<OutputDevice*>(this)->SetFont(rFont);
    FontMetric aMetric(GetFontMetric());
    const_cast<OutputDevice*>(this)->SetFont(aOldFont);
    return aMetric;
}

bool OutputDevice::GetFontCharMap(FontCharMapRef& rxFontCharMap) const
{
    if (!InitFont())
        return false;

    rxFontCharMap = mpFontController->GetFontCharMap(mpGraphics);

    return !rxFontCharMap->IsDefaultMap();
}

bool OutputDevice::GetFontCapabilities(vcl::FontCapabilities& rFontCapabilities) const
{
    if (!InitFont())
        return false;

    return mpFontController->GetFontCapabilities(mpGraphics, rFontCapabilities);
}

tools::Long OutputDevice::GetFontExtLeading() const
{
    // [Step 9] Refactor: Use FontRealization
    if (mpFontRealization && mpFontRealization->mxFont)
        return mpFontRealization->mxFont->mxFontMetric->GetExternalLeading();
    return 0;
}

void OutputDevice::ImplClearFontData(const bool bNewFontLists)
{
    // the currently selected logical font is no longer needed
    mpFontInstance.clear();

    mbFontDirty = true;
    mbNewFont = true;

    if (bNewFontLists)
    {
        mpFontFaceCollection.reset();

        // release all physically selected fonts on this device
        if (AcquireGraphics())
            mpGraphics->ReleaseFonts();
    }

    ImplSVData* pSVData = ImplGetSVData();

    if (true)
        InvalidateFontCache();

    if (bNewFontLists && AcquireGraphics())
    {
        if (GetSharedFontCollection()
            && GetSharedFontCollection() != pSVData->maGDIData.mxScreenFontList)
            GetSharedFontCollection()->Clear();
    }
}

void OutputDevice::RefreshFontData(const bool bNewFontLists) { ImplRefreshFontData(bNewFontLists); }

void OutputDevice::ImplRefreshFontData(const bool bNewFontLists)
{
    if (bNewFontLists && AcquireGraphics())
        mpGraphics->GetDevFontList(GetFontCollection());
}

void OutputDevice::ImplUpdateFontData()
{
    ImplClearFontData(true /*bNewFontLists*/);
    ImplRefreshFontData(true /*bNewFontLists*/);
}

void OutputDevice::ImplClearAllFontData(bool bNewFontLists)
{
    ImplSVData* pSVData = ImplGetSVData();

    ImplUpdateFontDataForAllFrames(&OutputDevice::ImplClearFontData, bNewFontLists);

    // clear global font lists to have them updated
    pSVData->maGDIData.mxScreenFontCache->Invalidate();
    if (!bNewFontLists)
        return;

    pSVData->maGDIData.mxScreenFontList->Clear();
    vcl::Window* pFrame = pSVData->maFrameData.mpFirstFrame;
    if (!pFrame)
        return;

    if (pFrame->GetOutDev()->AcquireGraphics())
    {
        OutputDevice* pDevice = pFrame->GetOutDev();
        pDevice->mpGraphics->ClearDevFontCache();
        pDevice->mpGraphics->GetDevFontList(
            pFrame->mpWindowImpl->mpFrameData->mxFontCollection.get());
    }
}

void OutputDevice::ImplRefreshAllFontData(bool bNewFontLists)
{
    ImplUpdateFontDataForAllFrames(&OutputDevice::ImplRefreshFontData, bNewFontLists);
}

void OutputDevice::ImplUpdateAllFontData(bool bNewFontLists)
{
    OutputDevice::ImplClearAllFontData(bNewFontLists);
    OutputDevice::ImplRefreshAllFontData(bNewFontLists);
}

void OutputDevice::ImplUpdateFontDataForAllFrames(const FontUpdateHandler_t pHdl,
                                                  const bool bNewFontLists)
{
    ImplSVData* const pSVData = ImplGetSVData();

    // update all windows
    vcl::Window* pFrame = pSVData->maFrameData.mpFirstFrame;
    while (pFrame)
    {
        (pFrame->GetOutDev()->*pHdl)(bNewFontLists);

        vcl::Window* pSysWin = pFrame->mpWindowImpl->mpFrameData->mpFirstOverlap;
        while (pSysWin)
        {
            (pSysWin->GetOutDev()->*pHdl)(bNewFontLists);
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

void OutputDevice::BeginFontSubstitution()
{
    ImplSVData* pSVData = ImplGetSVData();
    pSVData->maGDIData.mbFontSubChanged = false;
}

void OutputDevice::EndFontSubstitution()
{
    ImplSVData* pSVData = ImplGetSVData();
    if (pSVData->maGDIData.mbFontSubChanged)
    {
        ImplUpdateAllFontData(false);

        DataChangedEvent aDCEvt(DataChangedEventType::FONTSUBSTITUTION);
        Application::ImplCallEventListenersApplicationDataChanged(&aDCEvt);
        Application::NotifyAllWindows(aDCEvt);
        pSVData->maGDIData.mbFontSubChanged = false;
    }
}

void OutputDevice::AddFontSubstitute(const OUString& rFontName, const OUString& rReplaceFontName,
                                     AddFontSubstituteFlags nFlags)
{
    vcl::font::DirectFontSubstitution*& rpSubst = ImplGetSVData()->maGDIData.mpDirectFontSubst;
    if (!rpSubst)
        rpSubst = new vcl::font::DirectFontSubstitution;
    rpSubst->AddFontSubstitute(rFontName, rReplaceFontName, nFlags);
    ImplGetSVData()->maGDIData.mbFontSubChanged = true;
}

void OutputDevice::RemoveFontsSubstitute()
{
    vcl::font::DirectFontSubstitution* pSubst = ImplGetSVData()->maGDIData.mpDirectFontSubst;
    if (pSubst)
        pSubst->RemoveFontsSubstitute();
}

//hidpi TODO: This routine has hard-coded font-sizes that break places such as DialControl
vcl::Font OutputDevice::GetDefaultFont(DefaultFontType nType, LanguageType eLang,
                                       GetDefaultFontFlags nFlags, const OutputDevice* pOutDev)
{
    static bool bFuzzing = comphelper::IsFuzzing();
    static bool bAbortOnFontSubstitute = [] {
        const char* pEnv = getenv("SAL_NON_APPLICATION_FONT_USE");
        return pEnv && strcmp(pEnv, "abort") == 0;
    }();

    if (!pOutDev && !bFuzzing) // default is NULL
        pOutDev = Application::GetDefaultDevice();

    OUString aSearch;
    if (!bFuzzing)
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

        // during cppunit tests with SAL_NON_APPLICATION_FONT_USE set we don't have any bundled fonts
        // that support the default CTL and CJK languages of Hindi and Chinese, so just pick something
        // (unsuitable) that does exist, if they get used with SAL_NON_APPLICATION_FONT_USE=abort then
        // glyph fallback will trigger std::abort
        if (bAbortOnFontSubstitute)
        {
            if (eLang == LANGUAGE_HINDI || eLang == LANGUAGE_CHINESE_SIMPLIFIED)
                aSearch = "DejaVu Sans";
        }
    }
    else
        aSearch = "Liberation Serif";

    vcl::Font aFont;
    aFont.SetPitch(PITCH_VARIABLE);

    switch (nType)
    {
        case DefaultFontType::SANS_UNICODE:
        case DefaultFontType::UI_SANS:
        case DefaultFontType::SANS:
        case DefaultFontType::LATIN_HEADING:
        case DefaultFontType::LATIN_SPREADSHEET:
        case DefaultFontType::LATIN_DISPLAY:
            aFont.SetFamily(FAMILY_SWISS);
            break;

        case DefaultFontType::SERIF:
        case DefaultFontType::LATIN_TEXT:
        case DefaultFontType::LATIN_PRESENTATION:
            aFont.SetFamily(FAMILY_ROMAN);
            break;

        case DefaultFontType::FIXED:
        case DefaultFontType::LATIN_FIXED:
        case DefaultFontType::UI_FIXED:
            aFont.SetPitch(PITCH_FIXED);
            aFont.SetFamily(FAMILY_MODERN);
            break;

        case DefaultFontType::SYMBOL:
            aFont.SetCharSet(RTL_TEXTENCODING_SYMBOL);
            break;

        case DefaultFontType::CJK_TEXT:
        case DefaultFontType::CJK_PRESENTATION:
        case DefaultFontType::CJK_SPREADSHEET:
        case DefaultFontType::CJK_HEADING:
        case DefaultFontType::CJK_DISPLAY:
        case DefaultFontType::CTL_TEXT:
        case DefaultFontType::CTL_PRESENTATION:
        case DefaultFontType::CTL_SPREADSHEET:
        case DefaultFontType::CTL_HEADING:
        case DefaultFontType::CTL_DISPLAY:
            aFont.SetFamily(FAMILY_SYSTEM); // don't care, but don't use font subst config later...
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
            pOutDev->ImplInitFontList();

            // Search Font in the FontList
            OUString aName;
            sal_Int32 nIndex = 0;
            do
            {
                vcl::font::PhysicalFontFamily* pFontFamily
                    = pOutDev->GetSharedFontCollection()->FindFontFamily(
                        GetNextFontToken(aSearch, nIndex));
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
                    SAL_WARN_IF(!comphelper::IsFuzzing(), "vcl.gdi",
                                "No default window has been set for the application - we really "
                                "shouldn't be able to get here");
                    aFont.SetFamilyName(aSearch.getToken(0, ';'));
                }
                else
                {
                    pOutDev->ImplInitFontList();

                    aFont.SetFamilyName(aSearch);

                    // convert to pixel height
                    Size aSize = pOutDev->LogicToDevicePixel(aFont.GetFontSize());
                    if (!aSize.Height())
                    {
                        // use default pixel height only when logical height is zero
                        if (aFont.GetFontHeight())
                            aSize.setHeight(1);
                        else
                            aSize.setHeight((12 * pOutDev->GetDPIY()) / 72);
                    }

                    // use default width only when logical width is zero
                    if ((0 == aSize.Width()) && (0 != aFont.GetFontSize().Width()))
                        aSize.setWidth(1);

                    // get the name of the first available font
                    float fExactHeight = static_cast<float>(aSize.Height());
                    rtl::Reference<LogicalFontInstance> pFontInstance
                        = pOutDev->mpFontController->RealizeFont(pOutDev->GetFontCollection(),
                                                                 aFont, aSize, fExactHeight);
                    if (pFontInstance)
                    {
                        assert(pFontInstance->GetFontFace());
                        aFont.SetFamilyName(pFontInstance->GetFontFace()->GetFamilyName());
                    }
                }
            }
            else
                aFont.SetFamilyName(aSearch);
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
    SAL_INFO("vcl.gdi", "OutputDevice::GetDefaultFont() Type="
                            << s << " lang=" << eLang << " flags=" << static_cast<int>(nFlags)
                            << " family=\"" << aFont.GetFamilyName() << "\"");
#endif

    return aFont;
}

void OutputDevice::ImplInitFontList() const
{
    if (mpFontController)
    {
        if (!mpFontController->GetSharedFontCollection())
            mpFontController->SetFontCollection(GetSharedFontCollection());

        mpFontController->InitializeFonts(mpGraphics);
    }

    if (GetSharedFontCollection()->Count())
        return;

    if (!(mpGraphics || AcquireGraphics()))
        return;

    assert(mpGraphics);

    SAL_INFO("vcl.gdi", "OutputDevice::ImplInitFontList()");

    // Abort if no fonts are found; VCL cannot function without a system font
    if (!GetSharedFontCollection()->Count())
    {
        OUString aError(
            u"Application error: no fonts and no vcl resource found on your system"_ustr);
        OUString aResStr(VclResId(SV_ACCESSERROR_NO_FONTS));
        if (!aResStr.isEmpty())
            aError = aResStr;
        Application::Abort(aError);
    }
}

bool OutputDevice::InitFont() const
{
    DBG_TESTSOLARMUTEX();

    if (mpFontController->NeedsUpdate(mpGraphicsState->maFont, mbNewFont))
    {
        if (!ImplNewFont())
            return false;
    }

    // [Step 7] Hybrid Check: Prefer FontRealization, fallback to mpFontInstance
    LogicalFontInstance* pFontToUse = nullptr;
    if (mpFontRealization && mpFontRealization->mxFont)
        pFontToUse = mpFontRealization->mxFont.get();
    else if (mpFontInstance)
        pFontToUse = mpFontInstance.get();

    if (!pFontToUse)
        return false;

    if (!mpGraphics)
    {
        if (!AcquireGraphics())
            return false;
    }
    else if (!mbFontDirty)
        return true;

    assert(mpGraphics);
    mpGraphics->SetFont(pFontToUse, 0);
    mbFontDirty = false;
    return true;
}

const LogicalFontInstance* OutputDevice::GetFontInstance() const
{
    if (!InitFont())
        return nullptr;

    // [Step 7] Hybrid Return
    if (mpFontRealization && mpFontRealization->mxFont)
        return mpFontRealization->mxFont.get();

    return mpFontInstance.get();
}

bool OutputDevice::ImplNewFont() const
{
    DBG_TESTSOLARMUTEX();

    SAL_INFO("vcl.gdi", "ImplNewFont: Start. Dirty=" << mbFontDirty << " New=" << mbNewFont);

    if (!mbNewFont)
        return true;

    if (!mpGraphics && !AcquireGraphics())
    {
        SAL_WARN("vcl.gdi", "OutputDevice::ImplNewFont(): no Graphics, no Font");
        return false;
    }
    assert(mpGraphics);

    ImplInitFontList();

    Size aReqSize = mpGraphicsState->maFont.GetFontSize();
    SAL_INFO("vcl.gdi", "ImplNewFont: Requesting Family='"
                            << mpGraphicsState->maFont.GetFamilyName()
                            << "' Size=" << aReqSize.Width() << "x" << aReqSize.Height()
                            << " MapMode=" << mpMapper->IsMapModeEnabled() << " DPI=" << GetDPIY());

    auto[fExactHeight, aSize]
        = mpFontController->CalculateDeviceSize(mpGraphicsState->maFont, *mpMapper, GetDPIY());
    // [Refactor] Step 2: OLE Scaling delegated to FontController
    if (mpFontController->NeedsOLEFontScaleFix(*mpMapper, aSize))
    {
        SAL_INFO("vcl.gdi", "ImplNewFont: Correcting OLE Font Scale");
        aSize = mpFontController->GetOLECorrectedSize(*mpMapper, aSize, aSize.Height());
    }


    SAL_INFO("vcl.gdi", "ImplNewFont: Calculated Pixel Size=" << aSize.Width() << "x"
                                                              << aSize.Height()
                                                              << " ExactHeight=" << fExactHeight);

    const bool bNonAntialiased = mpFontController->ShouldDisableAntialiasing(
        GetAntialiasing(), GetSettings().GetStyleSettings(),
        mpGraphicsState->maFont.GetFontSize().Height());

    mpFontController->RealizeFont(GetFontCollection(), mpGraphicsState->maFont, aSize, fExactHeight,
                                  bNonAntialiased);

    rtl::Reference<LogicalFontInstance> pOldFontInstance = mpFontInstance;
    mpFontInstance = mpFontController->RealizeFont(GetFontCollection(), mpGraphicsState->maFont,
                                                   aSize, fExactHeight, bNonAntialiased);

    if (!mpFontInstance)
    {
        SAL_WARN("vcl.gdi", "ImplNewFont: !!! NO FONT INSTANCE FOUND for request !!!");
    }
    else
    {
        OUString sName = "Unknown";

        if (mpFontInstance->GetFontFace())
            sName = mpFontInstance->GetFontFace()->GetFamilyName();

        SAL_INFO("vcl.gdi", "ImplNewFont: Success. Selected physical font: " << sName);
    }

    // We must update the struct *before* calling InitFont, because InitFont
    // now prefers reading from mpFontRealization.
    if (mpFontRealization)
        mpFontRealization->mxFont = mpFontInstance;

    const bool bNewFontInstance = pOldFontInstance.get() != mpFontInstance.get();
    pOldFontInstance.clear();

    LogicalFontInstance* pFontInstance = mpFontInstance.get();

    if (!pFontInstance)
    {
        SAL_WARN("vcl.gdi", "OutputDevice::ImplNewFont(): no LogicalFontInstance, no Font");
        return false;
    }

    // Compute font size in points for optical sizing.
    if (!pFontInstance->GetPointSize())
    {
        auto nHeight = mpGraphicsState->maFont.GetFontHeight();
        auto eFrom = MapToO3tlLength(GetMapMode().GetMapUnit());
        float fPointSize = o3tl::convert(float(nHeight), eFrom, o3tl::Length::pt);
        pFontInstance->SetPointSize(fPointSize);
    }

    // mark when lower layers need to get involved
    mbNewFont = false;
    if (bNewFontInstance)
        mbFontDirty = true;

    if (!pFontInstance->mbInit && InitFont())
    {
        SAL_INFO("vcl.gdi", "ImplNewFont: Triggering hardware initialization (InitFont)");
        mpFontController->InitializeInstance(mpFontInstance.get(), mpGraphics);
        ImplInitFontMetrics(mpFontInstance.get());
        SetFontOrientation(mpFontInstance.get());
    }

    std::tie(mpFontRealization->nXOffset, mpFontRealization->nYOffset,
             mpFontRealization->nEmphasisAscent, mpFontRealization->nEmphasisDescent)
        = mpFontController->CalculateTextOffsets(mpGraphicsState->maFont, mpFontInstance.get());

    // Use local temporary variables to bypass the bit-field reference restriction
    bool bTextLines = false;
    bool bTextSpecial = false;

    std::tie(bTextLines, bTextSpecial)
        = mpFontController->GetTextLayoutFlags(mpGraphicsState->maFont);

    bool bRet = true;


    if (mpFontRealization)
    {
        mpFontRealization->mxFont = mpFontInstance;
        mpFontRealization->bHasLineDecorations = bTextLines;
        mpFontRealization->bHasSpecialEffects = bTextSpecial;
        mpFontRealization->eLayoutMode = mpGraphicsState->mnTextLayoutMode;
    }

    return bRet;
}

void OutputDevice::ImplInitFontMetrics(LogicalFontInstance* pFontInstance) const
{
    pFontInstance->mxFontMetric->ImplInitTextLineSize(this);
    pFontInstance->mxFontMetric->ImplInitAboveTextLineSize(this);
    pFontInstance->mxFontMetric->ImplInitFlags(this);

    pFontInstance->mnLineHeight
        = pFontInstance->mxFontMetric->GetAscent() + pFontInstance->mxFontMetric->GetDescent();
}

void OutputDevice::SetFontOrientation(LogicalFontInstance* const pFontInstance) const
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

void OutputDevice::ImplDrawEmphasisMark(tools::Long nBaseX, tools::Long nX, tools::Long nY,
                                        const tools::PolyPolygon& rPolyPoly, bool bPolyLine,
                                        const tools::Rectangle& rRect1,
                                        const tools::Rectangle& rRect2)
{
    if (IsRTLEnabled())
        nX = nBaseX - (nX - nBaseX - 1);

    nX -= GetOutOffXPixel();
    nY -= GetOutOffYPixel();

    if (rPolyPoly.Count())
    {
        if (bPolyLine)
        {
            tools::Polygon aPoly = rPolyPoly.GetObject(0);
            aPoly.Move(nX, nY);
            DrawPolyLine(aPoly);
        }
        else
        {
            tools::PolyPolygon aPolyPoly = rPolyPoly;
            aPolyPoly.Move(nX, nY);
            DrawPolyPolygon(aPolyPoly);
        }
    }

    if (!rRect1.IsEmpty())
    {
        tools::Rectangle aRect(Point(nX + rRect1.Left(), nY + rRect1.Top()), rRect1.GetSize());
        DrawRect(aRect);
    }

    if (!rRect2.IsEmpty())
    {
        tools::Rectangle aRect(Point(nX + rRect2.Left(), nY + rRect2.Top()), rRect2.GetSize());

        DrawRect(aRect);
    }
}

void OutputDevice::ImplDrawEmphasisMarks(SalLayout& rSalLayout)
{
    vcl::font::FontRealization const* pRealization = mpFontRealization.get();
    if (!pRealization || !pRealization->mxFont)
        return;

    auto popIt = ScopedPush(vcl::PushFlags::FILLCOLOR | vcl::PushFlags::LINECOLOR
                            | vcl::PushFlags::MAPMODE);
    GDIMetaFile* pOldMetaFile = mpMetaFile;
    mpMetaFile = nullptr;
    mpMapper->EnableMapMode(false);

    FontEmphasisMark nEmphasisMark = mpGraphicsState->maFont.GetEmphasisMarkStyle();
    tools::Long nEmphasisHeight;

    if (nEmphasisMark & FontEmphasisMark::PosBelow)
        nEmphasisHeight = pRealization->nEmphasisDescent;
    else
        nEmphasisHeight = pRealization->nEmphasisAscent;

    vcl::font::EmphasisMark aEmphasisMark(nEmphasisMark, nEmphasisHeight, GetDPIY());

    if (aEmphasisMark.IsShapePolyLine())
    {
        SetLineColor(GetTextColor());
        SetFillColor();
    }
    else
    {
        SetLineColor();
        SetFillColor(GetTextColor());
    }

    Point aOffset(0, 0);
    Point aOffsetVert(0, 0);

    if (nEmphasisMark & FontEmphasisMark::PosBelow)
    {
        aOffset.AdjustY(pRealization->mxFont->mxFontMetric->GetDescent()
                        + aEmphasisMark.GetYOffset());
        aOffsetVert = aOffset;
    }
    else
    {
        aOffset.AdjustY(
            -(pRealization->mxFont->mxFontMetric->GetAscent() + aEmphasisMark.GetYOffset()));
        // Todo: use ideographic em-box or ideographic character face information.
        aOffsetVert.AdjustY(-(pRealization->mxFont->mxFontMetric->GetAscent()
                              + pRealization->mxFont->mxFontMetric->GetDescent()
                              + aEmphasisMark.GetYOffset()));
    }

    tools::Long nEmphasisWidth2 = aEmphasisMark.GetWidth() / 2;
    tools::Long nEmphasisHeight2 = nEmphasisHeight / 2;
    aOffset += Point(nEmphasisWidth2, nEmphasisHeight2);

    basegfx::B2DPoint aOutPoint;
    basegfx::B2DRectangle aRectangle;
    const GlyphItem* pGlyph;
    const LogicalFontInstance* pGlyphFont;
    int nStart = 0;
    while (rSalLayout.GetNextGlyph(&pGlyph, aOutPoint, nStart, &pGlyphFont))
    {
        if (!pGlyph->GetGlyphBoundRect(pGlyphFont, aRectangle))
            continue;

        if (!pGlyph->IsSpacing())
        {
            Point aAdjPoint;
            if (pGlyph->IsVertical())
            {
                aAdjPoint = aOffsetVert;
                aAdjPoint.AdjustX((-pGlyph->origWidth() + aEmphasisMark.GetWidth()) / 2);
            }
            else
            {
                aAdjPoint = aOffset;
                aAdjPoint.AdjustX(aRectangle.getMinX()
                                  + (aRectangle.getWidth() - aEmphasisMark.GetWidth()) / 2);
            }

            if (pRealization->mxFont->mnOrientation)
            {
                Point aOriginPt(0, 0);
                aOriginPt.RotateAround(aAdjPoint, pRealization->mxFont->mnOrientation);
            }
            aOutPoint.adjustX(aAdjPoint.X() - nEmphasisWidth2);
            aOutPoint.adjustY(aAdjPoint.Y() - nEmphasisHeight2);
            ImplDrawEmphasisMark(rSalLayout.DrawBase().getX(), aOutPoint.getX(), aOutPoint.getY(),
                                 aEmphasisMark.GetShape(), aEmphasisMark.IsShapePolyLine(),
                                 aEmphasisMark.GetRect1(), aEmphasisMark.GetRect2());
        }
    }

    mpMetaFile = pOldMetaFile;
}

std::unique_ptr<SalLayout> OutputDevice::getFallbackLayout(LogicalFontInstance* pLogicalFont,
                                                           int nFallbackLevel,
                                                           vcl::text::ImplLayoutArgs& rLayoutArgs,
                                                           const SalLayoutGlyphs* pGlyphs) const
{
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

bool OutputDevice::ForceFallbackFont(vcl::Font const& rFallbackFont)
{
    vcl::Font aOldFont = GetFont();
    SetFont(rFallbackFont);
    if (!InitFont())
        return false;

    mpForcedFallbackInstance = mpFontInstance;
    SetFont(aOldFont);
    if (!InitFont())
        return false;

    if (mpForcedFallbackInstance)
        return true;

    return false;
}

std::unique_ptr<SalLayout>
OutputDevice::ImplGlyphFallbackLayout(std::unique_ptr<SalLayout> pSalLayout,
                                      vcl::text::ImplLayoutArgs& rLayoutArgs,
                                      const SalLayoutGlyphs* pGlyphs) const
{
    // This function relies on a valid mpFontInstance, if it doesn't exist bail out
    // - we'd have crashed later on anyway. At least here we can catch the error in debug
    // mode.
    if (!mpFontRealization || !mpFontRealization->mxFont)
    {
        SAL_WARN("vcl.gdi", "No font entry set in OutputDevice");
        assert(mpFontRealization->mxFont);
        return nullptr;
    }

    // prepare multi level glyph fallback
    std::unique_ptr<MultiSalLayout> pMultiSalLayout;
    ImplLayoutRuns aLayoutRuns = rLayoutArgs.maRuns;
    rLayoutArgs.PrepareFallback(nullptr);
    rLayoutArgs.mnFlags |= SalLayoutFlags::ForFallback;

    // get list of code units that need glyph fallback
    bool bRTL;
    int nMinRunPos, nEndRunPos;
    OUStringBuffer aMissingCodeBuf(512);
    while (rLayoutArgs.GetNextRun(&nMinRunPos, &nEndRunPos, &bRTL))
        aMissingCodeBuf.append(rLayoutArgs.mrStr.subView(nMinRunPos, nEndRunPos - nMinRunPos));
    rLayoutArgs.ResetPos();
    OUString aMissingCodes = aMissingCodeBuf.makeStringAndClear();

    vcl::font::FontSelectPattern aFontSelData(mpFontRealization->mxFont->GetFontSelectPattern());
    SalLayoutGlyphsImpl* pGlyphsImpl = pGlyphs ? pGlyphs->Impl(1) : nullptr;

    bool bHasUsedFallback = false;

    // try if fallback fonts support the missing code units
    for (int nFallbackLevel = 1; nFallbackLevel < MAX_FALLBACK; ++nFallbackLevel)
    {
        rtl::Reference<LogicalFontInstance> pFallbackFont;
        if (!bHasUsedFallback && mpForcedFallbackInstance)
        {
            pFallbackFont = mpForcedFallbackInstance;
            bHasUsedFallback = true;
        }
        else if (pGlyphsImpl != nullptr)
        {
            pFallbackFont = pGlyphsImpl->GetFont();
        }

        // find a font family suited for glyph fallback
        // GetGlyphFallbackFont() needs a valid FontInstance
        // if the system-specific glyph fallback is active
        OUString oldMissingCodes = aMissingCodes;
        if (!pFallbackFont)
            pFallbackFont = GetFontCache().GetGlyphFallbackFont(GetFontCollection(), aFontSelData,
                                                                mpFontRealization->mxFont.get(),
                                                                nFallbackLevel, aMissingCodes);
        if (!pFallbackFont)
            break;

        SAL_INFO("vcl", "Fallback font (level "
                            << nFallbackLevel
                            << "): family: " << pFallbackFont->GetFontFace()->GetFamilyName()
                            << ", style: " << pFallbackFont->GetFontFace()->GetStyleName());

        if (nFallbackLevel < MAX_FALLBACK - 1)
        {
            // ignore fallback font if it is the same as the original font
            // TODO: This seems broken. Either the font does not provide any of the missing
            // codes, in which case the fallback should not select it. Or it does provide
            // some of the missing codes, and then why weren't they used the first time?
            // This will just loop repeatedly finding the same font (it used to remove
            // the found font from mxFontCache, but doesn't do that anymore and I don't
            // see how doing that would remove the font from consideration for fallback).
            if (mpFontRealization->mxFont->GetFontFace() == pFallbackFont->GetFontFace())
            {
                if (aMissingCodes != oldMissingCodes)
                {
                    SAL_INFO("vcl.gdi", "Font fallback to the same font, but has missing codes");
                    // Restore the missing codes if we're not going to use this font.
                    aMissingCodes = oldMissingCodes;
                }
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

        if (pGlyphs != nullptr)
            pGlyphsImpl = pGlyphs->Impl(nFallbackLevel + 1);

        // break when this fallback was sufficient
        if (!rLayoutArgs.PrepareFallback(pGlyphsImpl))
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
    rLayoutArgs.maRuns = std::move(aLayoutRuns);

    return pSalLayout;
}

tools::Long OutputDevice::GetMinKashida() const
{
    if (!ImplNewFont())
        return 0;

    double nKashidaWidth = mpFontController->GetMinKashidaWidth(mpFontRealization->mxFont.get());

    if (!mpMapper->IsMapModeEnabled())
        nKashidaWidth = std::ceil(nKashidaWidth);

    return mpMapper->DevicePixelToLogicWidth(nKashidaWidth);
}

// tdf#163105: Get map of valid kashida positions for a single word
void OutputDevice::GetWordKashidaPositions(const OUString& rText, std::vector<bool>* pOutMap) const
{
    pOutMap->clear();

    auto nEnd = rText.getLength();

    std::unique_ptr<SalLayout> pSalLayout = ImplLayout(rText, 0, nEnd);
    if (!pSalLayout)
        return;

    // tdf#163215: VCL cannot suggest valid kashida positions for certain fonts (e.g. AAT).
    if (!pSalLayout->HasFontKashidaPositions())
        return;

    pOutMap->resize(nEnd, false);
    for (sal_Int32 i = 0; i < nEnd; ++i)
    {
        auto nNextPos = i + 1;

        // Skip combining marks to find the next character after this position.
        while (nNextPos < nEnd
               && u_getIntPropertyValue(rText[nNextPos], UCHAR_JOINING_TYPE) == U_JT_TRANSPARENT)
        {
            ++nNextPos;
        }

        pOutMap->at(i) = pSalLayout->IsKashidaPosValid(i, nNextPos);
    }
}

bool OutputDevice::GetGlyphBoundRects(const Point& rOrigin, const OUString& rStr, int nIndex,
                                      int nLen, std::vector<tools::Rectangle>& rVector) const
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

sal_Int32 OutputDevice::HasGlyphs(const vcl::Font& rTempFont, std::u16string_view rStr,
                                  sal_Int32 nIndex, sal_Int32 nLen) const
{
    if (nIndex >= static_cast<sal_Int32>(rStr.size()))
        return nIndex;
    sal_Int32 nEnd;
    if (nLen == -1)
        nEnd = rStr.size();
    else
        nEnd = std::min<sal_Int32>(rStr.size(), nIndex + nLen);

    SAL_WARN_IF(nIndex >= nEnd, "vcl.gdi", "StartPos >= EndPos?");
    SAL_WARN_IF(nEnd > static_cast<sal_Int32>(rStr.size()), "vcl.gdi", "String too short");

    // to get the map temporarily set font
    const vcl::Font aOrigFont = GetFont();
    const_cast<OutputDevice&>(*this).SetFont(rTempFont);
    FontCharMapRef xFontCharMap;
    bool bRet = GetFontCharMap(xFontCharMap);
    const_cast<OutputDevice&>(*this).SetFont(aOrigFont);

    // if fontmap is unknown assume it doesn't have the glyphs
    if (!bRet)
        return nIndex;

    for (sal_Int32 i = nIndex; nIndex < nEnd; ++i, ++nIndex)
        if (!xFontCharMap->HasChar(rStr[i]))
            return nIndex;

    return -1;
}

void OutputDevice::ReleaseFontCache() { mpFontController->mxFontCache.reset(); }

void OutputDevice::ReleaseFontCollection() { SetFontCollection(nullptr); }

void OutputDevice::SetFontCollectionFromSVData()
{
    SetFontCollection(ImplGetSVData()->maGDIData.mxScreenFontList->Clone());
}

void OutputDevice::ResetNewFontCache()
{
    mpFontController->mxFontCache = std::make_shared<ImplFontCache>();
}

void OutputDevice::ImplReleaseFonts()
{
    mpGraphics->ReleaseFonts();

    mbNewFont = true;
    mbFontDirty = true;

    mpFontInstance.clear();
    mpForcedFallbackInstance.clear();
    mpFontFaceCollection.reset();
}

tools::Long OutputDevice::GetEmphasisAscent() const
{
    if (mpFontRealization)
        return mpFontRealization->nEmphasisAscent;
    return 0;
}

tools::Long OutputDevice::GetEmphasisDescent() const
{
    if (mpFontRealization)
        return mpFontRealization->nEmphasisDescent;
    return mpFontRealization->nEmphasisDescent;
}

void OutputDevice::AdoptSharedFontCache(const std::shared_ptr<ImplFontCache>& pShared)
{
    if (mpFontController)
        mpFontController->AdoptCache(pShared);
}

void OutputDevice::InvalidateFontCache()
{
    if (mpFontController)
        mpFontController->InvalidateCache();
}

void OutputDevice::ClearFontCache()
{
    if (mpFontController)
        mpFontController->ClearCache();
}

void OutputDevice::ResetFontCache()
{
    if (!mpFontController)
        mpFontController = std::make_unique<vcl::font::FontController>();
    mpFontController->ResetCache();
}

ImplFontCache& OutputDevice::GetFontCache() const
{
    if (!mpFontController)
        const_cast<OutputDevice*>(this)->mpFontController
            = std::make_unique<vcl::font::FontController>();
    return mpFontController->GetCache();
}

rtl::Reference<LogicalFontInstance>
OutputDevice::GetFontInstance(vcl::font::PhysicalFontCollection* pPFC, const vcl::Font& rFont,
                              const Size& rSize, float fHeight) const
{
    return GetFontCache().GetFontInstance(pPFC, rFont, rSize, fHeight);
}

void OutputDevice::AcquireScreenFontCache()
{
    ImplSVData* pSVData = ImplGetSVData();
    AdoptSharedFontCache(pSVData->maGDIData.mxScreenFontCache);
}

bool OutputDevice::IsScreenFontCache() const
{
    ImplSVData* pSVData = ImplGetSVData();
    // Use the safe getter which handles lazy initialization
    return &GetFontCache() == pSVData->maGDIData.mxScreenFontCache.get();
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
