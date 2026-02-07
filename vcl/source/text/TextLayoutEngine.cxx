/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <basegfx/point/b2dpoint.hxx>
#include <tools/gen.hxx>
#include <unotools/fontdefs.hxx>
#include <tools/lineend.hxx>
#include <i18nlangtag/mslangid.hxx>
#include <i18nutil/digitlocalization.hxx>
#include <i18nutil/unicode.hxx>

#include <vcl/outdev.hxx>
#include <vcl/fntstyle.hxx>
#include <vcl/glyphitem.hxx>
#include <vcl/font.hxx>
#include <vcl/vclenum.hxx>
#include <vcl/svapp.hxx>
#include <vcl/mnemonic.hxx>

#include <font/FontMetricData.hxx>
#include <font/FontController.hxx>
#include <font/LogicalFontInstance.hxx>
#include <font/FontSelectPattern.hxx>
#include <font/PhysicalFontFace.hxx>
#include <salgdi.hxx>
#include <sallayout.hxx>
#include <textlayout.hxx>
#include <textlineinfo.hxx>
#include <text/TextLayoutEngine.hxx>
#include <text/TextAnalyzer.hxx>
#include <vcl/text/DefaultFallbackStrategy.hxx>
#include <CoordinateMapper.hxx>
#include <GraphicsState.hxx>
#include <text/TextLayoutRequest.hxx>

#include <unicode/uchar.h>

#include <cstdio>
#include <utility>

