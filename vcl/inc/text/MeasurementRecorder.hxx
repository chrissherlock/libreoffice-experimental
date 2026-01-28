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
#include <rtl/ustring.hxx>
#include <vector>

class OutputDevice;
class SalLayout;
class Point;
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
// Records layout data for external measurement (e.g. GetTextRect).
// Writes bounding boxes into the provided vector.
class VCL_DLLPUBLIC MeasurementRecorder
{
private:
    std::vector<tools::Rectangle>& mrRects;
    OUString* mpDisplayText;
    const vcl::Region& mrClip;

public:
    MeasurementRecorder(std::vector<tools::Rectangle>& rRects, OUString* pDisplayText,
                        const vcl::Region& rClip);

    bool IsActive() const { return true; } // Always active if created

    void Record(OutputDevice& rDev, const Point& rStartPt, const OUString& rStr, sal_Int32 nIndex,
                sal_Int32 nLen, const SalLayout* pLayout);
};

} // namespace vcl::text

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
