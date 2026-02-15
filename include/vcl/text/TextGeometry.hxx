/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
#pragma once

#include <sal/types.h>
#include <tools/solar.h>
#include <tools/fontenum.hxx>
#include <tools/gen.hxx>
#include <tools/long.hxx>
#include <tools/poly.hxx>
#include <basegfx/point/b2dpoint.hxx>
#include <basegfx/polygon/b2dpolygon.hxx>
#include <basegfx/polygon/b2dpolypolygon.hxx>
#include <basegfx/range/b2drectangle.hxx>

#include <vcl/dllapi.h>
#include <vcl/rendercontext/DrawTextFlags.hxx>
#include <vcl/kernarray.hxx>
#include <vcl/vclenum.hxx>
#include <vcl/fntstyle.hxx>
#include <vcl/text/MnemonicGeometry.hxx>

#include <functional>
#include <vector>
#include <span>
#include <optional>

class Point;
class OutputDevice;
class SalLayout;
class SalLayoutGlyphs;

namespace vcl::font
{
struct FontRealization;
}

class CoordinateMapper;

namespace vcl
{
class TextLayoutCommon;
}

namespace vcl::text
{
struct TextSpan;
struct MirroringContext;
struct LayoutCacheData;
struct LayoutResources;
struct LayoutRequest;
class TextLayoutGeometry;
class TextLayoutRequest;

struct MirroringContext
{
    tools::Long nX; // The original X coordinate
    tools::Long nGraphicsWidth; // Width of the underlying graphics
    tools::Long nOutputWidth; // Width of the OutputDevice
    tools::Long nOutOffX; // X Offset of the OutputDevice
    bool bHasMirroredGraphics;
    bool bIsRTL;
};

struct SAL_DLLPUBLIC RotatedGeometry
{
    bool mbIsPolygon;
    tools::Rectangle maRect; // Optimization for 0, 90, 180, 270 deg
    tools::Polygon maPoly; // Fallback for arbitrary angles
};

class VCL_DLLPUBLIC TextGeometry
{
public:
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

    static LayoutResult CalculateLayout(const CoordinateMapper& rMapper, const LayoutRequest& rReq,
                                        const vcl::TextLayoutCommon& rLayout);

    static tools::Rectangle AlignAndRotateTextRect(const tools::Rectangle& rTargetRect,
                                                   tools::Long nContentWidth,
                                                   tools::Long nContentHeight, DrawTextFlags nStyle,
                                                   Degree10 nOrientation);

    static Point GetRotationOrigin(const Point& rPos, const Size& rTextSize, Degree10 nOrientation,
                                   TextAlign eAlign);

    static RotatedGeometry GetRotatedGeometry(const Point& rBase, const tools::Rectangle& rRect,
                                              Degree10 nOrientation);

    static Point GetRotatedImageOrigin(const Point& rBase, const tools::Rectangle& rLocalBounds,
                                       Degree10 nOrientation);

    static tools::Long GetMirroredX(const MirroringContext& rCtx);

    static tools::Long GetReliefOffset(sal_Int32 nDPIX, FontRelief eRelief);
    static tools::Long GetShadowOffset(tools::Long nLineHeight, bool bIsOutline);

    static const std::vector<basegfx::B2DPoint>& GetOutlineOffsets();

    static MnemonicGeometry
    GetMnemonicGeometry(std::function<double(tools::Long)> const& fnLogicWidthToDeviceSubPixel,
                        std::function<tools::Long(tools::Long)> const& fnLogicWidthToDevicePixel,
                        std::function<Point(const Point&)> const& fnLogicToPixel,
                        const MnemonicDeviceParams& rParams, KernArraySpan aDXArray,
                        sal_Int32 nRelPos, const Point& rLinePos, bool bTrailing = false);

    static Point CalculateLayoutOrigin(const OutputDevice& rDev, const tools::Rectangle& rRect,
                                       tools::Long nTextWidth, tools::Long nTextHeight,
                                       DrawTextFlags nStyle, TextAlign eAlign);

    static bool GetTextOutlines(const LayoutResources& rResources,
                                basegfx::B2DPolyPolygonVector& rVector, const OUString& rStr,
                                sal_Int32 nBase, sal_Int32 nIndex, sal_Int32 nLen,
                                sal_uLong nLayoutWidth, std::span<const double> pDXArray,
                                std::span<const sal_Bool> pKashidaArray);

    /** Orchestrates the calculation of text bounding rectangles in logical units. */
    static bool GetLogicalTextBoundRect(const LayoutResources& rRes, basegfx::B2DRectangle& rRect,
                                        const OUString& rStr, sal_Int32 nBase, sal_Int32 nIndex,
                                        sal_Int32 nLen, sal_uLong nLayoutWidth,
                                        std::span<const double> pDXArray,
                                        std::span<const sal_Bool> pKashidaArray,
                                        const SalLayoutGlyphs* pGlyphs);
    static void GetWordLineSegments(const SalLayout& rSalLayout,
                                    const ::vcl::font::FontRealization& rFontRealization,
                                    std::vector<std::pair<double, double>>& rSegments);

    static void GetGlyphRectsFromLayout(const SalLayout& rLayout, const Point& rStartPt,
                                        const OUString& rStr, sal_Int32 nLen,
                                        std::vector<tools::Rectangle>& rRects);

    static std::unique_ptr<SalLayout> GetStrikeoutCharLayout(const LayoutResources& rRes,
                                                             tools::Long nTargetWidth,
                                                             FontStrikeout eStrikeout);

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

    static double FillPartialTextArray(const LayoutResources& rRes, const SalLayout& rLayout,
                                       KernArray* pKernArray, sal_Int32 nIndex, sal_Int32 nLen,
                                       sal_Int32 nPartIndex, sal_Int32 nPartLen,
                                       const OUString& rCaretStr);

    static double GetPartialTextArray(const LayoutResources& rRes, const TextSpan& rSpan,
                                      KernArray* pKernArray, sal_Int32 nPartIndex,
                                      sal_Int32 nPartLen, bool bCaret,
                                      const vcl::text::LayoutCacheData& rCache,
                                      std::optional<tools::Rectangle>* pBounds);

    static void GetCaretPositions(const LayoutResources& rRes, const TextSpan& rSpan,
                                  std::vector<double>& rCaretPositions,
                                  const LayoutCacheData& rCache);

    static basegfx::B2DHomMatrix
    CalculateOutlineTransform(const SalLayout& rLayout,
                              const vcl::font::FontRealization& rRealization, double nXOffset);

    /** Records font mapping usage for diagnostic purposes if tracking is enabled.
     * This captures which fonts were actually used (including fallbacks) to satisfy the request.
     */
    static void ApplyPositioning(const LayoutResources& rRes, SalLayout& rLayout,
                                 TextLayoutRequest& rArgs, const Point& rLogicalPos,
                                 double nEndGlyphCoord);

    static basegfx::B2DPoint MapLogicalToDevicePos(const LayoutResources& rRes,
                                                   const Point& rLogicalPos);
};

} // namespace vcl::text
