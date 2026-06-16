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

#include <clipping/ClippingObserver.hxx>
#include <clipping_window.hxx>
#include <salframe.hxx>
#include <salgeom.hxx>
#include <salobj.hxx>
#include <window.h>

namespace vcl {

void Window::InvalidateClipState()
{
    mpWindowImpl->mnClipStateVersion++;

    if (mpWindowImpl->mpClippingObserver)
        mpWindowImpl->mpClippingObserver->OnWindowGeometryChanged(*this);

    if (auto* pParent = GetParent())
        pParent->InvalidateClipState();
}

sal_uInt64 Window::GetClipStateVersion() const { return mpWindowImpl->mnClipStateVersion; }

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

Point Window::OutputToScreenPixel( const Point& rPos ) const
{
    // relative to top level parent
    return Point( rPos.X() + GetOutDev()->GetDeviceOriginX(), rPos.Y() + GetOutDev()->GetDeviceOriginY() );
}

Point Window::ScreenToOutputPixel( const Point& rPos ) const
{
    // relative to top level parent
    return Point( rPos.X() - GetOutDev()->GetDeviceOriginX(), rPos.Y() - GetOutDev()->GetDeviceOriginY() );
}

// normalized screen pixel are independent of mirroring
Point Window::OutputToNormalizedScreenPixel( const Point& rPos ) const
{
    // relative to top level parent
    tools::Long offx = ImplGetUnmirroredOutOffX();
    return Point( rPos.X()+offx, rPos.Y() + GetOutDev()->GetDeviceOriginY() );
}

Point Window::NormalizedScreenToOutputPixel( const Point& rPos ) const
{
    // relative to top level parent
    tools::Long offx = ImplGetUnmirroredOutOffX();
    return Point( rPos.X()-offx, rPos.Y() - GetOutDev()->GetDeviceOriginY() );
}

AbsoluteScreenPixelPoint Window::OutputToAbsoluteScreenPixel( const Point& rPos ) const
{
    // relative to the screen
    Point p = OutputToScreenPixel( rPos );
    SalFrameGeometry g = mpWindowImpl->mpFrame->GetGeometry();
    p.AdjustX(g.x() );
    p.AdjustY(g.y() );
    return AbsoluteScreenPixelPoint(p);
}

Point Window::AbsoluteScreenToOutputPixel( const AbsoluteScreenPixelPoint& rPos ) const
{
    // relative to the screen
    Point p = ScreenToOutputPixel( Point(rPos) );
    SalFrameGeometry g = mpWindowImpl->mpFrame->GetGeometry();
    p.AdjustX( -(g.x()) );
    p.AdjustY( -(g.y()) );
    return p;
}

AbsoluteScreenPixelRectangle Window::ImplOutputToUnmirroredAbsoluteScreenPixel( const tools::Rectangle &rRect ) const
{
    // this method creates unmirrored screen coordinates to be compared with the desktop
    // and is used for positioning of RTL popup windows correctly on the screen
    SalFrameGeometry g = mpWindowImpl->mpFrame->GetUnmirroredGeometry();

    Point p1 = rRect.TopRight();
    p1 = OutputToScreenPixel(p1);
    p1.setX( g.x()+g.width()-p1.X() );
    p1.AdjustY(g.y() );

    Point p2 = rRect.BottomLeft();
    p2 = OutputToScreenPixel(p2);
    p2.setX( g.x()+g.width()-p2.X() );
    p2.AdjustY(g.y() );

    return AbsoluteScreenPixelRectangle( AbsoluteScreenPixelPoint(p1), AbsoluteScreenPixelPoint(p2) );
}

tools::Rectangle Window::ImplUnmirroredAbsoluteScreenToOutputPixel( const AbsoluteScreenPixelRectangle &rRect ) const
{
    // undo ImplOutputToUnmirroredAbsoluteScreenPixel
    SalFrameGeometry g = mpWindowImpl->mpFrame->GetUnmirroredGeometry();

    Point p1( rRect.TopRight() );
    p1.AdjustY(-g.y() );
    p1.setX( g.x()+g.width()-p1.X() );
    p1 = ScreenToOutputPixel(p1);

    Point p2( rRect.BottomLeft() );
    p2.AdjustY(-g.y());
    p2.setX( g.x()+g.width()-p2.X() );
    p2 = ScreenToOutputPixel(p2);

    return tools::Rectangle( p1, p2 );
}

} /* namespace vcl */

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
