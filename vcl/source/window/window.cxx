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
                const Size aOutSize = mpWindowImpl->mpFrame->GetClientSize();
                ImplHandleResize(this, aOutSize.Width(), aOutSize.Height());
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

void Window::SetUpdateMode( bool bUpdate )
{
    if (mpWindowImpl)
    {
        mpWindowImpl->mbNoUpdate = !bUpdate;
        CompatStateChanged( StateChangedType::UpdateMode );
    }
}

void Window::SetCursor( vcl::Cursor* pCursor )
{

    if ( mpWindowImpl->mpCursor != pCursor )
    {
        if ( mpWindowImpl->mpCursor )
            mpWindowImpl->mpCursor->ImplHide();
        mpWindowImpl->mpCursor = pCursor;
        if ( pCursor )
            pCursor->ImplShow();
    }
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

} /* namespace vcl */

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
