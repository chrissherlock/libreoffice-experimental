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

using namespace ::com::sun::star::uno;
using namespace ::com::sun::star::lang;
using namespace ::com::sun::star::datatransfer::clipboard;
using namespace ::com::sun::star::datatransfer::dnd;

WindowImpl::WindowImpl( vcl::Window& rWindow, WindowType eType )
{
    mpClippingState = std::make_unique<WindowClippingState>();
    mpHierarchy     = std::make_unique<WindowHierarchy>();

    mxOutDev = VclPtr<vcl::WindowOutputDevice>::Create(rWindow);
    mfZoom                              = 1.0;
    mfPartialScrollX                    = 0.0;
    mfPartialScrollY                    = 0.0;
    mpClippingState->maWinRegion        = vcl::Region(true);
    mpClippingState->maWinClipRegion    = vcl::Region(true);
    mpWinData                           = nullptr;                      // Extra Window Data, that we don't need for all windows
    mpFrameData                         = nullptr;                      // Frame Data
    mpFrame                             = nullptr;                      // Pointer to frame window
    mpSysObj                            = nullptr;
    mpFrameWindow                       = nullptr;                      // window to top level parent (same as frame window)
    mpOverlapWindow                     = nullptr;                      // first overlap parent
    mpBorderWindow                      = nullptr;                      // Border-Window
    mpClientWindow                      = nullptr;                      // Client-Window of a FrameWindow
    mpLastFocusWindow                   = nullptr;                      // window for focus restore
    mpDlgCtrlDownWindow                 = nullptr;                      // window for dialog control
    mnEventListenersIteratingCount = 0;
    mnChildEventListenersIteratingCount = 0;
    mpCursor                            = nullptr;                      // cursor
    maPointer                           = PointerStyle::Arrow;
    mpVCLXWindow                        = nullptr;
    mpAccessibleInfos                   = nullptr;
    maControlForeground                 = COL_TRANSPARENT;  // no foreground set
    maControlBackground                 = COL_TRANSPARENT;  // no background set
    mnLeftBorder                        = 0;                         // width of left border
    mnTopBorder                         = 0;                         // width of top border
    mnRightBorder                       = 0;                         // width of right border
    mnBottomBorder                      = 0;                         // width of bottom border
    mnWidthRequest                      = -1;                        // width request
    mnHeightRequest                     = -1;                        // height request
    mnOptimalWidthCache                 = -1;                        // optimal width cache
    mnOptimalHeightCache                = -1;                        // optimal height cache
    mnX                                 = 0;                         // X-Position to Parent
    mnY                                 = 0;                         // Y-Position to Parent
    mnAbsScreenX                        = 0;                         // absolute X-position on screen, used for RTL window positioning
    mpClippingState->mpChildClipRegion  = nullptr;                      // Child-Clip-Region when ClipChildren
    mpPaintRegion                       = nullptr;                      // Paint-ClipRegion
    mnStyle                             = 0;                         // style (init in ImplInitWindow)
    mnPrevStyle                         = 0;                         // prevstyle (set in SetStyle)
    mnExtendedStyle                     = WindowExtendedStyle::NONE; // extended style (init in ImplInitWindow)
    meType                              = eType;                     // type
    mnGetFocusFlags                     = GetFocusFlags::NONE;       // Flags for GetFocus()-Call
    mnWaitCount                         = 0;                         // Wait-Count (>1 == "wait" mouse pointer)
    mnPaintFlags                        = ImplPaintFlags::NONE;      // Flags for ImplCallPaint
    mnActivateMode                      = ActivateModeFlags::NONE;   // Will be converted in System/Overlap-Windows
    mnDlgCtrlFlags                      = DialogControlFlags::NONE;  // DialogControl-Flags
    meAlwaysInputMode                   = AlwaysInputNone;           // AlwaysEnableInput not called
    meHalign                            = VclAlign::Fill;
    meValign                            = VclAlign::Fill;
    mePackType                          = VclPackType::Start;
    mnPadding                           = 0;
    mnGridHeight                        = 1;
    mnGridLeftAttach                    = -1;
    mnGridTopAttach                     = -1;
    mnGridWidth                         = 1;
    mnBorderWidth                       = 0;
    mnMarginLeft                        = 0;
    mnMarginRight                       = 0;
    mnMarginTop                         = 0;
    mnMarginBottom                      = 0;
    mbFrame                             = false;                     // true: Window is a frame window
    mbBorderWin                         = false;                     // true: Window is a border window
    mbOverlapWin                        = false;                     // true: Window is an overlap window
    mbSysWin                            = false;                     // true: SystemWindow is the base class
    mbDialog                            = false;                     // true: Dialog is the base class
    mbDockWin                           = false;                     // true: DockingWindow is the base class
    mbFloatWin                          = false;                     // true: FloatingWindow is the base class
    mbPushButton                        = false;                     // true: PushButton is the base class
    mbToolBox                           = false;                     // true: ToolBox is the base class
    mbMenuFloatingWindow                = false;                     // true: MenuFloatingWindow is the base class
    mbSplitter                          = false;                     // true: Splitter is the base class
    mbVisible                           = false;                     // true: Show( true ) called
    mbOverlapVisible                    = false;                     // true: Hide called for visible window from ImplHideAllOverlapWindow()
    mbDisabled                          = false;                     // true: Enable( false ) called
    mbInputDisabled                     = false;                     // true: EnableInput( false ) called
    mbNoUpdate                          = false;                     // true: SetUpdateMode( false ) called
    mbNoParentUpdate                    = false;                     // true: SetParentUpdateMode( false ) called
    mbActive                            = false;                     // true: Window Active
    mbReallyVisible                     = false;                     // true: this and all parents to an overlapped window are visible
    mbReallyShown                       = false;                     // true: this and all parents to an overlapped window are shown
    mbInInitShow                        = false;                     // true: we are in InitShow
    mbChildPtrOverwrite                 = false;                     // true: PointerStyle overwrites Child-Pointer
    mbNoPtrVisible                      = false;                     // true: ShowPointer( false ) called
    mbPaintFrame                        = false;                     // true: Paint is visible, but not painted
    mbInPaint                           = false;                     // true: Inside PaintHdl
    mbMouseButtonDown                   = false;                     // true: BaseMouseButtonDown called
    mbMouseButtonUp                     = false;                     // true: BaseMouseButtonUp called
    mbKeyInput                          = false;                     // true: BaseKeyInput called
    mbKeyUp                             = false;                     // true: BaseKeyUp called
    mbCommand                           = false;                     // true: BaseCommand called
    mbDefPos                            = true;                      // true: Position is not Set
    mbDefSize                           = true;                      // true: Size is not Set
    mbCallMove                          = true;                      // true: Move must be called by Show
    mbCallResize                        = true;                      // true: Resize must be called by Show
    mbWaitSystemResize                  = true;                      // true: Wait for System-Resize
    mbChildTransparent                  = false;                     // true: Child-windows are allowed to switch to transparent (incl. Parent-CLIPCHILDREN)
    mbPaintTransparent                  = false;                     // true: Paints should be executed on the Parent
    mbMouseTransparent                  = false;                     // true: Window is transparent for Mouse
    mbDlgCtrlStart                      = false;                     // true: From here on own Dialog-Control
    mbFocusVisible                      = false;                     // true: Focus Visible
    mbUseNativeFocus                    = false;
    mbNativeFocusVisible                = false;                     // true: native Focus Visible
    mbInShowFocus                       = false;                     // prevent recursion
    mbInHideFocus                       = false;                     // prevent recursion
    mbTrackVisible                      = false;                     // true: Tracking Visible
    mbControlForeground                 = false;                     // true: Foreground-Property set
    mbControlBackground                 = false;                     // true: Background-Property set
    mbAlwaysOnTop                       = false;                     // true: always visible for all others windows
    mbCompoundControl                   = false;                     // true: Composite Control => Listener...
    mbCompoundControlHasFocus           = false;                     // true: Composite Control has focus somewhere
    mbPaintDisabled                     = false;                     // true: Paint should not be executed
    mbAllResize                         = false;                     // true: Also sent ResizeEvents with 0,0
    mbInDispose                         = false;                     // true: We're still in Window::dispose()
    mbExtTextInput                      = false;                     // true: ExtTextInput-Mode is active
    mbInFocusHdl                        = false;                     // true: Within GetFocus-Handler
    mbCreatedWithToolkit                = false;
    mbSuppressAccessibilityEvents       = false;                     // true: do not send any accessibility events
    mbDrawSelectionBackground           = false;                     // true: draws transparent window background to indicate (toolbox) selection
    mbIsInTaskPaneList                  = false;                     // true: window was added to the taskpanelist in the topmost system window
    mnNativeBackground                  = ControlPart::NONE;         // initialize later, depends on type
    mbHelpTextDynamic                   = false;                     // true: append help id in HELP_DEBUG case
    mbFakeFocusSet                      = false;                     // true: pretend as if the window has focus.
    mbHexpand                           = false;
    mbVexpand                           = false;
    mbExpand                            = false;
    mbFill                              = true;
    mbSecondary                         = false;
    mbNonHomogeneous                    = false;
    static bool bDoubleBuffer = getenv("VCL_DOUBLEBUFFERING_FORCE_ENABLE");
    mbDoubleBufferingRequested = bDoubleBuffer; // when we are not sure, assume it cannot do double-buffering via RenderContext
    mpLOKNotifier                       = nullptr;
    mnLOKWindowId                       = 0;
    mbUseFrameData                      = false;
}

WindowImpl::~WindowImpl()
{
    mpClippingState->mpChildClipRegion.reset();
    mpAccessibleInfos.reset();
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
