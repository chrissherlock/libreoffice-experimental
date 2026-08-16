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
#include <tools/debug.hxx>
#include <tools/time.hxx>
#include <unotools/localedatawrapper.hxx>
#include <comphelper/lok.hxx>

#include <vcl/event.hxx>
#include <vcl/cursor.hxx>
#include <vcl/toolkit/floatwin.hxx>
#include <vcl/toolkit/dialog.hxx>
#include <vcl/toolkit/edit.hxx>
#include <vcl/help.hxx>
#include <vcl/dockwin.hxx>
#include <vcl/menu.hxx>
#include <vcl/CoordinateMapper.hxx>

#include <window.h>
#include <clipping_window.hxx>
#include <salframe.hxx>
#include <accmgr.hxx>
#include <helpwin.hxx>
#include <brdwin.hxx>

#include "GenericDropTargetDropContext.hxx"
#include "GenericDropTargetDragContext.hxx"
#include "HandleGestureEventBase.hxx"
#include "HandleGestureEvent.hxx"
#include "HandleWheelEvent.hxx"
#include "HandleGesturePanEvent.hxx"
#include "HandleGestureSwipeEvent.hxx"
#include "HandleGestureLongPressEvent.hxx"
#include "HandleGestureRotateEvent.hxx"
#include "HandleGestureZoomEvent.hxx"

constexpr tools::Long IMPL_MIN_NEEDSYSWIN = 49;
#ifdef MACOSX
constexpr sal_uInt16 MOUSE_MODIFIER_MASK = KEY_SHIFT | KEY_MOD1 | KEY_MOD2 | KEY_MOD3;
#else
constexpr sal_uInt16 MOUSE_MODIFIER_MASK = KEY_SHIFT | KEY_MOD1 | KEY_MOD2;
#endif

bool ImplCallPreNotify(NotifyEvent& rEvt) { return rEvt.GetWindow()->CompatPreNotify(rEvt); }

static Point lcl_GetCommandPosition(const VclPtr<vcl::Window>& pChild, Point const* pPos,
                                    bool bMouse)
{
    if (pPos)
        return *pPos;

    if (bMouse)
        return pChild->GetPointerPosPixel();

    // simulate mouse position at center of window
    Size aSize(pChild->GetOutputSizePixel());
    return Point(aSize.getWidth() / 2, aSize.getHeight() / 2);
}

bool ImplCallCommand(const VclPtr<vcl::Window>& pChild, CommandEventId nEvt, void const* pData,
                     bool bMouse, Point const* pPos)
{
    Point aPos = lcl_GetCommandPosition(pChild, pPos, bMouse);

    CommandEvent aCEvt(aPos, nEvt, bMouse, pData);
    NotifyEvent aNCmdEvt(NotifyEventType::COMMAND, pChild, &aCEvt);

    if (bool bPreNotify = ImplCallPreNotify(aNCmdEvt); pChild->isDisposed() || bPreNotify)
        return false;

    pChild->ImplGetWindowImpl()->mbCommand = false;
    pChild->Command(aCEvt);

    if (pChild->isDisposed())
        return false;

    pChild->ImplNotifyKeyMouseCommandEventListeners(aNCmdEvt);

    if (pChild->isDisposed())
        return false;

    if (pChild->ImplGetWindowImpl()->mbCommand)
        return true;

    return false;
}

static bool lcl_IsValidFrameFloatPopup(const vcl::Window* pFrameWindow)
{
    ImplSVData* pSVData = ImplGetSVData();
    if (const vcl::Window* pFloatWin = pSVData->mpWinData->mpFirstFloat)
        return pFrameWindow->ImplIsWindowOrChild(pFloatWin, true);

    return false;
}

static bool lcl_CanCloseOnAppFocus()
{
    ImplSVData* pSVData = ImplGetSVData();
    return bool(pSVData->mpWinData->mpFirstFloat->GetPopupModeFlags()
                & FloatWinPopupFlags::NoAppFocusClose);
}

static void lcl_EndPopupMode()
{
    ImplSVData* pSVData = ImplGetSVData();
    pSVData->mpWinData->mpFirstFloat->EndPopupMode(FloatWinPopupEndFlags::Cancel
                                                   | FloatWinPopupEndFlags::CloseAll);
}

static void lcl_KillOwnPopups(vcl::Window const* pWindow)
{
    if (lcl_IsValidFrameFloatPopup(pWindow->ImplGetWindowImpl()->mpFrameWindow))
        return;

    if (lcl_CanCloseOnAppFocus())
        return;

    lcl_EndPopupMode();
}

static bool lcl_ShouldBufferResize(const vcl::Window* pWindow)
{
    // use resize buffering for user resizes
    // ownerdraw decorated windows and floating windows can be resized immediately (i.e. synchronously)
    if (!pWindow->ImplGetWindowImpl()->mbFrame || !(pWindow->GetStyle() & WB_SIZEABLE)
        || (pWindow->GetStyle()
            & WB_OWNERDRAWDECORATION) // synchronous resize for ownerdraw decorated windows (toolbars)
        || pWindow->ImplGetWindowImpl()
               ->mbFloatWin) // synchronous resize for floating windows, #i43799#
    {
        return false;
    }

    return true;
}

static bool lcl_ShouldStartResizeTimer(const vcl::Window* pWindow)
{
    if (!lcl_ShouldBufferResize(pWindow))
        return false;

    vcl::Window* pTarget = pWindow->ImplGetWindowImpl()->mpClientWindow
                               ? pWindow->ImplGetWindowImpl()->mpClientWindow.get()
                               : const_cast<vcl::Window*>(pWindow);

    if (WorkWindow* pWorkWindow = dynamic_cast<WorkWindow*>(pTarget);
        !pWorkWindow || pWorkWindow->IsPresentationMode())
        return false;

    return true;
}

static bool lcl_ShouldSkipResizePropagation(const vcl::Window* pWindow)
{
    return !pWindow->IsVisible() && !pWindow->ImplGetWindow()->ImplGetWindowImpl()->mbAllResize
           && !(pWindow->ImplGetWindowImpl()->mbFrame
                && pWindow->ImplGetWindowImpl()
                       ->mpClientWindow); // propagate resize for system border windows
}

static void lcl_HandleResizePropagation(vcl::Window* pWindow)
{
    if (lcl_ShouldSkipResizePropagation(pWindow))
        pWindow->ImplGetWindowImpl()->mbCallResize = true;

    if (!lcl_ShouldStartResizeTimer(pWindow))
    {
        pWindow->ImplCallResize(); // otherwise menus cannot be positioned
        return;
    }

    pWindow->ImplGetWindowImpl()->mpFrameData->maResizeIdle.Start();
}

static bool lcl_HasSizeChanged(const vcl::Window* pWindow, tools::Long nNewWidth,
                               tools::Long nNewHeight)
{
    return (nNewWidth != pWindow->GetOutputSizePixel().Width())
           || (nNewHeight != pWindow->GetOutDev()->GetOutputHeightPixel());
}

static void lcl_HandleResizeDimensions(vcl::Window* pWindow, tools::Long nNewWidth,
                                       tools::Long nNewHeight)
{
    if (bool bChanged = lcl_HasSizeChanged(pWindow, nNewWidth, nNewHeight);
        !((nNewWidth > 0 && nNewHeight > 0)
          || (pWindow->ImplGetWindow()->ImplGetWindowImpl()->mbAllResize && bChanged)))
        return;

    pWindow->GetOutDev()->SetOutputWidthPixel(nNewWidth);
    pWindow->GetOutDev()->SetOutputHeightPixel(nNewHeight);
    pWindow->ImplGetWindowImpl()->mbWaitSystemResize = false;

    if (pWindow->IsReallyVisible())
        vcl::clipping::setClipFlag(*pWindow);

    lcl_HandleResizePropagation(pWindow);

    if (pWindow->SupportsDoubleBuffering() && pWindow->ImplGetWindowImpl()->mbFrame)
    {
        // Propagate resize for the frame's buffer.
        pWindow->ImplGetWindowImpl()->mpFrameData->mpBuffer->SetOutputSizePixel(
            pWindow->GetOutputSizePixel());
    }
}

static bool lcl_CanMoveOrSize(const vcl::Window* pWindow, tools::Long nNewWidth,
                              tools::Long nNewHeight)
{
    return lcl_HasSizeChanged(pWindow, nNewWidth, nNewHeight)
           && (pWindow->GetStyle() & (WB_MOVEABLE | WB_SIZEABLE));
}

static void lcl_HandleMinimizedState(vcl::Window* pWindow, tools::Long nNewWidth,
                                     tools::Long nNewHeight)
{
    if (bool bMinimized = (nNewWidth <= 0) || (nNewHeight <= 0);
        bMinimized != pWindow->ImplGetWindowImpl()->mpFrameData->mbMinimized)
    {
        pWindow->ImplGetWindowImpl()->mpFrameWindow->ImplNotifyIconifiedState(bMinimized);
        pWindow->ImplGetWindowImpl()->mpFrameData->mbMinimized = bMinimized;
    }
}

