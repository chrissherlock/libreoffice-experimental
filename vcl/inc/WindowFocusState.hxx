/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <vcl/focus.hxx>

struct WindowFocusState
{
    GetFocusFlags mnGetFocusFlags = GetFocusFlags::NONE;
    ActivateModeFlags mnActivateMode = ActivateModeFlags::NONE;

    bool mbActive : 1 = false;
    bool mbFocusVisible : 1 = false;
    bool mbUseNativeFocus : 1 = false;
    bool mbNativeFocusVisible : 1 = false;
    bool mbInShowFocus : 1 = false;
    bool mbInHideFocus : 1 = false;
    bool mbCompoundControl : 1 = false;
    bool mbCompoundControlHasFocus : 1 = false;

    WindowFocusState();
    ~WindowFocusState();
};

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
