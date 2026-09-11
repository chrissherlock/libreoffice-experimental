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
#include <vcl/toolkit/dialog.hxx>
#include <vcl/window.hxx>

#include <ImplFrameData.hxx>
#include <ImplWinData.hxx>
#include <WindowPlatformState.hxx>
#include <WindowFocusState.hxx>
#include <WindowClassification.hxx>
#include <WindowInvalidation.hxx>
#include <WindowLOKData.hxx>
#include <WindowInput.hxx>
#include <WindowControlAppearance.hxx>
#include <WindowControlState.hxx>
#include <WindowHierarchy.hxx>
#include <brdwin.hxx>
#include <clipping.hxx>
#include <clipping_window.hxx>
#include <salgdi.hxx>
#include <scrwnd.hxx>
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
    pOverlapWindow->mpInput->mpLastFocusWindow = nullptr;
    return pOverlapWindow;
}

void Window::ImplTransferFocusToParent()
{
    vcl::Window* pTarget = GetParent();

    // When windows overlap, give focus to the parent of the next FrameWindow
    if (mpHierarchy->mpBorderWindow)
    {
        if (mpHierarchy->mpBorderWindow->ImplIsOverlapWindow())
            pTarget = mpHierarchy->mpBorderWindow->mpHierarchy->mpOverlapWindow;
    }
    else if (ImplIsOverlapWindow())
    {
        pTarget = mpHierarchy->mpOverlapWindow;
    }

    if (pTarget && pTarget->IsEnabled() && pTarget->IsInputEnabled() && !pTarget->IsInModalMode())
        pTarget->GrabFocus();
    else
        mpHierarchy->mpFrameWindow->GrabFocus();
}

vcl::Window* Window::ImplTransferFocus()
{
    vcl::Window* pOverlapWindow = ImplGetFirstOverlapWindow();

    if (!ImplContainsFocus())
        return pOverlapWindow;

    if (mpClassification->mbFrame)
        return ImplResetOverlapFocusState(pOverlapWindow);

    ImplTransferFocusToParent();

    if (HasFocus())
        return ImplResetOverlapFocusState(pOverlapWindow);

    return pOverlapWindow;
}

bool Window::ImplShouldPassFocusToLastWindow() const
{
    return HasFocus() && mpInput->mpLastFocusWindow
           && !(mpControlState->mnDlgCtrlFlags & DialogControlFlags::WantFocus);
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
        mpInput->mpLastFocusWindow->GrabFocus();

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
    if (mpClassification && mpPlatformState->mpFrameData)
        return mpPlatformState->mpFrameData->mpFocusWin;
    else
        return VclPtr<vcl::Window>();
}

void Window::SetFakeFocus(bool bFocus) { ImplGetWindowInput()->mbFakeFocusSet = bFocus; }

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
    if (!mpClassification || mpClassification->mbInDispose)
        Window::GetFocus();
    else
        GetFocus();
}

void Window::CompatLoseFocus()
{
    if (!mpClassification || mpClassification->mbInDispose)
        Window::LoseFocus();
    else
        LoseFocus();
}

void Window::ShowFocus(const tools::Rectangle& rRect)
{
    if (mpFocusState->isInShowFocus())
        return;

    mpFocusState->enterShowFocus();

    // native themeing suggest not to use focus rects
    if (!(mpFocusState->usesNativeFocus() && IsNativeWidgetEnabled()))
        ImplShowFocusRect(rRect);
    else
        ImplShowNativeFocus();

    mpFocusState->leaveShowFocus();
}

static bool lcl_IsSameFocusRect(const vcl::Window* pWindow, const tools::Rectangle& rRect)
{
    return !pWindow->IsInPaint() && pWindow->ImplGetFocusState()->isFocusVisible()
           && *pWindow->ImplGetWinData()->mpFocusRect == rRect;
}

void Window::ImplShowFocusRect(const tools::Rectangle& rRect)
{
    if (lcl_IsSameFocusRect(this, rRect))
    {
        mpFocusState->leaveShowFocus();
        return;
    }

    ImplWinData* pWinData = ImplGetWinData();

    if (!mpInvalidation->mbInPaint)
    {
        if (mpFocusState->isFocusVisible())
            ImplInvertFocus(*pWinData->mpFocusRect);

        ImplInvertFocus(rRect);
    }

    pWinData->mpFocusRect = rRect;
    mpFocusState->makeFocusVisible();
}

