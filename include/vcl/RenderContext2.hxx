/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
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

#include <rtl/ref.hxx>
#include <tools/solar.h>
#include <tools/color.hxx>
#include <tools/fontenum.hxx>
#include <tools/mapunit.hxx>
#include <tools/poly.hxx>
#include <unotools/fontdefs.hxx>
#include <basegfx/matrix/b2dhommatrix.hxx>
#include <basegfx/polygon/b2dpolypolygon.hxx>
#include <i18nlangtag/lang.h>

#include <vcl/dllapi.h>
#include <vcl/ImplLayoutArgs.hxx>
#include <vcl/ImplMapRes.hxx>
#include <vcl/RasterOp.hxx>
#include <vcl/bitmapex.hxx>
#include <vcl/devicecoordinate.hxx>
#include <vcl/flags/AddFontSubstituteFlags.hxx>
#include <vcl/flags/AntialiasingFlags.hxx>
#include <vcl/flags/ComplexTextLayoutFlags.hxx>
#include <vcl/flags/DrawImageFlags.hxx>
#include <vcl/flags/DrawGridFlags.hxx>
#include <vcl/flags/DrawModeFlags.hxx>
#include <vcl/flags/DrawTextFlags.hxx>
#include <vcl/flags/GetDefaultFontFlags.hxx>
#include <vcl/flags/InvertFlags.hxx>
#include <vcl/flags/PushFlags.hxx>
#include <vcl/font.hxx>
#include <vcl/lineinfo.hxx>
#include <vcl/mapmod.hxx>
#include <vcl/metric.hxx>
#include <vcl/region.hxx>
#include <vcl/salnativewidgets.hxx>
#include <vcl/textrectinfo.hxx>
#include <vcl/vclptr.hxx>
#include <vcl/vclreferencebase.hxx>
#include <vcl/wall.hxx>

#include <vector>

#include <com/sun/star/awt/DeviceInfo.hpp>

#define GRADIENT_DEFAULT_STEPCOUNT 0
#define HATCH_MAXPOINTS 1024

class AllSettings;
class Hatch;
class Image;
class ImplDeviceFontList;
class ImplDeviceFontSizeList;
class ImplFontCache;
class ImplMultiTextLineInfo;
class LogicalFontInstance;
class PhysicalFontCollection;
class SalGraphics;
class SalLayout;
class SalLayoutGlyphs;
class VirtualDevice;
struct ImplOutDevData;
struct OutDevState;
struct SalTwoRect;

namespace basegfx
{
class B2DPolyPolygon;
}

namespace tools
{
class Line;
class Rectangle;
class Polygon;
}

namespace vcl
{
class ITextLayout;
class TextLayoutCache;
struct FontCapabilities;

namespace font
{
struct Feature;
}
}

class VCL_DLLPUBLIC RenderContext2 : public virtual VclReferenceBase
{
    friend VirtualDevice;

public:
    RenderContext2();
    virtual ~RenderContext2();

    Color GetPixel(Point const& rPt) const;
    virtual void DrawPixel(Point const& rPt);
    virtual void DrawPixel(Point const& rPt, Color const& rColor);

    virtual void DrawLine(Point const& rStartPt, Point const& rEndPt);
    virtual void DrawLine(Point const& rStartPt, Point const& rEndPt, LineInfo const& rLineInfo);

    /** Helper for line geometry paint with support for graphic expansion (pattern and fat_to_area)
     */
    void DrawLine(basegfx::B2DPolyPolygon aLinePolyPolygon, LineInfo const& rInfo);

    virtual void DrawRect(tools::Rectangle const& rRect);
    virtual void DrawRect(tools::Rectangle const& rRect, sal_uLong nHorzRount,
                          sal_uLong nVertRound);

    virtual void DrawBorder(tools::Rectangle aBorderRect);

    virtual void DrawGrid(tools::Rectangle const& rRect, Size const& rDist, DrawGridFlags nFlags);

    virtual void DrawCheckered(Point const& rPos, Size const& rSize, sal_uInt32 nLen = 8,
                               Color aStart = COL_WHITE, Color aEnd = COL_BLACK);

    virtual void DrawEllipse(tools::Rectangle const& rRect);
    virtual void DrawArc(tools::Rectangle const& rRect, Point const& rStartPt, Point const& rEndPt);
    virtual void DrawPie(tools::Rectangle const& rRect, Point const& rStartPt, Point const& rEndPt);
    virtual void DrawChord(tools::Rectangle const& rRect, Point const& rStartPt,
                           Point const& rEndPt);

    void Invert(tools::Rectangle const& rRect, InvertFlags nFlags = InvertFlags::NONE);
    void Invert(tools::Polygon const& rPoly, InvertFlags nFlags = InvertFlags::NONE);

    virtual void DrawPolyLine(tools::Polygon const& rPoly);
    virtual void DrawPolyLine(const tools::Polygon& rPoly, const LineInfo& rLineInfo);
    virtual void DrawPolyLine(const basegfx::B2DPolygon&, double fLineWidth = 0.0,
                              basegfx::B2DLineJoin eLineJoin = basegfx::B2DLineJoin::Round,
                              css::drawing::LineCap eLineCap = css::drawing::LineCap_BUTT,
                              double fMiterMinimumAngle = basegfx::deg2rad(15.0));
    virtual void DrawPolyPolygon(tools::PolyPolygon const& rPolyPoly);
    virtual void DrawPolyPolygon(basegfx::B2DPolyPolygon const& rB2DPolyPoly);

    /** Render the given polygon

        The given polygon is stroked with the current LineColor, and
        filled with the current FillColor. If one of these colors are
        transparent, the corresponding stroke or fill stays
        invisible. Start and end point of the polygon are
        automatically connected.

        @see DrawPolyLine
     */
    virtual void DrawPolygon(tools::Polygon const& rPoly);

    virtual void DrawGradient(tools::Rectangle const& rRect, Gradient const& rGradient);
    virtual void DrawGradient(tools::PolyPolygon const& rPolyPoly, Gradient const& rGradient);

    virtual void DrawHatch(tools::PolyPolygon const& rPolyPoly, Hatch const& rHatch);

    virtual Bitmap GetBitmap(Point const& rSrcPt, Size const& rSize) const;
    virtual void DrawBitmap(Point const& rDestPt, Bitmap const& rBitmap);
    virtual void DrawBitmap(Point const& rDestPt, Size const& rDestSize, Bitmap const& rBitmap);
    virtual void DrawBitmap(Point const& rDestPt, Size const& rDestSize, Point const& rSrcPtPixel,
                            Size const& rSrcSizePixel, Bitmap const& rBitmap);

    virtual void DrawText(Point const& rStartPt, OUString const& rStr, sal_Int32 nIndex = 0,
                          sal_Int32 nLen = -1, std::vector<tools::Rectangle>* pVector = nullptr,
                          OUString* pDisplayText = nullptr,
                          SalLayoutGlyphs const* pLayoutCache = nullptr);

    virtual void DrawText(tools::Rectangle const& rRect, OUString const& rStr,
                          DrawTextFlags nStyle = DrawTextFlags::NONE,
                          std::vector<tools::Rectangle>* pVector = nullptr,
                          OUString* pDisplayText = nullptr,
                          vcl::ITextLayout* _pTextLayout = nullptr);

