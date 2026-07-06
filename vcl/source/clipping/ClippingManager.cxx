/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <vcl/window.hxx>
#include <vcl/vclevent.hxx>
#include <vcl/CoordinateMapper.hxx>

#include <clipping/ClippingManager.hxx>
#include <clipping/ClipStateBuilder.hxx>
#include <clipping/traits.hxx>
#include <devicedispatcher.hxx>
#include <salobj.hxx>
#include <window.h>

namespace vcl::clipping
{
ClippingManager::ClippingManager(vcl::Window& rRoot)
    : mrRoot(rRoot)
{
    mrRoot.AddEventListener(LINK(this, ClippingManager, WindowEventHdl));
}

ClippingManager::~ClippingManager()
{
    mrRoot.RemoveEventListener(LINK(this, ClippingManager, WindowEventHdl));
}

const ClipPlan& ClippingManager::GetClipPlan(vcl::Window& rWindow)
{
    auto& rEntry = maCache[&rWindow];

    // Get the global clock for this window hierarchy
    sal_uInt64 nCurrentEpoch = 1;
    if (WindowImpl* pImpl = rWindow.ImplGetWindowImpl())
    {
        if (pImpl->mpFrameData)
            nCurrentEpoch = pImpl->mpFrameData->mnClipGeometryEpoch;
    }

    // Lazy Compilation: Only rebuild if our cache is older than the frame's epoch
    if (nCurrentEpoch > rEntry.mnLastCompiledEpoch)
    {
        ClipState aState = ClipStateBuilder::Build(rWindow);
        rEntry.maPlan = ClipCompiler::Compile(aState);
        rEntry.mnLastCompiledEpoch = nCurrentEpoch;
    }

    return rEntry.maPlan;
}

void ClippingManager::InvalidateWindow(vcl::Window& rWindow) { maCache.erase(&rWindow); }

IMPL_LINK(ClippingManager, WindowEventHdl, VclWindowEvent&, rEvent, void)
{
    vcl::Window* pWindow = rEvent.GetWindow();
    if (!pWindow)
        return;

    switch (rEvent.GetId())
    {
        // Layout and Position changes (The "Hard" invalidations)
        case VclEventId::WindowShow:
        case VclEventId::WindowHide:
        case VclEventId::WindowMove:
        case VclEventId::WindowResize:
        case VclEventId::WindowDocking:
        case VclEventId::WindowEndDocking:
        {
            // Trigger the version increment on the window itself.
            // This propagates up the tree to the root.
            pWindow->InvalidateClipState();
            break;
        }

        // Cleanup: Handle the destruction of nodes in our dependency graph.
        case VclEventId::WindowChildDestroyed:
        {
            // Only clean up if this window is in our cache.
            // This is safer than calling OnWindowDestroyed globally.
            OnWindowDestroyed(*pWindow);
            break;
        }

        default:
            // All other VclEventIds are ignored.
            break;
    }
}

void ClippingManager::OnWindowDestroyed(vcl::Window& rWindow) { maCache.erase(&rWindow); }

void ClippingManager::OnWindowGeometryChanged(vcl::Window& rWindow) { maCache.erase(&rWindow); }

void ClippingManager::UpdateNativeWindowClip(vcl::Window& rWindow)
{
    WindowImpl* pImpl = rWindow.ImplGetWindowImpl();
    if (!pImpl)
        return;

    if (!pImpl->mpSysObj)
    {
        vcl::Window* pChild = pImpl->mpHierarchy->mpFirstChild;
        while (pChild)
        {
            UpdateNativeWindowClip(*pChild);
            pChild = pChild->ImplGetWindowImpl()->mpHierarchy->mpNext;
        }

        vcl::Window* pOverlap = pImpl->mpHierarchy->mpFirstOverlap;
        while (pOverlap)
        {
            UpdateNativeWindowClip(*pOverlap);
            pOverlap = pOverlap->ImplGetWindowImpl()->mpHierarchy->mpNextOverlap;
        }
        return;
    }

    const auto& rPlan = GetClipPlan(rWindow);

    if (!pImpl->mbReallyVisible || rPlan.mbEmpty || rPlan.maFinalRegion.IsEmpty())
    {
        pImpl->mpSysObj->Show(false);
        return;
    }

    vcl::Region aTargetRegion = rPlan.maFinalRegion;
    vcl::Region rWinRectRegion(rWindow.GetOutputRectPixel());

    if (aTargetRegion == rWinRectRegion)
    {
        pImpl->mpSysObj->ResetClipRegion();
        pImpl->mpSysObj->Show(true);
        return;
    }

    aTargetRegion.Move(-rWindow.GetOutDev()->GetDeviceOriginX(),
                       -rWindow.GetOutDev()->GetDeviceOriginY());

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

void ClippingManager::CalcOverlapRegion(vcl::Window& rWindow, const tools::Rectangle& rSourceRect,
                                        vcl::Region& rRegion, bool bChildren, bool bSiblings)
{
    ClipState aState = ClipStateBuilder::Build(rWindow, ClipSpace::AbsoluteDevice);
    vcl::Region aBase(rSourceRect);

    if (aState.maCustomRegion)
        rRegion.Intersect(*aState.maCustomRegion);

    // maSiblings contains ALL overlaps (ancestral and lateral)
    if (bSiblings)
    {
        for (const auto& rSibling : aState.maSiblings)
        {
            vcl::Region aTemp(aBase);
            aTemp.Intersect(rSibling.maBounds);
            rRegion.Union(aTemp);
        }
    }

    if (bChildren)
    {
        for (const auto& rChild : aState.maChildren)
        {
            vcl::Region aTemp(aBase);
            aTemp.Intersect(rChild.maBounds);
            rRegion.Union(aTemp);
        }
    }
}

void ClippingManager::ClipBoundaries(vcl::Window& rWindow, vcl::Region& rRegion, bool bThis,
                                     bool bOverlaps)
{
    ClipState aState = ClipStateBuilder::Build(rWindow, ClipSpace::AbsoluteDevice);

    if (bThis)
    {
        rRegion.Intersect(aState.maBounds);
        if (aState.maCustomRegion)
            rRegion.Intersect(*aState.maCustomRegion);

        if (bOverlaps)
        {
            for (const auto& rSibling : aState.maSiblings)
                rRegion.Exclude(rSibling.maBounds);
        }
        return;
    }

    if (!rWindow.ImplIsOverlapWindow())
    {
        vcl::Window* pParent = rWindow.ImplGetParent();
        if (pParent)
        {
            ClipState aParentState = ClipStateBuilder::Build(*pParent, ClipSpace::AbsoluteDevice);
            rRegion.Intersect(aParentState.maBounds);
            if (aParentState.maCustomRegion)
                rRegion.Intersect(*aParentState.maCustomRegion);
        }
        return;
    }

    WindowImpl* pImpl = rWindow.ImplGetWindowImpl();
    if (!pImpl->mbFrame && pImpl->mpFrameWindow)
    {
        rRegion.Intersect(
            tools::Rectangle(Point(0, 0), pImpl->mpFrameWindow->GetOutputSizePixel()));
    }

    if (!bOverlaps || rRegion.IsEmpty())
        return;

    for (const auto& rSibling : aState.maSiblings)
        rRegion.Exclude(rSibling.maBounds);
}

void ClippingManager::ClipToPaintRegion(OutputDevice& rDevice, tools::Rectangle& rDstRect)
{
    vcl::DispatchDevice(rDevice, [&rDstRect](auto& rTypedDev) {
        using T = std::decay_t<decltype(rTypedDev)>;

        // Compile-time trait verification happens right here in the manager
        if constexpr (has_hierarchical_clipping_v<T>)
        {
            const vcl::Region aPaintRgn(rTypedDev.GetOwnerWindow()->GetPaintRegion());
            if (aPaintRgn.IsNull())
                return;

            auto aBoundRect = vcl::LogicRect(aPaintRgn.GetBoundRect());
            auto aWindowRect
                = rTypedDev.template convertTo<vcl::WindowRect>(aBoundRect, rTypedDev.GetMapMode())
                      .get();

            rDstRect.Intersection(aWindowRect);
        }
    });
}

void ClippingManager::ExcludeWindowRegion(vcl::Window& rWindow, vcl::Region& rRegion)
{
    vcl::Region aWindowRegion(rWindow.GetOutputRectPixel());

    if (aWindowRegion.IsEmpty())
        return;

    WindowImpl* pImpl = rWindow.ImplGetWindowImpl();
    if (pImpl && pImpl->mpClippingState->mbWinRegion)
    {
        aWindowRegion.Intersect(
            rWindow.GetOutDev()->GetMapper().ViewToDevice(pImpl->mpClippingState->maWinRegion));
    }

    rRegion.Exclude(aWindowRegion);
}

void ClippingManager::SetParentClipMode(vcl::Window* pWindow, ParentClipMode nMode)
{
    if (!pWindow)
        return;

    WindowImpl* pImpl = pWindow->ImplGetWindowImpl();

    if (pImpl->mpBorderWindow)
    {
        SetParentClipMode(pImpl->mpBorderWindow.get(), nMode);
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

ParentClipMode ClippingManager::GetParentClipMode(const vcl::Window& rWindow)
{
    WindowImpl* pWindowImpl = rWindow.ImplGetWindowImpl();

    if (pWindowImpl->mpBorderWindow)
        return GetParentClipMode(*pWindowImpl->mpBorderWindow);

    return pWindowImpl->mpClippingState->meParentClipMode;
}

} // namespace vcl::clipping

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
