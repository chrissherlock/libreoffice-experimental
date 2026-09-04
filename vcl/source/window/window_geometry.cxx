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

#include <ImplFrameData.hxx>
#include <WindowImpl.hxx>
#include <WindowClippingState.hxx>
#include <WindowHierarchy.hxx>
#include <WindowLayoutData.hxx>
#include <WindowGeometry.hxx>
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

Point Window::GetPosPixel() const { return mpGeometry->maPos; }

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

WindowLayoutData* Window::ImplGetEffectiveWindowLayoutData() const
{
    if (mpWindowImpl->mpBorderWindow)
        return mpWindowImpl->mpBorderWindow->mpLayoutData.get();

    return mpLayoutData.get();
}

void Window::ImplQueueResizeOnGroup() const
{
    WindowLayoutData* pLayoutData = ImplGetEffectiveWindowLayoutData();

    if (!pLayoutData->m_xSizeGroup
        || pLayoutData->m_xSizeGroup->get_mode() == VclSizeGroupMode::NONE)
        return;

    const std::set<VclPtr<vcl::Window>>& rWindows = pLayoutData->m_xSizeGroup->get_widgets();

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
    if (!mpLayoutData)
        return;

    if (WindowLayoutData* pLayoutData = ImplGetEffectiveWindowLayoutData();
        pLayoutData->mnHeightRequest != nHeightRequest)
    {
        pLayoutData->mnHeightRequest = nHeightRequest;
        queue_resize();
    }
}

void Window::set_width_request(sal_Int32 nWidthRequest)
{
    if (!mpLayoutData)
        return;

    if (WindowLayoutData* pLayoutData = ImplGetEffectiveWindowLayoutData();
        pLayoutData->mnWidthRequest != nWidthRequest)
    {
        pLayoutData->mnWidthRequest = nWidthRequest;
        queue_resize();
    }
}

Size Window::get_ungrouped_preferred_size() const
{
    Size aPreferredSize(get_width_request(), get_height_request());

    if (aPreferredSize.Width() != -1 && aPreferredSize.Height() != -1)
        return aPreferredSize;

    // cache gets blown away by queue_resize
    WindowLayoutData* pLayoutData = ImplGetEffectiveWindowLayoutData();

    if (pLayoutData->mnOptimalWidthCache == -1 || pLayoutData->mnOptimalHeightCache == -1)
    {
        Size aOptimal(GetOptimalSize());
        pLayoutData->mnOptimalWidthCache = aOptimal.Width();
        pLayoutData->mnOptimalHeightCache = aOptimal.Height();
    }

    if (aPreferredSize.Width() == -1)
        aPreferredSize.setWidth(pLayoutData->mnOptimalWidthCache);

    if (aPreferredSize.Height() == -1)
        aPreferredSize.setHeight(pLayoutData->mnOptimalHeightCache);

    return aPreferredSize;
}

Size Window::get_preferred_size() const
{
    Size aPreferredSize(get_ungrouped_preferred_size());

    WindowLayoutData* pLayoutData = ImplGetEffectiveWindowLayoutData();

    if (!pLayoutData->m_xSizeGroup)
        return aPreferredSize;

    const VclSizeGroupMode eMode = pLayoutData->m_xSizeGroup->get_mode();

    if (eMode == VclSizeGroupMode::NONE)
        return aPreferredSize;

    const bool bIgnoreInHidden = pLayoutData->m_xSizeGroup->get_ignore_hidden();
    const std::set<VclPtr<vcl::Window>>& rWindows = pLayoutData->m_xSizeGroup->get_widgets();

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
    WindowLayoutData* pLayoutData = ImplGetEffectiveWindowLayoutData();

    return pLayoutData->meHalign;
}

void Window::set_halign(VclAlign eAlign)
{
    WindowLayoutData* pLayoutData = ImplGetEffectiveWindowLayoutData();

    pLayoutData->meHalign = eAlign;
}

VclAlign Window::get_valign() const
{
    WindowLayoutData* pLayoutData = ImplGetEffectiveWindowLayoutData();

    return pLayoutData->meValign;
}

