#include <CoordinateMapper.hxx>
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
#include <i18nlangtag/mslangid.hxx>
#include <i18nutil/digitlocalization.hxx>
#include <i18nutil/unicode.hxx>

#include <vcl/outdev.hxx>
#include <vcl/fntstyle.hxx>
#include <vcl/glyphitem.hxx>
#include <vcl/font.hxx>
#include <vcl/vclenum.hxx>
#include <vcl/svapp.hxx>

#include <font/FontController.hxx>
#include <font/LogicalFontInstance.hxx>
#include <font/FontSelectPattern.hxx>
#include <font/PhysicalFontFace.hxx>
#include <salgdi.hxx>
#include <sallayout.hxx>
#include <text/TextLayoutEngine.hxx>
#include <GraphicsState.hxx>
#include <text/TextLayoutRequest.hxx>

#include <unicode/uchar.h>

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

std::vector<Point> TextLayoutEngine::GetEmphasisMarkPositions(const SalLayout& rLayout,
                                                              long nAscent, long nDescent,
                                                              FontEmphasisMark nStyle)
{
    std::vector<Point> aPositions;

    // Explicit bool cast for o3tl flags
    bool bBelow = bool(nStyle & FontEmphasisMark::PosBelow);
    long nYOffset = bBelow ? nDescent : -nAscent;

    // Empirical visual centering logic
    long nSpacing = (nAscent + nDescent) / 4;
    nYOffset += bBelow ? (nSpacing / 2) : -(nSpacing / 2);

    int nIterator = 0;
    const GlyphItem* pGlyph = nullptr;
    basegfx::B2DPoint aPos;

    while (rLayout.GetNextGlyph(&pGlyph, aPos, nIterator))
    {
        if (!pGlyph)
            continue;

        // Convert floating point positions to integer Device Pixels
        long nX = static_cast<long>(aPos.getX()) + (pGlyph->origWidth() / 2);
        long nY = static_cast<long>(aPos.getY()) + nYOffset;

        aPositions.emplace_back(nX, nY);
    }

    return aPositions;
}

