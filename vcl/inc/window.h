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