void ImplHandleResize(vcl::Window* pWindow, tools::Long nNewWidth, tools::Long nNewHeight)
{
    if (lcl_CanMoveOrSize(pWindow, nNewWidth, nNewHeight))
    {
        lcl_KillOwnPopups(pWindow);

        if (pWindow->ImplGetWindow() != ImplGetSVHelpData().mpHelpWin)
            ImplDestroyHelpWindow(true);
    }

    lcl_HandleResizeDimensions(pWindow, nNewWidth, nNewHeight);

    pWindow->ImplGetWindowImpl()->mpFrameData->mbNeedSysWindow
        = (nNewWidth < IMPL_MIN_NEEDSYSWIN) || (nNewHeight < IMPL_MIN_NEEDSYSWIN);

    lcl_HandleMinimizedState(pWindow, nNewWidth, nNewHeight);
}

static void lcl_HandleMove(vcl::Window* pWindow)
{
    if (pWindow->ImplGetWindowImpl()->mbFrame && pWindow->ImplIsFloatingWindow()
        && pWindow->IsReallyVisible())
    {
        static_cast<FloatingWindow*>(pWindow)->EndPopupMode(FloatWinPopupEndFlags::TearOff);
        pWindow->ImplCallMove();
    }

    if (pWindow->GetStyle() & (WB_MOVEABLE | WB_SIZEABLE))
    {
        lcl_KillOwnPopups(pWindow);
        if (pWindow->ImplGetWindow() != ImplGetSVHelpData().mpHelpWin)
            ImplDestroyHelpWindow(true);
    }

    if (pWindow->IsVisible())
        pWindow->ImplCallMove();
    else
        pWindow->ImplGetWindowImpl()->mbCallMove
            = true; // make sure the framepos will be updated on the next Show()

    if (pWindow->ImplGetWindowImpl()->mbFrame && pWindow->ImplGetWindowImpl()->mpClientWindow)
        pWindow->ImplGetWindowImpl()
            ->mpClientWindow->ImplCallMove(); // notify client to update geometry
}

static void lcl_ActivateFloatingWindows(vcl::Window const* pWindow, bool bActive)
{
    // First check all overlapping windows
    for (vcl::Window* pTempWindow = pWindow->ImplGetWindowImpl()->mpHierarchy->mpFirstOverlap;
         pTempWindow; pTempWindow = pTempWindow->ImplGetWindowImpl()->mpHierarchy->mpNext)
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

bool vcl::Window::ImplSyncDelayedFocus()
{
    ImplGetWindowImpl()->mpFrameData->mnFocusId = nullptr;

    bool bHasFocus = ImplGetWindowImpl()->mpFrameData->mbHasFocus
                     || ImplGetWindowImpl()->mpFrameData->mbSysObjFocus;

    // If the status has been preserved, because we got back the focus
    // in the meantime, we do nothing
    if (!bHasFocus)
    {
        GrabFocus();
        return false;
    }

    // redraw all floating windows inactive
    if (ImplGetWindowImpl()->mpFrameData->mbStartFocusState != bHasFocus)
        lcl_ActivateFloatingWindows(this, bHasFocus);

    if (!ImplGetWindowImpl()->mpFrameData->mpFocusWin)
        return true;

    if (ImplCanReceiveFocus() && ImplRestoreFocusToWindow())
        return true;

    ImplSVData* pSVData = ImplGetSVData();
    vcl::Window* pTopLevelWindow
        = ImplGetWindowImpl()->mpFrameData->mpFocusWin->ImplGetFirstOverlapWindow();

    if ((!pTopLevelWindow->IsInputEnabled() || pTopLevelWindow->IsInModalMode())
        && !pSVData->mpWinData->mpExecuteDialogs.empty())
    {
        pSVData->mpWinData->mpExecuteDialogs.back()->ToTop(ToTopFlags::RestoreWhenMin
                                                           | ToTopFlags::GrabFocusOnly);
        return true;
    }

    pTopLevelWindow->GrabFocus();
    return true;
}

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

bool vcl::Window::ImplProcessFocusGain()
{
    // redraw all floating windows inactive
    if (!ImplGetWindowImpl()->mpFrameData->mbStartFocusState)
        lcl_ActivateFloatingWindows(this, true);

    if (!ImplGetWindowImpl()->mpFrameData->mpFocusWin)
    {
        GrabFocus();
        return true;
    }

    if (ImplCanReceiveFocus() && ImplRestoreFocusToWindow())
        return true;

    vcl::Window* pTopLevelWindow
        = ImplGetWindowImpl()->mpFrameData->mpFocusWin->ImplGetFirstOverlapWindow();

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
    ImplSVData* pSVData = ImplGetSVData();
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
        pOverlapWindow && pOverlapWindow->ImplGetWindowImpl())
        pOverlapWindow->ImplGetWindowImpl()->mpLastFocusWindow = this;

    pSVData->mpWinData->mpFocusWin = nullptr;

    if (ImplGetWindowImpl() && ImplGetWindowImpl()->mpCursor)
        ImplGetWindowImpl()->mpCursor->ImplHide();
}

static bool lcl_CanDeactivateWindow(const vcl::Window* pOverlapWindow,
                                    const vcl::Window* pRealWindow)
{
    return pOverlapWindow && pOverlapWindow->ImplGetWindowImpl() && pRealWindow
           && pRealWindow->ImplGetWindowImpl();
}

void vcl::Window::ImplDeactivateFocus()
{
    vcl::Window* pOldOverlapWindow = ImplGetFirstOverlapWindow();

    if (vcl::Window* pOldRealWindow = pOldOverlapWindow->ImplGetWindow();
        !lcl_CanDeactivateWindow(pOldOverlapWindow, pOldRealWindow))
        return;
    else
    {
        pOldOverlapWindow->ImplGetWindowImpl()->mbActive = false;
        pOldOverlapWindow->Deactivate();

        if (pOldRealWindow == pOldOverlapWindow)
            return;

        pOldRealWindow->ImplGetWindowImpl()->mbActive = false;
        pOldRealWindow->Deactivate();
    }
}

void vcl::Window::ImplNotifyLostFocus()
{
#ifdef _WIN32
    // To avoid problems with the Unix IME
    EndExtTextInput();
#endif

    NotifyEvent aNEvt(NotifyEventType::LOSEFOCUS, this);

    if (!ImplCallPreNotify(aNEvt))
        CompatLoseFocus();

    ImplCallDeactivateListeners(nullptr);
}

IMPL_LINK_NOARG(vcl::Window, ImplAsyncFocusHdl, void*, void)
{
    if (!ImplGetWindowImpl() || !ImplGetWindowImpl()->mpFrameData)
        return;

    // If the status has been preserved, because we got back the focus
    // in the meantime, we do nothing
    bool bHasFocus = ImplSyncDelayedFocus();

    // next execute the delayed functions
    if (bHasFocus && ImplProcessFocusGain())
        return;

    if (vcl::Window* pFocusWin = ImplGetWindowImpl()->mpFrameData->mpFocusWin)
        pFocusWin->ImplProcessFocusLoss();

    // Redraw all floating window inactive
    if (ImplGetWindowImpl()->mpFrameData->mbStartFocusState != bHasFocus)
        lcl_ActivateFloatingWindows(this, bHasFocus);
}

static void lcl_HandleGetFocus(vcl::Window* pWindow)
{
    if (!pWindow || !pWindow->ImplGetWindowImpl() || !pWindow->ImplGetWindowImpl()->mpFrameData)
        return;

    pWindow->ImplGetWindowImpl()->mpFrameData->mbHasFocus = true;

    // execute Focus-Events after a delay, such that SystemChildWindows
    // do not blink when they receive focus
    if (pWindow->ImplGetWindowImpl()->mpFrameData->mnFocusId)
        return;

    pWindow->ImplGetWindowImpl()->mpFrameData->mbStartFocusState
        = !pWindow->ImplGetWindowImpl()->mpFrameData->mbHasFocus;
    pWindow->ImplGetWindowImpl()->mpFrameData->mnFocusId
        = Application::PostUserEvent(LINK(pWindow, vcl::Window, ImplAsyncFocusHdl), nullptr, true);

    if (vcl::Window* pFocusWin = pWindow->ImplGetWindowImpl()->mpFrameData->mpFocusWin;
        pFocusWin && pFocusWin->ImplGetWindowImpl()->mpCursor)
        pFocusWin->ImplGetWindowImpl()->mpCursor->ImplShow();
}

static bool lcl_HasActiveTrackerForFrame(const vcl::Window* pWindow)
{
    ImplSVData* pSVData = ImplGetSVData();

    if (!pSVData->mpWinData->mpTrackWin)
        return false;

    const vcl::Window* pTrackWin = pSVData->mpWinData->mpTrackWin.get();

    return pTrackWin->ImplGetWindowImpl()
           && pTrackWin->ImplGetWindowImpl()->mpFrameWindow == pWindow;
}

