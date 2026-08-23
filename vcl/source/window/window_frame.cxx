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

#include <comphelper/configuration.hxx>

#include <vcl/window.hxx>
#include <vcl/wintypes.hxx>

#include <window.h>
#include <brdwin.hxx>
#include <salframe.hxx>
#include <salgdi.hxx>
#include <salinst.hxx>
#include <svdata.hxx>

#include <com/sun/star/uno/Reference.hxx>
#include <com/sun/star/uno/RuntimeException.hpp>

namespace vcl
{
bool Window::ImplShouldInherit3DLook(const vcl::Window* pParent) const
{
    return !mpWindowImpl->mbOverlapWin && pParent && (pParent->GetStyle() & WB_3DLOOK);
}

WinBits Window::ImplApplyBorderAnd3DStyle(WinBits nStyle, const vcl::Window* pParent) const
{
    if (ImplShouldInherit3DLook(pParent))
        nStyle |= WB_3DLOOK;

    if (ImplNeedsSystemChildBorder(nStyle))
        nStyle |= WB_BORDER;

    return nStyle;
}

bool Window::ImplNeedsSystemChildBorder(WinBits nStyle) const
{
    return !mpWindowImpl->mbFrame && !mpWindowImpl->mbBorderWin && !mpWindowImpl->mpBorderWindow
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

vcl::Window* Window::ImplInitBorderWindow(vcl::Window* pParent, WinBits nStyle,
                                          BorderWindowStyle nBorderTypeStyle)
{
    // create border window if necessary
    if (!mpWindowImpl->mbFrame && !mpWindowImpl->mbBorderWin && !mpWindowImpl->mpBorderWindow
        && (nStyle & WB_BORDER))
    {
        VclPtrInstance<ImplBorderWindow> pBorderWin(
            pParent, nStyle & (WB_BORDER | WB_DIALOGCONTROL | WB_NODIALOGCONTROL),
            nBorderTypeStyle);
        static_cast<vcl::Window*>(pBorderWin)->mpWindowImpl->mpClientWindow = this;
        pBorderWin->GetBorder(mpWindowImpl->mnLeftBorder, mpWindowImpl->mnTopBorder,
                              mpWindowImpl->mnRightBorder, mpWindowImpl->mnBottomBorder);
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
    if ((!(nFrameStyle & ~SalFrameStyleFlags::CLOSEABLE)
         && (mpWindowImpl->mbFloatWin
             || ((GetType() == WindowType::BORDERWINDOW)
                 && static_cast<const ImplBorderWindow*>(this)->mbFloatWindow)
             || (nStyle & WB_SYSTEMFLOATWIN)))
        || ((GetType() == WindowType::BORDERWINDOW)
            && static_cast<const ImplBorderWindow*>(this)->mbFloatWindow
            && (nStyle & WB_OWNERDRAWDECORATION)))
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

SalFrame* Window::ImplCreateFrame(vcl::Window* pParent, SystemParentData* pSystemParentData,
                                  SalFrameStyleFlags nFrameStyle)
{
    ImplSVData* pSVData = ImplGetSVData();

    SalFrame* pParentFrame = nullptr;
    if (pParent)
        pParentFrame = pParent->mpWindowImpl->mpFrame;

    SalFrame* pFrame;
    if (pSystemParentData)
        pFrame = pSVData->mpDefInst->CreateChildFrame(pSystemParentData,
                                                      nFrameStyle | SalFrameStyleFlags::PLUG);
    else
        pFrame = pSVData->mpDefInst->CreateFrame(pParentFrame, nFrameStyle);

    if (!pFrame)
    {
        // do not abort but throw an exception, may be the current thread terminates anyway (plugin-scenario)
        throw css::uno::RuntimeException(u"Could not create system window!"_ustr,
                                         css::uno::Reference<css::uno::XInterface>());
    }

    pFrame->SetCallback(this, ImplWindowFrameProc);
    return pFrame;
}

void Window::ImplSetupFrame(SalFrame* pFrame, WinBits nStyle, vcl::Window* pInitialParent)
{
    // set window frame data
    mpWindowImpl->mpFrameData = new ImplFrameData(this);
    mpWindowImpl->mpFrame = pFrame;
    mpWindowImpl->mpFrameWindow = this;
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
        mpWindowImpl->mbDisabled = pParent->mpWindowImpl->mbDisabled;
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
    return !pSVData->maAppData.mbSettingsInit && !(nStyle & (WB_INTROWIN | WB_DEFAULTWIN));
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

void Window::ImplInitAppFontData(vcl::Window const* pWindow)
{
    ImplSVData* pSVData = ImplGetSVData();
    tools::Long nTextHeight = pWindow->GetTextHeight();
    tools::Long nTextWidth = pWindow->approximate_char_width() * 8;
    tools::Long nSymHeight = nTextHeight * 4;
    // Make the basis wider if the font is too narrow
    // such that the dialog looks symmetrical and does not become too narrow.
    // Add some extra space when the dialog has the same width,
    // as a little more space is better.
    if (nSymHeight > nTextWidth)
        nTextWidth = nSymHeight;
    else if (nSymHeight + 5 > nTextWidth)
        nTextWidth = nSymHeight + 5;
    pSVData->maGDIData.mnAppFontX = nTextWidth * 10 / 8;
    pSVData->maGDIData.mnAppFontY = nTextHeight * 10;

#ifdef MACOSX
    // FIXME: this is currently only on macOS, check with other
    // platforms
    if (pSVData->maNWFData.mbNoFocusRects)
    {
        // try to find out whether there is a large correction
        // of control sizes, if yes, make app font scalings larger
        // so dialog positioning is not completely off
        ImplControlValue aControlValue;
        tools::Rectangle aCtrlRegion(
            Point(), Size(nTextWidth < 10 ? 10 : nTextWidth, nTextHeight < 10 ? 10 : nTextHeight));
        tools::Rectangle aBoundingRgn(aCtrlRegion);
        tools::Rectangle aContentRgn(aCtrlRegion);
        if (pWindow->GetNativeControlRegion(ControlType::Editbox, ControlPart::Entire, aCtrlRegion,
                                            ControlState::ENABLED, aControlValue, aBoundingRgn,
                                            aContentRgn))
        {
            // comment: the magical +6 is for the extra border in bordered
            // (which is the standard) edit fields
            if (aContentRgn.GetHeight() - nTextHeight > (nTextHeight + 4) / 4)
                pSVData->maGDIData.mnAppFontY = (aContentRgn.GetHeight() - 4) * 10;
        }
    }
#endif
}

SalGraphics* Window::ImplGetFrameGraphics() const
{
    if (mpWindowImpl->mpFrameWindow->GetOutDev()->mpGraphics)
    {
        mpWindowImpl->mpFrameWindow->GetOutDev()->GetClipState().Invalidate();
    }
    else
    {
        OutputDevice* pFrameWinOutDev = mpWindowImpl->mpFrameWindow->GetOutDev();
        if (!pFrameWinOutDev->AcquireGraphics())
        {
            return nullptr;
        }
    }
    mpWindowImpl->mpFrameWindow->GetOutDev()->mpGraphics->ResetClipRegion();
    return mpWindowImpl->mpFrameWindow->GetOutDev()->mpGraphics;
}

} // end vcl namespace

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
