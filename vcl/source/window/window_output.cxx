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

#include <tools/mapunit.hxx>

#include <vcl/CoordinateMapper.hxx>
#include <vcl/font.hxx>
#include <vcl/outdev.hxx>
#include <vcl/window.hxx>

#include <windowdev.hxx>

namespace vcl
{
vcl::Font Window::GetDrawPixelFont(OutputDevice const* pDev) const
{
    vcl::Font aFont = GetPointFont(*GetOutDev());
    MapMode aPtMapMode(MapUnit::MapPoint);
    const auto aFontSize
        = pDev->convertTo<vcl::WindowSize>(vcl::LogicSize(aFont.GetFontSize()), aPtMapMode);
    aFont.SetFontSize(aFontSize.get());
    return aFont;
}

tools::Long Window::GetDrawPixel(OutputDevice const* pDev, tools::Long nPixels) const
{
    tools::Long nP = nPixels;
    if (pDev->GetOutDevType() != OUTDEV_WINDOW)
    {
        MapMode aMap(MapUnit::Map100thMM);
        auto aSz = convertTo<vcl::WindowSize>(vcl::LogicSize(nP, 0), aMap);
        nP = aSz->Width();
    }
    return nP;
}

const Font& Window::GetFont() const { return GetOutDev()->GetFont(); }
void Window::SetFont(Font const& font) { return GetOutDev()->SetFont(font); }

float Window::approximate_char_width() const { return GetOutDev()->approximate_char_width(); }

const Wallpaper& Window::GetBackground() const { return GetOutDev()->GetBackground(); }
bool Window::IsBackground() const { return GetOutDev()->IsBackground(); }
tools::Long Window::GetTextHeight() const { return GetOutDev()->GetTextHeight(); }
tools::Long Window::GetTextWidth(const OUString& rStr, sal_Int32 nIndex, sal_Int32 nLen,
                                 vcl::text::TextLayoutCache const* pCache,
                                 SalLayoutGlyphs const* const pLayoutCache) const
{
    return GetOutDev()->GetTextWidth(rStr, nIndex, nLen, pCache, pLayoutCache);
}
float Window::approximate_digit_width() const { return GetOutDev()->approximate_digit_width(); }

bool Window::IsNativeControlSupported(ControlType nType, ControlPart nPart) const
{
    return GetOutDev()->IsNativeControlSupported(nType, nPart);
}

bool Window::GetNativeControlRegion(ControlType nType, ControlPart nPart,
                                    const tools::Rectangle& rControlRegion, ControlState nState,
                                    const ImplControlValue& aValue,
                                    tools::Rectangle& rNativeBoundingRegion,
                                    tools::Rectangle& rNativeContentRegion) const
{
    return GetOutDev()->GetNativeControlRegion(nType, nPart, rControlRegion, nState, aValue,
                                               rNativeBoundingRegion, rNativeContentRegion);
}

void Window::SetTextLineColor() { GetOutDev()->SetTextLineColor(); }

void Window::SetTextLineColor(const Color& rColor) { GetOutDev()->SetTextLineColor(rColor); }

void Window::SetOverlineColor() { GetOutDev()->SetOverlineColor(); }

void Window::SetOverlineColor(const Color& rColor) { GetOutDev()->SetOverlineColor(rColor); }

void Window::SetTextFillColor() { GetOutDev()->SetTextFillColor(); }

void Window::SetTextFillColor(const Color& rColor) { GetOutDev()->SetTextFillColor(rColor); }

const MapMode& Window::GetMapMode() const { return GetOutDev()->GetMapMode(); }

void Window::SetBackground() { GetOutDev()->SetBackground(); }

void Window::SetBackground(const Wallpaper& rBackground)
{
    GetOutDev()->SetBackground(rBackground);
}

void Window::SetMappingPolicy(vcl::MappingPolicy ePolicy)
{
    GetOutDev()->SetMappingPolicy(ePolicy);
}

vcl::MappingPolicy Window::GetMappingPolicy() const { return GetOutDev()->GetMappingPolicy(); }

void Window::SetTextColor(const Color& rColor) { GetOutDev()->SetTextColor(rColor); }

const Color& Window::GetTextColor() const { return GetOutDev()->GetTextColor(); }

const Color& Window::GetTextLineColor() const { return GetOutDev()->GetTextLineColor(); }

bool Window::IsTextLineColor() const { return GetOutDev()->IsTextLineColor(); }

Color Window::GetTextFillColor() const { return GetOutDev()->GetTextFillColor(); }

bool Window::IsTextFillColor() const { return GetOutDev()->IsTextFillColor(); }

const Color& Window::GetOverlineColor() const { return GetOutDev()->GetOverlineColor(); }

bool Window::IsOverlineColor() const { return GetOutDev()->IsOverlineColor(); }

void Window::SetTextAlign(TextAlign eAlign) { GetOutDev()->SetTextAlign(eAlign); }

float Window::GetDPIScaleFactor() const { return GetOutDev()->GetDPIScaleFactor(); }

tools::Long Window::GetDeviceOriginX() const { return GetOutDev()->GetDeviceOriginX(); }

tools::Long Window::GetDeviceOriginY() const { return GetOutDev()->GetDeviceOriginY(); }

void Window::SetMapMode() { GetOutDev()->SetMapMode(); }

void Window::SetMapMode(const MapMode& rNewMapMode) { GetOutDev()->SetMapMode(rNewMapMode); }

bool Window::IsRTLEnabled() const { return GetOutDev()->IsRTLEnabled(); }

TextAlign Window::GetTextAlign() const { return GetOutDev()->GetTextAlign(); }

const AllSettings& Window::GetSettings() const { return GetOutDev()->GetSettings(); }

tools::Rectangle Window::GetTextRect(const tools::Rectangle& rRect, const OUString& rStr,
                                     DrawTextFlags nStyle, TextRectInfo* pInfo,
                                     const vcl::TextLayoutCommon* _pTextLayout) const
{
    return GetOutDev()->GetTextRect(rRect, rStr, nStyle, pInfo, _pTextLayout);
}

void Window::SetSettings(const AllSettings& rSettings) { GetOutDev()->SetSettings(rSettings); }

void Window::SetSettings(const AllSettings& rSettings, bool bChild)
{
    static_cast<vcl::WindowOutputDevice*>(GetOutDev())->SetSettings(rSettings, bChild);
}

Color Window::GetBackgroundColor() const { return GetOutDev()->GetBackgroundColor(); }

void Window::EnableRTL(bool bEnable) { GetOutDev()->EnableRTL(bEnable); }

} // end vcl namespace

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
