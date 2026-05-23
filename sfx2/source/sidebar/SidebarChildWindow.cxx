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

#include <tools/mapunit.hxx>
#include <sfx2/chalign.hxx>
#include <sfx2/sidebar/TabBar.hxx>
#include <sfx2/bindings.hxx>
#include <sfx2/sidebar/SidebarChildWindow.hxx>
#include <sfx2/sidebar/SidebarDockingWindow.hxx>
#include <sfx2/sfxsids.hrc>
#include <helpids.h>
#include <comphelper/lok.hxx>

namespace sfx2::sidebar {

SFX_IMPL_DOCKINGWINDOW_WITHID(SidebarChildWindow, SID_SIDEBAR);

SidebarChildWindow::SidebarChildWindow(vcl::Window* pParentWindow, sal_uInt16 nId,
                                       SfxBindings& rBindings, SfxChildWinInfo& rInfo)
    : SfxChildWindow(pParentWindow, nId)
{
    auto pDockWin = VclPtr<SidebarDockingWindow>::Create(
        rBindings, *this, pParentWindow,
        WB_STDDOCKWIN | WB_OWNERDRAWDECORATION | WB_CLIPCHILDREN | WB_SIZEABLE | WB_3DLOOK);
    SetWindow(pDockWin);
    SetAlignment(SfxChildAlignment::RIGHT);

    pDockWin->SetHelpId(HID_SIDEBAR_WINDOW);
    pDockWin->SetOutputSizePixel(Size(GetDefaultWidth(pDockWin), 450));

    if (rInfo.aExtraString.isEmpty() && rInfo.aModule != "sdraw" && rInfo.aModule != "simpress"
        && rInfo.aModule != "smath")
    {
        // When this is the first start (never had the sidebar open yet),
        // default to non-expanded sidebars in Writer and Calc.
        //
        // HACK: unfortunately I haven't found a clean solution to do
        // this, so do it this way:
        //
        if (!comphelper::LibreOfficeKit::isActive())
        {
            pDockWin->SetSizePixel(
                Size(TabBar::GetDefaultWidth(),
                     pDockWin->GetSizePixel().Height()));
        }
    }

    pDockWin->Initialize(rInfo);

    if (comphelper::LibreOfficeKit::isActive())
    {
        // Undock sidebar in LOK to allow for resizing freely
        // (i.e. when the client window is resized) and collapse
        // it so the client can open it on demand.
        pDockWin->SetFloatingSize(Size(pDockWin->GetSizePixel().Width(),
                                       pDockWin->GetSizePixel().Height()));
        pDockWin->SetFloatingMode(true);
    }

    SetHideNotDelete(true);

    pDockWin->Show();
}

sal_Int32 SidebarChildWindow::GetDefaultWidth(vcl::Window const* pWindow)
{
    if (!pWindow)
        return 0;

    // Width of the paragraph panel (logical units).
    const static sal_Int32 nMaxPropertyPageWidth(146);

    auto aConvertedPos = pWindow->convertTo<vcl::WindowPoint>(
        vcl::LogicPoint(Point(nMaxPropertyPageWidth, 1)),
        MapMode(MapUnit::MapAppFont)
    );

    // Add the tab bar width (pixels) to the pixel X-coordinate.
    return aConvertedPos->X() + TabBar::GetDefaultWidth();
}

} // end of namespace sfx2::sidebar

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