void Window::set_valign(VclAlign eAlign)
{
    WindowLayoutData* pLayoutData = ImplGetEffectiveWindowLayoutData();

    pLayoutData->meValign = eAlign;
}

bool Window::get_hexpand() const
{
    WindowLayoutData* pLayoutData = ImplGetEffectiveWindowLayoutData();

    return pLayoutData->mbHexpand;
}

void Window::set_hexpand(bool bExpand)
{
    WindowLayoutData* pLayoutData = ImplGetEffectiveWindowLayoutData();

    pLayoutData->mbHexpand = bExpand;
}

bool Window::get_vexpand() const
{
    WindowLayoutData* pLayoutData = ImplGetEffectiveWindowLayoutData();

    return pLayoutData->mbVexpand;
}

void Window::set_vexpand(bool bExpand)
{
    WindowLayoutData* pLayoutData = ImplGetEffectiveWindowLayoutData();

    pLayoutData->mbVexpand = bExpand;
}

bool Window::get_expand() const
{
    WindowLayoutData* pLayoutData = ImplGetEffectiveWindowLayoutData();

    return pLayoutData->mbExpand;
}

void Window::set_expand(bool bExpand)
{
    WindowLayoutData* pLayoutData = ImplGetEffectiveWindowLayoutData();

    pLayoutData->mbExpand = bExpand;
}

VclPackType Window::get_pack_type() const
{
    WindowLayoutData* pLayoutData = ImplGetEffectiveWindowLayoutData();

    return pLayoutData->mePackType;
}

void Window::set_pack_type(VclPackType ePackType)
{
    WindowLayoutData* pLayoutData = ImplGetEffectiveWindowLayoutData();

    pLayoutData->mePackType = ePackType;
}

sal_Int32 Window::get_padding() const
{
    WindowLayoutData* pLayoutData = ImplGetEffectiveWindowLayoutData();

    return pLayoutData->mnPadding;
}

void Window::set_padding(sal_Int32 nPadding)
{
    WindowLayoutData* pLayoutData = ImplGetEffectiveWindowLayoutData();

    pLayoutData->mnPadding = nPadding;
}

bool Window::get_fill() const
{
    WindowLayoutData* pLayoutData = ImplGetEffectiveWindowLayoutData();

    return pLayoutData->mbFill;
}

void Window::set_fill(bool bFill)
{
    WindowLayoutData* pLayoutData = ImplGetEffectiveWindowLayoutData();

    pLayoutData->mbFill = bFill;
}

sal_Int32 Window::get_grid_width() const
{
    WindowLayoutData* pLayoutData = ImplGetEffectiveWindowLayoutData();

    return pLayoutData->mnGridWidth;
}

void Window::set_grid_width(sal_Int32 nCols)
{
    WindowLayoutData* pLayoutData = ImplGetEffectiveWindowLayoutData();

    pLayoutData->mnGridWidth = nCols;
}

sal_Int32 Window::get_grid_left_attach() const
{
    WindowLayoutData* pLayoutData = ImplGetEffectiveWindowLayoutData();

    return pLayoutData->mnGridLeftAttach;
}

void Window::set_grid_left_attach(sal_Int32 nAttach)
{
    WindowLayoutData* pLayoutData = ImplGetEffectiveWindowLayoutData();

    pLayoutData->mnGridLeftAttach = nAttach;
}

sal_Int32 Window::get_grid_height() const
{
    WindowLayoutData* pLayoutData = ImplGetEffectiveWindowLayoutData();

    return pLayoutData->mnGridHeight;
}

void Window::set_grid_height(sal_Int32 nRows)
{
    WindowLayoutData* pLayoutData = ImplGetEffectiveWindowLayoutData();

    pLayoutData->mnGridHeight = nRows;
}

sal_Int32 Window::get_grid_top_attach() const
{
    WindowLayoutData* pLayoutData = ImplGetEffectiveWindowLayoutData();

    return pLayoutData->mnGridTopAttach;
}

void Window::set_grid_top_attach(sal_Int32 nAttach)
{
    WindowLayoutData* pLayoutData = ImplGetEffectiveWindowLayoutData();

    pLayoutData->mnGridTopAttach = nAttach;
}