static void lcl_HandleLoseFocus(vcl::Window* pWindow)
{
    if (!pWindow)
        return;

    ImplSVData* pSVData = ImplGetSVData();

    // Abort the autoscroll if the frame loses focus
    if (pSVData->mpWinData->mpAutoScrollWin)
        pSVData->mpWinData->mpAutoScrollWin->EndAutoScroll();

    // Abort tracking if the frame loses focus
    if (lcl_HasActiveTrackerForFrame(pWindow))
        pSVData->mpWinData->mpTrackWin->EndTracking(TrackingEventFlags::Cancel);

    if (pWindow->ImplGetWindowImpl() && pWindow->ImplGetWindowImpl()->mpFrameData)
    {
        pWindow->ImplGetWindowImpl()->mpFrameData->mbHasFocus = false;

        // execute Focus-Events after a delay, such that SystemChildWindows
        // do not flicker when they receive focus
        if (!pWindow->ImplGetWindowImpl()->mpFrameData->mnFocusId)
        {
            pWindow->ImplGetWindowImpl()->mpFrameData->mbStartFocusState
                = !pWindow->ImplGetWindowImpl()->mpFrameData->mbHasFocus;
            pWindow->ImplGetWindowImpl()->mpFrameData->mnFocusId = Application::PostUserEvent(
                LINK(pWindow, vcl::Window, ImplAsyncFocusHdl), nullptr, true);
        }

        if (vcl::Window* pFocusWin = pWindow->ImplGetWindowImpl()->mpFrameData->mpFocusWin;
            pFocusWin && pFocusWin->ImplGetWindowImpl()->mpCursor)
            pFocusWin->ImplGetWindowImpl()->mpCursor->ImplHide();
    }

    // Make sure that no menu is visible when a toplevel window loses focus.
    if (VclPtr<FloatingWindow> pFirstFloat = pSVData->mpWinData->mpFirstFloat;
        pFirstFloat && pFirstFloat->IsMenuFloatingWindow() && !pWindow->GetParent())
        pFirstFloat->EndPopupMode(FloatWinPopupEndFlags::Cancel | FloatWinPopupEndFlags::CloseAll);
}

namespace
{
struct DelayedCloseEvent
{
    VclPtr<vcl::Window> pWindow;
};
}

static void lcl_DelayedCloseEventLink(void* pCEvent, void*)
{
    std::unique_ptr<DelayedCloseEvent> pEv(static_cast<DelayedCloseEvent*>(pCEvent));

    if (pEv->pWindow->isDisposed())
        return;

    // dispatch to correct window type
    if (pEv->pWindow->IsSystemWindow())
        static_cast<SystemWindow*>(pEv->pWindow.get())->Close();
    else if (pEv->pWindow->IsDockingWindow())
        static_cast<DockingWindow*>(pEv->pWindow.get())->Close();
}

static bool lcl_IsInPrivatePopupMode(const vcl::Window* pWindow)
{
    return pWindow->ImplIsFloatingWindow()
           && static_cast<const FloatingWindow*>(pWindow)->ImplIsInPrivatePopupMode();
}

static void lcl_HandleClose(const vcl::Window* pWindow)
{
    ImplSVData* pSVData = ImplGetSVData();

    bool bWasPopup = lcl_IsInPrivatePopupMode(pWindow);

    // on Close stop all floating modes and end popups
    if (pSVData->mpWinData->mpFirstFloat)
    {
        FloatingWindow* pLastLevelFloat
            = pSVData->mpWinData->mpFirstFloat->ImplFindLastLevelFloat();
        pLastLevelFloat->EndPopupMode(FloatWinPopupEndFlags::Cancel
                                      | FloatWinPopupEndFlags::CloseAll);
    }

    if (ImplGetSVHelpData().mbExtHelpMode)
        Help::EndExtHelp();

    if (ImplGetSVHelpData().mpHelpWin)
        ImplDestroyHelpWindow(false);

    // AutoScrollMode
    if (pSVData->mpWinData->mpAutoScrollWin)
        pSVData->mpWinData->mpAutoScrollWin->EndAutoScroll();

    if (pSVData->mpWinData->mpTrackWin)
        pSVData->mpWinData->mpTrackWin->EndTracking(TrackingEventFlags::Cancel
                                                    | TrackingEventFlags::Key);

    if (bWasPopup)
        return;

    vcl::Window* pWin = pWindow->ImplGetWindow();

    if (SystemWindow* pSysWin = dynamic_cast<SystemWindow*>(pWin))
    {
        // See if the custom close handler is set.
        if (const Link<SystemWindow&, void>& rLink = pSysWin->GetCloseHdl(); rLink.IsSet())
        {
            rLink.Call(*pSysWin);
            return;
        }
    }

    // check whether close is allowed
    if (pWin->IsEnabled() && pWin->IsInputEnabled() && !pWin->IsInModalMode())
    {
        DelayedCloseEvent* pEv = new DelayedCloseEvent;
        pEv->pWindow = pWin;
        Application::PostUserEvent(LINK_NONMEMBER(pEv, lcl_DelayedCloseEventLink));
    }
}

static void lcl_HandleUserEvent(ImplSVEvent* pSVEvent)
{
    if (!pSVEvent)
        return;

    if (pSVEvent->mbCall)
        pSVEvent->maLink.Call(pSVEvent->mpData);

    delete pSVEvent;
}

MouseEventModifiers ImplGetMouseMoveMode(SalMouseEvent const* pEvent)
{
    MouseEventModifiers nMode = MouseEventModifiers::NONE;

    if (!pEvent->mnCode)
        nMode |= MouseEventModifiers::SIMPLEMOVE;

    if ((pEvent->mnCode & MOUSE_LEFT) && !(pEvent->mnCode & KEY_MOD1))
        nMode |= MouseEventModifiers::DRAGMOVE;

    if ((pEvent->mnCode & MOUSE_LEFT) && (pEvent->mnCode & KEY_MOD1))
        nMode |= MouseEventModifiers::DRAGCOPY;

    return nMode;
}

static bool lcl_IsSingleLeftButton(SalMouseEvent const* pEvent)
{
    return (pEvent->mnButton == MOUSE_LEFT) && !(pEvent->mnCode & (MOUSE_MIDDLE | MOUSE_RIGHT));
}

static bool lcl_IsMultiSelectButton(SalMouseEvent const* pEvent)
{
    return (pEvent->mnButton == MOUSE_LEFT) && (pEvent->mnCode & KEY_MOD1)
           && !(pEvent->mnCode & (MOUSE_MIDDLE | MOUSE_RIGHT | KEY_SHIFT));
}

static bool lcl_IsRangeSelectButton(SalMouseEvent const* pEvent)
{
    return (pEvent->mnButton == MOUSE_LEFT) && (pEvent->mnCode & KEY_SHIFT)
           && !(pEvent->mnCode & (MOUSE_MIDDLE | MOUSE_RIGHT | KEY_MOD1));
}

MouseEventModifiers ImplGetMouseButtonMode(SalMouseEvent const* pEvent)
{
    MouseEventModifiers nMode = MouseEventModifiers::NONE;

    if (pEvent->mnButton == MOUSE_LEFT)
        nMode |= MouseEventModifiers::SIMPLECLICK;

    if (lcl_IsSingleLeftButton(pEvent))
        nMode |= MouseEventModifiers::SELECT;

    if (lcl_IsMultiSelectButton(pEvent))
        nMode |= MouseEventModifiers::MULTISELECT;

    if (lcl_IsRangeSelectButton(pEvent))
        nMode |= MouseEventModifiers::RANGESELECT;

    return nMode;
}

static bool lcl_HandleSalMouseMoveBase(vcl::Window* pWindow, SalMouseEvent const* pEvent,
                                       bool bLeave)
{
    return ImplHandleMouseEvent(pWindow, NotifyEventType::MOUSEMOVE, bLeave, pEvent->mnX,
                                pEvent->mnY, pEvent->mnTime, pEvent->mnCode,
                                ImplGetMouseMoveMode(pEvent));
}

static bool lcl_HandleSalMouseLeave(vcl::Window* pWindow, SalMouseEvent const* pEvent)
{
    return lcl_HandleSalMouseMoveBase(pWindow, pEvent, true);
}

static bool lcl_HandleSalMouseMove(vcl::Window* pWindow, SalMouseEvent const* pEvent)
{
    return lcl_HandleSalMouseMoveBase(pWindow, pEvent, false);
}

static sal_uInt16 lcl_GetMouseButtonCode(SalMouseEvent const* pEvent)
{
    return pEvent->mnButton | (pEvent->mnCode & MOUSE_MODIFIER_MASK);
}

static bool lcl_HandleSalMouseButtonBase(vcl::Window* pWindow, SalMouseEvent const* pEvent,
                                         NotifyEventType nEventType)
{
    return ImplHandleMouseEvent(pWindow, nEventType, false, pEvent->mnX, pEvent->mnY,
                                pEvent->mnTime, lcl_GetMouseButtonCode(pEvent),
                                ImplGetMouseButtonMode(pEvent));
}

static bool lcl_HandleSalMouseButtonDown(vcl::Window* pWindow, SalMouseEvent const* pEvent)
{
    return lcl_HandleSalMouseButtonBase(pWindow, pEvent, NotifyEventType::MOUSEBUTTONDOWN);
}

static bool lcl_HandleSalMouseButtonUp(vcl::Window* pWindow, SalMouseEvent const* pEvent)
{
    return lcl_HandleSalMouseButtonBase(pWindow, pEvent, NotifyEventType::MOUSEBUTTONUP);
}

