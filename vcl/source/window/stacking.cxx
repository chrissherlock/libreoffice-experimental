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

#include <vcl/dndlistenercontainer.hxx>
#include <vcl/syswin.hxx>
#include <vcl/window.hxx>
#include <vcl/taskpanelist.hxx>
#include <sal/log.hxx>

#include <clipping/ClippingManager.hxx>
#include <salframe.hxx>
#include <salobj.hxx>
#include <svdata.hxx>
#include <window.h>
#include <brdwin.hxx>

#include <com/sun/star/awt/XTopWindow.hpp>
#include <com/sun/star/awt/XVclWindowPeer.hpp>

using namespace ::com::sun::star::uno;
using namespace ::com::sun::star::datatransfer::clipboard;
using namespace ::com::sun::star::datatransfer::dnd;
using namespace ::com::sun::star;

using ::com::sun::star::awt::XTopWindow;

struct ImplCalcToTopData
{
    std::unique_ptr<ImplCalcToTopData> mpNext;
    VclPtr<vcl::Window>                mpWindow;
    std::unique_ptr<vcl::Region>       mpInvalidateRegion;
};

namespace vcl {

vcl::Window* Window::ImplGetTopmostFrameWindow() const
{
    const vcl::Window *pTopmostParent = this;
    while( pTopmostParent->ImplGetParent() )
        pTopmostParent = pTopmostParent->ImplGetParent();
    return pTopmostParent->mpWindowImpl->mpFrameWindow;
}

void Window::ImplInsertWindow( vcl::Window* pParent )
{
    mpWindowImpl->mpHierarchy->mpParent = pParent;
    mpWindowImpl->mpHierarchy->mpRealParent = pParent;

    if ( !pParent || mpWindowImpl->mbFrame )
        return;

    // search frame window and set window frame data
    vcl::Window* pFrameParent = pParent->mpWindowImpl->mpFrameWindow;
    mpWindowImpl->mpFrameData = pFrameParent->mpWindowImpl->mpFrameData;
    if (mpWindowImpl->mpFrame != pFrameParent->mpWindowImpl->mpFrame)
    {
        mpWindowImpl->mpFrame = pFrameParent->mpWindowImpl->mpFrame;
        if (mpWindowImpl->mpSysObj)
            mpWindowImpl->mpSysObj->Reparent(mpWindowImpl->mpFrame);
    }
    mpWindowImpl->mpFrameWindow   = pFrameParent;
    mpWindowImpl->mbFrame         = false;

    // search overlap window and insert window in list
    if ( ImplIsOverlapWindow() )
    {
        vcl::Window* pFirstOverlapParent = pParent;
        while ( !pFirstOverlapParent->ImplIsOverlapWindow() )
            pFirstOverlapParent = pFirstOverlapParent->ImplGetParent();
        mpWindowImpl->mpOverlapWindow = pFirstOverlapParent;

        // --- Update global frame overlap list pointers ---
        mpWindowImpl->mpHierarchy->mpNextOverlap = mpWindowImpl->mpFrameData->mpFirstOverlap;
        if ( mpWindowImpl->mpHierarchy->mpNextOverlap )
            mpWindowImpl->mpHierarchy->mpNextOverlap->mpWindowImpl->mpHierarchy->mpPrevOverlap = this; // Set backward link
        mpWindowImpl->mpFrameData->mpFirstOverlap = this;
        mpWindowImpl->mpHierarchy->mpPrevOverlap = nullptr; // New head node has no previous node

        // Overlap-Windows are by default the uppermost
        mpWindowImpl->mpHierarchy->mpNext = pFirstOverlapParent->mpWindowImpl->mpHierarchy->mpFirstOverlap;
        pFirstOverlapParent->mpWindowImpl->mpHierarchy->mpFirstOverlap = this;
        if ( !pFirstOverlapParent->mpWindowImpl->mpHierarchy->mpLastOverlap )
            pFirstOverlapParent->mpWindowImpl->mpHierarchy->mpLastOverlap = this;
        else
            mpWindowImpl->mpHierarchy->mpNext->mpWindowImpl->mpHierarchy->mpPrev = this;
    }
    else
    {
        if ( pParent->ImplIsOverlapWindow() )
            mpWindowImpl->mpOverlapWindow = pParent;
        else
            mpWindowImpl->mpOverlapWindow = pParent->mpWindowImpl->mpOverlapWindow;
        mpWindowImpl->mpHierarchy->mpPrev = pParent->mpWindowImpl->mpHierarchy->mpLastChild;
        pParent->mpWindowImpl->mpHierarchy->mpLastChild = this;
        if ( !pParent->mpWindowImpl->mpHierarchy->mpFirstChild )
            pParent->mpWindowImpl->mpHierarchy->mpFirstChild = this;
        else
            mpWindowImpl->mpHierarchy->mpPrev->mpWindowImpl->mpHierarchy->mpNext = this;
    }

    InvalidateClipState();
}

void Window::ImplRemoveWindow( bool bRemoveFrameData )
{
    // Frame windows do not participate in parent/sibling tree updates
    if ( mpWindowImpl->mbFrame )
    {
        if ( bRemoveFrameData )
            GetOutDev()->ReleaseGraphics();
        return;
    }

    // Unlink from the absolute FrameData global overlap collection
    if ( ImplIsOverlapWindow() )
    {
        // True O(1) extraction utilizing our new mpPrevOverlap pointer
        if ( mpWindowImpl->mpHierarchy->mpPrevOverlap )
            mpWindowImpl->mpHierarchy->mpPrevOverlap->mpWindowImpl->mpHierarchy->mpNextOverlap = mpWindowImpl->mpHierarchy->mpNextOverlap;
        else
            mpWindowImpl->mpFrameData->mpFirstOverlap = mpWindowImpl->mpHierarchy->mpNextOverlap;

        if ( mpWindowImpl->mpHierarchy->mpNextOverlap )
            mpWindowImpl->mpHierarchy->mpNextOverlap->mpWindowImpl->mpHierarchy->mpPrevOverlap = mpWindowImpl->mpHierarchy->mpPrevOverlap;

        mpWindowImpl->mpHierarchy->mpPrevOverlap = nullptr;
        mpWindowImpl->mpHierarchy->mpNextOverlap = nullptr;
    }

    // Unlink from the forward sibling chains (Adjusting heads)
    if ( mpWindowImpl->mpHierarchy->mpPrev )
        mpWindowImpl->mpHierarchy->mpPrev->mpWindowImpl->mpHierarchy->mpNext = mpWindowImpl->mpHierarchy->mpNext;
    else if ( ImplIsOverlapWindow() )
        mpWindowImpl->mpOverlapWindow->mpWindowImpl->mpHierarchy->mpFirstOverlap = mpWindowImpl->mpHierarchy->mpNext;
    else if ( mpWindowImpl->mpHierarchy->mpParent )
        mpWindowImpl->mpHierarchy->mpParent->mpWindowImpl->mpHierarchy->mpFirstChild = mpWindowImpl->mpHierarchy->mpNext;

    // Unlink from the backward sibling chains (Adjusting tails)
    if ( mpWindowImpl->mpHierarchy->mpNext )
        mpWindowImpl->mpHierarchy->mpNext->mpWindowImpl->mpHierarchy->mpPrev = mpWindowImpl->mpHierarchy->mpPrev;
    else if ( ImplIsOverlapWindow() )
        mpWindowImpl->mpOverlapWindow->mpWindowImpl->mpHierarchy->mpLastOverlap = mpWindowImpl->mpHierarchy->mpPrev;
    else if ( mpWindowImpl->mpHierarchy->mpParent )
        mpWindowImpl->mpHierarchy->mpParent->mpWindowImpl->mpHierarchy->mpLastChild = mpWindowImpl->mpHierarchy->mpPrev;

    // Isolate this specific window node instance completely
    mpWindowImpl->mpHierarchy->mpPrev = nullptr;
    mpWindowImpl->mpHierarchy->mpNext = nullptr;

    // Trigger invalidation because this window's bounds
    // are no longer an exclusion factor for its siblings.
    InvalidateClipState();

    if ( bRemoveFrameData )
        GetOutDev()->ReleaseGraphics();
}

void Window::reorderWithinParent(sal_uInt16 nNewPosition)
{
    sal_uInt16 nChildCount = 0;
    vcl::Window *pSource = mpWindowImpl->mpHierarchy->mpParent->mpWindowImpl->mpHierarchy->mpFirstChild;
    while (pSource)
    {
        if (nChildCount == nNewPosition)
            break;
        pSource = pSource->mpWindowImpl->mpHierarchy->mpNext;
        nChildCount++;
    }

    if (pSource == this) //already at the right place
        return;

    ImplRemoveWindow(false);

    if (pSource)
    {
        mpWindowImpl->mpHierarchy->mpNext = pSource;
        mpWindowImpl->mpHierarchy->mpPrev = pSource->mpWindowImpl->mpHierarchy->mpPrev;
        pSource->mpWindowImpl->mpHierarchy->mpPrev = this;
    }
    else
        mpWindowImpl->mpHierarchy->mpParent->mpWindowImpl->mpHierarchy->mpLastChild = this;

    if (mpWindowImpl->mpHierarchy->mpPrev)
        mpWindowImpl->mpHierarchy->mpPrev->mpWindowImpl->mpHierarchy->mpNext = this;
    else
        mpWindowImpl->mpHierarchy->mpParent->mpWindowImpl->mpHierarchy->mpFirstChild = this;

    // After reordering, the sibling chains are different.
    // Invalidate so the next ClipStateBuilder::Build() sees the new order.
    InvalidateClipState();
}

void Window::ImplToBottomChild()
{
    if ( ImplIsOverlapWindow() || mpWindowImpl->mbReallyVisible || (mpWindowImpl->mpHierarchy->mpParent->mpWindowImpl->mpHierarchy->mpLastChild.get() == this) )
        return;

    // put the window to the end of the list
    if ( mpWindowImpl->mpHierarchy->mpPrev )
        mpWindowImpl->mpHierarchy->mpPrev->mpWindowImpl->mpHierarchy->mpNext = mpWindowImpl->mpHierarchy->mpNext;
    else
    {
        // coverity[copy_paste_error : FALSE] - this is correct mpFirstChild, not mpNext
        mpWindowImpl->mpHierarchy->mpParent->mpWindowImpl->mpHierarchy->mpFirstChild = mpWindowImpl->mpHierarchy->mpNext;
    }
    mpWindowImpl->mpHierarchy->mpNext->mpWindowImpl->mpHierarchy->mpPrev = mpWindowImpl->mpHierarchy->mpPrev;
    mpWindowImpl->mpHierarchy->mpPrev = mpWindowImpl->mpHierarchy->mpParent->mpWindowImpl->mpHierarchy->mpLastChild;
    mpWindowImpl->mpHierarchy->mpParent->mpWindowImpl->mpHierarchy->mpLastChild = this;
    mpWindowImpl->mpHierarchy->mpPrev->mpWindowImpl->mpHierarchy->mpNext = this;
    mpWindowImpl->mpHierarchy->mpNext = nullptr;

    InvalidateClipState();
}

void Window::ImplCalcToTop( ImplCalcToTopData* pPrevData )
{
    SAL_WARN_IF( !ImplIsOverlapWindow(), "vcl", "Window::ImplCalcToTop(): Is not an OverlapWindow" );

    if ( mpWindowImpl->mbFrame )
        return;

    if ( !IsReallyVisible() )
        return;

    // calculate region, where the window overlaps with other windows
    vcl::Region  aRegion( GetOutputRectPixel() );
    vcl::Region  aInvalidateRegion;

    auto& rManager = GetOutDev()->GetClippingManager(*this);
    rManager.CalcOverlapRegion(*this, aRegion.GetBoundRect(), aInvalidateRegion, false, true);

    if ( !aInvalidateRegion.IsEmpty() )
    {
        ImplCalcToTopData* pData = new ImplCalcToTopData;
        pPrevData->mpNext.reset(pData);
        pData->mpWindow = this;
        pData->mpInvalidateRegion.reset(new vcl::Region(std::move(aInvalidateRegion)));
    }
}

void Window::ImplToTop( ToTopFlags nFlags )
{
    SAL_WARN_IF( !ImplIsOverlapWindow(), "vcl", "Window::ImplToTop(): Is not an OverlapWindow" );

    if ( mpWindowImpl->mbFrame )
    {
        // on a mouse click in the external window, it is the latter's
        // responsibility to assure our frame is put in front
        if ( !mpWindowImpl->mpFrameData->mbHasFocus &&
             !mpWindowImpl->mpFrameData->mbSysObjFocus &&
             !mpWindowImpl->mpFrameData->mbInSysObjFocusHdl &&
             !mpWindowImpl->mpFrameData->mbInSysObjToTopHdl )
        {
            // do not bring floating windows on the client to top
            if( !ImplGetClientWindow() || !(ImplGetClientWindow()->GetStyle() & WB_SYSTEMFLOATWIN) )
            {
                SalFrameToTop nSysFlags = SalFrameToTop::NONE;
                if ( nFlags & ToTopFlags::RestoreWhenMin )
                    nSysFlags |= SalFrameToTop::RestoreWhenMin;
                if ( nFlags & ToTopFlags::ForegroundTask )
                    nSysFlags |= SalFrameToTop::ForegroundTask;
                if ( nFlags & ToTopFlags::GrabFocusOnly )
                    nSysFlags |= SalFrameToTop::GrabFocusOnly;
                mpWindowImpl->mpFrame->ToTop( nSysFlags );
            }
        }
    }
    else
    {
        if ( mpWindowImpl->mpOverlapWindow->mpWindowImpl->mpHierarchy->mpFirstOverlap.get() != this )
        {
            // remove window from the list
            mpWindowImpl->mpHierarchy->mpPrev->mpWindowImpl->mpHierarchy->mpNext = mpWindowImpl->mpHierarchy->mpNext;
            if ( mpWindowImpl->mpHierarchy->mpNext )
                mpWindowImpl->mpHierarchy->mpNext->mpWindowImpl->mpHierarchy->mpPrev = mpWindowImpl->mpHierarchy->mpPrev;
            else
            {
                // coverity[copy_paste_error : FALSE] - this is correct mpLastOverlap, not mpPrev
                mpWindowImpl->mpOverlapWindow->mpWindowImpl->mpHierarchy->mpLastOverlap = mpWindowImpl->mpHierarchy->mpPrev;
            }

            // take AlwaysOnTop into account
            bool    bOnTop = IsAlwaysOnTopEnabled();
            vcl::Window* pNextWin = mpWindowImpl->mpOverlapWindow->mpWindowImpl->mpHierarchy->mpFirstOverlap;
            if ( !bOnTop )
            {
                while ( pNextWin )
                {
                    if ( !pNextWin->IsAlwaysOnTopEnabled() )
                        break;
                    pNextWin = pNextWin->mpWindowImpl->mpHierarchy->mpNext;
                }
            }

            // add the window to the list again
            mpWindowImpl->mpHierarchy->mpNext = pNextWin;
            if ( pNextWin )
            {
                mpWindowImpl->mpHierarchy->mpPrev = pNextWin->mpWindowImpl->mpHierarchy->mpPrev;
                pNextWin->mpWindowImpl->mpHierarchy->mpPrev = this;
            }
            else
            {
                mpWindowImpl->mpHierarchy->mpPrev = mpWindowImpl->mpOverlapWindow->mpWindowImpl->mpHierarchy->mpLastOverlap;
                mpWindowImpl->mpOverlapWindow->mpWindowImpl->mpHierarchy->mpLastOverlap = this;
            }
            if ( mpWindowImpl->mpHierarchy->mpPrev )
                mpWindowImpl->mpHierarchy->mpPrev->mpWindowImpl->mpHierarchy->mpNext = this;
            else
                mpWindowImpl->mpOverlapWindow->mpWindowImpl->mpHierarchy->mpFirstOverlap = this;

            if ( IsReallyVisible() )
            {
                // Moving an overlap window to the top changes the obscuration math
                // for all other overlap windows in this specific tier.
                vcl::Window* pOverlap = mpWindowImpl->mpOverlapWindow->mpWindowImpl->mpHierarchy->mpFirstOverlap;
                while (pOverlap)
                {
                    // Increment the version counter. The ClippingManager will lazily
                    // recompile their exact bounds only if they are actually painted.
                    pOverlap->InvalidateClipState();
                    pOverlap = pOverlap->ImplGetWindowImpl()->mpHierarchy->mpNext;
                }

                // Trigger a physical repaint of the parent frame so the
                // OS rendering engine redraws the windows in their new Z-order.
                mpWindowImpl->mpOverlapWindow->ImplInvalidate(nullptr, InvalidateFlags::Children | InvalidateFlags::NoTransparent);
            }
        }
    }
}

void Window::ImplStartToTop( ToTopFlags nFlags )
{
    ImplCalcToTopData   aStartData;
    ImplCalcToTopData*  pCurData;
    vcl::Window* pOverlapWindow;
    if ( ImplIsOverlapWindow() )
        pOverlapWindow = this;
    else
        pOverlapWindow = mpWindowImpl->mpOverlapWindow;

    // first calculate paint areas
    vcl::Window* pTempOverlapWindow = pOverlapWindow;
    aStartData.mpNext = nullptr;
    pCurData = &aStartData;
    do
    {
        pTempOverlapWindow->ImplCalcToTop( pCurData );
        if ( pCurData->mpNext )
            pCurData = pCurData->mpNext.get();
        pTempOverlapWindow = pTempOverlapWindow->mpWindowImpl->mpOverlapWindow;
    }
    while ( !pTempOverlapWindow->mpWindowImpl->mbFrame );
    // next calculate the paint areas of the ChildOverlap windows
    pTempOverlapWindow = mpWindowImpl->mpHierarchy->mpFirstOverlap;
    while ( pTempOverlapWindow )
    {
        pTempOverlapWindow->ImplCalcToTop( pCurData );
        if ( pCurData->mpNext )
            pCurData = pCurData->mpNext.get();
        pTempOverlapWindow = pTempOverlapWindow->mpWindowImpl->mpHierarchy->mpNext;
    }

    // and next change the windows list
    pTempOverlapWindow = pOverlapWindow;
    do
    {
        pTempOverlapWindow->ImplToTop( nFlags );
        pTempOverlapWindow = pTempOverlapWindow->mpWindowImpl->mpOverlapWindow;
    }
    while ( !pTempOverlapWindow->mpWindowImpl->mbFrame );
    // as last step invalidate the invalid areas
    pCurData = aStartData.mpNext.get();
    while ( pCurData )
    {
        pCurData->mpWindow->ImplInvalidateFrameRegion( pCurData->mpInvalidateRegion.get(), InvalidateFlags::Children );
        pCurData = pCurData->mpNext.get();
    }
}

void Window::ImplFocusToTop( ToTopFlags nFlags, bool bReallyVisible )
{
    // do we need to fetch the focus?
    if ( !(nFlags & ToTopFlags::NoGrabFocus) )
    {
        // first window with GrabFocus-Activate gets the focus
        vcl::Window* pFocusWindow = this;
        while ( !pFocusWindow->ImplIsOverlapWindow() )
        {
            // if the window has no BorderWindow, we
            // should always find the belonging BorderWindow
            if ( !pFocusWindow->mpWindowImpl->mpBorderWindow )
            {
                if ( pFocusWindow->mpWindowImpl->mnActivateMode & ActivateModeFlags::GrabFocus )
                    break;
            }
            pFocusWindow = pFocusWindow->ImplGetParent();
        }
        if ( (pFocusWindow->mpWindowImpl->mnActivateMode & ActivateModeFlags::GrabFocus) &&
             !pFocusWindow->HasChildPathFocus( true ) )
            pFocusWindow->GrabFocus();
    }

    if ( bReallyVisible )
        ImplGenerateMouseMove();
}

void Window::ImplShowAllOverlaps()
{
    vcl::Window* pOverlapWindow = mpWindowImpl->mpHierarchy->mpFirstOverlap;
    while ( pOverlapWindow )
    {
        if ( pOverlapWindow->mpWindowImpl->mbOverlapVisible )
        {
            pOverlapWindow->Show( true, ShowFlags::NoActivate );
            pOverlapWindow->mpWindowImpl->mbOverlapVisible = false;
        }

        pOverlapWindow = pOverlapWindow->mpWindowImpl->mpHierarchy->mpNext;
    }
}

void Window::ImplHideAllOverlaps()
{
    vcl::Window* pOverlapWindow = mpWindowImpl->mpHierarchy->mpFirstOverlap;
    while ( pOverlapWindow )
    {
        if ( pOverlapWindow->IsVisible() )
        {
            pOverlapWindow->mpWindowImpl->mbOverlapVisible = true;
            pOverlapWindow->Show( false );
        }

        pOverlapWindow = pOverlapWindow->mpWindowImpl->mpHierarchy->mpNext;
    }
}

void Window::ToTop( ToTopFlags nFlags )
{
    if (!mpWindowImpl)
        return;

    ImplStartToTop( nFlags );
    ImplFocusToTop( nFlags, IsReallyVisible() );
}

void Window::SetZOrder( vcl::Window* pRefWindow, ZOrderFlags nFlags )
{
    if ( mpWindowImpl->mpBorderWindow )
    {
        mpWindowImpl->mpBorderWindow->SetZOrder( pRefWindow, nFlags );
        return;
    }

    if ( nFlags & ZOrderFlags::First )
    {
        if ( ImplIsOverlapWindow() )
            pRefWindow = mpWindowImpl->mpOverlapWindow->mpWindowImpl->mpHierarchy->mpFirstOverlap;
        else
            pRefWindow = mpWindowImpl->mpHierarchy->mpParent->mpWindowImpl->mpHierarchy->mpFirstChild;
        nFlags |= ZOrderFlags::Before;
    }
    else if ( nFlags & ZOrderFlags::Last )
    {
        if ( ImplIsOverlapWindow() )
            pRefWindow = mpWindowImpl->mpOverlapWindow->mpWindowImpl->mpHierarchy->mpLastOverlap;
        else
            pRefWindow = mpWindowImpl->mpHierarchy->mpParent->mpWindowImpl->mpHierarchy->mpLastChild;
        nFlags |= ZOrderFlags::Behind;
    }

    while ( pRefWindow && pRefWindow->mpWindowImpl->mpBorderWindow )
        pRefWindow = pRefWindow->mpWindowImpl->mpBorderWindow;
    if (!pRefWindow || pRefWindow == this || mpWindowImpl->mbFrame)
        return;

    SAL_WARN_IF( pRefWindow->mpWindowImpl->mpHierarchy->mpParent != mpWindowImpl->mpHierarchy->mpParent, "vcl", "Window::SetZOrder() - pRefWindow has other parent" );
    if ( nFlags & ZOrderFlags::Before )
    {
        if ( pRefWindow->mpWindowImpl->mpHierarchy->mpPrev.get() == this )
            return;

        if ( ImplIsOverlapWindow() )
        {
            if ( mpWindowImpl->mpHierarchy->mpPrev )
                mpWindowImpl->mpHierarchy->mpPrev->mpWindowImpl->mpHierarchy->mpNext = mpWindowImpl->mpHierarchy->mpNext;
            else
                mpWindowImpl->mpOverlapWindow->mpWindowImpl->mpHierarchy->mpFirstOverlap = mpWindowImpl->mpHierarchy->mpNext;
            if ( mpWindowImpl->mpHierarchy->mpNext )
                mpWindowImpl->mpHierarchy->mpNext->mpWindowImpl->mpHierarchy->mpPrev = mpWindowImpl->mpHierarchy->mpPrev;
            else
                mpWindowImpl->mpOverlapWindow->mpWindowImpl->mpHierarchy->mpLastOverlap = mpWindowImpl->mpHierarchy->mpPrev;
            if ( !pRefWindow->mpWindowImpl->mpHierarchy->mpPrev )
                mpWindowImpl->mpOverlapWindow->mpWindowImpl->mpHierarchy->mpFirstOverlap = this;
        }
        else
        {
            if ( mpWindowImpl->mpHierarchy->mpPrev )
                mpWindowImpl->mpHierarchy->mpPrev->mpWindowImpl->mpHierarchy->mpNext = mpWindowImpl->mpHierarchy->mpNext;
            else
                mpWindowImpl->mpHierarchy->mpParent->mpWindowImpl->mpHierarchy->mpFirstChild = mpWindowImpl->mpHierarchy->mpNext;
            if ( mpWindowImpl->mpHierarchy->mpNext )
                mpWindowImpl->mpHierarchy->mpNext->mpWindowImpl->mpHierarchy->mpPrev = mpWindowImpl->mpHierarchy->mpPrev;
            else
                mpWindowImpl->mpHierarchy->mpParent->mpWindowImpl->mpHierarchy->mpLastChild = mpWindowImpl->mpHierarchy->mpPrev;
            if ( !pRefWindow->mpWindowImpl->mpHierarchy->mpPrev )
                mpWindowImpl->mpHierarchy->mpParent->mpWindowImpl->mpHierarchy->mpFirstChild = this;
        }

        mpWindowImpl->mpHierarchy->mpPrev = pRefWindow->mpWindowImpl->mpHierarchy->mpPrev;
        mpWindowImpl->mpHierarchy->mpNext = pRefWindow;
        if ( mpWindowImpl->mpHierarchy->mpPrev )
            mpWindowImpl->mpHierarchy->mpPrev->mpWindowImpl->mpHierarchy->mpNext = this;
        mpWindowImpl->mpHierarchy->mpNext->mpWindowImpl->mpHierarchy->mpPrev = this;
    }
    else if ( nFlags & ZOrderFlags::Behind )
    {
        if ( pRefWindow->mpWindowImpl->mpHierarchy->mpNext.get() == this )
            return;

        if ( ImplIsOverlapWindow() )
        {
            if ( mpWindowImpl->mpHierarchy->mpPrev )
                mpWindowImpl->mpHierarchy->mpPrev->mpWindowImpl->mpHierarchy->mpNext = mpWindowImpl->mpHierarchy->mpNext;
            else
                mpWindowImpl->mpOverlapWindow->mpWindowImpl->mpHierarchy->mpFirstOverlap = mpWindowImpl->mpHierarchy->mpNext;
            if ( mpWindowImpl->mpHierarchy->mpNext )
                mpWindowImpl->mpHierarchy->mpNext->mpWindowImpl->mpHierarchy->mpPrev = mpWindowImpl->mpHierarchy->mpPrev;
            else
                mpWindowImpl->mpOverlapWindow->mpWindowImpl->mpHierarchy->mpLastOverlap = mpWindowImpl->mpHierarchy->mpPrev;
            if ( !pRefWindow->mpWindowImpl->mpHierarchy->mpNext )
                mpWindowImpl->mpOverlapWindow->mpWindowImpl->mpHierarchy->mpLastOverlap = this;
        }
        else
        {
            if ( mpWindowImpl->mpHierarchy->mpPrev )
                mpWindowImpl->mpHierarchy->mpPrev->mpWindowImpl->mpHierarchy->mpNext = mpWindowImpl->mpHierarchy->mpNext;
            else
                mpWindowImpl->mpHierarchy->mpParent->mpWindowImpl->mpHierarchy->mpFirstChild = mpWindowImpl->mpHierarchy->mpNext;
            if ( mpWindowImpl->mpHierarchy->mpNext )
                mpWindowImpl->mpHierarchy->mpNext->mpWindowImpl->mpHierarchy->mpPrev = mpWindowImpl->mpHierarchy->mpPrev;
            else
                mpWindowImpl->mpHierarchy->mpParent->mpWindowImpl->mpHierarchy->mpLastChild = mpWindowImpl->mpHierarchy->mpPrev;
            if ( !pRefWindow->mpWindowImpl->mpHierarchy->mpNext )
                mpWindowImpl->mpHierarchy->mpParent->mpWindowImpl->mpHierarchy->mpLastChild = this;
        }

        mpWindowImpl->mpHierarchy->mpPrev = pRefWindow;
        mpWindowImpl->mpHierarchy->mpNext = pRefWindow->mpWindowImpl->mpHierarchy->mpNext;
        if ( mpWindowImpl->mpHierarchy->mpNext )
            mpWindowImpl->mpHierarchy->mpNext->mpWindowImpl->mpHierarchy->mpPrev = this;
        mpWindowImpl->mpHierarchy->mpPrev->mpWindowImpl->mpHierarchy->mpNext = this;
    }

    InvalidateClipState();

    if ( !IsReallyVisible() )
        return;

    Invalidate(InvalidateFlags::Children | InvalidateFlags::NoTransparent);
}

void Window::EnableAlwaysOnTop( bool bEnable )
{

    mpWindowImpl->mbAlwaysOnTop = bEnable;

    if ( mpWindowImpl->mpBorderWindow )
        mpWindowImpl->mpBorderWindow->EnableAlwaysOnTop( bEnable );
    else if ( bEnable && IsReallyVisible() )
        ToTop();

    if ( mpWindowImpl->mbFrame )
        mpWindowImpl->mpFrame->SetAlwaysOnTop( bEnable );
}

bool Window::IsTopWindow() const
{
    if ( !mpWindowImpl || mpWindowImpl->mbInDispose )
        return false;

    // topwindows must be frames or they must have a borderwindow which is a frame
    if( !mpWindowImpl->mbFrame && (!mpWindowImpl->mpBorderWindow || !mpWindowImpl->mpBorderWindow->mpWindowImpl->mbFrame ) )
        return false;

    ImplGetWinData();
    if( mpWindowImpl->mpWinData->mnIsTopWindow == sal_uInt16(~0))    // still uninitialized
    {
        // #113722#, cache result of expensive queryInterface call
        vcl::Window *pThisWin = const_cast<vcl::Window*>(this);
        uno::Reference< XTopWindow > xTopWindow( pThisWin->GetComponentInterface(), UNO_QUERY );
        pThisWin->mpWindowImpl->mpWinData->mnIsTopWindow = xTopWindow.is() ? 1 : 0;
    }
    return mpWindowImpl->mpWinData->mnIsTopWindow == 1;
}

vcl::Window* Window::ImplFindWindow( const Point& rFramePos )
{
    vcl::Window* pTempWindow;
    vcl::Window* pFindWindow;

    // first check all overlapping windows
    pTempWindow = mpWindowImpl->mpHierarchy->mpFirstOverlap;
    while ( pTempWindow )
    {
        pFindWindow = pTempWindow->ImplFindWindow( rFramePos );
        if ( pFindWindow )
            return pFindWindow;
        pTempWindow = pTempWindow->mpWindowImpl->mpHierarchy->mpNext;
    }

    // then we check our window
    if ( !mpWindowImpl->mbVisible )
        return nullptr;

    WindowHitTest nHitTest = ImplHitTest( rFramePos );
    if ( nHitTest & WindowHitTest::Inside )
    {
        // and then we check all child windows
        pTempWindow = mpWindowImpl->mpHierarchy->mpFirstChild;
        while ( pTempWindow )
        {
            pFindWindow = pTempWindow->ImplFindWindow( rFramePos );
            if ( pFindWindow )
                return pFindWindow;
            pTempWindow = pTempWindow->mpWindowImpl->mpHierarchy->mpNext;
        }

        if ( nHitTest & WindowHitTest::Transparent )
            return nullptr;
        else
            return this;
    }

    return nullptr;
}

bool Window::ImplIsRealParentPath( const vcl::Window* pWindow ) const
{
    pWindow = pWindow->GetParent();
    while ( pWindow )
    {
        if ( pWindow == this )
            return true;
        pWindow = pWindow->GetParent();
    }

    return false;
}

bool Window::ImplIsChild( const vcl::Window* pWindow, bool bSystemWindow ) const
{
    do
    {
        if ( !bSystemWindow && pWindow->ImplIsOverlapWindow() )
            break;

        pWindow = pWindow->ImplGetParent();

        if ( pWindow == this )
            return true;
    }
    while ( pWindow );

    return false;
}

bool Window::ImplIsWindowOrChild( const vcl::Window* pWindow, bool bSystemWindow ) const
{
    if ( this == pWindow )
        return true;
    return ImplIsChild( pWindow, bSystemWindow );
}

void Window::ImplResetReallyVisible()
{
    bool bBecameReallyInvisible = mpWindowImpl->mbReallyVisible;

    GetOutDev()->mbDevOutput     = false;
    mpWindowImpl->mbReallyVisible = false;
    mpWindowImpl->mbReallyShown   = false;

    // the SHOW/HIDE events serve as indicators to send child creation/destroy events to the access bridge.
    // For this, the data member of the event must not be NULL.
    // Previously, we did this in Window::Show, but there some events got lost in certain situations.
    if( bBecameReallyInvisible && ImplIsAccessibleCandidate() )
        CallEventListeners( VclEventId::WindowHide, this );
        // TODO. It's kind of a hack that we're re-using the VclEventId::WindowHide. Normally, we should
        // introduce another event which explicitly triggers the Accessibility implementations.

    vcl::Window* pWindow = mpWindowImpl->mpHierarchy->mpFirstOverlap;
    while ( pWindow )
    {
        if ( pWindow->mpWindowImpl->mbReallyVisible )
            pWindow->ImplResetReallyVisible();
        pWindow = pWindow->mpWindowImpl->mpHierarchy->mpNext;
    }

    pWindow = mpWindowImpl->mpHierarchy->mpFirstChild;
    while ( pWindow )
    {
        if ( pWindow->mpWindowImpl->mbReallyVisible )
            pWindow->ImplResetReallyVisible();
        pWindow = pWindow->mpWindowImpl->mpHierarchy->mpNext;
    }
}

void Window::ImplUpdateWindowPtr( vcl::Window* pWindow )
{
    if ( mpWindowImpl->mpFrameWindow != pWindow->mpWindowImpl->mpFrameWindow )
    {
        // release graphic
        OutputDevice *pOutDev = GetOutDev();
        pOutDev->ReleaseGraphics();
    }

    mpWindowImpl->mpFrameData     = pWindow->mpWindowImpl->mpFrameData;
    if (mpWindowImpl->mpFrame != pWindow->mpWindowImpl->mpFrame)
    {
        mpWindowImpl->mpFrame = pWindow->mpWindowImpl->mpFrame;
        if (mpWindowImpl->mpSysObj)
            mpWindowImpl->mpSysObj->Reparent(mpWindowImpl->mpFrame);
    }
    mpWindowImpl->mpFrameWindow   = pWindow->mpWindowImpl->mpFrameWindow;
    if ( pWindow->ImplIsOverlapWindow() )
        mpWindowImpl->mpOverlapWindow = pWindow;
    else
        mpWindowImpl->mpOverlapWindow = pWindow->mpWindowImpl->mpOverlapWindow;

    vcl::Window* pChild = mpWindowImpl->mpHierarchy->mpFirstChild;
    while ( pChild )
    {
        pChild->ImplUpdateWindowPtr( pWindow );
        pChild = pChild->mpWindowImpl->mpHierarchy->mpNext;
    }
}

void Window::ImplUpdateWindowPtr()
{
    vcl::Window* pChild = mpWindowImpl->mpHierarchy->mpFirstChild;
    while ( pChild )
    {
        pChild->ImplUpdateWindowPtr( this );
        pChild = pChild->mpWindowImpl->mpHierarchy->mpNext;
    }
}

void Window::ImplUpdateOverlapWindowPtr( bool bNewFrame )
{
    bool bVisible = IsVisible();
    Show( false );
    ImplRemoveWindow( bNewFrame );
    vcl::Window* pRealParent = mpWindowImpl->mpHierarchy->mpRealParent;
    ImplInsertWindow( ImplGetParent() );
    mpWindowImpl->mpHierarchy->mpRealParent = pRealParent;
    ImplUpdateWindowPtr();
    if ( ImplUpdatePos() )
        ImplUpdateNativeObjectPos();

    if ( bNewFrame )
    {
        vcl::Window* pOverlapWindow = mpWindowImpl->mpHierarchy->mpFirstOverlap;
        while ( pOverlapWindow )
        {
            vcl::Window* pNextOverlapWindow = pOverlapWindow->mpWindowImpl->mpHierarchy->mpNext;
            pOverlapWindow->ImplUpdateOverlapWindowPtr( bNewFrame );
            pOverlapWindow = pNextOverlapWindow;
        }
    }

    if ( bVisible )
        Show();
}

SystemWindow* Window::GetSystemWindow() const
{

    const vcl::Window* pWin = this;
    while ( pWin && !pWin->IsSystemWindow() )
        pWin  = pWin->GetParent();
    return static_cast<SystemWindow*>(const_cast<Window*>(pWin));
}

static SystemWindow *ImplGetLastSystemWindow( vcl::Window *pWin )
{
    // get the most top-level system window, the one that contains the taskpanelist
    SystemWindow *pSysWin = nullptr;
    if( !pWin )
        return pSysWin;
    vcl::Window *pMyParent = pWin;
    while ( pMyParent )
    {
        if ( pMyParent->IsSystemWindow() )
            pSysWin = static_cast<SystemWindow*>(pMyParent);
        pMyParent = pMyParent->GetParent();
    }
    return pSysWin;
}

void Window::SetParent( vcl::Window* pNewParent )
{
    SAL_WARN_IF( !pNewParent, "vcl", "Window::SetParent(): pParent == NULL" );
    SAL_WARN_IF( pNewParent == this, "vcl", "someone tried to reparent a window to itself" );

    if( !pNewParent || pNewParent == this )
        return;

    if (!mpWindowImpl)
    {
        SAL_WARN("vcl", "Window::SetParent(): mpWindowImpl == NULL");
        return;
    }

    // check if the taskpanelist would change and move the window pointer accordingly
    SystemWindow *pSysWin = ImplGetLastSystemWindow(this);
    SystemWindow *pNewSysWin = nullptr;
    bool bChangeTaskPaneList = false;
    if( pSysWin && pSysWin->ImplIsInTaskPaneList( this ) )
    {
        pNewSysWin = ImplGetLastSystemWindow( pNewParent );
        if( pNewSysWin && pNewSysWin != pSysWin )
        {
            bChangeTaskPaneList = true;
            pSysWin->GetTaskPaneList()->RemoveWindow( this );
        }
    }
    // remove ownerdraw decorated windows from list in the top-most frame window
    if( (GetStyle() & WB_OWNERDRAWDECORATION) && mpWindowImpl->mbFrame )
    {
        ::std::vector< VclPtr<vcl::Window> >& rList = ImplGetOwnerDrawList();
        auto p = ::std::find( rList.begin(), rList.end(), VclPtr<vcl::Window>(this) );
        if( p != rList.end() )
            rList.erase( p );
    }

    ImplSetFrameParent( pNewParent );

    if ( mpWindowImpl->mpBorderWindow )
    {
        mpWindowImpl->mpHierarchy->mpRealParent = pNewParent;
        mpWindowImpl->mpBorderWindow->SetParent( pNewParent );
        return;
    }

    if ( mpWindowImpl->mpHierarchy->mpParent.get() == pNewParent )
        return;

    if ( mpWindowImpl->mbFrame )
        mpWindowImpl->mpFrame->SetParent( pNewParent->mpWindowImpl->mpFrame );

    bool bVisible = IsVisible();
    Show( false, ShowFlags::NoFocusChange );

    // check if the overlap window changes
    vcl::Window* pOldOverlapWindow;
    vcl::Window* pNewOverlapWindow = nullptr;
    if ( ImplIsOverlapWindow() )
        pOldOverlapWindow = nullptr;
    else
    {
        pNewOverlapWindow = pNewParent->ImplGetFirstOverlapWindow();
        if ( mpWindowImpl->mpOverlapWindow.get() != pNewOverlapWindow )
            pOldOverlapWindow = mpWindowImpl->mpOverlapWindow;
        else
            pOldOverlapWindow = nullptr;
    }

    // convert windows in the hierarchy
    bool bFocusOverlapWin = HasChildPathFocus( true );
    bool bFocusWin = HasChildPathFocus();
    bool bNewFrame = pNewParent->mpWindowImpl->mpFrameWindow != mpWindowImpl->mpFrameWindow;
    if ( bNewFrame )
    {
        if ( mpWindowImpl->mpFrameData->mpFocusWin )
        {
            if ( IsWindowOrChild( mpWindowImpl->mpFrameData->mpFocusWin ) )
                mpWindowImpl->mpFrameData->mpFocusWin = nullptr;
        }
        if ( mpWindowImpl->mpFrameData->mpMouseMoveWin )
        {
            if ( IsWindowOrChild( mpWindowImpl->mpFrameData->mpMouseMoveWin ) )
                mpWindowImpl->mpFrameData->mpMouseMoveWin = nullptr;
        }
        if ( mpWindowImpl->mpFrameData->mpMouseDownWin )
        {
            if ( IsWindowOrChild( mpWindowImpl->mpFrameData->mpMouseDownWin ) )
                mpWindowImpl->mpFrameData->mpMouseDownWin = nullptr;
        }
    }
    ImplRemoveWindow( bNewFrame );
    ImplInsertWindow( pNewParent );

    if ( mpWindowImpl->mpClippingState->meParentClipMode & ParentClipMode::Clip )
        pNewParent->mpWindowImpl->mpClippingState->mbClipChildren = true;

    ImplUpdateWindowPtr();
    if ( ImplUpdatePos() )
        ImplUpdateNativeObjectPos();

    // If the Overlap-Window has changed, we need to test whether
    // OverlapWindows that had the Child window as their parent
    // need to be put into the window hierarchy.
    if ( ImplIsOverlapWindow() )
    {
        if ( bNewFrame )
        {
            vcl::Window* pOverlapWindow = mpWindowImpl->mpHierarchy->mpFirstOverlap;
            while ( pOverlapWindow )
            {
                vcl::Window* pNextOverlapWindow = pOverlapWindow->mpWindowImpl->mpHierarchy->mpNext;
                pOverlapWindow->ImplUpdateOverlapWindowPtr( bNewFrame );
                pOverlapWindow = pNextOverlapWindow;
            }
        }
    }
    else if ( pOldOverlapWindow )
    {
        // reset Focus-Save
        if ( bFocusWin ||
             (pOldOverlapWindow->mpWindowImpl->mpLastFocusWindow &&
              IsWindowOrChild( pOldOverlapWindow->mpWindowImpl->mpLastFocusWindow )) )
            pOldOverlapWindow->mpWindowImpl->mpLastFocusWindow = nullptr;

        vcl::Window* pOverlapWindow = pOldOverlapWindow->mpWindowImpl->mpHierarchy->mpFirstOverlap;
        while ( pOverlapWindow )
        {
            vcl::Window* pNextOverlapWindow = pOverlapWindow->mpWindowImpl->mpHierarchy->mpNext;
            if ( ImplIsRealParentPath( pOverlapWindow->ImplGetWindow() ) )
                pOverlapWindow->ImplUpdateOverlapWindowPtr( bNewFrame );
            pOverlapWindow = pNextOverlapWindow;
        }

        // update activate-status at next overlap window
        if ( HasChildPathFocus( true ) )
            ImplCallFocusChangeActivate( pNewOverlapWindow, pOldOverlapWindow );
    }

    // also convert Activate-Status
    if ( bNewFrame )
    {
        if ( (GetType() == WindowType::BORDERWINDOW) &&
             (ImplGetWindow()->GetType() == WindowType::FLOATINGWINDOW) )
            static_cast<ImplBorderWindow*>(this)->SetDisplayActive( mpWindowImpl->mpFrameData->mbHasFocus );
    }

    // when required give focus to new frame if
    // FocusWindow is changed with SetParent()
    if ( bFocusOverlapWin )
    {
        mpWindowImpl->mpFrameData->mpFocusWin = Application::GetFocusWindow();
        if ( !mpWindowImpl->mpFrameData->mbHasFocus )
        {
            mpWindowImpl->mpFrame->ToTop( SalFrameToTop::NONE );
        }
    }

    // Assure DragSource and DropTarget members are created
    if ( bNewFrame )
    {
            GetDropTarget();
    }

    if( bChangeTaskPaneList )
        pNewSysWin->GetTaskPaneList()->AddWindow( this );

    if( (GetStyle() & WB_OWNERDRAWDECORATION) && mpWindowImpl->mbFrame )
        ImplGetOwnerDrawList().emplace_back(this );

    if ( bVisible )
        Show( true, ShowFlags::NoFocusChange | ShowFlags::NoActivate );

    InvalidateClipState();
}

bool Window::IsAncestorOf( const vcl::Window& rWindow ) const
{
    return ImplIsRealParentPath(&rWindow);
}

sal_uInt16 Window::GetChildCount() const
{
    if (!mpWindowImpl)
        return 0;

    sal_uInt16  nChildCount = 0;
    vcl::Window* pChild = mpWindowImpl->mpHierarchy->mpFirstChild;
    while ( pChild )
    {
        nChildCount++;
        pChild = pChild->mpWindowImpl->mpHierarchy->mpNext;
    }

    return nChildCount;
}

vcl::Window* Window::GetChild( sal_uInt16 nChild ) const
{
    if (!mpWindowImpl)
        return nullptr;

    sal_uInt16  nChildCount = 0;
    vcl::Window* pChild = mpWindowImpl->mpHierarchy->mpFirstChild;
    while ( pChild )
    {
        if ( nChild == nChildCount )
            return pChild;
        pChild = pChild->mpWindowImpl->mpHierarchy->mpNext;
        nChildCount++;
    }

    return nullptr;
}

vcl::Window* Window::GetWindow( GetWindowType nType ) const
{
    if (!mpWindowImpl)
        return nullptr;

    switch ( nType )
    {
        case GetWindowType::Parent:
            return mpWindowImpl->mpHierarchy->mpRealParent;

        case GetWindowType::FirstChild:
            return mpWindowImpl->mpHierarchy->mpFirstChild;

        case GetWindowType::LastChild:
            return mpWindowImpl->mpHierarchy->mpLastChild;

        case GetWindowType::Prev:
            return mpWindowImpl->mpHierarchy->mpPrev;

        case GetWindowType::Next:
            return mpWindowImpl->mpHierarchy->mpNext;

        case GetWindowType::FirstOverlap:
            return mpWindowImpl->mpHierarchy->mpFirstOverlap;

        case GetWindowType::Overlap:
            if ( ImplIsOverlapWindow() )
                return const_cast<vcl::Window*>(this);
            else
                return mpWindowImpl->mpOverlapWindow;

        case GetWindowType::ParentOverlap:
            if ( ImplIsOverlapWindow() )
                return mpWindowImpl->mpOverlapWindow;
            else
                return mpWindowImpl->mpOverlapWindow->mpWindowImpl->mpOverlapWindow;

        case GetWindowType::Client:
            return this->ImplGetWindow();

        case GetWindowType::RealParent:
            return ImplGetParent();

        case GetWindowType::Frame:
            return mpWindowImpl->mpFrameWindow;

        case GetWindowType::Border:
            if ( mpWindowImpl->mpBorderWindow )
                return mpWindowImpl->mpBorderWindow->GetWindow( GetWindowType::Border );
            return const_cast<vcl::Window*>(this);

        case GetWindowType::FirstTopWindowChild:
            return ImplGetWinData()->maTopWindowChildren.empty() ? nullptr : (*ImplGetWinData()->maTopWindowChildren.begin()).get();

        case GetWindowType::NextTopWindowSibling:
        {
            if ( !mpWindowImpl->mpHierarchy->mpRealParent )
                return nullptr;
            const ::std::list< VclPtr<vcl::Window> >& rTopWindows( mpWindowImpl->mpHierarchy->mpRealParent->ImplGetWinData()->maTopWindowChildren );
            ::std::list< VclPtr<vcl::Window> >::const_iterator myPos =
                ::std::find( rTopWindows.begin(), rTopWindows.end(), this );
            if ( ( myPos == rTopWindows.end() ) || ( ++myPos == rTopWindows.end() ) )
                return nullptr;
            return *myPos;
        }

    }

    return nullptr;
}

bool Window::IsChild( const vcl::Window* pWindow ) const
{
    do
    {
        if ( pWindow->ImplIsOverlapWindow() )
            break;

        pWindow = pWindow->ImplGetParent();

        if ( pWindow == this )
            return true;
    }
    while ( pWindow );

    return false;
}

bool Window::IsWindowOrChild( const vcl::Window* pWindow, bool bSystemWindow ) const
{

    if ( this == pWindow )
        return true;
    return ImplIsChild( pWindow, bSystemWindow );
}

void Window::ImplSetFrameParent( const vcl::Window* pParent )
{
    vcl::Window* pFrameWindow = ImplGetSVData()->maFrameData.mpFirstFrame;
    while( pFrameWindow )
    {
        // search all frames that are children of this window
        // and reparent them
        if( ImplIsRealParentPath( pFrameWindow ) )
        {
            SAL_WARN_IF( mpWindowImpl->mpFrame == pFrameWindow->mpWindowImpl->mpFrame, "vcl", "SetFrameParent to own" );
            SAL_WARN_IF( !mpWindowImpl->mpFrame, "vcl", "no frame" );
            SalFrame* pParentFrame = pParent ? pParent->mpWindowImpl->mpFrame : nullptr;
            pFrameWindow->mpWindowImpl->mpFrame->SetParent( pParentFrame );
        }
        pFrameWindow = pFrameWindow->mpWindowImpl->mpFrameData->mpNextFrame;
    }
}

} /* namespace vcl */

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
