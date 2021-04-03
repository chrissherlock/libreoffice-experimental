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

#pragma once

#include <vcl/RenderContext2.hxx>
#include <vcl/OutDevType.hxx>
#include <vcl/OutDevViewType.hxx>
#include <vcl/cairo.hxx>
#include <vcl/flags/DrawFlags.hxx>
#include <vcl/metaactiontypes.hxx>
#include <vcl/outdevstate.hxx>

#include <memory>
#include <vector>

namespace tools
{
class Line;
}

namespace basegfx
{
class B2DHomMatrix;
class B2DPolygon;
class B2IVector;
typedef B2IVector B2ISize;
}

class Animation;
class BitmapEx;
class FontCharMap;
class GDIMetaFile;
class GfxLink;
class Gradient;
class Hatch;
class Image;
class LineInfo;
class OutputDevice;
class Printer;
class SalLayoutGlyphs;
class VCLXGraphics;
class VirtualDevice;

namespace vcl
{
class ExtOutDevData;
class ITextLayout;
class Window;
typedef OutputDevice RenderContext;
}

namespace com::sun::star::awt
{
class XGraphics;
}

typedef tools::SvRef<FontCharMap> FontCharMapRef;

typedef struct _cairo_surface cairo_surface_t;

class SAL_WARN_UNUSED VCL_DLLPUBLIC OutputDevice : public RenderContext2
{
    friend class Printer;
    friend class VirtualDevice;
    friend class vcl::Window;
    friend class WorkWindow;
    friend void ImplHandleResize(vcl::Window* pWindow, tools::Long nNewWidth,
                                 tools::Long nNewHeight);

public:
    void SetConnectMetaFile(GDIMetaFile* pMtf);
    GDIMetaFile* GetConnectMetaFile() const { return mpMetaFile; }

    void SetRefPoint() override;
    void SetRefPoint(Point const& rRefPoint) override;

    css::uno::Reference<css::awt::XGraphics> CreateUnoGraphics();
    std::vector<VCLXGraphics*>* GetUnoGraphicsList() const { return mpUnoGraphicsList; }
    std::vector<VCLXGraphics*>* CreateUnoGraphicsList();

    /** @name Helper functions
     */
    ///@{

    OutDevType GetOutDevType() const { return meOutDevType; }

    /** Query an OutputDevice to see whether it supports a specific operation

     @returns true if operation supported, else false
    */
    bool SupportsOperation(OutDevSupportType) const;
    ///@}

    /** @name Direct OutputDevice drawing functions
     */
    ///@{

    void DrawOutDev(Point const& rDestPt, Size const& rDestSize, Point const& rSrcPt,
                    Size const& rSrcSize) override;

    void DrawOutDev(Point const& rDestPt, Size const& rDestSize, Point const& rSrcPt,
                    Size const& rSrcSize, RenderContext2 const& rOutDev) override;
    ///@}

    /** @name OutputDevice state functions
     */
    ///@{

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

    void SetFont(vcl::Font const& rNewFont) override;

    ///@}

    /** @name Clipping functions
     */
    ///@{

    void SetClipRegion() override;
    void SetClipRegion(vcl::Region const& rRegion) override;

    void MoveClipRegion(tools::Long nHorzMove, tools::Long nVertMove) override;
    void IntersectClipRegion(tools::Rectangle const& rRect) override;
    void IntersectClipRegion(vcl::Region const& rRegion) override;
    ///@}

    /** @name Pixel functions
     */
    ///@{

    void DrawPixel(Point const& rPt) override;
    void DrawPixel(Point const& rPt, Color const& rColor) override;
    ///@}

    /** @name Rectangle functions
     */
    ///@{

    void DrawRect(tools::Rectangle const& rRect) override;
    void DrawRect(tools::Rectangle const& rRect, sal_uLong nHorzRount,
                  sal_uLong nVertRound) override;

    void DrawGrid(tools::Rectangle const& rRect, Size const& rDist, DrawGridFlags nFlags) override;
    void DrawBorder(tools::Rectangle aBorderRect) override;

    /// Fill the given rectangle with checkered rectangles of size nLen x nLen using the colors aStart and aEnd
    void DrawCheckered(const Point& rPos, const Size& rSize, sal_uInt32 nLen = 8,
                       Color aStart = COL_WHITE, Color aEnd = COL_BLACK) override;

    ///@}

    /** @name Line functions
     */
    ///@{

    void DrawLine(Point const& rStartPt, Point const& rEndPt) override;
    void DrawLine(Point const& rStartPt, Point const& rEndPt, LineInfo const& rLineInfo) override;

    ///@}

