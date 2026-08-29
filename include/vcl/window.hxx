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

#include <vcl/dllapi.h>
#include <vcl/outdev.hxx>
#include <vcl/MappingPolicy.hxx>
#include <tools/link.hxx>
#include <vcl/wintypes.hxx>
#include <vcl/vclenum.hxx>
#include <vcl/uitest/factory.hxx>
#include <vcl/IDialogRenderable.hxx>
#include <rtl/ustring.hxx>
#include <vcl/commandevent.hxx>

#include <memory>

enum class SalFrameStyleFlags;
enum class BorderWindowStyle;
struct ImplSVEvent;
struct ImplWinData;
struct ImplFrameData;
struct ImplCalcToTopData;
struct SystemEnvData;
struct SystemParentData;
class ImplBorderWindow;
class Timer;
class DNDListenerContainer;
class DockingManager;
class Scrollable;
class FixedText;
class MouseEvent;
class KeyEvent;
class CommandEvent;
class TrackingEvent;
class HelpEvent;
class DataChangedEvent;
class NotifyEvent;
class SystemWindow;
class SalFrame;
class MenuFloatingWindow;
class VCLXWindow;
class VclWindowEvent;
class AllSettings;
class InputContext;
class VclEventListeners;
class EditView;
enum class ImplPaintFlags;
enum class KeyIndicatorState;
enum class VclEventId;
enum class PointerStyle;

namespace com::sun::star
{
namespace awt
{
class XVclWindowPeer;
}
namespace datatransfer::clipboard
{
class XClipboard;
}
namespace datatransfer::dnd
{
class XDragGestureRecognizer;
class XDragSource;
class XDropTarget;
}
}

namespace comphelper
{
class OAccessible;
}

namespace rtl
{
template <class reference_type> class Reference;
}

namespace vcl
{
class Region;
struct ControlLayoutData;
}

namespace svt
{
class PopupWindowControllerImpl;
}

namespace weld
{
class Window;
}

template <class T> class VclPtr;

// Type for GetWindow()
enum class GetWindowType
{
    Parent = 0,
    FirstChild = 1,
    LastChild = 2,
    Prev = 3,
    Next = 4,
    FirstOverlap = 5,
    Overlap = 7,
    ParentOverlap = 8,
    Client = 9,
    RealParent = 10,
    Frame = 11,
    Border = 12,
    FirstTopWindowChild = 13,
    NextTopWindowSibling = 16,
};

// Flags for setPosSizePixel()
// These must match the definitions in css::awt::PosSize
enum class PosSizeFlags
{
    NONE = 0x0000,
    X = 0x0001,
    Y = 0x0002,
    Width = 0x0004,
    Height = 0x0008,
    Pos = X | Y,
    Size = Width | Height,
    PosSize = Pos | Size,
    All = PosSize,
};

namespace o3tl
{
template <> struct typed_flags<PosSizeFlags> : is_typed_flags<PosSizeFlags, 0x000f>
{
};
}

// Flags for SetZOrder()
enum class ZOrderFlags
{
    NONE = 0x0000,
    Before = 0x0001,
    Behind = 0x0002,
    First = 0x0004,
    Last = 0x0008,
};
namespace o3tl
{
template <> struct typed_flags<ZOrderFlags> : is_typed_flags<ZOrderFlags, 0x000f>
{
};
}

// Activate-Flags
enum class ActivateModeFlags
{
    NONE = 0,
    GrabFocus = 0x0001,
};
namespace o3tl
{
template <> struct typed_flags<ActivateModeFlags> : is_typed_flags<ActivateModeFlags, 0x0001>
{
};
}

// ToTop-Flags
enum class ToTopFlags
{
    NONE = 0x0000,
    RestoreWhenMin = 0x0001,
    ForegroundTask = 0x0002,
    NoGrabFocus = 0x0004,
    GrabFocusOnly = 0x0008,
};
namespace o3tl
{
template <> struct typed_flags<ToTopFlags> : is_typed_flags<ToTopFlags, 0x000f>
{
};
}

// Flags for Invalidate
// must match css::awt::InvalidateStyle
enum class InvalidateFlags
{
    NONE = 0x0000,
    /** The child windows are invalidated, too. */
    Children = 0x0001,
    /** The child windows are not invalidated. */
    NoChildren = 0x0002,
    /** The invalidated area is painted with the background color/pattern. */
    NoErase = 0x0004,
    /** The invalidated area is updated immediately. */
    Update = 0x0008,
    /** The parent window is invalidated, too. */
    Transparent = 0x0010,
    /** The parent window is not invalidated. */
    NoTransparent = 0x0020,
    /** The area is invalidated regardless of overlapping child windows. */
    NoClipChildren = 0x4000,
};
namespace o3tl
{
template <> struct typed_flags<InvalidateFlags> : is_typed_flags<InvalidateFlags, 0x403f>
{
};
}

// Flags for Validate
enum class ValidateFlags
{
    NONE = 0x0000,
    Children = 0x0001,
    NoChildren = 0x0002
};
namespace o3tl
{
template <> struct typed_flags<ValidateFlags> : is_typed_flags<ValidateFlags, 0x0003>
{
};
}

// Flags for Scroll
enum class ScrollFlags
{
    NONE = 0x0000,
    Clip = 0x0001,
    Children = 0x0002,
    NoChildren = 0x0004,
    UseClipRegion = 0x0008,
    Update = 0x0010, // paint immediately
};
namespace o3tl
{
template <> struct typed_flags<ScrollFlags> : is_typed_flags<ScrollFlags, 0x001f>
{
};
}

// Flags for ParentClipMode
enum class ParentClipMode
{
    NONE = 0x0000,
    Clip = 0x0001,
    NoClip = 0x0002,
};
namespace o3tl
{
template <> struct typed_flags<ParentClipMode> : is_typed_flags<ParentClipMode, 0x0003>
{
};
}

// Flags for ShowTracking()
enum class ShowTrackFlags
{
    NONE = 0x0000,
    Small = 0x0001,
    Big = 0x0002,
    Split = 0x0003,
    Object = 0x0004,
    StyleMask = 0x000F,
    TrackWindow = 0x1000,
    Clip = 0x2000,
};
namespace o3tl
{
template <> struct typed_flags<ShowTrackFlags> : is_typed_flags<ShowTrackFlags, 0x300f>
{
};
}

// Flags for StartTracking()
enum class StartTrackingFlags
{
    NONE = 0x0001,
    KeyMod = 0x0002,
    ScrollRepeat = 0x0004,
    ButtonRepeat = 0x0008,
};

namespace o3tl
{
template <> struct typed_flags<StartTrackingFlags> : is_typed_flags<StartTrackingFlags, 0x000f>
{
};
}

// Flags for StartAutoScroll()
enum class StartAutoScrollFlags
{
    NONE = 0x0000,
    Vert = 0x0001,
    Horz = 0x0002,
};
namespace o3tl
{
template <> struct typed_flags<StartAutoScrollFlags> : is_typed_flags<StartAutoScrollFlags, 0x0003>
{
};
}

// Flags for StateChanged()
enum class StateChangedType : sal_uInt16
{
    InitShow = 1,
    Visible = 2,
    UpdateMode = 3,
    Enable = 4,
    Text = 5,
    Data = 7,
    State = 8,
    Style = 9,
    Zoom = 10,
    ControlFont = 13,
    ControlForeground = 14,
    ControlBackground = 15,
    ReadOnly = 16,
    Mirroring = 18,
    Layout = 19,
    ControlFocus = 20
};

// GetFocusFlags
// must match constants in css:awt::FocusChangeReason
enum class GetFocusFlags
{
    NONE = 0x0000,
    Tab = 0x0001,
    CURSOR = 0x0002, // avoid name-clash with X11 #define
    Mnemonic = 0x0004,
    F6 = 0x0008,
    Forward = 0x0010,
    Backward = 0x0020,
    Around = 0x0040,
    UniqueMnemonic = 0x0100,
    Init = 0x0200,
    FloatWinPopupModeEndCancel = 0x0400,
};
namespace o3tl
{
template <> struct typed_flags<GetFocusFlags> : is_typed_flags<GetFocusFlags, 0x077f>
{
};
}

// DialogControl-Flags
enum class DialogControlFlags
{
    NONE = 0x0000,
    Return = 0x0001,
    WantFocus = 0x0002,
    FloatWinPopupModeEndCancel = 0x0004,
};
namespace o3tl
{
template <> struct typed_flags<DialogControlFlags> : is_typed_flags<DialogControlFlags, 0x0007>
{
};
}

// EndExtTextInput() Flags
enum class EndExtTextInputFlags
{
    NONE = 0x0000,
    Complete = 0x0001
};
namespace o3tl
{
template <> struct typed_flags<EndExtTextInputFlags> : is_typed_flags<EndExtTextInputFlags, 0x0001>
{
};
}

