/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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

#include <sal/config.h>
#include <comphelper/diagnose_ex.hxx>

#include <vcl/event.hxx>
#include <vcl/sysdata.hxx>
#include <vcl/syswin.hxx>
#include <vcl/vclevent.hxx>
#include <vcl/window.hxx>

#include <window.h>
#include <brdwin.hxx>
#include <salframe.hxx>
#include <salobj.hxx>
#include <svdata.hxx>

namespace vcl
{
vcl::Window* Window::ImplGetMenuBarWindow() const
{
    if (mpWindowImpl->mpBorderWindow
        && mpWindowImpl->mpBorderWindow->GetType() == WindowType::BORDERWINDOW)
        return static_cast<ImplBorderWindow*>(mpWindowImpl->mpBorderWindow.get())->mpMenuBarWindow;

    return nullptr;
}

void Window::ImplRestoreAppFocusWin()
{
    // #i56102# restore app focus win in case the
    // window was disabled when the frame focus changed
    ImplSVData* pSVData = ImplGetSVData();

    if (pSVData->mpWinData->mpFocusWin == nullptr && mpWindowImpl->mpFrameData->mbHasFocus
        && mpWindowImpl->mpFrameData->mpFocusWin == this)
    {
        pSVData->mpWinData->mpFocusWin = this;
    }
}

void Window::ImplCancelTrackingAndPassFocus()
{
    // the tracking mode will be stopped or the capture will be stolen
    // when a window is disabled,
    if (IsTracking())
        EndTracking(TrackingEventFlags::Cancel);

    if (IsMouseCaptured())
        ReleaseMouse();

    // try to pass focus to the next control
    // if the window has focus and is contained in the dialog control
    // mpWindowImpl->mbDisabled should only be set after a call of ImplDlgCtrlNextWindow().
    // Otherwise ImplDlgCtrlNextWindow() should be used
    if (HasFocus())
        ImplDlgCtrlNextWindow();
}

void Window::ImplEnableBorderAndMenuBar(bool bEnable)
{
    if (mpWindowImpl->mpBorderWindow)
    {
        mpWindowImpl->mpBorderWindow->Enable(bEnable, false);

        if (vcl::Window* pMenuBarWindow = ImplGetMenuBarWindow())
            pMenuBarWindow->Enable(bEnable);
    }
}

void Window::ImplEnableInputBorderAndMenuBar(bool bEnable)
{
    if (mpWindowImpl->mpBorderWindow)
    {
        mpWindowImpl->mpBorderWindow->EnableInput(bEnable, false);

        if (vcl::Window* pMenuBarWindow = ImplGetMenuBarWindow())
            pMenuBarWindow->EnableInput(bEnable);
    }
}

void Window::ImplUpdateEnableState(bool bEnable)
{
    if (mpWindowImpl->mbDisabled == !bEnable)
        return;

    mpWindowImpl->mbDisabled = !bEnable;

    if (mpWindowImpl->mpSysObj)
        mpWindowImpl->mpSysObj->Enable(bEnable && !mpWindowImpl->mbInputDisabled);

    CompatStateChanged(StateChangedType::Enable);

    CallEventListeners(bEnable ? VclEventId::WindowEnabled : VclEventId::WindowDisabled);
}

void Window::ImplUpdateInputEnableState(bool bEnable)
{
    if (mpWindowImpl->mbInputDisabled == !bEnable)
        return;

    mpWindowImpl->mbInputDisabled = !bEnable;

    if (mpWindowImpl->mpSysObj)
        mpWindowImpl->mpSysObj->Enable(!mpWindowImpl->mbDisabled && bEnable);
}

void Window::ImplEnableChildWindows(bool bEnable)
{
    VclPtr<vcl::Window> pChild = mpWindowImpl->mpHierarchy->mpFirstChild;
    while (pChild)
    {
        pChild->Enable(bEnable, true);
        pChild = pChild->mpWindowImpl->mpHierarchy->mpNext;
    }
}

void Window::ImplEnableInputChildWindows(bool bEnable)
{
    VclPtr<vcl::Window> pChild = mpWindowImpl->mpHierarchy->mpFirstChild;
    while (pChild)
    {
        pChild->EnableInput(bEnable, true);
        pChild = pChild->mpWindowImpl->mpHierarchy->mpNext;
    }
}

void Window::ImplAlwaysEnableInputChildWindows(bool bAlways)
{
    VclPtr<vcl::Window> pChild = mpWindowImpl->mpHierarchy->mpFirstChild;
    while (pChild)
    {
        pChild->AlwaysEnableInput(bAlways, true);
        pChild = pChild->mpWindowImpl->mpHierarchy->mpNext;
    }
}

void Window::Enable(bool bEnable, bool bChild)
{
    if (isDisposed())
        return;

    if (!bEnable)
        ImplCancelTrackingAndPassFocus();

    ImplEnableBorderAndMenuBar(bEnable);

    if (bEnable)
        ImplRestoreAppFocusWin();

    ImplUpdateEnableState(bEnable);

    if (bChild)
        ImplEnableChildWindows(bEnable);

    if (IsReallyVisible())
        ImplGenerateMouseMove();
}

void Window::ImplCancelTracking()
{
    // the tracking mode will be stopped or the capture will be stolen
    // when a window is disabled,
    if (IsTracking())
        EndTracking(TrackingEventFlags::Cancel);

    if (IsMouseCaptured())
        ReleaseMouse();
}

void Window::ImplSetInputState(bool bEnable)
{
    if (!bEnable && mpWindowImpl->meAlwaysInputMode == AlwaysInputEnabled)
        return;

    if (!bEnable)
        ImplCancelTracking();

    ImplUpdateInputEnableState(bEnable);
}

void Window::EnableInput(bool bEnable, bool bChild)
{
    if (!mpWindowImpl)
        return;

    ImplEnableInputBorderAndMenuBar(bEnable);

    ImplSetInputState(bEnable);

    if (bEnable)
        ImplRestoreAppFocusWin();

    if (bChild)
        ImplEnableInputChildWindows(bEnable);

    if (IsReallyVisible())
        ImplGenerateMouseMove();
}

void Window::EnableInput(bool bEnable, const vcl::Window* pExcludeWindow)
{
    if (!mpWindowImpl)
        return;

    EnableInput(bEnable);

    // pExecuteWindow is the first Overlap-Frame --> if this
    // shouldn't be the case, then this must be changed in dialog.cxx
    if (pExcludeWindow)
        pExcludeWindow = pExcludeWindow->ImplGetFirstOverlapWindow();
    vcl::Window* pSysWin = mpWindowImpl->mpFrameWindow->mpWindowImpl->mpFrameData->mpFirstOverlap;
    while (pSysWin)
    {
        // Is Window in the path from this window
        if (ImplGetFirstOverlapWindow()->ImplIsWindowOrChild(pSysWin, true))
        {
            // Is Window not in the exclude window path or not the
            // exclude window, then change the status
            if (!pExcludeWindow || !pExcludeWindow->ImplIsWindowOrChild(pSysWin, true))
                pSysWin->EnableInput(bEnable);
        }
        pSysWin = pSysWin->mpWindowImpl->mpHierarchy->mpNextOverlap;
    }

    // enable/disable floating system windows as well
    vcl::Window* pFrameWin = ImplGetSVData()->maFrameData.mpFirstFrame;
    while (pFrameWin)
    {
        if (pFrameWin->ImplIsFloatingWindow())
        {
            // Is Window in the path from this window
            if (ImplGetFirstOverlapWindow()->ImplIsWindowOrChild(pFrameWin, true))
            {
                // Is Window not in the exclude window path or not the
                // exclude window, then change the status
                if (!pExcludeWindow || !pExcludeWindow->ImplIsWindowOrChild(pFrameWin, true))
                    pFrameWin->EnableInput(bEnable);
            }
        }
        pFrameWin = pFrameWin->mpWindowImpl->mpFrameData->mpNextFrame;
    }

    // the same for ownerdraw floating windows
    if (!mpWindowImpl->mbFrame)
        return;

    ::std::vector<VclPtr<vcl::Window>>& rList = mpWindowImpl->mpFrameData->maOwnerDrawList;
    for (auto const& elem : rList)
    {
        // Is Window in the path from this window
        if (ImplGetFirstOverlapWindow()->ImplIsWindowOrChild(elem, true))
        {
            // Is Window not in the exclude window path or not the
            // exclude window, then change the status
            if (!pExcludeWindow || !pExcludeWindow->ImplIsWindowOrChild(elem, true))
                elem->EnableInput(bEnable);
        }
    }
}

void Window::AlwaysEnableInput(bool bAlways, bool bChild)
{
    if (mpWindowImpl->mpBorderWindow)
        mpWindowImpl->mpBorderWindow->AlwaysEnableInput(bAlways, false);

    if (bAlways && mpWindowImpl->meAlwaysInputMode != AlwaysInputEnabled)
    {
        mpWindowImpl->meAlwaysInputMode = AlwaysInputEnabled;
        EnableInput(true, false);
    }
    else if (!bAlways && mpWindowImpl->meAlwaysInputMode == AlwaysInputEnabled)
    {
        mpWindowImpl->meAlwaysInputMode = AlwaysInputNone;
    }

    if (bChild)
        ImplAlwaysEnableInputChildWindows(bAlways);
}

void Window::SetActivateMode(ActivateModeFlags nMode)
{
    if (mpWindowImpl->mpBorderWindow)
        mpWindowImpl->mpBorderWindow->SetActivateMode(nMode);

    if (mpWindowImpl->mnActivateMode == nMode)
        return;

    mpWindowImpl->mnActivateMode = nMode;

    // possibly trigger Deactivate/Activate
    if (mpWindowImpl->mnActivateMode != ActivateModeFlags::NONE)
    {
        if ((mpWindowImpl->mbActive || (GetType() == WindowType::BORDERWINDOW))
            && !HasChildPathFocus(true))
        {
            mpWindowImpl->mbActive = false;
            Deactivate();
        }
    }
    else
    {
        if (!mpWindowImpl->mbActive || (GetType() == WindowType::BORDERWINDOW))
        {
            mpWindowImpl->mbActive = true;
            Activate();
        }
    }
}

void Window::SetUpdateMode(bool bUpdate)
{
    if (mpWindowImpl)
    {
        mpWindowImpl->mbNoUpdate = !bUpdate;
        CompatStateChanged(StateChangedType::UpdateMode);
    }
}

} /* namespace vcl */

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
