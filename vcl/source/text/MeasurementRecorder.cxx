/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <vcl/outdev.hxx>
#include <text/MeasurementRecorder.hxx>
#include <text/layoutrecording.hxx>

namespace vcl::text
{
bool MeasurementRecorder::IsActive() const { return mrState.mpMeasurementVector != nullptr; }

void MeasurementRecorder::Record(OutputDevice& /*rDev*/, const OUString& rStr, sal_Int32 nIndex,
                                 sal_Int32 nLen, const SalLayout* pLayout)
{
    if (!IsActive())
        return;

    // Local aliases to maintain compatibility with existing logic
    std::vector<tools::Rectangle>& mrRects = *mrState.mpMeasurementVector;
    OUString* mpDisplayText = mrState.mpMeasurementString;
    const vcl::Region& mrClip = *mrState.mpMeasurementClip;

    // Delegate to shared free function, passing mpDisplayText to avoid unused warning
    FilterAndAppend(pLayout, rStr, nIndex, nLen, mrClip, mrRects, mpDisplayText);
}

} // namespace vcl::text

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
