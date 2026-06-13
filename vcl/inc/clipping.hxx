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

class WindowImpl;

namespace vcl
{
class Region;
}

namespace vcl::clipping
{
struct NativeSyncStatus
{
    bool bUpdate;
    bool bInvalidateDevice;
};

// Returns true if child clipping needs to be executed by the window
VCL_DLLPUBLIC bool initChildRegion(WindowImpl& rImpl);

// Core visibility synchronization pipeline
VCL_DLLPUBLIC bool syncNativeWindow(WindowImpl& rImpl, vcl::Region& rWinChildClipRegion,
                                    const vcl::Region* pOldRegion, bool& rOutUpdate);

VCL_DLLPUBLIC std::unique_ptr<vcl::Region> prepareClipInvalidation(WindowImpl& rImpl,
                                                                   bool bSysObjOnlySmaller);
VCL_DLLPUBLIC bool invalidateParentClipIfRequired(const WindowImpl& rChildImpl,
                                                  WindowImpl& rParentImpl, WinBits nParentStyle);
VCL_DLLPUBLIC NativeSyncStatus processClipResult(WindowImpl& rImpl, bool bClipSuccess,
                                                 bool bCurrentUpdate);

VCL_DLLPUBLIC std::vector<vcl::Window*> getChildWindows(const WindowImpl& rImpl);
VCL_DLLPUBLIC std::vector<vcl::Window*> getOverlapWindows(const WindowImpl& rImpl);
VCL_DLLPUBLIC std::vector<vcl::Window*> getFollowingSiblings(const WindowImpl& rImpl);
VCL_DLLPUBLIC std::vector<vcl::Window*> getAncestralOverlapSiblings(vcl::Window* pStartWindow);
VCL_DLLPUBLIC std::vector<vcl::Window*> getParentAncestorsUntilOverlap(vcl::Window* pStartWindow);

VCL_DLLPUBLIC void gatherNativeSyncTargets(vcl::Window* pWindow,
                                           std::vector<vcl::Window*>& rTargets);

VCL_DLLPUBLIC void intersectWindowRegion(vcl::Window& rWindow, vcl::Region& rRegion);
VCL_DLLPUBLIC void excludeWindowRegion(vcl::Window& rWindow, vcl::Region& rRegion);
VCL_DLLPUBLIC void excludeWindowAndOverlapRegions(vcl::Window& rWindow, vcl::Region& rRegion);

VCL_DLLPUBLIC void accumulateChildOverlaps(vcl::Window* pWindow, const vcl::Region& rInterRegion,
                                           vcl::Region& rRegion);
VCL_DLLPUBLIC void accumulateWindowAndChildOverlaps(vcl::Window* pWindow,
                                                    const vcl::Region& rInterRegion,
                                                    vcl::Region& rRegion);
/**
 * Calculates and accumulates layout overlap boundaries by walking up the window's
 * parent ancestry to the frame root, subtracting visibility regions along the path.
 */
void accumulateParentBoundaries(vcl::Window& rWindow, const vcl::Region& rInterRegion,
                                vcl::Region& rRegion);

/**
 * Iterates through a window's sibling layout tree, accumulating the coordinate
 * bounds of all overlapping visible sibling nodes into the target region.
 *
 * @return The state of bChildren to determine if downstream evaluation should continue.
 */
void accumulateSiblingBoundaries(vcl::Window& rWindow, const vcl::Region& rInterRegion,
                                 vcl::Region& rRegion, bool bSiblings);

/**
 * Iterates through a window's immediate child layout tree, accumulating the coordinate
 * bounds of all visible child nodes into the target region.
 */
void accumulateChildBoundaries(vcl::Window& rWindow, const vcl::Region& rInterRegion,
                               vcl::Region& rRegion);

/**
 * Coordinates the full sequence of visibility overlap evaluations for a given
 * source layout rectangle, factoring in custom window masks, frame boundaries,
 * parents, siblings, and child collections.
 */
void calcOverlapRegion(vcl::Window& rWindow, const tools::Rectangle& rSourceRect,
                       vcl::Region& rRegion, bool bChildren, bool bSiblings);

/**
 * Evaluates visible child windows against parent style and clip mode constraints,
 * subtracting matching child geometries from the target tracking region.
 * * @param rWindow The parent window context executing the layout pass.
 * @param rRegion The target clip region to be mutated in-place.
 * @return true if any visible child bypassed exclusion (requires special handling).
 */
bool clipChildren(const vcl::Window& rWindow, vcl::Region& rRegion);

/**
 * Unconditionally subtracts the geometric boundaries of all visible child windows
 * from the passed clipping region layout.
 * * @param rWindow The parent window context executing the layout pass.
 * @param rRegion The target clip region to be mutated in-place.
 */
void clipAllChildren(const vcl::Window& rWindow, vcl::Region& rRegion);

/**
 * Traverses the parent's child chain backwards to isolate preceding sibling layout
 * boxes, subtracting their visible regions from the current window's paint layer.
 * * @param rWindow The current window context whose siblings are being evaluated.
 * @param rRegion The target clip region to be mutated in-place.
 */
void clipSiblings(const vcl::Window& rWindow, vcl::Region& rRegion);

/**
 * Evaluates a window's state flags, lazily computing its baseline bounding box
 * geometry and intersecting it against preceding sibling layers.
 */
void initWinClipRegion(const vcl::Window& rWindow);

/**
 * @brief Lazily initializes the tracking child clipping region for a window.
 *
 * Checks the underlying dirty flags and, if required, triggers the cascading
 * geometry calculations to subtract overlapping or hidden child bounds from
 * the target layout element.
 */
void initWinChildClipRegion(const vcl::Window& rWindow);

/**
 * Recursively traverses the child overlap hierarchy, subtracting visible overlap
 * window footprints from the target canvas region.
 */
void excludeOverlapWindows(const vcl::Window& rWindow, vcl::Region& rRegion);

/**
 * Evaluates ancestral parent nodes and stacking context constraints to calculate
 * the definitive bounding canvas clipping box for a window layer.
 */
void clipBoundaries(const vcl::Window& rWindow, vcl::Region& rRegion, bool bThis, bool bOverlaps);

/**
 * Traverses the window border wrapper hierarchy to assign a ParentClipMode,
 * updating child-clipping state attributes on parental ancestors if required.
 */
void setParentClipMode(vcl::Window* pWindow, ParentClipMode nMode);

/**
 * @brief Resolves the effective parent clipping mode for a window context.
 *
 * If the provided window is managed by a border window wrapper, this function
 * recurses down to query the underlying frame decoration instead. Otherwise,
 * it directly extracts the ParentClipMode currently tracked inside the
 * window's clipping state block.
 */
ParentClipMode getParentClipMode(const vcl::Window& pWindow);

/**
 * @brief Retrieves the active child clipping region for a window implementation.
 *
 * This function enforces the lazy initialization pipeline: if the internal
 * clipping state is marked as dirty, it invokes the necessary subsystem geometry
 * calculations (via initWinClipRegion and initWinChildClipRegion) before
 * returning a reference to the computed region.
 */
Region& getWinChildClipRegion(vcl::Window& rWindow);

/**
 * Recalculates and flushes the native system object's clipping rectangles,
 * normalizing absolute coordinates down to the device origin space.
 */
void updateNativeObjectClipRegion(vcl::Window& rWindow, vcl::Region aRegion,
                                  const vcl::Region& rWinRectRegion);

/**
 * Syncs the window's tracking child clip region with the underlying platform
 * system object, triggering region updates and layout flushes if required.
 *
 * @return true if the native object layout update succeeded or was bypassed.
 */
bool nativeObjectClip(vcl::Window& rWindow, const vcl::Region* pOldRegion);

/**
 * Traverses the window tree starting from the given context node to identify
 * all active native system controls, forcing a clip region invalidation and
 * platform refresh on each target.
 */
void invalidateNativeClipTargets(vcl::Window* pStartWindow);

/**
 * Triggers a cascading invalidation of native system control clip targets
 * for the given window, and handles trailing sibling nodes if sibling
 * clipping flags are enabled.
 */
void updateNativeObjectClip(vcl::Window& rWindow);

/**
 * Recursively propagates clip-flag updates down through the child window hierarchy,
 * recalculating native platform clipping bounds where necessary.
 *
 * @param rWindow The window context initiating the propagation loop.
 * @param bSysObjOnlySmaller Optimization flag to limit bounds checking.
 * @return true if all children and native clipping setups updated successfully.
 */
bool setClipFlagChildren(vcl::Window& rWindow, bool bSysObjOnlySmaller);

inline void dirtyInitClipRegion(vcl::Window& rWindow)
{
    // Calling SetClipRegion() with no arguments or an empty region
    // is the standard, public VCL way to trip the mbInitClipRegion flag to true.
    rWindow.GetOutDev()->SetClipRegion();
}

/**
 * Orchestrates clip invalidation flag setting across the window tree, managing
 * routing behavior depending on whether the node is an overlap window, has parent
 * clipping configurations, or requires tracking across sibling components.
 *
 * @return true if the hierarchical flag update successfully completed.
 */
bool setClipFlag(vcl::Window& rWindow, bool bSysObjOnlySmaller = false);

/**
 * Iterates over a window context and its nested overlap window sequences,
 * updating hierarchy clip flags and layout state indicators.
 *
 * @return true if the hierarchical update pass completed successfully.
 */
bool setClipFlagOverlapWindows(vcl::Window& rWindow, bool bSysObjOnlySmaller = false);

/**
 * Calculates and accumulates layout overlap boundaries across the window's
 * high-level ancestral siblings and child overlap trees.
 */
void calcOverlapRegionOverlaps(const vcl::Window& rWindow, const vcl::Region& rInterRegion,
                               vcl::Region& rRegion);
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