#define IMPL_MINSIZE_BUTTON_WIDTH 70
#define IMPL_MINSIZE_BUTTON_HEIGHT 22
#define IMPL_EXTRA_BUTTON_WIDTH 18
#define IMPL_EXTRA_BUTTON_HEIGHT 10
#define IMPL_SEP_BUTTON_X 5
#define IMPL_SEP_BUTTON_Y 5
#define IMPL_MINSIZE_MSGBOX_WIDTH 150
#define IMPL_DIALOG_OFFSET 5
#define IMPL_DIALOG_BAR_OFFSET 3
#define IMPL_MSGBOX_OFFSET_EXTRA_X 0
#define IMPL_MSGBOX_OFFSET_EXTRA_Y 2
#define IMPL_SEP_MSGBOX_IMAGE 8

// ImplGetDlgWindow()
enum class GetDlgWindowType
{
    Prev,
    Next,
    First
};

namespace vcl
{
class Window;
}
namespace vcl
{
class Cursor;
}
namespace vcl
{
class WindowOutputDevice;
}
class Dialog;
class Edit;
class WindowImpl;
class PaintHelper;
class VclSizeGroup;
class Application;
class WorkWindow;
class MessBox;
class MessageDialog;
class DockingWindow;
class FloatingWindow;
class GroupBox;
class PushButton;
class RadioButton;
class SalInstanceWidget;
class SystemChildWindow;
class ImplDockingWindowWrapper;
class ImplPopupFloatWin;
class LifecycleTest;

enum class WindowHitTest
{
    NONE = 0x0000,
    Inside = 0x0001,
    Transparent = 0x0002
};
namespace o3tl
{
template <> struct typed_flags<WindowHitTest> : is_typed_flags<WindowHitTest, 0x0003>
{
};
};

enum class WindowExtendedStyle
{
    NONE = 0x0000,
    Document = 0x0001,
    DocModified = 0x0002,
    /**
     * This is a frame window that is requested to be hidden (not just "not yet
     * shown").
     */
    DocHidden = 0x0004,
};
namespace o3tl
{
template <> struct typed_flags<WindowExtendedStyle> : is_typed_flags<WindowExtendedStyle, 0x0007>
{
};
};

bool ImplCallCommand(const VclPtr<vcl::Window>& pChild, CommandEventId nEvt,
                     void const* pData = nullptr, bool bMouse = false, Point const* pPos = nullptr);

namespace vcl
{
enum class FocusAction
{
    Both,
    ActivateOnly,
    DeactivateOnly
};

class VCL_DLLPUBLIC Window : public virtual VclReferenceBase
{
    friend class ::vcl::Cursor;
    friend class ::vcl::WindowOutputDevice;
    friend class ::OutputDevice;
    friend class ::Application;
    friend class ::SystemWindow;
    friend class ::WorkWindow;
    friend class ::Dialog;
    friend class ::Edit;
    friend class ::MessBox;
    friend class ::MessageDialog;
    friend class ::DockingWindow;
    friend class ::FloatingWindow;
    friend class ::GroupBox;
    friend class ::PushButton;
    friend class ::RadioButton;
    friend class ::SalInstanceWidget;
    friend class ::SystemChildWindow;
    friend class ::ImplBorderWindow;
    friend class ::PaintHelper;
    friend class ::LifecycleTest;
    friend class ::VclEventListeners;

    // TODO: improve missing functionality
    // only required because of SetFloatingMode()
    friend class ::ImplDockingWindowWrapper;
    friend class ::ImplPopupFloatWin;
    friend class ::MenuFloatingWindow;

    friend class ::svt::PopupWindowControllerImpl;

public:
    // --- Lifecycle and Initialization ---
    explicit Window(vcl::Window* pParent, WinBits nStyle = 0);
    virtual ~Window() override;
    SAL_DLLPRIVATE static void ImplInitAppFontData(vcl::Window const* pWindow);

    // --- Hierarchy and Window State ---
    SAL_DLLPRIVATE vcl::Window* ImplGetFrameWindow() const;
    weld::Window* GetFrameWeld() const;
    vcl::Window* GetFrameWindow() const;
    SalFrame* ImplGetFrame() const;
    SAL_DLLPRIVATE ImplFrameData* ImplGetFrameData();
    vcl::Window* ImplGetWindow() const;
    SAL_DLLPRIVATE ImplWinData* ImplGetWinData() const;
    SAL_DLLPRIVATE vcl::Window* ImplGetClientWindow() const;
    SAL_DLLPRIVATE vcl::Window* ImplGetDlgWindow(sal_uInt16 n, GetDlgWindowType nType,
                                                 sal_uInt16 nStart = 0, sal_uInt16 nEnd = 0xFFFF,
                                                 sal_uInt16* pIndex = nullptr);
    SAL_DLLPRIVATE vcl::Window* ImplGetParent() const;
    SAL_DLLPRIVATE vcl::Window* ImplFindWindow(const Point& rFramePos);
    SAL_DLLPRIVATE bool ImplIsWindowOrChild(const vcl::Window* pWindow,
                                            bool bSystemWindow = false) const;
    SAL_DLLPRIVATE bool ImplIsChild(const vcl::Window* pWindow, bool bSystemWindow = false) const;
    SAL_DLLPRIVATE bool ImplIsFloatingWindow() const;
    SAL_DLLPRIVATE bool ImplIsPushButton() const;
    SAL_DLLPRIVATE bool ImplIsSplitter() const;
    SAL_DLLPRIVATE bool ImplIsOverlapWindow() const;
    SAL_DLLPRIVATE void ImplIsInTaskPaneList(bool mbIsInTaskList);
    SAL_DLLPRIVATE WindowImpl* ImplGetWindowImpl() const { return mpWindowImpl.get(); }
    void SetType(WindowType eType);
    WindowType GetType() const;
    bool IsSystemWindow() const;
    SAL_DLLPRIVATE bool IsDockingWindow() const;
    bool IsDialog() const;
    bool IsMenuFloatingWindow() const;
    bool IsNativeFrame() const;
    bool IsTopWindow() const;
    SystemWindow* GetSystemWindow() const;
    void SetParent(vcl::Window* pNewParent);
    vcl::Window* GetParent() const;
    SAL_DLLPRIVATE Dialog* GetParentDialog() const;
    bool IsAncestorOf(const vcl::Window& rWindow) const;
    sal_uInt16 GetChildCount() const;
    vcl::Window* GetChild(sal_uInt16 nChild) const;
    vcl::Window* GetWindow(GetWindowType nType) const;
    bool IsChild(const vcl::Window* pWindow) const;
    bool IsWindowOrChild(const vcl::Window* pWindow, bool bSystemWindow = false) const;
    SAL_DLLPRIVATE void CollectChildren(::std::vector<vcl::Window*>& rAllChildren);
    void ToTop(ToTopFlags nFlags = ToTopFlags::NONE);
    void SetZOrder(vcl::Window* pRefWindow, ZOrderFlags nFlags);
    SAL_DLLPRIVATE void EnableAlwaysOnTop(bool bEnable = true);
    SAL_DLLPRIVATE bool IsAlwaysOnTopEnabled() const;
    void Show(bool bVisible = true, ShowFlags nFlags = ShowFlags::NONE);
    void Hide() { Show(false); }
    bool IsVisible() const;
    bool IsReallyVisible() const;
    bool IsReallyShown() const;
    SAL_DLLPRIVATE bool IsInInitShow() const;
    void Enable(bool bEnable = true, bool bChild = true);
    void Disable(bool bChild = true) { Enable(false, bChild); }
    bool IsEnabled() const;
    void EnableInput(bool bEnable = true, bool bChild = true);
    SAL_DLLPRIVATE void EnableInput(bool bEnable, const vcl::Window* pExcludeWindow);
    bool IsInputEnabled() const;
    void AlwaysEnableInput(bool bAlways, bool bChild = true);
    SAL_DLLPRIVATE bool IsAlwaysEnableInput() const;
    void IncModalCount();
    void DecModalCount();
    bool IsInModalMode() const;
    SAL_DLLPRIVATE void ImplNotifyIconifiedState(bool bIconified);

    // --- Geometry, Position and Size ---
    SAL_DLLPRIVATE void ImplMirrorFramePos(Point& pt) const;
    SAL_DLLPRIVATE void ImplPosSizeWindow(tools::Long nX, tools::Long nY, tools::Long nWidth,
                                          tools::Long nHeight, PosSizeFlags nFlags);
    SAL_DLLPRIVATE void ImplCallResize();
    SAL_DLLPRIVATE void ImplCallMove();
    virtual void setPosSizePixel(tools::Long nX, tools::Long nY, tools::Long nWidth,
                                 tools::Long nHeight, PosSizeFlags nFlags = PosSizeFlags::All);
    virtual void SetPosPixel(const Point& rNewPos);
    virtual Point GetPosPixel() const;
    virtual void SetSizePixel(const Size& rNewSize);
    virtual Size GetSizePixel() const;
    virtual void SetPosSizePixel(const Point& rNewPos, const Size& rNewSize);
    virtual void SetOutputSizePixel(const Size& rNewSize);
    bool IsDefaultPos() const;
    SAL_DLLPRIVATE bool IsDefaultSize() const;
    Point GetOffsetPixelFrom(const vcl::Window& rWindow) const;
    void EnableAllResize();
    Size CalcWindowSize(const Size& rOutSz) const;
    SAL_DLLPRIVATE Size CalcOutputSize(const Size& rWinSz) const;
    tools::Long CalcTitleWidth() const;
    virtual void Move();
    virtual void Resize();
    Size GetOutputSizePixel() const;
    SAL_DLLPRIVATE tools::Rectangle GetOutputRectPixel() const;
    SAL_DLLPRIVATE static void ImplCalcSymbolRect(tools::Rectangle& rRect);

