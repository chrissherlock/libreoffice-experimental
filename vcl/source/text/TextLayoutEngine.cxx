/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <text/TextLayoutEngine.hxx>
#include <sallayout.hxx>
#include <basegfx/point/b2dpoint.hxx>
#include <unicode/uchar.h>
#include <tools/gen.hxx>

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

} // namespace vcl::text
/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
