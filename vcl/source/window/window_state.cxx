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

#include <ImplFrameData.hxx>
#include <WindowClassification.hxx>
#include <WindowPointerState.hxx>
#include <WindowA11y.hxx>
#include <WindowStyleState.hxx>
#include <WindowVisibilityState.hxx>
#include <WindowFocusState.hxx>
#include <WindowInput.hxx>
#include <WindowInvalidation.hxx>
#include <WindowHierarchy.hxx>
#include <WindowControlAppearance.hxx>
#include <WindowControlState.hxx>
#include <WindowGeometry.hxx>
#include <WindowPlatformState.hxx>
#include <brdwin.hxx>
#include <salframe.hxx>
#include <salobj.hxx>
#include <svdata.hxx>

namespace vcl
{
vcl::Window* Window::ImplGetMenuBarWindow() const
{
    if (mpHierarchy->mpBorderWindow
        && mpHierarchy->mpBorderWindow->GetType() == WindowType::BORDERWINDOW)
        return static_cast<ImplBorderWindow*>(mpHierarchy->mpBorderWindow.get())->mpMenuBarWindow;

    return nullptr;
}

void Window::ImplRestoreAppFocusWin()
{
    // #i56102# restore app focus win in case the
    // window was disabled when the frame focus changed
    ImplSVData* pSVData = ImplGetSVData();

    if (pSVData->mpWinData->mpFocusWin == nullptr && mpPlatformState->mpFrameData->mbHasFocus
        && mpPlatformState->mpFrameData->mpFocusWin == this)
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
    // mpClassification->mbDisabled should only be set after a call of ImplDlgCtrlNextWindow().
    // Otherwise ImplDlgCtrlNextWindow() should be used
    if (HasFocus())
        ImplDlgCtrlNextWindow();
}

void Window::ImplEnableBorderAndMenuBar(bool bEnable)
{
    if (mpHierarchy->mpBorderWindow)
    {
        mpHierarchy->mpBorderWindow->Enable(bEnable, false);

        if (vcl::Window* pMenuBarWindow = ImplGetMenuBarWindow())
            pMenuBarWindow->Enable(bEnable);
    }
}

void Window::ImplEnableInputBorderAndMenuBar(bool bEnable)
{
    if (mpHierarchy->mpBorderWindow)
    {
        mpHierarchy->mpBorderWindow->EnableInput(bEnable, false);

        if (vcl::Window* pMenuBarWindow = ImplGetMenuBarWindow())
            pMenuBarWindow->EnableInput(bEnable);
    }
}

void Window::ImplUpdateEnableState(bool bEnable)
{
    if (mpInput->mbDisabled == !bEnable)
        return;

    mpInput->mbDisabled = !bEnable;

    if (mpPlatformState->mpSysObj)
        mpPlatformState->mpSysObj->Enable(bEnable && !mpInput->mbInputDisabled);

    CompatStateChanged(StateChangedType::Enable);

    CallEventListeners(bEnable ? VclEventId::WindowEnabled : VclEventId::WindowDisabled);
}

void Window::ImplUpdateInputEnableState(bool bEnable)
{
    if (mpInput->mbInputDisabled == !bEnable)
        return;

    mpInput->mbInputDisabled = !bEnable;

    if (mpPlatformState->mpSysObj)
        mpPlatformState->mpSysObj->Enable(!mpInput->mbDisabled && bEnable);
}

void Window::ImplEnableChildWindows(bool bEnable)
{
    VclPtr<vcl::Window> pChild = mpHierarchy->mpFirstChild;
    while (pChild)
    {
        pChild->Enable(bEnable, true);
        pChild = pChild->mpHierarchy->mpNext;
    }
}

void Window::ImplEnableInputChildWindows(bool bEnable)
{
    VclPtr<vcl::Window> pChild = mpHierarchy->mpFirstChild;
    while (pChild)
    {
        pChild->EnableInput(bEnable, true);
        pChild = pChild->mpHierarchy->mpNext;
    }
}

void Window::ImplAlwaysEnableInputChildWindows(bool bAlways)
{
    VclPtr<vcl::Window> pChild = mpHierarchy->mpFirstChild;
    while (pChild)
    {
        pChild->AlwaysEnableInput(bAlways, true);
        pChild = pChild->mpHierarchy->mpNext;
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
    if (!bEnable && mpInput->meAlwaysInputMode == AlwaysInputEnabled)
        return;

    if (!bEnable)
        ImplCancelTracking();

    ImplUpdateInputEnableState(bEnable);
}

void Window::EnableInput(bool bEnable, bool bChild)
{
    if (!mpClassification)
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
         = mpHierarchy->mpFrameWindow->mpPlatformState->mpFrameData->mpFirstOverlap;
         pSysWin != nullptr; pSysWin = pSysWin->mpHierarchy->mpNextOverlap)
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
         pFrameWin = pFrameWin->mpPlatformState->mpFrameData->mpNextFrame)
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
    if (!mpClassification->mbFrame)
        return;

