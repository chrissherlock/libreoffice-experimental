/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once
#include <vcl/dllapi.h>
#include <tools/gen.hxx>
#include <rtl/ustring.hxx>

class OutputDevice;
class SalLayout;

namespace vcl::text
{
class TextRecordingState;

class VCL_DLLPUBLIC AccessibilityRecorder
{
    TextRecordingState& mrState;

public:
    explicit AccessibilityRecorder(TextRecordingState& rState)
        : mrState(rState)
    {
    }
    bool IsActive() const;
    // Point removed, bStartVisualLine preserved
    void Record(OutputDevice& rDev, const OUString& rStr, sal_Int32 nIndex, sal_Int32 nLen,
                const SalLayout* pLayout, bool bStartVisualLine = false);
};
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
