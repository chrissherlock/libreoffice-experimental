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

#include <tools/gen.hxx>
#include <tools/long.hxx>

#include <vcl/dllapi.h>
#include <vcl/vclenum.hxx>

#include <functional>

class LogicalFontInstance;
namespace vcl
{
class Font;
}

namespace vcl::text
{
class VCL_DLLPUBLIC FontMetricEngine
{
public:
    static void InitializeFontMetrics(
        const LogicalFontInstance* pFontInstance, const vcl::Font& rFont, tools::Long nDPIY,
        tools::Long nPixelWidth, std::function<tools::Long(const OUString&)> const& fnGetTextWidth,
        std::function<void(tools::Rectangle&, const OUString&)> const& fnGetBoundRect);

    static void InitializeTextLineMetrics(const LogicalFontInstance* pFontInstance,
                                          const vcl::Font& rFont, tools::Long nDPIY,
                                          tools::Long nSpaceWidth, tools::Long nBulletWidth);

    static void InitializeAboveTextLineMetrics(const LogicalFontInstance* pFontInstance,
                                               tools::Long nDPIY, tools::Long nUnderlineSize);

    static tools::Long GetAlignmentOffset(TextAlign eAlign, tools::Long nAscent,
                                          tools::Long nDescent);
};

} // namespace vcl::text

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