static bool lcl_HandleMenuEvent(vcl::Window const* pWindow, SalMenuEvent* pEvent, SalEvent nEvent)
{
    vcl::Window* pWin = pWindow->ImplGetWindowImpl()->mpHierarchy->mpFirstChild;
    while (pWin)
    {
        if (pWin->ImplGetWindowImpl()->mbSysWin)
            break;
        pWin = pWin->ImplGetWindowImpl()->mpHierarchy->mpNext;
    }

    if (!pWin)
        return false;

    if (MenuBar* pMenuBar = static_cast<SystemWindow*>(pWin)->GetMenuBar())
    {
        switch (nEvent)
        {
            case SalEvent::MenuActivate:
                pMenuBar->HandleMenuActivateEvent(static_cast<Menu*>(pEvent->mpMenu));
                return true;

            case SalEvent::MenuDeactivate:
                pMenuBar->HandleMenuDeActivateEvent(static_cast<Menu*>(pEvent->mpMenu));
                return true;

            case SalEvent::MenuHighlight:
                return pMenuBar->HandleMenuHighlightEvent(static_cast<Menu*>(pEvent->mpMenu),
                                                          pEvent->mnId);

            case SalEvent::MenuButtonCommand:
                return pMenuBar->HandleMenuButtonEvent(pEvent->mnId);

            case SalEvent::MenuCommand:
                return pMenuBar->HandleMenuCommandEvent(static_cast<Menu*>(pEvent->mpMenu),
                                                        pEvent->mnId);

            default:
                break;
        }
    }

    return false;
}

static bool lcl_IsFloatingWindow(const vcl::Window* pWindow)
{
    return pWindow->ImplGetWindowImpl() && pWindow->ImplGetWindowImpl()->mbFloatWin;
}

static bool lcl_IsDockingWindow(const vcl::Window* pWindow)
{
    return pWindow->ImplGetWindowImpl() && pWindow->ImplGetWindowImpl()->mbDockWin;
}

static bool lcl_ParentGrabsFocus(const vcl::Window* pChild)
{
    if (vcl::Window* pParent = pChild->GetWindow(GetWindowType::RealParent))
        return lcl_IsFloatingWindow(pParent)
               && static_cast<const FloatingWindow*>(pParent)->GrabsFocus();

    return false;
}

static bool lcl_GrabsFocusFloatingWindow(const vcl::Window* pChild)
{
    return lcl_IsFloatingWindow(pChild) && static_cast<const FloatingWindow*>(pChild)->GrabsFocus();
}

static bool lcl_GrabsFocusDockingWindow(const vcl::Window* pChild)
{
    return lcl_IsDockingWindow(pChild) && lcl_ParentGrabsFocus(pChild);
}

static bool lcl_GrabsFocusWindow(const vcl::Window* pChild)
{
    return lcl_GrabsFocusFloatingWindow(pChild) || lcl_GrabsFocusDockingWindow(pChild);
}

static vcl::Window* lcl_FindFocusWindow(vcl::Window* pWindow, ImplSVData* pSVData)
{
    vcl::Window* pChild = pSVData->mpWinData->mpFirstFloat;
    while (pChild && !lcl_GrabsFocusWindow(pChild))
    {
        pChild = pChild->GetParent();
    }

    if (!pChild)
        pChild = pWindow;

    if (auto pWinImpl = pChild->ImplGetWindowImpl(); pWinImpl && pWinImpl->mpFrameData)
        return pWinImpl->mpFrameData->mpFocusWin.get();

    return nullptr;
}

static vcl::Window* lcl_GetValidInputWindow(vcl::Window* pWindow, ImplSVData* pSVData)
{
    vcl::Window* pChild = lcl_FindFocusWindow(pWindow, pSVData);

    // no child - then no input
    if (!pChild)
        return nullptr;

    // We call also KeyInput if we haven't the focus, because on Unix
    // system this is often the case when a Lookup Choice Window has
    // the focus - because this windows send the KeyInput directly to
    // the window without resetting the focus

    // no keyinput to disabled windows
    if (!pChild->IsEnabled() || !pChild->IsInputEnabled() || pChild->IsInModalMode())
        return nullptr;

    return pChild;
}

static vcl::Window* lcl_GetKeyInputWindow(vcl::Window* pWindow)
{
    ImplSVData* pSVData = ImplGetSVData();

    // determine last input time
    pSVData->maAppData.mnLastInputTime = tools::Time::GetSystemTicks();

    // #127104# workaround for destroyed windows
    if (!pWindow->ImplGetWindowImpl())
        return nullptr;

    return lcl_GetValidInputWindow(pWindow, pSVData);
}

static bool lcl_HandleInputContextChange(vcl::Window* pWindow)
{
    vcl::Window* pChild = lcl_GetKeyInputWindow(pWindow);
    CommandInputContextData aData;
    return !ImplCallCommand(pChild, CommandEventId::InputContextChange, &aData);
}

static void lcl_HandleSalKeyMod(vcl::Window* pWindow, SalKeyModEvent const* pEvent)
{
    ImplSVData* pSVData = ImplGetSVData();
    if (vcl::Window* pTrackWin = pSVData->mpWinData->mpTrackWin)
        pWindow = pTrackWin;

    if (sal_uInt16 nOldCode
        = pWindow->ImplGetWindowImpl()->mpFrameData->mnMouseCode & MOUSE_MODIFIER_MASK;
        nOldCode != pEvent->mnCode)
    {
        sal_uInt16 nNewCode = pEvent->mnCode;
        nNewCode |= pWindow->ImplGetWindowImpl()->mpFrameData->mnMouseCode & ~MOUSE_MODIFIER_MASK;
        pWindow->ImplGetWindowImpl()->mpFrameWindow->ImplCallMouseMove(nNewCode, true);
    }

    // #105224# send commandevent to allow special treatment of Ctrl-LeftShift/Ctrl-RightShift etc.
    // + auto-accelerator feature, tdf#92630

    // try to find a key input window...
    vcl::Window* pChild = lcl_GetKeyInputWindow(pWindow);
    //...otherwise fail safe...
    if (!pChild)
        pChild = pWindow;

    CommandModKeyData data(pEvent->mnModKeyCode, pEvent->mbDown);
    ImplCallCommand(pChild, CommandEventId::ModKeyChange, &data);
}

static void lcl_HandleInputLanguageChange(vcl::Window* pWindow)
{
    if (vcl::Window* pChild = lcl_GetKeyInputWindow(pWindow))
        ImplCallCommand(pChild, CommandEventId::InputLanguageChange);
}

static void lcl_HandleSalSettings(SalEvent nEvent)
{
    if (Application* pApp = GetpApp())
    {
        if (nEvent == SalEvent::SettingsChanged)
        {
            AllSettings aSettings = Application::GetSettings();
            Application::MergeSystemSettings(aSettings);
            pApp->OverrideSystemSettings(aSettings);
            Application::SetSettings(aSettings);

            return;
        }

        DataChangedEventType nType;

        switch (nEvent)
        {
            case SalEvent::PrinterChanged:
                ImplDeletePrnQueueList();
                nType = DataChangedEventType::PRINTER;
                break;

            case SalEvent::DisplayChanged:
                nType = DataChangedEventType::DISPLAY;
                break;

            case SalEvent::FontChanged:
                OutputDevice::ImplUpdateAllFontData(true);
                nType = DataChangedEventType::FONTS;
                break;

            default:
                return;
        }

        DataChangedEvent aDCEvt(nType);
        Application::ImplCallEventListenersApplicationDataChanged(&aDCEvt);
        Application::NotifyAllWindows(aDCEvt);
    }
}

static tools::Rectangle lcl_GetChildCursorRect(const vcl::Window* pChild,
                                               const OutputDevice* pChildOutDev)
{
    if (const tools::Rectangle* pRect = pChild->GetCursorRect())
        return pChildOutDev->GetMapper().LogicToDevicePixel(*pRect,
                                                            pChildOutDev->GetMappingPolicy());

    if (vcl::Cursor* pCursor = pChild->GetCursor())
    {
        auto aPos = pChildOutDev->convertTo<vcl::DevicePoint>(vcl::LogicPoint(pCursor->GetPos()),
                                                              pChildOutDev->GetMapMode());
        auto aSize = pChild->convertTo<vcl::WindowSize>(vcl::LogicSize(pCursor->GetSize()),
                                                        pChild->GetMapMode());

        if (!aSize->Width())
            aSize->setWidth(pChild->GetSettings().GetStyleSettings().GetCursorSize());

        return tools::Rectangle(aPos.get(), aSize.get());
    }

    return tools::Rectangle(Point(pChild->GetDeviceOriginX(), pChild->GetDeviceOriginY()), Size());
}

static vcl::Window* lcl_GetExtTextInputWindow(vcl::Window* pWindow)
{
    ImplSVData* pSVData = ImplGetSVData();
    if (vcl::Window* pExtTextInputWin = pSVData->mpWinData->mpExtTextInputWin;
        pExtTextInputWin && pWindow->ImplIsWindowOrChild(pExtTextInputWin))
        return pExtTextInputWin;

    return lcl_GetKeyInputWindow(pWindow);
}

