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


#include <config_feature_desktop.h>
#include <config_vclplug.h>

#include <tools/time.hxx>

#include <LibreOfficeKit/LibreOfficeKitEnums.h>

#include <vcl/ITiledRenderable.hxx>
#include <vcl/dndlistenercontainer.hxx>
#include <vcl/ptrstyle.hxx>
#include <vcl/svapp.hxx>
#include <vcl/window.hxx>
#include <vcl/cursor.hxx>
#include <vcl/sysdata.hxx>
#include <vcl/event.hxx>

#include <sal/types.h>

#include <window/window.h>
#include <window/state/ImplFrameData.hxx>
#include <window/state/ImplWinData.hxx>
#include <window/state/WindowClassification.hxx>
#include <window/state/WindowPlatformState.hxx>
#include <window/state/WindowVisibilityState.hxx>
#include <window/state/WindowInput.hxx>
#include <window/state/WindowLOKData.hxx>
#include <window/state/WindowControlAppearance.hxx>
#include <window/state/WindowClippingState.hxx>
#include <window/state/WindowHierarchy.hxx>
#include <window/state/WindowFocusState.hxx>
#include <window/state/WindowPointerState.hxx>
#include <window/state/WindowControlState.hxx>
#include <svdata.hxx>
#include <salobj.hxx>
#include <salgdi.hxx>
#include <salframe.hxx>
#include <salinst.hxx>

#include <window/dnd/dndeventdispatcher.hxx>

#include <com/sun/star/datatransfer/dnd/XDragSource.hpp>
#include <com/sun/star/datatransfer/dnd/XDropTarget.hpp>
#include <com/sun/star/uno/XComponentContext.hpp>

#include <comphelper/processfactory.hxx>

using namespace ::com::sun::star::uno;

