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

    // colors used for item highlighting
    Color aSelectionBorderColor(pPaintColor ? *pPaintColor : rStyles.GetHighlightColor());
    Color aSelectionFillColor(aSelectionBorderColor);

    bool bDark = rStyles.GetFaceColor().IsDark();
    bool bBright = !bDark && rStyles.GetHighContrastMode();

    int c1 = aSelectionBorderColor.GetLuminance();
    int c2 = aWinBackgroundColor.GetLuminance();

    if (!bDark && !bBright && std::abs(c2 - c1) < (pPaintColor ? 40 : 75))
    {
        // contrast too low
        sal_uInt16 h, s, b;
        aSelectionFillColor.RGBtoHSB( h, s, b );
        if( b > 50 )    b -= 40;
        else            b += 40;
        aSelectionFillColor = Color::HSBtoRGB( h, s, b );
        aSelectionBorderColor = aSelectionFillColor;
    }

    if (bRoundEdges)
    {
        if (aSelectionBorderColor.IsDark())
            aSelectionBorderColor.IncreaseLuminance(128);
        else
            aSelectionBorderColor.DecreaseLuminance(128);
    }

    tools::Rectangle aRect(rRect);
    if (bDrawExtBorderOnly)
    {
        aRect.AdjustLeft( -1 );
        aRect.AdjustTop( -1 );
        aRect.AdjustRight(1 );
        aRect.AdjustBottom(1 );
    }
    auto popIt = ScopedPush(vcl::PushFlags::FILLCOLOR | vcl::PushFlags::LINECOLOR);

    if (bDrawBorder)
        SetLineColor(bDark ? COL_WHITE : (bBright ? COL_BLACK : aSelectionBorderColor));
    else
        SetLineColor();

    sal_uInt16 nPercent = 0;
    if (!nHighlight)
    {
        if (bDark)
            aSelectionFillColor = COL_BLACK;
        else
            nPercent = 80;  // just checked (light)
    }
    else
    {
        if (bChecked && nHighlight == 2)
        {
            if (bDark)
                aSelectionFillColor = COL_LIGHTGRAY;
            else if (bBright)
            {
                aSelectionFillColor = COL_BLACK;
                SetLineColor(COL_BLACK);
                nPercent = 0;
            }
            else
                nPercent = bRoundEdges ? 40 : 20; // selected, pressed or checked ( very dark )
        }
        else if (bChecked || nHighlight == 1)
        {
            if (bDark)
                aSelectionFillColor = COL_GRAY;
            else if (bBright)
            {
                aSelectionFillColor = COL_BLACK;
                SetLineColor(COL_BLACK);
                nPercent = 0;
            }
            else
                nPercent = bRoundEdges ? 60 : 35; // selected, pressed or checked ( very dark )
        }
        else
        {
            if (bDark)
                aSelectionFillColor = COL_LIGHTGRAY;
            else if (bBright)
            {
                aSelectionFillColor = COL_BLACK;
                SetLineColor(COL_BLACK);
                if (nHighlight == 3)
                    nPercent = 80;
                else
                    nPercent = 0;
            }
            else
                nPercent = 70; // selected ( dark )
        }
    }

    Color aSelectionTextColor;

    if (bDark && bDrawExtBorderOnly)
    {
        SetFillColor();
        aSelectionTextColor = rStyles.GetHighlightTextColor();
    }
    else
    {
        SetFillColor(aSelectionFillColor);

        Color aTextColor = pWinControlForeground ? *pWinControlForeground : rStyles.GetButtonTextColor();
        Color aHLTextColor = rStyles.GetHighlightTextColor();
        int nTextDiff = std::abs(aSelectionFillColor.GetLuminance() - aTextColor.GetLuminance());
        int nHLDiff = std::abs(aSelectionFillColor.GetLuminance() - aHLTextColor.GetLuminance());
        aSelectionTextColor = (nHLDiff >= nTextDiff) ? aHLTextColor : aTextColor;
    }

    if (bDark)
    {
        DrawRect(aRect);
    }
    else
    {
        if (bRoundEdges)
        {
            tools::Polygon aPoly(aRect, nCornerRadius, nCornerRadius);
            tools::PolyPolygon aPolyPoly(aPoly);
            DrawTransparent(aPolyPoly, nPercent);
        }
        else
        {
            tools::Polygon aPoly(aRect);
            tools::PolyPolygon aPolyPoly(aPoly);
            DrawTransparent(aPolyPoly, nPercent);
        }
    }

    return aSelectionTextColor;
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
