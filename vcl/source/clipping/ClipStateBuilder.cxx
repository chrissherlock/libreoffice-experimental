/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <vcl/window.hxx>

#include <window/state/WindowClippingState.hxx>
#include <window/state/WindowClassification.hxx>
#include <window/state/WindowHierarchy.hxx>
#include <clipping/ClipStateBuilder.hxx>

namespace vcl::clipping
{
ClipState ClipStateBuilder::BuildFromWindow(const vcl::Window& rWindow)
{
    ClipState aState;
    WindowClippingState* pImpl = rWindow.ImplGetClippingState();

    aState.bIsVisible = rWindow.IsVisible();
    aState.bClipChildren = (rWindow.GetStyle() & WB_CLIPCHILDREN) != 0;

    aState.maBounds = rWindow.GetWindowExtentsRelative(rWindow);

    // Flatten the hierarchy
    CollectSiblings(rWindow, aState.maSiblings);
    CollectChildren(rWindow, aState.maChildren);

    // Extract custom region
    // The WindowImpl holds the ClippingState and hierarchy pointers
    if (pImpl && pImpl->mbWinRegion)
        aState.maCustomRegion = rWindow.GetWindowClipRegionPixel();

    return aState;
}

void ClipStateBuilder::CollectSiblings(const vcl::Window& rWindow, std::vector<ClipNode>& rOut)
{
    const WindowHierarchy* pHierarchy = rWindow.ImplGetWindowHierarchy();
    if (!pHierarchy)
        return;

    pHierarchy->forEachSibling(rWindow.ImplIsOverlapWindow(), [&](const vcl::Window& rSibling) {
        if (&rSibling != &rWindow)
            rOut.emplace_back(rSibling.GetWindowExtentsRelative(rWindow));
    });
}

void ClipStateBuilder::CollectChildren(const vcl::Window& rWindow, std::vector<ClipNode>& rOut)
{
    const WindowHierarchy* pHierarchy = rWindow.ImplGetWindowHierarchy();
    if (!pHierarchy)
        return;

    WindowHierarchy::forEachSibling(pHierarchy->getFirstChild(), [&](const vcl::Window& rChild) {
        rOut.emplace_back(rChild.GetWindowExtentsRelative(rWindow));
    });
}
} // namespace vcl::clipping

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
