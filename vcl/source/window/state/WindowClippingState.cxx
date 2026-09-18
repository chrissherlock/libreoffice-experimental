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
#include <window/state/WindowClassification.hxx>
#include <window/state/WindowVisibilityState.hxx>
#include <window/state/WindowClippingState.hxx>
#include <window/state/WindowHierarchy.hxx>
#include <window/state/WindowPlatformState.hxx>
#include <salobj.hxx>

namespace vcl
{
namespace
{
/** Linearly traverses the sibling chain starting from pStartWindow until the end. */
std::vector<vcl::Window*> lcl_gatherWindowChain(vcl::Window* pStartWindow)
{
    std::vector<vcl::Window*> aWindows;

    WindowHierarchy::forEachSibling(pStartWindow,
                                    [&](vcl::Window& rWindow) { aWindows.push_back(&rWindow); });

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
            WindowHierarchy::forEachSibling(pParentHierarchy->getFirstOverlap(),
                                            [&](vcl::Window& rSibling) -> bool {
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

    if (auto* pHierarchy = pWindow->ImplGetWindowHierarchy())
    {
        for (vcl::Window* pChild : getChildWindows(*pHierarchy))
        {
            gatherNativeSyncTargets(pChild, rTargets);
        }

        for (vcl::Window* pOverlap : getOverlapWindows(*pHierarchy))
        {
            gatherNativeSyncTargets(pOverlap, rTargets);
        }
    }
}

bool lcl_IsParentClipRequired(ParentClipMode nClipMode, WinBits nStyle)
{
    return !(nClipMode & ParentClipMode::NoClip)
           && ((nClipMode & ParentClipMode::Clip) || (nStyle & WB_CLIPCHILDREN));
}

void lcl_updateNativeObjectClipRegion(vcl::Window& rWindow, vcl::Region aRegion,
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

void lcl_invalidateNativeClipTargets(vcl::Window* pStartWindow)
{
    if (!pStartWindow)
        return;

    std::vector<vcl::Window*> aTargets;
    gatherNativeSyncTargets(pStartWindow, aTargets);

    for (vcl::Window* pTarget : aTargets)
    {
        if (auto* pClipState = pTarget->ImplGetClippingState())
            pClipState->nativeObjectClip(*pTarget, nullptr);
    }
}
}

// -----------------------------------------------------------------------
// WindowClippingState implementations
// -----------------------------------------------------------------------

bool WindowClippingState::initChildRegion(vcl::Window& rWindow)
{
    // Safely clear the initialization flag on function exit
    comphelper::ScopeGuard aDeinitChildRegion([this]() { mbInitChildRegion = false; });

    WindowHierarchy* pHierarchy = rWindow.ImplGetWindowHierarchy();
    if (!pHierarchy || !pHierarchy->hasChildren())
    {
        mpChildClipRegion.reset();
        return false; // No children present; skip downstream clipping
    }

    if (!mpChildClipRegion)
        mpChildClipRegion = std::make_unique<vcl::Region>(maWinClipRegion);
    else
        *mpChildClipRegion = maWinClipRegion;

    return true; // Context contains children; signal the window to clip them
}

void WindowClippingState::initWinClipRegion(const vcl::Window& rWindow)
{
    if (!mbInitWinClipRegion)
        return; // Already initialized; bypass calculation pass

    // Establish baseline viewport bounds
    maWinClipRegion = rWindow.GetOutputRectPixel();
    if (mbWinRegion)
    {
        maWinClipRegion.Intersect(rWindow.GetOutDev()->GetMapper().ViewToDevice(maWinRegion));
    }

    // Intersect against preceding elements in the Z-order stack
    if (mbClipSiblings && !rWindow.ImplIsOverlapWindow())
        clipSiblings(rWindow, maWinClipRegion);

    clipBoundaries(rWindow, maWinClipRegion, false, true);

    if ((rWindow.GetStyle() & WB_CLIPCHILDREN) || mbClipChildren)
        mbInitChildRegion = true;

    mbInitWinClipRegion = false;
}

void WindowClippingState::initWinChildClipRegion(const vcl::Window& rWindow)
{
    if (initChildRegion(const_cast<vcl::Window&>(rWindow)))
        clipChildren(rWindow, *mpChildClipRegion);
}

vcl::Region& WindowClippingState::getWinChildClipRegion(vcl::Window& rWindow)
{
    if (mbInitWinClipRegion)
        initWinClipRegion(rWindow);

    if (mbInitChildRegion)
        initWinChildClipRegion(rWindow);

    if (mpChildClipRegion)
        return *mpChildClipRegion;

    return maWinClipRegion;
}

NativeSyncStatus WindowClippingState::processClipResult(bool bClipSuccess, bool bCurrentUpdate)
{
    if (!bClipSuccess)
    {
        mbInitWinClipRegion = true;
        return { false, true }; // bUpdate = false, bInvalidateDevice = true
    }

    return { bCurrentUpdate, false }; // Unchanged state
}

bool WindowClippingState::syncNativeWindow(WindowPlatformState& rImpl,
                                           WindowVisibilityState& rVisibilityState,
                                           vcl::Region& rWinChildClipRegion,
                                           const vcl::Region* pOldRegion, bool& rOutUpdate) const
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

std::unique_ptr<vcl::Region>
WindowClippingState::getClipInvalidationRegion(WindowPlatformState& rPlatformState,
                                               bool bSysObjOnlySmaller) const
{
    if (rPlatformState.mpSysObj && bSysObjOnlySmaller && !mbInitWinClipRegion)
        return std::make_unique<vcl::Region>(maWinClipRegion);

    return nullptr;
}

bool WindowClippingState::invalidateParentClipIfRequired(WindowClippingState& rParentImpl,
                                                         WinBits nParentStyle) const
{
    if ((nParentStyle & WB_CLIPCHILDREN) || (meParentClipMode & ParentClipMode::Clip))
    {
        rParentImpl.mbInitChildRegion = true;
        return true; // Signals that parent device clip region needs invalidation
    }

    return false;
}

void WindowClippingState::accumulateParentBoundaries(vcl::Window& rWindow,
                                                     const vcl::Region& rInterRegion,
                                                     vcl::Region& rRegion) const
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

            if (auto* pAncestorClip = rAncestor.ImplGetClippingState())
                pAncestorClip->excludeWindowRegion(rAncestor, aTempRegion);

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
        const WindowClassification* pClassification
            = pBoundaryWindow->ImplGetWindowClassification();
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

void WindowClippingState::accumulateSiblingBoundaries(vcl::Window& rWindow,
                                                      const vcl::Region& rInterRegion,
                                                      vcl::Region& rRegion, bool bSiblings) const
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
            if (auto* pSiblingClip = pSibling->ImplGetClippingState())
            {
                aTempRegion = rInterRegion;
                pSiblingClip->intersectWindowRegion(*pSibling, aTempRegion);
                rRegion.Union(aTempRegion);
            }
        }
    }
}

void WindowClippingState::accumulateChildBoundaries(vcl::Window& rWindow,
                                                    const vcl::Region& rInterRegion,
                                                    vcl::Region& rRegion) const
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
            if (auto* pChildClip = pChild->ImplGetClippingState())
            {
                aTempRegion = rInterRegion;
                pChildClip->intersectWindowRegion(*pChild, aTempRegion);
                rRegion.Union(aTempRegion);
            }
        }
    }
}

