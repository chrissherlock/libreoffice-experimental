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

#include <o3tl/safeint.hxx>
#include <tools/debug.hxx>
#include <tools/time.hxx>
#include <sal/log.hxx>

#include <unotools/localedatawrapper.hxx>

#include <dndeventdispatcher.hxx>
#include <comphelper/lok.hxx>
#include <vcl/QueueInfo.hxx>
#include <vcl/dndlistenercontainer.hxx>
#include <vcl/timer.hxx>
#include <vcl/event.hxx>
#include <vcl/GestureEventPan.hxx>
#include <vcl/GestureEventZoom.hxx>
#include <vcl/GestureEventRotate.hxx>
#include <vcl/settings.hxx>
#include <vcl/svapp.hxx>
#include <vcl/cursor.hxx>
#include <vcl/wrkwin.hxx>
#include <vcl/toolkit/floatwin.hxx>
#include <vcl/toolkit/dialog.hxx>
#include <vcl/toolkit/edit.hxx>
#include <vcl/help.hxx>
#include <vcl/dockwin.hxx>
#include <vcl/menu.hxx>
#include <vcl/virdev.hxx>
#include <vcl/uitest/logger.hxx>
#include <vcl/ptrstyle.hxx>
#include <vcl/CoordinateMapper.hxx>
#include <vcl/MappingPolicy.hxx>

#include <clipping_window.hxx>
#include <svdata.hxx>
#include <salwtype.hxx>
#include <salframe.hxx>
#include <accmgr.hxx>
#include <print.h>
#include <window.h>
#include <helpwin.hxx>
#include <brdwin.hxx>

#include "GenericDropTargetDropContext.hxx"
#include "GenericDropTargetDragContext.hxx"

#include <com/sun/star/datatransfer/dnd/DNDConstants.hpp>
#include <com/sun/star/datatransfer/dnd/XDragSource.hpp>
#include <com/sun/star/awt/MouseEvent.hpp>

#include <algorithm>

static bool lcl_HandleMouseFloatMode( vcl::Window* pChild, const Point& rMousePos,
                                      sal_uInt16 nCode, NotifyEventType nSVEvent,
                                      bool bMouseLeave )
{
    ImplSVData* pSVData = ImplGetSVData();

    if (!pSVData->mpWinData->mpFirstFloat || pSVData->mpWinData->mpCaptureWin
        || pSVData->mpWinData->mpFirstFloat->ImplIsFloatPopupModeWindow(pChild))
    {
        return false;
    }

    /*
     *  #93895# since floats are system windows, coordinates have
     *  to be converted to float relative for the hittest
     */
    bool            bHitTestInsideRect = false;
    FloatingWindow* pFloat = pSVData->mpWinData->mpFirstFloat->ImplFloatHitTest( pChild, rMousePos, bHitTestInsideRect );
    if ( nSVEvent == NotifyEventType::MOUSEMOVE )
    {
        if ( bMouseLeave )
            return true;

        if ( !pFloat || bHitTestInsideRect )
        {
            if ( ImplGetSVHelpData().mpHelpWin && !ImplGetSVHelpData().mbKeyboardHelp )
                ImplDestroyHelpWindow( true );
            pChild->ImplGetFrame()->SetPointer( PointerStyle::Arrow );
            return true;
        }

        return false;
    }

    if ( nCode & MOUSE_LEFT )
    {
        if ( nSVEvent == NotifyEventType::MOUSEBUTTONDOWN )
        {
            if ( !pFloat )
            {
                FloatingWindow* pLastLevelFloat = pSVData->mpWinData->mpFirstFloat->ImplFindLastLevelFloat();
                pLastLevelFloat->EndPopupMode( FloatWinPopupEndFlags::Cancel | FloatWinPopupEndFlags::CloseAll );
                return true;
            }
            else if ( bHitTestInsideRect )
            {
                pFloat->ImplSetMouseDown();
                return true;
            }
        }
        else
        {
            if ( pFloat )
            {
                if ( bHitTestInsideRect )
                {
                    if ( pFloat->ImplIsMouseDown() )
                        pFloat->EndPopupMode( FloatWinPopupEndFlags::Cancel );
                    return true;
                }
            }
            else
            {
                FloatingWindow* pLastLevelFloat = pSVData->mpWinData->mpFirstFloat->ImplFindLastLevelFloat();
                FloatWinPopupFlags nPopupFlags = pLastLevelFloat->GetPopupModeFlags();
                if ( !(nPopupFlags & FloatWinPopupFlags::NoMouseUpClose) )
                {
                    pLastLevelFloat->EndPopupMode( FloatWinPopupEndFlags::Cancel | FloatWinPopupEndFlags::CloseAll );
                    return true;
                }
            }
        }

        return false;
    }

    if (pFloat)
        return false;

    FloatingWindow* pLastLevelFloat = pSVData->mpWinData->mpFirstFloat->ImplFindLastLevelFloat();
    FloatWinPopupFlags nPopupFlags = pLastLevelFloat->GetPopupModeFlags();

    if ( nPopupFlags & FloatWinPopupFlags::AllMouseButtonClose )
    {
        if ( (nPopupFlags & FloatWinPopupFlags::NoMouseUpClose) &&
             (nSVEvent == NotifyEventType::MOUSEBUTTONUP) )
        {
            return true;
        }

        pLastLevelFloat->EndPopupMode( FloatWinPopupEndFlags::Cancel | FloatWinPopupEndFlags::CloseAll );
    }

    return true;
}

