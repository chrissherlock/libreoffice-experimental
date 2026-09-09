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

#include <ImplFrameData.hxx>
#include <ImplWinData.hxx>
#include <WindowImpl.hxx>
#include <WindowPlatformState.hxx>
#include <WindowVisibilityState.hxx>
#include <WindowInput.hxx>
#include <WindowLOKData.hxx>
#include <WindowControlAppearance.hxx>
#include <WindowClippingState.hxx>
#include <WindowHierarchy.hxx>
#include <WindowFocusState.hxx>
#include <WindowPointerState.hxx>
#include <WindowControlState.hxx>
#include <svdata.hxx>
#include <salobj.hxx>
#include <salgdi.hxx>
#include <salframe.hxx>
#include <salinst.hxx>

#include <dndeventdispatcher.hxx>

#include <com/sun/star/datatransfer/dnd/XDragSource.hpp>
#include <com/sun/star/datatransfer/dnd/XDropTarget.hpp>
#include <com/sun/star/uno/XComponentContext.hpp>

#include <comphelper/processfactory.hxx>

using namespace ::com::sun::star::uno;

namespace vcl {

WindowHitTest Window::ImplHitTest( const Point& rFramePos )
{
    Point aFramePos( rFramePos );
    if( GetOutDev()->ImplIsAntiparallel() )
    {
        const OutputDevice *pOutDev = GetOutDev();
        pOutDev->ReMirror( aFramePos );
    }

    if ( !GetOutputRectPixel().Contains( aFramePos ) )
        return WindowHitTest::NONE;

    if ( mpClippingState->mbWinRegion )
    {
        Point aTempPos = aFramePos;
        aTempPos.AdjustX( -GetOutDev()->GetDeviceOriginX() );
        aTempPos.AdjustY( -GetOutDev()->GetDeviceOriginY() );

        if ( !mpClippingState->maWinRegion.Contains( aTempPos ) )
            return WindowHitTest::NONE;
    }

    WindowHitTest nHitTest = WindowHitTest::Inside;
    if ( mpInput->mbMouseTransparent )
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

void Window::ImplCallMouseMove( sal_uInt16 nMouseCode, bool bModChanged )
{
    if ( !(mpPlatformState->mpFrameData->mbMouseIn && mpHierarchy->mpFrameWindow->mpVisibilityState->mbReallyVisible) )
        return;

    sal_uInt64 nTime   = tools::Time::GetSystemTicks();
    tools::Long    nX      = mpPlatformState->mpFrameData->mnLastMouseX;
    tools::Long    nY      = mpPlatformState->mpFrameData->mnLastMouseY;
    sal_uInt16  nCode   = nMouseCode;
    MouseEventModifiers nMode = mpPlatformState->mpFrameData->mnMouseMode;
    bool    bLeave;
    // check for MouseLeave
    bLeave = ((nX < 0) || (nY < 0) ||
              (nX >= mpHierarchy->mpFrameWindow->GetOutDev()->GetOutputWidthPixel()) ||
              (nY >= mpHierarchy->mpFrameWindow->GetOutDev()->GetOutputHeightPixel())) &&
             !ImplGetSVData()->mpWinData->mpCaptureWin;
    nMode |= MouseEventModifiers::SYNTHETIC;
    if ( bModChanged )
        nMode |= MouseEventModifiers::MODIFIERCHANGED;
    ImplHandleMouseEvent( mpHierarchy->mpFrameWindow, NotifyEventType::MOUSEMOVE, bLeave, Point(nX, nY), nTime, nCode, nMode );
}

void Window::ImplGenerateMouseMove()
{
    if ( mpWindowImpl && mpPlatformState->mpFrameData &&
         !mpPlatformState->mpFrameData->mnMouseMoveId )
        mpPlatformState->mpFrameData->mnMouseMoveId = Application::PostUserEvent( LINK( mpHierarchy->mpFrameWindow, Window, ImplGenerateMouseMoveHdl ), nullptr, true );
}

IMPL_LINK_NOARG(Window, ImplGenerateMouseMoveHdl, void*, void)
{
    mpPlatformState->mpFrameData->mnMouseMoveId = nullptr;
    vcl::Window* pCaptureWin = ImplGetSVData()->mpWinData->mpCaptureWin;
    if( ! pCaptureWin ||
        (pCaptureWin->mpWindowImpl && pCaptureWin->mpPlatformState->mpFrame == mpPlatformState->mpFrame)
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
    if (pInput && pInput->mbFakeFocusSet)
        return true;

    return false;
}

void Window::ImplGrabFocus( GetFocusFlags nFlags )
{
    // #143570# no focus for destructing windows
    if( !mpWindowImpl || mpWindowImpl->mbInDispose )
        return;

    // some event listeners do really bad stuff
    // => prepare for the worst
    VclPtr<vcl::Window> xWindow( this );

    // Currently the client window should always get the focus
    // Should the border window at some point be focusable
    // we need to change all GrabFocus() instances in VCL,
    // e.g. in ToTop()

    if ( mpHierarchy->mpClientWindow )
    {
        // For a lack of design we need a little hack here to
        // ensure that dialogs on close pass the focus back to
        // the correct window
        if ( mpInput->mpLastFocusWindow && (mpInput->mpLastFocusWindow.get() != this) &&
             !(mpControlState->mnDlgCtrlFlags & DialogControlFlags::WantFocus) &&
             mpInput->mpLastFocusWindow->IsEnabled() &&
             mpInput->mpLastFocusWindow->IsInputEnabled() &&
             ! mpInput->mpLastFocusWindow->IsInModalMode()
             )
            mpInput->mpLastFocusWindow->GrabFocus();
        else
            mpHierarchy->mpClientWindow->GrabFocus();
        return;
    }
    else if ( mpWindowImpl->mbFrame )
    {
        // For a lack of design we need a little hack here to
        // ensure that dialogs on close pass the focus back to
        // the correct window
        if ( mpInput->mpLastFocusWindow && (mpInput->mpLastFocusWindow.get() != this) &&
             !(mpControlState->mnDlgCtrlFlags & DialogControlFlags::WantFocus) &&
             mpInput->mpLastFocusWindow->IsEnabled() &&
             mpInput->mpLastFocusWindow->IsInputEnabled() &&
             ! mpInput->mpLastFocusWindow->IsInModalMode()
             )
        {
            mpInput->mpLastFocusWindow->GrabFocus();
            return;
        }
    }

    // If the Window is disabled, then we don't change the focus
    if ( !IsEnabled() || !IsInputEnabled() || IsInModalMode() )
        return;

    // we only need to set the focus if it is not already set
    // note: if some other frame is waiting for an asynchronous focus event
    // we also have to post an asynchronous focus event for this frame
    // which is done using ToTop
    ImplSVData* pSVData = ImplGetSVData();

    bool bAsyncFocusWaiting = false;
    vcl::Window *pFrame = pSVData->maFrameData.mpFirstFrame;
    while( pFrame && pFrame->mpWindowImpl && pFrame->mpPlatformState->mpFrameData )
    {
        if( pFrame != mpHierarchy->mpFrameWindow.get() && pFrame->mpPlatformState->mpFrameData->mnFocusId )
        {
            bAsyncFocusWaiting = true;
            break;
        }
        pFrame = pFrame->mpPlatformState->mpFrameData->mpNextFrame;
    }

    bool bHasFocus = lcl_IsWindowFocused(*this);

    bool bMustNotGrabFocus = false;
    // #100242#, check parent hierarchy if some floater prohibits grab focus

    vcl::Window *pParent = this;
    while( pParent )
    {
        if ((pParent->GetStyle() & WB_SYSTEMFLOATWIN) && !(pParent->GetStyle() & WB_MOVEABLE))
        {
            bMustNotGrabFocus = true;
            break;
        }
        if (!pParent->mpWindowImpl)
            break;
        pParent = pParent->mpHierarchy->mpParent;
    }

    if ( !(( pSVData->mpWinData->mpFocusWin.get() != this &&
             !mpWindowImpl->mbInDispose ) ||
           ( bAsyncFocusWaiting && !bHasFocus && !bMustNotGrabFocus )) )
        return;

    // EndExtTextInput if it is not the same window
    if (pSVData->mpWinData->mpExtTextInputWin
        && (pSVData->mpWinData->mpExtTextInputWin.get() != this))
        pSVData->mpWinData->mpExtTextInputWin->EndExtTextInput();

    // mark this windows as the last FocusWindow
    vcl::Window* pOverlapWindow = ImplGetFirstOverlapWindow();
    if (pOverlapWindow->mpWindowImpl)
        pOverlapWindow->mpInput->mpLastFocusWindow = this;
    mpPlatformState->mpFrameData->mpFocusWin = this;

    if( !bHasFocus )
    {
        // menu windows never get the system focus
        // the application will keep the focus
        if( bMustNotGrabFocus )
            return;
        else
        {
            // here we already switch focus as ToTop()
            // should not give focus to another window
            mpPlatformState->mpFrame->ToTop( SalFrameToTop::GrabFocus | SalFrameToTop::GrabFocusOnly );
            return;
        }
    }

    VclPtr<vcl::Window> pOldFocusWindow = pSVData->mpWinData->mpFocusWin;

    pSVData->mpWinData->mpFocusWin = this;

    if ( pOldFocusWindow && pOldFocusWindow->mpWindowImpl )
    {
        // Cursor hidden
        if ( pOldFocusWindow->mpControlAppearance->mpCursor )
            pOldFocusWindow->mpControlAppearance->mpCursor->ImplHide();
    }

    // !!!!! due to old SV-Office Activate/Deactivate handling
    // !!!!! first as before
    if ( pOldFocusWindow )
    {
        // remember Focus
        vcl::Window* pOldOverlapWindow = pOldFocusWindow->ImplGetFirstOverlapWindow();
        vcl::Window* pNewOverlapWindow = ImplGetFirstOverlapWindow();
        if ( pOldOverlapWindow != pNewOverlapWindow )
            ImplCallFocusChangeActivate( pNewOverlapWindow, pOldOverlapWindow );
    }
    else
    {
        vcl::Window* pNewOverlapWindow = ImplGetFirstOverlapWindow();
        if ( pNewOverlapWindow && pNewOverlapWindow->mpWindowImpl )
        {
            vcl::Window* pNewRealWindow = pNewOverlapWindow->ImplGetWindow();
            pNewOverlapWindow->mpFocusState->mbActive = true;
            pNewOverlapWindow->Activate();
            if ( pNewRealWindow != pNewOverlapWindow  && pNewRealWindow && pNewRealWindow->mpWindowImpl )
            {
                pNewRealWindow->mpFocusState->mbActive = true;
                pNewRealWindow->Activate();
            }
        }
    }

    // call Get- and LoseFocus
    if ( pOldFocusWindow && ! pOldFocusWindow->isDisposed() )
    {
        NotifyEvent aNEvt( NotifyEventType::LOSEFOCUS, pOldFocusWindow );
        if ( !ImplCallPreNotify( aNEvt ) )
            pOldFocusWindow->CompatLoseFocus();
        pOldFocusWindow->ImplCallDeactivateListeners( this );
    }

    if (pSVData->mpWinData->mpFocusWin.get() == this)
    {
        if ( mpPlatformState->mpSysObj )
        {
            mpPlatformState->mpFrameData->mpFocusWin = this;
            if ( !mpPlatformState->mpFrameData->mbInSysObjFocusHdl )
                mpPlatformState->mpSysObj->GrabFocus();
        }

        if (pSVData->mpWinData->mpFocusWin.get() == this)
        {
            if (mpControlAppearance && mpControlAppearance->mpCursor)
                mpControlAppearance->mpCursor->ImplShow();

            mpInput->mbInFocusHdl = true;
            mpFocusState->mnGetFocusFlags = nFlags;

            // if we're changing focus due to closing a popup floating window
            // notify the new focus window so it can restore the inner focus
            // eg, toolboxes can select their recent active item
            if( pOldFocusWindow &&
                ! pOldFocusWindow->isDisposed() &&
                ( pOldFocusWindow->GetDialogControlFlags() & DialogControlFlags::FloatWinPopupModeEndCancel ) )
            {
                mpFocusState->mnGetFocusFlags |= GetFocusFlags::FloatWinPopupModeEndCancel;
            }

            NotifyEvent aNEvt( NotifyEventType::GETFOCUS, this );

            if ( !ImplCallPreNotify( aNEvt ) && !xWindow->isDisposed() )
                CompatGetFocus();

            if( !xWindow->isDisposed() )
            {
                if (pOldFocusWindow && pOldFocusWindow->isDisposed())
                    pOldFocusWindow = nullptr;
                ImplCallActivateListeners(pOldFocusWindow);
            }

            if( !xWindow->isDisposed() )
            {
                mpFocusState->mnGetFocusFlags = GetFocusFlags::NONE;
                mpInput->mbInFocusHdl = false;
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
    if (!EventNotify(aNEvt) && mpWindowImpl)
        mpInput->mbMouseButtonDown = true;
}

void Window::MouseButtonUp( const MouseEvent& rMEvt )
{
    NotifyEvent aNEvt( NotifyEventType::MOUSEBUTTONUP, this, &rMEvt );
    if (!EventNotify(aNEvt) && mpWindowImpl)
        mpInput->mbMouseButtonUp = true;
}

void Window::SetMouseTransparent( bool bTransparent )
{

    if ( mpHierarchy->mpBorderWindow )
        mpHierarchy->mpBorderWindow->SetMouseTransparent( bTransparent );

    if( mpPlatformState->mpSysObj )
        mpPlatformState->mpSysObj->SetMouseTransparent( bTransparent );

    mpInput->mbMouseTransparent = bTransparent;
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
        if (mpWindowImpl && mpPlatformState->mpFrame)
            mpPlatformState->mpFrame->CaptureMouse( false );
        ImplGenerateMouseMove();
    }
}

bool Window::IsMouseCaptured() const
{
    return (this == ImplGetSVData()->mpWinData->mpCaptureWin);
}

void Window::SetPointer( PointerStyle nPointer )
{
    if (mpControlAppearance->maPointer == nPointer)
        return;

    mpControlAppearance->maPointer = nPointer;

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
    return ImplGetWinData()->mbMouseOver;
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
    if( !mpWindowImpl )
        return {};

    if( ! mpLOKData->mxDNDListenerContainer.is() )
    {
        sal_Int8 nDefaultActions = 0;

        if( mpPlatformState->mpFrameData )
        {
            if( ! mpPlatformState->mpFrameData->mxDropTarget.is() )
            {
                // initialization is done in GetDragSource
                GetDragSource();
            }

            if( mpPlatformState->mpFrameData->mxDropTarget.is() )
            {
                nDefaultActions = mpPlatformState->mpFrameData->mxDropTarget->getDefaultActions();

                if( ! mpPlatformState->mpFrameData->mxDropTargetListener.is() )
                {
                    mpPlatformState->mpFrameData->mxDropTargetListener = new DNDEventDispatcher( mpHierarchy->mpFrameWindow );

                    try
                    {
                        mpPlatformState->mpFrameData->mxDropTarget->addDropTargetListener( mpPlatformState->mpFrameData->mxDropTargetListener );

                        // register also as drag gesture listener if directly supported by drag source
                        Reference< css::datatransfer::dnd::XDragGestureRecognizer > xDragGestureRecognizer(
                            mpPlatformState->mpFrameData->mxDragSource, UNO_QUERY);

                        if( xDragGestureRecognizer.is() )
                        {
                            xDragGestureRecognizer->addDragGestureListener(mpPlatformState->mpFrameData->mxDropTargetListener);
                        }
                        else
                            mpPlatformState->mpFrameData->mbInternalDragGestureRecognizer = true;

                    }
                    catch (const RuntimeException&)
                    {
                        // release all instances
                        mpPlatformState->mpFrameData->mxDropTarget.clear();
                        mpPlatformState->mpFrameData->mxDragSource.clear();
                    }
                }
            }

        }

        mpLOKData->mxDNDListenerContainer = new DNDListenerContainer( nDefaultActions );
    }

    // this object is located in the same process, so there will be no runtime exception
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