    vcl::Window* pFirstOverlap = ImplGetFirstOverlapWindow();
    ::std::vector<VclPtr<vcl::Window>>& rList = mpPlatformState->mpFrameData->maOwnerDrawList;

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
    if (!mpClassification)
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
    if (mpHierarchy->mpBorderWindow)
        mpHierarchy->mpBorderWindow->AlwaysEnableInput(bAlways, false);

    if (bAlways && mpInput->meAlwaysInputMode != AlwaysInputEnabled)
    {
        mpInput->meAlwaysInputMode = AlwaysInputEnabled;
        EnableInput(true, false);
    }
    else if (!bAlways && mpInput->meAlwaysInputMode == AlwaysInputEnabled)
    {
        mpInput->meAlwaysInputMode = AlwaysInputNone;
    }

    if (bChild)
        ImplAlwaysEnableInputChildWindows(bAlways);
}

void Window::SetActivateMode(ActivateModeFlags nMode)
{
    if (mpHierarchy->mpBorderWindow)
        mpHierarchy->mpBorderWindow->SetActivateMode(nMode);

    if (mpFocusState->mnActivateMode == nMode)
        return;

    mpFocusState->mnActivateMode = nMode;

    // possibly trigger Deactivate/Activate
    if (mpFocusState->mnActivateMode != ActivateModeFlags::NONE)
    {
        if ((mpFocusState->mbActive || (GetType() == WindowType::BORDERWINDOW))
            && !HasChildPathFocus(true))
        {
            mpFocusState->mbActive = false;
            Deactivate();
        }
    }
    else
    {
        if (!mpFocusState->mbActive || (GetType() == WindowType::BORDERWINDOW))
        {
            mpFocusState->mbActive = true;
            Activate();
        }
    }
}

void Window::SetUpdateMode(bool bUpdate)
{
    if (mpClassification)
    {
        mpInvalidation->mbNoUpdate = !bUpdate;
        CompatStateChanged(StateChangedType::UpdateMode);
    }
}

// --------- old inline methods ---------------

vcl::Window* Window::ImplGetBorderWindow() const
{
    return mpClassification ? mpHierarchy->mpBorderWindow.get() : nullptr;
}

void Window::ImplSetMouseTransparent(bool bTransparent)
{
    if (mpInput)
        mpInput->mbMouseTransparent = bTransparent;
}

bool Window::IsFormControl() const
{
    return mpClassification ? mpClassification->mbIsFormControl : false;
}

void Window::SetFormControl(bool bFormControl)
{
    if (mpClassification)
        mpClassification->mbIsFormControl = bFormControl;
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
    return mpClassification && mpClassification->mbMenuFloatingWindow;
}

bool Window::IsNativeFrame() const
{
    // #101741 do not check for WB_CLOSEABLE because undecorated floaters (like menus!) are closeable
    if (mpClassification->mbFrame && (mpStyleState->mnStyle & (WB_MOVEABLE | WB_SIZEABLE)))
        return true;

    return false;
}

void Window::EnableAllResize() { mpGeometry->mbAllResize = true; }

void Window::EnableChildTransparentMode(bool bEnable)
{
    mpInvalidation->mbChildTransparent = bEnable;
}

