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
class VCL_DLLPUBLIC LayoutRecorder
{
    ImplOutDevData* mpOutDevData;

public:
    LayoutRecorder(ImplOutDevData* pData)
        : mpOutDevData(pData)
    {
    }

    bool IsActive() const;

    // Instance method: Uses mpOutDevData to record to internal buffers
    void RecordLayoutData(OutputDevice& rDev, const Point& rStartPt, const OUString& rStr,
                          sal_Int32 nIndex, sal_Int32 nLen);

    // Static method: Pure logic, no internal state dependency.
    // Mirrors the legacy OutputDevice::ImplFilterAndRecordGlyphs exactly.
    static void FilterAndRecordGlyphs(OutputDevice& rDev, const Point& rStartPt,
                                      const OUString& rStr, sal_Int32 nIndex, sal_Int32 nLen,
                                      const vcl::Region& rClip,
                                      std::vector<tools::Rectangle>& rVector,
                                      OUString* pDisplayText);
};

} // namespace vcl::text

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