    virtual void DrawTextArray(Point const& rStartPt, OUString const& rStr,
                               tools::Long const* pDXAry, sal_Int32 nIndex = 0, sal_Int32 nLen = -1,
                               SalLayoutFlags flags = SalLayoutFlags::NONE,
                               SalLayoutGlyphs const* pLayoutCache = nullptr);

    virtual void DrawStretchText(Point const& rStartPt, sal_uLong nWidth, OUString const& rStr,
                                 sal_Int32 nIndex = 0, sal_Int32 nLen = -1);

    virtual void DrawTextLine(const Point& rPos, tools::Long nWidth, FontStrikeout eStrikeout,
                              FontLineStyle eUnderline, FontLineStyle eOverline,
                              bool bUnderlineAbove = false);

    void DrawWaveLine(Point const& rStartPos, Point const& rEndPos, tools::Long nLineWidth = 1);

    void DrawCtrlText(Point const& rPos, OUString const& rStr, sal_Int32 nIndex = 0,
                      sal_Int32 nLen = -1, DrawTextFlags nStyle = DrawTextFlags::Mnemonic,
                      std::vector<tools::Rectangle>* pVector = nullptr,
                      OUString* pDisplayText = nullptr, SalLayoutGlyphs const* pGlyphs = nullptr);

    SAL_DLLPRIVATE void BlendBitmap(SalTwoRect const& rPosAry, Bitmap const& rBmp);

    /** Query extended bitmap (with alpha channel, if available).
     */
    BitmapEx GetBitmapEx(Point const& rSrcPt, Size const& rSize) const;
    virtual void DrawBitmapEx(Point const& rDestPt, BitmapEx const& rBitmapEx);
    virtual void DrawBitmapEx(Point const& rDestPt, Size const& rDestSize,
                              BitmapEx const& rBitmapEx);
    virtual void DrawBitmapEx(Point const& rDestPt, Size const& rDestSize, Point const& rSrcPtPixel,
                              Size const& rSrcSizePixel, BitmapEx const& rBitmapEx);

    /** Return true if DrawTransformedBitmapEx() is fast.

        @since 7.2
    */
    virtual bool HasFastDrawTransformedBitmap() const;
    virtual void DrawTransformedBitmapEx(const basegfx::B2DHomMatrix& rTransformation,
                                         const BitmapEx& rBitmapEx, double fAlpha = 1.0);

    virtual void DrawMask(Point const& rDestPt, Bitmap const& rBitmap, Color const& rMaskColor);
    virtual void DrawMask(Point const& rDestPt, Size const& rDestSize, Bitmap const& rBitmap,
                          Color const& rMaskColor);
    virtual void DrawMask(Point const& rDestPt, Size const& rDestSize, Point const& rSrcPtPixel,
                          Size const& rSrcSizePixel, Bitmap const& rBitmap,
                          Color const& rMaskColor);

    virtual void DrawImage(Point const& rPos, Image const& rImage,
                           DrawImageFlags nStyle = DrawImageFlags::NONE);
    virtual void DrawImage(Point const& rPos, Size const& rSize, Image const& rImage,
                           DrawImageFlags nStyle = DrawImageFlags::NONE);

    virtual bool CanAnimate();

    /** Determine if native widgets can be enabled
     */
    virtual bool CanEnableNativeWidget() const { return false; }

    /** Query the platform layer for control support
     */
    bool IsNativeControlSupported(ControlType nType, ControlPart nPart) const;

    /** Query the native control to determine if it was acted upon
     */
    bool HitTestNativeScrollbar(ControlPart nPart, tools::Rectangle const& rControlRegion,
                                Point const& aPos, bool& rIsInside) const;

    /** Request rendering of a particular control and/or part
     */
    bool DrawNativeControl(ControlType nType, ControlPart nPart,
                           tools::Rectangle const& rControlRegion, ControlState nState,
                           ImplControlValue const& aValue, OUString const& aCaption,
                           Color const& rBackgroundColor = COL_AUTO);

    /** Query the native control's actual drawing region (including adornment)
     */
    bool GetNativeControlRegion(ControlType nType, ControlPart nPart,
                                tools::Rectangle const& rControlRegion, ControlState nState,
                                ImplControlValue const& aValue,
                                tools::Rectangle& rNativeBoundingRegion,
                                tools::Rectangle& rNativeContentRegion) const;

    /** Get the graphic context that the output device uses to draw on.

     If no graphics device exists, then initialize it.

     @returns SalGraphics instance.
     */
    SalGraphics const* GetGraphics() const;
    SalGraphics* GetGraphics();

    virtual void Push(PushFlags nFlags = PushFlags::ALL);
    virtual void Pop();
    void ClearStack();

    bool IsClipRegion() const;
    vcl::Region GetClipRegion() const;
    virtual vcl::Region GetActiveClipRegion() const;
    virtual vcl::Region GetOutputBoundsClipRegion() const;
    virtual void SetClipRegion();
    virtual void SetClipRegion(vcl::Region const& rRegion);
    bool SelectClipRegion(vcl::Region const&, SalGraphics* pGraphics = nullptr);
    virtual void MoveClipRegion(tools::Long nHorzMove, tools::Long nVertMove);
    virtual void IntersectClipRegion(const tools::Rectangle& rRect);
    virtual void IntersectClipRegion(const vcl::Region& rRegion);

    void EnableOutput(bool bEnable = true);
    bool IsOutputEnabled() const;
    bool IsDeviceOutputNecessary() const;

    SAL_DLLPRIVATE void ReMirror(Point& rPoint) const;
    SAL_DLLPRIVATE void ReMirror(tools::Rectangle& rRect) const;
    SAL_DLLPRIVATE void ReMirror(vcl::Region& rRegion) const;
    virtual bool HasMirroredGraphics() const;

    virtual bool IsScreenComp() const;
    virtual sal_uInt16 GetBitCount() const;
    virtual bool IsVirtual() const;

    SAL_DLLPRIVATE bool ImplIsRecordLayout() const;

    /// tells whether this output device is RTL in an LTR UI or LTR in a RTL UI
    SAL_DLLPRIVATE bool ImplIsAntiparallel() const;
    /// Enabling/disabling RTL only makes sense for OutputDevices that use a mirroring SalGraphicsLayout
    virtual void EnableRTL(bool bEnable = true);
    bool IsRTLEnabled() const;

    ComplexTextLayoutFlags GetLayoutMode() const;
    virtual void SetLayoutMode(ComplexTextLayoutFlags nTextLayoutMode);

    DrawModeFlags GetDrawMode() const;
    void SetDrawMode(DrawModeFlags nDrawMode);

    AntialiasingFlags GetAntialiasing() const;
    void SetAntialiasing(AntialiasingFlags nMode);

    LanguageType GetDigitLanguage() const;
    virtual void SetDigitLanguage(LanguageType);

    Color const& GetLineColor() const;
    bool IsLineColor() const;
    virtual void SetLineColor();
    virtual void SetLineColor(Color const& rColor);

    Color const& GetFillColor() const;
    bool IsFillColor() const;
    virtual void SetFillColor();
    virtual void SetFillColor(Color const& rColor);

    bool IsFontAvailable(OUString const& rFontName) const;
    vcl::Font const& GetFont() const;
    static vcl::Font GetDefaultFont(DefaultFontType nType, LanguageType eLang,
                                    GetDefaultFontFlags nFlags,
                                    RenderContext2 const* pOutDev = nullptr);
    virtual void SetFont(vcl::Font const& rNewFont);

