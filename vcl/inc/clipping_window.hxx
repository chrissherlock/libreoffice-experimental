/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

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
#include <vector>
#include <memory>

class WindowImpl;

namespace vcl
{
class Region;
}

namespace vcl::clipping
{
// =========================================================================
// PUBLIC ORCHESTRATION (The "Drivers")
// =========================================================================

VCL_DLLPUBLIC void calcOverlapRegion(vcl::Window& rWindow, const tools::Rectangle& rSourceRect,
                                     vcl::Region& rRegion, bool bChildren, bool bSiblings);

VCL_DLLPUBLIC void initWinChildClipRegion(const vcl::Window& rWindow);
VCL_DLLPUBLIC void initWinClipRegion(const vcl::Window& rWindow);

// =========================================================================
// GEOMETRIC ACCUMULATORS (The "Math Engines")
// =========================================================================

// Restored to bool return to satisfy paint.cxx requirements
VCL_DLLPUBLIC bool clipChildren(const vcl::Window& rWindow, vcl::Region& rRegion);
VCL_DLLPUBLIC void clipAllChildren(const vcl::Window& rWindow, vcl::Region& rRegion);
VCL_DLLPUBLIC void clipSiblings(const vcl::Window& rWindow, vcl::Region& rRegion);
VCL_DLLPUBLIC void clipBoundaries(const vcl::Window& rWindow, vcl::Region& rRegion, bool bThis,
                                  bool bOverlaps);

VCL_DLLPUBLIC void excludeWindowRegion(vcl::Window& rWindow, vcl::Region& rRegion);
VCL_DLLPUBLIC void excludeOverlapWindows(const vcl::Window& rWindow, vcl::Region& rRegion);
VCL_DLLPUBLIC void intersectWindowRegion(vcl::Window& rWindow, vcl::Region& rRegion);

VCL_DLLPUBLIC void accumulateParentBoundaries(vcl::Window& rWindow, const vcl::Region& rInterRegion,
                                              vcl::Region& rRegion);
VCL_DLLPUBLIC void accumulateSiblingBoundaries(vcl::Window& rWindow,
                                               const vcl::Region& rInterRegion,
                                               vcl::Region& rRegion, bool bSiblings);
VCL_DLLPUBLIC void accumulateChildBoundaries(vcl::Window& rWindow, const vcl::Region& rInterRegion,
                                             vcl::Region& rRegion);
VCL_DLLPUBLIC void accumulateWindowAndChildOverlaps(vcl::Window* pWindow,
                                                    const vcl::Region& rInterRegion,
                                                    vcl::Region& rRegion);
VCL_DLLPUBLIC void accumulateChildOverlaps(vcl::Window* pWindow, const vcl::Region& rInterRegion,
                                           vcl::Region& rRegion);

// =========================================================================
// INTERNAL SYNC & HELPERS (Platform Integration & Window Tree Walkers)
// =========================================================================

inline void dirtyInitClipRegion(vcl::Window& rWindow) { rWindow.GetOutDev()->SetClipRegion(); }

struct NativeSyncStatus
{
    bool bUpdate;
    bool bInvalidateDevice;
};

VCL_DLLPUBLIC void updateNativeObjectClip(vcl::Window& rWindow);

VCL_DLLPUBLIC void setParentClipMode(vcl::Window* pWindow, ParentClipMode nMode);
VCL_DLLPUBLIC ParentClipMode getParentClipMode(const vcl::Window& rWindow);
VCL_DLLPUBLIC vcl::Region& getWinChildClipRegion(vcl::Window& rWindow);

VCL_DLLPUBLIC std::vector<vcl::Window*> getChildWindows(const WindowImpl& rImpl);
VCL_DLLPUBLIC std::vector<vcl::Window*> getOverlapWindows(const WindowImpl& rImpl);
VCL_DLLPUBLIC std::vector<vcl::Window*> getFollowingSiblings(const WindowImpl& rImpl);
VCL_DLLPUBLIC std::vector<vcl::Window*> getAncestralOverlapSiblings(vcl::Window* pStartWindow);

VCL_DLLPUBLIC bool initChildRegion(WindowImpl& rImpl);

void calcOverlapRegionOverlaps(const vcl::Window& rWindow, const vcl::Region& rInterRegion,
                               vcl::Region& rRegion);

} // namespace vcl::clipping

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
