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

bool ImplCallPreNotify( NotifyEvent& rEvt )
{
    return rEvt.GetWindow()->CompatPreNotify( rEvt );
}

static Point lcl_GetCommandPosition(const VclPtr<vcl::Window>& pChild, Point const * pPos, bool bMouse)
{
    if ( pPos )
        return *pPos;

    if ( bMouse )
        return pChild->GetPointerPosPixel();

    // simulate mouse position at center of window
    Size aSize( pChild->GetOutputSizePixel() );
    return Point( aSize.getWidth() / 2, aSize.getHeight() / 2 );
}

bool ImplCallCommand(const VclPtr<vcl::Window>& pChild, CommandEventId nEvt, void const * pData, bool bMouse, Point const * pPos)
{
    Point aPos = lcl_GetCommandPosition(pChild, pPos, bMouse);

    CommandEvent aCEvt(aPos, nEvt, bMouse, pData);
    NotifyEvent aNCmdEvt(NotifyEventType::COMMAND, pChild, &aCEvt);

    bool bPreNotify = ImplCallPreNotify(aNCmdEvt);

    if (pChild->isDisposed())
        return false;

    if (bPreNotify)
        return false;

    pChild->ImplGetWindowImpl()->mbCommand = false;
    pChild->Command( aCEvt );

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
    const vcl::Window* pFloatWin = pSVData->mpWinData->mpFirstFloat;

    return pFloatWin && pFrameWindow->ImplIsWindowOrChild(pFloatWin, true);
}

static bool lcl_CanCloseOnAppFocus()
{
    ImplSVData* pSVData = ImplGetSVData();
    return bool(pSVData->mpWinData->mpFirstFloat->GetPopupModeFlags() & FloatWinPopupFlags::NoAppFocusClose);
}

static void lcl_EndPopupMode()
{
    ImplSVData* pSVData = ImplGetSVData();
    pSVData->mpWinData->mpFirstFloat->EndPopupMode(FloatWinPopupEndFlags::Cancel | FloatWinPopupEndFlags::CloseAll);
}

static void lcl_KillOwnPopups(vcl::Window const * pWindow)
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
        || (pWindow->GetStyle() & WB_OWNERDRAWDECORATION)  // synchronous resize for ownerdraw decorated windows (toolbars)
        || pWindow->ImplGetWindowImpl()->mbFloatWin)      // synchronous resize for floating windows, #i43799#
    {
        return false;
    }

    return true;
}

static bool lcl_ShouldStartResizeTimer(const vcl::Window* pWindow)
{
    if (!lcl_ShouldBufferResize(pWindow))
        return false;

    if (pWindow->ImplGetWindowImpl()->mpClientWindow)
    {
        // #i42750# presentation wants to be informed about resize
        // as early as possible
        WorkWindow* pWorkWindow = dynamic_cast<WorkWindow*>(pWindow->ImplGetWindowImpl()->mpClientWindow.get());
        if (!pWorkWindow || pWorkWindow->IsPresentationMode())
            return false;
    }
    else
    {
        WorkWindow* pWorkWindow = dynamic_cast<WorkWindow*>(const_cast<vcl::Window*>(pWindow));
        if (!pWorkWindow || pWorkWindow->IsPresentationMode())
            return false;
    }

    return true;
}

static bool lcl_ShouldSkipResizePropagation(const vcl::Window* pWindow)
{
    return !pWindow->IsVisible() && !pWindow->ImplGetWindow()->ImplGetWindowImpl()->mbAllResize &&
        !(pWindow->ImplGetWindowImpl()->mbFrame && pWindow->ImplGetWindowImpl()->mpClientWindow);    // propagate resize for system border windows
}

static void lcl_HandleResizePropagation(vcl::Window* pWindow)
{
    if (lcl_ShouldSkipResizePropagation(pWindow))
        pWindow->ImplGetWindowImpl()->mbCallResize = true;

    if (lcl_ShouldStartResizeTimer(pWindow))
        pWindow->ImplGetWindowImpl()->mpFrameData->maResizeIdle.Start();
    else
        pWindow->ImplCallResize(); // otherwise menus cannot be positioned
}

static bool lcl_HasSizeChanged(const vcl::Window* pWindow, tools::Long nNewWidth, tools::Long nNewHeight)
{
    return (nNewWidth != pWindow->GetOutputSizePixel().Width()) || (nNewHeight != pWindow->GetOutDev()->GetOutputHeightPixel());
}

static void lcl_HandleResizeDimensions(vcl::Window* pWindow, tools::Long nNewWidth, tools::Long nNewHeight)
{
    bool bChanged = lcl_HasSizeChanged(pWindow, nNewWidth, nNewHeight);

    if (!((nNewWidth > 0 && nNewHeight > 0) || (pWindow->ImplGetWindow()->ImplGetWindowImpl()->mbAllResize && bChanged)))
        return;

    pWindow->GetOutDev()->SetOutputWidthPixel(nNewWidth);
    pWindow->GetOutDev()->SetOutputHeightPixel(nNewHeight);
    pWindow->ImplGetWindowImpl()->mbWaitSystemResize = false;

    if ( pWindow->IsReallyVisible() )
        vcl::clipping::setClipFlag(*pWindow);

    lcl_HandleResizePropagation(pWindow);

    if (pWindow->SupportsDoubleBuffering() && pWindow->ImplGetWindowImpl()->mbFrame)
    {
        // Propagate resize for the frame's buffer.
        pWindow->ImplGetWindowImpl()->mpFrameData->mpBuffer->SetOutputSizePixel(pWindow->GetOutputSizePixel());
    }
}

static bool lcl_CanMoveOrSize(const vcl::Window* pWindow, tools::Long nNewWidth, tools::Long nNewHeight)
{
    return lcl_HasSizeChanged(pWindow, nNewWidth, nNewHeight) && (pWindow->GetStyle() & (WB_MOVEABLE | WB_SIZEABLE));
}

static void lcl_HandleMinimizedState(vcl::Window* pWindow, tools::Long nNewWidth, tools::Long nNewHeight)
{
    bool bMinimized = (nNewWidth <= 0) || (nNewHeight <= 0);
    if (bMinimized != pWindow->ImplGetWindowImpl()->mpFrameData->mbMinimized)
        pWindow->ImplGetWindowImpl()->mpFrameWindow->ImplNotifyIconifiedState(bMinimized);
    pWindow->ImplGetWindowImpl()->mpFrameData->mbMinimized = bMinimized;
}

void ImplHandleResize( vcl::Window* pWindow, tools::Long nNewWidth, tools::Long nNewHeight )
{
    if (lcl_CanMoveOrSize(pWindow, nNewWidth, nNewHeight))
    {
        lcl_KillOwnPopups(pWindow);

        if (pWindow->ImplGetWindow() != ImplGetSVHelpData().mpHelpWin)
            ImplDestroyHelpWindow(true);
    }

    lcl_HandleResizeDimensions(pWindow, nNewWidth, nNewHeight);

    pWindow->ImplGetWindowImpl()->mpFrameData->mbNeedSysWindow = (nNewWidth < IMPL_MIN_NEEDSYSWIN) ||
                                            (nNewHeight < IMPL_MIN_NEEDSYSWIN);

    lcl_HandleMinimizedState(pWindow, nNewWidth, nNewHeight);
}

static void lcl_HandleMove( vcl::Window* pWindow )
{
    if( pWindow->ImplGetWindowImpl()->mbFrame && pWindow->ImplIsFloatingWindow() && pWindow->IsReallyVisible() )
    {
        static_cast<FloatingWindow*>(pWindow)->EndPopupMode( FloatWinPopupEndFlags::TearOff );
        pWindow->ImplCallMove();
    }

    if( pWindow->GetStyle() & (WB_MOVEABLE|WB_SIZEABLE) )
    {
        lcl_KillOwnPopups( pWindow );
        if( pWindow->ImplGetWindow() != ImplGetSVHelpData().mpHelpWin )
            ImplDestroyHelpWindow( true );
    }

    if ( pWindow->IsVisible() )
        pWindow->ImplCallMove();
    else
        pWindow->ImplGetWindowImpl()->mbCallMove = true; // make sure the framepos will be updated on the next Show()

    if ( pWindow->ImplGetWindowImpl()->mbFrame && pWindow->ImplGetWindowImpl()->mpClientWindow )
        pWindow->ImplGetWindowImpl()->mpClientWindow->ImplCallMove();   // notify client to update geometry

}

