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
#include <vcl/toolkit/dialog.hxx>
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

void Window::ImplEnableOverlapWindowsInput(bool bEnable, const vcl::Window* pExcludeWindow)
{
    vcl::Window* pFirstOverlap = ImplGetFirstOverlapWindow();

    for (vcl::Window* pSysWin
         = mpWindowImpl->mpFrameWindow->mpWindowImpl->mpFrameData->mpFirstOverlap;
         pSysWin != nullptr; pSysWin = pSysWin->mpWindowImpl->mpHierarchy->mpNextOverlap)
    {
        // Skip if Window is not in the path from this window
        if (!pFirstOverlap->ImplIsWindowOrChild(pSysWin, true))
            continue;

        // Skip if Window is in the exclude window path
        if (pExcludeWindow && pExcludeWindow->ImplIsWindowOrChild(pSysWin, true))
            continue;

        pSysWin->EnableInput(bEnable);
    }
}

void Window::ImplEnableFloatingWindowsInput(bool bEnable, const vcl::Window* pExcludeWindow)
{
    vcl::Window* pFirstOverlap = ImplGetFirstOverlapWindow();

    for (vcl::Window* pFrameWin = ImplGetSVData()->maFrameData.mpFirstFrame; pFrameWin != nullptr;
         pFrameWin = pFrameWin->mpWindowImpl->mpFrameData->mpNextFrame)
    {
        if (!pFrameWin->ImplIsFloatingWindow())
            continue;

        // Skip if Window is not in the path from this window
        if (!pFirstOverlap->ImplIsWindowOrChild(pFrameWin, true))
            continue;

        // Skip if Window is in the exclude window path
        if (pExcludeWindow && pExcludeWindow->ImplIsWindowOrChild(pFrameWin, true))
            continue;

        pFrameWin->EnableInput(bEnable);
    }
}