void WindowClippingState::accumulateChildOverlaps(vcl::Window* pWindow,
                                                  const vcl::Region& rInterRegion,
                                                  vcl::Region& rRegion) const
{
    if (auto* pHierarchy = pWindow->ImplGetWindowHierarchy())
    {
        for (vcl::Window* pOverlap : getOverlapWindows(*pHierarchy))
        {
            if (auto* pOverlapClip = pOverlap->ImplGetClippingState())
                pOverlapClip->accumulateWindowAndChildOverlaps(pOverlap, rInterRegion, rRegion);
        }
    }
}

void WindowClippingState::accumulateWindowAndChildOverlaps(vcl::Window* pWindow,
                                                           const vcl::Region& rInterRegion,
                                                           vcl::Region& rRegion) const
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

void WindowClippingState::intersectWindowRegion(const vcl::Window& rWindow,
                                                vcl::Region& rRegion) const
{
    // First, clip to the base rectangular output boundary
    rRegion.Intersect(rWindow.GetOutputRectPixel());

    // If the window has a custom user-defined geometric clip path, apply it as well
    if (mbWinRegion)
    {
        rRegion.Intersect(rWindow.GetOutDev()->GetMapper().ViewToDevice(maWinRegion));
    }
}

void WindowClippingState::excludeWindowRegion(const vcl::Window& rWindow,
                                              vcl::Region& rRegion) const
{
    // If the target window has a custom boundary path, extract its intersection block
    if (mbWinRegion)
    {
        vcl::Region aRegion(rWindow.GetOutputRectPixel());
        aRegion.Intersect(rWindow.GetOutDev()->GetMapper().ViewToDevice(maWinRegion));
        rRegion.Exclude(aRegion);
    }
    else
    {
        // Otherwise, simply exclude the standard bounding box
        rRegion.Exclude(rWindow.GetOutputRectPixel());
    }
}

