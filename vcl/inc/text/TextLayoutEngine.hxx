/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <vcl/dllapi.h>
#include <sallayout.hxx>
#include <vcl/outdev.hxx>
#include <vector>
#include <vcl/rendercontext/SalLayoutFlags.hxx>
#include <functional>
#include <tools/gen.hxx> // For global Point
#include <tools/fontenum.hxx> // For FontEmphasisMark
#include <rtl/ustring.hxx>
#include <font/FontLookupCriteria.hxx>

class SalLayout;
class LogicalFontInstance;
namespace vcl
{
struct GraphicsState;
class Font;
}

class GraphicsState;

namespace vcl::font
{
struct FontRealization;
}

class ImplFontCache;
namespace vcl::font
{
class PhysicalFontCollection;
}
class LogicalFontInstance;
class SalLayoutGlyphsImpl;

namespace vcl::text
{
using FallbackLayoutFactory
    = std::function<std::unique_ptr<SalLayout>(LogicalFontInstance*, int, ImplLayoutArgs&)>;

class ILayoutFactory
{
public:
    virtual ~ILayoutFactory() = default;
    virtual std::unique_ptr<SalLayout> CreateLayout(int nFallbackLevel) = 0;
    virtual void SetFont(LogicalFontInstance* pFont, int nFallbackLevel) = 0;
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
     * @param pFontInstance   The font instance to update.
     * @param nDPIY           Device vertical DPI.
     * @param nPixelWidth     Width of 1 logical unit in pixels.
     * @param fnGetTextWidth  Callback: Returns width of a string in logic units.
     * @param fnGetBoundRect  Callback: Fills the bounding rectangle of a string.
     */

    /** Determines BiDi flags based on layout mode and string content. */
    static SalLayoutFlags GetBiDiLayoutFlags(vcl::text::ComplexTextLayoutFlags eLayoutMode,
                                             std::u16string_view rStr, sal_Int32 nMinIndex,
                                             sal_Int32 nEndIndex);

    static void InitializeFontMetrics(
        LogicalFontInstance* pFontInstance, const vcl::Font& rFont, long nDPIY, long nPixelWidth,
        std::function<long(const OUString&)> const& fnGetTextWidth,
        std::function<void(tools::Rectangle&, const OUString&)> const& fnGetBoundRect);

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
     * Creates and configures a LayoutRequest (ImplLayoutArgs) based on the current device state.
     * This moves the "configuration" logic out of OutputDevice.
     */
    static vcl::text::ImplLayoutArgs
    CreateLayoutRequest(OUString& rStr, sal_Int32 nMinIndex, sal_Int32 nLen, double nPixelWidth,
                        SalLayoutFlags nFlags, const vcl::text::TextLayoutCache* pCache,
                        const GraphicsState& rState, const font::FontRealization& rRealization,
                        bool bRTL);

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
    static OUString IdentifyMissingChars(const vcl::text::ImplLayoutArgs& rArgs);

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
    static std::unique_ptr<SalLayout> ResolveMissingGlyphs(std::unique_ptr<SalLayout> pBaseLayout,
                                                           vcl::text::ImplLayoutArgs& rLayoutArgs,
                                                           const SalLayoutGlyphs* pGlyphs,
                                                           const FontLookupCriteria& rCriteria,
                                                           ILayoutFactory& rFactory);
};

} // namespace vcl::text
/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