    /** @name Polyline functions
     */
    ///@{

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
                      double fMiterMinimumAngle = basegfx::deg2rad(15.0)) override;

    /** Render the given polygon as a line stroke

        The given polygon is stroked with the current LineColor, start
        and end point are not automatically connected. The line is
        rendered according to the specified LineInfo, e.g. supplying a
        dash pattern, or a line thickness.

        @see DrawPolygon
        @see DrawPolyPolygon
     */
    void DrawPolyLine(tools::Polygon const& rPoly, LineInfo const& rLineInfo) override;

    // #i101491#
    // Helper who tries to use SalGDI's DrawPolyLine direct and returns it's bool.
    bool DrawPolyLineDirect(const basegfx::B2DHomMatrix& rObjectTransform,
                            const basegfx::B2DPolygon& rB2DPolygon, double fLineWidth = 0.0,
                            double fTransparency = 0.0,
                            const std::vector<double>* = nullptr, // MM01
                            basegfx::B2DLineJoin eLineJoin = basegfx::B2DLineJoin::NONE,
                            css::drawing::LineCap eLineCap = css::drawing::LineCap_BUTT,
                            double fMiterMinimumAngle = basegfx::deg2rad(15.0)) override;
    ///@}

    /** @name Polygon functions
     */
    ///@{

    void DrawPolygon(tools::Polygon const& rPoly) override;
    void DrawPolygon(basegfx::B2DPolygon const&);

    /** Render the given poly-polygon

        The given poly-polygon is stroked with the current LineColor,
        and filled with the current FillColor. If one of these colors
        are transparent, the corresponding stroke or fill stays
        invisible. Start and end points of the contained polygons are
        automatically connected.

        @see DrawPolyLine
     */
    void DrawPolyPolygon(tools::PolyPolygon const& rPolyPoly) override;
    void DrawPolyPolygon(basegfx::B2DPolyPolygon const& rPolyPoly) override;

    ///@}

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

    void DrawGradient(tools::Rectangle const& rRect, Gradient const& rGradient) override;
    void DrawGradient(tools::PolyPolygon const& rPolyPoly, Gradient const& rGradient) override;

    void AddGradientActions(const tools::Rectangle& rRect, const Gradient& rGradient,
                            GDIMetaFile& rMtf);

    ///@}

    /** @name Hatch functions
     */
    ///@{

#ifdef _MSC_VER
    void DrawHatch(tools::PolyPolygon const& rPolyPoly, ::Hatch const& rHatch) override;
    void AddHatchActions(tools::PolyPolygon const& rPolyPoly, ::Hatch const& rHatch,
                         GDIMetaFile& rMtf);
#else
    void DrawHatch(tools::PolyPolygon const& rPolyPoly, Hatch const& rHatch) override;
    void AddHatchActions(tools::PolyPolygon const& rPolyPoly, Hatch const& rHatch,
                         GDIMetaFile& rMtf);
