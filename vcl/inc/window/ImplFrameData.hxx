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

#include <vcl/event.hxx>
#include <vcl/idle.hxx>
#include <vcl/inputctx.hxx>

#include <window/dndeventdispatcher.hxx>

struct ImplFrameData
{
    Idle maPaintIdle; //< paint idle handler
    Idle maResizeIdle; //< resize timer
    InputContext maOldInputContext; //< last set Input Context
    VclPtr<vcl::Window> mpNextFrame; //< next frame window
    VclPtr<vcl::Window> mpFirstOverlap; //< first overlap vcl::Window
    VclPtr<vcl::Window>
        mpFocusWin; //< focus window (is also set, when frame doesn't have the focus)
    VclPtr<vcl::Window> mpMouseMoveWin; //< last window, where MouseMove() called
    VclPtr<vcl::Window> mpMouseDownWin; //< last window, where MouseButtonDown() called
    VclPtr<vcl::Window> mpTrackWin; //< window, that is in tracking mode
    std::vector<VclPtr<vcl::Window>>
        maOwnerDrawList; //< List of system windows with owner draw decoration
    std::shared_ptr<vcl::font::PhysicalFontCollection>
        mxFontCollection; //< Font-List for this frame
    std::shared_ptr<ImplFontCache> mxFontCache; //< Font-Cache for this frame
    sal_Int32 mnDPIX; //< Original Screen Resolution
    sal_Int32 mnDPIY; //< Original Screen Resolution
    ImplSVEvent* mnFocusId; //< FocusId for PostUserLink
    ImplSVEvent* mnMouseMoveId; //< MoveId for PostUserLink
    tools::Long mnLastMouseX; //< last x mouse position
    tools::Long mnLastMouseY; //< last y mouse position
    tools::Long mnBeforeLastMouseX; //< last but one x mouse position
    tools::Long mnBeforeLastMouseY; //< last but one y mouse position
    tools::Long mnFirstMouseX; //< first x mouse position by mousebuttondown
    tools::Long mnFirstMouseY; //< first y mouse position by mousebuttondown
    tools::Long mnLastMouseWinX; //< last x mouse position, rel. to pMouseMoveWin
    tools::Long mnLastMouseWinY; //< last y mouse position, rel. to pMouseMoveWin
    sal_uInt16 mnModalMode; //< frame based modal count (app based makes no sense anymore)
    sal_uInt64 mnMouseDownTime; //< mouse button down time for double click
    sal_uInt16 mnClickCount; //< mouse click count
    sal_uInt16 mnFirstMouseCode; //< mouse code by mousebuttondown
    sal_uInt16 mnMouseCode; //< mouse code
    MouseEventModifiers mnMouseMode; //< mouse mode
    bool mbHasFocus; //< focus
    bool mbInMouseMove; //< is MouseMove on stack
    bool mbMouseIn; //> is Mouse inside the frame
    bool mbStartDragCalled; //< is command startdrag called
    bool mbNeedSysWindow; //< set, when FrameSize <= IMPL_MIN_NEEDSYSWIN
    bool mbMinimized; //< set, when FrameSize <= 0
    bool mbStartFocusState; //< FocusState, when sending the event
    bool mbInSysObjFocusHdl; //< within a SysChildren's GetFocus handler
    bool mbInSysObjToTopHdl; //< within a SysChildren's ToTop handler
    bool mbSysObjFocus; //< does a SysChild have focus
    sal_Int32 mnTouchPanPositionX;
    sal_Int32 mnTouchPanPositionY;

    css::uno::Reference<css::datatransfer::dnd::XDragSource> mxDragSource;
    css::uno::Reference<css::datatransfer::dnd::XDropTarget> mxDropTarget;
    rtl::Reference<DNDEventDispatcher>
        mxDropTargetListener; // css::datatransfer::dnd::XDropTargetListener
    css::uno::Reference<css::datatransfer::clipboard::XClipboard> mxClipboard;

    bool mbInternalDragGestureRecognizer;
    bool mbDragging;
    VclPtr<VirtualDevice> mpBuffer; ///< Buffer for the double-buffering
    bool mbInBufferedPaint; ///< PaintHelper is in the process of painting into this buffer.
    tools::Rectangle
        maBufferedRect; ///< Rectangle in the buffer that has to be painted to the screen.

    ImplFrameData(vcl::Window* pWindow);
};

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
