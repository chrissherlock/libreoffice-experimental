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

#pragma once

#include <sal/config.h>

#include <tools/fract.hxx>
#include <vcl/commandevent.hxx>
#include <vcl/idle.hxx>
#include <vcl/inputctx.hxx>
#include <vcl/virdev.hxx>
#include <vcl/window.hxx>
#include <vcl/settings.hxx>
#include <o3tl/deleter.hxx>
#include <o3tl/typed_flags_set.hxx>
#include "windowdev.hxx"
#include "salwtype.hxx"

#include <optional>
#include <list>
#include <memory>
#include <vector>
#include <set>

class FixedText;
class VclSizeGroup;
class VirtualDevice;
namespace vcl::font { class PhysicalFontCollection; }
class ImplFontCache;
class VCLXWindow;
namespace vcl { class WindowData; }
class SalFrame;
class SalObject;
class DNDEventDispatcher;
class DNDListenerContainer;
enum class MouseEventModifiers;
enum class NotifyEventType;
enum class ActivateModeFlags;
enum class DialogControlFlags;
enum class GetFocusFlags;
enum class ParentClipMode;
enum class SalEvent;

namespace com::sun::star {
    namespace accessibility {
        class XAccessible;
        class XAccessibleContext;
        class XAccessibleEditableText;
    }

    namespace awt {
        class XVclWindowPeer;
        class XWindow;
    }
    namespace uno {
        class Any;
        class XInterface;
    }
    namespace datatransfer {
        namespace clipboard {
            class XClipboard;
        }
        namespace dnd {
            class XDropTargetListener;
            class XDragGestureRecognizer;
            class XDragSource;
            class XDropTarget;
        }
    }
}

VCL_DLLPUBLIC Size bestmaxFrameSizeForScreenSize(const Size &rScreenSize);

//return true if this window and its stack of containers are all shown
bool isVisibleInLayout(const vcl::Window *pWindow);

//return true if this window and its stack of containers are all enabled
bool isEnabledInLayout(const vcl::Window *pWindow);

bool ImplWindowFrameProc( vcl::Window* pInst, SalEvent nEvent, const void* pEvent );

MouseEventModifiers ImplGetMouseMoveMode( SalMouseEvent const * pEvent );

MouseEventModifiers ImplGetMouseButtonMode( SalMouseEvent const * pEvent );

struct ImplWinData
{
    std::optional<OUString>
                        mpExtOldText;
    std::unique_ptr<ExtTextInputAttr[]>
                        mpExtOldAttrAry;
    std::optional<tools::Rectangle>
                        mpCursorRect;
    tools::Long                mnCursorExtWidth;
    bool                mbVertical;
    std::unique_ptr<tools::Rectangle[]>
                        mpCompositionCharRects;
    tools::Long                mnCompositionCharRects;
    std::optional<tools::Rectangle>
                        mpFocusRect;
    std::optional<tools::Rectangle>
                        mpTrackRect;
    ShowTrackFlags      mnTrackFlags;
    sal_uInt16          mnIsTopWindow;
    bool                mbMouseOver;            //< tracks mouse over for native widget paint effect
    bool                mbEnableNativeWidget;   //< toggle native widget rendering
    ::std::list< VclPtr<vcl::Window> >
                        maTopWindowChildren;

     ImplWinData();
    ~ImplWinData();
};