    FontMetric GetDevFont(int nDevFontIndex) const;
    int GetDevFontCount() const;
    bool AddTempDevFont(OUString const& rFileURL, OUString const& rFontName);
    Size GetDevFontSize(vcl::Font const& rFont, int nSizeIndex) const;
    int GetDevFontSizeCount(vcl::Font const&) const;

    void RefreshFontData(const bool bNewFontLists);

    FontMetric GetFontMetric() const;
    FontMetric GetFontMetric(const vcl::Font& rFont) const;

    bool GetFontCharMap(FontCharMapRef& rxFontCharMap) const;
    bool GetFontCapabilities(vcl::FontCapabilities& rFontCapabilities) const;

    bool GetFontFeatures(std::vector<vcl::font::Feature>& rFontFeatures) const;

    bool GetGlyphBoundRects(const Point& rOrigin, const OUString& rStr, int nIndex, int nLen,
                            std::vector<tools::Rectangle>& rVector);

    sal_Int32 HasGlyphs(const vcl::Font& rFont, const OUString& rStr, sal_Int32 nIndex = 0,
                        sal_Int32 nLen = -1) const;

    tools::Long GetMinKashida() const;

    // i60594
    // validate kashida positions against the current font
    // returns count of invalid kashida positions
    sal_Int32 ValidateKashidas(const OUString& rTxt, sal_Int32 nIdx, sal_Int32 nLen,
                               sal_Int32 nKashCount, // number of suggested kashida positions (in)
                               const sal_Int32* pKashidaPos, // suggested kashida positions (in)
                               sal_Int32* pKashidaPosDropped // invalid kashida positions (out)
                               ) const;

    static void BeginFontSubstitution();
    static void EndFontSubstitution();
    static void AddFontSubstitute(const OUString& rFontName, const OUString& rReplaceFontName,
                                  AddFontSubstituteFlags nFlags);
    static void RemoveFontsSubstitute();

    Color const& GetTextColor() const;
    virtual void SetTextColor(Color const& rColor);

    bool IsTextLineColor() const;
    Color const& GetTextLineColor() const;
    virtual void SetTextLineColor();
    virtual void SetTextLineColor(Color const& rColor);

    bool IsTextFillColor() const;
    Color GetTextFillColor() const;
    virtual void SetTextFillColor();
    virtual void SetTextFillColor(Color const& rColor);

    bool IsOverlineColor() const;
    Color const& GetOverlineColor() const;
    virtual void SetOverlineColor();
    virtual void SetOverlineColor(Color const& rColor);

    TextAlign GetTextAlign() const;
    virtual void SetTextAlign(TextAlign eAlign);

    sal_Int32 GetTextBreak(OUString const& rStr, tools::Long nTextWidth, sal_Int32 nIndex,
                           sal_Int32 nLen = -1, tools::Long nCharExtra = 0,
                           vcl::TextLayoutCache const* = nullptr,
                           SalLayoutGlyphs const* pGlyphs = nullptr) const;
    sal_Int32 GetTextBreak(OUString const& rStr, tools::Long nTextWidth, sal_Unicode nExtraChar,
                           sal_Int32& rExtraCharPos, sal_Int32 nIndex, sal_Int32 nLen,
                           tools::Long nCharExtra, vcl::TextLayoutCache const* = nullptr) const;

    tools::Rectangle GetTextRect(tools::Rectangle const& rRect, const OUString& rStr,
                                 DrawTextFlags nStyle = DrawTextFlags::WordBreak,
                                 TextRectInfo* pInfo = nullptr,
                                 vcl::ITextLayout const* _pTextLayout = nullptr) const;

    bool GetTextIsRTL(OUString const&, sal_Int32 nIndex, sal_Int32 nLen) const;

    void GetCaretPositions(OUString const&, tools::Long* pCaretXArray, sal_Int32 nIndex,
                           sal_Int32 nLen, SalLayoutGlyphs const* pGlyphs = nullptr) const;

    OUString GetEllipsisString(OUString const& rStr, tools::Long nMaxWidth,
                               DrawTextFlags nStyle = DrawTextFlags::EndEllipsis) const;

    static OUString GetNonMnemonicString(const OUString& rStr, sal_Int32& rMnemonicPos);
    static OUString GetNonMnemonicString(const OUString& rStr);

    tools::Long GetCtrlTextWidth(OUString const& rStr,
                                 SalLayoutGlyphs const* pLayoutCache = nullptr) const;

    bool GetTextOutlines(PolyPolyVector&, OUString const& rStr, sal_Int32 nBase = 0,
                         sal_Int32 nIndex = 0, sal_Int32 nLen = -1, sal_uLong nLayoutWidth = 0,
                         tools::Long const* pDXArray = nullptr) const;
    bool GetTextOutlines(basegfx::B2DPolyPolygonVector& rVector, OUString const& rStr,
                         sal_Int32 nBase, sal_Int32 nIndex = 0, sal_Int32 nLen = -1,
                         sal_uLong nLayoutWidth = 0, tools::Long const* pDXArray = nullptr) const;
    bool GetTextOutline(tools::PolyPolygon&, OUString const& rStr) const;

    virtual void SetSettings(AllSettings const& rSettings);
    AllSettings const& GetSettings() const;

    bool IsBackground() const;
    Wallpaper const& GetBackground() const;
    virtual void SetBackground();
    virtual void SetBackground(Wallpaper const& rBackground);

    bool IsRefPoint() const;
    Point const& GetRefPoint() const;
    virtual void SetRefPoint();
    virtual void SetRefPoint(Point const& rRefPoint);

    RasterOp GetRasterOp() const;
    virtual void SetRasterOp(RasterOp eRasterOp);

    void EnableMapMode(bool bEnable = true);
    bool IsMapModeEnabled() const;
    MapMode const& GetMapMode() const;
    virtual void SetMapMode();
    virtual void SetMapMode(MapMode const& rNewMapMode);
    virtual void SetRelativeMapMode(MapMode const& rNewMapMode);

    /** Get the output device's DPI x-axis value.

     @returns x-axis DPI value
     */
    sal_Int32 GetDPIX() const;

    /** Get the output device's DPI y-axis value.

     @returns y-axis DPI value
     */
    sal_Int32 GetDPIY() const;

    SAL_DLLPRIVATE void SetDPIX(sal_Int32 nDPIX);
    SAL_DLLPRIVATE void SetDPIY(sal_Int32 nDPIY);
    float GetDPIScaleFactor() const;
    sal_Int32 GetDPIScalePercentage() const;
    void SetDPIScalePercentage(sal_Int32 nPercent);

    Size GetOutputSizePixel() const;
    tools::Long GetOutputWidthPixel() const;
    tools::Long GetOutputHeightPixel() const;
    tools::Long GetOutOffXPixel() const;
    tools::Long GetOutOffYPixel() const;
    void SetOutOffXPixel(tools::Long nOutOffX);
    void SetOutOffYPixel(tools::Long nOutOffY);
    Point GetOutputOffPixel() const;
    tools::Rectangle GetOutputRectPixel() const;
    Size GetOutputSize() const;

