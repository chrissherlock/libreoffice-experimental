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

#include <vcl/event.hxx>
#include <vcl/window.hxx>

#include <window.h>
#include <svdata.hxx>

namespace vcl
{
bool Window::ImplHasFocusedChild() const
{
    ImplSVData* pSVData = ImplGetSVData();
    if (!pSVData->mpWinData->mpFocusWin || !IsAncestorOf(*pSVData->mpWinData->mpFocusWin))
        return false;

#if OSL_DEBUG_LEVEL > 0
    OUString aTempStr
        = "Window (" + GetText()
          + ") with focused child window destroyed ! THIS WILL LEAD TO CRASHES AND MUST BE FIXED !";
    SAL_WARN("vcl", aTempStr);
    Application::Abort(aTempStr);
#endif

    return true;
}

bool Window::ImplContainsFocus() const
{
    ImplSVData* pSVData = ImplGetSVData();
    return (pSVData->mpWinData->mpFocusWin == this) || ImplHasFocusedChild();
}

vcl::Window* Window::ImplResetOverlapFocusState(vcl::Window* pOverlapWindow)
{
    ImplSVData* pSVData = ImplGetSVData();

    pSVData->mpWinData->mpFocusWin = nullptr;
    pOverlapWindow->mpWindowImpl->mpLastFocusWindow = nullptr;
    return pOverlapWindow;
}

void Window::ImplTransferFocusToParent()
{
    vcl::Window* pTarget = GetParent();

    // When windows overlap, give focus to the parent of the next FrameWindow
    if (mpWindowImpl->mpBorderWindow)
    {
        if (mpWindowImpl->mpBorderWindow->ImplIsOverlapWindow())
            pTarget = mpWindowImpl->mpBorderWindow->mpWindowImpl->mpOverlapWindow;
    }
    else if (ImplIsOverlapWindow())
    {
        pTarget = mpWindowImpl->mpOverlapWindow;
    }

    if (pTarget && pTarget->IsEnabled() && pTarget->IsInputEnabled() && !pTarget->IsInModalMode())
        pTarget->GrabFocus();
    else
        mpWindowImpl->mpFrameWindow->GrabFocus();
}

vcl::Window* Window::ImplTransferFocus()
{
    vcl::Window* pOverlapWindow = ImplGetFirstOverlapWindow();

    if (!ImplContainsFocus())
        return pOverlapWindow;

    if (mpWindowImpl->mbFrame)
        return ImplResetOverlapFocusState(pOverlapWindow);

    ImplTransferFocusToParent();

    if (HasFocus())
        return ImplResetOverlapFocusState(pOverlapWindow);

    return pOverlapWindow;
}

bool Window::ImplShouldPassFocusToLastWindow() const
{
    return HasFocus() && mpWindowImpl->mpLastFocusWindow
           && !(mpWindowImpl->mnDlgCtrlFlags & DialogControlFlags::WantFocus);
}

void Window::GetFocus()
{
    if (ImplShouldPassFocusToLastWindow())
    {
        // Calling GrabFocus() triggers synchronous events. We must hold a reference
        // to 'this' because an event handler (like a macro or user script) might
        // destroy this parent window during the focus transfer. If that happens,
        // we must bail out immediately to avoid a use-after-free crash.
        VclPtr<vcl::Window> xWindow(this);
        mpWindowImpl->mpLastFocusWindow->GrabFocus();

        if (xWindow->isDisposed())
            return;
    }

    NotifyEvent aNEvt(NotifyEventType::GETFOCUS, this);
    CompatNotify(aNEvt);
}

void Window::LoseFocus()
{
    NotifyEvent aNEvt(NotifyEventType::LOSEFOCUS, this);
    CompatNotify(aNEvt);
}

void Window::GrabFocus() { ImplGrabFocus(GetFocusFlags::NONE); }

bool Window::HasFocus() const { return (this == ImplGetSVData()->mpWinData->mpFocusWin); }

void Window::GrabFocusToDocument() { ImplGrabFocusToDocument(GetFocusFlags::NONE); }

VclPtr<vcl::Window> Window::GetFocusedWindow() const
{
    if (mpWindowImpl && mpWindowImpl->mpFrameData)
        return mpWindowImpl->mpFrameData->mpFocusWin;
    else
        return VclPtr<vcl::Window>();
}

void Window::SetFakeFocus(bool bFocus) { ImplGetWindowImpl()->mbFakeFocusSet = bFocus; }

bool Window::HasChildPathFocus(bool bSystemWindow) const
{
    vcl::Window* pFocusWin = ImplGetSVData()->mpWinData->mpFocusWin;
    if (pFocusWin)
        return ImplIsWindowOrChild(pFocusWin, bSystemWindow);
    return false;
}

/*
 * The rationale here is that we moved destructors to
 * dispose and this altered a lot of code paths, that
 * are better left unchanged for now.
 */
void Window::CompatGetFocus()
{
    if (!mpWindowImpl || mpWindowImpl->mbInDispose)
        Window::GetFocus();
    else
        GetFocus();
}

void Window::CompatLoseFocus()
{
    if (!mpWindowImpl || mpWindowImpl->mbInDispose)
        Window::LoseFocus();
    else
        LoseFocus();
}

} /* namespace vcl */

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
