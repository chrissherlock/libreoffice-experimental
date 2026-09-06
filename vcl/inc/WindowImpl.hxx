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

#pragma once

#include <tools/color.hxx>
#include <tools/gen.hxx>
#include <tools/link.hxx>

#include <vcl/inputctx.hxx>
#include <vcl/ptrstyle.hxx>
#include <vcl/region.hxx>
#include <vcl/vclptr.hxx>
#include <vcl/wintypes.hxx>
#include <vcl/window.hxx>

#include <window.h>
#include <WindowHelpData.hxx>

#include <set>

class CommandEvent;
class FixedText;
struct ImplAccessibleInfos;
struct ImplFrameData;
class PushButton;
class SalObject;
class SalFrame;
class VclSizeGroup;
class VclWindowEvent;
class VCLXWindow;
struct WindowClippingState;
struct WindowHierarchy;

namespace vcl
{
class Cursor;
class Window;
class WindowOutputDevice;
}

class WindowImpl
{
private:
    WindowImpl(const WindowImpl&) = delete;
    WindowImpl& operator=(const WindowImpl&) = delete;
public:
    WindowImpl( WindowType );
    ~WindowImpl();

    ImplFrameData*      mpFrameData;
    SalFrame*           mpFrame;
    SalObject*          mpSysObj;

    OUString            maText;
    VCLXWindow*         mpVCLXWindow;

    WinBits             mnStyle;
    WinBits             mnPrevStyle;
    WindowExtendedStyle mnExtendedStyle;
    WindowType          meType;
    sal_uInt16          mnWaitCount;
    DialogControlFlags  mnDlgCtrlFlags;
    bool                mbFrame:1,
                        mbBorderWin:1,
                        mbOverlapWin:1,
                        mbSysWin:1,
                        mbDialog:1,
                        mbDockWin:1,
                        mbFloatWin:1,
                        mbPushButton:1,
                        mbNoUpdate:1,
                        mbNoParentUpdate:1,
                        mbDefPos:1,
                        mbDefSize:1,
                        mbCallMove:1,
                        mbCallResize:1,
                        mbWaitSystemResize:1,
                        mbChildTransparent:1,
                        mbDlgCtrlStart:1,
                        mbTrackVisible:1,
                        mbAlwaysOnTop:1,
                        mbAllResize:1,
                        mbInDispose:1,
                        mbCreatedWithToolkit:1,
                        mbToolBox:1,
                        mbSplitter:1,
                        mbMenuFloatingWindow:1,
                        mbDrawSelectionBackground:1,
                        mbIsInTaskPaneList:1,
                        mbDoubleBufferingRequested:1;
    bool mbIsFormControl : 1 = false;
};

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
