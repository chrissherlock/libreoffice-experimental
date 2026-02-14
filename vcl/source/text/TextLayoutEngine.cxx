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
#include <o3tl/unit_conversion.hxx>

#include <vcl/outdev.hxx>
#include <vcl/fntstyle.hxx>
#include <vcl/glyphitem.hxx>
#include <vcl/font.hxx>
#include <vcl/vclenum.hxx>
#include <vcl/svapp.hxx>
#include <vcl/mnemonic.hxx>
#include <vcl/text/TextSpan.hxx>
#include <vcl/text/LayoutCacheData.hxx>
#include <vcl/text/CaretManager.hxx>

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
#include <vcl/text/TextGeometry.hxx>
#include <text/TextJustifier.hxx>
#include <text/TextAnalyzer.hxx>
#include <text/TextLayoutPositioning.hxx>
#include <vcl/text/DefaultFallbackStrategy.hxx>
#include <CoordinateMapper.hxx>
#include <GraphicsState.hxx>
#include <text/TextLayoutRequest.hxx>

#include <unicode/uchar.h>

#include <cstdio>
#include <utility>

namespace vcl::text
{
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

    TextJustifier::JustifyLayout(rLayout, rArgs);
    TextJustifier::ApplyHorizontalOffset(rLayout, rArgs, aPos);
    TextJustifier::SetAnchorPoint(rLayout, aPos);
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
    TextJustifier::PrepareJustification(rRes, rConstraints.pDXArray, rConstraints.pKashidaArray,
                                        rSpan.Index, nLen, rSelection.DrawMinCharPos,
                                        rSelection.DrawEndCharPos, aLayoutArgs, nEndGlyphCoord);

    std::unique_ptr<SalLayout> pSalLayout = PerformTextLayout(rRes, aLayoutArgs, pEffectiveGlyphs);

    if (!pSalLayout)
        return nullptr;

    if (rConstraints.Flags & SalLayoutFlags::GlyphItemsOnly)
        return pSalLayout;

    ApplyPositioning(rRes, *pSalLayout, aLayoutArgs, rConstraints.LogicalPos, nEndGlyphCoord);

    TrackLayoutFonts(rRes.rGraphicsState.maFont, pSalLayout.get());

    return pSalLayout;
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
        TextJustifier::ZeroFillKernArray(pKernArray, nNormalizedPartLen);
        return 0.0;
    }

    lcl_convertBoundRectToLogic(*pSalLayout, rRes.rMapper, pBounds);

    return FillPartialTextArray(rRes, *pSalLayout, pKernArray, rSpan.Index, nLen, nPartIndex,
                                nNormalizedPartLen, bCaret ? rSpan.Text : OUString());
}

void TextLayoutEngine::GetCaretPositions(const LayoutResources& rRes,
                                         const vcl::text::TextSpan& rSpan,
                                         std::vector<double>& rCaretPositions,
                                         const LayoutCacheData& rCache)
{
    std::unique_ptr<SalLayout> pGeneratedLayout;
    const SalLayout* pLayout = nullptr;

    // Prepare default arguments for Layout()
    vcl::text::LayoutConstraints aConstraints;
    vcl::text::RenderSelection aSelection;

    // Use the passed rSpan directly
    if (rCache.pGlyphs)
    {
        pGeneratedLayout = Layout(rRes, rSpan, aConstraints, rCache, aSelection);
        pLayout = pGeneratedLayout.get();
    }
    else
    {
        pGeneratedLayout = Layout(rRes, rSpan, aConstraints, rCache, aSelection);
        pLayout = pGeneratedLayout.get();
    }

    if (!pLayout)
        return;

    // Delegate to the component
    CaretManager::GetCaretPositions(rRes, rSpan, rCaretPositions, *pLayout);
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

void TextLayoutEngine::InitializeTextLineMetrics(const LogicalFontInstance* pFontInstance,
                                                 const vcl::Font& rFont, tools::Long nDPIY,
                                                 tools::Long nSpaceWidth, tools::Long nBulletWidth)
{
    if (!pFontInstance || !pFontInstance->mxFontMetric)
        return;

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

} // namespace vcl::text

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
