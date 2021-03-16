/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * This file incorporates work covered by the following license notice:
 *
 *   Licensed to the Apache Software Foundation (ASF) under one or more
 *   contributor license agreements. See the NOTICE file distributed
 *   with this work for additional information regarding copyright
 *   ownership. The ASF licenses this file to you under the Apache
 *   License, Version 2.0 (the "License"); you may not use this file
 *   except in compliance with the License. You may obtain a copy of
 *   the License at http://www.apache.org/licenses/LICENSE-2.0 .
 */

#ifndef INCLUDED_VCL_OUTDEV_HXX
#define INCLUDED_VCL_OUTDEV_HXX

#include <tools/gen.hxx>
#include <tools/ref.hxx>
#include <tools/solar.h>
#include <tools/color.hxx>
#include <tools/poly.hxx>
#include <o3tl/typed_flags_set.hxx>

#include <vcl/dllapi.h>
#include <vcl/RenderContext2.hxx>
#include <vcl/bitmap.hxx>
#include <vcl/cairo.hxx>
#include <vcl/devicecoordinate.hxx>
#include <vcl/flags/SalLayoutFlags.hxx>
#include <vcl/font.hxx>
#include <vcl/region.hxx>
#include <vcl/mapmod.hxx>
#include <vcl/wall.hxx>
#include <vcl/metaactiontypes.hxx>
#include <vcl/salnativewidgets.hxx>
#include <vcl/outdevstate.hxx>

#include <basegfx/numeric/ftools.hxx>
#include <basegfx/vector/b2enums.hxx>
#include <basegfx/polygon/b2dpolypolygon.hxx>

#include <unotools/fontdefs.hxx>

#include <com/sun/star/drawing/LineCap.hpp>
#include <com/sun/star/uno/Reference.h>
#include <com/sun/star/awt/DeviceInfo.hpp>

#include <memory>
#include <vector>

struct ImplOutDevData;
class LogicalFontInstance;
struct SystemGraphicsData;
class ImplFontCache;
class PhysicalFontCollection;
class ImplDeviceFontList;
class ImplDeviceFontSizeList;
class ImplMultiTextLineInfo;
class SalGraphics;
class Gradient;
class Hatch;
class AllSettings;
class BitmapReadAccess;
class BitmapEx;
class Image;
class TextRectInfo;
class FontMetric;
class GDIMetaFile;
class GfxLink;
namespace tools
{
class Line;
}
class LineInfo;
class AlphaMask;
class FontCharMap;
class SalLayout;
class ImplLayoutArgs;
class VirtualDevice;
struct SalTwoRect;
class Printer;
class VCLXGraphics;
class OutDevStateStack;
class SalLayoutGlyphs;

namespace vcl
{
class ExtOutDevData;
class ITextLayout;
struct FontCapabilities;
class TextLayoutCache;
class Window;
namespace font
{
struct Feature;
}
}

namespace basegfx
{
class B2DHomMatrix;
class B2DPolygon;
class B2IVector;
typedef B2IVector B2ISize;
}

namespace com::sun::star::awt
{
class XGraphics;
}

#if defined UNX
#define GLYPH_FONT_HEIGHT 128
#else
#define GLYPH_FONT_HEIGHT 256
#endif

typedef std::vector<tools::Rectangle> MetricVector;

// OutputDevice-Types

// Flags for DrawImage(), these must match the definitions in css::awt::ImageDrawMode
enum class DrawImageFlags
{
    NONE = 0x0000,
    Disable = 0x0001,
    Highlight = 0x0002,
    Deactive = 0x0004,
    ColorTransform = 0x0008,
    SemiTransparent = 0x0010,
};
namespace o3tl
{
template <> struct typed_flags<DrawImageFlags> : is_typed_flags<DrawImageFlags, 0x001f>
{
};
}

// Flags for DrawGrid()
enum class DrawGridFlags
{
    NONE = 0x0000,
    Dots = 0x0001,
    HorzLines = 0x0002,
    VertLines = 0x0004
};
namespace o3tl
{
template <> struct typed_flags<DrawGridFlags> : is_typed_flags<DrawGridFlags, 0x0007>
{
};
}

// Flags for Invert()
enum class InvertFlags
{
    NONE = 0x0000,
    N50 = 0x0001,
    TrackFrame = 0x0002
};
namespace o3tl
{
template <> struct typed_flags<InvertFlags> : is_typed_flags<InvertFlags, 0x0003>
{
};
}

enum OutDevType
{
    OUTDEV_WINDOW,
    OUTDEV_PRINTER,
    OUTDEV_VIRDEV,
    OUTDEV_PDF
};

enum class OutDevViewType
{
    DontKnow,
    PrintPreview,
    SlideShow
};

// OutputDevice

typedef tools::SvRef<FontCharMap> FontCharMapRef;

BmpMirrorFlags AdjustTwoRect(SalTwoRect& rTwoRect, const Size& rSizePix);
void AdjustTwoRect(SalTwoRect& rTwoRect, const tools::Rectangle& rValidSrcRect);

class OutputDevice;

namespace vcl
{
typedef OutputDevice RenderContext;
}