void Window::set_border_width(sal_Int32 nBorderWidth)
{
    WindowLayoutData* pLayoutData = ImplGetEffectiveWindowLayoutData();

    pLayoutData->mnBorderWidth = nBorderWidth;
}

sal_Int32 Window::get_border_width() const
{
    WindowLayoutData* pLayoutData = ImplGetEffectiveWindowLayoutData();

    return pLayoutData->mnBorderWidth;
}

void Window::set_margin_start(sal_Int32 nWidth)
{
    if (WindowLayoutData* pLayoutData = ImplGetEffectiveWindowLayoutData();
        pLayoutData->mnMarginLeft != nWidth)
    {
        pLayoutData->mnMarginLeft = nWidth;
        queue_resize();
    }
}

sal_Int32 Window::get_margin_start() const
{
    WindowLayoutData* pLayoutData = ImplGetEffectiveWindowLayoutData();

    return pLayoutData->mnMarginLeft;
}

void Window::set_margin_end(sal_Int32 nWidth)
{
    if (WindowLayoutData* pLayoutData = ImplGetEffectiveWindowLayoutData();
        pLayoutData->mnMarginRight != nWidth)
    {
        pLayoutData->mnMarginRight = nWidth;
        queue_resize();
    }
}

sal_Int32 Window::get_margin_end() const
{
    WindowLayoutData* pLayoutData = ImplGetEffectiveWindowLayoutData();

    return pLayoutData->mnMarginRight;
}

void Window::set_margin_top(sal_Int32 nWidth)
{
    if (WindowLayoutData* pLayoutData = ImplGetEffectiveWindowLayoutData();
        pLayoutData->mnMarginTop != nWidth)
    {
        pLayoutData->mnMarginTop = nWidth;
        queue_resize();
    }
}

sal_Int32 Window::get_margin_top() const
{
    WindowLayoutData* pLayoutData = ImplGetEffectiveWindowLayoutData();

    return pLayoutData->mnMarginTop;
}

void Window::set_margin_bottom(sal_Int32 nWidth)
{
    if (WindowLayoutData* pLayoutData = ImplGetEffectiveWindowLayoutData();
        pLayoutData->mnMarginBottom != nWidth)
    {
        pLayoutData->mnMarginBottom = nWidth;
        queue_resize();
    }
}

sal_Int32 Window::get_margin_bottom() const
{
    WindowLayoutData* pLayoutData = ImplGetEffectiveWindowLayoutData();

    return pLayoutData->mnMarginBottom;
}

sal_Int32 Window::get_height_request() const
{
    WindowLayoutData* pLayoutData = ImplGetEffectiveWindowLayoutData();

    return pLayoutData->mnHeightRequest;
}

sal_Int32 Window::get_width_request() const
{
    WindowLayoutData* pLayoutData = ImplGetEffectiveWindowLayoutData();

    return pLayoutData->mnWidthRequest;
}

bool Window::get_secondary() const
{
    WindowLayoutData* pLayoutData = ImplGetEffectiveWindowLayoutData();

    return pLayoutData->mbSecondary;
}

void Window::set_secondary(bool bSecondary)
{
    WindowLayoutData* pLayoutData = ImplGetEffectiveWindowLayoutData();

    pLayoutData->mbSecondary = bSecondary;
}

bool Window::get_non_homogeneous() const
{
    WindowLayoutData* pLayoutData = ImplGetEffectiveWindowLayoutData();

    return pLayoutData->mbNonHomogeneous;
}

void Window::set_non_homogeneous(bool bNonHomogeneous)
{
    WindowLayoutData* pLayoutData = ImplGetEffectiveWindowLayoutData();

    pLayoutData->mbNonHomogeneous = bNonHomogeneous;
}

void Window::add_to_size_group(const std::shared_ptr<VclSizeGroup>& xGroup)
{
    WindowLayoutData* pLayoutData = ImplGetEffectiveWindowLayoutData();

    // TODO multiple groups
    pLayoutData->m_xSizeGroup = xGroup;
    pLayoutData->m_xSizeGroup->insert(this);

    if (VclSizeGroupMode::NONE != pLayoutData->m_xSizeGroup->get_mode())
        queue_resize();
}