    // --- Rendering and Invalidation ---
    SAL_DLLPRIVATE void ImplInvalidateFrameRegion(const vcl::Region* pRegion,
                                                  InvalidateFlags nFlags);
    SAL_DLLPRIVATE void ImplInvalidateOverlapFrameRegion(const vcl::Region& rRegion);
    SAL_DLLPRIVATE void ImplUpdateAll();
    virtual void PrePaint(vcl::RenderContext& rRenderContext);
    virtual void Paint(vcl::RenderContext& rRenderContext, const tools::Rectangle& rRect);
    virtual void PostPaint(vcl::RenderContext& rRenderContext);
    SAL_DLLPRIVATE void Erase(vcl::RenderContext& rRenderContext);
    virtual void Draw(OutputDevice& rDev, const Point& rPos, SystemTextColorFlags nFlags);
    void Invalidate(InvalidateFlags nFlags = InvalidateFlags::NONE);
    void Invalidate(const tools::Rectangle& rRect, InvalidateFlags nFlags = InvalidateFlags::NONE);
    void Invalidate(const vcl::Region& rRegion, InvalidateFlags nFlags = InvalidateFlags::NONE);
    virtual void LogicInvalidate(const tools::Rectangle* pRectangle);
    virtual bool InvalidateByForeignEditView(EditView*);
    virtual void PixelInvalidate(const tools::Rectangle* pRectangle);
    void Validate();
    SAL_DLLPRIVATE bool HasPaintEvent() const;
    void PaintImmediately();
    void EnablePaint(bool bEnable);
    bool IsPaintEnabled() const;
    void SetUpdateMode(bool bUpdate);
    bool IsUpdateMode() const;
    void SetParentUpdateMode(bool bUpdate);
    bool SupportsDoubleBuffering() const;
    void RequestDoubleBuffering(bool bRequest);
    void SetParentClipMode(ParentClipMode nMode = ParentClipMode::NONE);
    ParentClipMode GetParentClipMode() const;
    void SetWindowRegionPixel();
    void SetWindowRegionPixel(const vcl::Region& rRegion);
    vcl::Region GetWindowClipRegionPixel() const;
    vcl::Region GetPaintRegion() const;
    bool IsInPaint() const;
    void ExpandPaintClipRegion(const vcl::Region& rRegion);
    void EnableChildTransparentMode(bool bEnable = true);
    bool IsChildTransparentModeEnabled() const;
    void SetMouseTransparent(bool bTransparent);
    bool IsMouseTransparent() const;
    void SetPaintTransparent(bool bTransparent);
    bool IsPaintTransparent() const;
    void EnableClipSiblings(bool bClipSiblings = true);

    // --- Styling, Colors and Fonts ---
    void SetStyle(WinBits nStyle);
    WinBits GetStyle() const;
    SAL_DLLPRIVATE WinBits GetPrevStyle() const;
    void SetExtendedStyle(WindowExtendedStyle nExtendedStyle);
    WindowExtendedStyle GetExtendedStyle() const;
    void SetBorderStyle(WindowBorderStyle nBorderStyle);
    WindowBorderStyle GetBorderStyle() const;
    void GetBorder(sal_Int32& rLeftBorder, sal_Int32& rTopBorder, sal_Int32& rRightBorder,
                   sal_Int32& rBottomBorder) const;
    void SetPointFont(vcl::RenderContext& rRenderContext, const vcl::Font& rFont,
                      bool bUseRenderContextDPI = false);
    vcl::Font GetPointFont(vcl::RenderContext const& rRenderContext) const;
    void SetZoomedPointFont(vcl::RenderContext& rRenderContext, const vcl::Font& rFont);
    void SetControlFont();
    void SetControlFont(const vcl::Font& rFont);
    vcl::Font GetControlFont() const;
    bool IsControlFont() const;
    void ApplyControlFont(vcl::RenderContext& rRenderContext, const vcl::Font& rDefaultFont);
    void SetControlForeground();
    void SetControlForeground(const Color& rColor);
    const Color& GetControlForeground() const;
    bool IsControlForeground() const;
    void ApplyControlForeground(vcl::RenderContext& rRenderContext, const Color& rDefaultColor);
    void SetControlBackground();
    void SetControlBackground(const Color& rColor);
    const Color& GetControlBackground() const;
    bool IsControlBackground() const;
    void ApplyControlBackground(vcl::RenderContext& rRenderContext, const Color& rDefaultColor);
    Color GetBackgroundColor() const;
    const Wallpaper& GetBackground() const;
    bool IsBackground() const;
    void SetBackground();
    void SetBackground(const Wallpaper& rBackground);
    void SetFont(const vcl::Font& rNewFont);
    const vcl::Font& GetFont() const;
    void SetTextColor(const Color& rColor);
    const Color& GetTextColor() const;
    void SetTextFillColor();
    void SetTextFillColor(const Color& rColor);
    Color GetTextFillColor() const;
    SAL_DLLPRIVATE bool IsTextFillColor() const;
    void SetTextLineColor();
    void SetTextLineColor(const Color& rColor);
    const Color& GetTextLineColor() const;
    bool IsTextLineColor() const;
    SAL_DLLPRIVATE void SetOverlineColor();
    SAL_DLLPRIVATE void SetOverlineColor(const Color& rColor);
    SAL_DLLPRIVATE const Color& GetOverlineColor() const;
    SAL_DLLPRIVATE bool IsOverlineColor() const;
    void SetTextAlign(TextAlign eAlign);
    SAL_DLLPRIVATE TextAlign GetTextAlign() const;

    // --- Focus and Activation ---
    bool IsCompoundControl() const;
    SAL_DLLPRIVATE void ImplGrabFocus(GetFocusFlags nFlags);
    SAL_DLLPRIVATE void ImplGrabFocusToDocument(GetFocusFlags nFlags);
    SAL_DLLPRIVATE void ImplInvertFocus(const tools::Rectangle& rRect);
    SAL_DLLPRIVATE void ImplControlFocus(GetFocusFlags nFlags = GetFocusFlags::NONE);
    SAL_DLLPRIVATE void CompatGetFocus();
    SAL_DLLPRIVATE void CompatLoseFocus();
    virtual void Activate();
    virtual void Deactivate();
    virtual void GetFocus();
    virtual void LoseFocus();
    void GrabFocus();
    bool HasFocus() const;
    bool HasChildPathFocus(bool bSystemWindow = false) const;
    bool IsActive() const;
    bool HasActiveChildFrame() const;
    GetFocusFlags GetGetFocusFlags() const;
    void GrabFocusToDocument();
    VclPtr<vcl::Window> GetFocusedWindow() const;
    SAL_DLLPRIVATE void SetFakeFocus(bool bFocus);
    SAL_DLLPRIVATE static VclPtr<vcl::Window> SaveFocus();
    SAL_DLLPRIVATE static void EndSaveFocus(const VclPtr<vcl::Window>& xFocusWin);
    virtual void ShowFocus(const tools::Rectangle& rRect);
    void HideFocus();
    SAL_DLLPRIVATE void SetActivateMode(ActivateModeFlags nMode);
    SAL_DLLPRIVATE ActivateModeFlags GetActivateMode() const;