VCL_DLLPUBLIC void DrawFocusRect(vcl::RenderContext& rRenderContext, const tools::Rectangle& rRect);

typedef struct _cairo_surface cairo_surface_t;

/**
* Some things multiple-inherit from VclAbstractDialog and OutputDevice,
* so we need to use virtual inheritance to keep the referencing counting
* OK.
*/
class SAL_WARN_UNUSED VCL_DLLPUBLIC OutputDevice : public RenderContext2
{
    friend class Printer;
    friend class VirtualDevice;
    friend class vcl::Window;
    friend class WorkWindow;
    friend void ImplHandleResize(vcl::Window* pWindow, tools::Long nNewWidth,
                                 tools::Long nNewHeight);

private:
    OutputDevice(const OutputDevice&) = delete;
    OutputDevice& operator=(const OutputDevice&) = delete;

    mutable VclPtr<OutputDevice> mpPrevGraphics; ///< Previous output device in list
    mutable VclPtr<OutputDevice> mpNextGraphics; ///< Next output device in list
    GDIMetaFile* mpMetaFile;
    std::vector<VCLXGraphics*>* mpUnoGraphicsList;
    vcl::ExtOutDevData* mpExtOutDevData;

    const OutDevType meOutDevType;
    OutDevViewType meOutDevViewType;
    Color maTextLineColor;

    /** @name Initialization and accessor functions
     */
    ///@{

protected:
    OutputDevice(OutDevType eOutDevType);
    virtual ~OutputDevice() override;
    virtual void dispose() override;

public:
    void SetConnectMetaFile(GDIMetaFile* pMtf);
    GDIMetaFile* GetConnectMetaFile() const { return mpMetaFile; }

    SystemGraphicsData GetSystemGfxData() const;
    bool SupportsCairo() const;
    /// Create Surface from given cairo surface
    cairo::SurfaceSharedPtr CreateSurface(const cairo::CairoSurfaceSharedPtr& rSurface) const;
    /// Create surface with given dimensions
    cairo::SurfaceSharedPtr CreateSurface(int x, int y, int width, int height) const;
    /// Create Surface for given bitmap data
    cairo::SurfaceSharedPtr CreateBitmapSurface(const BitmapSystemData& rData,
                                                const Size& rSize) const;
    /// Return native handle for underlying surface
    css::uno::Any GetNativeSurfaceHandle(cairo::SurfaceSharedPtr& rSurface,
                                         const basegfx::B2ISize& rSize) const;
    css::uno::Any GetSystemGfxDataAny() const;

    void SetRefPoint() override;
    void SetRefPoint(Point const& rRefPoint) override;

    css::uno::Reference<css::awt::XGraphics> CreateUnoGraphics();
    std::vector<VCLXGraphics*>* GetUnoGraphicsList() const { return mpUnoGraphicsList; }
    std::vector<VCLXGraphics*>* CreateUnoGraphicsList();

    virtual size_t GetSyncCount() const { return 0xffffffff; }

    /** @name Helper functions
     */
    ///@{

public:
    OutDevType GetOutDevType() const { return meOutDevType; }

    /** Query an OutputDevice to see whether it supports a specific operation

     @returns true if operation supported, else false
    */
    bool SupportsOperation(OutDevSupportType) const;

    void SetExtOutDevData(vcl::ExtOutDevData* pExtOutDevData) { mpExtOutDevData = pExtOutDevData; }
    vcl::ExtOutDevData* GetExtOutDevData() const { return mpExtOutDevData; }

    ///@}

public:
    virtual Size GetButtonBorderSize() { return Size(1, 1); };
    virtual Color GetMonochromeButtonColor() { return COL_WHITE; }

    /** @name Direct OutputDevice drawing functions
     */
    ///@{

public:
    virtual void Flush() {}

    virtual void DrawOutDev(const Point& rDestPt, const Size& rDestSize, const Point& rSrcPt,
                            const Size& rSrcSize);

    virtual void DrawOutDev(const Point& rDestPt, const Size& rDestSize, const Point& rSrcPt,
                            const Size& rSrcSize, const OutputDevice& rOutDev);

    virtual void CopyArea(const Point& rDestPt, const Point& rSrcPt, const Size& rSrcSize,
                          bool bWindowInvalidate = false);

protected:
    virtual void CopyDeviceArea(SalTwoRect& aPosAry, bool bWindowInvalidate);

    virtual tools::Rectangle GetBackgroundComponentBounds() const;

    virtual const OutputDevice* DrawOutDevDirectCheck(const OutputDevice& rSrcDev) const;

    virtual void DrawOutDevDirectProcess(const OutputDevice& rSrcDev, SalTwoRect& rPosAry,
                                         SalGraphics* pSrcGraphics);

    SAL_DLLPRIVATE void drawOutDevDirect(const OutputDevice& rSrcDev, SalTwoRect& rPosAry);

private:
    // not implemented; to detect misuses of DrawOutDev(...OutputDevice&);
    SAL_DLLPRIVATE void DrawOutDev(const Point&, const Size&, const Point&, const Size&,
                                   const Printer&)
        = delete;
    ///@}

