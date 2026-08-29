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

#include <limits.h>

#include <o3tl/float_int_conversion.hxx>
#include <sal/log.hxx>

#include <tools/helpers.hxx>
#include <tools/mapunit.hxx>

#include <vcl/MappingPolicy.hxx>
#include <vcl/toolkit/dialog.hxx>
#include <vcl/event.hxx>
#include <vcl/toolkit/fixed.hxx>
#include <vcl/layout.hxx>
#include <vcl/timer.hxx>
#include <vcl/window.hxx>
#include <vcl/scrollable.hxx>
#include <vcl/toolkit/scrbar.hxx>
#include <vcl/dockwin.hxx>
#include <vcl/settings.hxx>
#include <vcl/builder.hxx>
#include <o3tl/string_view.hxx>
#include <vcl/CoordinateMapper.hxx>

#include <clipping.hxx>
#include <clipping_window.hxx>
#include <window.h>
#include <svdata.hxx>
#include <salgdi.hxx>
#include <salframe.hxx>
#include <scrwnd.hxx>

#include <com/sun/star/accessibility/AccessibleRelation.hpp>
#include <com/sun/star/accessibility/AccessibleRole.hpp>

using namespace com::sun::star;

namespace vcl {

void Window::SetZoom( double fZoom )
{
    if ( mpWindowImpl && mpWindowImpl->mfZoom != fZoom )
    {
        mpWindowImpl->mfZoom = fZoom;
        CompatStateChanged( StateChangedType::Zoom );
    }
}

void Window::SetZoomedPointFont(vcl::RenderContext& rRenderContext, const vcl::Font& rFont)
{
    double fZoom = GetZoom();
    if (fZoom != 1.0)
    {
        vcl::Font aFont(rFont);
        Size aSize = aFont.GetFontSize();
        aSize.setWidth(basegfx::fround<tools::Long>(aSize.Width() * fZoom));
        aSize.setHeight(basegfx::fround<tools::Long>(aSize.Height() * fZoom));
        aFont.SetFontSize(aSize);
        SetPointFont(rRenderContext, aFont);
    }
    else
    {
        SetPointFont(rRenderContext, rFont);
    }
}

tools::Long Window::CalcZoom( tools::Long nCalc ) const
{
    double fZoom = GetZoom();
    if ( fZoom != 1.0)
    {
        double n = nCalc * fZoom;
        nCalc = basegfx::fround<tools::Long>(n);
    }
    return nCalc;
}

void Window::SetControlFont()
{
    if (mpWindowImpl && mpWindowImpl->mpControlFont)
    {
        mpWindowImpl->mpControlFont.reset();
        CompatStateChanged(StateChangedType::ControlFont);
    }
}

void Window::SetControlFont(const vcl::Font& rFont)
{
    if (rFont == vcl::Font())
    {
        SetControlFont();
        return;
    }

    if (mpWindowImpl->mpControlFont)
    {
        if (*mpWindowImpl->mpControlFont == rFont)
            return;
        *mpWindowImpl->mpControlFont = rFont;
    }
    else
        mpWindowImpl->mpControlFont = rFont;

    CompatStateChanged(StateChangedType::ControlFont);
}

vcl::Font Window::GetControlFont() const
{
    if (mpWindowImpl->mpControlFont)
        return *mpWindowImpl->mpControlFont;
    else
    {
        vcl::Font aFont;
        return aFont;
    }
}

void Window::ApplyControlFont(vcl::RenderContext& rRenderContext, const vcl::Font& rFont)
{
    vcl::Font aFont(rFont);
    if (IsControlFont())
        aFont.Merge(GetControlFont());
    SetZoomedPointFont(rRenderContext, aFont);
}

void Window::SetControlForeground()
{
    if (mpWindowImpl->mbControlForeground)
    {
        mpWindowImpl->maControlForeground = COL_TRANSPARENT;
        mpWindowImpl->mbControlForeground = false;
        CompatStateChanged(StateChangedType::ControlForeground);
    }
}

void Window::SetControlForeground(const Color& rColor)
{
    if (rColor.IsTransparent())
    {
        if (mpWindowImpl->mbControlForeground)
        {
            mpWindowImpl->maControlForeground = COL_TRANSPARENT;
            mpWindowImpl->mbControlForeground = false;
            CompatStateChanged(StateChangedType::ControlForeground);
        }
    }
    else
    {
        if (mpWindowImpl->maControlForeground != rColor)
        {
            mpWindowImpl->maControlForeground = rColor;
            mpWindowImpl->mbControlForeground = true;
            CompatStateChanged(StateChangedType::ControlForeground);
        }
    }
}

void Window::ApplyControlForeground(vcl::RenderContext& rRenderContext, const Color& rDefaultColor)
{
    Color aTextColor(rDefaultColor);
    if (IsControlForeground())
        aTextColor = GetControlForeground();
    rRenderContext.SetTextColor(aTextColor);
}

void Window::SetControlBackground()
{
    if (mpWindowImpl->mbControlBackground)
    {
        mpWindowImpl->maControlBackground = COL_TRANSPARENT;
        mpWindowImpl->mbControlBackground = false;
        CompatStateChanged(StateChangedType::ControlBackground);
    }
}

void Window::SetControlBackground(const Color& rColor)
{
    if (rColor.IsTransparent())
    {
        if (mpWindowImpl->mbControlBackground)
        {
            mpWindowImpl->maControlBackground = COL_TRANSPARENT;
            mpWindowImpl->mbControlBackground = false;
            CompatStateChanged(StateChangedType::ControlBackground);
        }
    }
    else
    {
        if (mpWindowImpl->maControlBackground != rColor)
        {
            mpWindowImpl->maControlBackground = rColor;
            mpWindowImpl->mbControlBackground = true;
            CompatStateChanged(StateChangedType::ControlBackground);
        }
    }
}

void Window::ApplyControlBackground(vcl::RenderContext& rRenderContext, const Color& rDefaultColor)
{
    Color aColor(rDefaultColor);
    if (IsControlBackground())
        aColor = GetControlBackground();
    rRenderContext.SetBackground(aColor);
}

vcl::Font Window::GetDrawPixelFont(OutputDevice const * pDev) const
{
    vcl::Font aFont = GetPointFont(*GetOutDev());
    MapMode aPtMapMode(MapUnit::MapPoint);
    const auto aFontSize = pDev->convertTo<vcl::WindowSize>(vcl::LogicSize(aFont.GetFontSize()), aPtMapMode);
    aFont.SetFontSize(aFontSize.get());
    return aFont;
}

tools::Long Window::GetDrawPixel( OutputDevice const * pDev, tools::Long nPixels ) const
{
    tools::Long nP = nPixels;
    if ( pDev->GetOutDevType() != OUTDEV_WINDOW )
    {
        MapMode aMap( MapUnit::Map100thMM );
        auto aSz = convertTo<vcl::WindowSize>(vcl::LogicSize(Size(nP, 0)), aMap);
        nP = aSz->Width();
    }
    return nP;
}

DockingManager* Window::GetDockingManager()
{
    return ImplGetDockingManager();
}

void Window::EnableDocking( bool bEnable )
{
    // update list of dockable windows
    if( bEnable )
        ImplGetDockingManager()->AddWindow( this );
    else
        ImplGetDockingManager()->RemoveWindow( this );
}

// retrieves the list of owner draw decorated windows for this window hierarchy
::std::vector<VclPtr<vcl::Window> >& Window::ImplGetOwnerDrawList()
{
    return ImplGetTopmostFrameWindow()->mpWindowImpl->mpFrameData->maOwnerDrawList;
}

void Window::SetHelpId( const OUString& rHelpId )
{
    mpWindowImpl->maHelpId = rHelpId;
}

const OUString& Window::GetHelpId() const
{
    return mpWindowImpl->maHelpId;
}

// --------- old inline methods ---------------

vcl::Window* Window::ImplGetBorderWindow() const
{
    return mpWindowImpl ? mpWindowImpl->mpBorderWindow.get() : nullptr;
}

bool Window::IsDockingWindow() const
{
    return mpWindowImpl && mpWindowImpl->mbDockWin;
}

void Window::ImplSetMouseTransparent( bool bTransparent )
{
    if (mpWindowImpl)
        mpWindowImpl->mbMouseTransparent = bTransparent;
}

WinBits Window::GetStyle() const
{
    return mpWindowImpl ? mpWindowImpl->mnStyle : 0;
}

WinBits Window::GetPrevStyle() const
{
    return mpWindowImpl ? mpWindowImpl->mnPrevStyle : 0;
}

WindowExtendedStyle Window::GetExtendedStyle() const
{
    return mpWindowImpl ? mpWindowImpl->mnExtendedStyle : WindowExtendedStyle::NONE;
}

bool Window::IsFormControl() const
{
    return mpWindowImpl ? mpWindowImpl->mbIsFormControl : false;
}

void Window::SetFormControl(bool bFormControl)
{
    if (mpWindowImpl)
        mpWindowImpl->mbIsFormControl = bFormControl;
}

Dialog* Window::GetParentDialog() const
{
    const vcl::Window *pWindow = this;

    while( pWindow )
    {
        if( pWindow->IsDialog() )
            break;

        pWindow = pWindow->GetParent();
    }

    return const_cast<Dialog *>(dynamic_cast<const Dialog*>(pWindow));
}

bool Window::IsMenuFloatingWindow() const
{
    return mpWindowImpl && mpWindowImpl->mbMenuFloatingWindow;
}

bool Window::IsNativeFrame() const
{
    if( mpWindowImpl->mbFrame )
        // #101741 do not check for WB_CLOSEABLE because undecorated floaters (like menus!) are closeable
        if( mpWindowImpl->mnStyle & (WB_MOVEABLE | WB_SIZEABLE) )
            return true;
        else
            return false;
    else
        return false;
}

void Window::EnableAllResize()
{
    mpWindowImpl->mbAllResize = true;
}

void Window::EnableChildTransparentMode( bool bEnable )
{
    mpWindowImpl->mbChildTransparent = bEnable;
}

bool Window::IsChildTransparentModeEnabled() const
{
    return mpWindowImpl && mpWindowImpl->mbChildTransparent;
}

bool Window::IsMouseTransparent() const
{
    return mpWindowImpl && mpWindowImpl->mbMouseTransparent;
}

bool Window::IsPaintTransparent() const
{
    return mpWindowImpl && mpWindowImpl->mbPaintTransparent;
}

void Window::SetDialogControlStart( bool bStart )
{
    mpWindowImpl->mbDlgCtrlStart = bStart;
}

bool Window::IsDialogControlStart() const
{
    return mpWindowImpl && mpWindowImpl->mbDlgCtrlStart;
}

void Window::SetDialogControlFlags( DialogControlFlags nFlags )
{
    mpWindowImpl->mnDlgCtrlFlags = nFlags;
}

DialogControlFlags Window::GetDialogControlFlags() const
{
    return mpWindowImpl->mnDlgCtrlFlags;
}

const InputContext& Window::GetInputContext() const
{
    return mpWindowImpl->maInputContext;
}

bool Window::IsControlFont() const
{
    return bool(mpWindowImpl->mpControlFont);
}

const Color& Window::GetControlForeground() const
{
    return mpWindowImpl->maControlForeground;
}

bool Window::IsControlForeground() const
{
    return mpWindowImpl->mbControlForeground;
}

const Color& Window::GetControlBackground() const
{
    return mpWindowImpl->maControlBackground;
}

bool Window::IsControlBackground() const
{
    return mpWindowImpl->mbControlBackground;
}

bool Window::IsInPaint() const
{
    return mpWindowImpl && mpWindowImpl->mbInPaint;
}

bool Window::IsVisible() const
{
    return mpWindowImpl && mpWindowImpl->mbVisible;
}

bool Window::IsReallyVisible() const
{
    return mpWindowImpl && mpWindowImpl->mbReallyVisible;
}

bool Window::IsReallyShown() const
{
    return mpWindowImpl && mpWindowImpl->mbReallyShown;
}

bool Window::IsInInitShow() const
{
    return mpWindowImpl->mbInInitShow;
}

bool Window::IsEnabled() const
{
    return mpWindowImpl && !mpWindowImpl->mbDisabled;
}

bool Window::IsInputEnabled() const
{
    return mpWindowImpl && !mpWindowImpl->mbInputDisabled;
}

bool Window::IsAlwaysEnableInput() const
{
    return mpWindowImpl->meAlwaysInputMode == AlwaysInputEnabled;
}

ActivateModeFlags Window::GetActivateMode() const
{
    return mpWindowImpl->mnActivateMode;

}

bool Window::IsAlwaysOnTopEnabled() const
{
    return mpWindowImpl->mbAlwaysOnTop;
}

void Window::EnablePaint( bool bEnable )
{
    mpWindowImpl->mbPaintDisabled = !bEnable;
}

bool Window::IsPaintEnabled() const
{
    return !mpWindowImpl->mbPaintDisabled;
}

bool Window::IsUpdateMode() const
{
    return !mpWindowImpl->mbNoUpdate;
}

void Window::SetParentUpdateMode( bool bUpdate )
{
    mpWindowImpl->mbNoParentUpdate = !bUpdate;
}

bool Window::IsActive() const
{
    return mpWindowImpl->mbActive;
}

GetFocusFlags Window::GetGetFocusFlags() const
{
    return mpWindowImpl->mnGetFocusFlags;
}

bool Window::IsCompoundControl() const
{
    return mpWindowImpl && mpWindowImpl->mbCompoundControl;
}

bool Window::IsWait() const
{
    return (mpWindowImpl->mnWaitCount != 0);
}

vcl::Cursor* Window::GetCursor() const
{
    if (!mpWindowImpl)
        return nullptr;
    return mpWindowImpl->mpCursor;
}

double Window::GetZoom() const
{
    return mpWindowImpl->mfZoom;
}

bool Window::IsZoom() const
{
    return mpWindowImpl->mfZoom != 1.0;
}

void Window::SetHelpText( const OUString& rHelpText )
{
    mpWindowImpl->maHelpText = rHelpText;
    mpWindowImpl->mbHelpTextDynamic = true;
}

void Window::SetQuickHelpText( const OUString& rHelpText )
{
    if (mpWindowImpl)
        mpWindowImpl->maQuickHelpText = rHelpText;
}

const OUString& Window::GetQuickHelpText() const
{
    return mpWindowImpl->maQuickHelpText;
}

bool Window::IsCreatedWithToolkit() const
{
    return mpWindowImpl->mbCreatedWithToolkit;
}

void Window::SetCreatedWithToolkit( bool b )
{
    mpWindowImpl->mbCreatedWithToolkit = b;
}

PointerStyle Window::GetPointer() const
{
    return mpWindowImpl->maPointer;
}

VCLXWindow* Window::GetWindowPeer() const
{
    return mpWindowImpl ? mpWindowImpl->mpVCLXWindow : nullptr;
}

} /* namespace vcl */

void InvertFocusRect(vcl::RenderContext& rRenderContext, const tools::Rectangle& rRect)
{
    const int nBorder = 1;
    rRenderContext.Invert(tools::Rectangle(Point(rRect.Left(), rRect.Top()), Size(rRect.GetWidth(), nBorder)), InvertFlags::N50);
    rRenderContext.Invert(tools::Rectangle(Point(rRect.Left(), rRect.Bottom()-nBorder+1), Size(rRect.GetWidth(), nBorder)), InvertFlags::N50);
    rRenderContext.Invert(tools::Rectangle(Point(rRect.Left(), rRect.Top()+nBorder), Size(nBorder, rRect.GetHeight()-(nBorder*2))), InvertFlags::N50);
    rRenderContext.Invert(tools::Rectangle(Point(rRect.Right()-nBorder+1, rRect.Top()+nBorder), Size(nBorder, rRect.GetHeight()-(nBorder*2))), InvertFlags::N50);
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
