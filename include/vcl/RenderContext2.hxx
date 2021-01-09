/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <tools/color.hxx>
#include <tools/fontenum.hxx>
#include <i18nlangtag/lang.h>

#include <vcl/dllapi.h>
#include <vcl/font.hxx>
#include <vcl/settings.hxx>
#include <vcl/ComplexTextLayoutFlags.hxx>
#include <vcl/DrawModeFlags.hxx>
#include <vcl/Geometry.hxx>
#include <vcl/MappingMetrics.hxx>
#include <vcl/RasterOp.hxx>

#include <memory>

class AllSettings;
class SalGraphics;

class VCL_DLLPUBLIC RenderContext2
{
public:
    RenderContext2();
    virtual ~RenderContext2() {}

    virtual AllSettings const& GetSettings() const;
    virtual void SetSettings(AllSettings const& rSettings);

    bool IsOpaqueLineColor() const;
    Color const& GetLineColor() const;
    virtual void SetLineColor(Color const& rColor = COL_TRANSPARENT);
    void FlagLineColorAsTransparent();
    void FlagLineColorAsOpaque();

    bool IsOpaqueFillColor() const;
    Color const& GetFillColor() const;
    virtual void SetFillColor(Color const& rColor = COL_TRANSPARENT);
    void FlagFillColorAsTransparent();
    void FlagFillColorAsOpaque();

    Color const& GetTextColor() const;
    virtual void SetTextColor(Color const& rColor);

    bool IsOpaqueTextLineColor() const;
    Color GetTextLineColor() const;
    virtual void SetTextLineColor(Color const& rColor = COL_TRANSPARENT);

    bool IsOpaqueTextFillColor() const;
    Color GetTextFillColor() const;
    virtual void SetTextFillColor(Color const& rColor = COL_TRANSPARENT);

    bool IsOpaqueOverlineColor() const;
    Color GetOverlineColor() const;
    virtual void SetOverlineColor(Color const& rColor = COL_TRANSPARENT);

    bool IsNewFont() const;
    void SetNewFontFlag(bool bFlag);

    vcl::Font const& GetFont() const;
    virtual void SetFont(vcl::Font const& rNewFont);

    DrawModeFlags GetDrawMode() const;
    virtual void SetDrawMode(DrawModeFlags nDrawMode);

    RasterOp GetRasterOp() const;
    virtual void SetRasterOp(RasterOp eRasterOp);

    ComplexTextLayoutFlags GetLayoutMode() const;
    virtual void SetLayoutMode(ComplexTextLayoutFlags nTextLayoutMode);

    LanguageType GetDigitLanguage() const;
    virtual void SetDigitLanguage(LanguageType eTextLanguage);

    TextAlign GetTextAlign() const;
    virtual void SetTextAlign(TextAlign eAlign);

    bool IsMapModeEnabled() const;
    virtual void EnableMapMode(bool bEnable = true);

    Size GetSizeInPixels() const;
    tools::Long GetWidthInPixels() const;
    tools::Long GetHeightInPixels() const;
    void SetSizeInPixels(Size const& rSize);
    void SetWidthInPixels(tools::Long nWidth);
    void SetHeightInPixels(tools::Long nHeight);

    tools::Long GetXOffsetInPixels() const;
    tools::Long GetYOffsetInPixels() const;
    void SetXOffsetInPixels(tools::Long nOffsetXpx);
    void SetYOffsetInPixels(tools::Long nOffsetYpx);

    Size GetOffsetFromOriginInPixels() const;
    tools::Long GetXOffsetFromOriginInPixels() const;
    tools::Long GetYOffsetFromOriginInPixels() const;

    /** Set an offset in pixels

        This method offsets every drawing operation that converts its
        coordinates to pixel by the given value. Normally, the effect
        can be achieved by setting a MapMode with a different origin.
        Unfortunately, this origin is in logical coordinates and can
        lead to rounding errors (see #102532# for details).

        @attention This offset is only applied when converting to pixel,
        i.e. some output modes such as metafile recordings might be
        completely unaffected by this method! Use with care.
        Furthermore, if the OutputDevice's MapMode is the default
        (that's MapUnit::MapPixel), then any pixel offset set is ignored
        also. This might be unintuitive for cases, but would have been
        far more fragile to implement. What's more, the reason why the
        pixel offset was introduced (avoiding rounding errors) does not
        apply for MapUnit::MapPixel, because one can always use the
        MapMode origin then.

        @param rOffset The offset in pixels
     */
    virtual void SetOffsetFromOriginInPixels(Size const& rOffset);

