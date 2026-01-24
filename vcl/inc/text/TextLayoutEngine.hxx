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

#include <ImplLayoutRuns.hxx>
#include <font/FontLookupCriteria.hxx>

#include <vector>
#include <memory>
#include <functional>
#include <optional>
#include <span>

class CoordinateMapper;
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

class ILayoutFactory
{
public:
    virtual ~ILayoutFactory() = default;
    virtual std::unique_ptr<SalLayout> CreateLayout(int nFallbackLevel) = 0;
    virtual void SetFont(LogicalFontInstance* pFont, int nFallbackLevel) = 0;
};

struct TextLayoutPositioning
{
    basegfx::B2DPoint aDrawBase;
    bool bSubpixelPositioning;
    bool bRightAlign;
    bool bHasDXArray;
    double nEndGlyphCoord; // For RTL alignment (0 if unused)
};

struct LayoutResources
{
    const LogicalFontInstance* pFont;
    const CoordinateMapper& rMapper;
    ImplFontCache* pFontCache;
    vcl::font::PhysicalFontCollection* pFontCollection;
    const LogicalFontInstance* pForcedFallback;
    std::function<SalGraphics*()> fnGetGraphics;
    bool bRTLEnabled;
    bool bSubpixelPositioning;
    const vcl::GraphicsState& rGraphicsState;
    const vcl::font::FontRealization& rFontRealization;
};

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

    /** Initializes font metrics (bullet offset, CJK centering) using callbacks.
     * Independent of OutputDevice.
     */
    static void InitializeFontMetrics(
        LogicalFontInstance* pFontInstance, const vcl::Font& rFont, long nDPIY, long nPixelWidth,
        std::function<long(const OUString&)> const& fnGetTextWidth,
        std::function<void(tools::Rectangle&, const OUString&)> const& fnGetBoundRect);

    /** Determines BiDi flags based on layout mode and string content. */
    static SalLayoutFlags GetBiDiLayoutFlags(vcl::text::ComplexTextLayoutFlags eLayoutMode,
                                             std::u16string_view rStr, sal_Int32 nMinIndex,
                                             sal_Int32 nEndIndex);

    /** Applies digit localization to the string if the language requires it. */
    static void ApplyDigitLocalization(const vcl::GraphicsState& rGraphicsState, OUString& rStr,
                                       sal_Int32 nMinIndex, sal_Int32& rEndIndex);

    /** Calculates the Layout Flags based on the Graphics State and Font Realization. */
    static SalLayoutFlags CalculateLayoutFlags(const vcl::GraphicsState& rGraphicsState,
                                               const vcl::font::FontRealization& rFontRealization,
                                               bool bRTLWindow, std::u16string_view rStr,
                                               sal_Int32 nMinIndex, sal_Int32 nEndIndex,
                                               SalLayoutFlags nExistingFlags);

    /**
     * Creates and configures a LayoutRequest (TextLayoutRequest) based on the current device state.
     * This moves the "configuration" logic out of OutputDevice.
     */
    static vcl::text::TextLayoutRequest
    CreateLayoutRequest(OUString& rStr, sal_Int32 nMinIndex, sal_Int32 nLen, double nPixelWidth,
                        SalLayoutFlags nFlags, const vcl::text::TextLayoutCache* pCache,
                        const vcl::GraphicsState& rState,
                        const vcl::font::FontRealization& rRealization, bool bRTL);

    /**
     * Finds a suitable fallback font for the given missing characters.
     * Considers forced fallbacks, cached glyphs, and system font fallback.
     */
    static rtl::Reference<LogicalFontInstance> FindFallbackFont(const FontLookupCriteria& rCriteria,
                                                                int nFallbackLevel,
                                                                OUString& rMissingCodes,
                                                                bool& rHasUsedFallback,
                                                                SalLayoutGlyphsImpl* pGlyphsImpl);

    /**
     * Scans the layout arguments to identify which characters need fallback.
     * Returns a string containing all missing characters found in fallback runs.
     */
    static OUString IdentifyMissingChars(vcl::text::TextLayoutRequest& rArgs);

    /**
     * Merges a fallback layout into the main layout container.
     * Promotes the base layout to a MultiSalLayout if necessary.
     */
    static void MergeFallback(std::unique_ptr<MultiSalLayout>& rMultiSalLayout,
                              std::unique_ptr<SalLayout>& rBaseLayout,
                              std::unique_ptr<SalLayout> pFallback, const ImplLayoutRuns& rRuns,
                              bool bIsLastLevel);

    /**
     * Iteratively finds fallback fonts and creates layouts to resolve characters
     * missing from the base layout.
     */
    static std::unique_ptr<SalLayout>
    ResolveMissingGlyphs(std::unique_ptr<SalLayout> pBaseLayout,
                         vcl::text::TextLayoutRequest& rLayoutArgs, const SalLayoutGlyphs* pGlyphs,
                         const FontLookupCriteria& rCriteria, ILayoutFactory& rFactory);

    static void PrepareJustification(const LayoutResources& rRes, KernArraySpan pDXArray,
                                     std::span<const sal_Bool> pKashidaArray, sal_Int32 nMinIndex,
                                     sal_Int32 nLen, std::optional<sal_Int32> nDrawMinCharPos,
                                     std::optional<sal_Int32> nDrawEndCharPos,
                                     vcl::text::TextLayoutRequest& rLayoutArgs,
                                     double& rEndGlyphCoord);

    static basegfx::B2DPoint MapLogicalToDevicePos(const LayoutResources& rRes,
                                                   const Point& rLogicalPos);
    static void FillAlignmentContext(TextLayoutPositioning& rPos,
                                     const vcl::text::TextLayoutRequest& rArgs,
                                     double nEndGlyphCoord);
    static void JustifyLayout(SalLayout& rLayout, vcl::text::TextLayoutRequest& rArgs);
    static void ApplyHorizontalOffset(SalLayout& rLayout, const vcl::text::TextLayoutRequest& rArgs,
                                      const TextLayoutPositioning& rPositioning);
    static void SetAnchorPoint(SalLayout& rLayout, const TextLayoutPositioning& rPositioning);

    // Layout Orchestration
    static std::unique_ptr<SalLayout>
    Layout(const LayoutResources& rRes, vcl::text::TextLayoutRequest& rArgs, KernArraySpan pDXArray,
           std::span<const sal_Bool> pKashidaArray, const Point& rLogicalPos,
           const SalLayoutGlyphs* pGlyphs = nullptr);

    /** Fills a KernArray with logical widths and returns the total width. */
    static double FillPartialTextArray(const LayoutResources& rRes, const SalLayout& rLayout,
                                       KernArray* pKernArray, sal_Int32 nIndex, sal_Int32 nLen,
                                       sal_Int32 nPartIndex, sal_Int32 nPartLen,
                                       const OUString& rCaretStr);

    static bool PrepareNormalizedLayoutInput(const OUString& rOrigStr, sal_Int32 nMinIndex,
                                             sal_Int32& rLen, OUString& rStr,
                                             const vcl::font::FontRealization& rFontRealization,
                                             const vcl::text::TextLayoutCache*& rpLayoutCache,
                                             const SalLayoutGlyphs*& rpGlyphs);

    /** Calculates the total height of the font in device pixels, including emphasis marks. */
    static double GetTextHeightPixel(const vcl::font::FontRealization& rRealization);

    /** Calculates the subpixel layout width from logical units. */
    static double CalculateLayoutWidth(const LayoutResources& rRes, tools::Long nLogicWidth);

    /** Validates that cached glyphs are in a consistent state for layout reuse. */
    static void ValidateGlyphCache(const SalLayoutGlyphs* pGlyphs);

    /**
     * Acquires the SalGraphics and creates the initial SalLayout object.
     * Returns nullptr if graphics acquisition fails.
     */
    static std::unique_ptr<SalLayout> CreateBaseLayout(const LayoutResources& rRes);

    /** * Records font mapping usage for diagnostic purposes if tracking is enabled.
     * This captures which fonts were actually used (including fallbacks) to satisfy the request.
     */

    // Diagnostic Font Tracking
    static void StartTracking();
    static OutputDevice::FontMappingUseData FinishTracking();
    static bool IsTracking();
    static void TrackLayoutFonts(const vcl::Font& rFont, const SalLayout* pLayout);

    static std::unique_ptr<SalLayout> ResolveFallbacks(const LayoutResources& rRes,
                                                       std::unique_ptr<SalLayout> pLayout,
                                                       vcl::text::TextLayoutRequest& rArgs,
                                                       const SalLayoutGlyphs* pGlyphs);

    static void ApplyPositioning(const LayoutResources& rRes, SalLayout& rLayout,
                                 vcl::text::TextLayoutRequest& rArgs, const Point& rLogicalPos,
                                 double nEndGlyphCoord);

    /** Executes the core layout loop: creates base layout, runs initial layout, and resolves fallbacks. */
    static std::unique_ptr<SalLayout> PerformTextLayout(const LayoutResources& rRes,
                                                        vcl::text::TextLayoutRequest& rArgs,
                                                        const SalLayoutGlyphs* pGlyphs);

    /** Orchestrates the complete layout process. */
    static std::unique_ptr<SalLayout> Layout(const LayoutResources& rRes,
                                             const vcl::text::TextSpan& rSpan,
                                             const vcl::text::LayoutConstraints& rConstraints,
                                             const vcl::text::LayoutCacheData& rCache,
                                             const vcl::text::RenderSelection& rSelection);

    static void ZeroFillKernArray(KernArray* pKernArray, sal_Int32 nLen);

    static sal_Int32 GetNormalizedLength(const OUString& rStr, sal_Int32 nIdx, sal_Int32 nLen);

    static double GetPartialTextArray(const LayoutResources& rRes, const vcl::text::TextSpan& rSpan,
                                      KernArray* pKernArray, sal_Int32 nPartIndex,
                                      sal_Int32 nPartLen, bool bCaret,
                                      const vcl::text::LayoutCacheData& rCache,
                                      std::optional<tools::Rectangle>* pBounds);

    static void FixupCaretPositions(std::vector<double>& rCaretPixelPos);
};

} // namespace vcl::text
