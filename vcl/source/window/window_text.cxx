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

#include <vcl/vclevent.hxx>
#include <vcl/window.hxx>

#include <window.h>
#include <salframe.hxx>

namespace vcl
{
void Window::SetText(const OUString& rStr)
{
    if (!mpWindowImpl || rStr == mpWindowImpl->maText)
        return;

    OUString oldTitle(mpWindowImpl->maText);
    mpWindowImpl->maText = rStr;

    if (mpWindowImpl->mpBorderWindow)
        mpWindowImpl->mpBorderWindow->SetText(rStr);
    else if (mpWindowImpl->mbFrame)
        mpWindowImpl->mpFrame->SetTitle(rStr);

    CallEventListeners(VclEventId::WindowFrameTitleChanged, &oldTitle);

    // #107247# needed for accessibility
    // The VclEventId::WindowFrameTitleChanged is (mis)used to notify accessible name changes.
    // Therefore a window, which is labeled by this window, must also notify an accessible
    // name change.
    if (IsReallyVisible())
    {
        if (vcl::Window* pWindow = GetAccessibleRelationLabelFor(); pWindow && pWindow != this)
            pWindow->CallEventListeners(VclEventId::WindowFrameTitleChanged, &oldTitle);
    }

    CompatStateChanged(StateChangedType::Text);
}

OUString Window::GetText() const { return mpWindowImpl->maText; }

OUString Window::GetDisplayText() const { return GetText(); }

const OUString& Window::GetHelpText() const
{
    const OUString& rStrHelpId(GetHelpId());
    const bool bStrHelpId = !rStrHelpId.isEmpty();

    if (mpWindowImpl->mbHelpTextDynamic && bStrHelpId)
    {
        static const char* pEnv = getenv("HELP_DEBUG");
        if (pEnv && *pEnv)
        {
            mpWindowImpl->maHelpText
                = mpWindowImpl->maHelpText + "\n------------------\n" + rStrHelpId;
        }

        mpWindowImpl->mbHelpTextDynamic = false;
    }

    // Fallback to Window::GetAccessibleDescription without reentry to GetHelpText()
    if (mpWindowImpl->maHelpText.isEmpty() && mpWindowImpl->mpAccessibleInfos
        && mpWindowImpl->mpAccessibleInfos->pAccessibleDescription)
    {
        return *mpWindowImpl->mpAccessibleInfos->pAccessibleDescription;
    }

    return mpWindowImpl->maHelpText;
}
} // end vcl namespace

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
