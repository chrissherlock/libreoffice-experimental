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

#include <window/ImplFrameData.hxx>
#include <window/ImplWinData.hxx>
#include <window/WindowVisibilityState.hxx>
#include <window/WindowPlatformState.hxx>
#include <window/WindowClassification.hxx>
#include <window/WindowInput.hxx>
#include <window/WindowClippingState.hxx>
#include <window/WindowHierarchy.hxx>
#include <window/WindowFocusState.hxx>
#include <window/clipping_window.hxx>
#include <salframe.hxx>
#include <salobj.hxx>
#include <svdata.hxx>
#include <window/subclasses/decorations/brdwin.hxx>

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
    return pTopmostParent->mpHierarchy->mpFrameWindow;
}

void Window::ImplInsertWindow( vcl::Window* pParent )
{
    mpHierarchy->mpParent = pParent;
    mpHierarchy->mpRealParent = pParent;

    if ( !pParent || mpClassification->mbFrame )
        return;

    // search frame window and set window frame data
    vcl::Window* pFrameParent = pParent->mpHierarchy->mpFrameWindow;
    mpPlatformState->mpFrameData = pFrameParent->mpPlatformState->mpFrameData;
    if (mpPlatformState->mpFrame != pFrameParent->mpPlatformState->mpFrame)
    {
        mpPlatformState->mpFrame = pFrameParent->mpPlatformState->mpFrame;
        if (mpPlatformState->mpSysObj)
            mpPlatformState->mpSysObj->Reparent(mpPlatformState->mpFrame);
    }
    mpHierarchy->mpFrameWindow   = pFrameParent;
    mpClassification->mbFrame         = false;

    // search overlap window and insert window in list
    if ( ImplIsOverlapWindow() )
    {
        vcl::Window* pFirstOverlapParent = pParent;
        while ( !pFirstOverlapParent->ImplIsOverlapWindow() )
            pFirstOverlapParent = pFirstOverlapParent->ImplGetParent();
        mpHierarchy->mpOverlapWindow = pFirstOverlapParent;

        // --- Update global frame overlap list pointers ---
        mpHierarchy->mpNextOverlap = mpPlatformState->mpFrameData->mpFirstOverlap;
        if ( mpHierarchy->mpNextOverlap )
            mpHierarchy->mpNextOverlap->mpHierarchy->mpPrevOverlap = this; // Set backward link
        mpPlatformState->mpFrameData->mpFirstOverlap = this;
        mpHierarchy->mpPrevOverlap = nullptr; // New head node has no previous node

        // Overlap-Windows are by default the uppermost
        mpHierarchy->mpNext = pFirstOverlapParent->mpHierarchy->mpFirstOverlap;
        pFirstOverlapParent->mpHierarchy->mpFirstOverlap = this;
        if ( !pFirstOverlapParent->mpHierarchy->mpLastOverlap )
            pFirstOverlapParent->mpHierarchy->mpLastOverlap = this;
        else
            mpHierarchy->mpNext->mpHierarchy->mpPrev = this;
    }
    else
    {
        if ( pParent->ImplIsOverlapWindow() )
            mpHierarchy->mpOverlapWindow = pParent;
        else
            mpHierarchy->mpOverlapWindow = pParent->mpHierarchy->mpOverlapWindow;
        mpHierarchy->mpPrev = pParent->mpHierarchy->mpLastChild;
        pParent->mpHierarchy->mpLastChild = this;
        if ( !pParent->mpHierarchy->mpFirstChild )
            pParent->mpHierarchy->mpFirstChild = this;
        else
            mpHierarchy->mpPrev->mpHierarchy->mpNext = this;
    }
}

void Window::ImplRemoveWindow( bool bRemoveFrameData )
{
    // Frame windows do not participate in parent/sibling tree updates
    if ( mpClassification->mbFrame )
    {
        if ( bRemoveFrameData )
            GetOutDev()->ReleaseGraphics();
        return;
    }

    // Unlink from the absolute FrameData global overlap collection
    if ( ImplIsOverlapWindow() )
    {
        // True O(1) extraction utilizing our new mpPrevOverlap pointer
        if ( mpHierarchy->mpPrevOverlap )
            mpHierarchy->mpPrevOverlap->mpHierarchy->mpNextOverlap = mpHierarchy->mpNextOverlap;
        else
            mpPlatformState->mpFrameData->mpFirstOverlap = mpHierarchy->mpNextOverlap;

        if ( mpHierarchy->mpNextOverlap )
            mpHierarchy->mpNextOverlap->mpHierarchy->mpPrevOverlap = mpHierarchy->mpPrevOverlap;

        mpHierarchy->mpPrevOverlap = nullptr;
        mpHierarchy->mpNextOverlap = nullptr;
    }

    // Unlink from the forward sibling chains (Adjusting heads)
    if ( mpHierarchy->mpPrev )
        mpHierarchy->mpPrev->mpHierarchy->mpNext = mpHierarchy->mpNext;
    else if ( ImplIsOverlapWindow() )
        mpHierarchy->mpOverlapWindow->mpHierarchy->mpFirstOverlap = mpHierarchy->mpNext;
    else if ( mpHierarchy->mpParent )
        mpHierarchy->mpParent->mpHierarchy->mpFirstChild = mpHierarchy->mpNext;

    // Unlink from the backward sibling chains (Adjusting tails)
    if ( mpHierarchy->mpNext )
        mpHierarchy->mpNext->mpHierarchy->mpPrev = mpHierarchy->mpPrev;
    else if ( ImplIsOverlapWindow() )
        mpHierarchy->mpOverlapWindow->mpHierarchy->mpLastOverlap = mpHierarchy->mpPrev;
    else if ( mpHierarchy->mpParent )
        mpHierarchy->mpParent->mpHierarchy->mpLastChild = mpHierarchy->mpPrev;

    // Isolate this specific window node instance completely
    mpHierarchy->mpPrev = nullptr;
    mpHierarchy->mpNext = nullptr;

    if ( bRemoveFrameData )
    {
        GetOutDev()->ReleaseGraphics();
    }
}

