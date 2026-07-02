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

#include <clipping/ClippingManager.hxx>
#include <clipping/ClipStateBuilder.hxx>
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

    // Lazy Compilation:
    // We only compile if the version counter has advanced.
    if (rWindow.GetClipStateVersion() > rEntry.mnLastCompiledVersion)
    {
        ClipState aState = ClipStateBuilder::Build(rWindow);

        rEntry.maPlan = ClipCompiler::Compile(aState);

        rEntry.mnLastCompiledVersion = rWindow.GetClipStateVersion();
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

void ClippingManager::OnWindowGeometryChanged(vcl::Window& rWindow)
{
    // Find the window in our active cache
    auto it = maCache.find(&rWindow);
    if (it != maCache.end())
    {
        // Explicitly mark the cached plan as completely stale.
        // Even though the window's internal version counter has incremented,
        // zeroing this out guarantees the next call to GetClipPlan()
        // will trigger a fresh ClipStateBuilder::Build() pass.
        it->second.mnLastCompiledVersion = 0;
    }
}

} // namespace vcl::clipping

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
