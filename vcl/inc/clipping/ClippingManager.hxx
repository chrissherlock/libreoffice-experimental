/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <vcl/dllapi.h>
#include <vcl/window.hxx>

#include <clipping/ClipCompiler.hxx>
#include <clipping/ClipState.hxx>
#include <clipping/ClippingManager.hxx>

#include <map>
#include <memory>
#include <vector>

class OutputDevice;

namespace vcl::clipping
{
/**
 * ClippingManager acts as the coordinator for the reactive clipping graph.
 * It is owned by the OutputDevice and provides cached, compiled ClipPlans
 * for windows within its hierarchy.
 */
class VCL_DLLPUBLIC ClippingManager
{
public:
    explicit ClippingManager(vcl::Window& rRoot);
    ~ClippingManager();

    void OnWindowGeometryChanged(vcl::Window& rWindow);
    void OnWindowDestroyed(vcl::Window& rWindow);

    const ClipPlan& GetClipPlan(vcl::Window& rWindow);
    void InvalidateWindow(vcl::Window& rWindow);

    void UpdateNativeWindowClip(vcl::Window& rWindow);

    void CalcOverlapRegion(vcl::Window& rWindow, const tools::Rectangle& rSourceRect,
                           vcl::Region& rRegion, bool bChildren, bool bSiblings);

    void ClipToPaintRegion(OutputDevice& rDevice, tools::Rectangle& rDstRect);

    static void SetParentClipMode(vcl::Window* pWindow, ParentClipMode nMode);
    static ParentClipMode GetParentClipMode(const vcl::Window& rWindow);

private:
    struct CacheEntry
    {
        ClipPlan maPlan;
        sal_uInt64 mnLastCompiledEpoch = 0;
    };

    vcl::Window& mrRoot;
    std::map<vcl::Window*, CacheEntry> maCache;

    DECL_LINK(WindowEventHdl, VclWindowEvent&, void);

    /**
     * Removes the area occupied by the specified window from the provided region.
     * Handles both the window's bounding box and any custom window region set
     * via SetWindowRegionPixel().
     */
    void ExcludeWindowRegion(vcl::Window& rWindow, vcl::Region& rRegion);
};
} // namespace vcl::clipping

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
