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
#include <tools/gen.hxx>
#include <vcl/outdev.hxx>
#include <vcl/text/TextSpan.hxx>
#include <vcl/text/LayoutCacheData.hxx>
#include <vcl/text/LayoutResources.hxx>
#include <textlineinfo.hxx>
#include <functional>

namespace vcl
{
class TextLayoutCommon;
}

namespace vcl::text
{
struct SAL_DLLPUBLIC MultiLineLayout
{
    ImplMultiTextLineInfo aLineInfo;
    OUString aLastLine;
    sal_Int32 nFormatLines = 0;
    DrawTextFlags nResultStyle = DrawTextFlags::NONE;
};

class VCL_DLLPUBLIC MultiLineEngine
{
public:
    static OUString
    GetEllipsisString(const OUString& rStr, tools::Long nMaxWidth, DrawTextFlags nStyle,
                      const std::function<tools::Long(const OUString&)>& rfnGetTextWidth);

    static sal_Int32 GetTextBreak(const LayoutResources& rRes, const TextSpan& rSpan,
                                  tools::Long nMaxLineWidth, tools::Long nCharExtra,
                                  const vcl::text::LayoutCacheData& rCache);

    static sal_Int32 GetTextBreakArray(const LayoutResources& rRes, const TextSpan& rSpan,
                                       tools::Long nTextWidth,
                                       std::optional<sal_Unicode> nHyphenChar,
                                       std::optional<sal_Int32*> pHyphenPos, tools::Long nCharExtra,
                                       KernArraySpan aKernArray,
                                       const vcl::text::LayoutCacheData& rCache);

    static void CalculateMultiLineLayout(vcl::TextLayoutCommon& rLayout, MultiLineLayout& rRes,
                                         const tools::Rectangle& rRect, tools::Long nTextHeight,
                                         tools::Long nWidth, tools::Long nHeight,
                                         const OUString& rStr, DrawTextFlags nStyle);
};

} // namespace vcl::text