namespace vcl {

WindowHitTest Window::ImplHitTest( const Point& rFramePos )
{
    Point aFramePos( rFramePos );
    if ( GetOutDev()->ImplIsAntiparallel() )
    {
        const OutputDevice* pOutDev = GetOutDev();
        pOutDev->ReMirror( aFramePos );
    }

    if ( !GetOutputRectPixel().Contains( aFramePos ) )
        return WindowHitTest::NONE;

    if ( mpClippingState->hasWindowRegion() )
    {
        Point aTempPos = aFramePos;
        aTempPos.AdjustX( -GetOutDev()->GetDeviceOriginX() );
        aTempPos.AdjustY( -GetOutDev()->GetDeviceOriginY() );

        if ( !mpClippingState->containsWindowRegionPoint( aTempPos ) )
            return WindowHitTest::NONE;
    }

    WindowHitTest nHitTest = WindowHitTest::Inside;
    if ( mpInput->isMouseTransparent() )
        nHitTest |= WindowHitTest::Transparent;

    return nHitTest;
}

bool Window::ImplTestMousePointerSet()
{
    // as soon as mouse is captured, switch mouse-pointer
    if ( IsMouseCaptured() )
        return true;

    // if the mouse is over the window, switch it
    tools::Rectangle aClientRect( Point( 0, 0 ), GetOutputSizePixel() );
    return aClientRect.Contains( GetPointerPosPixel() );
}

PointerStyle Window::ImplGetMousePointer() const
{
    PointerStyle    ePointerStyle;
    bool            bWait = false;

    if ( IsEnabled() && IsInputEnabled() && ! IsInModalMode() )
        ePointerStyle = GetPointer();
    else
        ePointerStyle = PointerStyle::Arrow;

    const vcl::Window* pWindow = this;
    do
    {
        // when the pointer is not visible stop the search, as
        // this status should not be overwritten
        if ( pWindow->mpPointerState->mbNoPtrVisible )
            return PointerStyle::Null;

        if ( !bWait )
        {
            if ( pWindow->mpPointerState->mnWaitCount )
            {
                ePointerStyle = PointerStyle::Wait;
                bWait = true;
            }
            else
            {
                if ( pWindow->mpPointerState->mbChildPtrOverwrite )
                    ePointerStyle = pWindow->GetPointer();
            }
        }

        if ( pWindow->ImplIsOverlapWindow() )
            break;

        pWindow = pWindow->ImplGetParent();
    }
    while ( pWindow );

    return ePointerStyle;
}

void Window::ImplCallMouseMove(sal_uInt16 nMouseCode, bool bModChanged)
{
    ImplFrameData* pFrameData = mpPlatformState ? mpPlatformState->mpFrameData : nullptr;
    if (!pFrameData || !pFrameData->mbMouseIn)
        return;

    vcl::Window* pFrameWindow = mpHierarchy ? mpHierarchy->getFrameWindow() : nullptr;
    if (!pFrameWindow)
        return;

    const WindowVisibilityState* pVisibility = pFrameWindow->ImplGetVisibilityState();
    if (!pVisibility || !pVisibility->mbReallyVisible)
        return;

    const OutputDevice* pFrameOutDev = pFrameWindow->GetOutDev();
    if (!pFrameOutDev)
        return;

    const sal_uInt64 nTime = tools::Time::GetSystemTicks();
    const tools::Long nX = pFrameData->mnLastMouseX;
    const tools::Long nY = pFrameData->mnLastMouseY;

    MouseEventModifiers nMode = pFrameData->mnMouseMode | MouseEventModifiers::SYNTHETIC;
    if (bModChanged)
        nMode |= MouseEventModifiers::MODIFIERCHANGED;

    ImplSVData* pSVData = ImplGetSVData();
    const bool bHasCapture = pSVData && pSVData->mpWinData && pSVData->mpWinData->mpCaptureWin;

    // Check for MouseLeave
    const bool bLeave = !bHasCapture && ((nX < 0) || (nY < 0)
                                         || (nX >= pFrameOutDev->GetOutputWidthPixel())
                                         || (nY >= pFrameOutDev->GetOutputHeightPixel()));

    ImplHandleMouseEvent(pFrameWindow, NotifyEventType::MOUSEMOVE, bLeave, Point(nX, nY), nTime, nMouseCode, nMode);
}

void Window::ImplGenerateMouseMove()
{
    if (!mpClassification)
        return;

    ImplFrameData* pFrameData = mpPlatformState ? mpPlatformState->mpFrameData : nullptr;
    if (!pFrameData || pFrameData->mnMouseMoveId)
        return;

    vcl::Window* pFrameWindow = mpHierarchy ? mpHierarchy->getFrameWindow() : nullptr;
    if (!pFrameWindow)
        return;

    pFrameData->mnMouseMoveId = Application::PostUserEvent(
        LINK(pFrameWindow, Window, ImplGenerateMouseMoveHdl), nullptr, true);
}

IMPL_LINK_NOARG(Window, ImplGenerateMouseMoveHdl, void*, void)
{
    mpPlatformState->mpFrameData->mnMouseMoveId = nullptr;
    vcl::Window* pCaptureWin = ImplGetSVData()->mpWinData->mpCaptureWin;
    if( ! pCaptureWin ||
        (pCaptureWin->mpClassification && pCaptureWin->mpPlatformState->mpFrame == mpPlatformState->mpFrame)
    )
    {
        ImplCallMouseMove( mpPlatformState->mpFrameData->mnMouseCode );
    }
}

void Window::ImplInvertFocus( const tools::Rectangle& rRect )
{
    InvertTracking( rRect, ShowTrackFlags::Small | ShowTrackFlags::TrackWindow );
}

static bool lcl_IsWindowFocused(const vcl::Window& rWindow)
{
    WindowPlatformState* pImpl = rWindow.ImplGetPlatformState();
    if (!pImpl)
        return false;

    if (pImpl->mpSysObj)
        return true;

    if (pImpl->mpFrameData->mbHasFocus)
        return true;

    WindowInput* pInput = rWindow.ImplGetWindowInput();
    if (pInput && pInput->hasFakeFocus())
        return true;

    return false;
}

void Window::ImplGrabFocus(GetFocusFlags nFlags)
{
    // #143570# no focus for destructing windows
    if (!mpClassification || mpClassification->isInDispose())
        return;

    // Some event listeners do really bad stuff
    // => prepare for the worst
    VclPtr<vcl::Window> xWindow(this);

    // Currently the client window should always get the focus.
    // Should the border window at some point be focusable
    // we need to change all GrabFocus() instances in VCL, e.g. in ToTop()
    vcl::Window* pClientWindow = mpHierarchy ? mpHierarchy->getClientWindow() : nullptr;
    if (pClientWindow)
    {
        // For a lack of design we need a little hack here to
        // ensure that dialogs on close pass the focus back to
        // the correct window
        vcl::Window* pLastFocus = mpInput ? mpInput->getLastFocusWindow() : nullptr;
        const DialogControlFlags nDlgCtrlFlags = mpControlState ? mpControlState->mnDlgCtrlFlags : DialogControlFlags::NONE;
        if (pLastFocus && (pLastFocus != this)
            && !(nDlgCtrlFlags & DialogControlFlags::WantFocus)
            && pLastFocus->IsEnabled()
            && pLastFocus->IsInputEnabled()
            && !pLastFocus->IsInModalMode())
        {
            pLastFocus->GrabFocus();
        }
        else
        {
            pClientWindow->GrabFocus();
        }
        return;
    }
    else if (mpClassification->isFrame())
    {
        // For a lack of design we need a little hack here to
        // ensure that dialogs on close pass the focus back to
        // the correct window
        vcl::Window* pLastFocus = mpInput ? mpInput->getLastFocusWindow() : nullptr;
        const DialogControlFlags nDlgCtrlFlags = mpControlState ? mpControlState->mnDlgCtrlFlags : DialogControlFlags::NONE;
        if (pLastFocus && (pLastFocus != this)
            && !(nDlgCtrlFlags & DialogControlFlags::WantFocus)
            && pLastFocus->IsEnabled()
            && pLastFocus->IsInputEnabled()
            && !pLastFocus->IsInModalMode())
        {
            pLastFocus->GrabFocus();
            return;
        }
    }

    // If the Window is disabled, then we don't change the focus
    if (!IsEnabled() || !IsInputEnabled() || IsInModalMode())
        return;

    // We only need to set the focus if it is not already set.
    // Note: if some other frame is waiting for an asynchronous focus event
    // we also have to post an asynchronous focus event for this frame
    // which is done using ToTop
    ImplSVData* pSVData = ImplGetSVData();
    if (!pSVData || !pSVData->mpWinData)
        return;

    bool bAsyncFocusWaiting = false;
    vcl::Window* pFrameWindow = mpHierarchy ? mpHierarchy->getFrameWindow() : nullptr;
    vcl::Window* pFrame = pSVData->maFrameData.mpFirstFrame;
    while (pFrame)
    {
        WindowPlatformState* pFramePlatformState = pFrame->ImplGetPlatformState();
        ImplFrameData* pCurrFrameData = pFramePlatformState ? pFramePlatformState->mpFrameData : nullptr;
        if (!pFrame->ImplGetWindowClassification() || !pCurrFrameData)
            break;

        if (pFrame != pFrameWindow && pCurrFrameData->mnFocusId)
        {
            bAsyncFocusWaiting = true;
            break;
        }
        pFrame = pCurrFrameData->mpNextFrame;
    }

    const bool bHasFocus = lcl_IsWindowFocused(*this);
    bool bMustNotGrabFocus = false;

    // #100242#, check parent hierarchy if some floater prohibits grab focus
    vcl::Window* pParent = this;
    while (pParent)
    {
        if ((pParent->GetStyle() & WB_SYSTEMFLOATWIN) && !(pParent->GetStyle() & WB_MOVEABLE))
        {
            bMustNotGrabFocus = true;
            break;
        }
        if (!pParent->ImplGetWindowClassification())
            break;

        const WindowHierarchy* pParentHierarchy = pParent->ImplGetWindowHierarchy();
        pParent = pParentHierarchy ? pParentHierarchy->getParent() : nullptr;
    }

    if (!((pSVData->mpWinData->mpFocusWin.get() != this
           && !mpClassification->isInDispose())
          || (bAsyncFocusWaiting && !bHasFocus && !bMustNotGrabFocus)))
    {
        return;
    }

    // EndExtTextInput if it is not the same window
    if (pSVData->mpWinData->mpExtTextInputWin
        && (pSVData->mpWinData->mpExtTextInputWin.get() != this))
    {
        pSVData->mpWinData->mpExtTextInputWin->EndExtTextInput();
    }

    // Mark this window as the last FocusWindow
    vcl::Window* pOverlapWindow = ImplGetFirstOverlapWindow();
    if (pOverlapWindow && pOverlapWindow->ImplGetWindowClassification() && pOverlapWindow->mpInput)
        pOverlapWindow->mpInput->setLastFocusWindow(this);

    ImplFrameData* pFrameData = mpPlatformState ? mpPlatformState->mpFrameData : nullptr;
    if (pFrameData)
        pFrameData->mpFocusWin = this;

    if (!bHasFocus)
    {
        // Menu windows never get the system focus; the application keeps it
        if (bMustNotGrabFocus)
            return;

        // Here we already switch focus as ToTop() should not give focus to another window
        if (mpPlatformState && mpPlatformState->mpFrame)
            mpPlatformState->mpFrame->ToTop(SalFrameToTop::GrabFocus | SalFrameToTop::GrabFocusOnly);
        return;
    }

    VclPtr<vcl::Window> pOldFocusWindow = pSVData->mpWinData->mpFocusWin;
    pSVData->mpWinData->mpFocusWin = this;

    if (pOldFocusWindow && pOldFocusWindow->mpControlAppearance)
        pOldFocusWindow->mpControlAppearance->hideCursor();

    // Due to old SV-Office Activate/Deactivate handling: first as before
    if (pOldFocusWindow)
    {
        // Remember Focus
        vcl::Window* pOldOverlapWindow = pOldFocusWindow->ImplGetFirstOverlapWindow();
        vcl::Window* pNewOverlapWindow = ImplGetFirstOverlapWindow();
        if (pOldOverlapWindow != pNewOverlapWindow)
            ImplCallFocusChangeActivate(pNewOverlapWindow, pOldOverlapWindow);
    }
    else
    {
        vcl::Window* pNewOverlapWindow = ImplGetFirstOverlapWindow();
        if (pNewOverlapWindow && pNewOverlapWindow->ImplGetWindowClassification())
        {
            vcl::Window* pNewRealWindow = pNewOverlapWindow->ImplGetWindow();
            if (pNewOverlapWindow->mpFocusState)
                pNewOverlapWindow->mpFocusState->setActive(true);
            pNewOverlapWindow->Activate();

            if (pNewRealWindow != pNewOverlapWindow && pNewRealWindow && pNewRealWindow->ImplGetWindowClassification())
            {
                if (pNewRealWindow->mpFocusState)
                    pNewRealWindow->mpFocusState->setActive(true);
                pNewRealWindow->Activate();
            }
        }
    }

    // Call Get- and LoseFocus
    if (pOldFocusWindow && !pOldFocusWindow->isDisposed())
    {
        NotifyEvent aNEvt(NotifyEventType::LOSEFOCUS, pOldFocusWindow);
        if (!ImplCallPreNotify(aNEvt))
            pOldFocusWindow->CompatLoseFocus();
        pOldFocusWindow->ImplCallDeactivateListeners(this);
    }

    if (pSVData->mpWinData->mpFocusWin.get() == this)
    {
        if (mpPlatformState && mpPlatformState->mpSysObj)
        {
            if (pFrameData)
                pFrameData->mpFocusWin = this;
            if (!pFrameData || !pFrameData->mbInSysObjFocusHdl)
                mpPlatformState->mpSysObj->GrabFocus();
        }

        if (pSVData->mpWinData->mpFocusWin.get() == this)
        {
            if (mpControlAppearance)
                mpControlAppearance->showCursor();

            if (mpInput)
                mpInput->setInFocusHdl(true);

            if (mpFocusState)
                mpFocusState->setFocusFlags(nFlags);

            // If we're changing focus due to closing a popup floating window,
            // notify the new focus window so it can restore inner focus (e.g. toolboxes)
            if (pOldFocusWindow
                && !pOldFocusWindow->isDisposed()
                && (pOldFocusWindow->GetDialogControlFlags() & DialogControlFlags::FloatWinPopupModeEndCancel))
            {
                if (mpFocusState)
                    mpFocusState->addFocusFlags(GetFocusFlags::FloatWinPopupModeEndCancel);
            }

            NotifyEvent aNEvt(NotifyEventType::GETFOCUS, this);

            if (!ImplCallPreNotify(aNEvt) && !xWindow->isDisposed())
                CompatGetFocus();

            if (!xWindow->isDisposed())
            {
                if (pOldFocusWindow && pOldFocusWindow->isDisposed())
                    pOldFocusWindow = nullptr;
                ImplCallActivateListeners(pOldFocusWindow);
            }

            if (!xWindow->isDisposed())
            {
                if (mpFocusState)
                    mpFocusState->clearFocusFlags();
                if (mpInput)
                    mpInput->setInFocusHdl(false);
            }
        }
    }

    ImplNewInputContext();
}

void Window::ImplGrabFocusToDocument( GetFocusFlags nFlags )
{
    vcl::Window *pWin = this;
    while( pWin )
    {
        if( !pWin->GetParent() )
        {
            pWin->mpPlatformState->mpFrame->GrabFocus();
            pWin->ImplGetFrameWindow()->GetWindow( GetWindowType::Client )->ImplGrabFocus(nFlags);
            return;
        }
        pWin = pWin->GetParent();
    }
}

void Window::MouseMove( const MouseEvent& rMEvt )
{
    NotifyEvent aNEvt( NotifyEventType::MOUSEMOVE, this, &rMEvt );
    EventNotify(aNEvt);
}

void Window::MouseButtonDown( const MouseEvent& rMEvt )
{
    NotifyEvent aNEvt( NotifyEventType::MOUSEBUTTONDOWN, this, &rMEvt );

    if (!EventNotify(aNEvt) && mpClassification)
        mpInput->setMouseButtonDown();
}

void Window::MouseButtonUp( const MouseEvent& rMEvt )
{
    NotifyEvent aNEvt( NotifyEventType::MOUSEBUTTONUP, this, &rMEvt );

    if (!EventNotify(aNEvt) && mpClassification)
        mpInput->setMouseButtonUp();
}

void Window::SetMouseTransparent(bool bTransparent)
{
    if (vcl::Window* pBorderWindow = mpHierarchy ? mpHierarchy->getBorderWindow() : nullptr)
        pBorderWindow->SetMouseTransparent(bTransparent);

    if (mpPlatformState && mpPlatformState->mpSysObj)
        mpPlatformState->mpSysObj->SetMouseTransparent(bTransparent);

    if (mpInput)
    {
        if (bTransparent)
            mpInput->makeMouseTransparent();
        else
            mpInput->makeMouseOpaque();
    }
}

void Window::LocalStartDrag()
{
    ImplGetFrameData()->mbDragging = true;
}

void Window::CaptureMouse()
{
    ImplSVData* pSVData = ImplGetSVData();

    // possibly stop tracking
    if (pSVData->mpWinData->mpTrackWin.get() != this)
    {
        if (pSVData->mpWinData->mpTrackWin)
            pSVData->mpWinData->mpTrackWin->EndTracking(TrackingEventFlags::Cancel);
    }

    if (pSVData->mpWinData->mpCaptureWin.get() != this)
    {
        pSVData->mpWinData->mpCaptureWin = this;
        mpPlatformState->mpFrame->CaptureMouse( true );
    }
}

void Window::ReleaseMouse()
{
    if (IsMouseCaptured())
    {
        ImplSVData* pSVData = ImplGetSVData();
        pSVData->mpWinData->mpCaptureWin = nullptr;
        if (mpClassification && mpPlatformState->mpFrame)
            mpPlatformState->mpFrame->CaptureMouse( false );
        ImplGenerateMouseMove();
    }
}

bool Window::IsMouseCaptured() const
{
    return (this == ImplGetSVData()->mpWinData->mpCaptureWin);
}

void Window::SetPointer(PointerStyle ePointer)
{
    mpControlAppearance->setPointer(ePointer);

    // possibly immediately move pointer
    if (!mpPlatformState->mpFrameData->mbInMouseMove && ImplTestMousePointerSet())
        mpPlatformState->mpFrame->SetPointer( ImplGetMousePointer() );
}

void Window::EnableChildPointerOverwrite( bool bOverwrite )
{
    if (mpPointerState->mbChildPtrOverwrite == bOverwrite)
        return;

    mpPointerState->mbChildPtrOverwrite = bOverwrite;

    // possibly immediately move pointer
    if ( !mpPlatformState->mpFrameData->mbInMouseMove && ImplTestMousePointerSet() )
        mpPlatformState->mpFrame->SetPointer( ImplGetMousePointer() );
}

void Window::SetPointerPosPixel( const Point& rPos )
{
    Point aPos = OutputToScreenPixel( rPos );
    const OutputDevice *pOutDev = GetOutDev();
    if( pOutDev->HasMirroredGraphics() )
    {
        if( !IsRTLEnabled() )
        {
            pOutDev->ReMirror( aPos );
        }
        // mirroring is required here, SetPointerPos bypasses SalGraphics
        aPos.setX( GetOutDev()->mpGraphics->mirror2( aPos.X(), *GetOutDev() ) );
    }
    else if( GetOutDev()->ImplIsAntiparallel() )
    {
        pOutDev->ReMirror( aPos );
    }
    mpPlatformState->mpFrame->SetPointerPos( aPos.X(), aPos.Y() );
}

void Window::SetLastMousePos(const Point& rPos)
{
    // Do this conversion, so when GetPointerPosPixel() calls
    // ScreenToOutputPixel(), we get back the original position.
    Point aPos = OutputToScreenPixel(rPos);
    mpPlatformState->mpFrameData->mnLastMouseX = aPos.X();
    mpPlatformState->mpFrameData->mnLastMouseY = aPos.Y();
}

Point Window::GetPointerPosPixel()
{

    Point aPos( mpPlatformState->mpFrameData->mnLastMouseX, mpPlatformState->mpFrameData->mnLastMouseY );
    if( GetOutDev()->ImplIsAntiparallel() )
    {
        const OutputDevice *pOutDev = GetOutDev();
        pOutDev->ReMirror( aPos );
    }
    return ScreenToOutputPixel( aPos );
}

Point Window::GetLastPointerPosPixel()
{

    Point aPos( mpPlatformState->mpFrameData->mnBeforeLastMouseX, mpPlatformState->mpFrameData->mnBeforeLastMouseY );
    if( GetOutDev()->ImplIsAntiparallel() )
    {
        const OutputDevice *pOutDev = GetOutDev();
        pOutDev->ReMirror( aPos );
    }
    return ScreenToOutputPixel( aPos );
}

void Window::ShowPointer( bool bVisible )
{

    if (mpPointerState->mbNoPtrVisible != !bVisible)
    {
        mpPointerState->mbNoPtrVisible = !bVisible;

        // possibly immediately move pointer
        if ( !mpPlatformState->mpFrameData->mbInMouseMove && ImplTestMousePointerSet() )
            mpPlatformState->mpFrame->SetPointer( ImplGetMousePointer() );
    }
}

Window::PointerState Window::GetPointerState()
{
    PointerState aState;
    aState.mnState = 0;

    if (mpPlatformState->mpFrame)
    {
        SalFrame::SalPointerState aSalPointerState = mpPlatformState->mpFrame->GetPointerState();
        if( GetOutDev()->ImplIsAntiparallel() )
        {
            const OutputDevice *pOutDev = GetOutDev();
            pOutDev->ReMirror( aSalPointerState.maPos );
        }
        aState.maPos = ScreenToOutputPixel( aSalPointerState.maPos );
        aState.mnState = aSalPointerState.mnState;
    }
    return aState;
}

bool Window::IsMouseOver() const
{
    return mpInput->isMouseOver();
}

void Window::EnterWait()
{

    mpPointerState->mnWaitCount++;

    if ( mpPointerState->mnWaitCount == 1 )
    {
        // possibly immediately move pointer
        if ( !mpPlatformState->mpFrameData->mbInMouseMove && ImplTestMousePointerSet() )
            mpPlatformState->mpFrame->SetPointer( ImplGetMousePointer() );
    }
}

void Window::LeaveWait()
{
    if( !mpPointerState )
        return;

    if ( mpPointerState->mnWaitCount )
    {
        mpPointerState->mnWaitCount--;

        if ( !mpPointerState->mnWaitCount )
        {
            // possibly immediately move pointer
            if ( mpPlatformState && !mpPlatformState->mpFrameData->mbInMouseMove && ImplTestMousePointerSet() )
                mpPlatformState->mpFrame->SetPointer( ImplGetMousePointer() );
        }
    }
}

bool Window::ImplStopDnd()
{
    bool bRet = false;
    if( mpPlatformState->mpFrameData && mpPlatformState->mpFrameData->mxDropTargetListener.is() )
    {
        bRet = true;
        mpPlatformState->mpFrameData->mxDropTarget.clear();
        mpPlatformState->mpFrameData->mxDragSource.clear();
        mpPlatformState->mpFrameData->mxDropTargetListener.clear();
    }

    return bRet;
}

void Window::ImplStartDnd()
{
    GetDropTarget();
}

rtl::Reference<DNDListenerContainer> Window::GetDropTarget()
{
    if (!mpClassification || !mpLOKData)
        return {};

    if (!mpLOKData->mxDNDListenerContainer.is())
    {
        sal_Int8 nDefaultActions = 0;

        ImplFrameData* pFrameData = mpPlatformState ? mpPlatformState->mpFrameData : nullptr;
        if (pFrameData)
        {
            if (!pFrameData->mxDropTarget.is())
            {
                // Initialization is done in GetDragSource
                GetDragSource();
            }

            if (pFrameData->mxDropTarget.is())
            {
                nDefaultActions = pFrameData->mxDropTarget->getDefaultActions();

                if (!pFrameData->mxDropTargetListener.is())
                {
                    vcl::Window* pFrameWindow = mpHierarchy ? mpHierarchy->getFrameWindow() : nullptr;
                    pFrameData->mxDropTargetListener = new DNDEventDispatcher(pFrameWindow);

                    try
                    {
                        pFrameData->mxDropTarget->addDropTargetListener(pFrameData->mxDropTargetListener);

                        // Register also as drag gesture listener if directly supported by drag source
                        css::uno::Reference<css::datatransfer::dnd::XDragGestureRecognizer> xDragGestureRecognizer(
                            pFrameData->mxDragSource, css::uno::UNO_QUERY);

                        if (xDragGestureRecognizer.is())
                        {
                            xDragGestureRecognizer->addDragGestureListener(pFrameData->mxDropTargetListener);
                        }
                        else
                        {
                            pFrameData->mbInternalDragGestureRecognizer = true;
                        }
                    }
                    catch (const css::uno::RuntimeException&)
                    {
                        // Release all instances
                        pFrameData->mxDropTarget.clear();
                        pFrameData->mxDragSource.clear();
                    }
                }
            }
        }

        mpLOKData->mxDNDListenerContainer = new DNDListenerContainer(nDefaultActions);
    }

    // This object is located in the same process, so there will be no runtime exception
    return mpLOKData->mxDNDListenerContainer;
}

Reference< css::datatransfer::dnd::XDragSource > Window::GetDragSource()
{
#if HAVE_FEATURE_DESKTOP
    const SystemEnvData* pEnvData = GetSystemData();
    if (!mpPlatformState->mpFrameData || !pEnvData)
        return Reference<css::datatransfer::dnd::XDragSource>();
    if (mpPlatformState->mpFrameData->mxDragSource.is())
        return mpPlatformState->mpFrameData->mxDragSource;

    try
    {
        SalInstance* pInst = GetSalInstance();
        mpPlatformState->mpFrameData->mxDragSource = pInst->CreateDragSource(*pEnvData);
        mpPlatformState->mpFrameData->mxDropTarget = pInst->CreateDropTarget(*pEnvData);
    }
    catch (const Exception&)
    {
        mpPlatformState->mpFrameData->mxDropTarget.clear();
        mpPlatformState->mpFrameData->mxDragSource.clear();
    }
    return mpPlatformState->mpFrameData->mxDragSource;
#else
    return Reference< css::datatransfer::dnd::XDragSource > ();
#endif
}

} /* namespace vcl */

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
