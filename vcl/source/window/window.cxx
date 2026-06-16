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

#include <sal/log.hxx>
#include <sal/types.h>
#include <rtl/strbuf.hxx>
#include <o3tl/float_int_conversion.hxx>
#include <o3tl/string_view.hxx>
#include <comphelper/configuration.hxx>
#include <comphelper/lok.hxx>
#include <comphelper/OAccessible.hxx>
#include <unotools/fontdefs.hxx>

#include <vcl/builder.hxx>
#include <vcl/cursor.hxx>
#include <vcl/dndlistenercontainer.hxx>
#include <vcl/layout.hxx>
#include <vcl/rendercontext/GetDefaultFontFlags.hxx>
#include <vcl/taskpanelist.hxx>
#include <vcl/toolkit/dialog.hxx>
#include <vcl/toolkit/fixed.hxx>
#include <vcl/uitest/uiobject.hxx>
#include <vcl/scrollable.hxx>
#include <vcl/toolkit/unowrap.hxx>
#include <vcl/window.hxx>
#include <vcl/CoordinateMapper.hxx>

#include <brdwin.hxx>
#include <clipping.hxx>
#include <clipping_window.hxx>
#include <dndeventdispatcher.hxx>
#include <helpwin.hxx>
#include <impfontcache.hxx>
#include <salframe.hxx>
#include <salgdi.hxx>
#include <salinst.hxx>
#include <salobj.hxx>
#include <scrwnd.hxx>
#include <svdata.hxx>
#include <toolbox.h>
#include <window.h>
#include <ImplOutDevData.hxx>

#ifdef _WIN32 // see #140456#
#include <win/salframe.h>
#endif

#include "impldockingwrapper.hxx"

#include <com/sun/star/accessibility/AccessibleRole.hpp>
#include <com/sun/star/accessibility/AccessibleStateType.hpp>
#include <com/sun/star/accessibility/XAccessibleEditableText.hpp>
#include <com/sun/star/awt/XVclWindowPeer.hpp>

using namespace ::com::sun::star::uno;
using namespace ::com::sun::star::lang;
using namespace ::com::sun::star::datatransfer::clipboard;
using namespace ::com::sun::star::datatransfer::dnd;

