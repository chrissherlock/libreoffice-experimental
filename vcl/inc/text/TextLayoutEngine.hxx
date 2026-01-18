/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <vcl/dllapi.h>
#include <vector>
#include <tools/gen.hxx> // For global Point
#include <tools/fontenum.hxx> // For FontEmphasisMark
#include <rtl/ustring.hxx>

class SalLayout;

namespace vcl::text
{
class VCL_DLLPUBLIC TextLayoutEngine
{
public:
    /** Analyzes a layout to find valid Kashida insertion points. */
    static void GetWordKashidaPositions(const SalLayout& rLayout, std::u16string_view rText,
                                        std::vector<bool>& rOutMap);

    /** Calculates the device-pixel positions for emphasis marks.
     * @return A vector of global Points (in device pixels).
     */
    static std::vector<Point> GetEmphasisMarkPositions(const SalLayout& rLayout, long nAscent,
                                                       long nDescent, FontEmphasisMark nStyle);
};

} // namespace vcl::text
/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
