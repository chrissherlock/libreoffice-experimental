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

void Window::ImplDeInitDND()
{
    // shutdown drag and drop listener container
    if (mpWindowImpl->mxDNDListenerContainer.is())
        mpWindowImpl->mxDNDListenerContainer->dispose();

    if (!mpWindowImpl->mbFrame || !mpWindowImpl->mpFrameData)
        return;

    try
    {
        // deregister drop target listener
        if (mpWindowImpl->mpFrameData->mxDropTargetListener.is())
        {
            Reference<XDragGestureRecognizer> xDragGestureRecognizer(mpWindowImpl->mpFrameData->mxDragSource, UNO_QUERY);
            if (xDragGestureRecognizer.is())
            {
                xDragGestureRecognizer->removeDragGestureListener(mpWindowImpl->mpFrameData->mxDropTargetListener);
            }

            mpWindowImpl->mpFrameData->mxDropTarget->removeDropTargetListener(mpWindowImpl->mpFrameData->mxDropTargetListener);
            mpWindowImpl->mpFrameData->mxDropTargetListener.clear();
        }

        // shutdown drag and drop for this frame window
        Reference<XComponent> xComponent(mpWindowImpl->mpFrameData->mxDropTarget, UNO_QUERY);

        // DNDEventDispatcher does not hold a reference of the DropTarget,
        // so it's ok if it does not support XComponent
        if (xComponent.is())
            xComponent->dispose();
    }
    catch (const Exception&)
    {
        // can be safely ignored here.
    }
}

void Window::ImplDeInitAccessibility()
{
    UnoWrapperBase* pWrapper = UnoWrapperBase::GetUnoWrapper(false);
    if (pWrapper)
        pWrapper->WindowDestroyed(this);

    if (mpWindowImpl->mpAccessible.is())
    {
        mpWindowImpl->mpAccessible->dispose();
        mpWindowImpl->mpAccessible.clear();
    }

    if (mpWindowImpl->mpAccessibleInfos)
        mpWindowImpl->mpAccessibleInfos->pAccessibleParent.clear();
}

void Window::ImplRemoveFromTaskPaneList()
{
    vcl::Window* pMyParent = GetParent();
    SystemWindow* pMySysWin = nullptr;

    while (pMyParent)
    {
        if (pMyParent->IsSystemWindow())
            pMySysWin = dynamic_cast<SystemWindow *>(pMyParent);

        pMyParent = pMyParent->GetParent();
    }

    if (pMySysWin && pMySysWin->ImplIsInTaskPaneList(this))
        pMySysWin->GetTaskPaneList()->RemoveWindow(this);
    else
        SAL_WARN("vcl", "Window (" << GetText() << ") not found in TaskPanelList");
}

void Window::ImplRemoveOwnerDrawDecoratedFrame()
{
    if (!(GetStyle() & WB_OWNERDRAWDECORATION) || !mpWindowImpl->mbFrame)
        return;

    auto& rList = ImplGetOwnerDrawList();
    auto p = std::find(rList.begin(), rList.end(), VclPtr<vcl::Window>(this));
    if (p != rList.end())
        rList.erase(p);
}

bool Window::ImplHasFocusedChild() const
{
    ImplSVData* pSVData = ImplGetSVData();
    if (!pSVData->mpWinData->mpFocusWin || !IsAncestorOf(*pSVData->mpWinData->mpFocusWin))
        return false;

    // #122232#, this must not happen and is an application bug ! but we try some cleanup to hopefully avoid crashes, see below
#if OSL_DEBUG_LEVEL > 0
    OUString aTempStr = "Window (" + GetText() +
            ") with focused child window destroyed ! THIS WILL LEAD TO CRASHES AND MUST BE FIXED !";
    SAL_WARN("vcl", aTempStr);
    Application::Abort(aTempStr);
#endif

    return true;
}

bool Window::ImplContainsFocus() const
{
    ImplSVData* pSVData = ImplGetSVData();
    return (pSVData->mpWinData->mpFocusWin == this) || ImplHasFocusedChild();
}

