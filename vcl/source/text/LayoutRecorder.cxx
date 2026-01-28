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
LayoutRecorder::LayoutRecorder(ImplOutDevData* pData)
    : mpOutDevData(pData)
{
}

LayoutRecorder::LayoutRecorder(std::vector<tools::Rectangle>& rRects, OUString* pDisplayText,
                               const vcl::Region& rClip)
    : mpVector(&rRects)
    , mpDisplayText(pDisplayText)
    , mpClip(&rClip)
{
}

bool LayoutRecorder::IsActive() const
{
    if (mpVector)
        return true;
    return mpOutDevData && mpOutDevData->mpRecordLayout;
}

void LayoutRecorder::RecordLineStart()
{
    // Safety check: Line indices only exist in Internal Mode
    if (mpOutDevData && mpOutDevData->mpRecordLayout)
    {
        auto& rRecord = *mpOutDevData->mpRecordLayout;
        rRecord.m_aLineIndices.push_back(rRecord.m_aDisplayText.getLength());
    }
}

void LayoutRecorder::Record(OutputDevice& rDev, const Point& rStartPt, const OUString& rStr,
                            sal_Int32 nIndex, sal_Int32 nLen, bool bStartVisualLine)
{
    if (!IsActive())
        return;

    // --- Mode 1: External Recording (Direct) ---
    // Note: Line indices are irrelevant for GetTextRect, so bStartVisualLine is ignored.
    if (mpVector)
    {
        FilterAndAppend(rDev, rStartPt, rStr, nIndex, nLen, *mpClip, *mpVector, mpDisplayText);
        return;
    }

    // --- Mode 2: Internal Recording (Accessibility) ---
    if (mpOutDevData && mpOutDevData->mpRecordLayout)
    {
        // 1. Handle Line Start (Controlled safely here)
        if (bStartVisualLine)
            RecordLineStart();

        // 2. Prepare Clip
        auto& rRecord = *mpOutDevData->mpRecordLayout;
        vcl::Region aClip(rDev.GetOutputBoundsClipRegion());
        aClip.Intersect(mpOutDevData->maRecordRect);

        // 3. Record Glyphs
        FilterAndAppend(rDev, rStartPt, rStr, nIndex, nLen, aClip, rRecord.m_aUnicodeBoundRects,
                        &rRecord.m_aDisplayText);
    }
}

void LayoutRecorder::FilterAndAppend(OutputDevice& rDev, const Point& rStartPt,
                                     const OUString& rStr, sal_Int32 nIndex, sal_Int32 nLen,
                                     const vcl::Region& rClip,
                                     std::vector<tools::Rectangle>& rOutRects, OUString* pOutText)
{
    if (rClip.IsNull())
    {
        rDev.GetGlyphBoundRects(rStartPt, rStr, nIndex, nLen, rOutRects);
        if (pOutText)
            *pOutText += rStr.subView(nIndex, nLen);
        return;
    }

    std::vector<tools::Rectangle> aGlyphRects;
    rDev.GetGlyphBoundRects(rStartPt, rStr, nIndex, nLen, aGlyphRects);

    vcl::text::TextLayoutEngine::FilterVisibleGlyphs(rStr, nIndex, rClip, aGlyphRects, rOutRects,
                                                     pOutText);
}

} // namespace vcl::text

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