void Window::reorderWithinParent(sal_uInt16 nNewPosition)
{
    sal_uInt16 nChildCount = 0;
    vcl::Window *pSource = mpHierarchy->mpParent->mpHierarchy->mpFirstChild;
    while (pSource)
    {
        if (nChildCount == nNewPosition)
            break;
        pSource = pSource->mpHierarchy->mpNext;
        nChildCount++;
    }

    if (pSource == this) //already at the right place
        return;

    ImplRemoveWindow(false);

    if (pSource)
    {
        mpHierarchy->mpNext = pSource;
        mpHierarchy->mpPrev = pSource->mpHierarchy->mpPrev;
        pSource->mpHierarchy->mpPrev = this;
    }
    else
        mpHierarchy->mpParent->mpHierarchy->mpLastChild = this;

    if (mpHierarchy->mpPrev)
        mpHierarchy->mpPrev->mpHierarchy->mpNext = this;
    else
        mpHierarchy->mpParent->mpHierarchy->mpFirstChild = this;
}

void Window::ImplToBottomChild()
{
    if ( ImplIsOverlapWindow() || mpVisibilityState->mbReallyVisible || (mpHierarchy->mpParent->mpHierarchy->mpLastChild.get() == this) )
        return;

    // put the window to the end of the list
    if ( mpHierarchy->mpPrev )
    {
        mpHierarchy->mpPrev->mpHierarchy->mpNext = mpHierarchy->mpNext;
    }
    else
    {
        // coverity[copy_paste_error : FALSE] - this is correct mpFirstChild, not mpNext
        mpHierarchy->mpParent->mpHierarchy->mpFirstChild = mpHierarchy->mpNext;
    }

    mpHierarchy->mpNext->mpHierarchy->mpPrev = mpHierarchy->mpPrev;
    mpHierarchy->mpPrev = mpHierarchy->mpParent->mpHierarchy->mpLastChild;
    mpHierarchy->mpParent->mpHierarchy->mpLastChild = this;
    mpHierarchy->mpPrev->mpHierarchy->mpNext = this;
    mpHierarchy->mpNext = nullptr;
}

