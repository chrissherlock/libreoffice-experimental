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

void excludeWindowAndOverlapRegions(vcl::Window& rWindow, vcl::Region& rRegion)
{
    if (rWindow.ImplGetWindowImpl()->mbReallyVisible)
        excludeWindowRegion(rWindow, rRegion);

    excludeOverlapWindows(rWindow, rRegion);
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

static void lcl_updateNativeObjectClipRegion(vcl::Window& rWindow, vcl::Region aRegion,
                                             const vcl::Region& rWinRectRegion)
{
    WindowImpl* pImpl = rWindow.ImplGetWindowImpl();
    if (!pImpl || !pImpl->mpSysObj)
        return;

    if (aRegion == rWinRectRegion)
    {
        pImpl->mpSysObj->ResetClipRegion();
        return;
    }

    aRegion.Move(-rWindow.GetOutDev()->GetDeviceOriginX(),
                 -rWindow.GetOutDev()->GetDeviceOriginY());

    // Set/update system object clip region
    RectangleVector aRectangles;
    aRegion.GetRegionRectangles(aRectangles);
    pImpl->mpSysObj->BeginSetClipRegion(aRectangles.size());

    for (auto const& rectangle : aRectangles)
    {
        pImpl->mpSysObj->UnionClipRegion(rectangle.Left(), rectangle.Top(), rectangle.GetWidth(),
                                         rectangle.GetHeight());
    }

    pImpl->mpSysObj->EndSetClipRegion();
}

bool nativeObjectClip(vcl::Window& rWindow, const vcl::Region* pOldRegion)
{
    WindowImpl* pWindowImpl = rWindow.ImplGetWindowImpl();
    if (!pWindowImpl || !pWindowImpl->mpSysObj)
        return true;

    if (!pOldRegion && !pWindowImpl->mpClippingState->mbInitWinClipRegion)
        return true;

    vcl::Region& rWinChildClipRegion = getWinChildClipRegion(rWindow);
    bool bUpdate = true;

    if (syncNativeWindow(*pWindowImpl, rWinChildClipRegion, pOldRegion, bUpdate))
        return bUpdate;

    lcl_updateNativeObjectClipRegion(rWindow, rWinChildClipRegion,
                                     vcl::Region(rWindow.GetOutputRectPixel()));

    pWindowImpl->mpSysObj->Show(true);

    return bUpdate;
}

static void lcl_invalidateNativeClipTargets(vcl::Window* pStartWindow)
{
    if (!pStartWindow)
        return;

    std::vector<vcl::Window*> aTargets;
    gatherNativeSyncTargets(pStartWindow, aTargets);

    for (vcl::Window* pTarget : aTargets)
    {
        nativeObjectClip(*pTarget, nullptr);
    }
}

void updateNativeObjectClip(vcl::Window& rWindow)
{
    WindowImpl* pImpl = rWindow.ImplGetWindowImpl();
    if (!pImpl)
        return;

    if (pImpl->mbOverlapWin)
    {
        lcl_invalidateNativeClipTargets(&rWindow);
        return;
    }

    lcl_invalidateNativeClipTargets(&rWindow);

    if (!pImpl->mpClippingState->mbClipSiblings)
        return;

    for (vcl::Window* pSibling : getFollowingSiblings(*pImpl))
    {
        lcl_invalidateNativeClipTargets(pSibling);
    }
}

bool setClipFlagChildren(vcl::Window& rWindow, bool bSysObjOnlySmaller)
{
    WindowImpl* pImpl = rWindow.ImplGetWindowImpl();
    if (!pImpl)
        return true;

    auto pOldRegion = prepareClipInvalidation(*pImpl, bSysObjOnlySmaller);

    dirtyInitClipRegion(rWindow);
    pImpl->mpClippingState->mbInitWinClipRegion = true;

    bool bUpdate = true;
    for (vcl::Window* pChild : getChildWindows(*pImpl))
    {
        if (!setClipFlagChildren(*pChild, bSysObjOnlySmaller))
            bUpdate = false;
    }

    if (!pImpl->mpSysObj)
        return bUpdate;

    bool bClipSuccess = nativeObjectClip(rWindow, pOldRegion.get());

    auto[bNewUpdate, bInvalidateDevice] = processClipResult(*pImpl, bClipSuccess, bUpdate);
    bUpdate = bNewUpdate;

    if (bInvalidateDevice)
        dirtyInitClipRegion(rWindow);

    return bUpdate;
}

bool setClipFlag(vcl::Window& rWindow, bool bSysObjOnlySmaller)
{
    WindowImpl* pWindowImpl = rWindow.ImplGetWindowImpl();
    if (!pWindowImpl)
        return true;

    if (!rWindow.ImplIsOverlapWindow())
    {
        if (pWindowImpl->mpFrameWindow)
            return setClipFlagOverlapWindows(*pWindowImpl->mpFrameWindow, bSysObjOnlySmaller);

        return true;
    }

    bool bUpdate = setClipFlagChildren(rWindow, bSysObjOnlySmaller);

    vcl::Window* pParent = rWindow.ImplGetParent();
    if (pParent)
    {
        WindowImpl* pParentImpl = pParent->ImplGetWindowImpl();
        // Explicit return value checking replaces hidden references
        if (pParentImpl
            && invalidateParentClipIfRequired(*pWindowImpl, *pParentImpl, pParent->GetStyle()))
        {
            dirtyInitClipRegion(*pParent);
        }
    }

    if (pWindowImpl->mpClippingState->mbClipSiblings)
    {
        for (vcl::Window* pSibling : getFollowingSiblings(*pWindowImpl))
        {
            if (!setClipFlagChildren(*pSibling, bSysObjOnlySmaller))
                bUpdate = false;
        }
    }

    return bUpdate;
}

bool setClipFlagOverlapWindows(vcl::Window& rWindow, bool bSysObjOnlySmaller)
{
    WindowImpl* pImpl = rWindow.ImplGetWindowImpl();
    if (!pImpl)
        return true;

    bool bUpdate = setClipFlagChildren(rWindow, bSysObjOnlySmaller);

    for (vcl::Window* pWindow : getOverlapWindows(*pImpl))
    {
        if (!setClipFlagOverlapWindows(*pWindow, bSysObjOnlySmaller))
            bUpdate = false;
    }

    return bUpdate;
}

} // namespace vcl::clipping

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
