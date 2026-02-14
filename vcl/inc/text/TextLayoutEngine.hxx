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

class VCL_DLLPUBLIC TextLayoutEngine
{
public:
    static std::unique_ptr<SalLayout> GetStrikeoutCharLayout(const LayoutResources& rRes,
                                                             tools::Long nTargetWidth,
                                                             FontStrikeout eStrikeout);

    /** Analyzes a layout to find valid Kashida insertion points. */

    /** Calculates the device-pixel positions for emphasis marks.
     * @return A vector of global Points (in device pixels).
     */
    static void GetEmphasisMarkPositions(const SalLayout& rSalLayout,
                                         const font::FontRealization& rFontRealization,
                                         const font::EmphasisMark& rMark, bool bEmphasisBelow,
                                         std::vector<Point>& rPoints);

    /** Initializes font metrics (bullet offset, CJK centering) using callbacks.
     * Independent of OutputDevice.
     */
    static void InitializeFontMetrics(
        const LogicalFontInstance* pFontInstance, const ::vcl::Font& rFont, long nDPIY,
        long nPixelWidth, std::function<long(const OUString&)> const& fnGetTextWidth,
        std::function<void(tools::Rectangle&, const OUString&)> const& fnGetBoundRect);

    /** Determines BiDi flags based on layout mode and string content. */
    static SalLayoutFlags GetBiDiLayoutFlags(::vcl::text::ComplexTextLayoutFlags eLayoutMode,
                                             std::u16string_view rStr, sal_Int32 nMinIndex,
                                             sal_Int32 nEndIndex);

    /** Applies digit localization to the string if the language requires it. */
    static void ApplyDigitLocalization(const ::vcl::GraphicsState& rGraphicsState, OUString& rStr,
                                       sal_Int32 nMinIndex, sal_Int32& rEndIndex);

    /** Calculates the Layout Flags based on the Graphics State and Font Realization. */
    static SalLayoutFlags CalculateLayoutFlags(const ::vcl::GraphicsState& rGraphicsState,
                                               const ::vcl::font::FontRealization& rFontRealization,
                                               bool bRTLWindow, std::u16string_view rStr,
                                               sal_Int32 nMinIndex, sal_Int32 nEndIndex,
                                               SalLayoutFlags nExistingFlags);

    /**
     * Creates and configures a LayoutRequest (TextLayoutRequest) based on the current device state.
     * This moves the "configuration" logic out of OutputDevice.
     */
    static ::vcl::text::TextLayoutRequest
    CreateLayoutRequest(OUString& rStr, sal_Int32 nMinIndex, sal_Int32 nLen, double nPixelWidth,
                        SalLayoutFlags nFlags, const vcl::text::TextLayoutCache* pCache,
                        const ::vcl::GraphicsState& rState,
                        const ::vcl::font::FontRealization& rRealization, bool bRTL);

    static basegfx::B2DPoint MapLogicalToDevicePos(const LayoutResources& rRes,
                                                   const Point& rLogicalPos);
    static void FillAlignmentContext(TextLayoutPositioning& rPos,
                                     const ::vcl::text::TextLayoutRequest& rArgs,
                                     double nEndGlyphCoord);

    // Layout Orchestration
    static std::unique_ptr<SalLayout>
    Layout(const LayoutResources& rRes, ::vcl::text::TextLayoutRequest& rArgs,
           KernArraySpan pDXArray, std::span<const sal_Bool> pKashidaArray,
           const Point& rLogicalPos, const SalLayoutGlyphs* pGlyphs = nullptr);

    /** Fills a KernArray with logical widths and returns the total width. */
    static double FillPartialTextArray(const LayoutResources& rRes, const SalLayout& rLayout,
                                       KernArray* pKernArray, sal_Int32 nIndex, sal_Int32 nLen,
                                       sal_Int32 nPartIndex, sal_Int32 nPartLen,
                                       const OUString& rCaretStr);

    static bool PrepareNormalizedLayoutInput(const OUString& rOrigStr, sal_Int32 nMinIndex,
                                             sal_Int32& rLen, OUString& rStr,
                                             const ::vcl::font::FontRealization& rFontRealization,
                                             const vcl::text::TextLayoutCache*& rpLayoutCache,
                                             const SalLayoutGlyphs*& rpGlyphs);

    /** Calculates the total height of the font in device pixels, including emphasis marks. */
    static double GetTextHeightPixel(const ::vcl::font::FontRealization& rRealization);

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
    static void TrackLayoutFonts(const ::vcl::Font& rFont, const SalLayout* pLayout);

    // ResolveFallbacks moved to DefaultFallbackStrategy

    static void ApplyPositioning(const LayoutResources& rRes, SalLayout& rLayout,
                                 ::vcl::text::TextLayoutRequest& rArgs, const Point& rLogicalPos,
                                 double nEndGlyphCoord);

    /** Executes the core layout loop: creates base layout, runs initial layout, and resolves fallbacks. */
    static std::unique_ptr<SalLayout> PerformTextLayout(const LayoutResources& rRes,
                                                        ::vcl::text::TextLayoutRequest& rArgs,
                                                        const SalLayoutGlyphs* pGlyphs);

    /** Orchestrates the complete layout process. */
    static std::unique_ptr<SalLayout> Layout(const LayoutResources& rRes, const TextSpan& rSpan,
                                             const vcl::text::LayoutConstraints& rConstraints,
                                             const vcl::text::LayoutCacheData& rCache,
                                             const vcl::text::RenderSelection& rSelection);

