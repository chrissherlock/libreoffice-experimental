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
#include <clipping/traits.hxx>
#include <devicedispatcher.hxx>
#include <window.h>

namespace vcl::clipping
{
ClipState ClipStateBuilder::Build(const OutputDevice& rDevice)
{
    return vcl::DispatchDevice(rDevice, [](const auto& rTypedDev) {
        using T = std::decay_t<decltype(rTypedDev)>;
        ClipState aState;

        if constexpr (has_hierarchical_clipping_v<T>)
        {
            // By default, the rendering pipeline uses Local coordinates
            return Build(*rTypedDev.GetOwnerWindow(), ClipSpace::Local);
        }
        else
        {
            // The "One-Level Hierarchy" logic for Printers and VirDevs
            aState.bIsVisible = true;
            aState.bClipChildren = false;
            aState.bClipSiblings = false;

            aState.maBounds = tools::Rectangle(Point(0, 0), rTypedDev.GetOutputSizePixel());

            const auto& rDevState = rTypedDev.GetClipState();
            if (rDevState.mbHasCustomClip)
                aState.maCustomRegion = rTypedDev.GetMapper().ViewToDevice(rDevState.maRegion);

            return aState;
        }
    });
}

ClipState ClipStateBuilder::Build(const vcl::Window& rWindow, ClipSpace eSpace)
{
    ClipState aState;
    WindowImpl* pImpl = rWindow.ImplGetWindowImpl();

    aState.bIsVisible = pImpl->mbReallyVisible;

    // Unified Clip Flags
    aState.bClipChildren = (rWindow.GetStyle() & WB_CLIPCHILDREN) != 0
                           || (pImpl->mpClippingState && pImpl->mpClippingState->mbClipChildren);
    aState.bClipSiblings = pImpl->mpClippingState && pImpl->mpClippingState->mbClipSiblings;

    // Bounds & Custom Region extraction
    if (eSpace == ClipSpace::AbsoluteDevice)
    {
        aState.maBounds = GetNodeBounds(rWindow, eSpace);

        // Intersect with parent bounds to prevent phantom out-of-bounds painting
        if (pImpl->mpHierarchy && pImpl->mpHierarchy->mpParent)
        {
            tools::Rectangle aParentBounds = GetNodeBounds(*pImpl->mpHierarchy->mpParent, eSpace);
            aState.maBounds.Intersection(aParentBounds);
        }

        if (pImpl->mpClippingState && pImpl->mpClippingState->mbWinRegion)
            aState.maCustomRegion = rWindow.GetWindowClipRegionPixel();
    }
    else
    {
        aState.maBounds = GetNodeBounds(rWindow, eSpace);

        if (pImpl->mpClippingState && pImpl->mpClippingState->mbWinRegion)
        {
            aState.maCustomRegion = rWindow.GetOutDev()->GetMapper().ViewToDevice(
                pImpl->mpClippingState->maWinRegion);
        }
    }

    // Unified tree traversal
    CollectSiblings(rWindow, aState.maSiblings, eSpace);
    CollectChildren(rWindow, aState.maChildren, eSpace);

    return aState;
}

tools::Rectangle ClipStateBuilder::GetNodeBounds(const vcl::Window& rNode, ClipSpace eSpace)
{
    if (eSpace == ClipSpace::AbsoluteDevice)
    {
        const OutputDevice* pOutDev = rNode.GetOutDev();
        return tools::Rectangle(Point(pOutDev->GetDeviceOriginX(), pOutDev->GetDeviceOriginY()),
                                rNode.GetOutputSizePixel());
    }
    return rNode.GetOutputRectPixel();
}

void ClipStateBuilder::CollectSiblings(const vcl::Window& rWindow, std::vector<ClipNode>& rOut,
                                       ClipSpace eSpace)
{
    WindowImpl* pImpl = rWindow.ImplGetWindowImpl();
    if (!pImpl->mpHierarchy)
        return;

    // Standard Siblings (Windows that share our parent but are physically above us in Z-order)
    vcl::Window* pSibling = pImpl->mpHierarchy->mpNext;
    while (pSibling)
    {
        WindowImpl* pSibImpl = pSibling->ImplGetWindowImpl();
        if (pSibImpl->mbReallyVisible && !pSibImpl->mbPaintTransparent)
            rOut.push_back({ GetNodeBounds(*pSibling, eSpace) });

        pSibling = pSibImpl->mpHierarchy->mpNext;
    }

    // Overlap Windows (Floating windows acting as superior siblings)
    if (rWindow.ImplIsOverlapWindow())
    {
        vcl::Window* pOverlap = pImpl->mpHierarchy->mpNextOverlap;
        while (pOverlap)
        {
            WindowImpl* pOverlapImpl = pOverlap->ImplGetWindowImpl();
            if (pOverlapImpl->mbReallyVisible && !pOverlapImpl->mbPaintTransparent)
                rOut.push_back({ GetNodeBounds(*pOverlap, eSpace) });

            pOverlap = pOverlapImpl->mpHierarchy->mpNextOverlap;
        }
    }
    else if (rWindow.ImplGetParent())
    {
        vcl::Window* pOverlap
            = rWindow.ImplGetParent()->ImplGetWindowImpl()->mpHierarchy->mpFirstOverlap;
        while (pOverlap)
        {
            WindowImpl* pOverlapImpl = pOverlap->ImplGetWindowImpl();
            if (pOverlap != &rWindow && pOverlapImpl->mbReallyVisible
                && !pOverlapImpl->mbPaintTransparent)
                rOut.push_back({ GetNodeBounds(*pOverlap, eSpace) });

            pOverlap = pOverlapImpl->mpHierarchy->mpNextOverlap;
        }
    }
}

void ClipStateBuilder::CollectChildren(const vcl::Window& rWindow, std::vector<ClipNode>& rOut,
                                       ClipSpace eSpace)
{
    WindowImpl* pImpl = rWindow.ImplGetWindowImpl();
    if (!pImpl->mpHierarchy)
        return;

    vcl::Window* pChild = pImpl->mpHierarchy->mpFirstChild;
    while (pChild)
    {
        // Filter out windows that should NEVER clip the parent
        if (pChild->IsReallyVisible() && !pChild->IsPaintTransparent()
            && pChild->GetParentClipMode() != ParentClipMode::NoClip)
        {
            rOut.push_back({ GetNodeBounds(*pChild, eSpace) });
        }
        pChild = pChild->ImplGetWindowImpl()->mpHierarchy->mpNext;
    }
}

} // namespace vcl::clipping