void Window::ImplCalcToTop( ImplCalcToTopData* pPrevData )
{
    SAL_WARN_IF( !ImplIsOverlapWindow(), "vcl", "Window::ImplCalcToTop(): Is not an OverlapWindow" );

    if ( mpClassification->mbFrame )
        return;

    if ( !IsReallyVisible() )
        return;

    // calculate region, where the window overlaps with other windows
    vcl::Region  aRegion( GetOutputRectPixel() );
    vcl::Region  aInvalidateRegion;
    vcl::clipping::calcOverlapRegionOverlaps(*this, aRegion, aInvalidateRegion);

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

    if ( mpClassification->mbFrame )
    {
        // on a mouse click in the external window, it is the latter's
        // responsibility to assure our frame is put in front
        if ( !mpPlatformState->mpFrameData->mbHasFocus &&
             !mpPlatformState->mpFrameData->mbSysObjFocus &&
             !mpPlatformState->mpFrameData->mbInSysObjFocusHdl &&
             !mpPlatformState->mpFrameData->mbInSysObjToTopHdl )
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
                mpPlatformState->mpFrame->ToTop( nSysFlags );
            }
        }
    }
    else
    {
        if ( mpHierarchy->mpOverlapWindow->mpHierarchy->mpFirstOverlap.get() != this )
        {
            // remove window from the list
            mpHierarchy->mpPrev->mpHierarchy->mpNext = mpHierarchy->mpNext;
            if ( mpHierarchy->mpNext )
                mpHierarchy->mpNext->mpHierarchy->mpPrev = mpHierarchy->mpPrev;
            else
            {
                // coverity[copy_paste_error : FALSE] - this is correct mpLastOverlap, not mpPrev
                mpHierarchy->mpOverlapWindow->mpHierarchy->mpLastOverlap = mpHierarchy->mpPrev;
            }

            // take AlwaysOnTop into account
            bool    bOnTop = IsAlwaysOnTopEnabled();
            vcl::Window* pNextWin = mpHierarchy->mpOverlapWindow->mpHierarchy->mpFirstOverlap;
            if ( !bOnTop )
            {
                while ( pNextWin )
                {
                    if ( !pNextWin->IsAlwaysOnTopEnabled() )
                        break;
                    pNextWin = pNextWin->mpHierarchy->mpNext;
                }
            }

            // add the window to the list again
            mpHierarchy->mpNext = pNextWin;
            if ( pNextWin )
            {
                mpHierarchy->mpPrev = pNextWin->mpHierarchy->mpPrev;
                pNextWin->mpHierarchy->mpPrev = this;
            }
            else
            {
                mpHierarchy->mpPrev = mpHierarchy->mpOverlapWindow->mpHierarchy->mpLastOverlap;
                mpHierarchy->mpOverlapWindow->mpHierarchy->mpLastOverlap = this;
            }
            if ( mpHierarchy->mpPrev )
                mpHierarchy->mpPrev->mpHierarchy->mpNext = this;
            else
                mpHierarchy->mpOverlapWindow->mpHierarchy->mpFirstOverlap = this;

            // recalculate ClipRegion of this and all overlapping windows
            if ( IsReallyVisible() )
                vcl::clipping::setClipFlagOverlapWindows(*mpHierarchy->mpOverlapWindow);
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
        pOverlapWindow = mpHierarchy->mpOverlapWindow;

    // first calculate paint areas
    vcl::Window* pTempOverlapWindow = pOverlapWindow;
    aStartData.mpNext = nullptr;
    pCurData = &aStartData;
    do
    {
        pTempOverlapWindow->ImplCalcToTop( pCurData );
        if ( pCurData->mpNext )
            pCurData = pCurData->mpNext.get();
        pTempOverlapWindow = pTempOverlapWindow->mpHierarchy->mpOverlapWindow;
    }
    while ( !pTempOverlapWindow->mpClassification->mbFrame );
    // next calculate the paint areas of the ChildOverlap windows
    pTempOverlapWindow = mpHierarchy->mpFirstOverlap;
    while ( pTempOverlapWindow )
    {
        pTempOverlapWindow->ImplCalcToTop( pCurData );
        if ( pCurData->mpNext )
            pCurData = pCurData->mpNext.get();
        pTempOverlapWindow = pTempOverlapWindow->mpHierarchy->mpNext;
    }

    // and next change the windows list
    pTempOverlapWindow = pOverlapWindow;
    do
    {
        pTempOverlapWindow->ImplToTop( nFlags );
        pTempOverlapWindow = pTempOverlapWindow->mpHierarchy->mpOverlapWindow;
    }
    while ( !pTempOverlapWindow->mpClassification->mbFrame );
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
            if (!pFocusWindow->mpHierarchy->mpBorderWindow && pFocusWindow->mpFocusState->canGrabFocusOnActivate())
                break;

            pFocusWindow = pFocusWindow->ImplGetParent();
        }

        if (pFocusWindow->mpFocusState->canGrabFocusOnActivate() && !pFocusWindow->HasChildPathFocus(true))
            pFocusWindow->GrabFocus();
    }

    if ( bReallyVisible )
        ImplGenerateMouseMove();
}

void Window::ImplShowAllOverlaps()
{
    vcl::Window* pOverlapWindow = mpHierarchy->mpFirstOverlap;
    while ( pOverlapWindow )
    {
        if ( pOverlapWindow->mpVisibilityState->mbOverlapVisible )
        {
            pOverlapWindow->Show( true, ShowFlags::NoActivate );
            pOverlapWindow->mpVisibilityState->mbOverlapVisible = false;
        }

        pOverlapWindow = pOverlapWindow->mpHierarchy->mpNext;
    }
}

void Window::ImplHideAllOverlaps()
{
    vcl::Window* pOverlapWindow = mpHierarchy->mpFirstOverlap;
    while ( pOverlapWindow )
    {
        if ( pOverlapWindow->IsVisible() )
        {
            pOverlapWindow->mpVisibilityState->mbOverlapVisible = true;
            pOverlapWindow->Show( false );
        }

        pOverlapWindow = pOverlapWindow->mpHierarchy->mpNext;
    }
}

void Window::ToTop( ToTopFlags nFlags )
{
    if (!mpClassification)
        return;

    ImplStartToTop( nFlags );
    ImplFocusToTop( nFlags, IsReallyVisible() );
}