    // --- Input Events (Mouse, Keyboard, Drag & Drop) ---
    DECL_DLLPRIVATE_LINK(ImplHandlePaintHdl, Timer*, void);
    DECL_DLLPRIVATE_LINK(ImplGenerateMouseMoveHdl, void*, void);
    DECL_DLLPRIVATE_LINK(ImplTrackTimerHdl, Timer*, void);
    DECL_DLLPRIVATE_LINK(ImplAsyncFocusHdl, void*, void);
    DECL_DLLPRIVATE_LINK(ImplHandleResizeTimerHdl, Timer*, void);
    SAL_DLLPRIVATE PointerStyle ImplGetMousePointer() const;
    SAL_DLLPRIVATE void ImplCallMouseMove(sal_uInt16 nMouseCode, bool bModChanged = false);
    SAL_DLLPRIVATE void ImplGenerateMouseMove();
    SAL_DLLPRIVATE void ImplNotifyKeyMouseCommandEventListeners(NotifyEvent& rNEvt);
    SAL_DLLPRIVATE void CompatStateChanged(StateChangedType nStateChange);
    SAL_DLLPRIVATE void CompatDataChanged(const DataChangedEvent& rDCEvt);
    SAL_DLLPRIVATE bool CompatPreNotify(NotifyEvent& rNEvt);
    SAL_DLLPRIVATE bool CompatNotify(NotifyEvent& rNEvt);
    virtual void MouseMove(const MouseEvent& rMEvt);
    virtual void MouseButtonDown(const MouseEvent& rMEvt);
    virtual void MouseButtonUp(const MouseEvent& rMEvt);
    virtual void KeyInput(const KeyEvent& rKEvt);
    virtual void KeyUp(const KeyEvent& rKEvt);
    virtual void Command(const CommandEvent& rCEvt);
    virtual void Tracking(const TrackingEvent& rTEvt);
    virtual void StateChanged(StateChangedType nStateChange);
    virtual void DataChanged(const DataChangedEvent& rDCEvt);
    virtual bool PreNotify(NotifyEvent& rNEvt);
    virtual bool EventNotify(NotifyEvent& rNEvt);
    void AddEventListener(const Link<VclWindowEvent&, void>& rEventListener);
    void RemoveEventListener(const Link<VclWindowEvent&, void>& rEventListener);
    void AddChildEventListener(const Link<VclWindowEvent&, void>& rEventListener);
    void RemoveChildEventListener(const Link<VclWindowEvent&, void>& rEventListener);
    ImplSVEvent* PostUserEvent(const Link<void*, void>& rLink, void* pCaller = nullptr,
                               bool bReferenceLink = false);
    void RemoveUserEvent(ImplSVEvent* nUserEvent);
    LanguageType GetInputLanguage() const;

    struct PointerState
    {
        sal_Int32 mnState; // the button state
        Point maPos; // mouse position in output coordinates
    };

    PointerState GetPointerState();
    bool IsMouseOver() const;
    void SetInputContext(const InputContext& rInputContext);
    const InputContext& GetInputContext() const;
    void PostExtTextInputEvent(VclEventId nType, const OUString& rText);
    SAL_DLLPRIVATE void EndExtTextInput();
    void SetCursorRect(const tools::Rectangle* pRect = nullptr, tools::Long nExtTextInputWidth = 0);
    SAL_DLLPRIVATE const tools::Rectangle* GetCursorRect() const;
    SAL_DLLPRIVATE tools::Long GetCursorExtTextInputWidth() const;
    void SetCompositionCharRect(const tools::Rectangle* pRect, tools::Long nCompositionLength,
                                bool bVertical = false);
    void LocalStartDrag();
    void CaptureMouse();
    void ReleaseMouse();
    bool IsMouseCaptured() const;
    virtual void SetPointer(PointerStyle);
    PointerStyle GetPointer() const;
    void EnableChildPointerOverwrite(bool bOverwrite);
    void SetPointerPosPixel(const Point& rPos);
    Point GetPointerPosPixel();
    SAL_DLLPRIVATE Point GetLastPointerPosPixel();
    void SetLastMousePos(const Point& rPos);
    void ShowPointer(bool bVisible);
    void EnterWait();
    void LeaveWait();
    bool IsWait() const;
    void SetCursor(vcl::Cursor* pCursor);
    vcl::Cursor* GetCursor() const;
    rtl::Reference<DNDListenerContainer> GetDropTarget();
    css::uno::Reference<css::datatransfer::dnd::XDragSource> GetDragSource();
    css::uno::Reference<css::datatransfer::clipboard::XClipboard> GetClipboard();
    void
    SetClipboard(css::uno::Reference<css::datatransfer::clipboard::XClipboard> const& xClipboard);
    KeyIndicatorState GetIndicatorState() const;
    void SimulateKeyPress(sal_uInt16 nKeyCode) const;
    virtual OUString GetSurroundingText() const;
    virtual Selection GetSurroundingTextSelection() const;
    virtual bool DeleteSurroundingText(const Selection& rSelection);
    SAL_DLLPRIVATE void SetCommandHdl(const Link<const CommandEvent&, bool>& rLink);
    SAL_DLLPRIVATE void SetMnemonicActivateHdl(const Link<vcl::Window&, bool>& rLink);

    // --- Scrolling and Tracking ---
    void ShowTracking(const tools::Rectangle& rRect, ShowTrackFlags nFlags = ShowTrackFlags::Small);
    void HideTracking();
    void InvertTracking(const tools::Rectangle& rRect, ShowTrackFlags nFlags);
    void StartTracking(StartTrackingFlags nFlags = StartTrackingFlags::NONE);
    void EndTracking(TrackingEventFlags nFlags = TrackingEventFlags::NONE);
    bool IsTracking() const;
    SAL_DLLPRIVATE void StartAutoScroll(StartAutoScrollFlags nFlags);
    SAL_DLLPRIVATE void EndAutoScroll();
    bool HandleScrollCommand(const CommandEvent& rCmd, Scrollable* pHScrl, Scrollable* pVScrl);
    SAL_DLLPRIVATE bool IsScrollable() const;
    virtual void Scroll(tools::Long nHorzScroll, tools::Long nVertScroll,
                        ScrollFlags nFlags = ScrollFlags::NONE);
    void Scroll(tools::Long nHorzScroll, tools::Long nVertScroll, const tools::Rectangle& rRect,
                ScrollFlags nFlags = ScrollFlags::NONE);

    // --- UI and Layout Properties ---
    virtual void queue_resize(StateChangedType eReason = StateChangedType::Layout);
    void set_height_request(sal_Int32 nHeightRequest);
    sal_Int32 get_height_request() const;
    void set_width_request(sal_Int32 nWidthRequest);
    sal_Int32 get_width_request() const;
    Size get_preferred_size() const;
    SAL_DLLPRIVATE VclAlign get_halign() const;
    SAL_DLLPRIVATE void set_halign(VclAlign eAlign);
    SAL_DLLPRIVATE VclAlign get_valign() const;
    SAL_DLLPRIVATE void set_valign(VclAlign eAlign);
    bool get_hexpand() const;
    void set_hexpand(bool bExpand);
    bool get_vexpand() const;
    void set_vexpand(bool bExpand);
    bool get_expand() const;
    void set_expand(bool bExpand);
    SAL_DLLPRIVATE bool get_fill() const;
    SAL_DLLPRIVATE void set_fill(bool bFill);
    void set_border_width(sal_Int32 nBorderWidth);
    SAL_DLLPRIVATE sal_Int32 get_border_width() const;
    SAL_DLLPRIVATE void set_margin_start(sal_Int32 nWidth);
    SAL_DLLPRIVATE sal_Int32 get_margin_start() const;
    SAL_DLLPRIVATE void set_margin_end(sal_Int32 nWidth);
    SAL_DLLPRIVATE sal_Int32 get_margin_end() const;
    void set_margin_top(sal_Int32 nWidth);
    SAL_DLLPRIVATE sal_Int32 get_margin_top() const;
    SAL_DLLPRIVATE void set_margin_bottom(sal_Int32 nWidth);
    SAL_DLLPRIVATE sal_Int32 get_margin_bottom() const;
    SAL_DLLPRIVATE VclPackType get_pack_type() const;
    SAL_DLLPRIVATE void set_pack_type(VclPackType ePackType);
    SAL_DLLPRIVATE sal_Int32 get_padding() const;
    SAL_DLLPRIVATE void set_padding(sal_Int32 nPadding);
    SAL_DLLPRIVATE sal_Int32 get_grid_width() const;
    SAL_DLLPRIVATE void set_grid_width(sal_Int32 nCols);
    SAL_DLLPRIVATE sal_Int32 get_grid_left_attach() const;
    SAL_DLLPRIVATE void set_grid_left_attach(sal_Int32 nAttach);
    SAL_DLLPRIVATE sal_Int32 get_grid_height() const;
    SAL_DLLPRIVATE void set_grid_height(sal_Int32 nRows);
    SAL_DLLPRIVATE sal_Int32 get_grid_top_attach() const;
    SAL_DLLPRIVATE void set_grid_top_attach(sal_Int32 nAttach);
    SAL_DLLPRIVATE bool get_secondary() const;
    SAL_DLLPRIVATE void set_secondary(bool bSecondary);
    SAL_DLLPRIVATE bool get_non_homogeneous() const;
    SAL_DLLPRIVATE void set_non_homogeneous(bool bNonHomogeneous);
    virtual bool set_property(const OUString& rKey, const OUString& rValue);
    SAL_DLLPRIVATE bool set_font_attribute(const OUString& rKey, std::u16string_view rValue);
    SAL_DLLPRIVATE void add_to_size_group(const std::shared_ptr<VclSizeGroup>& xGroup);
    SAL_DLLPRIVATE void remove_from_all_size_groups();
    SAL_DLLPRIVATE void add_mnemonic_label(FixedText* pLabel);
    SAL_DLLPRIVATE void remove_mnemonic_label(FixedText* pLabel);
    SAL_DLLPRIVATE const std::vector<VclPtr<FixedText>>& list_mnemonic_labels() const;
    SAL_DLLPRIVATE void reorderWithinParent(sal_uInt16 nNewPosition);
    void set_id(const OUString& rID);
    const OUString& get_id() const;
    void RecordLayoutData(vcl::ControlLayoutData* pLayout, const tools::Rectangle& rRect);

