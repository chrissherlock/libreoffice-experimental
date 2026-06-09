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

    vcl::Region  aRegion;

    if ( mxOwnerWindow->mpWindowImpl->mbInPaint )
    {
        aRegion = *(mxOwnerWindow->mpWindowImpl->mpPaintRegion);
    }
    else
    {
        aRegion = mxOwnerWindow->ImplGetWinChildClipRegion();
        // only this region is in frame coordinates, so re-mirror it
        // the mpWindowImpl->mpPaintRegion above is already correct (see ImplCallPaint()) !
        if( ImplIsAntiparallel() )
            ReMirror ( aRegion );
    }

    if ( mbClipRegion )
        aRegion.Intersect( GetMapper().ViewToDevice( maRegion ) );

    if ( aRegion.IsEmpty() )
    {
        mbOutputClipped = true;
    }
    else
    {
        mbOutputClipped = false;
        SelectClipRegion( aRegion );
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

    if (mpWindowImpl->mbInitWinClipRegion)
        const_cast<vcl::Window*>(this)->ImplInitWinClipRegion();

    aWinClipRegion = mpWindowImpl->maWinClipRegion;

    vcl::Region aWinRegion(GetOutputRectPixel());

    if (aWinRegion == aWinClipRegion)
        aWinClipRegion.SetNull();

    aWinClipRegion.Move(-GetOutDev()->GetDeviceOriginX(), -GetOutDev()->GetDeviceOriginY());

    return aWinClipRegion;
}


vcl::Region WindowOutputDevice::GetActiveClipRegion() const
{
    vcl::Region aRegion(true);

    if (mxOwnerWindow->mpWindowImpl->mbInPaint)
    {
        aRegion = *(mxOwnerWindow->mpWindowImpl->mpPaintRegion);
        aRegion.Move(-GetDeviceOriginX(), -GetDeviceOriginY());
    }

    if (mbClipRegion)
        aRegion.Intersect(maRegion);

    return convertTo<vcl::LogicRegion>(vcl::WindowRegion(aRegion)).get();
}

void WindowOutputDevice::ClipToPaintRegion(tools::Rectangle& rDstRect)
{
    const vcl::Region aPaintRgn(mxOwnerWindow->GetPaintRegion());

    if (!aPaintRgn.IsNull())
        rDstRect.Intersection(convertTo<vcl::WindowRect>(vcl::LogicRect(aPaintRgn.GetBoundRect()), GetMapMode()).get());
}

void Window::EnableClipSiblings(bool bClipSiblings)
{
    if (mpWindowImpl->mpBorderWindow)
        mpWindowImpl->mpBorderWindow->EnableClipSiblings(bClipSiblings);

    mpWindowImpl->mbClipSiblings = bClipSiblings;
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

    // clip to frame if required
    if (!mpWindowImpl->mbFrame)
        rRegion.Intersect(tools::Rectangle(Point(0, 0), mpWindowImpl->mpFrameWindow->GetOutputSizePixel()));

    if (!bOverlaps || rRegion.IsEmpty())
        return;

    // Clip Overlap Siblings
    vcl::Window* pStartOverlapWindow = this;
    while (!pStartOverlapWindow->mpWindowImpl->mbFrame)
    {
        vcl::Window* pOverlapWindow = pStartOverlapWindow->mpWindowImpl->mpOverlapWindow->mpWindowImpl->mpFirstOverlap;

        while (pOverlapWindow && (pOverlapWindow != pStartOverlapWindow))
        {
            pOverlapWindow->ImplExcludeOverlapWindows2(rRegion);
            pOverlapWindow = pOverlapWindow->mpWindowImpl->mpNext;
        }

        pStartOverlapWindow = pStartOverlapWindow->mpWindowImpl->mpOverlapWindow;
    }

    // Clip Child Overlap Windows
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
    vcl::Window* pWindow = mpWindowImpl->mpFirstChild;

    while (pWindow)
    {
        if (pWindow->mpWindowImpl->mbReallyVisible)
        {
            // read-out ParentClipMode-Flags
            ParentClipMode nClipMode = pWindow->GetParentClipMode();

            if (lcl_IsParentClipRequired(nClipMode, GetStyle()))
                pWindow->ImplExcludeWindowRegion(rRegion);
            else
                bOtherClip = true;
        }

        pWindow = pWindow->mpWindowImpl->mpNext;
    }

    return bOtherClip;
}

