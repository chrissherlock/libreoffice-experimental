/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <vcl/CoordinateMapper.hxx>
#include <vcl/region.hxx>
#include <vcl/window.hxx>

#include <WindowClippingState.hxx>
#include <WindowHierarchy.hxx>
#include <clipping_window.hxx>

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
        vcl::clipping::clipSiblings(rWindow, maWinClipRegion);

    vcl::clipping::clipBoundaries(rWindow, maWinClipRegion, false, true);

    if ((rWindow.GetStyle() & WB_CLIPCHILDREN) || mbClipChildren)
        mbInitChildRegion = true;

    mbInitWinClipRegion = false;
}

void WindowClippingState::initWinChildClipRegion(const vcl::Window& rWindow)
{
    if (initChildRegion(const_cast<vcl::Window&>(rWindow)))
        vcl::clipping::clipChildren(rWindow, *mpChildClipRegion);
}

bool WindowClippingState::initChildRegion(vcl::Window& rWindow)
{
    WindowHierarchy* pHierarchy = rWindow.ImplGetWindowHierarchy();

    // Safely clear the initialization flag on function exit
    comphelper::ScopeGuard aDeinitChildRegion([this]() { mbInitChildRegion = false; });

    if (!pHierarchy->mpFirstChild)
    {
        mpChildClipRegion.reset();
        return false; // No children present; skip downstream clipping
    }

    if (!mpChildClipRegion)
        mpChildClipRegion.reset(new vcl::Region(maWinClipRegion));
    else
        *mpChildClipRegion = maWinClipRegion;

    return true; // Context contains children; signal the window to clip them
}

vcl::Region& WindowClippingState::getWinChildClipRegion(vcl::Window& rWindow)
{
    if (mbInitWinClipRegion)
        initWinClipRegion(
            rWindow); // Use this->initWinClipRegion(rWindow) if you migrated this one too

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

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
