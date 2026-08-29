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
#include <clipping_window.hxx>
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

namespace vcl {

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

void Window::SetModalHierarchyHdl(const Link<bool, void>& rLink)
{
    ImplGetFrame()->SetModalHierarchyHdl(rLink);
}

KeyIndicatorState Window::GetIndicatorState() const
{
    return mpWindowImpl->mpFrame->GetIndicatorState();
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

void Window::ImplUpdateModalCount(int nDelta)
{
    vcl::Window* pFrameWindow = mpWindowImpl->mpFrameWindow;
    vcl::Window* pParent = pFrameWindow;
    while( pFrameWindow )
    {
        pFrameWindow->mpWindowImpl->mpFrameData->mnModalMode += nDelta;
        while( pParent && pParent->mpWindowImpl->mpFrameWindow == pFrameWindow )
        {
            pParent = pParent->GetParent();
        }
        pFrameWindow = pParent ? pParent->mpWindowImpl->mpFrameWindow.get() : nullptr;
    }
}

void Window::IncModalCount()
{
    ImplUpdateModalCount(1);
}

void Window::DecModalCount()
{
    ImplUpdateModalCount(-1);
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

} /* namespace vcl */

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
