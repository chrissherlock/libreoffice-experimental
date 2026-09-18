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

#include <vcl/window.hxx>

#include <window/window.h>
#include <window/state/ImplFrameData.hxx>
#include <window/state/ImplWinData.hxx>
#include <window/state/WindowClassification.hxx>
#include <window/state/WindowPlatformState.hxx>
#include <window/state/WindowVisibilityState.hxx>
#include <window/state/WindowClippingState.hxx>
#include <window/state/WindowHierarchy.hxx>
#include <window/state/WindowA11y.hxx>
#include <window/state/WindowInvalidation.hxx>
#include <window/state/WindowGeometry.hxx>
#include <salframe.hxx>
#include <svdata.hxx>

namespace vcl
{
bool Window::ImplShouldTransferFocusOnHide(ShowFlags nFlags) const
{
    const vcl::Window* pOverlapWindow = mpHierarchy ? mpHierarchy->getOverlapWindow() : nullptr;

    const bool bIsOverlapWindowAvailable = mpClassification && !mpClassification->mbFrame
                                           && pOverlapWindow && pOverlapWindow->IsEnabled()
                                           && pOverlapWindow->IsInputEnabled()
                                           && !pOverlapWindow->IsInModalMode();

    const bool bCanYieldFocus
        = ImplIsOverlapWindow() && !(nFlags & ShowFlags::NoFocusChange) && HasChildPathFocus();

    return bCanYieldFocus && bIsOverlapWindowAvailable;
}

void Window::ImplExpandInvalidationForNativeWidget(vcl::Region& rInvRegion) const
{
    if (!mpWinData || !mpWinData->mbEnableNativeWidget)
        return;

    /*
     * #i48371# native theming: some themes draw outside the control
     * area we tell them to (bad thing, but we cannot do much about it ).
     * On hiding these controls they get invalidated with their window rectangle
     * which leads to the parts outside the control area being left and not
     * invalidated. Workaround: invalidate an area on the parent, too
     */
    const int workaround_border = 5;
    tools::Rectangle aBounds(rInvRegion.GetBoundRect());
    aBounds.AdjustLeft(-workaround_border);
    aBounds.AdjustTop(-workaround_border);
    aBounds.AdjustRight(workaround_border);
    aBounds.AdjustBottom(workaround_border);
    rInvRegion = aBounds;
}

void Window::ImplInvalidateParentOnHide(vcl::Region& rInvRegion)
{
    ImplExpandInvalidationForNativeWidget(rInvRegion);

    if (!mpInvalidation->isParentUpdateSuppressed() && !rInvRegion.IsEmpty())
        ImplInvalidateParentFrameRegion(rInvRegion);

    ImplGenerateMouseMove();
}

vcl::Region Window::ImplGetWinClipRegion()
{
    if (mpClippingState->mbInitWinClipRegion)
        mpClippingState->initWinClipRegion(*this);

    return mpClippingState->maWinClipRegion;
}

std::optional<bool> Window::ImplHideWindow(ShowFlags nFlags)
{
    if (!mpVisibilityState || !mpVisibilityState->mbReallyVisible)
        return false;

    VclPtr<vcl::Window> xWindow(this);

    vcl::Region aInvRegion = ImplGetWinClipRegion();

    // initWinClipRegion can trigger re-entrant events or disposal
    if (!xWindow->mpClassification)
        return std::nullopt;

    const bool bRealVisibilityChanged = mpVisibilityState && mpVisibilityState->mbReallyVisible;
    ImplResetReallyVisible();

    if (mpClippingState)
        mpClippingState->setClipFlag(*this, /*bSysObjOnlySmaller=*/false);

    if (ImplShouldTransferFocusOnHide(nFlags))
    {
        if (vcl::Window* pOverlapWindow = mpHierarchy ? mpHierarchy->getOverlapWindow() : nullptr)
            pOverlapWindow->GrabFocus();
    }

    if (!mpClassification->mbFrame)
        ImplInvalidateParentOnHide(aInvRegion);

    return bRealVisibilityChanged;
}

std::optional<bool> Window::ImplHideCascade(ShowFlags nFlags)
{
    VclPtr<vcl::Window> xWindow(this);

    ImplHideAllOverlaps();
    if (!xWindow->mpClassification)
        return std::nullopt;

    vcl::Window* pBorderWin = mpHierarchy ? mpHierarchy->getBorderWindow() : nullptr;
    if (pBorderWin)
    {
        const bool bOldUpdate = pBorderWin->mpInvalidation
                                    ? pBorderWin->mpInvalidation->isParentUpdateSuppressed()
                                    : false;

        if (mpInvalidation && mpInvalidation->isParentUpdateSuppressed()
            && pBorderWin->mpInvalidation)
            pBorderWin->mpInvalidation->suppressParentUpdates(true);

        pBorderWin->Show(false, nFlags);

        if (pBorderWin->mpInvalidation)
            pBorderWin->mpInvalidation->suppressParentUpdates(bOldUpdate);
    }
    else if (mpClassification && mpClassification->mbFrame)
    {
        if (mpAccessibleData)
            mpAccessibleData->suspendEvents();

        if (mpPlatformState && mpPlatformState->mpFrame)
            mpPlatformState->mpFrame->Show(false);
    }

    CompatStateChanged(StateChangedType::Visible);

    bool bRealVisibilityChanged = false;
    if (mpVisibilityState && mpVisibilityState->mbReallyVisible)
    {
        std::optional<bool> bResult = ImplHideWindow(nFlags);
        if (!bResult)
            return std::nullopt;

        bRealVisibilityChanged = *bResult;
    }

    if (!xWindow->mpClassification)
        return std::nullopt;

    return bRealVisibilityChanged;
}

bool Window::ImplIsMismatchedSubControl() const
{
    return mpClassification->mbFrame && GetParent() && !GetParent()->isDisposed()
           && GetParent()->IsCompoundControl()
           && GetParent()->IsNativeWidgetEnabled() != IsNativeWidgetEnabled()
           && !(GetStyle() & WB_TOOLTIPWIN);
}

vcl::Window* Window::ImplGetVisibilityParent() const
{
    if (ImplIsOverlapWindow())
        return mpHierarchy ? mpHierarchy->getOverlapWindow() : nullptr;

    return ImplGetParent();
}

void Window::ImplRaiseOverlapWindow(ShowFlags nFlags)
{
    // If it is a SystemWindow it automatically pops up on top of
    // all other windows if needed.
    if (!ImplIsOverlapWindow() || (nFlags & ShowFlags::NoActivate))
        return;

    ToTopFlags nToTopFlags
        = (nFlags & ShowFlags::ForegroundTask) ? ToTopFlags::ForegroundTask : ToTopFlags::NONE;
    ImplStartToTop(nToTopFlags);
    ImplFocusToTop(ToTopFlags::NONE, false);

    if (!(nFlags & ShowFlags::ForegroundTask))
        FlashWindow(); // Inform user about window if we did not popup it at foreground
}

bool Window::ImplUpdateRealVisibility(ShowFlags nFlags)
{
    if (!mpClassification || !mpVisibilityState)
        return false;

    vcl::Window* pVisibilityParent = ImplGetVisibilityParent();

    // If it's not a frame and the parent isn't actually on screen,
    // we don't need to do any real visibility rendering yet.
    if (!mpClassification->mbFrame
        && (!pVisibilityParent || !pVisibilityParent->mpVisibilityState
            || !pVisibilityParent->mpVisibilityState->mbReallyVisible))
    {
        return false;
    }

    // If a window becomes visible, send all child windows a StateChange,
    // such that these can initialise themselves
    ImplCallInitShow();
    ImplRaiseOverlapWindow(nFlags);

    // Adjust mpVisibilityState->mbReallyVisible
    const bool bRealVisibilityChanged = !mpVisibilityState->mbReallyVisible;
    ImplSetReallyVisible();

    // Assure clip rectangles will be recalculated
    if (mpClippingState)
        mpClippingState->setClipFlag(*this, /*bSysObjOnlySmaller=*/false);

    if (!mpClassification->mbFrame)
    {
        InvalidateFlags nInvalidateFlags = InvalidateFlags::Children;

        if (!IsPaintTransparent())
            nInvalidateFlags |= InvalidateFlags::NoTransparent;

        ImplInvalidate(nullptr, nInvalidateFlags);
        ImplGenerateMouseMove();
    }

    return bRealVisibilityChanged;
}

bool Window::ImplShowBorderOrFrame(ShowFlags nFlags)
{
    vcl::Window* pBorderWindow = mpHierarchy ? mpHierarchy->getBorderWindow() : nullptr;
    if (pBorderWindow)
    {
        pBorderWindow->Show(true, nFlags);
        return true;
    }

    if (!mpClassification || !mpClassification->mbFrame)
        return true;

    // #106431#, hide SplashScreen
    ImplSVData* pSVData = ImplGetSVData();
    if (!pSVData->mpIntroWindow)
    {
        // The right way would be just to call this (not even in the 'if')
        auto pApp = GetpApp();
        if (pApp)
            pApp->InitFinished();
    }
    else if (!ImplIsWindowOrChild(pSVData->mpIntroWindow))
    {
        // ... but the VCL splash is broken, and it needs this
        // (for ./soffice .uno:NewDoc)
        pSVData->mpIntroWindow->Hide();
    }

    if (mpAccessibleData)
        mpAccessibleData->resumeEvents();

    if (mpInvalidation)
        mpInvalidation->setPaintFrame(true);

    VclPtr<vcl::Window> xWindow(this);

    if (!Application::IsHeadlessModeEnabled() && mpPlatformState && mpPlatformState->mpFrame)
    {
        bool bNoActivate(nFlags & (ShowFlags::NoActivate | ShowFlags::NoFocusChange));
        mpPlatformState->mpFrame->Show(true, bNoActivate);
    }

    // Check if the window was destroyed during the system Show() call
    if (!xWindow->mpClassification)
        return false;

    // Query the correct size of the window, if we are waiting for
    // a system resize
    if (mpGeometry && mpGeometry->mbWaitSystemResize && mpPlatformState && mpPlatformState->mpFrame)
    {
        const Size aOutSize = mpPlatformState->mpFrame->GetClientSize();
        ImplHandleResize(this, aOutSize.Width(), aOutSize.Height());
    }

    if (mpPlatformState && mpPlatformState->mpFrameData && mpPlatformState->mpFrameData->mpBuffer
        && mpPlatformState->mpFrameData->mpBuffer->GetOutputSizePixel() != GetOutputSizePixel())
    {
        // Make sure that the buffer size matches the window size, even if no resize was needed.
        mpPlatformState->mpFrameData->mpBuffer->SetOutputSizePixel(GetOutputSizePixel());
    }

    return true; // Window is still alive
}

std::optional<bool> Window::ImplShowWindow(ShowFlags nFlags)
{
    // inherit native widget flag for form controls
    if (ImplIsMismatchedSubControl())
        EnableNativeWidget(GetParent()->IsNativeWidgetEnabled());

    if (mpGeometry->mbCallMove)
        ImplCallMove();

    if (mpGeometry->mbCallResize)
        ImplCallResize();

    CompatStateChanged(StateChangedType::Visible);

    bool bRealVisibilityChanged = ImplUpdateRealVisibility(nFlags);

    if (!ImplShowBorderOrFrame(nFlags))
        return std::nullopt; // Window was destroyed

    ImplShowAllOverlaps();

    return bRealVisibilityChanged;
}

void Window::Show(bool bVisible, ShowFlags nFlags)
{
    if (!mpClassification || mpVisibilityState->mbVisible == bVisible)
        return;

    mpVisibilityState->mbVisible = bVisible;

    // Dispatch to the appropriate symmetric handler
    std::optional<bool> oRealVisChanged
        = bVisible ? ImplShowWindow(nFlags) : ImplHideCascade(nFlags);

    if (!oRealVisChanged)
        return; // Window was destroyed during the operation

    // Notify listeners if the real visibility didn't actually change.
    // Note: Per #104887#, we only notify with a NULL data pointer here for
    // standard clients. Accessibility bridge notifications happen in ImplSetReallyVisible.
    if (!*oRealVisChanged)
        CallEventListeners(bVisible ? VclEventId::WindowShow : VclEventId::WindowHide);
}

void Window::ImplSetReallyVisible()
{
    // #i43594# it is possible that INITSHOW was never send, because the visibility state changed between
    // ImplCallInitShow() and ImplSetReallyVisible() when called from Show()
    // mbReallyShown is a useful indicator
    if (mpVisibilityState && !mpVisibilityState->mbReallyShown)
        ImplCallInitShow();

    bool bBecameReallyVisible = mpVisibilityState ? !mpVisibilityState->mbReallyVisible : false;

    if (GetOutDev())
        GetOutDev()->mbDevOutput = true;

    if (mpVisibilityState)
    {
        mpVisibilityState->mbReallyVisible = true;
        mpVisibilityState->mbReallyShown = true;
    }

    // the SHOW/HIDE events serve as indicators to send child creation/destroy events to the access bridge.
    // For this, the data member of the event must not be NULL.
    // Previously, we did this in Window::Show, but there some events got lost in certain situations. Now
    // we're doing it when the visibility really changes
    if (bBecameReallyVisible && ImplIsAccessibleCandidate())
        CallEventListeners(VclEventId::WindowShow, this);
    // TODO. It's kind of a hack that we're re-using the VclEventId::WindowShow. Normally, we should
    // introduce another event which explicitly triggers the Accessibility implementations.

    // Helper to propagate visibility down the intrusive linked lists
    auto propagateToVisible = [](vcl::Window* pStart) {
        for (vcl::Window* pWin = pStart; pWin;)
        {
            if (pWin->mpVisibilityState && pWin->mpVisibilityState->mbVisible)
                pWin->ImplSetReallyVisible();

            const WindowHierarchy* pWinHierarchy = pWin->ImplGetWindowHierarchy();
            pWin = pWinHierarchy ? pWinHierarchy->getNextSibling() : nullptr;
        }
    };

    if (mpHierarchy)
    {
        propagateToVisible(mpHierarchy->getFirstOverlap());
        propagateToVisible(mpHierarchy->getFirstChild());
    }
}

} // end vcl namespace

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