void Window::SetZOrder( vcl::Window* pRefWindow, ZOrderFlags nFlags )
{

    if ( mpHierarchy->mpBorderWindow )
    {
        mpHierarchy->mpBorderWindow->SetZOrder( pRefWindow, nFlags );
        return;
    }

    if ( nFlags & ZOrderFlags::First )
    {
        if ( ImplIsOverlapWindow() )
            pRefWindow = mpHierarchy->mpOverlapWindow->mpHierarchy->mpFirstOverlap;
        else
            pRefWindow = mpHierarchy->mpParent->mpHierarchy->mpFirstChild;
        nFlags |= ZOrderFlags::Before;
    }
    else if ( nFlags & ZOrderFlags::Last )
    {
        if ( ImplIsOverlapWindow() )
            pRefWindow = mpHierarchy->mpOverlapWindow->mpHierarchy->mpLastOverlap;
        else
            pRefWindow = mpHierarchy->mpParent->mpHierarchy->mpLastChild;
        nFlags |= ZOrderFlags::Behind;
    }

    while ( pRefWindow && pRefWindow->mpHierarchy->mpBorderWindow )
        pRefWindow = pRefWindow->mpHierarchy->mpBorderWindow;
    if (!pRefWindow || pRefWindow == this || mpClassification->mbFrame)
        return;

    SAL_WARN_IF( pRefWindow->mpHierarchy->mpParent != mpHierarchy->mpParent, "vcl", "Window::SetZOrder() - pRefWindow has other parent" );
    if ( nFlags & ZOrderFlags::Before )
    {
        if ( pRefWindow->mpHierarchy->mpPrev.get() == this )
            return;

        if ( ImplIsOverlapWindow() )
        {
            if ( mpHierarchy->mpPrev )
                mpHierarchy->mpPrev->mpHierarchy->mpNext = mpHierarchy->mpNext;
            else
                mpHierarchy->mpOverlapWindow->mpHierarchy->mpFirstOverlap = mpHierarchy->mpNext;
            if ( mpHierarchy->mpNext )
                mpHierarchy->mpNext->mpHierarchy->mpPrev = mpHierarchy->mpPrev;
            else
                mpHierarchy->mpOverlapWindow->mpHierarchy->mpLastOverlap = mpHierarchy->mpPrev;
            if ( !pRefWindow->mpHierarchy->mpPrev )
                mpHierarchy->mpOverlapWindow->mpHierarchy->mpFirstOverlap = this;
        }
        else
        {
            if ( mpHierarchy->mpPrev )
                mpHierarchy->mpPrev->mpHierarchy->mpNext = mpHierarchy->mpNext;
            else
                mpHierarchy->mpParent->mpHierarchy->mpFirstChild = mpHierarchy->mpNext;
            if ( mpHierarchy->mpNext )
                mpHierarchy->mpNext->mpHierarchy->mpPrev = mpHierarchy->mpPrev;
            else
                mpHierarchy->mpParent->mpHierarchy->mpLastChild = mpHierarchy->mpPrev;
            if ( !pRefWindow->mpHierarchy->mpPrev )
                mpHierarchy->mpParent->mpHierarchy->mpFirstChild = this;
        }

        mpHierarchy->mpPrev = pRefWindow->mpHierarchy->mpPrev;
        mpHierarchy->mpNext = pRefWindow;
        if ( mpHierarchy->mpPrev )
            mpHierarchy->mpPrev->mpHierarchy->mpNext = this;
        mpHierarchy->mpNext->mpHierarchy->mpPrev = this;
    }
    else if ( nFlags & ZOrderFlags::Behind )
    {
        if ( pRefWindow->mpHierarchy->mpNext.get() == this )
            return;

        if ( ImplIsOverlapWindow() )
        {
            if ( mpHierarchy->mpPrev )
                mpHierarchy->mpPrev->mpHierarchy->mpNext = mpHierarchy->mpNext;
            else
                mpHierarchy->mpOverlapWindow->mpHierarchy->mpFirstOverlap = mpHierarchy->mpNext;
            if ( mpHierarchy->mpNext )
                mpHierarchy->mpNext->mpHierarchy->mpPrev = mpHierarchy->mpPrev;
            else
                mpHierarchy->mpOverlapWindow->mpHierarchy->mpLastOverlap = mpHierarchy->mpPrev;
            if ( !pRefWindow->mpHierarchy->mpNext )
                mpHierarchy->mpOverlapWindow->mpHierarchy->mpLastOverlap = this;
        }
        else
        {
            if ( mpHierarchy->mpPrev )
                mpHierarchy->mpPrev->mpHierarchy->mpNext = mpHierarchy->mpNext;
            else
                mpHierarchy->mpParent->mpHierarchy->mpFirstChild = mpHierarchy->mpNext;
            if ( mpHierarchy->mpNext )
                mpHierarchy->mpNext->mpHierarchy->mpPrev = mpHierarchy->mpPrev;
            else
                mpHierarchy->mpParent->mpHierarchy->mpLastChild = mpHierarchy->mpPrev;
            if ( !pRefWindow->mpHierarchy->mpNext )
                mpHierarchy->mpParent->mpHierarchy->mpLastChild = this;
        }

        mpHierarchy->mpPrev = pRefWindow;
        mpHierarchy->mpNext = pRefWindow->mpHierarchy->mpNext;
        if ( mpHierarchy->mpNext )
            mpHierarchy->mpNext->mpHierarchy->mpPrev = this;
        mpHierarchy->mpPrev->mpHierarchy->mpNext = this;
    }

    if ( !IsReallyVisible() )
        return;

    if ( !mpClippingState->mbInitWinClipRegion && mpClippingState->maWinClipRegion.IsEmpty() )
        return;

    bool bInitWinClipRegion = mpClippingState->mbInitWinClipRegion;
    vcl::clipping::setClipFlag(*this);

    // When ClipRegion was not initialised, assume
    // the window has not been sent, therefore do not
    // trigger any Invalidates. This is an optimization
    // for HTML documents with many controls. If this
    // check gives problems, a flag should be introduced
    // which tracks whether the window has already been
    // emitted after Show
    if ( bInitWinClipRegion )
        return;

    // Invalidate all windows which are next to each other
    // Is INCOMPLETE !!!
    tools::Rectangle   aWinRect = GetOutputRectPixel();
    vcl::Window*     pWindow = nullptr;
    if ( ImplIsOverlapWindow() )
    {
        if ( mpHierarchy->mpOverlapWindow )
            pWindow = mpHierarchy->mpOverlapWindow->mpHierarchy->mpFirstOverlap;
    }
    else
        pWindow = ImplGetParent()->mpHierarchy->mpFirstChild;
    // Invalidate all windows in front of us and which are covered by us
    while ( pWindow )
    {
        if ( pWindow == this )
            break;
        tools::Rectangle aCompRect = pWindow->GetOutputRectPixel();
        if ( aWinRect.Overlaps( aCompRect ) )
            pWindow->Invalidate( InvalidateFlags::Children | InvalidateFlags::NoTransparent );
        pWindow = pWindow->mpHierarchy->mpNext;
    }

    // If we are covered by a window in the background
    // we should redraw it
    while ( pWindow )
    {
        if ( pWindow != this )
        {
            tools::Rectangle aCompRect = pWindow->GetOutputRectPixel();
            if ( aWinRect.Overlaps( aCompRect ) )
            {
                Invalidate( InvalidateFlags::Children | InvalidateFlags::NoTransparent );
                break;
            }
        }
        pWindow = pWindow->mpHierarchy->mpNext;
    }
}

