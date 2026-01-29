/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <vcl/dllapi.h>
#include <sal/types.h>
#include <rtl/ustring.hxx>

class OutputDevice;
class SalLayout;
class Point;
namespace vcl::text
{
class TextRecordingState;
}

namespace vcl::text
{
// Records layout data for internal Accessibility use.
// Writes detailed glyph information into the TextRecordingState.
class VCL_DLLPUBLIC AccessibilityRecorder
{
private:
    const vcl::text::TextRecordingState& mrState;

public:
    explicit AccessibilityRecorder(const vcl::text::TextRecordingState& rState);

    bool IsActive() const;

    void Record(OutputDevice& rDev, const Point& rStartPt, const OUString& rStr, sal_Int32 nIndex,
                sal_Int32 nLen, const SalLayout* pLayout, bool bStartVisualLine = false);
};

} // namespace vcl::text
/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