    /** @name OutputDevice state functions
     */
    ///@{

public:
    void Push(PushFlags nFlags = PushFlags::ALL) override;
    void Pop() override;

    void SetLayoutMode(ComplexTextLayoutFlags nTextLayoutMode) override;
    void SetDigitLanguage(LanguageType) override;
    void SetRasterOp(RasterOp eRasterOp) override;

    /**
    If this OutputDevice is used for displaying a Print Preview
    the OutDevViewType should be set to 'OutDevViewType::PrintPreview'.

    A View can then make painting decisions dependent on this OutDevViewType.
    E.g. text colors need to be handled differently, dependent on whether it's a PrintPreview or not. (see #106611# for more)
    */
    void SetOutDevViewType(OutDevViewType eOutDevViewType) { meOutDevViewType = eOutDevViewType; }
    OutDevViewType GetOutDevViewType() const { return meOutDevViewType; }

    void SetLineColor() override;
    void SetLineColor(Color const& rColor) override;

    void SetFillColor() override;
    void SetFillColor(Color const& rColor) override;

    virtual Color GetBackgroundColor() const;
    virtual Color GetReadableFontColor(Color const& rFontColor, Color const& rBgColor) const;
    virtual void SaveBackground(VirtualDevice& rSaveDevice, const Point& rPos, const Size& rSize,
                                const Size& rBackgroundSize) const;

    void SetFont(vcl::Font const& rNewFont) override;

    ///@}

public:
    /** @name Clipping functions
     */
    ///@{

    void SetClipRegion() override;
    void SetClipRegion(vcl::Region const& rRegion) override;

    void MoveClipRegion(tools::Long nHorzMove, tools::Long nVertMove) override;
    void IntersectClipRegion(tools::Rectangle const& rRect) override;
    void IntersectClipRegion(vcl::Region const& rRegion) override;
    ///@}

public:
    virtual void DrawBorder(tools::Rectangle aBorderRect);

    /** @name Pixel functions
     */
    ///@{

public:
    void DrawPixel(const Point& rPt);
    void DrawPixel(const Point& rPt, const Color& rColor);

    Color GetPixel(const Point& rPt) const;
    ///@}

    /** @name Rectangle functions
     */
    ///@{

public:
    void DrawRect(tools::Rectangle const& rRect) override;
    void DrawRect(const tools::Rectangle& rRect, sal_uLong nHorzRount, sal_uLong nVertRound);

    /// Fill the given rectangle with checkered rectangles of size nLen x nLen using the colors aStart and aEnd
    void DrawCheckered(const Point& rPos, const Size& rSize, sal_uInt32 nLen = 8,
                       Color aStart = COL_WHITE, Color aEnd = COL_BLACK);

    void DrawGrid(const tools::Rectangle& rRect, const Size& rDist, DrawGridFlags nFlags);

    ///@}

    /** @name Invert functions
     */
    ///@{
public:
    void Invert(const tools::Rectangle& rRect, InvertFlags nFlags = InvertFlags::NONE);
    void Invert(const tools::Polygon& rPoly, InvertFlags nFlags = InvertFlags::NONE);
    ///@}

    /** @name Line functions
     */
    ///@{

public:
    void DrawLine(Point const& rStartPt, Point const& rEndPt) override;
    void DrawLine(Point const& rStartPt, Point const& rEndPt, LineInfo const& rLineInfo) override;
    ///@}

    /** @name Polyline functions
     */
    ///@{

public:
    /** Render the given polygon as a line stroke

        The given polygon is stroked with the current LineColor, start
        and end point are not automatically connected

        @see DrawPolygon
        @see DrawPolyPolygon
     */
    void DrawPolyLine(const tools::Polygon& rPoly) override;

    void DrawPolyLine(const basegfx::B2DPolygon&, double fLineWidth = 0.0,
                      basegfx::B2DLineJoin eLineJoin = basegfx::B2DLineJoin::Round,
                      css::drawing::LineCap eLineCap = css::drawing::LineCap_BUTT,
                      double fMiterMinimumAngle = basegfx::deg2rad(15.0));

    /** Render the given polygon as a line stroke

        The given polygon is stroked with the current LineColor, start
        and end point are not automatically connected. The line is
        rendered according to the specified LineInfo, e.g. supplying a
        dash pattern, or a line thickness.

        @see DrawPolygon
        @see DrawPolyPolygon
     */
    void DrawPolyLine(const tools::Polygon& rPoly, const LineInfo& rLineInfo);

    // #i101491#
    // Helper who tries to use SalGDI's DrawPolyLine direct and returns it's bool.
    bool DrawPolyLineDirect(const basegfx::B2DHomMatrix& rObjectTransform,
                            const basegfx::B2DPolygon& rB2DPolygon, double fLineWidth = 0.0,
                            double fTransparency = 0.0,
                            const std::vector<double>* = nullptr, // MM01
                            basegfx::B2DLineJoin eLineJoin = basegfx::B2DLineJoin::NONE,
                            css::drawing::LineCap eLineCap = css::drawing::LineCap_BUTT,
                            double fMiterMinimumAngle = basegfx::deg2rad(15.0));

private:
    // #i101491#
    // Helper which holds the old line geometry creation and is extended to use AA when
    // switched on. Advantage is that line geometry is only temporarily used for paint
    SAL_DLLPRIVATE void drawPolyLine(const tools::Polygon& rPoly, const LineInfo& rLineInfo);