namespace vcl::text
{
void TextLayoutEngine::GetWordKashidaPositions(const SalLayout& rLayout, std::u16string_view rText,
                                               std::vector<bool>& rOutMap)
{
    rOutMap.clear();
    if (!rLayout.HasFontKashidaPositions())
        return;

    size_t nEnd = rText.length();
    rOutMap.resize(nEnd, false);

    for (size_t i = 0; i < nEnd; ++i)
    {
        size_t nNextPos = i + 1;
        while (nNextPos < nEnd
               && u_getIntPropertyValue(rText[nNextPos], UCHAR_JOINING_TYPE) == U_JT_TRANSPARENT)
            ++nNextPos;

        rOutMap[i] = rLayout.IsKashidaPosValid(i, nNextPos);
    }
}

void TextLayoutEngine::InitializeFontMetrics(
    const LogicalFontInstance* pFontInstance, const vcl::Font& rFont, long nDPIY, long nPixelWidth,
    std::function<long(const OUString&)> const& fnGetTextWidth,
    std::function<void(tools::Rectangle&, const OUString&)> const& fnGetBoundRect)
{
    if (!pFontInstance)
        return;

    // 1. Calculate Bullet Offset
    long nSpaceW = fnGetTextWidth(OUString(u' '));
    long nBulletW = fnGetTextWidth(u"\x00b7"_ustr);
    long nBulletOffset = (nSpaceW - nBulletW) >> 1;

    pFontInstance->mxFontMetric->ImplInitTextLineSize(pFontInstance, nDPIY, rFont, nBulletOffset);

    // 2. Set Above Text Line Size
    pFontInstance->mxFontMetric->ImplInitAboveTextLineSize(nDPIY, nPixelWidth);

    // 3. CJK Fullstop Centering
    bool bCentered = true;
    if (MsLangId::isCJK(rFont.GetLanguage()))
    {
        tools::Rectangle aRect;
        fnGetBoundRect(aRect, u"\x3001"_ustr); // Fullwidth fullstop

        const auto nH = rFont.GetFontSize().Height();
        const auto nB = aRect.Left();

        bCentered = nB > (((nH >> 1) + nH) >> 3);
    }
    pFontInstance->mxFontMetric->SetFullstopCenteredFlag(bCentered);
}

vcl::text::TextLayoutRequest TextLayoutEngine::CreateLayoutRequest(
    OUString& rStr, sal_Int32 nMinIndex, sal_Int32 nLen, double nPixelWidth, SalLayoutFlags nFlags,
    const vcl::text::TextLayoutCache* pCache, const GraphicsState& rState,
    const font::FontRealization& rRealization, bool bRTL)
{
    assert(nMinIndex >= 0);
    assert(nLen >= 0);

    // get string length for calculating extents
    sal_Int32 nEndIndex = rStr.getLength();
    if (nMinIndex + nLen < nEndIndex)
        nEndIndex = nMinIndex + nLen;

    // don't bother if there is nothing to do
    if (nEndIndex < nMinIndex)
        nEndIndex = nMinIndex;

    TextAnalyzer::ApplyDigitLocalization(rState, rStr, nMinIndex, nEndIndex);

    nFlags = TextAnalyzer::CalculateLayoutFlags(rState, rRealization, bRTL, rStr, nMinIndex,
                                                nEndIndex, nFlags);

    vcl::text::TextLayoutRequest aLayoutArgs(rStr, nMinIndex, nEndIndex, nFlags,
                                             rState.maFont.GetLanguageTag(), pCache);

    Degree10 nOrientation = rRealization.mxFont ? rRealization.mxFont->mnOrientation : 0_deg10;
    aLayoutArgs.SetOrientation(nOrientation);

    aLayoutArgs.SetLayoutWidth(nPixelWidth);

    return aLayoutArgs;
}

void TextLayoutEngine::JustifyLayout(SalLayout& rLayout, vcl::text::TextLayoutRequest& rArgs)
{
    rLayout.AdjustLayout(rArgs);
}

void TextLayoutEngine::ApplyHorizontalOffset(SalLayout& rLayout,
                                             const vcl::text::TextLayoutRequest& rArgs,
                                             const TextLayoutPositioning& rPositioning)
{
    if (!rPositioning.bRightAlign)
        return;

    double nRTLOffset;
    if (rPositioning.bHasDXArray)
        nRTLOffset = rPositioning.nEndGlyphCoord;
    else if (rArgs.mnLayoutWidth)
        nRTLOffset = rArgs.mnLayoutWidth;
    else
        nRTLOffset = rLayout.GetTextWidth();

    rLayout.DrawOffset().setX(1 - nRTLOffset);
}

void TextLayoutEngine::SetAnchorPoint(SalLayout& rLayout, const TextLayoutPositioning& rPositioning)
{
    rLayout.DrawBase() = rPositioning.aDrawBase;
}

static double lcl_applyDXArray(JustificationData& rJustification, const LayoutResources& rRes,
                               KernArraySpan pDXArray, sal_Int32 nMinCluster, sal_Int32 nLen)
{
    if (pDXArray.empty())
        return 0.0;

    double nEndCoord = 0.0;

    if (rRes.rMapper.IsMapModeEnabled())
    {
        for (int i = 0; i < nLen; ++i)
        {
            rJustification.SetTotalAdvance(nMinCluster + i,
                                           rRes.rMapper.LogicWidthToDeviceSubPixel(pDXArray[i]));
        }

        nEndCoord = rJustification.GetTotalAdvance(nMinCluster + nLen - 1);
    }
    else
    {
        for (int i = 0; i < nLen; ++i)
        {
            rJustification.SetTotalAdvance(nMinCluster + i, pDXArray[i]);
        }

        nEndCoord = std::round(rJustification.GetTotalAdvance(nMinCluster + nLen - 1));
    }

    return nEndCoord;
}

static void lcl_applyKashidaArray(JustificationData& rJustification,
                                  std::span<const sal_Bool> pKashidaArray, sal_Int32 nMinCluster)
{
    if (pKashidaArray.empty())
        return;

    for (sal_Int32 i = 0; i < static_cast<sal_Int32>(pKashidaArray.size()); ++i)
    {
        rJustification.SetKashidaPosition(nMinCluster + i, static_cast<bool>(pKashidaArray[i]));
    }
}

void TextLayoutEngine::PrepareJustification(const LayoutResources& rRes, KernArraySpan pDXArray,
                                            std::span<const sal_Bool> pKashidaArray,
                                            sal_Int32 nMinIndex, sal_Int32 nLen,
                                            std::optional<sal_Int32> nDrawMinCharPos,
                                            std::optional<sal_Int32> nDrawEndCharPos,
                                            vcl::text::TextLayoutRequest& rLayoutArgs,
                                            double& rEndGlyphCoord)
{
    if (pDXArray.empty() && pKashidaArray.empty())
        return;

    const auto nJustMinCluster = nDrawMinCharPos.value_or(nMinIndex);
    auto nJustLen = nLen;

    if (nDrawEndCharPos.has_value())
        nJustLen = *nDrawEndCharPos - nJustMinCluster;

    JustificationData aJustification{ nJustMinCluster, nJustLen };

    if (!pDXArray.empty())
    {
        rEndGlyphCoord
            = lcl_applyDXArray(aJustification, rRes, pDXArray, nJustMinCluster, nJustLen);
    }

    lcl_applyKashidaArray(aJustification, pKashidaArray, nJustMinCluster);

    rLayoutArgs.SetJustificationData(std::move(aJustification));
}

basegfx::B2DPoint TextLayoutEngine::MapLogicalToDevicePos(const LayoutResources& rRes,
                                                          const Point& rLogicalPos)
{
    // Use subpixel precision if MapMode is on OR if we were explicitly told to (e.g. PDF/Subpixel flag)
    if (rRes.rMapper.IsMapModeEnabled() || rRes.bSubpixelPositioning)
        return rRes.rMapper.LogicToDeviceSubPixel(rLogicalPos);

    Point aDevicePos = rRes.rMapper.LogicToDevicePixel(rLogicalPos);
    return basegfx::B2DPoint(aDevicePos.X(), aDevicePos.Y());
}

void TextLayoutEngine::FillAlignmentContext(TextLayoutPositioning& rPos,
                                            const vcl::text::TextLayoutRequest& rArgs,
                                            double nEndGlyphCoord)
{
    rPos.bRightAlign = bool(rArgs.mnFlags & SalLayoutFlags::RightAlign);
    rPos.nEndGlyphCoord = nEndGlyphCoord;
}

double TextLayoutEngine::FillPartialTextArray(const LayoutResources& rRes, const SalLayout& rLayout,
                                              KernArray* pKernArray, sal_Int32 nIndex,
                                              sal_Int32 nLen, sal_Int32 nPartIndex,
                                              sal_Int32 nPartLen, const OUString& rCaretStr)
{
    std::vector<double> aDXPixelArray;
    std::vector<double>* pDXPixelArray = nullptr;
    if (pKernArray)
    {
        aDXPixelArray.resize(nPartLen);
        pDXPixelArray = &aDXPixelArray;
    }

    double nWidth = 0.0;
    if (nIndex == nPartIndex && nLen == nPartLen)
        nWidth = rLayout.FillDXArray(pDXPixelArray, rCaretStr);
    else
        nWidth
            = rLayout.FillPartialDXArray(pDXPixelArray, rCaretStr, nPartIndex - nIndex, nPartLen);

    if (pDXPixelArray)
    {
        for (int i = 1; i < nPartLen; ++i)
            (*pDXPixelArray)[i] += (*pDXPixelArray)[i - 1];

        if (rRes.rMapper.IsMapModeEnabled())
        {
            for (int i = 0; i < nPartLen; ++i)
                (*pDXPixelArray)[i]
                    = rRes.rMapper.DevicePixelToLogicWidthDouble((*pDXPixelArray)[i]);
        }

        pKernArray->resize(nPartLen);
        for (int i = 0; i < nPartLen; ++i)
            (*pKernArray)[i] = (*pDXPixelArray)[i];
    }

    return rRes.rMapper.DevicePixelToLogicWidthDouble(nWidth);
}

bool TextLayoutEngine::PrepareNormalizedLayoutInput(
    const OUString& rOrigStr, sal_Int32 nMinIndex, sal_Int32& rLen, OUString& rStr,
    const vcl::font::FontRealization& rFontRealization,
    const vcl::text::TextLayoutCache*& rpLayoutCache, const SalLayoutGlyphs*& rpGlyphs)
{
    // Check string index and length
    if (rLen == -1 || nMinIndex + rLen > rOrigStr.getLength())
    {
        const sal_Int32 nNewLen = rOrigStr.getLength() - nMinIndex;
        if (nNewLen <= 0)
            return false;
        rLen = nNewLen;
    }

    rStr = rOrigStr;

    // Recode string if needed
    if (rFontRealization.mxFont && rFontRealization.mxFont->mpConversion)
    {
        rFontRealization.mxFont->mpConversion->RecodeString(rStr, 0, rStr.getLength());
        rpLayoutCache = nullptr; // don't use cache with modified string!
        rpGlyphs = nullptr;
    }

    return true;
}

// Helper for Diagnostic Font Tracking
namespace
{
static OutputDevice::FontMappingUseData* g_pFontMappingUseData = nullptr;
}

void TextLayoutEngine::StartTracking()
{
    delete g_pFontMappingUseData;
    g_pFontMappingUseData = new OutputDevice::FontMappingUseData;
}

OutputDevice::FontMappingUseData TextLayoutEngine::FinishTracking()
{
    if (!g_pFontMappingUseData)
        return {};
    OutputDevice::FontMappingUseData aRet = std::move(*g_pFontMappingUseData);
    delete g_pFontMappingUseData;
    g_pFontMappingUseData = nullptr;
    return aRet;
}

bool TextLayoutEngine::IsTracking() { return g_pFontMappingUseData != nullptr; }

void TextLayoutEngine::TrackLayoutFonts(const vcl::Font& rFont, const SalLayout* pLayout)
{
    if (!pLayout || !IsTracking())
        return;

    OUString aOriginalName = rFont.GetStyleName().isEmpty()
                                 ? rFont.GetFamilyName()
                                 : rFont.GetFamilyName() + "/" + rFont.GetStyleName();

    std::vector<OUString> aUsedFontNames;
    SalLayoutGlyphs aGlyphs = pLayout->GetGlyphs();
    int nLevel = 0;
    while (const SalLayoutGlyphsImpl* pImpl = aGlyphs.Impl(nLevel++))
    {
        const vcl::font::PhysicalFontFace* pFace = pImpl->GetFont()->GetFontFace();
        OUString aName = pFace->GetStyleName().isEmpty()
                             ? pFace->GetFamilyName()
                             : pFace->GetFamilyName() + "/" + pFace->GetStyleName();
        aUsedFontNames.push_back(aName);
    }

    for (auto& rItem : *g_pFontMappingUseData)
    {
        if (rItem.mOriginalFont == aOriginalName && rItem.mUsedFonts == aUsedFontNames)
        {
            ++rItem.mCount;
            return;
        }
    }

    g_pFontMappingUseData->push_back({ aOriginalName, std::move(aUsedFontNames), 1 });
}

void TextLayoutEngine::ValidateGlyphCache(const SalLayoutGlyphs* pGlyphs)
{
    if (!pGlyphs)
        return;

    if (!pGlyphs->IsValid())
    {
        SAL_WARN("vcl", "Trying to setup invalid cached glyphs - falling back to relayout!");
        return;
    }

#ifdef DBG_UTIL
    for (int level = 0;; ++level)
    {
        SalLayoutGlyphsImpl* glyphsImpl = pGlyphs->Impl(level);
        if (glyphsImpl == nullptr)
            break;
        assert(glyphsImpl->GetFlags() & SalLayoutFlags::GlyphItemsOnly);
    }
#endif
}

double TextLayoutEngine::GetTextHeightPixel(const vcl::font::FontRealization& rRealization)
{
    if (!rRealization.mxFont)
        return 0.0;

    return static_cast<double>(rRealization.mxFont->mnLineHeight + rRealization.nEmphasisAscent
                               + rRealization.nEmphasisDescent);
}

double TextLayoutEngine::CalculateLayoutWidth(const LayoutResources& rRes, tools::Long nLogicWidth)
{
    if (nLogicWidth && rRes.rMapper.IsMapModeEnabled())
        return rRes.rMapper.LogicWidthToDeviceSubPixel(nLogicWidth);

    return static_cast<double>(nLogicWidth);
}

std::unique_ptr<SalLayout> TextLayoutEngine::CreateBaseLayout(const LayoutResources& rRes)
{
    SalGraphics* pGraphics = rRes.fnGetGraphics();
    if (!pGraphics)
        return nullptr;

    std::unique_ptr<SalLayout> pSalLayout = pGraphics->GetTextLayout(0);

    // tdf#168002: Activate subpixel positioning if required
    if (pSalLayout)
        pSalLayout->SetSubpixelPositioning(rRes.bSubpixelPositioning);

    return pSalLayout;
}

void TextLayoutEngine::ApplyPositioning(const LayoutResources& rRes, SalLayout& rLayout,
                                        vcl::text::TextLayoutRequest& rArgs,
                                        const Point& rLogicalPos, double nEndGlyphCoord)
{
    TextLayoutPositioning aPos;
    aPos.bSubpixelPositioning = rRes.bSubpixelPositioning;

    FillAlignmentContext(aPos, rArgs, nEndGlyphCoord);
    aPos.aDrawBase = MapLogicalToDevicePos(rRes, rLogicalPos);

    JustifyLayout(rLayout, rArgs);
    ApplyHorizontalOffset(rLayout, rArgs, aPos);
    SetAnchorPoint(rLayout, aPos);
}

std::unique_ptr<SalLayout> TextLayoutEngine::PerformTextLayout(const LayoutResources& rRes,
                                                               vcl::text::TextLayoutRequest& rArgs,
                                                               const SalLayoutGlyphs* pGlyphs)
{
    std::unique_ptr<SalLayout> pSalLayout = CreateBaseLayout(rRes);

    if (pSalLayout && !pSalLayout->LayoutText(rArgs, pGlyphs ? pGlyphs->Impl(0) : nullptr))
        pSalLayout.reset();

    if (!pSalLayout)
        return nullptr;

    if (rArgs.HasFallbackRun() && rRes.pFont->GetFontSelectPattern().mnHeight >= 3)
    {
        pSalLayout = DefaultFallbackStrategy().ResolveFallbacks(rRes, std::move(pSalLayout), rArgs,
                                                                pGlyphs);
    }

    return pSalLayout;
}

std::unique_ptr<SalLayout>
TextLayoutEngine::Layout(const LayoutResources& rRes, const vcl::text::TextSpan& rSpan,
                         const vcl::text::LayoutConstraints& rConstraints,
                         const vcl::text::LayoutCacheData& rCache,
                         const vcl::text::RenderSelection& rSelection)
{
    // Create local copies of cache pointers because the engine might modify them (set to null)
    const vcl::text::TextLayoutCache* pLayoutCache = rCache.pCache;
    const SalLayoutGlyphs* pGlyphs = rCache.pGlyphs;

    // Validate
    ValidateGlyphCache(pGlyphs);
    const SalLayoutGlyphs* pEffectiveGlyphs = (pGlyphs && !pGlyphs->IsValid()) ? nullptr : pGlyphs;

    OUString aStr;
    // Normalize Input (Recode string if needed)
    sal_Int32 nLen = rSpan.Length;
    if (!PrepareNormalizedLayoutInput(rSpan.Text, rSpan.Index, nLen, aStr, rRes.rFontRealization,
                                      pLayoutCache, pEffectiveGlyphs))
    {
        return nullptr;
    }

    double nPixelWidth = CalculateLayoutWidth(rRes, rConstraints.LogicalWidth);

    vcl::text::TextLayoutRequest aLayoutArgs = CreateLayoutRequest(
        aStr, rSpan.Index, nLen, nPixelWidth, rConstraints.Flags, pLayoutCache, rRes.rGraphicsState,
        rRes.rFontRealization, rRes.bRTLEnabled);

    if (rSelection.DrawOriginCluster.has_value())
        aLayoutArgs.mnDrawOriginCluster = *rSelection.DrawOriginCluster;
    if (rSelection.DrawMinCharPos.has_value())
        aLayoutArgs.mnDrawMinCharPos = *rSelection.DrawMinCharPos;
    if (rSelection.DrawEndCharPos.has_value())
        aLayoutArgs.mnDrawEndCharPos = *rSelection.DrawEndCharPos;

    double nEndGlyphCoord = 0.0;
    PrepareJustification(rRes, rConstraints.pDXArray, rConstraints.pKashidaArray, rSpan.Index, nLen,
                         rSelection.DrawMinCharPos, rSelection.DrawEndCharPos, aLayoutArgs,
                         nEndGlyphCoord);

    std::unique_ptr<SalLayout> pSalLayout = PerformTextLayout(rRes, aLayoutArgs, pEffectiveGlyphs);

    if (!pSalLayout)
        return nullptr;

    if (rConstraints.Flags & SalLayoutFlags::GlyphItemsOnly)
        return pSalLayout;

    ApplyPositioning(rRes, *pSalLayout, aLayoutArgs, rConstraints.LogicalPos, nEndGlyphCoord);

    TrackLayoutFonts(rRes.rGraphicsState.maFont, pSalLayout.get());

    return pSalLayout;
}

void TextLayoutEngine::ZeroFillKernArray(KernArray* pKernArray, sal_Int32 nLen)
{
    if (pKernArray)
        pKernArray->assign(std::max<sal_Int32>(0, nLen), 0.0);
}

static void lcl_convertBoundRectToLogic(const SalLayout& rLayout, const CoordinateMapper& rMapper,
                                        std::optional<tools::Rectangle>* pBounds)
{
    if (!pBounds)
        return;

    basegfx::B2DRectangle aB2DRect;

    if (rLayout.GetBoundRect(aB2DRect))
    {
        tools::Rectangle aRect = SalLayout::BoundRect2Rectangle(aB2DRect);
        *pBounds = rMapper.DevicePixelToLogic(aRect);
    }
}

double TextLayoutEngine::GetPartialTextArray(const LayoutResources& rRes,
                                             const vcl::text::TextSpan& rSpan,
                                             KernArray* pKernArray, sal_Int32 nPartIndex,
                                             sal_Int32 nPartLen, bool bCaret,
                                             const vcl::text::LayoutCacheData& rCache,
                                             std::optional<tools::Rectangle>* pBounds)
{
    if (rSpan.Index >= rSpan.Text.getLength())
        return 0.0;

    // Normalize lengths
    sal_Int32 nLen = TextAnalyzer::GetNormalizedLength(rSpan.Text, rSpan.Index, rSpan.Length);
    sal_Int32 nNormalizedPartLen
        = TextAnalyzer::GetNormalizedLength(rSpan.Text, nPartIndex, nPartLen);

    vcl::text::TextSpan aNormalizedSpan{ rSpan.Text, rSpan.Index, nLen };
    vcl::text::LayoutConstraints aConstraints{ Point(0, 0), 0, {}, {}, SalLayoutFlags::NONE };

    vcl::text::RenderSelection aSelection;
    if (rSpan.Index != nPartIndex || nLen != nNormalizedPartLen)
    {
        // If we are measuring a subset, tell the layout engine about the range
        aSelection
            = vcl::text::RenderSelection{ nPartIndex, nPartIndex, nPartIndex + nNormalizedPartLen };
    }

    std::unique_ptr<SalLayout> pSalLayout
        = Layout(rRes, aNormalizedSpan, aConstraints, rCache, aSelection);

    if (!pSalLayout)
    {
        ZeroFillKernArray(pKernArray, nNormalizedPartLen);
        return 0.0;
    }

    lcl_convertBoundRectToLogic(*pSalLayout, rRes.rMapper, pBounds);

    return FillPartialTextArray(rRes, *pSalLayout, pKernArray, rSpan.Index, nLen, nPartIndex,
                                nNormalizedPartLen, bCaret ? rSpan.Text : OUString());
}

void TextLayoutEngine::FixupCaretPositions(std::vector<double>& rCaretPixelPos)
{
    const int nCaretPos = static_cast<int>(rCaretPixelPos.size());
    int nFirstValidIndex = nCaretPos; // Initialize to "not found" state

    // Find first valid coordinate
    for (int i = 0; i < nCaretPos; ++i)
    {
        if (rCaretPixelPos[i] >= 0)
        {
            nFirstValidIndex = i;
            break;
        }
    }

    // Propagate coordinates
    double nXPos = (nFirstValidIndex < nCaretPos) ? rCaretPixelPos[nFirstValidIndex] : -1.0;
    for (int i = 0; i < nCaretPos; ++i)
    {
        if (rCaretPixelPos[i] >= 0)
            nXPos = rCaretPixelPos[i];
        else
            rCaretPixelPos[i] = nXPos;
    }
}

static double lcl_mirrorCoord(double nTotalWidth, double nOldPos)
{
    return nTotalWidth - nOldPos - 1.0;
}

void TextLayoutEngine::MirrorCaretPositions(std::vector<double>& rCaretPixelPos, double nWidth)
{
    for (double& rPos : rCaretPixelPos)
    {
        rPos = lcl_mirrorCoord(nWidth, rPos);
    }
}

void TextLayoutEngine::ConvertPixelsToLogic(const CoordinateMapper& rMapper,
                                            std::vector<double>& rCaretPixelPos)
{
    if (!rMapper.IsMapModeEnabled())
        return;

    for (double& rPos : rCaretPixelPos)
    {
        rPos = rMapper.DevicePixelToLogicWidthDouble(rPos);
    }
}

void TextLayoutEngine::GetCaretPositions(const LayoutResources& rRes,
                                         const vcl::text::TextSpan& rSpan, KernArray& rCaretPos,
                                         const vcl::text::LayoutCacheData& rCache)
{
    sal_Int32 nLen = TextAnalyzer::GetNormalizedLength(rSpan.Text, rSpan.Index, rSpan.Length);
    rCaretPos.assign(nLen * 2, -1);

    const vcl::text::LayoutConstraints aConstraints{ Point(0, 0), 0, {}, {}, SalLayoutFlags::NONE };
    std::unique_ptr<SalLayout> pSalLayout = Layout(rRes, rSpan, aConstraints, rCache, {});

    if (!pSalLayout)
        return;

    // Measure Carets
    std::vector<double> aCaretPixelPos;
    pSalLayout->GetCaretPositions(aCaretPixelPos, rSpan.Text);

    FixupCaretPositions(aCaretPixelPos);

    if (rRes.bRTLEnabled)
        MirrorCaretPositions(aCaretPixelPos, pSalLayout->GetTextWidth());

    ConvertPixelsToLogic(rRes.rMapper, aCaretPixelPos);

    for (size_t i = 0; i < aCaretPixelPos.size(); ++i)
    {
        rCaretPos[i] = aCaretPixelPos[i];
    }
}

tools::Long TextLayoutEngine::GetSubPixelFactor(const CoordinateMapper& rMapper)
{
    // Use 64 as a factor when MapMode is disabled to maintain subpixel granularity
    return rMapper.IsMapModeEnabled() ? 1 : 64;
}

double TextLayoutEngine::GetLayoutPixelWidth(const CoordinateMapper& rMapper,
                                             tools::Long nLogicWidth, tools::Long nSubPixelFactor)
{
    // High-precision conversion from logical units to device subpixels
    return rMapper.LogicWidthToDeviceSubPixel(nLogicWidth * nSubPixelFactor);
}

sal_Int32 TextLayoutEngine::GetTextBreak(const LayoutResources& rRes,
                                         const vcl::text::TextSpan& rSpan,
                                         tools::Long nMaxLineWidth, tools::Long nCharExtra,
                                         const vcl::text::LayoutCacheData& rCache)
{
    const vcl::text::LayoutConstraints aConstraints{ Point(0, 0), 0, {}, {}, SalLayoutFlags::NONE };
    std::unique_ptr<SalLayout> pSalLayout = Layout(rRes, rSpan, aConstraints, rCache, {});

    if (!pSalLayout)
        return -1;

    const tools::Long nSubPixelFactor = GetSubPixelFactor(rRes.rMapper);
    const double nTextPixelWidth
        = GetLayoutPixelWidth(rRes.rMapper, nMaxLineWidth, nSubPixelFactor);

    double nExtraPixelWidth = 0;
    if (nCharExtra != 0)
        nExtraPixelWidth = GetLayoutPixelWidth(rRes.rMapper, nCharExtra, nSubPixelFactor);

    return pSalLayout->GetTextBreak(nTextPixelWidth, nExtraPixelWidth, nSubPixelFactor);
}

bool TextLayoutEngine::GetTextIsRTL(const LayoutResources& rRes, const OUString& rString,
                                    sal_Int32 nIndex, sal_Int32 nLen)
{
    OUString aStr(rString);
    vcl::text::TextLayoutRequest aArgs
        = CreateLayoutRequest(aStr, nIndex, nLen, 0, SalLayoutFlags::NONE, nullptr,
                              rRes.rGraphicsState, rRes.rFontRealization, rRes.bRTLEnabled);

    bool bRTL = false;
    int nCharPos = -1;
    if (!aArgs.GetNextPos(&nCharPos, &bRTL))
        return false;

    return (nCharPos != nIndex);
}

sal_Int32 TextLayoutEngine::GetTextBreakArray(
    const LayoutResources& rRes, const vcl::text::TextSpan& rSpan, tools::Long nTextWidth,
    std::optional<sal_Unicode> nHyphenChar, std::optional<sal_Int32*> pHyphenPos,
    tools::Long nCharExtra, KernArraySpan aKernArray, const vcl::text::LayoutCacheData& rCache)
{
    if (pHyphenPos.has_value())
        **pHyphenPos = -1;

    const vcl::text::LayoutConstraints aConstraints{
        Point(0, 0), 0, aKernArray, {}, SalLayoutFlags::NONE
    };
    std::unique_ptr<SalLayout> pSalLayout = Layout(rRes, rSpan, aConstraints, rCache, {});

    if (!pSalLayout)
        return -1;

    const tools::Long nSubPixelFactor = GetSubPixelFactor(rRes.rMapper);
    double nTextPixelWidth = GetLayoutPixelWidth(rRes.rMapper, nTextWidth, nSubPixelFactor);
    double nExtraPixelWidth
        = (nCharExtra != 0) ? GetLayoutPixelWidth(rRes.rMapper, nCharExtra, nSubPixelFactor) : 0;

    sal_Int32 nRetVal
        = pSalLayout->GetTextBreak(nTextPixelWidth, nExtraPixelWidth, nSubPixelFactor);

    if (!nHyphenChar.has_value())
        return nRetVal;

    OUString aHyphenStr(*nHyphenChar);
    vcl::text::TextSpan aHyphenSpan{ aHyphenStr, 0, 1 };
    std::unique_ptr<SalLayout> pHyphenLayout = Layout(rRes, aHyphenSpan, {}, {}, {});

    if (!pHyphenLayout)
        return nRetVal;

    double nHyphenPixelWidth = pHyphenLayout->GetTextWidth() * nSubPixelFactor;
    nTextPixelWidth -= nHyphenPixelWidth;
    if (nExtraPixelWidth > 0)
        nTextPixelWidth -= nExtraPixelWidth;

    if (pHyphenPos.has_value())
    {
        **pHyphenPos = pSalLayout->GetTextBreak(nTextPixelWidth, nExtraPixelWidth, nSubPixelFactor);
        if (**pHyphenPos > nRetVal)
            **pHyphenPos = nRetVal;
    }

    return nRetVal;
}

bool TextLayoutEngine::GetLogicalTextBoundRect(const LayoutResources& rRes,
                                               basegfx::B2DRectangle& rRect, const OUString& rStr,
                                               sal_Int32 nBase, sal_Int32 nIndex, sal_Int32 nLen,
                                               sal_uLong nLayoutWidth, KernArraySpan pDXArray,
                                               std::span<const sal_Bool> pKashidaArray,
                                               const SalLayoutGlyphs* pGlyphs)
{
    bool bRet = false;
    rRect.reset();

    // calculate offset when nBase!=nIndex
    double nXOffset = 0;
    if (nBase != nIndex)
    {
        sal_Int32 nStart = std::min(nBase, nIndex);
        sal_Int32 nOfsLen = std::max(nBase, nIndex) - nStart;

        std::unique_ptr<SalLayout> pOfsLayout = Layout(
            rRes, vcl::text::TextSpan{ rStr, nStart, nOfsLen },
            vcl::text::LayoutConstraints{ Point(0, 0), static_cast<tools::Long>(nLayoutWidth),
                                          pDXArray, pKashidaArray, SalLayoutFlags::NONE },
            {}, {});

        if (pOfsLayout)
        {
            nXOffset = pOfsLayout->GetTextWidth();

            if (nBase < nIndex)
                nXOffset = -nXOffset;
        }
    }

    // Main Layout
    std::unique_ptr<SalLayout> pSalLayout
        = Layout(rRes, vcl::text::TextSpan{ rStr, nIndex, nLen },
                 vcl::text::LayoutConstraints{ Point(0, 0), static_cast<tools::Long>(nLayoutWidth),
                                               pDXArray, pKashidaArray, SalLayoutFlags::NONE },
                 vcl::text::LayoutCacheData{ nullptr, pGlyphs }, {});

    if (pSalLayout)
    {
        basegfx::B2DRectangle aPixelRect;
        bRet = pSalLayout->GetBoundRect(aPixelRect);

        if (bRet)
        {
            basegfx::B2DPoint aPos = pSalLayout->GetDrawPosition(basegfx::B2DPoint(nXOffset, 0));
            // Apply font offsets and transform to logical units
            aPixelRect.translate(rRes.rFontRealization.nXOffset - aPos.getX(),
                                 rRes.rFontRealization.nYOffset - aPos.getY());
            rRect = rRes.rMapper.PixelToLogic(aPixelRect);

            if (rRes.rMapper.IsMapModeEnabled())
                rRect.translate(rRes.rMapper.GetMappingXOffset(), rRes.rMapper.GetMappingYOffset());
        }
    }

    return bRet;
}

tools::Rectangle
TextLayoutEngine::GetTextInkBounds(const SalLayout& rSalLayout,
                                   const vcl::font::FontRealization& rFontRealization,
                                   bool bApplyRotation)
{
    const basegfx::B2DPoint aPoint = rSalLayout.GetDrawPosition();
    tools::Long nX = aPoint.getX();
    tools::Long nY
        = aPoint.getY()
          - (rFontRealization.mxFont->mxFontMetric->GetAscent() + rFontRealization.nEmphasisAscent);

    double nWidth = rSalLayout.GetTextWidth();
    tools::Long nHeight = rFontRealization.mxFont->mnLineHeight + rFontRealization.nEmphasisAscent
                          + rFontRealization.nEmphasisDescent;

    basegfx::B2DRectangle aBoundRect;
    if (rSalLayout.GetBoundRect(aBoundRect))
        return SalLayout::BoundRect2Rectangle(aBoundRect);

    if (bApplyRotation && rFontRealization.mxFont->mnOrientation)
    {
        const tools::Long nBaseX = nX;
        const tools::Long nBaseY = nY;
        if (!(rFontRealization.mxFont->mnOrientation % 900_deg10))
        {
            tools::Long nX2 = nX + nWidth;
            tools::Long nY2 = nY + nHeight;
            Point aBasePt(nBaseX, nBaseY);
            aBasePt.RotateAround(nX, nY, rFontRealization.mxFont->mnOrientation);
            aBasePt.RotateAround(nX2, nY2, rFontRealization.mxFont->mnOrientation);
            nWidth = nX2 - nX;
            nHeight = nY2 - nY;
        }
        else
        {
            tools::Rectangle aRect(Point(nX, nY), Size(nWidth + 1, nHeight + 1));
            tools::Polygon aPoly(aRect);
            aPoly.Rotate(Point(nBaseX, nBaseY), rFontRealization.mxFont->mnOrientation);
            return aPoly.GetBoundRect();
        }
    }
    return tools::Rectangle(Point(nX, nY), Size(nWidth, nHeight));
}

void TextLayoutEngine::GetEmphasisMarkPositions(const SalLayout& rSalLayout,
                                                const vcl::font::FontRealization& rFontRealization,
                                                const vcl::font::EmphasisMark& rMark,
                                                bool bEmphasisBelow, std::vector<Point>& rPoints)
{
    rPoints.clear();
    if (!rFontRealization.mxFont)
        return;

    // Calculate base anchor (Ascent or Descent line)
    const tools::Long nBaseOffset
        = bEmphasisBelow ? rFontRealization.nEmphasisDescent : -rFontRealization.nEmphasisAscent;
    const basegfx::B2DPoint aDrawPos = rSalLayout.GetDrawPosition();
    const tools::Long nAnchorY = aDrawPos.getY() + nBaseOffset;

    // Prepare visual adjustments (centering and mark-specific offset)
    const tools::Long nXCenterOff = rMark.GetWidth() / 2;
    const tools::Long nYCenterOff
        = (bEmphasisBelow ? rFontRealization.nEmphasisDescent : rFontRealization.nEmphasisAscent)
          / 2;
    const tools::Long nShapeAdj = bEmphasisBelow ? rMark.GetYOffset() : -rMark.GetYOffset();

    int nStart = 0;
    const GlyphItem* pGlyph = nullptr;
    basegfx::B2DPoint aPos;

    while (rSalLayout.GetNextGlyph(&pGlyph, aPos, nStart))
    {
        if (!pGlyph)
            continue;

        Point aMarkPt(static_cast<tools::Long>(aPos.getX()) - nXCenterOff,
                      nAnchorY + nShapeAdj - nYCenterOff);

        rPoints.push_back(aMarkPt);
    }
}

basegfx::B2DHomMatrix TextLayoutEngine::CalculateOutlineTransform(
    const SalLayout& rLayout, const vcl::font::FontRealization& rRealization, double nXOffset)
{
    basegfx::B2DHomMatrix aMatrix;

    // This matches the logic in OutputDevice::GetTextOutlines
    if (nXOffset != 0 || rRealization.nXOffset != 0 || rRealization.nYOffset != 0)
    {
        basegfx::B2DPoint aRotatedOfs(rRealization.nXOffset, rRealization.nYOffset);

        // Calculate the relative draw position for the given offset
        // This handles cases where text is drawn at an X-offset (e.g. for bold simulation or composition)
        aRotatedOfs -= rLayout.GetDrawPosition(basegfx::B2DPoint(nXOffset, 0));

        aMatrix.translate(aRotatedOfs.getX(), aRotatedOfs.getY());
    }
    return aMatrix;
}

void TextLayoutEngine::GetWordLineSegments(const SalLayout& rSalLayout,
                                           const vcl::font::FontRealization& rRealization,
                                           std::vector<std::pair<double, double>>& rSegments)
{
    rSegments.clear();
    const basegfx::B2DPoint aStartPt = rSalLayout.DrawBase();
    const LogicalFontInstance* pFont = rRealization.mxFont.get();
    const Degree10 nOrientation = pFont ? pFont->mnOrientation : 0_deg10;

    basegfx::B2DPoint aPos;
    double nDist = 0;
    double nWidth = 0;
    const GlyphItem* pGlyph = nullptr;
    int nStart = 0;

    while (rSalLayout.GetNextGlyph(&pGlyph, aPos, nStart))
    {
        if (!pGlyph->IsSpacing())
        {
            if (nWidth == 0)
            {
                nDist = aPos.getX() - aStartPt.getX();
                if (nOrientation)
                {
                    const double nDY = aPos.getY() - aStartPt.getY();
                    const double fRad = toRadians(nOrientation);
                    nDist = nDist * cos(fRad) - nDY * sin(fRad);
                }
            }
            nWidth += pGlyph->newWidth();
        }
        else if (nWidth > 0)
        {
            rSegments.push_back({ nDist, nWidth });
            nWidth = 0;
        }
    }
    if (nWidth > 0)
        rSegments.push_back({ nDist, nWidth });
}

void TextLayoutEngine::InitializeTextLineMetrics(const LogicalFontInstance* pFontInstance,
                                                 const vcl::Font& rFont, tools::Long nDPIY,
                                                 tools::Long nSpaceWidth, tools::Long nBulletWidth)
{
    if (!pFontInstance || !pFontInstance->mxFontMetric)
        return;

    // Logic migrated from OutputDevice::ImplInitTextLineSize
    tools::Long nBulletOffset = (nSpaceWidth - nBulletWidth) >> 1;

    pFontInstance->mxFontMetric->ImplInitTextLineSize(pFontInstance, nDPIY, rFont, nBulletOffset);
}

void TextLayoutEngine::InitializeAboveTextLineMetrics(const LogicalFontInstance* pFontInstance,
                                                      tools::Long nDPIY, tools::Long nPixelWidth)
{
    if (!pFontInstance || !pFontInstance->mxFontMetric)
        return;

    pFontInstance->mxFontMetric->ImplInitAboveTextLineSize(nDPIY, nPixelWidth);
}

tools::Long TextLayoutEngine::GetAlignmentOffset(TextAlign eAlign, tools::Long nAscent,
                                                 tools::Long nDescent)
{
    if (eAlign == ALIGN_BOTTOM)
        return -nDescent;

    if (eAlign == ALIGN_TOP)
        return nAscent;

    return 0;
}

OUString TextLayoutEngine::GetEllipsisString(
    const OUString& rStr, tools::Long nMaxWidth, DrawTextFlags nStyle,
    const std::function<tools::Long(const OUString&)>& rfnGetTextWidth)
{
    // Trivial Case
    if (rStr.isEmpty() || rfnGetTextWidth(rStr) <= nMaxWidth)
        return rStr;

    const OUString aEllipsisStr = "...";
    const bool bClipText = bool(nStyle & DrawTextFlags::Clip);
    sal_Int32 nLen = rStr.getLength();

    // Center Ellipsis (Balanced Truncation)
    if (nStyle & DrawTextFlags::CenterEllipsis)
    {
        sal_Int32 nIndex = nLen / 2;
        sal_Int32 nEraseChars = std::max<sal_Int32>(4, nLen - (nIndex * 4) / 3);

        while (nEraseChars < nLen)
        {
            sal_Int32 i = (nLen - nEraseChars) / 2;
            OUString aTmpStr = rStr.copy(0, i) + aEllipsisStr + rStr.copy(i + nEraseChars);

            if (rfnGetTextWidth(aTmpStr) <= nMaxWidth)
                return aTmpStr;

            nEraseChars++;
        }

        return aEllipsisStr;
    }

    // Path Ellipsis
    if (nStyle & DrawTextFlags::PathEllipsis)
    {
        sal_Int32 nLastSep = rStr.lastIndexOf('/');
        if (nLastSep == -1)
            nLastSep = rStr.lastIndexOf('\\');

        if (nLastSep != -1 && nLastSep > 0)
        {
            for (sal_Int32 i = 1; i < nLastSep; ++i)
            {
                OUString aTest = rStr.copy(0, i) + aEllipsisStr + rStr.copy(nLastSep);

                if (rfnGetTextWidth(aTest) > nMaxWidth)
                {
                    return (i > 1)
                               ? OUString(rStr.copy(0, i - 1) + aEllipsisStr + rStr.copy(nLastSep))
                               : OUString(aEllipsisStr + rStr.copy(nLastSep));
                }
            }
        }
    }

    // News Ellipsis (Structure Preserving)
    if (nStyle & DrawTextFlags::NewsEllipsis)
    {
        OUString aCurrentStr = rStr;
        auto lcl_IsSep = [](sal_Unicode c) { return c == '.'; };
        sal_Int32 nLastContent = nLen;

        while (nLastContent > 0)
        {
            nLastContent--;
            if (lcl_IsSep(aCurrentStr[nLastContent]))
                break;
        }

        while (nLastContent > 0 && lcl_IsSep(aCurrentStr[nLastContent - 1]))
        {
            nLastContent--;
        }

        OUString aLastStr = aCurrentStr.copy(nLastContent);

        if (rfnGetTextWidth(aEllipsisStr + aLastStr) <= nMaxWidth)
        {
            sal_Int32 nFirstContent = 0;

            while (nFirstContent < nLastContent)
            {
                nFirstContent++;
                if (lcl_IsSep(aCurrentStr[nFirstContent]))
                    break;
            }

            while (nFirstContent < nLastContent && lcl_IsSep(aCurrentStr[nFirstContent]))
            {
                nFirstContent++;
            }

            if (nFirstContent < nLastContent)
            {
                if (nFirstContent > 4)
                    nFirstContent = 4;

                OUString aFirstStr = aCurrentStr.copy(0, nFirstContent) + aEllipsisStr;
                OUString aTempStr = aFirstStr + aLastStr;

                if (rfnGetTextWidth(aTempStr) <= nMaxWidth)
                {
                    do
                    {
                        aCurrentStr = aTempStr;
                        if (nLastContent > aCurrentStr.getLength())
                            nLastContent = aCurrentStr.getLength();

                        while (nFirstContent < nLastContent)
                        {
                            nLastContent--;
                            if (lcl_IsSep(aCurrentStr[nLastContent]))
                                break;
                        }

                        while (nFirstContent < nLastContent
                               && lcl_IsSep(aCurrentStr[nLastContent - 1]))
                        {
                            nLastContent--;
                        }

                        if (nFirstContent < nLastContent)
                        {
                            aTempStr = aFirstStr + aCurrentStr.copy(nLastContent);

                            if (rfnGetTextWidth(aTempStr) > nMaxWidth)
                                break;
                        }
                    } while (nFirstContent < nLastContent);

                    return aCurrentStr;
                }
            }
        }
        // If News logic fails to find a fit, it falls through to End Ellipsis below.
    }

    // End Ellipsis (Priority: Text+Dots > Text(1) > Dots)
    // Iterate from full length down to 1 char.
    for (sal_Int32 i = nLen - 1; i >= 1; --i)
    {
        // Case A: Try fitting "Text..."
        // We only try this if i > 1.
        // Parity Rule: If i == 1 ("a"), we NEVER add dots, we just return "a" if it fits.
        if (i > 1)
        {
            OUString aWithDots = rStr.copy(0, i) + aEllipsisStr;
            if (rfnGetTextWidth(aWithDots) <= nMaxWidth)
                return aWithDots;
        }
        else
        {
            // Case B: i == 1. Check if "a" fits without dots.
            OUString aFirstChar = rStr.copy(0, 1);
            if (rfnGetTextWidth(aFirstChar) <= nMaxWidth)
                return aFirstChar;
        }
    }

    // Ultimate Fallback
    // If nothing above fit, and Clip is requested, return "a" (even if it's too wide).
    if (bClipText)
        return rStr.copy(0, 1);

    // Otherwise, return "..."
    return aEllipsisStr;
}

bool TextLayoutEngine::GetTextOutlines(const LayoutResources& rResources,
                                       basegfx::B2DPolyPolygonVector& rVector, const OUString& rStr,
                                       sal_Int32 nBase, sal_Int32 nIndex, sal_Int32 nLen,
                                       sal_uLong nLayoutWidth, KernArraySpan pDXArray,
                                       std::span<const sal_Bool> pKashidaArray)
{
    bool bRet = false;
    rVector.clear();

    if (nLen < 0)
        nLen = rStr.getLength() - nIndex;

    rVector.reserve(nLen);

    std::unique_ptr<SalLayout> pSalLayout;
    double nXOffset = 0;

    if (nBase != nIndex)
    {
        sal_Int32 nStart = std::min(nBase, nIndex);
        sal_Int32 nOfsLen = std::max(nBase, nIndex) - nStart;

        pSalLayout = Layout(rResources, TextSpan{ rStr, nStart, nOfsLen },
                            LayoutConstraints{ Point(0, 0), static_cast<tools::Long>(nLayoutWidth),
                                               pDXArray, pKashidaArray, SalLayoutFlags::NONE },
                            LayoutCacheData{ nullptr, nullptr }, RenderSelection{});

        if (pSalLayout)
        {
            nXOffset = pSalLayout->GetTextWidth();
            pSalLayout.reset();
            if (nBase < nIndex)
                nXOffset = -nXOffset;
        }
    }

    pSalLayout = Layout(rResources, TextSpan{ rStr, nIndex, nLen },
                        LayoutConstraints{ Point(0, 0), static_cast<tools::Long>(nLayoutWidth),
                                           pDXArray, pKashidaArray, SalLayoutFlags::NONE },
                        LayoutCacheData{ nullptr, nullptr }, RenderSelection{});

    if (pSalLayout)
    {
        bRet = pSalLayout->GetOutline(rVector);

        if (bRet)
        {
            basegfx::B2DHomMatrix aMatrix
                = CalculateOutlineTransform(*pSalLayout, rResources.rFontRealization, nXOffset);

            if (!aMatrix.isIdentity())
            {
                for (auto& elem : rVector)
                {
                    elem.transform(aMatrix);
                }
            }
        }
    }

    return bRet;
}

TextLayoutEngine::MnemonicGeometry TextLayoutEngine::GetMnemonicGeometry(
    std::function<double(tools::Long)> const& fnLogicWidthToDeviceSubPixel,
    std::function<tools::Long(tools::Long)> const& fnLogicWidthToDevicePixel,
    std::function<Point(const Point&)> const& fnLogicToPixel, const MnemonicDeviceParams& rParams,
    KernArraySpan aDXArray, sal_Int32 nRelPos, const Point& rLinePos, bool bTrailing)
{
    MnemonicGeometry aGeo;

    sal_Int32 lc_x1 = nRelPos ? static_cast<sal_Int32>(aDXArray[nRelPos - 1]) : 0;
    sal_Int32 lc_x2 = static_cast<sal_Int32>(aDXArray[nRelPos]);

    aGeo.nWidth = static_cast<tools::Long>(fnLogicWidthToDeviceSubPixel(std::abs(lc_x1 - lc_x2)));

    Point aTempPos = fnLogicToPixel(rLinePos);

    aGeo.nY = rParams.nOutOffY + aTempPos.Y() + fnLogicWidthToDevicePixel(rParams.nLogicalAscent);

    sal_Int32 nCharOffset = bTrailing ? std::max(lc_x1, lc_x2) : std::min(lc_x1, lc_x2);

    aGeo.nX = rParams.nOutOffX + aTempPos.X() + fnLogicWidthToDevicePixel(nCharOffset);

    return aGeo;
}

TextLayoutEngine::LayoutResult
TextLayoutEngine::CalculateLayout(const CoordinateMapper& rMapper, const LayoutRequest& rReq,
                                  const vcl::TextLayoutCommon& rLayout)
{
    LayoutResult aRes;
    OUString aCleanText = rReq.aText;

    if (rReq.nStyle & DrawTextFlags::Mnemonic)
        aCleanText = removeMnemonicFromString(aCleanText);

    aRes.aDisplayText = aCleanText;
    tools::Long nAvailableWidth = rReq.aTargetRect.GetWidth();
    tools::Long nLineHeight = rReq.nFontHeight ? rReq.nFontHeight : 1;

    if (rReq.nStyle & DrawTextFlags::MultiLine)
    {
        ImplMultiTextLineInfo aMultiLineInfo;
        // rLayout.GetTextLines must be marked const in TextLayoutCommon
        rLayout.GetTextLines(rReq.aTargetRect, nLineHeight, aMultiLineInfo, nAvailableWidth,
                             aCleanText, rReq.nStyle);

        aRes.nLineCount = aMultiLineInfo.Count();
        for (sal_Int32 i = 0; i < aRes.nLineCount; ++i)
        {
            aRes.nMaxWidth = std::max(aRes.nMaxWidth, aMultiLineInfo.GetLine(i).GetWidth());
        }

        // Multi-line Ellipsis Check
        sal_Int32 nMaxLines = static_cast<sal_Int32>(rReq.aTargetRect.GetHeight() / nLineHeight);
        if (nMaxLines > 0 && aRes.nLineCount > nMaxLines
            && (rReq.nStyle & DrawTextFlags::EndEllipsis))
        {
            aRes.bEllipsisGenerated = true;
            aRes.nLineCount = nMaxLines; // Clamp height to visible area
        }
    }
    else
    {
        aRes.nMaxWidth = rLayout.GetTextWidth(aCleanText, 0, -1);

        if (aRes.nMaxWidth > nAvailableWidth && (rReq.nStyle & TEXT_DRAW_ELLIPSIS))
        {
            aRes.aDisplayText = rLayout.GetEllipsisString(aCleanText, nAvailableWidth, rReq.nStyle);
            aRes.nMaxWidth = nAvailableWidth;
            aRes.bEllipsisGenerated = true;
        }
    }

    // Coordinate Calculation (Alignment & Rotation)
    // Account for the full block height in multi-line scenarios
    aRes.aTextRect = TextGeometry::AlignAndRotateTextRect(rReq.aTargetRect, aRes.nMaxWidth,
                                                          nLineHeight * aRes.nLineCount,
                                                          rReq.nStyle, rReq.nFontOrientation);

    aRes.aDrawPosition = aRes.aTextRect.TopLeft();

    // Mnemonic Geometry
    if (rReq.nMnemonicPos != -1
        && TextAnalyzer::IsMnemonicInRange(rReq.nMnemonicPos, 0, aRes.aDisplayText.getLength()))
    {
        MnemonicDeviceParams aParams{ rReq.nFontAscent, 0, 0 };
        KernArray aDXArray;
        rLayout.GetTextArray(aRes.aDisplayText, &aDXArray, 0, -1, true);

        aRes.aMnemonic = GetMnemonicGeometry(
            [&](tools::Long w) { return rMapper.LogicWidthToDeviceSubPixel(w); },
            [&](tools::Long w) { return rMapper.LogicWidthToDevicePixel(w); },
            [&](const Point& p) { return rMapper.LogicToPixel(p); }, aParams, aDXArray,
            rReq.nMnemonicPos, aRes.aDrawPosition);
        aRes.bHasMnemonic = true;
    }

    return aRes;
}

void TextLayoutEngine::FilterVisibleGlyphs(const OUString& rStr, sal_Int32 nIndex,
                                           const vcl::Region& rClip,
                                           const std::vector<tools::Rectangle>& rGlyphRects,
                                           std::vector<tools::Rectangle>& rOutVisibleRects,
                                           OUString* pOutVisibleText)
{
    bool bInserted = false;
    sal_Int32 nCurrentIdx = nIndex;

    for (auto it = rGlyphRects.begin(); it != rGlyphRects.end(); ++it, ++nCurrentIdx)
    {
        bool bAppend = false;

        // Standard visibility check: does the glyph ink overlap the clip?
        if (rClip.Overlaps(*it))
        {
            bAppend = true;
        }
        // Heuristic: Keep a space if it follows a visible character AND the next character is visible.
        // This prevents "floating" spaces while maintaining word separation.
        else if (rStr[nCurrentIdx] == ' ' && bInserted)
        {
            auto next = it;
            if (++next != rGlyphRects.end() && rClip.Overlaps(*next))
                bAppend = true;
        }

        if (bAppend)
        {
            rOutVisibleRects.push_back(*it);

            if (pOutVisibleText)
                *pOutVisibleText += OUStringChar(rStr[nCurrentIdx]);

            bInserted = true;
        }
    }
}

Point TextLayoutEngine::CalculateLayoutOrigin(const OutputDevice& rDev,
                                              const tools::Rectangle& rRect, tools::Long nTextWidth,
                                              tools::Long nTextHeight, DrawTextFlags nStyle,
                                              TextAlign eAlign)
{
    Point aPos = rRect.TopLeft();
    tools::Long nWidth = rRect.GetWidth();
    tools::Long nHeight = rRect.GetHeight();

    // Horizontal text alignment
    if (nStyle & DrawTextFlags::Right)
        aPos.AdjustX(nWidth - nTextWidth);
    else if (nStyle & DrawTextFlags::Center)
        aPos.AdjustX((nWidth - nTextWidth) / 2);

    // Vertical font alignment
    if (eAlign == ALIGN_BOTTOM)
        aPos.AdjustY(nTextHeight);
    else if (eAlign == ALIGN_BASELINE)
        aPos.AdjustY(rDev.GetFontMetric().GetAscent());

    if (nStyle & DrawTextFlags::Bottom)
        aPos.AdjustY(nHeight - nTextHeight);
    else if (nStyle & DrawTextFlags::VCenter)
        aPos.AdjustY((nHeight - nTextHeight) / 2);

    return aPos;
}

const LogicalFontInstance* pForcedFallback;
void TextLayoutEngine::CalculateMultiLineLayout(vcl::TextLayoutCommon& rLayout,
                                                MultiLineLayout& rRes,
                                                const tools::Rectangle& rRect,
                                                tools::Long nTextHeight, tools::Long nWidth,
                                                tools::Long nHeight, const OUString& rStr,
                                                DrawTextFlags nStyle)
{
    rRes.nResultStyle = nStyle;

    tools::Long nMaxTextWidth
        = rLayout.GetTextLines(rRect, nTextHeight, rRes.aLineInfo, nWidth, rStr, nStyle);
    sal_Int32 nLines = static_cast<sal_Int32>(nHeight / nTextHeight);
    rRes.nFormatLines = rRes.aLineInfo.Count();

    if (nLines <= 0)
        nLines = 1;

    if (rRes.nFormatLines > nLines)
    {
        if (nStyle & DrawTextFlags::EndEllipsis)
        {
            rRes.nFormatLines = nLines - 1;

            ImplTextLineInfo& rLineInfo = rRes.aLineInfo.GetLine(rRes.nFormatLines);
            OUString aLastLine = convertLineEnd(rStr.copy(rLineInfo.GetIndex()), LINEEND_LF);

            OUStringBuffer aLastLineBuffer(aLastLine);
            sal_Int32 nLastLineLen = aLastLineBuffer.getLength();
            for (sal_Int32 i = 0; i < nLastLineLen; i++)
            {
                if (aLastLineBuffer[i] == '\n')
                    aLastLineBuffer[i] = ' ';
            }
            aLastLine = aLastLineBuffer.makeStringAndClear();

            rRes.aLastLine = rLayout.GetEllipsisString(aLastLine, nWidth, nStyle);

            rRes.nResultStyle &= ~DrawTextFlags(DrawTextFlags::VCenter | DrawTextFlags::Bottom);
            rRes.nResultStyle |= DrawTextFlags::Top;
        }
    }
    else
    {
        if (nMaxTextWidth <= nWidth)
            rRes.nResultStyle &= ~DrawTextFlags::Clip;
    }

    if (rRes.nFormatLines * nTextHeight > nHeight)
        rRes.nResultStyle |= DrawTextFlags::Clip;
}

std::unique_ptr<SalLayout> TextLayoutEngine::GetStrikeoutCharLayout(const LayoutResources& rRes,
                                                                    tools::Long nTargetWidth,
                                                                    FontStrikeout eStrikeout)
{
    if (nTargetWidth <= 0)
        return nullptr;

    const char cStrikeoutChar = (eStrikeout == STRIKEOUT_SLASH) ? '/' : 'X';
    static const int nTestStrLen = 4;
    static const int nMaxStrikeStrLen = 2048;

    sal_Unicode aChars[nMaxStrikeStrLen + 1]; // +1 for safety
    for (int i = 0; i < nTestStrLen; ++i)
        aChars[i] = cStrikeoutChar;

    OUString aStrikeoutTest(aChars, nTestStrLen);

    // Measure the width of the strikeout character
    vcl::text::TextSpan aSpan(aStrikeoutTest, 0, nTestStrLen);
    vcl::text::LayoutConstraints aConstraints(Point(0, 0), 0, {}, {}, SalLayoutFlags::NONE);
    vcl::text::LayoutCacheData aCache;
    vcl::text::RenderSelection aSel;

    std::unique_ptr<SalLayout> pLayout
        = TextLayoutEngine::Layout(rRes, aSpan, aConstraints, aCache, aSel);

    tools::Long nStrikeoutWidth = 0;
    if (pLayout)
    {
        nStrikeoutWidth = pLayout->GetTextWidth() / nTestStrLen;
    }

    if (nStrikeoutWidth <= 0)
        return nullptr;

    int nStrikeStrLen = (nTargetWidth + (nStrikeoutWidth - 1)) / nStrikeoutWidth;

    if (nStrikeStrLen > nMaxStrikeStrLen)
        nStrikeStrLen = nMaxStrikeStrLen;
    else if (nStrikeStrLen < 0)
        nStrikeStrLen = 0;

    // Build the full strikeout string
    for (int i = nTestStrLen; i < nStrikeStrLen; ++i)
        aChars[i] = cStrikeoutChar;

    const OUString aStrikeoutText(aChars, nStrikeStrLen);
    vcl::text::TextSpan aFinalSpan(aStrikeoutText, 0, nStrikeStrLen);

    // Create the final layout with BiDiStrong forced
    vcl::text::LayoutConstraints aFinalConstraints(Point(0, 0), 0, {}, {},
                                                   SalLayoutFlags::BiDiStrong);

    return TextLayoutEngine::Layout(rRes, aFinalSpan, aFinalConstraints, aCache, aSel);
}

void TextLayoutEngine::GetGlyphRectsFromLayout(const SalLayout& rLayout, const Point& rStartPt,
                                               const OUString& rStr, sal_Int32 nLen,
                                               std::vector<tools::Rectangle>& rRects)
{
    rRects.clear();
    rRects.reserve(nLen);

    if (nLen <= 0)
        return;

    // Fix 1: Use double vector (as per vcllayout.hxx) and pass string
    std::vector<double> aDXArray;
    rLayout.FillDXArray(&aDXArray, rStr);

    tools::Long nX = rStartPt.X();

    // Fix 2: GetBoundRect returns bool and takes B2DRectangle ref
    basegfx::B2DRectangle aBounds;
    rLayout.GetBoundRect(aBounds);

    // Convert B2DRectangle to integers relative to start point
    tools::Long nTop = rStartPt.Y() + static_cast<tools::Long>(aBounds.getMinY());
    tools::Long nBottom = rStartPt.Y() + static_cast<tools::Long>(aBounds.getMaxY());

    // Safety check for empty bounds
    if (aBounds.isEmpty())
    {
        nTop = rStartPt.Y();
        nBottom = rStartPt.Y() + 10; // Fallback height
    }

    tools::Long nPrevX = 0;
    size_t nArraySize = aDXArray.size();

    for (int i = 0; i < nLen; ++i)
    {
        if (static_cast<size_t>(i) >= nArraySize)
            break;

        // FillDXArray returns absolute positions
        tools::Long nCurrX = static_cast<tools::Long>(aDXArray[i]);

        rRects.emplace_back(nX + nPrevX, nTop, nX + nCurrX, nBottom);

        nPrevX = nCurrX;
    }
}
} // namespace vcl::text

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
