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
#include <tools/fontenum.hxx>

#include <vcl/dllapi.h>
#include <vcl/rendercontext/SalLayoutFlags.hxx>
#include <vcl/outdev.hxx>
#include <vcl/text/LayoutResources.hxx>
#include <vcl/text/TextGeometry.hxx>
#include <vcl/text/TextDecorator.hxx>
#include <vcl/text/MnemonicGeometry.hxx>

#include <ImplLayoutRuns.hxx>
#include <font/EmphasisMark.hxx>
#include <font/FontLookupCriteria.hxx>
#include <textlineinfo.hxx>
#include <text/ILayoutFactory.hxx>

#include <vector>
#include <memory>
#include <functional>
#include <optional>
#include <span>

#define TEXT_DRAW_ELLIPSIS                                                                         \
    (DrawTextFlags::EndEllipsis | DrawTextFlags::PathEllipsis | DrawTextFlags::NewsEllipsis)

class Point;
namespace tools
{
class Rectangle;
}

class CoordinateMapper;
class FontMetricData;
class ImplFontCache;
class ImplFontList;
class LogicalFontInstance;
class MultiSalLayout;
class SalGraphics;
class SalLayout;
class SalLayoutGlyphsImpl;

namespace vcl
{
class Font;
struct GraphicsState;
namespace font
{
struct FontRealization;
class PhysicalFontCollection;
}
namespace text
{
class TextLayoutRequest;
class TextLayoutCache;
}
}

namespace vcl::text
{
using FallbackLayoutFactory
    = std::function<std::unique_ptr<SalLayout>(LogicalFontInstance*, int, TextLayoutRequest&)>;

struct TextLayoutPositioning;
class TextLayoutRequest;

class VCL_DLLPUBLIC TextLayoutEngine
{
public:
    /**
     * Creates and configures a LayoutRequest (TextLayoutRequest) based on the current device state.
     * This moves the "configuration" logic out of OutputDevice.
     */
    static ::vcl::text::TextLayoutRequest
    CreateLayoutRequest(OUString& rStr, sal_Int32 nMinIndex, sal_Int32 nLen, double nPixelWidth,
                        SalLayoutFlags nFlags, const vcl::text::TextLayoutCache* pCache,
                        const ::vcl::GraphicsState& rState,
                        const ::vcl::font::FontRealization& rRealization, bool bRTL);

    static std::unique_ptr<SalLayout> PerformTextLayout(const LayoutResources& rRes,
                                                        ::vcl::text::TextLayoutRequest& rArgs,
                                                        const SalLayoutGlyphs* pGlyphs);

    /** Orchestrates the complete layout process. */
    static std::unique_ptr<SalLayout> Layout(const LayoutResources& rRes, const TextSpan& rSpan,
                                             const vcl::text::LayoutConstraints& rConstraints,
                                             const vcl::text::LayoutCacheData& rCache,
                                             const vcl::text::RenderSelection& rSelection);

    /** Calculates the subpixel layout width from logical units. */
    static double CalculateLayoutWidth(const LayoutResources& rRes, tools::Long nLogicWidth);

    static void FillAlignmentContext(TextLayoutPositioning& rPos,
                                     const ::vcl::text::TextLayoutRequest& rArgs,
                                     double nEndGlyphCoord);

    static bool PrepareNormalizedLayoutInput(const OUString& rOrigStr, sal_Int32 nMinIndex,
                                             sal_Int32& rLen, OUString& rStr,
                                             const ::vcl::font::FontRealization& rFontRealization,
                                             const vcl::text::TextLayoutCache*& rpLayoutCache,
                                             const SalLayoutGlyphs*& rpGlyphs);

    /** Calculates the total height of the font in device pixels, including emphasis marks. */
    static double GetTextHeightPixel(const ::vcl::font::FontRealization& rRealization);

    /** Validates that cached glyphs are in a consistent state for layout reuse. */
    static void ValidateGlyphCache(const SalLayoutGlyphs* pGlyphs);

    /**
     * Acquires the SalGraphics and creates the initial SalLayout object.
     * Returns nullptr if graphics acquisition fails.
     */
    static std::unique_ptr<SalLayout> CreateBaseLayout(const LayoutResources& rRes);

private:
    /** Calculates the subpixel factor (1 or 64) based on the mapping state. */
    static tools::Long GetSubPixelFactor(const CoordinateMapper& rMapper);

    /** Converts logical widths to layout units, accounting for subpixel scaling. */
    static double GetLayoutPixelWidth(const CoordinateMapper& rMapper, tools::Long nLogicWidth,
                                      tools::Long nSubPixelFactor);
};

} // namespace vcl::text