void Window::remove_from_all_size_groups()
{
    // TODO multiple groups
    if (WindowLayoutData* pLayoutData = ImplGetEffectiveWindowLayoutData();
        pLayoutData->m_xSizeGroup)
    {
        if (VclSizeGroupMode::NONE != pLayoutData->m_xSizeGroup->get_mode())
            queue_resize();

        pLayoutData->m_xSizeGroup->erase(this);
        pLayoutData->m_xSizeGroup.reset();
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
        nWidth -= mpWindowImpl->mpClientWindow->mpGeometry->mnLeftBorder;
        nWidth -= mpWindowImpl->mpClientWindow->mpGeometry->mnRightBorder;

        nHeight -= mpWindowImpl->mpClientWindow->mpGeometry->mnTopBorder;
        nHeight -= mpWindowImpl->mpClientWindow->mpGeometry->mnBottomBorder;
    }

    return Size(nWidth, nHeight);
}

bool Window::ImplHasAntiparallelParent() const
{
    return mpWindowImpl->mpHierarchy->mpParent
           && !mpWindowImpl->mpHierarchy->mpParent->mpWindowImpl->mbFrame
           && mpWindowImpl->mpHierarchy->mpParent->GetOutDev()->ImplIsAntiparallel();
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
            rPtDev.setX(mpGeometry->mnAbsScreenX);
            rOrgX = mpGeometry->maPos.X();
        }
    }

    const bool bParentIsAntiparallel = !bXAlreadyMirrored && ImplHasAntiparallelParent();

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
        return mpGeometry->mnAbsScreenX == aPtDev.X() && nX == mpGeometry->mnX
               && nOrgX == mpGeometry->maPos.X();
    }();

    if (bPositionUnchanged)
        return false;

    if (bCopyBits && !rpOverlapRegion)
    {
        rpOverlapRegion.reset(new vcl::Region());
        vcl::clipping::calcOverlapRegion(*this, GetOutputRectPixel(), *rpOverlapRegion, false,
                                         true);
    }

    mpGeometry->mnX = nX;
    mpGeometry->maPos.setX(nOrgX);
    mpGeometry->mnAbsScreenX = aPtDev.X();

    return true;
}

bool Window::ImplUpdatePosY(tools::Long nY, bool bCopyBits,
                            std::unique_ptr<vcl::Region>& rpOverlapRegion)
{
    // check maPos as well, as it could have been changed for client windows (ImplCallMove())
    if (nY == mpGeometry->mnY && nY == mpGeometry->maPos.Y())
        return false;

    if (bCopyBits && !rpOverlapRegion)
    {
        rpOverlapRegion.reset(new vcl::Region());
        vcl::clipping::calcOverlapRegion(*this, GetOutputRectPixel(), *rpOverlapRegion, false,
                                         true);
    }

    mpGeometry->mnY = nY;
    mpGeometry->maPos.setY(nY);

    return true;
}

