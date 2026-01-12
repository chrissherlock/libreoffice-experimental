/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <vcl/font.hxx>

#include <font/LogicalFontInstance.hxx>
#include <impfontcache.hxx>
#include <salgdi.hxx>
#include <FontController.hxx>

#include <tuple>

namespace vcl::font
{
FontController::FontController()
    : mxFontInstance(nullptr)
    , meTextAlign(TextAlign::ALIGN_TOP)
{
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

void FontController::RealizeFont(ImplFontCache& rCache, const vcl::Font& rFont, const Size& rSize,
                                 float fExactHeight, bool bNonAntialiased)
{
    if (!mxFontCollection)
        return; // Cannot realize without a collection

    mxFontInstance = rCache.GetFontInstance(mxFontCollection.get(), rFont, rSize, fExactHeight,
                                            bNonAntialiased);
}

void FontController::InitializeInstance(LogicalFontInstance* pFontInstance, SalGraphics* pGraphics)
{
    if (!pFontInstance || !pGraphics)
        return;

    // Guard: only initialize if not already done
    if (pFontInstance->mbInit)
        return;

    pFontInstance->mbInit = true;

    // Apply orientation from the selection pattern
    pFontInstance->mxFontMetric->SetOrientation(
        pFontInstance->GetFontSelectPattern().mnOrientation);

    // Fetch physical metrics directly from the graphics driver
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

    // ALIGN_BASELINE sets offsets to 0, which is our default

    // 3. Handle Rotation/Orientation
    if (pFontInstance->mnOrientation && (nHorizontalOffset || nVerticalOffset))
    {
        Point aOriginPt(0, 0);
        aOriginPt.RotateAround(nHorizontalOffset, nVerticalOffset, pFontInstance->mnOrientation);
    }

    return std::make_tuple(nHorizontalOffset, nVerticalOffset, nEmphasisAscent, nEmphasisDescent);
}

} // end namespace vcl::font

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