void Window::ImplShowNativeFocus()
{
    if (mpFocusState->isNativeFocusVisible())
        return;

    mpFocusState->makeNativeFocusVisible();

    if (!mpInvalidation->mbInPaint)
        Invalidate();
}

void Window::HideFocus()
{
    if (mpFocusState->isInHideFocus())
        return;

    mpFocusState->enterHideFocus();

    // native themeing can suggest not to use focus rects
    if (!(mpFocusState->usesNativeFocus() && IsNativeWidgetEnabled()))
    {
        if (!mpFocusState->isFocusVisible())
        {
            mpFocusState->leaveHideFocus();
            return;
        }

        if (!mpInvalidation->mbInPaint)
            ImplInvertFocus(*ImplGetWinData()->mpFocusRect);

        mpFocusState->hideFocusVisible();
    }
    else
    {
        if (mpFocusState->isNativeFocusVisible())
        {
            mpFocusState->hideNativeFocusVisible();
            if (!mpInvalidation->mbInPaint)
                Invalidate();
        }
    }

    mpFocusState->leaveHideFocus();
}

void Window::ShowTracking(const tools::Rectangle& rRect, ShowTrackFlags nFlags)
{
    ImplWinData* pWinData = ImplGetWinData();

    if (!mpInvalidation->mbInPaint || !(nFlags & ShowTrackFlags::TrackWindow))
    {
        if (mpClassification->mbTrackVisible)
        {
            if ((*pWinData->mpTrackRect == rRect) && (pWinData->mnTrackFlags == nFlags))
                return;

            InvertTracking(*pWinData->mpTrackRect, pWinData->mnTrackFlags);
        }

        InvertTracking(rRect, nFlags);
    }

    pWinData->mpTrackRect = rRect;
    pWinData->mnTrackFlags = nFlags;
    mpClassification->mbTrackVisible = true;
}

void Window::HideTracking()
{
    if (!mpClassification->mbTrackVisible)
        return;

    ImplWinData* pWinData = ImplGetWinData();

    if (!mpInvalidation->mbInPaint || !(pWinData->mnTrackFlags & ShowTrackFlags::TrackWindow))
        InvertTracking(*pWinData->mpTrackRect, pWinData->mnTrackFlags);

    mpClassification->mbTrackVisible = false;
}

constexpr tools::Long INCLUSIVE_OFFSET = 1;
constexpr tools::Long OPPOSING_SIDES = 2;

constexpr tools::Long innerTop(tools::Long nTop, tools::Long nBorder) { return nTop + nBorder; }

constexpr tools::Long innerBottom(tools::Long nBottom, tools::Long nBorder)
{
    return nBottom - nBorder + INCLUSIVE_OFFSET;
}

constexpr tools::Long innerRight(tools::Long nRight, tools::Long nBorder)
{
    return nRight - nBorder + INCLUSIVE_OFFSET;
}

constexpr tools::Long innerHeight(tools::Long nHeight, tools::Long nBorder)
{
    return nHeight - (nBorder * OPPOSING_SIDES);
}