namespace vcl {

Window::Window( WindowType eType )
    : mpWindowImpl(new WindowImpl( *this, eType ))
{
    // true: this outdev will be mirrored if RTL window layout (UI mirroring) is globally active
    mpWindowImpl->mxOutDev->mbEnableRTL = AllSettings::GetLayoutRTL();
}

Window::Window( vcl::Window* pParent, WinBits nStyle )
    : mpWindowImpl(new WindowImpl( *this, WindowType::WINDOW ))
{
    // true: this outdev will be mirrored if RTL window layout (UI mirroring) is globally active
    mpWindowImpl->mxOutDev->mbEnableRTL = AllSettings::GetLayoutRTL();

    ImplInit( pParent, nStyle, nullptr );
}

#if OSL_DEBUG_LEVEL > 0
static OString lcl_createWindowInfo(const vcl::Window* pWindow)
{
    // skip border windows, they do not carry information that
    // would help with diagnosing the problem
    const vcl::Window* pTempWin( pWindow );
    while ( pTempWin && pTempWin->GetType() == WindowType::BORDERWINDOW ) {
        pTempWin = pTempWin->GetWindow( GetWindowType::FirstChild );
    }
    // check if pTempWin is not null, otherwise use the
    // original address
    if ( pTempWin ) {
        pWindow = pTempWin;
    }

    return OString::Concat(" ") +
       typeid( *pWindow ).name() +
       "(" +
        OUStringToOString(
            pWindow->GetText(),
            RTL_TEXTENCODING_UTF8
            ) +
       ")";
}
#endif

bool Window::ImplShouldNotifyAccessibleParent() const
{
    return !IsNativeFrame()
        && mpWindowImpl->mbReallyVisible
        && ImplIsAccessibleCandidate()
        && GetAccessibleParentWindow();
}

static void lcl_ShutdownDragAndDrop(WindowImpl* pImpl)
{
    if (!pImpl->mbFrame || !pImpl->mpFrameData)
        return;

    try
    {
        auto& rFrameData = pImpl->mpFrameData;

        // deregister drop target listener
        if (rFrameData->mxDropTargetListener.is())
        {
            Reference<XDragGestureRecognizer> xDragGestureRecognizer(rFrameData->mxDragSource, UNO_QUERY);
            if (xDragGestureRecognizer.is())
            {
                xDragGestureRecognizer->removeDragGestureListener(rFrameData->mxDropTargetListener);
            }

            rFrameData->mxDropTarget->removeDropTargetListener(rFrameData->mxDropTargetListener);
            rFrameData->mxDropTargetListener.clear();
        }

        // shutdown drag and drop for this frame window
        Reference<XComponent> xComponent(rFrameData->mxDropTarget, UNO_QUERY);
        if (xComponent.is())
            xComponent->dispose();
    }
    catch (const Exception&)
    {
        // can be safely ignored here.
    }
}

#if OSL_DEBUG_LEVEL > 0
void Window::ImplAssertNoWindowLeaks()
{
    OStringBuffer aErrorStr;
    bool bError = false;
    vcl::Window* pTempWin;
    ImplSVData* pSVData = ImplGetSVData();

    // Check for live children
    if (mpWindowImpl->mpHierarchy->mpFirstChild)
    {
        OStringBuffer aTempStr = "Window (" + lcl_createWindowInfo(this) +
                                 ") with live children destroyed: ";
        pTempWin = mpWindowImpl->mpHierarchy->mpFirstChild;
        while (pTempWin)
        {
            aTempStr.append(lcl_createWindowInfo(pTempWin));
            pTempWin = pTempWin->mpWindowImpl->mpHierarchy->mpNext;
        }
        OSL_FAIL(aTempStr.getStr());
        Application::Abort(OStringToOUString(aTempStr, RTL_TEXTENCODING_UTF8));
    }

    // Check for live overlapping windows
    if (mpWindowImpl->mpFrameData != nullptr)
    {
        pTempWin = mpWindowImpl->mpFrameData->mpFirstOverlap;
        while (pTempWin)
        {
            if (ImplIsRealParentPath(pTempWin))
            {
                bError = true;
                aErrorStr.append(lcl_createWindowInfo(pTempWin));
            }
            pTempWin = pTempWin->mpWindowImpl->mpHierarchy->mpNextOverlap;
        }
        if (bError)
        {
            OString aTempStr = "Window (" + lcl_createWindowInfo(this) +
                               ") with live SystemWindows destroyed: " + aErrorStr.makeStringAndClear();
            OSL_FAIL(aTempStr.getStr());
            Application::Abort(OStringToOUString(aTempStr, RTL_TEXTENCODING_UTF8));
        }
    }

    // Check SystemWindows
    bError = false;
    pTempWin = pSVData->maFrameData.mpFirstFrame;
    while (pTempWin)
    {
        if (ImplIsRealParentPath(pTempWin))
        {
            bError = true;
            aErrorStr.append(lcl_createWindowInfo(pTempWin));
        }
        pTempWin = pTempWin->mpWindowImpl->mpFrameData->mpNextFrame;
    }

    if (bError)
    {
        OString aTempStr = "Window (" + lcl_createWindowInfo(this) +
                           ") with live SystemWindows destroyed: " + aErrorStr.makeStringAndClear();
        OSL_FAIL(aTempStr.getStr());
        Application::Abort(OStringToOUString(aTempStr, RTL_TEXTENCODING_UTF8));
    }

    // TaskPane check
    vcl::Window* pMyParent = GetParent();
    SystemWindow* pMySysWin = nullptr;

    while (pMyParent)
    {
        if (pMyParent->IsSystemWindow())
            pMySysWin = dynamic_cast<SystemWindow*>(pMyParent);

        pMyParent = pMyParent->GetParent();
    }

    if (pMySysWin && pMySysWin->ImplIsInTaskPaneList(this))
    {
        OString aTempStr = "Window (" + lcl_createWindowInfo(this) + ") still in TaskPanelList!";
        OSL_FAIL(aTempStr.getStr());
        Application::Abort(OStringToOUString(aTempStr, RTL_TEXTENCODING_UTF8));
    }
}
#endif

void Window::ImplRemoveFromTaskPaneList()
{
    if (!mpWindowImpl->mbIsInTaskPaneList)
        return;

    vcl::Window* pMyParent = GetParent();
    SystemWindow* pMySysWin = nullptr;

    while (pMyParent)
    {
        if (pMyParent->IsSystemWindow())
        {
            pMySysWin = dynamic_cast<SystemWindow*>(pMyParent);
        }
        pMyParent = pMyParent->GetParent();
    }

    bool bInList = pMySysWin && pMySysWin->ImplIsInTaskPaneList(this);

    if (bInList)
    {
        pMySysWin->GetTaskPaneList()->RemoveWindow(this);
    }

    SAL_WARN_IF(!bInList, "vcl", "Window (" << GetText() << ") not found in TaskPanelList");
}

void Window::ImplTransferFocus(bool bHasFocusedChild)
{
    ImplSVData* pSVData = ImplGetSVData();
    vcl::Window* pOverlapWindow = ImplGetFirstOverlapWindow();

    // If we or a child have focus, pass it to another window
    if (pSVData->mpWinData->mpFocusWin == this || bHasFocusedChild)
    {
        if (mpWindowImpl->mbFrame)
        {
            pSVData->mpWinData->mpFocusWin = nullptr;
            pOverlapWindow->mpWindowImpl->mpLastFocusWindow = nullptr;
        }
        else
        {
            vcl::Window* pParent = GetParent();
            vcl::Window* pBorderWindow = mpWindowImpl->mpBorderWindow;

            // when windows overlap, give focus to the parent of the next FrameWindow
            if (pBorderWindow)
            {
                if (pBorderWindow->ImplIsOverlapWindow())
                    pParent = pBorderWindow->mpWindowImpl->mpOverlapWindow;
            }
            else if (ImplIsOverlapWindow())
            {
                pParent = mpWindowImpl->mpOverlapWindow;
            }

            if (pParent && pParent->IsEnabled() && pParent->IsInputEnabled() && !pParent->IsInModalMode())
                pParent->GrabFocus();
            else
                mpWindowImpl->mpFrameWindow->GrabFocus();

            // If the focus was set back to 'this' set it to nothing
            if (pSVData->mpWinData->mpFocusWin == this)
            {
                pSVData->mpWinData->mpFocusWin = nullptr;
                pOverlapWindow->mpWindowImpl->mpLastFocusWindow = nullptr;
            }
        }
    }

    // Clear any stale cached focus references in the overlap window
    if (pOverlapWindow != nullptr && pOverlapWindow->mpWindowImpl->mpLastFocusWindow == this)
        pOverlapWindow->mpWindowImpl->mpLastFocusWindow = nullptr;
}

void Window::dispose()
{
    assert( mpWindowImpl );
    assert( !mpWindowImpl->mbInDispose ); // should only be called from disposeOnce()
    assert( (!mpWindowImpl->mpHierarchy->mpParent ||
             mpWindowImpl->mpHierarchy->mpParent->mpWindowImpl) &&
            "vcl::Window child should have its parent disposed first" );

    // remove Key and Mouse events issued by Application::PostKey/MouseEvent
    Application::RemoveMouseAndKeyEvents( this );

    // Dispose of the canvas implementation (which, currently, has an
    // own wrapper window as a child to this one.
    GetOutDev()->ImplDisposeCanvas();

    mpWindowImpl->mbInDispose = true;

    CallEventListeners( VclEventId::ObjectDying );

    // do not send child events for frames that were registered as native frames
    if (ImplShouldNotifyAccessibleParent())
        GetAccessibleParentWindow()->CallEventListeners(VclEventId::WindowChildDestroyed, this);

    // remove associated data structures from dockingmanager
    ImplGetDockingManager()->RemoveWindow( this );

    // remove ownerdraw decorated windows from list in the top-most frame window
    if( (GetStyle() & WB_OWNERDRAWDECORATION) && mpWindowImpl->mbFrame )
    {
        ::std::vector< VclPtr<vcl::Window> >& rList = ImplGetOwnerDrawList();
        auto p = ::std::find( rList.begin(), rList.end(), VclPtr<vcl::Window>(this) );
        if( p != rList.end() )
            rList.erase( p );
    }

    // shutdown drag and drop
    if( mpWindowImpl->mxDNDListenerContainer.is() )
        mpWindowImpl->mxDNDListenerContainer->dispose();

    lcl_ShutdownDragAndDrop(mpWindowImpl.get());

    UnoWrapperBase* pWrapper = UnoWrapperBase::GetUnoWrapper( false );
    if ( pWrapper )
        pWrapper->WindowDestroyed( this );

    if (mpWindowImpl->mpAccessible.is())
    {
        mpWindowImpl->mpAccessible->dispose();
        mpWindowImpl->mpAccessible.clear();
    }

    if (mpWindowImpl->mpAccessibleInfos)
        mpWindowImpl->mpAccessibleInfos->pAccessibleParent.clear();

    ImplSVData* pSVData = ImplGetSVData();

    if ( ImplGetSVHelpData().mpHelpWin && (ImplGetSVHelpData().mpHelpWin->GetParent() == this) )
        ImplDestroyHelpWindow( true );

    SAL_WARN_IF(pSVData->mpWinData->mpTrackWin.get() == this, "vcl.window",
                "Window::~Window(): Window is in TrackingMode");
    SAL_WARN_IF(IsMouseCaptured(), "vcl.window",
                "Window::~Window(): Window has the mouse captured");

    // due to old compatibility
    if (pSVData->mpWinData->mpTrackWin == this)
        EndTracking();

    if (IsMouseCaptured())
        ReleaseMouse();

#if OSL_DEBUG_LEVEL > 0
    ImplAssertNoWindowLeaks();
#endif

    ImplRemoveFromTaskPaneList();

    // remove from size-group if necessary
    remove_from_all_size_groups();

    // clear mnemonic labels
    std::vector<VclPtr<FixedText> > aMnemonicLabels(list_mnemonic_labels());
    for (auto const& mnemonicLabel : aMnemonicLabels)
    {
        remove_mnemonic_label(mnemonicLabel);
    }

    // hide window in order to trigger the Paint-Handling
    Hide();

    // EndExtTextInputMode
    if (pSVData->mpWinData->mpExtTextInputWin == this)
    {
        EndExtTextInput();
        if (pSVData->mpWinData->mpExtTextInputWin == this)
            pSVData->mpWinData->mpExtTextInputWin = nullptr;
    }

    // check if the focus window is our child
    bool bHasFocusedChild = false;
    if (pSVData->mpWinData->mpFocusWin && ImplIsRealParentPath(pSVData->mpWinData->mpFocusWin))
    {
        // #122232#, this must not happen and is an application bug ! but we try some cleanup to hopefully avoid crashes, see below
        bHasFocusedChild = true;
#if OSL_DEBUG_LEVEL > 0
        OUString aTempStr = "Window (" + GetText() +
                ") with focused child window destroyed ! THIS WILL LEAD TO CRASHES AND MUST BE FIXED !";
        SAL_WARN( "vcl", aTempStr );
        Application::Abort(aTempStr);
#endif
    }

    ImplTransferFocus(bHasFocusedChild);

    // reset hint for DefModalDialogParent
    if( pSVData->maFrameData.mpActiveApplicationFrame == this )
        pSVData->maFrameData.mpActiveApplicationFrame = nullptr;

    // reset hint of what was the last wheeled window
    if (pSVData->mpWinData->mpLastWheelWindow == this)
        pSVData->mpWinData->mpLastWheelWindow = nullptr;

    // reset marked windows
    if ( mpWindowImpl->mpFrameData != nullptr )
    {
        if ( mpWindowImpl->mpFrameData->mpFocusWin == this )
            mpWindowImpl->mpFrameData->mpFocusWin = nullptr;
        if ( mpWindowImpl->mpFrameData->mpMouseMoveWin == this )
            mpWindowImpl->mpFrameData->mpMouseMoveWin = nullptr;
        if ( mpWindowImpl->mpFrameData->mpMouseDownWin == this )
            mpWindowImpl->mpFrameData->mpMouseDownWin = nullptr;
    }

    // reset Deactivate-Window
    if (pSVData->mpWinData->mpLastDeacWin == this)
        pSVData->mpWinData->mpLastDeacWin = nullptr;

    if ( mpWindowImpl->mbFrame && mpWindowImpl->mpFrameData )
    {
        if ( mpWindowImpl->mpFrameData->mnFocusId )
            Application::RemoveUserEvent( mpWindowImpl->mpFrameData->mnFocusId );
        mpWindowImpl->mpFrameData->mnFocusId = nullptr;
        if ( mpWindowImpl->mpFrameData->mnMouseMoveId )
            Application::RemoveUserEvent( mpWindowImpl->mpFrameData->mnMouseMoveId );
        mpWindowImpl->mpFrameData->mnMouseMoveId = nullptr;
    }

    // release SalGraphics
    VclPtr<OutputDevice> pOutDev = GetOutDev();
    pOutDev->ReleaseGraphics();

    // remove window from the lists
    ImplRemoveWindow( true );

    // de-register as "top window child" at our parent, if necessary
    if ( mpWindowImpl->mbFrame )
    {
        bool bIsTopWindow
            = mpWindowImpl->mpWinData && (mpWindowImpl->mpWinData->mnIsTopWindow == 1);
        if ( mpWindowImpl->mpHierarchy->mpRealParent && bIsTopWindow )
        {
            ImplWinData* pParentWinData = mpWindowImpl->mpHierarchy->mpRealParent->ImplGetWinData();

            auto myPos = ::std::find( pParentWinData->maTopWindowChildren.begin(),
                pParentWinData->maTopWindowChildren.end(), VclPtr<vcl::Window>(this) );
            SAL_WARN_IF( myPos == pParentWinData->maTopWindowChildren.end(), "vcl.window", "Window::~Window: inconsistency in top window chain!" );
            if ( myPos != pParentWinData->maTopWindowChildren.end() )
                pParentWinData->maTopWindowChildren.erase( myPos );
        }
    }

    mpWindowImpl->mpWinData.reset();

    // remove BorderWindow or Frame window data
    mpWindowImpl->mpBorderWindow.disposeAndClear();
    if ( mpWindowImpl->mbFrame )
    {
        if ( pSVData->maFrameData.mpFirstFrame == this )
            pSVData->maFrameData.mpFirstFrame = mpWindowImpl->mpFrameData->mpNextFrame;
        else
        {
            sal_Int32 nWindows = 0;
            vcl::Window* pSysWin = pSVData->maFrameData.mpFirstFrame;
            while ( pSysWin && pSysWin->mpWindowImpl->mpFrameData->mpNextFrame.get() != this )
            {
                pSysWin = pSysWin->mpWindowImpl->mpFrameData->mpNextFrame;
                nWindows++;
            }

            if ( pSysWin )
            {
                assert (mpWindowImpl->mpFrameData->mpNextFrame.get() != pSysWin);
                pSysWin->mpWindowImpl->mpFrameData->mpNextFrame = mpWindowImpl->mpFrameData->mpNextFrame;
            }
            else // if it is not in the list, we can't remove it.
                SAL_WARN("vcl.window", "Window " << this << " marked as frame window, "
                         "is missing from list of " << nWindows << " frames");
        }
        if (mpWindowImpl->mpFrame) // otherwise exception during init
        {
            mpWindowImpl->mpFrame->SetCallback( nullptr, nullptr );
            pSVData->mpDefInst->DestroyFrame( mpWindowImpl->mpFrame );
        }
        assert (mpWindowImpl->mpFrameData->mnFocusId == nullptr);
        assert (mpWindowImpl->mpFrameData->mnMouseMoveId == nullptr);

        mpWindowImpl->mpFrameData->mpBuffer.disposeAndClear();
        delete mpWindowImpl->mpFrameData;
        mpWindowImpl->mpFrameData = nullptr;
    }

    if (mpWindowImpl->mxWindowPeer)
        mpWindowImpl->mxWindowPeer->dispose();

    // should be the last statements
    mpWindowImpl.reset();

    pOutDev.disposeAndClear();
    // just to make loplugin:vclwidgets happy
    VclReferenceBase::dispose();
}

Window::~Window()
{
    disposeOnce();
}

// We will eventually being removing the inheritance of OutputDevice
// from Window. It will be replaced with a transient relationship such
// that the OutputDevice is only live for the scope of the Paint method.
// In the meantime this can help move us towards a Window use an
// OutputDevice, not being one.

::OutputDevice const* Window::GetOutDev() const
{
    return mpWindowImpl ? mpWindowImpl->mxOutDev.get() : nullptr;
}

::OutputDevice* Window::GetOutDev()
{
    return mpWindowImpl ? mpWindowImpl->mxOutDev.get() : nullptr;
}

Size Window::GetOptimalSize() const { return Size(); }

void Window::ImplAdjustNWFSizes()
{
    for (Window* pWin = GetWindow(GetWindowType::FirstChild); pWin;
         pWin = pWin->GetWindow(GetWindowType::Next))
        pWin->ImplAdjustNWFSizes();
}

const Font& Window::GetFont() const { return GetOutDev()->GetFont(); }
void Window::SetFont(Font const& font) { return GetOutDev()->SetFont(font); }

float Window::approximate_char_width() const { return GetOutDev()->approximate_char_width(); }

const Wallpaper& Window::GetBackground() const { return GetOutDev()->GetBackground(); }
bool Window::IsBackground() const { return GetOutDev()->IsBackground(); }
tools::Long Window::GetTextHeight() const { return GetOutDev()->GetTextHeight(); }
tools::Long Window::GetTextWidth(const OUString& rStr, sal_Int32 nIndex, sal_Int32 nLen,
                                 vcl::text::TextLayoutCache const* pCache,
                                 SalLayoutGlyphs const* const pLayoutCache) const
{
    return GetOutDev()->GetTextWidth(rStr, nIndex, nLen, pCache, pLayoutCache);
}
float Window::approximate_digit_width() const { return GetOutDev()->approximate_digit_width(); }

bool Window::IsNativeControlSupported(ControlType nType, ControlPart nPart) const
{
    return GetOutDev()->IsNativeControlSupported(nType, nPart);
}

bool Window::GetNativeControlRegion(ControlType nType, ControlPart nPart,
                                    const tools::Rectangle& rControlRegion, ControlState nState,
                                    const ImplControlValue& aValue,
                                    tools::Rectangle& rNativeBoundingRegion,
                                    tools::Rectangle& rNativeContentRegion) const
{
    return GetOutDev()->GetNativeControlRegion(nType, nPart, rControlRegion, nState, aValue,
                                               rNativeBoundingRegion, rNativeContentRegion);
}

Size Window::GetOutputSizePixel() const
{
    if (!mpWindowImpl)
    {
        return Size();
    }

    return GetOutDev()->GetOutputSizePixel();
}

tools::Rectangle Window::GetOutputRectPixel() const { return GetOutDev()->GetOutputRectPixel(); }

void Window::SetTextLineColor() { GetOutDev()->SetTextLineColor(); }
void Window::SetTextLineColor(const Color& rColor) { GetOutDev()->SetTextLineColor(rColor); }
void Window::SetOverlineColor() { GetOutDev()->SetOverlineColor(); }
void Window::SetOverlineColor(const Color& rColor) { GetOutDev()->SetOverlineColor(rColor); }
void Window::SetTextFillColor() { GetOutDev()->SetTextFillColor(); }
void Window::SetTextFillColor(const Color& rColor) { GetOutDev()->SetTextFillColor(rColor); }
const MapMode& Window::GetMapMode() const { return GetOutDev()->GetMapMode(); }
void Window::SetBackground() { GetOutDev()->SetBackground(); }
void Window::SetBackground(const Wallpaper& rBackground)
{
    GetOutDev()->SetBackground(rBackground);
}
void Window::SetMappingPolicy(vcl::MappingPolicy ePolicy)
{
    GetOutDev()->SetMappingPolicy(ePolicy);
}
vcl::MappingPolicy Window::GetMappingPolicy() const { return GetOutDev()->GetMappingPolicy(); }

void Window::SetTextColor(const Color& rColor) { GetOutDev()->SetTextColor(rColor); }
const Color& Window::GetTextColor() const { return GetOutDev()->GetTextColor(); }
const Color& Window::GetTextLineColor() const { return GetOutDev()->GetTextLineColor(); }

bool Window::IsTextLineColor() const { return GetOutDev()->IsTextLineColor(); }

Color Window::GetTextFillColor() const { return GetOutDev()->GetTextFillColor(); }

bool Window::IsTextFillColor() const { return GetOutDev()->IsTextFillColor(); }

const Color& Window::GetOverlineColor() const { return GetOutDev()->GetOverlineColor(); }
bool Window::IsOverlineColor() const { return GetOutDev()->IsOverlineColor(); }
void Window::SetTextAlign(TextAlign eAlign) { GetOutDev()->SetTextAlign(eAlign); }

float Window::GetDPIScaleFactor() const { return GetOutDev()->GetDPIScaleFactor(); }
tools::Long Window::GetDeviceOriginX() const { return GetOutDev()->GetDeviceOriginX(); }
tools::Long Window::GetDeviceOriginY() const { return GetOutDev()->GetDeviceOriginY(); }
void Window::SetMapMode() { GetOutDev()->SetMapMode(); }
void Window::SetMapMode(const MapMode& rNewMapMode) { GetOutDev()->SetMapMode(rNewMapMode); }
bool Window::IsRTLEnabled() const { return GetOutDev()->IsRTLEnabled(); }
TextAlign Window::GetTextAlign() const { return GetOutDev()->GetTextAlign(); }
const AllSettings& Window::GetSettings() const { return GetOutDev()->GetSettings(); }

tools::Rectangle Window::GetTextRect(const tools::Rectangle& rRect, const OUString& rStr,
                                     DrawTextFlags nStyle, TextRectInfo* pInfo,
                                     const vcl::TextLayoutCommon* _pTextLayout) const
{
    return GetOutDev()->GetTextRect(rRect, rStr, nStyle, pInfo, _pTextLayout);
}

void Window::SetSettings(const AllSettings& rSettings) { GetOutDev()->SetSettings(rSettings); }
void Window::SetSettings(const AllSettings& rSettings, bool bChild)
{
    static_cast<vcl::WindowOutputDevice*>(GetOutDev())->SetSettings(rSettings, bChild);
}

Color Window::GetBackgroundColor() const { return GetOutDev()->GetBackgroundColor(); }

void Window::EnableRTL(bool bEnable) { GetOutDev()->EnableRTL(bEnable); }

void Window::FlashWindow() const
{
    vcl::Window* pMyParent = ImplGetTopmostFrameWindow();

    if (pMyParent && pMyParent->mpWindowImpl)
        pMyParent->mpWindowImpl->mpFrame->FlashWindow();
}

void Window::SetTaskBarProgress(int nCurrentProgress)
{
    vcl::Window* pMyParent = ImplGetTopmostFrameWindow();

    if (pMyParent && pMyParent->mpWindowImpl)
        pMyParent->mpWindowImpl->mpFrame->SetTaskBarProgress(nCurrentProgress);
}

void Window::SetTaskBarState(VclTaskBarStates eTaskBarState)
{
    vcl::Window* pMyParent = ImplGetTopmostFrameWindow();

    if (pMyParent && pMyParent->mpWindowImpl)
        pMyParent->mpWindowImpl->mpFrame->SetTaskBarState(eTaskBarState);
}
static sal_Int32 CountDPIScaleFactor(sal_Int32 nDPI)
{
#ifndef MACOSX
    // Setting of HiDPI is unfortunately all only a heuristic; and to add
    // insult to an injury, the system is constantly lying to us about
    // the DPI and whatnot
    // eg. fdo#77059 - set the value from which we do consider the
    // screen HiDPI to greater than 168
    if (nDPI > 216)      // 96 * 2   + 96 / 4
        return 250;
    else if (nDPI > 168) // 96 * 2   - 96 / 4
        return 200;
    else if (nDPI > 120) // 96 * 1.5 - 96 / 4
        return 150;
#else
    (void)nDPI;
#endif

    return 100;
}

void Window::ImplInit( vcl::Window* pParent, WinBits nStyle, SystemParentData* pSystemParentData )
{
    SAL_WARN_IF( !mpWindowImpl->mbFrame && !pParent && GetType() != WindowType::FIXEDIMAGE, "vcl.window",
        "Window::Window(): pParent == NULL" );

    ImplSVData* pSVData = ImplGetSVData();
    vcl::Window*     pRealParent = pParent;

    // inherit 3D look
    if ( !mpWindowImpl->mbOverlapWin && pParent && (pParent->GetStyle() & WB_3DLOOK) )
        nStyle |= WB_3DLOOK;

    // create border window if necessary
    if ( !mpWindowImpl->mbFrame && !mpWindowImpl->mbBorderWin && !mpWindowImpl->mpBorderWindow
         && (nStyle & (WB_BORDER | WB_SYSTEMCHILDWINDOW) ) )
    {
        BorderWindowStyle nBorderTypeStyle = BorderWindowStyle::NONE;
        if( nStyle & WB_SYSTEMCHILDWINDOW )
        {
            // handle WB_SYSTEMCHILDWINDOW
            // these should be analogous to a top level frame; meaning they
            // should have a border window with style BorderWindowStyle::Frame
            // which controls their size
            nBorderTypeStyle |= BorderWindowStyle::Frame;
            nStyle |= WB_BORDER;
        }
        VclPtrInstance<ImplBorderWindow> pBorderWin( pParent, nStyle & (WB_BORDER | WB_DIALOGCONTROL | WB_NODIALOGCONTROL), nBorderTypeStyle );
        static_cast<vcl::Window*>(pBorderWin)->mpWindowImpl->mpClientWindow = this;
        pBorderWin->GetBorder( mpWindowImpl->mnLeftBorder, mpWindowImpl->mnTopBorder, mpWindowImpl->mnRightBorder, mpWindowImpl->mnBottomBorder );
        mpWindowImpl->mpBorderWindow  = pBorderWin;
        pParent = mpWindowImpl->mpBorderWindow;
    }
    else if( !mpWindowImpl->mbFrame && ! pParent )
    {
        mpWindowImpl->mbOverlapWin  = true;
        mpWindowImpl->mbFrame = true;
    }

    // insert window in list
    ImplInsertWindow( pParent );
    mpWindowImpl->mnStyle = nStyle;

    if( pParent && ! mpWindowImpl->mbFrame )
        mpWindowImpl->mxOutDev->mbEnableRTL = AllSettings::GetLayoutRTL();

    // test for frame creation
    if ( mpWindowImpl->mbFrame )
    {
        // create frame
        SalFrameStyleFlags nFrameStyle = SalFrameStyleFlags::NONE;

        if ( nStyle & WB_MOVEABLE )
            nFrameStyle |= SalFrameStyleFlags::MOVEABLE;
        if ( nStyle & WB_SIZEABLE )
            nFrameStyle |= SalFrameStyleFlags::SIZEABLE;
        if ( nStyle & WB_CLOSEABLE )
            nFrameStyle |= SalFrameStyleFlags::CLOSEABLE;
        if ( nStyle & WB_APP )
            nFrameStyle |= SalFrameStyleFlags::DEFAULT;
        // check for undecorated floating window
        if( // 1. floating windows that are not moveable/sizeable (only closeable allowed)
            ( !(nFrameStyle & ~SalFrameStyleFlags::CLOSEABLE) &&
            ( mpWindowImpl->mbFloatWin || ((GetType() == WindowType::BORDERWINDOW) && static_cast<ImplBorderWindow*>(this)->mbFloatWindow) || (nStyle & WB_SYSTEMFLOATWIN) ) ) ||
            // 2. borderwindows of floaters with ownerdraw decoration
            ((GetType() == WindowType::BORDERWINDOW) && static_cast<ImplBorderWindow*>(this)->mbFloatWindow && (nStyle & WB_OWNERDRAWDECORATION) ) )
        {
            nFrameStyle = SalFrameStyleFlags::FLOAT;
            if( nStyle & WB_OWNERDRAWDECORATION )
                nFrameStyle |= SalFrameStyleFlags::OWNERDRAWDECORATION | SalFrameStyleFlags::NOSHADOW;
        }
        else if( mpWindowImpl->mbFloatWin )
            nFrameStyle |= SalFrameStyleFlags::TOOLWINDOW;

        if( nStyle & WB_INTROWIN )
            nFrameStyle |= SalFrameStyleFlags::INTRO;
        if( nStyle & WB_TOOLTIPWIN )
            nFrameStyle |= SalFrameStyleFlags::TOOLTIP;

        if( nStyle & WB_NOSHADOW )
            nFrameStyle |= SalFrameStyleFlags::NOSHADOW;

        if( nStyle & WB_SYSTEMCHILDWINDOW )
            nFrameStyle |= SalFrameStyleFlags::SYSTEMCHILD;

        switch (mpWindowImpl->meType)
        {
            case WindowType::DIALOG:
            case WindowType::TABDIALOG:
            case WindowType::MODELESSDIALOG:
            case WindowType::MESSBOX:
            case WindowType::INFOBOX:
            case WindowType::WARNINGBOX:
            case WindowType::ERRORBOX:
            case WindowType::QUERYBOX:
                nFrameStyle |= SalFrameStyleFlags::DIALOG;
                break;
            default:
                break;
        }

        // tdf#144624 for the DefaultWindow, which is never visible, don't
        // create an icon for it so construction of a DefaultWindow cannot
        // trigger creation of a VirtualDevice which itself requires a
        // DefaultWindow to exist
        if( nStyle & WB_DEFAULTWIN )
            nFrameStyle |= SalFrameStyleFlags::NOICON;

        SalFrame* pParentFrame = nullptr;
        if ( pParent )
            pParentFrame = pParent->mpWindowImpl->mpFrame;
        SalFrame* pFrame;
        if ( pSystemParentData )
            pFrame = pSVData->mpDefInst->CreateChildFrame( pSystemParentData, nFrameStyle | SalFrameStyleFlags::PLUG );
        else
            pFrame = pSVData->mpDefInst->CreateFrame( pParentFrame, nFrameStyle );
        if ( !pFrame )
        {
            // do not abort but throw an exception, may be the current thread terminates anyway (plugin-scenario)
            throw RuntimeException(
                u"Could not create system window!"_ustr,
                Reference< XInterface >() );
        }

        pFrame->SetCallback( this, ImplWindowFrameProc );

        // set window frame data
        mpWindowImpl->mpFrameData     = new ImplFrameData( this );
        mpWindowImpl->mpFrame         = pFrame;
        mpWindowImpl->mpFrameWindow   = this;
        mpWindowImpl->mpOverlapWindow = this;

        if (!(nStyle & WB_DEFAULTWIN) && mpWindowImpl->mbDoubleBufferingRequested)
            RequestDoubleBuffering(true);

        if ( pRealParent && IsTopWindow() )
        {
            ImplWinData* pParentWinData = pRealParent->ImplGetWinData();
            pParentWinData->maTopWindowChildren.emplace_back(this );
        }
    }

    // init data
    mpWindowImpl->mpHierarchy->mpRealParent = pRealParent;

    // #99318: make sure fontcache and list is available before call to SetSettings
    mpWindowImpl->mxOutDev->mxFontCollection = mpWindowImpl->mpFrameData->mxFontCollection;
    mpWindowImpl->mxOutDev->mxFontCache = mpWindowImpl->mpFrameData->mxFontCache;

    if ( mpWindowImpl->mbFrame )
    {
        if ( pParent )
        {
            mpWindowImpl->mpFrameData->mnDPIX     = pParent->mpWindowImpl->mpFrameData->mnDPIX;
            mpWindowImpl->mpFrameData->mnDPIY     = pParent->mpWindowImpl->mpFrameData->mnDPIY;
        }
        else
        {
            if (auto* pGraphics = GetOutDev()->GetGraphics())
            {
                pGraphics->GetResolution(mpWindowImpl->mpFrameData->mnDPIX,
                                         mpWindowImpl->mpFrameData->mnDPIY);
            }
        }

        // add ownerdraw decorated frame windows to list in the top-most frame window
        // so they can be hidden on lose focus
        if( nStyle & WB_OWNERDRAWDECORATION )
            ImplGetOwnerDrawList().emplace_back(this );

        // delay settings initialization until first "real" frame
        // this relies on the IntroWindow not needing any system settings
        if ( !pSVData->maAppData.mbSettingsInit &&
             ! (nStyle & (WB_INTROWIN|WB_DEFAULTWIN))
             )
        {
            // side effect: ImplUpdateGlobalSettings does an ImplGetFrame()->UpdateSettings
            ImplUpdateGlobalSettings( *pSVData->maAppData.mxSettings );
            mpWindowImpl->mxOutDev->SetSettings( *pSVData->maAppData.mxSettings );
            pSVData->maAppData.mbSettingsInit = true;
        }

        // If we create a Window with default size, query this
        // size directly, because we want resize all Controls to
        // the correct size before we display the window
        if ( nStyle & (WB_MOVEABLE | WB_SIZEABLE | WB_APP) )
        {
            tools::Long nWidth;
            tools::Long nHeight;

            mpWindowImpl->mpFrame->GetClientSize(nWidth, nHeight);

            mpWindowImpl->mxOutDev->SetOutputWidthPixel(nWidth);
            mpWindowImpl->mxOutDev->SetOutputHeightPixel(nHeight);
        }
    }
    else
    {
        if ( pParent )
        {
            if ( !ImplIsOverlapWindow() )
            {
                mpWindowImpl->mbDisabled          = pParent->mpWindowImpl->mbDisabled;
                mpWindowImpl->mbInputDisabled     = pParent->mpWindowImpl->mbInputDisabled;
                mpWindowImpl->meAlwaysInputMode   = pParent->mpWindowImpl->meAlwaysInputMode;
            }

            if (!comphelper::IsFuzzing())
            {
                // we don't want to call the WindowOutputDevice override of this because
                // it calls back into us.
                mpWindowImpl->mxOutDev->OutputDevice::SetSettings( pParent->GetSettings() );
            }
        }

    }

    // setup the scale factor for HiDPI displays
    mpWindowImpl->mxOutDev->SetDPIScalePercentage(CountDPIScaleFactor(mpWindowImpl->mpFrameData->mnDPIY));
    mpWindowImpl->mxOutDev->SetDPIX(mpWindowImpl->mpFrameData->mnDPIX);
    mpWindowImpl->mxOutDev->SetDPIY(mpWindowImpl->mpFrameData->mnDPIY);

    if (!comphelper::IsFuzzing())
    {
        const StyleSettings& rStyleSettings = mpWindowImpl->mxOutDev->moSettings->GetStyleSettings();
        mpWindowImpl->mxOutDev->maFont = rStyleSettings.GetAppFont();

        if ( nStyle & WB_3DLOOK )
        {
            SetTextColor( rStyleSettings.GetButtonTextColor() );
            SetBackground( Wallpaper( rStyleSettings.GetFaceColor() ) );
        }
        else
        {
            SetTextColor( rStyleSettings.GetWindowTextColor() );
            SetBackground( Wallpaper( rStyleSettings.GetWindowColor() ) );
        }
    }
    else
    {
        mpWindowImpl->mxOutDev->maFont = OutputDevice::GetDefaultFont( DefaultFontType::FIXED, LANGUAGE_ENGLISH_US, GetDefaultFontFlags::NONE );
    }

    ImplPointToLogic(*GetOutDev(), mpWindowImpl->mxOutDev->maFont);

    (void)ImplUpdatePos();

    // calculate app font res (except for the Intro Window or the default window)
    if ( mpWindowImpl->mbFrame && !pSVData->maGDIData.mnAppFontX && ! (nStyle & (WB_INTROWIN|WB_DEFAULTWIN)) )
        ImplInitAppFontData( this );
}

void Window::ImplInitAppFontData( vcl::Window const * pWindow )
{
    ImplSVData* pSVData = ImplGetSVData();
    tools::Long nTextHeight = pWindow->GetTextHeight();
    tools::Long nTextWidth = pWindow->approximate_char_width() * 8;
    tools::Long nSymHeight = nTextHeight*4;
    // Make the basis wider if the font is too narrow
    // such that the dialog looks symmetrical and does not become too narrow.
    // Add some extra space when the dialog has the same width,
    // as a little more space is better.
    if ( nSymHeight > nTextWidth )
        nTextWidth = nSymHeight;
    else if ( nSymHeight+5 > nTextWidth )
        nTextWidth = nSymHeight+5;
    pSVData->maGDIData.mnAppFontX = nTextWidth * 10 / 8;
    pSVData->maGDIData.mnAppFontY = nTextHeight * 10;

#ifdef MACOSX
    // FIXME: this is currently only on macOS, check with other
    // platforms
    if( pSVData->maNWFData.mbNoFocusRects )
    {
        // try to find out whether there is a large correction
        // of control sizes, if yes, make app font scalings larger
        // so dialog positioning is not completely off
        ImplControlValue aControlValue;
        tools::Rectangle aCtrlRegion( Point(), Size( nTextWidth < 10 ? 10 : nTextWidth, nTextHeight < 10 ? 10 : nTextHeight ) );
        tools::Rectangle aBoundingRgn( aCtrlRegion );
        tools::Rectangle aContentRgn( aCtrlRegion );
        if( pWindow->GetNativeControlRegion( ControlType::Editbox, ControlPart::Entire, aCtrlRegion,
                                             ControlState::ENABLED, aControlValue,
                                             aBoundingRgn, aContentRgn ) )
        {
            // comment: the magical +6 is for the extra border in bordered
            // (which is the standard) edit fields
            if( aContentRgn.GetHeight() - nTextHeight > (nTextHeight+4)/4 )
                pSVData->maGDIData.mnAppFontY = (aContentRgn.GetHeight()-4) * 10;
        }
    }
#endif
}

ImplWinData* Window::ImplGetWinData() const
{
    if (!mpWindowImpl->mpWinData)
    {
        static const char* pNoNWF = getenv( "SAL_NO_NWF" );

        const_cast<vcl::Window*>(this)->mpWindowImpl->mpWinData.reset(new ImplWinData);
        mpWindowImpl->mpWinData->mbEnableNativeWidget = !(pNoNWF && *pNoNWF); // true: try to draw this control with native theme API
    }

    return mpWindowImpl->mpWinData.get();
}


SalGraphics* Window::ImplGetFrameGraphics() const
{
    if ( mpWindowImpl->mpFrameWindow->GetOutDev()->mpGraphics )
    {
        mpWindowImpl->mpFrameWindow->GetOutDev()->GetClipState().Invalidate();
    }
    else
    {
        OutputDevice* pFrameWinOutDev = mpWindowImpl->mpFrameWindow->GetOutDev();
        if ( ! pFrameWinOutDev->AcquireGraphics() )
        {
            return nullptr;
        }
    }
    mpWindowImpl->mpFrameWindow->GetOutDev()->mpGraphics->ResetClipRegion();
    return mpWindowImpl->mpFrameWindow->GetOutDev()->mpGraphics;
}

void Window::ImplSetReallyVisible()
{
    // #i43594# it is possible that INITSHOW was never send, because the visibility state changed between
    // ImplCallInitShow() and ImplSetReallyVisible() when called from Show()
    // mbReallyShown is a useful indicator
    if( !mpWindowImpl->mbReallyShown )
        ImplCallInitShow();

    bool bBecameReallyVisible = !mpWindowImpl->mbReallyVisible;

    GetOutDev()->mbDevOutput     = true;
    mpWindowImpl->mbReallyVisible = true;
    mpWindowImpl->mbReallyShown   = true;

    // the SHOW/HIDE events serve as indicators to send child creation/destroy events to the access bridge.
    // For this, the data member of the event must not be NULL.
    // Previously, we did this in Window::Show, but there some events got lost in certain situations. Now
    // we're doing it when the visibility really changes
    if( bBecameReallyVisible && ImplIsAccessibleCandidate() )
        CallEventListeners( VclEventId::WindowShow, this );
        // TODO. It's kind of a hack that we're re-using the VclEventId::WindowShow. Normally, we should
        // introduce another event which explicitly triggers the Accessibility implementations.

    vcl::Window* pWindow = mpWindowImpl->mpHierarchy->mpFirstOverlap;
    while ( pWindow )
    {
        if ( pWindow->mpWindowImpl->mbVisible )
            pWindow->ImplSetReallyVisible();
        pWindow = pWindow->mpWindowImpl->mpHierarchy->mpNext;
    }

    pWindow = mpWindowImpl->mpHierarchy->mpFirstChild;
    while ( pWindow )
    {
        if ( pWindow->mpWindowImpl->mbVisible )
            pWindow->ImplSetReallyVisible();
        pWindow = pWindow->mpWindowImpl->mpHierarchy->mpNext;
    }
}

void Window::ImplInitResolutionSettings()
{
    // recalculate AppFont-resolution and DPI-resolution
    if (mpWindowImpl->mbFrame)
    {
        GetOutDev()->SetDPIX(mpWindowImpl->mpFrameData->mnDPIX);
        GetOutDev()->SetDPIY(mpWindowImpl->mpFrameData->mnDPIY);

        // setup the scale factor for HiDPI displays
        GetOutDev()->SetDPIScalePercentage(CountDPIScaleFactor(mpWindowImpl->mpFrameData->mnDPIY));
        const StyleSettings& rStyleSettings = GetOutDev()->moSettings->GetStyleSettings();
        SetPointFont(*GetOutDev(), rStyleSettings.GetAppFont());
    }
    else if ( mpWindowImpl->mpHierarchy->mpParent )
    {
        GetOutDev()->SetDPIX(mpWindowImpl->mpHierarchy->mpParent->GetOutDev()->GetDPIX());
        GetOutDev()->SetDPIY(mpWindowImpl->mpHierarchy->mpParent->GetOutDev()->GetDPIY());
        GetOutDev()->SetDPIScalePercentage(mpWindowImpl->mpHierarchy->mpParent->GetOutDev()->GetDPIScalePercentage());
    }

    // update the recalculated values for logical units
    // and also tools belonging to the values
    if (GetMappingPolicy() == vcl::MappingPolicy::ApplyMapMode)
    {
        MapMode aMapMode = GetMapMode();
        SetMapMode();
        SetMapMode( aMapMode );
    }
}

void Window::ImplPointToLogic(vcl::RenderContext const & rRenderContext, vcl::Font& rFont,
                            bool bUseRenderContextDPI) const
{
    Size aSize = rFont.GetFontSize();

    if (aSize.Width())
    {
        aSize.setWidth( aSize.Width() *
            ( bUseRenderContextDPI ? rRenderContext.GetDPIX() : mpWindowImpl->mpFrameData->mnDPIX) );
        aSize.AdjustWidth(72 / 2 );
        aSize.setWidth( aSize.Width() / 72 );
    }
    aSize.setHeight( aSize.Height()
        * ( bUseRenderContextDPI ? rRenderContext.GetDPIY() : mpWindowImpl->mpFrameData->mnDPIY) );
    aSize.AdjustHeight(72/2 );
    aSize.setHeight( aSize.Height() / 72 );

    aSize =  rRenderContext.convertTo<vcl::LogicSize>(vcl::WindowSize(aSize));

    rFont.SetFontSize(aSize);
}

void Window::ImplLogicToPoint(vcl::RenderContext const & rRenderContext, vcl::Font& rFont) const
{
    auto aSize = rRenderContext.convertTo<vcl::WindowSize>(vcl::LogicSize(rFont.GetFontSize()), rRenderContext.GetMapMode());

    if (aSize->Width())
    {
        aSize->setWidth(aSize->Width() * 72);
        aSize->AdjustWidth(mpWindowImpl->mpFrameData->mnDPIX / 2);
        aSize->setWidth(aSize->Width() / mpWindowImpl->mpFrameData->mnDPIX);
    }
    aSize->setHeight(aSize->Height() * 72);
    aSize->AdjustHeight(mpWindowImpl->mpFrameData->mnDPIY / 2);
    aSize->setHeight(aSize->Height() / mpWindowImpl->mpFrameData->mnDPIY);

    rFont.SetFontSize(aSize.get());
}

bool Window::ImplUpdatePos()
{
    bool bSysChild = false;

    if ( ImplIsOverlapWindow() )
    {
        GetOutDev()->SetDeviceOriginX(mpWindowImpl->mnX);
        GetOutDev()->SetDeviceOriginY(mpWindowImpl->mnY);
    }
    else
    {
        vcl::Window* pParent = ImplGetParent();

        GetOutDev()->SetDeviceOriginX(mpWindowImpl->mnX + pParent->GetOutDev()->GetDeviceOriginX());
        GetOutDev()->SetDeviceOriginY(mpWindowImpl->mnY + pParent->GetOutDev()->GetDeviceOriginY());
    }

    VclPtr< vcl::Window > pChild = mpWindowImpl->mpHierarchy->mpFirstChild;
    while ( pChild )
    {
        if ( pChild->ImplUpdatePos() )
            bSysChild = true;
        pChild = pChild->mpWindowImpl->mpHierarchy->mpNext;
    }

    if ( mpWindowImpl->mpSysObj )
        bSysChild = true;

    return bSysChild;
}

void Window::ImplUpdateNativeObjectPos()
{
    if ( mpWindowImpl->mpSysObj )
        mpWindowImpl->mpSysObj->SetPosSize( GetOutDev()->GetDeviceOriginX(), GetOutDev()->GetDeviceOriginY(), GetOutDev()->GetOutputWidthPixel(), GetOutDev()->GetOutputHeightPixel() );

    VclPtr< vcl::Window > pChild = mpWindowImpl->mpHierarchy->mpFirstChild;
    while ( pChild )
    {
        pChild->ImplUpdateNativeObjectPos();
        pChild = pChild->mpWindowImpl->mpHierarchy->mpNext;
    }
}

void Window::ImplPosSizeWindow( tools::Long nX, tools::Long nY,
                                tools::Long nWidth, tools::Long nHeight, PosSizeFlags nFlags )
{
    bool    bNewPos         = false;
    bool    bNewSize        = false;
    bool    bCopyBits       = false;
    tools::Long    nOldOutOffX     = GetOutDev()->GetDeviceOriginX();
    tools::Long    nOldOutOffY     = GetOutDev()->GetDeviceOriginY();
    tools::Long    nOldOutWidth    = GetOutDev()->GetOutputWidthPixel();
    tools::Long    nOldOutHeight   = GetOutDev()->GetOutputHeightPixel();
    std::unique_ptr<vcl::Region> pOverlapRegion;
    std::unique_ptr<vcl::Region> pOldRegion;

    if ( IsReallyVisible() )
    {
        tools::Rectangle aOldWinRect( Point( nOldOutOffX, nOldOutOffY ),
                               Size( nOldOutWidth, nOldOutHeight ) );
        pOldRegion.reset( new vcl::Region( aOldWinRect ) );
        if ( mpWindowImpl->mpClippingState->mbWinRegion )
            pOldRegion->Intersect( GetOutDev()->GetMapper().ViewToDevice( mpWindowImpl->mpClippingState->maWinRegion ) );

        if ( GetOutDev()->GetOutputWidthPixel() && GetOutDev()->GetOutputHeightPixel() && !mpWindowImpl->mbPaintTransparent &&
             !mpWindowImpl->mpClippingState->mbInitWinClipRegion && !mpWindowImpl->mpClippingState->maWinClipRegion.IsEmpty() &&
             !HasPaintEvent() )
            bCopyBits = true;
    }

    bool bnXRecycled = false; // avoid duplicate mirroring in RTL case
    if ( nFlags & PosSizeFlags::Width )
    {
        if(!( nFlags & PosSizeFlags::X ))
        {
            nX = mpWindowImpl->mnX;
            nFlags |= PosSizeFlags::X;
            bnXRecycled = true; // we're using a mnX which was already mirrored in RTL case
        }

        if ( nWidth < 0 )
            nWidth = 0;
        if ( nWidth != GetOutDev()->GetOutputWidthPixel() )
        {
            GetOutDev()->SetOutputWidthPixel(nWidth);
            bNewSize = true;
            bCopyBits = false;
        }
    }
    if ( nFlags & PosSizeFlags::Height )
    {
        if ( nHeight < 0 )
            nHeight = 0;
        if ( nHeight != GetOutDev()->GetOutputHeightPixel() )
        {
            GetOutDev()->SetOutputHeightPixel(nHeight);
            bNewSize = true;
            bCopyBits = false;
        }
    }

    if ( nFlags & PosSizeFlags::X )
    {
        tools::Long nOrgX = nX;
        Point aPtDev( nX+GetOutDev()->GetDeviceOriginX(), 0 );
        OutputDevice *pOutDev = GetOutDev();
        if( pOutDev->HasMirroredGraphics() )
        {
            aPtDev.setX( GetOutDev()->mpGraphics->mirror2( aPtDev.X(), *GetOutDev() ) );

            // #106948# always mirror our pos if our parent is not mirroring, even
            // if we are also not mirroring
            // RTL: check if parent is in different coordinates
            if( !bnXRecycled && mpWindowImpl->mpHierarchy->mpParent && !mpWindowImpl->mpHierarchy->mpParent->mpWindowImpl->mbFrame && mpWindowImpl->mpHierarchy->mpParent->GetOutDev()->ImplIsAntiparallel() )
            {
                nX = mpWindowImpl->mpHierarchy->mpParent->GetOutDev()->GetOutputWidthPixel() - GetOutDev()->GetOutputWidthPixel() - nX;
            }
            /* #i99166# An LTR window in RTL UI that gets sized only would be
               expected to not moved its upper left point
            */
            if( bnXRecycled )
            {
                if( GetOutDev()->ImplIsAntiparallel() )
                {
                    aPtDev.setX( mpWindowImpl->mnAbsScreenX );
                    nOrgX = mpWindowImpl->maPos.X();
                }
            }
        }
        else if( !bnXRecycled && mpWindowImpl->mpHierarchy->mpParent && !mpWindowImpl->mpHierarchy->mpParent->mpWindowImpl->mbFrame && mpWindowImpl->mpHierarchy->mpParent->GetOutDev()->ImplIsAntiparallel() )
        {
            // mirrored window in LTR UI
            nX = mpWindowImpl->mpHierarchy->mpParent->GetOutDev()->GetOutputWidthPixel() - GetOutDev()->GetOutputWidthPixel() - nX;
        }

        // check maPos as well, as it could have been changed for client windows (ImplCallMove())
        if ( mpWindowImpl->mnAbsScreenX != aPtDev.X() || nX != mpWindowImpl->mnX || nOrgX != mpWindowImpl->maPos.X() )
        {
            if ( bCopyBits && !pOverlapRegion )
            {
                pOverlapRegion.reset( new vcl::Region() );
                vcl::clipping::calcOverlapRegion(*this, GetOutputRectPixel(), *pOverlapRegion, false, true);
            }

            mpWindowImpl->mnX = nX;
            mpWindowImpl->maPos.setX( nOrgX );
            mpWindowImpl->mnAbsScreenX = aPtDev.X();
            bNewPos = true;
        }
    }
    if ( nFlags & PosSizeFlags::Y )
    {
        // check maPos as well, as it could have been changed for client windows (ImplCallMove())
        if ( nY != mpWindowImpl->mnY || nY != mpWindowImpl->maPos.Y() )
        {
            if ( bCopyBits && !pOverlapRegion )
            {
                pOverlapRegion.reset( new vcl::Region() );
                vcl::clipping::calcOverlapRegion(*this, GetOutputRectPixel(), *pOverlapRegion, false, true);
            }
            mpWindowImpl->mnY = nY;
            mpWindowImpl->maPos.setY( nY );
            bNewPos = true;
        }
    }

    if ( !(bNewPos || bNewSize) )
        return;

    bool bUpdateSysObjPos = false;
    if ( bNewPos )
        bUpdateSysObjPos = ImplUpdatePos();

    // the borderwindow always specifies the position for its client window
    if ( mpWindowImpl->mpBorderWindow )
        mpWindowImpl->maPos = mpWindowImpl->mpBorderWindow->mpWindowImpl->maPos;

    if ( mpWindowImpl->mpClientWindow )
    {
        mpWindowImpl->mpClientWindow->ImplPosSizeWindow( mpWindowImpl->mpClientWindow->mpWindowImpl->mnLeftBorder,
                                           mpWindowImpl->mpClientWindow->mpWindowImpl->mnTopBorder,
                                           GetOutDev()->GetOutputWidthPixel() - mpWindowImpl->mpClientWindow->mpWindowImpl->mnLeftBorder-mpWindowImpl->mpClientWindow->mpWindowImpl->mnRightBorder,
                                           GetOutDev()->GetOutputHeightPixel() - mpWindowImpl->mpClientWindow->mpWindowImpl->mnTopBorder-mpWindowImpl->mpClientWindow->mpWindowImpl->mnBottomBorder,
                                           PosSizeFlags::X | PosSizeFlags::Y |
                                           PosSizeFlags::Width | PosSizeFlags::Height );
        // If we have a client window, then this is the position
        // of the Application's floating windows
        mpWindowImpl->mpClientWindow->mpWindowImpl->maPos = mpWindowImpl->maPos;
        if ( bNewPos )
        {
            if ( mpWindowImpl->mpClientWindow->IsVisible() )
            {
                mpWindowImpl->mpClientWindow->ImplCallMove();
            }
            else
            {
                mpWindowImpl->mpClientWindow->mpWindowImpl->mbCallMove = true;
            }
        }
    }

    // Move()/Resize() will be called only for Show(), such that
    // at least one is called before Show()
    if ( IsVisible() )
    {
        if ( bNewPos )
        {
            ImplCallMove();
        }
        if ( bNewSize )
        {
            ImplCallResize();
        }
    }
    else
    {
        if ( bNewPos )
            mpWindowImpl->mbCallMove = true;
        if ( bNewSize )
            mpWindowImpl->mbCallResize = true;
    }

    bool bUpdateSysObjClip = false;
    if ( IsReallyVisible() )
    {
        if ( bNewPos || bNewSize )
        {
            // set Clip-Flag
            bUpdateSysObjClip = !vcl::clipping::setClipFlag(*this, true);
        }

        // invalidate window content ?
        if ( bNewPos || (GetOutDev()->GetOutputWidthPixel() > nOldOutWidth) || (GetOutDev()->GetOutputHeightPixel() > nOldOutHeight) )
        {
            if ( bNewPos )
            {
                bool bInvalidate = false;
                bool bParentPaint = true;
                if ( !ImplIsOverlapWindow() )
                    bParentPaint = mpWindowImpl->mpHierarchy->mpParent->IsPaintEnabled();
                if ( bCopyBits && bParentPaint && !HasPaintEvent() )
                {
                    vcl::Region aRegion( GetOutputRectPixel() );
                    if ( mpWindowImpl->mpClippingState->mbWinRegion )
                        aRegion.Intersect( GetOutDev()->GetMapper().ViewToDevice( mpWindowImpl->mpClippingState->maWinRegion ) );
                    vcl::clipping::clipBoundaries(*this, aRegion, false, true);
                    if ( !pOverlapRegion->IsEmpty() )
                    {
                        pOverlapRegion->Move( GetOutDev()->GetDeviceOriginX() - nOldOutOffX, GetOutDev()->GetDeviceOriginY() - nOldOutOffY );
                        aRegion.Exclude( *pOverlapRegion );
                    }
                    if ( !aRegion.IsEmpty() )
                    {
                        // adapt Paint areas
                        ImplMoveAllInvalidateRegions( tools::Rectangle( Point( nOldOutOffX, nOldOutOffY ),
                                                                 Size( nOldOutWidth, nOldOutHeight ) ),
                                                      GetOutDev()->GetDeviceOriginX() - nOldOutOffX, GetOutDev()->GetDeviceOriginY() - nOldOutOffY,
                                                      true );
                        SalGraphics* pGraphics = ImplGetFrameGraphics();
                        if ( pGraphics )
                        {

                            OutputDevice *pOutDev = GetOutDev();
                            const bool bSelectClipRegion = pOutDev->SelectClipRegion( aRegion, pGraphics );
                            if ( bSelectClipRegion )
                            {
                                pGraphics->CopyArea( GetOutDev()->GetDeviceOriginX(), GetOutDev()->GetDeviceOriginY(),
                                                     nOldOutOffX, nOldOutOffY,
                                                     nOldOutWidth, nOldOutHeight,
                                                     *GetOutDev() );
                            }
                            else
                                bInvalidate = true;
                        }
                        else
                            bInvalidate = true;
                        if ( !bInvalidate )
                        {
                            if ( !pOverlapRegion->IsEmpty() )
                                ImplInvalidateFrameRegion( pOverlapRegion.get(), InvalidateFlags::Children );
                        }
                    }
                    else
                        bInvalidate = true;
                }
                else
                    bInvalidate = true;
                if ( bInvalidate )
                    ImplInvalidateFrameRegion( nullptr, InvalidateFlags::Children );
            }
            else
            {
                vcl::Region aRegion( GetOutputRectPixel() );
                aRegion.Exclude( *pOldRegion );
                if ( mpWindowImpl->mpClippingState->mbWinRegion )
                    aRegion.Intersect( GetOutDev()->GetMapper().ViewToDevice( mpWindowImpl->mpClippingState->maWinRegion ) );
                vcl::clipping::clipBoundaries(*this, aRegion, false, true);
                if ( !aRegion.IsEmpty() )
                    ImplInvalidateFrameRegion( &aRegion, InvalidateFlags::Children );
            }
        }

        // invalidate Parent or Overlaps
        if ( bNewPos ||
             (GetOutDev()->GetOutputWidthPixel() < nOldOutWidth) || (GetOutDev()->GetOutputHeightPixel() < nOldOutHeight) )
        {
            vcl::Region aRegion( *pOldRegion );
            if ( !mpWindowImpl->mbPaintTransparent )
                vcl::clipping::excludeWindowRegion(*this, aRegion);

            vcl::clipping::clipBoundaries(*this, aRegion, false, true);

            if ( !aRegion.IsEmpty() && !mpWindowImpl->mpBorderWindow )
                ImplInvalidateParentFrameRegion( aRegion );
        }
    }

    // adapt system objects
    if ( bUpdateSysObjClip )
        vcl::clipping::updateNativeObjectClip(*this);

    if ( bUpdateSysObjPos )
        ImplUpdateNativeObjectPos();

    if ( bNewSize && mpWindowImpl->mpSysObj )
        mpWindowImpl->mpSysObj->SetPosSize( GetOutDev()->GetDeviceOriginX(), GetOutDev()->GetDeviceOriginY(), GetOutDev()->GetOutputWidthPixel(), GetOutDev()->GetOutputHeightPixel() );
}

void Window::ImplNewInputContext()
{
    ImplSVData* pSVData = ImplGetSVData();
    vcl::Window* pFocusWin = pSVData->mpWinData->mpFocusWin;
    if ( !pFocusWin || !pFocusWin->mpWindowImpl || pFocusWin->isDisposed() )
        return;

    // Is InputContext changed?
    const InputContext& rInputContext = pFocusWin->GetInputContext();
    if ( rInputContext == pFocusWin->mpWindowImpl->mpFrameData->maOldInputContext )
        return;

    pFocusWin->mpWindowImpl->mpFrameData->maOldInputContext = rInputContext;

    SalInputContext         aNewContext;
    const vcl::Font&        rFont = rInputContext.GetFont();
    const OUString&         rFontName = rFont.GetFamilyName();
    if (!rFontName.isEmpty())
    {
        OutputDevice *pFocusWinOutDev = pFocusWin->GetOutDev();
        Size aSize = pFocusWinOutDev->GetMapper().LogicToViewDistance( rFont.GetFontSize(), pFocusWinOutDev->GetMappingPolicy() );
        if ( !aSize.Height() )
        {
            // only set default sizes if the font height in logical
            // coordinates equals 0
            if ( rFont.GetFontSize().Height() )
                aSize.setHeight( 1 );
            else
                aSize.setHeight( (12*pFocusWin->GetOutDev()->GetDPIY())/72 );
        }
        aNewContext.mpFont =
                        pFocusWin->GetOutDev()->mxFontCache->GetFontInstance(
                            pFocusWin->GetOutDev()->mxFontCollection.get(),
                            rFont, aSize, static_cast<float>(aSize.Height()) );
    }
    aNewContext.mnOptions   = rInputContext.GetOptions();
    pFocusWin->ImplGetFrame()->SetInputContext( &aNewContext );
}

void Window::SetModalHierarchyHdl(const Link<bool, void>& rLink)
{
    ImplGetFrame()->SetModalHierarchyHdl(rLink);
}

KeyIndicatorState Window::GetIndicatorState() const
{
    return mpWindowImpl->mpFrame->GetIndicatorState();
}

void Window::SimulateKeyPress( sal_uInt16 nKeyCode ) const
{
    mpWindowImpl->mpFrame->SimulateKeyPress(nKeyCode);
}

void Window::KeyInput( const KeyEvent& rKEvt )
{
#ifndef _WIN32 // On Windows, dialogs react to accelerators  without Alt (tdf#157649)
    KeyCode cod = rKEvt.GetKeyCode ();

    // do not respond to accelerators unless Alt or Ctrl is held
    if (cod.GetCode () >= 0x200 && cod.GetCode () <= 0x219)
    {
        bool autoacc = ImplGetSVData()->maNWFData.mbAutoAccel;
        if (autoacc && cod.GetModifier () != KEY_MOD2 && !(cod.GetModifier() & KEY_MOD1))
            return;
    }
#endif

    NotifyEvent aNEvt( NotifyEventType::KEYINPUT, this, &rKEvt );
    if ( !CompatNotify( aNEvt ) )
        mpWindowImpl->mbKeyInput = true;
}

void Window::KeyUp( const KeyEvent& rKEvt )
{
    NotifyEvent aNEvt( NotifyEventType::KEYUP, this, &rKEvt );
    if ( !CompatNotify( aNEvt ) )
        mpWindowImpl->mbKeyUp = true;
}

void Window::Draw(OutputDevice&, const Point&, SystemTextColorFlags) {}

void Window::Move() {}

void Window::Resize() {}

void Window::Activate() {}

void Window::Deactivate() {}

void Window::GetFocus()
{
    if ( HasFocus() && mpWindowImpl->mpLastFocusWindow && !(mpWindowImpl->mnDlgCtrlFlags & DialogControlFlags::WantFocus) )
    {
        VclPtr<vcl::Window> xWindow(this);
        mpWindowImpl->mpLastFocusWindow->GrabFocus();
        if( xWindow->isDisposed() )
            return;
    }

    NotifyEvent aNEvt( NotifyEventType::GETFOCUS, this );
    CompatNotify( aNEvt );
}

void Window::LoseFocus()
{
    NotifyEvent aNEvt( NotifyEventType::LOSEFOCUS, this );
    CompatNotify( aNEvt );
}

void Window::SetCommandHdl(const Link<const CommandEvent&, bool>& rLink)
{
    if (mpWindowImpl)
        mpWindowImpl->maCommandHdl = rLink;
}

void Window::SetHelpHdl(const Link<vcl::Window&, bool>& rLink)
{
    if (mpWindowImpl) // may be called after dispose
    {
        mpWindowImpl->maHelpRequestHdl = rLink;
    }
}

void Window::RequestHelp( const HelpEvent& rHEvt )
{
    // if Balloon-Help is requested, show the balloon
    // with help text set
    if ( rHEvt.GetMode() & HelpEventMode::BALLOON )
    {
        OUString rStr = GetHelpText();
        if ( rStr.isEmpty() )
            rStr = GetQuickHelpText();
        if ( rStr.isEmpty() && ImplGetParent() && !ImplIsOverlapWindow() )
            ImplGetParent()->RequestHelp( rHEvt );
        else
        {
            Point aPos = GetPosPixel();
            if ( ImplGetParent() && !ImplIsOverlapWindow() )
                aPos = OutputToScreenPixel(Point(0, 0));
            tools::Rectangle   aRect( aPos, GetSizePixel() );

            Help::ShowBalloon( this, rHEvt.GetMousePosPixel(), aRect, rStr );
        }
    }
    else if ( rHEvt.GetMode() & HelpEventMode::QUICK )
    {
        const OUString& rStr = GetQuickHelpText();
        if ( rStr.isEmpty() && ImplGetParent() && !ImplIsOverlapWindow() )
            ImplGetParent()->RequestHelp( rHEvt );
        else
        {
            Point aPos = GetPosPixel();
            if ( ImplGetParent() && !ImplIsOverlapWindow() )
                aPos = OutputToScreenPixel(Point(0, 0));
            tools::Rectangle   aRect( aPos, GetSizePixel() );
            Help::ShowQuickHelp( this, aRect, rStr, QuickHelpFlags::CtrlText );
        }
    }
    else if (!mpWindowImpl->maHelpRequestHdl.IsSet() || mpWindowImpl->maHelpRequestHdl.Call(*this))
    {
        OUString aStrHelpId( GetHelpId() );
        if ( aStrHelpId.isEmpty() && ImplGetParent() )
            ImplGetParent()->RequestHelp( rHEvt );
        else
        {
            Help* pHelp = Application::GetHelp();
            if ( pHelp )
            {
                if( !aStrHelpId.isEmpty() )
                    pHelp->Start( aStrHelpId, this );
                else
                    pHelp->Start( u"" OOO_HELP_INDEX ""_ustr, this );
            }
        }
    }
}

void Window::Command( const CommandEvent& rCEvt )
{
    if (mpWindowImpl && mpWindowImpl->maCommandHdl.Call(rCEvt))
        return;

    CallEventListeners( VclEventId::WindowCommand, const_cast<CommandEvent *>(&rCEvt) );

    NotifyEvent aNEvt( NotifyEventType::COMMAND, this, &rCEvt );
    if ( !CompatNotify( aNEvt ) )
        mpWindowImpl->mbCommand = true;
}

void Window::Tracking( const TrackingEvent& rTEvt )
{

    ImplDockingWindowWrapper *pWrapper = ImplGetDockingManager()->GetDockingWindowWrapper( this );
    if( pWrapper )
        pWrapper->Tracking( rTEvt );
}

void Window::StateChanged(StateChangedType eType)
{
    switch (eType)
    {
        //stuff that doesn't invalidate the layout
        case StateChangedType::ControlForeground:
        case StateChangedType::ControlBackground:
        case StateChangedType::UpdateMode:
        case StateChangedType::ReadOnly:
        case StateChangedType::Enable:
        case StateChangedType::State:
        case StateChangedType::Data:
        case StateChangedType::InitShow:
        case StateChangedType::ControlFocus:
            break;
        //stuff that does invalidate the layout
        default:
            queue_resize(eType);
            break;
    }
}

void Window::SetStyle( WinBits nStyle )
{
    if ( mpWindowImpl && mpWindowImpl->mnStyle != nStyle )
    {
        mpWindowImpl->mnPrevStyle = mpWindowImpl->mnStyle;
        mpWindowImpl->mnStyle = nStyle;
        CompatStateChanged( StateChangedType::Style );
    }
}

void Window::SetExtendedStyle( WindowExtendedStyle nExtendedStyle )
{

    if ( mpWindowImpl->mnExtendedStyle == nExtendedStyle )
        return;

    vcl::Window* pWindow = ImplGetBorderWindow();
    if( ! pWindow )
        pWindow = this;
    if( pWindow->mpWindowImpl->mbFrame )
    {
        SalExtStyle nExt = 0;
        if( nExtendedStyle & WindowExtendedStyle::Document )
            nExt |= SAL_FRAME_EXT_STYLE_DOCUMENT;
        if( nExtendedStyle & WindowExtendedStyle::DocModified )
            nExt |= SAL_FRAME_EXT_STYLE_DOCMODIFIED;

        pWindow->ImplGetFrame()->SetExtendedFrameStyle( nExt );
    }
    mpWindowImpl->mnExtendedStyle = nExtendedStyle;
}

void Window::SetBorderStyle( WindowBorderStyle nBorderStyle )
{

    if ( !mpWindowImpl->mpBorderWindow )
        return;

    if( nBorderStyle == WindowBorderStyle::REMOVEBORDER &&
        ! mpWindowImpl->mpBorderWindow->mpWindowImpl->mbFrame &&
        mpWindowImpl->mpBorderWindow->mpWindowImpl->mpHierarchy->mpParent
        )
    {
        // this is a little awkward: some controls (e.g. svtools ProgressBar)
        // cannot avoid getting constructed with WB_BORDER but want to disable
        // borders in case of NWF drawing. So they need a method to remove their border window
        VclPtr<vcl::Window> pBorderWin = mpWindowImpl->mpBorderWindow;
        // remove us as border window's client
        pBorderWin->mpWindowImpl->mpClientWindow = nullptr;
        mpWindowImpl->mpBorderWindow = nullptr;
        mpWindowImpl->mpHierarchy->mpRealParent = pBorderWin->mpWindowImpl->mpHierarchy->mpParent;
        // reparent us above the border window
        SetParent( pBorderWin->mpWindowImpl->mpHierarchy->mpParent );
        // set us to the position and size of our previous border
        Point aBorderPos( pBorderWin->GetPosPixel() );
        Size aBorderSize( pBorderWin->GetSizePixel() );
        setPosSizePixel( aBorderPos.X(), aBorderPos.Y(), aBorderSize.Width(), aBorderSize.Height() );
        // release border window
        pBorderWin.disposeAndClear();

        // set new style bits
        SetStyle( GetStyle() & (~WB_BORDER) );
    }
    else
    {
        if ( mpWindowImpl->mpBorderWindow->GetType() == WindowType::BORDERWINDOW )
            static_cast<ImplBorderWindow*>(mpWindowImpl->mpBorderWindow.get())->SetBorderStyle( nBorderStyle );
        else
            mpWindowImpl->mpBorderWindow->SetBorderStyle( nBorderStyle );
    }
}

WindowBorderStyle Window::GetBorderStyle() const
{

    if ( mpWindowImpl->mpBorderWindow )
    {
        if ( mpWindowImpl->mpBorderWindow->GetType() == WindowType::BORDERWINDOW )
            return static_cast<ImplBorderWindow*>(mpWindowImpl->mpBorderWindow.get())->GetBorderStyle();
        else
            return mpWindowImpl->mpBorderWindow->GetBorderStyle();
    }

    return WindowBorderStyle::NONE;
}

tools::Long Window::CalcTitleWidth() const
{

    if ( mpWindowImpl->mpBorderWindow )
    {
        if ( mpWindowImpl->mpBorderWindow->GetType() == WindowType::BORDERWINDOW )
            return static_cast<ImplBorderWindow*>(mpWindowImpl->mpBorderWindow.get())->CalcTitleWidth();
        else
            return mpWindowImpl->mpBorderWindow->CalcTitleWidth();
    }
    else if ( mpWindowImpl->mbFrame && (mpWindowImpl->mnStyle & WB_MOVEABLE) )
    {
        // we guess the width for frame windows as we do not know the
        // border of external dialogs
        const StyleSettings& rStyleSettings = GetSettings().GetStyleSettings();
        vcl::Font aFont = GetFont();
        const_cast<vcl::Window*>(this)->SetPointFont(const_cast<::OutputDevice&>(*GetOutDev()), rStyleSettings.GetTitleFont());
        tools::Long nTitleWidth = GetTextWidth( GetText() );
        const_cast<vcl::Window*>(this)->SetFont( aFont );
        nTitleWidth += rStyleSettings.GetTitleHeight() * 3;
        nTitleWidth += StyleSettings::GetBorderSize() * 2;
        nTitleWidth += 10;
        return nTitleWidth;
    }

    return 0;
}

void Window::SetInputContext( const InputContext& rInputContext )
{

    mpWindowImpl->maInputContext = rInputContext;
    if ( !mpWindowImpl->mbInFocusHdl && HasFocus() )
        ImplNewInputContext();
}

void Window::PostExtTextInputEvent(VclEventId nType, const OUString& rText)
{
    switch (nType)
    {
    case VclEventId::ExtTextInput:
    {
        std::unique_ptr<ExtTextInputAttr[]> pAttr(new ExtTextInputAttr[rText.getLength()]);
        for (int i = 0; i < rText.getLength(); ++i) {
            pAttr[i] = ExtTextInputAttr::Underline;
        }
        SalExtTextInputEvent aEvent { rText, pAttr.get(), rText.getLength(), EXTTEXTINPUT_CURSOR_OVERWRITE };
        ImplWindowFrameProc(this, SalEvent::ExtTextInput, &aEvent);
    }
    break;
    case VclEventId::EndExtTextInput:
        ImplWindowFrameProc(this, SalEvent::EndExtTextInput, nullptr);
        break;
    default:
        assert(false);
    }
}

void Window::EndExtTextInput()
{
    if ( mpWindowImpl->mbExtTextInput )
        ImplGetFrame()->EndExtTextInput( EndExtTextInputFlags::Complete );
}

void Window::SetCursorRect( const tools::Rectangle* pRect, tools::Long nExtTextInputWidth )
{

    ImplWinData* pWinData = ImplGetWinData();
    if ( pWinData->mpCursorRect )
    {
        if ( pRect )
            pWinData->mpCursorRect = *pRect;
        else
            pWinData->mpCursorRect.reset();
    }
    else
    {
        if ( pRect )
            pWinData->mpCursorRect = *pRect;
    }

    pWinData->mnCursorExtWidth = nExtTextInputWidth;

}

const tools::Rectangle* Window::GetCursorRect() const
{

    ImplWinData* pWinData = ImplGetWinData();
    return pWinData->mpCursorRect ? &*pWinData->mpCursorRect : nullptr;
}

tools::Long Window::GetCursorExtTextInputWidth() const
{

    ImplWinData* pWinData = ImplGetWinData();
    return pWinData->mnCursorExtWidth;
}

void Window::SetCompositionCharRect( const tools::Rectangle* pRect, tools::Long nCompositionLength, bool bVertical ) {

    ImplWinData* pWinData = ImplGetWinData();
    pWinData->mpCompositionCharRects.reset();
    pWinData->mbVertical = bVertical;
    pWinData->mnCompositionCharRects = nCompositionLength;
    if ( pRect && (nCompositionLength > 0) )
    {
        pWinData->mpCompositionCharRects.reset( new tools::Rectangle[nCompositionLength] );
        for (tools::Long i = 0; i < nCompositionLength; ++i)
            pWinData->mpCompositionCharRects[i] = pRect[i];
    }
}

void Window::CollectChildren(::std::vector<vcl::Window *>& rAllChildren )
{
    rAllChildren.push_back( this );

    VclPtr< vcl::Window > pChild = mpWindowImpl->mpHierarchy->mpFirstChild;
    while ( pChild )
    {
        pChild->CollectChildren( rAllChildren );
        pChild = pChild->mpWindowImpl->mpHierarchy->mpNext;
    }
}

void Window::SetPointFont(vcl::RenderContext& rRenderContext, const vcl::Font& rFont,
                          bool bUseRenderContextDPI)
{
    vcl::Font aFont = rFont;
    ImplPointToLogic(rRenderContext, aFont, bUseRenderContextDPI);
    rRenderContext.SetFont(aFont);
}

vcl::Font Window::GetPointFont(vcl::RenderContext const & rRenderContext) const
{
    vcl::Font aFont = rRenderContext.GetFont();
    ImplLogicToPoint(rRenderContext, aFont);
    return aFont;
}

void Window::Show(bool bVisible, ShowFlags nFlags)
{
    if ( !mpWindowImpl || mpWindowImpl->mbVisible == bVisible )
        return;

    VclPtr<vcl::Window> xWindow(this);

    bool bRealVisibilityChanged = false;
    mpWindowImpl->mbVisible = bVisible;

    if ( !bVisible )
    {
        ImplHideAllOverlaps();
        if( !xWindow->mpWindowImpl )
            return;

        if ( mpWindowImpl->mpBorderWindow )
        {
            bool bOldUpdate = mpWindowImpl->mpBorderWindow->mpWindowImpl->mbNoParentUpdate;
            if ( mpWindowImpl->mbNoParentUpdate )
                mpWindowImpl->mpBorderWindow->mpWindowImpl->mbNoParentUpdate = true;
            mpWindowImpl->mpBorderWindow->Show( false, nFlags );
            mpWindowImpl->mpBorderWindow->mpWindowImpl->mbNoParentUpdate = bOldUpdate;
        }
        else if ( mpWindowImpl->mbFrame )
        {
            mpWindowImpl->mbSuppressAccessibilityEvents = true;
            mpWindowImpl->mpFrame->Show( false );
        }

        CompatStateChanged( StateChangedType::Visible );

        if ( mpWindowImpl->mbReallyVisible )
        {
            if ( mpWindowImpl->mpClippingState->mbInitWinClipRegion )
                clipping::initWinClipRegion(*this);

            vcl::Region aInvRegion = mpWindowImpl->mpClippingState->maWinClipRegion;

            if( !xWindow->mpWindowImpl )
                return;

            bRealVisibilityChanged = mpWindowImpl->mbReallyVisible;
            ImplResetReallyVisible();
            vcl::clipping::setClipFlag(*this);

            if ( ImplIsOverlapWindow() && !mpWindowImpl->mbFrame )
            {
                // convert focus
                if ( !(nFlags & ShowFlags::NoFocusChange) && HasChildPathFocus() )
                {
                    if ( mpWindowImpl->mpOverlapWindow->IsEnabled() &&
                         mpWindowImpl->mpOverlapWindow->IsInputEnabled() &&
                         ! mpWindowImpl->mpOverlapWindow->IsInModalMode()
                         )
                        mpWindowImpl->mpOverlapWindow->GrabFocus();
                }
            }

            if ( !mpWindowImpl->mbFrame )
            {
                if (mpWindowImpl->mpWinData && mpWindowImpl->mpWinData->mbEnableNativeWidget)
                {
                    /*
                    * #i48371# native theming: some themes draw outside the control
                    * area we tell them to (bad thing, but we cannot do much about it ).
                    * On hiding these controls they get invalidated with their window rectangle
                    * which leads to the parts outside the control area being left and not
                    * invalidated. Workaround: invalidate an area on the parent, too
                    */
                    const int workaround_border = 5;
                    tools::Rectangle aBounds( aInvRegion.GetBoundRect() );
                    aBounds.AdjustLeft( -workaround_border );
                    aBounds.AdjustTop( -workaround_border );
                    aBounds.AdjustRight(workaround_border );
                    aBounds.AdjustBottom(workaround_border );
                    aInvRegion = aBounds;
                }
                if ( !mpWindowImpl->mbNoParentUpdate )
                {
                    if ( !aInvRegion.IsEmpty() )
                        ImplInvalidateParentFrameRegion( aInvRegion );
                }
                ImplGenerateMouseMove();
            }
        }
    }
    else
    {
        // inherit native widget flag for form controls
        // required here, because frames never show up in the child hierarchy - which should be fixed...
        // eg, the drop down of a combobox which is a system floating window
        if( mpWindowImpl->mbFrame && GetParent() && !GetParent()->isDisposed() &&
            GetParent()->IsCompoundControl() &&
            GetParent()->IsNativeWidgetEnabled() != IsNativeWidgetEnabled() &&
            !(GetStyle() & WB_TOOLTIPWIN) )
        {
            EnableNativeWidget( GetParent()->IsNativeWidgetEnabled() );
        }

        if ( mpWindowImpl->mbCallMove )
        {
            ImplCallMove();
        }
        if ( mpWindowImpl->mbCallResize )
        {
            ImplCallResize();
        }

        CompatStateChanged( StateChangedType::Visible );

        vcl::Window* pTestParent;
        if ( ImplIsOverlapWindow() )
            pTestParent = mpWindowImpl->mpOverlapWindow;
        else
            pTestParent = ImplGetParent();
        if ( mpWindowImpl->mbFrame || pTestParent->mpWindowImpl->mbReallyVisible )
        {
            // if a window becomes visible, send all child windows a StateChange,
            // such that these can initialise themselves
            ImplCallInitShow();

            // If it is a SystemWindow it automatically pops up on top of
            // all other windows if needed.
            if (ImplIsOverlapWindow())
            {
                if (!(nFlags & ShowFlags::NoActivate))
                {
                    ImplStartToTop((nFlags & ShowFlags::ForegroundTask) ? ToTopFlags::ForegroundTask
                                                                        : ToTopFlags::NONE);
                    ImplFocusToTop(ToTopFlags::NONE, false);

                    if (!(nFlags & ShowFlags::ForegroundTask))
                    {
                        // Inform user about window if we did not popup it at foreground
                        FlashWindow();
                    }
                }
            }

            // adjust mpWindowImpl->mbReallyVisible
            bRealVisibilityChanged = !mpWindowImpl->mbReallyVisible;
            ImplSetReallyVisible();

            // assure clip rectangles will be recalculated
            vcl::clipping::setClipFlag(*this);

            if ( !mpWindowImpl->mbFrame )
            {
                InvalidateFlags nInvalidateFlags = InvalidateFlags::Children;
                if( ! IsPaintTransparent() )
                    nInvalidateFlags |= InvalidateFlags::NoTransparent;
                ImplInvalidate( nullptr, nInvalidateFlags );
                ImplGenerateMouseMove();
            }
        }

        if ( mpWindowImpl->mpBorderWindow )
            mpWindowImpl->mpBorderWindow->Show( true, nFlags );
        else if ( mpWindowImpl->mbFrame )
        {
            // #106431#, hide SplashScreen
            ImplSVData* pSVData = ImplGetSVData();
            if ( !pSVData->mpIntroWindow )
            {
                // The right way would be just to call this (not even in the 'if')
                auto pApp = GetpApp();
                if ( pApp )
                    pApp->InitFinished();
            }
            else if ( !ImplIsWindowOrChild( pSVData->mpIntroWindow ) )
            {
                // ... but the VCL splash is broken, and it needs this
                // (for ./soffice .uno:NewDoc)
                pSVData->mpIntroWindow->Hide();
            }

            //SAL_WARN_IF( mpWindowImpl->mbSuppressAccessibilityEvents, "vcl", "Window::Show() - Frame reactivated");
            mpWindowImpl->mbSuppressAccessibilityEvents = false;

            mpWindowImpl->mbPaintFrame = true;
            if (!Application::IsHeadlessModeEnabled())
            {
                bool bNoActivate(nFlags & (ShowFlags::NoActivate|ShowFlags::NoFocusChange));
                mpWindowImpl->mpFrame->Show( true, bNoActivate );
            }
            if( !xWindow->mpWindowImpl )
                return;

            // Query the correct size of the window, if we are waiting for
            // a system resize
            if ( mpWindowImpl->mbWaitSystemResize )
            {
                tools::Long nOutWidth;
                tools::Long nOutHeight;
                mpWindowImpl->mpFrame->GetClientSize( nOutWidth, nOutHeight );
                ImplHandleResize( this, nOutWidth, nOutHeight );
            }

            if (mpWindowImpl->mpFrameData->mpBuffer && mpWindowImpl->mpFrameData->mpBuffer->GetOutputSizePixel() != GetOutputSizePixel())
                // Make sure that the buffer size matches the window size, even if no resize was needed.
                mpWindowImpl->mpFrameData->mpBuffer->SetOutputSizePixel(GetOutputSizePixel());
        }

        if( !xWindow->mpWindowImpl )
            return;

        ImplShowAllOverlaps();
    }

    if( !xWindow->mpWindowImpl )
        return;

    // the SHOW/HIDE events also serve as indicators to send child creation/destroy events to the access bridge
    // However, the access bridge only uses this event if the data member is not NULL (it's kind of a hack that
    // we re-use the SHOW/HIDE events this way, with this particular semantics).
    // Since #104887#, the notifications for the access bridge are done in Impl(Set|Reset)ReallyVisible. Here, we
    // now only notify with a NULL data pointer, for all other clients except the access bridge.
    if ( !bRealVisibilityChanged )
        CallEventListeners( mpWindowImpl->mbVisible ? VclEventId::WindowShow : VclEventId::WindowHide );
}

Size Window::GetSizePixel() const
{
    if (!mpWindowImpl)
    {
        SAL_WARN("vcl.layout", "WTF no windowimpl");
        return Size(0,0);
    }

    // #i43257# trigger pending resize handler to assure correct window sizes
    if( mpWindowImpl->mpFrameData->maResizeIdle.IsActive() )
    {
        VclPtr<vcl::Window> xWindow( const_cast<Window*>(this) );
        mpWindowImpl->mpFrameData->maResizeIdle.Stop();
        mpWindowImpl->mpFrameData->maResizeIdle.Invoke( nullptr );
        if( xWindow->isDisposed() )
            return Size(0,0);
    }

    return Size( GetOutDev()->GetOutputWidthPixel() + mpWindowImpl->mnLeftBorder+mpWindowImpl->mnRightBorder,
                 GetOutDev()->GetOutputHeightPixel() + mpWindowImpl->mnTopBorder+mpWindowImpl->mnBottomBorder );
}

void Window::GetBorder( sal_Int32& rLeftBorder, sal_Int32& rTopBorder,
                               sal_Int32& rRightBorder, sal_Int32& rBottomBorder ) const
{
    rLeftBorder     = mpWindowImpl->mnLeftBorder;
    rTopBorder      = mpWindowImpl->mnTopBorder;
    rRightBorder    = mpWindowImpl->mnRightBorder;
    rBottomBorder   = mpWindowImpl->mnBottomBorder;
}

void Window::Enable( bool bEnable, bool bChild )
{
    if ( isDisposed() )
        return;

    if ( !bEnable )
    {
        // the tracking mode will be stopped or the capture will be stolen
        // when a window is disabled,
        if ( IsTracking() )
            EndTracking( TrackingEventFlags::Cancel );
        if ( IsMouseCaptured() )
            ReleaseMouse();
        // try to pass focus to the next control
        // if the window has focus and is contained in the dialog control
        // mpWindowImpl->mbDisabled should only be set after a call of ImplDlgCtrlNextWindow().
        // Otherwise ImplDlgCtrlNextWindow() should be used
        if ( HasFocus() )
            ImplDlgCtrlNextWindow();
    }

    if ( mpWindowImpl->mpBorderWindow )
    {
        mpWindowImpl->mpBorderWindow->Enable( bEnable, false );
        if ( (mpWindowImpl->mpBorderWindow->GetType() == WindowType::BORDERWINDOW) &&
             static_cast<ImplBorderWindow*>(mpWindowImpl->mpBorderWindow.get())->mpMenuBarWindow )
            static_cast<ImplBorderWindow*>(mpWindowImpl->mpBorderWindow.get())->mpMenuBarWindow->Enable( bEnable );
    }

    // #i56102# restore app focus win in case the
    // window was disabled when the frame focus changed
    ImplSVData* pSVData = ImplGetSVData();
    if (bEnable && pSVData->mpWinData->mpFocusWin == nullptr
        && mpWindowImpl->mpFrameData->mbHasFocus && mpWindowImpl->mpFrameData->mpFocusWin == this)
        pSVData->mpWinData->mpFocusWin = this;

    if ( mpWindowImpl->mbDisabled != !bEnable )
    {
        mpWindowImpl->mbDisabled = !bEnable;
        if ( mpWindowImpl->mpSysObj )
            mpWindowImpl->mpSysObj->Enable( bEnable && !mpWindowImpl->mbInputDisabled );
        CompatStateChanged( StateChangedType::Enable );

        CallEventListeners( bEnable ? VclEventId::WindowEnabled : VclEventId::WindowDisabled );
    }

    if ( bChild )
    {
        VclPtr< vcl::Window > pChild = mpWindowImpl->mpHierarchy->mpFirstChild;
        while ( pChild )
        {
            pChild->Enable( bEnable, bChild );
            pChild = pChild->mpWindowImpl->mpHierarchy->mpNext;
        }
    }

    if ( IsReallyVisible() )
        ImplGenerateMouseMove();
}

void Window::EnableInput( bool bEnable, bool bChild )
{
    if (!mpWindowImpl)
        return;

    if ( mpWindowImpl->mpBorderWindow )
    {
        mpWindowImpl->mpBorderWindow->EnableInput( bEnable, false );
        if ( (mpWindowImpl->mpBorderWindow->GetType() == WindowType::BORDERWINDOW) &&
             static_cast<ImplBorderWindow*>(mpWindowImpl->mpBorderWindow.get())->mpMenuBarWindow )
            static_cast<ImplBorderWindow*>(mpWindowImpl->mpBorderWindow.get())->mpMenuBarWindow->EnableInput( bEnable );
    }

    if ( (!bEnable && mpWindowImpl->meAlwaysInputMode != AlwaysInputEnabled) || bEnable )
    {
        // automatically stop the tracking mode or steal capture
        // if the window is disabled
        if ( !bEnable )
        {
            if ( IsTracking() )
                EndTracking( TrackingEventFlags::Cancel );
            if ( IsMouseCaptured() )
                ReleaseMouse();
        }

        if ( mpWindowImpl->mbInputDisabled != !bEnable )
        {
            mpWindowImpl->mbInputDisabled = !bEnable;
            if ( mpWindowImpl->mpSysObj )
                mpWindowImpl->mpSysObj->Enable( !mpWindowImpl->mbDisabled && bEnable );
        }
    }

    // #i56102# restore app focus win in case the
    // window was disabled when the frame focus changed
    ImplSVData* pSVData = ImplGetSVData();
    if (bEnable && pSVData->mpWinData->mpFocusWin == nullptr
        && mpWindowImpl->mpFrameData->mbHasFocus && mpWindowImpl->mpFrameData->mpFocusWin == this)
        pSVData->mpWinData->mpFocusWin = this;

    if ( bChild )
    {
        VclPtr< vcl::Window > pChild = mpWindowImpl->mpHierarchy->mpFirstChild;
        while ( pChild )
        {
            pChild->EnableInput( bEnable, bChild );
            pChild = pChild->mpWindowImpl->mpHierarchy->mpNext;
        }
    }

    if ( IsReallyVisible() )
        ImplGenerateMouseMove();
}

void Window::EnableInput( bool bEnable, const vcl::Window* pExcludeWindow )
{
    if (!mpWindowImpl)
        return;

    EnableInput( bEnable );

    // pExecuteWindow is the first Overlap-Frame --> if this
    // shouldn't be the case, then this must be changed in dialog.cxx
    if( pExcludeWindow )
        pExcludeWindow = pExcludeWindow->ImplGetFirstOverlapWindow();
    vcl::Window* pSysWin = mpWindowImpl->mpFrameWindow->mpWindowImpl->mpFrameData->mpFirstOverlap;
    while ( pSysWin )
    {
        // Is Window in the path from this window
        if ( ImplGetFirstOverlapWindow()->ImplIsWindowOrChild( pSysWin, true ) )
        {
            // Is Window not in the exclude window path or not the
            // exclude window, then change the status
            if ( !pExcludeWindow || !pExcludeWindow->ImplIsWindowOrChild( pSysWin, true ) )
                pSysWin->EnableInput( bEnable );
        }
        pSysWin = pSysWin->mpWindowImpl->mpHierarchy->mpNextOverlap;
    }

    // enable/disable floating system windows as well
    vcl::Window* pFrameWin = ImplGetSVData()->maFrameData.mpFirstFrame;
    while ( pFrameWin )
    {
        if( pFrameWin->ImplIsFloatingWindow() )
        {
            // Is Window in the path from this window
            if ( ImplGetFirstOverlapWindow()->ImplIsWindowOrChild( pFrameWin, true ) )
            {
                // Is Window not in the exclude window path or not the
                // exclude window, then change the status
                if ( !pExcludeWindow || !pExcludeWindow->ImplIsWindowOrChild( pFrameWin, true ) )
                    pFrameWin->EnableInput( bEnable );
            }
        }
        pFrameWin = pFrameWin->mpWindowImpl->mpFrameData->mpNextFrame;
    }

    // the same for ownerdraw floating windows
    if( !mpWindowImpl->mbFrame )
        return;

    ::std::vector< VclPtr<vcl::Window> >& rList = mpWindowImpl->mpFrameData->maOwnerDrawList;
    for (auto const& elem : rList)
    {
        // Is Window in the path from this window
        if ( ImplGetFirstOverlapWindow()->ImplIsWindowOrChild( elem, true ) )
        {
            // Is Window not in the exclude window path or not the
            // exclude window, then change the status
            if ( !pExcludeWindow || !pExcludeWindow->ImplIsWindowOrChild( elem, true ) )
                elem->EnableInput( bEnable );
        }
    }
}

void Window::AlwaysEnableInput( bool bAlways, bool bChild )
{

    if ( mpWindowImpl->mpBorderWindow )
        mpWindowImpl->mpBorderWindow->AlwaysEnableInput( bAlways, false );

    if( bAlways && mpWindowImpl->meAlwaysInputMode != AlwaysInputEnabled )
    {
        mpWindowImpl->meAlwaysInputMode = AlwaysInputEnabled;
        EnableInput(true, false);
    }
    else if( ! bAlways && mpWindowImpl->meAlwaysInputMode == AlwaysInputEnabled )
    {
        mpWindowImpl->meAlwaysInputMode = AlwaysInputNone;
    }

    if ( bChild )
    {
        VclPtr< vcl::Window > pChild = mpWindowImpl->mpHierarchy->mpFirstChild;
        while ( pChild )
        {
            pChild->AlwaysEnableInput( bAlways, bChild );
            pChild = pChild->mpWindowImpl->mpHierarchy->mpNext;
        }
    }
}

void Window::SetActivateMode( ActivateModeFlags nMode )
{

    if ( mpWindowImpl->mpBorderWindow )
        mpWindowImpl->mpBorderWindow->SetActivateMode( nMode );

    if ( mpWindowImpl->mnActivateMode == nMode )
        return;

    mpWindowImpl->mnActivateMode = nMode;

    // possibly trigger Deactivate/Activate
    if ( mpWindowImpl->mnActivateMode != ActivateModeFlags::NONE )
    {
        if ( (mpWindowImpl->mbActive || (GetType() == WindowType::BORDERWINDOW)) &&
             !HasChildPathFocus( true ) )
        {
            mpWindowImpl->mbActive = false;
            Deactivate();
        }
    }
    else
    {
        if ( !mpWindowImpl->mbActive || (GetType() == WindowType::BORDERWINDOW) )
        {
            mpWindowImpl->mbActive = true;
            Activate();
        }
    }
}

void Window::setPosSizePixel( tools::Long nX, tools::Long nY,
                              tools::Long nWidth, tools::Long nHeight, PosSizeFlags nFlags )
{
    bool bHasValidSize = !mpWindowImpl->mbDefSize;

    if ( nFlags & PosSizeFlags::Pos )
        mpWindowImpl->mbDefPos = false;
    if ( nFlags & PosSizeFlags::Size )
        mpWindowImpl->mbDefSize = false;

    // The top BorderWindow is the window which is to be positioned
    VclPtr<vcl::Window> pWindow = this;
    while ( pWindow->mpWindowImpl->mpBorderWindow )
        pWindow = pWindow->mpWindowImpl->mpBorderWindow;

    if (!pWindow->mpWindowImpl->mbFrame)
    {
        pWindow->ImplPosSizeWindow( nX, nY, nWidth, nHeight, nFlags );

        if ( IsReallyVisible() )
            ImplGenerateMouseMove();

        return;
    }

    // Note: if we're positioning a frame, the coordinates are interpreted
    // as being the top-left corner of the window's client area and NOT
    // as the position of the border ! (due to limitations of several UNIX window managers)
    tools::Long nOldWidth  = pWindow->GetOutDev()->GetOutputWidthPixel();

    if ( !(nFlags & PosSizeFlags::Width) )
        nWidth = pWindow->GetOutDev()->GetOutputWidthPixel();
    if ( !(nFlags & PosSizeFlags::Height) )
        nHeight = pWindow->GetOutDev()->GetOutputHeightPixel();

    sal_uInt16 nSysFlags=0;
    VclPtr<vcl::Window> pParent = GetParent();
    VclPtr<vcl::Window> pWinParent = pWindow->GetParent();

    if( nFlags & PosSizeFlags::Width )
        nSysFlags |= SAL_FRAME_POSSIZE_WIDTH;
    if( nFlags & PosSizeFlags::Height )
        nSysFlags |= SAL_FRAME_POSSIZE_HEIGHT;
    if( nFlags & PosSizeFlags::X )
    {
        nSysFlags |= SAL_FRAME_POSSIZE_X;
        if( pWinParent && (pWindow->GetStyle() & WB_SYSTEMCHILDWINDOW) )
        {
            nX += pWinParent->GetOutDev()->GetDeviceOriginX();
        }
        if( pParent && pParent->GetOutDev()->ImplIsAntiparallel() )
        {
            tools::Rectangle aRect( Point ( nX, nY ), Size( nWidth, nHeight ) );
            const OutputDevice *pParentOutDev = pParent->GetOutDev();
            if (!comphelper::LibreOfficeKit::isActive())
                pParentOutDev->ReMirror( aRect );
            nX = aRect.Left();
        }
    }
    if( !comphelper::LibreOfficeKit::isActive() &&
        !(nFlags & PosSizeFlags::X) && bHasValidSize &&
        pWindow->mpWindowImpl->mpFrame->GetWidth())
    {
        // RTL: make sure the old right aligned position is not changed
        // system windows will always grow to the right
        if ( pWinParent )
        {
            OutputDevice *pParentOutDev = pWinParent->GetOutDev();
            if( pParentOutDev->HasMirroredGraphics() )
            {
                const SalFrameGeometry aSysGeometry = mpWindowImpl->mpFrame->GetUnmirroredGeometry();
                const SalFrameGeometry aParentSysGeometry =
                    pWinParent->mpWindowImpl->mpFrame->GetUnmirroredGeometry();
                tools::Long myWidth = nOldWidth;
                if( !myWidth )
                    myWidth = aSysGeometry.width();
                if( !myWidth )
                    myWidth = nWidth;
                nFlags |= PosSizeFlags::X;
                nSysFlags |= SAL_FRAME_POSSIZE_X;
                nX = aParentSysGeometry.x() - aSysGeometry.leftDecoration() + aParentSysGeometry.width()
                    - myWidth - 1 - aSysGeometry.x();
            }
        }
    }
    if( nFlags & PosSizeFlags::Y )
    {
        nSysFlags |= SAL_FRAME_POSSIZE_Y;
        if( pWinParent && (pWindow->GetStyle() & WB_SYSTEMCHILDWINDOW) )
        {
            nY += pWinParent->GetOutDev()->GetDeviceOriginY();
        }
    }

    if( nSysFlags & (SAL_FRAME_POSSIZE_WIDTH|SAL_FRAME_POSSIZE_HEIGHT) )
    {
        // check for min/max client size and adjust size accordingly
        // otherwise it may happen that the resize event is ignored, i.e. the old size remains
        // unchanged but ImplHandleResize() is called with the wrong size
        SystemWindow *pSystemWindow = dynamic_cast< SystemWindow* >( pWindow.get() );
        if( pSystemWindow )
        {
            Size aMinSize = pSystemWindow->GetMinOutputSizePixel();
            Size aMaxSize = pSystemWindow->GetMaxOutputSizePixel();
            if( nWidth < aMinSize.Width() )
                nWidth = aMinSize.Width();
            if( nHeight < aMinSize.Height() )
                nHeight = aMinSize.Height();

            if( nWidth > aMaxSize.Width() )
                nWidth = aMaxSize.Width();
            if( nHeight > aMaxSize.Height() )
                nHeight = aMaxSize.Height();
        }
    }

    pWindow->mpWindowImpl->mpFrame->SetPosSize( nX, nY, nWidth, nHeight, nSysFlags );

    // Adjust resize with the hack of different client size and frame geometries to fix
    // native menu bars. Eventually this should be replaced by proper mnTopBorder usage.
    pWindow->mpWindowImpl->mpFrame->GetClientSize(nWidth, nHeight);

    // Resize should be called directly. If we haven't
    // set the correct size, we get a second resize from
    // the system with the correct size. This can be happened
    // if the size is too small or too large.
    ImplHandleResize( pWindow, nWidth, nHeight );
}

Point Window::GetPosPixel() const
{
    return mpWindowImpl->maPos;
}

AbsoluteScreenPixelRectangle Window::GetDesktopRectPixel() const
{
    AbsoluteScreenPixelRectangle rRect;
    mpWindowImpl->mpFrameWindow->mpWindowImpl->mpFrame->GetWorkArea( rRect );
    return rRect;
}

tools::Long Window::ImplGetUnmirroredOutOffX() const
{
    // revert GetDeviceOriginX() changes that were potentially made in ImplPosSizeWindow
    tools::Long offx = GetOutDev()->GetDeviceOriginX();
    const OutputDevice *pOutDev = GetOutDev();

    if (!pOutDev->HasMirroredGraphics())
        return offx;

    if (!mpWindowImpl->mpHierarchy->mpParent
        || mpWindowImpl->mpHierarchy->mpParent->mpWindowImpl->mbFrame
        || !mpWindowImpl->mpHierarchy->mpParent->GetOutDev()->ImplIsAntiparallel())
    {
        return offx;
    }

    if ( !ImplIsOverlapWindow() )
        offx -= mpWindowImpl->mpHierarchy->mpParent->GetOutDev()->GetDeviceOriginX();

    offx = mpWindowImpl->mpHierarchy->mpParent->GetOutDev()->GetOutputWidthPixel() - GetOutDev()->GetOutputWidthPixel() - offx;

    if ( !ImplIsOverlapWindow() )
        offx += mpWindowImpl->mpHierarchy->mpParent->GetOutDev()->GetDeviceOriginX();

    return offx;
}

void Window::Scroll( tools::Long nHorzScroll, tools::Long nVertScroll, ScrollFlags nFlags )
{

    ImplScroll( GetOutputRectPixel(),
                nHorzScroll, nVertScroll, nFlags & ~ScrollFlags::Clip );
}

void Window::Scroll( tools::Long nHorzScroll, tools::Long nVertScroll,
                     const tools::Rectangle& rRect, ScrollFlags nFlags )
{
    OutputDevice *pOutDev = GetOutDev();
    tools::Rectangle aRect = pOutDev->GetMapper().LogicToDevicePixel(rRect, pOutDev->GetMappingPolicy());
    aRect.Intersection( GetOutputRectPixel() );
    if ( !aRect.IsEmpty() )
        ImplScroll( aRect, nHorzScroll, nVertScroll, nFlags );
}

void Window::SetUpdateMode( bool bUpdate )
{
    if (mpWindowImpl)
    {
        mpWindowImpl->mbNoUpdate = !bUpdate;
        CompatStateChanged( StateChangedType::UpdateMode );
    }
}

void Window::GrabFocus()
{
    ImplGrabFocus( GetFocusFlags::NONE );
}

bool Window::HasFocus() const
{
    return (this == ImplGetSVData()->mpWinData->mpFocusWin);
}

void Window::GrabFocusToDocument()
{
    ImplGrabFocusToDocument(GetFocusFlags::NONE);
}

VclPtr<vcl::Window> Window::GetFocusedWindow() const
{
    if (mpWindowImpl && mpWindowImpl->mpFrameData)
        return mpWindowImpl->mpFrameData->mpFocusWin;

    return VclPtr<vcl::Window>();
}

void Window::SetFakeFocus( bool bFocus )
{
    ImplGetWindowImpl()->mbFakeFocusSet = bFocus;
}

bool Window::HasChildPathFocus( bool bSystemWindow ) const
{

    vcl::Window* pFocusWin = ImplGetSVData()->mpWinData->mpFocusWin;
    if ( pFocusWin )
        return ImplIsWindowOrChild( pFocusWin, bSystemWindow );
    return false;
}

void Window::SetCursor( vcl::Cursor* pCursor )
{
    if (mpWindowImpl->mpCursor == pCursor)
        return;

    if (mpWindowImpl->mpCursor)
        mpWindowImpl->mpCursor->ImplHide();

    mpWindowImpl->mpCursor = pCursor;

    if (pCursor)
        pCursor->ImplShow();
}

void Window::SetText( const OUString& rStr )
{
    if (!mpWindowImpl || rStr == mpWindowImpl->maText)
        return;

    OUString oldTitle( mpWindowImpl->maText );
    mpWindowImpl->maText = rStr;

    if ( mpWindowImpl->mpBorderWindow )
        mpWindowImpl->mpBorderWindow->SetText( rStr );
    else if ( mpWindowImpl->mbFrame )
        mpWindowImpl->mpFrame->SetTitle( rStr );

    CallEventListeners( VclEventId::WindowFrameTitleChanged, &oldTitle );

    // #107247# needed for accessibility
    // The VclEventId::WindowFrameTitleChanged is (mis)used to notify accessible name changes.
    // Therefore a window, which is labeled by this window, must also notify an accessible
    // name change.
    if ( IsReallyVisible() )
    {
        vcl::Window* pWindow = GetAccessibleRelationLabelFor();
        if ( pWindow && pWindow != this )
            pWindow->CallEventListeners( VclEventId::WindowFrameTitleChanged, &oldTitle );
    }

    CompatStateChanged( StateChangedType::Text );
}

OUString Window::GetText() const
{

    return mpWindowImpl->maText;
}

OUString Window::GetDisplayText() const
{

    return GetText();
}

const Wallpaper& Window::GetDisplayBackground() const
{
    // FIXME: fix issue 52349, need to fix this really in
    // all NWF enabled controls
    const ToolBox* pTB = dynamic_cast<const ToolBox*>(this);
    if( pTB && IsNativeWidgetEnabled() )
        return pTB->ImplGetToolBoxPrivateData()->maDisplayBackground;

    if( !IsBackground() )
    {
        if( mpWindowImpl->mpHierarchy->mpParent )
            return mpWindowImpl->mpHierarchy->mpParent->GetDisplayBackground();
    }

    const Wallpaper& rBack = GetBackground();
    if( ! rBack.IsBitmap() &&
        ! rBack.IsGradient() &&
        rBack.GetColor()== COL_TRANSPARENT &&
        mpWindowImpl->mpHierarchy->mpParent )
            return mpWindowImpl->mpHierarchy->mpParent->GetDisplayBackground();
    return rBack;
}

const OUString& Window::GetHelpText() const
{
    const OUString& rStrHelpId(GetHelpId());
    const bool bStrHelpId = !rStrHelpId.isEmpty();

    if (mpWindowImpl->mbHelpTextDynamic && bStrHelpId)
    {
        static const char* pEnv = getenv( "HELP_DEBUG" );
        if( pEnv && *pEnv )
        {
            mpWindowImpl->maHelpText = mpWindowImpl->maHelpText + "\n------------------\n" + rStrHelpId;
        }
        mpWindowImpl->mbHelpTextDynamic = false;
    }

    //Fallback to Window::GetAccessibleDescription without reentry to GetHelpText()
    if (mpWindowImpl->maHelpText.isEmpty() && mpWindowImpl->mpAccessibleInfos && mpWindowImpl->mpAccessibleInfos->pAccessibleDescription)
        return *mpWindowImpl->mpAccessibleInfos->pAccessibleDescription;
    return mpWindowImpl->maHelpText;
}

void Window::SetWindowPeer( Reference< css::awt::XVclWindowPeer > const & xPeer, VCLXWindow* pVCLXWindow  )
{
    if (!mpWindowImpl || mpWindowImpl->mbInDispose)
        return;

    // be safe against re-entrance: first clear the old ref, then assign the new one
    if (mpWindowImpl->mxWindowPeer)
    {
        // first, disconnect the peer from ourself, otherwise disposing it, will dispose us
        UnoWrapperBase* pWrapper = UnoWrapperBase::GetUnoWrapper();
        SAL_WARN_IF( !pWrapper, "vcl.window", "SetComponentInterface: No Wrapper!" );
        if ( pWrapper )
            pWrapper->SetWindowInterface( nullptr, mpWindowImpl->mxWindowPeer );
        mpWindowImpl->mxWindowPeer->dispose();
        mpWindowImpl->mxWindowPeer.clear();
    }
    mpWindowImpl->mxWindowPeer = xPeer;

    mpWindowImpl->mpVCLXWindow = pVCLXWindow;
}

Reference< css::awt::XVclWindowPeer > Window::GetComponentInterface( bool bCreate )
{
    if ( !mpWindowImpl->mxWindowPeer.is() && bCreate )
    {
        UnoWrapperBase* pWrapper = UnoWrapperBase::GetUnoWrapper();
        if ( pWrapper )
            mpWindowImpl->mxWindowPeer = pWrapper->GetWindowInterface( this );
    }
    return mpWindowImpl->mxWindowPeer;
}

void Window::SetComponentInterface( Reference< css::awt::XVclWindowPeer > const & xIFace )
{
    UnoWrapperBase* pWrapper = UnoWrapperBase::GetUnoWrapper();
    SAL_WARN_IF( !pWrapper, "vcl.window", "SetComponentInterface: No Wrapper!" );
    if ( pWrapper )
        pWrapper->SetWindowInterface( this, xIFace );
}

typedef std::map<vcl::LOKWindowId, VclPtr<vcl::Window>> LOKWindowsMap;

namespace {

LOKWindowsMap& GetLOKWindowsMap()
{
    // Map to remember the LOKWindowId <-> Window binding.
    static LOKWindowsMap s_aLOKWindowsMap;

    return s_aLOKWindowsMap;
}

}

// Counter to be able to have unique id's for each window.
static vcl::LOKWindowId sLastLOKWindowId = 1;

void Window::SetLOKNotifier(const vcl::ILibreOfficeKitNotifier* pNotifier, bool bParent)
{
    // don't allow setting this twice
    assert(mpWindowImpl->mpLOKNotifier == nullptr);
    assert(pNotifier);
    // never use this in the desktop case
    assert(comphelper::LibreOfficeKit::isActive());

    if (!bParent)
    {
        // assign the LOK window id
        assert(mpWindowImpl->mnLOKWindowId == 0);
        mpWindowImpl->mnLOKWindowId = sLastLOKWindowId++;
        GetLOKWindowsMap().emplace(mpWindowImpl->mnLOKWindowId, this);
    }

    mpWindowImpl->mpLOKNotifier = pNotifier;
}

void Window::SetLOKWindowId()
{
    // never use this in the desktop case
    assert(comphelper::LibreOfficeKit::isActive());

    // assign the LOK window id
    assert(mpWindowImpl->mnLOKWindowId == 0);
    mpWindowImpl->mnLOKWindowId = sLastLOKWindowId++;
    GetLOKWindowsMap().emplace(mpWindowImpl->mnLOKWindowId, this);
}

VclPtr<Window> Window::FindLOKWindow(vcl::LOKWindowId nWindowId)
{
    const auto it = GetLOKWindowsMap().find(nWindowId);
    if (it != GetLOKWindowsMap().end())
        return it->second;

    return VclPtr<Window>();
}

bool Window::IsLOKWindowsEmpty()
{
    return GetLOKWindowsMap().empty();
}

void Window::ReleaseLOKNotifier()
{
    // unregister the LOK window binding
    if (mpWindowImpl->mnLOKWindowId > 0)
        GetLOKWindowsMap().erase(mpWindowImpl->mnLOKWindowId);

    mpWindowImpl->mpLOKNotifier = nullptr;
    mpWindowImpl->mnLOKWindowId = 0;
}

ILibreOfficeKitNotifier::~ILibreOfficeKitNotifier()
{
    if (!comphelper::LibreOfficeKit::isActive())
    {
        return;
    }

    for (auto it = GetLOKWindowsMap().begin(); it != GetLOKWindowsMap().end();)
    {
        WindowImpl* pWindowImpl = it->second->ImplGetWindowImpl();
        if (pWindowImpl && pWindowImpl->mpLOKNotifier == this)
        {
            pWindowImpl->mpLOKNotifier = nullptr;
            pWindowImpl->mnLOKWindowId = 0;
            it = GetLOKWindowsMap().erase(it);
            continue;
        }

        ++it;
    }
}

const vcl::ILibreOfficeKitNotifier* Window::GetLOKNotifier() const
{
    return mpWindowImpl ? mpWindowImpl->mpLOKNotifier : nullptr;
}

vcl::LOKWindowId Window::GetLOKWindowId() const
{
    return mpWindowImpl ? mpWindowImpl->mnLOKWindowId : 0;
}

VclPtr<vcl::Window> Window::GetParentWithLOKNotifier()
{
    VclPtr<vcl::Window> pWindow(this);

    while (pWindow && !pWindow->GetLOKNotifier())
        pWindow = pWindow->GetParent();

    return pWindow;
}

void Window::ImplCallDeactivateListeners( vcl::Window *pNew )
{
    // no deactivation if the newly activated window is my child
    if ( !pNew || !ImplIsChild( pNew ) )
    {
        VclPtr<vcl::Window> xWindow(this);
        CallEventListeners( VclEventId::WindowDeactivate, pNew );
        if( !xWindow->mpWindowImpl )
            return;

        // #100759#, avoid walking the wrong frame's hierarchy
        //           eg, undocked docking windows (ImplDockFloatWin)
        if ( ImplGetParent() && ImplGetParent()->mpWindowImpl &&
             mpWindowImpl->mpFrameWindow == ImplGetParent()->mpWindowImpl->mpFrameWindow )
            ImplGetParent()->ImplCallDeactivateListeners( pNew );
    }
}

void Window::ImplCallActivateListeners( vcl::Window *pOld )
{
    // no activation if the old active window is my child
    if ( pOld && ImplIsChild( pOld ))
        return;

    VclPtr<vcl::Window> xWindow(this);
    CallEventListeners( VclEventId::WindowActivate, pOld );
    if( !xWindow->mpWindowImpl )
        return;

    if ( ImplGetParent() )
        ImplGetParent()->ImplCallActivateListeners( pOld );
    else if( (mpWindowImpl->mnStyle & WB_INTROWIN) == 0 )
    {
        // top level frame reached: store hint for DefModalDialogParent
        ImplGetSVData()->maFrameData.mpActiveApplicationFrame = mpWindowImpl->mpFrameWindow;
    }
}

void Window::SetClipboard(Reference<XClipboard> const & xClipboard)
{
    if (mpWindowImpl->mpFrameData)
        mpWindowImpl->mpFrameData->mxClipboard = xClipboard;
}

Reference< XClipboard > Window::GetClipboard()
{
    if (!mpWindowImpl->mpFrameData)
        return static_cast<XClipboard*>(nullptr);
    if (!mpWindowImpl->mpFrameData->mxClipboard.is())
        mpWindowImpl->mpFrameData->mxClipboard = GetSystemClipboard();
    return mpWindowImpl->mpFrameData->mxClipboard;
}

void Window::RecordLayoutData( vcl::ControlLayoutData* pLayout, const tools::Rectangle& rRect )
{
    assert(GetOutDev()->mpOutDevData);
    GetOutDev()->mpOutDevData->mpRecordLayout = pLayout;
    GetOutDev()->mpOutDevData->maRecordRect = rRect;
    Paint(*GetOutDev(), rRect);
    GetOutDev()->mpOutDevData->mpRecordLayout = nullptr;
}

bool Window::IsScrollable() const
{
    // check for scrollbars
    VclPtr< vcl::Window > pChild = mpWindowImpl->mpHierarchy->mpFirstChild;
    while( pChild )
    {
        if( pChild->GetType() == WindowType::SCROLLBAR )
            return true;
        else
            pChild = pChild->mpWindowImpl->mpHierarchy->mpNext;
    }
    return false;
}

void Window::ImplMirrorFramePos( Point &pt ) const
{
    pt.setX(mpWindowImpl->mpFrame->GetWidth() - 1 - pt.X());
}

// frame based modal counter (dialogs are not modal to the whole application anymore)
bool Window::IsInModalMode() const
{
    return (mpWindowImpl->mpFrameWindow->mpWindowImpl->mpFrameData->mnModalMode != 0);
}

void Window::IncModalCount()
{
    vcl::Window* pFrameWindow = mpWindowImpl->mpFrameWindow;
    vcl::Window* pParent = pFrameWindow;
    while( pFrameWindow )
    {
        pFrameWindow->mpWindowImpl->mpFrameData->mnModalMode++;
        while( pParent && pParent->mpWindowImpl->mpFrameWindow == pFrameWindow )
        {
            pParent = pParent->GetParent();
        }
        pFrameWindow = pParent ? pParent->mpWindowImpl->mpFrameWindow.get() : nullptr;
    }
}
void Window::DecModalCount()
{
    vcl::Window* pFrameWindow = mpWindowImpl->mpFrameWindow;
    vcl::Window* pParent = pFrameWindow;
    while( pFrameWindow )
    {
        pFrameWindow->mpWindowImpl->mpFrameData->mnModalMode--;
        while( pParent && pParent->mpWindowImpl->mpFrameWindow == pFrameWindow )
        {
            pParent = pParent->GetParent();
        }
        pFrameWindow = pParent ? pParent->mpWindowImpl->mpFrameWindow.get() : nullptr;
    }
}

void Window::ImplIsInTaskPaneList( bool mbIsInTaskList )
{
    mpWindowImpl->mbIsInTaskPaneList = mbIsInTaskList;
}

void Window::ImplNotifyIconifiedState( bool bIconified )
{
    mpWindowImpl->mpFrameWindow->CallEventListeners( bIconified ? VclEventId::WindowMinimize : VclEventId::WindowNormalize );
    // #109206# notify client window as well to have toolkit topwindow listeners notified
    if( mpWindowImpl->mpFrameWindow->mpWindowImpl->mpClientWindow && mpWindowImpl->mpFrameWindow != mpWindowImpl->mpFrameWindow->mpWindowImpl->mpClientWindow )
        mpWindowImpl->mpFrameWindow->mpWindowImpl->mpClientWindow->CallEventListeners( bIconified ? VclEventId::WindowMinimize : VclEventId::WindowNormalize );
}

bool Window::HasActiveChildFrame() const
{
    bool bRet = false;
    vcl::Window *pFrameWin = ImplGetSVData()->maFrameData.mpFirstFrame;
    while( pFrameWin )
    {
        if( pFrameWin != mpWindowImpl->mpFrameWindow )
        {
            bool bDecorated = false;
            VclPtr< vcl::Window > pChildFrame = pFrameWin->ImplGetWindow();
            // #i15285# unfortunately WB_MOVEABLE is the same as WB_TABSTOP which can
            // be removed for ToolBoxes to influence the keyboard accessibility
            // thus WB_MOVEABLE is no indicator for decoration anymore
            // but FloatingWindows carry this information in their TitleType...
            // TODO: avoid duplicate WinBits !!!
            if( pChildFrame && pChildFrame->ImplIsFloatingWindow() )
                bDecorated = static_cast<FloatingWindow*>(pChildFrame.get())->GetTitleType() != FloatWinTitleType::NONE;
            if( bDecorated || (pFrameWin->mpWindowImpl->mnStyle & (WB_MOVEABLE | WB_SIZEABLE) ) )
                if( pChildFrame && pChildFrame->IsVisible() && pChildFrame->IsActive() )
                {
                    if( ImplIsChild( pChildFrame, true ) )
                    {
                        bRet = true;
                        break;
                    }
                }
        }
        pFrameWin = pFrameWin->mpWindowImpl->mpFrameData->mpNextFrame;
    }
    return bRet;
}

LanguageType Window::GetInputLanguage() const
{
    return mpWindowImpl->mpFrame->GetInputLanguage();
}

void Window::EnableNativeWidget( bool bEnable )
{
    static const char* pNoNWF = getenv( "SAL_NO_NWF" );
    if( pNoNWF && *pNoNWF )
        bEnable = false;

    if( bEnable != ImplGetWinData()->mbEnableNativeWidget )
    {
        ImplGetWinData()->mbEnableNativeWidget = bEnable;

        // send datachanged event to allow for internal changes required for NWF
        // like clipmode, transparency, etc.
        DataChangedEvent aDCEvt( DataChangedEventType::SETTINGS, &*GetOutDev()->moSettings, AllSettingsFlags::STYLE );
        CompatDataChanged( aDCEvt );

        // sometimes the borderwindow is queried, so keep it in sync
        if( mpWindowImpl->mpBorderWindow )
            mpWindowImpl->mpBorderWindow->ImplGetWinData()->mbEnableNativeWidget = bEnable;
    }

    // push down, useful for compound controls
    VclPtr< vcl::Window > pChild = mpWindowImpl->mpHierarchy->mpFirstChild;
    while( pChild )
    {
        pChild->EnableNativeWidget( bEnable );
        pChild = pChild->mpWindowImpl->mpHierarchy->mpNext;
    }
}

bool Window::IsNativeWidgetEnabled() const
{
    return mpWindowImpl && ImplGetWinData()->mbEnableNativeWidget;
}

OUString Window::GetSurroundingText() const
{
  return OUString();
}

Selection Window::GetSurroundingTextSelection() const
{
  return Selection( 0, 0 );
}

namespace
{
    using namespace com::sun::star;