    // --- Dialog Control ---
    bool IsFormControl() const;
    void SetFormControl(bool bFormControl);
    void SetDialogControlStart(bool bStart);
    SAL_DLLPRIVATE bool IsDialogControlStart() const;
    void SetDialogControlFlags(DialogControlFlags nFlags);
    SAL_DLLPRIVATE DialogControlFlags GetDialogControlFlags() const;

    // --- Accessibility ---
    rtl::Reference<comphelper::OAccessible> GetAccessible(bool bCreate = true);
    void SetAccessible(const rtl::Reference<comphelper::OAccessible>& rpAccessible);
    vcl::Window* GetAccessibleParentWindow() const;
    sal_uInt16 GetAccessibleChildWindowCount();
    vcl::Window* GetAccessibleChildWindow(sal_uInt16 n);
    rtl::Reference<comphelper::OAccessible> GetAccessibleParent() const;
    void SetAccessibleParent(const rtl::Reference<comphelper::OAccessible>& rpParent);
    void SetAccessibleRole(sal_uInt16 nRole);
    sal_uInt16 GetAccessibleRole() const;
    void SetAccessibleName(const OUString& rName);
    OUString GetAccessibleName() const;
    SAL_DLLPRIVATE void SetAccessibleDescription(const OUString& rDescr);
    OUString GetAccessibleDescription() const;
    SAL_DLLPRIVATE void SetAccessibleRelationLabeledBy(vcl::Window* pLabeledBy);
    vcl::Window* GetAccessibleRelationLabeledBy() const;
    SAL_DLLPRIVATE void SetAccessibleRelationLabelFor(vcl::Window* pLabelFor);
    vcl::Window* GetAccessibleRelationLabelFor() const;
    vcl::Window* GetAccessibleRelationMemberOf() const;
    bool IsAccessibilityEventsSuppressed();
    KeyEvent GetActivationKey() const;

    // --- Settings and Context ---
    SAL_DLLPRIVATE void UpdateSettings(const AllSettings& rSettings, bool bChild = false);
    SAL_DLLPRIVATE void NotifyAllChildren(DataChangedEvent& rDCEvt);
    const AllSettings& GetSettings() const;
    void SetSettings(const AllSettings& rSettings);
    void SetSettings(const AllSettings& rSettings, bool bChild);
    virtual const SystemEnvData* GetSystemData() const;

    // --- OutputDevice and Coordinate Conversion ---
    ::OutputDevice const* GetOutDev() const;
    ::OutputDevice* GetOutDev();
    template <typename TargetType, typename SourceType>
    auto convertTo(const SourceType& rSource, const MapMode& rMapMode) const
    {
        return GetOutDev()->convertTo<TargetType>(rSource, rMapMode);
    }
    template <typename TargetType, typename SourceType>
    auto convertTo(const SourceType& rSource) const
    {
        return GetOutDev()->convertTo<TargetType>(rSource);
    }
    template <typename TargetT, typename SourceT>
    TargetT convertLogic(const SourceT& rSourceGeom, const MapMode* pSrc = nullptr,
                         const MapMode* pDst = nullptr) const
    {
        return GetOutDev()->convertLogic<TargetT, SourceT>(rSourceGeom, pSrc, pDst);
    }
    const MapMode& GetMapMode() const;
    void SetMappingPolicy(vcl::MappingPolicy ePolicy = vcl::MappingPolicy::ApplyMapMode);
    vcl::MappingPolicy GetMappingPolicy() const;
    SAL_DLLPRIVATE void SetMapMode();
    void SetMapMode(const MapMode& rNewMapMode);
    SAL_DLLPRIVATE tools::Long GetDrawPixel(::OutputDevice const* pDev, tools::Long nPixels) const;
    vcl::Font GetDrawPixelFont(::OutputDevice const* pDev) const;
    Point OutputToScreenPixel(const Point& rPos) const;
    Point ScreenToOutputPixel(const Point& rPos) const;
    Point OutputToNormalizedScreenPixel(const Point& rPos) const;
    SAL_DLLPRIVATE Point NormalizedScreenToOutputPixel(const Point& rPos) const;
    AbsoluteScreenPixelPoint OutputToAbsoluteScreenPixel(const Point& rPos) const;
    Point AbsoluteScreenToOutputPixel(const AbsoluteScreenPixelPoint& rPos) const;
    AbsoluteScreenPixelRectangle GetDesktopRectPixel() const;
    tools::Rectangle GetWindowExtentsRelative(const vcl::Window& rRelativeWindow) const;
    AbsoluteScreenPixelRectangle GetWindowExtentsAbsolute() const;
    float GetDPIScaleFactor() const;
    tools::Long GetDeviceOriginX() const;
    tools::Long GetDeviceOriginY() const;
    SAL_DLLPRIVATE tools::Long LogicWidthToDevicePixel(tools::Long nWidth) const;

    // --- Text and Fonts ---
    virtual void SetText(const OUString& rStr);
    virtual OUString GetText() const;
    virtual OUString GetDisplayText() const;
    virtual const Wallpaper& GetDisplayBackground() const;
    tools::Rectangle GetTextRect(const tools::Rectangle& rRect, const OUString& rStr,
                                 DrawTextFlags nStyle = DrawTextFlags::WordBreak,
                                 TextRectInfo* pInfo = nullptr,
                                 const vcl::TextLayoutCommon* _pTextLayout = nullptr) const;
    tools::Long GetTextWidth(const OUString& rStr, sal_Int32 nIndex = 0, sal_Int32 nLen = -1,
                             vcl::text::TextLayoutCache const* = nullptr,
                             SalLayoutGlyphs const* const pLayoutCache = nullptr) const;
    tools::Long GetTextHeight() const;
    float approximate_digit_width() const;

    // --- Miscellaneous (Help, LOK, Native, Toolkit) ---
    void SetHelpText(const OUString& rHelpText);
    const OUString& GetHelpText() const;
    void SetQuickHelpText(const OUString& rHelpText);
    const OUString& GetQuickHelpText() const;
    void SetHelpId(const OUString&);
    const OUString& GetHelpId() const;
    virtual void RequestHelp(const HelpEvent& rHEvt);
    SAL_DLLPRIVATE void SetHelpHdl(const Link<vcl::Window&, bool>& rLink);
    void SetModalHierarchyHdl(const Link<bool, void>& rLink);
    virtual css::uno::Reference<css::awt::XVclWindowPeer> GetComponentInterface(bool bCreate
                                                                                = true);
    void SetComponentInterface(css::uno::Reference<css::awt::XVclWindowPeer> const& xIFace);
    void SetUseFrameData(bool bUseFrameData);
    void SetLOKNotifier(const vcl::ILibreOfficeKitNotifier* pNotifier, bool bParent = false);
    const vcl::ILibreOfficeKitNotifier* GetLOKNotifier() const;
    vcl::LOKWindowId GetLOKWindowId() const;
    void SetLOKWindowId();
    VclPtr<vcl::Window> GetParentWithLOKNotifier();
    void ReleaseLOKNotifier();
    static VclPtr<vcl::Window> FindLOKWindow(vcl::LOKWindowId nWindowId);
    SAL_DLLPRIVATE static bool IsLOKWindowsEmpty();
    virtual void FlashWindow() const;
    void SetTaskBarProgress(int nCurrentProgress);
    void SetTaskBarState(VclTaskBarStates eTaskBarState);
    VCLXWindow* GetWindowPeer() const;
    void SetWindowPeer(css::uno::Reference<css::awt::XVclWindowPeer> const& xPeer,
                       VCLXWindow* pVCLXWindow);
    SAL_DLLPRIVATE bool IsCreatedWithToolkit() const;
    void SetCreatedWithToolkit(bool b);
    virtual FactoryFunction GetUITestFactory() const;
    void SetZoom(const double fZoom);
    double GetZoom() const;
    bool IsZoom() const;
    tools::Long CalcZoom(tools::Long n) const;
    virtual void EnableRTL(bool bEnable = true);
    bool IsRTLEnabled() const;
    virtual bool IsChart() const { return false; }
    virtual bool IsStarMath() const { return false; }
    void EnableNativeWidget(bool bEnable = true);
    bool IsNativeWidgetEnabled() const;
    void PaintToDevice(OutputDevice& rDevice, const Point& rPos);
    SAL_DLLPRIVATE bool IsNativeControlSupported(ControlType nType, ControlPart nPart) const;
    SAL_DLLPRIVATE bool GetNativeControlRegion(ControlType nType, ControlPart nPart,
                                               const tools::Rectangle& rControlRegion,
                                               ControlState nState, const ImplControlValue& aValue,
                                               tools::Rectangle& rNativeBoundingRegion,
                                               tools::Rectangle& rNativeContentRegion) const;
    void EnableDocking(bool bEnable = true);
    static DockingManager* GetDockingManager();

protected:
    // --- Lifecycle and Initialization ---
    virtual void dispose() override;
    SAL_DLLPRIVATE void ImplInit(vcl::Window* pParent, WinBits nStyle,
                                 SystemParentData* pSystemParentData);
    SAL_DLLPRIVATE explicit Window(WindowType eType);

