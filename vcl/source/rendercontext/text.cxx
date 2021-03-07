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

#include <rtl/ustrbuf.hxx>

#include <vcl/RenderContext2.hxx>
#include <vcl/TextLayoutCache.hxx>
#include <vcl/settings.hxx>
#include <vcl/virdev.hxx>

#include <drawmode.hxx>
#include <fontinstance.hxx>

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

TextAlign RenderContext2::GetTextAlign() const { return maFont.GetAlignment(); }

void RenderContext2::SetTextAlign(TextAlign eAlign)
{
    if (maFont.GetAlignment() != eAlign)
    {
        maFont.SetAlignment(eAlign);
        mbNewFont = true;
    }

    if (mpAlphaVDev)
        mpAlphaVDev->SetTextAlign(eAlign);
}

ImplLayoutArgs
RenderContext2::ImplPrepareLayoutArgs(OUString& rStr, const sal_Int32 nMinIndex,
                                      const sal_Int32 nLen, DeviceCoordinate nPixelWidth,
                                      DeviceCoordinate const* pDXArray, SalLayoutFlags nLayoutFlags,
                                      vcl::TextLayoutCache const* const pLayoutCache) const
{
    assert(nMinIndex >= 0);
    assert(nLen >= 0);

    // get string length for calculating extents
    sal_Int32 nEndIndex = rStr.getLength();

    if (nMinIndex + nLen < nEndIndex)
        nEndIndex = nMinIndex + nLen;

    // don't bother if there is nothing to do
    if (nEndIndex < nMinIndex)
        nEndIndex = nMinIndex;

    if (mnTextLayoutMode & ComplexTextLayoutFlags::BiDiRtl)
        nLayoutFlags |= SalLayoutFlags::BiDiRtl;

    if (mnTextLayoutMode & ComplexTextLayoutFlags::BiDiStrong)
    {
        nLayoutFlags |= SalLayoutFlags::BiDiStrong;
    }
    else if (!(mnTextLayoutMode & ComplexTextLayoutFlags::BiDiRtl))
    {
        // Disable Bidi if no RTL hint and only known LTR codes used.
        bool bAllLtr = true;
        for (sal_Int32 i = nMinIndex; i < nEndIndex; i++)
        {
            // [0x0000, 0x052F] are Latin, Greek and Cyrillic.
            // [0x0370, 0x03FF] has a few holes as if Unicode 10.0.0, but
            //                  hopefully no RTL character will be encoded there.
            if (rStr[i] > 0x052F)
            {
                bAllLtr = false;
                break;
            }
        }

        if (bAllLtr)
            nLayoutFlags |= SalLayoutFlags::BiDiStrong;
    }

    if (!maFont.IsKerning())
        nLayoutFlags |= SalLayoutFlags::DisableKerning;

    if (maFont.GetKerning() & FontKerning::Asian)
        nLayoutFlags |= SalLayoutFlags::KerningAsian;

    if (maFont.IsVertical())
        nLayoutFlags |= SalLayoutFlags::Vertical;

    if (meTextLanguage) //TODO: (mnTextLayoutMode & ComplexTextLayoutFlags::SubstituteDigits)
    {
        // disable character localization when no digits used
        const sal_Unicode* pBase = rStr.getStr();
        const sal_Unicode* pStr = pBase + nMinIndex;
        const sal_Unicode* pEnd = pBase + nEndIndex;
        std::optional<OUStringBuffer> xTmpStr;
        for (; pStr < pEnd; ++pStr)
        {
            // TODO: are there non-digit localizations?
            if ((*pStr >= '0') && (*pStr <= '9'))
            {
                // translate characters to local preference
                sal_UCS4 cChar = GetLocalizedChar(*pStr, meTextLanguage);
                if (cChar != *pStr)
                {
                    if (!xTmpStr)
                        xTmpStr = OUStringBuffer(rStr);

                    // TODO: are the localized digit surrogates?
                    (*xTmpStr)[pStr - pBase] = cChar;
                }
            }
        }
        if (xTmpStr)
            rStr = (*xTmpStr).makeStringAndClear();
    }

    // right align for RTL text, DRAWPOS_REVERSED, RTL window style
    bool bRightAlign = bool(mnTextLayoutMode & ComplexTextLayoutFlags::BiDiRtl);

    if (mnTextLayoutMode & ComplexTextLayoutFlags::TextOriginLeft)
        bRightAlign = false;
    else if (mnTextLayoutMode & ComplexTextLayoutFlags::TextOriginRight)
        bRightAlign = true;

    // SSA: hack for western office, ie text get right aligned
    //      for debugging purposes of mirrored UI
    bool bRTLWindow = IsRTLEnabled();
    bRightAlign ^= bRTLWindow;

    if (bRightAlign)
        nLayoutFlags |= SalLayoutFlags::RightAlign;

    // set layout options
    ImplLayoutArgs aLayoutArgs(rStr, nMinIndex, nEndIndex, nLayoutFlags, maFont.GetLanguageTag(),
                               pLayoutCache);

    Degree10 nOrientation = mpFontInstance ? mpFontInstance->mnOrientation : 0_deg10;
    aLayoutArgs.SetOrientation(nOrientation);

    aLayoutArgs.SetLayoutWidth(nPixelWidth);
    aLayoutArgs.SetDXArray(pDXArray);

    return aLayoutArgs;
}
/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
