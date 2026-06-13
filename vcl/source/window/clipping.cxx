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
        aRegion = vcl::clipping::getWinChildClipRegion(*mxOwnerWindow);

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

void Window::SetParentClipMode(ParentClipMode eMode)
{
    vcl::clipping::setParentClipMode(this, eMode);
}

ParentClipMode Window::GetParentClipMode() const
{
    return vcl::clipping::getParentClipMode(*this);
}

void Window::ExpandPaintClipRegion(const vcl::Region& rRegion)
{
    if (!mpWindowImpl->mpPaintRegion)
        return;

    WindowRegion aPixRegion(rRegion);
    vcl::Region aDevPixRegion = GetOutDev()->GetMapper().ViewToDevice(aPixRegion.get());

    vcl::Region aWinChildRegion = vcl::clipping::getWinChildClipRegion(*this);

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
        clipping::initWinClipRegion(*this);

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

void Window::ImplCalcOverlapRegion( const tools::Rectangle& rSourceRect, vcl::Region& rRegion,
                                    bool bChildren, bool bSiblings )
{
    vcl::Region  aRegion( rSourceRect );
    if ( mpWindowImpl->mpClippingState->mbWinRegion )
        rRegion.Intersect( GetOutDev()->GetMapper().ViewToDevice( mpWindowImpl->mpClippingState->maWinRegion ) );
    vcl::Region  aTempRegion;
    vcl::Window* pWindow;

    vcl::clipping::calcOverlapRegionOverlaps(*this, aRegion, rRegion);

    // Parent-Boundaries
    pWindow = this;
    if ( !ImplIsOverlapWindow() )
    {
        pWindow = ImplGetParent();
        do
        {
            aTempRegion = aRegion;
            vcl::clipping::excludeWindowRegion(*pWindow, aTempRegion);
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
    if (bSiblings && !ImplIsOverlapWindow())
    {
        for (vcl::Window* pSibling : vcl::clipping::getChildWindows(*ImplGetParent()->ImplGetWindowImpl()))
        {
            if (pSibling->ImplGetWindowImpl()->mbReallyVisible && (pSibling != this))
            {
                aTempRegion = aRegion;
                vcl::clipping::intersectWindowRegion(*pSibling, aTempRegion);
                rRegion.Union(aTempRegion);
            }
        }
    }

    if ( !bChildren )
        return;

    // Children
    for (vcl::Window* pChild : vcl::clipping::getChildWindows(*mpWindowImpl))
    {
        if (pChild->ImplGetWindowImpl()->mbReallyVisible)
        {
            aTempRegion = aRegion;
            vcl::clipping::intersectWindowRegion(*pChild, aTempRegion);
            rRegion.Union(aTempRegion);
        }
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
