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

#include <sal/log.hxx>
#include <tools/debug.hxx>
#include <tools/mapunit.hxx>
#include <i18nlangtag/mslangid.hxx>
#include <unotools/fontdefs.hxx>
#include <o3tl/unit_conversion.hxx>

#include <vcl/deviceconcepts.hxx>
#include <vcl/fontcapabilities.hxx>
#include <vcl/metafile/MetafileRecorder.hxx>
#include <vcl/metafile/MetaAction.hxx>
#include <vcl/print.hxx>
#include <vcl/rendercontext/AntialiasingFlags.hxx>
#include <vcl/rendercontext/GetDefaultFontFlags.hxx>
#include <vcl/settings.hxx>

#include <window.h>
#include <devicedispatcher.hxx>
#include <drawmode.hxx>
#include <font/EmphasisMark.hxx>
#include <font/FontController.hxx>
#include <font/FontMetricResolver.hxx>
#include <font/LogicalFontInstance.hxx>
#include <font/PhysicalFontCollection.hxx>
#include <font/PhysicalFontFaceCollection.hxx>
#include <sallayout.hxx>
#include <salgdi.hxx>
#include <svdata.hxx>
#include <text/FontMetricEngine.hxx>
#include <text/TextLayoutEngine.hxx>
#include <CoordinateMapper.hxx>
#include <GraphicsState.hxx>

#include <unicode/uchar.h>

const vcl::Font& OutputDevice::GetFont() const { return mpGraphicsState->maFont; }

