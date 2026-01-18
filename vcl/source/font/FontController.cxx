/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <osl/thread.h>
#include <unotools/fontdefs.hxx>
#include <comphelper/configuration.hxx>
#include <i18nlangtag/mslangid.hxx>

#include <vcl/event.hxx>
#include <vcl/font.hxx>
#include <vcl/fntstyle.hxx>
#include <vcl/metric.hxx>
#include <vcl/outdev.hxx>
#include <vcl/rendercontext/AntialiasingFlags.hxx>
#include <vcl/rendercontext/GetDefaultFontFlags.hxx>
#include <vcl/settings.hxx>
#include <vcl/svapp.hxx>

#include <CoordinateMapper.hxx>
#include <FontController.hxx>
#include <font/DirectFontSubstitution.hxx>
#include <font/FeatureCollector.hxx>
#include <font/LogicalFontInstance.hxx>
#include <font/PhysicalFontCollection.hxx>
#include <font/PhysicalFontFaceCollection.hxx>
#include <font/PhysicalFontFace.hxx>
#include <impfontcache.hxx>
#include <salgdi.hxx>
#include <svdata.hxx>

#include <tuple>

namespace vcl::font
{
FontController::FontController()
    : mxFontInstance(nullptr)
    , mxFontCache(std::make_shared<ImplFontCache>())
    , meTextAlign(TextAlign::ALIGN_TOP)
{
}

void FontController::InitializeFonts(SalGraphics* pGraphics)
{
    if (!pGraphics)
        return;

    if (!mxFontCollection)
        mxFontCollection = std::make_shared<PhysicalFontCollection>();

    if (mxFontCollection->Count() == 0)
        pGraphics->GetDevFontList(mxFontCollection.get());
}

bool FontController::NeedsUpdate(const vcl::Font& rFont, bool bNewFont) const
{
    if (bNewFont || !mxFontInstance)
        return true;

    const vcl::font::FontSelectPattern& rCurrentPattern = mxFontInstance->GetFontSelectPattern();

    if (rCurrentPattern.maTargetName != rFont.GetFamilyName())
        return true;

    if (rCurrentPattern.mnHeight != rFont.GetFontHeight())
        return true;

    return false;
}

bool FontController::ShouldDisableAntialiasing(AntialiasingFlags eAntialisingFlags,
                                               const StyleSettings& rStyleSettings,
                                               tools::Long nHeight) const
{
    bool bNonAntialiased(eAntialisingFlags & AntialiasingFlags::DisableText);

    if (!comphelper::IsFuzzing())
    {
        bNonAntialiased |= bool(rStyleSettings.GetDisplayOptions() & DisplayOptions::AADisable);
        bNonAntialiased |= (int(rStyleSettings.GetAntialiasingMinPixelHeight()) > nHeight);
    }

    return bNonAntialiased;
}

std::pair<float, Size> FontController::CalculateDeviceSize(const vcl::Font& rFont,
                                                           const CoordinateMapper& rMapper,
                                                           tools::Long nDPIY) const
{
    float fExactHeight = rMapper.LogicHeightToDeviceSubPixel(rFont.GetFontHeight());

    Size aSize = rMapper.LogicToDevicePixel(rFont.GetFontSize());

    if (!aSize.Height())
    {
        if (rFont.GetFontSize().Height())
            aSize.setHeight(1);
        else
            aSize.setHeight((12 * nDPIY) / 72);

        fExactHeight = static_cast<float>(aSize.Height());
    }

    if (aSize.Width() == 0 && rFont.GetFontSize().Width() != 0)
        aSize.setWidth(1);

    return std::make_pair(fExactHeight, aSize);
}

void FontController::InitializeInstance(LogicalFontInstance* pFontInstance, SalGraphics* pGraphics)
{
    if (!pFontInstance || !pGraphics)
        return;

    // Guard: only initialize if not already done
    if (pFontInstance->mbInit)
        return;

    pFontInstance->mbInit = true;

    pFontInstance->mxFontMetric->SetOrientation(
        pFontInstance->GetFontSelectPattern().mnOrientation);

    pGraphics->GetFontMetric(pFontInstance->mxFontMetric, 0);
}

std::tuple<tools::Long, tools::Long, tools::Long, tools::Long>
FontController::CalculateTextOffsets(const vcl::Font& rFont,
                                     const LogicalFontInstance* pFontInstance) const
{
    if (!pFontInstance)
        return std::make_tuple(0, 0, 0, 0);

    tools::Long nEmphasisAscent = 0;
    tools::Long nEmphasisDescent = 0;
    tools::Long nVerticalOffset = 0;
    tools::Long nHorizontalOffset = 0;

    if (rFont.GetEmphasisMark() & FontEmphasisMark::Style)
    {
        FontEmphasisMark nEmphasisMark = rFont.GetEmphasisMarkStyle();
        tools::Long nEmphasisHeight = (pFontInstance->mnLineHeight * 250) / 1000;

        if (nEmphasisHeight < 1)
            nEmphasisHeight = 1;

        if (nEmphasisMark & FontEmphasisMark::PosBelow)
            nEmphasisDescent = nEmphasisHeight;
        else
            nEmphasisAscent = nEmphasisHeight;
    }

    TextAlign eAlign = rFont.GetAlignment();

    if (eAlign == ALIGN_TOP)
        nVerticalOffset = pFontInstance->mxFontMetric->GetAscent() + nEmphasisAscent;
    else if (eAlign == ALIGN_BOTTOM)
        nVerticalOffset = -pFontInstance->mxFontMetric->GetDescent() + nEmphasisDescent;

    if (pFontInstance->mnOrientation && (nHorizontalOffset || nVerticalOffset))
    {
        Point aOriginPt(0, 0);
        aOriginPt.RotateAround(nHorizontalOffset, nVerticalOffset, pFontInstance->mnOrientation);
    }

    return std::make_tuple(nHorizontalOffset, nVerticalOffset, nEmphasisAscent, nEmphasisDescent);
}

std::tuple<bool, bool> FontController::GetTextLayoutFlags(const vcl::Font& rFont) const
{
    // Determine if any text lines (underline, overline, strikeout) are active
    bool bTextLines
        = ((rFont.GetUnderline() != LINESTYLE_NONE) && (rFont.GetUnderline() != LINESTYLE_DONTKNOW))
          || ((rFont.GetOverline() != LINESTYLE_NONE)
              && (rFont.GetOverline() != LINESTYLE_DONTKNOW))
          || ((rFont.GetStrikeout() != STRIKEOUT_NONE)
              && (rFont.GetStrikeout() != STRIKEOUT_DONTKNOW));

    // Determine if special effects (shadow, outline, relief) are active
    bool bTextSpecial
        = rFont.IsShadow() || rFont.IsOutline() || (rFont.GetRelief() != FontRelief::NONE);

    return std::make_tuple(bTextLines, bTextSpecial);
}

int FontController::CalculateOLEStorageWidth(const LogicalFontInstance* pFontInstance,
                                             tools::Long nMapXNum, tools::Long nMapXDen,
                                             tools::Long nMapYNum, tools::Long nMapYDen) const
{
    if (!pFontInstance)
        return 0;

    const float fDenominator = static_cast<float>(nMapYNum) * nMapXDen;
    if (fDenominator == 0.0)
        return 0;

    const float fNumerator = static_cast<float>(nMapXNum) * nMapYDen;
    const float fStretch = fNumerator / fDenominator;

    const int nOrigWidth = pFontInstance->mxFontMetric->GetWidth();
    const int nNewWidth = static_cast<int>(nOrigWidth * fStretch + 0.5);

    if (nNewWidth == nOrigWidth || nNewWidth == 0)
        return 0;

    return nNewWidth;
}

void FontController::PopulateFontMetric(FontMetric& rMetric, const vcl::Font& rLogicalFont,
                                        const LogicalFontInstance* pFontInstance,
                                        tools::Long nEmphasisAscent,
                                        tools::Long nEmphasisDescent) const
{
    if (!pFontInstance)
        return;

    FontMetricDataRef xFontMetric = pFontInstance->mxFontMetric;

    // Set basic font identity and style info
    rMetric.SetFamilyName(rLogicalFont.GetFamilyName());
    rMetric.SetStyleName(xFontMetric->GetStyleName());
    rMetric.SetCharSet(xFontMetric->IsMicrosoftSymbolEncoded() ? RTL_TEXTENCODING_SYMBOL
                                                               : RTL_TEXTENCODING_UNICODE);
    rMetric.SetFamily(xFontMetric->GetFamilyType());
    rMetric.SetPitch(xFontMetric->GetPitch());
    rMetric.SetWeight(xFontMetric->GetWeight());
    rMetric.SetItalic(xFontMetric->GetItalic());
    rMetric.SetWidthType(xFontMetric->GetWidthType());

    // Calculate logical pixel size
    rMetric.SetFontSize(Size(xFontMetric->GetWidth(), xFontMetric->GetAscent()
                                                          + xFontMetric->GetDescent()
                                                          - xFontMetric->GetInternalLeading()));

    // Handle orientation: own orientation takes precedence over metric orientation
    if (pFontInstance->mnOwnOrientation)
        rMetric.SetOrientation(pFontInstance->mnOwnOrientation);
    else
        rMetric.SetOrientation(xFontMetric->GetOrientation());

    // Set core metrics including emphasis mark adjustments
    rMetric.SetAscent(xFontMetric->GetAscent() + nEmphasisAscent);
    rMetric.SetDescent(xFontMetric->GetDescent() + nEmphasisDescent);
    rMetric.SetInternalLeading(xFontMetric->GetInternalLeading() + nEmphasisAscent);
    rMetric.SetLineHeight(xFontMetric->GetAscent() + xFontMetric->GetDescent() + nEmphasisAscent
                          + nEmphasisDescent);

    // Miscellaneous metrics
    rMetric.SetFullstopCenteredFlag(xFontMetric->IsFullstopCentered());
    rMetric.SetBulletOffset(xFontMetric->GetBulletOffset());
    rMetric.SetSlant(xFontMetric->GetSlant());
    rMetric.SetHangingBaseline(xFontMetric->GetHangingBaseline());
    rMetric.SetUnitEm(xFontMetric->GetUnitEm());
    rMetric.SetHorCJKAdvance(xFontMetric->GetHorCJKAdvance());
    rMetric.SetVertCJKAdvance(xFontMetric->GetVertCJKAdvance());
    rMetric.SetQuality(xFontMetric->GetQuality());
}

double FontController::GetMinKashidaWidth(const LogicalFontInstance* pFontInstance) const
{
    if (!pFontInstance || !pFontInstance->mxFontMetric)
        return 0.0;

    return pFontInstance->mxFontMetric->GetMinKashida();
}

bool FontController::GetFontCapabilities(SalGraphics* pGraphics,
                                         vcl::FontCapabilities& rFontCapabilities) const
{
    if (!pGraphics)
        return false;

    return pGraphics->GetFontCapabilities(rFontCapabilities);
}

FontCharMapRef FontController::GetFontCharMap(SalGraphics* pGraphics) const
{
    if (!pGraphics)
        return FontCharMapRef(new FontCharMap());

    FontCharMapRef xFontCharMap(pGraphics->GetFontCharMap());

    if (!xFontCharMap.is())
        xFontCharMap = FontCharMapRef(new FontCharMap());

    return xFontCharMap;
}

void FontController::GetFontFeatures(const LogicalFontInstance* pFontInstance,
                                     std::vector<vcl::font::Feature>& rFontFeatures) const
{
    if (!pFontInstance)
        return;

    // Determine the UI language for feature name localization
    const LanguageTag& rOfficeLanguage = Application::GetSettings().GetUILanguageTag();

    // Use the FeatureCollector to query the physical font face
    vcl::font::FeatureCollector aFeatureCollector(pFontInstance->GetFontFace(), rFontFeatures,
                                                  rOfficeLanguage);
    aFeatureCollector.collect();
}

rtl::Reference<LogicalFontInstance>
FontController::RealizeFont(vcl::font::PhysicalFontCollection* pColl, const vcl::Font& rFont,
                            const SalGraphics* pGraphics, const Size& rSize, float fExactHeight,
                            bool bNonAntialiased)
{
    vcl::Font aFontToUse = rFont;
    vcl::Font aSubstFont;
    OUString aMissingCodes;

    if (GetFontSubstitution(pGraphics, rFont, aMissingCodes, aSubstFont))
        aFontToUse = aSubstFont;

    if (!mxFontCache)
        return nullptr;

    return mxFontCache->GetFontInstance(pColl, aFontToUse, rSize, fExactHeight, bNonAntialiased);
}

void FontController::InvalidateCache()
{
    if (mxFontCache)
        mxFontCache->Invalidate();
}

bool FontController::NeedsOLEFontScaleFix(const CoordinateMapper& rMapper, const Size& rSize) const
{
    // #95414# Fix for OLE objects which use scale factors creatively
    return rMapper.IsMapModeEnabled() && !rSize.Width();
}

Size FontController::GetOLECorrectedSize(const CoordinateMapper& rMapper, const Size& rSize,
                                         tools::Long nHeight) const
{
    Size aNewSize = rSize;

    // Formula: Width = Height * (ScaleX / ScaleY)
    //        = Height * (XNum * YDen) / (XDen * YNum)

    tools::Long nXNum = rMapper.GetMappingXNumerator();
    tools::Long nXDen = rMapper.GetMappingXDenominator();
    tools::Long nYNum = rMapper.GetMappingYNumerator();
    tools::Long nYDen = rMapper.GetMappingYDenominator();

    // Check if the aspect ratio is distorted (non-uniform scaling)
    if ((nXNum * nYDen) != (nYNum * nXDen))
    {
        if (nXDen != 0 && nYNum != 0)
        {
            // Use long long to prevent overflow
            double fScaleX = static_cast<double>(nXNum) / nXDen;
            double fScaleY = static_cast<double>(nYNum) / nYDen;

            // Calculate width based on the ratio of the scales to ensure precision
            tools::Long nWidth
                = static_cast<tools::Long>(std::round(nHeight * (fScaleX / fScaleY)));

            // Safety clamp: if we have height, we must have width
            if (nWidth == 0 && nHeight > 0)
                nWidth = 1;

            aNewSize.setWidth(nWidth);
        }
    }
    return aNewSize;
}

bool FontController::IsFontAvailable(std::u16string_view rFontName) const
{
    if (!mxFontCollection)
        return false;

    return mxFontCollection->FindFontFamily(rFontName) != nullptr;
}

bool FontController::GetFontSubstitution(const SalGraphics* /*pGraphics*/, const vcl::Font& rFont,
                                         OUString& /*rMissingCodes*/,
                                         vcl::Font& /*rSubstFont*/) const
{
    if (!mxFontCollection)
        return false;

    if (mxFontCollection->FindFontFamily(rFont.GetFamilyName()))
        return false;

    return false;
}

bool FontController::AddTempDevFont(SalGraphics* pGraphics, const OUString& rFileURL,
                                    const OUString& rFontName)
{
    if (pGraphics && mxFontCollection)
        return pGraphics->AddTempDevFont(mxFontCollection.get(), rFileURL, rFontName);
    return false;
}

bool FontController::RemoveTempDevFont(SalGraphics* pGraphics, const OUString& rFileURL,
                                       const OUString& rFontName)
{
    if (pGraphics)
        return pGraphics->RemoveTempDevFont(rFileURL, rFontName);
    return false;
}

sal_uInt32 FontController::GetFontFaceCollectionCount() const
{
    if (!mxFontCollection)
        return 0;

    return mxFontCollection->Count();
}

bool FontController::ActivateFontOnDevice(SalGraphics* pGraphics,
                                          LogicalFontInstance* pFontInstance)
{
    if (!pGraphics || !pFontInstance)
        return false;

    // The Controller now owns the hardware interaction
    pGraphics->SetFont(pFontInstance, 0);
    return true;
}

FontMetric FontController::GetFontMetricFromCollection(SalGraphics* pGraphics,
                                                       size_t nDevFontIndex) const
{
    if (!mxFontCollection)
        const_cast<FontController*>(this)->InitializeFonts(pGraphics);

    if (mxFontCollection)
    {
        std::unique_ptr<vcl::font::PhysicalFontFaceCollection> pFaces
            = mxFontCollection->GetFontFaceCollection();

        if (pFaces && nDevFontIndex < static_cast<sal_uInt32>(pFaces->Count()))
        {
            const vcl::font::PhysicalFontFace* pFace = pFaces->Get(nDevFontIndex);

            if (pFace)
            {
                FontMetric aMetric;

                // Populate identity from PhysicalFontFace/FontAttributes
                // Use the attributes provided by the face
                aMetric.SetFamilyName(pFace->GetFamilyName());
                aMetric.SetStyleName(pFace->GetStyleName());
                aMetric.SetWeight(pFace->GetWeight());
                aMetric.SetItalic(pFace->GetItalic());
                aMetric.SetWidthType(pFace->GetWidthType());

                // Since this is a "Face" in a collection without a specific size,
                // height-based metrics (Ascent/Descent) are often 0 or
                // default until a LogicalFontInstance is created.

                return aMetric;
            }
        }
    }

    return FontMetric();
}

void FontController::RefreshFromGraphics(SalGraphics* pGraphics)
{
    if (pGraphics)
        pGraphics->GetDevFontList(GetFontCollection());
}

void FontController::ClearFontResources(SalGraphics* pGraphics, bool bNewFontLists)
{
    mxFontInstance.clear();

    if (bNewFontLists && pGraphics)
        pGraphics->ReleaseFonts();

    InvalidateCache();

    if (bNewFontLists && pGraphics)
    {
        ImplSVData* pSVData = ImplGetSVData();
        if (GetSharedFontCollection()
            && GetSharedFontCollection() != pSVData->maGDIData.mxScreenFontList)
            GetSharedFontCollection()->Clear();
    }
}

void FontController::UpdateFontData(SalGraphics* pGraphics, bool bNewFontLists)
{
    ClearFontResources(pGraphics, bNewFontLists);
    RefreshFromGraphics(pGraphics);
}

void FontController::BeginFontSubstitution()
{
    ImplSVData* pSVData = ImplGetSVData();
    pSVData->maGDIData.mbFontSubChanged = false;
}

void FontController::EndFontSubstitution()
{
    ImplSVData* pSVData = ImplGetSVData();
    if (pSVData->maGDIData.mbFontSubChanged)
    {
        OutputDevice::ImplUpdateAllFontData(false);

        DataChangedEvent aDCEvt(DataChangedEventType::FONTSUBSTITUTION);
        Application::ImplCallEventListenersApplicationDataChanged(&aDCEvt);
        Application::NotifyAllWindows(aDCEvt);
        pSVData->maGDIData.mbFontSubChanged = false;
    }
}

void FontController::AddFontSubstitute(const OUString& rFontName, const OUString& rReplaceFontName,
                                       AddFontSubstituteFlags nFlags)
{
    vcl::font::DirectFontSubstitution*& rpSubst = ImplGetSVData()->maGDIData.mpDirectFontSubst;
    if (!rpSubst)
        rpSubst = new vcl::font::DirectFontSubstitution;
    rpSubst->AddFontSubstitute(rFontName, rReplaceFontName, nFlags);
    ImplGetSVData()->maGDIData.mbFontSubChanged = true;
}

void FontController::RemoveFontsSubstitute()
{
    vcl::font::DirectFontSubstitution* pSubst = ImplGetSVData()->maGDIData.mpDirectFontSubst;
    if (pSubst)
        pSubst->RemoveFontsSubstitute();
}

void FontController::UpdateAllFontData(bool bNewFontLists)
{
    ClearAllFontData(bNewFontLists);
    RefreshAllFontData(bNewFontLists);
}

void FontController::ClearAllFontData(bool bNewFontLists)
{
    OutputDevice::ImplClearFontsOnAllFrames(bNewFontLists);

    // Clear global font lists
    ImplSVData* pSVData = ImplGetSVData();
    pSVData->maGDIData.mxScreenFontCache->Invalidate();
    if (bNewFontLists && pSVData->maGDIData.mxScreenFontList)
        pSVData->maGDIData.mxScreenFontList->Clear();

    // Update first frame (fetch new list)
    if (bNewFontLists)
        OutputDevice::ImplUpdateFirstFrameGraphics();
}

void FontController::RefreshAllFontData(bool bNewFontLists)
{
    OutputDevice::ImplRefreshFontsOnAllFrames(bNewFontLists);
}

vcl::Font FontController::GetDefaultFont(DefaultFontType nType, LanguageType eLang,
                                         GetDefaultFontFlags nFlags, const OutputDevice* pOutDev)
{
    // Implementation moved from OutputDevice
    static bool bFuzzing = comphelper::IsFuzzing();
    static bool bAbortOnFontSubstitute = [] {
        const char* pEnv = getenv("SAL_NON_APPLICATION_FONT_USE");
        return pEnv && strcmp(pEnv, "abort") == 0;
    }();

    if (!pOutDev && !bFuzzing)
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
            aSearch = rDefaults.getUserInterfaceFont(aLanguageTag);

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
            aFont.SetFamily(FAMILY_SYSTEM);
            break;
    }

