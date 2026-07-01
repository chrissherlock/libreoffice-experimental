/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <vcl/region.hxx>
#include <vcl/window.hxx>
#include <vcl/CoordinateMapper.hxx>

#include <clipping/ClippingManager.hxx>
#include <clipping_window.hxx>
#include <window.h>
#include <salobj.hxx>

namespace vcl::clipping
{
bool initChildRegion(WindowImpl& rImpl)
{
    // Safely clear the initialization flag on function exit
    comphelper::ScopeGuard aDeinitChildRegion(
        [&rImpl]() { rImpl.mpClippingState->mbInitChildRegion = false; });

    if (!rImpl.mpHierarchy->mpFirstChild)
    {
        rImpl.mpClippingState->mpChildClipRegion.reset();
        return false; // No children present; skip downstream clipping
    }

    if (!rImpl.mpClippingState->mpChildClipRegion)
        rImpl.mpClippingState->mpChildClipRegion.reset(
            new vcl::Region(rImpl.mpClippingState->maWinClipRegion));
    else
        *rImpl.mpClippingState->mpChildClipRegion = rImpl.mpClippingState->maWinClipRegion;

    return true; // Context contains children; signal the window to clip them
}

void initWinChildClipRegion(const vcl::Window& rWindow)
{
    WindowImpl* pWindowImpl = rWindow.ImplGetWindowImpl();

    if (initChildRegion(*pWindowImpl))
        clipChildren(rWindow, *pWindowImpl->mpClippingState->mpChildClipRegion);
}

/** Linearly traverses an intrusive linked list of window nodes following
    the sibling chain until a terminator null pointer is encountered. */
static std::vector<vcl::Window*> lcl_gatherWindowChain(vcl::Window* pStartWindow)
{
    std::vector<vcl::Window*> aWindows;
    vcl::Window* pCurrent = pStartWindow;

    while (pCurrent)
    {
        aWindows.push_back(pCurrent);
        pCurrent = pCurrent->ImplGetWindowImpl()->mpHierarchy->mpNext;
    }

    return aWindows;
}

std::vector<vcl::Window*> getChildWindows(const WindowImpl& rImpl)
{
    return lcl_gatherWindowChain(rImpl.mpHierarchy->mpFirstChild);
}

std::vector<vcl::Window*> getOverlapWindows(const WindowImpl& rImpl)
{
    return lcl_gatherWindowChain(rImpl.mpHierarchy->mpFirstOverlap);
}

std::vector<vcl::Window*> getFollowingSiblings(const WindowImpl& rImpl)
{
    return lcl_gatherWindowChain(rImpl.mpHierarchy->mpNext);
}

std::vector<vcl::Window*> getAncestralOverlapSiblings(vcl::Window* pStartWindow)
{
    std::vector<vcl::Window*> aTargets;
    vcl::Window* pCurrentLevel = pStartWindow;

    // Traverse up the overlap window hierarchy until we hit the frame boundary
    while (pCurrentLevel && !pCurrentLevel->ImplGetWindowImpl()->mbFrame)
    {
        vcl::Window* pParentOverlap = pCurrentLevel->ImplGetWindowImpl()->mpOverlapWindow;
        if (!pParentOverlap)
            break; // Safety guard

        // Gather preceding overlap siblings at this specific tier
        vcl::Window* pSibling = pParentOverlap->ImplGetWindowImpl()->mpHierarchy->mpFirstOverlap;
        while (pSibling && (pSibling != pCurrentLevel))
        {
            aTargets.push_back(pSibling);
            pSibling = pSibling->ImplGetWindowImpl()->mpHierarchy->mpNext;
        }

        // Step up to the next hierarchical level
        pCurrentLevel = pParentOverlap;
    }

    return aTargets;
}

Region& getWinChildClipRegion(vcl::Window& rWindow)
{
    WindowImpl* pWindowImpl = rWindow.ImplGetWindowImpl();

    if (pWindowImpl->mpClippingState->mbInitWinClipRegion)
        initWinClipRegion(rWindow);

    if (pWindowImpl->mpClippingState->mbInitChildRegion)
        initWinChildClipRegion(rWindow);

    if (pWindowImpl->mpClippingState->mpChildClipRegion)
        return *pWindowImpl->mpClippingState->mpChildClipRegion;

    return pWindowImpl->mpClippingState->maWinClipRegion;
}

void accumulateParentBoundaries(vcl::Window& rWindow, const vcl::Region& rInterRegion,
                                vcl::Region& rRegion)
{
    WindowImpl* pImpl = rWindow.ImplGetWindowImpl();
    if (!pImpl)
        return;

    vcl::Region aTempRegion;
    vcl::Window* pWindow = &rWindow;

    if (!rWindow.ImplIsOverlapWindow())
    {
        pWindow = rWindow.ImplGetParent();
        do
        {
            aTempRegion = rInterRegion;
            excludeWindowRegion(*pWindow, aTempRegion);
            rRegion.Union(aTempRegion);

            if (pWindow->ImplIsOverlapWindow())
                break;

            pWindow = pWindow->ImplGetParent();
        } while (pWindow);
    }

    if (pWindow && pWindow->ImplGetWindowImpl() && !pWindow->ImplGetWindowImpl()->mbFrame)
    {
        if (pImpl->mpFrameWindow)
        {
            aTempRegion = rInterRegion;
            aTempRegion.Exclude(
                tools::Rectangle(Point(0, 0), pImpl->mpFrameWindow->GetOutputSizePixel()));
            rRegion.Union(aTempRegion);
        }
    }
}

void accumulateSiblingBoundaries(vcl::Window& rWindow, const vcl::Region& rInterRegion,
                                 vcl::Region& rRegion, bool bSiblings)
{
    WindowImpl* pImpl = rWindow.ImplGetWindowImpl();
    if (!pImpl)
        return;

    if (!bSiblings || rWindow.ImplIsOverlapWindow())
        return;

    vcl::Window* pParent = rWindow.ImplGetParent();
    if (!pParent || !pParent->ImplGetWindowImpl())
        return;

    vcl::Region aTempRegion;

    for (vcl::Window* pSibling : getChildWindows(*pParent->ImplGetWindowImpl()))
    {
        WindowImpl* pSiblingImpl = pSibling->ImplGetWindowImpl();
        if (pSiblingImpl && pSiblingImpl->mbReallyVisible && (pSibling != &rWindow))
        {
            aTempRegion = rInterRegion;
            intersectWindowRegion(*pSibling, aTempRegion);
            rRegion.Union(aTempRegion);
        }
    }
}

void accumulateChildBoundaries(vcl::Window& rWindow, const vcl::Region& rInterRegion,
                               vcl::Region& rRegion)
{
    WindowImpl* pImpl = rWindow.ImplGetWindowImpl();
    if (!pImpl)
        return;

    vcl::Region aTempRegion;
    for (vcl::Window* pChild : getChildWindows(*pImpl))
    {
        WindowImpl* pChildImpl = pChild->ImplGetWindowImpl();
        if (pChildImpl && pChildImpl->mbReallyVisible)
        {
            aTempRegion = rInterRegion;
            intersectWindowRegion(*pChild, aTempRegion);
            rRegion.Union(aTempRegion);
        }
    }
}

void accumulateChildOverlaps(vcl::Window* pWindow, const vcl::Region& rInterRegion,
                             vcl::Region& rRegion)
{
    for (vcl::Window* pOverlap : getOverlapWindows(*pWindow->ImplGetWindowImpl()))
    {
        accumulateWindowAndChildOverlaps(pOverlap, rInterRegion, rRegion);
    }
}

void accumulateWindowAndChildOverlaps(vcl::Window* pWindow, const vcl::Region& rInterRegion,
                                      vcl::Region& rRegion)
{
    if (pWindow->ImplGetWindowImpl()->mbReallyVisible)
    {
        vcl::Region aTempRegion(rInterRegion);
        intersectWindowRegion(*pWindow, aTempRegion);
        rRegion.Union(aTempRegion);
    }

    // Delegate down to collect this node's downstream children
    accumulateChildOverlaps(pWindow, rInterRegion, rRegion);
}

void intersectWindowRegion(vcl::Window& rWindow, vcl::Region& rRegion)
{
    // First, clip to the base rectangular output boundary
    rRegion.Intersect(rWindow.GetOutputRectPixel());

    // If the window has a custom user-defined geometric clip path, apply it as well
    if (rWindow.ImplGetWindowImpl()->mpClippingState->mbWinRegion)
    {
        rRegion.Intersect(rWindow.GetOutDev()->GetMapper().ViewToDevice(
            rWindow.ImplGetWindowImpl()->mpClippingState->maWinRegion));
    }
}

void excludeWindowRegion(vcl::Window& rWindow, vcl::Region& rRegion)
{
    // If the target window has a custom boundary path, extract its intersection block
    if (rWindow.ImplGetWindowImpl()->mpClippingState->mbWinRegion)
    {
        vcl::Region aRegion(rWindow.GetOutputRectPixel());
        aRegion.Intersect(rWindow.GetOutDev()->GetMapper().ViewToDevice(
            rWindow.ImplGetWindowImpl()->mpClippingState->maWinRegion));
        rRegion.Exclude(aRegion);
    }
    else
    {
        // Otherwise, simply exclude the standard bounding box
        rRegion.Exclude(rWindow.GetOutputRectPixel());
    }
}

static bool lcl_IsParentClipRequired(ParentClipMode nClipMode, WinBits nStyle)
{
    return !(nClipMode & ParentClipMode::NoClip)
           && ((nClipMode & ParentClipMode::Clip) || (nStyle & WB_CLIPCHILDREN));
}

bool clipChildren(const vcl::Window& rWindow, vcl::Region& rRegion)
{
    bool bOtherClip = false;
    WinBits nParentStyle = rWindow.GetStyle();

    for (vcl::Window* pChild : getChildWindows(*rWindow.ImplGetWindowImpl()))
    {
        if (pChild->ImplGetWindowImpl()->mbReallyVisible)
        {
            ParentClipMode nClipMode = getParentClipMode(*pChild);

            if (lcl_IsParentClipRequired(nClipMode, nParentStyle))
                excludeWindowRegion(*pChild, rRegion);
            else
                bOtherClip = true;
        }
    }

    return bOtherClip;
}

void clipAllChildren(const vcl::Window& rWindow, vcl::Region& rRegion)
{
    for (vcl::Window* pChild : getChildWindows(*rWindow.ImplGetWindowImpl()))
    {
        if (pChild->ImplGetWindowImpl()->mbReallyVisible)
            excludeWindowRegion(*pChild, rRegion);
    }
}

void clipSiblings(const vcl::Window& rWindow, vcl::Region& rRegion)
{
    vcl::Window* pParent = rWindow.ImplGetParent();
    if (!pParent)
        return;

    for (vcl::Window* pSibling : getChildWindows(*pParent->ImplGetWindowImpl()))
    {
        if (pSibling == &rWindow)
            break; // We only clip against preceding siblings

        if (pSibling->ImplGetWindowImpl()->mbReallyVisible)
            excludeWindowRegion(*pSibling, rRegion);
    }
}

void initWinClipRegion(const vcl::Window& rWindow)
{
    WindowImpl* pImpl = rWindow.ImplGetWindowImpl();
    if (!pImpl->mpClippingState->mbInitWinClipRegion)
        return; // Already initialized; bypass calculation pass

    // Establish baseline viewport bounds
    pImpl->mpClippingState->maWinClipRegion = rWindow.GetOutputRectPixel();
    if (pImpl->mpClippingState->mbWinRegion)
    {
        pImpl->mpClippingState->maWinClipRegion.Intersect(
            rWindow.GetOutDev()->GetMapper().ViewToDevice(pImpl->mpClippingState->maWinRegion));
    }

    // Intersect against preceding elements in the Z-order stack
    if (pImpl->mpClippingState->mbClipSiblings && !rWindow.ImplIsOverlapWindow())
        clipSiblings(rWindow, pImpl->mpClippingState->maWinClipRegion);

    clipBoundaries(rWindow, pImpl->mpClippingState->maWinClipRegion, false, true);

    if ((rWindow.GetStyle() & WB_CLIPCHILDREN) || pImpl->mpClippingState->mbClipChildren)
        pImpl->mpClippingState->mbInitChildRegion = true;

    pImpl->mpClippingState->mbInitWinClipRegion = false;
}

void excludeOverlapWindows(const vcl::Window& rWindow, vcl::Region& rRegion)
{
    for (vcl::Window* pOverlap : getOverlapWindows(*rWindow.ImplGetWindowImpl()))
    {
        if (pOverlap->ImplGetWindowImpl()->mbReallyVisible)
        {
            excludeWindowRegion(*pOverlap, rRegion);
            excludeOverlapWindows(*pOverlap, rRegion);
        }
    }
}

void clipBoundaries(const vcl::Window& rWindow, vcl::Region& rRegion, bool bThis, bool bOverlaps)
{
    WindowImpl* pImpl = rWindow.ImplGetWindowImpl();

    if (bThis)
    {
        if (pImpl->mpClippingState->mbInitWinClipRegion)
            initWinClipRegion(rWindow);

        rRegion.Intersect(pImpl->mpClippingState->maWinClipRegion);
        return;
    }

    if (!rWindow.ImplIsOverlapWindow())
    {
        vcl::Window* pParent = rWindow.ImplGetParent();
        if (pParent)
        {
            WindowImpl* pParentImpl = pParent->ImplGetWindowImpl();
            if (pParentImpl->mpClippingState->mbInitWinClipRegion)
                initWinClipRegion(*pParent);

            rRegion.Intersect(pParentImpl->mpClippingState->maWinClipRegion);
        }
        return;
    }

    if (!pImpl->mbFrame)
    {
        rRegion.Intersect(
            tools::Rectangle(Point(0, 0), pImpl->mpFrameWindow->GetOutputSizePixel()));
    }

    if (!bOverlaps || rRegion.IsEmpty())
        return;

    for (vcl::Window* pOverlapWin : getAncestralOverlapSiblings(const_cast<vcl::Window*>(&rWindow)))
    {
        if (pOverlapWin->ImplGetWindowImpl()->mbReallyVisible)
            excludeWindowRegion(*pOverlapWin, rRegion);

        excludeOverlapWindows(*pOverlapWin, rRegion);
    }

    excludeOverlapWindows(rWindow, rRegion);
}

void setParentClipMode(vcl::Window* pWindow, ParentClipMode nMode)
{
    if (!pWindow)
        return;

    WindowImpl* pImpl = pWindow->ImplGetWindowImpl();

    if (pImpl->mpBorderWindow)
    {
        setParentClipMode(pImpl->mpBorderWindow.get(), nMode);
        return;
    }

    if (pImpl->mbOverlapWin)
        return;

    pImpl->mpClippingState->meParentClipMode = nMode;

    if (nMode & ParentClipMode::Clip)
    {
        if (pImpl->mpHierarchy && pImpl->mpHierarchy->mpParent)
        {
            WindowImpl* pParentImpl = pImpl->mpHierarchy->mpParent->ImplGetWindowImpl();
            pParentImpl->mpClippingState->mbClipChildren = true;
        }
    }
}

ParentClipMode getParentClipMode(const vcl::Window& rWindow)
{
    WindowImpl* pWindowImpl = rWindow.ImplGetWindowImpl();

    if (pWindowImpl->mpBorderWindow)
        return getParentClipMode(*pWindowImpl->mpBorderWindow);

    return pWindowImpl->mpClippingState->meParentClipMode;
}

void updateNativeObjectClip(vcl::Window& rWindow)
{
    WindowImpl* pImpl = rWindow.ImplGetWindowImpl();
    if (!pImpl)
        return;

    // If this window doesn't own a native system handle, recurse
    if (!pImpl->mpSysObj)
    {
        vcl::Window* pChild = pImpl->mpHierarchy->mpFirstChild;
        while (pChild)
        {
            updateNativeObjectClip(*pChild);
            pChild = pChild->ImplGetWindowImpl()->mpHierarchy->mpNext;
        }

        vcl::Window* pOverlap = pImpl->mpHierarchy->mpFirstOverlap;
        while (pOverlap)
        {
            updateNativeObjectClip(*pOverlap);
            pOverlap = pOverlap->ImplGetWindowImpl()->mpHierarchy->mpNextOverlap;
        }
        return;
    }

    auto& rManager = rWindow.GetOutDev()->GetClippingManager(rWindow);
    const auto& rPlan = rManager.GetClipPlan(rWindow);

    // Handle visibility or total occlusion state safely
    if (!pImpl->mbReallyVisible || rPlan.mbEmpty || rPlan.maFinalRegion.IsEmpty())
    {
        pImpl->mpSysObj->Show(false);
        return;
    }

    // Reset to unclipped frame coordinates if the plan matches our baseline output rect
    vcl::Region aTargetRegion = rPlan.maFinalRegion;
    vcl::Region rWinRectRegion(rWindow.GetOutputRectPixel());

    if (aTargetRegion == rWinRectRegion)
    {
        pImpl->mpSysObj->ResetClipRegion();
        pImpl->mpSysObj->Show(true);
        return;
    }

    // Translate VCL absolute coordinates to the system object's device space
    aTargetRegion.Move(-rWindow.GetOutDev()->GetDeviceOriginX(),
                       -rWindow.GetOutDev()->GetDeviceOriginY());

    // Blit the rectangles to the underlying OS windowing sub-system
    RectangleVector aRectangles;
    aTargetRegion.GetRegionRectangles(aRectangles);
    pImpl->mpSysObj->BeginSetClipRegion(aRectangles.size());

    for (auto const& rectangle : aRectangles)
    {
        pImpl->mpSysObj->UnionClipRegion(rectangle.Left(), rectangle.Top(), rectangle.GetWidth(),
                                         rectangle.GetHeight());
    }

    pImpl->mpSysObj->EndSetClipRegion();
    pImpl->mpSysObj->Show(true);
}

void calcOverlapRegionOverlaps(const vcl::Window& rWindow, const vcl::Region& rInterRegion,
                               vcl::Region& rRegion)
{
    const WindowImpl* pImpl = rWindow.ImplGetWindowImpl();
    if (!pImpl)
        return;

    // High-level ancestral sibling walk
    for (vcl::Window* pOverlapWin : getAncestralOverlapSiblings(const_cast<vcl::Window*>(&rWindow)))
    {
        accumulateWindowAndChildOverlaps(pOverlapWin, rInterRegion, rRegion);
    }

    // Child overlap window execution
    vcl::Window* pOverlapParent
        = !pImpl->mbOverlapWin ? pImpl->mpOverlapWindow.get() : const_cast<vcl::Window*>(&rWindow);
    accumulateChildOverlaps(pOverlapParent, rInterRegion, rRegion);
}

void calcOverlapRegion(vcl::Window& rWindow, const tools::Rectangle& rSourceRect,
                       vcl::Region& rRegion, bool bChildren, bool bSiblings)
{
    WindowImpl* pImpl = rWindow.ImplGetWindowImpl();
    if (!pImpl)
        return;

    vcl::Region aRegion(rSourceRect);

    if (pImpl->mpClippingState->mbWinRegion)
    {
        rRegion.Intersect(
            rWindow.GetOutDev()->GetMapper().ViewToDevice(pImpl->mpClippingState->maWinRegion));
    }

    calcOverlapRegionOverlaps(rWindow, aRegion, rRegion);
    accumulateParentBoundaries(rWindow, aRegion, rRegion);
    accumulateSiblingBoundaries(rWindow, aRegion, rRegion, bSiblings);

    if (!bChildren)
        return;

    accumulateChildBoundaries(rWindow, aRegion, rRegion);
}

} // namespace vcl::clipping

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