    // --- Hierarchy and Window State ---
    SAL_DLLPRIVATE vcl::Window* ImplGetBorderWindow() const;

    // --- Rendering and Invalidation ---
    SAL_DLLPRIVATE void ImplInvalidateParentFrameRegion(const vcl::Region& rRegion);
    SAL_DLLPRIVATE void ImplValidateFrameRegion(const vcl::Region* rRegion, ValidateFlags nFlags);
    SAL_DLLPRIVATE void ImplValidate();
    SAL_DLLPRIVATE void ImplMoveInvalidateRegion(const tools::Rectangle& rRect,
                                                 tools::Long nHorzScroll, tools::Long nVertScroll,
                                                 bool bChildren);
    SAL_DLLPRIVATE void ImplMoveAllInvalidateRegions(const tools::Rectangle& rRect,
                                                     tools::Long nHorzScroll,
                                                     tools::Long nVertScroll, bool bChildren);
    virtual void ImplInvalidate(const vcl::Region* pRegion, InvalidateFlags nFlags);
    SAL_DLLPRIVATE void PushPaintHelper(PaintHelper* pHelper, vcl::RenderContext& rRenderContext);
    SAL_DLLPRIVATE void PopPaintHelper(PaintHelper const* pHelper);
    virtual void ImplPaintToDevice(OutputDevice& rTargetOutDev, const Point& rPos);

    // --- Focus and Activation ---
    SAL_DLLPRIVATE void ImplProcessFocusLoss();

    // --- Input Events ---
    virtual WindowHitTest ImplHitTest(const Point& rFramePos);
    void CallEventListeners(VclEventId nEvent, void* pData = nullptr);

    // --- Properties and Layout ---
    SAL_DLLPRIVATE void ImplSetMouseTransparent(bool bTransparent);
    void SetCompoundControl(bool bCompound);
    virtual void ImplAdjustNWFSizes();
    virtual Size GetOptimalSize() const;
    SAL_DLLPRIVATE void InvalidateSizeCache();

    // --- Settings ---
    virtual void ApplySettings(vcl::RenderContext& rRenderContext);

    // --- Accessibility ---
    virtual rtl::Reference<comphelper::OAccessible> CreateAccessible();
    SAL_DLLPRIVATE vcl::Window* getLegacyNonLayoutAccessibleRelationMemberOf() const;
    SAL_DLLPRIVATE vcl::Window* getLegacyNonLayoutAccessibleRelationLabeledBy() const;
    SAL_DLLPRIVATE vcl::Window* getLegacyNonLayoutAccessibleRelationLabelFor() const;
    virtual vcl::Window* getAccessibleRelationLabelFor() const;
    virtual sal_uInt16 getDefaultAccessibleRole() const;
    virtual OUString getDefaultAccessibleName() const;

    // --- Scrolling ---
    SAL_DLLPRIVATE void ImplScroll(const tools::Rectangle& rRect, tools::Long nHorzScroll,
                                   tools::Long nVertScroll, ScrollFlags nFlags);

    // --- Text and Fonts ---
    SAL_DLLPRIVATE float approximate_char_width() const;

private:
    // --- Data Members ---
    std::unique_ptr<WindowImpl> mpWindowImpl;

    // --- Lifecycle and Teardown ---
    SAL_DLLPRIVATE void ImplDeInitDND();
    SAL_DLLPRIVATE void ImplDeInitAccessibility();
#if OSL_DEBUG_LEVEL > 0
    SAL_DLLPRIVATE void ImplCheckLiveChildrenOnDestroy();
#endif
    SAL_DLLPRIVATE void ImplDisposeFrameData();

    // --- Hierarchy and Window State ---
    SAL_DLLPRIVATE void ImplSetFrameParent(const vcl::Window* pParent);
    SAL_DLLPRIVATE void ImplInsertWindow(vcl::Window* pParent);
    SAL_DLLPRIVATE void ImplRemoveWindow(bool bRemoveFrameData);
    SAL_DLLPRIVATE bool ImplIsRealParentPath(const vcl::Window* pWindow) const;
    SAL_DLLPRIVATE void ImplUpdateWindowPtr(vcl::Window* pWindow);
    SAL_DLLPRIVATE void ImplUpdateWindowPtr();
    SAL_DLLPRIVATE void ImplUpdateOverlapWindowPtr(bool bNewFrame);
    SAL_DLLPRIVATE void ImplToBottomChild();
    SAL_DLLPRIVATE void ImplCalcToTop(ImplCalcToTopData* pPrevData);
    SAL_DLLPRIVATE void ImplToTop(ToTopFlags nFlags);
    SAL_DLLPRIVATE void ImplStartToTop(ToTopFlags nFlags);
    SAL_DLLPRIVATE void ImplFocusToTop(ToTopFlags nFlags, bool bReallyVisible);
    SAL_DLLPRIVATE void ImplShowAllOverlaps();
    SAL_DLLPRIVATE void ImplHideAllOverlaps();
    SAL_DLLPRIVATE ::std::vector<VclPtr<vcl::Window>>& ImplGetOwnerDrawList();
    SAL_DLLPRIVATE vcl::Window* ImplGetTopmostFrameWindow() const;
    SAL_DLLPRIVATE void ImplRemoveFromTaskPaneList();
    SAL_DLLPRIVATE void ImplRemoveOwnerDrawDecoratedFrame();
    SAL_DLLPRIVATE void ImplResetGlobalWindowPointers();
    SAL_DLLPRIVATE void ImplResetFrameDataPointers();
    SAL_DLLPRIVATE void ImplDeregisterTopWindowChild();
    SAL_DLLPRIVATE WindowImpl* ImplGetEffectiveWindowImpl() const;
    SAL_DLLPRIVATE bool ImplRequiresParentLayoutUpdate(const vcl::Window* pParent) const;
    SAL_DLLPRIVATE void ImplQueueResizeOnGroup() const;

    // --- Geometry, Position, and Size ---
    SAL_DLLPRIVATE bool ImplUpdatePos();
    SAL_DLLPRIVATE void ImplUpdateNativeObjectPos();
    SAL_DLLPRIVATE bool ImplUpdateOutputSize(PosSizeFlags nFlags, tools::Long nWidth,
                                             tools::Long nHeight);
    SAL_DLLPRIVATE Size ImplGetClientAvailableSize() const;
    SAL_DLLPRIVATE void ImplAdjustPosForRTL(tools::Long& rX, tools::Long& rOrgX, Point& rPtDev,
                                            bool bXAlreadyMirrored);
    SAL_DLLPRIVATE bool ImplUpdatePos(PosSizeFlags nFlags, tools::Long nX, tools::Long nY,
                                      bool bXAlreadyMirrored, bool bCopyBits,
                                      std::unique_ptr<vcl::Region>& rpOverlapRegion);
    SAL_DLLPRIVATE bool ImplUpdatePosX(tools::Long nX, bool bXAlreadyMirrored, bool bCopyBits,
                                       std::unique_ptr<vcl::Region>& rpOverlapRegion);
    SAL_DLLPRIVATE bool ImplUpdatePosY(tools::Long nY, bool bCopyBits,
                                       std::unique_ptr<vcl::Region>& rpOverlapRegion);
    SAL_DLLPRIVATE void ImplUpdateClientWindow(bool bNewPos);
    SAL_DLLPRIVATE void ImplCallMoveResize(bool bNewPos, bool bNewSize);
    SAL_DLLPRIVATE void ImplDeferMoveResize(bool bNewPos, bool bNewSize);
    SAL_DLLPRIVATE void ImplUpdateFramePosition();
    SAL_DLLPRIVATE void ImplUpdateFramePos(SalFrame* pParentFrame);
    SAL_DLLPRIVATE void ImplUpdateClientWindowPos();
    SAL_DLLPRIVATE tools::Long ImplGetBorderWidth() const;
    SAL_DLLPRIVATE tools::Long ImplGetBorderHeight() const;

