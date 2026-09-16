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

#include <window/window.h>
#include <window/state/clipping_window.hxx>
#include <window/state/WindowClassification.hxx>
#include <window/state/WindowVisibilityState.hxx>
#include <window/state/WindowClippingState.hxx>
#include <window/state/WindowHierarchy.hxx>
#include <window/state/WindowPlatformState.hxx>
#include <salobj.hxx>

namespace vcl::clipping
{
bool initChildRegion(vcl::Window& rWindow)
{
    WindowClippingState* pImpl = rWindow.ImplGetClippingState();
    if (!pImpl)
        return false;

    // Safely clear the initialization flag on function exit
    comphelper::ScopeGuard aDeinitChildRegion([pImpl]() { pImpl->mbInitChildRegion = false; });

    WindowHierarchy* pHierarchy = rWindow.ImplGetWindowHierarchy();
    if (!pHierarchy || !pHierarchy->hasChildren())
    {
        pImpl->mpChildClipRegion.reset();
        return false; // No children present; skip downstream clipping
    }

    if (!pImpl->mpChildClipRegion)
        pImpl->mpChildClipRegion = std::make_unique<vcl::Region>(pImpl->maWinClipRegion);
    else
        *pImpl->mpChildClipRegion = pImpl->maWinClipRegion;

    return true; // Context contains children; signal the window to clip them
}

bool syncNativeWindow(WindowPlatformState& rImpl, WindowVisibilityState& rVisibilityState,
                      vcl::Region& rWinChildClipRegion, const vcl::Region* pOldRegion,
                      bool& rOutUpdate)
{
    if (!rImpl.mpSysObj)
    {
        rOutUpdate = true;
        return true;
    }

    if (!rVisibilityState.mbReallyVisible || rWinChildClipRegion.IsEmpty())
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

std::unique_ptr<vcl::Region> getClipInvalidationRegion(WindowPlatformState& rPlatformState,
                                                       WindowClippingState& rClippingState,
                                                       bool bSysObjOnlySmaller)
{
    if (rPlatformState.mpSysObj && bSysObjOnlySmaller && !rClippingState.mbInitWinClipRegion)
        return std::make_unique<vcl::Region>(rClippingState.maWinClipRegion);

    return nullptr;
}

bool invalidateParentClipIfRequired(const WindowClippingState& rChildImpl,
                                    WindowClippingState& rParentImpl, WinBits nParentStyle)
{
    if ((nParentStyle & WB_CLIPCHILDREN) || (rChildImpl.meParentClipMode & ParentClipMode::Clip))
    {
        rParentImpl.mbInitChildRegion = true;
        return true; // Signals that parent device clip region needs invalidation
    }

    return false;
}

/** Linearly traverses the sibling chain starting from pStartWindow until the end. */
static std::vector<vcl::Window*> lcl_gatherWindowChain(vcl::Window* pStartWindow)
{
    std::vector<vcl::Window*> aWindows;

    WindowHierarchy::forEachSibling(pStartWindow, [&](vcl::Window& rWindow) {
        aWindows.push_back(&rWindow);
    });

    return aWindows;
}

std::vector<vcl::Window*> getChildWindows(const WindowHierarchy& rHierarchy)
{
    return lcl_gatherWindowChain(rHierarchy.getFirstChild());
}

std::vector<vcl::Window*> getOverlapWindows(const WindowHierarchy& rHierarchy)
{
    return lcl_gatherWindowChain(rHierarchy.getFirstOverlap());
}

std::vector<vcl::Window*> getFollowingSiblings(const WindowHierarchy& rHierarchy)
{
    return lcl_gatherWindowChain(rHierarchy.getNextSibling());
}

std::vector<vcl::Window*> getAncestralOverlapSiblings(vcl::Window* pStartWindow)
{
    std::vector<vcl::Window*> aTargets;
    vcl::Window* pCurrentLevel = pStartWindow;

    // Traverse up the overlap window hierarchy until we hit the frame boundary
    while (pCurrentLevel)
    {
        const WindowClassification* pClassification = pCurrentLevel->ImplGetWindowClassification();
        if (pClassification && pClassification->mbFrame)
            break;

        const WindowHierarchy* pHierarchy = pCurrentLevel->ImplGetWindowHierarchy();
        if (!pHierarchy)
            break;

        vcl::Window* pParentOverlap = pHierarchy->getOverlapWindow();
        if (!pParentOverlap)
            break;

        const WindowHierarchy* pParentHierarchy = pParentOverlap->ImplGetWindowHierarchy();
        if (pParentHierarchy)
        {
            // Gather preceding overlap siblings at this specific tier
            WindowHierarchy::forEachSibling(pParentHierarchy->getFirstOverlap(), [&](vcl::Window& rSibling) -> bool {
                if (&rSibling == pCurrentLevel)
                    return false; // Stop when hitting our branch

                aTargets.push_back(&rSibling);
                return true;
            });
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

    for (vcl::Window* pChild : getChildWindows(*pWindow->ImplGetWindowHierarchy()))
    {
        gatherNativeSyncTargets(pChild, rTargets);
    }

    for (vcl::Window* pOverlap : getOverlapWindows(*pWindow->ImplGetWindowHierarchy()))
    {
        gatherNativeSyncTargets(pOverlap, rTargets);
    }
}

void accumulateParentBoundaries(vcl::Window& rWindow, const vcl::Region& rInterRegion,
                                vcl::Region& rRegion)
{
    WindowHierarchy* pHierarchy = rWindow.ImplGetWindowHierarchy();
    if (!pHierarchy)
        return;

    vcl::Window* pBoundaryWindow = &rWindow;

    if (!rWindow.ImplIsOverlapWindow())
    {
        pBoundaryWindow = nullptr;
        pHierarchy->forEachAncestor([&](vcl::Window& rAncestor) -> bool {
            vcl::Region aTempRegion = rInterRegion;
            excludeWindowRegion(rAncestor, aTempRegion);
            rRegion.Union(aTempRegion);

            if (rAncestor.ImplIsOverlapWindow())
            {
                pBoundaryWindow = &rAncestor;
                return false; // Reached enclosing overlap boundary; stop climbing
            }

            return true;
        });
    }

    if (pBoundaryWindow)
    {
        const WindowClassification* pClassification = pBoundaryWindow->ImplGetWindowClassification();
        if (pClassification && !pClassification->mbFrame)
        {
            if (vcl::Window* pFrameWindow = pHierarchy->getFrameWindow())
            {
                vcl::Region aTempRegion = rInterRegion;
                aTempRegion.Exclude(
                    tools::Rectangle(Point(0, 0), pFrameWindow->GetOutputSizePixel()));
                rRegion.Union(aTempRegion);
            }
        }
    }
}

void accumulateSiblingBoundaries(vcl::Window& rWindow, const vcl::Region& rInterRegion,
                                 vcl::Region& rRegion, bool bSiblings)
{
    WindowClassification* pImpl = rWindow.ImplGetWindowClassification();
    if (!pImpl)
        return;

    if (!bSiblings || rWindow.ImplIsOverlapWindow())
        return;

    vcl::Window* pParent = rWindow.ImplGetParent();
    if (!pParent || !pParent->ImplGetWindowHierarchy())
        return;

    vcl::Region aTempRegion;

    for (vcl::Window* pSibling : getChildWindows(*pParent->ImplGetWindowHierarchy()))
    {
        WindowClassification* pSiblingImpl = pSibling->ImplGetWindowClassification();
        WindowVisibilityState* pSiblingVisibility = pSibling->ImplGetVisibilityState();
        if (pSiblingImpl && pSiblingVisibility->mbReallyVisible && (pSibling != &rWindow))
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
    const WindowHierarchy* pHierarchy = rWindow.ImplGetWindowHierarchy();
    if (!pHierarchy)
        return;

    vcl::Region aTempRegion;
    for (vcl::Window* pChild : getChildWindows(*pHierarchy))
    {
        WindowClassification* pChildImpl = pChild->ImplGetWindowClassification();
        WindowVisibilityState* pChildVisibility = pChild->ImplGetVisibilityState();
        if (pChildImpl && pChildVisibility->mbReallyVisible)
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
    for (vcl::Window* pOverlap : getOverlapWindows(*pWindow->ImplGetWindowHierarchy()))
    {
        accumulateWindowAndChildOverlaps(pOverlap, rInterRegion, rRegion);
    }
}

void accumulateWindowAndChildOverlaps(vcl::Window* pWindow, const vcl::Region& rInterRegion,
                                      vcl::Region& rRegion)
{
    if (pWindow->ImplGetVisibilityState()->mbReallyVisible)
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
    if (rWindow.ImplGetClippingState()->mbWinRegion)
    {
        rRegion.Intersect(rWindow.GetOutDev()->GetMapper().ViewToDevice(
            rWindow.ImplGetClippingState()->maWinRegion));
    }
}

void excludeWindowRegion(vcl::Window& rWindow, vcl::Region& rRegion)
{
    // If the target window has a custom boundary path, extract its intersection block
    if (rWindow.ImplGetClippingState()->mbWinRegion)
    {
        vcl::Region aRegion(rWindow.GetOutputRectPixel());
        aRegion.Intersect(rWindow.GetOutDev()->GetMapper().ViewToDevice(
            rWindow.ImplGetClippingState()->maWinRegion));
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
    if (rWindow.ImplGetVisibilityState()->mbReallyVisible)
        excludeWindowRegion(rWindow, rRegion);

    excludeOverlapWindows(rWindow, rRegion);
}

void clipAllChildren(const vcl::Window& rWindow, vcl::Region& rRegion)
{
    for (vcl::Window* pChild : getChildWindows(*rWindow.ImplGetWindowHierarchy()))
    {
        if (pChild->ImplGetVisibilityState()->mbReallyVisible)
            excludeWindowRegion(*pChild, rRegion);
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

    for (vcl::Window* pChild : getChildWindows(*rWindow.ImplGetWindowHierarchy()))
    {
        if (pChild->ImplGetVisibilityState()->mbReallyVisible)
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

void clipSiblings(const vcl::Window& rWindow, vcl::Region& rRegion)
{
    vcl::Window* pParent = rWindow.ImplGetParent();
    if (!pParent)
        return;

    for (vcl::Window* pSibling : getChildWindows(*pParent->ImplGetWindowHierarchy()))
    {
        if (pSibling == &rWindow)
            break; // We only clip against preceding siblings

        if (pSibling->ImplGetVisibilityState()->mbReallyVisible)
            excludeWindowRegion(*pSibling, rRegion);
    }
}

void initWinClipRegion(const vcl::Window& rWindow)
{
    WindowClippingState* pImpl = rWindow.ImplGetClippingState();
    if (!pImpl->mbInitWinClipRegion)
        return; // Already initialized; bypass calculation pass

    // Establish baseline viewport bounds
    pImpl->maWinClipRegion = rWindow.GetOutputRectPixel();
    if (pImpl->mbWinRegion)
    {
        pImpl->maWinClipRegion.Intersect(
            rWindow.GetOutDev()->GetMapper().ViewToDevice(pImpl->maWinRegion));
    }

    // Intersect against preceding elements in the Z-order stack
    if (pImpl->mbClipSiblings && !rWindow.ImplIsOverlapWindow())
        clipSiblings(rWindow, pImpl->maWinClipRegion);

    clipBoundaries(rWindow, pImpl->maWinClipRegion, false, true);

    if ((rWindow.GetStyle() & WB_CLIPCHILDREN) || pImpl->mbClipChildren)
        pImpl->mbInitChildRegion = true;

    pImpl->mbInitWinClipRegion = false;
}

void excludeOverlapWindows(const vcl::Window& rWindow, vcl::Region& rRegion)
{
    for (vcl::Window* pOverlap : getOverlapWindows(*rWindow.ImplGetWindowHierarchy()))
    {
        if (pOverlap->ImplGetVisibilityState()->mbReallyVisible)
        {
            excludeWindowRegion(*pOverlap, rRegion);
            excludeOverlapWindows(*pOverlap, rRegion);
        }
    }
}

void clipBoundaries(const vcl::Window& rWindow, vcl::Region& rRegion, bool bThis, bool bOverlaps)
{
    WindowClippingState* pClippingState = rWindow.ImplGetClippingState();
    if (!pClippingState)
        return;

    if (bThis)
    {
        if (pClippingState->mbInitWinClipRegion)
            initWinClipRegion(rWindow);

        rRegion.Intersect(pClippingState->maWinClipRegion);
        return;
    }

    if (!rWindow.ImplIsOverlapWindow())
    {
        vcl::Window* pParent = rWindow.ImplGetParent();
        if (pParent)
        {
            WindowClippingState* pParentClippingState = pParent->ImplGetClippingState();
            if (pParentClippingState)
            {
                if (pParentClippingState->mbInitWinClipRegion)
                    initWinClipRegion(*pParent);

                rRegion.Intersect(pParentClippingState->maWinClipRegion);
            }
        }
        return;
    }

    const WindowHierarchy* pHierarchy = rWindow.ImplGetWindowHierarchy();
    const WindowClassification* pClassification = rWindow.ImplGetWindowClassification();

    if (pClassification && pClassification->mbFrame)
    {
        if (pHierarchy && pHierarchy->getFrameWindow())
        {
            rRegion.Intersect(
                tools::Rectangle(Point(0, 0), pHierarchy->getFrameWindow()->GetOutputSizePixel()));
        }
    }

    if (!bOverlaps || rRegion.IsEmpty())
        return;

    for (vcl::Window* pOverlapWin : getAncestralOverlapSiblings(const_cast<vcl::Window*>(&rWindow)))
    {
        const WindowVisibilityState* pVisState = pOverlapWin->ImplGetVisibilityState();
        if (pVisState && pVisState->mbReallyVisible)
            excludeWindowRegion(*pOverlapWin, rRegion);

        excludeOverlapWindows(*pOverlapWin, rRegion);
    }

    excludeOverlapWindows(rWindow, rRegion);
}

void setParentClipMode(vcl::Window* pWindow, ParentClipMode nMode)
{
    if (!pWindow)
        return;

    WindowHierarchy* pHierarchy = pWindow->ImplGetWindowHierarchy();
    if (!pHierarchy)
        return;

    if (pHierarchy->hasBorderWindow())
    {
        setParentClipMode(pHierarchy->getBorderWindow(), nMode);
        return;
    }

    if (pWindow->ImplIsOverlapWindow())
        return;

    WindowClippingState* pClippingState = pWindow->ImplGetClippingState();
    if (!pClippingState)
        return;

    pClippingState->meParentClipMode = nMode;

    if ((nMode & ParentClipMode::Clip) && pHierarchy->getParent())
    {
        if (WindowClippingState* pParentClippingState = pHierarchy->getParent()->ImplGetClippingState())
            pParentClippingState->mbClipChildren = true;
    }
}

ParentClipMode getParentClipMode(const vcl::Window& rWindow)
{
    const WindowHierarchy* pHierarchy = rWindow.ImplGetWindowHierarchy();
    if (pHierarchy && pHierarchy->hasBorderWindow())
        return getParentClipMode(*pHierarchy->getBorderWindow());

    const WindowClippingState* pClippingState = rWindow.ImplGetClippingState();
    return pClippingState ? pClippingState->meParentClipMode : ParentClipMode::NoClip;
}

static void lcl_updateNativeObjectClipRegion(vcl::Window& rWindow, vcl::Region aRegion,
                                             const vcl::Region& rWinRectRegion)
{
    WindowPlatformState* pPlatformState = rWindow.ImplGetPlatformState();
    if (!pPlatformState || !pPlatformState->mpSysObj)
        return;

    if (aRegion == rWinRectRegion)
    {
        pPlatformState->mpSysObj->ResetClipRegion();
        return;
    }

    aRegion.Move(-rWindow.GetOutDev()->GetDeviceOriginX(),
                 -rWindow.GetOutDev()->GetDeviceOriginY());

    // Set/update system object clip region
    RectangleVector aRectangles;
    aRegion.GetRegionRectangles(aRectangles);
    pPlatformState->mpSysObj->BeginSetClipRegion(aRectangles.size());

    for (auto const& rectangle : aRectangles)
    {
        pPlatformState->mpSysObj->UnionClipRegion(rectangle.Left(), rectangle.Top(),
                                                  rectangle.GetWidth(), rectangle.GetHeight());
    }

    pPlatformState->mpSysObj->EndSetClipRegion();
}

bool nativeObjectClip(vcl::Window& rWindow, const vcl::Region* pOldRegion)
{
    WindowPlatformState* pPlatformState = rWindow.ImplGetPlatformState();
    if (!pPlatformState || !pPlatformState->mpSysObj)
        return true;

    WindowClippingState* pClippingState = rWindow.ImplGetClippingState();

    if (!pOldRegion && !pClippingState->mbInitWinClipRegion)
        return true;

    // Call getWinChildClipRegion as a member function of WindowClippingState
    vcl::Region& rWinChildClipRegion = pClippingState->getWinChildClipRegion(rWindow);

    bool bUpdate = true;
    WindowVisibilityState* pVisibilityState = rWindow.ImplGetVisibilityState();

    if (syncNativeWindow(*pPlatformState, *pVisibilityState, rWinChildClipRegion, pOldRegion,
                         bUpdate))
        return bUpdate;

    lcl_updateNativeObjectClipRegion(rWindow, rWinChildClipRegion,
                                     vcl::Region(rWindow.GetOutputRectPixel()));

    pPlatformState->mpSysObj->Show(true);

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
    WindowClassification* pImpl = rWindow.ImplGetWindowClassification();
    if (!pImpl)
        return;

    if (pImpl->mbOverlapWin)
    {
        lcl_invalidateNativeClipTargets(&rWindow);
        return;
    }

    lcl_invalidateNativeClipTargets(&rWindow);

    WindowClippingState* pClippingState = rWindow.ImplGetClippingState();

    if (!pClippingState->mbClipSiblings)
        return;

    for (vcl::Window* pSibling : getFollowingSiblings(*rWindow.ImplGetWindowHierarchy()))
    {
        lcl_invalidateNativeClipTargets(pSibling);
    }
}

bool setClipFlagChildren(vcl::Window& rWindow, bool bSysObjOnlySmaller)
{
    WindowClippingState* pClippingState = rWindow.ImplGetClippingState();
    if (!pClippingState)
        return true;

    WindowPlatformState* pPlatformState = rWindow.ImplGetPlatformState();
    auto pOldRegion
        = getClipInvalidationRegion(*pPlatformState, *pClippingState, bSysObjOnlySmaller);

    dirtyInitClipRegion(rWindow);
    pClippingState->mbInitWinClipRegion = true;

    bool bUpdate = true;
    for (vcl::Window* pChild : getChildWindows(*rWindow.ImplGetWindowHierarchy()))
    {
        if (!setClipFlagChildren(*pChild, bSysObjOnlySmaller))
            bUpdate = false;
    }

    if (!pPlatformState || !pPlatformState->mpSysObj)
        return bUpdate;

    bool bClipSuccess = nativeObjectClip(rWindow, pOldRegion.get());

    auto[bNewUpdate, bInvalidateDevice] = pClippingState->processClipResult(bClipSuccess, bUpdate);
    bUpdate = bNewUpdate;

    if (bInvalidateDevice)
        dirtyInitClipRegion(rWindow);

    return bUpdate;
}

bool setClipFlag(vcl::Window& rWindow, bool bSysObjOnlySmaller)
{
    WindowHierarchy* pHierarchy = rWindow.ImplGetWindowHierarchy();
    if (!pHierarchy)
        return true;

    if (!rWindow.ImplIsOverlapWindow())
    {
        if (vcl::Window* pFrameWindow = pHierarchy->getFrameWindow())
            return setClipFlagOverlapWindows(*pFrameWindow, bSysObjOnlySmaller);

        return true;
    }

    bool bUpdate = setClipFlagChildren(rWindow, bSysObjOnlySmaller);

    vcl::Window* pParent = rWindow.ImplGetParent();
    WindowClippingState* pClippingState = rWindow.ImplGetClippingState();

    if (pParent && pClippingState)
    {
        WindowClippingState* pParentClippingState = pParent->ImplGetClippingState();
        if (pParentClippingState
            && invalidateParentClipIfRequired(*pClippingState, *pParentClippingState,
                                              pParent->GetStyle()))
        {
            dirtyInitClipRegion(*pParent);
        }
    }

    if (pClippingState && pClippingState->mbClipSiblings)
    {
        WindowHierarchy::forEachSibling(pHierarchy->getNextSibling(), [&](vcl::Window& rSibling) {
            if (!setClipFlagChildren(rSibling, bSysObjOnlySmaller))
                bUpdate = false;
        });
    }

    return bUpdate;
}

bool setClipFlagOverlapWindows(vcl::Window& rWindow, bool bSysObjOnlySmaller)
{
    WindowHierarchy* pHierarchy = rWindow.ImplGetWindowHierarchy();
    if (!pHierarchy)
        return true;

    bool bUpdate = setClipFlagChildren(rWindow, bSysObjOnlySmaller);

    for (vcl::Window* pWindow : getOverlapWindows(*pHierarchy))
    {
        if (!setClipFlagOverlapWindows(*pWindow, bSysObjOnlySmaller))
            bUpdate = false;
    }

    return bUpdate;
}

void calcOverlapRegionOverlaps(const vcl::Window& rWindow, const vcl::Region& rInterRegion,
                               vcl::Region& rRegion)
{
    const WindowHierarchy* pHierarchy = rWindow.ImplGetWindowHierarchy();
    if (!pHierarchy)
        return;

    // High-level ancestral sibling walk
    for (vcl::Window* pOverlapWin : getAncestralOverlapSiblings(const_cast<vcl::Window*>(&rWindow)))
    {
        accumulateWindowAndChildOverlaps(pOverlapWin, rInterRegion, rRegion);
    }

    // Child overlap window execution
    vcl::Window* pOverlapParent = !rWindow.ImplIsOverlapWindow()
                                      ? pHierarchy->getOverlapWindow()
                                      : const_cast<vcl::Window*>(&rWindow);
    if (pOverlapParent)
        accumulateChildOverlaps(pOverlapParent, rInterRegion, rRegion);
}

void calcOverlapRegion(vcl::Window& rWindow, const tools::Rectangle& rSourceRect,
                       vcl::Region& rRegion, bool bChildren, bool bSiblings)
{
    WindowClippingState* pClippingState = rWindow.ImplGetClippingState();
    if (!pClippingState)
        return;

    vcl::Region aRegion(rSourceRect);

    if (pClippingState->mbWinRegion)
    {
        rRegion.Intersect(
            rWindow.GetOutDev()->GetMapper().ViewToDevice(pClippingState->maWinRegion));
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
