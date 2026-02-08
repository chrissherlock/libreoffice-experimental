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
#include <vector>
#include <vcl/text/LayoutResources.hxx>
#include <vcl/text/TextSpan.hxx>
#include <vcl/text/LayoutCacheData.hxx>

class SalLayout;
class CoordinateMapper;

namespace vcl::text
{
class VCL_DLLPUBLIC CaretManager
{
public:
    static void GetCaretPositions(const LayoutResources& rRes, const TextSpan& rSpan,
                                  std::vector<double>& rCaretPositions, const SalLayout& rLayout);

    static void MirrorCaretPositions(std::vector<double>& rCaretPixelPos, double nWidth);

    static void ConvertPixelsToLogic(const CoordinateMapper& rMapper,
                                     std::vector<double>& rCaretPixelPos);
};

} // namespace vcl::text
