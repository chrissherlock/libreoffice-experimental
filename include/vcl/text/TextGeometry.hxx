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
struct MirroringContext;
struct LayoutResources;

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
};

} // namespace vcl::text