    tools::Long GetTextArray(OUString const& rStr, tools::Long* pDXAry, sal_Int32 nIndex = 0,
                             sal_Int32 nLen = -1, vcl::TextLayoutCache const* = nullptr,
                             SalLayoutGlyphs const* const pLayoutCache = nullptr) const;

    /** Return the exact bounding rectangle of rStr.

        The text is then drawn exactly from rRect.TopLeft() to
        rRect.BottomRight(), don't assume that rRect.TopLeft() is [0, 0].

        Please note that you don't always want to use GetTextBoundRect(); in
        many cases you actually want to use GetTextHeight(), because
        GetTextBoundRect() gives you the exact bounding rectangle regardless
        what is the baseline of the text.

        Code snippet to get just exactly the text (no filling around that) as
        a bitmap via a VirtualDevice (regardless what is the baseline):

        <code>
        VirtualDevice aDevice;
        vcl::Font aFont = aDevice.GetFont();
        aFont.SetSize(Size(0, 96));
        aFont.SetColor(COL_BLACK);
        aDevice.SetFont(aFont);
        aDevice.Erase();

        tools::Rectangle aRect;
        aDevice.GetTextBoundRect(aRect, aText);
        aDevice.SetOutputSize(Size(aRect.Right() + 1, aRect.Bottom() + 1));
        aDevice.SetBackground(Wallpaper(COL_TRANSPARENT));
        aDevice.DrawText(Point(0,0), aText);

        // exactly only the text, regardless of the baseline
        Bitmap aBitmap(aDevice.GetBitmap(aRect.TopLeft(), aRect.GetSize()));
        </code>

        Code snippet to get the text as a bitmap via a Virtual device that
        contains even the filling so that the baseline is always preserved
        (ie. the text will not jump up and down according to whether it
        contains 'y' or not etc.)

        <code>
        VirtualDevice aDevice;
        // + the appropriate font / device setup, see above

        aDevice.SetOutputSize(Size(aDevice.GetTextWidth(aText), aDevice.GetTextHeight()));
        aDevice.SetBackground(Wallpaper(COL_TRANSPARENT));
        aDevice.DrawText(Point(0,0), aText);

        // bitmap that contains even the space around the text,
        // that means, preserves the baseline etc.
        Bitmap aBitmap(aDevice.GetBitmap(Point(0, 0), aDevice.GetOutputSize()));
        </code>
    */
    bool GetTextBoundRect(tools::Rectangle& rRect, OUString const& rStr, sal_Int32 nBase = 0,
                          sal_Int32 nIndex = 0, sal_Int32 nLen = -1, sal_uLong nLayoutWidth = 0,
                          tools::Long const* pDXArray = nullptr,
                          SalLayoutGlyphs const* pGlyphs = nullptr) const;

    /** Height where any character of the current font fits; in logic coordinates.

        See also GetTextBoundRect() for more explanation + code examples.
    */
    tools::Long GetTextHeight() const;
    float approximate_digit_width() const;

    /** Width of the text.

        See also GetTextBoundRect() for more explanation + code examples.
    */
    tools::Long GetTextWidth(OUString const& rStr, sal_Int32 nIndex = 0, sal_Int32 nLen = -1,
                             vcl::TextLayoutCache const* = nullptr,
                             SalLayoutGlyphs const* const pLayoutCache = nullptr) const;

    std::unique_ptr<SalLayout> ImplLayout(OUString const&, sal_Int32 nIndex, sal_Int32 nLen,
                                          Point const& rLogicPos = Point(0, 0),
                                          tools::Long nLogicWidth = 0,
                                          tools::Long const* pLogicDXArray = nullptr,
                                          SalLayoutFlags flags = SalLayoutFlags::NONE,
                                          vcl::TextLayoutCache const* = nullptr,
                                          SalLayoutGlyphs const* pGlyphs = nullptr) const;

    tools::Rectangle ImplGetTextBoundRect(SalLayout const&);

    static std::shared_ptr<vcl::TextLayoutCache> CreateTextLayoutCache(OUString const&);

    SAL_DLLPRIVATE void ImplUpdateFontData();

    //drop font data for all outputdevices.
    //If bNewFontLists is true then empty lists of system fonts
    SAL_DLLPRIVATE static void ImplClearAllFontData(bool bNewFontLists);
    //fetch font data for all outputdevices
    //If bNewFontLists is true then fetch lists of system fonts
    SAL_DLLPRIVATE static void ImplRefreshAllFontData(bool bNewFontLists);
    //drop and fetch font data for all outputdevices
    //If bNewFontLists is true then drop and refetch lists of system fonts
    SAL_DLLPRIVATE static void ImplUpdateAllFontData(bool bNewFontLists);

    virtual css::awt::DeviceInfo GetDeviceInfo() const;

    /** Get the offset in pixel

        @see RenderContext2::SetPixelOffset for details

        @return the current offset in pixel
     */
    Size GetPixelOffset() const;

    /** Set an offset in pixel

        This method offsets every drawing operation that converts its
        coordinates to pixel by the given value. Normally, the effect
        can be achieved by setting a MapMode with a different
        origin. Unfortunately, this origin is in logical coordinates
        and can lead to rounding errors (see #102532# for details).

        @attention This offset is only applied when converting to
        pixel, i.e. some output modes such as metafile recordings
        might be completely unaffected by this method! Use with
        care. Furthermore, if the RenderContext2's MapMode is the
        default (that's MapUnit::MapPixel), then any pixel offset set is
        ignored also. This might be unintuitive for cases, but would
        have been far more fragile to implement. What's more, the
        reason why the pixel offset was introduced (avoiding rounding
        errors) does not apply for MapUnit::MapPixel, because one can always
        use the MapMode origin then.

        @param rOffset
        The offset in pixel
     */
    void SetPixelOffset(const Size& rOffset);

    // #i75163#
    basegfx::B2DHomMatrix GetViewTransformation() const;
    basegfx::B2DHomMatrix GetInverseViewTransformation() const;

    basegfx::B2DHomMatrix GetViewTransformation(MapMode const& rMapMode) const;
    basegfx::B2DHomMatrix GetInverseViewTransformation(MapMode const& rMapMode) const;

    Point LogicToPixel(const Point& rLogicPt) const;

    Size LogicToPixel(const Size& rLogicSize) const;

    tools::Rectangle LogicToPixel(const tools::Rectangle& rLogicRect) const;

    tools::Polygon LogicToPixel(const tools::Polygon& rLogicPoly) const;

    tools::PolyPolygon LogicToPixel(const tools::PolyPolygon& rLogicPolyPoly) const;

    basegfx::B2DPolyPolygon LogicToPixel(const basegfx::B2DPolyPolygon& rLogicPolyPoly) const;

    vcl::Region LogicToPixel(const vcl::Region& rLogicRegion) const;

    Point LogicToPixel(const Point& rLogicPt, const MapMode& rMapMode) const;

    Size LogicToPixel(const Size& rLogicSize, const MapMode& rMapMode) const;

    tools::Rectangle LogicToPixel(const tools::Rectangle& rLogicRect,
                                  const MapMode& rMapMode) const;

    tools::Polygon LogicToPixel(const tools::Polygon& rLogicPoly, const MapMode& rMapMode) const;

    basegfx::B2DPolyPolygon LogicToPixel(const basegfx::B2DPolyPolygon& rLogicPolyPoly,
                                         const MapMode& rMapMode) const;

    Point PixelToLogic(const Point& rDevicePt) const;

