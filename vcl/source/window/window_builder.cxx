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

#include <o3tl/string_view.hxx>

#include <vcl/builder.hxx>
#include <vcl/font.hxx>
#include <vcl/toolkit/fixed.hxx>
#include <vcl/vclenum.hxx>
#include <vcl/window.hxx>

#include <window.h>

#include <com/sun/star/accessibility/AccessibleRelation.hpp>
#include <com/sun/star/accessibility/AccessibleRole.hpp>

namespace vcl
{
static VclAlign toAlign(std::u16string_view rValue)
{
    VclAlign eRet = VclAlign::Fill;

    if (rValue == u"fill")
        eRet = VclAlign::Fill;
    else if (rValue == u"start")
        eRet = VclAlign::Start;
    else if (rValue == u"end")
        eRet = VclAlign::End;
    else if (rValue == u"center")
        eRet = VclAlign::Center;
    return eRet;
}

bool Window::set_font_attribute(const OUString& rKey, std::u16string_view rValue)
{
    if (rKey == "weight")
    {
        vcl::Font aFont(GetControlFont());
        if (rValue == u"thin")
            aFont.SetWeight(WEIGHT_THIN);
        else if (rValue == u"ultralight")
            aFont.SetWeight(WEIGHT_ULTRALIGHT);
        else if (rValue == u"light")
            aFont.SetWeight(WEIGHT_LIGHT);
        else if (rValue == u"book")
            aFont.SetWeight(WEIGHT_SEMILIGHT);
        else if (rValue == u"normal")
            aFont.SetWeight(WEIGHT_NORMAL);
        else if (rValue == u"medium")
            aFont.SetWeight(WEIGHT_MEDIUM);
        else if (rValue == u"semibold")
            aFont.SetWeight(WEIGHT_SEMIBOLD);
        else if (rValue == u"bold")
            aFont.SetWeight(WEIGHT_BOLD);
        else if (rValue == u"ultrabold")
            aFont.SetWeight(WEIGHT_ULTRABOLD);
        else
            aFont.SetWeight(WEIGHT_BLACK);
        SetControlFont(aFont);
    }
    else if (rKey == "style")
    {
        vcl::Font aFont(GetControlFont());
        if (rValue == u"normal")
            aFont.SetItalic(ITALIC_NONE);
        else if (rValue == u"oblique")
            aFont.SetItalic(ITALIC_OBLIQUE);
        else if (rValue == u"italic")
            aFont.SetItalic(ITALIC_NORMAL);
        SetControlFont(aFont);
    }
    else if (rKey == "underline")
    {
        vcl::Font aFont(GetControlFont());
        aFont.SetUnderline(toBool(rValue) ? LINESTYLE_SINGLE : LINESTYLE_NONE);
        SetControlFont(aFont);
    }
    else if (rKey == "scale")
    {
        // if no control font was set yet, take the underlying font from the device
        vcl::Font aFont(IsControlFont() ? GetControlFont() : GetPointFont(*GetOutDev()));
        aFont.SetFontHeight(aFont.GetFontHeight() * o3tl::toDouble(rValue));
        SetControlFont(aFont);
    }
    else if (rKey == "size")
    {
        vcl::Font aFont(GetControlFont());
        sal_Int32 nHeight = o3tl::toInt32(rValue) / 1000;
        aFont.SetFontHeight(nHeight);
        SetControlFont(aFont);
    }
    else
    {
        SAL_INFO("vcl.layout", "unhandled font attribute: " << rKey);
        return false;
    }
    return true;
}

bool Window::set_property(const OUString& rKey, const OUString& rValue)
{
    if ((rKey == "label") || (rKey == "title") || (rKey == "text"))
    {
        SetText(BuilderUtils::convertMnemonicMarkup(rValue));
    }
    else if (rKey == "visible")
        Show(toBool(rValue));
    else if (rKey == "sensitive")
        Enable(toBool(rValue));
    else if (rKey == "resizable")
    {
        WinBits nBits = GetStyle();
        nBits &= ~WB_SIZEABLE;
        if (toBool(rValue))
            nBits |= WB_SIZEABLE;
        SetStyle(nBits);
    }
    else if (rKey == "xalign")
    {
        WinBits nBits = GetStyle();
        nBits &= ~(WB_LEFT | WB_CENTER | WB_RIGHT);

        float f = rValue.toFloat();
        assert(f == 0.0 || f == 1.0 || f == 0.5);
        if (f == 0.0)
            nBits |= WB_LEFT;
        else if (f == 1.0)
            nBits |= WB_RIGHT;
        else if (f == 0.5)
            nBits |= WB_CENTER;

        SetStyle(nBits);
    }
    else if (rKey == "justification")
    {
        WinBits nBits = GetStyle();
        nBits &= ~(WB_LEFT | WB_CENTER | WB_RIGHT);

        if (rValue == "left")
            nBits |= WB_LEFT;
        else if (rValue == "right")
            nBits |= WB_RIGHT;
        else if (rValue == "center")
            nBits |= WB_CENTER;

        SetStyle(nBits);
    }
    else if (rKey == "yalign")
    {
        WinBits nBits = GetStyle();
        nBits &= ~(WB_TOP | WB_VCENTER | WB_BOTTOM);

        float f = rValue.toFloat();
        assert(f == 0.0 || f == 1.0 || f == 0.5);
        if (f == 0.0)
            nBits |= WB_TOP;
        else if (f == 1.0)
            nBits |= WB_BOTTOM;
        else if (f == 0.5)
            nBits |= WB_VCENTER;

        SetStyle(nBits);
    }
    else if (rKey == "wrap")
    {
        WinBits nBits = GetStyle();
        nBits &= ~WB_WORDBREAK;
        if (toBool(rValue))
            nBits |= WB_WORDBREAK;
        SetStyle(nBits);
    }
    else if (rKey == "height-request")
        set_height_request(rValue.toInt32());
    else if (rKey == "width-request")
        set_width_request(rValue.toInt32());
    else if (rKey == "hexpand")
        set_hexpand(toBool(rValue));
    else if (rKey == "vexpand")
        set_vexpand(toBool(rValue));
    else if (rKey == "halign")
        set_halign(toAlign(rValue));
    else if (rKey == "valign")
        set_valign(toAlign(rValue));
    else if (rKey == "tooltip-markup")
        SetQuickHelpText(rValue);
    else if (rKey == "tooltip-text")
        SetQuickHelpText(rValue);
    else if (rKey == "border-width")
        set_border_width(rValue.toInt32());
    else if (rKey == "margin-start" || rKey == "margin-left")
    {
        assert(rKey == "margin-start" && "margin-left deprecated in favor of margin-start");
        set_margin_start(rValue.toInt32());
    }
    else if (rKey == "margin-end" || rKey == "margin-right")
    {
        assert(rKey == "margin-end" && "margin-right deprecated in favor of margin-end");
        set_margin_end(rValue.toInt32());
    }
    else if (rKey == "margin-top")
        set_margin_top(rValue.toInt32());
    else if (rKey == "margin-bottom")
        set_margin_bottom(rValue.toInt32());
    else if (rKey == "hscrollbar-policy")
    {
        WinBits nBits = GetStyle();
        nBits &= ~(WB_AUTOHSCROLL | WB_HSCROLL);
        if (rValue == "always")
            nBits |= WB_HSCROLL;
        else if (rValue == "automatic")
            nBits |= WB_AUTOHSCROLL;
        SetStyle(nBits);
    }
    else if (rKey == "vscrollbar-policy")
    {
        WinBits nBits = GetStyle();
        nBits &= ~(WB_AUTOVSCROLL | WB_VSCROLL);
        if (rValue == "always")
            nBits |= WB_VSCROLL;
        else if (rValue == "automatic")
            nBits |= WB_AUTOVSCROLL;
        SetStyle(nBits);
    }
    else if (rKey == "accessible-name")
    {
        SetAccessibleName(rValue);
    }
    else if (rKey == "accessible-description")
    {
        SetAccessibleDescription(rValue);
    }
    else if (rKey == "accessible-role")
    {
        sal_Int16 role = BuilderUtils::getRoleFromName(rValue);
        if (role != css::accessibility::AccessibleRole::UNKNOWN)
            SetAccessibleRole(role);
    }
    else if (rKey == "use-markup")
    {
        //https://live.gnome.org/GnomeGoals/RemoveMarkupInMessages
        SAL_WARN_IF(toBool(rValue), "vcl.layout", "Use pango attributes instead of mark-up");
    }
    else if (rKey == "has-focus")
    {
        if (toBool(rValue))
            GrabFocus();
    }
    else if (rKey == "can-focus")
    {
        WinBits nBits = GetStyle();
        nBits &= ~(WB_TABSTOP | WB_NOTABSTOP);
        if (toBool(rValue))
            nBits |= WB_TABSTOP;
        else
            nBits |= WB_NOTABSTOP;
        SetStyle(nBits);
    }
    else
    {
        SAL_INFO("vcl.layout", "unhandled property: " << rKey);
        return false;
    }
    return true;
}

void Window::add_mnemonic_label(FixedText* pLabel)
{
    std::vector<VclPtr<FixedText>>& v = mpWindowImpl->m_aMnemonicLabels;
    if (std::find(v.begin(), v.end(), VclPtr<FixedText>(pLabel)) != v.end())
        return;
    v.emplace_back(pLabel);
    pLabel->set_mnemonic_widget(this);
}

void Window::remove_mnemonic_label(FixedText* pLabel)
{
    std::vector<VclPtr<FixedText>>& v = mpWindowImpl->m_aMnemonicLabels;
    auto aFind = std::find(v.begin(), v.end(), VclPtr<FixedText>(pLabel));
    if (aFind == v.end())
        return;
    v.erase(aFind);
    pLabel->set_mnemonic_widget(nullptr);
}

const std::vector<VclPtr<FixedText>>& Window::list_mnemonic_labels() const
{
    return mpWindowImpl->m_aMnemonicLabels;
}
} // end vcl namespace

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
