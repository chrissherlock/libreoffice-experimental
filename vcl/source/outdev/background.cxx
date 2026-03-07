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

#include <vcl/virdev.hxx>

Color OutputDevice::GetBackgroundColor() const
{
    return GetBackground().GetColor();
}

void OutputDevice::SetBackground()
{
    maBackground = Wallpaper();
    mbBackground = false;
}

void OutputDevice::SetBackground( const Wallpaper& rBackground )
{
    maBackground = rBackground;

    if( rBackground.GetStyle() == WallpaperStyle::NONE )
        mbBackground = false;
    else
        mbBackground = true;
}

namespace
{
struct SelectionPaintStyle
{
    bool       bUseSolidFill;
    sal_uInt16 nTransparencyPercent;
};
} // end anonymous namespace

/**
 * Determines HOW the selection background should be rendered (solid vs. transparent).
 */
static SelectionPaintStyle lcl_CalculateSelectionStyle(
    const StyleSettings& rStyles,
    sal_uInt16 nHighlight,
    bool bChecked,
    bool bRoundEdges)
{
    SelectionPaintStyle aStyle{ false, 0 };

    bool bDark = rStyles.GetFaceColor().IsDark();
    bool bBright = !bDark && rStyles.GetHighContrastMode();

    aStyle.bUseSolidFill = bDark;

    if (!nHighlight)
    {
        if (!bDark)
            aStyle.nTransparencyPercent = 80;
    }
    else
    {
        if (bChecked && nHighlight == 2)
        {
            if (!bDark && !bBright)
                aStyle.nTransparencyPercent = bRoundEdges ? 40 : 20;
        }
        else if (bChecked || nHighlight == 1)
        {
            if (!bDark && !bBright)
                aStyle.nTransparencyPercent = bRoundEdges ? 60 : 35;
        }
        else
        {
            if (bBright)
                aStyle.nTransparencyPercent = (nHighlight == 3) ? 80 : 0;
            else if (!bDark)
                aStyle.nTransparencyPercent = 70;
        }
    }

    return aStyle;
}

/**
 * Ensures the base highlight color has enough contrast against the window background.
 */
static Color lcl_GetBaseHighlightColor(const StyleSettings& rStyles, Color aWinBgColor, Color const * pPaintColor)
{
    Color aBaseColor(pPaintColor ? *pPaintColor : rStyles.GetHighlightColor());
    bool bDark = rStyles.GetFaceColor().IsDark();
    bool bBright = !bDark && rStyles.GetHighContrastMode();

    if (!bDark && !bBright)
    {
        int c1 = aBaseColor.GetLuminance();
        int c2 = aWinBgColor.GetLuminance();

        if (std::abs(c2 - c1) < (pPaintColor ? 40 : 75))
        {
            sal_uInt16 h, s, b;
            aBaseColor.RGBtoHSB(h, s, b);
            b = (b > 50) ? b - 40 : b + 40;
            return Color::HSBtoRGB(h, s, b);
        }
    }
    return aBaseColor;
}

/**
 * Returns the most legible text color (base vs highlight) over the given fill color.
 */
static Color lcl_GetContrastingTextColor(const StyleSettings& rStyles, const Color& rFillColor, Color const * pWinControlForeground)
{
    Color aBaseText = pWinControlForeground ? *pWinControlForeground : rStyles.GetButtonTextColor();
    Color aHighlightText = rStyles.GetHighlightTextColor();

    int nTextDiff = std::abs(rFillColor.GetLuminance() - aBaseText.GetLuminance());
    int nHLDiff = std::abs(rFillColor.GetLuminance() - aHighlightText.GetLuminance());

    return (nHLDiff >= nTextDiff) ? aHighlightText : aBaseText;
}

/**
 * Evaluates widget state to determine the correct background fill color.
 */