void Window::InvertTracking(const tools::Rectangle& rRect, ShowTrackFlags nFlags)
{
    OutputDevice* pOutDev = GetOutDev();
    tools::Rectangle aRect(
        pOutDev->GetMapper().LogicToDevicePixel(rRect, pOutDev->GetMappingPolicy()));

    if (aRect.IsEmpty())
        return;
    aRect.Normalize();

    SalGraphics* pGraphics;

    if (nFlags & ShowTrackFlags::TrackWindow)
    {
        if (!GetOutDev()->IsDeviceOutputNecessary())
            return;

        // we need a graphics
        if (!GetOutDev()->mpGraphics)
        {
            if (!pOutDev->AcquireGraphics())
                return;
        }

        if (!GetOutDev()->GetClipState().IsReady())
            vcl::clipping::initDeviceClipRegion(*GetOutDev());

        if (GetOutDev()->GetClipState().IsClippedOut())
            return;

        pGraphics = GetOutDev()->mpGraphics;
    }
    else
    {
        pGraphics = ImplGetFrameGraphics();

        if (nFlags & ShowTrackFlags::Clip)
        {
            vcl::Region aRegion(GetOutputRectPixel());
            vcl::clipping::clipBoundaries(*this, aRegion, false, false);
            pOutDev->SelectClipRegion(aRegion, pGraphics);
        }
    }

    ShowTrackFlags nStyle = nFlags & ShowTrackFlags::StyleMask;
    if (nStyle == ShowTrackFlags::Object)
    {
        pGraphics->Invert(aRect.Left(), aRect.Top(), aRect.GetWidth(), aRect.GetHeight(),
                          SalInvert::TrackFrame, *GetOutDev());
        return;
    }

    if (nStyle == ShowTrackFlags::Split)
    {
        pGraphics->Invert(aRect.Left(), aRect.Top(), aRect.GetWidth(), aRect.GetHeight(),
                          SalInvert::N50, *GetOutDev());
        return;
    }

    constexpr tools::Long DEFAULT_TRACK_BORDER = 1;
    constexpr tools::Long BIG_TRACK_BORDER = 5;

    const tools::Long nBorder
        = (nStyle == ShowTrackFlags::Big) ? BIG_TRACK_BORDER : DEFAULT_TRACK_BORDER;

    pGraphics->Invert(aRect.Left(), aRect.Top(), aRect.GetWidth(), nBorder, SalInvert::N50,
                      *GetOutDev());

    pGraphics->Invert(aRect.Left(), innerBottom(aRect.Bottom(), nBorder), aRect.GetWidth(), nBorder,
                      SalInvert::N50, *GetOutDev());

    pGraphics->Invert(aRect.Left(), innerTop(aRect.Top(), nBorder), nBorder,
                      innerHeight(aRect.GetHeight(), nBorder), SalInvert::N50, *GetOutDev());

    pGraphics->Invert(innerRight(aRect.Right(), nBorder), innerTop(aRect.Top(), nBorder), nBorder,
                      innerHeight(aRect.GetHeight(), nBorder), SalInvert::N50, *GetOutDev());
}

IMPL_LINK(Window, ImplTrackTimerHdl, Timer*, pTimer, void)
{
    if (!mpClassification)
    {
        SAL_WARN("vcl", "ImplTrackTimerHdl has outlived dispose");
        return;
    }

    ImplSVData* pSVData = ImplGetSVData();

    // if Button-Repeat we have to change the timeout
    if (pSVData->mpWinData->mnTrackFlags & StartTrackingFlags::ButtonRepeat)
        pTimer->SetTimeout(GetSettings().GetMouseSettings().GetButtonRepeat());

    // create Tracking-Event
    Point aMousePos(mpPlatformState->mpFrameData->mnLastMouseX,
                    mpPlatformState->mpFrameData->mnLastMouseY);
    if (GetOutDev()->ImplIsAntiparallel())
    {
        // re-mirror frame pos at pChild
        const OutputDevice* pOutDev = GetOutDev();
        pOutDev->ReMirror(aMousePos);
    }

    MouseEvent aMEvt(ScreenToOutputPixel(aMousePos), mpPlatformState->mpFrameData->mnClickCount,
                     MouseEventModifiers::NONE, mpPlatformState->mpFrameData->mnMouseCode,
                     mpPlatformState->mpFrameData->mnMouseCode);

    TrackingEvent aTEvt(aMEvt, TrackingEventFlags::Repeat);
    Tracking(aTEvt);
}

void Window::SetUseFrameData(bool bUseFrameData)
{
    if (mpLOKData)
        mpLOKData->mbUseFrameData = bUseFrameData;
}

void Window::StartTracking(StartTrackingFlags nFlags)
{
    if (!mpClassification)
        return;

    ImplSVData* pSVData = ImplGetSVData();
    VclPtr<vcl::Window> pTrackWin = mpLOKData->mbUseFrameData
                                        ? mpPlatformState->mpFrameData->mpTrackWin
                                        : pSVData->mpWinData->mpTrackWin;

    if (pTrackWin && pTrackWin.get() != this)
        pTrackWin->EndTracking(TrackingEventFlags::Cancel);

    SAL_WARN_IF(pSVData->mpWinData->mpTrackTimer, "vcl",
                "StartTracking called while TrackerTimer still running");

    if (!mpLOKData->mbUseFrameData
        && (nFlags & (StartTrackingFlags::ScrollRepeat | StartTrackingFlags::ButtonRepeat)))
    {
        pSVData->mpWinData->mpTrackTimer.reset(
            new AutoTimer("vcl::Window pSVData->mpWinData->mpTrackTimer"));

        if (nFlags & StartTrackingFlags::ScrollRepeat)
            pSVData->mpWinData->mpTrackTimer->SetTimeout(MouseSettings::GetScrollRepeat());
        else
            pSVData->mpWinData->mpTrackTimer->SetTimeout(MouseSettings::GetButtonStartRepeat());
        pSVData->mpWinData->mpTrackTimer->SetInvokeHandler(LINK(this, Window, ImplTrackTimerHdl));
        pSVData->mpWinData->mpTrackTimer->Start();
    }

    if (mpLOKData->mbUseFrameData)
    {
        mpPlatformState->mpFrameData->mpTrackWin = this;
    }
    else
    {
        pSVData->mpWinData->mpTrackWin = this;
        pSVData->mpWinData->mnTrackFlags = nFlags;
        CaptureMouse();
    }
}