    // --- Rendering and Invalidation ---
    SAL_DLLPRIVATE SalGraphics* ImplGetFrameGraphics() const;
    SAL_DLLPRIVATE void ImplCallPaint(const vcl::Region* pRegion, ImplPaintFlags nPaintFlags);
    SAL_DLLPRIVATE void ImplCallOverlapPaint();
    SAL_DLLPRIVATE bool ImplHasValidClippingRegion() const;
    SAL_DLLPRIVATE bool ImplShouldPaintImmediately() const;
    SAL_DLLPRIVATE bool ImplCopyArea(vcl::Region& rRegion, const tools::Rectangle& rInitialWinRect);
    SAL_DLLPRIVATE bool ImplCopyBitsRegion(std::unique_ptr<vcl::Region>& rpOverlapRegion,
                                           const tools::Rectangle& rInitialWinRect);
    SAL_DLLPRIVATE void ImplInvalidateMovedWindow(bool bCopyBits,
                                                  const tools::Rectangle& rInitialWinRect,
                                                  std::unique_ptr<vcl::Region>& rpOverlapRegion);
    SAL_DLLPRIVATE void ImplInvalidateGrownWindow(const vcl::Region& rInitialRegion);
    SAL_DLLPRIVATE void ImplInvalidateWindowContent(bool bNewPos, bool bCopyBits,
                                                    const tools::Rectangle& rInitialWinRect,
                                                    std::unique_ptr<vcl::Region>& rpOverlapRegion,
                                                    const vcl::Region& rInitialRegion);
    SAL_DLLPRIVATE void ImplInvalidateParentOrOverlaps(const vcl::Region& rInitialRegion);
    SAL_DLLPRIVATE bool ImplInvalidateVisibleRegions(bool bNewPos, bool bNewSize, bool bCopyBits,
                                                     const tools::Rectangle& rInitialWinRect,
                                                     std::unique_ptr<vcl::Region>& rpOverlapRegion,
                                                     const vcl::Region& rInitialRegion);
    SAL_DLLPRIVATE void ImplExpandInvalidationForNativeWidget(vcl::Region& rInvRegion) const;
    SAL_DLLPRIVATE void ImplInvalidateParentOnHide(vcl::Region& rInvRegion);
    SAL_DLLPRIVATE vcl::Region ImplGetWinClipRegion();

    // --- Visibility and Show/Hide ---
    SAL_DLLPRIVATE void ImplResetReallyVisible();
    SAL_DLLPRIVATE void ImplSetReallyVisible();
    SAL_DLLPRIVATE void ImplCallInitShow();
    SAL_DLLPRIVATE std::optional<bool> ImplHideWindow(ShowFlags nFlags);
    SAL_DLLPRIVATE std::optional<bool> ImplHideCascade(ShowFlags nFlags);
    SAL_DLLPRIVATE vcl::Window* ImplGetVisibilityParent() const;
    SAL_DLLPRIVATE void ImplRaiseOverlapWindow(ShowFlags nFlags);
    SAL_DLLPRIVATE bool ImplUpdateRealVisibility(ShowFlags nFlags);
    SAL_DLLPRIVATE bool ImplShowBorderOrFrame(ShowFlags nFlags);
    SAL_DLLPRIVATE std::optional<bool> ImplShowWindow(ShowFlags nFlags);

    // --- Focus and Activation ---
    SAL_DLLPRIVATE static void ImplCallFocusChangeActivate(vcl::Window* pNewOverlapWindow,
                                                           vcl::Window* pOldOverlapWindow);
    SAL_DLLPRIVATE vcl::Window* ImplGetFirstOverlapWindow();
    SAL_DLLPRIVATE const vcl::Window* ImplGetFirstOverlapWindow() const;
    SAL_DLLPRIVATE bool ImplCanReceiveFocus() const;
    SAL_DLLPRIVATE bool ImplRestoreFocusToWindow();
    SAL_DLLPRIVATE bool ImplSyncDelayedFocus();
    SAL_DLLPRIVATE bool ImplProcessFocusGain();
    SAL_DLLPRIVATE void ImplClearFocus();
    SAL_DLLPRIVATE void ImplDeactivateFocus();
    SAL_DLLPRIVATE void ImplNotifyLostFocus();
    SAL_DLLPRIVATE bool ImplResolveFocusLocally();
    SAL_DLLPRIVATE vcl::Window* ImplResetOverlapFocusState(vcl::Window* pOverlapWindow);
    SAL_DLLPRIVATE void ImplTransferFocusToParent();
    SAL_DLLPRIVATE bool ImplShouldPassFocusToLastWindow() const;
    SAL_DLLPRIVATE bool ImplShouldTransferFocusOnHide(ShowFlags nFlags) const;
    SAL_DLLPRIVATE void ImplCallActivateListeners(vcl::Window*);
    SAL_DLLPRIVATE void ImplCallDeactivateListeners(vcl::Window*);
    SAL_DLLPRIVATE bool ImplHasFocusedChild() const;
    SAL_DLLPRIVATE bool ImplContainsFocus() const;
    SAL_DLLPRIVATE vcl::Window* ImplTransferFocus();
    SAL_DLLPRIVATE void ImplNotifyFocusListeners(NotifyEvent& rNEvt);
    SAL_DLLPRIVATE bool ImplShouldForwardFocusToChild(const NotifyEvent& rNEvt) const;
    SAL_DLLPRIVATE bool ImplIsCompoundControlGainingFocus() const;
    SAL_DLLPRIVATE bool ImplIsCompoundControlLosingFocus() const;
    SAL_DLLPRIVATE bool ImplUpdateCompoundControlFocusGain();
    SAL_DLLPRIVATE bool ImplUpdateCompoundControlFocusLoss();
    SAL_DLLPRIVATE bool ImplIsActivatable() const;
    SAL_DLLPRIVATE bool ImplHasActiveChildFrame(const vcl::Window* pFrameWin) const;
    static SAL_DLLPRIVATE FocusAction ImplResolveFocusAction(vcl::Window* pOldRealWindow,
                                                             vcl::Window* pOldOverlapWindow,
                                                             vcl::Window* pNewRealWindow,
                                                             vcl::Window* pNewOverlapWindow);
    static SAL_DLLPRIVATE FocusAction
    ImplCheckNonActivatableNewWindow(vcl::Window* pNewRealWindow, vcl::Window* pOldOverlapWindow);
    static SAL_DLLPRIVATE FocusAction
    ImplResolveLastDeactivatedWindow(vcl::Window* pNewOverlapWindow);
    static SAL_DLLPRIVATE void ImplDeactivateOldWindows(FocusAction eFocusAction,
                                                        vcl::Window* pOldOverlapWindow,
                                                        vcl::Window* pOldRealWindow);
    static SAL_DLLPRIVATE void ImplActivateNewWindows(FocusAction eFocusAction,
                                                      vcl::Window* pNewOverlapWindow,
                                                      vcl::Window* pNewRealWindow);
    SAL_DLLPRIVATE void ImplRestoreAppFocusWin();
    SAL_DLLPRIVATE void ImplShowFocusRect(ImplWinData* pWinData, const tools::Rectangle& rRect);
    SAL_DLLPRIVATE void ImplShowNativeFocus();

    // --- Events and Input ---
    SAL_DLLPRIVATE bool ImplTestMousePointerSet();
    SAL_DLLPRIVATE static void ImplNewInputContext();
    SAL_DLLPRIVATE bool ImplStopDnd();
    SAL_DLLPRIVATE void ImplStartDnd();
    SAL_DLLPRIVATE bool ImplDelegatePreNotifyToParent(NotifyEvent& rNEvt);
    SAL_DLLPRIVATE bool ImplDispatchDialogControlEvent(NotifyEvent& rNEvt, bool bIsFloatingMode);
    SAL_DLLPRIVATE bool ImplDispatchDialogControlKeyEvent(NotifyEvent& rNEvt, bool bIsFloatingMode);
    SAL_DLLPRIVATE bool ImplDispatchDialogControlFocusEvent(NotifyEvent& rNEvt);
    SAL_DLLPRIVATE bool ImplDispatchDockingMouseEvent(const NotifyEvent& rNEvt,
                                                      ImplDockingWindowWrapper* pWrapper);
    SAL_DLLPRIVATE bool ImplAttemptDockingSequence(const NotifyEvent& rNEvt,
                                                   ImplDockingWindowWrapper* pWrapper);
    SAL_DLLPRIVATE bool ImplCanStartDocking(const MouseEvent* pMEvt,
                                            const ImplDockingWindowWrapper* pWrapper) const;
    SAL_DLLPRIVATE bool ImplDispatchDockingEvent(const NotifyEvent& rNEvt,
                                                 ImplDockingWindowWrapper* pWrapper);
    SAL_DLLPRIVATE void ImplDispatchCompoundControlCommand(const NotifyEvent& rNEvt,
                                                           const CommandEvent* pCEvt);
    SAL_DLLPRIVATE bool ImplDispatchCommandEvent(const NotifyEvent& rNEvt);
    SAL_DLLPRIVATE bool ImplDispatchKeyMouseEvent(const NotifyEvent& rNEvt);
    SAL_DLLPRIVATE void ImplCancelTracking();
    SAL_DLLPRIVATE void ImplCancelTrackingAndPassFocus();
    SAL_DLLPRIVATE void ImplEnableBorderAndMenuBar(bool bEnable);
    SAL_DLLPRIVATE void ImplEnableInputBorderAndMenuBar(bool bEnable);
    SAL_DLLPRIVATE void ImplUpdateEnableState(bool bEnable);
    SAL_DLLPRIVATE void ImplUpdateInputEnableState(bool bEnable);
    SAL_DLLPRIVATE void ImplEnableChildWindows(bool bEnable);
    SAL_DLLPRIVATE void ImplEnableInputChildWindows(bool bEnable);
    SAL_DLLPRIVATE void ImplAlwaysEnableInputChildWindows(bool bAlways);
    SAL_DLLPRIVATE void ImplSetInputState(bool bEnable);
    SAL_DLLPRIVATE void ImplEnableOverlapWindowsInput(bool bEnable,
                                                      const vcl::Window* pExcludeWindow);
    SAL_DLLPRIVATE void ImplEnableFloatingWindowsInput(bool bEnable,
                                                       const vcl::Window* pExcludeWindow);
    SAL_DLLPRIVATE void ImplEnableOwnerDrawWindowsInput(bool bEnable,
                                                        const vcl::Window* pExcludeWindow);