void Window::ImplClipAllChildren( vcl::Region& rRegion ) const
{
    vcl::Window* pWindow = mpWindowImpl->mpFirstChild;
    while ( pWindow )
    {
        if ( pWindow->mpWindowImpl->mbReallyVisible )
            pWindow->ImplExcludeWindowRegion( rRegion );
        pWindow = pWindow->mpWindowImpl->mpNext;
    }
}

void Window::ImplClipSiblings(vcl::Region& rRegion) const
{
    vcl::Window* pWindow = ImplGetParent()->mpWindowImpl->mpFirstChild;

    while (pWindow)
    {
        if (pWindow == this)
            break;

        if (pWindow->mpWindowImpl->mbReallyVisible)
            pWindow->ImplExcludeWindowRegion(rRegion);

        pWindow = pWindow->mpWindowImpl->mpNext;
    }
}

void Window::ImplInitWinClipRegion()
{
    // Build Window Region
    mpWindowImpl->maWinClipRegion = GetOutputRectPixel();
    if ( mpWindowImpl->mbWinRegion )
        mpWindowImpl->maWinClipRegion.Intersect( GetOutDev()->GetMapper().ViewToDevice( mpWindowImpl->maWinRegion ) );

    // ClipSiblings
    if ( mpWindowImpl->mbClipSiblings && !ImplIsOverlapWindow() )
        ImplClipSiblings( mpWindowImpl->maWinClipRegion );

    // Clip Parent Boundaries
    ImplClipBoundaries( mpWindowImpl->maWinClipRegion, false, true );

    // Clip Children
    if ( (GetStyle() & WB_CLIPCHILDREN) || mpWindowImpl->mbClipChildren )
        mpWindowImpl->mbInitChildRegion = true;

    mpWindowImpl->mbInitWinClipRegion = false;
}

void Window::ImplInitWinChildClipRegion()
{
    comphelper::ScopeGuard aDeinitChildRegion([this]() { mpWindowImpl->mbInitChildRegion = false; });

    if (!mpWindowImpl->mpFirstChild)
    {
        mpWindowImpl->mpChildClipRegion.reset();
        return;
    }

    if (!mpWindowImpl->mpChildClipRegion)
        mpWindowImpl->mpChildClipRegion.reset(new vcl::Region(mpWindowImpl->maWinClipRegion));
    else
        *mpWindowImpl->mpChildClipRegion = mpWindowImpl->maWinClipRegion;

    ImplClipChildren(*mpWindowImpl->mpChildClipRegion);
}

Region& Window::ImplGetWinChildClipRegion()
{
    if (mpWindowImpl->mbInitWinClipRegion)
        ImplInitWinClipRegion();

    if (mpWindowImpl->mbInitChildRegion)
        ImplInitWinChildClipRegion();

    if (mpWindowImpl->mpChildClipRegion)
        return *mpWindowImpl->mpChildClipRegion;

    return mpWindowImpl->maWinClipRegion;
}

void Window::ImplUpdateSysObjClipRegion(vcl::Region aRegion, const vcl::Region& rWinRectRegion)
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

bool Window::ImplSysObjClip(const vcl::Region* pOldRegion)
{
    if (!mpWindowImpl->mpSysObj)
        return true;

    // If the window is hidden or its clipping region is empty, hide the system object early
    if (!mpWindowImpl->mbReallyVisible || ImplGetWinChildClipRegion().IsEmpty())
    {
        mpWindowImpl->mpSysObj->Show(false);
        return true;
    }

    vcl::Region& rWinChildClipRegion = ImplGetWinChildClipRegion();

    // the system object is definitively visible
    bool bUpdate = true;
    if (pOldRegion)
    {
        vcl::Region aNewRegion = rWinChildClipRegion;
        rWinChildClipRegion.Intersect(*pOldRegion);
        bUpdate = (aNewRegion == rWinChildClipRegion);
    }

    ImplUpdateSysObjClipRegion(rWinChildClipRegion, vcl::Region(GetOutputRectPixel()));

    mpWindowImpl->mpSysObj->Show(true);

    return bUpdate;
}

void Window::ImplUpdateSysObjChildrenClip()
{
    if ( mpWindowImpl->mpSysObj && mpWindowImpl->mbInitWinClipRegion )
        ImplSysObjClip( nullptr );

    vcl::Window* pWindow = mpWindowImpl->mpFirstChild;
    while ( pWindow )
    {
        pWindow->ImplUpdateSysObjChildrenClip();
        pWindow = pWindow->mpWindowImpl->mpNext;
    }
}

