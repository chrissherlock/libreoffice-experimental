
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

#include <vcl/CoordinateMapper.hxx>
#include <vcl/cursor.hxx>
#include <vcl/event.hxx>
#include <vcl/toolbox.hxx>
#include <vcl/window.hxx>

#include <toolbox.h>
#include <ImplFrameData.hxx>
#include <ImplWinData.hxx>
#include <WindowImpl.hxx>
#include <WindowHierarchy.hxx>
#include <WindowControlAppearance.hxx>
#include <WindowGeometry.hxx>
#include <brdwin.hxx>
#include <salframe.hxx>

namespace vcl
{
static bool lcl_ApplyStyleChange(WindowImpl* pImpl, WinBits nStyle)
{
    if (!pImpl || pImpl->mnStyle == nStyle)
        return false;

    pImpl->mnPrevStyle = pImpl->mnStyle;
    pImpl->mnStyle = nStyle;

    return true;
}

void Window::SetStyle(WinBits nStyle)
{
    if (lcl_ApplyStyleChange(mpWindowImpl.get(), nStyle))
        CompatStateChanged(StateChangedType::Style);
}

static SalExtStyle lcl_GetExtendedStyle(WindowExtendedStyle nExtendedStyle)
{
    SalExtStyle nExt = 0;

    if (nExtendedStyle & WindowExtendedStyle::Document)
        nExt |= SAL_FRAME_EXT_STYLE_DOCUMENT;

    if (nExtendedStyle & WindowExtendedStyle::DocModified)
        nExt |= SAL_FRAME_EXT_STYLE_DOCMODIFIED;

    return nExt;
}

void Window::SetExtendedStyle(WindowExtendedStyle nExtendedStyle)
{
    if (mpWindowImpl->mnExtendedStyle == nExtendedStyle)
        return;

    vcl::Window* pWindow = ImplGetBorderWindow();

    if (!pWindow)
        pWindow = this;

    if (pWindow->mpWindowImpl->mbFrame)
        pWindow->ImplGetFrame()->SetExtendedFrameStyle(lcl_GetExtendedStyle(nExtendedStyle));

    mpWindowImpl->mnExtendedStyle = nExtendedStyle;
}

bool Window::ImplShouldHaveBorder(WindowBorderStyle nBorderStyle)
{
    return nBorderStyle != WindowBorderStyle::REMOVEBORDER
           || mpHierarchy->mpBorderWindow->mpWindowImpl->mbFrame
           || !mpHierarchy->mpBorderWindow->mpHierarchy->mpParent;
}

void Window::ImplSetBorderWindowStyle(WindowBorderStyle nBorderStyle)
{
    vcl::Window* pBorderWindow = mpHierarchy->mpBorderWindow.get();

    if (pBorderWindow->GetType() == WindowType::BORDERWINDOW)
        static_cast<ImplBorderWindow*>(pBorderWindow)->SetBorderStyle(nBorderStyle);
    else
        pBorderWindow->SetBorderStyle(nBorderStyle);
}

void Window::SetBorderStyle(WindowBorderStyle nBorderStyle)
{
    if (!mpHierarchy->mpBorderWindow)
        return;

    if (ImplShouldHaveBorder(nBorderStyle))
    {
        ImplSetBorderWindowStyle(nBorderStyle);
        return;
    }

    // this is a little awkward: some controls (e.g. svtools ProgressBar)
    // cannot avoid getting constructed with WB_BORDER but want to disable
    // borders in case of NWF drawing. So they need a method to remove their border window
    VclPtr<vcl::Window> pBorderWin = mpHierarchy->mpBorderWindow;

    // remove us as border window's client
    pBorderWin->mpHierarchy->mpClientWindow = nullptr;
    mpHierarchy->mpBorderWindow = nullptr;
    mpHierarchy->mpRealParent = pBorderWin->mpHierarchy->mpParent;

    // reparent us above the border window
    SetParent(pBorderWin->mpHierarchy->mpParent);

    // set us to the position and size of our previous border
    Point aBorderPos(pBorderWin->GetPosPixel());
    Size aBorderSize(pBorderWin->GetSizePixel());
    setPosSizePixel(aBorderPos.X(), aBorderPos.Y(), aBorderSize.Width(), aBorderSize.Height());

    // release border window
    pBorderWin.disposeAndClear();

    // set new style bits
    SetStyle(GetStyle() & (~WB_BORDER));
}

WindowBorderStyle Window::GetBorderStyle() const
{
    if (!mpHierarchy->mpBorderWindow)
        return WindowBorderStyle::NONE;

    if (mpHierarchy->mpBorderWindow->GetType() == WindowType::BORDERWINDOW)
        return static_cast<ImplBorderWindow*>(mpHierarchy->mpBorderWindow.get())->GetBorderStyle();

    return mpHierarchy->mpBorderWindow->GetBorderStyle();
}

void Window::GetBorder(sal_Int32& rLeftBorder, sal_Int32& rTopBorder, sal_Int32& rRightBorder,
                       sal_Int32& rBottomBorder) const
{
    rLeftBorder = mpGeometry->mnLeftBorder;
    rTopBorder = mpGeometry->mnTopBorder;
    rRightBorder = mpGeometry->mnRightBorder;
    rBottomBorder = mpGeometry->mnBottomBorder;
}

bool Window::ImplShouldFallbackToParentBackground(const Wallpaper& rBack) const
{
    return !rBack.IsBitmap() && !rBack.IsGradient() && rBack.GetColor() == COL_TRANSPARENT
           && mpHierarchy->mpParent;
}

const Wallpaper& Window::GetDisplayBackground() const
{
    // FIXME: fix issue 52349, need to fix this really in
    // all NWF enabled controls
    if (const ToolBox* pTB = dynamic_cast<const ToolBox*>(this); pTB && IsNativeWidgetEnabled())
        return pTB->ImplGetToolBoxPrivateData()->maDisplayBackground;

    if (!IsBackground() && mpHierarchy->mpParent)
        return mpHierarchy->mpParent->GetDisplayBackground();

    const Wallpaper& rBack = GetBackground();

    if (ImplShouldFallbackToParentBackground(rBack))
        return mpHierarchy->mpParent->GetDisplayBackground();

    return rBack;
}

void Window::EnableNativeWidget(bool bEnable)
{
    static const char* pNoNWF = getenv("SAL_NO_NWF");

    if (pNoNWF && *pNoNWF)
        bEnable = false;

    ImplUpdateNativeWidgetState(bEnable);
    ImplEnableChildNativeWidgets(bEnable);
}

void Window::ImplUpdateNativeWidgetState(bool bEnable)
{
    if (ImplGetWinData()->mbEnableNativeWidget == bEnable)
        return;

    ImplGetWinData()->mbEnableNativeWidget = bEnable;

    // send datachanged event to allow for internal changes required for NWF
    // like clipmode, transparency, etc.
    DataChangedEvent aDCEvt(DataChangedEventType::SETTINGS, &*GetOutDev()->moSettings,
                            AllSettingsFlags::STYLE);
    CompatDataChanged(aDCEvt);

    // sometimes the borderwindow is queried, so keep it in sync
    if (mpHierarchy->mpBorderWindow)
        mpHierarchy->mpBorderWindow->ImplGetWinData()->mbEnableNativeWidget = bEnable;
}

void Window::ImplEnableChildNativeWidgets(bool bEnable)
{
    // push down, useful for compound controls
    for (VclPtr<vcl::Window> pChild = mpHierarchy->mpFirstChild; pChild != nullptr;
         pChild = pChild->mpHierarchy->mpNext)
    {
        pChild->EnableNativeWidget(bEnable);
    }
}

bool Window::IsNativeWidgetEnabled() const
{
    return mpWindowImpl && ImplGetWinData()->mbEnableNativeWidget;
}

void Window::ApplySettings(vcl::RenderContext& /*rRenderContext*/) {}

void Window::SetPointFont(vcl::RenderContext& rRenderContext, const vcl::Font& rFont,
                          bool bUseRenderContextDPI)
{
    vcl::Font aFont = rFont;
    ImplPointToLogic(rRenderContext, aFont, bUseRenderContextDPI);
    rRenderContext.SetFont(aFont);
}

vcl::Font Window::GetPointFont(vcl::RenderContext const& rRenderContext) const
{
    vcl::Font aFont = rRenderContext.GetFont();
    ImplLogicToPoint(rRenderContext, aFont);
    return aFont;
}

void Window::SetCursor(vcl::Cursor* pCursor)
{
    if (!mpControlAppearance)
        return;

    if (mpControlAppearance->mpCursor == pCursor)
        return;

    if (mpControlAppearance->mpCursor)
        mpControlAppearance->mpCursor->ImplHide();

    mpControlAppearance->mpCursor = pCursor;

    if (pCursor)
        pCursor->ImplShow();
}

void Window::ImplPointToLogic(vcl::RenderContext const& rRenderContext, vcl::Font& rFont,
                              bool bUseRenderContextDPI) const
{
    Size aSize = rFont.GetFontSize();

    if (aSize.Width())
    {
        aSize.setWidth(aSize.Width()
                       * (bUseRenderContextDPI ? rRenderContext.GetDPIX()
                                               : mpWindowImpl->mpFrameData->mnDPIX));
        aSize.AdjustWidth(72 / 2);
        aSize.setWidth(aSize.Width() / 72);
    }

    aSize.setHeight(
        aSize.Height()
        * (bUseRenderContextDPI ? rRenderContext.GetDPIY() : mpWindowImpl->mpFrameData->mnDPIY));
    aSize.AdjustHeight(72 / 2);
    aSize.setHeight(aSize.Height() / 72);

    aSize = rRenderContext.convertTo<vcl::LogicSize>(vcl::WindowSize(aSize));

    rFont.SetFontSize(aSize);
}

void Window::ImplLogicToPoint(vcl::RenderContext const& rRenderContext, vcl::Font& rFont) const
{
    auto aSize = rRenderContext.convertTo<vcl::WindowSize>(vcl::LogicSize(rFont.GetFontSize()),
                                                           rRenderContext.GetMapMode());

    if (aSize->Width())
    {
        aSize->setWidth(aSize->Width() * 72);
        aSize->AdjustWidth(mpWindowImpl->mpFrameData->mnDPIX / 2);
        aSize->setWidth(aSize->Width() / mpWindowImpl->mpFrameData->mnDPIX);
    }

    aSize->setHeight(aSize->Height() * 72);
    aSize->AdjustHeight(mpWindowImpl->mpFrameData->mnDPIY / 2);
    aSize->setHeight(aSize->Height() / mpWindowImpl->mpFrameData->mnDPIY);

    rFont.SetFontSize(aSize.get());
}

void Window::SetControlFont()
{
    if (mpControlAppearance && mpControlAppearance->mpControlFont)
    {
        mpControlAppearance->mpControlFont.reset();
        CompatStateChanged(StateChangedType::ControlFont);
    }
}

void Window::SetControlFont(const vcl::Font& rFont)
{
    if (rFont == vcl::Font())
    {
        SetControlFont();
        return;
    }

    if (!mpControlAppearance)
        return;

    if (mpControlAppearance->mpControlFont)
    {
        if (*mpControlAppearance->mpControlFont == rFont)
            return;

        *mpControlAppearance->mpControlFont = rFont;
    }
    else
    {
        mpControlAppearance->mpControlFont = rFont;
    }

    CompatStateChanged(StateChangedType::ControlFont);
}

vcl::Font Window::GetControlFont() const
{
    if (mpControlAppearance && mpControlAppearance->mpControlFont)
        return *mpControlAppearance->mpControlFont;

    vcl::Font aFont;
    return aFont;
}

void Window::ApplyControlFont(vcl::RenderContext& rRenderContext, const vcl::Font& rFont)
{
    vcl::Font aFont(rFont);
    if (IsControlFont())
        aFont.Merge(GetControlFont());
    SetZoomedPointFont(rRenderContext, aFont);
}

void Window::SetControlForeground()
{
    if (mpControlAppearance && mpControlAppearance->mbControlForeground)
    {
        mpControlAppearance->maControlForeground = COL_TRANSPARENT;
        mpControlAppearance->mbControlForeground = false;
        CompatStateChanged(StateChangedType::ControlForeground);
    }
}

void Window::SetControlForeground(const Color& rColor)
{
    if (rColor.IsTransparent())
    {
        if (mpControlAppearance && mpControlAppearance->mbControlForeground)
        {
            mpControlAppearance->maControlForeground = COL_TRANSPARENT;
            mpControlAppearance->mbControlForeground = false;
            CompatStateChanged(StateChangedType::ControlForeground);
        }
    }
    else
    {
        if (mpControlAppearance && mpControlAppearance->maControlForeground != rColor)
        {
            mpControlAppearance->maControlForeground = rColor;
            mpControlAppearance->mbControlForeground = true;
            CompatStateChanged(StateChangedType::ControlForeground);
        }
    }
}

void Window::ApplyControlForeground(vcl::RenderContext& rRenderContext, const Color& rDefaultColor)
{
    Color aTextColor(rDefaultColor);
    if (IsControlForeground())
        aTextColor = GetControlForeground();
    rRenderContext.SetTextColor(aTextColor);
}

void Window::SetControlBackground()
{
    if (mpControlAppearance && mpControlAppearance->mbControlBackground)
    {
        mpControlAppearance->maControlBackground = COL_TRANSPARENT;
        mpControlAppearance->mbControlBackground = false;
        CompatStateChanged(StateChangedType::ControlBackground);
    }
}

void Window::SetControlBackground(const Color& rColor)
{
    if (!mpControlAppearance)
        return;

    if (rColor.IsTransparent())
    {
        if (mpControlAppearance->mbControlBackground)
        {
            mpControlAppearance->maControlBackground = COL_TRANSPARENT;
            mpControlAppearance->mbControlBackground = false;
            CompatStateChanged(StateChangedType::ControlBackground);
        }
    }
    else
    {
        if (mpControlAppearance->maControlBackground != rColor)
        {
            mpControlAppearance->maControlBackground = rColor;
            mpControlAppearance->mbControlBackground = true;
            CompatStateChanged(StateChangedType::ControlBackground);
        }
    }
}

void Window::ApplyControlBackground(vcl::RenderContext& rRenderContext, const Color& rDefaultColor)
{
    Color aColor(rDefaultColor);
    if (IsControlBackground())
        aColor = GetControlBackground();
    rRenderContext.SetBackground(aColor);
}

WinBits Window::GetStyle() const { return mpWindowImpl ? mpWindowImpl->mnStyle : 0; }

WinBits Window::GetPrevStyle() const { return mpWindowImpl ? mpWindowImpl->mnPrevStyle : 0; }

WindowExtendedStyle Window::GetExtendedStyle() const
{
    return mpWindowImpl ? mpWindowImpl->mnExtendedStyle : WindowExtendedStyle::NONE;
}
} // end vcl namespace

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