void TextLayoutEngine::InitializeFontMetrics(
    LogicalFontInstance* pFontInstance, const vcl::Font& rFont, long nDPIY, long nPixelWidth,
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

SalLayoutFlags TextLayoutEngine::GetBiDiLayoutFlags(vcl::text::ComplexTextLayoutFlags eLayoutMode,
                                                    std::u16string_view rStr,
                                                    const sal_Int32 nMinIndex,
                                                    const sal_Int32 nEndIndex)
{
    SalLayoutFlags nLayoutFlags = SalLayoutFlags::NONE;

    if (eLayoutMode & vcl::text::ComplexTextLayoutFlags::BiDiRtl)
    {
        nLayoutFlags |= SalLayoutFlags::BiDiRtl;
    }
    if (eLayoutMode & vcl::text::ComplexTextLayoutFlags::BiDiStrong)
    {
        nLayoutFlags |= SalLayoutFlags::BiDiStrong;
    }
    else if (!(eLayoutMode & vcl::text::ComplexTextLayoutFlags::BiDiRtl))
    {
        // Disable Bidi if no RTL hint and only known LTR codes used.
        bool bAllLtr = true;
        for (sal_Int32 i = nMinIndex; i < nEndIndex; i++)
        {
            // [0x0000, 0x052F] are Latin, Greek and Cyrillic.
            if (rStr[i] > 0x052F)
            {
                bAllLtr = false;
                break;
            }
        }

        if (bAllLtr)
            nLayoutFlags |= SalLayoutFlags::BiDiStrong;
    }

    return nLayoutFlags;
}

SalLayoutFlags TextLayoutEngine::CalculateLayoutFlags(
    const vcl::GraphicsState& rGraphicsState, const vcl::font::FontRealization& rFontRealization,
    bool bRTLWindow, std::u16string_view rStr, sal_Int32 nMinIndex, sal_Int32 nEndIndex,
    SalLayoutFlags nExistingFlags)
{
    SalLayoutFlags nFlags = nExistingFlags;

    nFlags |= TextLayoutEngine::GetBiDiLayoutFlags(rFontRealization.eLayoutMode, rStr, nMinIndex,
                                                   nEndIndex);

    if (!rGraphicsState.maFont.IsKerning())
        nFlags |= SalLayoutFlags::DisableKerning;

    if (rGraphicsState.maFont.GetKerning() & FontKerning::Asian)
        nFlags |= SalLayoutFlags::KerningAsian;

    if (rGraphicsState.maFont.IsVertical())
        nFlags |= SalLayoutFlags::Vertical;

    if (rGraphicsState.maFont.IsFixKerning()
        || (rFontRealization.mxFont
            && rFontRealization.mxFont->GetFontSelectPattern().GetPitch() == PITCH_FIXED))
    {
        nFlags |= SalLayoutFlags::DisableLigatures;
    }

    bool bRightAlign
        = bool(rFontRealization.eLayoutMode & vcl::text::ComplexTextLayoutFlags::BiDiRtl);

    if (rFontRealization.eLayoutMode & vcl::text::ComplexTextLayoutFlags::TextOriginLeft)
        bRightAlign = false;
    else if (rFontRealization.eLayoutMode & vcl::text::ComplexTextLayoutFlags::TextOriginRight)
        bRightAlign = true;

    bRightAlign ^= bRTLWindow;

    if (bRightAlign)
        nFlags |= SalLayoutFlags::RightAlign;

    return nFlags;
}

void TextLayoutEngine::ApplyDigitLocalization(const vcl::GraphicsState& rGraphicsState,
                                              OUString& rStr, sal_Int32 nMinIndex,
                                              sal_Int32& rEndIndex)
{
    if (rGraphicsState.meTextLanguage)
    {
        sal_Int32 nSubstringLen = rEndIndex - nMinIndex;
        rStr = i18nutil::LocalizeDigitsInString(rStr, rGraphicsState.meTextLanguage, nMinIndex,
                                                nSubstringLen);
        rEndIndex = nMinIndex + nSubstringLen;
    }
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

    vcl::text::TextLayoutEngine::ApplyDigitLocalization(rState, rStr, nMinIndex, nEndIndex);

    nFlags = vcl::text::TextLayoutEngine::CalculateLayoutFlags(rState, rRealization, bRTL, rStr,
                                                               nMinIndex, nEndIndex, nFlags);

    vcl::text::TextLayoutRequest aLayoutArgs(rStr, nMinIndex, nEndIndex, nFlags,
                                             rState.maFont.GetLanguageTag(), pCache);

    Degree10 nOrientation = rRealization.mxFont ? rRealization.mxFont->mnOrientation : 0_deg10;
    aLayoutArgs.SetOrientation(nOrientation);

    aLayoutArgs.SetLayoutWidth(nPixelWidth);

    return aLayoutArgs;
}

rtl::Reference<LogicalFontInstance>
TextLayoutEngine::FindFallbackFont(const FontLookupCriteria& rCriteria, int nFallbackLevel,
                                   OUString& rMissingCodes, bool& rHasUsedFallback,
                                   SalLayoutGlyphsImpl* pGlyphsImpl)
{
    ImplFontCache& rFontCache = rCriteria.rCache;
    LogicalFontInstance* pBaseFont = rCriteria.pReferenceFont;
    const rtl::Reference<LogicalFontInstance>& pForcedFallback = rCriteria.pPriorityFallback;

    if (!pBaseFont)
    {
        // Without a reference font, we can only return the forced fallback if available.
        // We cannot perform a frantic search for "similar" fonts without a reference pattern.
        if (!rHasUsedFallback && pForcedFallback)
        {
            rHasUsedFallback = true;
            return pForcedFallback;
        }
        return nullptr;
    }

    vcl::font::FontSelectPattern aFontSelData(pBaseFont->GetFontSelectPattern());

    rtl::Reference<LogicalFontInstance> pFallbackFont;

    if (!rHasUsedFallback && pForcedFallback)
    {
        pFallbackFont = pForcedFallback;
        rHasUsedFallback = true;
    }
    else if (pGlyphsImpl != nullptr)
    {
        pFallbackFont = pGlyphsImpl->GetFont();
    }

    if (!pFallbackFont)
    {
        pFallbackFont = rFontCache.GetGlyphFallbackFont(rCriteria, aFontSelData, nFallbackLevel,
                                                        rMissingCodes);
    }

    return pFallbackFont;
}

OUString TextLayoutEngine::IdentifyMissingChars(vcl::text::TextLayoutRequest& rArgs)
{
    OUStringBuffer aMissingCodeBuf;

    for (const auto& rRun : rArgs.maRuns)
    {
        for (auto i = rRun.m_nMinRunPos; i < rRun.m_nEndRunPos; ++i)
        {
            aMissingCodeBuf.append(rArgs.mrStr[i]);
        }
    }

    return aMissingCodeBuf.makeStringAndClear();
}

void TextLayoutEngine::MergeFallback(std::unique_ptr<MultiSalLayout>& rMultiSalLayout,
                                     std::unique_ptr<SalLayout>& rBaseLayout,
                                     std::unique_ptr<SalLayout> pFallback,
                                     const ImplLayoutRuns& rRuns, bool bIsLastLevel)
{
    if (!rMultiSalLayout)
        rMultiSalLayout.reset(new MultiSalLayout(std::move(rBaseLayout)));

    rMultiSalLayout->AddFallback(std::move(pFallback), rRuns);

    if (bIsLastLevel)
        rMultiSalLayout->SetIncomplete(true);
}

std::unique_ptr<SalLayout> TextLayoutEngine::ResolveMissingGlyphs(
    std::unique_ptr<SalLayout> pBaseLayout, vcl::text::TextLayoutRequest& rLayoutArgs,
    const SalLayoutGlyphs* pGlyphs, const FontLookupCriteria& rCriteria, ILayoutFactory& rFactory)
{
    LogicalFontInstance* pBaseFont = rCriteria.pReferenceFont;

    std::unique_ptr<MultiSalLayout> pMultiSalLayout;
    ImplLayoutRuns aSavedRuns = rLayoutArgs.maRuns;
    rLayoutArgs.PrepareFallback(nullptr);
    rLayoutArgs.mnFlags |= SalLayoutFlags::ForFallback;

    OUString aMissingCodes = IdentifyMissingChars(rLayoutArgs);

    SalLayoutGlyphsImpl* pGlyphsImpl = pGlyphs ? pGlyphs->Impl(1) : nullptr;
    bool bHasUsedFallback = false;

    for (int nFallbackLevel = 1; nFallbackLevel < MAX_FALLBACK; ++nFallbackLevel)
    {
        OUString oldMissingCodes = aMissingCodes;

        rtl::Reference<LogicalFontInstance> pFallbackFont = FindFallbackFont(
            rCriteria, nFallbackLevel, aMissingCodes, bHasUsedFallback, pGlyphsImpl);

        if (!pFallbackFont)
            break;

        if (nFallbackLevel < MAX_FALLBACK - 1)
        {
            if (pBaseFont->GetFontFace() == pFallbackFont->GetFontFace())
            {
                if (aMissingCodes != oldMissingCodes)
                    aMissingCodes = oldMissingCodes;
                continue;
            }
        }

        rFactory.SetFont(pFallbackFont.get(), nFallbackLevel);
        std::unique_ptr<SalLayout> pFallback = rFactory.CreateLayout(nFallbackLevel);

        if (pFallback)
        {
            rLayoutArgs.ResetPos();
            if (pFallback->LayoutText(rLayoutArgs, pGlyphsImpl))
            {
                MergeFallback(pMultiSalLayout, pBaseLayout, std::move(pFallback),
                              rLayoutArgs.maRuns, (nFallbackLevel == MAX_FALLBACK - 1));
            }
        }

        if (pGlyphs)
            pGlyphsImpl = pGlyphs->Impl(nFallbackLevel + 1);
        if (!rLayoutArgs.PrepareFallback(pGlyphsImpl))
            break;
    }

    if (pMultiSalLayout)
    {
        if (pMultiSalLayout->LayoutText(rLayoutArgs, nullptr))
            pBaseLayout = std::move(pMultiSalLayout);
        else
            pBaseLayout = pMultiSalLayout->ReleaseBaseLayout();
    }

    rLayoutArgs.maRuns = std::move(aSavedRuns);
    return pBaseLayout;
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

namespace
{
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

} // end anonymous namespace

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

// Local factory implementation for resolving fallbacks
class GraphicLayoutFactory : public ILayoutFactory
{
    std::function<SalGraphics*()> m_fnGetGraphics;

public:
    GraphicLayoutFactory(std::function<SalGraphics*()> fn)
        : m_fnGetGraphics(std::move(fn))
    {
    }

    virtual std::unique_ptr<SalLayout> CreateLayout(int nFallbackLevel) override
    {
        SalGraphics* pGraphics = m_fnGetGraphics();
        return pGraphics ? pGraphics->GetTextLayout(nFallbackLevel) : nullptr;
    }

    virtual void SetFont(LogicalFontInstance* pFont, int nFallbackLevel) override
    {
        SalGraphics* pGraphics = m_fnGetGraphics();
        if (pGraphics)
            pGraphics->SetFont(pFont, nFallbackLevel);
    }
};

std::unique_ptr<SalLayout> TextLayoutEngine::ResolveFallbacks(const LayoutResources& rRes,
                                                              std::unique_ptr<SalLayout> pLayout,
                                                              vcl::text::TextLayoutRequest& rArgs,
                                                              const SalLayoutGlyphs* pGlyphs)
{
    if (rArgs.HasFallbackRun() && rRes.pFont->GetFontSelectPattern().mnHeight >= 3)
    {
        GraphicLayoutFactory aFactory(rRes.fnGetGraphics);

        FontLookupCriteria aCriteria
            = { *rRes.pFontCache, rRes.pFontCollection,
                const_cast<LogicalFontInstance*>(rRes.pFont),
                rtl::Reference<LogicalFontInstance>(
                    const_cast<LogicalFontInstance*>(rRes.pForcedFallback)) };

        return ResolveMissingGlyphs(std::move(pLayout), rArgs, pGlyphs, aCriteria, aFactory);
    }
    return pLayout;
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
    // 1. Create Base Layout
    std::unique_ptr<SalLayout> pSalLayout = CreateBaseLayout(rRes);

    // 2. Initial Layout Run
    if (pSalLayout && !pSalLayout->LayoutText(rArgs, pGlyphs ? pGlyphs->Impl(0) : nullptr))
        pSalLayout.reset();

    if (!pSalLayout)
        return nullptr;

    // 3. Fallback Resolution
    if (rArgs.HasFallbackRun() && rRes.pFont->GetFontSelectPattern().mnHeight >= 3)
    {
        GraphicLayoutFactory aFactory(rRes.fnGetGraphics);

        FontLookupCriteria aCriteria
            = { *rRes.pFontCache, rRes.pFontCollection,
                const_cast<LogicalFontInstance*>(rRes.pFont),
                rtl::Reference<LogicalFontInstance>(
                    const_cast<LogicalFontInstance*>(rRes.pForcedFallback)) };

        pSalLayout
            = ResolveMissingGlyphs(std::move(pSalLayout), rArgs, pGlyphs, aCriteria, aFactory);
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
} // namespace vcl::text
