
/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <vcl/window.hxx>

#include <window.h>
#include <clipping/ClipStateBuilder.hxx>

namespace vcl::clipping
{
ClipState ClipStateBuilder::BuildFromWindow(const vcl::Window& rWindow)
{
    ClipState aState;
    WindowImpl* pImpl = rWindow.ImplGetWindowImpl();

    aState.bIsVisible = rWindow.IsVisible();
    aState.bClipChildren = (rWindow.GetStyle() & WB_CLIPCHILDREN) != 0;

    aState.maBounds = rWindow.GetWindowExtentsRelative(rWindow);

    // Flatten the hierarchy
    CollectSiblings(rWindow, aState.maSiblings);
    CollectChildren(rWindow, aState.maChildren);

    // Extract custom region
    // The WindowImpl holds the ClippingState and hierarchy pointers
    if (pImpl->mpClippingState && pImpl->mpClippingState->mbWinRegion)
        aState.maCustomRegion = rWindow.GetWindowClipRegionPixel();

    return aState;
}

void ClipStateBuilder::CollectSiblings(const vcl::Window& rWindow, std::vector<ClipNode>& rOut)
{
    // Access the hierarchy pointer inside WindowImpl
    WindowImpl* pImpl = rWindow.ImplGetWindowImpl();
    if (!pImpl->mpHierarchy)
        return;

    vcl::Window* pSibling = pImpl->mpHierarchy->mpFirstOverlap;
    while (pSibling)
    {
        rOut.push_back({ pSibling->GetWindowExtentsRelative(rWindow) });
        // Correct way to navigate the overlap linked list
        pSibling = pSibling->ImplGetWindowImpl()->mpHierarchy->mpNextOverlap;
    }
}

void ClipStateBuilder::CollectChildren(const vcl::Window& rWindow, std::vector<ClipNode>& rOut)
{
    WindowImpl* pImpl = rWindow.ImplGetWindowImpl();
    if (!pImpl->mpHierarchy)
        return;

    vcl::Window* pChild = pImpl->mpHierarchy->mpFirstChild;
    while (pChild)
    {
        rOut.push_back({ pChild->GetWindowExtentsRelative(rWindow) });
        pChild = pChild->ImplGetWindowImpl()->mpHierarchy->mpNext;
    }
}
} // namespace vcl::clipping

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