    Size PixelToLogic(const Size& rDeviceSize) const;

    tools::Rectangle PixelToLogic(const tools::Rectangle& rDeviceRect) const;

    tools::Polygon PixelToLogic(const tools::Polygon& rDevicePoly) const;

    tools::PolyPolygon PixelToLogic(const tools::PolyPolygon& rDevicePolyPoly) const;

    basegfx::B2DPolyPolygon PixelToLogic(const basegfx::B2DPolyPolygon& rDevicePolyPoly) const;

    vcl::Region PixelToLogic(const vcl::Region& rDeviceRegion) const;

    Point PixelToLogic(const Point& rDevicePt, const MapMode& rMapMode) const;

    Size PixelToLogic(const Size& rDeviceSize, const MapMode& rMapMode) const;

    tools::Rectangle PixelToLogic(const tools::Rectangle& rDeviceRect,
                                  const MapMode& rMapMode) const;

    tools::Polygon PixelToLogic(const tools::Polygon& rDevicePoly, const MapMode& rMapMode) const;

    basegfx::B2DPolygon PixelToLogic(const basegfx::B2DPolygon& rDevicePoly,
                                     const MapMode& rMapMode) const;

    basegfx::B2DPolyPolygon PixelToLogic(const basegfx::B2DPolyPolygon& rDevicePolyPoly,
                                         const MapMode& rMapMode) const;

    Point LogicToLogic(const Point& rPtSource, const MapMode* pMapModeSource,
                       const MapMode* pMapModeDest) const;

    Size LogicToLogic(const Size& rSzSource, const MapMode* pMapModeSource,
                      const MapMode* pMapModeDest) const;

    tools::Rectangle LogicToLogic(const tools::Rectangle& rRectSource,
                                  const MapMode* pMapModeSource, const MapMode* pMapModeDest) const;

    static Point LogicToLogic(const Point& rPtSource, const MapMode& rMapModeSource,
                              const MapMode& rMapModeDest);

    static Size LogicToLogic(const Size& rSzSource, const MapMode& rMapModeSource,
                             const MapMode& rMapModeDest);

    static tools::Rectangle LogicToLogic(const tools::Rectangle& rRectSource,
                                         const MapMode& rMapModeSource,
                                         const MapMode& rMapModeDest);

    static tools::Long LogicToLogic(tools::Long nLongSource, MapUnit eUnitSource,
                                    MapUnit eUnitDest);

    static basegfx::B2DPolygon LogicToLogic(const basegfx::B2DPolygon& rPoly,
                                            const MapMode& rMapModeSource,
                                            const MapMode& rMapModeDest);

    // create a mapping transformation from rMapModeSource to rMapModeDest (the above methods
    // for B2DPoly/Polygons use this internally anyway to transform the B2DPolygon)
    static basegfx::B2DHomMatrix LogicToLogic(const MapMode& rMapModeSource,
                                              const MapMode& rMapModeDest);

    /** Convert a logical rectangle to a rectangle in physical device pixel units.

     @param         rLogicRect  Const reference to a rectangle in logical units

     @returns Rectangle based on physical device pixel coordinates and units.
     */
    SAL_DLLPRIVATE tools::Rectangle
    ImplLogicToDevicePixel(const tools::Rectangle& rLogicRect) const;

    /** Convert a logical point to a physical point on the device.

     @param         rLogicPt    Const reference to a point in logical units.

     @returns Physical point on the device.
     */
    SAL_DLLPRIVATE Point ImplLogicToDevicePixel(const Point& rLogicPt) const;

    /** Convert a logical width to a width in units of device pixels.

     To get the number of device pixels, it must calculate the X-DPI of the device and
     the map scaling factor. If there is no mapping, then it just returns the
     width as nothing more needs to be done.

     @param         nWidth      Logical width

     @returns Width in units of device pixels.
     */
    SAL_DLLPRIVATE tools::Long ImplLogicWidthToDevicePixel(tools::Long nWidth) const;

protected:
    virtual void dispose();

    /** Acquire a graphics device that the output device uses to draw on.

     There is an LRU of RenderContext2s that is used to get the graphics. The
     actual creation of a SalGraphics instance is done via the SalFrame
     implementation.

     However, the SalFrame instance will only return a valid SalGraphics
     instance if it is not in use or there wasn't one in the first place. When
     this happens, AcquireGraphics finds the least recently used RenderContext2
     in a different frame and "steals" it (releases it then starts using it).

     If there are no frames to steal an RenderContext2's SalGraphics instance from
     then it blocks until the graphics is released.

     Once it has acquired a graphics instance, then we add the RenderContext2 to
     the LRU.

     @returns true if was able to initialize the graphics device, false otherwise.
     */
    virtual bool AcquireGraphics() const = 0;

    /** Release the graphics device, and remove it from the graphics device
     list.

     @param         bRelease    Determines whether to release the fonts of the
                                physically released graphics device.
     */
    virtual void ReleaseGraphics(bool bRelease = true) = 0;

    void ImplDrawText(SalLayout&);

    static void ImplDrawText(RenderContext2& rTargetDevice, tools::Rectangle const& rRect,
                             OUString const& rOrigStr, DrawTextFlags nStyle,
                             std::vector<tools::Rectangle>* pVector, OUString* pDisplayText,
                             vcl::ITextLayout& _rLayout);

    SAL_DLLPRIVATE void ImplDrawTextRect(tools::Long nBaseX, tools::Long nBaseY, tools::Long nX,
                                         tools::Long nY, tools::Long nWidth, tools::Long nHeight);

    virtual void DrawDeviceBitmapEx(Point const& rDestPt, Size const& rDestSize,
                                    Point const& rSrcPtPixel, Size const& rSrcSizePixel,
                                    BitmapEx& rBitmapEx);

    // make private
    SAL_DLLPRIVATE void DrawDeviceAlphaBitmap(Bitmap const& rBmp, AlphaMask const& rAlpha,
                                              Point const& rDestPt, Size const& rDestSize,
                                              Point const& rSrcPtPixel, Size const& rSrcSizePixel);

    /** Transform and draw a bitmap directly

     @param     rFullTransform      The B2DHomMatrix used for the transformation
     @param     rBitmapEx           Reference to the bitmap to be transformed and drawn

     @return true if it was able to draw the bitmap, false if not
     */
    virtual bool DrawTransformBitmapExDirect(basegfx::B2DHomMatrix const& rFullTransform,
                                             BitmapEx const& rBitmapEx, double fAlpha = 1.0);

    virtual void DrawDeviceMask(Bitmap const& rMask, Color const& rMaskColor, Point const& rDestPt,
                                Size const& rDestSize, Point const& rSrcPtPixel,
                                Size const& rSrcSizePixel);

    SAL_DLLPRIVATE void ImplDrawPolygon(tools::Polygon const& rPoly,
                                        tools::PolyPolygon const* pClipPolyPoly = nullptr);

    virtual bool DrawPolyLineDirect(basegfx::B2DHomMatrix const& rObjectTransform,
                                    basegfx::B2DPolygon const& rB2DPolygon, double fLineWidth = 0.0,
                                    double fTransparency = 0.0,
                                    std::vector<double> const* = nullptr, // MM01
                                    basegfx::B2DLineJoin eLineJoin = basegfx::B2DLineJoin::NONE,
                                    css::drawing::LineCap eLineCap = css::drawing::LineCap_BUTT,
                                    double fMiterMinimumAngle = basegfx::deg2rad(15.0));