    ///@}

    /** @name Polygon functions
     */
    ///@{

public:
    void DrawPolygon(tools::Polygon const& rPoly) override;
    void DrawPolygon(const basegfx::B2DPolygon&);

    /** Render the given poly-polygon

        The given poly-polygon is stroked with the current LineColor,
        and filled with the current FillColor. If one of these colors
        are transparent, the corresponding stroke or fill stays
        invisible. Start and end points of the contained polygons are
        automatically connected.

        @see DrawPolyLine
     */
    void DrawPolyPolygon(tools::PolyPolygon const& rPolyPoly) override;
    void DrawPolyPolygon(const basegfx::B2DPolyPolygon&);

    ///@}

public:
    /** @name Curved shape functions
     */
    ///@{

    void DrawEllipse(tools::Rectangle const& rRect) override;
    void DrawArc(tools::Rectangle const& rRect, Point const& rStartPt,
                 Point const& rEndPt) override;
    void DrawPie(tools::Rectangle const& rRect, Point const& rStartPt,
                 Point const& rEndPt) override;
    void DrawChord(tools::Rectangle const& rRect, Point const& rStartPt,
                   Point const& rEndPt) override;

    ///@}

    /** @name Gradient functions
     */
    ///@{

public:
    void DrawGradient(tools::Rectangle const& rRect, Gradient const& rGradient) override;
    void DrawGradient(tools::PolyPolygon const& rPolyPoly, Gradient const& rGradient) override;

    void AddGradientActions(const tools::Rectangle& rRect, const Gradient& rGradient,
                            GDIMetaFile& rMtf);

private:
    SAL_DLLPRIVATE void DrawLinearGradientToMetafile(const tools::Rectangle& rRect,
                                                     const Gradient& rGradient);
    SAL_DLLPRIVATE void DrawComplexGradientToMetafile(const tools::Rectangle& rRect,
                                                      const Gradient& rGradient);
    ///@}

    /** @name Hatch functions
     */
    ///@{

public:
#ifdef _MSC_VER
    void DrawHatch(tools::PolyPolygon const& rPolyPoly, ::Hatch const& rHatch) override;
    void AddHatchActions(tools::PolyPolygon const& rPolyPoly, ::Hatch const& rHatch,
                         GDIMetaFile& rMtf);
#else
    void DrawHatch(tools::PolyPolygon const& rPolyPoly, Hatch const& rHatch) override;
    void AddHatchActions(tools::PolyPolygon const& rPolyPoly, Hatch const& rHatch,
                         GDIMetaFile& rMtf);
#endif

protected:
    void DrawHatchLine(Point const& rStartPoint, Point const& rEndPoint) override;
    void DrawHatchLines(tools::Line const& rLine, tools::PolyPolygon const& rPolyPoly,
                        Point* pPtBuffer) override;
    ///@}

    /** @name Wallpaper functions
     */
    ///@{

public:
    void DrawWallpaper(const tools::Rectangle& rRect, const Wallpaper& rWallpaper);

    void Erase();
    void Erase(const tools::Rectangle& rRect);

protected:
    void DrawGradientWallpaper(tools::Long nX, tools::Long nY, tools::Long nWidth,
                               tools::Long nHeight, const Wallpaper& rWallpaper);

private:
    SAL_DLLPRIVATE void DrawWallpaper(tools::Long nX, tools::Long nY, tools::Long nWidth,
                                      tools::Long nHeight, const Wallpaper& rWallpaper);
    SAL_DLLPRIVATE void DrawColorWallpaper(tools::Long nX, tools::Long nY, tools::Long nWidth,
                                           tools::Long nHeight, const Wallpaper& rWallpaper);
    SAL_DLLPRIVATE void DrawBitmapWallpaper(tools::Long nX, tools::Long nY, tools::Long nWidth,
                                            tools::Long nHeight, const Wallpaper& rWallpaper);
    ///@}

    /** @name Text functions
     */
    ///@{

public:
    void DrawText(const Point& rStartPt, const OUString& rStr, sal_Int32 nIndex = 0,
                  sal_Int32 nLen = -1, MetricVector* pVector = nullptr,
                  OUString* pDisplayText = nullptr, const SalLayoutGlyphs* pLayoutCache = nullptr);

    void DrawText(const tools::Rectangle& rRect, const OUString& rStr,
                  DrawTextFlags nStyle = DrawTextFlags::NONE, MetricVector* pVector = nullptr,
                  OUString* pDisplayText = nullptr, vcl::ITextLayout* _pTextLayout = nullptr);

    static void ImplDrawText(OutputDevice& rTargetDevice, const tools::Rectangle& rRect,
                             const OUString& rOrigStr, DrawTextFlags nStyle, MetricVector* pVector,
                             OUString* pDisplayText, vcl::ITextLayout& _rLayout);

    void ImplDrawText(SalLayout&);

    void ImplDrawTextBackground(const SalLayout&);

