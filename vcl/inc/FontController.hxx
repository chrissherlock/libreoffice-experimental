/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <rtl/ref.hxx>
#include <tools/color.hxx>

#include <vcl/vclenum.hxx>

class ImplFontCache;
class PhysicalFontCollection;
class LogicalFontInstance;

#include <tuple>

namespace vcl::font
{
class Font;

class FontController
{
public:
    rtl::Reference<LogicalFontInstance> mxFontInstance;
    std::shared_ptr<PhysicalFontCollection> mxFontCollection;

    TextAlign meTextAlign;

    FontController();

    bool NeedsUpdate(const vcl::Font& rRequestedFont, bool bDeviceDirty) const;

    void RealizeFont(ImplFontCache& rCache, const vcl::Font& rFont, const Size& rSize,
                     float fExactHeight, bool bNonAntialiased);

    void InitializeInstance(LogicalFontInstance* pFontInstance, SalGraphics* pGraphics);

    /**
     * Calculates vertical/horizontal offsets and emphasis metrics.
     * Returns a tuple of: <nHorzOffset, nVertOffset, nEmphasisAscent, nEmphasisDescent>
     */
    std::tuple<tools::Long, tools::Long, tools::Long, tools::Long>
    CalculateTextOffsets(const vcl::Font& rFont, const LogicalFontInstance* pFontInstance) const;

    /**
     * Evaluates if the font requires special rendering for lines or effects.
     * Returns a tuple of: <bTextLines, bTextSpecial>
     */
    std::tuple<bool, bool> GetTextLayoutFlags(const vcl::Font& rFont) const;
};

} // namespace vcl

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