void Window::EndTracking(TrackingEventFlags nFlags)
{
    if (!mpClassification)
        return;

    ImplSVData* pSVData = ImplGetSVData();
    VclPtr<vcl::Window> pTrackWin = mpLOKData->mbUseFrameData
                                        ? mpPlatformState->mpFrameData->mpTrackWin
                                        : pSVData->mpWinData->mpTrackWin;

    if (pTrackWin.get() != this)
        return;

    if (!mpLOKData->mbUseFrameData && pSVData->mpWinData->mpTrackTimer)
        pSVData->mpWinData->mpTrackTimer.reset();

    mpPlatformState->mpFrameData->mpTrackWin = pSVData->mpWinData->mpTrackWin = nullptr;
    pSVData->mpWinData->mnTrackFlags = StartTrackingFlags::NONE;
    ReleaseMouse();

    // call EndTracking if required
    if (!mpPlatformState->mpFrameData)
        return;

    Point aMousePos(mpPlatformState->mpFrameData->mnLastMouseX,
                    mpPlatformState->mpFrameData->mnLastMouseY);
    if (GetOutDev()->ImplIsAntiparallel())
    {
        // re-mirror frame pos at pChild
        const OutputDevice* pOutDev = GetOutDev();
        pOutDev->ReMirror(aMousePos);
    }

    MouseEvent aMEvt(ScreenToOutputPixel(aMousePos), mpPlatformState->mpFrameData->mnClickCount,
                     MouseEventModifiers::NONE, mpPlatformState->mpFrameData->mnMouseCode,
                     mpPlatformState->mpFrameData->mnMouseCode);

    TrackingEvent aTEvt(aMEvt, nFlags | TrackingEventFlags::End);

    // CompatTracking effectively
    if (!mpClassification || mpClassification->mbInDispose)
    {
        Window::Tracking(aTEvt);
        return;
    }

    Tracking(aTEvt);
}

bool Window::IsTracking() const
{
    if (!mpClassification)
        return false;

    if (mpLOKData->mbUseFrameData && mpPlatformState->mpFrameData)
        return mpPlatformState->mpFrameData->mpTrackWin == this;

    if (!mpLOKData->mbUseFrameData && ImplGetSVData()->mpWinData)
        return ImplGetSVData()->mpWinData->mpTrackWin == this;

    return false;
}

void Window::StartAutoScroll(StartAutoScrollFlags nFlags)
{
    ImplSVData* pSVData = ImplGetSVData();

    if (pSVData->mpWinData->mpAutoScrollWin.get() != this && pSVData->mpWinData->mpAutoScrollWin)
    {
        pSVData->mpWinData->mpAutoScrollWin->EndAutoScroll();
    }

    pSVData->mpWinData->mpAutoScrollWin = this;
    pSVData->mpWinData->mnAutoScrollFlags = nFlags;
    pSVData->maAppData.mpWheelWindow = VclPtr<ImplWheelWindow>::Create(this);
}

void Window::EndAutoScroll()
{
    ImplSVData* pSVData = ImplGetSVData();

    if (pSVData->mpWinData->mpAutoScrollWin.get() != this)
        return;

    pSVData->mpWinData->mpAutoScrollWin = nullptr;
    pSVData->mpWinData->mnAutoScrollFlags = StartAutoScrollFlags::NONE;
    pSVData->maAppData.mpWheelWindow->ImplStop();
    pSVData->maAppData.mpWheelWindow.disposeAndClear();
}

VclPtr<vcl::Window> Window::SaveFocus()
{
    ImplSVData* pSVData = ImplGetSVData();
    if (pSVData->mpWinData->mpFocusWin)
        return pSVData->mpWinData->mpFocusWin;

    return nullptr;
}