    void DrawCtrlText(const Point& rPos, const OUString& rStr, sal_Int32 nIndex = 0,
                      sal_Int32 nLen = -1, DrawTextFlags nStyle = DrawTextFlags::Mnemonic,
                      MetricVector* pVector = nullptr, OUString* pDisplayText = nullptr,
                      const SalLayoutGlyphs* pGlyphs = nullptr);

    void DrawTextLine(const Point& rPos, tools::Long nWidth, FontStrikeout eStrikeout,
                      FontLineStyle eUnderline, FontLineStyle eOverline,
                      bool bUnderlineAbove = false);

    void ImplDrawTextLine(tools::Long nBaseX, tools::Long nX, tools::Long nY,
                          DeviceCoordinate nWidth, FontStrikeout eStrikeout,
                          FontLineStyle eUnderline, FontLineStyle eOverline, bool bUnderlineAbove);

    void ImplDrawTextLines(SalLayout&, FontStrikeout eStrikeout, FontLineStyle eUnderline,
                           FontLineStyle eOverline, bool bWordLine, bool bUnderlineAbove);

    void DrawWaveLine(const Point& rStartPos, const Point& rEndPos, tools::Long nLineWidth = 1);

    bool ImplDrawRotateText(SalLayout&);

    tools::Rectangle GetTextRect(const tools::Rectangle& rRect, const OUString& rStr,
                                 DrawTextFlags nStyle = DrawTextFlags::WordBreak,
                                 TextRectInfo* pInfo = nullptr,
                                 const vcl::ITextLayout* _pTextLayout = nullptr) const;

    tools::Rectangle ImplGetTextBoundRect(const SalLayout&);

    bool GetTextOutline(tools::PolyPolygon&, const OUString& rStr) const;

    bool GetTextOutlines(PolyPolyVector&, const OUString& rStr, sal_Int32 nBase = 0,
                         sal_Int32 nIndex = 0, sal_Int32 nLen = -1, sal_uLong nLayoutWidth = 0,
                         const tools::Long* pDXArray = nullptr) const;

    bool GetTextOutlines(basegfx::B2DPolyPolygonVector& rVector, const OUString& rStr,
                         sal_Int32 nBase, sal_Int32 nIndex = 0, sal_Int32 nLen = -1,
                         sal_uLong nLayoutWidth = 0, const tools::Long* pDXArray = nullptr) const;

    OUString GetEllipsisString(const OUString& rStr, tools::Long nMaxWidth,
                               DrawTextFlags nStyle = DrawTextFlags::EndEllipsis) const;

    tools::Long GetCtrlTextWidth(const OUString& rStr,
                                 const SalLayoutGlyphs* pLayoutCache = nullptr) const;

    static OUString GetNonMnemonicString(const OUString& rStr, sal_Int32& rMnemonicPos);

    static OUString GetNonMnemonicString(const OUString& rStr)
    {
        sal_Int32 nDummy;
        return GetNonMnemonicString(rStr, nDummy);
    }

    /** Generate MetaTextActions for the text rect

        This method splits up the text rect into multiple
        MetaTextActions, one for each line of text. This is comparable
        to AddGradientActions(), which splits up a gradient into its
        constituent polygons. Parameter semantics fully compatible to
        DrawText().
     */
    void AddTextRectActions(const tools::Rectangle& rRect, const OUString& rOrigStr,
                            DrawTextFlags nStyle, GDIMetaFile& rMtf);

    void SetTextColor(Color const& rColor) override;
    void SetTextFillColor() override;
    void SetTextFillColor(Color const& rColor) override;
    void SetTextLineColor() override;
    void SetTextLineColor(Color const& rColor) override;
    void SetOverlineColor() override;
    void SetOverlineColor(Color const& rColor) override;

    void SetTextAlign(TextAlign eAlign) override;

    /** Height where any character of the current font fits; in logic coordinates.

        See also GetTextBoundRect() for more explanation + code examples.
    */
    tools::Long GetTextHeight() const;
    float approximate_digit_width() const;

    void DrawTextArray(const Point& rStartPt, const OUString& rStr, const tools::Long* pDXAry,
                       sal_Int32 nIndex = 0, sal_Int32 nLen = -1,
                       SalLayoutFlags flags = SalLayoutFlags::NONE,
                       const SalLayoutGlyphs* pLayoutCache = nullptr);

