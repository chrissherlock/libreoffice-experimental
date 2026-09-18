/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <tools/gen.hxx>
#include <o3tl/typed_flags_set.hxx>

#include <vcl/dllapi.h>
#include <vcl/region.hxx>
#include <vcl/wintypes.hxx>

#include <memory>

namespace vcl
{
class Window;
}
struct WindowPlatformState;
struct WindowVisibilityState;

namespace vcl
{
struct NativeSyncStatus
{
    bool bUpdate;
    bool bInvalidateDevice;
};

class VCL_DLLPUBLIC WindowClippingState
{
public:
    // Core clipping state variables
    bool mbInitWinClipRegion = true;
    bool mbInitChildRegion = true;
    bool mbWinRegion = false;
    bool mbClipSiblings = false;
    bool mbClipChildren = false;
    ParentClipMode meParentClipMode = ParentClipMode::NONE;

    vcl::Region maWinRegion;
    vcl::Region maWinClipRegion;
    std::unique_ptr<vcl::Region> mpChildClipRegion;

    WindowClippingState() = default;
    ~WindowClippingState() = default;

    bool hasWindowRegion() const { return mbWinRegion; }
    const vcl::Region& getWindowRegion() const { return maWinRegion; }
    vcl::Region& getWindowRegion() { return maWinRegion; }
    bool containsWindowRegionPoint(const Point& rPoint) const
    {
        return maWinRegion.Contains(rPoint);
    }
    bool isInitNeeded() const { return mbInitWinClipRegion || mbInitChildRegion; }
    bool hasClipRegion() const { return !maWinClipRegion.IsEmpty(); }

    // -------------------------------------------------------------------
    // Initialization & Synchronization
    // -------------------------------------------------------------------
    bool initChildRegion(vcl::Window& rWindow);
    void initWinClipRegion(const vcl::Window& rWindow);
    void initWinChildClipRegion(const vcl::Window& rWindow);
    vcl::Region& getWinChildClipRegion(vcl::Window& rWindow);

    NativeSyncStatus processClipResult(bool bClipSuccess, bool bCurrentUpdate);
    bool syncNativeWindow(WindowPlatformState& rImpl, WindowVisibilityState& rVisibilityState,
                          vcl::Region& rWinChildClipRegion, const vcl::Region* pOldRegion,
                          bool& rOutUpdate) const;

    std::unique_ptr<vcl::Region> getClipInvalidationRegion(WindowPlatformState& rPlatformState,
                                                           bool bSysObjOnlySmaller) const;

    bool invalidateParentClipIfRequired(WindowClippingState& rParentImpl,
                                        WinBits nParentStyle) const;

    // -------------------------------------------------------------------
    // Accumulation
    // -------------------------------------------------------------------
    void accumulateParentBoundaries(vcl::Window& rWindow, const vcl::Region& rInterRegion,
                                    vcl::Region& rRegion) const;
    void accumulateSiblingBoundaries(vcl::Window& rWindow, const vcl::Region& rInterRegion,
                                     vcl::Region& rRegion, bool bSiblings) const;
    void accumulateChildBoundaries(vcl::Window& rWindow, const vcl::Region& rInterRegion,
                                   vcl::Region& rRegion) const;
    void accumulateChildOverlaps(vcl::Window* pWindow, const vcl::Region& rInterRegion,
                                 vcl::Region& rRegion) const;
    void accumulateWindowAndChildOverlaps(vcl::Window* pWindow, const vcl::Region& rInterRegion,
                                          vcl::Region& rRegion) const;

    // -------------------------------------------------------------------
    // Intersection & Exclusion
    // -------------------------------------------------------------------
    void intersectWindowRegion(const vcl::Window& rWindow, vcl::Region& rRegion) const;
    void excludeWindowRegion(const vcl::Window& rWindow, vcl::Region& rRegion) const;
    void excludeWindowAndOverlapRegions(vcl::Window& rWindow, vcl::Region& rRegion) const;
    void excludeOverlapWindows(const vcl::Window& rWindow, vcl::Region& rRegion) const;

    // -------------------------------------------------------------------
    // Clipping Execution
    // -------------------------------------------------------------------
    void clipAllChildren(const vcl::Window& rWindow, vcl::Region& rRegion) const;
    bool clipChildren(const vcl::Window& rWindow, vcl::Region& rRegion) const;
    void clipSiblings(const vcl::Window& rWindow, vcl::Region& rRegion) const;
    void clipBoundaries(const vcl::Window& rWindow, vcl::Region& rRegion, bool bThis,
                        bool bOverlaps);

    void setParentClipMode(vcl::Window& rWindow, ParentClipMode nMode);
    ParentClipMode getParentClipMode(const vcl::Window& rWindow) const;

    bool nativeObjectClip(vcl::Window& rWindow, const vcl::Region* pOldRegion);
    void updateNativeObjectClip(vcl::Window& rWindow);

    bool setClipFlagChildren(vcl::Window& rWindow, bool bSysObjOnlySmaller = false);
    bool setClipFlag(vcl::Window& rWindow, bool bSysObjOnlySmaller = false);
    bool setClipFlagOverlapWindows(vcl::Window& rWindow, bool bSysObjOnlySmaller = false);

    void calcOverlapRegionOverlaps(const vcl::Window& rWindow, const vcl::Region& rInterRegion,
                                   vcl::Region& rRegion) const;
    void calcOverlapRegion(vcl::Window& rWindow, const tools::Rectangle& rSourceRect,
                           vcl::Region& rRegion, bool bChildren, bool bSiblings) const;
};

} // namespace vcl

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
