/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <rtl/ustring.hxx>
#include <vcl/dllapi.h>
#include <vector>

class OutputDevice;
struct ImplOutDevData;

class Point;
class SalLayout;
namespace tools
{
class Rectangle;
}
namespace vcl
{
class Region;
}

namespace vcl::text
{
// A unified recorder that handles both internal (accessibility) and external (measurement)
// layout recording scenarios.
class VCL_DLLPUBLIC LayoutRecorder
{
private:
    // Internal Mode State (Accessibility)
    ImplOutDevData* mpOutDevData = nullptr;

    // External Mode State (Measurement/GetTextRect)
    std::vector<tools::Rectangle>* mpVector = nullptr;
    OUString* mpDisplayText = nullptr;
    const vcl::Region* mpClip = nullptr;

public:
    // Constructor for Internal Mode
    // Records to mpOutDevData buffers and manages line indices.
    explicit LayoutRecorder(ImplOutDevData* pData);

    // Constructor for External Mode
    // Records directly to the provided vector and string using the provided clip.
    LayoutRecorder(std::vector<tools::Rectangle>& rRects, OUString* pDisplayText,
                   const vcl::Region& rClip);

    // Returns true if a valid recording target exists.
    bool IsActive() const;

    // The single public API for recording.
    // bStartVisualLine: Set to true if this text operation represents the start of a new line
    //                   (e.g., DrawTextArray). Defaults to false (e.g., DrawText).
    void Record(OutputDevice& rDev, const Point& rStartPt, const OUString& rStr, sal_Int32 nIndex,
                sal_Int32 nLen, const SalLayout* pLayout = nullptr, bool bStartVisualLine = false);

private:
    // Private helper: pushes the line index. Only allowed via Record(..., true).
    void RecordLineStart();

    // Private helper: core filtering logic
    void FilterAndAppend(OutputDevice& rDev, const Point& rStartPt, const OUString& rStr,
                         sal_Int32 nIndex, sal_Int32 nLen, const SalLayout* pLayout,
                         const vcl::Region& rClip, std::vector<tools::Rectangle>& rOutRects,
                         OUString* pOutText);
};

} // namespace vcl::text

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