vcl::Window* Window::ImplTransferFocus()
{
    vcl::Window* pOverlapWindow = ImplGetFirstOverlapWindow();

    if (!ImplContainsFocus())
        return pOverlapWindow;

    ImplSVData* pSVData = ImplGetSVData();

    if (mpWindowImpl->mbFrame)
    {
        pSVData->mpWinData->mpFocusWin = nullptr;
        pOverlapWindow->mpWindowImpl->mpLastFocusWindow = nullptr;

        return pOverlapWindow;
    }

    vcl::Window* pParent = GetParent();
    vcl::Window* pBorderWindow = mpWindowImpl->mpBorderWindow;

    // when windows overlap, give focus to the parent
    // of the next FrameWindow
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

    return pOverlapWindow;
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

WinBits Window::ImplApplyBorderAnd3DStyle(WinBits nStyle, const vcl::Window* pParent) const
{
    // Inherit 3D look from parent if applicable
    if (!mpWindowImpl->mbOverlapWin && pParent && (pParent->GetStyle() & WB_3DLOOK))
        nStyle |= WB_3DLOOK;

    // A system child window acts as a top-level frame and strictly requires a border
    if (!mpWindowImpl->mbFrame && !mpWindowImpl->mbBorderWin && !mpWindowImpl->mpBorderWindow
        && (nStyle & WB_SYSTEMCHILDWINDOW))
    {
        nStyle |= WB_BORDER;
    }

    return nStyle;
}

bool Window::ImplNeedsSystemChildBorder(WinBits nStyle) const
{
    return !mpWindowImpl->mbFrame
        && !mpWindowImpl->mbBorderWin
        && !mpWindowImpl->mpBorderWindow
        && (nStyle & WB_SYSTEMCHILDWINDOW);
}

BorderWindowStyle Window::ImplGetBorderWindowStyle(WinBits nStyle) const
{
    if (!ImplNeedsSystemChildBorder(nStyle))
        return BorderWindowStyle::NONE;

    // handle WB_SYSTEMCHILDWINDOW
    // these should be analogous to a top level frame; meaning they
    // should have a border window with style BorderWindowStyle::Frame
    // which controls their size
    return BorderWindowStyle::Frame;
}

vcl::Window* Window::ImplInitBorderWindow(vcl::Window* pParent, WinBits nStyle, BorderWindowStyle nBorderTypeStyle)
{
    // create border window if necessary
    if (!mpWindowImpl->mbFrame && !mpWindowImpl->mbBorderWin && !mpWindowImpl->mpBorderWindow
        && (nStyle & WB_BORDER))
    {
        VclPtrInstance<ImplBorderWindow> pBorderWin(pParent, nStyle & (WB_BORDER | WB_DIALOGCONTROL | WB_NODIALOGCONTROL), nBorderTypeStyle);
        static_cast<vcl::Window*>(pBorderWin)->mpWindowImpl->mpClientWindow = this;
        pBorderWin->GetBorder(mpWindowImpl->mnLeftBorder, mpWindowImpl->mnTopBorder, mpWindowImpl->mnRightBorder, mpWindowImpl->mnBottomBorder);
        mpWindowImpl->mpBorderWindow = pBorderWin;

        // Return the newly created border window to act as the new parent
        return mpWindowImpl->mpBorderWindow;
    }

    // fallback for frameless windows with no parent
    if (!mpWindowImpl->mbFrame && !pParent)
    {
        mpWindowImpl->mbOverlapWin = true;
        mpWindowImpl->mbFrame = true;
    }

    return pParent;
}

SalFrameStyleFlags Window::ImplGetFrameStyle(WinBits nStyle) const
{
    SalFrameStyleFlags nFrameStyle = SalFrameStyleFlags::NONE;

    if (nStyle & WB_MOVEABLE)
        nFrameStyle |= SalFrameStyleFlags::MOVEABLE;
    if (nStyle & WB_SIZEABLE)
        nFrameStyle |= SalFrameStyleFlags::SIZEABLE;
    if (nStyle & WB_CLOSEABLE)
        nFrameStyle |= SalFrameStyleFlags::CLOSEABLE;
    if (nStyle & WB_APP)
        nFrameStyle |= SalFrameStyleFlags::DEFAULT;

    // check for undecorated floating window
    if ((!(nFrameStyle & ~SalFrameStyleFlags::CLOSEABLE) &&
        (mpWindowImpl->mbFloatWin || ((GetType() == WindowType::BORDERWINDOW) && static_cast<const ImplBorderWindow*>(this)->mbFloatWindow) || (nStyle & WB_SYSTEMFLOATWIN))) ||
        ((GetType() == WindowType::BORDERWINDOW) && static_cast<const ImplBorderWindow*>(this)->mbFloatWindow && (nStyle & WB_OWNERDRAWDECORATION)))
    {
        nFrameStyle = SalFrameStyleFlags::FLOAT;
        if (nStyle & WB_OWNERDRAWDECORATION)
            nFrameStyle |= SalFrameStyleFlags::OWNERDRAWDECORATION | SalFrameStyleFlags::NOSHADOW;
    }
    else if (mpWindowImpl->mbFloatWin)
        nFrameStyle |= SalFrameStyleFlags::TOOLWINDOW;

    if (nStyle & WB_INTROWIN)
        nFrameStyle |= SalFrameStyleFlags::INTRO;
    if (nStyle & WB_TOOLTIPWIN)
        nFrameStyle |= SalFrameStyleFlags::TOOLTIP;

    if (nStyle & WB_NOSHADOW)
        nFrameStyle |= SalFrameStyleFlags::NOSHADOW;

    if (nStyle & WB_SYSTEMCHILDWINDOW)
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
    if (nStyle & WB_DEFAULTWIN)
        nFrameStyle |= SalFrameStyleFlags::NOICON;

    return nFrameStyle;
}

SalFrame* Window::ImplCreateFrame(vcl::Window* pParent, SystemParentData* pSystemParentData, SalFrameStyleFlags nFrameStyle)
{
    ImplSVData* pSVData = ImplGetSVData();

    SalFrame* pParentFrame = nullptr;
    if (pParent)
        pParentFrame = pParent->mpWindowImpl->mpFrame;

    SalFrame* pFrame;
    if (pSystemParentData)
        pFrame = pSVData->mpDefInst->CreateChildFrame(pSystemParentData, nFrameStyle | SalFrameStyleFlags::PLUG);
    else
        pFrame = pSVData->mpDefInst->CreateFrame(pParentFrame, nFrameStyle);

    if (!pFrame)
    {
        // do not abort but throw an exception, may be the current thread terminates anyway (plugin-scenario)
        throw RuntimeException(
            u"Could not create system window!"_ustr,
            Reference<XInterface>());
    }

    pFrame->SetCallback(this, ImplWindowFrameProc);
    return pFrame;
}

void Window::ImplSetupFrame(SalFrame* pFrame, WinBits nStyle, vcl::Window* pInitialParent)
{
    // set window frame data
    mpWindowImpl->mpFrameData     = new ImplFrameData(this);
    mpWindowImpl->mpFrame         = pFrame;
    mpWindowImpl->mpFrameWindow   = this;
    mpWindowImpl->mpOverlapWindow = this;

    auto shouldDoubleBuffer = [nStyle, this]() {
        return !(nStyle & WB_DEFAULTWIN) && mpWindowImpl->mbDoubleBufferingRequested;
    };

    if (shouldDoubleBuffer())
        RequestDoubleBuffering(true);

    if (pInitialParent && IsTopWindow())
    {
        ImplWinData* pParentWinData = pInitialParent->ImplGetWinData();
        pParentWinData->maTopWindowChildren.emplace_back(this);
    }
}

void Window::ImplInitFrameResolution(vcl::Window* pParent, WinBits nStyle)
{
    if (pParent)
    {
        mpWindowImpl->mpFrameData->mnDPIX = pParent->mpWindowImpl->mpFrameData->mnDPIX;
        mpWindowImpl->mpFrameData->mnDPIY = pParent->mpWindowImpl->mpFrameData->mnDPIY;
    }
    else if (auto* pGraphics = GetOutDev()->GetGraphics())
    {
        pGraphics->GetResolution(mpWindowImpl->mpFrameData->mnDPIX,
                                 mpWindowImpl->mpFrameData->mnDPIY);
    }

    // If we create a Window with default size, query this
    // size directly, because we want resize all Controls to
    // the correct size before we display the window
    if (nStyle & (WB_MOVEABLE | WB_SIZEABLE | WB_APP))
    {
        const Size aSize = mpWindowImpl->mpFrame->GetClientSize();

        mpWindowImpl->mxOutDev->SetOutputWidthPixel(aSize.Width());
        mpWindowImpl->mxOutDev->SetOutputHeightPixel(aSize.Height());
    }
}

void Window::ImplInitFromParentState(vcl::Window* pParent)
{
    if (!pParent)
        return;

    if (!ImplIsOverlapWindow())
    {
        mpWindowImpl->mbDisabled      = pParent->mpWindowImpl->mbDisabled;
        mpWindowImpl->mbInputDisabled = pParent->mpWindowImpl->mbInputDisabled;
        mpWindowImpl->meAlwaysInputMode = pParent->mpWindowImpl->meAlwaysInputMode;
    }

    if (!comphelper::IsFuzzing())
    {
        // we don't want to call the WindowOutputDevice override of this because
        // it calls back into us.
        mpWindowImpl->mxOutDev->OutputDevice::SetSettings(pParent->GetSettings());
    }
}

void Window::ImplInitResolution(vcl::Window* pParent, WinBits nStyle)
{
    if (mpWindowImpl->mbFrame)
        ImplInitFrameResolution(pParent, nStyle);
    else
        ImplInitFromParentState(pParent);
}

static bool lcl_ShouldInitAppSettings(const ImplSVData* pSVData, WinBits nStyle)
{
    return !pSVData->maAppData.mbSettingsInit &&
           !(nStyle & (WB_INTROWIN | WB_DEFAULTWIN));
}

void Window::ImplInitSettings(WinBits nStyle)
{
    if (!mpWindowImpl->mbFrame)
        return;

    // add ownerdraw decorated frame windows to list in the top-most frame window
    // so they can be hidden on lose focus
    if (nStyle & WB_OWNERDRAWDECORATION)
        ImplGetOwnerDrawList().emplace_back(this);

    ImplSVData* pSVData = ImplGetSVData();

    if (!lcl_ShouldInitAppSettings(pSVData, nStyle))
        return;

    // side effect: ImplUpdateGlobalSettings does an ImplGetFrame()->UpdateSettings
    ImplUpdateGlobalSettings(*pSVData->maAppData.mxSettings);
    mpWindowImpl->mxOutDev->SetSettings(*pSVData->maAppData.mxSettings);
    pSVData->maAppData.mbSettingsInit = true;
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
    else
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
    if (xText.is())
    {
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

    return false;
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

} /* namespace vcl */

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
