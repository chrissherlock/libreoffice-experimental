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

#include <clipping.hxx>
#include <window.h>
#include <salobj.hxx>

namespace vcl::clipping
{
bool initChildRegion(WindowImpl& rImpl)
{
    // Safely clear the initialization flag on function exit
    comphelper::ScopeGuard aDeinitChildRegion([&rImpl]() { rImpl.mbInitChildRegion = false; });

    if (!rImpl.mpFirstChild)
    {
        rImpl.mpChildClipRegion.reset();
        return false; // No children present; skip downstream clipping
    }

    if (!rImpl.mpChildClipRegion)
        rImpl.mpChildClipRegion.reset(new vcl::Region(rImpl.maWinClipRegion));
    else
        *rImpl.mpChildClipRegion = rImpl.maWinClipRegion;

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
    if (rImpl.mpSysObj && bSysObjOnlySmaller && !rImpl.mbInitWinClipRegion)
        return std::make_unique<vcl::Region>(rImpl.maWinClipRegion);

    return nullptr;
}

bool invalidateParentClipIfRequired(const WindowImpl& rChildImpl, WindowImpl& rParentImpl,
                                    WinBits nParentStyle)
{
    if ((nParentStyle & WB_CLIPCHILDREN) || (rChildImpl.mnParentClipMode & ParentClipMode::Clip))
    {
        rParentImpl.mbInitChildRegion = true;
        return true; // Signals that parent device clip region needs invalidation
    }

    return false;
}

NativeSyncStatus processClipResult(WindowImpl& rImpl, bool bClipSuccess, bool bCurrentUpdate)
{
    if (!bClipSuccess)
    {
        rImpl.mbInitWinClipRegion = true;
        return { false, true }; // bUpdate = false, bInvalidateDevice = true
    }

    return { bCurrentUpdate, false }; // Unchanged state
}

std::vector<vcl::Window*> getChildWindows(const WindowImpl& rImpl)
{
    std::vector<vcl::Window*> aChildren;
    vcl::Window* pChild = rImpl.mpFirstChild;

    while (pChild)
    {
        aChildren.push_back(pChild);
        pChild = pChild->ImplGetWindowImpl()->mpNext;
    }

    return aChildren;
}

std::vector<vcl::Window*> getOverlapWindows(const WindowImpl& rImpl)
{
    std::vector<vcl::Window*> aOverlaps;
    vcl::Window* pOverlap = rImpl.mpFirstOverlap;

    while (pOverlap)
    {
        aOverlaps.push_back(pOverlap);
        pOverlap = pOverlap->ImplGetWindowImpl()->mpNext;
    }

    return aOverlaps;
}

std::vector<vcl::Window*> getFollowingSiblings(const WindowImpl& rImpl)
{
    std::vector<vcl::Window*> aSiblings;
    vcl::Window* pSibling = rImpl.mpNext;

    while (pSibling)
    {
        aSiblings.push_back(pSibling);
        pSibling = pSibling->ImplGetWindowImpl()->mpNext; // Fixes private visibility error
    }

    return aSiblings;
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

} // namespace vcl::clipping

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