static void lcl_HandleExtTextInputPos(vcl::Window* pWindow, tools::Rectangle& rRect,
                                      tools::Long& rInputWidth, bool* pVertical)
{
    if (vcl::Window* pExtTextInputWin = lcl_GetExtTextInputWindow(pWindow))
    {
        const OutputDevice* pExtTextInputOutDev = pExtTextInputWin->GetOutDev();
        ImplCallCommand(pExtTextInputWin, CommandEventId::CursorPos);

        rRect = lcl_GetChildCursorRect(pExtTextInputWin, pExtTextInputOutDev);

        rInputWidth = pExtTextInputWin->LogicWidthToDevicePixel(
            pExtTextInputWin->GetCursorExtTextInputWidth());

        if (!rInputWidth)
            rInputWidth = rRect.GetWidth();

        if (pVertical)
            *pVertical = pExtTextInputWin->GetInputContext().GetFont().IsVertical();
    }
    else if (pVertical)
    {
        *pVertical = false;
    }
}

static void lcl_HandleSalExtTextInputPos(vcl::Window* pWindow, SalExtTextInputPosEvent* pEvt)
{
    tools::Rectangle aCursorRect;
    lcl_HandleExtTextInputPos(pWindow, aCursorRect, pEvt->mnExtWidth, &pEvt->mbVertical);

    if (aCursorRect.IsEmpty())
    {
        pEvt->mnX = -1;
        pEvt->mnY = -1;
        pEvt->mnWidth = -1;
        pEvt->mnHeight = -1;
        return;
    }

    pEvt->mnX = aCursorRect.Left();
    pEvt->mnY = aCursorRect.Top();
    pEvt->mnWidth = aCursorRect.GetWidth();
    pEvt->mnHeight = aCursorRect.GetHeight();
}

static bool lcl_HandleShowDialog(vcl::Window* pWindow, ShowDialogId nDialogId)
{
    if (!pWindow)
        return false;

    if (pWindow->GetType() == WindowType::BORDERWINDOW)
    {
        if (vcl::Window* pWrkWin = pWindow->GetWindow(GetWindowType::Client))
            pWindow = pWrkWin;
    }
    CommandDialogData aCmdData(nDialogId);
    return ImplCallCommand(pWindow, CommandEventId::ShowDialog, &aCmdData);
}

static void lcl_HandleSalSurroundingTextRequest(vcl::Window* pWindow,
                                                SalSurroundingTextRequestEvent* pEvt)
{
    vcl::Window* pChild = lcl_GetKeyInputWindow(pWindow);
    if (!pChild)
    {
        pEvt->maText.clear();
        pEvt->mnStart = 0;
        pEvt->mnEnd = 0;
        return;
    }

    pEvt->maText = pChild->GetSurroundingText();
    Selection aSelRange = pChild->GetSurroundingTextSelection();

    sal_uLong nSelectionAnchorPos = 0;

    if (aSelRange.Min() < 0)
        nSelectionAnchorPos = 0;
    else if (aSelRange.Min() > pEvt->maText.getLength())
        nSelectionAnchorPos = pEvt->maText.getLength();
    else
        nSelectionAnchorPos = aSelRange.Min();

    sal_uLong nCursorPos = 0;

    if (aSelRange.Max() < 0)
        nCursorPos = 0;
    else if (aSelRange.Max() > pEvt->maText.getLength())
        nCursorPos = pEvt->maText.getLength();
    else
        nCursorPos = aSelRange.Max();

    pEvt->mnCursorPos = nCursorPos;
    pEvt->mnStart = std::min(nSelectionAnchorPos, nCursorPos);
    pEvt->mnEnd = std::max(nSelectionAnchorPos, nCursorPos);
}

static void lcl_HandleSalDeleteSurroundingTextRequest(vcl::Window* pWindow,
                                                      SalSurroundingTextSelectionChangeEvent* pEvt)
{
    vcl::Window* pChild = lcl_GetKeyInputWindow(pWindow);
    Selection aSelection(pEvt->mnStart, pEvt->mnEnd);

    if (!pChild || !pChild->DeleteSurroundingText(aSelection))
    {
        pEvt->mnStart = pEvt->mnEnd = SAL_MAX_UINT32;
        return;
    }

    pEvt->mnStart = aSelection.Min();
    pEvt->mnEnd = aSelection.Max();
}

static void lcl_HandleSurroundingTextSelectionChange(vcl::Window* pWindow, sal_uLong nStart,
                                                     sal_uLong nEnd)
{
    if (vcl::Window* pChild = lcl_GetKeyInputWindow(pWindow))
    {
        CommandSelectionChangeData data(nStart, nEnd);
        ImplCallCommand(pChild, CommandEventId::SelectionChange, &data);
    }
}

static void lcl_HandleStartReconversion(vcl::Window* pWindow)
{
    if (vcl::Window* pChild = lcl_GetKeyInputWindow(pWindow))
        ImplCallCommand(pChild, CommandEventId::PrepareReconversion);
}

static void lcl_HandleSalQueryCharPosition(vcl::Window* pWindow, SalQueryCharPositionEvent* pEvt)
{
    pEvt->mbValid = false;
    pEvt->mbVertical = false;
    pEvt->maCursorBound = AbsoluteScreenPixelRectangle();

    ImplSVData* pSVData = ImplGetSVData();
    vcl::Window* pChild = pSVData->mpWinData->mpExtTextInputWin;

    if (!pChild || !pWindow->ImplIsWindowOrChild(pChild))
        pChild = lcl_GetKeyInputWindow(pWindow);

    if (!pChild)
        return;

    ImplCallCommand(pChild, CommandEventId::QueryCharPosition);

    if (ImplWinData* pWinData = pChild->ImplGetWinData();
        pWinData->mpCompositionCharRects
        && pEvt->mnCharPos < o3tl::make_unsigned(pWinData->mnCompositionCharRects))
    {
        const OutputDevice* pChildOutDev = pChild->GetOutDev();
        const tools::Rectangle& aRect = pWinData->mpCompositionCharRects[pEvt->mnCharPos];
        tools::Rectangle aDeviceRect
            = pChildOutDev->GetMapper().LogicToDevicePixel(aRect, pChildOutDev->GetMappingPolicy());
        AbsoluteScreenPixelPoint aAbsScreenPos = pChild->OutputToAbsoluteScreenPixel(
            pChild->ScreenToOutputPixel(aDeviceRect.TopLeft()));

        pEvt->maCursorBound = AbsoluteScreenPixelRectangle(aAbsScreenPos, aDeviceRect.GetSize());
        pEvt->mbVertical = pWinData->mbVertical;
        pEvt->mbValid = true;
    }
}

