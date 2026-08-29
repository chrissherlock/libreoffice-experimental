
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

#include <vcl/cursor.hxx>
#include <vcl/event.hxx>
#include <vcl/toolbox.hxx>
#include <vcl/window.hxx>

#include <toolbox.h>
#include <window.h>
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
           || mpWindowImpl->mpBorderWindow->mpWindowImpl->mbFrame
           || !mpWindowImpl->mpBorderWindow->mpWindowImpl->mpHierarchy->mpParent;
}

void Window::ImplSetBorderWindowStyle(WindowBorderStyle nBorderStyle)
{
    vcl::Window* pBorderWindow = mpWindowImpl->mpBorderWindow.get();

    if (pBorderWindow->GetType() == WindowType::BORDERWINDOW)
        static_cast<ImplBorderWindow*>(pBorderWindow)->SetBorderStyle(nBorderStyle);
    else
        pBorderWindow->SetBorderStyle(nBorderStyle);
}

void Window::SetBorderStyle(WindowBorderStyle nBorderStyle)
{
    if (!mpWindowImpl->mpBorderWindow)
        return;

    if (ImplShouldHaveBorder(nBorderStyle))
    {
        ImplSetBorderWindowStyle(nBorderStyle);
        return;
    }

    // this is a little awkward: some controls (e.g. svtools ProgressBar)
    // cannot avoid getting constructed with WB_BORDER but want to disable
    // borders in case of NWF drawing. So they need a method to remove their border window
    VclPtr<vcl::Window> pBorderWin = mpWindowImpl->mpBorderWindow;

    // remove us as border window's client
    pBorderWin->mpWindowImpl->mpClientWindow = nullptr;
    mpWindowImpl->mpBorderWindow = nullptr;
    mpWindowImpl->mpHierarchy->mpRealParent = pBorderWin->mpWindowImpl->mpHierarchy->mpParent;

    // reparent us above the border window
    SetParent(pBorderWin->mpWindowImpl->mpHierarchy->mpParent);

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
    if (!mpWindowImpl->mpBorderWindow)
        return WindowBorderStyle::NONE;

    if (mpWindowImpl->mpBorderWindow->GetType() == WindowType::BORDERWINDOW)
        return static_cast<ImplBorderWindow*>(mpWindowImpl->mpBorderWindow.get())->GetBorderStyle();

    return mpWindowImpl->mpBorderWindow->GetBorderStyle();
}

void Window::GetBorder(sal_Int32& rLeftBorder, sal_Int32& rTopBorder, sal_Int32& rRightBorder,
                       sal_Int32& rBottomBorder) const
{
    rLeftBorder = mpWindowImpl->mnLeftBorder;
    rTopBorder = mpWindowImpl->mnTopBorder;
    rRightBorder = mpWindowImpl->mnRightBorder;
    rBottomBorder = mpWindowImpl->mnBottomBorder;
}

bool Window::ImplShouldFallbackToParentBackground(const Wallpaper& rBack) const
{
    return !rBack.IsBitmap() && !rBack.IsGradient() && rBack.GetColor() == COL_TRANSPARENT
           && mpWindowImpl->mpHierarchy->mpParent;
}

const Wallpaper& Window::GetDisplayBackground() const
{
    // FIXME: fix issue 52349, need to fix this really in
    // all NWF enabled controls
    if (const ToolBox* pTB = dynamic_cast<const ToolBox*>(this); pTB && IsNativeWidgetEnabled())
        return pTB->ImplGetToolBoxPrivateData()->maDisplayBackground;

    if (!IsBackground() && mpWindowImpl->mpHierarchy->mpParent)
        return mpWindowImpl->mpHierarchy->mpParent->GetDisplayBackground();

    const Wallpaper& rBack = GetBackground();

    if (ImplShouldFallbackToParentBackground(rBack))
        return mpWindowImpl->mpHierarchy->mpParent->GetDisplayBackground();

    return rBack;
}

void Window::EnableNativeWidget(bool bEnable)
{
    static const char* pNoNWF = getenv("SAL_NO_NWF");

    if (pNoNWF && *pNoNWF)
        bEnable = false;

    if (bEnable != ImplGetWinData()->mbEnableNativeWidget)
    {
        ImplGetWinData()->mbEnableNativeWidget = bEnable;

        // send datachanged event to allow for internal changes required for NWF
        // like clipmode, transparency, etc.
        DataChangedEvent aDCEvt(DataChangedEventType::SETTINGS, &*GetOutDev()->moSettings,
                                AllSettingsFlags::STYLE);
        CompatDataChanged(aDCEvt);

        // sometimes the borderwindow is queried, so keep it in sync
        if (mpWindowImpl->mpBorderWindow)
            mpWindowImpl->mpBorderWindow->ImplGetWinData()->mbEnableNativeWidget = bEnable;
    }

    // push down, useful for compound controls
    VclPtr<vcl::Window> pChild = mpWindowImpl->mpHierarchy->mpFirstChild;
    while (pChild)
    {
        pChild->EnableNativeWidget(bEnable);
        pChild = pChild->mpWindowImpl->mpHierarchy->mpNext;
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
    if (mpWindowImpl->mpCursor == pCursor)
        return;

    if (mpWindowImpl->mpCursor)
        mpWindowImpl->mpCursor->ImplHide();

    mpWindowImpl->mpCursor = pCursor;

    if (pCursor)
        pCursor->ImplShow();
}

} // end vcl namespace

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