void WindowClippingState::excludeWindowAndOverlapRegions(vcl::Window& rWindow,
                                                         vcl::Region& rRegion) const
{
    if (rWindow.ImplGetVisibilityState()->mbReallyVisible)
        excludeWindowRegion(rWindow, rRegion);

    excludeOverlapWindows(rWindow, rRegion);
}

void WindowClippingState::clipAllChildren(const vcl::Window& rWindow, vcl::Region& rRegion) const
{
    if (auto* pHierarchy = rWindow.ImplGetWindowHierarchy())
    {
        for (vcl::Window* pChild : getChildWindows(*pHierarchy))
        {
            if (pChild->ImplGetVisibilityState()->mbReallyVisible)
            {
                if (auto* pChildClip = pChild->ImplGetClippingState())
                    pChildClip->excludeWindowRegion(*pChild, rRegion);
            }
        }
    }
}

bool WindowClippingState::clipChildren(const vcl::Window& rWindow, vcl::Region& rRegion) const
{
    bool bOtherClip = false;
    WinBits nParentStyle = rWindow.GetStyle();

    if (auto* pHierarchy = rWindow.ImplGetWindowHierarchy())
    {
        for (vcl::Window* pChild : getChildWindows(*pHierarchy))
        {
            if (pChild->ImplGetVisibilityState()->mbReallyVisible)
            {
                if (auto* pChildClip = pChild->ImplGetClippingState())
                {
                    ParentClipMode nClipMode = pChildClip->getParentClipMode(*pChild);

                    if (lcl_IsParentClipRequired(nClipMode, nParentStyle))
                        pChildClip->excludeWindowRegion(*pChild, rRegion);
                    else
                        bOtherClip = true;
                }
            }
        }
    }

    return bOtherClip;
}

void WindowClippingState::clipSiblings(const vcl::Window& rWindow, vcl::Region& rRegion) const
{
    vcl::Window* pParent = rWindow.ImplGetParent();
    if (!pParent)
        return;

    if (auto* pHierarchy = pParent->ImplGetWindowHierarchy())
    {
        for (vcl::Window* pSibling : getChildWindows(*pHierarchy))
        {
            if (pSibling == &rWindow)
                break; // We only clip against preceding siblings

            if (pSibling->ImplGetVisibilityState()->mbReallyVisible)
            {
                if (auto* pSiblingClip = pSibling->ImplGetClippingState())
                    pSiblingClip->excludeWindowRegion(*pSibling, rRegion);
            }
        }
    }
}

void WindowClippingState::excludeOverlapWindows(const vcl::Window& rWindow,
                                                vcl::Region& rRegion) const
{
    if (auto* pHierarchy = rWindow.ImplGetWindowHierarchy())
    {
        for (vcl::Window* pOverlap : getOverlapWindows(*pHierarchy))
        {
            if (pOverlap->ImplGetVisibilityState()->mbReallyVisible)
            {
                if (auto* pOverlapClip = pOverlap->ImplGetClippingState())
                {
                    pOverlapClip->excludeWindowRegion(*pOverlap, rRegion);
                    pOverlapClip->excludeOverlapWindows(*pOverlap, rRegion);
                }
            }
        }
    }
}

void WindowClippingState::clipBoundaries(const vcl::Window& rWindow, vcl::Region& rRegion,
                                         bool bThis, bool bOverlaps)
{
    if (bThis)
    {
        if (mbInitWinClipRegion)
            initWinClipRegion(rWindow);

        rRegion.Intersect(maWinClipRegion);
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
                    pParentClippingState->initWinClipRegion(*pParent);

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
        {
            if (auto* pOverlapClip = pOverlapWin->ImplGetClippingState())
                pOverlapClip->excludeWindowRegion(*pOverlapWin, rRegion);
        }

        if (auto* pOverlapClip = pOverlapWin->ImplGetClippingState())
            pOverlapClip->excludeOverlapWindows(*pOverlapWin, rRegion);
    }

    excludeOverlapWindows(rWindow, rRegion);
}

