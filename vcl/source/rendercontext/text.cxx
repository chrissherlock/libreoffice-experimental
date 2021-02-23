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

#include <vcl/RenderContext2.hxx>
#include <vcl/settings.hxx>
#include <vcl/virdev.hxx>

#include <drawmode.hxx>

ComplexTextLayoutFlags RenderContext2::GetLayoutMode() const { return mnTextLayoutMode; }

void RenderContext2::SetLayoutMode(ComplexTextLayoutFlags nTextLayoutMode)
{
    mnTextLayoutMode = nTextLayoutMode;

    if (mpAlphaVDev)
        mpAlphaVDev->SetLayoutMode(nTextLayoutMode);
}

LanguageType RenderContext2::GetDigitLanguage() const { return meTextLanguage; }

void RenderContext2::SetDigitLanguage(LanguageType eTextLanguage)
{
    meTextLanguage = eTextLanguage;

    if (mpAlphaVDev)
        mpAlphaVDev->SetDigitLanguage(eTextLanguage);
}

Color const& RenderContext2::GetTextColor() const { return maTextColor; }

void RenderContext2::SetTextColor(Color const& rColor)
{
    Color aColor = GetDrawModeTextColor(rColor, GetDrawMode(), GetSettings().GetStyleSettings());

    if (maTextColor != aColor)
    {
        maTextColor = aColor;
        mbInitTextColor = true;
    }

    if (mpAlphaVDev)
        mpAlphaVDev->SetTextColor(COL_BLACK);
}

bool RenderContext2::IsTextFillColor() const { return !maFont.IsTransparent(); }

Color RenderContext2::GetTextFillColor() const
{
    if (maFont.IsTransparent())
        return COL_TRANSPARENT;
    else
        return maFont.GetFillColor();
}

void RenderContext2::SetTextFillColor()
{
    if (maFont.GetColor() != COL_TRANSPARENT)
        maFont.SetFillColor(COL_TRANSPARENT);

    if (!maFont.IsTransparent())
        maFont.SetTransparent(true);

    if (mpAlphaVDev)
        mpAlphaVDev->SetTextFillColor();
}

void RenderContext2::SetTextFillColor(Color const& rColor)
{
    Color aColor(rColor);
    bool bTransFill = aColor.IsTransparent();

    if (!bTransFill)
    {
        if (mnDrawMode
            & (DrawModeFlags::BlackFill | DrawModeFlags::WhiteFill | DrawModeFlags::GrayFill
               | DrawModeFlags::NoFill | DrawModeFlags::SettingsFill))
        {
            if (mnDrawMode & DrawModeFlags::BlackFill)
            {
                aColor = COL_BLACK;
            }
            else if (mnDrawMode & DrawModeFlags::WhiteFill)
            {
                aColor = COL_WHITE;
            }
            else if (mnDrawMode & DrawModeFlags::GrayFill)
            {
                const sal_uInt8 cLum = aColor.GetLuminance();
                aColor = Color(cLum, cLum, cLum);
            }
            else if (mnDrawMode & DrawModeFlags::SettingsFill)
            {
                aColor = GetSettings().GetStyleSettings().GetWindowColor();
            }
            else if (mnDrawMode & DrawModeFlags::NoFill)
            {
                aColor = COL_TRANSPARENT;
                bTransFill = true;
            }
        }
    }

    if (maFont.GetFillColor() != aColor)
        maFont.SetFillColor(aColor);

    if (maFont.IsTransparent() != bTransFill)
        maFont.SetTransparent(bTransFill);

    if (mpAlphaVDev)
        mpAlphaVDev->SetTextFillColor(COL_BLACK);
}

bool RenderContext2::IsTextLineColor() const { return !maTextLineColor.IsTransparent(); }

Color const& RenderContext2::GetTextLineColor() const { return maTextLineColor; }

void RenderContext2::SetTextLineColor()
{
    maTextLineColor = COL_TRANSPARENT;

    if (mpAlphaVDev)
        mpAlphaVDev->SetTextLineColor();
}

void RenderContext2::SetTextLineColor(Color const& rColor)
{
    maTextLineColor = GetDrawModeTextColor(rColor, GetDrawMode(), GetSettings().GetStyleSettings());

    if (mpAlphaVDev)
        mpAlphaVDev->SetTextLineColor(COL_BLACK);
}

bool RenderContext2::IsOverlineColor() const { return !maOverlineColor.IsTransparent(); }

Color const& RenderContext2::GetOverlineColor() const { return maOverlineColor; }

void RenderContext2::SetOverlineColor()
{
    maOverlineColor = COL_TRANSPARENT;

    if (mpAlphaVDev)
        mpAlphaVDev->SetOverlineColor();
}

void RenderContext2::SetOverlineColor(Color const& rColor)
{
    maOverlineColor = GetDrawModeTextColor(rColor, GetDrawMode(), GetSettings().GetStyleSettings());

    if (mpAlphaVDev)
        mpAlphaVDev->SetOverlineColor(COL_BLACK);
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
