
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
#include <vcl/CoordinateMapper.hxx>

#include <clipping/ClipStateBuilder.hxx>
#include <window.h>

namespace vcl::clipping
{
ClipState ClipStateBuilder::BuildFromWindow(const vcl::Window& rWindow)
{
    ClipState aState;
    WindowImpl* pImpl = rWindow.ImplGetWindowImpl();

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

ClipState ClipStateBuilder::Build(vcl::Window& rWindow)
{
    ClipState aState;
    WindowImpl* pImpl = rWindow.ImplGetWindowImpl();

    // --- 1. Topological Context ---
    aState.maBounds = rWindow.GetOutputRectPixel();

    if (pImpl->mpClippingState->mbWinRegion)
    {
        aState.maCustomRegion
            = rWindow.GetOutDev()->GetMapper().ViewToDevice(pImpl->mpClippingState->maWinRegion);
    }
    else
    {
        aState.maCustomRegion = std::nullopt;
    }

    // --- 2. Policy Flags ---
    aState.bIsVisible = pImpl->mbReallyVisible;

    // In VCL, WB_CLIPCHILDREN and WB_CLIPSIBLINGS are standard WinBits used to dictate overlap logic
    aState.bClipChildren = (rWindow.GetStyle() & WB_CLIPCHILDREN) != 0;
    aState.bClipSiblings = pImpl->mpClippingState->mbClipSiblings;

    // --- 3. Hierarchy / State Flattening ---

    // Collect Children (If the compiler needs to subtract child bounds)
    vcl::Window* pChild = pImpl->mpHierarchy->mpFirstChild;
    while (pChild)
    {
        WindowImpl* pChildImpl = pChild->ImplGetWindowImpl();
        if (pChildImpl->mbReallyVisible)
        {
            ClipNode aNode;
            aNode.maBounds = pChild->GetOutputRectPixel();
            // If ClipNode later requires custom shapes, add them here
            aState.maChildren.push_back(aNode);
        }
        pChild = pChildImpl->mpHierarchy->mpNext;
    }

    // Collect Siblings (Windows that share our parent but are physically above us in Z-order)
    vcl::Window* pSibling = pImpl->mpHierarchy->mpNext;
    while (pSibling)
    {
        WindowImpl* pSibImpl = pSibling->ImplGetWindowImpl();
        // A sibling only obscures us if it is visible and NOT transparent
        if (pSibImpl->mbReallyVisible && !pSibImpl->mbPaintTransparent)
        {
            ClipNode aNode;
            aNode.maBounds = pSibling->GetOutputRectPixel();
            aState.maSiblings.push_back(aNode);
        }
        pSibling = pSibImpl->mpHierarchy->mpNext;
    }

    // Collect Overlaps (Floating windows acting as superior siblings)
    if (rWindow.ImplIsOverlapWindow())
    {
        vcl::Window* pOverlap = pImpl->mpHierarchy->mpNextOverlap;
        while (pOverlap)
        {
            WindowImpl* pOverlapImpl = pOverlap->ImplGetWindowImpl();
            if (pOverlapImpl->mbReallyVisible && !pOverlapImpl->mbPaintTransparent)
            {
                ClipNode aNode;
                aNode.maBounds = pOverlap->GetOutputRectPixel();
                aState.maSiblings.push_back(aNode);
            }
            pOverlap = pOverlapImpl->mpHierarchy->mpNextOverlap;
        }
    }
    else if (rWindow.ImplGetParent())
    {
        // Standard children are obscured by ANY floating window attached to their parent
        vcl::Window* pOverlap
            = rWindow.ImplGetParent()->ImplGetWindowImpl()->mpHierarchy->mpFirstOverlap;
        while (pOverlap)
        {
            WindowImpl* pOverlapImpl = pOverlap->ImplGetWindowImpl();
            if (pOverlapImpl->mbReallyVisible && !pOverlapImpl->mbPaintTransparent)
            {
                ClipNode aNode;
                aNode.maBounds = pOverlap->GetOutputRectPixel();
                aState.maSiblings.push_back(aNode);
            }
            pOverlap = pOverlapImpl->mpHierarchy->mpNextOverlap;
        }
    }

    return aState;
}

} // namespace vcl::clipping

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