    uno::Reference<accessibility::XAccessibleEditableText>
    lcl_FindFocusedEditableText(uno::Reference<accessibility::XAccessibleContext> const& xContext)
    {
        if (!xContext.is())
            return uno::Reference<accessibility::XAccessibleEditableText>();

        sal_Int64 nState = xContext->getAccessibleStateSet();
        if (nState & accessibility::AccessibleStateType::FOCUSED)
        {
            uno::Reference<accessibility::XAccessibleEditableText> xText(xContext, uno::UNO_QUERY);
            if (xText.is())
                return xText;
            if (nState & accessibility::AccessibleStateType::MANAGES_DESCENDANTS)
                return uno::Reference<accessibility::XAccessibleEditableText>();
        }

        bool bSafeToIterate = true;
        sal_Int64 nCount = xContext->getAccessibleChildCount();
        if (nCount < 0 || nCount > SAL_MAX_UINT16 /* slow enough for anyone */)
            bSafeToIterate = false;
        if (!bSafeToIterate)
            return uno::Reference<accessibility::XAccessibleEditableText>();

        for (sal_Int64 i = 0; i < xContext->getAccessibleChildCount(); ++i)
        {
            uno::Reference<accessibility::XAccessible> xChild = xContext->getAccessibleChild(i);
            if (!xChild.is())
                continue;
            uno::Reference<accessibility::XAccessibleContext> xChildContext
                = xChild->getAccessibleContext();
            if (!xChildContext.is())
                continue;
            uno::Reference<accessibility::XAccessibleEditableText> xText
                = lcl_FindFocusedEditableText(xChildContext);
            if (xText.is())
                return xText;
        }
        return uno::Reference<accessibility::XAccessibleEditableText>();
    }