static void lcl_HandleMouseHelpRequest( vcl::Window* pChild, const Point& rMousePos )
{
    ImplSVHelpData& aHelpData = ImplGetSVHelpData();
    if ( aHelpData.mpHelpWin &&
         ( aHelpData.mpHelpWin->IsWindowOrChild( pChild ) ||
           pChild->IsWindowOrChild( aHelpData.mpHelpWin ) ))
        return;

    HelpEventMode nHelpMode = HelpEventMode::NONE;
    if ( aHelpData.mbQuickHelp )
        nHelpMode = HelpEventMode::QUICK;
    if ( aHelpData.mbBalloonHelp )
        nHelpMode |= HelpEventMode::BALLOON;
    if ( !(bool(nHelpMode)) )
        return;

    if ( pChild->IsInputEnabled() && !pChild->IsInModalMode() )
    {
        HelpEvent aHelpEvent( rMousePos, nHelpMode );
        aHelpData.mbRequestingHelp = true;
        pChild->RequestHelp( aHelpEvent );
        aHelpData.mbRequestingHelp = false;
    }
    // #104172# do not kill keyboard activated tooltips
    else if ( aHelpData.mpHelpWin && !aHelpData.mbKeyboardHelp)
    {
        ImplDestroyHelpWindow( true );
    }
}

static void lcl_SetMousePointer( vcl::Window const * pChild )
{
    if ( ImplGetSVHelpData().mbExtHelpMode )
        pChild->ImplGetFrame()->SetPointer( PointerStyle::Help );
    else
        pChild->ImplGetFrame()->SetPointer( pChild->ImplGetMousePointer() );
}

/*  #i34277# delayed context menu activation;
*   necessary if there already was a popup menu running.
*/

namespace {

struct ContextMenuEvent
{
    VclPtr<vcl::Window>  pWindow;
    Point           aChildPos;
};

}

static void lcl_ContextMenuEventLink( void* pCEvent, void* )
{
    ContextMenuEvent* pEv = static_cast<ContextMenuEvent*>(pCEvent);

    if( ! pEv->pWindow->isDisposed() )
    {
        ImplCallCommand( pEv->pWindow, CommandEventId::ContextMenu, nullptr, true, &pEv->aChildPos );
    }
    delete pEv;
}