    if (!aSearch.isEmpty())
    {
        aFont.SetFontHeight(12);
        aFont.SetWeight(WEIGHT_NORMAL);
        aFont.SetLanguage(eLang);

        if (aFont.GetCharSet() == RTL_TEXTENCODING_DONTKNOW)
            aFont.SetCharSet(osl_getThreadTextEncoding());

        if (pOutDev)
        {
            pOutDev->ImplInitFontList();
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

        if (aFont.GetFamilyName().isEmpty())
        {
            if (nFlags & GetDefaultFontFlags::OnlyOne)
            {
                if (!pOutDev)
                {
                    SAL_WARN_IF(!comphelper::IsFuzzing(), "vcl.gdi", "No default window set");
                    aFont.SetFamilyName(aSearch.getToken(0, ';'));
                }
                else
                {
                    pOutDev->ImplInitFontList();
                    aFont.SetFamilyName(aSearch);
                    Size aSize = pOutDev->LogicToDevicePixel(aFont.GetFontSize());
                    if (!aSize.Height())
                    {
                        if (aFont.GetFontHeight())
                            aSize.setHeight(1);
                        else
                            aSize.setHeight((12 * pOutDev->GetDPIY()) / 72);
                    }
                    if ((0 == aSize.Width()) && (0 != aFont.GetFontSize().Width()))
                        aSize.setWidth(1);

                    float fExactHeight = static_cast<float>(aSize.Height());
                    rtl::Reference<LogicalFontInstance> pFontInstance
                        = pOutDev->mpFontController->RealizeFont(pOutDev->GetFontCollection(),
                                                                 aFont, pOutDev->GetGraphics(),
                                                                 aSize, fExactHeight, false);
                    if (pFontInstance)
                        aFont.SetFamilyName(pFontInstance->GetFontFace()->GetFamilyName());
                }
            }
            else
                aFont.SetFamilyName(aSearch);
        }
    }
    return aFont;
}

} // end namespace vcl::font

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