    uno::Reference<accessibility::XAccessibleEditableText> lcl_GetxText(vcl::Window *pFocusWin)
    {
        uno::Reference<accessibility::XAccessibleEditableText> xText;
        try
        {
            rtl::Reference<comphelper::OAccessible> pAccessible = pFocusWin->GetAccessible();
            if (pAccessible.is())
                xText = lcl_FindFocusedEditableText(pAccessible);
        }
        catch(const uno::Exception&)
        {
            TOOLS_WARN_EXCEPTION( "vcl.gtk3", "Exception in getting input method surrounding text");
        }
        return xText;
    }
}

// this is a rubbish implementation using a11y, ideally all subclasses implementing
// GetSurroundingText/GetSurroundingTextSelection should implement this and then this
// should be removed in favor of a stub that returns false
bool Window::DeleteSurroundingText(const Selection& rSelection)
{
    uno::Reference<accessibility::XAccessibleEditableText> xText = lcl_GetxText(this);
    if (!xText.is())
        return false;

    sal_Int32 nPosition = xText->getCaretPosition();

    // #i111768# range checking
    sal_Int32 nDeletePos = rSelection.Min();
    sal_Int32 nDeleteEnd = rSelection.Max();

    if (nDeletePos < 0)
        nDeletePos = 0;

    if (nDeleteEnd < 0)
        nDeleteEnd = 0;

    if (nDeleteEnd > xText->getCharacterCount())
        nDeleteEnd = xText->getCharacterCount();

    xText->deleteText(nDeletePos, nDeleteEnd);

    //tdf91641 adjust cursor if deleted chars shift it forward (normal case)
    if (nDeletePos < nPosition)
    {
        if (nDeleteEnd <= nPosition)
            nPosition = nPosition - (nDeleteEnd - nDeletePos);
        else
            nPosition = nDeletePos;

        if (xText->getCharacterCount() >= nPosition)
            xText->setCaretPosition( nPosition );
    }
    return true;
}

void Window::ApplySettings(vcl::RenderContext& /*rRenderContext*/)
{
}

const SystemEnvData* Window::GetSystemData() const
{

    return mpWindowImpl->mpFrame ? &mpWindowImpl->mpFrame->GetSystemData() : nullptr;
}

bool Window::SupportsDoubleBuffering() const
{
    return mpWindowImpl->mpFrameData->mpBuffer;
}

void Window::RequestDoubleBuffering(bool bRequest)
{
    if (bRequest)
    {
        mpWindowImpl->mpFrameData->mpBuffer = VclPtrInstance<VirtualDevice>();
        // Make sure that the buffer size matches the frame size.
        mpWindowImpl->mpFrameData->mpBuffer->SetOutputSizePixel(mpWindowImpl->mpFrameWindow->GetOutputSizePixel());
    }
    else
        mpWindowImpl->mpFrameData->mpBuffer.reset();
}

/*
 * The rationale here is that we moved destructors to
 * dispose and this altered a lot of code paths, that
 * are better left unchanged for now.
 */
void Window::CompatGetFocus()
{
    if (!mpWindowImpl || mpWindowImpl->mbInDispose)
        Window::GetFocus();
    else
        GetFocus();
}

void Window::CompatLoseFocus()
{
    if (!mpWindowImpl || mpWindowImpl->mbInDispose)
        Window::LoseFocus();
    else
        LoseFocus();
}

void Window::CompatStateChanged( StateChangedType nStateChange )
{
    if (!mpWindowImpl || mpWindowImpl->mbInDispose)
        Window::StateChanged(nStateChange);
    else
        StateChanged(nStateChange);
}

void Window::CompatDataChanged( const DataChangedEvent& rDCEvt )
{
    if (!mpWindowImpl || mpWindowImpl->mbInDispose)
        Window::DataChanged(rDCEvt);
    else
        DataChanged(rDCEvt);
}

bool Window::CompatPreNotify( NotifyEvent& rNEvt )
{
    if (!mpWindowImpl || mpWindowImpl->mbInDispose)
        return Window::PreNotify( rNEvt );
    else
        return PreNotify( rNEvt );
}

bool Window::CompatNotify( NotifyEvent& rNEvt )
{
    if (!mpWindowImpl || mpWindowImpl->mbInDispose)
        return Window::EventNotify( rNEvt );
    else
        return EventNotify( rNEvt );
}

void Window::set_id(const OUString& rID)
{
    mpWindowImpl->maID = rID;
}

const OUString& Window::get_id() const
{
    static OUString empty;
    return mpWindowImpl ? mpWindowImpl->maID : empty;
}

FactoryFunction Window::GetUITestFactory() const
{
    return WindowUIObject::create;
}

tools::Long Window::LogicWidthToDevicePixel(tools::Long nWidth) const
{
    return GetOutDev()->LogicWidthToDevicePixel(nWidth);
}

void Window::ShowFocus( const tools::Rectangle& rRect )
{
    if( mpWindowImpl->mbInShowFocus )
        return;
    mpWindowImpl->mbInShowFocus = true;

    ImplWinData* pWinData = ImplGetWinData();

    // native themeing suggest not to use focus rects
    if( ! ( mpWindowImpl->mbUseNativeFocus &&
            IsNativeWidgetEnabled() ) )
    {
        if ( !mpWindowImpl->mbInPaint )
        {
            if ( mpWindowImpl->mbFocusVisible )
            {
                if ( *pWinData->mpFocusRect == rRect )
                {
                    mpWindowImpl->mbInShowFocus = false;
                    return;
                }

                ImplInvertFocus( *pWinData->mpFocusRect );
            }

            ImplInvertFocus( rRect );
        }
        pWinData->mpFocusRect = rRect;
        mpWindowImpl->mbFocusVisible = true;
    }
    else
    {
        if( ! mpWindowImpl->mbNativeFocusVisible )
        {
            mpWindowImpl->mbNativeFocusVisible = true;
            if ( !mpWindowImpl->mbInPaint )
                Invalidate();
        }
    }
    mpWindowImpl->mbInShowFocus = false;
}

void Window::HideFocus()
{

    if( mpWindowImpl->mbInHideFocus )
        return;
    mpWindowImpl->mbInHideFocus = true;

    // native themeing can suggest not to use focus rects
    if( ! ( mpWindowImpl->mbUseNativeFocus &&
            IsNativeWidgetEnabled() ) )
    {
        if ( !mpWindowImpl->mbFocusVisible )
        {
            mpWindowImpl->mbInHideFocus = false;
            return;
        }

        if ( !mpWindowImpl->mbInPaint )
            ImplInvertFocus( *ImplGetWinData()->mpFocusRect );
        mpWindowImpl->mbFocusVisible = false;
    }
    else
    {
        if( mpWindowImpl->mbNativeFocusVisible )
        {
            mpWindowImpl->mbNativeFocusVisible = false;
            if ( !mpWindowImpl->mbInPaint )
                Invalidate();
        }
    }
    mpWindowImpl->mbInHideFocus = false;
}

void Window::ShowTracking( const tools::Rectangle& rRect, ShowTrackFlags nFlags )
{
    ImplWinData* pWinData = ImplGetWinData();

    if ( !mpWindowImpl->mbInPaint || !(nFlags & ShowTrackFlags::TrackWindow) )
    {
        if ( mpWindowImpl->mbTrackVisible )
        {
            if ( (*pWinData->mpTrackRect  == rRect) &&
                 (pWinData->mnTrackFlags    == nFlags) )
                return;

            InvertTracking( *pWinData->mpTrackRect, pWinData->mnTrackFlags );
        }

        InvertTracking( rRect, nFlags );
    }

    pWinData->mpTrackRect = rRect;
    pWinData->mnTrackFlags      = nFlags;
    mpWindowImpl->mbTrackVisible              = true;
}

void Window::HideTracking()
{
    if (!mpWindowImpl->mbTrackVisible)
        return;

    ImplWinData* pWinData = ImplGetWinData();
    if (!mpWindowImpl->mbInPaint || !(pWinData->mnTrackFlags & ShowTrackFlags::TrackWindow))
        InvertTracking(*pWinData->mpTrackRect, pWinData->mnTrackFlags);

    mpWindowImpl->mbTrackVisible = false;
}

void Window::InvertTracking( const tools::Rectangle& rRect, ShowTrackFlags nFlags )
{
    OutputDevice *pOutDev = GetOutDev();
    tools::Rectangle aRect(pOutDev->GetMapper().LogicToDevicePixel(rRect, pOutDev->GetMappingPolicy()));

    if ( aRect.IsEmpty() )
        return;
    aRect.Normalize();

    SalGraphics* pGraphics;

    if ( nFlags & ShowTrackFlags::TrackWindow )
    {
        if ( !GetOutDev()->IsDeviceOutputNecessary() )
            return;

        // we need a graphics
        if ( !GetOutDev()->mpGraphics )
        {
            if ( !pOutDev->AcquireGraphics() )
                return;
        }

        if ( !GetOutDev()->GetClipState().IsReady() )
            vcl::clipping::initDeviceClipRegion(*GetOutDev());

        if ( GetOutDev()->GetClipState().IsClippedOut() )
            return;

        pGraphics = GetOutDev()->mpGraphics;
    }
    else
    {
        pGraphics = ImplGetFrameGraphics();

        if ( nFlags & ShowTrackFlags::Clip )
        {
            vcl::Region aRegion( GetOutputRectPixel() );
            vcl::clipping::clipBoundaries(*this, aRegion, false, false);
            pOutDev->SelectClipRegion( aRegion, pGraphics );
        }
    }

    ShowTrackFlags nStyle = nFlags & ShowTrackFlags::StyleMask;
    if ( nStyle == ShowTrackFlags::Object )
        pGraphics->Invert( aRect.Left(), aRect.Top(), aRect.GetWidth(), aRect.GetHeight(), SalInvert::TrackFrame, *GetOutDev() );
    else if ( nStyle == ShowTrackFlags::Split )
        pGraphics->Invert( aRect.Left(), aRect.Top(), aRect.GetWidth(), aRect.GetHeight(), SalInvert::N50, *GetOutDev() );
    else
    {
        tools::Long nBorder = 1;
        if ( nStyle == ShowTrackFlags::Big )
            nBorder = 5;
        pGraphics->Invert( aRect.Left(), aRect.Top(), aRect.GetWidth(), nBorder, SalInvert::N50, *GetOutDev() );
        pGraphics->Invert( aRect.Left(), aRect.Bottom()-nBorder+1, aRect.GetWidth(), nBorder, SalInvert::N50, *GetOutDev() );
        pGraphics->Invert( aRect.Left(), aRect.Top()+nBorder, nBorder, aRect.GetHeight()-(nBorder*2), SalInvert::N50, *GetOutDev() );
        pGraphics->Invert( aRect.Right()-nBorder+1, aRect.Top()+nBorder, nBorder, aRect.GetHeight()-(nBorder*2), SalInvert::N50, *GetOutDev() );
    }
}

IMPL_LINK( Window, ImplTrackTimerHdl, Timer*, pTimer, void )
{
    if (!mpWindowImpl)
    {
        SAL_WARN("vcl", "ImplTrackTimerHdl has outlived dispose");
        return;
    }

    ImplSVData* pSVData = ImplGetSVData();

    // if Button-Repeat we have to change the timeout
    if ( pSVData->mpWinData->mnTrackFlags & StartTrackingFlags::ButtonRepeat )
        pTimer->SetTimeout( GetSettings().GetMouseSettings().GetButtonRepeat() );

    // create Tracking-Event
    Point           aMousePos( mpWindowImpl->mpFrameData->mnLastMouseX, mpWindowImpl->mpFrameData->mnLastMouseY );
    if( GetOutDev()->ImplIsAntiparallel() )
    {
        // re-mirror frame pos at pChild
        const OutputDevice *pOutDev = GetOutDev();
        pOutDev->ReMirror( aMousePos );
    }
    MouseEvent      aMEvt( ScreenToOutputPixel( aMousePos ),
                           mpWindowImpl->mpFrameData->mnClickCount, MouseEventModifiers::NONE,
                           mpWindowImpl->mpFrameData->mnMouseCode,
                           mpWindowImpl->mpFrameData->mnMouseCode );
    TrackingEvent   aTEvt( aMEvt, TrackingEventFlags::Repeat );
    Tracking( aTEvt );
}

void Window::SetUseFrameData(bool bUseFrameData)
{
    if (mpWindowImpl)
        mpWindowImpl->mbUseFrameData = bUseFrameData;
}

void Window::StartTracking( StartTrackingFlags nFlags )
{
    if (!mpWindowImpl)
        return;

    ImplSVData* pSVData = ImplGetSVData();
    VclPtr<vcl::Window> pTrackWin = mpWindowImpl->mbUseFrameData ?
        mpWindowImpl->mpFrameData->mpTrackWin :
        pSVData->mpWinData->mpTrackWin;

    if ( pTrackWin.get() != this )
    {
        if ( pTrackWin )
            pTrackWin->EndTracking( TrackingEventFlags::Cancel );
    }

    SAL_WARN_IF(pSVData->mpWinData->mpTrackTimer, "vcl", "StartTracking called while TrackerTimer still running");

    if ( !mpWindowImpl->mbUseFrameData &&
         (nFlags & (StartTrackingFlags::ScrollRepeat | StartTrackingFlags::ButtonRepeat)) )
    {
        pSVData->mpWinData->mpTrackTimer.reset(new AutoTimer("vcl::Window pSVData->mpWinData->mpTrackTimer"));

        if ( nFlags & StartTrackingFlags::ScrollRepeat )
            pSVData->mpWinData->mpTrackTimer->SetTimeout( MouseSettings::GetScrollRepeat() );
        else
            pSVData->mpWinData->mpTrackTimer->SetTimeout( MouseSettings::GetButtonStartRepeat() );
        pSVData->mpWinData->mpTrackTimer->SetInvokeHandler( LINK( this, Window, ImplTrackTimerHdl ) );
        pSVData->mpWinData->mpTrackTimer->Start();
    }

    if (mpWindowImpl->mbUseFrameData)
    {
        mpWindowImpl->mpFrameData->mpTrackWin = this;
    }
    else
    {
        pSVData->mpWinData->mpTrackWin   = this;
        pSVData->mpWinData->mnTrackFlags = nFlags;
        CaptureMouse();
    }
}

void Window::EndTracking( TrackingEventFlags nFlags )
{
    if (!mpWindowImpl)
        return;

    ImplSVData* pSVData = ImplGetSVData();
    VclPtr<vcl::Window> pTrackWin = mpWindowImpl->mbUseFrameData ?
        mpWindowImpl->mpFrameData->mpTrackWin :
        pSVData->mpWinData->mpTrackWin;

    if ( pTrackWin.get() != this )
        return;

    if ( !mpWindowImpl->mbUseFrameData && pSVData->mpWinData->mpTrackTimer )
        pSVData->mpWinData->mpTrackTimer.reset();

    mpWindowImpl->mpFrameData->mpTrackWin = pSVData->mpWinData->mpTrackWin = nullptr;
    pSVData->mpWinData->mnTrackFlags  = StartTrackingFlags::NONE;
    ReleaseMouse();

    // call EndTracking if required
    if (mpWindowImpl->mpFrameData)
    {
        Point           aMousePos( mpWindowImpl->mpFrameData->mnLastMouseX, mpWindowImpl->mpFrameData->mnLastMouseY );
        if( GetOutDev()->ImplIsAntiparallel() )
        {
            // re-mirror frame pos at pChild
            const OutputDevice *pOutDev = GetOutDev();
            pOutDev->ReMirror( aMousePos );
        }

        MouseEvent      aMEvt( ScreenToOutputPixel( aMousePos ),
                               mpWindowImpl->mpFrameData->mnClickCount, MouseEventModifiers::NONE,
                               mpWindowImpl->mpFrameData->mnMouseCode,
                               mpWindowImpl->mpFrameData->mnMouseCode );
        TrackingEvent   aTEvt( aMEvt, nFlags | TrackingEventFlags::End );
        // CompatTracking effectively
        if (!mpWindowImpl || mpWindowImpl->mbInDispose)
            return Window::Tracking( aTEvt );
        else
            return Tracking( aTEvt );
    }
}

bool Window::IsTracking() const
{
    if (!mpWindowImpl)
        return false;
    if (mpWindowImpl->mbUseFrameData && mpWindowImpl->mpFrameData)
    {
        return mpWindowImpl->mpFrameData->mpTrackWin == this;
    }
    if (!mpWindowImpl->mbUseFrameData && ImplGetSVData()->mpWinData)
    {
        return ImplGetSVData()->mpWinData->mpTrackWin == this;
    }
    return false;
}

void Window::StartAutoScroll( StartAutoScrollFlags nFlags )
{
    ImplSVData* pSVData = ImplGetSVData();

    if ( pSVData->mpWinData->mpAutoScrollWin.get() != this )
    {
        if ( pSVData->mpWinData->mpAutoScrollWin )
            pSVData->mpWinData->mpAutoScrollWin->EndAutoScroll();
    }

    pSVData->mpWinData->mpAutoScrollWin = this;
    pSVData->mpWinData->mnAutoScrollFlags = nFlags;
    pSVData->maAppData.mpWheelWindow = VclPtr<ImplWheelWindow>::Create( this );
}

void Window::EndAutoScroll()
{
    ImplSVData* pSVData = ImplGetSVData();

    if ( pSVData->mpWinData->mpAutoScrollWin.get() == this )
    {
        pSVData->mpWinData->mpAutoScrollWin = nullptr;
        pSVData->mpWinData->mnAutoScrollFlags = StartAutoScrollFlags::NONE;
        pSVData->maAppData.mpWheelWindow->ImplStop();
        pSVData->maAppData.mpWheelWindow.disposeAndClear();
    }
}

VclPtr<vcl::Window> Window::SaveFocus()
{
    ImplSVData* pSVData = ImplGetSVData();
    if ( pSVData->mpWinData->mpFocusWin )
    {
        return pSVData->mpWinData->mpFocusWin;
    }
    else
        return nullptr;
}

void Window::EndSaveFocus(const VclPtr<vcl::Window>& xFocusWin)
{
    if (xFocusWin && !xFocusWin->isDisposed())
    {
        xFocusWin->GrabFocus();
    }
}

void Window::SetZoom( double fZoom )
{
    if ( mpWindowImpl && mpWindowImpl->mfZoom != fZoom )
    {
        mpWindowImpl->mfZoom = fZoom;
        CompatStateChanged( StateChangedType::Zoom );
    }
}

void Window::SetZoomedPointFont(vcl::RenderContext& rRenderContext, const vcl::Font& rFont)
{
    double fZoom = GetZoom();
    if (fZoom != 1.0)
    {
        vcl::Font aFont(rFont);
        Size aSize = aFont.GetFontSize();
        aSize.setWidth(basegfx::fround<tools::Long>(aSize.Width() * fZoom));
        aSize.setHeight(basegfx::fround<tools::Long>(aSize.Height() * fZoom));
        aFont.SetFontSize(aSize);
        SetPointFont(rRenderContext, aFont);
    }
    else
    {
        SetPointFont(rRenderContext, rFont);
    }
}

tools::Long Window::CalcZoom( tools::Long nCalc ) const
{
    double fZoom = GetZoom();
    if ( fZoom != 1.0)
    {
        double n = nCalc * fZoom;
        nCalc = basegfx::fround<tools::Long>(n);
    }
    return nCalc;
}

void Window::SetControlFont()
{
    if (mpWindowImpl && mpWindowImpl->mpControlFont)
    {
        mpWindowImpl->mpControlFont.reset();
        CompatStateChanged(StateChangedType::ControlFont);
    }
}

void Window::SetControlFont(const vcl::Font& rFont)
{
    if (rFont == vcl::Font())
    {
        SetControlFont();
        return;
    }

    if (mpWindowImpl->mpControlFont)
    {
        if (*mpWindowImpl->mpControlFont == rFont)
            return;
        *mpWindowImpl->mpControlFont = rFont;
    }
    else
        mpWindowImpl->mpControlFont = rFont;

    CompatStateChanged(StateChangedType::ControlFont);
}

vcl::Font Window::GetControlFont() const
{
    if (mpWindowImpl->mpControlFont)
        return *mpWindowImpl->mpControlFont;

    vcl::Font aFont;
    return aFont;
}

void Window::ApplyControlFont(vcl::RenderContext& rRenderContext, const vcl::Font& rFont)
{
    vcl::Font aFont(rFont);
    if (IsControlFont())
        aFont.Merge(GetControlFont());
    SetZoomedPointFont(rRenderContext, aFont);
}

void Window::SetControlForeground()
{
    if (!mpWindowImpl->mbControlForeground)
        return;

    mpWindowImpl->maControlForeground = COL_TRANSPARENT;
    mpWindowImpl->mbControlForeground = false;
    CompatStateChanged(StateChangedType::ControlForeground);
}

void Window::SetControlForeground(const Color& rColor)
{
    if (rColor.IsTransparent())
    {
        if (mpWindowImpl->mbControlForeground)
        {
            mpWindowImpl->maControlForeground = COL_TRANSPARENT;
            mpWindowImpl->mbControlForeground = false;
            CompatStateChanged(StateChangedType::ControlForeground);
        }

        return;
    }

    if (mpWindowImpl->maControlForeground == rColor)
        return;

    mpWindowImpl->maControlForeground = rColor;
    mpWindowImpl->mbControlForeground = true;
    CompatStateChanged(StateChangedType::ControlForeground);
}

void Window::ApplyControlForeground(vcl::RenderContext& rRenderContext, const Color& rDefaultColor)
{
    Color aTextColor(rDefaultColor);
    if (IsControlForeground())
        aTextColor = GetControlForeground();
    rRenderContext.SetTextColor(aTextColor);
}

void Window::SetControlBackground()
{
    if (!mpWindowImpl->mbControlBackground)
        return;

    mpWindowImpl->maControlBackground = COL_TRANSPARENT;
    mpWindowImpl->mbControlBackground = false;
    CompatStateChanged(StateChangedType::ControlBackground);
}

void Window::SetControlBackground(const Color& rColor)
{
    if (rColor.IsTransparent())
    {
        if (mpWindowImpl->mbControlBackground)
        {
            mpWindowImpl->maControlBackground = COL_TRANSPARENT;
            mpWindowImpl->mbControlBackground = false;
            CompatStateChanged(StateChangedType::ControlBackground);
        }

        return;
    }

    if (mpWindowImpl->maControlBackground == rColor)
        return;

    mpWindowImpl->maControlBackground = rColor;
    mpWindowImpl->mbControlBackground = true;
    CompatStateChanged(StateChangedType::ControlBackground);
}

void Window::ApplyControlBackground(vcl::RenderContext& rRenderContext, const Color& rDefaultColor)
{
    Color aColor(rDefaultColor);
    if (IsControlBackground())
        aColor = GetControlBackground();
    rRenderContext.SetBackground(aColor);
}

Size Window::CalcWindowSize( const Size& rOutSz ) const
{
    Size aSz = rOutSz;
    aSz.AdjustWidth(mpWindowImpl->mnLeftBorder+mpWindowImpl->mnRightBorder );
    aSz.AdjustHeight(mpWindowImpl->mnTopBorder+mpWindowImpl->mnBottomBorder );
    return aSz;
}

Size Window::CalcOutputSize( const Size& rWinSz ) const
{
    Size aSz = rWinSz;
    aSz.AdjustWidth( -(mpWindowImpl->mnLeftBorder+mpWindowImpl->mnRightBorder) );
    aSz.AdjustHeight( -(mpWindowImpl->mnTopBorder+mpWindowImpl->mnBottomBorder) );
    return aSz;
}

vcl::Font Window::GetDrawPixelFont(OutputDevice const * pDev) const
{
    vcl::Font aFont = GetPointFont(*GetOutDev());
    MapMode aPtMapMode(MapUnit::MapPoint);
    const auto aFontSize = pDev->convertTo<vcl::WindowSize>(vcl::LogicSize(aFont.GetFontSize()), aPtMapMode);
    aFont.SetFontSize(aFontSize.get());
    return aFont;
}

tools::Long Window::GetDrawPixel(OutputDevice const * pDev, tools::Long nPixels) const
{
    tools::Long nP = nPixels;

    if (pDev->GetOutDevType() == OUTDEV_WINDOW)
        return nPixels;

    MapMode aMap(MapUnit::Map100thMM);
    auto aSz = convertTo<vcl::WindowSize>(vcl::LogicSize(Size(nP, 0)), aMap);

    return aSz->Width();
}

// returns how much was actually scrolled (so that abs(retval) <= abs(nN))
static double lcl_HandleScrollHelper( Scrollable* pScrl, double nN, bool isMultiplyByLineSize )
{
    if (!pScrl || !nN || pScrl->Inactive())
        return 0.0;

    tools::Long nNewPos = pScrl->GetThumbPos();
    double scrolled = nN;

    if ( nN == double(-LONG_MAX) )
        nNewPos += pScrl->GetPageSize();
    else if ( nN == double(LONG_MAX) )
        nNewPos -= pScrl->GetPageSize();
    else
    {
        // allowing both chunked and continuous scrolling
        if(isMultiplyByLineSize){
            nN*=pScrl->GetLineSize();
        }

        // compute how many quantized units to scroll
        tools::Long magnitude = o3tl::saturating_cast<tools::Long>(fabs(nN));
        tools::Long change = copysign(magnitude, nN);

        nNewPos = nNewPos - change;

        scrolled = double(change);
        // convert back to chunked/continuous
        if(isMultiplyByLineSize){
            scrolled /= pScrl->GetLineSize();
        }
    }

    pScrl->DoScroll( nNewPos );

    return scrolled;
}

bool Window::HandleScrollCommand( const CommandEvent& rCmd,
                                  Scrollable* pHScrl, Scrollable* pVScrl )
{
    if (!pHScrl && !pVScrl)
        return false;

    bool bRet = false;

    switch( rCmd.GetCommand() )
    {
        case CommandEventId::StartAutoScroll:
        {
            StartAutoScrollFlags nFlags = StartAutoScrollFlags::NONE;
            if ( pHScrl )
            {
                if ( (pHScrl->GetVisibleSize() < pHScrl->GetRangeMax()) &&
                     !pHScrl->Inactive() )
                    nFlags |= StartAutoScrollFlags::Horz;
            }
            if ( pVScrl )
            {
                if ( (pVScrl->GetVisibleSize() < pVScrl->GetRangeMax()) &&
                     !pVScrl->Inactive() )
                    nFlags |= StartAutoScrollFlags::Vert;
            }

            if ( nFlags != StartAutoScrollFlags::NONE )
            {
                StartAutoScroll( nFlags );
                bRet = true;
            }
        }
        break;

        case CommandEventId::Wheel:
        {
            const CommandWheelData* pData = rCmd.GetWheelData();

            if ( pData && (CommandWheelMode::SCROLL == pData->GetMode()) )
            {
                if (!pData->IsDeltaPixel())
                {
                    double nScrollLines = pData->GetScrollLines();
                    double nLines;
                    double* partialScroll = pData->IsHorz()
                        ? &mpWindowImpl->mfPartialScrollX
                        : &mpWindowImpl->mfPartialScrollY;
                    if ( nScrollLines == COMMAND_WHEEL_PAGESCROLL )
                    {
                        if ( pData->GetDelta() < 0 )
                            nLines = double(-LONG_MAX);
                        else
                            nLines = double(LONG_MAX);
                    }
                    else
                        nLines = *partialScroll + pData->GetNotchDelta() * nScrollLines;
                    if ( nLines )
                    {
                        Scrollable* pScrl = pData->IsHorz() ? pHScrl : pVScrl;
                        double scrolled = lcl_HandleScrollHelper( pScrl, nLines, true );
                        *partialScroll = nLines - scrolled;
                        bRet = true;
                    }
                }
                else
                {
                    // Mobile / touch scrolling section
                    const Point & deltaPoint = rCmd.GetMousePosPixel();

                    double deltaXInPixels = double(deltaPoint.X());
                    double deltaYInPixels = double(deltaPoint.Y());
                    Size winSize = GetOutputSizePixel();

                    if(pHScrl)
                    {
                        double visSizeX = double(pHScrl->GetVisibleSize());
                        double ratioX = deltaXInPixels / double(winSize.getWidth());
                        tools::Long deltaXInLogic = tools::Long(visSizeX * ratioX);
                        // Touch need to work by pixels. Did not apply this to
                        // Android, as android code may require adaptations
                        // to work with this scrolling code
#ifndef IOS
                        tools::Long lineSizeX = pHScrl->GetLineSize();

                        if(lineSizeX)
                        {
                            deltaXInLogic /= lineSizeX;
                        }
                        else
                        {
                            deltaXInLogic = 0;
                        }
#endif
                        if ( deltaXInLogic)
                        {
#ifndef IOS
                            bool const isMultiplyByLineSize = true;
#else
                            bool const isMultiplyByLineSize = false;
#endif
                            lcl_HandleScrollHelper( pHScrl, deltaXInLogic, isMultiplyByLineSize );
                            bRet = true;
                        }
                    }
                    if(pVScrl)
                    {
                        double visSizeY = double(pVScrl->GetVisibleSize());
                        double ratioY = deltaYInPixels / double(winSize.getHeight());
                        tools::Long deltaYInLogic = tools::Long(visSizeY * ratioY);

                        // Touch need to work by pixels. Did not apply this to
                        // Android, as android code may require adaptations
                        // to work with this scrolling code
#ifndef IOS
                        tools::Long lineSizeY = pVScrl->GetLineSize();
                        if(lineSizeY)
                        {
                            deltaYInLogic /= lineSizeY;
                        }
                        else
                        {
                            deltaYInLogic = 0;
                        }
#endif
                        if ( deltaYInLogic )
                        {
#ifndef IOS
                            bool const isMultiplyByLineSize = true;
#else
                            bool const isMultiplyByLineSize = false;
#endif
                            lcl_HandleScrollHelper( pVScrl, deltaYInLogic, isMultiplyByLineSize );

                            bRet = true;
                        }
                    }
                }
            }
        }
        break;

        case CommandEventId::GesturePan:
        {
            const CommandGesturePanData* pData = rCmd.GetGesturePanData();
            if (pData)
            {
                if (pData->meEventType == GestureEventPanType::Begin)
                {
                    if (pHScrl)
                        mpWindowImpl->mpFrameData->mnTouchPanPositionX = pHScrl->GetThumbPos();
                    if (pVScrl)
                        mpWindowImpl->mpFrameData->mnTouchPanPositionY = pVScrl->GetThumbPos();
                }
                else if (pData->meEventType == GestureEventPanType::Update)
                {
                    bool bHorz = pData->meOrientation == PanningOrientation::Horizontal;
                    Scrollable* pScrl = bHorz ? pHScrl : pVScrl;
                    if (pScrl)
                    {
                        Point aGesturePt(pData->mfX, pData->mfY);
                        tools::Rectangle aWinRect(this->GetOutputRectPixel());
                        bool bContains = aWinRect.Contains(aGesturePt);
                        if (bContains)
                        {
                            double nWinSize;
                            tools::Long nOriginalPos;
                            if (bHorz)
                            {
                                nWinSize = GetOutputSizePixel().getWidth();
                                nOriginalPos = mpWindowImpl->mpFrameData->mnTouchPanPositionX;
                            }
                            else
                            {
                                nWinSize = GetOutputSizePixel().getHeight();
                                nOriginalPos = mpWindowImpl->mpFrameData->mnTouchPanPositionY;
                            }
                            double nOffset = pData->mfOffset;
                            double nRatio = nOffset / nWinSize;
                            tools::Long nVisibleSize = pScrl->GetVisibleSize();
                            tools::Long nDeltaInLogic = tools::Long(nVisibleSize * nRatio);
                            tools::Long nNewPos = nOriginalPos - nDeltaInLogic;

                            pScrl->DoScroll(nNewPos);
                        }
                    }
                }
                else if (pData->meEventType == GestureEventPanType::End)
                {
                    mpWindowImpl->mpFrameData->mnTouchPanPositionX = -1;
                    mpWindowImpl->mpFrameData->mnTouchPanPositionY = -1;
                }
                bRet = true;
            }
            break;
        }

        case CommandEventId::AutoScroll:
        {
            const CommandScrollData* pData = rCmd.GetAutoScrollData();
            if ( pData && (pData->GetDeltaX() || pData->GetDeltaY()) )
            {
                ImplHandleScroll( pHScrl, pData->GetDeltaX(),
                                  pVScrl, pData->GetDeltaY() );
                bRet = true;
            }
        }
        break;

        default:
        break;
    }

    return bRet;
}

void Window::ImplHandleScroll( Scrollable* pHScrl, double nX,
                               Scrollable* pVScrl, double nY )
{
    lcl_HandleScrollHelper( pHScrl, nX, true );
    lcl_HandleScrollHelper( pVScrl, nY, true );
}

DockingManager* Window::GetDockingManager()
{
    return ImplGetDockingManager();
}

void Window::EnableDocking( bool bEnable )
{
    // update list of dockable windows
    if( bEnable )
        ImplGetDockingManager()->AddWindow( this );
    else
        ImplGetDockingManager()->RemoveWindow( this );
}

// retrieves the list of owner draw decorated windows for this window hierarchy
::std::vector<VclPtr<vcl::Window> >& Window::ImplGetOwnerDrawList()
{
    return ImplGetTopmostFrameWindow()->mpWindowImpl->mpFrameData->maOwnerDrawList;
}

void Window::SetHelpId( const OUString& rHelpId )
{
    mpWindowImpl->maHelpId = rHelpId;
}

const OUString& Window::GetHelpId() const
{
    return mpWindowImpl->maHelpId;
}

// --------- old inline methods ---------------

vcl::Window* Window::ImplGetWindow() const
{
    if ( mpWindowImpl->mpClientWindow )
        return mpWindowImpl->mpClientWindow;
    else
        return const_cast<vcl::Window*>(this);
}

ImplFrameData* Window::ImplGetFrameData()
{
    return mpWindowImpl ? mpWindowImpl->mpFrameData : nullptr;
}

SalFrame* Window::ImplGetFrame() const
{
    return mpWindowImpl ? mpWindowImpl->mpFrame : nullptr;
}

weld::Window* Window::GetFrameWeld() const
{
    SalFrame* pFrame = ImplGetFrame();
    return pFrame ? pFrame->GetFrameWeld() : nullptr;
}

vcl::Window* Window::GetFrameWindow() const
{
    SalFrame* pFrame = ImplGetFrame();
    return pFrame ? pFrame->GetWindow() : nullptr;
}

vcl::Window* Window::ImplGetParent() const
{
    return mpWindowImpl ? mpWindowImpl->mpHierarchy->mpParent.get() : nullptr;
}

vcl::Window* Window::ImplGetClientWindow() const
{
    return mpWindowImpl ? mpWindowImpl->mpClientWindow.get() : nullptr;
}

vcl::Window* Window::ImplGetBorderWindow() const
{
    return mpWindowImpl ? mpWindowImpl->mpBorderWindow.get() : nullptr;
}

vcl::Window* Window::ImplGetFirstOverlapWindow()
{
    if (!mpWindowImpl)
        return nullptr;

    if (mpWindowImpl->mbOverlapWin)
        return this;

    return mpWindowImpl->mpOverlapWindow;
}

const vcl::Window* Window::ImplGetFirstOverlapWindow() const
{
    if (!mpWindowImpl)
        return nullptr;

    if (mpWindowImpl->mbOverlapWin)
        return this;

    return mpWindowImpl->mpOverlapWindow;
}

vcl::Window* Window::ImplGetFrameWindow() const
{
    return mpWindowImpl ? mpWindowImpl->mpFrameWindow.get() : nullptr;
}

bool Window::IsDockingWindow() const
{
    return mpWindowImpl && mpWindowImpl->mbDockWin;
}

bool Window::ImplIsFloatingWindow() const
{
    return mpWindowImpl && mpWindowImpl->mbFloatWin;
}

bool Window::ImplIsSplitter() const
{
    return mpWindowImpl && mpWindowImpl->mbSplitter;
}

bool Window::ImplIsPushButton() const
{
    return mpWindowImpl && mpWindowImpl->mbPushButton;
}

bool Window::ImplIsOverlapWindow() const
{
    return mpWindowImpl && mpWindowImpl->mbOverlapWin;
}

void Window::ImplSetMouseTransparent( bool bTransparent )
{
    if (mpWindowImpl)
        mpWindowImpl->mbMouseTransparent = bTransparent;
}

void Window::SetCompoundControl( bool bCompound )
{
    if (mpWindowImpl)
        mpWindowImpl->mbCompoundControl = bCompound;
}

WinBits Window::GetStyle() const
{
    return mpWindowImpl ? mpWindowImpl->mnStyle : 0;
}

WinBits Window::GetPrevStyle() const
{
    return mpWindowImpl ? mpWindowImpl->mnPrevStyle : 0;
}

WindowExtendedStyle Window::GetExtendedStyle() const
{
    return mpWindowImpl ? mpWindowImpl->mnExtendedStyle : WindowExtendedStyle::NONE;
}

void Window::SetType( WindowType eType )
{
    if (mpWindowImpl)
        mpWindowImpl->meType = eType;
}

WindowType Window::GetType() const
{
    if (mpWindowImpl)
        return mpWindowImpl->meType;

    return WindowType::NONE;
}

bool Window::IsFormControl() const
{
    return mpWindowImpl ? mpWindowImpl->mbIsFormControl : false;
}

void Window::SetFormControl(bool bFormControl)
{
    if (mpWindowImpl)
        mpWindowImpl->mbIsFormControl = bFormControl;
}

Dialog* Window::GetParentDialog() const
{
    const vcl::Window *pWindow = this;

    while( pWindow )
    {
        if( pWindow->IsDialog() )
            break;

        pWindow = pWindow->GetParent();
    }

    return const_cast<Dialog *>(dynamic_cast<const Dialog*>(pWindow));
}

bool Window::IsSystemWindow() const
{
    return mpWindowImpl && mpWindowImpl->mbSysWin;
}

bool Window::IsDialog() const
{
    return mpWindowImpl && mpWindowImpl->mbDialog;
}

bool Window::IsMenuFloatingWindow() const
{
    return mpWindowImpl && mpWindowImpl->mbMenuFloatingWindow;
}

bool Window::IsNativeFrame() const
{
    if (!mpWindowImpl->mbFrame)
        return false;

    // #101741 do not check for WB_CLOSEABLE because undecorated floaters (like menus!) are closeable
    if (mpWindowImpl->mnStyle & (WB_MOVEABLE | WB_SIZEABLE))
        return true;

    return false;
}

void Window::EnableAllResize()
{
    mpWindowImpl->mbAllResize = true;
}

void Window::EnableChildTransparentMode( bool bEnable )
{
    mpWindowImpl->mbChildTransparent = bEnable;
}

bool Window::IsChildTransparentModeEnabled() const
{
    return mpWindowImpl && mpWindowImpl->mbChildTransparent;
}

bool Window::IsMouseTransparent() const
{
    return mpWindowImpl && mpWindowImpl->mbMouseTransparent;
}

bool Window::IsPaintTransparent() const
{
    return mpWindowImpl && mpWindowImpl->mbPaintTransparent;
}

void Window::SetDialogControlStart( bool bStart )
{
    mpWindowImpl->mbDlgCtrlStart = bStart;
}

bool Window::IsDialogControlStart() const
{
    return mpWindowImpl && mpWindowImpl->mbDlgCtrlStart;
}

void Window::SetDialogControlFlags( DialogControlFlags nFlags )
{
    mpWindowImpl->mnDlgCtrlFlags = nFlags;
}

DialogControlFlags Window::GetDialogControlFlags() const
{
    return mpWindowImpl->mnDlgCtrlFlags;
}

const InputContext& Window::GetInputContext() const
{
    return mpWindowImpl->maInputContext;
}

bool Window::IsControlFont() const
{
    return bool(mpWindowImpl->mpControlFont);
}

const Color& Window::GetControlForeground() const
{
    return mpWindowImpl->maControlForeground;
}

bool Window::IsControlForeground() const
{
    return mpWindowImpl->mbControlForeground;
}

const Color& Window::GetControlBackground() const
{
    return mpWindowImpl->maControlBackground;
}

bool Window::IsControlBackground() const
{
    return mpWindowImpl->mbControlBackground;
}

bool Window::IsInPaint() const
{
    return mpWindowImpl && mpWindowImpl->mbInPaint;
}

vcl::Window* Window::GetParent() const
{
    return mpWindowImpl ? mpWindowImpl->mpHierarchy->mpRealParent.get() : nullptr;
}

bool Window::IsVisible() const
{
    return mpWindowImpl && mpWindowImpl->mbVisible;
}

bool Window::IsReallyVisible() const
{
    return mpWindowImpl && mpWindowImpl->mbReallyVisible;
}

bool Window::IsReallyShown() const
{
    return mpWindowImpl && mpWindowImpl->mbReallyShown;
}

bool Window::IsInInitShow() const
{
    return mpWindowImpl->mbInInitShow;
}

bool Window::IsEnabled() const
{
    return mpWindowImpl && !mpWindowImpl->mbDisabled;
}

bool Window::IsInputEnabled() const
{
    return mpWindowImpl && !mpWindowImpl->mbInputDisabled;
}

bool Window::IsAlwaysEnableInput() const
{
    return mpWindowImpl->meAlwaysInputMode == AlwaysInputEnabled;
}

ActivateModeFlags Window::GetActivateMode() const
{
    return mpWindowImpl->mnActivateMode;

}

bool Window::IsAlwaysOnTopEnabled() const
{
    return mpWindowImpl->mbAlwaysOnTop;
}

bool Window::IsDefaultPos() const
{
    return mpWindowImpl->mbDefPos;
}

bool Window::IsDefaultSize() const
{
    return mpWindowImpl->mbDefSize;
}

Point Window::GetOffsetPixelFrom(const vcl::Window& rWindow) const
{
    return Point(GetDeviceOriginX() - rWindow.GetDeviceOriginX(), GetDeviceOriginY() - rWindow.GetDeviceOriginY());
}

void Window::EnablePaint( bool bEnable )
{
    mpWindowImpl->mbPaintDisabled = !bEnable;
}

bool Window::IsPaintEnabled() const
{
    return !mpWindowImpl->mbPaintDisabled;
}

bool Window::IsUpdateMode() const
{
    return !mpWindowImpl->mbNoUpdate;
}

void Window::SetParentUpdateMode( bool bUpdate )
{
    mpWindowImpl->mbNoParentUpdate = !bUpdate;
}

bool Window::IsActive() const
{
    return mpWindowImpl->mbActive;
}

GetFocusFlags Window::GetGetFocusFlags() const
{
    return mpWindowImpl->mnGetFocusFlags;
}

bool Window::IsCompoundControl() const
{
    return mpWindowImpl && mpWindowImpl->mbCompoundControl;
}

bool Window::IsWait() const
{
    return (mpWindowImpl->mnWaitCount != 0);
}

vcl::Cursor* Window::GetCursor() const
{
    if (!mpWindowImpl)
        return nullptr;
    return mpWindowImpl->mpCursor;
}

double Window::GetZoom() const
{
    return mpWindowImpl->mfZoom;
}

bool Window::IsZoom() const
{
    return mpWindowImpl->mfZoom != 1.0;
}

void Window::SetHelpText( const OUString& rHelpText )
{
    mpWindowImpl->maHelpText = rHelpText;
    mpWindowImpl->mbHelpTextDynamic = true;
}

void Window::SetQuickHelpText( const OUString& rHelpText )
{
    if (mpWindowImpl)
        mpWindowImpl->maQuickHelpText = rHelpText;
}

const OUString& Window::GetQuickHelpText() const
{
    return mpWindowImpl->maQuickHelpText;
}

bool Window::IsCreatedWithToolkit() const
{
    return mpWindowImpl->mbCreatedWithToolkit;
}

void Window::SetCreatedWithToolkit( bool b )
{
    mpWindowImpl->mbCreatedWithToolkit = b;
}

PointerStyle Window::GetPointer() const
{
    return mpWindowImpl->maPointer;
}

VCLXWindow* Window::GetWindowPeer() const
{
    return mpWindowImpl ? mpWindowImpl->mpVCLXWindow : nullptr;
}

void Window::SetPosPixel( const Point& rNewPos )
{
    setPosSizePixel( rNewPos.X(), rNewPos.Y(), 0, 0, PosSizeFlags::Pos );
}

void Window::SetSizePixel( const Size& rNewSize )
{
    setPosSizePixel( 0, 0, rNewSize.Width(), rNewSize.Height(),
                     PosSizeFlags::Size );
}

void Window::SetPosSizePixel( const Point& rNewPos, const Size& rNewSize )
{
    setPosSizePixel( rNewPos.X(), rNewPos.Y(),
                     rNewSize.Width(), rNewSize.Height());
}

void Window::SetOutputSizePixel( const Size& rNewSize )
{
    SetSizePixel( Size( rNewSize.Width()+mpWindowImpl->mnLeftBorder+mpWindowImpl->mnRightBorder,
                        rNewSize.Height()+mpWindowImpl->mnTopBorder+mpWindowImpl->mnBottomBorder ) );
}

//When a widget wants to renegotiate layout, get toplevel parent dialog and call
//resize on it. Mark all intermediate containers (or container-alike) widgets
//as dirty for the size remains unchanged, but layout changed circumstances
namespace
{
    bool queue_ungrouped_resize(vcl::Window const *pOrigWindow)
    {
        bool bSomeoneCares = false;

        vcl::Window *pWindow = pOrigWindow->GetParent();
        if (pWindow)
        {
            if (isContainerWindow(*pWindow))
            {
                bSomeoneCares = true;
            }
            else if (pWindow->GetType() == WindowType::TABCONTROL)
            {
                bSomeoneCares = true;
            }
            pWindow->queue_resize();
        }

        return bSomeoneCares;
    }
}

void Window::InvalidateSizeCache()
{
    WindowImpl *pWindowImpl = mpWindowImpl->mpBorderWindow ? mpWindowImpl->mpBorderWindow->mpWindowImpl.get() : mpWindowImpl.get();
    pWindowImpl->mnOptimalWidthCache = -1;
    pWindowImpl->mnOptimalHeightCache = -1;
}

static bool HasParentDockingWindow(const vcl::Window* pWindow)
{
    while( pWindow )
    {
        if( pWindow->IsDockingWindow() )
            return true;

        pWindow = pWindow->GetParent();
    }

    return false;
}

void Window::queue_resize(StateChangedType eReason)
{
    if (isDisposed())
        return;

    bool bSomeoneCares = queue_ungrouped_resize(this);

    if (eReason != StateChangedType::Visible)
    {
        InvalidateSizeCache();
    }

    WindowImpl *pWindowImpl = mpWindowImpl->mpBorderWindow ? mpWindowImpl->mpBorderWindow->mpWindowImpl.get() : mpWindowImpl.get();
    if (pWindowImpl->m_xSizeGroup && pWindowImpl->m_xSizeGroup->get_mode() != VclSizeGroupMode::NONE)
    {
        std::set<VclPtr<vcl::Window> > &rWindows = pWindowImpl->m_xSizeGroup->get_widgets();
        for (VclPtr<vcl::Window> const & pOther : rWindows)
        {
            if (pOther == this)
                continue;
            queue_ungrouped_resize(pOther);
        }
    }

    if (bSomeoneCares && !isDisposed())
    {
        //fdo#57090 force a resync of the borders of the borderwindow onto this
        //window in case they have changed
        vcl::Window* pBorderWindow = ImplGetBorderWindow();
        if (pBorderWindow)
            pBorderWindow->Resize();
    }
    if (VclPtr<vcl::Window> pParent = GetParentWithLOKNotifier())
    {
        Size aSize = GetSizePixel();
        if (!aSize.IsEmpty() && !pParent->IsInInitShow()
            && (GetParentDialog() || HasParentDockingWindow(this)))
            LogicInvalidate(nullptr);
    }
}

namespace
{
    VclAlign toAlign(std::u16string_view rValue)
    {
        VclAlign eRet = VclAlign::Fill;

        if (rValue == u"fill")
            eRet = VclAlign::Fill;
        else if (rValue == u"start")
            eRet = VclAlign::Start;
        else if (rValue == u"end")
            eRet = VclAlign::End;
        else if (rValue == u"center")
            eRet = VclAlign::Center;
        return eRet;
    }
}

bool Window::set_font_attribute(const OUString &rKey, std::u16string_view rValue)
{
    if (rKey == "weight")
    {
        vcl::Font aFont(GetControlFont());
        if (rValue == u"thin")
            aFont.SetWeight(WEIGHT_THIN);
        else if (rValue == u"ultralight")
            aFont.SetWeight(WEIGHT_ULTRALIGHT);
        else if (rValue == u"light")
            aFont.SetWeight(WEIGHT_LIGHT);
        else if (rValue == u"book")
            aFont.SetWeight(WEIGHT_SEMILIGHT);
        else if (rValue == u"normal")
            aFont.SetWeight(WEIGHT_NORMAL);
        else if (rValue == u"medium")
            aFont.SetWeight(WEIGHT_MEDIUM);
        else if (rValue == u"semibold")
            aFont.SetWeight(WEIGHT_SEMIBOLD);
        else if (rValue == u"bold")
            aFont.SetWeight(WEIGHT_BOLD);
        else if (rValue == u"ultrabold")
            aFont.SetWeight(WEIGHT_ULTRABOLD);
        else
            aFont.SetWeight(WEIGHT_BLACK);
        SetControlFont(aFont);
    }
    else if (rKey == "style")
    {
        vcl::Font aFont(GetControlFont());
        if (rValue == u"normal")
            aFont.SetItalic(ITALIC_NONE);
        else if (rValue == u"oblique")
            aFont.SetItalic(ITALIC_OBLIQUE);
        else if (rValue == u"italic")
            aFont.SetItalic(ITALIC_NORMAL);
        SetControlFont(aFont);
    }
    else if (rKey == "underline")
    {
        vcl::Font aFont(GetControlFont());
        aFont.SetUnderline(toBool(rValue) ? LINESTYLE_SINGLE : LINESTYLE_NONE);
        SetControlFont(aFont);
    }
    else if (rKey == "scale")
    {
        // if no control font was set yet, take the underlying font from the device
        vcl::Font aFont(IsControlFont() ? GetControlFont() : GetPointFont(*GetOutDev()));
        aFont.SetFontHeight(aFont.GetFontHeight() * o3tl::toDouble(rValue));
        SetControlFont(aFont);
    }
    else if (rKey == "size")
    {
        vcl::Font aFont(GetControlFont());
        sal_Int32 nHeight = o3tl::toInt32(rValue) / 1000;
        aFont.SetFontHeight(nHeight);
        SetControlFont(aFont);
    }
    else
    {
        SAL_INFO("vcl.layout", "unhandled font attribute: " << rKey);
        return false;
    }
    return true;
}

bool Window::set_property(const OUString &rKey, const OUString &rValue)
{
    if ((rKey == "label") || (rKey == "title") || (rKey == "text") )
    {
        SetText(BuilderUtils::convertMnemonicMarkup(rValue));
    }
    else if (rKey == "visible")
        Show(toBool(rValue));
    else if (rKey == "sensitive")
        Enable(toBool(rValue));
    else if (rKey == "resizable")
    {
        WinBits nBits = GetStyle();
        nBits &= ~WB_SIZEABLE;
        if (toBool(rValue))
            nBits |= WB_SIZEABLE;
        SetStyle(nBits);
    }
    else if (rKey == "xalign")
    {
        WinBits nBits = GetStyle();
        nBits &= ~(WB_LEFT | WB_CENTER | WB_RIGHT);

        float f = rValue.toFloat();
        assert(f == 0.0 || f == 1.0 || f == 0.5);
        if (f == 0.0)
            nBits |= WB_LEFT;
        else if (f == 1.0)
            nBits |= WB_RIGHT;
        else if (f == 0.5)
            nBits |= WB_CENTER;

        SetStyle(nBits);
    }
    else if (rKey == "justification")
    {
        WinBits nBits = GetStyle();
        nBits &= ~(WB_LEFT | WB_CENTER | WB_RIGHT);

        if (rValue == "left")
            nBits |= WB_LEFT;
        else if (rValue == "right")
            nBits |= WB_RIGHT;
        else if (rValue == "center")
            nBits |= WB_CENTER;

        SetStyle(nBits);
    }
    else if (rKey == "yalign")
    {
        WinBits nBits = GetStyle();
        nBits &= ~(WB_TOP | WB_VCENTER | WB_BOTTOM);

        float f = rValue.toFloat();
        assert(f == 0.0 || f == 1.0 || f == 0.5);
        if (f == 0.0)
            nBits |= WB_TOP;
        else if (f == 1.0)
            nBits |= WB_BOTTOM;
        else if (f == 0.5)
            nBits |= WB_VCENTER;

        SetStyle(nBits);
    }
    else if (rKey == "wrap")
    {
        WinBits nBits = GetStyle();
        nBits &= ~WB_WORDBREAK;
        if (toBool(rValue))
            nBits |= WB_WORDBREAK;
        SetStyle(nBits);
    }
    else if (rKey == "height-request")
        set_height_request(rValue.toInt32());
    else if (rKey == "width-request")
        set_width_request(rValue.toInt32());
    else if (rKey == "hexpand")
        set_hexpand(toBool(rValue));
    else if (rKey == "vexpand")
        set_vexpand(toBool(rValue));
    else if (rKey == "halign")
        set_halign(toAlign(rValue));
    else if (rKey == "valign")
        set_valign(toAlign(rValue));
    else if (rKey == "tooltip-markup")
        SetQuickHelpText(rValue);
    else if (rKey == "tooltip-text")
        SetQuickHelpText(rValue);
    else if (rKey == "border-width")
        set_border_width(rValue.toInt32());
    else if (rKey == "margin-start" || rKey == "margin-left")
    {
        assert(rKey == "margin-start" && "margin-left deprecated in favor of margin-start");
        set_margin_start(rValue.toInt32());
    }
    else if (rKey == "margin-end" || rKey == "margin-right")
    {
        assert(rKey == "margin-end" && "margin-right deprecated in favor of margin-end");
        set_margin_end(rValue.toInt32());
    }
    else if (rKey == "margin-top")
        set_margin_top(rValue.toInt32());
    else if (rKey == "margin-bottom")
        set_margin_bottom(rValue.toInt32());
    else if (rKey == "hscrollbar-policy")
    {
        WinBits nBits = GetStyle();
        nBits &= ~(WB_AUTOHSCROLL|WB_HSCROLL);
        if (rValue == "always")
            nBits |= WB_HSCROLL;
        else if (rValue == "automatic")
            nBits |= WB_AUTOHSCROLL;
        SetStyle(nBits);
    }
    else if (rKey == "vscrollbar-policy")
    {
        WinBits nBits = GetStyle();
        nBits &= ~(WB_AUTOVSCROLL|WB_VSCROLL);
        if (rValue == "always")
            nBits |= WB_VSCROLL;
        else if (rValue == "automatic")
            nBits |= WB_AUTOVSCROLL;
        SetStyle(nBits);
    }
    else if (rKey == "accessible-name")
    {
        SetAccessibleName(rValue);
    }
    else if (rKey == "accessible-description")
    {
        SetAccessibleDescription(rValue);
    }
    else if (rKey == "accessible-role")
    {
        sal_Int16 role = BuilderUtils::getRoleFromName(rValue);
        if (role != css::accessibility::AccessibleRole::UNKNOWN)
            SetAccessibleRole(role);
    }
    else if (rKey == "use-markup")
    {
        //https://live.gnome.org/GnomeGoals/RemoveMarkupInMessages
        SAL_WARN_IF(toBool(rValue), "vcl.layout", "Use pango attributes instead of mark-up");
    }
    else if (rKey == "has-focus")
    {
        if (toBool(rValue))
            GrabFocus();
    }
    else if (rKey == "can-focus")
    {
        WinBits nBits = GetStyle();
        nBits &= ~(WB_TABSTOP|WB_NOTABSTOP);
        if (toBool(rValue))
            nBits |= WB_TABSTOP;
        else
            nBits |= WB_NOTABSTOP;
        SetStyle(nBits);
    }
    else
    {
        SAL_INFO("vcl.layout", "unhandled property: " << rKey);
        return false;
    }
    return true;
}

void Window::set_height_request(sal_Int32 nHeightRequest)
{
    if (!mpWindowImpl)
        return;

    WindowImpl *pWindowImpl = mpWindowImpl->mpBorderWindow ? mpWindowImpl->mpBorderWindow->mpWindowImpl.get() : mpWindowImpl.get();

    if (pWindowImpl->mnHeightRequest == nHeightRequest)
        return;

    pWindowImpl->mnHeightRequest = nHeightRequest;
    queue_resize();
}

void Window::set_width_request(sal_Int32 nWidthRequest)
{
    if (!mpWindowImpl)
        return;

    WindowImpl *pWindowImpl = mpWindowImpl->mpBorderWindow ? mpWindowImpl->mpBorderWindow->mpWindowImpl.get() : mpWindowImpl.get();

    if (pWindowImpl->mnWidthRequest == nWidthRequest)
        return;

    pWindowImpl->mnWidthRequest = nWidthRequest;
    queue_resize();
}

Size Window::get_ungrouped_preferred_size() const
{
    Size aRet(get_width_request(), get_height_request());

    if (aRet.Width() != -1 && aRet.Height() != -1)
        return aRet;

    // cache gets blown away by queue_resize
    WindowImpl *pWindowImpl = mpWindowImpl->mpBorderWindow ? mpWindowImpl->mpBorderWindow->mpWindowImpl.get() : mpWindowImpl.get();
    if (pWindowImpl->mnOptimalWidthCache == -1 || pWindowImpl->mnOptimalHeightCache == -1)
    {
        Size aOptimal(GetOptimalSize());
        pWindowImpl->mnOptimalWidthCache = aOptimal.Width();
        pWindowImpl->mnOptimalHeightCache = aOptimal.Height();
    }

    if (aRet.Width() == -1)
        aRet.setWidth( pWindowImpl->mnOptimalWidthCache );

    if (aRet.Height() == -1)
        aRet.setHeight( pWindowImpl->mnOptimalHeightCache );

    return aRet;
}

Size Window::get_preferred_size() const
{
    Size aRet(get_ungrouped_preferred_size());

    WindowImpl *pWindowImpl = mpWindowImpl->mpBorderWindow ? mpWindowImpl->mpBorderWindow->mpWindowImpl.get() : mpWindowImpl.get();
    if (!pWindowImpl->m_xSizeGroup)
        return aRet;

    const VclSizeGroupMode eMode = pWindowImpl->m_xSizeGroup->get_mode();
    if (eMode == VclSizeGroupMode::NONE)
        return aRet;

    const bool bIgnoreInHidden = pWindowImpl->m_xSizeGroup->get_ignore_hidden();
    const std::set<VclPtr<vcl::Window> > &rWindows = pWindowImpl->m_xSizeGroup->get_widgets();

    for (const vcl::Window* pOther : rWindows)
    {
        if (pOther == this)
            continue;

        if (bIgnoreInHidden && !pOther->IsVisible())
            continue;

        Size aOtherSize = pOther->get_ungrouped_preferred_size();

        if (eMode == VclSizeGroupMode::Both || eMode == VclSizeGroupMode::Horizontal)
            aRet.setWidth( std::max(aRet.Width(), aOtherSize.Width()) );

        if (eMode == VclSizeGroupMode::Both || eMode == VclSizeGroupMode::Vertical)
            aRet.setHeight( std::max(aRet.Height(), aOtherSize.Height()) );
    }

    return aRet;
}

VclAlign Window::get_halign() const
{
    WindowImpl *pWindowImpl = mpWindowImpl->mpBorderWindow ? mpWindowImpl->mpBorderWindow->mpWindowImpl.get() : mpWindowImpl.get();
    return pWindowImpl->meHalign;
}

void Window::set_halign(VclAlign eAlign)
{
    WindowImpl *pWindowImpl = mpWindowImpl->mpBorderWindow ? mpWindowImpl->mpBorderWindow->mpWindowImpl.get() : mpWindowImpl.get();
    pWindowImpl->meHalign = eAlign;
}

VclAlign Window::get_valign() const
{
    WindowImpl *pWindowImpl = mpWindowImpl->mpBorderWindow ? mpWindowImpl->mpBorderWindow->mpWindowImpl.get() : mpWindowImpl.get();
    return pWindowImpl->meValign;
}

void Window::set_valign(VclAlign eAlign)
{
    WindowImpl *pWindowImpl = mpWindowImpl->mpBorderWindow ? mpWindowImpl->mpBorderWindow->mpWindowImpl.get() : mpWindowImpl.get();
    pWindowImpl->meValign = eAlign;
}

bool Window::get_hexpand() const
{
    WindowImpl *pWindowImpl = mpWindowImpl->mpBorderWindow ? mpWindowImpl->mpBorderWindow->mpWindowImpl.get() : mpWindowImpl.get();
    return pWindowImpl->mbHexpand;
}

void Window::set_hexpand(bool bExpand)
{
    WindowImpl *pWindowImpl = mpWindowImpl->mpBorderWindow ? mpWindowImpl->mpBorderWindow->mpWindowImpl.get() : mpWindowImpl.get();
    pWindowImpl->mbHexpand = bExpand;
}

bool Window::get_vexpand() const
{
    WindowImpl *pWindowImpl = mpWindowImpl->mpBorderWindow ? mpWindowImpl->mpBorderWindow->mpWindowImpl.get() : mpWindowImpl.get();
    return pWindowImpl->mbVexpand;
}

void Window::set_vexpand(bool bExpand)
{
    WindowImpl *pWindowImpl = mpWindowImpl->mpBorderWindow ? mpWindowImpl->mpBorderWindow->mpWindowImpl.get() : mpWindowImpl.get();
    pWindowImpl->mbVexpand = bExpand;
}

bool Window::get_expand() const
{
    WindowImpl *pWindowImpl = mpWindowImpl->mpBorderWindow ? mpWindowImpl->mpBorderWindow->mpWindowImpl.get() : mpWindowImpl.get();
    return pWindowImpl->mbExpand;
}

void Window::set_expand(bool bExpand)
{
    WindowImpl *pWindowImpl = mpWindowImpl->mpBorderWindow ? mpWindowImpl->mpBorderWindow->mpWindowImpl.get() : mpWindowImpl.get();
    pWindowImpl->mbExpand = bExpand;
}

VclPackType Window::get_pack_type() const
{
    WindowImpl *pWindowImpl = mpWindowImpl->mpBorderWindow ? mpWindowImpl->mpBorderWindow->mpWindowImpl.get() : mpWindowImpl.get();
    return pWindowImpl->mePackType;
}

void Window::set_pack_type(VclPackType ePackType)
{
    WindowImpl *pWindowImpl = mpWindowImpl->mpBorderWindow ? mpWindowImpl->mpBorderWindow->mpWindowImpl.get() : mpWindowImpl.get();
    pWindowImpl->mePackType = ePackType;
}

sal_Int32 Window::get_padding() const
{
    WindowImpl *pWindowImpl = mpWindowImpl->mpBorderWindow ? mpWindowImpl->mpBorderWindow->mpWindowImpl.get() : mpWindowImpl.get();
    return pWindowImpl->mnPadding;
}

void Window::set_padding(sal_Int32 nPadding)
{
    WindowImpl *pWindowImpl = mpWindowImpl->mpBorderWindow ? mpWindowImpl->mpBorderWindow->mpWindowImpl.get() : mpWindowImpl.get();
    pWindowImpl->mnPadding = nPadding;
}

bool Window::get_fill() const
{
    WindowImpl *pWindowImpl = mpWindowImpl->mpBorderWindow ? mpWindowImpl->mpBorderWindow->mpWindowImpl.get() : mpWindowImpl.get();
    return pWindowImpl->mbFill;
}

void Window::set_fill(bool bFill)
{
    WindowImpl *pWindowImpl = mpWindowImpl->mpBorderWindow ? mpWindowImpl->mpBorderWindow->mpWindowImpl.get() : mpWindowImpl.get();
    pWindowImpl->mbFill = bFill;
}

sal_Int32 Window::get_grid_width() const
{
    WindowImpl *pWindowImpl = mpWindowImpl->mpBorderWindow ? mpWindowImpl->mpBorderWindow->mpWindowImpl.get() : mpWindowImpl.get();
    return pWindowImpl->mnGridWidth;
}

void Window::set_grid_width(sal_Int32 nCols)
{
    WindowImpl *pWindowImpl = mpWindowImpl->mpBorderWindow ? mpWindowImpl->mpBorderWindow->mpWindowImpl.get() : mpWindowImpl.get();
    pWindowImpl->mnGridWidth = nCols;
}

sal_Int32 Window::get_grid_left_attach() const
{
    WindowImpl *pWindowImpl = mpWindowImpl->mpBorderWindow ? mpWindowImpl->mpBorderWindow->mpWindowImpl.get() : mpWindowImpl.get();
    return pWindowImpl->mnGridLeftAttach;
}

void Window::set_grid_left_attach(sal_Int32 nAttach)
{
    WindowImpl *pWindowImpl = mpWindowImpl->mpBorderWindow ? mpWindowImpl->mpBorderWindow->mpWindowImpl.get() : mpWindowImpl.get();
    pWindowImpl->mnGridLeftAttach = nAttach;
}

sal_Int32 Window::get_grid_height() const
{
    WindowImpl *pWindowImpl = mpWindowImpl->mpBorderWindow ? mpWindowImpl->mpBorderWindow->mpWindowImpl.get() : mpWindowImpl.get();
    return pWindowImpl->mnGridHeight;
}

void Window::set_grid_height(sal_Int32 nRows)
{
    WindowImpl *pWindowImpl = mpWindowImpl->mpBorderWindow ? mpWindowImpl->mpBorderWindow->mpWindowImpl.get() : mpWindowImpl.get();
    pWindowImpl->mnGridHeight = nRows;
}

sal_Int32 Window::get_grid_top_attach() const
{
    WindowImpl *pWindowImpl = mpWindowImpl->mpBorderWindow ? mpWindowImpl->mpBorderWindow->mpWindowImpl.get() : mpWindowImpl.get();
    return pWindowImpl->mnGridTopAttach;
}

void Window::set_grid_top_attach(sal_Int32 nAttach)
{
    WindowImpl *pWindowImpl = mpWindowImpl->mpBorderWindow ? mpWindowImpl->mpBorderWindow->mpWindowImpl.get() : mpWindowImpl.get();
    pWindowImpl->mnGridTopAttach = nAttach;
}

void Window::set_border_width(sal_Int32 nBorderWidth)
{
    WindowImpl *pWindowImpl = mpWindowImpl->mpBorderWindow ? mpWindowImpl->mpBorderWindow->mpWindowImpl.get() : mpWindowImpl.get();
    pWindowImpl->mnBorderWidth = nBorderWidth;
}

sal_Int32 Window::get_border_width() const
{
    WindowImpl *pWindowImpl = mpWindowImpl->mpBorderWindow ? mpWindowImpl->mpBorderWindow->mpWindowImpl.get() : mpWindowImpl.get();
    return pWindowImpl->mnBorderWidth;
}

void Window::set_margin_start(sal_Int32 nWidth)
{
    WindowImpl *pWindowImpl = mpWindowImpl->mpBorderWindow ? mpWindowImpl->mpBorderWindow->mpWindowImpl.get() : mpWindowImpl.get();
    if (pWindowImpl->mnMarginLeft != nWidth)
    {
        pWindowImpl->mnMarginLeft = nWidth;
        queue_resize();
    }
}

sal_Int32 Window::get_margin_start() const
{
    WindowImpl *pWindowImpl = mpWindowImpl->mpBorderWindow ? mpWindowImpl->mpBorderWindow->mpWindowImpl.get() : mpWindowImpl.get();
    return pWindowImpl->mnMarginLeft;
}

void Window::set_margin_end(sal_Int32 nWidth)
{
    WindowImpl *pWindowImpl = mpWindowImpl->mpBorderWindow ? mpWindowImpl->mpBorderWindow->mpWindowImpl.get() : mpWindowImpl.get();
    if (pWindowImpl->mnMarginRight != nWidth)
    {
        pWindowImpl->mnMarginRight = nWidth;
        queue_resize();
    }
}

sal_Int32 Window::get_margin_end() const
{
    WindowImpl *pWindowImpl = mpWindowImpl->mpBorderWindow ? mpWindowImpl->mpBorderWindow->mpWindowImpl.get() : mpWindowImpl.get();
    return pWindowImpl->mnMarginRight;
}

void Window::set_margin_top(sal_Int32 nWidth)
{
    WindowImpl *pWindowImpl = mpWindowImpl->mpBorderWindow ? mpWindowImpl->mpBorderWindow->mpWindowImpl.get() : mpWindowImpl.get();
    if (pWindowImpl->mnMarginTop != nWidth)
    {
        pWindowImpl->mnMarginTop = nWidth;
        queue_resize();
    }
}

sal_Int32 Window::get_margin_top() const
{
    WindowImpl *pWindowImpl = mpWindowImpl->mpBorderWindow ? mpWindowImpl->mpBorderWindow->mpWindowImpl.get() : mpWindowImpl.get();
    return pWindowImpl->mnMarginTop;
}

void Window::set_margin_bottom(sal_Int32 nWidth)
{
    WindowImpl *pWindowImpl = mpWindowImpl->mpBorderWindow ? mpWindowImpl->mpBorderWindow->mpWindowImpl.get() : mpWindowImpl.get();
    if (pWindowImpl->mnMarginBottom != nWidth)
    {
        pWindowImpl->mnMarginBottom = nWidth;
        queue_resize();
    }
}

sal_Int32 Window::get_margin_bottom() const
{
    WindowImpl *pWindowImpl = mpWindowImpl->mpBorderWindow ? mpWindowImpl->mpBorderWindow->mpWindowImpl.get() : mpWindowImpl.get();
    return pWindowImpl->mnMarginBottom;
}

sal_Int32 Window::get_height_request() const
{
    WindowImpl *pWindowImpl = mpWindowImpl->mpBorderWindow ? mpWindowImpl->mpBorderWindow->mpWindowImpl.get() : mpWindowImpl.get();
    return pWindowImpl->mnHeightRequest;
}

sal_Int32 Window::get_width_request() const
{
    WindowImpl *pWindowImpl = mpWindowImpl->mpBorderWindow ? mpWindowImpl->mpBorderWindow->mpWindowImpl.get() : mpWindowImpl.get();
    return pWindowImpl->mnWidthRequest;
}

bool Window::get_secondary() const
{
    WindowImpl *pWindowImpl = mpWindowImpl->mpBorderWindow ? mpWindowImpl->mpBorderWindow->mpWindowImpl.get() : mpWindowImpl.get();
    return pWindowImpl->mbSecondary;
}

void Window::set_secondary(bool bSecondary)
{
    WindowImpl *pWindowImpl = mpWindowImpl->mpBorderWindow ? mpWindowImpl->mpBorderWindow->mpWindowImpl.get() : mpWindowImpl.get();
    pWindowImpl->mbSecondary = bSecondary;
}

bool Window::get_non_homogeneous() const
{
    WindowImpl *pWindowImpl = mpWindowImpl->mpBorderWindow ? mpWindowImpl->mpBorderWindow->mpWindowImpl.get() : mpWindowImpl.get();
    return pWindowImpl->mbNonHomogeneous;
}

void Window::set_non_homogeneous(bool bNonHomogeneous)
{
    WindowImpl *pWindowImpl = mpWindowImpl->mpBorderWindow ? mpWindowImpl->mpBorderWindow->mpWindowImpl.get() : mpWindowImpl.get();
    pWindowImpl->mbNonHomogeneous = bNonHomogeneous;
}

void Window::add_to_size_group(const std::shared_ptr<VclSizeGroup>& xGroup)
{
    WindowImpl *pWindowImpl = mpWindowImpl->mpBorderWindow ? mpWindowImpl->mpBorderWindow->mpWindowImpl.get() : mpWindowImpl.get();
    //To-Do, multiple groups
    pWindowImpl->m_xSizeGroup = xGroup;
    pWindowImpl->m_xSizeGroup->insert(this);
    if (VclSizeGroupMode::NONE != pWindowImpl->m_xSizeGroup->get_mode())
        queue_resize();
}

void Window::remove_from_all_size_groups()
{
    WindowImpl *pWindowImpl = mpWindowImpl->mpBorderWindow ? mpWindowImpl->mpBorderWindow->mpWindowImpl.get() : mpWindowImpl.get();
    //To-Do, multiple groups
    if (pWindowImpl->m_xSizeGroup)
    {
        if (VclSizeGroupMode::NONE != pWindowImpl->m_xSizeGroup->get_mode())
            queue_resize();
        pWindowImpl->m_xSizeGroup->erase(this);
        pWindowImpl->m_xSizeGroup.reset();
    }
}

void Window::add_mnemonic_label(FixedText *pLabel)
{
    std::vector<VclPtr<FixedText> >& v = mpWindowImpl->m_aMnemonicLabels;
    if (std::find(v.begin(), v.end(), VclPtr<FixedText>(pLabel)) != v.end())
        return;
    v.emplace_back(pLabel);
    pLabel->set_mnemonic_widget(this);
}

void Window::remove_mnemonic_label(FixedText *pLabel)
{
    std::vector<VclPtr<FixedText> >& v = mpWindowImpl->m_aMnemonicLabels;
    auto aFind = std::find(v.begin(), v.end(), VclPtr<FixedText>(pLabel));
    if (aFind == v.end())
        return;
    v.erase(aFind);
    pLabel->set_mnemonic_widget(nullptr);
}

const std::vector<VclPtr<FixedText> >& Window::list_mnemonic_labels() const
{
    return mpWindowImpl->m_aMnemonicLabels;
}

} /* namespace vcl */

void InvertFocusRect(vcl::RenderContext& rRenderContext, const tools::Rectangle& rRect)
{
    const int nBorder = 1;
    rRenderContext.Invert(tools::Rectangle(Point(rRect.Left(), rRect.Top()), Size(rRect.GetWidth(), nBorder)), InvertFlags::N50);
    rRenderContext.Invert(tools::Rectangle(Point(rRect.Left(), rRect.Bottom()-nBorder+1), Size(rRect.GetWidth(), nBorder)), InvertFlags::N50);
    rRenderContext.Invert(tools::Rectangle(Point(rRect.Left(), rRect.Top()+nBorder), Size(nBorder, rRect.GetHeight()-(nBorder*2))), InvertFlags::N50);
    rRenderContext.Invert(tools::Rectangle(Point(rRect.Right()-nBorder+1, rRect.Top()+nBorder), Size(nBorder, rRect.GetHeight()-(nBorder*2))), InvertFlags::N50);
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