bool ImplHandleMouseEvent( const VclPtr<vcl::Window>& xWindow, NotifyEventType nSVEvent, bool bMouseLeave,
                           tools::Long nX, tools::Long nY, sal_uInt64 nMsgTime,
                           sal_uInt16 nCode, MouseEventModifiers nMode )
{
    SAL_INFO( "vcl.debugevent",
              "mouse event "
               "(NotifyEventType " << static_cast<sal_uInt16>(nSVEvent) << ") "
               "(MouseLeave " << bMouseLeave << ") "
               "(X, Y " << nX << ", " << nY << ") "
               "(Code " << nCode << ") "
               "(Modifiers " << static_cast<sal_uInt16>(nMode) << ")");
    ImplSVHelpData& aHelpData = ImplGetSVHelpData();
    ImplSVData* pSVData = ImplGetSVData();
    Point       aMousePos( nX, nY );
    VclPtr<vcl::Window> pChild;
    bool        bRet(false);
    sal_uInt16  nClicks(0);
    ImplFrameData* pWinFrameData = xWindow->ImplGetFrameData();
    sal_uInt16      nOldCode = pWinFrameData->mnMouseCode;

    if (comphelper::LibreOfficeKit::isActive() && AllSettings::GetLayoutRTL()
        && xWindow->GetOutDev() && !xWindow->GetOutDev()->ImplIsAntiparallel())
    {
        xWindow->GetOutDev()->ReMirror(aMousePos);
        nX = aMousePos.X();
        nY = aMousePos.Y();
    }

    // we need a mousemove event, before we get a mousebuttondown or a
    // mousebuttonup event
    if ( (nSVEvent == NotifyEventType::MOUSEBUTTONDOWN) || (nSVEvent == NotifyEventType::MOUSEBUTTONUP) )
    {
        if ( (nSVEvent == NotifyEventType::MOUSEBUTTONUP) && aHelpData.mbExtHelpMode )
            Help::EndExtHelp();

        if ( aHelpData.mpHelpWin )
        {
            if( xWindow->ImplGetWindow() == aHelpData.mpHelpWin )
            {
                ImplDestroyHelpWindow( false );
                return true; // xWindow is dead now - avoid crash!
            }
            else
            {
                ImplDestroyHelpWindow( true );
            }
        }

        if ( (pWinFrameData->mnLastMouseX != nX) ||
             (pWinFrameData->mnLastMouseY != nY) )
        {
            sal_uInt16 nMoveCode = nCode & ~(MOUSE_LEFT | MOUSE_RIGHT | MOUSE_MIDDLE);
            ImplHandleMouseEvent(xWindow, NotifyEventType::MOUSEMOVE, false, nX, nY, nMsgTime, nMoveCode, nMode);
        }
    }

    // update frame data
    pWinFrameData->mnBeforeLastMouseX = pWinFrameData->mnLastMouseX;
    pWinFrameData->mnBeforeLastMouseY = pWinFrameData->mnLastMouseY;
    pWinFrameData->mnLastMouseX = nX;
    pWinFrameData->mnLastMouseY = nY;
    pWinFrameData->mnMouseCode  = nCode;
    MouseEventModifiers const nTmpMask = MouseEventModifiers::SYNTHETIC | MouseEventModifiers::MODIFIERCHANGED;
    pWinFrameData->mnMouseMode  = nMode & ~nTmpMask;

    if ( bMouseLeave )
    {
        pWinFrameData->mbMouseIn = false;
        if ( ImplGetSVHelpData().mpHelpWin && !ImplGetSVHelpData().mbKeyboardHelp )
        {
            ImplDestroyHelpWindow( true );

            if ( xWindow->isDisposed() )
                return true; // xWindow is dead now - avoid crash! (#122045#)
        }
    }
    else
    {
        pWinFrameData->mbMouseIn = true;
    }

    DBG_ASSERT(!pSVData->mpWinData->mpTrackWin
                   || (pSVData->mpWinData->mpTrackWin == pSVData->mpWinData->mpCaptureWin),
               "ImplHandleMouseEvent: TrackWin != CaptureWin");

    // AutoScrollMode
    if (pSVData->mpWinData->mpAutoScrollWin && (nSVEvent == NotifyEventType::MOUSEBUTTONDOWN))
    {
        pSVData->mpWinData->mpAutoScrollWin->EndAutoScroll();
        return true;
    }

    // find mouse window
    if (pSVData->mpWinData->mpCaptureWin)
    {
        pChild = pSVData->mpWinData->mpCaptureWin;

        SAL_WARN_IF( xWindow != pChild->ImplGetFrameWindow(), "vcl",
                    "ImplHandleMouseEvent: mouse event is not sent to capture window" );

        // java client cannot capture mouse correctly
        if ( xWindow != pChild->ImplGetFrameWindow() )
            return false;

        if ( bMouseLeave )
            return false;
    }
    else
    {
        if ( bMouseLeave )
            pChild = nullptr;
        else
            pChild = xWindow->ImplFindWindow( aMousePos );
    }

    // test this because mouse events are buffered in the remote version
    // and size may not be in sync
    if ( !pChild && !bMouseLeave )
        return false;

    // execute a few tests and catch the message or implement the status
    if ( pChild )
    {
        if( pChild->GetOutDev()->ImplIsAntiparallel() )
        {
            // re-mirror frame pos at pChild
            const OutputDevice *pChildWinOutDev = pChild->GetOutDev();
            pChildWinOutDev->ReMirror( aMousePos );
        }

        // no mouse messages to disabled windows
        // #106845# if the window was disabled during capturing we have to pass the mouse events to release capturing
        if (pSVData->mpWinData->mpCaptureWin.get() != pChild
            && (!pChild->IsEnabled() || !pChild->IsInputEnabled() || pChild->IsInModalMode()))
        {
            lcl_HandleMouseFloatMode( pChild, aMousePos, nCode, nSVEvent, bMouseLeave );
            if ( nSVEvent == NotifyEventType::MOUSEMOVE )
            {
                lcl_HandleMouseHelpRequest( pChild, aMousePos );
                if( pWinFrameData->mpMouseMoveWin.get() != pChild )
                    nMode |= MouseEventModifiers::ENTERWINDOW;
            }

            // Call the hook also, if Window is disabled

            if ( nSVEvent == NotifyEventType::MOUSEBUTTONDOWN )
            {
                return true;
            }
            else
            {
                // Set normal MousePointer for disabled windows
                if ( nSVEvent == NotifyEventType::MOUSEMOVE )
                    lcl_SetMousePointer( pChild );

                return false;
            }
        }

        // End ExtTextInput-Mode, if the user click in the same TopLevel Window
        if (pSVData->mpWinData->mpExtTextInputWin
            && ((nSVEvent == NotifyEventType::MOUSEBUTTONDOWN)
                || (nSVEvent == NotifyEventType::MOUSEBUTTONUP)))
            pSVData->mpWinData->mpExtTextInputWin->EndExtTextInput();
    }

    // determine mouse event data
    if ( nSVEvent == NotifyEventType::MOUSEMOVE )
    {
        // check if MouseMove belongs to same window and if the
        // status did not change
        if ( pChild )
        {
            Point aChildMousePos = pChild->ScreenToOutputPixel( aMousePos );
            if ( !bMouseLeave &&
                 (pChild == pWinFrameData->mpMouseMoveWin) &&
                 (aChildMousePos.X() == pWinFrameData->mnLastMouseWinX) &&
                 (aChildMousePos.Y() == pWinFrameData->mnLastMouseWinY) &&
                 (nOldCode == pWinFrameData->mnMouseCode) )
            {
                // set mouse pointer anew, as it could have changed
                // due to the mode switch
                lcl_SetMousePointer( pChild );
                return false;
            }

            pWinFrameData->mnLastMouseWinX = aChildMousePos.X();
            pWinFrameData->mnLastMouseWinY = aChildMousePos.Y();
        }

        // mouse click
        nClicks = pWinFrameData->mnClickCount;

        // call Start-Drag handler if required
        // Warning: should be called before Move, as otherwise during
        // fast mouse movements the applications move to the selection state
        vcl::Window* pMouseDownWin = pWinFrameData->mpMouseDownWin;
        if ( pMouseDownWin )
        {
            // check for matching StartDrag mode. We only compare
            // the status of the mouse buttons, such that e. g. Mod1 can
            // change immediately to the copy mode
            const MouseSettings& rMSettings = pMouseDownWin->GetSettings().GetMouseSettings();
            if ( (nCode & (MOUSE_LEFT | MOUSE_RIGHT | MOUSE_MIDDLE)) ==
                 (MouseSettings::GetStartDragCode() & (MOUSE_LEFT | MOUSE_RIGHT | MOUSE_MIDDLE)) )
            {
                if ( !pMouseDownWin->ImplGetFrameData()->mbStartDragCalled )
                {
                    tools::Long nDragW  = rMSettings.GetStartDragWidth();
                    tools::Long nDragH  = rMSettings.GetStartDragHeight();
                    //long nMouseX = nX;
                    //long nMouseY = nY;
                    tools::Long nMouseX = aMousePos.X(); // #106074# use the possibly re-mirrored coordinates (RTL) ! nX,nY are unmodified !
                    tools::Long nMouseY = aMousePos.Y();
                    if ( (((nMouseX-nDragW) > pMouseDownWin->ImplGetFrameData()->mnFirstMouseX) ||
                           ((nMouseX+nDragW) < pMouseDownWin->ImplGetFrameData()->mnFirstMouseX)) ||
                         (((nMouseY-nDragH) > pMouseDownWin->ImplGetFrameData()->mnFirstMouseY) ||
                           ((nMouseY+nDragH) < pMouseDownWin->ImplGetFrameData()->mnFirstMouseY)) )
                    {
                        pMouseDownWin->ImplGetFrameData()->mbStartDragCalled  = true;

                        // Check if drag source provides its own recognizer
                        if( pMouseDownWin->ImplGetFrameData()->mbInternalDragGestureRecognizer )
                        {
                            // query DropTarget from child window
                            rtl::Reference< DNDListenerContainer > xDragGestureRecognizer(
                                    pMouseDownWin->ImplGetWindowImpl()->mxDNDListenerContainer );

                            if( xDragGestureRecognizer.is() )
                            {
                                // retrieve mouse position relative to mouse down window
                                Point relLoc = pMouseDownWin->ScreenToOutputPixel( Point(
                                    pMouseDownWin->ImplGetFrameData()->mnFirstMouseX,
                                    pMouseDownWin->ImplGetFrameData()->mnFirstMouseY ) );

                                // create a UNO mouse event out of the available data
                                css::awt::MouseEvent aMouseEvent( static_cast < css::uno::XInterface * > ( nullptr ),
#ifdef MACOSX
                                    nCode & (KEY_SHIFT | KEY_MOD1 | KEY_MOD2 | KEY_MOD3),
#else
                                    nCode & (KEY_SHIFT | KEY_MOD1 | KEY_MOD2),
#endif
                                    nCode & (MOUSE_LEFT | MOUSE_RIGHT | MOUSE_MIDDLE),
                                    nMouseX,
                                    nMouseY,
                                    nClicks,
                                    false );

                                SolarMutexReleaser aReleaser;

                                // FIXME: where do I get Action from ?
                                css::uno::Reference< css::datatransfer::dnd::XDragSource > xDragSource = pMouseDownWin->GetDragSource();

                                if( xDragSource.is() )
                                {
                                    xDragGestureRecognizer->fireDragGestureEvent( 0,
                                        relLoc.X(), relLoc.Y(), xDragSource, css::uno::Any( aMouseEvent ) );
                                }
                            }
                        }
                    }
                }
            }
            else
            {
                pMouseDownWin->ImplGetFrameData()->mbStartDragCalled  = true;
            }
        }

        if (xWindow->isDisposed())
            return true;

        // test for mouseleave and mouseenter
        VclPtr<vcl::Window> pMouseMoveWin = pWinFrameData->mpMouseMoveWin;
        if ( pChild != pMouseMoveWin )
        {
            if ( pMouseMoveWin )
            {
                Point       aLeaveMousePos = pMouseMoveWin->ScreenToOutputPixel( aMousePos );
                MouseEvent  aMLeaveEvt( aLeaveMousePos, nClicks, nMode | MouseEventModifiers::LEAVEWINDOW, nCode, nCode );
                NotifyEvent aNLeaveEvt( NotifyEventType::MOUSEMOVE, pMouseMoveWin, &aMLeaveEvt );
                pWinFrameData->mbInMouseMove = true;
                pMouseMoveWin->ImplGetWinData()->mbMouseOver = false;

                // A MouseLeave can destroy this window
                if ( !ImplCallPreNotify( aNLeaveEvt ) )
                {
                    pMouseMoveWin->MouseMove( aMLeaveEvt );
                    if( !pMouseMoveWin->isDisposed() )
                        aNLeaveEvt.GetWindow()->ImplNotifyKeyMouseCommandEventListeners( aNLeaveEvt );
                }

                pWinFrameData->mpMouseMoveWin = nullptr;
                pWinFrameData->mbInMouseMove = false;

                if ( pChild && pChild->isDisposed() )
                    pChild = nullptr;
                if ( pMouseMoveWin->isDisposed() )
                    return true;
            }

            nMode |= MouseEventModifiers::ENTERWINDOW;
        }

        pWinFrameData->mpMouseMoveWin = pChild;

        if( pChild )
            pChild->ImplGetWinData()->mbMouseOver = true;

        // MouseLeave
        if ( !pChild )
            return false;
    }
    else
    {
        if (pChild)
        {
            // mouse click
            if ( nSVEvent == NotifyEventType::MOUSEBUTTONDOWN )
            {
                const MouseSettings& rMSettings = pChild->GetSettings().GetMouseSettings();
                sal_uInt64 nDblClkTime = rMSettings.GetDoubleClickTime();
                tools::Long    nDblClkW    = rMSettings.GetDoubleClickWidth();
                tools::Long    nDblClkH    = rMSettings.GetDoubleClickHeight();
                //long    nMouseX     = nX;
                //long    nMouseY     = nY;
                tools::Long nMouseX = aMousePos.X();   // #106074# use the possibly re-mirrored coordinates (RTL) ! nX,nY are unmodified !
                tools::Long nMouseY = aMousePos.Y();

                if ( (pChild == pChild->ImplGetFrameData()->mpMouseDownWin) &&
                     (nCode == pChild->ImplGetFrameData()->mnFirstMouseCode) &&
                     ((nMsgTime-pChild->ImplGetFrameData()->mnMouseDownTime) < nDblClkTime) &&
                     ((nMouseX-nDblClkW) <= pChild->ImplGetFrameData()->mnFirstMouseX) &&
                     ((nMouseX+nDblClkW) >= pChild->ImplGetFrameData()->mnFirstMouseX) &&
                     ((nMouseY-nDblClkH) <= pChild->ImplGetFrameData()->mnFirstMouseY) &&
                     ((nMouseY+nDblClkH) >= pChild->ImplGetFrameData()->mnFirstMouseY) )
                {
                    pChild->ImplGetFrameData()->mnClickCount++;
                    pChild->ImplGetFrameData()->mbStartDragCalled  = true;
                }
                else
                {
                    pChild->ImplGetFrameData()->mpMouseDownWin     = pChild;
                    pChild->ImplGetFrameData()->mnClickCount       = 1;
                    pChild->ImplGetFrameData()->mnFirstMouseX      = nMouseX;
                    pChild->ImplGetFrameData()->mnFirstMouseY      = nMouseY;
                    pChild->ImplGetFrameData()->mnFirstMouseCode   = nCode;
                    pChild->ImplGetFrameData()->mbStartDragCalled  = (nCode & (MOUSE_LEFT | MOUSE_RIGHT | MOUSE_MIDDLE)) !=
                                                                     (MouseSettings::GetStartDragCode() & (MOUSE_LEFT | MOUSE_RIGHT | MOUSE_MIDDLE));
                }
                pChild->ImplGetFrameData()->mnMouseDownTime = nMsgTime;
            }

            nClicks = pChild->ImplGetFrameData()->mnClickCount;
        }

        pSVData->maAppData.mnLastInputTime = tools::Time::GetSystemTicks();
    }

    SAL_WARN_IF( !pChild, "vcl", "ImplHandleMouseEvent: pChild == NULL" );

    if (!pChild)
        return false;

    // create mouse event
    Point aChildPos = pChild->ScreenToOutputPixel( aMousePos );
    MouseEvent aMEvt( aChildPos, nClicks, nMode, nCode, nCode );


    // tracking window gets the mouse events
    if (pSVData->mpWinData->mpTrackWin)
        pChild = pSVData->mpWinData->mpTrackWin;

    // handle FloatingMode
    if (!pSVData->mpWinData->mpTrackWin && pSVData->mpWinData->mpFirstFloat
        && lcl_HandleMouseFloatMode(pChild, aMousePos, nCode, nSVEvent, bMouseLeave))
    {
        if ( !pChild->isDisposed() )
            pChild->ImplGetFrameData()->mbStartDragCalled = true;

        return true;
    }

    // call handler
    bool bCallHelpRequest = true;
    SAL_WARN_IF( !pChild, "vcl", "ImplHandleMouseEvent: pChild is NULL" );

    if (!pChild)
        return false;

    NotifyEvent aNEvt( nSVEvent, pChild, &aMEvt );
    if ( nSVEvent == NotifyEventType::MOUSEMOVE )
        pChild->ImplGetFrameData()->mbInMouseMove = true;

    // bring window into foreground on mouseclick
    if ( nSVEvent == NotifyEventType::MOUSEBUTTONDOWN )
    {
        if (!pSVData->mpWinData->mpFirstFloat
            && // totop for floating windows in popup would change the focus and would close them immediately
            !(pChild->ImplGetFrameWindow()->GetStyle()
              & WB_OWNERDRAWDECORATION)) // ownerdrawdecorated windows must never grab focus
        {
            pChild->ToTop();
        }

        if ( pChild->isDisposed() )
            return true;
    }

    if ( ImplCallPreNotify( aNEvt ) || pChild->isDisposed() )
    {
        bRet = true;
    }
    else
    {
        bRet = false;
        if ( nSVEvent == NotifyEventType::MOUSEMOVE )
        {
            if (pSVData->mpWinData->mpTrackWin)
            {
                TrackingEvent aTEvt( aMEvt );
                pChild->Tracking( aTEvt );
                if ( !pChild->isDisposed() )
                {
                    // When ScrollRepeat, we restart the timer
                    if (pSVData->mpWinData->mpTrackTimer
                        && (pSVData->mpWinData->mnTrackFlags & StartTrackingFlags::ScrollRepeat))
                        pSVData->mpWinData->mpTrackTimer->Start();
                }
                bCallHelpRequest = false;
                bRet = true;
            }
            else
            {
                if( pChild->isDisposed() )
                {
                    bCallHelpRequest = false;
                }
                else
                {
                    // if the MouseMove handler changes the help window's visibility
                    // the HelpRequest handler should not be called anymore
                    vcl::Window* pOldHelpTextWin = ImplGetSVHelpData().mpHelpWin;
                    pChild->MouseMove( aMEvt );
                    if ( pOldHelpTextWin != ImplGetSVHelpData().mpHelpWin )
                        bCallHelpRequest = false;
                }
            }
        }
        else if ( nSVEvent == NotifyEventType::MOUSEBUTTONDOWN )
        {
            if ( pSVData->mpWinData->mpTrackWin )
            {
                bRet = true;
            }
            else
            {
                pChild->ImplGetWindowImpl()->mbMouseButtonDown = false;
                pChild->MouseButtonDown( aMEvt );
            }
        }
        else
        {
            if (pSVData->mpWinData->mpTrackWin)
            {
                pChild->EndTracking();
                bRet = true;
            }
            else
            {
                pChild->ImplGetWindowImpl()->mbMouseButtonUp = false;
                pChild->MouseButtonUp( aMEvt );
            }
        }

        assert(aNEvt.GetWindow() == pChild);

        if (!pChild->isDisposed())
            pChild->ImplNotifyKeyMouseCommandEventListeners( aNEvt );
    }

    if (pChild->isDisposed())
        return true;

    if ( nSVEvent == NotifyEventType::MOUSEMOVE )
        pChild->ImplGetWindowImpl()->mpFrameData->mbInMouseMove = false;

    if ( nSVEvent == NotifyEventType::MOUSEMOVE )
    {
        if ( bCallHelpRequest && !ImplGetSVHelpData().mbKeyboardHelp )
            lcl_HandleMouseHelpRequest( pChild, pChild->OutputToScreenPixel( aMEvt.GetPosPixel() ) );
        bRet = true;
    }
    else if ( !bRet )
    {
        if ( nSVEvent == NotifyEventType::MOUSEBUTTONDOWN )
        {
            if ( !pChild->ImplGetWindowImpl()->mbMouseButtonDown )
                bRet = true;
        }
        else
        {
            if ( !pChild->ImplGetWindowImpl()->mbMouseButtonUp )
                bRet = true;
        }
    }

    if ( nSVEvent == NotifyEventType::MOUSEMOVE )
    {
        // set new mouse pointer
        if ( !bMouseLeave )
            lcl_SetMousePointer( pChild );

        return bRet;
    }

    if ( (nSVEvent != NotifyEventType::MOUSEBUTTONDOWN) && (nSVEvent != NotifyEventType::MOUSEBUTTONUP) )
        return bRet;

    // Command-Events
    if ( /*!bRet &&*/ (nClicks == 1) && (nSVEvent == NotifyEventType::MOUSEBUTTONDOWN) &&
         (nCode == MOUSE_MIDDLE) )
    {
        MouseMiddleButtonAction nMiddleAction = pChild->GetSettings().GetMouseSettings().GetMiddleButtonAction();
        if ( nMiddleAction == MouseMiddleButtonAction::AutoScroll )
            bRet = !ImplCallCommand( pChild, CommandEventId::StartAutoScroll, nullptr, true, &aChildPos );
        else if ( nMiddleAction == MouseMiddleButtonAction::PasteSelection )
            bRet = !ImplCallCommand( pChild, CommandEventId::PasteSelection, nullptr, true, &aChildPos );

        return bRet;
    }

    // ContextMenu
    if ( (nCode != MouseSettings::GetContextMenuCode()) ||
         (nClicks != MouseSettings::GetContextMenuClicks()) )
    {
        return bRet;
    }

    bool bContextMenu = (nSVEvent == NotifyEventType::MOUSEBUTTONDOWN);
    if ( !bContextMenu )
        return bRet;

    if ( !pSVData->maAppData.mpActivePopupMenu )
        return !ImplCallCommand( pChild, CommandEventId::ContextMenu, nullptr, true, &aChildPos );

    /*  #i34277# there already is a context menu open
    *   that was probably just closed with EndPopupMode.
    *   We need to give the eventual corresponding
    *   PopupMenu::Execute a chance to end properly.
    *   Therefore delay context menu command and
    *   issue only after popping one frame of the
    *   Yield stack.
    */
    ContextMenuEvent* pEv = new ContextMenuEvent;
    pEv->pWindow = std::move(pChild);
    pEv->aChildPos = aChildPos;
    Application::PostUserEvent( LINK_NONMEMBER( pEv, lcl_ContextMenuEventLink ) );

    return bRet;
}