void Window::EnableAlwaysOnTop( bool bEnable )
{

    mpClassification->mbAlwaysOnTop = bEnable;

    if ( mpHierarchy->mpBorderWindow )
        mpHierarchy->mpBorderWindow->EnableAlwaysOnTop( bEnable );
    else if ( bEnable && IsReallyVisible() )
        ToTop();

    if ( mpClassification->mbFrame )
        mpPlatformState->mpFrame->SetAlwaysOnTop( bEnable );
}

bool Window::IsTopWindow() const
{
    if ( !mpClassification || mpClassification->mbInDispose )
        return false;

    // topwindows must be frames or they must have a borderwindow which is a frame
    if( !mpClassification->mbFrame && (!mpHierarchy->mpBorderWindow || !mpHierarchy->mpBorderWindow->mpClassification->mbFrame ) )
        return false;

    ImplGetWinData();
    if( mpWinData->mnIsTopWindow == sal_uInt16(~0))    // still uninitialized
    {
        // #113722#, cache result of expensive queryInterface call
        vcl::Window *pThisWin = const_cast<vcl::Window*>(this);
        uno::Reference< XTopWindow > xTopWindow( pThisWin->GetComponentInterface(), UNO_QUERY );
        pThisWin->mpWinData->mnIsTopWindow = xTopWindow.is() ? 1 : 0;
    }
    return mpWinData->mnIsTopWindow == 1;
}

vcl::Window* Window::ImplFindWindow( const Point& rFramePos )
{
    vcl::Window* pTempWindow;
    vcl::Window* pFindWindow;

    // first check all overlapping windows
    pTempWindow = mpHierarchy->mpFirstOverlap;
    while ( pTempWindow )
    {
        pFindWindow = pTempWindow->ImplFindWindow( rFramePos );
        if ( pFindWindow )
            return pFindWindow;
        pTempWindow = pTempWindow->mpHierarchy->mpNext;
    }

    // then we check our window
    if ( !mpVisibilityState->mbVisible )
        return nullptr;

    WindowHitTest nHitTest = ImplHitTest( rFramePos );
    if ( nHitTest & WindowHitTest::Inside )
    {
        // and then we check all child windows
        pTempWindow = mpHierarchy->mpFirstChild;
        while ( pTempWindow )
        {
            pFindWindow = pTempWindow->ImplFindWindow( rFramePos );
            if ( pFindWindow )
                return pFindWindow;
            pTempWindow = pTempWindow->mpHierarchy->mpNext;
        }

        if ( nHitTest & WindowHitTest::Transparent )
            return nullptr;
        else
            return this;
    }

    return nullptr;
}

