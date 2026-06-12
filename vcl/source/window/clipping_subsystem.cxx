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

#include <clipping.hxx>
#include <window.h>
#include <salobj.hxx>

namespace vcl::clipping
{
bool initChildRegion(WindowImpl& rImpl)
{
    // Safely clear the initialization flag on function exit
    comphelper::ScopeGuard aDeinitChildRegion(
        [&rImpl]() { rImpl.mpClippingState->mbInitChildRegion = false; });

    if (!rImpl.mpFirstChild)
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

bool syncNativeWindow(WindowImpl& rImpl, vcl::Region& rWinChildClipRegion,
                      const vcl::Region* pOldRegion, bool& rOutUpdate)
{
    if (!rImpl.mpSysObj)
    {
        rOutUpdate = true;
        return true;
    }

    if (!rImpl.mbReallyVisible || rWinChildClipRegion.IsEmpty())
    {
        rImpl.mpSysObj->Show(false);
        rOutUpdate = true;
        return true;
    }

    rOutUpdate = true;
    if (pOldRegion)
    {
        vcl::Region aNewRegion = rWinChildClipRegion;
        rWinChildClipRegion.Intersect(*pOldRegion);
        rOutUpdate = (aNewRegion == rWinChildClipRegion);
    }

    return false; // Signal that downstream native updates are required
}

std::unique_ptr<vcl::Region> prepareClipInvalidation(WindowImpl& rImpl, bool bSysObjOnlySmaller)
{
    if (rImpl.mpSysObj && bSysObjOnlySmaller && !rImpl.mpClippingState->mbInitWinClipRegion)
        return std::make_unique<vcl::Region>(rImpl.mpClippingState->maWinClipRegion);

    return nullptr;
}

bool invalidateParentClipIfRequired(const WindowImpl& rChildImpl, WindowImpl& rParentImpl,
                                    WinBits nParentStyle)
{
    if ((nParentStyle & WB_CLIPCHILDREN)
        || (rChildImpl.mpClippingState->meParentClipMode & ParentClipMode::Clip))
    {
        rParentImpl.mpClippingState->mbInitChildRegion = true;
        return true; // Signals that parent device clip region needs invalidation
    }

    return false;
}

NativeSyncStatus processClipResult(WindowImpl& rImpl, bool bClipSuccess, bool bCurrentUpdate)
{
    if (!bClipSuccess)
    {
        rImpl.mpClippingState->mbInitWinClipRegion = true;
        return { false, true }; // bUpdate = false, bInvalidateDevice = true
    }

    return { bCurrentUpdate, false }; // Unchanged state
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
        pCurrent = pCurrent->ImplGetWindowImpl()->mpNext;
    }

    return aWindows;
}

std::vector<vcl::Window*> getChildWindows(const WindowImpl& rImpl)
{
    return lcl_gatherWindowChain(rImpl.mpFirstChild);
}

std::vector<vcl::Window*> getOverlapWindows(const WindowImpl& rImpl)
{
    return lcl_gatherWindowChain(rImpl.mpFirstOverlap);
}

std::vector<vcl::Window*> getFollowingSiblings(const WindowImpl& rImpl)
{
    return lcl_gatherWindowChain(rImpl.mpNext);
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
        vcl::Window* pSibling = pParentOverlap->ImplGetWindowImpl()->mpFirstOverlap;
        while (pSibling && (pSibling != pCurrentLevel))
        {
            aTargets.push_back(pSibling);
            pSibling = pSibling->ImplGetWindowImpl()->mpNext;
        }

        // Step up to the next hierarchical level
        pCurrentLevel = pParentOverlap;
    }

    return aTargets;
}

void gatherNativeSyncTargets(vcl::Window* pWindow, std::vector<vcl::Window*>& rTargets)
{
    if (!pWindow)
        return;

    rTargets.push_back(pWindow);

    for (vcl::Window* pChild : getChildWindows(*pWindow->ImplGetWindowImpl()))
    {
        gatherNativeSyncTargets(pChild, rTargets);
    }

    for (vcl::Window* pOverlap : getOverlapWindows(*pWindow->ImplGetWindowImpl()))
    {
        gatherNativeSyncTargets(pOverlap, rTargets);
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
        intersectWindowRegion(pWindow, aTempRegion);
        rRegion.Union(aTempRegion);
    }

    // Delegate down to collect this node's downstream children
    accumulateChildOverlaps(pWindow, rInterRegion, rRegion);
}

void intersectWindowRegion(vcl::Window* pWindow, vcl::Region& rRegion)
{
    // First, clip to the base rectangular output boundary
    rRegion.Intersect(pWindow->GetOutputRectPixel());

    // If the window has a custom user-defined geometric clip path, apply it as well
    if (pWindow->ImplGetWindowImpl()->mbWinRegion)
    {
        rRegion.Intersect(pWindow->GetOutDev()->GetMapper().ViewToDevice(
            pWindow->ImplGetWindowImpl()->maWinRegion));
    }
}

void excludeWindowRegion(vcl::Window* pWindow, vcl::Region& rRegion)
{
    // If the target window has a custom boundary path, extract its intersection block
    if (pWindow->ImplGetWindowImpl()->mbWinRegion)
    {
        vcl::Region aRegion(pWindow->GetOutputRectPixel());
        aRegion.Intersect(pWindow->GetOutDev()->GetMapper().ViewToDevice(
            pWindow->ImplGetWindowImpl()->maWinRegion));
        rRegion.Exclude(aRegion);
    }
    else
    {
        // Otherwise, simply exclude the standard bounding box
        rRegion.Exclude(pWindow->GetOutputRectPixel());
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
            ParentClipMode nClipMode = pChild->GetParentClipMode();

            if (lcl_IsParentClipRequired(nClipMode, nParentStyle))
                excludeWindowRegion(pChild, rRegion);
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
            excludeWindowRegion(pChild, rRegion);
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
            excludeWindowRegion(pSibling, rRegion);
    }
}

} // namespace vcl::clipping

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