struct ImplFrameData
{
    Idle                maPaintIdle;            //< paint idle handler
    Idle                maResizeIdle;          //< resize timer
    InputContext        maOldInputContext;      //< last set Input Context
    VclPtr<vcl::Window> mpNextFrame;            //< next frame window
    VclPtr<vcl::Window> mpFirstOverlap;         //< first overlap vcl::Window
    VclPtr<vcl::Window> mpFocusWin;             //< focus window (is also set, when frame doesn't have the focus)
    VclPtr<vcl::Window> mpMouseMoveWin;         //< last window, where MouseMove() called
    VclPtr<vcl::Window> mpMouseDownWin;         //< last window, where MouseButtonDown() called
    VclPtr<vcl::Window> mpTrackWin;             //< window, that is in tracking mode
    std::vector<VclPtr<vcl::Window> > maOwnerDrawList;    //< List of system windows with owner draw decoration
    std::shared_ptr<vcl::font::PhysicalFontCollection> mxFontCollection;   //< Font-List for this frame
    std::shared_ptr<ImplFontCache> mxFontCache; //< Font-Cache for this frame
    sal_Int32           mnDPIX;                 //< Original Screen Resolution
    sal_Int32           mnDPIY;                 //< Original Screen Resolution
    ImplSVEvent *       mnFocusId;              //< FocusId for PostUserLink
    ImplSVEvent *       mnMouseMoveId;          //< MoveId for PostUserLink
    tools::Long                mnLastMouseX;           //< last x mouse position
    tools::Long                mnLastMouseY;           //< last y mouse position
    tools::Long                mnBeforeLastMouseX;     //< last but one x mouse position
    tools::Long                mnBeforeLastMouseY;     //< last but one y mouse position
    tools::Long                mnFirstMouseX;          //< first x mouse position by mousebuttondown
    tools::Long                mnFirstMouseY;          //< first y mouse position by mousebuttondown
    tools::Long                mnLastMouseWinX;        //< last x mouse position, rel. to pMouseMoveWin
    tools::Long                mnLastMouseWinY;        //< last y mouse position, rel. to pMouseMoveWin
    sal_uInt16          mnModalMode;            //< frame based modal count (app based makes no sense anymore)
    sal_uInt64          mnMouseDownTime;        //< mouse button down time for double click
    sal_uInt16          mnClickCount;           //< mouse click count
    sal_uInt16          mnFirstMouseCode;       //< mouse code by mousebuttondown
    sal_uInt16          mnMouseCode;            //< mouse code
    MouseEventModifiers mnMouseMode;            //< mouse mode
    bool                mbHasFocus;             //< focus
    bool                mbInMouseMove;          //< is MouseMove on stack
    bool                mbMouseIn;              //> is Mouse inside the frame
    bool                mbStartDragCalled;      //< is command startdrag called
    bool                mbNeedSysWindow;        //< set, when FrameSize <= IMPL_MIN_NEEDSYSWIN
    bool                mbMinimized;            //< set, when FrameSize <= 0
    bool                mbStartFocusState;      //< FocusState, when sending the event
    bool                mbInSysObjFocusHdl;     //< within a SysChildren's GetFocus handler
    bool                mbInSysObjToTopHdl;     //< within a SysChildren's ToTop handler
    bool                mbSysObjFocus;          //< does a SysChild have focus
    sal_Int32           mnTouchPanPositionX;
    sal_Int32           mnTouchPanPositionY;

    css::uno::Reference< css::datatransfer::dnd::XDragSource > mxDragSource;
    css::uno::Reference< css::datatransfer::dnd::XDropTarget > mxDropTarget;
    rtl::Reference< DNDEventDispatcher > mxDropTargetListener; // css::datatransfer::dnd::XDropTargetListener
    css::uno::Reference< css::datatransfer::clipboard::XClipboard > mxClipboard;

    bool                mbInternalDragGestureRecognizer;
    bool                mbDragging;
    VclPtr<VirtualDevice> mpBuffer; ///< Buffer for the double-buffering
    bool mbInBufferedPaint; ///< PaintHelper is in the process of painting into this buffer.
    tools::Rectangle maBufferedRect; ///< Rectangle in the buffer that has to be painted to the screen.

    ImplFrameData( vcl::Window *pWindow );
};

struct ImplAccessibleInfos
{
    sal_uInt16          nAccessibleRole;
    std::optional<OUString>
                        pAccessibleName;
    std::optional<OUString>
                        pAccessibleDescription;
    rtl::Reference<comphelper::OAccessible> pAccessibleParent;
    VclPtr<vcl::Window> pLabeledByWindow;
    VclPtr<vcl::Window> pLabelForWindow;

