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

#include <sal/config.h>

#include <rtl/strbuf.hxx>
#include <sal/log.hxx>

#include <sal/types.h>
#include <comphelper/diagnose_ex.hxx>
#include <comphelper/OAccessible.hxx>
#include <vcl/dndlistenercontainer.hxx>
#include <vcl/salgtype.hxx>
#include <vcl/event.hxx>
#include <vcl/cursor.hxx>
#include <vcl/rendercontext/AntialiasingFlags.hxx>
#include <vcl/rendercontext/GetDefaultFontFlags.hxx>
#include <vcl/svapp.hxx>
#include <vcl/transfer.hxx>
#include <vcl/vclevent.hxx>
#include <vcl/window.hxx>
#include <vcl/syswin.hxx>
#include <vcl/dockwin.hxx>
#include <vcl/wall.hxx>
#include <vcl/toolkit/fixed.hxx>
#include <vcl/toolkit/button.hxx>
#include <vcl/taskpanelist.hxx>
#include <vcl/toolkit/unowrap.hxx>
#include <tools/lazydelete.hxx>
#include <vcl/virdev.hxx>
#include <vcl/settings.hxx>
#include <vcl/sysdata.hxx>
#include <vcl/ptrstyle.hxx>
#include <vcl/IDialogRenderable.hxx>
#include <vcl/uitest/uiobject.hxx>
#include <vcl/CoordinateMapper.hxx>
#include <vcl/MappingPolicy.hxx>

#include <ImplOutDevData.hxx>
#include <impfontcache.hxx>
#include <salframe.hxx>
#include <salobj.hxx>
#include <salinst.hxx>
#include <salgdi.hxx>
#include <svdata.hxx>
#include <window.h>
#include <toolbox.h>
#include <brdwin.hxx>
#include <helpwin.hxx>
#include <dndeventdispatcher.hxx>

#include <com/sun/star/accessibility/AccessibleRelation.hpp>
#include <com/sun/star/accessibility/AccessibleRole.hpp>
#include <com/sun/star/accessibility/AccessibleStateType.hpp>
#include <com/sun/star/accessibility/XAccessible.hpp>
#include <com/sun/star/accessibility/XAccessibleEditableText.hpp>
#include <com/sun/star/awt/XVclWindowPeer.hpp>
#include <com/sun/star/datatransfer/clipboard/XClipboard.hpp>
#include <com/sun/star/datatransfer/dnd/XDragGestureRecognizer.hpp>
#include <com/sun/star/datatransfer/dnd/XDropTarget.hpp>
#include <com/sun/star/rendering/CanvasFactory.hpp>
#include <com/sun/star/rendering/XSpriteCanvas.hpp>
#include <comphelper/configuration.hxx>
#include <comphelper/lok.hxx>
#include <comphelper/processfactory.hxx>
#include <osl/diagnose.h>
#include <tools/debug.hxx>
#include <tools/json_writer.hxx>
#include <unotools/fontdefs.hxx>
#include <boost/property_tree/ptree.hpp>

#include <cassert>
#include <typeinfo>

#ifdef _WIN32 // see #140456#
#include <win/salframe.h>
#endif

#include "impldockingwrapper.hxx"

ImplFrameData::ImplFrameData( vcl::Window *pWindow )
    : maPaintIdle( "vcl::Window maPaintIdle" ),
      maResizeIdle( "vcl::Window maResizeIdle" )
{
    ImplSVData* pSVData = ImplGetSVData();
    assert (pSVData->maFrameData.mpFirstFrame.get() != pWindow);
    mpNextFrame        = pSVData->maFrameData.mpFirstFrame;
    pSVData->maFrameData.mpFirstFrame = pWindow;
    mpFirstOverlap     = nullptr;
    mpFocusWin         = nullptr;
    mpMouseMoveWin     = nullptr;
    mpMouseDownWin     = nullptr;
    mpTrackWin         = nullptr;
    mxFontCollection   = pSVData->maGDIData.mxScreenFontList;
    mxFontCache        = pSVData->maGDIData.mxScreenFontCache;
    mnFocusId          = nullptr;
    mnMouseMoveId      = nullptr;
    mnLastMouseX       = -32767;
    mnLastMouseY       = -32767;
    mnBeforeLastMouseX = -32767;
    mnBeforeLastMouseY = -32767;
    mnFirstMouseX      = -32767;
    mnFirstMouseY      = -32767;
    mnLastMouseWinX    = -32767;
    mnLastMouseWinY    = -32767;
    mnModalMode        = 0;
    mnMouseDownTime    = 0;
    mnClickCount       = 0;
    mnFirstMouseCode   = 0;
    mnMouseCode        = 0;
    mnMouseMode        = MouseEventModifiers::NONE;
    mbHasFocus         = false;
    mbInMouseMove      = false;
    mbMouseIn          = false;
    mbStartDragCalled  = false;
    mbNeedSysWindow    = false;
    mbMinimized        = false;
    mbStartFocusState  = false;
    mbInSysObjFocusHdl = false;
    mbInSysObjToTopHdl = false;
    mbSysObjFocus      = false;
    maPaintIdle.SetPriority( TaskPriority::REPAINT );
    maPaintIdle.SetInvokeHandler( LINK( pWindow, vcl::Window, ImplHandlePaintHdl ) );
    maResizeIdle.SetPriority( TaskPriority::RESIZE );
    maResizeIdle.SetInvokeHandler( LINK( pWindow, vcl::Window, ImplHandleResizeTimerHdl ) );
    mbInternalDragGestureRecognizer = false;
    mbDragging = false;
    mbInBufferedPaint = false;
    mnDPIX = 96;
    mnDPIY = 96;
    mnTouchPanPositionX = -1;
    mnTouchPanPositionY = -1;
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
