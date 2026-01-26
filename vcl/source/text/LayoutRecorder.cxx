/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <tools/gen.hxx>
#include <vcl/outdev.hxx>
#include <vcl/ctrl.hxx>
#include <text/LayoutRecorder.hxx>
#include <text/TextLayoutEngine.hxx>
#include <ImplOutDevData.hxx>

namespace vcl::text
{
bool LayoutRecorder::IsActive() const { return mpOutDevData && mpOutDevData->mpRecordLayout; }

void LayoutRecorder::RecordLayoutData(OutputDevice& rDev, const Point& rStartPt,
                                      const OUString& rStr, sal_Int32 nIndex, sal_Int32 nLen)
{
    if (!IsActive())
        return;

    auto& rRecord = *mpOutDevData->mpRecordLayout;
    rRecord.m_aLineIndices.push_back(rRecord.m_aDisplayText.getLength());

    vcl::Region aClip(rDev.GetOutputBoundsClipRegion());
    aClip.Intersect(mpOutDevData->maRecordRect);

    // Call the static helper for the actual filtering logic
    FilterAndRecordGlyphs(rDev, rStartPt, rStr, nIndex, nLen, aClip, rRecord.m_aUnicodeBoundRects,
                          &rRecord.m_aDisplayText);
}

void LayoutRecorder::FilterAndRecordGlyphs(OutputDevice& rDev, const Point& rStartPt,
                                           const OUString& rStr, sal_Int32 nIndex, sal_Int32 nLen,
                                           const vcl::Region& rClip,
                                           std::vector<tools::Rectangle>& rVector,
                                           OUString* pDisplayText)
{
    // DIRECT MIGRATION of logic from OutputDevice::ImplFilterAndRecordGlyphs
    // This preserves the "Dry Run" optimization used by GetTextRect (pVector path).

    if (rClip.IsNull())
    {
        // Optimization: No clip means we write directly to the output vector
        rDev.GetGlyphBoundRects(rStartPt, rStr, nIndex, nLen, rVector);

        if (pDisplayText)
            *pDisplayText += rStr.subView(nIndex, nLen);

        return;
    }

    // Complex path: We need to filter the glyphs against the clip region
    std::vector<tools::Rectangle> aGlyphRects;
    rDev.GetGlyphBoundRects(rStartPt, rStr, nIndex, nLen, aGlyphRects);

    vcl::text::TextLayoutEngine::FilterVisibleGlyphs(rStr, nIndex, rClip, aGlyphRects, rVector,
                                                     pDisplayText);
}

} // namespace vcl::text

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