bool Window::IsChildTransparentModeEnabled() const
{
    return mpInvalidation && mpInvalidation->mbChildTransparent;
}

bool Window::IsMouseTransparent() const { return mpInput && mpInput->mbMouseTransparent; }

bool Window::IsPaintTransparent() const
{
    return mpInvalidation && mpInvalidation->mbPaintTransparent;
}

void Window::SetDialogControlStart(bool bStart) { mpControlState->mbDlgCtrlStart = bStart; }

bool Window::IsDialogControlStart() const
{
    return mpControlState && mpControlState->mbDlgCtrlStart;
}

void Window::SetDialogControlFlags(DialogControlFlags nFlags)
{
    mpControlState->mnDlgCtrlFlags = nFlags;
}

DialogControlFlags Window::GetDialogControlFlags() const { return mpControlState->mnDlgCtrlFlags; }

const InputContext& Window::GetInputContext() const { return mpInput->maInputContext; }

bool Window::IsControlFont() const
{
    return bool(mpControlAppearance && mpControlAppearance->mpControlFont);
}

const Color& Window::GetControlForeground() const
{
    return mpControlAppearance->maControlForeground;
}

bool Window::IsControlForeground() const
{
    return mpControlAppearance && mpControlAppearance->mbControlForeground;
}

const Color& Window::GetControlBackground() const
{
    return mpControlAppearance->maControlBackground;
}

bool Window::IsControlBackground() const
{
    return mpControlAppearance && mpControlAppearance->mbControlBackground;
}

bool Window::IsInPaint() const { return mpInvalidation && mpInvalidation->mbInPaint; }

bool Window::IsVisible() const { return mpVisibilityState && mpVisibilityState->mbVisible; }

bool Window::IsReallyVisible() const
{
    return mpVisibilityState && mpVisibilityState->mbReallyVisible;
}

bool Window::IsReallyShown() const { return mpVisibilityState && mpVisibilityState->mbReallyShown; }

bool Window::IsInInitShow() const { return mpVisibilityState->mbInInitShow; }

bool Window::IsEnabled() const { return mpInput && !mpInput->mbDisabled; }

bool Window::IsInputEnabled() const { return mpInput && !mpInput->mbInputDisabled; }

bool Window::IsAlwaysEnableInput() const
{
    return mpInput->meAlwaysInputMode == AlwaysInputEnabled;
}

ActivateModeFlags Window::GetActivateMode() const { return mpFocusState->mnActivateMode; }

bool Window::IsAlwaysOnTopEnabled() const { return mpClassification->mbAlwaysOnTop; }

void Window::EnablePaint(bool bEnable) { mpInvalidation->mbPaintDisabled = !bEnable; }

bool Window::IsPaintEnabled() const { return !mpInvalidation->mbPaintDisabled; }

bool Window::IsUpdateMode() const { return !mpInvalidation->mbNoUpdate; }

void Window::SetParentUpdateMode(bool bUpdate) { mpInvalidation->mbNoParentUpdate = !bUpdate; }

bool Window::IsActive() const { return mpFocusState->mbActive; }

GetFocusFlags Window::GetGetFocusFlags() const { return mpFocusState->mnGetFocusFlags; }

bool Window::IsCompoundControl() const { return mpFocusState && mpFocusState->mbCompoundControl; }

bool Window::IsWait() const { return (mpPointerState->mnWaitCount != 0); }

vcl::Cursor* Window::GetCursor() const
{
    if (!mpControlAppearance)
        return nullptr;

    return mpControlAppearance->mpCursor;
}

bool Window::IsCreatedWithToolkit() const { return mpClassification->mbCreatedWithToolkit; }

void Window::SetCreatedWithToolkit(bool b) { mpClassification->mbCreatedWithToolkit = b; }

PointerStyle Window::GetPointer() const { return mpControlAppearance->mePointer; }

VCLXWindow* Window::GetWindowPeer() const
{
    return mpAccessibleData ? mpAccessibleData->getVCLXWindow() : nullptr;
}

} /* namespace vcl */

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
