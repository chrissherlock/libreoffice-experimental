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

/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <vcl/window.hxx>
#include <vcl/virdev.hxx>
#include <vcl/outdev.hxx>
#include <tools/debug.hxx>
#include <salobj.hxx>
#include <window.h>

#include <CoordinateMapper.hxx>
#include <ClippingController.hxx>
#include <salgdi.hxx>

namespace vcl {

void WindowOutputDevice::ImplCalculateAndApplyClip()
{
    vcl::Region aRegion;

    // Determine the Base Visibility Region
    // If we are in a Paint event, we only care about the damaged area.
    // Otherwise, we calculate what's visible relative to child windows.
    if (mxOwnerWindow->mpWindowImpl->mbInPaint)
    {
        aRegion = *(mxOwnerWindow->mpWindowImpl->mpPaintRegion);
    }
    else
    {
        aRegion = mxOwnerWindow->ImplGetWinChildClipRegion();

        // Handle RTL Mirroring
        // If the window is 'Antiparallel' (RTL), we must re-mirror the
        // calculated coordinates to match the physical screen space.
        if (ImplIsAntiparallel())
            ReMirror(aRegion);
    }

    // Intersect with the User-Defined Clip
    // This is the clip set by the programmer (e.g., SetClipRegion).
    // We must move it to absolute coordinates to match the Window Region.
    if (mpClippingController->HasClipRegion())
    {
        vcl::Region aUserClip = mpClippingController->GetClipRegion();
        aUserClip.Move(GetOutOffXPixel(), GetOutOffYPixel());

        aRegion.Intersect(aUserClip);
    }

    // Update the Clipping Controller and Graphics Backend
    if (aRegion.IsEmpty())
    {
        // Optimization: Tell the controller to skip drawing commands entirely.
        mpClippingController->SetOutputClipped(true);
    }
    else
    {
        mpClippingController->SetOutputClipped(false);

        // Push the final calculated region to the SalGraphics driver.
        SetGraphicsClip(aRegion);
    }
}

void Window::SetParentClipMode( ParentClipMode nMode )
{
    if ( mpWindowImpl->mpBorderWindow )
        mpWindowImpl->mpBorderWindow->SetParentClipMode( nMode );
    else
    {
        if ( !ImplIsOverlapWindow() )
        {
            mpWindowImpl->mnParentClipMode = nMode;
            if ( nMode & ParentClipMode::Clip )
                mpWindowImpl->mpParent->mpWindowImpl->mbClipChildren = true;
        }
    }
}

ParentClipMode Window::GetParentClipMode() const
{
    if ( mpWindowImpl->mpBorderWindow )
        return mpWindowImpl->mpBorderWindow->GetParentClipMode();
    else
        return mpWindowImpl->mnParentClipMode;
}

void Window::ExpandPaintClipRegion( const vcl::Region& rRegion )
{
    if( !mpWindowImpl->mpPaintRegion )
        return;

    vcl::Region aPixRegion = LogicToPixel( rRegion );
    vcl::Region aDevPixRegion = GetOutDev()->PixelToDevicePixel( aPixRegion );

    vcl::Region aWinChildRegion = ImplGetWinChildClipRegion();
    if( GetOutDev()->ImplIsAntiparallel() )
    {
        const OutputDevice *pOutDev = GetOutDev();
        pOutDev->ReMirror( aWinChildRegion );
    }

    aDevPixRegion.Intersect( aWinChildRegion );
    if( ! aDevPixRegion.IsEmpty() )
    {
        mpWindowImpl->mpPaintRegion->Union( aDevPixRegion );

        GetOutDev()->GetClippingController().SetDirty(true);
    }
}

vcl::Region Window::GetWindowClipRegionPixel() const
{
    vcl::Region aWinClipRegion;

    if ( mpWindowImpl->mbInitWinClipRegion )
        const_cast<vcl::Window*>(this)->ImplInitWinClipRegion();
    aWinClipRegion = mpWindowImpl->maWinClipRegion;

    vcl::Region aWinRegion( GetOutputRectPixel() );

    if ( aWinRegion == aWinClipRegion )
        aWinClipRegion.SetNull();

    aWinClipRegion.Move(-GetOutDev()->GetOutOffXPixel(), -GetOutDev()->GetOutOffYPixel());

    return aWinClipRegion;
}

void Window::EnableClipSiblings( bool bClipSiblings )
{
    if ( mpWindowImpl->mpBorderWindow )
        mpWindowImpl->mpBorderWindow->EnableClipSiblings( bClipSiblings );

    mpWindowImpl->mbClipSiblings = bClipSiblings;
}

void Window::ImplClipBoundaries( vcl::Region& rRegion, bool bThis, bool bOverlaps )
{
    if ( bThis )
        ImplIntersectWindowClipRegion( rRegion );
    else if ( ImplIsOverlapWindow() )
    {
        if ( !mpWindowImpl->mbFrame )
            rRegion.Intersect( tools::Rectangle( Point( 0, 0 ), mpWindowImpl->mpFrameWindow->GetOutputSizePixel() ) );

        if ( bOverlaps && !rRegion.IsEmpty() )
        {
            vcl::Window* pStartOverlapWindow = this;
            while ( !pStartOverlapWindow->mpWindowImpl->mbFrame )
            {
                vcl::Window* pOverlapWindow = pStartOverlapWindow->mpWindowImpl->mpOverlapWindow->mpWindowImpl->mpFirstOverlap;
                while ( pOverlapWindow && (pOverlapWindow != pStartOverlapWindow) )
                {
                    pOverlapWindow->ImplExcludeOverlapWindows2( rRegion );
                    pOverlapWindow = pOverlapWindow->mpWindowImpl->mpNext;
                }
                pStartOverlapWindow = pStartOverlapWindow->mpWindowImpl->mpOverlapWindow;
            }
            ImplExcludeOverlapWindows( rRegion );
        }
    }
    else
        ImplGetParent()->ImplIntersectWindowClipRegion( rRegion );
}

bool Window::ImplClipChildren( vcl::Region& rRegion ) const
{
    bool bOtherClip = false;
    vcl::Window* pWindow = mpWindowImpl->mpFirstChild;
    while ( pWindow )
    {
        if ( pWindow->mpWindowImpl->mbReallyVisible )
        {
            ParentClipMode nClipMode = pWindow->GetParentClipMode();
            if ( !(nClipMode & ParentClipMode::NoClip) &&
                 ((nClipMode & ParentClipMode::Clip) || (GetStyle() & WB_CLIPCHILDREN)) )
                pWindow->ImplExcludeWindowRegion( rRegion );
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

void Window::ImplClipSiblings( vcl::Region& rRegion ) const
{
    vcl::Window* pWindow = ImplGetParent()->mpWindowImpl->mpFirstChild;
    while ( pWindow )
    {
        if ( pWindow == this )
            break;

        if ( pWindow->mpWindowImpl->mbReallyVisible )
            pWindow->ImplExcludeWindowRegion( rRegion );

        pWindow = pWindow->mpWindowImpl->mpNext;
    }
}

void Window::ImplInitWinClipRegion()
{
    mpWindowImpl->maWinClipRegion = GetOutputRectPixel();
    if ( mpWindowImpl->mbWinRegion )
        mpWindowImpl->maWinClipRegion.Intersect( GetOutDev()->PixelToDevicePixel( mpWindowImpl->maWinRegion ) );

    if ( mpWindowImpl->mbClipSiblings && !ImplIsOverlapWindow() )
        ImplClipSiblings( mpWindowImpl->maWinClipRegion );

    ImplClipBoundaries( mpWindowImpl->maWinClipRegion, false, true );

    if ( (GetStyle() & WB_CLIPCHILDREN) || mpWindowImpl->mbClipChildren )
        mpWindowImpl->mbInitChildRegion = true;

    mpWindowImpl->mbInitWinClipRegion = false;
}

void Window::ImplInitWinChildClipRegion()
{
    if ( !mpWindowImpl->mpFirstChild )
    {
        mpWindowImpl->mpChildClipRegion.reset();
    }
    else
    {
        if ( !mpWindowImpl->mpChildClipRegion )
            mpWindowImpl->mpChildClipRegion.reset( new vcl::Region( mpWindowImpl->maWinClipRegion ) );
        else
            *mpWindowImpl->mpChildClipRegion = mpWindowImpl->maWinClipRegion;

        ImplClipChildren( *mpWindowImpl->mpChildClipRegion );
    }

    mpWindowImpl->mbInitChildRegion = false;
}

Region& Window::ImplGetWinChildClipRegion()
{
    if ( mpWindowImpl->mbInitWinClipRegion )
        ImplInitWinClipRegion();
    if ( mpWindowImpl->mbInitChildRegion )
        ImplInitWinChildClipRegion();
    if ( mpWindowImpl->mpChildClipRegion )
        return *mpWindowImpl->mpChildClipRegion;
    return mpWindowImpl->maWinClipRegion;
}

bool Window::ImplSysObjClip( const vcl::Region* pOldRegion )
{
    bool bUpdate = true;
    if ( mpWindowImpl->mpSysObj )
    {
        bool bVisibleState = mpWindowImpl->mbReallyVisible;
        if ( bVisibleState )
        {
            vcl::Region& rWinChildClipRegion = ImplGetWinChildClipRegion();
            if (!rWinChildClipRegion.IsEmpty())
            {
                if ( pOldRegion )
                {
                    vcl::Region aNewRegion = rWinChildClipRegion;
                    rWinChildClipRegion.Intersect(*pOldRegion);
                    bUpdate = aNewRegion == rWinChildClipRegion;
                }

                vcl::Region aRegion = rWinChildClipRegion;
                vcl::Region aWinRectRegion( GetOutputRectPixel() );

                if ( aRegion == aWinRectRegion )
                    mpWindowImpl->mpSysObj->ResetClipRegion();
                else
                {
                    aRegion.Move(-GetOutDev()->GetOutOffXPixel(), -GetOutDev()->GetOutOffYPixel());
                    RectangleVector aRectangles;
                    aRegion.GetRegionRectangles(aRectangles);
                    mpWindowImpl->mpSysObj->BeginSetClipRegion(aRectangles.size());
                    for (auto const& rectangle : aRectangles)
                    {
                        mpWindowImpl->mpSysObj->UnionClipRegion(
                            rectangle.Left(), rectangle.Top(),
                            rectangle.GetWidth(), rectangle.GetHeight());
                    }
                    mpWindowImpl->mpSysObj->EndSetClipRegion();
                }
            }
            else
                bVisibleState = false;
        }
        mpWindowImpl->mpSysObj->Show( bVisibleState );
    }
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
    if ( !ImplIsOverlapWindow() )
    {
        ImplUpdateSysObjChildrenClip();
        if ( mpWindowImpl->mbClipSiblings )
        {
            vcl::Window* pWindow = mpWindowImpl->mpNext;
            while ( pWindow )
            {
                pWindow->ImplUpdateSysObjChildrenClip();
                pWindow = pWindow->mpWindowImpl->mpNext;
            }
        }
    }
    else
        mpWindowImpl->mpFrameWindow->ImplUpdateSysObjOverlapsClip();
}

bool Window::ImplSetClipFlagChildren( bool bSysObjOnlySmaller )
{
    bool bUpdate = true;
    if ( mpWindowImpl->mpSysObj )
    {
        std::unique_ptr<vcl::Region> pOldRegion;
        if ( bSysObjOnlySmaller && !mpWindowImpl->mbInitWinClipRegion )
            pOldRegion.reset(new vcl::Region( mpWindowImpl->maWinClipRegion ));

        GetOutDev()->GetClippingController().SetDirty(true);
        mpWindowImpl->mbInitWinClipRegion = true;

        vcl::Window* pWindow = mpWindowImpl->mpFirstChild;
        while ( pWindow )
        {
            if ( !pWindow->ImplSetClipFlagChildren( bSysObjOnlySmaller ) )
                bUpdate = false;
            pWindow = pWindow->mpWindowImpl->mpNext;
        }

        if ( !ImplSysObjClip( pOldRegion.get() ) )
        {
            GetOutDev()->GetClippingController().SetDirty(true);
            mpWindowImpl->mbInitWinClipRegion = true;
            bUpdate = false;
        }
    }
    else
    {
        GetOutDev()->GetClippingController().SetDirty(true);
        mpWindowImpl->mbInitWinClipRegion = true;

        vcl::Window* pWindow = mpWindowImpl->mpFirstChild;
        while ( pWindow )
        {
            if ( !pWindow->ImplSetClipFlagChildren( bSysObjOnlySmaller ) )
                bUpdate = false;
            pWindow = pWindow->mpWindowImpl->mpNext;
        }
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

bool Window::ImplSetClipFlag( bool bSysObjOnlySmaller )
{
    if ( !ImplIsOverlapWindow() )
    {
        bool bUpdate = ImplSetClipFlagChildren( bSysObjOnlySmaller );
        vcl::Window* pParent = ImplGetParent();
        if ( pParent &&
             ((pParent->GetStyle() & WB_CLIPCHILDREN) || (mpWindowImpl->mnParentClipMode & ParentClipMode::Clip)) )
        {
            pParent->GetOutDev()->GetClippingController().SetDirty(true);
            pParent->mpWindowImpl->mbInitChildRegion = true;
        }

        if ( mpWindowImpl->mbClipSiblings )
        {
            vcl::Window* pWindow = mpWindowImpl->mpNext;
            while ( pWindow )
            {
                if ( !pWindow->ImplSetClipFlagChildren( bSysObjOnlySmaller ) )
                    bUpdate = false;
                pWindow = pWindow->mpWindowImpl->mpNext;
            }
        }
        return bUpdate;
    }
    else
        return mpWindowImpl->mpFrameWindow->ImplSetClipFlagOverlapWindows( bSysObjOnlySmaller );
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
        rRegion.Intersect( GetOutDev()->PixelToDevicePixel( mpWindowImpl->maWinRegion ) );
}

void Window::ImplExcludeWindowRegion( vcl::Region& rRegion )
{
    if ( mpWindowImpl->mbWinRegion )
    {
        vcl::Region aRegion( GetOutputRectPixel() );
        aRegion.Intersect( GetOutDev()->PixelToDevicePixel( mpWindowImpl->maWinRegion ) );
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

    if ( !ImplIsOverlapWindow() )
        mpWindowImpl->mpOverlapWindow->ImplIntersectAndUnionOverlapWindows( rInterRegion, rRegion );
    else
        ImplIntersectAndUnionOverlapWindows( rInterRegion, rRegion );
}

void Window::ImplCalcOverlapRegion( const tools::Rectangle& rSourceRect, vcl::Region& rRegion,
                                    bool bChildren, bool bSiblings )
{
    vcl::Region aRegion( rSourceRect );
    if ( mpWindowImpl->mbWinRegion )
        rRegion.Intersect( GetOutDev()->PixelToDevicePixel( mpWindowImpl->maWinRegion ) );
    vcl::Region aTempRegion;
    vcl::Window* pWindow;

    ImplCalcOverlapRegionOverlaps( aRegion, rRegion );

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

void WindowOutputDevice::ImplSaveWindowBackground(VirtualDevice& rSaveDevice, const Point& rPos, const Size& rSize) const
{
    MapMode aTempMap(GetMapMode());
    aTempMap.SetOrigin(Point());
    rSaveDevice.SetMapMode(aTempMap);

    // This is the Window-specific secret sauce
    if (mxOwnerWindow->mpWindowImpl->mpPaintRegion)
    {
        vcl::Region aClip(*mxOwnerWindow->mpWindowImpl->mpPaintRegion);
        const Point aPixPos(LogicToPixel(rPos));

        aClip.Move(-GetOutOffXPixel(), -GetOutOffYPixel());
        aClip.Intersect(tools::Rectangle(aPixPos, LogicToPixel(rSize)));

        if (!aClip.IsEmpty())
        {
            const vcl::Region aOldClip(rSaveDevice.GetClipRegion());
            const Point aPixOffset(rSaveDevice.LogicToPixel(Point()));
            const bool bMap = rSaveDevice.IsMapModeEnabled();

            aClip.Move(aPixOffset.X() - aPixPos.X(), aPixOffset.Y() - aPixPos.Y());

            rSaveDevice.EnableMapMode(false);
            rSaveDevice.SetClipRegion(aClip);
            rSaveDevice.EnableMapMode(bMap);
            rSaveDevice.DrawOutDev(Point(), rSize, rPos, rSize, *this);
            rSaveDevice.SetClipRegion(aOldClip);
        }
    }
    else
    {
        rSaveDevice.DrawOutDev(Point(), rSize, rPos, rSize, *this);
    }

    rSaveDevice.SetMapMode(MapMode());
}

} // namespace vcl

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