bool Window::IsAncestorOf(const vcl::Window& rWindow) const
{
    vcl::Window* pWindow = rWindow.GetParent();

    while (pWindow)
    {
        if (pWindow == this)
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
    bool bBecameReallyInvisible = mpVisibilityState->mbReallyVisible;

    GetOutDev()->mbDevOutput     = false;
    mpVisibilityState->mbReallyVisible = false;
    mpVisibilityState->mbReallyShown   = false;

    // the SHOW/HIDE events serve as indicators to send child creation/destroy events to the access bridge.
    // For this, the data member of the event must not be NULL.
    // Previously, we did this in Window::Show, but there some events got lost in certain situations.
    if( bBecameReallyInvisible && ImplIsAccessibleCandidate() )
        CallEventListeners( VclEventId::WindowHide, this );
        // TODO. It's kind of a hack that we're re-using the VclEventId::WindowHide. Normally, we should
        // introduce another event which explicitly triggers the Accessibility implementations.

    vcl::Window* pWindow = mpHierarchy->mpFirstOverlap;
    while ( pWindow )
    {
        if ( pWindow->mpVisibilityState->mbReallyVisible )
            pWindow->ImplResetReallyVisible();
        pWindow = pWindow->mpHierarchy->mpNext;
    }

    pWindow = mpHierarchy->mpFirstChild;
    while ( pWindow )
    {
        if ( pWindow->mpVisibilityState->mbReallyVisible )
            pWindow->ImplResetReallyVisible();
        pWindow = pWindow->mpHierarchy->mpNext;
    }
}

void Window::ImplUpdateWindowPtr( vcl::Window* pWindow )
{
    if ( mpHierarchy->mpFrameWindow != pWindow->mpHierarchy->mpFrameWindow )
    {
        // release graphic
        OutputDevice *pOutDev = GetOutDev();
        pOutDev->ReleaseGraphics();
    }

    mpPlatformState->mpFrameData     = pWindow->mpPlatformState->mpFrameData;
    if (mpPlatformState->mpFrame != pWindow->mpPlatformState->mpFrame)
    {
        mpPlatformState->mpFrame = pWindow->mpPlatformState->mpFrame;
        if (mpPlatformState->mpSysObj)
            mpPlatformState->mpSysObj->Reparent(mpPlatformState->mpFrame);
    }
    mpHierarchy->mpFrameWindow   = pWindow->mpHierarchy->mpFrameWindow;
    if ( pWindow->ImplIsOverlapWindow() )
        mpHierarchy->mpOverlapWindow = pWindow;
    else
        mpHierarchy->mpOverlapWindow = pWindow->mpHierarchy->mpOverlapWindow;

    vcl::Window* pChild = mpHierarchy->mpFirstChild;
    while ( pChild )
    {
        pChild->ImplUpdateWindowPtr( pWindow );
        pChild = pChild->mpHierarchy->mpNext;
    }
}

void Window::ImplUpdateWindowPtr()
{
    vcl::Window* pChild = mpHierarchy->mpFirstChild;
    while ( pChild )
    {
        pChild->ImplUpdateWindowPtr( this );
        pChild = pChild->mpHierarchy->mpNext;
    }
}

void Window::ImplUpdateOverlapWindowPtr( bool bNewFrame )
{
    bool bVisible = IsVisible();
    Show( false );
    ImplRemoveWindow( bNewFrame );
    vcl::Window* pRealParent = mpHierarchy->mpRealParent;
    ImplInsertWindow( ImplGetParent() );
    mpHierarchy->mpRealParent = pRealParent;
    ImplUpdateWindowPtr();
    if ( ImplUpdatePos() )
        ImplUpdateNativeObjectPos();

    if ( bNewFrame )
    {
        vcl::Window* pOverlapWindow = mpHierarchy->mpFirstOverlap;
        while ( pOverlapWindow )
        {
            vcl::Window* pNextOverlapWindow = pOverlapWindow->mpHierarchy->mpNext;
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

vcl::Window* Window::GetParent() const
{
    return mpClassification ? mpHierarchy->mpRealParent.get() : nullptr;
}

void Window::SetParent( vcl::Window* pNewParent )
{
    SAL_WARN_IF( !pNewParent, "vcl", "Window::SetParent(): pParent == NULL" );
    SAL_WARN_IF( pNewParent == this, "vcl", "someone tried to reparent a window to itself" );

    if( !pNewParent || pNewParent == this )
        return;

    if (!mpClassification)
    {
        SAL_WARN("vcl", "Window::SetParent(): mpClassification == NULL");
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
    if( (GetStyle() & WB_OWNERDRAWDECORATION) && mpClassification->mbFrame )
    {
        ::std::vector< VclPtr<vcl::Window> >& rList = ImplGetOwnerDrawList();
        auto p = ::std::find( rList.begin(), rList.end(), VclPtr<vcl::Window>(this) );
        if( p != rList.end() )
            rList.erase( p );
    }

    ImplSetFrameParent( pNewParent );

    if ( mpHierarchy->mpBorderWindow )
    {
        mpHierarchy->mpRealParent = pNewParent;
        mpHierarchy->mpBorderWindow->SetParent( pNewParent );
        return;
    }

    if ( mpHierarchy->mpParent.get() == pNewParent )
        return;

    if ( mpClassification->mbFrame )
        mpPlatformState->mpFrame->SetParent( pNewParent->mpPlatformState->mpFrame );

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
        if ( mpHierarchy->mpOverlapWindow.get() != pNewOverlapWindow )
            pOldOverlapWindow = mpHierarchy->mpOverlapWindow;
        else
            pOldOverlapWindow = nullptr;
    }

    // convert windows in the hierarchy
    bool bFocusOverlapWin = HasChildPathFocus( true );
    bool bFocusWin = HasChildPathFocus();
    bool bNewFrame = pNewParent->mpHierarchy->mpFrameWindow != mpHierarchy->mpFrameWindow;
    if ( bNewFrame )
    {
        if ( mpPlatformState->mpFrameData->mpFocusWin )
        {
            if ( IsWindowOrChild( mpPlatformState->mpFrameData->mpFocusWin ) )
                mpPlatformState->mpFrameData->mpFocusWin = nullptr;
        }
        if ( mpPlatformState->mpFrameData->mpMouseMoveWin )
        {
            if ( IsWindowOrChild( mpPlatformState->mpFrameData->mpMouseMoveWin ) )
                mpPlatformState->mpFrameData->mpMouseMoveWin = nullptr;
        }
        if ( mpPlatformState->mpFrameData->mpMouseDownWin )
        {
            if ( IsWindowOrChild( mpPlatformState->mpFrameData->mpMouseDownWin ) )
                mpPlatformState->mpFrameData->mpMouseDownWin = nullptr;
        }
    }
    ImplRemoveWindow( bNewFrame );
    ImplInsertWindow( pNewParent );

    if ( mpClippingState->meParentClipMode & ParentClipMode::Clip )
        pNewParent->mpClippingState->mbClipChildren = true;

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
            vcl::Window* pOverlapWindow = mpHierarchy->mpFirstOverlap;
            while ( pOverlapWindow )
            {
                vcl::Window* pNextOverlapWindow = pOverlapWindow->mpHierarchy->mpNext;
                pOverlapWindow->ImplUpdateOverlapWindowPtr( bNewFrame );
                pOverlapWindow = pNextOverlapWindow;
            }
        }
    }
    else if ( pOldOverlapWindow )
    {
        // reset Focus-Save
        vcl::Window* pLastFocus = pOldOverlapWindow->mpInput->getLastFocusWindow();
        if (bFocusWin || (pLastFocus && IsWindowOrChild(pLastFocus)))
            pOldOverlapWindow->mpInput->clearLastFocusWindow();

        vcl::Window* pOverlapWindow = pOldOverlapWindow->mpHierarchy->getFirstOverlap();
        while (pOverlapWindow)
        {
            vcl::Window* pNextOverlapWindow = pOverlapWindow->mpHierarchy->getNext();
            if (IsAncestorOf(*pOverlapWindow->ImplGetWindow()))
                pOverlapWindow->ImplUpdateOverlapWindowPtr(bNewFrame);
            pOverlapWindow = pNextOverlapWindow;
        }

        // update activate-status at next overlap window
        if (HasChildPathFocus(true))
            ImplCallFocusChangeActivate(pNewOverlapWindow, pOldOverlapWindow);
    }

    // also convert Activate-Status
    if ( bNewFrame )
    {
        if ( (GetType() == WindowType::BORDERWINDOW) &&
             (ImplGetWindow()->GetType() == WindowType::FLOATINGWINDOW) )
            static_cast<ImplBorderWindow*>(this)->SetDisplayActive( mpPlatformState->mpFrameData->mbHasFocus );
    }

    // when required give focus to new frame if
    // FocusWindow is changed with SetParent()
    if ( bFocusOverlapWin )
    {
        mpPlatformState->mpFrameData->mpFocusWin = Application::GetFocusWindow();
        if ( !mpPlatformState->mpFrameData->mbHasFocus )
        {
            mpPlatformState->mpFrame->ToTop( SalFrameToTop::NONE );
        }
    }

    // Assure DragSource and DropTarget members are created
    if ( bNewFrame )
    {
            GetDropTarget();
    }

    if( bChangeTaskPaneList )
        pNewSysWin->GetTaskPaneList()->AddWindow( this );

    if( (GetStyle() & WB_OWNERDRAWDECORATION) && mpClassification->mbFrame )
        ImplGetOwnerDrawList().emplace_back(this );

    if ( bVisible )
        Show( true, ShowFlags::NoFocusChange | ShowFlags::NoActivate );
}

sal_uInt16 Window::GetChildCount() const
{
    if (!mpClassification)
        return 0;

    sal_uInt16  nChildCount = 0;
    vcl::Window* pChild = mpHierarchy->mpFirstChild;
    while ( pChild )
    {
        nChildCount++;
        pChild = pChild->mpHierarchy->mpNext;
    }

    return nChildCount;
}

vcl::Window* Window::GetChild( sal_uInt16 nChild ) const
{
    if (!mpClassification)
        return nullptr;

    sal_uInt16  nChildCount = 0;
    vcl::Window* pChild = mpHierarchy->mpFirstChild;
    while ( pChild )
    {
        if ( nChild == nChildCount )
            return pChild;
        pChild = pChild->mpHierarchy->mpNext;
        nChildCount++;
    }

    return nullptr;
}

vcl::Window* Window::GetWindow( GetWindowType nType ) const
{
    if (!mpClassification)
        return nullptr;

    switch ( nType )
    {
        case GetWindowType::Parent:
            return mpHierarchy->mpRealParent;

        case GetWindowType::FirstChild:
            return mpHierarchy->mpFirstChild;

        case GetWindowType::LastChild:
            return mpHierarchy->mpLastChild;

        case GetWindowType::Prev:
            return mpHierarchy->mpPrev;

        case GetWindowType::Next:
            return mpHierarchy->mpNext;

        case GetWindowType::FirstOverlap:
            return mpHierarchy->mpFirstOverlap;

        case GetWindowType::Overlap:
            if ( ImplIsOverlapWindow() )
                return const_cast<vcl::Window*>(this);
            else
                return mpHierarchy->mpOverlapWindow;

        case GetWindowType::ParentOverlap:
            if ( ImplIsOverlapWindow() )
                return mpHierarchy->mpOverlapWindow;
            else
                return mpHierarchy->mpOverlapWindow->mpHierarchy->mpOverlapWindow;

        case GetWindowType::Client:
            return this->ImplGetWindow();

        case GetWindowType::RealParent:
            return ImplGetParent();

        case GetWindowType::Frame:
            return mpHierarchy->mpFrameWindow;

        case GetWindowType::Border:
            if ( mpHierarchy->mpBorderWindow )
                return mpHierarchy->mpBorderWindow->GetWindow( GetWindowType::Border );
            return const_cast<vcl::Window*>(this);

        case GetWindowType::FirstTopWindowChild:
            return ImplGetWinData()->maTopWindowChildren.empty() ? nullptr : (*ImplGetWinData()->maTopWindowChildren.begin()).get();

        case GetWindowType::NextTopWindowSibling:
        {
            if ( !mpHierarchy->mpRealParent )
                return nullptr;
            const ::std::list< VclPtr<vcl::Window> >& rTopWindows( mpHierarchy->mpRealParent->ImplGetWinData()->maTopWindowChildren );
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
        if( IsAncestorOf( *pFrameWindow ) )
        {
            SAL_WARN_IF( mpPlatformState->mpFrame == pFrameWindow->mpPlatformState->mpFrame, "vcl", "SetFrameParent to own" );
            SAL_WARN_IF( !mpPlatformState->mpFrame, "vcl", "no frame" );
            SalFrame* pParentFrame = pParent ? pParent->mpPlatformState->mpFrame : nullptr;
            pFrameWindow->mpPlatformState->mpFrame->SetParent( pParentFrame );
        }
        pFrameWindow = pFrameWindow->mpPlatformState->mpFrameData->mpNextFrame;
    }
}

vcl::Window* Window::ImplGetParent() const
{
    return mpHierarchy ? mpHierarchy->mpParent.get() : nullptr;
}

bool Window::ImplIsOverlapWindow() const
{
    return mpClassification && mpClassification->mbOverlapWin;
}

vcl::Window* Window::ImplGetFirstOverlapWindow()
{
    if (!mpClassification)
        return nullptr;

    if ( mpClassification->mbOverlapWin )
        return this;
    else
        return mpHierarchy ? mpHierarchy->mpOverlapWindow : nullptr;
}

const vcl::Window* Window::ImplGetFirstOverlapWindow() const
{
    if (!mpClassification)
        return nullptr;

    if ( mpClassification->mbOverlapWin )
        return this;
    else
        return mpHierarchy ? mpHierarchy->mpOverlapWindow : nullptr;
}

} /* namespace vcl */

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
