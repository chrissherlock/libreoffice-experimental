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
#include <salgeom.hxx>
#include <salobj.hxx>
#include <window.h>

namespace vcl {

vcl::Region WindowOutputDevice::GetOutputBoundsClipRegion() const
{
    vcl::Region aClip(GetClipRegion());
    aClip.Intersect(tools::Rectangle(Point(), GetOutputSize()));

    return aClip;
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
    GetOutDev()->GetClipState().Invalidate();
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

void Window::EnableClipSiblings(bool bClipSiblings)
{
    if (mpWindowImpl->mpBorderWindow)
        mpWindowImpl->mpBorderWindow->EnableClipSiblings(bClipSiblings);

    mpWindowImpl->mpClippingState->mbClipSiblings = bClipSiblings;
}

// with decoration
tools::Rectangle Window::GetWindowExtentsRelative(const vcl::Window & rRelativeWindow) const
{
    AbsoluteScreenPixelRectangle aRect = GetWindowExtentsAbsolute();
    // #106399# express coordinates relative to borderwindow
    const vcl::Window *pRelWin = rRelativeWindow.mpWindowImpl->mpBorderWindow ? rRelativeWindow.mpWindowImpl->mpBorderWindow.get() : &rRelativeWindow;
    return tools::Rectangle(
        pRelWin->AbsoluteScreenToOutputPixel( aRect.GetPos() ),
        aRect.GetSize() );
}

// with decoration
AbsoluteScreenPixelRectangle Window::GetWindowExtentsAbsolute() const
{
    // make sure we use the extent of our border window,
    // otherwise we miss a few pixels
    const vcl::Window *pWin = mpWindowImpl->mpBorderWindow ? mpWindowImpl->mpBorderWindow : this;

    AbsoluteScreenPixelPoint aPos( pWin->OutputToAbsoluteScreenPixel( Point(0,0) ) );
    Size aSize ( pWin->GetSizePixel() );
    // #104088# do not add decoration to the workwindow to be compatible to java accessibility api
    if( mpWindowImpl->mbFrame || (mpWindowImpl->mpBorderWindow && mpWindowImpl->mpBorderWindow->mpWindowImpl->mbFrame && GetType() != WindowType::WORKWINDOW) )
    {
        SalFrameGeometry g = mpWindowImpl->mpFrame->GetGeometry();
        aPos.AdjustX( -sal_Int32(g.leftDecoration()) );
        aPos.AdjustY( -sal_Int32(g.topDecoration()) );
        aSize.AdjustWidth(g.leftDecoration() + g.rightDecoration() );
        aSize.AdjustHeight(g.topDecoration() + g.bottomDecoration() );
    }
    return AbsoluteScreenPixelRectangle( aPos, aSize );
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