    // --- Settings and State ---
    SAL_DLLPRIVATE void ImplInitResolutionSettings();
    SAL_DLLPRIVATE void ImplUpdateGlobalSettings(AllSettings& rSettings,
                                                 bool bCallHdl = true) const;
    SAL_DLLPRIVATE void ImplUpdateModalCount(int nDelta);

    // --- OutputDevice, Coordinates, and Math ---
    SAL_DLLPRIVATE void ImplPointToLogic(vcl::RenderContext const& rRenderContext, vcl::Font& rFont,
                                         bool bUseRenderContextDPI = false) const;
    SAL_DLLPRIVATE void ImplLogicToPoint(vcl::RenderContext const& rRenderContext,
                                         vcl::Font& rFont) const;
    SAL_DLLPRIVATE AbsoluteScreenPixelRectangle
    ImplOutputToUnmirroredAbsoluteScreenPixel(const tools::Rectangle& rRect) const;
    SAL_DLLPRIVATE tools::Rectangle
    ImplUnmirroredAbsoluteScreenToOutputPixel(const AbsoluteScreenPixelRectangle& rRect) const;
    SAL_DLLPRIVATE tools::Long ImplGetUnmirroredOutOffX() const;
    SAL_DLLPRIVATE bool ImplHasAntiparallelParent() const;
    SAL_DLLPRIVATE tools::Long ImplGetParentDeviceOriginX() const;
    SAL_DLLPRIVATE tools::Long ImplUnmirrorXOffset(tools::Long nMirroredOffset) const;

    // --- Styling, Borders, and Native Rendering ---
    SAL_DLLPRIVATE bool ImplShouldInherit3DLook(const vcl::Window* pParent) const;
    SAL_DLLPRIVATE bool ImplNeedsBorderWindow(WinBits nStyle) const;
    SAL_DLLPRIVATE vcl::Window* ImplCreateBorderWindow(vcl::Window* pParent, WinBits nStyle,
                                                       BorderWindowStyle nBorderTypeStyle);
    SAL_DLLPRIVATE bool ImplIsUndecoratedFloatingWindow(WinBits nStyle,
                                                        SalFrameStyleFlags nFrameStyle) const;
    SAL_DLLPRIVATE SalFrameStyleFlags
    ImplApplyFloatWindowStyle(WinBits nStyle, SalFrameStyleFlags nFrameStyle) const;
    SAL_DLLPRIVATE SalFrameStyleFlags ImplGetBaseFrameStyle(WinBits nStyle) const;
    SAL_DLLPRIVATE SalFrameStyleFlags ImplGetExtendedFrameStyle(WinBits nStyle) const;
    SAL_DLLPRIVATE SalFrameStyleFlags ImplGetDialogFrameStyle() const;
    SAL_DLLPRIVATE bool ImplShouldHaveBorder(WindowBorderStyle nBorderStyle);
    SAL_DLLPRIVATE void ImplSetBorderWindowStyle(WindowBorderStyle nBorderStyle);
    SAL_DLLPRIVATE bool ImplShouldFallbackToParentBackground(const Wallpaper& rBack) const;
    SAL_DLLPRIVATE void ImplEnableChildNativeWidgets(bool bEnable);
    SAL_DLLPRIVATE void ImplUpdateNativeWidgetState(bool bEnable);
    SAL_DLLPRIVATE WinBits ImplApplyBorderAnd3DStyle(WinBits nStyle,
                                                     const vcl::Window* pParent) const;
    SAL_DLLPRIVATE BorderWindowStyle ImplGetBorderWindowStyle(WinBits nStyle) const;
    SAL_DLLPRIVATE bool ImplNeedsSystemChildBorder(WinBits nStyle) const;
    SAL_DLLPRIVATE vcl::Window* ImplInitBorderWindow(vcl::Window* pParent, WinBits nStyle,
                                                     BorderWindowStyle nBorderTypeStyle);
    SAL_DLLPRIVATE SalFrameStyleFlags ImplGetFrameStyle(WinBits nStyle) const;
    SAL_DLLPRIVATE SalFrame* ImplCreateFrame(vcl::Window* pParent,
                                             SystemParentData* pSystemParentData,
                                             SalFrameStyleFlags nFrameStyle);
    SAL_DLLPRIVATE void ImplSetupFrame(SalFrame* pFrame, WinBits nStyle,
                                       vcl::Window* pInitialParent);
    SAL_DLLPRIVATE void ImplInitResolution(vcl::Window* pParent, WinBits nStyle);
    SAL_DLLPRIVATE void ImplInitFrameResolution(vcl::Window* pParent, WinBits nStyle);
    SAL_DLLPRIVATE void ImplInitSettings(WinBits nStyle);
    SAL_DLLPRIVATE void ImplInitFromParentState(vcl::Window* pParent);
    SAL_DLLPRIVATE bool ImplIsMismatchedSubControl() const;

    // --- Scrolling and Gestures ---
    SAL_DLLPRIVATE static void ImplHandleScroll(Scrollable* pHScrl, double nX, Scrollable* pVScrl,
                                                double nY);
    SAL_DLLPRIVATE bool ImplTriggerAutoScroll(Scrollable* pHScrl, Scrollable* pVScrl);
    SAL_DLLPRIVATE bool ImplExecuteWheelScroll(const CommandEvent& rCmd, Scrollable* pHScrl,
                                               Scrollable* pVScrl);
    SAL_DLLPRIVATE bool ImplExecuteGesturePan(const CommandEvent& rCmd, Scrollable* pHScrl,
                                              Scrollable* pVScrl);
    SAL_DLLPRIVATE bool ImplExecuteAutoScroll(const CommandEvent& rCmd, Scrollable* pHScrl,
                                              Scrollable* pVScrl);
    SAL_DLLPRIVATE double ImplCalculateWheelScrollLines(const CommandWheelData* pData);
    SAL_DLLPRIVATE bool ImplExecuteLineScroll(const CommandWheelData* pData, Scrollable* pHScrl,
                                              Scrollable* pVScrl);
    SAL_DLLPRIVATE bool ImplExecutePixelScroll(const CommandEvent& rCmd, Scrollable* pHScrl,
                                               Scrollable* pVScrl);
    SAL_DLLPRIVATE bool ImplExecuteGesturePanScroll(const CommandGesturePanData* pData,
                                                    Scrollable* pHScrl, Scrollable* pVScrl);

    // --- Dialog Control ---
    SAL_DLLPRIVATE bool ImplDlgCtrl(const KeyEvent& rKEvt, bool bKeyInput);
    SAL_DLLPRIVATE bool ImplHasDlgCtrl() const;
    SAL_DLLPRIVATE void ImplDlgCtrlNextWindow();
    SAL_DLLPRIVATE void ImplDlgCtrlFocusChanged(const vcl::Window* pWindow, bool bGetFocus);
    SAL_DLLPRIVATE vcl::Window* ImplFindDlgCtrlWindow(const vcl::Window* pWindow);

    // --- Miscellaneous / Frames / Toolbars / LOK ---
    SAL_DLLPRIVATE SalFrame* ImplFindParentFrame() const;
    SAL_DLLPRIVATE vcl::Window* ImplGetMenuBarWindow() const;
    SAL_DLLPRIVATE void ImplEnableRTL(bool bEnable);
    SAL_DLLPRIVATE tools::Rectangle ImplGetHelpScreenRect() const;
    SAL_DLLPRIVATE void ImplShowBalloonHelp(const HelpEvent& rHEvt);
    SAL_DLLPRIVATE void ImplShowQuickHelp(const HelpEvent& rHEvt);
    SAL_DLLPRIVATE void ImplStartHelp(const HelpEvent& rHEvt);
    SAL_DLLPRIVATE bool ImplIsAccessibleCandidate() const;
    SAL_DLLPRIVATE Size get_ungrouped_preferred_size() const;
};
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