void OutputDevice::SetFont(const vcl::Font& rNewFont)
{
    vcl::Font aFont
        = vcl::drawmode::GetFont(rNewFont, GetDrawMode(), GetSettings().GetStyleSettings());

    {
        maRecorder.RecordFont(aFont);
        // the color and alignment actions don't belong here
        // TODO: get rid of them without breaking anything...
        maRecorder.RecordTextAlign(aFont.GetAlignment());
        maRecorder.RecordTextFillColor(aFont.GetFillColor(), !aFont.IsTransparent());
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
        maRecorder.RecordTextColor(aFont.GetColor());
    }
    mpGraphicsState->maFont = aFont;
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

FontMetric OutputDevice::GetFontMetricFromCollection(sal_uInt32 nDevFontIndex) const
{
    if (!mpFontController)
        const_cast<OutputDevice*>(this)->mpFontController = std::make_unique<vcl::font::FontController>();

    if (!mpGraphics)
        AcquireGraphics();

    return mpFontController->GetFontMetricFromCollection(mpGraphics, nDevFontIndex);
}

sal_uInt32 OutputDevice::GetFontFaceCollectionCount() const
{
    if (!mpFontController)
        return 0;

    return mpFontController->GetFontFaceCollectionCount();
}

bool OutputDevice::IsFontAvailable(std::u16string_view rFontName) const
{
    InitializeFonts();

    return mpFontController && mpFontController->IsFontAvailable(rFontName);
}

bool OutputDevice::AddTempDevFont(const OUString& rFileURL, const OUString& rFontName) const
{
    if (!mpGraphics && !const_cast<OutputDevice*>(this)->AcquireGraphics())
        return false;
    return mpFontController && mpFontController->AddTempDevFont(mpGraphics, rFileURL, rFontName);
}

bool OutputDevice::RemoveTempDevFont(const OUString& rFileURL, const OUString& rFontName)
{
    if (!mpGraphics && !AcquireGraphics())
        return false;
    return mpFontController && mpFontController->RemoveTempDevFont(mpGraphics, rFileURL, rFontName);
}

bool OutputDevice::GetFontFeatures(std::vector<vcl::font::Feature>& rFontFeatures) const
{
    if (!ImplUpdateFontInstance())
        return false;

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

    if (!ImplUpdateFontInstance())
        return aMetric;

    aMetric = mpGraphicsState->maFont;
    aMetric.SetAlignment(TextAlign::ALIGN_TOP);

    mpFontController->PopulateFontMetric(
        aMetric, mpGraphicsState->maFont, mpFontRealization->mxFont.get(),
        mpFontRealization->nEmphasisAscent, mpFontRealization->nEmphasisDescent);

    mpMapper->PixelToLogic(aMetric, GetFontExtLeading());

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
    if (!mpGraphics && !const_cast<OutputDevice*>(this)->AcquireGraphics())
        return false;
    return mpFontController && mpFontController->GetFontCapabilities(mpGraphics, rFontCapabilities);
}

tools::Long OutputDevice::GetFontExtLeading() const
{
    if (mpFontRealization && mpFontRealization->mxFont)
        return mpFontRealization->mxFont->mxFontMetric->GetExternalLeading();

    return 0;
}

void OutputDevice::ImplClearFontData(const bool bNewFontLists)
{
    // the currently selected logical font is no longer needed
    mpFontInstance.clear();

    if (mpFontController)
        mpFontController->ResetGraphicsState();

    if (bNewFontLists)
        mpFontFaceCollection.reset();

    SalGraphics* pGraphics = nullptr;
    if (AcquireGraphics())
        pGraphics = mpGraphics;

    mpFontController->ClearFontResources(pGraphics, bNewFontLists);
}

void OutputDevice::RefreshFontData(const bool bNewFontLists) { ImplRefreshFontData(bNewFontLists); }

void OutputDevice::ImplRefreshFontData(const bool bNewFontLists)
{
    if (bNewFontLists && AcquireGraphics())
        mpFontController->RefreshFromGraphics(mpGraphics);
}

void OutputDevice::ImplUpdateFontData()
{
    SalGraphics* pGraphics = nullptr;
    if (AcquireGraphics())
        pGraphics = mpGraphics;

    mpFontController->UpdateFontData(pGraphics, true);
}

void OutputDevice::ImplClearFontsOnAllFrames(bool bNewFontLists)
{
    ImplUpdateFontDataForAllFrames(&OutputDevice::ImplClearFontData, bNewFontLists);
}

void OutputDevice::ImplClearAllFontData(bool bNewFontLists)
{
    // Forwarding for legacy calls
    vcl::font::FontController::ClearAllFontData(bNewFontLists);
}

void OutputDevice::ImplRefreshFontsOnAllFrames(bool bNewFontLists)
{
    ImplUpdateFontDataForAllFrames(&OutputDevice::ImplRefreshFontData, bNewFontLists);
}

void OutputDevice::ImplRefreshAllFontData(bool bNewFontLists)
{
     // Forwarding for legacy calls
     vcl::font::FontController::RefreshAllFontData(bNewFontLists);
}

void OutputDevice::ImplUpdateFirstFrameGraphics()
{
    ImplSVData* pSVData = ImplGetSVData();
    vcl::Window* pFrame = pSVData->maFrameData.mpFirstFrame;
    if (!pFrame)
        return;

    if (pFrame->GetOutDev()->AcquireGraphics())
    {
        OutputDevice* pDevice = pFrame->GetOutDev();

        pDevice->mpFontController->UpdateSystemFontList(pDevice->mpGraphics);}
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

void OutputDevice::BeginFontSubstitution() { vcl::font::FontController::BeginFontSubstitution(); }

void OutputDevice::EndFontSubstitution() { vcl::font::FontController::EndFontSubstitution(); }

void OutputDevice::AddFontSubstitute(const OUString& rFontName, const OUString& rReplaceFontName, AddFontSubstituteFlags nFlags)
{
    vcl::font::FontController::AddFontSubstitute(rFontName, rReplaceFontName, nFlags);
}

void OutputDevice::RemoveFontsSubstitute() { vcl::font::FontController::RemoveFontsSubstitute(); }

//hidpi TODO: This routine has hard-coded font-sizes that break places such as DialControl
vcl::Font OutputDevice::GetDefaultFont(DefaultFontType nType, LanguageType eLang,
                                       GetDefaultFontFlags nFlags, const OutputDevice* pOutDev)
{
    return vcl::font::FontController::GetDefaultFont(nType, eLang, nFlags, pOutDev);
}

void OutputDevice::InitializeFonts() const
{
    if (mpFontController)
        mpFontController->InitializeFonts(mpGraphics);
}

bool OutputDevice::InitFont() const
{
    DBG_TESTSOLARMUTEX();

    if (!mpFontController)
        const_cast<OutputDevice*>(this)->mpFontController = std::make_unique<vcl::font::FontController>();

    if (mpFontController->NeedsUpdate(mpGraphicsState->maFont, *mpMapper))
    {
        if (!const_cast<OutputDevice*>(this)->ImplUpdateFontInstance())
            return false;
    }

    LogicalFontInstance* pFontToUse = nullptr;

    if (mpFontRealization && mpFontRealization->mxFont)
        pFontToUse = mpFontRealization->mxFont.get();
    else if (mpFontInstance)
        pFontToUse = mpFontInstance.get();

    if (!pFontToUse)
        return false;

    if (!mpGraphics)
    {
        if (!const_cast<OutputDevice*>(this)->AcquireGraphics())
            return false;
    }

    if (mpGraphics && mpFontController->ActivateFontOnDevice(mpGraphics, pFontToUse))
        return true;

    return false;
}

const LogicalFontInstance* OutputDevice::GetFontInstance() const
{
    if (!InitFont())
        return nullptr;

    if (mpFontRealization && mpFontRealization->mxFont)
        return mpFontRealization->mxFont.get();

    return mpFontInstance.get();
}

bool OutputDevice::ImplUpdateFontInstance() const
{
    DBG_TESTSOLARMUTEX();

    if (!mpGraphics && !const_cast<OutputDevice*>(this)->AcquireGraphics())
    {
        SAL_WARN("vcl.gdi", "ImplUpdateFontInstance: no Graphics, no Font");
        return false;
    }
    assert(mpGraphics);

    // Create the callback to handle the initialization step
    // This captures 'this' safely because ImplUpdateFontInstance is a member function
    auto fnInit = [this](LogicalFontInstance* pInstance) {
        this->ImplInitializeFontInstance(pInstance);
    };

    bool bRet = mpFontController->UpdateFontInstanceState(
        mpGraphics,
        *mpMapper,
        mpGraphicsState->maFont,
        mpFontRealization,
        mpFontInstance,
        GetDPIY(),
        GetAntialiasing(),
        GetSettings().GetStyleSettings(),
        fnInit
    );

    if (bRet && mpFontRealization)
        mpFontRealization->eLayoutMode = mpGraphicsState->mnTextLayoutMode;

    return bRet;
}

void OutputDevice::ImplInitializeFontInstance(LogicalFontInstance* pFontInstance) const
{
    if (!pFontInstance->mbInit && InitFont())
    {
        // Compute font size in points for optical sizing before HarfBuzz initialization
        if (!pFontInstance->GetPointSize())
        {
            auto nHeight = mpGraphicsState->maFont.GetFontHeight();
            auto eFrom = MapToO3tlLength(GetMapMode().GetMapUnit());
            float fPointSize = o3tl::convert(float(nHeight), eFrom, o3tl::Length::pt);
            pFontInstance->SetPointSize(fPointSize);
        }

        mpFontController->InitializeInstance(pFontInstance, mpGraphics);

        ImplInitFontMetrics(pFontInstance);

        vcl::font::DeviceFontCapabilities aCaps;
        vcl::DispatchDevice(*this, [&aCaps](auto& rDev) {
            using DevType = std::decay_t<decltype(rDev)>;
            aCaps.bSupportsGlyphSynthesis = vcl::GlyphSynthesisCapable<DevType>;
        });

        vcl::font::FontMetricResolver::ResolveMetrics(aCaps, pFontInstance);
    }
}

void OutputDevice::ImplInitFontMetrics(LogicalFontInstance* pFontInstance) const
{
    auto fnMeasureWidth = [this](const OUString& rStr) -> tools::Long {
        return GetTextWidth(rStr);
    };

    auto fnMeasureRect = [this](tools::Rectangle& rRect, const OUString& rStr) {
        GetLogicalTextBoundRect(rRect, rStr);
    };

    tools::Long nDPIY = GetDPIY();
    tools::Long nPixelWidth = LogicToPixel(Size(1, 0)).Width();

    vcl::text::FontMetricEngine::InitializeFontMetrics(
        pFontInstance, GetFont(), nDPIY, nPixelWidth, fnMeasureWidth, fnMeasureRect);

    pFontInstance->mnLineHeight
        = pFontInstance->mxFontMetric->GetAscent() + pFontInstance->mxFontMetric->GetDescent();

    pFontInstance->mbInit = true;
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

tools::Long OutputDevice::GetMinKashida() const
{
    if (!ImplUpdateFontInstance())
        return 0;

    double nKashidaWidth = mpFontController->GetMinKashidaWidth(mpFontRealization->mxFont.get());

    if (!mpMapper->IsMapModeEnabled())
        nKashidaWidth = std::ceil(nKashidaWidth);

    return mpMapper->DevicePixelToLogicWidth(nKashidaWidth);
}

sal_Int32 OutputDevice::HasGlyphs(const vcl::Font& rTempFont, std::u16string_view rStr,
                                  sal_Int32 nIndex, sal_Int32 nLen) const
{
    if (nIndex >= static_cast<sal_Int32>(rStr.size())) return nIndex;
    sal_Int32 nEnd = (nLen == -1) ? rStr.size() : std::min<sal_Int32>(rStr.size(), nIndex + nLen);

    const vcl::Font aOrigFont = GetFont();
    const_cast<OutputDevice&>(*this).SetFont(rTempFont);
    FontCharMapRef xFontCharMap;
    bool bRet = GetFontCharMap(xFontCharMap);
    const_cast<OutputDevice&>(*this).SetFont(aOrigFont);

    if (!bRet) return nIndex;
    for (sal_Int32 i = nIndex; nIndex < nEnd; ++i, ++nIndex)
    {
        if (!xFontCharMap->HasChar(rStr[i]))
            return nIndex;
    }

    return -1;
}

void OutputDevice::ReleaseFontCache()
{
    if (mpFontController)
        mpFontController->mxFontCache.reset();
}

void OutputDevice::ReleaseFontCollection()
{
    SetFontCollection(nullptr);
}

void OutputDevice::SetFontCollectionFromSVData()
{
    SetFontCollection(ImplGetSVData()->maGDIData.mxScreenFontList->Clone());
}

void OutputDevice::ResetNewFontCache()
{
    if (mpFontController)
        mpFontController->mxFontCache = std::make_shared<ImplFontCache>();
}

namespace vcl
{
    template <typename T>
    struct font_resource_policy
    {
        static void release(OutputDevice& rDev)
        {
            if constexpr (ManagedFontCache<T>)
            {
                if (rDev.mpFontController)
                    rDev.mpFontController->ClearAllCache(rDev.mpGraphics);

                rDev.mpForcedFallbackInstance.clear();
            }
            else // This path is for Printer and non-managed devices
            {
                rDev.mpFontInstance.clear();
            }
        }
    };
}

void OutputDevice::ImplReleaseFonts()
{
    // Clear the device's local metadata reference immediately
    mpFontFaceCollection.reset();

    if (mpGraphics)
        mpGraphics->ReleaseFonts();

    // Resolve the concrete type (Printer&, VirtualDevice&, etc.) and execute policy
    vcl::DispatchDevice(*this, [](auto& rConcreteDev) {
        using DeviceT = std::decay_t<decltype(rConcreteDev)>;
        vcl::font_resource_policy<DeviceT>::release(rConcreteDev);
    });
}

tools::Long OutputDevice::GetEmphasisAscent() const
{
    return mpFontRealization ? mpFontRealization->nEmphasisAscent : 0;
}

tools::Long OutputDevice::GetEmphasisDescent() const
{
    return mpFontRealization ? mpFontRealization->nEmphasisDescent : 0;
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
        const_cast<OutputDevice*>(this)->mpFontController = std::make_unique<vcl::font::FontController>();

    return mpFontController->GetCache();
}

rtl::Reference<LogicalFontInstance> OutputDevice::GetFontInstance(vcl::font::PhysicalFontCollection* pPFC, const vcl::Font& rFont, const Size& rSize, float fHeight) const
{
    return GetFontCache().GetFontInstance(pPFC, rFont, rSize, fHeight);
}

void OutputDevice::AcquireScreenFontCache()
{
    AdoptSharedFontCache(ImplGetSVData()->maGDIData.mxScreenFontCache);
}

bool OutputDevice::IsScreenFontCache() const
{
    return &GetFontCache() == ImplGetSVData()->maGDIData.mxScreenFontCache.get();
}

Color OutputDevice::GetReadableFontColor(const Color& rFontColor, const Color& rBgColor) const
{
    Color aColor = rFontColor;

    vcl::DispatchDevice(*this, [&aColor, &rFontColor, &rBgColor](auto& rDev) {
        using DevType = std::decay_t<decltype(rDev)>;

        if constexpr (vcl::SubtractiveColorDevice<DevType>)
        {
            // In a subtractive model (Ink on Paper), we force black to
            // ensure the sharpest contrast against the reflective canvas.
            aColor = COL_BLACK;
        }
        else
        {
            // Standard additive heuristic for emissive screens:
            if (rBgColor.IsDark() && rFontColor.IsDark())
                aColor = COL_WHITE;
            else if (rBgColor.IsBright() && rFontColor.IsBright())
                aColor = COL_BLACK;
            else
                aColor = rFontColor;
        }
    });

    return aColor;
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