    ImplAccessibleInfos();
    ~ImplAccessibleInfos();
};

enum class ImplPaintFlags {
    NONE             = 0x0000,
    Paint            = 0x0001,
    PaintAll         = 0x0002,
    PaintAllChildren = 0x0004,
    PaintChildren    = 0x0008,
    Erase            = 0x0010,
    CheckRtl         = 0x0020,
};
namespace o3tl {
    template<> struct typed_flags<ImplPaintFlags> : is_typed_flags<ImplPaintFlags, 0x003f> {};
}

struct WindowClippingState
{
    // Proper constructor ensuring no "garbage" memory patterns
    WindowClippingState()
        : meParentClipMode(ParentClipMode::NoClip)
        , mbInitWinClipRegion(true)
        , mbInitChildRegion(false)
        , mbClipSiblings(false)
        , mbClipChildren(false)
        , mbWinRegion(false)
    {
    }

    vcl::Region                  maWinClipRegion;
    std::unique_ptr<vcl::Region> mpChildClipRegion;
    ParentClipMode               meParentClipMode;
    bool                         mbInitWinClipRegion;
    bool                         mbInitChildRegion;
    bool                         mbClipSiblings;
    bool                         mbClipChildren;

    vcl::Region                  maWinRegion;
    bool                         mbWinRegion;
};

struct WindowHierarchy
{
    VclPtr<vcl::Window> mpParent;            // Parent (includes BorderWindow)
    VclPtr<vcl::Window> mpRealParent;        // Real parent (excludes BorderWindow)

    VclPtr<vcl::Window> mpFirstChild;        // First child window
    VclPtr<vcl::Window> mpLastChild;         // Last child window

    VclPtr<vcl::Window> mpFirstOverlap;      // First overlap window child
    VclPtr<vcl::Window> mpLastOverlap;       // Last overlap window child

    VclPtr<vcl::Window> mpPrev;              // Previous sibling window
    VclPtr<vcl::Window> mpNext;              // Next sibling window

    VclPtr<vcl::Window> mpPrevOverlap;       // Previous overlap window of frame
    VclPtr<vcl::Window> mpNextOverlap;       // Next overlap window of frame
};

namespace vcl
{
/// Sets up the buffer to have settings matching the window, and restores the original state in the dtor.
class VCL_DLLPUBLIC PaintBufferGuard
{
    ImplFrameData* mpFrameData;
    VclPtr<vcl::Window> m_pWindow;
    bool mbBackground;
    Wallpaper maBackground;
    AllSettings maSettings;
    tools::Long mnOutOffX;
    tools::Long mnOutOffY;
    tools::Rectangle m_aPaintRect;
public:
    PaintBufferGuard(ImplFrameData* pFrameData, vcl::Window* pWindow);
    ~PaintBufferGuard();
    /// If this is called, then the dtor will also copy rRectangle to the window from the buffer, before restoring the state.
    void SetPaintRect(const tools::Rectangle& rRectangle);
    /// Returns either the frame's buffer or the window, in case of no buffering.
    vcl::RenderContext* GetRenderContext();
};
typedef std::unique_ptr<PaintBufferGuard, o3tl::default_delete<PaintBufferGuard>> PaintBufferGuardPtr;
}

// helper methods

bool ImplHandleMouseEvent( const VclPtr<vcl::Window>& xWindow, NotifyEventType nSVEvent, bool bMouseLeave,
                           Point aMousePos, sal_uInt64 nMsgTime, sal_uInt16 nCode, MouseEventModifiers nMode );

bool ImplLOKHandleMouseEvent( const VclPtr<vcl::Window>& xWindow, NotifyEventType nSVEvent, bool bMouseLeave,
                              tools::Long nX, tools::Long nY, sal_uInt64 nMsgTime,
                              sal_uInt16 nCode, MouseEventModifiers nMode, sal_uInt16 nClicks);

void ImplHandleResize( vcl::Window* pWindow, tools::Long nNewWidth, tools::Long nNewHeight );

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
