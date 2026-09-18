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

#include <sal/log.hxx>

#include <vcl/dndlistenercontainer.hxx>
#include <vcl/taskpanelist.hxx>
#include <vcl/syswin.hxx>
#include <vcl/window.hxx>

#include <salframe.hxx>
#include <salobj.hxx>
#include <svdata.hxx>
#include <window/subclasses/decorations/brdwin.hxx>
#include <window/state/ImplFrameData.hxx>
#include <window/state/ImplWinData.hxx>
#include <window/state/WindowVisibilityState.hxx>
#include <window/state/WindowPlatformState.hxx>
#include <window/state/WindowClassification.hxx>
#include <window/state/WindowInput.hxx>
#include <window/state/WindowClippingState.hxx>
#include <window/state/WindowHierarchy.hxx>
#include <window/state/WindowFocusState.hxx>

#include <com/sun/star/awt/XTopWindow.hpp>
#include <com/sun/star/awt/XVclWindowPeer.hpp>

struct ImplCalcToTopData
{
    std::unique_ptr<ImplCalcToTopData> mpNext;
    VclPtr<vcl::Window>                mpWindow;
    std::unique_ptr<vcl::Region>       mpInvalidateRegion;
};

namespace vcl
{

void Window::ImplInsertWindow(vcl::Window* pParent)
{
    mpHierarchy->setParent(pParent);
    mpHierarchy->setRealParent(pParent);

    if (!pParent || mpClassification->mbFrame)
        return;

    ImplAttachToFrame(pParent);

    if (ImplIsOverlapWindow())
        mpHierarchy->insertOverlapWindow(this, pParent, *mpPlatformState->mpFrameData);
    else
        mpHierarchy->linkIntoChildChain(this, pParent);
}

void Window::ImplAttachToFrame(vcl::Window* pParent)
{
    vcl::Window* pFrameParent = pParent->mpHierarchy->getFrameWindow();
    mpPlatformState->mpFrameData = pFrameParent->mpPlatformState->mpFrameData;

    if (mpPlatformState->mpFrame != pFrameParent->mpPlatformState->mpFrame)
    {
        mpPlatformState->mpFrame = pFrameParent->mpPlatformState->mpFrame;

        if (mpPlatformState->mpSysObj)
            mpPlatformState->mpSysObj->Reparent(mpPlatformState->mpFrame);
    }

    mpHierarchy->setFrameWindow(pFrameParent);
    mpClassification->mbFrame = false;
}

void Window::ImplRemoveWindow(bool bRemoveFrameData)
{
    // Frame windows do not participate in parent/sibling tree updates
    if (!mpClassification->mbFrame)
    {
        if (ImplIsOverlapWindow())
            mpHierarchy->unlinkOverlapWindow(*mpPlatformState->mpFrameData);
        else
            mpHierarchy->unlinkFromChildChain();
    }

    if (bRemoveFrameData)
        GetOutDev()->ReleaseGraphics();
}

void Window::reorderWithinParent(sal_uInt16 nNewPosition)
{
    if (mpHierarchy)
        mpHierarchy->reorderWithinParent(this, nNewPosition);
}

void Window::ImplToBottomChild()
{
    if (ImplIsOverlapWindow() || mpVisibilityState->mbReallyVisible)
        return;

    if (mpHierarchy)
        mpHierarchy->toBottomChild(this);
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
    mpClippingState->calcOverlapRegionOverlaps(*this, aRegion, aInvalidateRegion);

    if ( !aInvalidateRegion.IsEmpty() )
    {
        ImplCalcToTopData* pData = new ImplCalcToTopData;
        pPrevData->mpNext.reset(pData);
        pData->mpWindow = this;
        pData->mpInvalidateRegion.reset(new vcl::Region(std::move(aInvalidateRegion)));
    }
}

void Window::ImplToTop(ToTopFlags nFlags)
{
    SAL_WARN_IF(!ImplIsOverlapWindow(), "vcl", "Window::ImplToTop(): Is not an OverlapWindow");

    if (mpClassification->mbFrame)
    {
        ImplBringFrameToTop(nFlags);
        return;
    }

    if (mpHierarchy && mpHierarchy->toTopOverlap(this, IsAlwaysOnTopEnabled()))
    {
        vcl::Window* pOverlapWindow = mpHierarchy->getOverlapWindow();
        if (IsReallyVisible() && pOverlapWindow && mpClippingState)
            mpClippingState->setClipFlagOverlapWindows(*pOverlapWindow, /*bSysObjOnlySmaller=*/false);
    }
}

static SalFrameToTop lcl_toSalFrameToTop(ToTopFlags nFlags)
{
    SalFrameToTop nSysFlags = SalFrameToTop::NONE;

    if (nFlags & ToTopFlags::RestoreWhenMin)
        nSysFlags |= SalFrameToTop::RestoreWhenMin;

    if (nFlags & ToTopFlags::ForegroundTask)
        nSysFlags |= SalFrameToTop::ForegroundTask;

    if (nFlags & ToTopFlags::GrabFocusOnly)
        nSysFlags |= SalFrameToTop::GrabFocusOnly;

    return nSysFlags;
}

void Window::ImplBringFrameToTop(ToTopFlags nFlags)
{
    // On a mouse click in the external window, it is the latter's
    // responsibility to ensure our frame is put in front
    if (mpPlatformState->mpFrameData->mbHasFocus ||
        mpPlatformState->mpFrameData->mbSysObjFocus ||
        mpPlatformState->mpFrameData->mbInSysObjFocusHdl ||
        mpPlatformState->mpFrameData->mbInSysObjToTopHdl)
    {
        return;
    }

    // Do not bring floating windows on the client to top
    if (ImplGetClientWindow() && (ImplGetClientWindow()->GetStyle() & WB_SYSTEMFLOATWIN))
        return;

    mpPlatformState->mpFrame->ToTop(lcl_toSalFrameToTop(nFlags));
}

void Window::ImplStartToTop(ToTopFlags nFlags)
{
    vcl::Window* pOverlapWindow = ImplIsOverlapWindow() ? this : mpHierarchy->getOverlapWindow();

    ImplCalcToTopData aStartData;
    aStartData.mpNext = nullptr;

    ImplCollectToTopPaintAreas(pOverlapWindow, aStartData);
    ImplReorderOverlapChain(pOverlapWindow, nFlags);
    ImplInvalidateToTopRegions(aStartData.mpNext.get());
}

void Window::ImplCollectToTopPaintAreas(vcl::Window* pOverlapWindow, ImplCalcToTopData& rStartData)
{
    ImplCalcToTopData* pCurData = &rStartData;

    // Calculate paint areas along ancestor overlap chain up to frame
    vcl::Window* pTempOverlapWindow = pOverlapWindow;
    do
    {
        pTempOverlapWindow->ImplCalcToTop(pCurData);

        if (pCurData->mpNext)
            pCurData = pCurData->mpNext.get();

        pTempOverlapWindow = pTempOverlapWindow->mpHierarchy->getOverlapWindow();
    }
    while (!pTempOverlapWindow->mpClassification->mbFrame);

    // Calculate paint areas of child overlap windows
    if (vcl::Window* pFirstOverlap = mpHierarchy ? mpHierarchy->getFirstOverlap() : nullptr)
    {
        WindowHierarchy::forEachSibling(pFirstOverlap, [&](vcl::Window& rOverlapChild) {
            rOverlapChild.ImplCalcToTop(pCurData);
            if (pCurData->mpNext)
                pCurData = pCurData->mpNext.get();
        });
    }
}

void Window::ImplReorderOverlapChain(vcl::Window* pOverlapWindow, ToTopFlags nFlags)
{
    vcl::Window* pTempOverlapWindow = pOverlapWindow;
    do
    {
        pTempOverlapWindow->ImplToTop(nFlags);
        pTempOverlapWindow = pTempOverlapWindow->mpHierarchy->getOverlapWindow();
    }
    while (!pTempOverlapWindow->mpClassification->mbFrame);
}

void Window::ImplInvalidateToTopRegions(const ImplCalcToTopData* pCurData)
{
    while (pCurData)
    {
        pCurData->mpWindow->ImplInvalidateFrameRegion(pCurData->mpInvalidateRegion.get(), InvalidateFlags::Children);
        pCurData = pCurData->mpNext.get();
    }
}

void Window::ImplFocusToTop(ToTopFlags nFlags, bool bReallyVisible)
{
    if (!(nFlags & ToTopFlags::NoGrabFocus))
    {
        vcl::Window* pFocusWindow = this;
        while (!pFocusWindow->ImplIsOverlapWindow())
        {
            if (!pFocusWindow->mpHierarchy->hasBorderWindow() && pFocusWindow->mpFocusState->canGrabFocusOnActivate())
                break;

            pFocusWindow = pFocusWindow->ImplGetParent();
        }

        if (pFocusWindow->mpFocusState->canGrabFocusOnActivate() && !pFocusWindow->HasChildPathFocus(true))
            pFocusWindow->GrabFocus();
    }

    if (bReallyVisible)
        ImplGenerateMouseMove();
}

void Window::ImplShowAllOverlaps()
{
    if (vcl::Window* pFirstOverlap = mpHierarchy ? mpHierarchy->getFirstOverlap() : nullptr)
    {
        WindowHierarchy::forEachSibling(pFirstOverlap, [](vcl::Window& rOverlap) {
            if (rOverlap.mpVisibilityState && rOverlap.mpVisibilityState->mbOverlapVisible)
            {
                rOverlap.Show(true, ShowFlags::NoActivate);
                rOverlap.mpVisibilityState->mbOverlapVisible = false;
            }
        });
    }
}

void Window::ImplHideAllOverlaps()
{
    if (vcl::Window* pFirstOverlap = mpHierarchy ? mpHierarchy->getFirstOverlap() : nullptr)
    {
        WindowHierarchy::forEachSibling(pFirstOverlap, [](vcl::Window& rOverlap) {
            if (rOverlap.IsVisible() && rOverlap.mpVisibilityState)
            {
                rOverlap.mpVisibilityState->mbOverlapVisible = true;
                rOverlap.Show(false);
            }
        });
    }
}

void Window::ToTop( ToTopFlags nFlags )
{
    if (!mpClassification)
        return;

    ImplStartToTop( nFlags );
    ImplFocusToTop( nFlags, IsReallyVisible() );
}

void Window::SetZOrder(vcl::Window* pRefWindow, ZOrderFlags nFlags)
{
    if (mpHierarchy->hasBorderWindow())
    {
        mpHierarchy->getBorderWindow()->SetZOrder(pRefWindow, nFlags);
        return;
    }

    pRefWindow = ImplResolveZOrderReference(pRefWindow, nFlags);

    if (!pRefWindow || pRefWindow == this || mpClassification->mbFrame)
        return;

    SAL_WARN_IF(pRefWindow->mpHierarchy->getParent() != mpHierarchy->getParent(),
                "vcl", "Window::SetZOrder() - pRefWindow has other parent");

    if (mpHierarchy && mpHierarchy->reorderRelativeTo(this, pRefWindow, nFlags, ImplIsOverlapWindow()))
        ImplInvalidateZOrderPeers();
}

static ZOrderFlags lcl_normalizeZOrderFlags(ZOrderFlags nFlags)
{
    if (nFlags & ZOrderFlags::First)
        return nFlags | ZOrderFlags::Before;

    if (nFlags & ZOrderFlags::Last)
        return nFlags | ZOrderFlags::Behind;

    return nFlags;
}

vcl::Window* Window::ImplResolveZOrderReference(vcl::Window* pRefWindow, ZOrderFlags& rFlags) const
{
    if (rFlags & (ZOrderFlags::First | ZOrderFlags::Last))
    {
        if (mpHierarchy)
            pRefWindow = mpHierarchy->getBoundarySibling(rFlags, ImplIsOverlapWindow());

        rFlags = lcl_normalizeZOrderFlags(rFlags);
   }

    return WindowHierarchy::findOutermostBorderWindow(pRefWindow);
}

void Window::ImplInvalidateZOrderPeers()
{
    if (!IsReallyVisible())
        return;

    if (!mpClippingState->mbInitWinClipRegion && mpClippingState->maWinClipRegion.IsEmpty())
        return;

    bool bInitWinClipRegion = mpClippingState->mbInitWinClipRegion;
    mpClippingState->setClipFlag(*this);

    if (bInitWinClipRegion || !mpHierarchy)
        return;

    const tools::Rectangle aWinRect = GetOutputRectPixel();
    bool bBehindThis = false;

    mpHierarchy->forEachSibling(ImplIsOverlapWindow(), [&](vcl::Window& rPeer) -> bool {
        if (&rPeer == this)
        {
            bBehindThis = true;
            return true; // Continue to windows behind us
        }

        const tools::Rectangle aCompRect = rPeer.GetOutputRectPixel();
        if (!aWinRect.Overlaps(aCompRect))
            return true;

        if (!bBehindThis)
        {
            // Invalidate windows in front of us that are covered by us
            rPeer.Invalidate(InvalidateFlags::Children | InvalidateFlags::NoTransparent);
            return true;
        }

        // Invalidate ourselves if covered by a window in the background and stop
        Invalidate(InvalidateFlags::Children | InvalidateFlags::NoTransparent);
        return false;
    });
}

void Window::EnableAlwaysOnTop(bool bEnable)
{
    mpClassification->mbAlwaysOnTop = bEnable;

    if (mpHierarchy->hasBorderWindow())
        mpHierarchy->getBorderWindow()->EnableAlwaysOnTop(bEnable);
    else if (bEnable && IsReallyVisible())
        ToTop();

    if (mpClassification->mbFrame)
        mpPlatformState->mpFrame->SetAlwaysOnTop(bEnable);
}

bool Window::IsTopWindow() const
{
    if (!mpClassification || mpClassification->mbInDispose)
        return false;

    if (!mpClassification->mbFrame && (!mpHierarchy->hasBorderWindow() || !mpHierarchy->getBorderWindow()->mpClassification->mbFrame))
        return false;

    ImplGetWinData();
    if (mpWinData->mnIsTopWindow == sal_uInt16(~0))
    {
        vcl::Window* pThisWin = const_cast<vcl::Window*>(this);
        css::uno::Reference<css::awt::XTopWindow> xTopWindow(pThisWin->GetComponentInterface(), css::uno::UNO_QUERY);
        pThisWin->mpWinData->mnIsTopWindow = xTopWindow.is() ? 1 : 0;
    }

    return mpWinData->mnIsTopWindow == 1;
}

vcl::Window* Window::ImplFindOverlapWindow(const Point& rFramePos)
{
    vcl::Window* pFound = nullptr;
    if (vcl::Window* pFirstOverlap = mpHierarchy ? mpHierarchy->getFirstOverlap() : nullptr)
    {
        WindowHierarchy::forEachSibling(pFirstOverlap, [&](vcl::Window& rOverlap) -> bool {
            pFound = rOverlap.ImplFindWindow(rFramePos);
            return pFound == nullptr;
        });
    }
    return pFound;
}

vcl::Window* Window::ImplFindWindow(const Point& rFramePos)
{
    // First check all overlapping windows
    if (vcl::Window* pFindWindow = ImplFindOverlapWindow(rFramePos))
        return pFindWindow;

    // Check visibility and hit test for this window
    if (!mpVisibilityState->mbVisible)
        return nullptr;

    WindowHitTest nHitTest = ImplHitTest(rFramePos);
    if (!(nHitTest & WindowHitTest::Inside))
        return nullptr;

    // Check all child windows
    if (vcl::Window* pFindWindow = ImplFindChildWindow(rFramePos))
        return pFindWindow;

    if (nHitTest & WindowHitTest::Transparent)
        return nullptr;

    return this;
}

vcl::Window* Window::ImplFindChildWindow(const Point& rFramePos)
{
    vcl::Window* pFound = nullptr;
    if (vcl::Window* pFirstChild = mpHierarchy ? mpHierarchy->getFirstChild() : nullptr)
    {
        WindowHierarchy::forEachSibling(pFirstChild, [&](vcl::Window& rChild) -> bool {
            pFound = rChild.ImplFindWindow(rFramePos);
            return pFound == nullptr;
        });
    }
    return pFound;
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
    ImplClearReallyVisibleState();
    ImplResetOverlapWindowsReallyVisible();
    ImplResetChildWindowsReallyVisible();
}

void Window::ImplClearReallyVisibleState()
{
    bool bBecameReallyInvisible = mpVisibilityState->mbReallyVisible;

    GetOutDev()->mbDevOutput = false;
    mpVisibilityState->mbReallyVisible = false;
    mpVisibilityState->mbReallyShown = false;

    // The SHOW/HIDE events serve as indicators to send child creation/destroy events to the access bridge.
    // For this, the data member of the event must not be NULL.
    // Previously, we did this in Window::Show, but there some events got lost in certain situations.
    if (bBecameReallyInvisible && ImplIsAccessibleCandidate())
        CallEventListeners(VclEventId::WindowHide, this);
}

void Window::ImplResetOverlapWindowsReallyVisible()
{
    if (vcl::Window* pFirstOverlap = mpHierarchy ? mpHierarchy->getFirstOverlap() : nullptr)
    {
        WindowHierarchy::forEachSibling(pFirstOverlap, [](vcl::Window& rOverlap) {
            if (rOverlap.mpVisibilityState && rOverlap.mpVisibilityState->mbReallyVisible)
                rOverlap.ImplResetReallyVisible();
        });
    }
}

void Window::ImplResetChildWindowsReallyVisible()
{
    if (vcl::Window* pFirstChild = mpHierarchy ? mpHierarchy->getFirstChild() : nullptr)
    {
        WindowHierarchy::forEachSibling(pFirstChild, [](vcl::Window& rChild) {
            if (rChild.mpVisibilityState && rChild.mpVisibilityState->mbReallyVisible)
                rChild.ImplResetReallyVisible();
        });
    }
}

void Window::ImplUpdateWindowPtr(vcl::Window* pWindow)
{
    if (mpHierarchy->getFrameWindow() != pWindow->mpHierarchy->getFrameWindow())
    {
        OutputDevice* pOutDev = GetOutDev();
        pOutDev->ReleaseGraphics();
    }

    mpPlatformState->mpFrameData = pWindow->mpPlatformState->mpFrameData;

    if (mpPlatformState->mpFrame != pWindow->mpPlatformState->mpFrame)
    {
        mpPlatformState->mpFrame = pWindow->mpPlatformState->mpFrame;

        if (mpPlatformState->mpSysObj)
            mpPlatformState->mpSysObj->Reparent(mpPlatformState->mpFrame);
    }

    mpHierarchy->setFrameWindow(pWindow->mpHierarchy->getFrameWindow());

    if (pWindow->ImplIsOverlapWindow())
        mpHierarchy->setOverlapWindow(pWindow);
    else
        mpHierarchy->setOverlapWindow(pWindow->mpHierarchy->getOverlapWindow());

    if (vcl::Window* pFirstChild = mpHierarchy ? mpHierarchy->getFirstChild() : nullptr)
    {
        WindowHierarchy::forEachSibling(pFirstChild, [pWindow](vcl::Window& rChild) {
            rChild.ImplUpdateWindowPtr(pWindow);
        });
    }
}

void Window::ImplUpdateWindowPtr()
{
    if (vcl::Window* pFirstChild = mpHierarchy ? mpHierarchy->getFirstChild() : nullptr)
    {
        WindowHierarchy::forEachSibling(pFirstChild, [this](vcl::Window& rChild) {
            rChild.ImplUpdateWindowPtr(this);
        });
    }
}

void Window::ImplUpdateOverlapWindowPtr(bool bNewFrame)
{
    bool bVisible = IsVisible();
    Show(false);
    ImplRemoveWindow(bNewFrame);
    vcl::Window* pRealParent = mpHierarchy->getRealParent();
    ImplInsertWindow(ImplGetParent());
    mpHierarchy->setRealParent(pRealParent);
    ImplUpdateWindowPtr();

    if (ImplUpdatePos())
        ImplUpdateNativeObjectPos();

    if (!bNewFrame)
        return;

    if (vcl::Window* pFirstOverlap = mpHierarchy ? mpHierarchy->getFirstOverlap() : nullptr)
    {
        WindowHierarchy::forEachSibling(pFirstOverlap, [bNewFrame](vcl::Window& rOverlap) {
            rOverlap.ImplUpdateOverlapWindowPtr(bNewFrame);
        });
    }

    if (bVisible)
        Show();
}

SystemWindow* Window::GetSystemWindow() const
{
    const vcl::Window* pWin = this;
    while ( pWin && !pWin->IsSystemWindow() )
    {
        pWin  = pWin->GetParent();
    }

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
    return mpHierarchy ? mpHierarchy->getRealParent() : nullptr;
}

void Window::SetParent(vcl::Window* pNewParent)
{
    SAL_WARN_IF(!pNewParent, "vcl", "Window::SetParent(): pParent == NULL");
    SAL_WARN_IF(pNewParent == this, "vcl", "someone tried to reparent a window to itself");

    if (!pNewParent || pNewParent == this)
        return;

    if (!mpClassification)
    {
        SAL_WARN("vcl", "Window::SetParent(): mpClassification == NULL");
        return;
    }

    SystemWindow* pSysWin = ImplGetLastSystemWindow(this);
    SystemWindow* pNewSysWin = nullptr;
    bool bChangeTaskPaneList = false;

    if (pSysWin && pSysWin->ImplIsInTaskPaneList(this))
    {
        pNewSysWin = ImplGetLastSystemWindow(pNewParent);

        if (pNewSysWin && pNewSysWin != pSysWin)
        {
            bChangeTaskPaneList = true;
            pSysWin->GetTaskPaneList()->RemoveWindow(this);
        }
    }

    if ((GetStyle() & WB_OWNERDRAWDECORATION) && mpClassification->mbFrame)
    {
        std::vector<VclPtr<vcl::Window>>& rList = ImplGetOwnerDrawList();
        auto p = std::find(rList.begin(), rList.end(), VclPtr<vcl::Window>(this));

        if (p != rList.end())
            rList.erase(p);
    }

    ImplSetFrameParent(pNewParent);

    if (mpHierarchy->hasBorderWindow())
    {
        mpHierarchy->setRealParent(pNewParent);
        mpHierarchy->getBorderWindow()->SetParent(pNewParent);
        return;
    }

    if (mpHierarchy->getParent() == pNewParent)
        return;

    if (mpClassification->mbFrame)
        mpPlatformState->mpFrame->SetParent(pNewParent->mpPlatformState->mpFrame);

    bool bVisible = IsVisible();
    Show(false, ShowFlags::NoFocusChange);

    vcl::Window* pOldOverlapWindow = nullptr;
    vcl::Window* pNewOverlapWindow = nullptr;

    if (!ImplIsOverlapWindow())
    {
        pNewOverlapWindow = pNewParent->ImplGetFirstOverlapWindow();

        if (mpHierarchy->getOverlapWindow() != pNewOverlapWindow)
            pOldOverlapWindow = mpHierarchy->getOverlapWindow();
    }

    bool bFocusOverlapWin = HasChildPathFocus(true);
    bool bFocusWin = HasChildPathFocus();
    bool bNewFrame = pNewParent->mpHierarchy->getFrameWindow() != mpHierarchy->getFrameWindow();

    if (bNewFrame)
        ImplClearFrameTracking();

    ImplRemoveWindow(bNewFrame);
    ImplInsertWindow(pNewParent);

    if (mpClippingState->meParentClipMode & ParentClipMode::Clip)
        pNewParent->mpClippingState->mbClipChildren = true;

    ImplUpdateWindowPtr();
    if (ImplUpdatePos())
        ImplUpdateNativeObjectPos();

    if (ImplIsOverlapWindow())
        ImplUpdateChildOverlapWindows(bNewFrame);
    else if (pOldOverlapWindow)
        ImplUpdateFormerOverlapWindow(pOldOverlapWindow, pNewOverlapWindow, bFocusWin, bNewFrame);

    ImplUpdateFrameActivation(bNewFrame, bFocusOverlapWin);

    if (bChangeTaskPaneList)
        pNewSysWin->GetTaskPaneList()->AddWindow(this);

    if ((GetStyle() & WB_OWNERDRAWDECORATION) && mpClassification->mbFrame)
        ImplGetOwnerDrawList().emplace_back(this);

    if (bVisible)
        Show(true, ShowFlags::NoFocusChange | ShowFlags::NoActivate);
}

void Window::ImplClearFrameTracking()
{
    if (mpPlatformState->mpFrameData->mpFocusWin && IsWindowOrChild(mpPlatformState->mpFrameData->mpFocusWin))
        mpPlatformState->mpFrameData->mpFocusWin = nullptr;

    if (mpPlatformState->mpFrameData->mpMouseMoveWin && IsWindowOrChild(mpPlatformState->mpFrameData->mpMouseMoveWin))
        mpPlatformState->mpFrameData->mpMouseMoveWin = nullptr;

    if (mpPlatformState->mpFrameData->mpMouseDownWin && IsWindowOrChild(mpPlatformState->mpFrameData->mpMouseDownWin))
        mpPlatformState->mpFrameData->mpMouseDownWin = nullptr;
}

void Window::ImplUpdateChildOverlapWindows(bool bNewFrame)
{
    if (!bNewFrame)
        return;

    if (vcl::Window* pFirstOverlap = mpHierarchy ? mpHierarchy->getFirstOverlap() : nullptr)
    {
        WindowHierarchy::forEachSibling(pFirstOverlap, [bNewFrame](vcl::Window& rOverlap) {
            rOverlap.ImplUpdateOverlapWindowPtr(bNewFrame);
        });
    }
}

void Window::ImplUpdateFormerOverlapWindow(vcl::Window* pOldOverlapWindow,
                                          vcl::Window* pNewOverlapWindow,
                                          bool bFocusWin,
                                          bool bNewFrame)
{
    vcl::Window* pLastFocus = pOldOverlapWindow->mpInput->getLastFocusWindow();
    if (bFocusWin || (pLastFocus && IsWindowOrChild(pLastFocus)))
        pOldOverlapWindow->mpInput->clearLastFocusWindow();

    if (vcl::Window* pFirstOverlap = pOldOverlapWindow->mpHierarchy ? pOldOverlapWindow->mpHierarchy->getFirstOverlap() : nullptr)
    {
        WindowHierarchy::forEachSibling(pFirstOverlap, [this, bNewFrame](vcl::Window& rOverlap) {
            if (IsAncestorOf(*rOverlap.ImplGetWindow()))
                rOverlap.ImplUpdateOverlapWindowPtr(bNewFrame);
        });
    }

    if (HasChildPathFocus(true))
        ImplCallFocusChangeActivate(pNewOverlapWindow, pOldOverlapWindow);
}

void Window::ImplUpdateFrameActivation(bool bNewFrame, bool bFocusOverlapWin)
{
    if (bNewFrame)
    {
        if (GetType() == WindowType::BORDERWINDOW &&
            ImplGetWindow()->GetType() == WindowType::FLOATINGWINDOW)
        {
            static_cast<ImplBorderWindow*>(this)->SetDisplayActive(mpPlatformState->mpFrameData->mbHasFocus);
        }
    }

    if (bFocusOverlapWin)
    {
        mpPlatformState->mpFrameData->mpFocusWin = Application::GetFocusWindow();

        if (!mpPlatformState->mpFrameData->mbHasFocus)
            mpPlatformState->mpFrame->ToTop(SalFrameToTop::NONE);
    }

    if (bNewFrame)
        GetDropTarget();
}

sal_uInt16 Window::GetChildCount() const
{
    if (!mpClassification || !mpHierarchy)
        return 0;

    sal_uInt16 nChildCount = 0;
    if (vcl::Window* pFirstChild = mpHierarchy->getFirstChild())
    {
        WindowHierarchy::forEachSibling(pFirstChild, [&](vcl::Window&) {
            ++nChildCount;
        });
    }

    return nChildCount;
}

vcl::Window* Window::GetChild(sal_uInt16 nChild) const
{
    if (!mpClassification || !mpHierarchy)
        return nullptr;

    sal_uInt16 nCurrent = 0;
    vcl::Window* pTarget = nullptr;
    if (vcl::Window* pFirstChild = mpHierarchy->getFirstChild())
    {
        WindowHierarchy::forEachSibling(pFirstChild, [&](vcl::Window& rChild) -> bool {
            if (nCurrent == nChild)
            {
                pTarget = &rChild;
                return false;
            }
            ++nCurrent;
            return true;
        });
    }

    return pTarget;
}

vcl::Window* Window::GetWindow(GetWindowType nType) const
{
    if (!mpClassification || !mpHierarchy)
        return nullptr;

    switch (nType)
    {
        case GetWindowType::Parent:
            return mpHierarchy->getRealParent();

        case GetWindowType::FirstChild:
            return mpHierarchy->getFirstChild();

        case GetWindowType::LastChild:
            return mpHierarchy->getLastChild();

        case GetWindowType::Prev:
            return mpHierarchy->getPrevSibling();

        case GetWindowType::Next:
            return mpHierarchy->getNextSibling();

        case GetWindowType::FirstOverlap:
            return mpHierarchy->getFirstOverlap();

        case GetWindowType::Overlap:
            if (ImplIsOverlapWindow())
                return const_cast<vcl::Window*>(this);
            return mpHierarchy->getOverlapWindow();

        case GetWindowType::ParentOverlap:
            if (ImplIsOverlapWindow())
                return mpHierarchy->getOverlapWindow();
            return (mpHierarchy->getOverlapWindow() && mpHierarchy->getOverlapWindow()->mpHierarchy)
                       ? mpHierarchy->getOverlapWindow()->mpHierarchy->getOverlapWindow()
                       : nullptr;

        case GetWindowType::Client:
            return this->ImplGetWindow();

        case GetWindowType::RealParent:
            return ImplGetParent();

        case GetWindowType::Frame:
            return mpHierarchy->getFrameWindow();

        case GetWindowType::Border:
            if (mpHierarchy->hasBorderWindow())
                return mpHierarchy->getBorderWindow()->GetWindow(GetWindowType::Border);
            return const_cast<vcl::Window*>(this);

        case GetWindowType::FirstTopWindowChild:
            return ImplGetWinData()->maTopWindowChildren.empty() ? nullptr : (*ImplGetWinData()->maTopWindowChildren.begin()).get();

        case GetWindowType::NextTopWindowSibling:
        {
            if (!mpHierarchy->getRealParent())
                return nullptr;

            const ::std::list<VclPtr<vcl::Window>>& rTopWindows(mpHierarchy->getRealParent()->ImplGetWinData()->maTopWindowChildren);
            auto myPos = std::find(rTopWindows.begin(), rTopWindows.end(), this);

            if ((myPos == rTopWindows.end()) || (++myPos == rTopWindows.end()))
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
    return mpHierarchy ? mpHierarchy->getParent() : nullptr;
}

bool Window::ImplIsOverlapWindow() const
{
    return mpClassification && mpClassification->mbOverlapWin;
}

vcl::Window* Window::ImplGetFirstOverlapWindow()
{
    if (!mpClassification)
        return nullptr;

    if (mpClassification->mbOverlapWin)
        return this;

    return mpHierarchy ? mpHierarchy->getOverlapWindow() : nullptr;
}

const vcl::Window* Window::ImplGetFirstOverlapWindow() const
{
    if (!mpClassification)
        return nullptr;

    if (mpClassification->mbOverlapWin)
        return this;

    return mpHierarchy ? mpHierarchy->getOverlapWindow() : nullptr;
}

} /* namespace vcl */

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
