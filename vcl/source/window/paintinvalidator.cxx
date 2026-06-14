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

#include <clipping_window.hxx>
#include <salframe.hxx>
#include <salgeom.hxx>
#include <salobj.hxx>
#include <window.h>

namespace vcl {

void Window::ImplInvalidateFrameRegion( const vcl::Region* pRegion, InvalidateFlags nFlags )
{
    // set PAINTCHILDREN for all parent windows till the first OverlapWindow
    if ( !ImplIsOverlapWindow() )
    {
        vcl::Window* pTempWindow = this;
        ImplPaintFlags nTranspPaint = IsPaintTransparent() ? ImplPaintFlags::Paint : ImplPaintFlags::NONE;
        do
        {
            pTempWindow = pTempWindow->ImplGetParent();
            if ( pTempWindow->mpWindowImpl->mnPaintFlags & ImplPaintFlags::PaintChildren )
                break;
            pTempWindow->mpWindowImpl->mnPaintFlags |= ImplPaintFlags::PaintChildren | nTranspPaint;
            if( ! pTempWindow->IsPaintTransparent() )
                nTranspPaint = ImplPaintFlags::NONE;
        }
        while ( !pTempWindow->ImplIsOverlapWindow() );
    }

    // set Paint-Flags
    mpWindowImpl->mnPaintFlags |= ImplPaintFlags::Paint;
    if ( nFlags & InvalidateFlags::Children )
        mpWindowImpl->mnPaintFlags |= ImplPaintFlags::PaintAllChildren;
    if ( !(nFlags & InvalidateFlags::NoErase) )
        mpWindowImpl->mnPaintFlags |= ImplPaintFlags::Erase;

    if ( !pRegion )
        mpWindowImpl->mnPaintFlags |= ImplPaintFlags::PaintAll;
    else if ( !(mpWindowImpl->mnPaintFlags & ImplPaintFlags::PaintAll) )
    {
        // if not everything has to be redrawn, add the region to it
        mpWindowImpl->maInvalidateRegion.Union( *pRegion );
    }

    // Handle transparent windows correctly: invalidate must be done on the first opaque parent
    if( ((IsPaintTransparent() && !(nFlags & InvalidateFlags::NoTransparent)) || (nFlags & InvalidateFlags::Transparent) )
            && ImplGetParent() )
    {
        vcl::Window *pParent = ImplGetParent();
        while( pParent && pParent->IsPaintTransparent() )
            pParent = pParent->ImplGetParent();
        if( pParent )
        {
            vcl::Region *pChildRegion;
            if ( mpWindowImpl->mnPaintFlags & ImplPaintFlags::PaintAll )
                // invalidate the whole child window region in the parent
                pChildRegion = &vcl::clipping::getWinChildClipRegion(*this);
            else
                // invalidate the same region in the parent that has to be repainted in the child
                pChildRegion = &mpWindowImpl->maInvalidateRegion;

            nFlags |= InvalidateFlags::Children;  // paint should also be done on all children
            nFlags &= ~InvalidateFlags::NoErase;  // parent should paint and erase to create proper background
            pParent->ImplInvalidateFrameRegion( pChildRegion, nFlags );
        }
    }

    if ( !mpWindowImpl->mpFrameData->maPaintIdle.IsActive() )
        mpWindowImpl->mpFrameData->maPaintIdle.Start();
}

void Window::ImplInvalidateOverlapFrameRegion( const vcl::Region& rRegion )
{
    vcl::Region aRegion = rRegion;

    vcl::clipping::clipBoundaries(*this, aRegion, true, true);
    if ( !aRegion.IsEmpty() )
        ImplInvalidateFrameRegion( &aRegion, InvalidateFlags::Children );

    // now we invalidate the overlapping windows
    vcl::Window* pTempWindow = mpWindowImpl->mpHierarchy->mpFirstOverlap;
    while ( pTempWindow )
    {
        if ( pTempWindow->IsVisible() )
            pTempWindow->ImplInvalidateOverlapFrameRegion( rRegion );

        pTempWindow = pTempWindow->mpWindowImpl->mpHierarchy->mpNext;
    }
}

void Window::ImplInvalidateParentFrameRegion( const vcl::Region& rRegion )
{
    if ( mpWindowImpl->mbOverlapWin )
        mpWindowImpl->mpFrameWindow->ImplInvalidateOverlapFrameRegion( rRegion );
    else
    {
        if( ImplGetParent() )
            ImplGetParent()->ImplInvalidateFrameRegion( &rRegion, InvalidateFlags::Children );
    }
}

void Window::ImplInvalidate( const vcl::Region* pRegion, InvalidateFlags nFlags )
{
    // check what has to be redrawn
    bool bInvalidateAll = !pRegion;

    // take Transparent-Invalidate into account
    vcl::Window* pOpaqueWindow = this;
    if ( (mpWindowImpl->mbPaintTransparent && !(nFlags & InvalidateFlags::NoTransparent)) || (nFlags & InvalidateFlags::Transparent) )
    {
        vcl::Window* pTempWindow = pOpaqueWindow->ImplGetParent();
        while ( pTempWindow )
        {
            if ( !pTempWindow->IsPaintTransparent() )
            {
                pOpaqueWindow = pTempWindow;
                nFlags |= InvalidateFlags::Children;
                bInvalidateAll = false;
                break;
            }

            if ( pTempWindow->ImplIsOverlapWindow() )
                break;

            pTempWindow = pTempWindow->ImplGetParent();
        }
    }

    // assemble region
    InvalidateFlags nOrgFlags = nFlags;
    if ( !(nFlags & (InvalidateFlags::Children | InvalidateFlags::NoChildren)) )
    {
        if ( GetStyle() & WB_CLIPCHILDREN )
            nFlags |= InvalidateFlags::NoChildren;
        else
            nFlags |= InvalidateFlags::Children;
    }
    if ( (nFlags & InvalidateFlags::NoChildren) && mpWindowImpl->mpHierarchy->mpFirstChild )
        bInvalidateAll = false;
    if ( bInvalidateAll )
        ImplInvalidateFrameRegion( nullptr, nFlags );
    else
    {
        vcl::Region      aRegion( GetOutputRectPixel() );
        if ( pRegion )
        {
            // RTL: remirror region before intersecting it
            if ( GetOutDev()->ImplIsAntiparallel() )
            {
                const OutputDevice *pOutDev = GetOutDev();

                vcl::Region aRgn( *pRegion );
                pOutDev->ReMirror( aRgn );
                aRegion.Intersect( aRgn );
            }
            else
                aRegion.Intersect( *pRegion );
        }

        vcl::clipping::clipBoundaries(*this, aRegion, true, true);

        if ( nFlags & InvalidateFlags::NoChildren )
        {
            nFlags &= ~InvalidateFlags::Children;
            if ( !(nFlags & InvalidateFlags::NoClipChildren) )
            {
                if ( nOrgFlags & InvalidateFlags::NoChildren )
                    vcl::clipping::clipAllChildren(*this, aRegion);
                else
                {
                    if (vcl::clipping::clipChildren(*this, aRegion))
                        nFlags |= InvalidateFlags::Children;
                }
            }
        }
        if ( !aRegion.IsEmpty() )
            ImplInvalidateFrameRegion( &aRegion, nFlags );  // transparency is handled here, pOpaqueWindow not required
    }

    if ( nFlags & InvalidateFlags::Update )
        pOpaqueWindow->PaintImmediately();        // start painting at the opaque parent
}

void Window::ImplMoveInvalidateRegion( const tools::Rectangle& rRect,
                                       tools::Long nHorzScroll, tools::Long nVertScroll,
                                       bool bChildren )
{
    if ( (mpWindowImpl->mnPaintFlags & (ImplPaintFlags::Paint | ImplPaintFlags::PaintAll)) == ImplPaintFlags::Paint )
    {
        vcl::Region aTempRegion = mpWindowImpl->maInvalidateRegion;
        aTempRegion.Intersect( rRect );
        aTempRegion.Move( nHorzScroll, nVertScroll );
        mpWindowImpl->maInvalidateRegion.Union( aTempRegion );
    }

    if ( bChildren && (mpWindowImpl->mnPaintFlags & ImplPaintFlags::PaintChildren) )
    {
        vcl::Window* pWindow = mpWindowImpl->mpHierarchy->mpFirstChild;
        while ( pWindow )
        {
            pWindow->ImplMoveInvalidateRegion( rRect, nHorzScroll, nVertScroll, true );
            pWindow = pWindow->mpWindowImpl->mpHierarchy->mpNext;
        }
    }
}

void Window::ImplMoveAllInvalidateRegions( const tools::Rectangle& rRect,
                                           tools::Long nHorzScroll, tools::Long nVertScroll,
                                           bool bChildren )
{
    // also shift Paint-Region when paints need processing
    ImplMoveInvalidateRegion( rRect, nHorzScroll, nVertScroll, bChildren );
    // Paint-Region should be shifted, as drawn by the parents
    if ( ImplIsOverlapWindow() )
        return;

    vcl::Region  aPaintAllRegion;
    vcl::Window* pPaintAllWindow = this;
    do
    {
        pPaintAllWindow = pPaintAllWindow->ImplGetParent();
        if ( pPaintAllWindow->mpWindowImpl->mnPaintFlags & ImplPaintFlags::PaintAllChildren )
        {
            if ( pPaintAllWindow->mpWindowImpl->mnPaintFlags & ImplPaintFlags::PaintAll )
            {
                aPaintAllRegion.SetEmpty();
                break;
            }
            else
                aPaintAllRegion.Union( pPaintAllWindow->mpWindowImpl->maInvalidateRegion );
        }
    }
    while ( !pPaintAllWindow->ImplIsOverlapWindow() );
    if ( !aPaintAllRegion.IsEmpty() )
    {
        aPaintAllRegion.Move( nHorzScroll, nVertScroll );
        InvalidateFlags nPaintFlags = InvalidateFlags::NONE;
        if ( bChildren )
            nPaintFlags |= InvalidateFlags::Children;
        ImplInvalidateFrameRegion( &aPaintAllRegion, nPaintFlags );
    }
}

void Window::ImplValidateFrameRegion( const vcl::Region* pRegion, ValidateFlags nFlags )
{
    if ( !pRegion )
        mpWindowImpl->maInvalidateRegion.SetEmpty();
    else
    {
        // when all child windows have to be drawn we need to invalidate them before doing so
        if ( (mpWindowImpl->mnPaintFlags & ImplPaintFlags::PaintAllChildren) && mpWindowImpl->mpHierarchy->mpFirstChild )
        {
            vcl::Region aChildRegion = mpWindowImpl->maInvalidateRegion;
            if ( mpWindowImpl->mnPaintFlags & ImplPaintFlags::PaintAll )
            {
                aChildRegion = GetOutputRectPixel();
            }
            vcl::Window* pChild = mpWindowImpl->mpHierarchy->mpFirstChild;
            while ( pChild )
            {
                pChild->Invalidate( aChildRegion, InvalidateFlags::Children | InvalidateFlags::NoTransparent );
                pChild = pChild->mpWindowImpl->mpHierarchy->mpNext;
            }
        }
        if ( mpWindowImpl->mnPaintFlags & ImplPaintFlags::PaintAll )
        {
            mpWindowImpl->maInvalidateRegion = GetOutputRectPixel();
        }
        mpWindowImpl->maInvalidateRegion.Exclude( *pRegion );
    }
    mpWindowImpl->mnPaintFlags &= ~ImplPaintFlags::PaintAll;

    if ( nFlags & ValidateFlags::Children )
    {
        vcl::Window* pChild = mpWindowImpl->mpHierarchy->mpFirstChild;
        while ( pChild )
        {
            pChild->ImplValidateFrameRegion( pRegion, nFlags );
            pChild = pChild->mpWindowImpl->mpHierarchy->mpNext;
        }
    }
}

void Window::ImplValidate()
{
    // assemble region
    bool    bValidateAll = true;
    ValidateFlags nFlags = ValidateFlags::NONE;
    if ( GetStyle() & WB_CLIPCHILDREN )
        nFlags |= ValidateFlags::NoChildren;
    else
        nFlags |= ValidateFlags::Children;
    if ( (nFlags & ValidateFlags::NoChildren) && mpWindowImpl->mpHierarchy->mpFirstChild )
        bValidateAll = false;
    if ( bValidateAll )
        ImplValidateFrameRegion( nullptr, nFlags );
    else
    {
        vcl::Region      aRegion( GetOutputRectPixel() );
        vcl::clipping::clipBoundaries(*this, aRegion, true, true);

        if ( nFlags & ValidateFlags::NoChildren )
        {
            nFlags &= ~ValidateFlags::Children;
            if (vcl::clipping::clipChildren(*this, aRegion))
                nFlags |= ValidateFlags::Children;
        }
        if ( !aRegion.IsEmpty() )
            ImplValidateFrameRegion( &aRegion, nFlags );
    }
}

void Window::ImplUpdateAll()
{
    if ( !mpWindowImpl || !mpWindowImpl->mbReallyVisible )
        return;

    bool bFlush = false;
    if ( mpWindowImpl->mpFrameWindow->mpWindowImpl->mbPaintFrame )
    {
        Point aPoint( 0, 0 );
        vcl::Region aRegion( tools::Rectangle( aPoint, GetOutputSizePixel() ) );
        ImplInvalidateOverlapFrameRegion( aRegion );
        if ( mpWindowImpl->mbFrame || (mpWindowImpl->mpBorderWindow && mpWindowImpl->mpBorderWindow->mpWindowImpl->mbFrame) )
            bFlush = true;
    }

    // an update changes the OverlapWindow, such that for later paints
    // not too much has to be drawn, if ALLCHILDREN etc. is set
    vcl::Window* pWindow = ImplGetFirstOverlapWindow();
    pWindow->ImplCallOverlapPaint();

    if ( bFlush )
        GetOutDev()->Flush();
}

} /* namespace vcl */

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