    void ResetLogicalUnitsOffsetFromOrigin();
    sal_uInt32 GetXOffsetFromOriginInLogicalUnits() const;
    void SetXOffsetFromOriginInLogicalUnits(tools::Long nOffsetFromOriginXInLogicalUnits);
    sal_uInt32 GetYOffsetFromOriginInLogicalUnits() const;
    void SetYOffsetFromOriginInLogicalUnits(tools::Long nOffsetFromOriginYInLogicalUnits);

    tools::Long GetXMapOffset() const;
    void SetXMapOffset(tools::Long);
    tools::Long GetYMapOffset() const;
    void SetYMapOffset(tools::Long);
    tools::Long GetXMapNumerator() const;
    void SetXMapNumerator(tools::Long nNumerator);
    tools::Long GetYMapNumerator() const;
    void SetYMapNumerator(tools::Long nNumerator);
    tools::Long GetXMapDenominator() const;
    void SetXMapDenominator(tools::Long nDenominator);
    tools::Long GetYMapDenominator() const;
    void SetYMapDenominator(tools::Long nDenominator);

    sal_Int32 GetDPIX() const;
    sal_Int32 GetDPIY() const;
    void SetDPIX(sal_Int32 nDPIX);
    void SetDPIY(sal_Int32 nDPIY);
    float GetDPIScaleFactor() const;
    sal_Int32 GetDPIScalePercentage() const;
    void SetDPIScalePercentage(sal_Int32 nPercentage);

protected:
    /** Acquire a graphics device that the output device uses to draw on.

     There is an LRU of OutputDevices that is used to get the graphics. The
     actual creation of a SalGraphics instance is done via the SalFrame
     implementation.

     However, the SalFrame instance will only return a valid SalGraphics
     instance if it is not in use or there wasn't one in the first place. When
     this happens, AcquireGraphics finds the least recently used OutputDevice
     in a different frame and "steals" it (releases it then starts using it).

     If there are no frames to steal an OutputDevice's SalGraphics instance from
     then it blocks until the graphics is released.

     Once it has acquired a graphics instance, then we add the OutputDevice to
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

    bool IsInitLineColor() const;
    void SetInitLineColorFlag(bool bFlag);
    bool IsInitFillColor() const;
    void SetInitFillColorFlag(bool bFlag);
    bool IsInitTextColor() const;
    void SetInitTextColorFlag(bool bFlag);
    bool IsInitFont() const;
    void SetInitFontFlag(bool bFlag);

    // TODO these two init functions will need to become private once all related
    // functions are moved out of OutputDevice
    void InitLineColor();
    void InitFillColor();
    void InitTextColor();

    virtual void InitMapModeObjects();

    mutable SalGraphics* mpGraphics;
    std::unique_ptr<AllSettings> mxSettings;

    // TODO eventually make these private when all text/font functions migrated from
    // OutputDevice to RenderContext2
    Color maTextLineColor;
    vcl::Font maFont;
    ComplexTextLayoutFlags mnTextLayoutMode;
    LanguageType meTextLanguage;
    Geometry maGeometry;
    MappingMetrics maMappingMetric;

private:
    Color maTextColor;
    Color maLineColor;
    Color maFillColor;
    Color maOverlineColor;
    DrawModeFlags mnDrawMode;
    RasterOp meRasterOp;

    mutable bool mbOpaqueLineColor : 1;
    mutable bool mbOpaqueFillColor : 1;
    mutable bool mbInitLineColor : 1;
    mutable bool mbInitFillColor : 1;
    mutable bool mbInitTextColor : 1;
    mutable bool mbInitFont : 1;
    mutable bool mbNewFont : 1;
};

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
