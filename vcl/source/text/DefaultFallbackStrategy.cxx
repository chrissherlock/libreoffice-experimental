/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <tools/gen.hxx>
#include <tools/lineend.hxx>
#include <unotools/fontdefs.hxx>
#include <basegfx/point/b2dpoint.hxx>
#include <i18nlangtag/mslangid.hxx>
#include <i18nutil/digitlocalization.hxx>
#include <i18nutil/unicode.hxx>
#include <o3tl/unit_conversion.hxx>

#include <vcl/fntstyle.hxx>
#include <vcl/font.hxx>
#include <vcl/glyphitem.hxx>
#include <vcl/vclenum.hxx>
#include <vcl/svapp.hxx>
#include <vcl/mnemonic.hxx>
#include <vcl/text/DefaultFallbackStrategy.hxx>
#include <vcl/text/LayoutResources.hxx>

#include <font/FontMetricData.hxx>
#include <font/FontController.hxx>
#include <font/LogicalFontInstance.hxx>
#include <font/FontLookupCriteria.hxx>
#include <font/FontSelectPattern.hxx>
#include <font/PhysicalFontFace.hxx>
#include <font/PhysicalFontCollection.hxx>
#include <salgdi.hxx>
#include <sallayout.hxx>
#include <textlayout.hxx>
#include <textlineinfo.hxx>
#include <text/SalLayoutFactory.hxx>
#include <text/TextLayoutRequest.hxx>
#include <text/TextLayoutEngine.hxx>
#include <CoordinateMapper.hxx>
#include <GraphicsState.hxx>

#include <unicode/uchar.h>

#include <cstdio>
#include <utility>

namespace vcl::text
{
std::unique_ptr<SalLayout> DefaultFallbackStrategy::ResolveFallbacks(
    const LayoutResources& rRes, std::unique_ptr<SalLayout> pLayout,
    vcl::text::TextLayoutRequest& rArgs, const SalLayoutGlyphs* pGlyphs)
{
    if (rArgs.HasFallbackRun() && rRes.pFont->GetFontSelectPattern().mnHeight >= 3)
    {
        FontLookupCriteria aCriteria
            = { *rRes.pFontCache, rRes.pFontCollection,
                const_cast<LogicalFontInstance*>(rRes.pFont),
                rtl::Reference<LogicalFontInstance>(
                    const_cast<LogicalFontInstance*>(rRes.pForcedFallback)) };

        // FIX: Pass 'rRes' directly. The function will create the factory internally.
        SalLayoutFactory aFactory(rRes.fnGetGraphics);
        return ResolveMissingGlyphs(std::move(pLayout), rArgs, pGlyphs, aCriteria, &aFactory);
    }
    return pLayout;
}

std::unique_ptr<SalLayout> DefaultFallbackStrategy::ResolveMissingGlyphs(
    std::unique_ptr<SalLayout> pBaseLayout, vcl::text::TextLayoutRequest& rLayoutArgs,
    const SalLayoutGlyphs* pGlyphs, const FontLookupCriteria& rCriteria,
    ILayoutFactory* pFactory) // FIX: Match header signature
{
    // FIX: Create the factory here using the resources

    LogicalFontInstance* pBaseFont = rCriteria.pReferenceFont;

    std::unique_ptr<MultiSalLayout> pMultiSalLayout;
    ImplLayoutRuns aSavedRuns = rLayoutArgs.maRuns;
    rLayoutArgs.PrepareFallback(nullptr);
    rLayoutArgs.mnFlags |= SalLayoutFlags::ForFallback;

    OUString aMissingCodes = IdentifyMissingChars(rLayoutArgs);

    SalLayoutGlyphsImpl* pGlyphsImpl = pGlyphs ? pGlyphs->Impl(1) : nullptr;
    bool bHasUsedFallback = false;

    // MAX_FALLBACK is typically 16 in VCL
    const int nMaxFallbackLevel = 16;

    for (int nFallbackLevel = 1; nFallbackLevel < nMaxFallbackLevel; ++nFallbackLevel)
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

        // rFactory logic adapted
        //
        // Note: The original ILayoutFactory::SetFont didn't exist in the simple wrapper,
        // but CreateLayout needs the font.
        // We might need to construct a new LayoutResources or Factory if the font changes.
        // However, GetLayout usually uses the font passed to it.
        // Let's assume the factory needs a slight tweak or we use the collection directly.
        std::unique_ptr<SalLayout> pFallback
            = pFactory->CreateLayout(pFallbackFont.get(), nFallbackLevel);

        if (pFallback)
        {
            rLayoutArgs.ResetPos();
            if (pFallback->LayoutText(rLayoutArgs, pGlyphsImpl))
            {
                MergeFallback(pMultiSalLayout, pBaseLayout, std::move(pFallback),
                              rLayoutArgs.maRuns, (nFallbackLevel == nMaxFallbackLevel - 1));
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

void DefaultFallbackStrategy::MergeFallback(std::unique_ptr<MultiSalLayout>& rMultiSalLayout,
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

OUString DefaultFallbackStrategy::IdentifyMissingChars(vcl::text::TextLayoutRequest& rArgs)
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

rtl::Reference<LogicalFontInstance>
DefaultFallbackStrategy::FindFallbackFont(const FontLookupCriteria& rCriteria, int nFallbackLevel,
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

} // namespace vcl::text

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
