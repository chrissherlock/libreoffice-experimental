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

#include <window.h>
#include <clipping_window.hxx>
#include <salframe.hxx>
#include <svdata.hxx>

namespace vcl
{
bool Window::ImplShouldTransferFocusOnHide(ShowFlags nFlags) const
{
    const bool bIsOverlapWindowAvailable = !mpWindowImpl->mbFrame
                                           && mpWindowImpl->mpOverlapWindow->IsEnabled()
                                           && mpWindowImpl->mpOverlapWindow->IsInputEnabled()
                                           && !mpWindowImpl->mpOverlapWindow->IsInModalMode();

    const bool bCanYieldFocus
        = ImplIsOverlapWindow() && !(nFlags & ShowFlags::NoFocusChange) && HasChildPathFocus();

    return bCanYieldFocus && bIsOverlapWindowAvailable;
}

void Window::ImplExpandInvalidationForNativeWidget(vcl::Region& rInvRegion) const
{
    if (!mpWindowImpl->mpWinData || !mpWindowImpl->mpWinData->mbEnableNativeWidget)
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

    if (!mpWindowImpl->mbNoParentUpdate && !rInvRegion.IsEmpty())
        ImplInvalidateParentFrameRegion(rInvRegion);

    ImplGenerateMouseMove();
}

vcl::Region Window::ImplGetWinClipRegion()
{
    if (mpWindowImpl->mpClippingState->mbInitWinClipRegion)
        clipping::initWinClipRegion(*this);

    return mpWindowImpl->mpClippingState->maWinClipRegion;
}

std::optional<bool> Window::ImplHideWindow(ShowFlags nFlags)
{
    if (!mpWindowImpl->mbReallyVisible)
        return false;

    VclPtr<vcl::Window> xWindow(this);

    vcl::Region aInvRegion = ImplGetWinClipRegion();

    // initWinClipRegion can trigger re-entrant events or disposal
    if (!xWindow->mpWindowImpl)
        return std::nullopt;

    bool bRealVisibilityChanged = mpWindowImpl->mbReallyVisible;
    ImplResetReallyVisible();
    vcl::clipping::setClipFlag(*this);

    if (ImplShouldTransferFocusOnHide(nFlags))
        mpWindowImpl->mpOverlapWindow->GrabFocus();

    if (!mpWindowImpl->mbFrame)
        ImplInvalidateParentOnHide(aInvRegion);

    return bRealVisibilityChanged;
}

std::optional<bool> Window::ImplHideCascade(ShowFlags nFlags)
{
    VclPtr<vcl::Window> xWindow(this);

    ImplHideAllOverlaps();
    if (!xWindow->mpWindowImpl)
        return std::nullopt;

    if (mpWindowImpl->mpBorderWindow)
    {
        bool bOldUpdate = mpWindowImpl->mpBorderWindow->mpWindowImpl->mbNoParentUpdate;
        if (mpWindowImpl->mbNoParentUpdate)
            mpWindowImpl->mpBorderWindow->mpWindowImpl->mbNoParentUpdate = true;
        mpWindowImpl->mpBorderWindow->Show(false, nFlags);
        mpWindowImpl->mpBorderWindow->mpWindowImpl->mbNoParentUpdate = bOldUpdate;
    }
    else if (mpWindowImpl->mbFrame)
    {
        mpWindowImpl->mbSuppressAccessibilityEvents = true;
        mpWindowImpl->mpFrame->Show(false);
    }

    CompatStateChanged(StateChangedType::Visible);

    bool bRealVisibilityChanged = false;
    if (mpWindowImpl->mbReallyVisible)
    {
        std::optional<bool> bResult = ImplHideWindow(nFlags);
        if (!bResult)
            return std::nullopt;

        bRealVisibilityChanged = *bResult;
    }

    if (!xWindow->mpWindowImpl)
        return std::nullopt;

    return bRealVisibilityChanged;
}

bool Window::ImplIsMismatchedSubControl() const
{
    return mpWindowImpl->mbFrame && GetParent() && !GetParent()->isDisposed()
           && GetParent()->IsCompoundControl()
           && GetParent()->IsNativeWidgetEnabled() != IsNativeWidgetEnabled()
           && !(GetStyle() & WB_TOOLTIPWIN);
}

vcl::Window* Window::ImplGetVisibilityParent() const
{
    return ImplIsOverlapWindow() ? mpWindowImpl->mpOverlapWindow.get() : ImplGetParent();
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

void Window::Show(bool bVisible, ShowFlags nFlags)
{
    if (!mpWindowImpl || mpWindowImpl->mbVisible == bVisible)
        return;

    bool bRealVisibilityChanged = false;
    mpWindowImpl->mbVisible = bVisible;

    if (!bVisible)
    {
        std::optional<bool> bResult = ImplHideCascade(nFlags);
        if (!bResult)
            return;

        bRealVisibilityChanged = *bResult;
    }
    else
    {
        // inherit native widget flag for form controls
        // required here, because frames never show up in the child hierarchy - which should be fixed...
        // eg, the drop down of a combobox which is a system floating window
        if (ImplIsMismatchedSubControl())
            EnableNativeWidget(GetParent()->IsNativeWidgetEnabled());

        if (mpWindowImpl->mbCallMove)
            ImplCallMove();

        if (mpWindowImpl->mbCallResize)
            ImplCallResize();

        CompatStateChanged(StateChangedType::Visible);

        vcl::Window* pVisibilityParent = ImplGetVisibilityParent();

        if (mpWindowImpl->mbFrame || pVisibilityParent->mpWindowImpl->mbReallyVisible)
        {
            // if a window becomes visible, send all child windows a StateChange,
            // such that these can initialise themselves
            ImplCallInitShow();
            ImplRaiseOverlapWindow(nFlags);

            // adjust mpWindowImpl->mbReallyVisible
            bRealVisibilityChanged = !mpWindowImpl->mbReallyVisible;
            ImplSetReallyVisible();

            // assure clip rectangles will be recalculated
            vcl::clipping::setClipFlag(*this);

            if (!mpWindowImpl->mbFrame)
            {
                InvalidateFlags nInvalidateFlags = InvalidateFlags::Children;

                if (!IsPaintTransparent())
                    nInvalidateFlags |= InvalidateFlags::NoTransparent;

                ImplInvalidate(nullptr, nInvalidateFlags);
                ImplGenerateMouseMove();
            }
        }

        VclPtr<vcl::Window> xWindow(this);

        if (mpWindowImpl->mpBorderWindow)
        {
            mpWindowImpl->mpBorderWindow->Show(true, nFlags);
        }
        else if (mpWindowImpl->mbFrame)
        {
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

            //SAL_WARN_IF( mpWindowImpl->mbSuppressAccessibilityEvents, "vcl", "Window::Show() - Frame reactivated");
            mpWindowImpl->mbSuppressAccessibilityEvents = false;

            mpWindowImpl->mbPaintFrame = true;
            if (!Application::IsHeadlessModeEnabled())
            {
                bool bNoActivate(nFlags & (ShowFlags::NoActivate | ShowFlags::NoFocusChange));
                mpWindowImpl->mpFrame->Show(true, bNoActivate);
            }

            if (!xWindow->mpWindowImpl)
                return;

            // Query the correct size of the window, if we are waiting for
            // a system resize
            if (mpWindowImpl->mbWaitSystemResize)
            {
                const Size aOutSize = mpWindowImpl->mpFrame->GetClientSize();
                ImplHandleResize(this, aOutSize.Width(), aOutSize.Height());
            }

            if (mpWindowImpl->mpFrameData->mpBuffer
                && mpWindowImpl->mpFrameData->mpBuffer->GetOutputSizePixel()
                       != GetOutputSizePixel())
                // Make sure that the buffer size matches the window size, even if no resize was needed.
                mpWindowImpl->mpFrameData->mpBuffer->SetOutputSizePixel(GetOutputSizePixel());
        }

        if (!xWindow->mpWindowImpl)
            return;

        ImplShowAllOverlaps();
    }

    // the SHOW/HIDE events also serve as indicators to send child creation/destroy events to the access bridge
    // However, the access bridge only uses this event if the data member is not NULL (it's kind of a hack that
    // we re-use the SHOW/HIDE events this way, with this particular semantics).
    // Since #104887#, the notifications for the access bridge are done in Impl(Set|Reset)ReallyVisible. Here, we
    // now only notify with a NULL data pointer, for all other clients except the access bridge.
    if (!bRealVisibilityChanged)
        CallEventListeners(mpWindowImpl->mbVisible ? VclEventId::WindowShow
                                                   : VclEventId::WindowHide);
}

void Window::ImplSetReallyVisible()
{
    // #i43594# it is possible that INITSHOW was never send, because the visibility state changed between
    // ImplCallInitShow() and ImplSetReallyVisible() when called from Show()
    // mbReallyShown is a useful indicator
    if (!mpWindowImpl->mbReallyShown)
        ImplCallInitShow();

    bool bBecameReallyVisible = !mpWindowImpl->mbReallyVisible;

    GetOutDev()->mbDevOutput = true;
    mpWindowImpl->mbReallyVisible = true;
    mpWindowImpl->mbReallyShown = true;

    // the SHOW/HIDE events serve as indicators to send child creation/destroy events to the access bridge.
    // For this, the data member of the event must not be NULL.
    // Previously, we did this in Window::Show, but there some events got lost in certain situations. Now
    // we're doing it when the visibility really changes
    if (bBecameReallyVisible && ImplIsAccessibleCandidate())
        CallEventListeners(VclEventId::WindowShow, this);
    // TODO. It's kind of a hack that we're re-using the VclEventId::WindowShow. Normally, we should
    // introduce another event which explicitly triggers the Accessibility implementations.

    vcl::Window* pWindow = mpWindowImpl->mpHierarchy->mpFirstOverlap;
    while (pWindow)
    {
        if (pWindow->mpWindowImpl->mbVisible)
            pWindow->ImplSetReallyVisible();
        pWindow = pWindow->mpWindowImpl->mpHierarchy->mpNext;
    }

    pWindow = mpWindowImpl->mpHierarchy->mpFirstChild;
    while (pWindow)
    {
        if (pWindow->mpWindowImpl->mbVisible)
            pWindow->ImplSetReallyVisible();
        pWindow = pWindow->mpWindowImpl->mpHierarchy->mpNext;
    }
}

} // end vcl namespace

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
