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

#include <vcl/IDialogRenderable.hxx>
#include <vcl/window.hxx>

#include <window.h>

#include <cassert>
#include <map>

typedef std::map<vcl::LOKWindowId, VclPtr<vcl::Window>> LOKWindowsMap;

namespace
{
LOKWindowsMap& GetLOKWindowsMap()
{
    // Map to remember the LOKWindowId <-> Window binding.
    static LOKWindowsMap s_aLOKWindowsMap;

    return s_aLOKWindowsMap;
}
}

// Counter to be able to have unique id's for each window.
static vcl::LOKWindowId sLastLOKWindowId = 1;

namespace vcl
{
void Window::SetLOKNotifier(const vcl::ILibreOfficeKitNotifier* pNotifier, bool bParent)
{
    // don't allow setting this twice
    assert(mpWindowImpl->mpLOKNotifier == nullptr);
    assert(pNotifier);
    // never use this in the desktop case
    assert(comphelper::LibreOfficeKit::isActive());

    if (!bParent)
    {
        // assign the LOK window id
        assert(mpWindowImpl->mnLOKWindowId == 0);
        mpWindowImpl->mnLOKWindowId = sLastLOKWindowId++;
        GetLOKWindowsMap().emplace(mpWindowImpl->mnLOKWindowId, this);
    }

    mpWindowImpl->mpLOKNotifier = pNotifier;
}

void Window::SetLOKWindowId()
{
    // never use this in the desktop case
    assert(comphelper::LibreOfficeKit::isActive());

    // assign the LOK window id
    assert(mpWindowImpl->mnLOKWindowId == 0);
    mpWindowImpl->mnLOKWindowId = sLastLOKWindowId++;
    GetLOKWindowsMap().emplace(mpWindowImpl->mnLOKWindowId, this);
}

VclPtr<Window> Window::FindLOKWindow(vcl::LOKWindowId nWindowId)
{
    const auto it = GetLOKWindowsMap().find(nWindowId);
    if (it != GetLOKWindowsMap().end())
        return it->second;

    return VclPtr<Window>();
}

bool Window::IsLOKWindowsEmpty() { return GetLOKWindowsMap().empty(); }

void Window::ReleaseLOKNotifier()
{
    // unregister the LOK window binding
    if (mpWindowImpl->mnLOKWindowId > 0)
        GetLOKWindowsMap().erase(mpWindowImpl->mnLOKWindowId);

    mpWindowImpl->mpLOKNotifier = nullptr;
    mpWindowImpl->mnLOKWindowId = 0;
}

ILibreOfficeKitNotifier::~ILibreOfficeKitNotifier()
{
    if (!comphelper::LibreOfficeKit::isActive())
    {
        return;
    }

    for (auto it = GetLOKWindowsMap().begin(); it != GetLOKWindowsMap().end();)
    {
        WindowImpl* pWindowImpl = it->second->ImplGetWindowImpl();
        if (pWindowImpl && pWindowImpl->mpLOKNotifier == this)
        {
            pWindowImpl->mpLOKNotifier = nullptr;
            pWindowImpl->mnLOKWindowId = 0;
            it = GetLOKWindowsMap().erase(it);
            continue;
        }

        ++it;
    }
}

const vcl::ILibreOfficeKitNotifier* Window::GetLOKNotifier() const
{
    return mpWindowImpl ? mpWindowImpl->mpLOKNotifier : nullptr;
}

vcl::LOKWindowId Window::GetLOKWindowId() const
{
    return mpWindowImpl ? mpWindowImpl->mnLOKWindowId : 0;
}

VclPtr<vcl::Window> Window::GetParentWithLOKNotifier()
{
    VclPtr<vcl::Window> pWindow(this);

    while (pWindow && !pWindow->GetLOKNotifier())
        pWindow = pWindow->GetParent();

    return pWindow;
}
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
