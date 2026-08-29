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

#include <vcl/window.hxx>

#include <window.h>
#include <salframe.hxx>

namespace vcl
{
void Window::SetModalHierarchyHdl(const Link<bool, void>& rLink)
{
    ImplGetFrame()->SetModalHierarchyHdl(rLink);
}

// frame based modal counter (dialogs are not modal to the whole application anymore)
bool Window::IsInModalMode() const
{
    return (mpWindowImpl->mpFrameWindow->mpWindowImpl->mpFrameData->mnModalMode != 0);
}

void Window::ImplUpdateModalCount(int nDelta)
{
    vcl::Window* pFrameWindow = mpWindowImpl->mpFrameWindow;
    vcl::Window* pParent = pFrameWindow;
    while (pFrameWindow)
    {
        pFrameWindow->mpWindowImpl->mpFrameData->mnModalMode += nDelta;
        while (pParent && pParent->mpWindowImpl->mpFrameWindow == pFrameWindow)
        {
            pParent = pParent->GetParent();
        }
        pFrameWindow = pParent ? pParent->mpWindowImpl->mpFrameWindow.get() : nullptr;
    }
}

void Window::IncModalCount() { ImplUpdateModalCount(1); }

void Window::DecModalCount() { ImplUpdateModalCount(-1); }

} // end vcl namespace

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
