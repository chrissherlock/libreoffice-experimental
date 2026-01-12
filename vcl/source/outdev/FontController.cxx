/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <comphelper/configuration.hxx>

#include <vcl/font.hxx>
#include <vcl/fntstyle.hxx>
#include <vcl/metric.hxx>
#include <vcl/rendercontext/AntialiasingFlags.hxx>
#include <vcl/settings.hxx>

#include <font/LogicalFontInstance.hxx>
#include <impfontcache.hxx>
#include <salgdi.hxx>
#include <CoordinateMapper.hxx>
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

bool FontController::ShouldDisableAntialiasing(AntialiasingFlags eAntialisingFlags,
                                               const StyleSettings& rStyleSettings,
                                               tools::Long nHeight) const
{
    // decide if antialiasing is appropriate
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

    // Delegate directly to the SalGraphics driver
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

} // end namespace vcl::font

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