void Window::ImplUpdateClientWindow(bool bNewPos)
{
    if (!mpWindowImpl->mpClientWindow)
        return;

    const Point aClientOrigin(mpWindowImpl->mpClientWindow->mpGeometry->mnLeftBorder,
                              mpWindowImpl->mpClientWindow->mpGeometry->mnTopBorder);
    const Size aClientSize = ImplGetClientAvailableSize();

    mpWindowImpl->mpClientWindow->ImplPosSizeWindow(
        aClientOrigin.X(), aClientOrigin.Y(), aClientSize.Width(), aClientSize.Height(),
        PosSizeFlags::X | PosSizeFlags::Y | PosSizeFlags::Width | PosSizeFlags::Height);

    // If we have a client window, then this is the position
    // of the Application's floating windows
    mpWindowImpl->mpClientWindow->mpGeometry->maPos = mpGeometry->maPos;

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

bool Window::ImplCopyBitsRegion(std::unique_ptr<vcl::Region>& rpOverlapRegion,
                                const tools::Rectangle& rInitialWinRect)
{
    vcl::Region aRegion(GetOutputRectPixel());

    if (mpWindowImpl->mpClippingState->mbWinRegion)
        aRegion.Intersect(
            GetOutDev()->GetMapper().ViewToDevice(mpWindowImpl->mpClippingState->maWinRegion));

    vcl::clipping::clipBoundaries(*this, aRegion, false, true);

    if (!rpOverlapRegion->IsEmpty())
    {
        rpOverlapRegion->Move(GetOutDev()->GetDeviceOriginX() - rInitialWinRect.Left(),
                              GetOutDev()->GetDeviceOriginY() - rInitialWinRect.Top());
        aRegion.Exclude(*rpOverlapRegion);
    }

    if (aRegion.IsEmpty())
        return true; // bInvalidate = true

    // adapt Paint areas
    ImplMoveAllInvalidateRegions(rInitialWinRect,
                                 GetOutDev()->GetDeviceOriginX() - rInitialWinRect.Left(),
                                 GetOutDev()->GetDeviceOriginY() - rInitialWinRect.Top(), true);

    bool bInvalidate = ImplCopyArea(aRegion, rInitialWinRect);

    if (!bInvalidate)
    {
        if (!rpOverlapRegion->IsEmpty())
            ImplInvalidateFrameRegion(rpOverlapRegion.get(), InvalidateFlags::Children);
    }

    return bInvalidate;
}

void Window::ImplCallMoveResize(bool bNewPos, bool bNewSize)
{
    if (bNewPos)
        ImplCallMove();

    if (bNewSize)
        ImplCallResize();
}

void Window::ImplDeferMoveResize(bool bNewPos, bool bNewSize)
{
    if (bNewPos)
        mpWindowImpl->mbCallMove = true;

    if (bNewSize)
        mpWindowImpl->mbCallResize = true;
}

void Window::ImplInvalidateMovedWindow(bool bCopyBits, const tools::Rectangle& rInitialWinRect,
                                       std::unique_ptr<vcl::Region>& rpOverlapRegion)
{
    bool bInvalidate = false;
    bool bParentPaint = true;

    if (!ImplIsOverlapWindow())
        bParentPaint = mpWindowImpl->mpHierarchy->mpParent->IsPaintEnabled();

    if (bCopyBits && bParentPaint && !HasPaintEvent())
        bInvalidate = ImplCopyBitsRegion(rpOverlapRegion, rInitialWinRect);
    else
        bInvalidate = true;

    if (bInvalidate)
        ImplInvalidateFrameRegion(nullptr, InvalidateFlags::Children);
}

void Window::ImplInvalidateGrownWindow(const vcl::Region& rInitialRegion)
{
    vcl::Region aRegion(GetOutputRectPixel());
    aRegion.Exclude(rInitialRegion);

    if (mpWindowImpl->mpClippingState->mbWinRegion)
    {
        aRegion.Intersect(
            GetOutDev()->GetMapper().ViewToDevice(mpWindowImpl->mpClippingState->maWinRegion));
    }

    vcl::clipping::clipBoundaries(*this, aRegion, false, true);

    if (!aRegion.IsEmpty())
        ImplInvalidateFrameRegion(&aRegion, InvalidateFlags::Children);
}

void Window::ImplInvalidateWindowContent(bool bNewPos, bool bCopyBits,
                                         const tools::Rectangle& rInitialWinRect,
                                         std::unique_ptr<vcl::Region>& rpOverlapRegion,
                                         const vcl::Region& rInitialRegion)
{
    if (bNewPos)
    {
        ImplInvalidateMovedWindow(bCopyBits, rInitialWinRect, rpOverlapRegion);
        return;
    }

    auto HasOutputGrown = [](const OutputDevice* pOutDev, const Size& rInitialSize) {
        return pOutDev->GetOutputWidthPixel() > rInitialSize.Width()
               || pOutDev->GetOutputHeightPixel() > rInitialSize.Height();
    };

    if (HasOutputGrown(GetOutDev(), rInitialWinRect.GetSize()))
        ImplInvalidateGrownWindow(rInitialRegion);
}

void Window::ImplInvalidateParentOrOverlaps(const vcl::Region& rInitialRegion)
{
    vcl::Region aRegion(rInitialRegion);
    if (!mpWindowImpl->mbPaintTransparent)
        vcl::clipping::excludeWindowRegion(*this, aRegion);

    vcl::clipping::clipBoundaries(*this, aRegion, false, true);

    if (!aRegion.IsEmpty() && !mpWindowImpl->mpBorderWindow)
        ImplInvalidateParentFrameRegion(aRegion);
}

bool Window::ImplInvalidateVisibleRegions(bool bNewPos, bool bNewSize, bool bCopyBits,
                                          const tools::Rectangle& rInitialWinRect,
                                          std::unique_ptr<vcl::Region>& rpOverlapRegion,
                                          const vcl::Region& rInitialRegion)
{
    if (!IsReallyVisible())
        return false;

    bool bNeedsNativeClipUpdate = false;
    if (bNewPos || bNewSize)
        bNeedsNativeClipUpdate = !vcl::clipping::setClipFlag(*this, true);

    ImplInvalidateWindowContent(bNewPos, bCopyBits, rInitialWinRect, rpOverlapRegion,
                                rInitialRegion);

    auto HasOutputShrunk = [](const OutputDevice* pOutDev, const Size& rInitialSize) {
        return pOutDev->GetOutputWidthPixel() < rInitialSize.Width()
               || pOutDev->GetOutputHeightPixel() < rInitialSize.Height();
    };

    if (bNewPos || HasOutputShrunk(GetOutDev(), rInitialWinRect.GetSize()))
        ImplInvalidateParentOrOverlaps(rInitialRegion);

    return bNeedsNativeClipUpdate;
}

void Window::ImplPosSizeWindow(tools::Long nX, tools::Long nY, tools::Long nWidth,
                               tools::Long nHeight, PosSizeFlags nFlags)
{
    if ((nFlags & PosSizeFlags::Width) && nWidth < 0)
        nWidth = 0;

    if ((nFlags & PosSizeFlags::Height) && nHeight < 0)
        nHeight = 0;

    const tools::Rectangle aInitialWinRect(
        Point(GetOutDev()->GetDeviceOriginX(), GetOutDev()->GetDeviceOriginY()),
        Size(GetOutDev()->GetOutputWidthPixel(), GetOutDev()->GetOutputHeightPixel()));

    std::unique_ptr<vcl::Region> pInitialRegion;
    if (IsReallyVisible())
    {
        pInitialRegion.reset(new vcl::Region(aInitialWinRect));
        if (mpWindowImpl->mpClippingState->mbWinRegion)
            pInitialRegion->Intersect(
                GetOutDev()->GetMapper().ViewToDevice(mpWindowImpl->mpClippingState->maWinRegion));
    }

    bool bXAlreadyMirrored = false;
    if ((nFlags & PosSizeFlags::Width) && !(nFlags & PosSizeFlags::X))
    {
        nX = mpGeometry->mnX;
        nFlags |= PosSizeFlags::X;
        bXAlreadyMirrored = true;
    }

    const bool bNewSize = ImplUpdateOutputSize(nFlags, nWidth, nHeight);
    const bool bCopyBits = !bNewSize && IsReallyVisible() && ImplShouldPaintImmediately();

    std::unique_ptr<vcl::Region> pOverlapRegion;
    const bool bNewPos
        = ImplUpdatePos(nFlags, nX, nY, bXAlreadyMirrored, bCopyBits, pOverlapRegion);

    if (!(bNewPos || bNewSize))
        return;

    const bool bNeedsNativePosUpdate = bNewPos && ImplUpdatePos();

    // the borderwindow always specifies the position for its client window
    if (mpWindowImpl->mpBorderWindow)
        mpGeometry->maPos = mpWindowImpl->mpBorderWindow->mpGeometry->maPos;

    ImplUpdateClientWindow(bNewPos);

    // Move()/Resize() will be called only for Show(), such that
    // at least one is called before Show()
    if (IsVisible())
        ImplCallMoveResize(bNewPos, bNewSize);
    else
        ImplDeferMoveResize(bNewPos, bNewSize);

    // Fix potential null pointer dereference on pInitialRegion
    bool bNeedsNativeClipUpdate = false;
    if (pInitialRegion)
    {
        bNeedsNativeClipUpdate = ImplInvalidateVisibleRegions(
            bNewPos, bNewSize, bCopyBits, aInitialWinRect, pOverlapRegion, *pInitialRegion);
    }

    // Adapt system objects
    if (bNeedsNativeClipUpdate)
        vcl::clipping::updateNativeObjectClip(*this);

    if (bNeedsNativePosUpdate)
        ImplUpdateNativeObjectPos();

    if (bNewSize && mpWindowImpl->mpSysObj)
    {
        mpWindowImpl->mpSysObj->SetPosSize(
            GetOutDev()->GetDeviceOriginX(), GetOutDev()->GetDeviceOriginY(),
            GetOutDev()->GetOutputWidthPixel(), GetOutDev()->GetOutputHeightPixel());
    }
}

bool Window::ImplUpdatePos()
{
    bool bSysChild = false;

    if (ImplIsOverlapWindow())
    {
        GetOutDev()->SetDeviceOriginX(mpGeometry->mnX);
        GetOutDev()->SetDeviceOriginY(mpGeometry->mnY);
    }
    else
    {
        vcl::Window* pParent = ImplGetParent();

        GetOutDev()->SetDeviceOriginX(mpGeometry->mnX + pParent->GetOutDev()->GetDeviceOriginX());
        GetOutDev()->SetDeviceOriginY(mpGeometry->mnY + pParent->GetOutDev()->GetDeviceOriginY());
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

tools::Long Window::ImplGetParentDeviceOriginX() const
{
    if (mpWindowImpl->mpHierarchy->mpParent)
        return mpWindowImpl->mpHierarchy->mpParent->GetOutDev()->GetDeviceOriginX();

    return 0;
}

/**
 * Let W_p be the parent's width, W_c be the child's width, and X_m be the
 * current mirrored offset. The unmirrored offset X_u is calculated as:
 * X_u = W_p - W_c - X_m
 */
tools::Long Window::ImplUnmirrorXOffset(tools::Long nMirroredOffset) const
{
    const tools::Long nParentWidth
        = mpWindowImpl->mpHierarchy->mpParent->GetOutDev()->GetOutputWidthPixel();
    const tools::Long nChildWidth = GetOutDev()->GetOutputWidthPixel();

    return nParentWidth - nChildWidth - nMirroredOffset;
}

tools::Long Window::ImplGetUnmirroredOutOffX() const
{
    // revert GetDeviceOriginX() changes that were potentially made in ImplPosSizeWindow
    tools::Long offx = GetOutDev()->GetDeviceOriginX();
    const OutputDevice* pOutDev = GetOutDev();

    if (!pOutDev->HasMirroredGraphics() || !ImplHasAntiparallelParent())
        return offx;

    if (!ImplIsOverlapWindow())
        offx -= ImplGetParentDeviceOriginX();

    offx = ImplUnmirrorXOffset(offx);

    if (!ImplIsOverlapWindow())
        offx += ImplGetParentDeviceOriginX();

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
    WindowLayoutData* pLayoutData = mpWindowImpl->mpBorderWindow
                                        ? mpWindowImpl->mpBorderWindow->mpLayoutData.get()
                                        : mpLayoutData.get();
    pLayoutData->mnOptimalWidthCache = -1;
    pLayoutData->mnOptimalHeightCache = -1;
}

tools::Long Window::ImplGetBorderWidth() const
{
    return mpGeometry->mnLeftBorder + mpGeometry->mnRightBorder;
}

tools::Long Window::ImplGetBorderHeight() const
{
    return mpGeometry->mnTopBorder + mpGeometry->mnBottomBorder;
}

bool Window::IsScrollable() const
{
    // check for scrollbars
    VclPtr<vcl::Window> pChild = mpWindowImpl->mpHierarchy->mpFirstChild;
    while (pChild)
    {
        if (pChild->GetType() == WindowType::SCROLLBAR)
            return true;

        pChild = pChild->mpWindowImpl->mpHierarchy->mpNext;
    }

    return false;
}

void Window::ImplIsInTaskPaneList(bool mbIsInTaskList)
{
    mpWindowImpl->mbIsInTaskPaneList = mbIsInTaskList;
}

} // end namespace vcl

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
