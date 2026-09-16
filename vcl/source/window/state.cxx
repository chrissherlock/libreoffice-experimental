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

#include <window/state/ImplFrameData.hxx>
#include <window/state/WindowClassification.hxx>
#include <window/state/WindowPointerState.hxx>
#include <window/state/WindowA11y.hxx>
#include <window/state/WindowStyleState.hxx>
#include <window/state/WindowVisibilityState.hxx>
#include <window/state/WindowFocusState.hxx>
#include <window/state/WindowInput.hxx>
#include <window/state/WindowInvalidation.hxx>
#include <window/state/WindowHierarchy.hxx>
#include <window/state/WindowControlAppearance.hxx>
#include <window/state/WindowControlState.hxx>
#include <window/state/WindowGeometry.hxx>
#include <window/state/WindowPlatformState.hxx>
#include <window/subclasses/decorations/brdwin.hxx>
#include <salframe.hxx>
#include <salobj.hxx>
#include <svdata.hxx>

namespace vcl
{
vcl::Window* Window::ImplGetMenuBarWindow() const
{
    const vcl::Window* pBorderWindow = mpHierarchy ? mpHierarchy->getBorderWindow() : nullptr;
    if (pBorderWindow && pBorderWindow->GetType() == WindowType::BORDERWINDOW)
        return static_cast<const ImplBorderWindow*>(pBorderWindow)->mpMenuBarWindow;

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
    if (vcl::Window* pBorderWindow = mpHierarchy ? mpHierarchy->getBorderWindow() : nullptr)
    {
        pBorderWindow->Enable(bEnable, false);

        if (vcl::Window* pMenuBarWindow = ImplGetMenuBarWindow())
            pMenuBarWindow->Enable(bEnable);
    }
}

void Window::ImplEnableInputBorderAndMenuBar(bool bEnable)
{
    if (vcl::Window* pBorderWindow = mpHierarchy ? mpHierarchy->getBorderWindow() : nullptr)
    {
        pBorderWindow->EnableInput(bEnable, false);

        if (vcl::Window* pMenuBarWindow = ImplGetMenuBarWindow())
            pMenuBarWindow->EnableInput(bEnable);
    }
}

void Window::ImplUpdateEnableState(bool bEnable)
{
    if (mpInput->isWindowEnabled() == bEnable)
        return;

    if (bEnable)
        mpInput->enableWindow();
    else
        mpInput->disableWindow();

    if (mpPlatformState->mpSysObj)
        mpPlatformState->mpSysObj->Enable(bEnable && mpInput->isEnabled());

    CompatStateChanged(StateChangedType::Enable);

    CallEventListeners(bEnable ? VclEventId::WindowEnabled : VclEventId::WindowDisabled);
}

void Window::ImplUpdateInputEnableState(bool bEnable)
{
    if (mpInput->isEnabled() == bEnable)
        return;

    if (bEnable)
        mpInput->enable();
    else
        mpInput->disable();

    if (mpPlatformState->mpSysObj)
        mpPlatformState->mpSysObj->Enable(mpInput->isWindowEnabled() && bEnable);
}

void Window::ImplEnableChildWindows(bool bEnable)
{
    if (vcl::Window* pFirstChild = mpHierarchy ? mpHierarchy->getFirstChild() : nullptr)
    {
        WindowHierarchy::forEachSibling(pFirstChild, [bEnable](vcl::Window& rChild) {
            rChild.Enable(bEnable, true);
        });
    }
}

void Window::ImplEnableInputChildWindows(bool bEnable)
{
    if (vcl::Window* pFirstChild = mpHierarchy ? mpHierarchy->getFirstChild() : nullptr)
    {
        WindowHierarchy::forEachSibling(pFirstChild, [bEnable](vcl::Window& rChild) {
            rChild.EnableInput(bEnable, true);
        });
    }
}

void Window::ImplAlwaysEnableInputChildWindows(bool bAlways)
{
    if (vcl::Window* pFirstChild = mpHierarchy ? mpHierarchy->getFirstChild() : nullptr)
    {
        WindowHierarchy::forEachSibling(pFirstChild, [bAlways](vcl::Window& rChild) {
            rChild.AlwaysEnableInput(bAlways, true);
        });
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
    if (!bEnable && mpInput->isAlwaysInputEnabled())
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
    if (!pFirstOverlap)
        return;

    vcl::Window* pFrameWindow = mpHierarchy ? mpHierarchy->getFrameWindow() : nullptr;
    WindowPlatformState* pPlatformState = pFrameWindow ? pFrameWindow->ImplGetPlatformState() : nullptr;
    ImplFrameData* pFrameData = pPlatformState ? pPlatformState->mpFrameData : nullptr;

    if (!pFrameData)
        return;

    WindowHierarchy::forEachOverlap(pFrameData->mpFirstOverlap, [&](vcl::Window& rSysWin) {
        // Skip if Window is not in the path from this window
        if (!pFirstOverlap->ImplIsWindowOrChild(&rSysWin, true))
            return;

        // Skip if Window is in the exclude window path
        if (pExcludeWindow && pExcludeWindow->ImplIsWindowOrChild(&rSysWin, true))
            return;

        rSysWin.EnableInput(bEnable);
    });
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
    if (vcl::Window* pBorderWindow = mpHierarchy ? mpHierarchy->getBorderWindow() : nullptr)
        pBorderWindow->AlwaysEnableInput(bAlways, false);

    if (mpInput)
    {
        if (bAlways && !mpInput->isAlwaysInputEnabled())
        {
            mpInput->setAlwaysInput(true);
            EnableInput(true, false);
        }
        else if (!bAlways && mpInput->isAlwaysInputEnabled())
        {
            mpInput->setAlwaysInput(false);
        }
    }

    if (bChild)
        ImplAlwaysEnableInputChildWindows(bAlways);
}

void Window::SetActivateMode(ActivateModeFlags nMode)
{
    if (vcl::Window* pBorderWindow = mpHierarchy ? mpHierarchy->getBorderWindow() : nullptr)
        pBorderWindow->SetActivateMode(nMode);

    if (!mpFocusState || !mpFocusState->setActivateMode(nMode))
        return;

    // Possibly trigger Deactivate/Activate
    if (mpFocusState->canGrabFocusOnActivate()
        && (mpFocusState->isActive() || GetType() == WindowType::BORDERWINDOW)
        && !HasChildPathFocus(true))
    {
        mpFocusState->setActive(false);
        Deactivate();
        return;
    }

    if (mpFocusState->isActive() && (GetType() != WindowType::BORDERWINDOW))
        return;

    mpFocusState->setActive(true);
    Activate();
}

void Window::SetUpdateMode(bool bUpdate)
{
    if (mpClassification)
    {
        mpInvalidation->suppressUpdates(!bUpdate);
        CompatStateChanged(StateChangedType::UpdateMode);
    }
}

// --------- old inline methods ---------------

vcl::Window* Window::ImplGetBorderWindow() const
{
    return mpHierarchy ? mpHierarchy->getBorderWindow() : nullptr;
}

void Window::ImplSetMouseTransparent(bool bTransparent)
{
    if (!mpInput)
        return;

    if (bTransparent)
        mpInput->makeMouseTransparent();
    else
        mpInput->makeMouseOpaque();
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
    mpInvalidation->setChildTransparent(bEnable);
}

bool Window::IsChildTransparentModeEnabled() const { return mpInvalidation->isChildTransparent(); }

bool Window::IsMouseTransparent() const { return mpInput && mpInput->isMouseTransparent(); }

bool Window::IsPaintTransparent() const { return mpInvalidation->isPaintTransparent(); }

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

const InputContext& Window::GetInputContext() const { return mpInput->getInputContext(); }

bool Window::HasControlFont() const { return mpControlAppearance->hasControlFont(); }

const Color& Window::GetControlForeground() const
{
    return mpControlAppearance->getControlForeground();
}

bool Window::HasControlForeground() const { return mpControlAppearance->hasControlForeground(); }

const Color& Window::GetControlBackground() const
{
    return mpControlAppearance->getControlBackground();
}

bool Window::HasControlBackground() const { return mpControlAppearance->hasControlBackground(); }

bool Window::IsInPaint() const { return mpInvalidation->isInPaint(); }

bool Window::IsVisible() const { return mpVisibilityState && mpVisibilityState->mbVisible; }

bool Window::IsReallyVisible() const
{
    return mpVisibilityState && mpVisibilityState->mbReallyVisible;
}

bool Window::IsReallyShown() const { return mpVisibilityState && mpVisibilityState->mbReallyShown; }

bool Window::IsInInitShow() const { return mpVisibilityState->mbInInitShow; }

bool Window::IsEnabled() const { return mpInput && mpInput->isWindowEnabled(); }

bool Window::IsInputEnabled() const { return mpInput && mpInput->isEnabled(); }

bool Window::IsAlwaysEnableInput() const { return mpInput && mpInput->isAlwaysInputEnabled(); }

bool Window::IsAlwaysOnTopEnabled() const { return mpClassification->mbAlwaysOnTop; }

void Window::EnablePaint(bool bEnable)
{
    if (bEnable)
        mpInvalidation->enablePaint();
    else
        mpInvalidation->disablePaint();
}

bool Window::IsPaintEnabled() const { return !mpInvalidation->isPaintDisabled(); }

bool Window::IsUpdateMode() const { return !mpInvalidation->isUpdateSuppressed(); }

void Window::SetParentUpdateMode(bool bUpdate) { mpInvalidation->suppressParentUpdates(!bUpdate); }

ActivateModeFlags Window::GetActivateMode() const { return mpFocusState->getActivateMode(); }

bool Window::IsActive() const { return mpFocusState->isActive(); }

GetFocusFlags Window::GetGetFocusFlags() const { return mpFocusState->getFocusFlags(); }

bool Window::IsCompoundControl() const { return mpFocusState->isCompoundControl(); }

bool Window::IsWait() const { return (mpPointerState->mnWaitCount != 0); }

vcl::Cursor* Window::GetCursor() const { return mpControlAppearance->getCursor(); }

bool Window::IsCreatedWithToolkit() const { return mpClassification->mbCreatedWithToolkit; }

void Window::SetCreatedWithToolkit(bool b) { mpClassification->mbCreatedWithToolkit = b; }

PointerStyle Window::GetPointer() const { return mpControlAppearance->getPointer(); }

VCLXWindow* Window::GetWindowPeer() const
{
    return mpAccessibleData ? mpAccessibleData->getVCLXWindow() : nullptr;
}

} /* namespace vcl */

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