void WindowClippingState::setParentClipMode(vcl::Window& rWindow, ParentClipMode nMode)
{
    WindowHierarchy* pHierarchy = rWindow.ImplGetWindowHierarchy();
    if (!pHierarchy)
        return;

    if (pHierarchy->hasBorderWindow())
    {
        if (auto* pBorderClip = pHierarchy->getBorderWindow()->ImplGetClippingState())
            pBorderClip->setParentClipMode(*pHierarchy->getBorderWindow(), nMode);
        return;
    }

    if (rWindow.ImplIsOverlapWindow())
        return;

    meParentClipMode = nMode;

    if ((nMode & ParentClipMode::Clip) && pHierarchy->getParent())
    {
        if (WindowClippingState* pParentClippingState
            = pHierarchy->getParent()->ImplGetClippingState())
            pParentClippingState->mbClipChildren = true;
    }
}

ParentClipMode WindowClippingState::getParentClipMode(const vcl::Window& rWindow) const
{
    const WindowHierarchy* pHierarchy = rWindow.ImplGetWindowHierarchy();
    if (pHierarchy && pHierarchy->hasBorderWindow())
    {
        if (auto* pBorderClip = pHierarchy->getBorderWindow()->ImplGetClippingState())
            return pBorderClip->getParentClipMode(*pHierarchy->getBorderWindow());
    }

    return meParentClipMode;
}

bool WindowClippingState::nativeObjectClip(vcl::Window& rWindow, const vcl::Region* pOldRegion)
{
    WindowPlatformState* pPlatformState = rWindow.ImplGetPlatformState();
    if (!pPlatformState || !pPlatformState->mpSysObj)
        return true;

    if (!pOldRegion && !mbInitWinClipRegion)
        return true;

    vcl::Region& rWinChildClipRegion = getWinChildClipRegion(rWindow);

    bool bUpdate = true;
    WindowVisibilityState* pVisibilityState = rWindow.ImplGetVisibilityState();

    if (pVisibilityState
        && syncNativeWindow(*pPlatformState, *pVisibilityState, rWinChildClipRegion, pOldRegion,
                            bUpdate))
        return bUpdate;

    lcl_updateNativeObjectClipRegion(rWindow, rWinChildClipRegion,
                                     vcl::Region(rWindow.GetOutputRectPixel()));

    pPlatformState->mpSysObj->Show(true);

    return bUpdate;
}

void WindowClippingState::updateNativeObjectClip(vcl::Window& rWindow)
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

    if (!mbClipSiblings)
        return;

    if (auto* pHierarchy = rWindow.ImplGetWindowHierarchy())
    {
        for (vcl::Window* pSibling : getFollowingSiblings(*pHierarchy))
        {
            lcl_invalidateNativeClipTargets(pSibling);
        }
    }
}

bool WindowClippingState::setClipFlagChildren(vcl::Window& rWindow, bool bSysObjOnlySmaller)
{
    WindowPlatformState* pPlatformState = rWindow.ImplGetPlatformState();
    if (!pPlatformState)
        return true;

    auto pOldRegion = getClipInvalidationRegion(*pPlatformState, bSysObjOnlySmaller);

    // Using the internal flag setter equivalent to dirtyInitClipRegion
    mbInitWinClipRegion = true;

    bool bUpdate = true;
    if (auto* pHierarchy = rWindow.ImplGetWindowHierarchy())
    {
        for (vcl::Window* pChild : getChildWindows(*pHierarchy))
        {
            if (auto* pChildClip = pChild->ImplGetClippingState())
            {
                if (!pChildClip->setClipFlagChildren(*pChild, bSysObjOnlySmaller))
                    bUpdate = false;
            }
        }
    }

    if (!pPlatformState->mpSysObj)
        return bUpdate;

    bool bClipSuccess = nativeObjectClip(rWindow, pOldRegion.get());

    auto[bNewUpdate, bInvalidateDevice] = processClipResult(bClipSuccess, bUpdate);
    bUpdate = bNewUpdate;

    if (bInvalidateDevice)
    {
        mbInitWinClipRegion = true;
    }

    return bUpdate;
}

