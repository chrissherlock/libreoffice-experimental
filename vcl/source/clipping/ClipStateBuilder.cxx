/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <vcl/window.hxx>
#include <vcl/outdev.hxx>

#include <clipping_window.hxx>
#include <clipping/ClipStateBuilder.hxx>
#include <window.h>

namespace vcl::clipping
{
ClipState ClipStateBuilder::BuildFromWindow(const vcl::Window& rWindow)
{
    ClipState aState;
    WindowImpl* pImpl = rWindow.ImplGetWindowImpl();

    if (pImpl->mpClippingState->mbInitWinClipRegion)
        vcl::clipping::initWinClipRegion(const_cast<vcl::Window&>(rWindow));

    if (pImpl->mpClippingState->mbInitChildRegion)
        vcl::clipping::initWinChildClipRegion(const_cast<vcl::Window&>(rWindow));

    aState.bIsVisible = rWindow.IsVisible();

    aState.bClipChildren = (rWindow.GetStyle() & WB_CLIPCHILDREN) != 0
                           || (pImpl->mpClippingState && pImpl->mpClippingState->mbClipChildren);

    aState.bClipSiblings = pImpl->mpClippingState && pImpl->mpClippingState->mbClipSiblings;

    // Map bounds into Frame/Device coordinates
    const OutputDevice* pOutDev = rWindow.GetOutDev();
    aState.maBounds
        = tools::Rectangle(Point(pOutDev->GetDeviceOriginX(), pOutDev->GetDeviceOriginY()),
                           rWindow.GetOutputSizePixel());

    // Intersect with parent bounds to prevent phantom out-of-bounds painting
    if (pImpl->mpHierarchy && pImpl->mpHierarchy->mpParent)
    {
        const OutputDevice* pParentOutDev = pImpl->mpHierarchy->mpParent->GetOutDev();
        tools::Rectangle aParentBounds(
            Point(pParentOutDev->GetDeviceOriginX(), pParentOutDev->GetDeviceOriginY()),
            pImpl->mpHierarchy->mpParent->GetOutputSizePixel());
        aState.maBounds.Intersection(aParentBounds);
    }

    // Flatten the hierarchy
    CollectSiblings(rWindow, aState.maSiblings);
    CollectChildren(rWindow, aState.maChildren);

    // Extract custom region
    if (pImpl->mpClippingState && pImpl->mpClippingState->mbWinRegion)
        aState.maCustomRegion = rWindow.GetWindowClipRegionPixel();

    return aState;
}

void ClipStateBuilder::CollectSiblings(const vcl::Window& rWindow, std::vector<ClipNode>& rOut)
{
    WindowImpl* pImpl = rWindow.ImplGetWindowImpl();
    if (!pImpl->mpHierarchy)
        return;

    vcl::Window* pSibling = pImpl->mpHierarchy->mpFirstOverlap;
    while (pSibling)
    {
        // Only clip against siblings that are actually visible
        if (pSibling != &rWindow && pSibling->IsReallyVisible())
        {
            const OutputDevice* pSibOutDev = pSibling->GetOutDev();
            tools::Rectangle aBounds(
                Point(pSibOutDev->GetDeviceOriginX(), pSibOutDev->GetDeviceOriginY()),
                pSibling->GetOutputSizePixel());
            rOut.push_back({ aBounds });
        }
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
        // Filter out windows that should NEVER clip the parent
        // (Hidden windows, transparent backgrounds, or explicitly NoClip windows)
        if (pChild->IsReallyVisible() && !pChild->IsPaintTransparent()
            && pChild->GetParentClipMode() != ParentClipMode::NoClip)
        {
            const OutputDevice* pChildOutDev = pChild->GetOutDev();
            tools::Rectangle aBounds(
                Point(pChildOutDev->GetDeviceOriginX(), pChildOutDev->GetDeviceOriginY()),
                pChild->GetOutputSizePixel());
            rOut.push_back({ aBounds });
        }
        pChild = pChild->ImplGetWindowImpl()->mpHierarchy->mpNext;
    }
}
} // namespace vcl::clipping

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
