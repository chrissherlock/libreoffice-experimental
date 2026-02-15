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
#include <text/FontMappingTracker.hxx>
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

    double nPixelWidth = CoordinateMapper::CalculateLayoutWidth(rRes, rConstraints.LogicalWidth);

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

    TextGeometry::ApplyPositioning(rRes, *pSalLayout, aLayoutArgs, rConstraints.LogicalPos,
                                   nEndGlyphCoord);

    FontMappingTracker::TrackLayoutFonts(rRes.rGraphicsState.maFont, pSalLayout.get());

    return pSalLayout;
}

} // namespace vcl::text

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