#endif

    ///@}

    /** @name Wallpaper functions
     */
    ///@{

    void DrawWallpaper(const tools::Rectangle& rRect, const Wallpaper& rWallpaper) override;

    void Erase() override;
    void Erase(tools::Rectangle const& rRect) override;

    ///@}

    /** @name Text functions
     */
    ///@{

    void DrawText(Point const& rStartPt, OUString const& rStr, sal_Int32 nIndex = 0,
                  sal_Int32 nLen = -1, std::vector<tools::Rectangle>* pVector = nullptr,
                  OUString* pDisplayText = nullptr,
                  SalLayoutGlyphs const* pLayoutCache = nullptr) override;

    void DrawText(tools::Rectangle const& rRect, OUString const& rStr,
                  DrawTextFlags nStyle = DrawTextFlags::NONE,
                  std::vector<tools::Rectangle>* pVector = nullptr,
                  OUString* pDisplayText = nullptr,
                  vcl::ITextLayout* _pTextLayout = nullptr) override;

    void DrawTextLine(const Point& rPos, tools::Long nWidth, FontStrikeout eStrikeout,
                      FontLineStyle eUnderline, FontLineStyle eOverline,
                      bool bUnderlineAbove = false) override;

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
    virtual void SetSystemTextColor(DrawFlags nFlags, bool bEnabled = false);
    void SetTextFillColor() override;
    void SetTextFillColor(Color const& rColor) override;
    void SetTextLineColor() override;
    void SetTextLineColor(Color const& rColor) override;
    void SetOverlineColor() override;
    void SetOverlineColor(Color const& rColor) override;

    void SetTextAlign(TextAlign eAlign) override;

    void DrawTextArray(Point const& rStartPt, OUString const& rStr, tools::Long const* pDXAry,
                       sal_Int32 nIndex = 0, sal_Int32 nLen = -1,
                       SalLayoutFlags flags = SalLayoutFlags::NONE,
                       SalLayoutGlyphs const* pLayoutCache = nullptr) override;

    void DrawStretchText(Point const& rStartPt, sal_uLong nWidth, OUString const& rStr,
                         sal_Int32 nIndex = 0, sal_Int32 nLen = -1) override;
    ///@}

    /** @name Bitmap functions
     */
    ///@{

    void DrawBitmap(Point const& rDestPt, Bitmap const& rBitmap) override;
    void DrawBitmap(Point const& rDestPt, Size const& rDestSize, Bitmap const& rBitmap) override;
    void DrawBitmap(Point const& rDestPt, Size const& rDestSize, Point const& rSrcPtPixel,
                    Size const& rSrcSizePixel, Bitmap const& rBitmap) override;

    void DrawBitmapEx(const Point& rDestPt, const BitmapEx& rBitmapEx) override;
    void DrawBitmapEx(const Point& rDestPt, const Size& rDestSize,
                      const BitmapEx& rBitmapEx) override;
    void DrawBitmapEx(const Point& rDestPt, const Size& rDestSize, const Point& rSrcPtPixel,
                      const Size& rSrcSizePixel, const BitmapEx& rBitmapEx) override;

    void DrawImage(Point const& rPos, Image const& rImage,
                   DrawImageFlags nStyle = DrawImageFlags::NONE) override;
    void DrawImage(Point const& rPos, Size const& rSize, Image const& rImage,
                   DrawImageFlags nStyle = DrawImageFlags::NONE) override;

    void DrawAnimation(Animation* const pAnim, Point const& rDestPt,
                       Size const& rDestSz) const override;

    void DrawTransformedBitmapEx(const basegfx::B2DHomMatrix& rTransformation,
                                 const BitmapEx& rBitmapEx, double fAlpha = 1.0) override;

    ///@}

    /** @name Transparency functions
     */
    ///@{

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

    void DrawTransparent(tools::PolyPolygon const& rPolyPoly,
                         sal_uInt16 nTransparencePercent) override;

    void DrawTransparent(basegfx::B2DHomMatrix const& rObjectTransform,
                         basegfx::B2DPolyPolygon const& rB2DPolyPoly,
                         double fTransparency) override;

    void DrawTransparent(const GDIMetaFile& rMtf, const Point& rPos, const Size& rSize,
                         const Gradient& rTransparenceGradient);

    ///@}

    /** @name Mask functions
     */
    ///@{

    void DrawMask(Point const& rDestPt, Bitmap const& rBitmap, Color const& rMaskColor) override;
    void DrawMask(Point const& rDestPt, Size const& rDestSize, Bitmap const& rBitmap,
                  Color const& rMaskColor) override;
    void DrawScaledMask(Point const& rDestPt, Size const& rDestSize, Point const& rSrcPtPixel,
                        Size const& rSrcSizePixel, Bitmap const& rBitmap,
                        Color const& rMaskColor) override;
    void DrawMask(Point const& rDestPt, Size const& rDestSize, Point const& rSrcPtPixel,
                  Size const& rSrcSizePixel, Bitmap const& rBitmap,
                  Color const& rMaskColor) override;
    ///@}

    /** @name Map functions
     */
    ///@{

    void SetMapMode() override;
    void SetMapMode(MapMode const& rNewMapMode) override;
    virtual void SetMetafileMapMode(const MapMode& rNewMapMode, bool bIsRecord);
    ///@}

    /** @name EPS functions
     */
    ///@{

    bool DrawEPS(const Point& rPt, const Size& rSz, const GfxLink& rGfxLink,
                 GDIMetaFile* pSubst = nullptr) override;
    ///@}

protected:
    OutputDevice(OutDevType eOutDevType);
    virtual ~OutputDevice() override;
    virtual void dispose() override;

    void DrawHatchLine(Point const& rStartPoint, Point const& rEndPoint) override;
    void DrawHatchLines(tools::Line const& rLine, tools::PolyPolygon const& rPolyPoly,
                        Point* pPtBuffer) override;

    void DrawUntransformedBitmapEx(BitmapEx const& rBitmapEx, basegfx::B2DVector const& rTranslate,
                                   basegfx::B2DVector const& rScale) override;

    void DrawScaledBitmap(Point const& rDestPt, Size const& rDestSize, const Point& rSrcPtPixel,
                          Size const& rSrcSizePixel, Bitmap const& rBitmap) override;

    virtual void ClipAndDrawGradientMetafile(const Gradient& rGradient,
                                             const tools::PolyPolygon& rPolyPoly);

    virtual tools::Rectangle GetBackgroundComponentBounds() const;

private:
    OutputDevice(const OutputDevice&) = delete;
    OutputDevice& operator=(const OutputDevice&) = delete;

    void DrawBitmapEx2(Point const& rDestPt, Size const& rDestSize, Point const& rSrcPtPixel,
                       Size const& rSrcSizePixel, BitmapEx const& rBitmapEx);

    bool TryDirectBitmapExPaint() const override;

    SAL_DLLPRIVATE void DrawLinearGradientToMetafile(const tools::Rectangle& rRect,
                                                     const Gradient& rGradient);
    SAL_DLLPRIVATE void DrawComplexGradientToMetafile(const tools::Rectangle& rRect,
                                                      const Gradient& rGradient);

    mutable VclPtr<OutputDevice> mpPrevGraphics; ///< Previous output device in list
    mutable VclPtr<OutputDevice> mpNextGraphics; ///< Next output device in list
    GDIMetaFile* mpMetaFile;
    std::vector<VCLXGraphics*>* mpUnoGraphicsList;

    const OutDevType meOutDevType;
    OutDevViewType meOutDevViewType;
};

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