    static double GetPartialTextArray(const LayoutResources& rRes, const TextSpan& rSpan,
                                      KernArray* pKernArray, sal_Int32 nPartIndex,
                                      sal_Int32 nPartLen, bool bCaret,
                                      const vcl::text::LayoutCacheData& rCache,
                                      std::optional<tools::Rectangle>* pBounds);

    static void GetCaretPositions(const LayoutResources& rRes, const TextSpan& rSpan,
                                  KernArray& rCaretPos, const vcl::text::LayoutCacheData& rCache);

    /** Orchestrates finding the character index where text must break for a given width. */
    /** Determines if the text at the specified index and length is Right-to-Left (RTL). */
    static bool GetTextIsRTL(const LayoutResources& rRes, const OUString& rString, sal_Int32 nIndex,
                             sal_Int32 nLen);
    /** Orchestrates the calculation of text bounding rectangles in logical units. */

    /**
     * Calculates the visual bounding box of the rendered text in device pixels.
     *
     * Unlike logical bounds (which represent the theoretical advance width and line height),
     * this method calculates the "Ink Bounds"—the actual area where pixels are colored on
     * the device.
     *
     * The primary driver of this function is the transformation from a baseline-relative
     * coordinate system to an ink-relative coordinate system. In standard layout, 'nY'
     * represents the baseline; here, 'nY' is initialized to the "Top of the Ink":
     * * nY = Baseline - (FontAscent + EmphasisAscent)
     *
     * This "Top of the Ink" coordinate represents the true upper visual boundary of the
     * renderable area.
     *
     * Consolidating this calculation within the engine serves several critical purposes:
     * 1. Geometric Integrity: By establishing the ink-top immediately, subsequent
     * transformations—particularly rotation fallbacks—operate on a coherent
     * rectangle rather than floating offsets. This prevents "double-offset" bugs
     * where metrics are applied at the wrong stage of a transformation.
     * 2. Rendering Alignment: Provides a "ready-to-use" rectangle for callers like
     * OutputDevice::ImplDrawTextBackground and transparency alpha-masking,
     * ensuring visual elements are never clipped.
     * 3. Architectural Decoupling: Encapsulates the complexity of combining standard
     * font metrics with emphasis mark offsets, allowing OutputDevice to function
     * purely as a resource provider.
     *
     * @param rSalLayout       The specific layout instance containing glyph positions.
     * @param rFontRealization The font metrics and emphasis mark data for the current
     * rendering state.
     * @return tools::Rectangle The bounding box representing the visual extent of
     * the ink.
     */
    static tools::Rectangle GetTextInkBounds(const SalLayout& rSalLayout,
                                             const ::vcl::font::FontRealization& rFontRealization,
                                             bool bApplyRotation = true);

    static void GetEmphasisMarkPositions(const SalLayout& rSalLayout,
                                         const ::vcl::font::FontRealization& rFontRealization,
                                         bool bEmphasisBelow, std::vector<Point>& rPoints);

    static basegfx::B2DHomMatrix
    CalculateOutlineTransform(const SalLayout& rLayout,
                              const ::vcl::font::FontRealization& rRealization, double nXOffset);

    static void InitializeTextLineMetrics(const LogicalFontInstance* pFontInstance,
                                          const ::vcl::Font& rFont, tools::Long nDPIY,
                                          tools::Long nSpaceWidth, tools::Long nBulletWidth);

    static void InitializeAboveTextLineMetrics(const LogicalFontInstance* pFontInstance,
                                               tools::Long nDPIY, tools::Long nUnderlineSize);

    static tools::Long GetAlignmentOffset(TextAlign eAlign, tools::Long nAscent,
                                          tools::Long nDescent);

    struct LayoutRequest
    {
        OUString aText;
        tools::Rectangle aTargetRect;
        DrawTextFlags nStyle;
        sal_Int32 nMnemonicPos;
        Degree10 nFontOrientation;
        tools::Long nFontAscent;
        tools::Long nFontHeight;
    };

    struct LayoutResult
    {
        OUString aDisplayText; // Could be truncated with ellipsis
        tools::Rectangle aTextRect; // Final aligned and rotated bounds
        Point aDrawPosition; // Where to call _rLayout.DrawText
        MnemonicGeometry aMnemonic; // Coordinates for the underline
        bool bHasMnemonic;

        sal_Int32 nLineCount = 1;
        tools::Long nMaxWidth = 0;
        bool bEllipsisGenerated = false;
    };

    static void GetGlyphRectsFromLayout(const SalLayout& rLayout, const Point& rStartPt,
                                        const OUString& rStr, sal_Int32 nLen,
                                        std::vector<tools::Rectangle>& rRects);

    static LayoutResult CalculateLayout(const CoordinateMapper& rMapper, const LayoutRequest& rReq,
                                        const vcl::TextLayoutCommon& rLayout);

    /** * Filters glyphs based on a clip region, preserving spaces between visible characters.
     */
    static void FilterVisibleGlyphs(const OUString& rStr, sal_Int32 nIndex,
                                    const vcl::Region& rClip,
                                    const std::vector<tools::Rectangle>& rGlyphRects,
                                    std::vector<tools::Rectangle>& rOutVisibleRects,
                                    OUString* pOutVisibleText);

private:
    /** Calculates the subpixel factor (1 or 64) based on the mapping state. */
    static tools::Long GetSubPixelFactor(const CoordinateMapper& rMapper);

    /** Converts logical widths to layout units, accounting for subpixel scaling. */
    static double GetLayoutPixelWidth(const CoordinateMapper& rMapper, tools::Long nLogicWidth,
                                      tools::Long nSubPixelFactor);
};

} // namespace vcl::text
