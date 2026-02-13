/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <vcl/text/LayoutResources.hxx>

#include <sallayout.hxx>
#include <text/TextJustifier.hxx>
#include <text/TextLayoutEngine.hxx>
#include <text/TextLayoutRequest.hxx>
#include <text/TextLayoutPositioning.hxx>
#include <CoordinateMapper.hxx>
#include <justificationdata.hxx>

#include <unicode/uchar.h>

#include <cmath>

namespace vcl::text
{
TextJustifier::TextJustifier() {}

TextJustifier::~TextJustifier() {}

void TextJustifier::GetWordKashidaPositions(const SalLayout& rLayout, std::u16string_view rText,
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

namespace
{
double lcl_applyDXArray(JustificationData& rJustification, const LayoutResources& rRes,
                        std::span<const double> pDXArray, sal_Int32 nMinCluster, sal_Int32 nLen)
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

void lcl_applyKashidaArray(JustificationData& rJustification,
                           std::span<const sal_Bool> pKashidaArray, sal_Int32 nMinCluster)
{
    if (pKashidaArray.empty())
        return;

    for (sal_Int32 i = 0; i < static_cast<sal_Int32>(pKashidaArray.size()); ++i)
    {
        rJustification.SetKashidaPosition(nMinCluster + i, static_cast<bool>(pKashidaArray[i]));
    }
}

} // namespace

void TextJustifier::PrepareJustification(const LayoutResources& rRes,
                                         std::span<const double> pDXArray,
                                         std::span<const sal_Bool> pKashidaArray,
                                         sal_Int32 nMinIndex, sal_Int32 nLen,
                                         std::optional<sal_Int32> nDrawMinCharPos,
                                         std::optional<sal_Int32> nDrawEndCharPos,
                                         TextLayoutRequest& rLayoutArgs, double& rEndGlyphCoord)
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

void TextJustifier::JustifyLayout(SalLayout& rLayout, TextLayoutRequest& rArgs)
{
    rLayout.AdjustLayout(rArgs);
}

void TextJustifier::ApplyHorizontalOffset(SalLayout& rLayout, const TextLayoutRequest& rArgs,
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

void TextJustifier::SetAnchorPoint(SalLayout& rLayout, const TextLayoutPositioning& rPositioning)
{
    rLayout.DrawBase() = rPositioning.aDrawBase;
}
} // namespace vcl::text