    SAL_DLLPRIVATE void ImplDrawPolyPolygon(tools::PolyPolygon const& rPolyPoly,
                                            tools::PolyPolygon const* pClipPolyPoly);

    SAL_DLLPRIVATE void ImplDrawPolyPolygon(sal_uInt16 nPoly, tools::PolyPolygon const& rPolyPoly);

    virtual void DrawHatchLine(Point const& rStartPoint, Point const& rEndPoint);
    virtual void DrawHatchLines(tools::Line const& rLine, tools::PolyPolygon const& rPolyPoly,
                                Point* pPtBuffer);

    void CalcHatchValues(tools::Rectangle const& rRect, tools::Long nDist, Degree10 nAngle10,
                         Point& rPt1, Point& rPt2, Size& rInc, Point& rEndPt1);

    std::tuple<tools::Long, Point*> GenerateHatchPointBuffer(tools::Line const& rLine,
                                                             tools::PolyPolygon const& rPolyPoly,
                                                             Point* pPtBuffer);

    virtual void InitWaveLineColor(Color const& rColor, tools::Long);
    virtual std::tuple<bool, Size> GetWaveLineSize(tools::Long nLineWidth) const;

    virtual void DrawScaledBitmap(Point const& rDestPt, Size const& rDestSize,
                                  const Point& rSrcPtPixel, Size const& rSrcSizePixel,
                                  Bitmap const& rBitmap);

    virtual bool CanSubsampleBitmap() const { return true; }
    bool ProcessBitmapRasterOpInvert(Point const& rDestPt, Size const& rDestSize);
    bool ProcessBitmapDrawModeBlackWhite(Point const& rDestPt, Size const& rDestSize);
    Bitmap ProcessBitmapDrawModeGray(Bitmap const& rBitmap);

    virtual bool TryDirectBitmapExPaint() const;

    virtual void DrawUntransformedBitmapEx(BitmapEx const& rBitmapEx,
                                           basegfx::B2DVector const& rTranslate,
                                           basegfx::B2DVector const& rScale);

    std::tuple<Point, Size, BitmapEx>
    CreateTransformedBitmapFallback(BitmapEx const& rBitmapEx, Size const& rOriginalSizePixel,
                                    basegfx::B2DHomMatrix const& rTransformation,
                                    basegfx::B2DRange aVisibleRange, double fMaximumArea);

    void DrawMirroredBitmapEx(BitmapEx const& rBitmapEx, basegfx::B2DVector const& rTranslate,
                              basegfx::B2DVector const& rScale);

    bool DrawTransformedAlphaBitmapExDirect(basegfx::B2DHomMatrix const& rFullTransform,
                                            BitmapEx const& rBitmapEx, float fAlpha);
    BitmapEx ApplyAlphaBitmapEx(BitmapEx const& rBitmapEx, float fAlpha) const;

    virtual basegfx::B2DRange
    ReduceBitmapExVisibleRange(basegfx::B2DHomMatrix const& rFullTransform,
                               basegfx::B2DRange const& rVisibleRange);

    double GetMaximumBitmapExArea(basegfx::B2DRange const& rVisiblePixelRange);

    /** Perform actual rect clip against outdev dimensions, to generate
        empty clips whenever one of the values is completely off the device.

        @param aRegion      region to be clipped to the device dimensions
        @returns            region clipped to the device bounds
     **/
    virtual vcl::Region ClipToDeviceBounds(vcl::Region aRegion) const;
    virtual void ClipToPaintRegion(tools::Rectangle& rDstRect);
    SAL_DLLPRIVATE void SetDeviceClipRegion(vcl::Region const* pRegion);
    virtual void InitClipRegion();

    SAL_DLLPRIVATE void InitLineColor();
    SAL_DLLPRIVATE void InitFillColor();
    SAL_DLLPRIVATE void ImplInitTextColor();

    virtual void ImplInitMapModeObjects();

    css::awt::DeviceInfo GetCommonDeviceInfo(Size const& aDevSize) const;

    virtual tools::Rectangle GetBackgroundComponentBounds() const;

    /** Get device transformation.

     @since AOO bug 75163 (OpenOffice.org 2.4.3 - OOH 680 milestone 212)
     */
    SAL_DLLPRIVATE basegfx::B2DHomMatrix ImplGetDeviceTransformation() const;

    SAL_DLLPRIVATE DeviceCoordinate LogicWidthToDeviceCoordinate(tools::Long nWidth) const;

    /** Convert a logical X coordinate to a device pixel's X coordinate.

     To get the device's X coordinate, it must calculate the mapping offset
     coordinate X position (if there is one - if not then it just adds
     the pseudo-window offset to the logical X coordinate), the X-DPI of
     the device and the mapping's X scaling factor.

     @param         nX          Logical X coordinate

     @returns Device's X pixel coordinate
     */
    SAL_DLLPRIVATE tools::Long ImplLogicXToDevicePixel(tools::Long nX) const;

    /** Convert a logical Y coordinate to a device pixel's Y coordinate.

     To get the device's Y coordinate, it must calculate the mapping offset
     coordinate Y position (if there is one - if not then it just adds
     the pseudo-window offset to the logical Y coordinate), the Y-DPI of
     the device and the mapping's Y scaling factor.

     @param         nY          Logical Y coordinate

     @returns Device's Y pixel coordinate
     */
    SAL_DLLPRIVATE tools::Long ImplLogicYToDevicePixel(tools::Long nY) const;

    /** Convert a logical height to a height in units of device pixels.

     To get the number of device pixels, it must calculate the Y-DPI of the device and
     the map scaling factor. If there is no mapping, then it just returns the
     height as nothing more needs to be done.

     @param         nHeight     Logical height

     @returns Height in units of device pixels.
     */
    SAL_DLLPRIVATE tools::Long ImplLogicHeightToDevicePixel(tools::Long nHeight) const;

    /** Convert device pixels to a width in logical units.

     To get the logical width, it must calculate the X-DPI of the device and the
     map scaling factor.

     @param         nWidth      Width in device pixels

     @returns Width in logical units.
     */
    SAL_DLLPRIVATE tools::Long ImplDevicePixelToLogicWidth(tools::Long nWidth) const;

    /** Convert device pixels to a height in logical units.

     To get the logical height, it must calculate the Y-DPI of the device and the
     map scaling factor.

     @param         nHeight     Height in device pixels

     @returns Height in logical units.
     */
    SAL_DLLPRIVATE tools::Long ImplDevicePixelToLogicHeight(tools::Long nHeight) const;

    /** Convert logical height to device pixels, with exact sub-pixel value.

     To get the \em exact pixel height, it must calculate the Y-DPI of the device and the
     map scaling factor.

     @param         fLogicHeight     Exact height in logical units.

     @returns Exact height in pixels - returns as a float to provide for subpixel value.
     */
    SAL_DLLPRIVATE float ImplFloatLogicHeightToDevicePixel(float fLogicHeight) const;

    /** Convert a logical size to the size on the physical device.

     @param         rLogicSize  Const reference to a size in logical units

     @returns Physical size on the device.
     */
    SAL_DLLPRIVATE Size ImplLogicToDevicePixel(const Size& rLogicSize) const;