void Window::ImplUpdateSysObjOverlapsClip()
{
    ImplUpdateSysObjChildrenClip();

    vcl::Window* pWindow = mpWindowImpl->mpFirstOverlap;
    while ( pWindow )
    {
        pWindow->ImplUpdateSysObjOverlapsClip();
        pWindow = pWindow->mpWindowImpl->mpNext;
    }
}

void Window::ImplUpdateSysObjClip()
{
    if (ImplIsOverlapWindow())
    {
        mpWindowImpl->mpFrameWindow->ImplUpdateSysObjOverlapsClip();
        return;
    }

    ImplUpdateSysObjChildrenClip();

    // siblings should recalculate their clip region
    if (!mpWindowImpl->mbClipSiblings)
        return;

    vcl::Window* pWindow = mpWindowImpl->mpNext;

    while (pWindow)
    {
        pWindow->ImplUpdateSysObjChildrenClip();
        pWindow = pWindow->mpWindowImpl->mpNext;
    }
}

bool Window::ImplSetClipFlagChildren(bool bSysObjOnlySmaller)
{
    if (!mpWindowImpl->mpSysObj)
    {
        bool bUpdate = true;

        GetOutDev()->mbInitClipRegion = true;
        mpWindowImpl->mbInitWinClipRegion = true;

        vcl::Window* pWindow = mpWindowImpl->mpFirstChild;
        while (pWindow)
        {
            if (!pWindow->ImplSetClipFlagChildren(bSysObjOnlySmaller))
                bUpdate = false;

            pWindow = pWindow->mpWindowImpl->mpNext;
        }

        return bUpdate;
    }

    std::unique_ptr<vcl::Region> pOldRegion;
    if (bSysObjOnlySmaller && !mpWindowImpl->mbInitWinClipRegion)
        pOldRegion.reset(new vcl::Region( mpWindowImpl->maWinClipRegion));

    GetOutDev()->mbInitClipRegion = true;
    mpWindowImpl->mbInitWinClipRegion = true;

    vcl::Window* pWindow = mpWindowImpl->mpFirstChild;

    bool bUpdate = true;

    while (pWindow)
    {
        if (!pWindow->ImplSetClipFlagChildren(bSysObjOnlySmaller))
            bUpdate = false;

        pWindow = pWindow->mpWindowImpl->mpNext;
    }

    if (!ImplSysObjClip(pOldRegion.get()))
    {
        GetOutDev()->mbInitClipRegion = true;
        mpWindowImpl->mbInitWinClipRegion = true;
        bUpdate = false;
    }

    return bUpdate;
}

bool Window::ImplSetClipFlagOverlapWindows( bool bSysObjOnlySmaller )
{
    bool bUpdate = ImplSetClipFlagChildren( bSysObjOnlySmaller );

    vcl::Window* pWindow = mpWindowImpl->mpFirstOverlap;
    while ( pWindow )
    {
        if ( !pWindow->ImplSetClipFlagOverlapWindows( bSysObjOnlySmaller ) )
            bUpdate = false;
        pWindow = pWindow->mpWindowImpl->mpNext;
    }

    return bUpdate;
}

bool Window::ImplSetClipFlag(bool bSysObjOnlySmaller)
{
    if (!ImplIsOverlapWindow())
        return mpWindowImpl->mpFrameWindow->ImplSetClipFlagOverlapWindows(bSysObjOnlySmaller);

    bool bUpdate = ImplSetClipFlagChildren(bSysObjOnlySmaller);

    vcl::Window* pParent = ImplGetParent();

    if (pParent &&
        ((pParent->GetStyle() & WB_CLIPCHILDREN) || (mpWindowImpl->mnParentClipMode & ParentClipMode::Clip)))
    {
        pParent->GetOutDev()->mbInitClipRegion = true;
        pParent->mpWindowImpl->mbInitChildRegion = true;
    }

    // siblings should recalculate their clip region
    if (mpWindowImpl->mbClipSiblings)
    {
        vcl::Window* pWindow = mpWindowImpl->mpNext;
        while (pWindow)
        {
            if (!pWindow->ImplSetClipFlagChildren(bSysObjOnlySmaller))
                bUpdate = false;

            pWindow = pWindow->mpWindowImpl->mpNext;
        }
    }

    return bUpdate;
}

