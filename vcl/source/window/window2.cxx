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

namespace vcl
{

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
    // #101741 do not check for WB_CLOSEABLE because undecorated floaters (like menus!) are closeable
    if (mpWindowImpl->mbFrame && (mpWindowImpl->mnStyle & (WB_MOVEABLE | WB_SIZEABLE)))
        return true;

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