void Window::ImplEnableOwnerDrawWindowsInput(bool bEnable, const vcl::Window* pExcludeWindow)
{
    if (!mpWindowImpl->mbFrame)
        return;

    vcl::Window* pFirstOverlap = ImplGetFirstOverlapWindow();
    ::std::vector<VclPtr<vcl::Window>>& rList = mpWindowImpl->mpFrameData->maOwnerDrawList;

    for (auto const& elem : rList)
    {
        // Skip if Window is not in the path from this window
        if (!pFirstOverlap->ImplIsWindowOrChild(elem, true))
            continue;

        // Skip if Window is in the exclude window path
        if (pExcludeWindow && pExcludeWindow->ImplIsWindowOrChild(elem, true))
            continue;

        elem->EnableInput(bEnable);
    }
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

    ImplEnableOverlapWindowsInput(bEnable, pExcludeWindow);
    ImplEnableFloatingWindowsInput(bEnable, pExcludeWindow);
    ImplEnableOwnerDrawWindowsInput(bEnable, pExcludeWindow);
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

// --------- old inline methods ---------------

vcl::Window* Window::ImplGetBorderWindow() const
{
    return mpWindowImpl ? mpWindowImpl->mpBorderWindow.get() : nullptr;
}

void Window::ImplSetMouseTransparent(bool bTransparent)
{
    if (mpWindowImpl)
        mpWindowImpl->mbMouseTransparent = bTransparent;
}

bool Window::IsFormControl() const { return mpWindowImpl ? mpWindowImpl->mbIsFormControl : false; }

void Window::SetFormControl(bool bFormControl)
{
    if (mpWindowImpl)
        mpWindowImpl->mbIsFormControl = bFormControl;
}

Dialog* Window::GetParentDialog() const
{
    const vcl::Window* pWindow = this;

    while (pWindow)
    {
        if (pWindow->IsDialog())
            break;

        pWindow = pWindow->GetParent();
    }

    return const_cast<Dialog*>(dynamic_cast<const Dialog*>(pWindow));
}

bool Window::IsMenuFloatingWindow() const
{
    return mpWindowImpl && mpWindowImpl->mbMenuFloatingWindow;
}

bool Window::IsNativeFrame() const
{
    // #101741 do not check for WB_CLOSEABLE because undecorated floaters (like menus!) are closeable
    if (mpWindowImpl->mbFrame && (mpWindowImpl->mnStyle & (WB_MOVEABLE | WB_SIZEABLE)))
        return true;

    return false;
}

void Window::EnableAllResize() { mpWindowImpl->mbAllResize = true; }

void Window::EnableChildTransparentMode(bool bEnable)
{
    mpWindowImpl->mbChildTransparent = bEnable;
}

bool Window::IsChildTransparentModeEnabled() const
{
    return mpWindowImpl && mpWindowImpl->mbChildTransparent;
}

bool Window::IsMouseTransparent() const { return mpWindowImpl && mpWindowImpl->mbMouseTransparent; }

bool Window::IsPaintTransparent() const { return mpWindowImpl && mpWindowImpl->mbPaintTransparent; }

void Window::SetDialogControlStart(bool bStart) { mpWindowImpl->mbDlgCtrlStart = bStart; }

bool Window::IsDialogControlStart() const { return mpWindowImpl && mpWindowImpl->mbDlgCtrlStart; }

void Window::SetDialogControlFlags(DialogControlFlags nFlags)
{
    mpWindowImpl->mnDlgCtrlFlags = nFlags;
}

DialogControlFlags Window::GetDialogControlFlags() const { return mpWindowImpl->mnDlgCtrlFlags; }

const InputContext& Window::GetInputContext() const { return mpWindowImpl->maInputContext; }

bool Window::IsControlFont() const { return bool(mpWindowImpl->mpControlFont); }

const Color& Window::GetControlForeground() const { return mpWindowImpl->maControlForeground; }

bool Window::IsControlForeground() const { return mpWindowImpl->mbControlForeground; }

const Color& Window::GetControlBackground() const { return mpWindowImpl->maControlBackground; }

bool Window::IsControlBackground() const { return mpWindowImpl->mbControlBackground; }

bool Window::IsInPaint() const { return mpWindowImpl && mpWindowImpl->mbInPaint; }

bool Window::IsVisible() const { return mpWindowImpl && mpWindowImpl->mbVisible; }

bool Window::IsReallyVisible() const { return mpWindowImpl && mpWindowImpl->mbReallyVisible; }

bool Window::IsReallyShown() const { return mpWindowImpl && mpWindowImpl->mbReallyShown; }

bool Window::IsInInitShow() const { return mpWindowImpl->mbInInitShow; }

bool Window::IsEnabled() const { return mpWindowImpl && !mpWindowImpl->mbDisabled; }

bool Window::IsInputEnabled() const { return mpWindowImpl && !mpWindowImpl->mbInputDisabled; }

bool Window::IsAlwaysEnableInput() const
{
    return mpWindowImpl->meAlwaysInputMode == AlwaysInputEnabled;
}

ActivateModeFlags Window::GetActivateMode() const { return mpWindowImpl->mnActivateMode; }

bool Window::IsAlwaysOnTopEnabled() const { return mpWindowImpl->mbAlwaysOnTop; }

void Window::EnablePaint(bool bEnable) { mpWindowImpl->mbPaintDisabled = !bEnable; }

bool Window::IsPaintEnabled() const { return !mpWindowImpl->mbPaintDisabled; }

bool Window::IsUpdateMode() const { return !mpWindowImpl->mbNoUpdate; }

void Window::SetParentUpdateMode(bool bUpdate) { mpWindowImpl->mbNoParentUpdate = !bUpdate; }

bool Window::IsActive() const { return mpWindowImpl->mbActive; }

GetFocusFlags Window::GetGetFocusFlags() const { return mpWindowImpl->mnGetFocusFlags; }

bool Window::IsCompoundControl() const { return mpWindowImpl && mpWindowImpl->mbCompoundControl; }

bool Window::IsWait() const { return (mpWindowImpl->mnWaitCount != 0); }

vcl::Cursor* Window::GetCursor() const
{
    if (!mpWindowImpl)
        return nullptr;

    return mpWindowImpl->mpCursor;
}

bool Window::IsCreatedWithToolkit() const { return mpWindowImpl->mbCreatedWithToolkit; }

void Window::SetCreatedWithToolkit(bool b) { mpWindowImpl->mbCreatedWithToolkit = b; }

PointerStyle Window::GetPointer() const { return mpWindowImpl->maPointer; }

VCLXWindow* Window::GetWindowPeer() const
{
    return mpWindowImpl ? mpWindowImpl->mpVCLXWindow : nullptr;
}
} /* namespace vcl */

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
