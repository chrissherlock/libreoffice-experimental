/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <vcl/dllapi.h>
#include <vcl/inputctx.hxx>
#include <vcl/vclptr.hxx>
#include <vcl/wintypes.hxx>

namespace vcl
{
class Window;
}

class PushButton;

enum AlwaysInputMode
{
    AlwaysInputNone = 0,
    AlwaysInputEnabled = 1
};

struct WindowInput
{
    VclPtr<vcl::Window> mpLastFocusWindow;
    VclPtr<PushButton> mpDlgCtrlDownWindow;
    InputContext maInputContext;
    AlwaysInputMode meAlwaysInputMode = AlwaysInputNone;

    bool mbKeyInput : 1 = false;
    bool mbKeyUp : 1 = false;
    bool mbMouseButtonDown : 1 = false;
    bool mbMouseButtonUp : 1 = false;
    bool mbCommand : 1 = false;
    bool mbExtTextInput : 1 = false;
    bool mbInFocusHdl : 1 = false;
    bool mbFakeFocusSet : 1 = false;

    WindowInput();
    ~WindowInput();
};

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