void Window::EndSaveFocus(const VclPtr<vcl::Window>& xFocusWin)
{
    if (xFocusWin && !xFocusWin->isDisposed())
        xFocusWin->GrabFocus();
}

static void lcl_ActivateFloatingWindows(vcl::Window const* pWindow, bool bActive)
{
    for (vcl::Window* pTempWindow = pWindow->ImplGetWindowHierarchy()->mpFirstOverlap; pTempWindow;
         pTempWindow = pTempWindow->ImplGetWindowHierarchy()->mpNext)
    {
        if (pTempWindow->GetActivateMode() == ActivateModeFlags::NONE)
        {
            if ((pTempWindow->GetType() == WindowType::BORDERWINDOW)
                && (pTempWindow->ImplGetWindow()->GetType() == WindowType::FLOATINGWINDOW))
                static_cast<ImplBorderWindow*>(pTempWindow)->SetDisplayActive(bActive);
        }

        lcl_ActivateFloatingWindows(pTempWindow, bActive);
    }
}

bool vcl::Window::ImplRestoreFocusToWindow()
{
    if (!IsInputEnabled() || IsInModalMode())
        return false;

    if (IsEnabled())
    {
        GrabFocus();
        return true;
    }

    if (ImplHasDlgCtrl())
    {
        // #109094# if the focus is restored to a disabled dialog control (was disabled meanwhile)
        // try to move it to the next control
        ImplDlgCtrlNextWindow();
        return true;
    }

    return false;
}

bool vcl::Window::ImplCanReceiveFocus() const { return IsInputEnabled() && !IsInModalMode(); }

static bool lcl_ShouldBringDialogToTop(const vcl::Window* pTopLevelWindow)
{
    const ImplSVData* pSVData = ImplGetSVData();
    return (!pTopLevelWindow->IsInputEnabled() || pTopLevelWindow->IsInModalMode())
           && !pSVData->mpWinData->mpExecuteDialogs.empty();
}

static void lcl_BringExecutingDialogToTop()
{
    const ImplSVData* pSVData = ImplGetSVData();
    pSVData->mpWinData->mpExecuteDialogs.back()->ToTop(ToTopFlags::RestoreWhenMin
                                                       | ToTopFlags::GrabFocusOnly);
}

bool vcl::Window::ImplResolveFocusLocally()
{
    return !ImplGetPlatformState()->mpFrameData->mpFocusWin
           || (ImplCanReceiveFocus() && ImplRestoreFocusToWindow());
}

bool vcl::Window::ImplSyncDelayedFocus()
{
    ImplGetPlatformState()->mpFrameData->mnFocusId = nullptr;

    const bool bHasFocus = ImplGetPlatformState()->mpFrameData->mbHasFocus
                           || ImplGetPlatformState()->mpFrameData->mbSysObjFocus;

    // If the status has been preserved, because we got back the focus
    // in the meantime, we do nothing
    if (!bHasFocus)
        return false;

    // redraw all floating windows inactive
    if (ImplGetPlatformState()->mpFrameData->mbStartFocusState != bHasFocus)
        lcl_ActivateFloatingWindows(this, bHasFocus);

    if (ImplResolveFocusLocally())
        return true;

    vcl::Window* pTopLevelWindow
        = ImplGetPlatformState()->mpFrameData->mpFocusWin->ImplGetFirstOverlapWindow();

    if (lcl_ShouldBringDialogToTop(pTopLevelWindow))
    {
        lcl_BringExecutingDialogToTop();
        return true;
    }

    pTopLevelWindow->GrabFocus();
    return true;
}

bool vcl::Window::ImplProcessFocusGain()
{
    // redraw all floating windows inactive
    if (!ImplGetPlatformState()->mpFrameData->mbStartFocusState)
        lcl_ActivateFloatingWindows(this, true);

    if (!ImplGetPlatformState()->mpFrameData->mpFocusWin)
    {
        GrabFocus();
        return true;
    }

    if (ImplCanReceiveFocus() && ImplRestoreFocusToWindow())
        return true;

    vcl::Window* pTopLevelWindow
        = ImplGetPlatformState()->mpFrameData->mpFocusWin->ImplGetFirstOverlapWindow();

    if (lcl_ShouldBringDialogToTop(pTopLevelWindow))
    {
        lcl_BringExecutingDialogToTop();
        return true;
    }

    pTopLevelWindow->GrabFocus();
    return true;
}

