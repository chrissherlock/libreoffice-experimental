/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <vcl/outdev.hxx>
#include <vcl/ctrl.hxx>
#include <text/AccessibilityRecorder.hxx>
#include <vcl/text/TextRecordingState.hxx>
#include <text/layoutrecording.hxx>

namespace vcl::text
{
AccessibilityRecorder::AccessibilityRecorder(const vcl::text::TextRecordingState& rState)
    : mrState(rState)
{
}

bool AccessibilityRecorder::IsActive() const { return mrState.mpLayoutData != nullptr; }

void AccessibilityRecorder::Record(OutputDevice& rDev, const Point& /*rStartPt*/,
                                   const OUString& rStr, sal_Int32 nIndex, sal_Int32 nLen,
                                   const SalLayout* pLayout, bool bStartVisualLine)
{
    if (!IsActive())
        return;

    auto& rData = *mrState.mpLayoutData;

    if (bStartVisualLine)
        rData.m_aLineIndices.push_back(rData.m_aDisplayText.getLength());

    vcl::Region aClip(rDev.GetOutputBoundsClipRegion());
    aClip.Intersect(mrState.maRecordRect);

    // Delegate to shared free function
    FilterAndAppend(pLayout, rStr, nIndex, nLen, aClip, rData.m_aUnicodeBoundRects,
                    &rData.m_aDisplayText);
}

} // namespace vcl::text
/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
