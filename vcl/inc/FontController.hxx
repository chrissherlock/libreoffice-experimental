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

#include <vcl/dllapi.h>
#include <vcl/fontcharmap.hxx>
#include <vcl/vclenum.hxx>

class ImplFontCache;
class PhysicalFontCollection;
class LogicalFontInstance;

#include <tuple>

namespace vcl::font
{
class Font;

struct FontRealization
{
    rtl::Reference<LogicalFontInstance> mxFont;

    tools::Long nXOffset = 0;
    tools::Long nYOffset = 0;

    tools::Long nEmphasisAscent = 0;
    tools::Long nEmphasisDescent = 0;

    bool bHasLineDecorations = false;
    bool bHasSpecialEffects = false;

    vcl::text::ComplexTextLayoutFlags eLayoutMode = vcl::text::ComplexTextLayoutFlags::Default;
};

class VCL_DLLPUBLIC FontController
{
public:
    rtl::Reference<LogicalFontInstance> mxFontInstance;
    std::shared_ptr<PhysicalFontCollection> mxFontCollection;

    TextAlign meTextAlign;

    FontController();

    void InitializeFonts(SalGraphics* pGraphics);

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

    std::pair<float, Size> CalculateDeviceSize(const vcl::Font& rFont,
                                               const CoordinateMapper& rMapper,
                                               tools::Long nDPIY) const;

    /**
     * Evaluates if the font requires special rendering for lines or effects.
     * Returns a tuple of: <bTextLines, bTextSpecial>
     */
    std::tuple<bool, bool> GetTextLayoutFlags(const vcl::Font& rFont) const;

    bool ShouldDisableAntialiasing(AntialiasingFlags eAntialisingFlags,
                                   const StyleSettings& rStyleSettings, tools::Long nHeight) const;

    /**
     * Calculates a stretched width for OLE font scaling fixes.
     * Returns 0 if no fix is required or possible.
     */
    int CalculateOLEStorageWidth(const LogicalFontInstance* pFontInstance, tools::Long nMapXNum,
                                 tools::Long nMapXDen, tools::Long nMapYNum,
                                 tools::Long nMapYDen) const;

    void PopulateFontMetric(FontMetric& rMetric, const vcl::Font& rLogicalFont,
                            const LogicalFontInstance* pFontInstance, tools::Long nEmphasisAscent,
                            tools::Long nEmphasisDescent) const;

    double GetMinKashidaWidth(const LogicalFontInstance* pFontInstance) const;

    bool GetFontCapabilities(SalGraphics* pGraphics,
                             vcl::FontCapabilities& rFontCapabilities) const;

    FontCharMapRef GetFontCharMap(SalGraphics* pGraphics) const;

    void GetFontFeatures(const LogicalFontInstance* pFontInstance,
                         std::vector<vcl::font::Feature>& rFontFeatures) const;
};

} // namespace vcl

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
