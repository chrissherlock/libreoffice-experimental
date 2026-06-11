/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * This file incorporates work covered by the following license notice:
 *
 *   Licensed to the Apache Software Foundation (ASF) under one or more
 *   contributor license agreements. See the NOTICE file distributed
 *   with this work for additional information regarding copyright
 *   ownership. The ASF licenses this file to you under the Apache
 *   License, Version 2.0 (the "License"); you may not use this file
 *   except in compliance with the License. You may obtain a copy of
 *   the License at http://www.apache.org/licenses/LICENSE-2.0 .
 */

#include <tools/debug.hxx>
#include <comphelper/scopeguard.hxx>

#include <vcl/window.hxx>
#include <vcl/virdev.hxx>
#include <vcl/CoordinateMapper.hxx>

#include <clipping.hxx>
#include <salobj.hxx>
#include <window.h>

namespace vcl {

vcl::Region WindowOutputDevice::GetOutputBoundsClipRegion() const
{
    vcl::Region aClip(GetClipRegion());
    aClip.Intersect(tools::Rectangle(Point(), GetOutputSize()));

    return aClip;
}

void WindowOutputDevice::InitClipRegion()
{
    DBG_TESTSOLARMUTEX();

    vcl::Region aRegion;
    WindowImpl* pImpl = mxOwnerWindow->mpWindowImpl.get();

    // Establish the baseline layout geometry path
    if (pImpl->mbInPaint)
    {
        if (pImpl->mpPaintRegion)
            aRegion = *(pImpl->mpPaintRegion);
    }
    else
    {
        aRegion = mxOwnerWindow->ImplGetWinChildClipRegion();

        // Handle Right-to-Left (RTL) text and coordinate orientation switches
        if (ImplIsAntiparallel())
            ReMirror(aRegion);
    }

    // Intersect with any active user-defined clipping regions
    if (mbClipRegion)
    {
        aRegion.Intersect(GetMapper().ViewToDevice(maRegion));
    }

    // Dispatch the final sync commands to the graphics hardware driver
    if (aRegion.IsEmpty())
    {
        mbOutputClipped = true;
    }
    else
    {
        mbOutputClipped = false;
        SelectClipRegion(aRegion);
    }

    mbClipRegionSet = true;
    mbInitClipRegion = false;
}

void Window::SetParentClipMode(ParentClipMode nMode)
{
    if (mpWindowImpl->mpBorderWindow)
    {
        mpWindowImpl->mpBorderWindow->SetParentClipMode(nMode);
        return;
    }

    if (ImplIsOverlapWindow())
        return;

    mpWindowImpl->mnParentClipMode = nMode;

    if (nMode & ParentClipMode::Clip)
        mpWindowImpl->mpParent->mpWindowImpl->mbClipChildren = true;
}

ParentClipMode Window::GetParentClipMode() const
{
    if (mpWindowImpl->mpBorderWindow)
        return mpWindowImpl->mpBorderWindow->GetParentClipMode();

    return mpWindowImpl->mnParentClipMode;
}

void Window::ExpandPaintClipRegion(const vcl::Region& rRegion)
{
    if (!mpWindowImpl->mpPaintRegion)
        return;

    WindowRegion aPixRegion(rRegion);
    vcl::Region aDevPixRegion = GetOutDev()->GetMapper().ViewToDevice(aPixRegion.get());

    vcl::Region aWinChildRegion = ImplGetWinChildClipRegion();

    // only this region is in frame coordinates, so re-mirror it
    if (GetOutDev()->ImplIsAntiparallel())
    {
        const OutputDevice *pOutDev = GetOutDev();
        pOutDev->ReMirror(aWinChildRegion);
    }

    aDevPixRegion.Intersect(aWinChildRegion);

    if (aDevPixRegion.IsEmpty())
        return;

    mpWindowImpl->mpPaintRegion->Union(aDevPixRegion);
    GetOutDev()->mbInitClipRegion = true;
}

vcl::Region Window::GetWindowClipRegionPixel() const
{
    vcl::Region aWinClipRegion;

    if (mpWindowImpl->mpClippingState->mbInitWinClipRegion)
        const_cast<vcl::Window*>(this)->ImplInitWinClipRegion();

    aWinClipRegion = mpWindowImpl->mpClippingState->maWinClipRegion;

    vcl::Region aWinRegion(GetOutputRectPixel());

    if (aWinRegion == aWinClipRegion)
        aWinClipRegion.SetNull();

    aWinClipRegion.Move(-GetOutDev()->GetDeviceOriginX(), -GetOutDev()->GetDeviceOriginY());

    return aWinClipRegion;
}

vcl::Region WindowOutputDevice::GetActiveClipRegion() const
{
    vcl::Region aRegion(true);
    WindowImpl* pImpl = mxOwnerWindow->mpWindowImpl.get();

    if (pImpl->mbInPaint)
    {
        if (pImpl->mpPaintRegion)
            aRegion = *(pImpl->mpPaintRegion);

        aRegion.Move(-GetDeviceOriginX(), -GetDeviceOriginY());
    }

    if (mbClipRegion)
        aRegion.Intersect(maRegion);

    return convertTo<vcl::LogicRegion>(vcl::WindowRegion(aRegion)).get();
}

void WindowOutputDevice::ClipToPaintRegion(tools::Rectangle& rDstRect)
{
    const vcl::Region aPaintRgn(mxOwnerWindow->GetPaintRegion());
    if (aPaintRgn.IsNull())
        return;

    // Flatten the nested geometry type conversions for readability
    auto aBoundRect   = vcl::LogicRect(aPaintRgn.GetBoundRect());
    auto aWindowRect  = convertTo<vcl::WindowRect>(aBoundRect, GetMapMode()).get();

    rDstRect.Intersection(aWindowRect);
}

void Window::EnableClipSiblings(bool bClipSiblings)
{
    if (mpWindowImpl->mpBorderWindow)
        mpWindowImpl->mpBorderWindow->EnableClipSiblings(bClipSiblings);

    mpWindowImpl->mpClippingState->mbClipSiblings = bClipSiblings;
}

void Window::ImplClipBoundaries( vcl::Region& rRegion, bool bThis, bool bOverlaps )
{
    if (bThis)
    {
        ImplIntersectWindowClipRegion(rRegion);
        return;
    }

    if (!ImplIsOverlapWindow())
    {
        ImplGetParent()->ImplIntersectWindowClipRegion(rRegion);
        return;
    }

    if (!mpWindowImpl->mbFrame)
        rRegion.Intersect(tools::Rectangle(Point(0, 0), mpWindowImpl->mpFrameWindow->GetOutputSizePixel()));

    if (!bOverlaps || rRegion.IsEmpty())
        return;

    for (vcl::Window* pOverlapWin : vcl::clipping::getAncestralOverlapSiblings(this))
    {
        pOverlapWin->ImplExcludeOverlapWindows2(rRegion);
    }

    ImplExcludeOverlapWindows(rRegion);
}

static bool lcl_IsParentClipRequired(ParentClipMode nClipMode, WinBits nStyle)
{
    return !(nClipMode & ParentClipMode::NoClip)
           && ((nClipMode & ParentClipMode::Clip) || (nStyle & WB_CLIPCHILDREN));
}

bool Window::ImplClipChildren(vcl::Region& rRegion) const
{
    bool bOtherClip = false;

    for (vcl::Window* pWindow : vcl::clipping::getChildWindows(*mpWindowImpl))
    {
        if (pWindow->mpWindowImpl->mbReallyVisible)
        {
            ParentClipMode nClipMode = pWindow->GetParentClipMode();

            if (lcl_IsParentClipRequired(nClipMode, GetStyle()))
                pWindow->ImplExcludeWindowRegion(rRegion);
            else
                bOtherClip = true;
        }
    }

    return bOtherClip;
}

void Window::ImplClipAllChildren(vcl::Region& rRegion) const
{
    for (vcl::Window* pWindow : vcl::clipping::getChildWindows(*mpWindowImpl))
    {
        if (pWindow->mpWindowImpl->mbReallyVisible)
            pWindow->ImplExcludeWindowRegion(rRegion);
    }
}

void Window::ImplClipSiblings(vcl::Region& rRegion) const
{
    // Reuse our existing subsystem snapshot function to iterate siblings
    for (vcl::Window* pSibling : vcl::clipping::getChildWindows(*ImplGetParent()->ImplGetWindowImpl()))
    {
        if (pSibling == this)
            break; // We only clip against preceding siblings

        if (pSibling->ImplGetWindowImpl()->mbReallyVisible)
            pSibling->ImplExcludeWindowRegion(rRegion);
    }
}

void Window::ImplInitWinClipRegion()
{
    mpWindowImpl->mpClippingState->maWinClipRegion = GetOutputRectPixel();
    if (mpWindowImpl->mbWinRegion)
        mpWindowImpl->mpClippingState->maWinClipRegion.Intersect(GetOutDev()->GetMapper().ViewToDevice(mpWindowImpl->maWinRegion));

    if (mpWindowImpl->mpClippingState->mbClipSiblings && !ImplIsOverlapWindow())
        ImplClipSiblings(mpWindowImpl->mpClippingState->maWinClipRegion);

    ImplClipBoundaries(mpWindowImpl->mpClippingState->maWinClipRegion, false, true);

    if ((GetStyle() & WB_CLIPCHILDREN) || mpWindowImpl->mbClipChildren)
        mpWindowImpl->mpClippingState->mbInitChildRegion = true;

    mpWindowImpl->mpClippingState->mbInitWinClipRegion = false;
}

void Window::ImplInitWinChildClipRegion()
{
    if (vcl::clipping::initChildRegion(*mpWindowImpl))
        ImplClipChildren(*mpWindowImpl->mpClippingState->mpChildClipRegion);
}

Region& Window::ImplGetWinChildClipRegion()
{
    if (mpWindowImpl->mpClippingState->mbInitWinClipRegion)
        ImplInitWinClipRegion();

    if (mpWindowImpl->mpClippingState->mbInitChildRegion)
        ImplInitWinChildClipRegion();

    if (mpWindowImpl->mpClippingState->mpChildClipRegion)
        return *mpWindowImpl->mpClippingState->mpChildClipRegion;

    return mpWindowImpl->mpClippingState->maWinClipRegion;
}

void Window::ImplUpdateNativeObjectClipRegion(vcl::Region aRegion, const vcl::Region& rWinRectRegion)
{
    if (aRegion == rWinRectRegion)
    {
        mpWindowImpl->mpSysObj->ResetClipRegion();
        return;
    }

    aRegion.Move(-GetOutDev()->GetDeviceOriginX(), -GetOutDev()->GetDeviceOriginY());

    // Set/update system object clip region
    RectangleVector aRectangles;
    aRegion.GetRegionRectangles(aRectangles);
    mpWindowImpl->mpSysObj->BeginSetClipRegion(aRectangles.size());

    for (auto const& rectangle : aRectangles)
    {
        mpWindowImpl->mpSysObj->UnionClipRegion(
            rectangle.Left(),
            rectangle.Top(),
            rectangle.GetWidth(),
            rectangle.GetHeight());
    }

    mpWindowImpl->mpSysObj->EndSetClipRegion();
}

bool Window::ImplNativeObjectClip(const vcl::Region* pOldRegion)
{
    if (!mpWindowImpl->mpSysObj)
        return true;

    vcl::Region& rWinChildClipRegion = ImplGetWinChildClipRegion();
    bool bUpdate = true;

    if (vcl::clipping::syncNativeWindow(*mpWindowImpl, rWinChildClipRegion, pOldRegion, bUpdate))
        return bUpdate;

    ImplUpdateNativeObjectClipRegion(rWinChildClipRegion, vcl::Region(GetOutputRectPixel()));
    mpWindowImpl->mpSysObj->Show(true);

    return bUpdate;
}

void Window::ImplUpdateNativeObjectClip()
{
    if (ImplIsOverlapWindow())
    {
        // Fixes error: replaces the deleted ImplUpdateSysObjOverlapsClip call via snapshot pipeline
        std::vector<vcl::Window*> aFrameTargets;
        vcl::clipping::gatherNativeSyncTargets(mpWindowImpl->mpFrameWindow, aFrameTargets);

        for (vcl::Window* pWin : aFrameTargets)
        {
            if (pWin->ImplGetWindowImpl()->mpSysObj && pWin->ImplGetWindowImpl()->mpClippingState->mbInitWinClipRegion)
            {
                pWin->ImplNativeObjectClip(nullptr);
            }
        }
        return;
    }

    // Gather every downstream window node in a single structural snapshot
    std::vector<vcl::Window*> aSyncTargets;
    vcl::clipping::gatherNativeSyncTargets(this, aSyncTargets);

    for (vcl::Window* pWin : aSyncTargets)
    {
        if (pWin->ImplGetWindowImpl()->mpSysObj && pWin->ImplGetWindowImpl()->mpClippingState->mbInitWinClipRegion)
        {
            pWin->ImplNativeObjectClip(nullptr);
        }
    }

    // Handle edge-case sibling invalidations if required
    if (mpWindowImpl->mpClippingState->mbClipSiblings)
    {
        for (vcl::Window* pSibling : vcl::clipping::getFollowingSiblings(*mpWindowImpl))
        {
            std::vector<vcl::Window*> aSiblingTargets;
            vcl::clipping::gatherNativeSyncTargets(pSibling, aSiblingTargets);
            for (vcl::Window* pTarget : aSiblingTargets)
            {
                if (pTarget->ImplGetWindowImpl()->mpSysObj && pTarget->ImplGetWindowImpl()->mpClippingState->mbInitWinClipRegion)
                    pTarget->ImplNativeObjectClip(nullptr);
            }
        }
    }
}

bool Window::ImplSetClipFlagChildren(bool bSysObjOnlySmaller)
{
    auto pOldRegion = vcl::clipping::prepareClipInvalidation(*mpWindowImpl, bSysObjOnlySmaller);

    GetOutDev()->mbInitClipRegion = true;
    mpWindowImpl->mpClippingState->mbInitWinClipRegion = true;

    // The linked-list logic is gone. We loop over a clean, modern sequence.
    bool bUpdate = true;
    for (vcl::Window* pChild : vcl::clipping::getChildWindows(*mpWindowImpl))
    {
        if (!pChild->ImplSetClipFlagChildren(bSysObjOnlySmaller))
            bUpdate = false;
    }

    if (!mpWindowImpl->mpSysObj)
        return bUpdate;

    bool bClipSuccess = ImplNativeObjectClip(pOldRegion.get());

    auto [bNewUpdate, bInvalidateDevice] = vcl::clipping::processClipResult(*mpWindowImpl, bClipSuccess, bUpdate);
    bUpdate = bNewUpdate;

    if (bInvalidateDevice)
        GetOutDev()->mbInitClipRegion = true;

    return bUpdate;
}

bool Window::ImplSetClipFlagOverlapWindows(bool bSysObjOnlySmaller)
{
    bool bUpdate = ImplSetClipFlagChildren(bSysObjOnlySmaller);

    for (vcl::Window* pWindow : vcl::clipping::getOverlapWindows(*mpWindowImpl))
    {
        if (!pWindow->ImplSetClipFlagOverlapWindows(bSysObjOnlySmaller))
            bUpdate = false;
    }

    return bUpdate;
}

bool Window::ImplSetClipFlag(bool bSysObjOnlySmaller)
{
    if (!ImplIsOverlapWindow())
        return mpWindowImpl->mpFrameWindow->ImplSetClipFlagOverlapWindows(bSysObjOnlySmaller);

    bool bUpdate = ImplSetClipFlagChildren(bSysObjOnlySmaller);

    vcl::Window* pParent = ImplGetParent();

    if (pParent)
    {
        // Explicit return value checking replaces hidden references
        if (vcl::clipping::invalidateParentClipIfRequired(*mpWindowImpl, *pParent->mpWindowImpl, pParent->GetStyle()))
            pParent->GetOutDev()->mbInitClipRegion = true;
    }

    if (mpWindowImpl->mpClippingState->mbClipSiblings)
    {
        for (vcl::Window* pSibling : vcl::clipping::getFollowingSiblings(*mpWindowImpl))
        {
            if (!pSibling->ImplSetClipFlagChildren(bSysObjOnlySmaller))
                bUpdate = false;
        }
    }

    return bUpdate;
}

void Window::ImplIntersectWindowClipRegion( vcl::Region& rRegion )
{
    if ( mpWindowImpl->mpClippingState->mbInitWinClipRegion )
        ImplInitWinClipRegion();

    rRegion.Intersect( mpWindowImpl->mpClippingState->maWinClipRegion );
}

void Window::ImplIntersectWindowRegion( vcl::Region& rRegion )
{
    rRegion.Intersect( GetOutputRectPixel() );
    if ( mpWindowImpl->mbWinRegion )
        rRegion.Intersect( GetOutDev()->GetMapper().ViewToDevice( mpWindowImpl->maWinRegion ) );
}

void Window::ImplExcludeWindowRegion( vcl::Region& rRegion )
{
    if ( mpWindowImpl->mbWinRegion )
    {
        vcl::Region aRegion( GetOutputRectPixel() );
        aRegion.Intersect( GetOutDev()->GetMapper().ViewToDevice( mpWindowImpl->maWinRegion ) );
        rRegion.Exclude( aRegion );
    }
    else
    {
        rRegion.Exclude( GetOutputRectPixel() );
    }
}

void Window::ImplExcludeOverlapWindows(vcl::Region& rRegion) const
{
    for (vcl::Window* pWindow : vcl::clipping::getOverlapWindows(*mpWindowImpl))
    {
        if (pWindow->mpWindowImpl->mbReallyVisible)
        {
            pWindow->ImplExcludeWindowRegion(rRegion);
            pWindow->ImplExcludeOverlapWindows(rRegion);
        }
    }
}

void Window::ImplExcludeOverlapWindows2( vcl::Region& rRegion )
{
    if ( mpWindowImpl->mbReallyVisible )
        ImplExcludeWindowRegion( rRegion );

    ImplExcludeOverlapWindows( rRegion );
}

void Window::ImplIntersectAndUnionOverlapWindows( const vcl::Region& rInterRegion, vcl::Region& rRegion ) const
{
    vcl::clipping::accumulateChildOverlaps(const_cast<Window*>(this), rInterRegion, rRegion);
}

void Window::ImplIntersectAndUnionOverlapWindows2( const vcl::Region& rInterRegion, vcl::Region& rRegion )
{
    vcl::clipping::accumulateWindowAndChildOverlaps(this, rInterRegion, rRegion);
}

void Window::ImplCalcOverlapRegionOverlaps( const vcl::Region& rInterRegion, vcl::Region& rRegion ) const
{
    // High-level ancestral sibling walk
    for (vcl::Window* pOverlapWin : vcl::clipping::getAncestralOverlapSiblings(const_cast<Window*>(this)))
    {
        vcl::clipping::accumulateWindowAndChildOverlaps(pOverlapWin, rInterRegion, rRegion);
    }

    // Child overlap window execution
    vcl::Window* pOverlapParent = !ImplIsOverlapWindow() ? mpWindowImpl->mpOverlapWindow.get() : const_cast<Window*>(this);
    vcl::clipping::accumulateChildOverlaps(pOverlapParent, rInterRegion, rRegion);
}

void Window::ImplCalcOverlapRegion( const tools::Rectangle& rSourceRect, vcl::Region& rRegion,
                                    bool bChildren, bool bSiblings )
{
    vcl::Region  aRegion( rSourceRect );
    if ( mpWindowImpl->mbWinRegion )
        rRegion.Intersect( GetOutDev()->GetMapper().ViewToDevice( mpWindowImpl->maWinRegion ) );
    vcl::Region  aTempRegion;
    vcl::Window* pWindow;

    ImplCalcOverlapRegionOverlaps( aRegion, rRegion );

    // Parent-Boundaries
    pWindow = this;
    if ( !ImplIsOverlapWindow() )
    {
        pWindow = ImplGetParent();
        do
        {
            aTempRegion = aRegion;
            pWindow->ImplExcludeWindowRegion( aTempRegion );
            rRegion.Union( aTempRegion );
            if ( pWindow->ImplIsOverlapWindow() )
                break;
            pWindow = pWindow->ImplGetParent();
        }
        while ( pWindow );
    }
    if ( pWindow && !pWindow->mpWindowImpl->mbFrame )
    {
        aTempRegion = aRegion;
        aTempRegion.Exclude( tools::Rectangle( Point( 0, 0 ), mpWindowImpl->mpFrameWindow->GetOutputSizePixel() ) );
        rRegion.Union( aTempRegion );
    }

    // Siblings
    if ( bSiblings && !ImplIsOverlapWindow() )
    {
        pWindow = mpWindowImpl->mpParent->mpWindowImpl->mpFirstChild;
        do
        {
            if ( pWindow->mpWindowImpl->mbReallyVisible && (pWindow != this) )
            {
                aTempRegion = aRegion;
                pWindow->ImplIntersectWindowRegion( aTempRegion );
                rRegion.Union( aTempRegion );
            }
            pWindow = pWindow->mpWindowImpl->mpNext;
        }
        while ( pWindow );
    }

    if ( !bChildren )
        return;

    pWindow = mpWindowImpl->mpFirstChild;
    while ( pWindow )
    {
        if ( pWindow->mpWindowImpl->mbReallyVisible )
        {
            aTempRegion = aRegion;
            pWindow->ImplIntersectWindowRegion( aTempRegion );
            rRegion.Union( aTempRegion );
        }
        pWindow = pWindow->mpWindowImpl->mpNext;
    }
}

void WindowOutputDevice::SaveBackground(VirtualDevice& rSaveDevice, const Point& rPos, const Size& rSize, const Size&) const
{
    comphelper::ScopeGuard aResetMapMode([&rSaveDevice]() { rSaveDevice.SetMapMode(MapMode()); });

    if (!mxOwnerWindow || !mxOwnerWindow->mpWindowImpl || !mxOwnerWindow->mpWindowImpl->mpPaintRegion)
    {
        rSaveDevice.DrawOutDev(Point(), rSize, rPos, rSize, *this);
        return;
    }

    vcl::Region aClip(*mxOwnerWindow->mpWindowImpl->mpPaintRegion);
    aClip.Move(-GetDeviceOriginX(), -GetDeviceOriginY());

    const auto boundRect = convertTo<vcl::WindowRect>(vcl::LogicRect(tools::Rectangle(rPos, rSize)), GetMapMode());

    aClip.Intersect(boundRect.get());

    if (aClip.IsEmpty())
        return;

    const vcl::Region aOldClip(rSaveDevice.GetClipRegion());
    const vcl::MappingPolicy eOldPolicy = rSaveDevice.GetMappingPolicy();

    comphelper::ScopeGuard aDeviceGuard([&rSaveDevice, aOldClip, eOldPolicy]() {
        rSaveDevice.SetMappingPolicy(eOldPolicy);
        rSaveDevice.SetClipRegion(aOldClip);
    });

    const auto aPixPos = convertTo<vcl::WindowPoint>(vcl::LogicPoint(rPos), GetMapMode());
    const auto aPixOffset = rSaveDevice.convertTo<vcl::WindowPoint>(vcl::LogicPoint(0, 0), rSaveDevice.GetMapMode());

    // Move clip region to have the same distance to DestOffset
    aClip.Move(aPixOffset->X() - aPixPos->X(), aPixOffset->Y() - aPixPos->Y());

    // Set pixel clip region
    rSaveDevice.SetMappingPolicy(vcl::MappingPolicy::IgnoreMapMode);
    rSaveDevice.SetClipRegion(aClip);

    rSaveDevice.DrawOutDev(Point(), rSize, rPos, rSize, *this);
}

} /* namespace vcl */

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
