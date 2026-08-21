/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
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

#include <comphelper/lok.hxx>

#include <vcl/CoordinateMapper.hxx>
#include <vcl/layout.hxx>
#include <vcl/syswin.hxx>
#include <vcl/window.hxx>

#include <window.h>
#include <brdwin.hxx>
#include <clipping_window.hxx>
#include <salframe.hxx>
#include <salgdi.hxx>
#include <salobj.hxx>

namespace vcl
{
static VclPtr<vcl::Window> lcl_GetTopmostBorderWindow(vcl::Window* pStartWindow)
{
    vcl::Window* pWindow = pStartWindow;

    while (pWindow->ImplGetWindowImpl()->mpBorderWindow)
    {
        pWindow = pWindow->ImplGetWindowImpl()->mpBorderWindow.get();
    }

    return pWindow; // Implicitly converts back to VclPtr
}

static bool lcl_SetChildWindowPosSize(vcl::Window* pOriginalWindow, vcl::Window* pBorderWindow,
                                      tools::Long nX, tools::Long nY, tools::Long nWidth,
                                      tools::Long nHeight, PosSizeFlags nFlags)
{
    if (pBorderWindow->ImplGetWindowImpl()->mbFrame)
        return false;

    pBorderWindow->ImplPosSizeWindow(nX, nY, nWidth, nHeight, nFlags);

    if (pOriginalWindow->IsReallyVisible())
        pOriginalWindow->ImplGenerateMouseMove();

    return true;
}

static std::pair<tools::Long, tools::Long> lcl_CalcMissingDimensions(vcl::Window* pBorderWindow,
                                                                     tools::Long nWidth,
                                                                     tools::Long nHeight,
                                                                     PosSizeFlags nFlags)
{
    if (!(nFlags & PosSizeFlags::Width))
        nWidth = pBorderWindow->GetOutDev()->GetOutputWidthPixel();

    if (!(nFlags & PosSizeFlags::Height))
        nHeight = pBorderWindow->GetOutDev()->GetOutputHeightPixel();

    return std::make_pair(nWidth, nHeight);
}

static sal_uInt16 lcl_ConvertPosSizeToSysFlags(PosSizeFlags nFlags)
{
    sal_uInt16 nSysFlags = 0;

    if (nFlags & PosSizeFlags::X)
        nSysFlags |= SAL_FRAME_POSSIZE_X;

    if (nFlags & PosSizeFlags::Y)
        nSysFlags |= SAL_FRAME_POSSIZE_Y;

    if (nFlags & PosSizeFlags::Width)
        nSysFlags |= SAL_FRAME_POSSIZE_WIDTH;

    if (nFlags & PosSizeFlags::Height)
        nSysFlags |= SAL_FRAME_POSSIZE_HEIGHT;

    return nSysFlags;
}

static tools::Long lcl_CalculatePositionX(vcl::Window* pWindow, vcl::Window* pBorderWindow,
                                          tools::Long nX, tools::Long nY, tools::Long nWidth,
                                          tools::Long nHeight, PosSizeFlags nFlags)
{
    if (!(nFlags & PosSizeFlags::X))
        return nX;

    VclPtr<vcl::Window> pParent = pWindow->GetParent();
    VclPtr<vcl::Window> pWinParent = pBorderWindow->GetParent();

    if (pWinParent && (pBorderWindow->GetStyle() & WB_SYSTEMCHILDWINDOW))
    {
        nX += pWinParent->GetOutDev()->GetDeviceOriginX();
    }

    if (pParent && pParent->GetOutDev()->ImplIsAntiparallel())
    {
        tools::Rectangle aRect(Point(nX, nY), Size(nWidth, nHeight));
        const OutputDevice* pParentOutDev = pParent->GetOutDev();
        if (!comphelper::LibreOfficeKit::isActive())
            pParentOutDev->ReMirror(aRect);
        nX = aRect.Left();
    }

    return nX;
}

static bool lcl_ShouldPreserveRTLPosition(PosSizeFlags nFlags, bool bHasValidSize,
                                          tools::Long nWidth)
{
    if (comphelper::LibreOfficeKit::isActive())
        return false;

    if (nFlags & PosSizeFlags::X)
        return false;

    if (!bHasValidSize)
        return false;

    return nWidth != 0;
}

void Window::setPosSizePixel(tools::Long nX, tools::Long nY, tools::Long nWidth,
                             tools::Long nHeight, PosSizeFlags nFlags)
{
    if (nFlags & PosSizeFlags::Pos)
        mpWindowImpl->mbDefPos = false;

    if (nFlags & PosSizeFlags::Size)
        mpWindowImpl->mbDefSize = false;

    // The top BorderWindow is the window which is to be positioned
    VclPtr<vcl::Window> pBorderWindow = lcl_GetTopmostBorderWindow(this);

    if (lcl_SetChildWindowPosSize(this, pBorderWindow.get(), nX, nY, nWidth, nHeight, nFlags))
        return;

    // Note: if we're positioning a frame, the coordinates are interpreted
    // as being the top-left corner of the window's client area and NOT
    // as the position of the border ! (due to limitations of several UNIX window managers)
    std::tie(nWidth, nHeight)
        = lcl_CalcMissingDimensions(pBorderWindow.get(), nWidth, nHeight, nFlags);

    sal_uInt16 nSysFlags = lcl_ConvertPosSizeToSysFlags(nFlags);

    VclPtr<vcl::Window> pParent = GetParent();
    VclPtr<vcl::Window> pWinParent = pBorderWindow->GetParent();

    nX = lcl_CalculatePositionX(this, pBorderWindow.get(), nX, nY, nWidth, nHeight, nFlags);

    if (pWinParent && (pBorderWindow->GetStyle() & WB_SYSTEMCHILDWINDOW))
        nX += pWinParent->GetOutDev()->GetDeviceOriginX();

    // RTL: make sure the old right aligned position is not changed
    // system windows will always grow to the right

    const bool bHasValidSize = !mpWindowImpl->mbDefSize;

    if (pWinParent && pWinParent->GetOutDev()->HasMirroredGraphics()
        && lcl_ShouldPreserveRTLPosition(nFlags, bHasValidSize,
                                         pBorderWindow->mpWindowImpl->mpFrame->GetWidth()))
    {
        nFlags |= PosSizeFlags::X;
        nSysFlags |= SAL_FRAME_POSSIZE_X;

        const SalFrameGeometry aSysGeometry = mpWindowImpl->mpFrame->GetUnmirroredGeometry();
        const SalFrameGeometry aParentSysGeometry
            = pWinParent->mpWindowImpl->mpFrame->GetUnmirroredGeometry();

        tools::Long nBorderWinWidth = pBorderWindow->GetOutDev()->GetOutputWidthPixel();

        if (!nBorderWinWidth)
            nBorderWinWidth = aSysGeometry.width();

        if (!nBorderWinWidth)
            nBorderWinWidth = nWidth;

        nX = aParentSysGeometry.x() - aSysGeometry.leftDecoration() + aParentSysGeometry.width()
             - nBorderWinWidth - 1 - aSysGeometry.x();
    }

    if (nFlags & PosSizeFlags::Y)
    {
        if (pWinParent && (pBorderWindow->GetStyle() & WB_SYSTEMCHILDWINDOW))
            nY += pWinParent->GetOutDev()->GetDeviceOriginY();
    }

    if (nSysFlags & (SAL_FRAME_POSSIZE_WIDTH | SAL_FRAME_POSSIZE_HEIGHT))
    {
        // check for min/max client size and adjust size accordingly
        // otherwise it may happen that the resize event is ignored, i.e. the old size remains
        // unchanged but ImplHandleResize() is called with the wrong size
        SystemWindow* pSystemWindow = dynamic_cast<SystemWindow*>(pBorderWindow.get());
        if (pSystemWindow)
        {
            Size aMinSize = pSystemWindow->GetMinOutputSizePixel();
            Size aMaxSize = pSystemWindow->GetMaxOutputSizePixel();
            if (nWidth < aMinSize.Width())
                nWidth = aMinSize.Width();
            if (nHeight < aMinSize.Height())
                nHeight = aMinSize.Height();

            if (nWidth > aMaxSize.Width())
                nWidth = aMaxSize.Width();
            if (nHeight > aMaxSize.Height())
                nHeight = aMaxSize.Height();
        }
    }

    pBorderWindow->mpWindowImpl->mpFrame->SetPosSize(nX, nY, nWidth, nHeight, nSysFlags);

    // Adjust resize with the hack of different client size and frame geometries to fix
    // native menu bars. Eventually this should be replaced by proper mnTopBorder usage.
    const Size aClientSize = pBorderWindow->mpWindowImpl->mpFrame->GetClientSize();

    // Resize should be called directly. If we haven't
    // set the correct size, we get a second resize from
    // the system with the correct size. This can be happened
    // if the size is too small or too large.
    ImplHandleResize(pBorderWindow, aClientSize.getWidth(), aClientSize.Height());
}

Point Window::GetPosPixel() const { return mpWindowImpl->maPos; }

void Window::SetPosPixel(const Point& rNewPos)
{
    setPosSizePixel(rNewPos.X(), rNewPos.Y(), 0, 0, PosSizeFlags::Pos);
}

Size Window::GetSizePixel() const
{
    if (!mpWindowImpl)
    {
        SAL_WARN("vcl.layout", "WTF no windowimpl");
        return Size(0, 0);
    }

    // #i43257# trigger pending resize handler to assure correct window sizes
    if (mpWindowImpl->mpFrameData->maResizeIdle.IsActive())
    {
        VclPtr<vcl::Window> xWindow(const_cast<Window*>(this));
        mpWindowImpl->mpFrameData->maResizeIdle.Stop();
        mpWindowImpl->mpFrameData->maResizeIdle.Invoke(nullptr);
        if (xWindow->isDisposed())
            return Size(0, 0);
    }

    return Size(GetOutDev()->GetOutputWidthPixel() + mpWindowImpl->mnLeftBorder
                    + mpWindowImpl->mnRightBorder,
                GetOutDev()->GetOutputHeightPixel() + mpWindowImpl->mnTopBorder
                    + mpWindowImpl->mnBottomBorder);
}

void Window::SetSizePixel(const Size& rNewSize)
{
    setPosSizePixel(0, 0, rNewSize.Width(), rNewSize.Height(), PosSizeFlags::Size);
}

void Window::SetPosSizePixel(const Point& rNewPos, const Size& rNewSize)
{
    setPosSizePixel(rNewPos.X(), rNewPos.Y(), rNewSize.Width(), rNewSize.Height());
}

void Window::SetOutputSizePixel(const Size& rNewSize)
{
    SetSizePixel(
        Size(rNewSize.Width() + mpWindowImpl->mnLeftBorder + mpWindowImpl->mnRightBorder,
             rNewSize.Height() + mpWindowImpl->mnTopBorder + mpWindowImpl->mnBottomBorder));
}

bool Window::IsDefaultPos() const { return mpWindowImpl->mbDefPos; }

bool Window::IsDefaultSize() const { return mpWindowImpl->mbDefSize; }

Point Window::GetOffsetPixelFrom(const vcl::Window& rWindow) const
{
    return Point(GetDeviceOriginX() - rWindow.GetDeviceOriginX(),
                 GetDeviceOriginY() - rWindow.GetDeviceOriginY());
}

Point Window::OutputToScreenPixel(const Point& rPos) const
{
    // relative to top level parent
    return Point(rPos.X() + GetOutDev()->GetDeviceOriginX(),
                 rPos.Y() + GetOutDev()->GetDeviceOriginY());
}

Point Window::ScreenToOutputPixel(const Point& rPos) const
{
    // relative to top level parent
    return Point(rPos.X() - GetOutDev()->GetDeviceOriginX(),
                 rPos.Y() - GetOutDev()->GetDeviceOriginY());
}

// normalized screen pixel are independent of mirroring
Point Window::OutputToNormalizedScreenPixel(const Point& rPos) const
{
    // relative to top level parent
    tools::Long offx = ImplGetUnmirroredOutOffX();
    return Point(rPos.X() + offx, rPos.Y() + GetOutDev()->GetDeviceOriginY());
}

Point Window::NormalizedScreenToOutputPixel(const Point& rPos) const
{
    // relative to top level parent
    tools::Long offx = ImplGetUnmirroredOutOffX();
    return Point(rPos.X() - offx, rPos.Y() - GetOutDev()->GetDeviceOriginY());
}

AbsoluteScreenPixelPoint Window::OutputToAbsoluteScreenPixel(const Point& rPos) const
{
    // relative to the screen
    Point p = OutputToScreenPixel(rPos);
    SalFrameGeometry g = mpWindowImpl->mpFrame->GetGeometry();
    p.AdjustX(g.x());
    p.AdjustY(g.y());
    return AbsoluteScreenPixelPoint(p);
}

Point Window::AbsoluteScreenToOutputPixel(const AbsoluteScreenPixelPoint& rPos) const
{
    // relative to the screen
    Point p = ScreenToOutputPixel(Point(rPos));
    SalFrameGeometry g = mpWindowImpl->mpFrame->GetGeometry();
    p.AdjustX(-(g.x()));
    p.AdjustY(-(g.y()));
    return p;
}

AbsoluteScreenPixelRectangle Window::GetDesktopRectPixel() const
{
    AbsoluteScreenPixelRectangle rRect;
    mpWindowImpl->mpFrameWindow->mpWindowImpl->mpFrame->GetWorkArea(rRect);
    return rRect;
}

// with decoration
tools::Rectangle Window::GetWindowExtentsRelative(const vcl::Window& rRelativeWindow) const
{
    AbsoluteScreenPixelRectangle aRect = GetWindowExtentsAbsolute();
    // #106399# express coordinates relative to borderwindow
    const vcl::Window* pRelWin = rRelativeWindow.mpWindowImpl->mpBorderWindow
                                     ? rRelativeWindow.mpWindowImpl->mpBorderWindow.get()
                                     : &rRelativeWindow;
    return tools::Rectangle(pRelWin->AbsoluteScreenToOutputPixel(aRect.GetPos()), aRect.GetSize());
}

// with decoration
AbsoluteScreenPixelRectangle Window::GetWindowExtentsAbsolute() const
{
    // make sure we use the extent of our border window,
    // otherwise we miss a few pixels
    const vcl::Window* pWin = mpWindowImpl->mpBorderWindow ? mpWindowImpl->mpBorderWindow : this;

    AbsoluteScreenPixelPoint aPos(pWin->OutputToAbsoluteScreenPixel(Point(0, 0)));
    Size aSize(pWin->GetSizePixel());
    // #104088# do not add decoration to the workwindow to be compatible to java accessibility api
    if (mpWindowImpl->mbFrame
        || (mpWindowImpl->mpBorderWindow && mpWindowImpl->mpBorderWindow->mpWindowImpl->mbFrame
            && GetType() != WindowType::WORKWINDOW))
    {
        SalFrameGeometry g = mpWindowImpl->mpFrame->GetGeometry();
        aPos.AdjustX(-sal_Int32(g.leftDecoration()));
        aPos.AdjustY(-sal_Int32(g.topDecoration()));
        aSize.AdjustWidth(g.leftDecoration() + g.rightDecoration());
        aSize.AdjustHeight(g.topDecoration() + g.bottomDecoration());
    }
    return AbsoluteScreenPixelRectangle(aPos, aSize);
}

Size Window::GetOutputSizePixel() const
{
    if (!mpWindowImpl)
        return Size();

    return GetOutDev()->GetOutputSizePixel();
}

tools::Rectangle Window::GetOutputRectPixel() const { return GetOutDev()->GetOutputRectPixel(); }

Size Window::CalcWindowSize(const Size& rOutSz) const
{
    Size aSz = rOutSz;
    aSz.AdjustWidth(mpWindowImpl->mnLeftBorder + mpWindowImpl->mnRightBorder);
    aSz.AdjustHeight(mpWindowImpl->mnTopBorder + mpWindowImpl->mnBottomBorder);
    return aSz;
}

tools::Long Window::CalcTitleWidth() const
{
    if (mpWindowImpl->mpBorderWindow)
    {
        if (mpWindowImpl->mpBorderWindow->GetType() == WindowType::BORDERWINDOW)
            return static_cast<ImplBorderWindow*>(mpWindowImpl->mpBorderWindow.get())
                ->CalcTitleWidth();
        else
            return mpWindowImpl->mpBorderWindow->CalcTitleWidth();
    }
    else if (mpWindowImpl->mbFrame && (mpWindowImpl->mnStyle & WB_MOVEABLE))
    {
        // we guess the width for frame windows as we do not know the
        // border of external dialogs
        const StyleSettings& rStyleSettings = GetSettings().GetStyleSettings();
        vcl::Font aFont = GetFont();
        const_cast<vcl::Window*>(this)->SetPointFont(const_cast<::OutputDevice&>(*GetOutDev()),
                                                     rStyleSettings.GetTitleFont());
        tools::Long nTitleWidth = GetTextWidth(GetText());
        const_cast<vcl::Window*>(this)->SetFont(aFont);
        nTitleWidth += rStyleSettings.GetTitleHeight() * 3;
        nTitleWidth += StyleSettings::GetBorderSize() * 2;
        nTitleWidth += 10;
        return nTitleWidth;
    }

    return 0;
}

// When a widget wants to renegotiate layout, get toplevel parent dialog and call
// resize on it. Mark all intermediate containers (or container-alike) widgets
// as dirty for the size remains unchanged, but layout changed circumstances
static bool queue_ungrouped_resize(vcl::Window const* pOrigWindow)
{
    bool bSomeoneCares = false;

    vcl::Window* pWindow = pOrigWindow->GetParent();
    if (pWindow)
    {
        if (isContainerWindow(*pWindow))
        {
            bSomeoneCares = true;
        }
        else if (pWindow->GetType() == WindowType::TABCONTROL)
        {
            bSomeoneCares = true;
        }
        pWindow->queue_resize();
    }

    return bSomeoneCares;
}

static bool HasParentDockingWindow(const vcl::Window* pWindow)
{
    while (pWindow)
    {
        if (pWindow->IsDockingWindow())
            return true;

        pWindow = pWindow->GetParent();
    }

    return false;
}

void Window::queue_resize(StateChangedType eReason)
{
    if (isDisposed())
        return;

    bool bSomeoneCares = queue_ungrouped_resize(this);

    if (eReason != StateChangedType::Visible)
    {
        InvalidateSizeCache();
    }

    WindowImpl* pWindowImpl = mpWindowImpl->mpBorderWindow
                                  ? mpWindowImpl->mpBorderWindow->mpWindowImpl.get()
                                  : mpWindowImpl.get();
    if (pWindowImpl->m_xSizeGroup
        && pWindowImpl->m_xSizeGroup->get_mode() != VclSizeGroupMode::NONE)
    {
        std::set<VclPtr<vcl::Window>>& rWindows = pWindowImpl->m_xSizeGroup->get_widgets();
        for (VclPtr<vcl::Window> const& pOther : rWindows)
        {
            if (pOther == this)
                continue;
            queue_ungrouped_resize(pOther);
        }
    }

    if (bSomeoneCares && !isDisposed())
    {
        //fdo#57090 force a resync of the borders of the borderwindow onto this
        //window in case they have changed
        vcl::Window* pBorderWindow = ImplGetBorderWindow();
        if (pBorderWindow)
            pBorderWindow->Resize();
    }
    if (VclPtr<vcl::Window> pParent = GetParentWithLOKNotifier())
    {
        Size aSize = GetSizePixel();
        if (!aSize.IsEmpty() && !pParent->IsInInitShow()
            && (GetParentDialog() || HasParentDockingWindow(this)))
            LogicInvalidate(nullptr);
    }
}

void Window::set_height_request(sal_Int32 nHeightRequest)
{
    if (!mpWindowImpl)
        return;

    WindowImpl* pWindowImpl = mpWindowImpl->mpBorderWindow
                                  ? mpWindowImpl->mpBorderWindow->mpWindowImpl.get()
                                  : mpWindowImpl.get();

    if (pWindowImpl->mnHeightRequest != nHeightRequest)
    {
        pWindowImpl->mnHeightRequest = nHeightRequest;
        queue_resize();
    }
}

void Window::set_width_request(sal_Int32 nWidthRequest)
{
    if (!mpWindowImpl)
        return;

    WindowImpl* pWindowImpl = mpWindowImpl->mpBorderWindow
                                  ? mpWindowImpl->mpBorderWindow->mpWindowImpl.get()
                                  : mpWindowImpl.get();

    if (pWindowImpl->mnWidthRequest != nWidthRequest)
    {
        pWindowImpl->mnWidthRequest = nWidthRequest;
        queue_resize();
    }
}

Size Window::get_ungrouped_preferred_size() const
{
    Size aRet(get_width_request(), get_height_request());
    if (aRet.Width() == -1 || aRet.Height() == -1)
    {
        //cache gets blown away by queue_resize
        WindowImpl* pWindowImpl = mpWindowImpl->mpBorderWindow
                                      ? mpWindowImpl->mpBorderWindow->mpWindowImpl.get()
                                      : mpWindowImpl.get();
        if (pWindowImpl->mnOptimalWidthCache == -1 || pWindowImpl->mnOptimalHeightCache == -1)
        {
            Size aOptimal(GetOptimalSize());
            pWindowImpl->mnOptimalWidthCache = aOptimal.Width();
            pWindowImpl->mnOptimalHeightCache = aOptimal.Height();
        }

        if (aRet.Width() == -1)
            aRet.setWidth(pWindowImpl->mnOptimalWidthCache);
        if (aRet.Height() == -1)
            aRet.setHeight(pWindowImpl->mnOptimalHeightCache);
    }
    return aRet;
}

Size Window::get_preferred_size() const
{
    Size aRet(get_ungrouped_preferred_size());

    WindowImpl* pWindowImpl = mpWindowImpl->mpBorderWindow
                                  ? mpWindowImpl->mpBorderWindow->mpWindowImpl.get()
                                  : mpWindowImpl.get();
    if (pWindowImpl->m_xSizeGroup)
    {
        const VclSizeGroupMode eMode = pWindowImpl->m_xSizeGroup->get_mode();
        if (eMode != VclSizeGroupMode::NONE)
        {
            const bool bIgnoreInHidden = pWindowImpl->m_xSizeGroup->get_ignore_hidden();
            const std::set<VclPtr<vcl::Window>>& rWindows
                = pWindowImpl->m_xSizeGroup->get_widgets();
            for (const vcl::Window* pOther : rWindows)
            {
                if (pOther == this)
                    continue;
                if (bIgnoreInHidden && !pOther->IsVisible())
                    continue;
                Size aOtherSize = pOther->get_ungrouped_preferred_size();
                if (eMode == VclSizeGroupMode::Both || eMode == VclSizeGroupMode::Horizontal)
                    aRet.setWidth(std::max(aRet.Width(), aOtherSize.Width()));
                if (eMode == VclSizeGroupMode::Both || eMode == VclSizeGroupMode::Vertical)
                    aRet.setHeight(std::max(aRet.Height(), aOtherSize.Height()));
            }
        }
    }

    return aRet;
}

VclAlign Window::get_halign() const
{
    WindowImpl* pWindowImpl = mpWindowImpl->mpBorderWindow
                                  ? mpWindowImpl->mpBorderWindow->mpWindowImpl.get()
                                  : mpWindowImpl.get();
    return pWindowImpl->meHalign;
}

void Window::set_halign(VclAlign eAlign)
{
    WindowImpl* pWindowImpl = mpWindowImpl->mpBorderWindow
                                  ? mpWindowImpl->mpBorderWindow->mpWindowImpl.get()
                                  : mpWindowImpl.get();
    pWindowImpl->meHalign = eAlign;
}

VclAlign Window::get_valign() const
{
    WindowImpl* pWindowImpl = mpWindowImpl->mpBorderWindow
                                  ? mpWindowImpl->mpBorderWindow->mpWindowImpl.get()
                                  : mpWindowImpl.get();
    return pWindowImpl->meValign;
}

void Window::set_valign(VclAlign eAlign)
{
    WindowImpl* pWindowImpl = mpWindowImpl->mpBorderWindow
                                  ? mpWindowImpl->mpBorderWindow->mpWindowImpl.get()
                                  : mpWindowImpl.get();
    pWindowImpl->meValign = eAlign;
}

bool Window::get_hexpand() const
{
    WindowImpl* pWindowImpl = mpWindowImpl->mpBorderWindow
                                  ? mpWindowImpl->mpBorderWindow->mpWindowImpl.get()
                                  : mpWindowImpl.get();
    return pWindowImpl->mbHexpand;
}

void Window::set_hexpand(bool bExpand)
{
    WindowImpl* pWindowImpl = mpWindowImpl->mpBorderWindow
                                  ? mpWindowImpl->mpBorderWindow->mpWindowImpl.get()
                                  : mpWindowImpl.get();
    pWindowImpl->mbHexpand = bExpand;
}

bool Window::get_vexpand() const
{
    WindowImpl* pWindowImpl = mpWindowImpl->mpBorderWindow
                                  ? mpWindowImpl->mpBorderWindow->mpWindowImpl.get()
                                  : mpWindowImpl.get();
    return pWindowImpl->mbVexpand;
}

void Window::set_vexpand(bool bExpand)
{
    WindowImpl* pWindowImpl = mpWindowImpl->mpBorderWindow
                                  ? mpWindowImpl->mpBorderWindow->mpWindowImpl.get()
                                  : mpWindowImpl.get();
    pWindowImpl->mbVexpand = bExpand;
}

bool Window::get_expand() const
{
    WindowImpl* pWindowImpl = mpWindowImpl->mpBorderWindow
                                  ? mpWindowImpl->mpBorderWindow->mpWindowImpl.get()
                                  : mpWindowImpl.get();
    return pWindowImpl->mbExpand;
}

void Window::set_expand(bool bExpand)
{
    WindowImpl* pWindowImpl = mpWindowImpl->mpBorderWindow
                                  ? mpWindowImpl->mpBorderWindow->mpWindowImpl.get()
                                  : mpWindowImpl.get();
    pWindowImpl->mbExpand = bExpand;
}

VclPackType Window::get_pack_type() const
{
    WindowImpl* pWindowImpl = mpWindowImpl->mpBorderWindow
                                  ? mpWindowImpl->mpBorderWindow->mpWindowImpl.get()
                                  : mpWindowImpl.get();
    return pWindowImpl->mePackType;
}

void Window::set_pack_type(VclPackType ePackType)
{
    WindowImpl* pWindowImpl = mpWindowImpl->mpBorderWindow
                                  ? mpWindowImpl->mpBorderWindow->mpWindowImpl.get()
                                  : mpWindowImpl.get();
    pWindowImpl->mePackType = ePackType;
}

sal_Int32 Window::get_padding() const
{
    WindowImpl* pWindowImpl = mpWindowImpl->mpBorderWindow
                                  ? mpWindowImpl->mpBorderWindow->mpWindowImpl.get()
                                  : mpWindowImpl.get();
    return pWindowImpl->mnPadding;
}

void Window::set_padding(sal_Int32 nPadding)
{
    WindowImpl* pWindowImpl = mpWindowImpl->mpBorderWindow
                                  ? mpWindowImpl->mpBorderWindow->mpWindowImpl.get()
                                  : mpWindowImpl.get();
    pWindowImpl->mnPadding = nPadding;
}

bool Window::get_fill() const
{
    WindowImpl* pWindowImpl = mpWindowImpl->mpBorderWindow
                                  ? mpWindowImpl->mpBorderWindow->mpWindowImpl.get()
                                  : mpWindowImpl.get();
    return pWindowImpl->mbFill;
}

void Window::set_fill(bool bFill)
{
    WindowImpl* pWindowImpl = mpWindowImpl->mpBorderWindow
                                  ? mpWindowImpl->mpBorderWindow->mpWindowImpl.get()
                                  : mpWindowImpl.get();
    pWindowImpl->mbFill = bFill;
}

sal_Int32 Window::get_grid_width() const
{
    WindowImpl* pWindowImpl = mpWindowImpl->mpBorderWindow
                                  ? mpWindowImpl->mpBorderWindow->mpWindowImpl.get()
                                  : mpWindowImpl.get();
    return pWindowImpl->mnGridWidth;
}

void Window::set_grid_width(sal_Int32 nCols)
{
    WindowImpl* pWindowImpl = mpWindowImpl->mpBorderWindow
                                  ? mpWindowImpl->mpBorderWindow->mpWindowImpl.get()
                                  : mpWindowImpl.get();
    pWindowImpl->mnGridWidth = nCols;
}

sal_Int32 Window::get_grid_left_attach() const
{
    WindowImpl* pWindowImpl = mpWindowImpl->mpBorderWindow
                                  ? mpWindowImpl->mpBorderWindow->mpWindowImpl.get()
                                  : mpWindowImpl.get();
    return pWindowImpl->mnGridLeftAttach;
}

void Window::set_grid_left_attach(sal_Int32 nAttach)
{
    WindowImpl* pWindowImpl = mpWindowImpl->mpBorderWindow
                                  ? mpWindowImpl->mpBorderWindow->mpWindowImpl.get()
                                  : mpWindowImpl.get();
    pWindowImpl->mnGridLeftAttach = nAttach;
}

sal_Int32 Window::get_grid_height() const
{
    WindowImpl* pWindowImpl = mpWindowImpl->mpBorderWindow
                                  ? mpWindowImpl->mpBorderWindow->mpWindowImpl.get()
                                  : mpWindowImpl.get();
    return pWindowImpl->mnGridHeight;
}

void Window::set_grid_height(sal_Int32 nRows)
{
    WindowImpl* pWindowImpl = mpWindowImpl->mpBorderWindow
                                  ? mpWindowImpl->mpBorderWindow->mpWindowImpl.get()
                                  : mpWindowImpl.get();
    pWindowImpl->mnGridHeight = nRows;
}

sal_Int32 Window::get_grid_top_attach() const
{
    WindowImpl* pWindowImpl = mpWindowImpl->mpBorderWindow
                                  ? mpWindowImpl->mpBorderWindow->mpWindowImpl.get()
                                  : mpWindowImpl.get();
    return pWindowImpl->mnGridTopAttach;
}

void Window::set_grid_top_attach(sal_Int32 nAttach)
{
    WindowImpl* pWindowImpl = mpWindowImpl->mpBorderWindow
                                  ? mpWindowImpl->mpBorderWindow->mpWindowImpl.get()
                                  : mpWindowImpl.get();
    pWindowImpl->mnGridTopAttach = nAttach;
}

void Window::set_border_width(sal_Int32 nBorderWidth)
{
    WindowImpl* pWindowImpl = mpWindowImpl->mpBorderWindow
                                  ? mpWindowImpl->mpBorderWindow->mpWindowImpl.get()
                                  : mpWindowImpl.get();
    pWindowImpl->mnBorderWidth = nBorderWidth;
}

sal_Int32 Window::get_border_width() const
{
    WindowImpl* pWindowImpl = mpWindowImpl->mpBorderWindow
                                  ? mpWindowImpl->mpBorderWindow->mpWindowImpl.get()
                                  : mpWindowImpl.get();
    return pWindowImpl->mnBorderWidth;
}

void Window::set_margin_start(sal_Int32 nWidth)
{
    WindowImpl* pWindowImpl = mpWindowImpl->mpBorderWindow
                                  ? mpWindowImpl->mpBorderWindow->mpWindowImpl.get()
                                  : mpWindowImpl.get();
    if (pWindowImpl->mnMarginLeft != nWidth)
    {
        pWindowImpl->mnMarginLeft = nWidth;
        queue_resize();
    }
}

sal_Int32 Window::get_margin_start() const
{
    WindowImpl* pWindowImpl = mpWindowImpl->mpBorderWindow
                                  ? mpWindowImpl->mpBorderWindow->mpWindowImpl.get()
                                  : mpWindowImpl.get();
    return pWindowImpl->mnMarginLeft;
}

void Window::set_margin_end(sal_Int32 nWidth)
{
    WindowImpl* pWindowImpl = mpWindowImpl->mpBorderWindow
                                  ? mpWindowImpl->mpBorderWindow->mpWindowImpl.get()
                                  : mpWindowImpl.get();
    if (pWindowImpl->mnMarginRight != nWidth)
    {
        pWindowImpl->mnMarginRight = nWidth;
        queue_resize();
    }
}

sal_Int32 Window::get_margin_end() const
{
    WindowImpl* pWindowImpl = mpWindowImpl->mpBorderWindow
                                  ? mpWindowImpl->mpBorderWindow->mpWindowImpl.get()
                                  : mpWindowImpl.get();
    return pWindowImpl->mnMarginRight;
}

void Window::set_margin_top(sal_Int32 nWidth)
{
    WindowImpl* pWindowImpl = mpWindowImpl->mpBorderWindow
                                  ? mpWindowImpl->mpBorderWindow->mpWindowImpl.get()
                                  : mpWindowImpl.get();
    if (pWindowImpl->mnMarginTop != nWidth)
    {
        pWindowImpl->mnMarginTop = nWidth;
        queue_resize();
    }
}

sal_Int32 Window::get_margin_top() const
{
    WindowImpl* pWindowImpl = mpWindowImpl->mpBorderWindow
                                  ? mpWindowImpl->mpBorderWindow->mpWindowImpl.get()
                                  : mpWindowImpl.get();
    return pWindowImpl->mnMarginTop;
}

void Window::set_margin_bottom(sal_Int32 nWidth)
{
    WindowImpl* pWindowImpl = mpWindowImpl->mpBorderWindow
                                  ? mpWindowImpl->mpBorderWindow->mpWindowImpl.get()
                                  : mpWindowImpl.get();
    if (pWindowImpl->mnMarginBottom != nWidth)
    {
        pWindowImpl->mnMarginBottom = nWidth;
        queue_resize();
    }
}

sal_Int32 Window::get_margin_bottom() const
{
    WindowImpl* pWindowImpl = mpWindowImpl->mpBorderWindow
                                  ? mpWindowImpl->mpBorderWindow->mpWindowImpl.get()
                                  : mpWindowImpl.get();
    return pWindowImpl->mnMarginBottom;
}

sal_Int32 Window::get_height_request() const
{
    WindowImpl* pWindowImpl = mpWindowImpl->mpBorderWindow
                                  ? mpWindowImpl->mpBorderWindow->mpWindowImpl.get()
                                  : mpWindowImpl.get();
    return pWindowImpl->mnHeightRequest;
}

sal_Int32 Window::get_width_request() const
{
    WindowImpl* pWindowImpl = mpWindowImpl->mpBorderWindow
                                  ? mpWindowImpl->mpBorderWindow->mpWindowImpl.get()
                                  : mpWindowImpl.get();
    return pWindowImpl->mnWidthRequest;
}

bool Window::get_secondary() const
{
    WindowImpl* pWindowImpl = mpWindowImpl->mpBorderWindow
                                  ? mpWindowImpl->mpBorderWindow->mpWindowImpl.get()
                                  : mpWindowImpl.get();
    return pWindowImpl->mbSecondary;
}

void Window::set_secondary(bool bSecondary)
{
    WindowImpl* pWindowImpl = mpWindowImpl->mpBorderWindow
                                  ? mpWindowImpl->mpBorderWindow->mpWindowImpl.get()
                                  : mpWindowImpl.get();
    pWindowImpl->mbSecondary = bSecondary;
}

bool Window::get_non_homogeneous() const
{
    WindowImpl* pWindowImpl = mpWindowImpl->mpBorderWindow
                                  ? mpWindowImpl->mpBorderWindow->mpWindowImpl.get()
                                  : mpWindowImpl.get();
    return pWindowImpl->mbNonHomogeneous;
}

void Window::set_non_homogeneous(bool bNonHomogeneous)
{
    WindowImpl* pWindowImpl = mpWindowImpl->mpBorderWindow
                                  ? mpWindowImpl->mpBorderWindow->mpWindowImpl.get()
                                  : mpWindowImpl.get();
    pWindowImpl->mbNonHomogeneous = bNonHomogeneous;
}

void Window::add_to_size_group(const std::shared_ptr<VclSizeGroup>& xGroup)
{
    WindowImpl* pWindowImpl = mpWindowImpl->mpBorderWindow
                                  ? mpWindowImpl->mpBorderWindow->mpWindowImpl.get()
                                  : mpWindowImpl.get();
    //To-Do, multiple groups
    pWindowImpl->m_xSizeGroup = xGroup;
    pWindowImpl->m_xSizeGroup->insert(this);
    if (VclSizeGroupMode::NONE != pWindowImpl->m_xSizeGroup->get_mode())
        queue_resize();
}

void Window::remove_from_all_size_groups()
{
    WindowImpl* pWindowImpl = mpWindowImpl->mpBorderWindow
                                  ? mpWindowImpl->mpBorderWindow->mpWindowImpl.get()
                                  : mpWindowImpl.get();
    //To-Do, multiple groups
    if (pWindowImpl->m_xSizeGroup)
    {
        if (VclSizeGroupMode::NONE != pWindowImpl->m_xSizeGroup->get_mode())
            queue_resize();
        pWindowImpl->m_xSizeGroup->erase(this);
        pWindowImpl->m_xSizeGroup.reset();
    }
}

tools::Long Window::LogicWidthToDevicePixel(tools::Long nWidth) const
{
    return GetOutDev()->LogicWidthToDevicePixel(nWidth);
}

Size Window::GetOptimalSize() const { return Size(); }

void Window::ImplAdjustNWFSizes()
{
    for (Window* pWin = GetWindow(GetWindowType::FirstChild); pWin;
         pWin = pWin->GetWindow(GetWindowType::Next))
        pWin->ImplAdjustNWFSizes();
}

void Window::ImplPosSizeWindow(tools::Long nX, tools::Long nY, tools::Long nWidth,
                               tools::Long nHeight, PosSizeFlags nFlags)
{
    bool bNewPos = false;
    bool bNewSize = false;
    bool bCopyBits = false;
    tools::Long nOldOutOffX = GetOutDev()->GetDeviceOriginX();
    tools::Long nOldOutOffY = GetOutDev()->GetDeviceOriginY();
    tools::Long nOldOutWidth = GetOutDev()->GetOutputWidthPixel();
    tools::Long nOldOutHeight = GetOutDev()->GetOutputHeightPixel();
    std::unique_ptr<vcl::Region> pOverlapRegion;
    std::unique_ptr<vcl::Region> pOldRegion;

    if (IsReallyVisible())
    {
        tools::Rectangle aOldWinRect(Point(nOldOutOffX, nOldOutOffY),
                                     Size(nOldOutWidth, nOldOutHeight));
        pOldRegion.reset(new vcl::Region(aOldWinRect));
        if (mpWindowImpl->mpClippingState->mbWinRegion)
            pOldRegion->Intersect(
                GetOutDev()->GetMapper().ViewToDevice(mpWindowImpl->mpClippingState->maWinRegion));

        if (GetOutDev()->GetOutputWidthPixel() && GetOutDev()->GetOutputHeightPixel()
            && !mpWindowImpl->mbPaintTransparent
            && !mpWindowImpl->mpClippingState->mbInitWinClipRegion
            && !mpWindowImpl->mpClippingState->maWinClipRegion.IsEmpty() && !HasPaintEvent())
            bCopyBits = true;
    }

    bool bnXRecycled = false; // avoid duplicate mirroring in RTL case
    if (nFlags & PosSizeFlags::Width)
    {
        if (!(nFlags & PosSizeFlags::X))
        {
            nX = mpWindowImpl->mnX;
            nFlags |= PosSizeFlags::X;
            bnXRecycled = true; // we're using a mnX which was already mirrored in RTL case
        }

        if (nWidth < 0)
            nWidth = 0;
        if (nWidth != GetOutDev()->GetOutputWidthPixel())
        {
            GetOutDev()->SetOutputWidthPixel(nWidth);
            bNewSize = true;
            bCopyBits = false;
        }
    }
    if (nFlags & PosSizeFlags::Height)
    {
        if (nHeight < 0)
            nHeight = 0;
        if (nHeight != GetOutDev()->GetOutputHeightPixel())
        {
            GetOutDev()->SetOutputHeightPixel(nHeight);
            bNewSize = true;
            bCopyBits = false;
        }
    }

    if (nFlags & PosSizeFlags::X)
    {
        tools::Long nOrgX = nX;
        Point aPtDev(nX + GetOutDev()->GetDeviceOriginX(), 0);
        OutputDevice* pOutDev = GetOutDev();
        if (pOutDev->HasMirroredGraphics())
        {
            aPtDev.setX(GetOutDev()->mpGraphics->mirror2(aPtDev.X(), *GetOutDev()));

            // #106948# always mirror our pos if our parent is not mirroring, even
            // if we are also not mirroring
            // RTL: check if parent is in different coordinates
            if (!bnXRecycled && mpWindowImpl->mpHierarchy->mpParent
                && !mpWindowImpl->mpHierarchy->mpParent->mpWindowImpl->mbFrame
                && mpWindowImpl->mpHierarchy->mpParent->GetOutDev()->ImplIsAntiparallel())
            {
                nX = mpWindowImpl->mpHierarchy->mpParent->GetOutDev()->GetOutputWidthPixel()
                     - GetOutDev()->GetOutputWidthPixel() - nX;
            }
            /* #i99166# An LTR window in RTL UI that gets sized only would be
               expected to not moved its upper left point
            */
            if (bnXRecycled)
            {
                if (GetOutDev()->ImplIsAntiparallel())
                {
                    aPtDev.setX(mpWindowImpl->mnAbsScreenX);
                    nOrgX = mpWindowImpl->maPos.X();
                }
            }
        }
        else if (!bnXRecycled && mpWindowImpl->mpHierarchy->mpParent
                 && !mpWindowImpl->mpHierarchy->mpParent->mpWindowImpl->mbFrame
                 && mpWindowImpl->mpHierarchy->mpParent->GetOutDev()->ImplIsAntiparallel())
        {
            // mirrored window in LTR UI
            nX = mpWindowImpl->mpHierarchy->mpParent->GetOutDev()->GetOutputWidthPixel()
                 - GetOutDev()->GetOutputWidthPixel() - nX;
        }

        // check maPos as well, as it could have been changed for client windows (ImplCallMove())
        if (mpWindowImpl->mnAbsScreenX != aPtDev.X() || nX != mpWindowImpl->mnX
            || nOrgX != mpWindowImpl->maPos.X())
        {
            if (bCopyBits && !pOverlapRegion)
            {
                pOverlapRegion.reset(new vcl::Region());
                vcl::clipping::calcOverlapRegion(*this, GetOutputRectPixel(), *pOverlapRegion,
                                                 false, true);
            }

            mpWindowImpl->mnX = nX;
            mpWindowImpl->maPos.setX(nOrgX);
            mpWindowImpl->mnAbsScreenX = aPtDev.X();
            bNewPos = true;
        }
    }
    if (nFlags & PosSizeFlags::Y)
    {
        // check maPos as well, as it could have been changed for client windows (ImplCallMove())
        if (nY != mpWindowImpl->mnY || nY != mpWindowImpl->maPos.Y())
        {
            if (bCopyBits && !pOverlapRegion)
            {
                pOverlapRegion.reset(new vcl::Region());
                vcl::clipping::calcOverlapRegion(*this, GetOutputRectPixel(), *pOverlapRegion,
                                                 false, true);
            }
            mpWindowImpl->mnY = nY;
            mpWindowImpl->maPos.setY(nY);
            bNewPos = true;
        }
    }

    if (!(bNewPos || bNewSize))
        return;

    bool bUpdateSysObjPos = false;
    if (bNewPos)
        bUpdateSysObjPos = ImplUpdatePos();

    // the borderwindow always specifies the position for its client window
    if (mpWindowImpl->mpBorderWindow)
        mpWindowImpl->maPos = mpWindowImpl->mpBorderWindow->mpWindowImpl->maPos;

    if (mpWindowImpl->mpClientWindow)
    {
        mpWindowImpl->mpClientWindow->ImplPosSizeWindow(
            mpWindowImpl->mpClientWindow->mpWindowImpl->mnLeftBorder,
            mpWindowImpl->mpClientWindow->mpWindowImpl->mnTopBorder,
            GetOutDev()->GetOutputWidthPixel()
                - mpWindowImpl->mpClientWindow->mpWindowImpl->mnLeftBorder
                - mpWindowImpl->mpClientWindow->mpWindowImpl->mnRightBorder,
            GetOutDev()->GetOutputHeightPixel()
                - mpWindowImpl->mpClientWindow->mpWindowImpl->mnTopBorder
                - mpWindowImpl->mpClientWindow->mpWindowImpl->mnBottomBorder,
            PosSizeFlags::X | PosSizeFlags::Y | PosSizeFlags::Width | PosSizeFlags::Height);
        // If we have a client window, then this is the position
        // of the Application's floating windows
        mpWindowImpl->mpClientWindow->mpWindowImpl->maPos = mpWindowImpl->maPos;
        if (bNewPos)
        {
            if (mpWindowImpl->mpClientWindow->IsVisible())
            {
                mpWindowImpl->mpClientWindow->ImplCallMove();
            }
            else
            {
                mpWindowImpl->mpClientWindow->mpWindowImpl->mbCallMove = true;
            }
        }
    }

    // Move()/Resize() will be called only for Show(), such that
    // at least one is called before Show()
    if (IsVisible())
    {
        if (bNewPos)
        {
            ImplCallMove();
        }
        if (bNewSize)
        {
            ImplCallResize();
        }
    }
    else
    {
        if (bNewPos)
            mpWindowImpl->mbCallMove = true;
        if (bNewSize)
            mpWindowImpl->mbCallResize = true;
    }

    bool bUpdateSysObjClip = false;
    if (IsReallyVisible())
    {
        if (bNewPos || bNewSize)
        {
            // set Clip-Flag
            bUpdateSysObjClip = !vcl::clipping::setClipFlag(*this, true);
        }

        // invalidate window content ?
        if (bNewPos || (GetOutDev()->GetOutputWidthPixel() > nOldOutWidth)
            || (GetOutDev()->GetOutputHeightPixel() > nOldOutHeight))
        {
            if (bNewPos)
            {
                bool bInvalidate = false;
                bool bParentPaint = true;
                if (!ImplIsOverlapWindow())
                    bParentPaint = mpWindowImpl->mpHierarchy->mpParent->IsPaintEnabled();
                if (bCopyBits && bParentPaint && !HasPaintEvent())
                {
                    vcl::Region aRegion(GetOutputRectPixel());
                    if (mpWindowImpl->mpClippingState->mbWinRegion)
                        aRegion.Intersect(GetOutDev()->GetMapper().ViewToDevice(
                            mpWindowImpl->mpClippingState->maWinRegion));
                    vcl::clipping::clipBoundaries(*this, aRegion, false, true);
                    if (!pOverlapRegion->IsEmpty())
                    {
                        pOverlapRegion->Move(GetOutDev()->GetDeviceOriginX() - nOldOutOffX,
                                             GetOutDev()->GetDeviceOriginY() - nOldOutOffY);
                        aRegion.Exclude(*pOverlapRegion);
                    }
                    if (!aRegion.IsEmpty())
                    {
                        // adapt Paint areas
                        ImplMoveAllInvalidateRegions(
                            tools::Rectangle(Point(nOldOutOffX, nOldOutOffY),
                                             Size(nOldOutWidth, nOldOutHeight)),
                            GetOutDev()->GetDeviceOriginX() - nOldOutOffX,
                            GetOutDev()->GetDeviceOriginY() - nOldOutOffY, true);
                        SalGraphics* pGraphics = ImplGetFrameGraphics();
                        if (pGraphics)
                        {
                            OutputDevice* pOutDev = GetOutDev();
                            const bool bSelectClipRegion
                                = pOutDev->SelectClipRegion(aRegion, pGraphics);
                            if (bSelectClipRegion)
                            {
                                pGraphics->CopyArea(GetOutDev()->GetDeviceOriginX(),
                                                    GetOutDev()->GetDeviceOriginY(), nOldOutOffX,
                                                    nOldOutOffY, nOldOutWidth, nOldOutHeight,
                                                    *GetOutDev());
                            }
                            else
                                bInvalidate = true;
                        }
                        else
                            bInvalidate = true;
                        if (!bInvalidate)
                        {
                            if (!pOverlapRegion->IsEmpty())
                                ImplInvalidateFrameRegion(pOverlapRegion.get(),
                                                          InvalidateFlags::Children);
                        }
                    }
                    else
                        bInvalidate = true;
                }
                else
                    bInvalidate = true;
                if (bInvalidate)
                    ImplInvalidateFrameRegion(nullptr, InvalidateFlags::Children);
            }
            else
            {
                vcl::Region aRegion(GetOutputRectPixel());
                aRegion.Exclude(*pOldRegion);
                if (mpWindowImpl->mpClippingState->mbWinRegion)
                    aRegion.Intersect(GetOutDev()->GetMapper().ViewToDevice(
                        mpWindowImpl->mpClippingState->maWinRegion));
                vcl::clipping::clipBoundaries(*this, aRegion, false, true);
                if (!aRegion.IsEmpty())
                    ImplInvalidateFrameRegion(&aRegion, InvalidateFlags::Children);
            }
        }

        // invalidate Parent or Overlaps
        if (bNewPos || (GetOutDev()->GetOutputWidthPixel() < nOldOutWidth)
            || (GetOutDev()->GetOutputHeightPixel() < nOldOutHeight))
        {
            vcl::Region aRegion(*pOldRegion);
            if (!mpWindowImpl->mbPaintTransparent)
                vcl::clipping::excludeWindowRegion(*this, aRegion);

            vcl::clipping::clipBoundaries(*this, aRegion, false, true);

            if (!aRegion.IsEmpty() && !mpWindowImpl->mpBorderWindow)
                ImplInvalidateParentFrameRegion(aRegion);
        }
    }

    // adapt system objects
    if (bUpdateSysObjClip)
        vcl::clipping::updateNativeObjectClip(*this);

    if (bUpdateSysObjPos)
        ImplUpdateNativeObjectPos();

    if (bNewSize && mpWindowImpl->mpSysObj)
        mpWindowImpl->mpSysObj->SetPosSize(
            GetOutDev()->GetDeviceOriginX(), GetOutDev()->GetDeviceOriginY(),
            GetOutDev()->GetOutputWidthPixel(), GetOutDev()->GetOutputHeightPixel());
}

bool Window::ImplUpdatePos()
{
    bool bSysChild = false;

    if (ImplIsOverlapWindow())
    {
        GetOutDev()->SetDeviceOriginX(mpWindowImpl->mnX);
        GetOutDev()->SetDeviceOriginY(mpWindowImpl->mnY);
    }
    else
    {
        vcl::Window* pParent = ImplGetParent();

        GetOutDev()->SetDeviceOriginX(mpWindowImpl->mnX + pParent->GetOutDev()->GetDeviceOriginX());
        GetOutDev()->SetDeviceOriginY(mpWindowImpl->mnY + pParent->GetOutDev()->GetDeviceOriginY());
    }

    VclPtr<vcl::Window> pChild = mpWindowImpl->mpHierarchy->mpFirstChild;
    while (pChild)
    {
        if (pChild->ImplUpdatePos())
            bSysChild = true;
        pChild = pChild->mpWindowImpl->mpHierarchy->mpNext;
    }

    if (mpWindowImpl->mpSysObj)
        bSysChild = true;

    return bSysChild;
}

void Window::ImplUpdateNativeObjectPos()
{
    if (mpWindowImpl->mpSysObj)
        mpWindowImpl->mpSysObj->SetPosSize(
            GetOutDev()->GetDeviceOriginX(), GetOutDev()->GetDeviceOriginY(),
            GetOutDev()->GetOutputWidthPixel(), GetOutDev()->GetOutputHeightPixel());

    VclPtr<vcl::Window> pChild = mpWindowImpl->mpHierarchy->mpFirstChild;
    while (pChild)
    {
        pChild->ImplUpdateNativeObjectPos();
        pChild = pChild->mpWindowImpl->mpHierarchy->mpNext;
    }
}

tools::Long Window::ImplGetUnmirroredOutOffX() const
{
    // revert GetDeviceOriginX() changes that were potentially made in ImplPosSizeWindow
    tools::Long offx = GetOutDev()->GetDeviceOriginX();
    const OutputDevice* pOutDev = GetOutDev();
    if (pOutDev->HasMirroredGraphics())
    {
        if (mpWindowImpl->mpHierarchy->mpParent
            && !mpWindowImpl->mpHierarchy->mpParent->mpWindowImpl->mbFrame
            && mpWindowImpl->mpHierarchy->mpParent->GetOutDev()->ImplIsAntiparallel())
        {
            if (!ImplIsOverlapWindow())
                offx -= mpWindowImpl->mpHierarchy->mpParent->GetOutDev()->GetDeviceOriginX();

            offx = mpWindowImpl->mpHierarchy->mpParent->GetOutDev()->GetOutputWidthPixel()
                   - GetOutDev()->GetOutputWidthPixel() - offx;

            if (!ImplIsOverlapWindow())
                offx += mpWindowImpl->mpHierarchy->mpParent->GetOutDev()->GetDeviceOriginX();
        }
    }
    return offx;
}

AbsoluteScreenPixelRectangle
Window::ImplOutputToUnmirroredAbsoluteScreenPixel(const tools::Rectangle& rRect) const
{
    // this method creates unmirrored screen coordinates to be compared with the desktop
    // and is used for positioning of RTL popup windows correctly on the screen
    SalFrameGeometry g = mpWindowImpl->mpFrame->GetUnmirroredGeometry();

    Point p1 = rRect.TopRight();
    p1 = OutputToScreenPixel(p1);
    p1.setX(g.x() + g.width() - p1.X());
    p1.AdjustY(g.y());

    Point p2 = rRect.BottomLeft();
    p2 = OutputToScreenPixel(p2);
    p2.setX(g.x() + g.width() - p2.X());
    p2.AdjustY(g.y());

    return AbsoluteScreenPixelRectangle(AbsoluteScreenPixelPoint(p1), AbsoluteScreenPixelPoint(p2));
}

tools::Rectangle
Window::ImplUnmirroredAbsoluteScreenToOutputPixel(const AbsoluteScreenPixelRectangle& rRect) const
{
    // undo ImplOutputToUnmirroredAbsoluteScreenPixel
    SalFrameGeometry g = mpWindowImpl->mpFrame->GetUnmirroredGeometry();

    Point p1(rRect.TopRight());
    p1.AdjustY(-g.y());
    p1.setX(g.x() + g.width() - p1.X());
    p1 = ScreenToOutputPixel(p1);

    Point p2(rRect.BottomLeft());
    p2.AdjustY(-g.y());
    p2.setX(g.x() + g.width() - p2.X());
    p2 = ScreenToOutputPixel(p2);

    return tools::Rectangle(p1, p2);
}

Size Window::CalcOutputSize(const Size& rWinSz) const
{
    Size aSz = rWinSz;
    aSz.AdjustWidth(-(mpWindowImpl->mnLeftBorder + mpWindowImpl->mnRightBorder));
    aSz.AdjustHeight(-(mpWindowImpl->mnTopBorder + mpWindowImpl->mnBottomBorder));
    return aSz;
}

void Window::InvalidateSizeCache()
{
    WindowImpl* pWindowImpl = mpWindowImpl->mpBorderWindow
                                  ? mpWindowImpl->mpBorderWindow->mpWindowImpl.get()
                                  : mpWindowImpl.get();
    pWindowImpl->mnOptimalWidthCache = -1;
    pWindowImpl->mnOptimalHeightCache = -1;
}

} // end namespace vcl

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
