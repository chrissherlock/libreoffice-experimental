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
#include <vcl/event.hxx>
#include <vcl/window.hxx>

#include <window.h>
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

void Window::ShowFocus(const tools::Rectangle& rRect)
{
    if (mpWindowImpl->mbInShowFocus)
        return;

    mpWindowImpl->mbInShowFocus = true;

    ImplWinData* pWinData = ImplGetWinData();

    // native themeing suggest not to use focus rects
    if (!(mpWindowImpl->mbUseNativeFocus && IsNativeWidgetEnabled()))
        ImplShowFocusRect(pWinData, rRect);
    else
        ImplShowNativeFocus();

    mpWindowImpl->mbInShowFocus = false;
}

static bool lcl_IsSameFocusRect(const WindowImpl* pWindowImpl, const ImplWinData* pWinData,
                                const tools::Rectangle& rRect)
{
    return !pWindowImpl->mbInPaint && pWindowImpl->mbFocusVisible
           && *pWinData->mpFocusRect == rRect;
}

void Window::ImplShowFocusRect(ImplWinData* pWinData, const tools::Rectangle& rRect)
{
    if (lcl_IsSameFocusRect(mpWindowImpl.get(), pWinData, rRect))
    {
        mpWindowImpl->mbInShowFocus = false;
        return;
    }

    if (!mpWindowImpl->mbInPaint)
    {
        if (mpWindowImpl->mbFocusVisible)
            ImplInvertFocus(*pWinData->mpFocusRect);

        ImplInvertFocus(rRect);
    }

    pWinData->mpFocusRect = rRect;
    mpWindowImpl->mbFocusVisible = true;
}

void Window::ImplShowNativeFocus()
{
    if (mpWindowImpl->mbNativeFocusVisible)
        return;

    mpWindowImpl->mbNativeFocusVisible = true;

    if (!mpWindowImpl->mbInPaint)
        Invalidate();
}

void Window::HideFocus()
{
    if (mpWindowImpl->mbInHideFocus)
        return;
    mpWindowImpl->mbInHideFocus = true;

    // native themeing can suggest not to use focus rects
    if (!(mpWindowImpl->mbUseNativeFocus && IsNativeWidgetEnabled()))
    {
        if (!mpWindowImpl->mbFocusVisible)
        {
            mpWindowImpl->mbInHideFocus = false;
            return;
        }

        if (!mpWindowImpl->mbInPaint)
            ImplInvertFocus(*ImplGetWinData()->mpFocusRect);
        mpWindowImpl->mbFocusVisible = false;
    }
    else
    {
        if (mpWindowImpl->mbNativeFocusVisible)
        {
            mpWindowImpl->mbNativeFocusVisible = false;
            if (!mpWindowImpl->mbInPaint)
                Invalidate();
        }
    }

    mpWindowImpl->mbInHideFocus = false;
}

void Window::ShowTracking(const tools::Rectangle& rRect, ShowTrackFlags nFlags)
{
    ImplWinData* pWinData = ImplGetWinData();

    if (!mpWindowImpl->mbInPaint || !(nFlags & ShowTrackFlags::TrackWindow))
    {
        if (mpWindowImpl->mbTrackVisible)
        {
            if ((*pWinData->mpTrackRect == rRect) && (pWinData->mnTrackFlags == nFlags))
                return;

            InvertTracking(*pWinData->mpTrackRect, pWinData->mnTrackFlags);
        }

        InvertTracking(rRect, nFlags);
    }

    pWinData->mpTrackRect = rRect;
    pWinData->mnTrackFlags = nFlags;
    mpWindowImpl->mbTrackVisible = true;
}

void Window::HideTracking()
{
    if (!mpWindowImpl->mbTrackVisible)
        return;

    ImplWinData* pWinData = ImplGetWinData();

    if (!mpWindowImpl->mbInPaint || !(pWinData->mnTrackFlags & ShowTrackFlags::TrackWindow))
        InvertTracking(*pWinData->mpTrackRect, pWinData->mnTrackFlags);

    mpWindowImpl->mbTrackVisible = false;
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
    if (!mpWindowImpl)
    {
        SAL_WARN("vcl", "ImplTrackTimerHdl has outlived dispose");
        return;
    }

    ImplSVData* pSVData = ImplGetSVData();

    // if Button-Repeat we have to change the timeout
    if (pSVData->mpWinData->mnTrackFlags & StartTrackingFlags::ButtonRepeat)
        pTimer->SetTimeout(GetSettings().GetMouseSettings().GetButtonRepeat());

    // create Tracking-Event
    Point aMousePos(mpWindowImpl->mpFrameData->mnLastMouseX,
                    mpWindowImpl->mpFrameData->mnLastMouseY);
    if (GetOutDev()->ImplIsAntiparallel())
    {
        // re-mirror frame pos at pChild
        const OutputDevice* pOutDev = GetOutDev();
        pOutDev->ReMirror(aMousePos);
    }

    MouseEvent aMEvt(ScreenToOutputPixel(aMousePos), mpWindowImpl->mpFrameData->mnClickCount,
                     MouseEventModifiers::NONE, mpWindowImpl->mpFrameData->mnMouseCode,
                     mpWindowImpl->mpFrameData->mnMouseCode);

    TrackingEvent aTEvt(aMEvt, TrackingEventFlags::Repeat);
    Tracking(aTEvt);
}