    /** Convert a rectangle in physical pixel units to a rectangle in physical pixel units and coords.

     @param         rPixelRect  Const reference to rectangle in logical units and coords.

     @returns Rectangle based on logical coordinates and units.
     */
    SAL_DLLPRIVATE tools::Rectangle
    ImplDevicePixelToLogic(const tools::Rectangle& rPixelRect) const;

    /** Convert a logical polygon to a polygon in physical device pixel units.

     @param         rLogicPoly  Const reference to a polygon in logical units

     @returns Polygon based on physical device pixel coordinates and units.
     */
    SAL_DLLPRIVATE tools::Polygon ImplLogicToDevicePixel(const tools::Polygon& rLogicPoly) const;

    /** Convert a logical polypolygon to a polypolygon in physical device pixel units.

     @param         rLogicPolyPoly  Const reference to a polypolygon in logical units

     @returns Polypolygon based on physical device pixel coordinates and units.
     */
    SAL_DLLPRIVATE tools::PolyPolygon
    ImplLogicToDevicePixel(const tools::PolyPolygon& rLogicPolyPoly) const;

    /** Convert a line in logical units to a line in physical device pixel units.

     @param         rLineInfo   Const reference to a line in logical units

     @returns Line based on physical device pixel coordinates and units.
     */
    SAL_DLLPRIVATE LineInfo ImplLogicToDevicePixel(const LineInfo& rLineInfo) const;

    /** Convert a region in pixel units to a region in device pixel units and coords.

     @param         rRegion  Const reference to region.

     @returns vcl::Region based on device pixel coordinates and units.
     */
    SAL_DLLPRIVATE vcl::Region ImplPixelToDevicePixel(const vcl::Region& rRegion) const;

    /** Invalidate the view transformation.

     @since AOO bug 75163 (OpenOffice.org 2.4.3 - OOH 680 milestone 212)
     */
    SAL_DLLPRIVATE void ImplInvalidateViewTransform();

    SAL_DLLPRIVATE bool is_double_buffered_window() const;

    SAL_DLLPRIVATE bool InitFont() const;
    SAL_DLLPRIVATE bool ImplNewFont() const;
    SAL_DLLPRIVATE void ImplInitFontList() const;
    virtual void SetFontOrientation(LogicalFontInstance* const pFontInstance) const;

    SAL_DLLPRIVATE const LogicalFontInstance* GetFontInstance() const;
    SAL_DLLPRIVATE tools::Long GetEmphasisAscent() const { return mnEmphasisAscent; }
    SAL_DLLPRIVATE tools::Long GetEmphasisDescent() const { return mnEmphasisDescent; }

    virtual tools::Long GetFontExtLeading() const;

    virtual void ImplReleaseFonts();
    virtual void ImplClearFontData(bool bNewFontLists);
    virtual void ImplRefreshFontData(bool bNewFontLists);
    void ReleaseFontCache();
    void ReleaseFontCollection();
    void SetFontCollectionFromSVData();
    void ResetNewFontCache();

    SAL_DLLPRIVATE void ImplInitTextLineSize();
    SAL_DLLPRIVATE void ImplInitAboveTextLineSize();

    static SAL_DLLPRIVATE tools::Long ImplGetTextLines(ImplMultiTextLineInfo& rLineInfo,
                                                       tools::Long nWidth, OUString const& rStr,
                                                       DrawTextFlags nStyle,
                                                       vcl::ITextLayout const& _rLayout);

    static SAL_DLLPRIVATE OUString ImplGetEllipsisString(RenderContext2 const& rTargetDevice,
                                                         OUString const& rStr,
                                                         tools::Long nMaxWidth,
                                                         DrawTextFlags nStyle,
                                                         vcl::ITextLayout const& _rLayout);

    SAL_DLLPRIVATE void ImplGetEmphasisMark(tools::PolyPolygon& rPolyPoly, bool& rPolyLine,
                                            tools::Rectangle& rRect1, tools::Rectangle& rRect2,
                                            tools::Long& rYOff, tools::Long& rWidth,
                                            FontEmphasisMark eEmphasis, tools::Long nHeight);

    SAL_DLLPRIVATE static FontEmphasisMark ImplGetEmphasisMarkStyle(vcl::Font const& rFont);

    SAL_DLLPRIVATE void ImplDrawEmphasisMarks(SalLayout&);

    SAL_DLLPRIVATE float approximate_char_width() const;

    SAL_DLLPRIVATE ImplLayoutArgs ImplPrepareLayoutArgs(
        OUString&, const sal_Int32 nIndex, const sal_Int32 nLen, DeviceCoordinate nPixelWidth,
        const DeviceCoordinate* pPixelDXArray, SalLayoutFlags flags = SalLayoutFlags::NONE,
        vcl::TextLayoutCache const* = nullptr) const;

    SAL_DLLPRIVATE Color GetSingleColorGradientFill();

    SAL_DLLPRIVATE void DrawLinearGradient(tools::Rectangle const& rRect, Gradient const& rGradient,
                                           tools::PolyPolygon const* pClipPolyPoly);

    SAL_DLLPRIVATE void DrawComplexGradient(tools::Rectangle const& rRect,
                                            Gradient const& rGradient,
                                            tools::PolyPolygon const* pClipPolyPoly);

    virtual bool UsePolyPolygonForComplexGradient() = 0;

    SAL_DLLPRIVATE tools::Long GetGradientSteps(Gradient const& rGradient,
                                                tools::Rectangle const& rRect, bool bMtf,
                                                bool bComplex = false);

    virtual tools::Long GetGradientStepCount(tools::Long nMinRect);

    mutable SalGraphics* mpGraphics; ///< Graphics context to draw on
    VclPtr<VirtualDevice> mpAlphaVDev;

    ComplexTextLayoutFlags mnTextLayoutMode;
    DrawModeFlags mnDrawMode;
    AntialiasingFlags mnAntialiasing;
    RasterOp meRasterOp;
    std::unique_ptr<AllSettings> mxSettings;
    LanguageType meTextLanguage;
    Color maLineColor;
    Color maFillColor;
    vcl::Font maFont;
    Color maTextColor;
    Color maTextLineColor;
    Color maTextFillColor;
    Color maOverlineColor;
    Wallpaper maBackground;
    vcl::Region maRegion; ///< contains the clip region, see SetClipRegion(...)
    Point maRefPoint;
    MapMode maMapMode;
    ImplMapRes maMapRes;
    std::unique_ptr<ImplOutDevData> mpOutDevData;
    mutable std::shared_ptr<PhysicalFontCollection> mxFontCollection;
    mutable std::unique_ptr<ImplDeviceFontList> mpDeviceFontList;
    mutable std::unique_ptr<ImplDeviceFontSizeList> mpDeviceFontSizeList;
    mutable rtl::Reference<LogicalFontInstance> mpFontInstance;
    mutable std::shared_ptr<ImplFontCache> mxFontCache;
    /// Additional output pixel offset, applied in LogicToPixel (used by SetPixelOffset/GetPixelOffset)
    tools::Long mnOutOffOrigX;
    /// Additional output offset in _logical_ coordinates, applied in PixelToLogic (used by SetPixelOffset/GetPixelOffset)
    tools::Long mnOutOffLogicX;
    /// Additional output pixel offset, applied in LogicToPixel (used by SetPixelOffset/GetPixelOffset)
    tools::Long mnOutOffOrigY;
    /// Additional output offset in _logical_ coordinates, applied in PixelToLogic (used by SetPixelOffset/GetPixelOffset)
    tools::Long mnOutOffLogicY;
    /// Output offset for device output in pixel (pseudo window offset within window system's frames)
    tools::Long mnOutOffX;
    /// Output offset for device output in pixel (pseudo window offset within window system's frames)
    tools::Long mnOutOffY;
    tools::Long mnOutWidth;
    tools::Long mnOutHeight;
    /// font specific text alignment offsets in pixel units
    mutable tools::Long mnTextOffX;
    mutable tools::Long mnTextOffY;
    mutable tools::Long mnEmphasisAscent;
    mutable tools::Long mnEmphasisDescent;