void vcl::Window::ImplProcessFocusLoss()
{
    const ImplSVData* pSVData = ImplGetSVData();
    if (pSVData->mpWinData->mpFocusWin == this)
    {
        ImplClearFocus();
        ImplDeactivateFocus();
        ImplNotifyLostFocus();
    }
}

void vcl::Window::ImplClearFocus()
{
    ImplSVData* pSVData = ImplGetSVData();

    // transfer the FocusWindow
    if (vcl::Window* pOverlapWindow = ImplGetFirstOverlapWindow();
        pOverlapWindow && pOverlapWindow->ImplGetWindowInput())
        pOverlapWindow->ImplGetWindowInput()->mpLastFocusWindow = this;

    pSVData->mpWinData->mpFocusWin = nullptr;

    if (ImplGetControlAppearance() && ImplGetControlAppearance()->getCursor())
        ImplGetControlAppearance()->hideCursor();
}

static bool lcl_CanDeactivateWindow(const vcl::Window* pOverlapWindow,
                                    const vcl::Window* pRealWindow)
{
    return pOverlapWindow && pOverlapWindow->ImplGetWindowClassification() && pRealWindow
           && pRealWindow->ImplGetWindowClassification();
}

void vcl::Window::ImplDeactivateFocus()
{
    vcl::Window* pOldOverlapWindow = ImplGetFirstOverlapWindow();
    vcl::Window* pOldRealWindow = pOldOverlapWindow->ImplGetWindow();

    if (!lcl_CanDeactivateWindow(pOldOverlapWindow, pOldRealWindow))
        return;

    pOldOverlapWindow->ImplGetFocusState()->setActive(false);
    pOldOverlapWindow->Deactivate();

    if (pOldRealWindow == pOldOverlapWindow)
        return;

    pOldRealWindow->ImplGetFocusState()->setActive(false);
    pOldRealWindow->Deactivate();
}

void vcl::Window::ImplNotifyLostFocus()
{
#ifdef _WIN32
    // To avoid problems with the Unix IME
    EndExtTextInput();
#endif

    const NotifyEvent aNEvt(NotifyEventType::LOSEFOCUS, this);

    if (!ImplCallPreNotify(const_cast<NotifyEvent&>(aNEvt)))
        CompatLoseFocus();

    ImplCallDeactivateListeners(nullptr);
}

IMPL_LINK_NOARG(vcl::Window, ImplAsyncFocusHdl, void*, void)
{
    if (!ImplGetWindowClassification() || !ImplGetPlatformState()->mpFrameData)
        return;

    // If the status has been preserved, because we got back the focus
    // in the meantime, we do nothing
    const bool bHasFocus = ImplSyncDelayedFocus();

    // next execute the delayed functions
    if (bHasFocus && ImplProcessFocusGain())
        return;

    if (vcl::Window* pFocusWin = ImplGetPlatformState()->mpFrameData->mpFocusWin)
        pFocusWin->ImplProcessFocusLoss();

    // Redraw all floating window inactive
    if (ImplGetPlatformState()->mpFrameData->mbStartFocusState != bHasFocus)
        lcl_ActivateFloatingWindows(this, bHasFocus);
}

} /* namespace vcl */

void InvertFocusRect(vcl::RenderContext& rRenderContext, const tools::Rectangle& rRect)
{
    const int nBorder = 1;
    rRenderContext.Invert(
        tools::Rectangle(Point(rRect.Left(), rRect.Top()), Size(rRect.GetWidth(), nBorder)),
        InvertFlags::N50);
    rRenderContext.Invert(tools::Rectangle(Point(rRect.Left(), rRect.Bottom() - nBorder + 1),
                                           Size(rRect.GetWidth(), nBorder)),
                          InvertFlags::N50);
    rRenderContext.Invert(tools::Rectangle(Point(rRect.Left(), rRect.Top() + nBorder),
                                           Size(nBorder, rRect.GetHeight() - (nBorder * 2))),
                          InvertFlags::N50);
    rRenderContext.Invert(
        tools::Rectangle(Point(rRect.Right() - nBorder + 1, rRect.Top() + nBorder),
                         Size(nBorder, rRect.GetHeight() - (nBorder * 2))),
        InvertFlags::N50);
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
