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
#include <tools/long.hxx>
#include <vector>
#include <optional>
#include <span>
#include <vector>
#include <string_view>

class SalLayout;

namespace vcl::text
{
struct LayoutResources;
class TextLayoutRequest;
class VCL_DLLPUBLIC TextJustifier
{
public:
    TextJustifier();
    ~TextJustifier();

    /** Analyzes a layout to find valid Kashida insertion points. */
    static void GetWordKashidaPositions(const SalLayout& rLayout, std::u16string_view rText,
                                        std::vector<bool>& rOutMap);

    /** Prepares the JustificationData for the layout request based on DX and Kashida arrays. */
    static void PrepareJustification(const LayoutResources& rRes, std::span<const double> pDXArray,
                                     std::span<const sal_Bool> pKashidaArray, sal_Int32 nMinIndex,
                                     sal_Int32 nLen, std::optional<sal_Int32> nDrawMinCharPos,
                                     std::optional<sal_Int32> nDrawEndCharPos,
                                     TextLayoutRequest& rLayoutArgs, double& rEndGlyphCoord);
};

} // namespace vcl::text