void Window::ImplIntersectWindowClipRegion( vcl::Region& rRegion )
{
    if ( mpWindowImpl->mbInitWinClipRegion )
        ImplInitWinClipRegion();

    rRegion.Intersect( mpWindowImpl->maWinClipRegion );
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

void Window::ImplExcludeOverlapWindows( vcl::Region& rRegion ) const
{
    vcl::Window* pWindow = mpWindowImpl->mpFirstOverlap;
    while ( pWindow )
    {
        if ( pWindow->mpWindowImpl->mbReallyVisible )
        {
            pWindow->ImplExcludeWindowRegion( rRegion );
            pWindow->ImplExcludeOverlapWindows( rRegion );
        }

        pWindow = pWindow->mpWindowImpl->mpNext;
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
    vcl::Window* pWindow = mpWindowImpl->mpFirstOverlap;
    while ( pWindow )
    {
        if ( pWindow->mpWindowImpl->mbReallyVisible )
        {
            vcl::Region aTempRegion( rInterRegion );
            pWindow->ImplIntersectWindowRegion( aTempRegion );
            rRegion.Union( aTempRegion );
            pWindow->ImplIntersectAndUnionOverlapWindows( rInterRegion, rRegion );
        }

        pWindow = pWindow->mpWindowImpl->mpNext;
    }
}

void Window::ImplIntersectAndUnionOverlapWindows2( const vcl::Region& rInterRegion, vcl::Region& rRegion )
{
    if ( mpWindowImpl->mbReallyVisible )
    {
        vcl::Region aTempRegion( rInterRegion );
        ImplIntersectWindowRegion( aTempRegion );
        rRegion.Union( aTempRegion );
    }

    ImplIntersectAndUnionOverlapWindows( rInterRegion, rRegion );
}

void Window::ImplCalcOverlapRegionOverlaps( const vcl::Region& rInterRegion, vcl::Region& rRegion ) const
{
    // Clip Overlap Siblings
    vcl::Window const * pStartOverlapWindow;

    if ( !ImplIsOverlapWindow() )
        pStartOverlapWindow = mpWindowImpl->mpOverlapWindow;
    else
        pStartOverlapWindow = this;

    while ( !pStartOverlapWindow->mpWindowImpl->mbFrame )
    {
        vcl::Window* pOverlapWindow = pStartOverlapWindow->mpWindowImpl->mpOverlapWindow->mpWindowImpl->mpFirstOverlap;
        while ( pOverlapWindow && (pOverlapWindow != pStartOverlapWindow) )
        {
            pOverlapWindow->ImplIntersectAndUnionOverlapWindows2( rInterRegion, rRegion );
            pOverlapWindow = pOverlapWindow->mpWindowImpl->mpNext;
        }
        pStartOverlapWindow = pStartOverlapWindow->mpWindowImpl->mpOverlapWindow;
    }

    // Clip Child Overlap Windows
    if ( !ImplIsOverlapWindow() )
        mpWindowImpl->mpOverlapWindow->ImplIntersectAndUnionOverlapWindows( rInterRegion, rRegion );
    else
        ImplIntersectAndUnionOverlapWindows( rInterRegion, rRegion );
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

    // Modernized coordinate transformations
    const auto aPixPos = convertTo<vcl::WindowPoint>(vcl::LogicPoint(rPos), GetMapMode());
    const auto boundRect = convertTo<vcl::WindowRect>(vcl::LogicRect(tools::Rectangle(rPos, rSize)), GetMapMode());

    aClip.Intersect(boundRect.get());

    if (!aClip.IsEmpty())
    {
        const vcl::Region aOldClip(rSaveDevice.GetClipRegion());
        const vcl::MappingPolicy eOldPolicy = rSaveDevice.GetMappingPolicy();

        comphelper::ScopeGuard aDeviceGuard([&rSaveDevice, aOldClip, eOldPolicy]() {
            rSaveDevice.SetMappingPolicy(eOldPolicy);
            rSaveDevice.SetClipRegion(aOldClip);
        });

        const auto aPixOffset = rSaveDevice.convertTo<vcl::WindowPoint>(vcl::LogicPoint(0, 0), rSaveDevice.GetMapMode());

        // Move clip region to have the same distance to DestOffset
        aClip.Move(aPixOffset->X() - aPixPos->X(), aPixOffset->Y() - aPixPos->Y());

        // Set pixel clip region
        rSaveDevice.SetMappingPolicy(vcl::MappingPolicy::IgnoreMapMode);
        rSaveDevice.SetClipRegion(aClip);

        rSaveDevice.DrawOutDev(Point(), rSize, rPos, rSize, *this);
    }
}

} /* namespace vcl */

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