    void GetCaretPositions(const OUString&, tools::Long* pCaretXArray, sal_Int32 nIndex,
                           sal_Int32 nLen, const SalLayoutGlyphs* pGlyphs = nullptr) const;
    void DrawStretchText(const Point& rStartPt, sal_uLong nWidth, const OUString& rStr,
                         sal_Int32 nIndex = 0, sal_Int32 nLen = -1);
    sal_Int32 GetTextBreak(const OUString& rStr, tools::Long nTextWidth, sal_Int32 nIndex,
                           sal_Int32 nLen = -1, tools::Long nCharExtra = 0,
                           vcl::TextLayoutCache const* = nullptr,
                           const SalLayoutGlyphs* pGlyphs = nullptr) const;
    sal_Int32 GetTextBreak(const OUString& rStr, tools::Long nTextWidth, sal_Unicode nExtraChar,
                           sal_Int32& rExtraCharPos, sal_Int32 nIndex, sal_Int32 nLen,
                           tools::Long nCharExtra, vcl::TextLayoutCache const* = nullptr) const;
    static std::shared_ptr<vcl::TextLayoutCache> CreateTextLayoutCache(OUString const&);

protected:
    SAL_DLLPRIVATE void ImplInitTextLineSize();
    SAL_DLLPRIVATE void ImplInitAboveTextLineSize();
    static SAL_DLLPRIVATE tools::Long ImplGetTextLines(ImplMultiTextLineInfo& rLineInfo,
                                                       tools::Long nWidth, const OUString& rStr,
                                                       DrawTextFlags nStyle,
                                                       const vcl::ITextLayout& _rLayout);
    SAL_DLLPRIVATE float approximate_char_width() const;

private:
    SAL_DLLPRIVATE void ImplInitTextColor();

    SAL_DLLPRIVATE void ImplDrawTextDirect(SalLayout&, bool bTextLines);
    SAL_DLLPRIVATE void ImplDrawSpecialText(SalLayout&);
    SAL_DLLPRIVATE void ImplDrawTextRect(tools::Long nBaseX, tools::Long nBaseY, tools::Long nX,
                                         tools::Long nY, tools::Long nWidth, tools::Long nHeight);

    SAL_DLLPRIVATE static void ImplDrawWavePixel(tools::Long nOriginX, tools::Long nOriginY,
                                                 tools::Long nCurX, tools::Long nCurY,
                                                 Degree10 nOrientation, SalGraphics* pGraphics,
                                                 const OutputDevice& rOutDev, bool bDrawPixAsRect,
                                                 tools::Long nPixWidth, tools::Long nPixHeight);
    SAL_DLLPRIVATE void ImplDrawWaveLine(tools::Long nBaseX, tools::Long nBaseY,
                                         tools::Long nStartX, tools::Long nStartY,
                                         tools::Long nWidth, tools::Long nHeight,
                                         tools::Long nLineWidth, Degree10 nOrientation,
                                         const Color& rColor);
    SAL_DLLPRIVATE void ImplDrawWaveTextLine(tools::Long nBaseX, tools::Long nBaseY, tools::Long nX,
                                             tools::Long nY, tools::Long nWidth,
                                             FontLineStyle eTextLine, Color aColor, bool bIsAbove);
    SAL_DLLPRIVATE void ImplDrawStraightTextLine(tools::Long nBaseX, tools::Long nBaseY,
                                                 tools::Long nX, tools::Long nY, tools::Long nWidth,
                                                 FontLineStyle eTextLine, Color aColor,
                                                 bool bIsAbove);
    SAL_DLLPRIVATE void ImplDrawStrikeoutLine(tools::Long nBaseX, tools::Long nBaseY,
                                              tools::Long nX, tools::Long nY, tools::Long nWidth,
                                              FontStrikeout eStrikeout, Color aColor);
    SAL_DLLPRIVATE void ImplDrawStrikeoutChar(tools::Long nBaseX, tools::Long nBaseY,
                                              tools::Long nX, tools::Long nY, tools::Long nWidth,
                                              FontStrikeout eStrikeout, Color aColor);
    SAL_DLLPRIVATE void ImplDrawMnemonicLine(tools::Long nX, tools::Long nY, tools::Long nWidth);

    ///@}

    /** @name Layout functions
     */
    ///@{

public:
    SAL_DLLPRIVATE void ReMirror(Point& rPoint) const;
    SAL_DLLPRIVATE void ReMirror(tools::Rectangle& rRect) const;
    SAL_DLLPRIVATE void ReMirror(vcl::Region& rRegion) const;
    SAL_DLLPRIVATE bool ImplIsRecordLayout() const;
    virtual bool HasMirroredGraphics() const;

    // Enabling/disabling RTL only makes sense for OutputDevices that use a mirroring SalGraphicsLayout
    virtual void EnableRTL(bool bEnable = true);
    bool GetTextIsRTL(const OUString&, sal_Int32 nIndex, sal_Int32 nLen) const;

    ///@}

    /** @name Bitmap functions
     */
    ///@{

public:
    void DrawBitmap(Point const& rDestPt, Bitmap const& rBitmap) override;
    void DrawBitmap(Point const& rDestPt, Size const& rDestSize, Bitmap const& rBitmap) override;
    void DrawBitmap(Point const& rDestPt, Size const& rDestSize, Point const& rSrcPtPixel,
                    Size const& rSrcSizePixel, Bitmap const& rBitmap) override;

    /** @overload
        void DrawBitmapEx(
                const Point& rDestPt,
                const Size& rDestSize,
                const Point& rSrcPtPixel,
                const Size& rSecSizePixel,
                const BitmapEx& rBitmapEx,
                MetaActionType nAction = MetaActionType::BMPEXSCALEPART)
     */
    void DrawBitmapEx(const Point& rDestPt, const BitmapEx& rBitmapEx);