static void lcl_ActivateFloatingWindows( vcl::Window const * pWindow, bool bActive )
{
    // First check all overlapping windows
    vcl::Window* pTempWindow = pWindow->ImplGetWindowImpl()->mpHierarchy->mpFirstOverlap;
    while ( pTempWindow )
    {
        if ( pTempWindow->GetActivateMode() == ActivateModeFlags::NONE )
        {
            if ( (pTempWindow->GetType() == WindowType::BORDERWINDOW) &&
                 (pTempWindow->ImplGetWindow()->GetType() == WindowType::FLOATINGWINDOW) )
                static_cast<ImplBorderWindow*>(pTempWindow)->SetDisplayActive( bActive );
        }

        lcl_ActivateFloatingWindows( pTempWindow, bActive );
        pTempWindow = pTempWindow->ImplGetWindowImpl()->mpHierarchy->mpNext;
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

bool vcl::Window::ImplCanReceiveFocus() const
{
    return IsInputEnabled() && !IsInModalMode();
}

bool vcl::Window::ImplSyncDelayedFocus()
{
    ImplGetWindowImpl()->mpFrameData->mnFocusId = nullptr;

    bool bHasFocus = ImplGetWindowImpl()->mpFrameData->mbHasFocus || ImplGetWindowImpl()->mpFrameData->mbSysObjFocus;

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

    bool bHandled = false;

    if (ImplCanReceiveFocus())
        bHandled = ImplRestoreFocusToWindow();

    if (bHandled)
        return true;

    ImplSVData* pSVData = ImplGetSVData();
    vcl::Window* pTopLevelWindow = ImplGetWindowImpl()->mpFrameData->mpFocusWin->ImplGetFirstOverlapWindow();

    if ((!pTopLevelWindow->IsInputEnabled() || pTopLevelWindow->IsInModalMode())
        && !pSVData->mpWinData->mpExecuteDialogs.empty())
        pSVData->mpWinData->mpExecuteDialogs.back()->ToTop(ToTopFlags::RestoreWhenMin | ToTopFlags::GrabFocusOnly);
    else
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
    pSVData->mpWinData->mpExecuteDialogs.back()->ToTop(ToTopFlags::RestoreWhenMin | ToTopFlags::GrabFocusOnly);
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

    bool bHandled = false;

    if (ImplCanReceiveFocus())
        bHandled = ImplRestoreFocusToWindow();

    if (bHandled)
        return true;

    vcl::Window* pTopLevelWindow = ImplGetWindowImpl()->mpFrameData->mpFocusWin->ImplGetFirstOverlapWindow();

    if (lcl_ShouldBringDialogToTop(pTopLevelWindow))
        lcl_BringExecutingDialogToTop();
    else
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
    vcl::Window* pOverlapWindow = ImplGetFirstOverlapWindow();

    if (pOverlapWindow && pOverlapWindow->ImplGetWindowImpl())
        pOverlapWindow->ImplGetWindowImpl()->mpLastFocusWindow = this;

    pSVData->mpWinData->mpFocusWin = nullptr;

    if (ImplGetWindowImpl() && ImplGetWindowImpl()->mpCursor)
        ImplGetWindowImpl()->mpCursor->ImplHide();
}

static bool lcl_CanDeactivateWindow(const vcl::Window* pOverlapWindow, const vcl::Window* pRealWindow)
{
    return pOverlapWindow && pOverlapWindow->ImplGetWindowImpl() &&
           pRealWindow && pRealWindow->ImplGetWindowImpl();
}

void vcl::Window::ImplDeactivateFocus()
{
    vcl::Window* pOldOverlapWindow = ImplGetFirstOverlapWindow();
    vcl::Window* pOldRealWindow = pOldOverlapWindow->ImplGetWindow();

    if (!lcl_CanDeactivateWindow(pOldOverlapWindow, pOldRealWindow))
        return;

    pOldOverlapWindow->ImplGetWindowImpl()->mbActive = false;
    pOldOverlapWindow->Deactivate();

    if (pOldRealWindow == pOldOverlapWindow)
        return;

    pOldRealWindow->ImplGetWindowImpl()->mbActive = false;
    pOldRealWindow->Deactivate();
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

    vcl::Window* pFocusWin = ImplGetWindowImpl()->mpFrameData->mpFocusWin;
    if (pFocusWin)
        pFocusWin->ImplProcessFocusLoss();

    // Redraw all floating window inactive
    if ( ImplGetWindowImpl()->mpFrameData->mbStartFocusState != bHasFocus )
        lcl_ActivateFloatingWindows( this, bHasFocus );
}

static void lcl_HandleGetFocus( vcl::Window* pWindow )
{
    if (!pWindow || !pWindow->ImplGetWindowImpl() || !pWindow->ImplGetWindowImpl()->mpFrameData)
        return;

    pWindow->ImplGetWindowImpl()->mpFrameData->mbHasFocus = true;

    // execute Focus-Events after a delay, such that SystemChildWindows
    // do not blink when they receive focus
    if (pWindow->ImplGetWindowImpl()->mpFrameData->mnFocusId)
        return;

    pWindow->ImplGetWindowImpl()->mpFrameData->mbStartFocusState = !pWindow->ImplGetWindowImpl()->mpFrameData->mbHasFocus;
    pWindow->ImplGetWindowImpl()->mpFrameData->mnFocusId = Application::PostUserEvent( LINK( pWindow, vcl::Window, ImplAsyncFocusHdl ), nullptr, true);
    vcl::Window* pFocusWin = pWindow->ImplGetWindowImpl()->mpFrameData->mpFocusWin;
    if ( pFocusWin && pFocusWin->ImplGetWindowImpl()->mpCursor )
        pFocusWin->ImplGetWindowImpl()->mpCursor->ImplShow();
}

static void lcl_HandleLoseFocus( vcl::Window* pWindow )
{
    if (!pWindow)
        return;

    ImplSVData* pSVData = ImplGetSVData();

    // Abort the autoscroll if the frame loses focus
    if (pSVData->mpWinData->mpAutoScrollWin)
        pSVData->mpWinData->mpAutoScrollWin->EndAutoScroll();

    // Abort tracking if the frame loses focus
    if (pSVData->mpWinData->mpTrackWin)
    {
        if (pSVData->mpWinData->mpTrackWin->ImplGetWindowImpl() &&
            pSVData->mpWinData->mpTrackWin->ImplGetWindowImpl()->mpFrameWindow == pWindow)
            pSVData->mpWinData->mpTrackWin->EndTracking(TrackingEventFlags::Cancel);
    }

    if (pWindow->ImplGetWindowImpl() && pWindow->ImplGetWindowImpl()->mpFrameData)
    {
        pWindow->ImplGetWindowImpl()->mpFrameData->mbHasFocus = false;

        // execute Focus-Events after a delay, such that SystemChildWindows
        // do not flicker when they receive focus
        if ( !pWindow->ImplGetWindowImpl()->mpFrameData->mnFocusId )
        {
            pWindow->ImplGetWindowImpl()->mpFrameData->mbStartFocusState = !pWindow->ImplGetWindowImpl()->mpFrameData->mbHasFocus;
            pWindow->ImplGetWindowImpl()->mpFrameData->mnFocusId = Application::PostUserEvent( LINK( pWindow, vcl::Window, ImplAsyncFocusHdl ), nullptr, true );
        }

        vcl::Window* pFocusWin = pWindow->ImplGetWindowImpl()->mpFrameData->mpFocusWin;
        if ( pFocusWin && pFocusWin->ImplGetWindowImpl()->mpCursor )
            pFocusWin->ImplGetWindowImpl()->mpCursor->ImplHide();
    }

    // Make sure that no menu is visible when a toplevel window loses focus.
    VclPtr<FloatingWindow> pFirstFloat = pSVData->mpWinData->mpFirstFloat;
    if (pFirstFloat && pFirstFloat->IsMenuFloatingWindow() && !pWindow->GetParent())
    {
        pFirstFloat->EndPopupMode(FloatWinPopupEndFlags::Cancel | FloatWinPopupEndFlags::CloseAll);
    }
}

namespace {

struct DelayedCloseEvent
{
    VclPtr<vcl::Window> pWindow;
};

}

static void lcl_DelayedCloseEventLink( void* pCEvent, void* )
{
    DelayedCloseEvent* pEv = static_cast<DelayedCloseEvent*>(pCEvent);

    if( ! pEv->pWindow->isDisposed() )
    {
        // dispatch to correct window type
        if( pEv->pWindow->IsSystemWindow() )
            static_cast<SystemWindow*>(pEv->pWindow.get())->Close();
        else if( pEv->pWindow->IsDockingWindow() )
            static_cast<DockingWindow*>(pEv->pWindow.get())->Close();
    }
    delete pEv;
}

static void lcl_HandleClose( const vcl::Window* pWindow )
{
    ImplSVData* pSVData = ImplGetSVData();

    bool bWasPopup = false;
    if( pWindow->ImplIsFloatingWindow() &&
        static_cast<const FloatingWindow*>(pWindow)->ImplIsInPrivatePopupMode() )
    {
        bWasPopup = true;
    }

    // on Close stop all floating modes and end popups
    if (pSVData->mpWinData->mpFirstFloat)
    {
        FloatingWindow* pLastLevelFloat;
        pLastLevelFloat = pSVData->mpWinData->mpFirstFloat->ImplFindLastLevelFloat();
        pLastLevelFloat->EndPopupMode( FloatWinPopupEndFlags::Cancel | FloatWinPopupEndFlags::CloseAll );
    }
    if ( ImplGetSVHelpData().mbExtHelpMode )
        Help::EndExtHelp();
    if ( ImplGetSVHelpData().mpHelpWin )
        ImplDestroyHelpWindow( false );
    // AutoScrollMode
    if (pSVData->mpWinData->mpAutoScrollWin)
        pSVData->mpWinData->mpAutoScrollWin->EndAutoScroll();

    if (pSVData->mpWinData->mpTrackWin)
        pSVData->mpWinData->mpTrackWin->EndTracking( TrackingEventFlags::Cancel | TrackingEventFlags::Key );

    if (bWasPopup)
        return;

    vcl::Window *pWin = pWindow->ImplGetWindow();
    SystemWindow* pSysWin = dynamic_cast<SystemWindow*>(pWin);
    if (pSysWin)
    {
        // See if the custom close handler is set.
        const Link<SystemWindow&,void>& rLink = pSysWin->GetCloseHdl();
        if (rLink.IsSet())
        {
            rLink.Call(*pSysWin);
            return;
        }
    }

    // check whether close is allowed
    if ( pWin->IsEnabled() && pWin->IsInputEnabled() && !pWin->IsInModalMode() )
    {
        DelayedCloseEvent* pEv = new DelayedCloseEvent;
        pEv->pWindow = pWin;
        Application::PostUserEvent( LINK_NONMEMBER( pEv, lcl_DelayedCloseEventLink ) );
    }
}

static void lcl_HandleUserEvent( ImplSVEvent* pSVEvent )
{
    if (!pSVEvent)
        return;

    if ( pSVEvent->mbCall )
        pSVEvent->maLink.Call( pSVEvent->mpData );

    delete pSVEvent;
}

MouseEventModifiers ImplGetMouseMoveMode( SalMouseEvent const * pEvent )
{
    MouseEventModifiers nMode = MouseEventModifiers::NONE;
    if ( !pEvent->mnCode )
        nMode |= MouseEventModifiers::SIMPLEMOVE;
    if ( (pEvent->mnCode & MOUSE_LEFT) && !(pEvent->mnCode & KEY_MOD1) )
        nMode |= MouseEventModifiers::DRAGMOVE;
    if ( (pEvent->mnCode & MOUSE_LEFT) && (pEvent->mnCode & KEY_MOD1) )
        nMode |= MouseEventModifiers::DRAGCOPY;
    return nMode;
}

MouseEventModifiers ImplGetMouseButtonMode( SalMouseEvent const * pEvent )
{
    MouseEventModifiers nMode = MouseEventModifiers::NONE;
    if ( pEvent->mnButton == MOUSE_LEFT )
        nMode |= MouseEventModifiers::SIMPLECLICK;
    if ( (pEvent->mnButton == MOUSE_LEFT) && !(pEvent->mnCode & (MOUSE_MIDDLE | MOUSE_RIGHT)) )
        nMode |= MouseEventModifiers::SELECT;
    if ( (pEvent->mnButton == MOUSE_LEFT) && (pEvent->mnCode & KEY_MOD1) &&
         !(pEvent->mnCode & (MOUSE_MIDDLE | MOUSE_RIGHT | KEY_SHIFT)) )
        nMode |= MouseEventModifiers::MULTISELECT;
    if ( (pEvent->mnButton == MOUSE_LEFT) && (pEvent->mnCode & KEY_SHIFT) &&
         !(pEvent->mnCode & (MOUSE_MIDDLE | MOUSE_RIGHT | KEY_MOD1)) )
        nMode |= MouseEventModifiers::RANGESELECT;
    return nMode;
}

static bool lcl_HandleSalMouseLeave( vcl::Window* pWindow, SalMouseEvent const * pEvent )
{
    return ImplHandleMouseEvent( pWindow, NotifyEventType::MOUSEMOVE, true,
                                 pEvent->mnX, pEvent->mnY,
                                 pEvent->mnTime, pEvent->mnCode,
                                 ImplGetMouseMoveMode( pEvent ) );
}

static bool lcl_HandleSalMouseMove( vcl::Window* pWindow, SalMouseEvent const * pEvent )
{
    return ImplHandleMouseEvent( pWindow, NotifyEventType::MOUSEMOVE, false,
                                 pEvent->mnX, pEvent->mnY,
                                 pEvent->mnTime, pEvent->mnCode,
                                 ImplGetMouseMoveMode( pEvent ) );
}

static bool lcl_HandleSalMouseButtonDown( vcl::Window* pWindow, SalMouseEvent const * pEvent )
{
    return ImplHandleMouseEvent( pWindow, NotifyEventType::MOUSEBUTTONDOWN, false,
                                 pEvent->mnX, pEvent->mnY,
                                 pEvent->mnTime,
#ifdef MACOSX
                 pEvent->mnButton | (pEvent->mnCode & (KEY_SHIFT | KEY_MOD1 | KEY_MOD2 | KEY_MOD3)),
#else
                                 pEvent->mnButton | (pEvent->mnCode & (KEY_SHIFT | KEY_MOD1 | KEY_MOD2)),
#endif
                                 ImplGetMouseButtonMode( pEvent ) );
}

static bool lcl_HandleSalMouseButtonUp( vcl::Window* pWindow, SalMouseEvent const * pEvent )
{
    return ImplHandleMouseEvent( pWindow, NotifyEventType::MOUSEBUTTONUP, false,
                                 pEvent->mnX, pEvent->mnY,
                                 pEvent->mnTime,
#ifdef MACOSX
                 pEvent->mnButton | (pEvent->mnCode & (KEY_SHIFT | KEY_MOD1 | KEY_MOD2 | KEY_MOD3)),
#else
                                 pEvent->mnButton | (pEvent->mnCode & (KEY_SHIFT | KEY_MOD1 | KEY_MOD2)),
#endif
                                 ImplGetMouseButtonMode( pEvent ) );
}

static bool lcl_HandleMenuEvent( vcl::Window const * pWindow, SalMenuEvent* pEvent, SalEvent nEvent )
{
    // Find SystemWindow and its Menubar and let it dispatch the command
    vcl::Window *pWin = pWindow->ImplGetWindowImpl()->mpHierarchy->mpFirstChild;
    while ( pWin )
    {
        if ( pWin->ImplGetWindowImpl()->mbSysWin )
            break;
        pWin = pWin->ImplGetWindowImpl()->mpHierarchy->mpNext;
    }

    if (!pWin)
        return false;

    MenuBar *pMenuBar = static_cast<SystemWindow*>(pWin)->GetMenuBar();

    if (!pMenuBar)
        return false;

    switch( nEvent )
    {
        case SalEvent::MenuActivate:
            pMenuBar->HandleMenuActivateEvent( static_cast<Menu*>(pEvent->mpMenu) );
            return true;

        case SalEvent::MenuDeactivate:
            pMenuBar->HandleMenuDeActivateEvent( static_cast<Menu*>(pEvent->mpMenu) );
            return true;

        case SalEvent::MenuHighlight:
            return pMenuBar->HandleMenuHighlightEvent( static_cast<Menu*>(pEvent->mpMenu), pEvent->mnId );

        case SalEvent::MenuButtonCommand:
            return pMenuBar->HandleMenuButtonEvent( pEvent->mnId );

        case SalEvent::MenuCommand:
            return pMenuBar->HandleMenuCommandEvent( static_cast<Menu*>(pEvent->mpMenu), pEvent->mnId );

        default:
            break;
    }

    return false;
}

static vcl::Window* lcl_GetKeyInputWindow( vcl::Window* pWindow )
{
    ImplSVData* pSVData = ImplGetSVData();

    // determine last input time
    pSVData->maAppData.mnLastInputTime = tools::Time::GetSystemTicks();

    // #127104# workaround for destroyed windows
    if( pWindow->ImplGetWindowImpl() == nullptr )
        return nullptr;

    // find window - is every time the window which has currently the
    // focus or the last time the focus.

    // the first floating window always has the focus, try it, or any parent floating windows, first
    vcl::Window* pChild = pSVData->mpWinData->mpFirstFloat;
    while (pChild)
    {
        if (pChild->ImplGetWindowImpl())
        {
            if (pChild->ImplGetWindowImpl()->mbFloatWin)
            {
                if (static_cast<FloatingWindow *>(pChild)->GrabsFocus())
                    break;
            }
            else if (pChild->ImplGetWindowImpl()->mbDockWin)
            {
                vcl::Window* pParent = pChild->GetWindow(GetWindowType::RealParent);
                if (pParent && pParent->ImplGetWindowImpl()->mbFloatWin &&
                    static_cast<FloatingWindow *>(pParent)->GrabsFocus())
                    break;
            }
        }
        pChild = pChild->GetParent();
    }

    if (!pChild)
        pChild = pWindow;

    pChild = pChild->ImplGetWindowImpl() && pChild->ImplGetWindowImpl()->mpFrameData ? pChild->ImplGetWindowImpl()->mpFrameData->mpFocusWin.get() : nullptr;

    // no child - then no input
    if ( !pChild )
        return nullptr;

    // We call also KeyInput if we haven't the focus, because on Unix
    // system this is often the case when a Lookup Choice Window has
    // the focus - because this windows send the KeyInput directly to
    // the window without resetting the focus

    // no keyinput to disabled windows
    if ( !pChild->IsEnabled() || !pChild->IsInputEnabled() || pChild->IsInModalMode() )
        return nullptr;

    return pChild;
}

static bool lcl_HandleInputContextChange( vcl::Window* pWindow )
{
    vcl::Window* pChild = lcl_GetKeyInputWindow( pWindow );
    CommandInputContextData aData;
    return !ImplCallCommand( pChild, CommandEventId::InputContextChange, &aData );
}

static void lcl_HandleSalKeyMod( vcl::Window* pWindow, SalKeyModEvent const * pEvent )
{
    ImplSVData* pSVData = ImplGetSVData();
    vcl::Window* pTrackWin = pSVData->mpWinData->mpTrackWin;
    if ( pTrackWin )
        pWindow = pTrackWin;
#ifdef MACOSX
    sal_uInt16 nOldCode = pWindow->ImplGetWindowImpl()->mpFrameData->mnMouseCode & (KEY_SHIFT | KEY_MOD1 | KEY_MOD2 | KEY_MOD3);
#else
    sal_uInt16 nOldCode = pWindow->ImplGetWindowImpl()->mpFrameData->mnMouseCode & (KEY_SHIFT | KEY_MOD1 | KEY_MOD2);
#endif
    sal_uInt16 nNewCode = pEvent->mnCode;
    if ( nOldCode != nNewCode )
    {
#ifdef MACOSX
        nNewCode |= pWindow->ImplGetWindowImpl()->mpFrameData->mnMouseCode & ~(KEY_SHIFT | KEY_MOD1 | KEY_MOD2 | KEY_MOD3);
#else
        nNewCode |= pWindow->ImplGetWindowImpl()->mpFrameData->mnMouseCode & ~(KEY_SHIFT | KEY_MOD1 | KEY_MOD2);
#endif
        pWindow->ImplGetWindowImpl()->mpFrameWindow->ImplCallMouseMove( nNewCode, true );
    }

    // #105224# send commandevent to allow special treatment of Ctrl-LeftShift/Ctrl-RightShift etc.
    // + auto-accelerator feature, tdf#92630

    // try to find a key input window...
    vcl::Window* pChild = lcl_GetKeyInputWindow( pWindow );
    //...otherwise fail safe...
    if (!pChild)
        pChild = pWindow;

    CommandModKeyData data( pEvent->mnModKeyCode, pEvent->mbDown );
    ImplCallCommand( pChild, CommandEventId::ModKeyChange, &data );
}

static void lcl_HandleInputLanguageChange( vcl::Window* pWindow )
{
    // find window
    vcl::Window* pChild = lcl_GetKeyInputWindow( pWindow );
    if ( !pChild )
        return;

    ImplCallCommand( pChild, CommandEventId::InputLanguageChange );
}

static void lcl_HandleSalSettings( SalEvent nEvent )
{
    Application* pApp = GetpApp();
    if ( !pApp )
        return;

    if ( nEvent == SalEvent::SettingsChanged )
    {
        AllSettings aSettings = Application::GetSettings();
        Application::MergeSystemSettings( aSettings );
        pApp->OverrideSystemSettings( aSettings );
        Application::SetSettings( aSettings );
    }
    else
    {
        DataChangedEventType nType;
        switch ( nEvent )
        {
            case SalEvent::PrinterChanged:
                ImplDeletePrnQueueList();
                nType = DataChangedEventType::PRINTER;
                break;
            case SalEvent::DisplayChanged:
                nType = DataChangedEventType::DISPLAY;
                break;
            case SalEvent::FontChanged:
                OutputDevice::ImplUpdateAllFontData( true );
                nType = DataChangedEventType::FONTS;
                break;
            default:
                return;
        }

        DataChangedEvent aDCEvt( nType );
        Application::ImplCallEventListenersApplicationDataChanged(&aDCEvt);
        Application::NotifyAllWindows( aDCEvt );
    }
}

static void lcl_HandleExtTextInputPos( vcl::Window* pWindow,
                                       tools::Rectangle& rRect, tools::Long& rInputWidth,
                                       bool * pVertical )
{
    ImplSVData* pSVData = ImplGetSVData();
    vcl::Window* pChild = pSVData->mpWinData->mpExtTextInputWin;

    if ( !pChild )
        pChild = lcl_GetKeyInputWindow( pWindow );
    else
    {
        // Test, if the Window is related to the frame
        if ( !pWindow->ImplIsWindowOrChild( pChild ) )
            pChild = lcl_GetKeyInputWindow( pWindow );
    }

    if ( pChild )
    {
        const OutputDevice *pChildOutDev = pChild->GetOutDev();
        ImplCallCommand( pChild, CommandEventId::CursorPos );
        const tools::Rectangle* pRect = pChild->GetCursorRect();
        if ( pRect )
        {
            rRect = pChildOutDev->GetMapper().LogicToDevicePixel(*pRect, pChildOutDev->GetMappingPolicy());
        }
        else
        {
            vcl::Cursor* pCursor = pChild->GetCursor();
            if ( pCursor )
            {
                auto aPos = pChildOutDev->convertTo<vcl::DevicePoint>(vcl::LogicPoint(pCursor->GetPos()), pChildOutDev->GetMapMode());
                auto aSize = pChild->convertTo<vcl::WindowSize>(vcl::LogicSize(pCursor->GetSize()), pChild->GetMapMode());

                if (!aSize->Width())
                    aSize->setWidth(pChild->GetSettings().GetStyleSettings().GetCursorSize());

                rRect = tools::Rectangle(aPos.get(), aSize.get());
            }
            else
                rRect = tools::Rectangle( Point( pChild->GetDeviceOriginX(), pChild->GetDeviceOriginY() ), Size() );
        }
        rInputWidth = pChild->LogicWidthToDevicePixel(pChild->GetCursorExtTextInputWidth());
        if ( !rInputWidth )
            rInputWidth = rRect.GetWidth();
    }
    if (pVertical != nullptr)
        *pVertical
            = pChild != nullptr && pChild->GetInputContext().GetFont().IsVertical();
}

static void lcl_HandleSalExtTextInputPos( vcl::Window* pWindow, SalExtTextInputPosEvent* pEvt )
{
    tools::Rectangle aCursorRect;
    lcl_HandleExtTextInputPos( pWindow, aCursorRect, pEvt->mnExtWidth, &pEvt->mbVertical );
    if ( aCursorRect.IsEmpty() )
    {
        pEvt->mnX       = -1;
        pEvt->mnY       = -1;
        pEvt->mnWidth   = -1;
        pEvt->mnHeight  = -1;
    }
    else
    {
        pEvt->mnX       = aCursorRect.Left();
        pEvt->mnY       = aCursorRect.Top();
        pEvt->mnWidth   = aCursorRect.GetWidth();
        pEvt->mnHeight  = aCursorRect.GetHeight();
    }
}

static bool lcl_HandleShowDialog( vcl::Window* pWindow, ShowDialogId nDialogId )
{
    if( ! pWindow )
        return false;

    if( pWindow->GetType() == WindowType::BORDERWINDOW )
    {
        vcl::Window* pWrkWin = pWindow->GetWindow( GetWindowType::Client );
        if( pWrkWin )
            pWindow = pWrkWin;
    }
    CommandDialogData aCmdData( nDialogId );
    return ImplCallCommand( pWindow, CommandEventId::ShowDialog, &aCmdData );
}

static void lcl_HandleSalSurroundingTextRequest( vcl::Window *pWindow,
                         SalSurroundingTextRequestEvent *pEvt )
{
    vcl::Window* pChild = lcl_GetKeyInputWindow( pWindow );
    if ( !pChild )
    {
        pEvt->maText.clear();
        pEvt->mnStart = 0;
        pEvt->mnEnd = 0;
        return;
    }

    pEvt->maText = pChild->GetSurroundingText();
    Selection aSelRange = pChild->GetSurroundingTextSelection();

    sal_uLong nSelectionAnchorPos = 0;
    sal_uLong nCursorPos = 0;

    if( aSelRange.Min() < 0 )
        nSelectionAnchorPos = 0;
    else if( aSelRange.Min() > pEvt->maText.getLength() )
        nSelectionAnchorPos = pEvt->maText.getLength();
    else
        nSelectionAnchorPos = aSelRange.Min();

    if( aSelRange.Max() < 0 )
        nCursorPos = 0;
    else if( aSelRange.Max() > pEvt->maText.getLength() )
        nCursorPos = pEvt->maText.getLength();
    else
        nCursorPos = aSelRange.Max();

    pEvt->mnCursorPos = nCursorPos;
    pEvt->mnStart = std::min(nSelectionAnchorPos, nCursorPos);
    pEvt->mnEnd = std::max(nSelectionAnchorPos, nCursorPos);
}

static void lcl_HandleSalDeleteSurroundingTextRequest( vcl::Window *pWindow,
                         SalSurroundingTextSelectionChangeEvent *pEvt )
{
    vcl::Window* pChild = lcl_GetKeyInputWindow( pWindow );

    Selection aSelection(pEvt->mnStart, pEvt->mnEnd);
    if (pChild && pChild->DeleteSurroundingText(aSelection))
    {
        pEvt->mnStart = aSelection.Min();
        pEvt->mnEnd = aSelection.Max();
    }
    else
    {
        pEvt->mnStart = pEvt->mnEnd = SAL_MAX_UINT32;
    }
}

static void lcl_HandleSurroundingTextSelectionChange( vcl::Window *pWindow,
                              sal_uLong nStart,
                              sal_uLong nEnd )
{
    vcl::Window* pChild = lcl_GetKeyInputWindow( pWindow );
    if( pChild )
    {
        CommandSelectionChangeData data( nStart, nEnd );
        ImplCallCommand( pChild, CommandEventId::SelectionChange, &data );
    }
}

static void lcl_HandleStartReconversion( vcl::Window *pWindow )
{
    vcl::Window* pChild = lcl_GetKeyInputWindow( pWindow );
    if( pChild )
        ImplCallCommand( pChild, CommandEventId::PrepareReconversion );
}

static void lcl_HandleSalQueryCharPosition( vcl::Window *pWindow,
                                            SalQueryCharPositionEvent *pEvt )
{
    pEvt->mbValid = false;
    pEvt->mbVertical = false;
    pEvt->maCursorBound = AbsoluteScreenPixelRectangle();

    ImplSVData* pSVData = ImplGetSVData();
    vcl::Window* pChild = pSVData->mpWinData->mpExtTextInputWin;

    if ( !pChild )
        pChild = lcl_GetKeyInputWindow( pWindow );
    else
    {
        // Test, if the Window is related to the frame
        if ( !pWindow->ImplIsWindowOrChild( pChild ) )
            pChild = lcl_GetKeyInputWindow( pWindow );
    }

    if( !pChild )
        return;

    ImplCallCommand( pChild, CommandEventId::QueryCharPosition );

    ImplWinData* pWinData = pChild->ImplGetWinData();
    if ( !(pWinData->mpCompositionCharRects && pEvt->mnCharPos < o3tl::make_unsigned( pWinData->mnCompositionCharRects )) )
        return;

    const OutputDevice *pChildOutDev = pChild->GetOutDev();
    const tools::Rectangle& aRect = pWinData->mpCompositionCharRects[ pEvt->mnCharPos ];
    tools::Rectangle aDeviceRect = pChildOutDev->GetMapper().LogicToDevicePixel(aRect, pChildOutDev->GetMappingPolicy());
    AbsoluteScreenPixelPoint aAbsScreenPos = pChild->OutputToAbsoluteScreenPixel( pChild->ScreenToOutputPixel(aDeviceRect.TopLeft()) );
    pEvt->maCursorBound = AbsoluteScreenPixelRectangle(aAbsScreenPos, aDeviceRect.GetSize());
    pEvt->mbVertical = pWinData->mbVertical;
    pEvt->mbValid = true;
}

static bool lcl_HandleKey( vcl::Window* pWindow, NotifyEventType nSVEvent,
                           sal_uInt16 nKeyCode, sal_uInt16 nCharCode, sal_uInt16 nRepeat, bool bForward )
{
    ImplSVData* pSVData = ImplGetSVData();
    vcl::KeyCode aKeyCode( nKeyCode, nKeyCode );
    sal_uInt16 nEvCode = aKeyCode.GetCode();

    // allow application key listeners to remove the key event
    // but make sure we're not forwarding external KeyEvents, (ie where bForward is false)
    // because those are coming back from the listener itself and MUST be processed
    if( bForward )
    {
        VclEventId nVCLEvent;
        switch( nSVEvent )
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
    if ( nSVEvent == NotifyEventType::KEYINPUT )
    {
        if ( ImplGetSVHelpData().mbExtHelpMode )
        {
            Help::EndExtHelp();
            if ( nEvCode == KEY_ESCAPE )
                return true;
        }
        if ( ImplGetSVHelpData().mpHelpWin )
            ImplDestroyHelpWindow( false );

        // AutoScrollMode
        if (pSVData->mpWinData->mpAutoScrollWin)
        {
            pSVData->mpWinData->mpAutoScrollWin->EndAutoScroll();
            if ( nEvCode == KEY_ESCAPE )
                return true;
        }

        if (pSVData->mpWinData->mpTrackWin)
        {
            sal_uInt16 nOrigCode = aKeyCode.GetCode();

            if ( nOrigCode == KEY_ESCAPE )
            {
                pSVData->mpWinData->mpTrackWin->EndTracking( TrackingEventFlags::Cancel | TrackingEventFlags::Key );
                if (pSVData->mpWinData->mpFirstFloat)
                {
                    FloatingWindow* pLastLevelFloat = pSVData->mpWinData->mpFirstFloat->ImplFindLastLevelFloat();
                    if ( !(pLastLevelFloat->GetPopupModeFlags() & FloatWinPopupFlags::NoKeyClose) )
                    {
                        sal_uInt16 nEscCode = aKeyCode.GetCode();

                        if ( nEscCode == KEY_ESCAPE )
                            pLastLevelFloat->EndPopupMode( FloatWinPopupEndFlags::Cancel | FloatWinPopupEndFlags::CloseAll );
                    }
                }
                return true;
            }
            else if ( nOrigCode == KEY_RETURN )
            {
                pSVData->mpWinData->mpTrackWin->EndTracking( TrackingEventFlags::Key );
                return true;
            }
            else
                return true;
        }

        // handle FloatingMode
        if (pSVData->mpWinData->mpFirstFloat)
        {
            FloatingWindow* pLastLevelFloat = pSVData->mpWinData->mpFirstFloat->ImplFindLastLevelFloat();
            if ( !(pLastLevelFloat->GetPopupModeFlags() & FloatWinPopupFlags::NoKeyClose) )
            {
                sal_uInt16 nCode = aKeyCode.GetCode();

                if ( (nCode == KEY_ESCAPE) || bCtrlF6)
                {
                    pLastLevelFloat->EndPopupMode( FloatWinPopupEndFlags::Cancel | FloatWinPopupEndFlags::CloseAll );
                    if( !bCtrlF6 )
                        return true;
                }
            }
        }

        // test for accel
        if ( pSVData->maAppData.mpAccelMgr )
        {
            if ( pSVData->maAppData.mpAccelMgr->IsAccelKey( aKeyCode ) )
                return true;
        }
    }

    // find window
    VclPtr<vcl::Window> pChild = lcl_GetKeyInputWindow( pWindow );
    if ( !pChild )
        return false;

    // #i1820# use locale specific decimal separator
    if (nEvCode == KEY_DECIMAL)
    {
        // tdf#138932: don't modify the meaning of the key for password box
        bool bPass = false;
        if (auto pEdit = dynamic_cast<Edit*>(pChild.get()))
            bPass = pEdit->IsPassword();
        if (!bPass && Application::GetSettings().GetMiscSettings().GetEnableLocalizedDecimalSep())
        {
            OUString aSep(pWindow->GetSettings().GetLocaleDataWrapper().getNumDecimalSep());
            nCharCode = static_cast<sal_uInt16>(aSep[0]);
        }
    }

    // RTL: mirror cursor keys
    if( (aKeyCode.GetCode() == KEY_LEFT || aKeyCode.GetCode() == KEY_RIGHT) &&
      pChild->IsRTLEnabled() && pChild->GetOutDev()->HasMirroredGraphics() )
        aKeyCode = vcl::KeyCode( aKeyCode.GetCode() == KEY_LEFT ? KEY_RIGHT : KEY_LEFT, aKeyCode.GetModifier() );

    KeyEvent    aKeyEvt( static_cast<sal_Unicode>(nCharCode), aKeyCode, nRepeat );
    NotifyEvent aNotifyEvt( nSVEvent, pChild, &aKeyEvt );
    bool bKeyPreNotify = ImplCallPreNotify( aNotifyEvt );
    bool bRet = true;

    if ( !bKeyPreNotify && !pChild->isDisposed() )
    {
        if ( nSVEvent == NotifyEventType::KEYINPUT )
        {
            UITestLogger::getInstance().logKeyInput(pChild, aKeyEvt);
            pChild->ImplGetWindowImpl()->mbKeyInput = false;
            pChild->KeyInput( aKeyEvt );
        }
        else
        {
            pChild->ImplGetWindowImpl()->mbKeyUp = false;
            pChild->KeyUp( aKeyEvt );
        }
        if( !pChild->isDisposed() )
            aNotifyEvt.GetWindow()->ImplNotifyKeyMouseCommandEventListeners( aNotifyEvt );
    }

    if ( pChild->isDisposed() )
        return true;

    if ( nSVEvent == NotifyEventType::KEYINPUT )
    {
        if ( !bKeyPreNotify && pChild->ImplGetWindowImpl()->mbKeyInput )
        {
            sal_uInt16 nCode = aKeyCode.GetCode();

            // #101999# is focus in or below toolbox
            bool bToolboxFocus=false;
            if( (nCode == KEY_F1) && aKeyCode.IsShift() )
            {
                vcl::Window *pWin = pWindow->ImplGetWindowImpl()->mpFrameData->mpFocusWin;
                while( pWin )
                {
                    if( pWin->ImplGetWindowImpl()->mbToolBox )
                    {
                        bToolboxFocus = true;
                        break;
                    }
                    else
                        pWin = pWin->GetParent();
                }
            }

            // ContextMenu
            if ( (nCode == KEY_CONTEXTMENU) || ((nCode == KEY_F10) && aKeyCode.IsShift() && !aKeyCode.IsMod1() && !aKeyCode.IsMod2() ) )
                bRet = !ImplCallCommand( pChild, CommandEventId::ContextMenu );
            else if ( ( (nCode == KEY_F2) && aKeyCode.IsShift() ) || ( (nCode == KEY_F1) && aKeyCode.IsMod1() ) ||
                // #101999# no active help when focus in toolbox, simulate BalloonHelp instead
                ( (nCode == KEY_F1) && aKeyCode.IsShift() && bToolboxFocus ) )
            {
                // TipHelp via Keyboard (Shift-F2 or Ctrl-F1)
                // simulate mouseposition at center of window

                Size aSize = pChild->GetOutDev()->GetOutputSize();
                Point aPos( aSize.getWidth()/2, aSize.getHeight()/2 );
                aPos = pChild->OutputToScreenPixel( aPos );

                HelpEvent aHelpEvent( aPos, HelpEventMode::BALLOON );
                aHelpEvent.SetKeyboardActivated( true );
                ImplGetSVHelpData().mbSetKeyboardHelp = true;
                pChild->RequestHelp( aHelpEvent );
                ImplGetSVHelpData().mbSetKeyboardHelp = false;
            }
            else if ( (nCode == KEY_F1) || (nCode == KEY_HELP) )
            {
                if ( !aKeyCode.GetModifier() )
                {
                    if ( ImplGetSVHelpData().mbContextHelp )
                    {
                        Point       aMousePos = pChild->OutputToScreenPixel( pChild->GetPointerPosPixel() );
                        HelpEvent   aHelpEvent( aMousePos, HelpEventMode::CONTEXT );
                        pChild->RequestHelp( aHelpEvent );
                    }
                    else
                        bRet = false;
                }
                else if ( aKeyCode.IsShift() )
                {
                    if ( ImplGetSVHelpData().mbExtHelp )
                        Help::StartExtHelp();
                    else
                        bRet = false;
                }
            }
            else
                bRet = false;
        }
    }
    else
    {
        if ( !bKeyPreNotify && pChild->ImplGetWindowImpl()->mbKeyUp )
            bRet = false;
    }

    // #105591# send keyinput to parent if we are a floating window and the key was not processed yet
    if (bRet || !pWindow->ImplGetWindowImpl() || !pWindow->ImplGetWindowImpl()->mbFloatWin || !pWindow->GetParent() || (pWindow->ImplGetWindowImpl()->mpFrame == pWindow->GetParent()->ImplGetWindowImpl()->mpFrame) )
        return bRet;

    pChild = pWindow->GetParent();

    // call handler
    NotifyEvent aNEvt( nSVEvent, pChild, &aKeyEvt );
    bool bPreNotify = ImplCallPreNotify( aNEvt );
    if ( pChild->isDisposed() )
        return true;

    if ( !bPreNotify )
    {
        if ( nSVEvent == NotifyEventType::KEYINPUT )
        {
            pChild->ImplGetWindowImpl()->mbKeyInput = false;
            pChild->KeyInput( aKeyEvt );
        }
        else
        {
            pChild->ImplGetWindowImpl()->mbKeyUp = false;
            pChild->KeyUp( aKeyEvt );
        }

        if( !pChild->isDisposed() )
            aNEvt.GetWindow()->ImplNotifyKeyMouseCommandEventListeners( aNEvt );
        if ( pChild->isDisposed() )
            return true;
    }

    if( bPreNotify || !pChild->ImplGetWindowImpl()->mbKeyInput )
        return true;

    return bRet;
}

static bool lcl_HandleExtTextInput( vcl::Window* pWindow,
                                    const OUString& rText,
                                    const ExtTextInputAttr* pTextAttr,
                                    sal_Int32 nCursorPos, sal_uInt16 nCursorFlags )
{
    ImplSVData* pSVData = ImplGetSVData();
    vcl::Window*     pChild = nullptr;

    int nTries = 200;
    while( nTries-- )
    {
        pChild = pSVData->mpWinData->mpExtTextInputWin;
        if ( !pChild )
        {
            pChild = lcl_GetKeyInputWindow( pWindow );
            if ( !pChild )
                return false;
        }
        if( !pChild->ImplGetWindowImpl()->mpFrameData->mnFocusId )
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
    if ( !pChild->ImplGetWindowImpl()->mbExtTextInput )
    {
        pChild->ImplGetWindowImpl()->mbExtTextInput = true;
        pWinData->mpExtOldText = OUString();
        pWinData->mpExtOldAttrAry.reset();
        pSVData->mpWinData->mpExtTextInputWin = pChild;
        ImplCallCommand( pChild, CommandEventId::StartExtTextInput );
    }

    // be aware of being recursively called in StartExtTextInput
    if ( !pChild->ImplGetWindowImpl()->mbExtTextInput )
        return false;

    // Test for changes
    bool bOnlyCursor = false;
    sal_Int32 nMinLen = std::min( pWinData->mpExtOldText->getLength(), rText.getLength() );
    sal_Int32 nDeltaStart = 0;
    while ( nDeltaStart < nMinLen )
    {
        if ( (*pWinData->mpExtOldText)[nDeltaStart] != rText[nDeltaStart] )
            break;
        nDeltaStart++;
    }
    if ( pWinData->mpExtOldAttrAry || pTextAttr )
    {
        if ( !pWinData->mpExtOldAttrAry || !pTextAttr )
            nDeltaStart = 0;
        else
        {
            sal_Int32 i = 0;
            while ( i < nDeltaStart )
            {
                if ( pWinData->mpExtOldAttrAry[i] != pTextAttr[i] )
                {
                    nDeltaStart = i;
                    break;
                }
                i++;
            }
        }
    }
    if ( (nDeltaStart >= nMinLen) &&
         (pWinData->mpExtOldText->getLength() == rText.getLength()) )
        bOnlyCursor = true;

    // Call Event and store the information
    CommandExtTextInputData aData( rText, pTextAttr,
                                   nCursorPos, nCursorFlags,
                                   bOnlyCursor );
    *pWinData->mpExtOldText = rText;
    pWinData->mpExtOldAttrAry.reset();
    if ( pTextAttr )
    {
        pWinData->mpExtOldAttrAry.reset( new ExtTextInputAttr[rText.getLength()] );
        std::copy_n(pTextAttr, rText.getLength(), pWinData->mpExtOldAttrAry.get());
    }
    return !ImplCallCommand( pChild, CommandEventId::ExtTextInput, &aData );
}

static bool lcl_HandleEndExtTextInput()
{
    ImplSVData* pSVData = ImplGetSVData();
    vcl::Window* pChild = pSVData->mpWinData->mpExtTextInputWin;

    if (!pChild)
        return false;

    pChild->ImplGetWindowImpl()->mbExtTextInput = false;
    pSVData->mpWinData->mpExtTextInputWin = nullptr;
    ImplWinData* pWinData = pChild->ImplGetWinData();
    pWinData->mpExtOldText.reset();
    pWinData->mpExtOldAttrAry.reset();

    return !ImplCallCommand( pChild, CommandEventId::EndExtTextInput );

}

static bool lcl_HandleWheelEvent(vcl::Window* pWindow, const SalWheelMouseEvent& rEvt)
{
    HandleWheelEvent aHandler(pWindow, rEvt);
    return aHandler.HandleEvent(rEvt);
}

static bool lcl_HandleSwipe(vcl::Window *pWindow, const SalGestureSwipeEvent& rEvt)
{
    HandleGestureSwipeEvent aHandler(pWindow, rEvt);
    return aHandler.HandleEvent();
}

static bool lcl_HandleLongPress(vcl::Window *pWindow, const SalGestureLongPressEvent& rEvt)
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

static void lcl_HandlePaint( vcl::Window* pWindow, const tools::Rectangle& rBoundRect, bool bImmediateUpdate )
{
    // system paint events must be checked for re-mirroring
    pWindow->ImplGetWindowImpl()->mnPaintFlags |= ImplPaintFlags::CheckRtl;

    // trigger paint for all windows that live in the new paint region
    vcl::Region aRegion( rBoundRect );
    pWindow->ImplInvalidateOverlapFrameRegion( aRegion );
    if (!bImmediateUpdate)
        return;

    // #i87663# trigger possible pending resize notifications
    // (GetSizePixel does that for us)
    pWindow->GetSizePixel();
    // force drawing immediately
    pWindow->PaintImmediately();
}

static void lcl_HandleMoveResize( vcl::Window* pWindow, tools::Long nNewWidth, tools::Long nNewHeight )
{
    lcl_HandleMove( pWindow );
    ImplHandleResize( pWindow, nNewWidth, nNewHeight );
}

bool ImplWindowFrameProc( vcl::Window* _pWindow, SalEvent nEvent, const void* pEvent )
{
    DBG_TESTSOLARMUTEX();

    // Ensure the window survives during this method.
    VclPtr<vcl::Window> pWindow( _pWindow );

    bool bRet = false;

    // #119709# for some unknown reason it is possible to receive events (in this case key events)
    // although the corresponding VCL window must have been destroyed already
    // at least ImplGetWindowImpl() was NULL in these cases, so check this here
    if( pWindow->ImplGetWindowImpl() == nullptr )
        return false;

    switch ( nEvent )
    {
        case SalEvent::MouseMove:
            bRet = lcl_HandleSalMouseMove( pWindow, static_cast<SalMouseEvent const *>(pEvent) );
            break;
        case SalEvent::ExternalMouseMove:
        {
            MouseEvent const * pMouseEvt = static_cast<MouseEvent const *>(pEvent);
            SalMouseEvent   aSalMouseEvent;

            aSalMouseEvent.mnTime = tools::Time::GetSystemTicks();
            aSalMouseEvent.mnX = pMouseEvt->GetPosPixel().X();
            aSalMouseEvent.mnY = pMouseEvt->GetPosPixel().Y();
            aSalMouseEvent.mnButton = 0;
            aSalMouseEvent.mnCode = pMouseEvt->GetButtons() | pMouseEvt->GetModifier();

            bRet = lcl_HandleSalMouseMove( pWindow, &aSalMouseEvent );
        }
        break;
        case SalEvent::MouseLeave:
            bRet = lcl_HandleSalMouseLeave( pWindow, static_cast<SalMouseEvent const *>(pEvent) );
            break;
        case SalEvent::MouseButtonDown:
            bRet = lcl_HandleSalMouseButtonDown( pWindow, static_cast<SalMouseEvent const *>(pEvent) );
            break;
        case SalEvent::ExternalMouseButtonDown:
        {
            MouseEvent const * pMouseEvt = static_cast<MouseEvent const *>(pEvent);
            SalMouseEvent   aSalMouseEvent;

            aSalMouseEvent.mnTime = tools::Time::GetSystemTicks();
            aSalMouseEvent.mnX = pMouseEvt->GetPosPixel().X();
            aSalMouseEvent.mnY = pMouseEvt->GetPosPixel().Y();
            aSalMouseEvent.mnButton = pMouseEvt->GetButtons();
            aSalMouseEvent.mnCode = pMouseEvt->GetButtons() | pMouseEvt->GetModifier();

            bRet = lcl_HandleSalMouseButtonDown( pWindow, &aSalMouseEvent );
        }
        break;
        case SalEvent::MouseButtonUp:
            bRet = lcl_HandleSalMouseButtonUp( pWindow, static_cast<SalMouseEvent const *>(pEvent) );
            break;
        case SalEvent::ExternalMouseButtonUp:
        {
            MouseEvent const * pMouseEvt = static_cast<MouseEvent const *>(pEvent);
            SalMouseEvent   aSalMouseEvent;

            aSalMouseEvent.mnTime = tools::Time::GetSystemTicks();
            aSalMouseEvent.mnX = pMouseEvt->GetPosPixel().X();
            aSalMouseEvent.mnY = pMouseEvt->GetPosPixel().Y();
            aSalMouseEvent.mnButton = pMouseEvt->GetButtons();
            aSalMouseEvent.mnCode = pMouseEvt->GetButtons() | pMouseEvt->GetModifier();

            bRet = lcl_HandleSalMouseButtonUp( pWindow, &aSalMouseEvent );
        }
        break;
        case SalEvent::MouseActivate:
            bRet = false;
            break;
        case SalEvent::KeyInput:
            {
            SalKeyEvent const * pKeyEvt = static_cast<SalKeyEvent const *>(pEvent);
            bRet = lcl_HandleKey( pWindow, NotifyEventType::KEYINPUT,
                pKeyEvt->mnCode, pKeyEvt->mnCharCode, pKeyEvt->mnRepeat, true );
            }
            break;
        case SalEvent::ExternalKeyInput:
            {
            KeyEvent const * pKeyEvt = static_cast<KeyEvent const *>(pEvent);
            bRet = lcl_HandleKey( pWindow, NotifyEventType::KEYINPUT,
                pKeyEvt->GetKeyCode().GetFullCode(), pKeyEvt->GetCharCode(), pKeyEvt->GetRepeat(), false );
            }
            break;
        case SalEvent::KeyUp:
            {
            SalKeyEvent const * pKeyEvt = static_cast<SalKeyEvent const *>(pEvent);
            bRet = lcl_HandleKey( pWindow, NotifyEventType::KEYUP,
                pKeyEvt->mnCode, pKeyEvt->mnCharCode, pKeyEvt->mnRepeat, true );
            }
            break;
        case SalEvent::ExternalKeyUp:
            {
            KeyEvent const * pKeyEvt = static_cast<KeyEvent const *>(pEvent);
            bRet = lcl_HandleKey( pWindow, NotifyEventType::KEYUP,
                pKeyEvt->GetKeyCode().GetFullCode(), pKeyEvt->GetCharCode(), pKeyEvt->GetRepeat(), false );
            }
            break;
        case SalEvent::KeyModChange:
            lcl_HandleSalKeyMod( pWindow, static_cast<SalKeyModEvent const *>(pEvent) );
            break;

        case SalEvent::InputLanguageChange:
            lcl_HandleInputLanguageChange( pWindow );
            break;

        case SalEvent::MenuActivate:
        case SalEvent::MenuDeactivate:
        case SalEvent::MenuHighlight:
        case SalEvent::MenuCommand:
        case SalEvent::MenuButtonCommand:
            bRet = lcl_HandleMenuEvent( pWindow, const_cast<SalMenuEvent *>(static_cast<SalMenuEvent const *>(pEvent)), nEvent );
            break;

        case SalEvent::WheelMouse:
            bRet = lcl_HandleWheelEvent( pWindow, *static_cast<const SalWheelMouseEvent*>(pEvent));
            break;

        case SalEvent::Paint:
            {
            SalPaintEvent const * pPaintEvt = static_cast<SalPaintEvent const *>(pEvent);

            if( AllSettings::GetLayoutRTL() )
            {
                SalFrame* pSalFrame = pWindow->ImplGetWindowImpl()->mpFrame;
                const_cast<SalPaintEvent *>(pPaintEvt)->mnBoundX = pSalFrame->GetWidth() - pPaintEvt->mnBoundWidth - pPaintEvt->mnBoundX;
            }

            tools::Rectangle aBoundRect( Point( pPaintEvt->mnBoundX, pPaintEvt->mnBoundY ),
                                  Size( pPaintEvt->mnBoundWidth, pPaintEvt->mnBoundHeight ) );
            lcl_HandlePaint( pWindow, aBoundRect, pPaintEvt->mbImmediateUpdate );
            }
            break;

        case SalEvent::Move:
            lcl_HandleMove( pWindow );
            break;

        case SalEvent::Resize:
            {
            const Size aNewSize = pWindow->ImplGetWindowImpl()->mpFrame->GetClientSize();
            ImplHandleResize( pWindow, aNewSize.Width(), aNewSize.Height());
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
            lcl_KillOwnPopups( pWindow );
            }
            break;

        case SalEvent::GetFocus:
            lcl_HandleGetFocus( pWindow );
            break;
        case SalEvent::LoseFocus:
            lcl_HandleLoseFocus( pWindow );
            break;

        case SalEvent::Close:
            lcl_HandleClose( pWindow );
            break;

        case SalEvent::Shutdown:
            {
                static bool bInQueryExit = false;
                if( !bInQueryExit )
                {
                    bInQueryExit = true;
                    if ( GetpApp()->QueryExit() )
                    {
                        // end the message loop
                        Application::Quit();
                        return false;
                    }
                    else
                    {
                        bInQueryExit = false;
                        return true;
                    }
                }
                return false;
            }

        case SalEvent::SettingsChanged:
        case SalEvent::PrinterChanged:
        case SalEvent::DisplayChanged:
        case SalEvent::FontChanged:
            lcl_HandleSalSettings( nEvent );
            break;

        case SalEvent::UserEvent:
            lcl_HandleUserEvent( const_cast<ImplSVEvent *>(static_cast<ImplSVEvent const *>(pEvent)) );
            break;

        case SalEvent::ExtTextInput:
            {
            SalExtTextInputEvent const * pEvt = static_cast<SalExtTextInputEvent const *>(pEvent);
            bRet = lcl_HandleExtTextInput( pWindow,
                                           pEvt->maText, pEvt->mpTextAttr,
                                           pEvt->mnCursorPos, pEvt->mnCursorFlags );
            }
            break;
        case SalEvent::EndExtTextInput:
            bRet = lcl_HandleEndExtTextInput();
            break;
        case SalEvent::ExtTextInputPos:
            lcl_HandleSalExtTextInputPos( pWindow, const_cast<SalExtTextInputPosEvent *>(static_cast<SalExtTextInputPosEvent const *>(pEvent)) );
            break;
        case SalEvent::InputContextChange:
            bRet = lcl_HandleInputContextChange( pWindow );
            break;
        case SalEvent::ShowDialog:
            {
                ShowDialogId nLOKWindowId = static_cast<ShowDialogId>(reinterpret_cast<sal_IntPtr>(pEvent));
                bRet = lcl_HandleShowDialog( pWindow, nLOKWindowId );
            }
            break;
        case SalEvent::SurroundingTextRequest:
            lcl_HandleSalSurroundingTextRequest( pWindow, const_cast<SalSurroundingTextRequestEvent *>(static_cast<SalSurroundingTextRequestEvent const *>(pEvent)) );
            break;
        case SalEvent::DeleteSurroundingTextRequest:
            lcl_HandleSalDeleteSurroundingTextRequest( pWindow, const_cast<SalSurroundingTextSelectionChangeEvent *>(static_cast<SalSurroundingTextSelectionChangeEvent const *>(pEvent)) );
            break;
        case SalEvent::SurroundingTextSelectionChange:
        {
            SalSurroundingTextSelectionChangeEvent const * pEvt
             = static_cast<SalSurroundingTextSelectionChangeEvent const *>(pEvent);
            lcl_HandleSurroundingTextSelectionChange( pWindow,
                              pEvt->mnStart,
                              pEvt->mnEnd );
            [[fallthrough]]; // TODO: Fallthrough really intended?
        }
        case SalEvent::StartReconversion:
            lcl_HandleStartReconversion( pWindow );
            break;

        case SalEvent::QueryCharPosition:
            lcl_HandleSalQueryCharPosition( pWindow, const_cast<SalQueryCharPositionEvent *>(static_cast<SalQueryCharPositionEvent const *>(pEvent)) );
            break;

        case SalEvent::GestureSwipe:
            bRet = lcl_HandleSwipe(pWindow, *static_cast<const SalGestureSwipeEvent*>(pEvent));
            break;

        case SalEvent::GestureLongPress:
            bRet = lcl_HandleLongPress(pWindow, *static_cast<const SalGestureLongPressEvent*>(pEvent));
            break;

        case SalEvent::ExternalGesture:
        {
            auto const * pGestureEvent = static_cast<GestureEventPan const *>(pEvent);

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
            auto const * aSalGestureEvent = static_cast<SalGestureEvent const *>(pEvent);
            bRet = lcl_HandleGestureEvent(pWindow, *aSalGestureEvent);
        }
        break;
        case SalEvent::GestureZoom:
        {
            const auto * pGestureEvent = static_cast<SalGestureZoomEvent const *>(pEvent);
            bRet = lcl_HandleGestureZoomEvent(pWindow, *pGestureEvent);
        }
        break;
        case SalEvent::GestureRotate:
        {
            const auto * pGestureEvent = static_cast<SalGestureRotateEvent const *>(pEvent);
            bRet = lcl_HandleGestureRotateEvent(pWindow, *pGestureEvent);
        }
        break;
        default:
            SAL_WARN( "vcl.layout", "ImplWindowFrameProc(): unknown event (" << static_cast<int>(nEvent) << ")" );
            break;
    }

    return bRet;
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