bool WindowClippingState::setClipFlag(vcl::Window& rWindow, bool bSysObjOnlySmaller)
{
    WindowHierarchy* pHierarchy = rWindow.ImplGetWindowHierarchy();
    if (!pHierarchy)
        return true;

    if (!rWindow.ImplIsOverlapWindow())
    {
        if (vcl::Window* pFrameWindow = pHierarchy->getFrameWindow())
        {
            if (auto* pFrameClip = pFrameWindow->ImplGetClippingState())
                return pFrameClip->setClipFlagOverlapWindows(*pFrameWindow, bSysObjOnlySmaller);
        }

        return true;
    }

    bool bUpdate = setClipFlagChildren(rWindow, bSysObjOnlySmaller);

    vcl::Window* pParent = rWindow.ImplGetParent();

    if (pParent)
    {
        WindowClippingState* pParentClippingState = pParent->ImplGetClippingState();
        if (pParentClippingState
            && invalidateParentClipIfRequired(*pParentClippingState, pParent->GetStyle()))
        {
            pParentClippingState->mbInitWinClipRegion = true;
        }
    }

    if (mbClipSiblings)
    {
        WindowHierarchy::forEachSibling(pHierarchy->getNextSibling(), [&](vcl::Window& rSibling) {
            if (auto* pSiblingClip = rSibling.ImplGetClippingState())
            {
                if (!pSiblingClip->setClipFlagChildren(rSibling, bSysObjOnlySmaller))
                    bUpdate = false;
            }
        });
    }

    return bUpdate;
}

bool WindowClippingState::setClipFlagOverlapWindows(vcl::Window& rWindow, bool bSysObjOnlySmaller)
{
    WindowHierarchy* pHierarchy = rWindow.ImplGetWindowHierarchy();
    if (!pHierarchy)
        return true;

    bool bUpdate = setClipFlagChildren(rWindow, bSysObjOnlySmaller);

    for (vcl::Window* pWindow : getOverlapWindows(*pHierarchy))
    {
        if (auto* pOverlapClip = pWindow->ImplGetClippingState())
        {
            if (!pOverlapClip->setClipFlagOverlapWindows(*pWindow, bSysObjOnlySmaller))
                bUpdate = false;
        }
    }

    return bUpdate;
}

void WindowClippingState::calcOverlapRegionOverlaps(const vcl::Window& rWindow,
                                                    const vcl::Region& rInterRegion,
                                                    vcl::Region& rRegion) const
{
    const WindowHierarchy* pHierarchy = rWindow.ImplGetWindowHierarchy();
    if (!pHierarchy)
        return;

    // High-level ancestral sibling walk
    for (vcl::Window* pOverlapWin : getAncestralOverlapSiblings(const_cast<vcl::Window*>(&rWindow)))
    {
        if (auto* pOverlapClip = pOverlapWin->ImplGetClippingState())
            pOverlapClip->accumulateWindowAndChildOverlaps(pOverlapWin, rInterRegion, rRegion);
    }

    // Child overlap window execution
    vcl::Window* pOverlapParent = !rWindow.ImplIsOverlapWindow()
                                      ? pHierarchy->getOverlapWindow()
                                      : const_cast<vcl::Window*>(&rWindow);
    if (pOverlapParent)
    {
        if (auto* pParentClip = pOverlapParent->ImplGetClippingState())
            pParentClip->accumulateChildOverlaps(pOverlapParent, rInterRegion, rRegion);
    }
}

void WindowClippingState::calcOverlapRegion(vcl::Window& rWindow,
                                            const tools::Rectangle& rSourceRect,
                                            vcl::Region& rRegion, bool bChildren,
                                            bool bSiblings) const
{
    vcl::Region aRegion(rSourceRect);

    if (mbWinRegion)
    {
        rRegion.Intersect(rWindow.GetOutDev()->GetMapper().ViewToDevice(maWinRegion));
    }

    calcOverlapRegionOverlaps(rWindow, aRegion, rRegion);
    accumulateParentBoundaries(rWindow, aRegion, rRegion);
    accumulateSiblingBoundaries(rWindow, aRegion, rRegion, bSiblings);

    if (!bChildren)
        return;

    accumulateChildBoundaries(rWindow, aRegion, rRegion);
}

} // namespace vcl
/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