    /** @overload
        void DrawBitmapEx(
                const Point& rDestPt,
                const Size& rDestSize,
                const Point& rSrcPtPixel,
                const Size& rSecSizePixel,
                const BitmapEx& rBitmapEx,
                MetaActionType nAction = MetaActionType::BMPEXSCALEPART)
     */
    void DrawBitmapEx(const Point& rDestPt, const Size& rDestSize, const BitmapEx& rBitmapEx);

    void DrawBitmapEx(const Point& rDestPt, const Size& rDestSize, const Point& rSrcPtPixel,
                      const Size& rSrcSizePixel, const BitmapEx& rBitmapEx,
                      MetaActionType nAction = MetaActionType::BMPEXSCALEPART);

    /** @overload
        virtual void DrawImage(
                        const Point& rPos,
                        const Size& rSize,
                        const Image& rImage,
                        sal_uInt16 nStyle = 0)
     */
    void DrawImage(const Point& rPos, const Image& rImage,
                   DrawImageFlags nStyle = DrawImageFlags::NONE);

    void DrawImage(const Point& rPos, const Size& rSize, const Image& rImage,
                   DrawImageFlags nStyle = DrawImageFlags::NONE);

    /** Query extended bitmap (with alpha channel, if available).
     */
    BitmapEx GetBitmapEx(const Point& rSrcPt, const Size& rSize) const;

    /** Draw BitmapEx transformed

        @param rTransformation
        The transformation describing the target positioning of the given bitmap. Transforming
        the unit object coordinates (0, 0, 1, 1) with this matrix is the transformation to
        discrete coordinates

        @param rBitmapEx
        The BitmapEx to be painted

        @param fAlpha
        Optional additional alpha to use for drawing (0 to 1, 1 being no change).
    */
    void DrawTransformedBitmapEx(const basegfx::B2DHomMatrix& rTransformation,
                                 const BitmapEx& rBitmapEx, double fAlpha = 1.0);

    /** Return true if DrawTransformedBitmapEx() is fast.

        @since 7.2
    */
    bool HasFastDrawTransformedBitmap() const;

protected:
    void DrawScaledBitmap(Point const& rDestPt, Size const& rDestSize, const Point& rSrcPtPixel,
                    Size const& rSrcSizePixel, Bitmap const& rBitmap) override;

    virtual void DrawDeviceBitmapEx(Point const& rDestPt, Size const& rDestSize,
                                    Point const& rSrcPtPixel, Size const& rSrcSizePixel,
                                    BitmapEx& rBitmapEx);

    /** Transform and draw a bitmap directly

     @param     aFullTransform      The B2DHomMatrix used for the transformation
     @param     rBitmapEx           Reference to the bitmap to be transformed and drawn

     @return true if it was able to draw the bitmap, false if not
     */
    virtual bool DrawTransformBitmapExDirect(const basegfx::B2DHomMatrix& aFullTransform,
                                             const BitmapEx& rBitmapEx, double fAlpha = 1.0);

    /** Transform and reduce the area that needs to be drawn of the bitmap and return the new
        visible range and the maximum area.


      @param     aFullTransform      B2DHomMatrix used for transformation
      @param     aVisibleRange       The new visible area of the bitmap
      @param     fMaximumArea        The maximum area of the bitmap

      @returns true if there is an area to be drawn, otherwise nothing is left to be drawn
        so return false
      */
    virtual bool
    TransformAndReduceBitmapExToTargetRange(const basegfx::B2DHomMatrix& aFullTransform,
                                            basegfx::B2DRange& aVisibleRange, double& fMaximumArea);

private:
    SAL_DLLPRIVATE void DrawDeviceAlphaBitmap(Bitmap const& rBmp, AlphaMask const& rAlpha,
                                              Point const& rDestPt, Size const& rDestSize,
                                              Point const& rSrcPtPixel, Size const& rSrcSizePixel);

    SAL_DLLPRIVATE void DrawDeviceAlphaBitmapSlowPath(Bitmap const& rBitmap,
                                                      AlphaMask const& rAlpha,
                                                      tools::Rectangle aDstRect,
                                                      tools::Rectangle aBmpRect, Size const& aOutSz,
                                                      Point const& aOutPt);

    SAL_DLLPRIVATE void BlendBitmap(const SalTwoRect& rPosAry, const Bitmap& rBmp);

    SAL_DLLPRIVATE Bitmap BlendBitmap(Bitmap& aBmp, BitmapReadAccess const* pP,
                                      BitmapReadAccess const* pA, const sal_Int32 nOffY,
                                      const sal_Int32 nDstHeight, const sal_Int32 nOffX,
                                      const sal_Int32 nDstWidth, const tools::Rectangle& aBmpRect,
                                      const Size& aOutSz, const bool bHMirr, const bool bVMirr,
                                      const tools::Long* pMapX, const tools::Long* pMapY);

    SAL_DLLPRIVATE Bitmap BlendBitmapWithAlpha(Bitmap& aBmp, BitmapReadAccess const* pP,
                                               BitmapReadAccess const* pA,
                                               const tools::Rectangle& aDstRect,
                                               const sal_Int32 nOffY, const sal_Int32 nDstHeight,
                                               const sal_Int32 nOffX, const sal_Int32 nDstWidth,
                                               const tools::Long* pMapX, const tools::Long* pMapY);
    ///@}