static bool lcl_HandleKey(vcl::Window* pWindow, NotifyEventType nSVEvent, sal_uInt16 nKeyCode,
                          sal_uInt16 nCharCode, sal_uInt16 nRepeat, bool bForward)
{
    ImplSVData* pSVData = ImplGetSVData();
    vcl::KeyCode aKeyCode(nKeyCode, nKeyCode);
    sal_uInt16 nEvCode = aKeyCode.GetCode();

    // allow application key listeners to remove the key event
    // but make sure we're not forwarding external KeyEvents, (ie where bForward is false)
    // because those are coming back from the listener itself and MUST be processed
    if (bForward)
    {
        VclEventId nVCLEvent;
        switch (nSVEvent)
        {
            case NotifyEventType::KEYINPUT:
                nVCLEvent = VclEventId::WindowKeyInput;
                break;
            case NotifyEventType::KEYUP:
                nVCLEvent = VclEventId::WindowKeyUp;
                break;
            default:
                nVCLEvent = VclEventId::NONE;
                break;
        }
        KeyEvent aKeyEvent(static_cast<sal_Unicode>(nCharCode), aKeyCode, nRepeat);
        if (nVCLEvent != VclEventId::NONE && Application::HandleKey(nVCLEvent, pWindow, &aKeyEvent))
            return true;
    }

    bool bCtrlF6 = (aKeyCode.GetCode() == KEY_F6) && aKeyCode.IsMod1();

    // determine last input time
    pSVData->maAppData.mnLastInputTime = tools::Time::GetSystemTicks();

    // handle tracking window
    if (nSVEvent == NotifyEventType::KEYINPUT)
    {
        if (ImplGetSVHelpData().mbExtHelpMode)
        {
            Help::EndExtHelp();
            if (nEvCode == KEY_ESCAPE)
                return true;
        }
        if (ImplGetSVHelpData().mpHelpWin)
            ImplDestroyHelpWindow(false);

        // AutoScrollMode
        if (pSVData->mpWinData->mpAutoScrollWin)
        {
            pSVData->mpWinData->mpAutoScrollWin->EndAutoScroll();
            if (nEvCode == KEY_ESCAPE)
                return true;
        }

        if (pSVData->mpWinData->mpTrackWin)
        {
            if (sal_uInt16 nOrigCode = aKeyCode.GetCode(); nOrigCode == KEY_ESCAPE)
            {
                pSVData->mpWinData->mpTrackWin->EndTracking(TrackingEventFlags::Cancel
                                                            | TrackingEventFlags::Key);
                if (pSVData->mpWinData->mpFirstFloat)
                {
                    if (FloatingWindow* pLastLevelFloat
                        = pSVData->mpWinData->mpFirstFloat->ImplFindLastLevelFloat();
                        !(pLastLevelFloat->GetPopupModeFlags() & FloatWinPopupFlags::NoKeyClose))
                    {
                        if (sal_uInt16 nEscCode = aKeyCode.GetCode(); nEscCode == KEY_ESCAPE)
                            pLastLevelFloat->EndPopupMode(FloatWinPopupEndFlags::Cancel
                                                          | FloatWinPopupEndFlags::CloseAll);
                    }
                }
                return true;
            }
            else if (nOrigCode == KEY_RETURN)
            {
                pSVData->mpWinData->mpTrackWin->EndTracking(TrackingEventFlags::Key);
                return true;
            }
            else
                return true;
        }

        // handle FloatingMode
        if (pSVData->mpWinData->mpFirstFloat)
        {
            if (FloatingWindow* pLastLevelFloat
                = pSVData->mpWinData->mpFirstFloat->ImplFindLastLevelFloat();
                !(pLastLevelFloat->GetPopupModeFlags() & FloatWinPopupFlags::NoKeyClose))
            {
                if (sal_uInt16 nCode = aKeyCode.GetCode(); (nCode == KEY_ESCAPE) || bCtrlF6)
                {
                    pLastLevelFloat->EndPopupMode(FloatWinPopupEndFlags::Cancel
                                                  | FloatWinPopupEndFlags::CloseAll);
                    if (!bCtrlF6)
                        return true;
                }
            }
        }

        // test for accel
        if (pSVData->maAppData.mpAccelMgr)
        {
            if (pSVData->maAppData.mpAccelMgr->IsAccelKey(aKeyCode))
                return true;
        }
    }

    // find window
    VclPtr<vcl::Window> pChild = lcl_GetKeyInputWindow(pWindow);
    if (!pChild)
        return false;

    // #i1820# use locale specific decimal separator
    if (nEvCode == KEY_DECIMAL)
    {
        // tdf#138932: don't modify the meaning of the key for password box
        if (auto pEdit = dynamic_cast<Edit*>(pChild.get()); !(pEdit && pEdit->IsPassword()))
        {
            if (Application::GetSettings().GetMiscSettings().GetEnableLocalizedDecimalSep())
            {
                OUString aSep(pWindow->GetSettings().GetLocaleDataWrapper().getNumDecimalSep());
                nCharCode = static_cast<sal_uInt16>(aSep[0]);
            }
        }
    }

    // RTL: mirror cursor keys
    if ((aKeyCode.GetCode() == KEY_LEFT || aKeyCode.GetCode() == KEY_RIGHT)
        && pChild->IsRTLEnabled() && pChild->GetOutDev()->HasMirroredGraphics())
        aKeyCode = vcl::KeyCode(aKeyCode.GetCode() == KEY_LEFT ? KEY_RIGHT : KEY_LEFT,
                                aKeyCode.GetModifier());

    KeyEvent aKeyEvt(static_cast<sal_Unicode>(nCharCode), aKeyCode, nRepeat);
    NotifyEvent aNotifyEvt(nSVEvent, pChild, &aKeyEvt);
    bool bKeyPreNotify = ImplCallPreNotify(aNotifyEvt);
    bool bRet = true;

    if (!bKeyPreNotify && !pChild->isDisposed())
    {
        if (nSVEvent == NotifyEventType::KEYINPUT)
        {
            UITestLogger::getInstance().logKeyInput(pChild, aKeyEvt);
            pChild->ImplGetWindowImpl()->mbKeyInput = false;
            pChild->KeyInput(aKeyEvt);
        }
        else
        {
            pChild->ImplGetWindowImpl()->mbKeyUp = false;
            pChild->KeyUp(aKeyEvt);
        }
        if (!pChild->isDisposed())
            aNotifyEvt.GetWindow()->ImplNotifyKeyMouseCommandEventListeners(aNotifyEvt);
    }

    if (pChild->isDisposed())
        return true;

    if (nSVEvent == NotifyEventType::KEYINPUT)
    {
        if (!bKeyPreNotify && pChild->ImplGetWindowImpl()->mbKeyInput)
        {
            sal_uInt16 nCode = aKeyCode.GetCode();

            // ContextMenu
            if ((nCode == KEY_CONTEXTMENU)
                || ((nCode == KEY_F10) && aKeyCode.IsShift() && !aKeyCode.IsMod1()
                    && !aKeyCode.IsMod2()))
                bRet = !ImplCallCommand(pChild, CommandEventId::ContextMenu);
            else
            {
                // #101999# is focus in or below toolbox
                bool bToolboxFocus = false;
                if ((nCode == KEY_F1) && aKeyCode.IsShift())
                {
                    for (vcl::Window* pWin = pWindow->ImplGetWindowImpl()->mpFrameData->mpFocusWin;
                         pWin; pWin = pWin->GetParent())
                    {
                        if (pWin->ImplGetWindowImpl()->mbToolBox)
                        {
                            bToolboxFocus = true;
                            break;
                        }
                    }
                }

                if (((nCode == KEY_F2) && aKeyCode.IsShift())
                    || ((nCode == KEY_F1) && aKeyCode.IsMod1()) ||
                    // #101999# no active help when focus in toolbox, simulate BalloonHelp instead
                    ((nCode == KEY_F1) && aKeyCode.IsShift() && bToolboxFocus))
                {
                    // TipHelp via Keyboard (Shift-F2 or Ctrl-F1)
                    // simulate mouseposition at center of window

                    Size aSize = pChild->GetOutDev()->GetOutputSize();
                    Point aPos(aSize.getWidth() / 2, aSize.getHeight() / 2);
                    aPos = pChild->OutputToScreenPixel(aPos);

                    HelpEvent aHelpEvent(aPos, HelpEventMode::BALLOON);
                    aHelpEvent.SetKeyboardActivated(true);
                    ImplGetSVHelpData().mbSetKeyboardHelp = true;
                    pChild->RequestHelp(aHelpEvent);
                    ImplGetSVHelpData().mbSetKeyboardHelp = false;
                }
                else if ((nCode == KEY_F1) || (nCode == KEY_HELP))
                {
                    if (!aKeyCode.GetModifier())
                    {
                        if (ImplGetSVHelpData().mbContextHelp)
                        {
                            Point aMousePos
                                = pChild->OutputToScreenPixel(pChild->GetPointerPosPixel());
                            HelpEvent aHelpEvent(aMousePos, HelpEventMode::CONTEXT);
                            pChild->RequestHelp(aHelpEvent);
                        }
                        else
                            bRet = false;
                    }
                    else if (aKeyCode.IsShift())
                    {
                        if (ImplGetSVHelpData().mbExtHelp)
                            Help::StartExtHelp();
                        else
                            bRet = false;
                    }
                }
                else
                    bRet = false;
            }
        }
    }
    else
    {
        if (!bKeyPreNotify && pChild->ImplGetWindowImpl()->mbKeyUp)
            bRet = false;
    }

    // #105591# send keyinput to parent if we are a floating window and the key was not processed yet
    if (bRet || !pWindow->ImplGetWindowImpl() || !pWindow->ImplGetWindowImpl()->mbFloatWin
        || !pWindow->GetParent()
        || (pWindow->ImplGetWindowImpl()->mpFrame
            == pWindow->GetParent()->ImplGetWindowImpl()->mpFrame))
        return bRet;

    pChild = pWindow->GetParent();

    // call handler
    NotifyEvent aNEvt(nSVEvent, pChild, &aKeyEvt);
    if (bool bPreNotify = ImplCallPreNotify(aNEvt); pChild->isDisposed() || bPreNotify)
        return true;

    if (nSVEvent == NotifyEventType::KEYINPUT)
    {
        pChild->ImplGetWindowImpl()->mbKeyInput = false;
        pChild->KeyInput(aKeyEvt);
    }
    else
    {
        pChild->ImplGetWindowImpl()->mbKeyUp = false;
        pChild->KeyUp(aKeyEvt);
    }

    if (!pChild->isDisposed())
        aNEvt.GetWindow()->ImplNotifyKeyMouseCommandEventListeners(aNEvt);

    if (pChild->isDisposed() || !pChild->ImplGetWindowImpl()->mbKeyInput)
        return true;

    return bRet;
}