static std::optional<Color> lcl_GetSelectionFillColor(
    const StyleSettings& rStyles, Color aBaseColor,
    sal_uInt16 nHighlight, bool bChecked, bool bDrawExtBorderOnly)
{
    bool bDark = rStyles.GetFaceColor().IsDark();
    bool bBright = !bDark && rStyles.GetHighContrastMode();

    if (bDark && bDrawExtBorderOnly)
        return std::nullopt; // Signal to not fill, just draw outline

    if (!nHighlight)
        return bDark ? std::optional<Color>(COL_BLACK) : aBaseColor;

    if (bChecked && nHighlight == 2)
        return bDark ? COL_LIGHTGRAY : (bBright ? COL_BLACK : aBaseColor);

    if (bChecked || nHighlight == 1)
        return bDark ? COL_GRAY : (bBright ? COL_BLACK : aBaseColor);

    return bDark ? COL_LIGHTGRAY : (bBright ? COL_BLACK : aBaseColor);
}

/**
 * Evaluates widget state and geometry to determine the correct border color.
 */
static std::optional<Color> lcl_GetSelectionBorderColor(
    const StyleSettings& rStyles, Color aBaseColor, bool bDrawBorder, bool bRoundEdges)
{
    if (!bDrawBorder)
        return std::nullopt;

    bool bDark = rStyles.GetFaceColor().IsDark();
    bool bBright = !bDark && rStyles.GetHighContrastMode();

    if (bDark)
        return COL_WHITE;

    if (bBright)
        return COL_BLACK;

    Color aBorderColor = aBaseColor;
    if (bRoundEdges)
    {
        if (aBorderColor.IsDark())
            aBorderColor.IncreaseLuminance(128);
        else
            aBorderColor.DecreaseLuminance(128);
    }

    return aBorderColor;
}

Color OutputDevice::DrawSelectionBackground(const tools::Rectangle& rRect,
                                            Color aWinBackgroundColor,
                                            sal_uInt16 nHighlight,
                                            bool bChecked,
                                            bool bDrawBorder,
                                            bool bDrawExtBorderOnly,
                                            Color const * pWinControlForeground,
                                            tools::Long nCornerRadius,
                                            Color const * pPaintColor)
{
    if (rRect.IsEmpty())
        return COL_TRANSPARENT;

    bool bRoundEdges = nCornerRadius > 0;
    const StyleSettings& rStyles = GetSettings().GetStyleSettings();

    SelectionPaintStyle aStyle = lcl_CalculateSelectionStyle(
        rStyles, nHighlight, bChecked, bRoundEdges);

    Color aBaseColor = lcl_GetBaseHighlightColor(rStyles, aWinBackgroundColor, pPaintColor);

    std::optional<Color> oFillColor = lcl_GetSelectionFillColor(
        rStyles, aBaseColor, nHighlight, bChecked, bDrawExtBorderOnly);

    std::optional<Color> oLineColor = lcl_GetSelectionBorderColor(
        rStyles, aBaseColor, bDrawBorder, bRoundEdges);

    Color aTextColor;
    if (rStyles.GetFaceColor().IsDark() && bDrawExtBorderOnly)
    {
        aTextColor = rStyles.GetHighlightTextColor();
    }
    else
    {
        Color aPaintFill = oFillColor.value_or(aBaseColor);
        aTextColor = lcl_GetContrastingTextColor(rStyles, aPaintFill, pWinControlForeground);
    }

    tools::Rectangle aRect(rRect);
    if (bDrawExtBorderOnly)
    {
        aRect.AdjustLeft(-1);
        aRect.AdjustTop(-1);
        aRect.AdjustRight(1);
        aRect.AdjustBottom(1);
    }

    auto popIt = ScopedPush(vcl::PushFlags::FILLCOLOR | vcl::PushFlags::LINECOLOR);

    if (oLineColor)
        SetLineColor(*oLineColor);
    else
        SetLineColor();

    if (oFillColor)
        SetFillColor(*oFillColor);
    else
        SetFillColor();

    if (aStyle.bUseSolidFill)
    {
        DrawRect(aRect);
    }
    else
    {
        tools::Polygon aPoly = bRoundEdges ? tools::Polygon(aRect, nCornerRadius, nCornerRadius) : tools::Polygon(aRect);
        DrawTransparent(tools::PolyPolygon(aPoly), aStyle.nTransparencyPercent);
    }

    return aTextColor;
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