    /** @name Transparency functions
     */
    ///@{

public:
    /** helper method removing transparencies from a metafile (e.g. for printing)

        @returns
        true: transparencies were removed
        false: output metafile is unchanged input metafile

        @attention this is a member method, so current state can influence the result !
        @attention the output metafile is prepared in pixel mode for the currentOutputDevice
                   state. It can not be moved or rotated reliably anymore.
    */
    bool RemoveTransparenciesFromMetaFile(const GDIMetaFile& rInMtf, GDIMetaFile& rOutMtf,
                                          tools::Long nMaxBmpDPIX, tools::Long nMaxBmpDPIY,
                                          bool bReduceTransparency, bool bTransparencyAutoMode,
                                          bool bDownsampleBitmaps,
                                          const Color& rBackground = COL_TRANSPARENT);

    void DrawTransparent(const tools::PolyPolygon& rPolyPoly, sal_uInt16 nTransparencePercent);

    void DrawTransparent(const basegfx::B2DHomMatrix& rObjectTransform,
                         const basegfx::B2DPolyPolygon& rB2DPolyPoly, double fTransparency);

    void DrawTransparent(const GDIMetaFile& rMtf, const Point& rPos, const Size& rSize,
                         const Gradient& rTransparenceGradient);

protected:
    virtual void EmulateDrawTransparent(const tools::PolyPolygon& rPolyPoly,
                                        sal_uInt16 nTransparencePercent);
    void DrawInvisiblePolygon(const tools::PolyPolygon& rPolyPoly);

    virtual void ClipAndDrawGradientMetafile(const Gradient& rGradient,
                                             const tools::PolyPolygon& rPolyPoly);

private:
    SAL_DLLPRIVATE bool DrawTransparentNatively(const tools::PolyPolygon& rPolyPoly,
                                                sal_uInt16 nTransparencePercent);
    ///@}

    /** @name Mask functions
     */
    ///@{

public:
    void DrawMask(const Point& rDestPt, const Bitmap& rBitmap, const Color& rMaskColor);

    void DrawMask(const Point& rDestPt, const Size& rDestSize, const Bitmap& rBitmap,
                  const Color& rMaskColor);

    void DrawMask(const Point& rDestPt, const Size& rDestSize, const Point& rSrcPtPixel,
                  const Size& rSrcSizePixel, const Bitmap& rBitmap, const Color& rMaskColor,
                  MetaActionType nAction);

protected:
    virtual void DrawDeviceMask(const Bitmap& rMask, const Color& rMaskColor, const Point& rDestPt,
                                const Size& rDestSize, const Point& rSrcPtPixel,
                                const Size& rSrcSizePixel);
    ///@}

    /** @name Map functions
     */
    ///@{

public:
    void SetMapMode() override;
    void SetMapMode(MapMode const& rNewMapMode) override;
    virtual void SetMetafileMapMode(const MapMode& rNewMapMode, bool bIsRecord);
    ///@}

    /** @name Native Widget Rendering functions

        These all just call through to the private mpGraphics functions of the same name.
     */
    ///@{

public:
    /** Determine if native widgets can be enabled
     */
    virtual bool CanEnableNativeWidget() const { return false; }

    /** Query the platform layer for control support
     */
    bool IsNativeControlSupported(ControlType nType, ControlPart nPart) const;

    /** Query the native control to determine if it was acted upon
     */
    bool HitTestNativeScrollbar(ControlPart nPart, const tools::Rectangle& rControlRegion,
                                const Point& aPos, bool& rIsInside) const;

    /** Request rendering of a particular control and/or part
     */
    bool DrawNativeControl(ControlType nType, ControlPart nPart,
                           const tools::Rectangle& rControlRegion, ControlState nState,
                           const ImplControlValue& aValue, const OUString& aCaption,
                           const Color& rBackgroundColor = COL_AUTO);

    /** Query the native control's actual drawing region (including adornment)
     */
    bool GetNativeControlRegion(ControlType nType, ControlPart nPart,
                                const tools::Rectangle& rControlRegion, ControlState nState,
                                const ImplControlValue& aValue,
                                tools::Rectangle& rNativeBoundingRegion,
                                tools::Rectangle& rNativeContentRegion) const;
    ///@}

    /** @name EPS functions
     */
    ///@{

public:
    /** @returns boolean value to see if EPS could be painted directly.
        Theoretically, handing over a matrix would be needed to handle
        painting rotated EPS files (e.g. contained in Metafiles). This
        would then need to be supported for Mac and PS printers, but
        that's too much for now, wrote \#i107046# for this */
    bool DrawEPS(const Point& rPt, const Size& rSz, const GfxLink& rGfxLink,
                 GDIMetaFile* pSubst = nullptr);
    ///@}

public:
    virtual css::awt::DeviceInfo GetDeviceInfo() const;

protected:
    css::awt::DeviceInfo GetCommonDeviceInfo(Size const& aDevSize) const;
};

#endif // INCLUDED_VCL_OUTDEV_HXX

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
