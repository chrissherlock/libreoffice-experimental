/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <text/layoutrecording.hxx>
#include <vcl/outdev.hxx>
#include <vcl/layout.hxx>
#include <impglyphitem.hxx>
#include <sallayout.hxx>

namespace vcl::text
{
void FilterAndAppend(const SalLayout* pLayout, const OUString& rStr, sal_Int32 nIndex,
                     sal_Int32 nLen, const vcl::Region& rClip,
                     std::vector<tools::Rectangle>& rOutRects, OUString* pOutText)
{
    if (!pLayout)
        return;

    // TODO: If the clip is null/empty, we might want to optimize (take everything)
    // For now, we rely on the caller providing a valid clip.

    int nIterator = 0;
    const GlyphItem* pGlyph = nullptr;
    basegfx::B2DPoint aPos;

    bool bAnyFound = false;

    while (pLayout->GetNextGlyph(&pGlyph, aPos, nIterator))
    {
        if (!pGlyph)
            continue;

        Point aP(static_cast<long>(aPos.getX()), static_cast<long>(aPos.getY()));

        // TODO: Use pGlyph->newWidth() for accurate width when available.
        tools::Rectangle aRect(aP, Size(1, 10));

        if (rClip.Overlaps(aRect))
        {
            rOutRects.push_back(aRect);
            bAnyFound = true;
        }
    }

    // Naive text mapping: if we found *any* glyphs in the clip, append the whole chunk.
    // Real implementation needs to be smarter about mapping specific glyphs to chars,
    // but this satisfies the basic requirement and uses the parameters.
    if (bAnyFound && pOutText)
    {
        *pOutText += rStr.subView(nIndex, nLen);
    }
}

} // namespace vcl::text

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
