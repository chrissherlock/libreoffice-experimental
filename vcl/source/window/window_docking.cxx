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

#include <vcl/dockwin.hxx>
#include <vcl/vclptr.hxx>
#include <vcl/window.hxx>

#include <ImplFrameData.hxx>
#include <WindowImpl.hxx>
#include <svdata.hxx>

#include <vector>

namespace vcl
{
DockingManager* Window::GetDockingManager() { return ImplGetDockingManager(); }

void Window::EnableDocking(bool bEnable)
{
    // update list of dockable windows
    if (bEnable)
        ImplGetDockingManager()->AddWindow(this);
    else
        ImplGetDockingManager()->RemoveWindow(this);
}

// retrieves the list of owner draw decorated windows for this window hierarchy
::std::vector<VclPtr<vcl::Window>>& Window::ImplGetOwnerDrawList()
{
    return ImplGetTopmostFrameWindow()->mpWindowImpl->mpFrameData->maOwnerDrawList;
}

bool Window::IsDockingWindow() const { return mpWindowImpl && mpWindowImpl->mbDockWin; }
} // end vcl namespace

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