    mutable bool mbMap : 1;
    mutable bool mbInitFont : 1;
    mutable bool mbInitFillColor : 1;
    mutable bool mbInitTextColor : 1;
    mutable bool mbLineColor : 1;
    mutable bool mbInitLineColor : 1;
    mutable bool mbFillColor : 1;
    mutable bool mbNewFont : 1;
    mutable bool mbRefPoint : 1;
    mutable bool mbBackground : 1;
    mutable bool mbInitClipRegion : 1;
    mutable bool mbClipRegion : 1;
    mutable bool mbClipRegionSet : 1;
    mutable bool mbOutputClipped : 1;
    mutable bool mbEnableRTL : 1;
    mutable bool mbDevOutput : 1;
    mutable bool mbTextLines : 1;
    mutable bool mbTextSpecial : 1;

private:
    SAL_DLLPRIVATE void DrawDeviceAlphaBitmapSlowPath(Bitmap const& rBitmap,
                                                      AlphaMask const& rAlpha,
                                                      tools::Rectangle aDstRect,
                                                      tools::Rectangle aBmpRect, Size const& aOutSz,
                                                      Point const& aOutPt);

    SAL_DLLPRIVATE Bitmap BlendBitmap(Bitmap& aBmp, BitmapReadAccess const* pP,
                                      BitmapReadAccess const* pA, const sal_Int32 nOffY,
                                      const sal_Int32 nDstHeight, const sal_Int32 nOffX,
                                      const sal_Int32 nDstWidth, tools::Rectangle const& aBmpRect,
                                      const Size& aOutSz, const bool bHMirr, const bool bVMirr,
                                      tools::Long const* pMapX, tools::Long const* pMapY);

    SAL_DLLPRIVATE Bitmap BlendBitmapWithAlpha(Bitmap& aBmp, BitmapReadAccess const* pP,
                                               BitmapReadAccess const* pA,
                                               tools::Rectangle const& aDstRect,
                                               const sal_Int32 nOffY, const sal_Int32 nDstHeight,
                                               const sal_Int32 nOffX, const sal_Int32 nDstWidth,
                                               tools::Long const* pMapX, tools::Long const* pMapY);

    void DrawBitmapEx2(Point const& rDestPt, Size const& rDestSize, Point const& rSrcPtPixel,
                       Size const& rSrcSizePixel, BitmapEx const& rBitmapEx);

    SAL_DLLPRIVATE std::unique_ptr<SalLayout> ImplGlyphFallbackLayout(std::unique_ptr<SalLayout>,
                                                                      ImplLayoutArgs&,
                                                                      const SalLayoutGlyphs*) const;

    SAL_DLLPRIVATE std::unique_ptr<SalLayout> getFallbackLayout(LogicalFontInstance* pLogicalFont,
                                                                int nFallbackLevel,
                                                                ImplLayoutArgs& rLayoutArgs,
                                                                const SalLayoutGlyphs*) const;

    typedef void (RenderContext2::*FontUpdateHandler_t)(bool);

    SAL_DLLPRIVATE static void ImplUpdateFontDataForAllFrames(FontUpdateHandler_t pHdl,
                                                              bool bNewFontLists);

    SAL_DLLPRIVATE void ImplDrawEmphasisMark(tools::Long nBaseX, tools::Long nX, tools::Long nY,
                                             const tools::PolyPolygon& rPolyPoly, bool bPolyLine,
                                             const tools::Rectangle& rRect1,
                                             const tools::Rectangle& rRect2);

    // text functions
    SAL_DLLPRIVATE void ImplDrawTextDirect(SalLayout&, bool bTextLines);
    SAL_DLLPRIVATE void ImplDrawSpecialText(SalLayout&);
    void ImplDrawTextBackground(SalLayout const&);
    bool ImplDrawRotateText(SalLayout&);

    void ImplDrawTextLine(tools::Long nBaseX, tools::Long nX, tools::Long nY,
                          DeviceCoordinate nWidth, FontStrikeout eStrikeout,
                          FontLineStyle eUnderline, FontLineStyle eOverline, bool bUnderlineAbove);

    void ImplDrawTextLines(SalLayout&, FontStrikeout eStrikeout, FontLineStyle eUnderline,
                           FontLineStyle eOverline, bool bWordLine, bool bUnderlineAbove);

    SAL_DLLPRIVATE void ImplDrawWaveTextLine(tools::Long nBaseX, tools::Long nBaseY, tools::Long nX,
                                             tools::Long nY, tools::Long nWidth,
                                             FontLineStyle eTextLine, Color aColor, bool bIsAbove);

    SAL_DLLPRIVATE void ImplDrawStraightTextLine(tools::Long nBaseX, tools::Long nBaseY,
                                                 tools::Long nX, tools::Long nY, tools::Long nWidth,
                                                 FontLineStyle eTextLine, Color aColor,
                                                 bool bIsAbove);

    SAL_DLLPRIVATE void ImplDrawWaveLine(tools::Long nBaseX, tools::Long nBaseY,
                                         tools::Long nStartX, tools::Long nStartY,
                                         tools::Long nWidth, tools::Long nHeight,
                                         tools::Long nLineWidth, Degree10 nOrientation,
                                         Color const& rColor);

    SAL_DLLPRIVATE static void ImplDrawWavePixel(tools::Long nOriginX, tools::Long nOriginY,
                                                 tools::Long nCurX, tools::Long nCurY,
                                                 Degree10 nOrientation, SalGraphics* pGraphics,
                                                 RenderContext2 const& rOutDev, bool bDrawPixAsRect,
                                                 tools::Long nPixWidth, tools::Long nPixHeight);

    SAL_DLLPRIVATE void ImplDrawStrikeoutLine(tools::Long nBaseX, tools::Long nBaseY,
                                              tools::Long nX, tools::Long nY, tools::Long nWidth,
                                              FontStrikeout eStrikeout, Color aColor);

    SAL_DLLPRIVATE void ImplDrawStrikeoutChar(tools::Long nBaseX, tools::Long nBaseY,
                                              tools::Long nX, tools::Long nY, tools::Long nWidth,
                                              FontStrikeout eStrikeout, Color aColor);

    SAL_DLLPRIVATE void ImplDrawMnemonicLine(tools::Long nX, tools::Long nY, tools::Long nWidth);

    std::vector<OutDevState> maOutDevStateStack;
    sal_Int32 mnDPIX;
    sal_Int32 mnDPIY;
    sal_Int32
        mnDPIScalePercentage; ///< For HiDPI displays, we want to draw elements for a percentage larger

    mutable bool mbOutput : 1;
};

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
