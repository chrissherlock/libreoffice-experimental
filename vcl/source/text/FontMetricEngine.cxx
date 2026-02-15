/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <i18nlangtag/mslangid.hxx>

#include <vcl/font.hxx>

#include <text/FontMetricEngine.hxx>
#include <font/LogicalFontInstance.hxx>
#include <font/FontMetricData.hxx>

namespace vcl::text
{
void FontMetricEngine::InitializeFontMetrics(
    const LogicalFontInstance* pFontInstance, const vcl::Font& rFont, tools::Long nDPIY,
    tools::Long nPixelWidth, std::function<tools::Long(const OUString&)> const& fnGetTextWidth,
    std::function<void(tools::Rectangle&, const OUString&)> const& fnGetBoundRect)
{
    if (!pFontInstance || !pFontInstance->mxFontMetric)
        return;

    // Bullet Offset: Centering the bullet character between spaces
    tools::Long nSpaceW = fnGetTextWidth(u" "_ustr);
    tools::Long nBulletW = fnGetTextWidth(u"\x00b7"_ustr);
    tools::Long nBulletOffset = (nSpaceW - nBulletW) >> 1;

    pFontInstance->mxFontMetric->ImplInitTextLineSize(pFontInstance, nDPIY, rFont, nBulletOffset);
    pFontInstance->mxFontMetric->ImplInitAboveTextLineSize(nDPIY, nPixelWidth);

    // CJK Centering for full-width fullstops
    bool bCentered = true;
    if (MsLangId::isCJK(rFont.GetLanguage()))
    {
        tools::Rectangle aRect;
        fnGetBoundRect(aRect, u"\x3001"_ustr);
        const auto nH = rFont.GetFontSize().Height();
        const auto nB = aRect.Left();
        bCentered = nB > (((nH >> 1) + nH) >> 3);
    }
    pFontInstance->mxFontMetric->SetFullstopCenteredFlag(bCentered);
}

void FontMetricEngine::InitializeTextLineMetrics(const LogicalFontInstance* pFontInstance,
                                                 const vcl::Font& rFont, tools::Long nDPIY,
                                                 tools::Long nSpaceWidth, tools::Long nBulletWidth)
{
    if (!pFontInstance || !pFontInstance->mxFontMetric)
        return;

    tools::Long nBulletOffset = (nSpaceWidth - nBulletWidth) >> 1;
    pFontInstance->mxFontMetric->ImplInitTextLineSize(pFontInstance, nDPIY, rFont, nBulletOffset);
}

void FontMetricEngine::InitializeAboveTextLineMetrics(const LogicalFontInstance* pFontInstance,
                                                      tools::Long nDPIY, tools::Long nUnderlineSize)
{
    if (!pFontInstance || !pFontInstance->mxFontMetric)
        return;

    pFontInstance->mxFontMetric->ImplInitAboveTextLineSize(nDPIY, nUnderlineSize);
}

tools::Long FontMetricEngine::GetAlignmentOffset(TextAlign eAlign, tools::Long nAscent,
                                                 tools::Long nDescent)
{
    if (eAlign == ALIGN_BOTTOM)
        return -nDescent;
    if (eAlign == ALIGN_TOP)
        return nAscent;
    return 0;
}

} // namespace vcl::text

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