bool ImplLOKHandleMouseEvent(const VclPtr<vcl::Window>& xWindow, NotifyEventType nEvent, bool /*bMouseLeave*/,
                             tools::Long nX, tools::Long nY, sal_uInt64 /*nMsgTime*/,
                             sal_uInt16 nCode, MouseEventModifiers nMode, sal_uInt16 nClicks)
{
    Point aMousePos(nX, nY);

    if (!xWindow)
        return false;

    if (xWindow->isDisposed())
        return false;

    ImplFrameData* pFrameData = xWindow->ImplGetFrameData();
    if (!pFrameData)
        return false;

    Point aWinPos = xWindow->ScreenToOutputPixel(aMousePos);

    pFrameData->mnLastMouseX = nX;
    pFrameData->mnLastMouseY = nY;
    pFrameData->mnClickCount = nClicks;
    pFrameData->mnMouseCode = nCode;
    pFrameData->mbMouseIn = false;

    vcl::Window* pDragWin = pFrameData->mpMouseDownWin;
    if (pDragWin &&
        nEvent == NotifyEventType::MOUSEMOVE &&
        pFrameData->mbDragging)
    {
        css::uno::Reference<css::datatransfer::dnd::XDropTargetDragContext> xDropTargetDragContext =
            new GenericDropTargetDragContext();
        rtl::Reference<DNDListenerContainer> xDropTarget(
            pDragWin->ImplGetWindowImpl()->mxDNDListenerContainer);

        if (!xDropTarget.is() ||
            !xDropTargetDragContext.is() ||
            (nCode & (MOUSE_LEFT | MOUSE_RIGHT | MOUSE_MIDDLE)) !=
            (MouseSettings::GetStartDragCode() & (MOUSE_LEFT | MOUSE_RIGHT | MOUSE_MIDDLE)))
        {
            pFrameData->mbStartDragCalled = pFrameData->mbDragging = false;
            return false;
        }

        xDropTarget->fireDragOverEvent(
            xDropTargetDragContext,
            css::datatransfer::dnd::DNDConstants::ACTION_MOVE,
            aWinPos.X(),
            aWinPos.Y(),
            (css::datatransfer::dnd::DNDConstants::ACTION_COPY |
             css::datatransfer::dnd::DNDConstants::ACTION_MOVE |
             css::datatransfer::dnd::DNDConstants::ACTION_LINK));

        return true;
    }

    if (pDragWin &&
        nEvent == NotifyEventType::MOUSEBUTTONUP &&
        pFrameData->mbDragging)
    {
        css::uno::Reference<css::datatransfer::XTransferable> xTransfer;
        css::uno::Reference<css::datatransfer::dnd::XDropTargetDropContext> xDropTargetDropContext =
            new GenericDropTargetDropContext();
        rtl::Reference<DNDListenerContainer> xDropTarget(
            pDragWin->ImplGetWindowImpl()->mxDNDListenerContainer);

        if (!xDropTarget.is() || !xDropTargetDropContext.is())
        {
            pFrameData->mbStartDragCalled = pFrameData->mbDragging = false;
            return false;
        }

        Point dragOverPos = pDragWin->ScreenToOutputPixel(aMousePos);
        xDropTarget->fireDropEvent(
            xDropTargetDropContext,
            css::datatransfer::dnd::DNDConstants::ACTION_MOVE,
            dragOverPos.X(),
            dragOverPos.Y(),
            (css::datatransfer::dnd::DNDConstants::ACTION_COPY |
             css::datatransfer::dnd::DNDConstants::ACTION_MOVE |
             css::datatransfer::dnd::DNDConstants::ACTION_LINK),
            xTransfer);

        pFrameData->mbStartDragCalled = pFrameData->mbDragging = false;
        return true;
    }

    if (pFrameData->mbDragging)
    {
        // wrong status, reset
        pFrameData->mbStartDragCalled = pFrameData->mbDragging = false;
        return false;
    }

    vcl::Window* pDownWin = pFrameData->mpMouseDownWin;
    if (pDownWin && nEvent == NotifyEventType::MOUSEMOVE)
    {
        const MouseSettings& aSettings = pDownWin->GetSettings().GetMouseSettings();
        if ((nCode & (MOUSE_LEFT | MOUSE_RIGHT | MOUSE_MIDDLE)) ==
            (MouseSettings::GetStartDragCode() & (MOUSE_LEFT | MOUSE_RIGHT | MOUSE_MIDDLE)) )
        {
            if (!pFrameData->mbStartDragCalled)
            {
                tools::Long nDragWidth = aSettings.GetStartDragWidth();
                tools::Long nDragHeight = aSettings.GetStartDragHeight();
                tools::Long nMouseX = aMousePos.X();
                tools::Long nMouseY = aMousePos.Y();

                if ((((nMouseX - nDragWidth) > pFrameData->mnFirstMouseX) ||
                     ((nMouseX + nDragWidth) < pFrameData->mnFirstMouseX)) ||
                    (((nMouseY - nDragHeight) > pFrameData->mnFirstMouseY) ||
                     ((nMouseY + nDragHeight) < pFrameData->mnFirstMouseY)))
                {
                    pFrameData->mbStartDragCalled  = true;

                    if (pFrameData->mbInternalDragGestureRecognizer)
                    {
                        // query DropTarget from child window
                        rtl::Reference<DNDListenerContainer> xDragGestureRecognizer(
                            pDownWin->ImplGetWindowImpl()->mxDNDListenerContainer );

                        if (xDragGestureRecognizer.is())
                        {
                            // create a UNO mouse event out of the available data
                            css::awt::MouseEvent aEvent(
                                static_cast < css::uno::XInterface * > ( nullptr ),
 #ifdef MACOSX
                                nCode & (KEY_SHIFT | KEY_MOD1 | KEY_MOD2 | KEY_MOD3),
 #else
                                nCode & (KEY_SHIFT | KEY_MOD1 | KEY_MOD2),
 #endif
                                nCode & (MOUSE_LEFT | MOUSE_RIGHT | MOUSE_MIDDLE),
                                nMouseX,
                                nMouseY,
                                nClicks,
                                false);
                            css::uno::Reference< css::datatransfer::dnd::XDragSource > xDragSource =
                                pDownWin->GetDragSource();

                            if (xDragSource.is())
                            {
                                xDragGestureRecognizer->
                                    fireDragGestureEvent(
                                        0,
                                        aWinPos.X(),
                                        aWinPos.Y(),
                                        xDragSource,
                                        css::uno::Any(aEvent));
                            }
                        }
                    }
                }
            }
        }
    }

    MouseEvent aMouseEvent(aWinPos, nClicks, nMode, nCode, nCode);
    if (nEvent == NotifyEventType::MOUSEMOVE)
    {
        if (pFrameData->mpTrackWin)
        {
            TrackingEvent aTrackingEvent(aMouseEvent);
            pFrameData->mpTrackWin->Tracking(aTrackingEvent);
        }
        else
            xWindow->MouseMove(aMouseEvent);
    }
    else if (nEvent == NotifyEventType::MOUSEBUTTONDOWN &&
        !pFrameData->mpTrackWin)
    {
        pFrameData->mpMouseDownWin = xWindow;
        pFrameData->mnFirstMouseX = aMousePos.X();
        pFrameData->mnFirstMouseY = aMousePos.Y();

        xWindow->MouseButtonDown(aMouseEvent);
    }
    else
    {
        if (pFrameData->mpTrackWin)
        {
            pFrameData->mpTrackWin->EndTracking();
        }

        pFrameData->mpMouseDownWin = nullptr;
        pFrameData->mpMouseMoveWin = nullptr;
        pFrameData->mbStartDragCalled = false;
        xWindow->MouseButtonUp(aMouseEvent);
    }

    if (nEvent != NotifyEventType::MOUSEBUTTONDOWN)
        return true;

    // ContextMenu
    if ( (nCode == MouseSettings::GetContextMenuCode()) &&
         (nClicks == MouseSettings::GetContextMenuClicks()) )
    {
       ImplCallCommand(xWindow, CommandEventId::ContextMenu, nullptr, true, &aWinPos);
    }

    return true;
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
