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

static Size lcl_ClampSizeToWindowLimits(SystemWindow& rSystemWindow, tools::Long nWidth,
                                        tools::Long nHeight)
{
    Size aMinSize = rSystemWindow.GetMinOutputSizePixel();
    Size aMaxSize = rSystemWindow.GetMaxOutputSizePixel();

    if (nWidth < aMinSize.Width())
        nWidth = aMinSize.Width();

    if (nWidth > aMaxSize.Width())
        nWidth = aMaxSize.Width();

    if (nHeight < aMinSize.Height())
        nHeight = aMinSize.Height();

    if (nHeight > aMaxSize.Height())
        nHeight = aMaxSize.Height();

    return Size(nWidth, nHeight);
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

    nX = lcl_CalculatePositionX(this, pBorderWindow.get(), nX, nY, nWidth, nHeight, nFlags);

    const bool bHasValidSize = !mpWindowImpl->mbDefSize;

    if (VclPtr<vcl::Window> pWinParent = pBorderWindow->GetParent(); pWinParent)
    {
        const bool bIsSystemChild = (pBorderWindow->GetStyle() & WB_SYSTEMCHILDWINDOW);

        if (bIsSystemChild)
            nX += pWinParent->GetOutDev()->GetDeviceOriginX();

        // RTL: make sure the old right aligned position is not changed
        // system windows will always grow to the right
        if (pWinParent->GetOutDev()->HasMirroredGraphics()
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

        if (nFlags & PosSizeFlags::Y && bIsSystemChild)
            nY += pWinParent->GetOutDev()->GetDeviceOriginY();
    }

    if (SystemWindow* pSystemWindow = dynamic_cast<SystemWindow*>(pBorderWindow.get());
        pSystemWindow && nSysFlags & (SAL_FRAME_POSSIZE_WIDTH | SAL_FRAME_POSSIZE_HEIGHT))
    {
        // check for min/max client size and adjust size accordingly
        // otherwise it may happen that the resize event is ignored, i.e. the old size remains
        // unchanged but ImplHandleResize() is called with the wrong size
        const Size aClampedSize = lcl_ClampSizeToWindowLimits(*pSystemWindow, nWidth, nHeight);
        nWidth = aClampedSize.Width();
        nHeight = aClampedSize.Height();
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

    return CalcWindowSize(GetOutDev()->GetOutputSizePixel());
}

void Window::SetSizePixel(const Size& rNewSize)
{
    setPosSizePixel(0, 0, rNewSize.Width(), rNewSize.Height(), PosSizeFlags::Size);
}

void Window::SetPosSizePixel(const Point& rNewPos, const Size& rNewSize)
{
    setPosSizePixel(rNewPos.X(), rNewPos.Y(), rNewSize.Width(), rNewSize.Height());
}

void Window::SetOutputSizePixel(const Size& rNewSize) { SetSizePixel(CalcWindowSize(rNewSize)); }

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

Size Window::CalcWindowSize(const Size& rOutputSize) const
{
    return Size(rOutputSize.Width() + ImplGetBorderWidth(),
                rOutputSize.Height() + ImplGetBorderHeight());
}

tools::Long Window::CalcTitleWidth() const
{
    if (mpWindowImpl->mpBorderWindow)
    {
        if (mpWindowImpl->mpBorderWindow->GetType() == WindowType::BORDERWINDOW)
            return static_cast<ImplBorderWindow*>(mpWindowImpl->mpBorderWindow.get())
                ->CalcTitleWidth();

        return mpWindowImpl->mpBorderWindow->CalcTitleWidth();
    }

    if (!mpWindowImpl->mbFrame || !(mpWindowImpl->mnStyle & WB_MOVEABLE))
        return 0;

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

// When a widget wants to renegotiate layout, get toplevel parent dialog and call
// resize on it. Mark all intermediate containers (or container-alike) widgets
// as dirty for the size remains unchanged, but layout changed circumstances
static bool queue_ungrouped_resize(vcl::Window const* pOrigWindow)
{
    bool bSomeoneCares = false;

    if (vcl::Window* pWindow = pOrigWindow->GetParent(); pWindow)
    {
        if (isContainerWindow(*pWindow) || pWindow->GetType() == WindowType::TABCONTROL)
            bSomeoneCares = true;

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

WindowImpl* Window::ImplGetEffectiveWindowImpl() const
{
    return mpWindowImpl->mpBorderWindow ? mpWindowImpl->mpBorderWindow->mpWindowImpl.get()
                                        : mpWindowImpl.get();
}

void Window::ImplQueueResizeOnGroup() const
{
    WindowImpl* pWindowImpl = ImplGetEffectiveWindowImpl();

    if (!pWindowImpl->m_xSizeGroup
        || pWindowImpl->m_xSizeGroup->get_mode() == VclSizeGroupMode::NONE)
        return;

    const std::set<VclPtr<vcl::Window>>& rWindows = pWindowImpl->m_xSizeGroup->get_widgets();

    for (VclPtr<vcl::Window> const& pOther : rWindows)
    {
        if (pOther == this)
            continue;

        queue_ungrouped_resize(pOther);
    }
}

bool Window::ImplRequiresParentLayoutUpdate(const vcl::Window* pParent) const
{
    return !GetSizePixel().IsEmpty() && !pParent->IsInInitShow()
           && (GetParentDialog() || HasParentDockingWindow(this));
}

void Window::queue_resize(StateChangedType eReason)
{
    if (isDisposed())
        return;

    bool bSomeoneCares = queue_ungrouped_resize(this);

    if (eReason != StateChangedType::Visible)
        InvalidateSizeCache();

    ImplQueueResizeOnGroup();

    if (bSomeoneCares && !isDisposed())
    {
        // fdo#57090 force a resync of the borders of the borderwindow onto this
        // window in case they have changed
        if (vcl::Window* pBorderWindow = ImplGetBorderWindow(); pBorderWindow)
            pBorderWindow->Resize();
    }

    if (VclPtr<vcl::Window> pParent = GetParentWithLOKNotifier();
        pParent && ImplRequiresParentLayoutUpdate(pParent))
    {
        LogicInvalidate(nullptr);
    }
}

void Window::set_height_request(sal_Int32 nHeightRequest)
{
    if (!mpWindowImpl)
        return;

    if (WindowImpl* pWindowImpl = ImplGetEffectiveWindowImpl();
        pWindowImpl->mnHeightRequest != nHeightRequest)
    {
        pWindowImpl->mnHeightRequest = nHeightRequest;
        queue_resize();
    }
}

void Window::set_width_request(sal_Int32 nWidthRequest)
{
    if (!mpWindowImpl)
        return;

    if (WindowImpl* pWindowImpl = ImplGetEffectiveWindowImpl();
        pWindowImpl->mnWidthRequest != nWidthRequest)
    {
        pWindowImpl->mnWidthRequest = nWidthRequest;
        queue_resize();
    }
}

Size Window::get_ungrouped_preferred_size() const
{
    Size aPreferredSize(get_width_request(), get_height_request());

    if (aPreferredSize.Width() != -1 && aPreferredSize.Height() != -1)
        return aPreferredSize;

    // cache gets blown away by queue_resize
    WindowImpl* pWindowImpl = ImplGetEffectiveWindowImpl();

    if (pWindowImpl->mnOptimalWidthCache == -1 || pWindowImpl->mnOptimalHeightCache == -1)
    {
        Size aOptimal(GetOptimalSize());
        pWindowImpl->mnOptimalWidthCache = aOptimal.Width();
        pWindowImpl->mnOptimalHeightCache = aOptimal.Height();
    }

    if (aPreferredSize.Width() == -1)
        aPreferredSize.setWidth(pWindowImpl->mnOptimalWidthCache);

    if (aPreferredSize.Height() == -1)
        aPreferredSize.setHeight(pWindowImpl->mnOptimalHeightCache);

    return aPreferredSize;
}

Size Window::get_preferred_size() const
{
    Size aPreferredSize(get_ungrouped_preferred_size());

    WindowImpl* pWindowImpl = ImplGetEffectiveWindowImpl();

    if (!pWindowImpl->m_xSizeGroup)
        return aPreferredSize;

    const VclSizeGroupMode eMode = pWindowImpl->m_xSizeGroup->get_mode();

    if (eMode == VclSizeGroupMode::NONE)
        return aPreferredSize;

    const bool bIgnoreInHidden = pWindowImpl->m_xSizeGroup->get_ignore_hidden();
    const std::set<VclPtr<vcl::Window>>& rWindows = pWindowImpl->m_xSizeGroup->get_widgets();

    for (const vcl::Window* pOther : rWindows)
    {
        if (pOther == this)
            continue;

        if (bIgnoreInHidden && !pOther->IsVisible())
            continue;

        Size aOtherSize = pOther->get_ungrouped_preferred_size();

        if (eMode == VclSizeGroupMode::Both || eMode == VclSizeGroupMode::Horizontal)
            aPreferredSize.setWidth(std::max(aPreferredSize.Width(), aOtherSize.Width()));

        if (eMode == VclSizeGroupMode::Both || eMode == VclSizeGroupMode::Vertical)
            aPreferredSize.setHeight(std::max(aPreferredSize.Height(), aOtherSize.Height()));
    }

    return aPreferredSize;
}

VclAlign Window::get_halign() const
{
    WindowImpl* pWindowImpl = ImplGetEffectiveWindowImpl();

    return pWindowImpl->meHalign;
}

void Window::set_halign(VclAlign eAlign)
{
    WindowImpl* pWindowImpl = ImplGetEffectiveWindowImpl();

    pWindowImpl->meHalign = eAlign;
}

VclAlign Window::get_valign() const
{
    WindowImpl* pWindowImpl = ImplGetEffectiveWindowImpl();

    return pWindowImpl->meValign;
}

void Window::set_valign(VclAlign eAlign)
{
    WindowImpl* pWindowImpl = ImplGetEffectiveWindowImpl();

    pWindowImpl->meValign = eAlign;
}

bool Window::get_hexpand() const
{
    WindowImpl* pWindowImpl = ImplGetEffectiveWindowImpl();

    return pWindowImpl->mbHexpand;
}

void Window::set_hexpand(bool bExpand)
{
    WindowImpl* pWindowImpl = ImplGetEffectiveWindowImpl();

    pWindowImpl->mbHexpand = bExpand;
}

bool Window::get_vexpand() const
{
    WindowImpl* pWindowImpl = ImplGetEffectiveWindowImpl();

    return pWindowImpl->mbVexpand;
}

void Window::set_vexpand(bool bExpand)
{
    WindowImpl* pWindowImpl = ImplGetEffectiveWindowImpl();

    pWindowImpl->mbVexpand = bExpand;
}

bool Window::get_expand() const
{
    WindowImpl* pWindowImpl = ImplGetEffectiveWindowImpl();

    return pWindowImpl->mbExpand;
}

void Window::set_expand(bool bExpand)
{
    WindowImpl* pWindowImpl = ImplGetEffectiveWindowImpl();

    pWindowImpl->mbExpand = bExpand;
}

VclPackType Window::get_pack_type() const
{
    WindowImpl* pWindowImpl = ImplGetEffectiveWindowImpl();

    return pWindowImpl->mePackType;
}

void Window::set_pack_type(VclPackType ePackType)
{
    WindowImpl* pWindowImpl = ImplGetEffectiveWindowImpl();

    pWindowImpl->mePackType = ePackType;
}

sal_Int32 Window::get_padding() const
{
    WindowImpl* pWindowImpl = ImplGetEffectiveWindowImpl();

    return pWindowImpl->mnPadding;
}

void Window::set_padding(sal_Int32 nPadding)
{
    WindowImpl* pWindowImpl = ImplGetEffectiveWindowImpl();

    pWindowImpl->mnPadding = nPadding;
}

bool Window::get_fill() const
{
    WindowImpl* pWindowImpl = ImplGetEffectiveWindowImpl();

    return pWindowImpl->mbFill;
}

void Window::set_fill(bool bFill)
{
    WindowImpl* pWindowImpl = ImplGetEffectiveWindowImpl();

    pWindowImpl->mbFill = bFill;
}

sal_Int32 Window::get_grid_width() const
{
    WindowImpl* pWindowImpl = ImplGetEffectiveWindowImpl();

    return pWindowImpl->mnGridWidth;
}

void Window::set_grid_width(sal_Int32 nCols)
{
    WindowImpl* pWindowImpl = ImplGetEffectiveWindowImpl();

    pWindowImpl->mnGridWidth = nCols;
}

sal_Int32 Window::get_grid_left_attach() const
{
    WindowImpl* pWindowImpl = ImplGetEffectiveWindowImpl();

    return pWindowImpl->mnGridLeftAttach;
}

void Window::set_grid_left_attach(sal_Int32 nAttach)
{
    WindowImpl* pWindowImpl = ImplGetEffectiveWindowImpl();

    pWindowImpl->mnGridLeftAttach = nAttach;
}

sal_Int32 Window::get_grid_height() const
{
    WindowImpl* pWindowImpl = ImplGetEffectiveWindowImpl();

    return pWindowImpl->mnGridHeight;
}

void Window::set_grid_height(sal_Int32 nRows)
{
    WindowImpl* pWindowImpl = ImplGetEffectiveWindowImpl();

    pWindowImpl->mnGridHeight = nRows;
}

sal_Int32 Window::get_grid_top_attach() const
{
    WindowImpl* pWindowImpl = ImplGetEffectiveWindowImpl();

    return pWindowImpl->mnGridTopAttach;
}

void Window::set_grid_top_attach(sal_Int32 nAttach)
{
    WindowImpl* pWindowImpl = ImplGetEffectiveWindowImpl();

    pWindowImpl->mnGridTopAttach = nAttach;
}

void Window::set_border_width(sal_Int32 nBorderWidth)
{
    WindowImpl* pWindowImpl = ImplGetEffectiveWindowImpl();

    pWindowImpl->mnBorderWidth = nBorderWidth;
}

sal_Int32 Window::get_border_width() const
{
    WindowImpl* pWindowImpl = ImplGetEffectiveWindowImpl();

    return pWindowImpl->mnBorderWidth;
}

void Window::set_margin_start(sal_Int32 nWidth)
{
    if (WindowImpl* pWindowImpl = ImplGetEffectiveWindowImpl(); pWindowImpl->mnMarginLeft != nWidth)
    {
        pWindowImpl->mnMarginLeft = nWidth;
        queue_resize();
    }
}

sal_Int32 Window::get_margin_start() const
{
    WindowImpl* pWindowImpl = ImplGetEffectiveWindowImpl();

    return pWindowImpl->mnMarginLeft;
}

void Window::set_margin_end(sal_Int32 nWidth)
{
    if (WindowImpl* pWindowImpl = ImplGetEffectiveWindowImpl();
        pWindowImpl->mnMarginRight != nWidth)
    {
        pWindowImpl->mnMarginRight = nWidth;
        queue_resize();
    }
}

sal_Int32 Window::get_margin_end() const
{
    WindowImpl* pWindowImpl = ImplGetEffectiveWindowImpl();

    return pWindowImpl->mnMarginRight;
}

void Window::set_margin_top(sal_Int32 nWidth)
{
    if (WindowImpl* pWindowImpl = ImplGetEffectiveWindowImpl(); pWindowImpl->mnMarginTop != nWidth)
    {
        pWindowImpl->mnMarginTop = nWidth;
        queue_resize();
    }
}

sal_Int32 Window::get_margin_top() const
{
    WindowImpl* pWindowImpl = ImplGetEffectiveWindowImpl();

    return pWindowImpl->mnMarginTop;
}

void Window::set_margin_bottom(sal_Int32 nWidth)
{
    if (WindowImpl* pWindowImpl = ImplGetEffectiveWindowImpl();
        pWindowImpl->mnMarginBottom != nWidth)
    {
        pWindowImpl->mnMarginBottom = nWidth;
        queue_resize();
    }
}

sal_Int32 Window::get_margin_bottom() const
{
    WindowImpl* pWindowImpl = ImplGetEffectiveWindowImpl();

    return pWindowImpl->mnMarginBottom;
}

sal_Int32 Window::get_height_request() const
{
    WindowImpl* pWindowImpl = ImplGetEffectiveWindowImpl();

    return pWindowImpl->mnHeightRequest;
}

sal_Int32 Window::get_width_request() const
{
    WindowImpl* pWindowImpl = ImplGetEffectiveWindowImpl();

    return pWindowImpl->mnWidthRequest;
}

bool Window::get_secondary() const
{
    WindowImpl* pWindowImpl = ImplGetEffectiveWindowImpl();

    return pWindowImpl->mbSecondary;
}

void Window::set_secondary(bool bSecondary)
{
    WindowImpl* pWindowImpl = ImplGetEffectiveWindowImpl();

    pWindowImpl->mbSecondary = bSecondary;
}

bool Window::get_non_homogeneous() const
{
    WindowImpl* pWindowImpl = ImplGetEffectiveWindowImpl();

    return pWindowImpl->mbNonHomogeneous;
}

void Window::set_non_homogeneous(bool bNonHomogeneous)
{
    WindowImpl* pWindowImpl = ImplGetEffectiveWindowImpl();

    pWindowImpl->mbNonHomogeneous = bNonHomogeneous;
}

void Window::add_to_size_group(const std::shared_ptr<VclSizeGroup>& xGroup)
{
    WindowImpl* pWindowImpl = ImplGetEffectiveWindowImpl();

    // TODO multiple groups
    pWindowImpl->m_xSizeGroup = xGroup;
    pWindowImpl->m_xSizeGroup->insert(this);

    if (VclSizeGroupMode::NONE != pWindowImpl->m_xSizeGroup->get_mode())
        queue_resize();
}

void Window::remove_from_all_size_groups()
{
    // TODO multiple groups
    if (WindowImpl* pWindowImpl = ImplGetEffectiveWindowImpl(); pWindowImpl->m_xSizeGroup)
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
    {
        pWin->ImplAdjustNWFSizes();
    }
}

bool Window::ImplHasValidClippingRegion() const
{
    return !mpWindowImpl->mbPaintTransparent && !mpWindowImpl->mpClippingState->mbInitWinClipRegion
           && !mpWindowImpl->mpClippingState->maWinClipRegion.IsEmpty();
}

bool Window::ImplShouldPaintImmediately() const
{
    return GetOutDev()->GetOutputWidthPixel() && GetOutDev()->GetOutputHeightPixel()
           && ImplHasValidClippingRegion() && !HasPaintEvent();
}

bool Window::ImplUpdateOutputSize(PosSizeFlags nFlags, tools::Long nWidth, tools::Long nHeight)
{
    bool bNewSize = false;

    if ((nFlags & PosSizeFlags::Width) && nWidth != GetOutDev()->GetOutputWidthPixel())
    {
        GetOutDev()->SetOutputWidthPixel(nWidth);
        bNewSize = true;
    }

    if ((nFlags & PosSizeFlags::Height) && nHeight != GetOutDev()->GetOutputHeightPixel())
    {
        GetOutDev()->SetOutputHeightPixel(nHeight);
        bNewSize = true;
    }

    return bNewSize;
}

Size Window::ImplGetClientAvailableSize() const
{
    tools::Long nWidth = GetOutDev()->GetOutputWidthPixel();
    tools::Long nHeight = GetOutDev()->GetOutputHeightPixel();

    if (mpWindowImpl->mpClientWindow)
    {
        nWidth -= mpWindowImpl->mpClientWindow->mpWindowImpl->mnLeftBorder;
        nWidth -= mpWindowImpl->mpClientWindow->mpWindowImpl->mnRightBorder;

        nHeight -= mpWindowImpl->mpClientWindow->mpWindowImpl->mnTopBorder;
        nHeight -= mpWindowImpl->mpClientWindow->mpWindowImpl->mnBottomBorder;
    }

    return Size(nWidth, nHeight);
}

void Window::ImplAdjustPosForRTL(tools::Long& rX, tools::Long& rOrgX, Point& rPtDev,
                                 bool bXAlreadyMirrored)
{
    OutputDevice* pOutDev = GetOutDev();

    if (pOutDev->HasMirroredGraphics())
    {
        rPtDev.setX(pOutDev->mpGraphics->mirror2(rPtDev.X(), *pOutDev));

        if (bXAlreadyMirrored && pOutDev->ImplIsAntiparallel())
        {
            rPtDev.setX(mpWindowImpl->mnAbsScreenX);
            rOrgX = mpWindowImpl->maPos.X();
        }
    }

    bool bParentIsAntiparallel
        = !bXAlreadyMirrored && mpWindowImpl->mpHierarchy->mpParent
          && !mpWindowImpl->mpHierarchy->mpParent->mpWindowImpl->mbFrame
          && mpWindowImpl->mpHierarchy->mpParent->GetOutDev()->ImplIsAntiparallel();

    if (bParentIsAntiparallel)
    {
        rX = mpWindowImpl->mpHierarchy->mpParent->GetOutDev()->GetOutputWidthPixel()
             - pOutDev->GetOutputWidthPixel() - rX;
    }
}

bool Window::ImplUpdatePos(PosSizeFlags nFlags, tools::Long nX, tools::Long nY,
                           bool bXAlreadyMirrored, bool bCopyBits,
                           std::unique_ptr<vcl::Region>& rpOverlapRegion)
{
    bool bNewPos = false;

    if ((nFlags & PosSizeFlags::X)
        && ImplUpdatePosX(nX, bXAlreadyMirrored, bCopyBits, rpOverlapRegion))
    {
        bNewPos = true;
    }

    if ((nFlags & PosSizeFlags::Y) && ImplUpdatePosY(nY, bCopyBits, rpOverlapRegion))
        bNewPos = true;

    return bNewPos;
}

bool Window::ImplUpdatePosX(tools::Long nX, bool bXAlreadyMirrored, bool bCopyBits,
                            std::unique_ptr<vcl::Region>& rpOverlapRegion)
{
    tools::Long nOrgX = nX;
    Point aPtDev(nX + GetOutDev()->GetDeviceOriginX(), 0);

    ImplAdjustPosForRTL(nX, nOrgX, aPtDev, bXAlreadyMirrored);

    const bool bPositionUnchanged = [&]() {
        return mpWindowImpl->mnAbsScreenX == aPtDev.X() && nX == mpWindowImpl->mnX
               && nOrgX == mpWindowImpl->maPos.X();
    }();

    if (bPositionUnchanged)
        return false;

    if (bCopyBits && !rpOverlapRegion)
    {
        rpOverlapRegion.reset(new vcl::Region());
        vcl::clipping::calcOverlapRegion(*this, GetOutputRectPixel(), *rpOverlapRegion, false,
                                         true);
    }

    mpWindowImpl->mnX = nX;
    mpWindowImpl->maPos.setX(nOrgX);
    mpWindowImpl->mnAbsScreenX = aPtDev.X();

    return true;
}

bool Window::ImplUpdatePosY(tools::Long nY, bool bCopyBits,
                            std::unique_ptr<vcl::Region>& rpOverlapRegion)
{
    // check maPos as well, as it could have been changed for client windows (ImplCallMove())
    if (nY == mpWindowImpl->mnY && nY == mpWindowImpl->maPos.Y())
        return false;

    if (bCopyBits && !rpOverlapRegion)
    {
        rpOverlapRegion.reset(new vcl::Region());
        vcl::clipping::calcOverlapRegion(*this, GetOutputRectPixel(), *rpOverlapRegion, false,
                                         true);
    }

    mpWindowImpl->mnY = nY;
    mpWindowImpl->maPos.setY(nY);

    return true;
}

void Window::ImplUpdateClientWindow(bool bNewPos)
{
    if (!mpWindowImpl->mpClientWindow)
        return;

    const Point aClientOrigin(mpWindowImpl->mpClientWindow->mpWindowImpl->mnLeftBorder,
                              mpWindowImpl->mpClientWindow->mpWindowImpl->mnTopBorder);
    const Size aClientSize = ImplGetClientAvailableSize();

    mpWindowImpl->mpClientWindow->ImplPosSizeWindow(
        aClientOrigin.X(), aClientOrigin.Y(), aClientSize.Width(), aClientSize.Height(),
        PosSizeFlags::X | PosSizeFlags::Y | PosSizeFlags::Width | PosSizeFlags::Height);

    // If we have a client window, then this is the position
    // of the Application's floating windows
    mpWindowImpl->mpClientWindow->mpWindowImpl->maPos = mpWindowImpl->maPos;

    if (!bNewPos)
        return;

    if (mpWindowImpl->mpClientWindow->IsVisible())
        mpWindowImpl->mpClientWindow->ImplCallMove();
    else
        mpWindowImpl->mpClientWindow->mpWindowImpl->mbCallMove = true;
}

bool Window::ImplCopyArea(vcl::Region& rRegion, const tools::Rectangle& rInitialWinRect)
{
    SalGraphics* pGraphics = ImplGetFrameGraphics();
    if (!pGraphics)
        return true; // bInvalidate = true

    OutputDevice* pOutDev = GetOutDev();
    const bool bSelectClipRegion = pOutDev->SelectClipRegion(rRegion, pGraphics);

    if (!bSelectClipRegion)
        return true; // bInvalidate = true

    pGraphics->CopyArea(pOutDev->GetDeviceOriginX(), pOutDev->GetDeviceOriginY(),
                        rInitialWinRect.Left(), rInitialWinRect.Top(), rInitialWinRect.GetWidth(),
                        rInitialWinRect.GetHeight(), *pOutDev);
    return false;
}

void Window::ImplPosSizeWindow(tools::Long nX, tools::Long nY, tools::Long nWidth,
                               tools::Long nHeight, PosSizeFlags nFlags)
{
    if ((nFlags & PosSizeFlags::Width) && nWidth < 0)
        nWidth = 0;

    if ((nFlags & PosSizeFlags::Height) && nHeight < 0)
        nHeight = 0;

    const tools::Long nInitialOutOffX = GetOutDev()->GetDeviceOriginX();
    const tools::Long nInitialOutOffY = GetOutDev()->GetDeviceOriginY();
    const tools::Long nInitialOutWidth = GetOutDev()->GetOutputWidthPixel();
    const tools::Long nInitialOutHeight = GetOutDev()->GetOutputHeightPixel();

    std::unique_ptr<vcl::Region> pInitialRegion;

    if (IsReallyVisible())
    {
        tools::Rectangle aInitialWinRect(Point(nInitialOutOffX, nInitialOutOffY),
                                         Size(nInitialOutWidth, nInitialOutHeight));
        pInitialRegion.reset(new vcl::Region(aInitialWinRect));

        if (mpWindowImpl->mpClippingState->mbWinRegion)
            pInitialRegion->Intersect(
                GetOutDev()->GetMapper().ViewToDevice(mpWindowImpl->mpClippingState->maWinRegion));
    }

    bool bXAlreadyMirrored = false;

    if ((nFlags & PosSizeFlags::Width) && !(nFlags & PosSizeFlags::X))
    {
        nX = mpWindowImpl->mnX;
        nFlags |= PosSizeFlags::X;
        bXAlreadyMirrored = true;
    }

    bool bCopyBits = false;

    if (IsReallyVisible() && ImplShouldPaintImmediately())
        bCopyBits = true;

    bool bNewSize = ImplUpdateOutputSize(nFlags, nWidth, nHeight);

    if (bNewSize)
        bCopyBits = false;

    std::unique_ptr<vcl::Region> pOverlapRegion;
    bool bNewPos = ImplUpdatePos(nFlags, nX, nY, bXAlreadyMirrored, bCopyBits, pOverlapRegion);

    if (!(bNewPos || bNewSize))
        return;

    bool bUpdateSysObjPos = false;
    if (bNewPos)
        bUpdateSysObjPos = ImplUpdatePos();

    // the borderwindow always specifies the position for its client window
    if (mpWindowImpl->mpBorderWindow)
        mpWindowImpl->maPos = mpWindowImpl->mpBorderWindow->mpWindowImpl->maPos;

    ImplUpdateClientWindow(bNewPos);

    // Move()/Resize() will be called only for Show(), such that
    // at least one is called before Show()
    if (IsVisible())
    {
        if (bNewPos)
            ImplCallMove();

        if (bNewSize)
            ImplCallResize();
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
        if (bNewPos || (GetOutDev()->GetOutputWidthPixel() > nInitialOutWidth)
            || (GetOutDev()->GetOutputHeightPixel() > nInitialOutHeight))
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
                        pOverlapRegion->Move(GetOutDev()->GetDeviceOriginX() - nInitialOutOffX,
                                             GetOutDev()->GetDeviceOriginY() - nInitialOutOffY);
                        aRegion.Exclude(*pOverlapRegion);
                    }

                    if (!aRegion.IsEmpty())
                    {
                        const tools::Rectangle aInitialWinRect(
                            Point(nInitialOutOffX, nInitialOutOffY),
                            Size(nInitialOutWidth, nInitialOutHeight));

                        // adapt Paint areas
                        ImplMoveAllInvalidateRegions(
                            aInitialWinRect, GetOutDev()->GetDeviceOriginX() - nInitialOutOffX,
                            GetOutDev()->GetDeviceOriginY() - nInitialOutOffY, true);

                        bInvalidate = ImplCopyArea(aRegion, aInitialWinRect);

                        if (!bInvalidate)
                        {
                            if (!pOverlapRegion->IsEmpty())
                                ImplInvalidateFrameRegion(pOverlapRegion.get(),
                                                          InvalidateFlags::Children);
                        }
                    }
                    else
                    {
                        bInvalidate = true;
                    }
                }
                else
                {
                    bInvalidate = true;
                }

                if (bInvalidate)
                    ImplInvalidateFrameRegion(nullptr, InvalidateFlags::Children);
            }
            else
            {
                vcl::Region aRegion(GetOutputRectPixel());
                aRegion.Exclude(*pInitialRegion);
                if (mpWindowImpl->mpClippingState->mbWinRegion)
                    aRegion.Intersect(GetOutDev()->GetMapper().ViewToDevice(
                        mpWindowImpl->mpClippingState->maWinRegion));
                vcl::clipping::clipBoundaries(*this, aRegion, false, true);
                if (!aRegion.IsEmpty())
                    ImplInvalidateFrameRegion(&aRegion, InvalidateFlags::Children);
            }
        }

        // invalidate Parent or Overlaps
        if (bNewPos || (GetOutDev()->GetOutputWidthPixel() < nInitialOutWidth)
            || (GetOutDev()->GetOutputHeightPixel() < nInitialOutHeight))
        {
            vcl::Region aRegion(*pInitialRegion);
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
    aSz.AdjustWidth(-ImplGetBorderWidth());
    aSz.AdjustHeight(-ImplGetBorderHeight());
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

tools::Long Window::ImplGetBorderWidth() const
{
    return mpWindowImpl->mnLeftBorder + mpWindowImpl->mnRightBorder;
}

tools::Long Window::ImplGetBorderHeight() const
{
    return mpWindowImpl->mnTopBorder + mpWindowImpl->mnBottomBorder;
}

} // end namespace vcl

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