void Window::SetUseFrameData(bool bUseFrameData)
{
    if (mpWindowImpl)
        mpWindowImpl->mbUseFrameData = bUseFrameData;
}

void Window::StartTracking(StartTrackingFlags nFlags)
{
    if (!mpWindowImpl)
        return;

    ImplSVData* pSVData = ImplGetSVData();
    VclPtr<vcl::Window> pTrackWin = mpWindowImpl->mbUseFrameData
                                        ? mpWindowImpl->mpFrameData->mpTrackWin
                                        : pSVData->mpWinData->mpTrackWin;

    if (pTrackWin && pTrackWin.get() != this)
        pTrackWin->EndTracking(TrackingEventFlags::Cancel);

    SAL_WARN_IF(pSVData->mpWinData->mpTrackTimer, "vcl",
                "StartTracking called while TrackerTimer still running");

    if (!mpWindowImpl->mbUseFrameData
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

    if (mpWindowImpl->mbUseFrameData)
    {
        mpWindowImpl->mpFrameData->mpTrackWin = this;
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
    if (!mpWindowImpl)
        return;

    ImplSVData* pSVData = ImplGetSVData();
    VclPtr<vcl::Window> pTrackWin = mpWindowImpl->mbUseFrameData
                                        ? mpWindowImpl->mpFrameData->mpTrackWin
                                        : pSVData->mpWinData->mpTrackWin;

    if (pTrackWin.get() != this)
        return;

    if (!mpWindowImpl->mbUseFrameData && pSVData->mpWinData->mpTrackTimer)
        pSVData->mpWinData->mpTrackTimer.reset();

    mpWindowImpl->mpFrameData->mpTrackWin = pSVData->mpWinData->mpTrackWin = nullptr;
    pSVData->mpWinData->mnTrackFlags = StartTrackingFlags::NONE;
    ReleaseMouse();

    // call EndTracking if required
    if (!mpWindowImpl->mpFrameData)
        return;

    Point aMousePos(mpWindowImpl->mpFrameData->mnLastMouseX,
                    mpWindowImpl->mpFrameData->mnLastMouseY);
    if (GetOutDev()->ImplIsAntiparallel())
    {
        // re-mirror frame pos at pChild
        const OutputDevice* pOutDev = GetOutDev();
        pOutDev->ReMirror(aMousePos);
    }

    MouseEvent aMEvt(ScreenToOutputPixel(aMousePos), mpWindowImpl->mpFrameData->mnClickCount,
                     MouseEventModifiers::NONE, mpWindowImpl->mpFrameData->mnMouseCode,
                     mpWindowImpl->mpFrameData->mnMouseCode);

    TrackingEvent aTEvt(aMEvt, nFlags | TrackingEventFlags::End);

    // CompatTracking effectively
    if (!mpWindowImpl || mpWindowImpl->mbInDispose)
    {
        Window::Tracking(aTEvt);
        return;
    }

    Tracking(aTEvt);
}

bool Window::IsTracking() const
{
    if (!mpWindowImpl)
        return false;

    if (mpWindowImpl->mbUseFrameData && mpWindowImpl->mpFrameData)
        return mpWindowImpl->mpFrameData->mpTrackWin == this;

    if (!mpWindowImpl->mbUseFrameData && ImplGetSVData()->mpWinData)
        return ImplGetSVData()->mpWinData->mpTrackWin == this;

    return false;
}

void Window::StartAutoScroll(StartAutoScrollFlags nFlags)
{
    ImplSVData* pSVData = ImplGetSVData();

    if (pSVData->mpWinData->mpAutoScrollWin.get() != this)
    {
        if (pSVData->mpWinData->mpAutoScrollWin)
            pSVData->mpWinData->mpAutoScrollWin->EndAutoScroll();
    }

    pSVData->mpWinData->mpAutoScrollWin = this;
    pSVData->mpWinData->mnAutoScrollFlags = nFlags;
    pSVData->maAppData.mpWheelWindow = VclPtr<ImplWheelWindow>::Create(this);
}

void Window::EndAutoScroll()
{
    ImplSVData* pSVData = ImplGetSVData();

    if (pSVData->mpWinData->mpAutoScrollWin.get() == this)
    {
        pSVData->mpWinData->mpAutoScrollWin = nullptr;
        pSVData->mpWinData->mnAutoScrollFlags = StartAutoScrollFlags::NONE;
        pSVData->maAppData.mpWheelWindow->ImplStop();
        pSVData->maAppData.mpWheelWindow.disposeAndClear();
    }
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

} /* namespace vcl */

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