static bool lcl_HandleExtTextInput(vcl::Window* pWindow, const OUString& rText,
                                   const ExtTextInputAttr* pTextAttr, sal_Int32 nCursorPos,
                                   sal_uInt16 nCursorFlags)
{
    ImplSVData* pSVData = ImplGetSVData();
    vcl::Window* pChild = nullptr;

    int nTries = 200;
    while (nTries--)
    {
        pChild = pSVData->mpWinData->mpExtTextInputWin;
        if (!pChild)
        {
            pChild = lcl_GetKeyInputWindow(pWindow);
            if (!pChild)
                return false;
        }
        if (!pChild->ImplGetWindowImpl()->mpFrameData->mnFocusId)
            break;

        if (comphelper::LibreOfficeKit::isActive())
        {
            SAL_WARN("vcl", "Failed to get ext text input context");
            break;
        }
        Application::Yield();
    }

    // If it is the first ExtTextInput call, we inform the information
    // and allocate the data, which we must store in this mode
    ImplWinData* pWinData = pChild->ImplGetWinData();
    if (!pChild->ImplGetWindowImpl()->mbExtTextInput)
    {
        pChild->ImplGetWindowImpl()->mbExtTextInput = true;
        pWinData->mpExtOldText = OUString();
        pWinData->mpExtOldAttrAry.reset();
        pSVData->mpWinData->mpExtTextInputWin = pChild;
        ImplCallCommand(pChild, CommandEventId::StartExtTextInput);
    }

    // be aware of being recursively called in StartExtTextInput
    if (!pChild->ImplGetWindowImpl()->mbExtTextInput)
        return false;

    // Test for changes
    bool bOnlyCursor = false;
    sal_Int32 nMinLen = std::min(pWinData->mpExtOldText->getLength(), rText.getLength());
    sal_Int32 nDeltaStart = 0;
    while (nDeltaStart < nMinLen)
    {
        if ((*pWinData->mpExtOldText)[nDeltaStart] != rText[nDeltaStart])
            break;
        nDeltaStart++;
    }
    if (pWinData->mpExtOldAttrAry || pTextAttr)
    {
        if (!pWinData->mpExtOldAttrAry || !pTextAttr)
            nDeltaStart = 0;
        else
        {
            sal_Int32 i = 0;
            while (i < nDeltaStart)
            {
                if (pWinData->mpExtOldAttrAry[i] != pTextAttr[i])
                {
                    nDeltaStart = i;
                    break;
                }
                i++;
            }
        }
    }
    if ((nDeltaStart >= nMinLen) && (pWinData->mpExtOldText->getLength() == rText.getLength()))
        bOnlyCursor = true;

    // Call Event and store the information
    CommandExtTextInputData aData(rText, pTextAttr, nCursorPos, nCursorFlags, bOnlyCursor);
    *pWinData->mpExtOldText = rText;
    pWinData->mpExtOldAttrAry.reset();
    if (pTextAttr)
    {
        pWinData->mpExtOldAttrAry.reset(new ExtTextInputAttr[rText.getLength()]);
        std::copy_n(pTextAttr, rText.getLength(), pWinData->mpExtOldAttrAry.get());
    }
    return !ImplCallCommand(pChild, CommandEventId::ExtTextInput, &aData);
}

static bool lcl_HandleEndExtTextInput()
{
    ImplSVData* pSVData = ImplGetSVData();

    if (vcl::Window* pChild = pSVData->mpWinData->mpExtTextInputWin)
    {
        pChild->ImplGetWindowImpl()->mbExtTextInput = false;
        pSVData->mpWinData->mpExtTextInputWin = nullptr;
        ImplWinData* pWinData = pChild->ImplGetWinData();
        pWinData->mpExtOldText.reset();
        pWinData->mpExtOldAttrAry.reset();

        return !ImplCallCommand(pChild, CommandEventId::EndExtTextInput);
    }

    return false;
}

static bool lcl_HandleWheelEvent(vcl::Window* pWindow, const SalWheelMouseEvent& rEvt)
{
    HandleWheelEvent aHandler(pWindow, rEvt);
    return aHandler.HandleEvent(rEvt);
}

static bool lcl_HandleSwipe(vcl::Window* pWindow, const SalGestureSwipeEvent& rEvt)
{
    HandleGestureSwipeEvent aHandler(pWindow, rEvt);
    return aHandler.HandleEvent();
}

static bool lcl_HandleLongPress(vcl::Window* pWindow, const SalGestureLongPressEvent& rEvt)
{
    HandleGestureLongPressEvent aHandler(pWindow, rEvt);
    return aHandler.HandleEvent();
}

static bool lcl_HandleGestureEvent(vcl::Window* pWindow, const SalGestureEvent& rEvent)
{
    HandleGesturePanEvent aHandler(pWindow, rEvent);
    return aHandler.HandleEvent();
}

static bool lcl_HandleGestureZoomEvent(vcl::Window* pWindow, const SalGestureZoomEvent& rEvent)
{
    HandleGestureZoomEvent aHandler(pWindow, rEvent);
    return aHandler.HandleEvent();
}

static bool lcl_HandleGestureRotateEvent(vcl::Window* pWindow, const SalGestureRotateEvent& rEvent)
{
    HandleGestureRotateEvent aHandler(pWindow, rEvent);
    return aHandler.HandleEvent();
}

static void lcl_HandlePaint(vcl::Window* pWindow, const tools::Rectangle& rBoundRect,
                            bool bImmediateUpdate)
{
    // system paint events must be checked for re-mirroring
    pWindow->ImplGetWindowImpl()->mnPaintFlags |= ImplPaintFlags::CheckRtl;

    // trigger paint for all windows that live in the new paint region
    vcl::Region aRegion(rBoundRect);
    pWindow->ImplInvalidateOverlapFrameRegion(aRegion);
    if (!bImmediateUpdate)
        return;

    // #i87663# trigger possible pending resize notifications
    // (GetSizePixel does that for us)
    pWindow->GetSizePixel();
    // force drawing immediately
    pWindow->PaintImmediately();
}

static void lcl_HandleMoveResize(vcl::Window* pWindow, tools::Long nNewWidth,
                                 tools::Long nNewHeight)
{
    lcl_HandleMove(pWindow);
    ImplHandleResize(pWindow, nNewWidth, nNewHeight);
}

