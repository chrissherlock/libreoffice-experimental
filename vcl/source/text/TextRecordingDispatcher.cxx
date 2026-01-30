/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
#include <vcl/text/TextRecordingDispatcher.hxx>
#include <vcl/text/TextRecordingState.hxx>
#include <text/AccessibilityRecorder.hxx>
#include <text/MeasurementRecorder.hxx>

namespace vcl::text
{
void TextRecordingDispatcher::Dispatch(const TextRecordingState& rState, OutputDevice& rDev,
                                       const OUString& rStr, sal_Int32 nIndex, sal_Int32 nLen,
                                       const SalLayout* pLayout, bool bLineStart)
{
    if (!pLayout)
        return;

    // Dispatch to Measurement
    vcl::text::MeasurementRecorder aMeas(const_cast<TextRecordingState&>(rState));
    if (aMeas.IsActive())
        aMeas.Record(rDev, rStr, nIndex, nLen, pLayout);

    // Dispatch to Accessibility
    vcl::text::AccessibilityRecorder aAcc(const_cast<TextRecordingState&>(rState));
    if (aAcc.IsActive())
        aAcc.Record(rDev, rStr, nIndex, nLen, pLayout, bLineStart);
}
}
/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