bool ImplWindowFrameProc(vcl::Window* _pWindow, SalEvent nEvent, const void* pEvent)
{
    DBG_TESTSOLARMUTEX();

    // Ensure the window survives during this method.
    VclPtr<vcl::Window> pWindow(_pWindow);

    bool bRet = false;

    // #119709# for some unknown reason it is possible to receive events (in this case key events)
    // although the corresponding VCL window must have been destroyed already
    // at least ImplGetWindowImpl() was NULL in these cases, so check this here
    if (pWindow->ImplGetWindowImpl() == nullptr)
        return false;

    switch (nEvent)
    {
        case SalEvent::MouseMove:
            bRet = lcl_HandleSalMouseMove(pWindow, static_cast<SalMouseEvent const*>(pEvent));
            break;
        case SalEvent::ExternalMouseMove:
        {
            MouseEvent const* pMouseEvt = static_cast<MouseEvent const*>(pEvent);
            SalMouseEvent aSalMouseEvent;

            aSalMouseEvent.mnTime = tools::Time::GetSystemTicks();
            aSalMouseEvent.mnX = pMouseEvt->GetPosPixel().X();
            aSalMouseEvent.mnY = pMouseEvt->GetPosPixel().Y();
            aSalMouseEvent.mnButton = 0;
            aSalMouseEvent.mnCode = pMouseEvt->GetButtons() | pMouseEvt->GetModifier();

            bRet = lcl_HandleSalMouseMove(pWindow, &aSalMouseEvent);
        }
        break;
        case SalEvent::MouseLeave:
            bRet = lcl_HandleSalMouseLeave(pWindow, static_cast<SalMouseEvent const*>(pEvent));
            break;
        case SalEvent::MouseButtonDown:
            bRet = lcl_HandleSalMouseButtonDown(pWindow, static_cast<SalMouseEvent const*>(pEvent));
            break;
        case SalEvent::ExternalMouseButtonDown:
        {
            MouseEvent const* pMouseEvt = static_cast<MouseEvent const*>(pEvent);
            SalMouseEvent aSalMouseEvent;

            aSalMouseEvent.mnTime = tools::Time::GetSystemTicks();
            aSalMouseEvent.mnX = pMouseEvt->GetPosPixel().X();
            aSalMouseEvent.mnY = pMouseEvt->GetPosPixel().Y();
            aSalMouseEvent.mnButton = pMouseEvt->GetButtons();
            aSalMouseEvent.mnCode = pMouseEvt->GetButtons() | pMouseEvt->GetModifier();

            bRet = lcl_HandleSalMouseButtonDown(pWindow, &aSalMouseEvent);
        }
        break;
        case SalEvent::MouseButtonUp:
            bRet = lcl_HandleSalMouseButtonUp(pWindow, static_cast<SalMouseEvent const*>(pEvent));
            break;
        case SalEvent::ExternalMouseButtonUp:
        {
            MouseEvent const* pMouseEvt = static_cast<MouseEvent const*>(pEvent);
            SalMouseEvent aSalMouseEvent;

            aSalMouseEvent.mnTime = tools::Time::GetSystemTicks();
            aSalMouseEvent.mnX = pMouseEvt->GetPosPixel().X();
            aSalMouseEvent.mnY = pMouseEvt->GetPosPixel().Y();
            aSalMouseEvent.mnButton = pMouseEvt->GetButtons();
            aSalMouseEvent.mnCode = pMouseEvt->GetButtons() | pMouseEvt->GetModifier();

            bRet = lcl_HandleSalMouseButtonUp(pWindow, &aSalMouseEvent);
        }
        break;
        case SalEvent::MouseActivate:
            bRet = false;
            break;
        case SalEvent::KeyInput:
        {
            SalKeyEvent const* pKeyEvt = static_cast<SalKeyEvent const*>(pEvent);
            bRet = lcl_HandleKey(pWindow, NotifyEventType::KEYINPUT, pKeyEvt->mnCode,
                                 pKeyEvt->mnCharCode, pKeyEvt->mnRepeat, true);
        }
        break;
        case SalEvent::ExternalKeyInput:
        {
            KeyEvent const* pKeyEvt = static_cast<KeyEvent const*>(pEvent);
            bRet = lcl_HandleKey(pWindow, NotifyEventType::KEYINPUT,
                                 pKeyEvt->GetKeyCode().GetFullCode(), pKeyEvt->GetCharCode(),
                                 pKeyEvt->GetRepeat(), false);
        }
        break;
        case SalEvent::KeyUp:
        {
            SalKeyEvent const* pKeyEvt = static_cast<SalKeyEvent const*>(pEvent);
            bRet = lcl_HandleKey(pWindow, NotifyEventType::KEYUP, pKeyEvt->mnCode,
                                 pKeyEvt->mnCharCode, pKeyEvt->mnRepeat, true);
        }
        break;
        case SalEvent::ExternalKeyUp:
        {
            KeyEvent const* pKeyEvt = static_cast<KeyEvent const*>(pEvent);
            bRet = lcl_HandleKey(pWindow, NotifyEventType::KEYUP,
                                 pKeyEvt->GetKeyCode().GetFullCode(), pKeyEvt->GetCharCode(),
                                 pKeyEvt->GetRepeat(), false);
        }
        break;
        case SalEvent::KeyModChange:
            lcl_HandleSalKeyMod(pWindow, static_cast<SalKeyModEvent const*>(pEvent));
            break;

        case SalEvent::InputLanguageChange:
            lcl_HandleInputLanguageChange(pWindow);
            break;

        case SalEvent::MenuActivate:
        case SalEvent::MenuDeactivate:
        case SalEvent::MenuHighlight:
        case SalEvent::MenuCommand:
        case SalEvent::MenuButtonCommand:
            bRet = lcl_HandleMenuEvent(
                pWindow, const_cast<SalMenuEvent*>(static_cast<SalMenuEvent const*>(pEvent)),
                nEvent);
            break;

        case SalEvent::WheelMouse:
            bRet = lcl_HandleWheelEvent(pWindow, *static_cast<const SalWheelMouseEvent*>(pEvent));
            break;

        case SalEvent::Paint:
        {
            SalPaintEvent const* pPaintEvt = static_cast<SalPaintEvent const*>(pEvent);

            if (AllSettings::GetLayoutRTL())
            {
                SalFrame* pSalFrame = pWindow->ImplGetWindowImpl()->mpFrame;
                const_cast<SalPaintEvent*>(pPaintEvt)->mnBoundX
                    = pSalFrame->GetWidth() - pPaintEvt->mnBoundWidth - pPaintEvt->mnBoundX;
            }

            tools::Rectangle aBoundRect(Point(pPaintEvt->mnBoundX, pPaintEvt->mnBoundY),
                                        Size(pPaintEvt->mnBoundWidth, pPaintEvt->mnBoundHeight));
            lcl_HandlePaint(pWindow, aBoundRect, pPaintEvt->mbImmediateUpdate);
        }
        break;

        case SalEvent::Move:
            lcl_HandleMove(pWindow);
            break;

        case SalEvent::Resize:
        {
            const Size aNewSize = pWindow->ImplGetWindowImpl()->mpFrame->GetClientSize();
            ImplHandleResize(pWindow, aNewSize.Width(), aNewSize.Height());
        }
        break;

        case SalEvent::MoveResize:
        {
            SalFrameGeometry g = pWindow->ImplGetWindowImpl()->mpFrame->GetGeometry();
            lcl_HandleMoveResize(pWindow, g.width(), g.height());
        }
        break;

        case SalEvent::ClosePopups:
        {
            lcl_KillOwnPopups(pWindow);
        }
        break;

        case SalEvent::GetFocus:
            lcl_HandleGetFocus(pWindow);
            break;
        case SalEvent::LoseFocus:
            lcl_HandleLoseFocus(pWindow);
            break;

        case SalEvent::Close:
            lcl_HandleClose(pWindow);
            break;

        case SalEvent::Shutdown:
        {
            static bool bInQueryExit = false;

            if (bInQueryExit)
                return false;

            bInQueryExit = true;

            if (GetpApp()->QueryExit())
            {
                // end the message loop
                Application::Quit();
                return false;
            }

            bInQueryExit = false;
            return true;
        }

        case SalEvent::SettingsChanged:
        case SalEvent::PrinterChanged:
        case SalEvent::DisplayChanged:
        case SalEvent::FontChanged:
            lcl_HandleSalSettings(nEvent);
            break;

        case SalEvent::UserEvent:
            lcl_HandleUserEvent(const_cast<ImplSVEvent*>(static_cast<ImplSVEvent const*>(pEvent)));
            break;

        case SalEvent::ExtTextInput:
        {
            SalExtTextInputEvent const* pEvt = static_cast<SalExtTextInputEvent const*>(pEvent);
            bRet = lcl_HandleExtTextInput(pWindow, pEvt->maText, pEvt->mpTextAttr,
                                          pEvt->mnCursorPos, pEvt->mnCursorFlags);
        }
        break;
        case SalEvent::EndExtTextInput:
            bRet = lcl_HandleEndExtTextInput();
            break;
        case SalEvent::ExtTextInputPos:
            lcl_HandleSalExtTextInputPos(pWindow,
                                         const_cast<SalExtTextInputPosEvent*>(
                                             static_cast<SalExtTextInputPosEvent const*>(pEvent)));
            break;
        case SalEvent::InputContextChange:
            bRet = lcl_HandleInputContextChange(pWindow);
            break;
        case SalEvent::ShowDialog:
        {
            ShowDialogId nLOKWindowId
                = static_cast<ShowDialogId>(reinterpret_cast<sal_IntPtr>(pEvent));
            bRet = lcl_HandleShowDialog(pWindow, nLOKWindowId);
        }
        break;
        case SalEvent::SurroundingTextRequest:
            lcl_HandleSalSurroundingTextRequest(
                pWindow, const_cast<SalSurroundingTextRequestEvent*>(
                             static_cast<SalSurroundingTextRequestEvent const*>(pEvent)));
            break;
        case SalEvent::DeleteSurroundingTextRequest:
            lcl_HandleSalDeleteSurroundingTextRequest(
                pWindow, const_cast<SalSurroundingTextSelectionChangeEvent*>(
                             static_cast<SalSurroundingTextSelectionChangeEvent const*>(pEvent)));
            break;
        case SalEvent::SurroundingTextSelectionChange:
        {
            SalSurroundingTextSelectionChangeEvent const* pEvt
                = static_cast<SalSurroundingTextSelectionChangeEvent const*>(pEvent);
            lcl_HandleSurroundingTextSelectionChange(pWindow, pEvt->mnStart, pEvt->mnEnd);
            [[fallthrough]]; // TODO: Fallthrough really intended?
        }
        case SalEvent::StartReconversion:
            lcl_HandleStartReconversion(pWindow);
            break;

        case SalEvent::QueryCharPosition:
            lcl_HandleSalQueryCharPosition(
                pWindow, const_cast<SalQueryCharPositionEvent*>(
                             static_cast<SalQueryCharPositionEvent const*>(pEvent)));
            break;

        case SalEvent::GestureSwipe:
            bRet = lcl_HandleSwipe(pWindow, *static_cast<const SalGestureSwipeEvent*>(pEvent));
            break;

        case SalEvent::GestureLongPress:
            bRet = lcl_HandleLongPress(pWindow,
                                       *static_cast<const SalGestureLongPressEvent*>(pEvent));
            break;

        case SalEvent::ExternalGesture:
        {
            auto const* pGestureEvent = static_cast<GestureEventPan const*>(pEvent);

            SalGestureEvent aSalGestureEvent;
            aSalGestureEvent.mfOffset = pGestureEvent->mnOffset;
            aSalGestureEvent.mnX = pGestureEvent->mnX;
            aSalGestureEvent.mnY = pGestureEvent->mnY;
            aSalGestureEvent.meEventType = pGestureEvent->meEventType;
            aSalGestureEvent.meOrientation = pGestureEvent->meOrientation;

            bRet = lcl_HandleGestureEvent(pWindow, aSalGestureEvent);
        }
        break;
        case SalEvent::GesturePan:
        {
            auto const* aSalGestureEvent = static_cast<SalGestureEvent const*>(pEvent);
            bRet = lcl_HandleGestureEvent(pWindow, *aSalGestureEvent);
        }
        break;
        case SalEvent::GestureZoom:
        {
            const auto* pGestureEvent = static_cast<SalGestureZoomEvent const*>(pEvent);
            bRet = lcl_HandleGestureZoomEvent(pWindow, *pGestureEvent);
        }
        break;
        case SalEvent::GestureRotate:
        {
            const auto* pGestureEvent = static_cast<SalGestureRotateEvent const*>(pEvent);
            bRet = lcl_HandleGestureRotateEvent(pWindow, *pGestureEvent);
        }
        break;
        default:
            SAL_WARN("vcl.layout",
                     "ImplWindowFrameProc(): unknown event (" << static_cast<int>(nEvent) << ")");
            break;
    }

    return bRet;
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
