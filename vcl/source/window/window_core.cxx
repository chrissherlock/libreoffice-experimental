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

#include <osl/diagnose.h>
#include <comphelper/configuration.hxx>
#include <unotools/fontdefs.hxx>

#include <vcl/dockwin.hxx>
#include <vcl/rendercontext/GetDefaultFontFlags.hxx>
#include <vcl/svapp.hxx>
#include <vcl/vclevent.hxx>
#include <vcl/window.hxx>
#include <vcl/uitest/uiobject.hxx>

#include <window.h>
#include <dndeventdispatcher.hxx>
#include <helpwin.hxx>
#include <salframe.hxx>
#include <salinst.hxx>
#include <svdata.hxx>

#include <vector>

#include <com/sun/star/awt/XVclWindowPeer.hpp>

namespace vcl
{
Window::Window(WindowType eType)
    : mpWindowImpl(new WindowImpl(*this, eType))
{
    // true: this outdev will be mirrored if RTL window layout (UI mirroring) is globally active
    mpWindowImpl->mxOutDev->mbEnableRTL = AllSettings::GetLayoutRTL();
}

Window::Window(vcl::Window* pParent, WinBits nStyle)
    : mpWindowImpl(new WindowImpl(*this, WindowType::WINDOW))
{
    // true: this outdev will be mirrored if RTL window layout (UI mirroring) is globally active
    mpWindowImpl->mxOutDev->mbEnableRTL = AllSettings::GetLayoutRTL();

    ImplInit(pParent, nStyle, nullptr);
}

Window::~Window() { disposeOnce(); }

void Window::dispose()
{
    assert(mpWindowImpl);
    assert(!mpWindowImpl->mbInDispose); // should only be called from disposeOnce()
    assert(
        (!mpWindowImpl->mpHierarchy->mpParent || mpWindowImpl->mpHierarchy->mpParent->mpWindowImpl)
        && "vcl::Window child should have its parent disposed first");

    // remove Key and Mouse events issued by Application::PostKey/MouseEvent
    Application::RemoveMouseAndKeyEvents(this);

    // Dispose of the canvas implementation (which, currently, has an
    // own wrapper window as a child to this one.
    GetOutDev()->ImplDisposeCanvas();

    mpWindowImpl->mbInDispose = true;

    CallEventListeners(VclEventId::ObjectDying);

    // do not send child events for frames that were registered as native frames
    if (!IsNativeFrame() && mpWindowImpl->mbReallyVisible)
        if (ImplIsAccessibleCandidate() && GetAccessibleParentWindow())
            GetAccessibleParentWindow()->CallEventListeners(VclEventId::WindowChildDestroyed, this);

    // remove associated data structures from dockingmanager
    ImplGetDockingManager()->RemoveWindow(this);

    // remove ownerdraw decorated windows from list in the top-most frame window
    ImplRemoveOwnerDrawDecoratedFrame();

    ImplDeInitDND();
    ImplDeInitAccessibility();

#if OSL_DEBUG_LEVEL > 0
    ImplCheckLiveChildrenOnDestroy();
#endif

    if (ImplGetSVHelpData().mpHelpWin && (ImplGetSVHelpData().mpHelpWin->GetParent() == this))
        ImplDestroyHelpWindow(true);

    ImplSVData* pSVData = ImplGetSVData();

    SAL_WARN_IF(pSVData->mpWinData->mpTrackWin.get() == this, "vcl.window",
                "Window::~Window(): Window is in TrackingMode");
    SAL_WARN_IF(IsMouseCaptured(), "vcl.window",
                "Window::~Window(): Window has the mouse captured");

    // due to old compatibility
    if (pSVData->mpWinData->mpTrackWin == this)
        EndTracking();

    if (IsMouseCaptured())
        ReleaseMouse();

    if (mpWindowImpl->mbIsInTaskPaneList)
        ImplRemoveFromTaskPaneList();

    // remove from size-group if necessary
    remove_from_all_size_groups();

    // clear mnemonic labels
    for (auto const& mnemonicLabel : list_mnemonic_labels())
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

    vcl::Window* pOverlapWindow = ImplTransferFocus();

    if (pOverlapWindow != nullptr && pOverlapWindow->mpWindowImpl->mpLastFocusWindow == this)
        pOverlapWindow->mpWindowImpl->mpLastFocusWindow = nullptr;

    ImplResetGlobalWindowPointers();
    ImplResetFrameDataPointers();

    // release SalGraphics
    VclPtr<OutputDevice> pOutDev = GetOutDev();
    pOutDev->ReleaseGraphics();

    // remove window from the lists
    ImplRemoveWindow(true);

    ImplDeregisterTopWindowChild();

    mpWindowImpl->mpWinData.reset();

    ImplDisposeFrameData();

    if (mpWindowImpl->mxWindowPeer)
        mpWindowImpl->mxWindowPeer->dispose();

    // should be the last statements
    mpWindowImpl.reset();

    pOutDev.disposeAndClear();
    // just to make loplugin:vclwidgets happy
    VclReferenceBase::dispose();
}

static constexpr sal_Int32 lcl_CountDPIScaleFactor(sal_Int32 nDPI)
{
#ifndef MACOSX
    // Base DPI for a standard 100% scale display is 96.
    constexpr sal_Int32 nBaseDPI = 96;

    // We use a 25% scale increment (24 DPI) to establish mid-point thresholds
    // between the standard scaling tiers (100%, 150%, 200%, 250%).
    constexpr sal_Int32 nQuarterDPI = nBaseDPI / 4;

    // Calculate the threshold values for snapping to higher scales.
    constexpr sal_Int32 nSnap250 = (nBaseDPI * 2) + nQuarterDPI; // 216
    constexpr sal_Int32 nSnap200 = (nBaseDPI * 2) - nQuarterDPI; // 168 (fdo#77059)
    constexpr sal_Int32 nSnap150 = nBaseDPI + nQuarterDPI; // 120

    // Operating systems often report inaccurate or skewed physical DPI values.
    // Therefore, we use this heuristic to "snap" to the nearest logical UI
    // scaling percentage rather than calculating a raw, continuous ratio.
    if (nDPI > nSnap250)
        return 250;
    if (nDPI > nSnap200)
        return 200;
    if (nDPI > nSnap150)
        return 150;
#else
    // macOS handles high-DPI (Retina) scaling natively via the OS abstraction
    // layer, so we always default to the standard 100% baseline here.
    (void)nDPI;
#endif

    return 100;
}

void Window::ImplInit(vcl::Window* pParent, WinBits nStyle, SystemParentData* pSystemParentData)
{
    SAL_WARN_IF(!mpWindowImpl->mbFrame && !pParent && GetType() != WindowType::FIXEDIMAGE,
                "vcl.window", "Window::Window(): pParent == NULL");

    vcl::Window* pRealParent = pParent;

    nStyle = ImplApplyBorderAnd3DStyle(nStyle, pParent);
    BorderWindowStyle nBorderTypeStyle = ImplGetBorderWindowStyle(nStyle);

    pParent = ImplInitBorderWindow(pParent, nStyle, nBorderTypeStyle);

    // insert window in list
    ImplInsertWindow(pParent);
    mpWindowImpl->mnStyle = nStyle;

    if (pParent && !mpWindowImpl->mbFrame)
        mpWindowImpl->mxOutDev->mbEnableRTL = AllSettings::GetLayoutRTL();

    // test for frame creation
    if (mpWindowImpl->mbFrame)
    {
        SalFrameStyleFlags nFrameStyle = ImplGetFrameStyle(nStyle);
        SalFrame* pFrame = ImplCreateFrame(pParent, pSystemParentData, nFrameStyle);
        ImplSetupFrame(pFrame, nStyle, pRealParent);
    }

    // init data
    mpWindowImpl->mpHierarchy->mpRealParent = pRealParent;

    // #99318: make sure fontcache and list is available before call to SetSettings
    mpWindowImpl->mxOutDev->mxFontCollection = mpWindowImpl->mpFrameData->mxFontCollection;
    mpWindowImpl->mxOutDev->mxFontCache = mpWindowImpl->mpFrameData->mxFontCache;

    ImplInitResolution(pParent, nStyle);
    ImplInitSettings(nStyle);

    // setup the scale factor for HiDPI displays
    mpWindowImpl->mxOutDev->SetDPIScalePercentage(
        lcl_CountDPIScaleFactor(mpWindowImpl->mpFrameData->mnDPIY));
    mpWindowImpl->mxOutDev->SetDPIX(mpWindowImpl->mpFrameData->mnDPIX);
    mpWindowImpl->mxOutDev->SetDPIY(mpWindowImpl->mpFrameData->mnDPIY);

    if (!comphelper::IsFuzzing())
    {
        const StyleSettings& rStyleSettings
            = mpWindowImpl->mxOutDev->moSettings->GetStyleSettings();
        mpWindowImpl->mxOutDev->maFont = rStyleSettings.GetAppFont();

        if (nStyle & WB_3DLOOK)
        {
            SetTextColor(rStyleSettings.GetButtonTextColor());
            SetBackground(Wallpaper(rStyleSettings.GetFaceColor()));
        }
        else
        {
            SetTextColor(rStyleSettings.GetWindowTextColor());
            SetBackground(Wallpaper(rStyleSettings.GetWindowColor()));
        }
    }
    else
    {
        mpWindowImpl->mxOutDev->maFont = OutputDevice::GetDefaultFont(
            DefaultFontType::FIXED, LANGUAGE_ENGLISH_US, GetDefaultFontFlags::NONE);
    }

    ImplPointToLogic(*GetOutDev(), mpWindowImpl->mxOutDev->maFont);

    (void)ImplUpdatePos();

    ImplSVData* pSVData = ImplGetSVData();

    // calculate app font res (except for the Intro Window or the default window)
    if (mpWindowImpl->mbFrame && !pSVData->maGDIData.mnAppFontX
        && !(nStyle & (WB_INTROWIN | WB_DEFAULTWIN)))
        ImplInitAppFontData(this);
}

void Window::ImplInitResolutionSettings()
{
    // recalculate AppFont-resolution and DPI-resolution
    if (mpWindowImpl->mbFrame)
    {
        GetOutDev()->SetDPIX(mpWindowImpl->mpFrameData->mnDPIX);
        GetOutDev()->SetDPIY(mpWindowImpl->mpFrameData->mnDPIY);

        // setup the scale factor for HiDPI displays
        GetOutDev()->SetDPIScalePercentage(
            lcl_CountDPIScaleFactor(mpWindowImpl->mpFrameData->mnDPIY));
        const StyleSettings& rStyleSettings = GetOutDev()->moSettings->GetStyleSettings();
        SetPointFont(*GetOutDev(), rStyleSettings.GetAppFont());
    }
    else if (mpWindowImpl->mpHierarchy->mpParent)
    {
        GetOutDev()->SetDPIX(mpWindowImpl->mpHierarchy->mpParent->GetOutDev()->GetDPIX());
        GetOutDev()->SetDPIY(mpWindowImpl->mpHierarchy->mpParent->GetOutDev()->GetDPIY());
        GetOutDev()->SetDPIScalePercentage(
            mpWindowImpl->mpHierarchy->mpParent->GetOutDev()->GetDPIScalePercentage());
    }

    // update the recalculated values for logical units
    // and also tools belonging to the values
    if (GetMappingPolicy() == vcl::MappingPolicy::ApplyMapMode)
    {
        MapMode aMapMode = GetMapMode();
        SetMapMode();
        SetMapMode(aMapMode);
    }
}
WindowType Window::GetType() const
{
    if (mpWindowImpl)
        return mpWindowImpl->meType;
    else
        return WindowType::NONE;
}

void Window::SetType(WindowType eType)
{
    if (mpWindowImpl)
        mpWindowImpl->meType = eType;
}

bool Window::IsSystemWindow() const { return mpWindowImpl && mpWindowImpl->mbSysWin; }

bool Window::IsDialog() const { return mpWindowImpl && mpWindowImpl->mbDialog; }

void Window::CollectChildren(::std::vector<vcl::Window*>& rAllChildren)
{
    rAllChildren.push_back(this);

    VclPtr<vcl::Window> pChild = mpWindowImpl->mpHierarchy->mpFirstChild;
    while (pChild)
    {
        pChild->CollectChildren(rAllChildren);
        pChild = pChild->mpWindowImpl->mpHierarchy->mpNext;
    }
}

FactoryFunction Window::GetUITestFactory() const { return WindowUIObject::create; }

const OUString& Window::get_id() const
{
    static OUString empty;
    return mpWindowImpl ? mpWindowImpl->maID : empty;
}

void Window::set_id(const OUString& rID) { mpWindowImpl->maID = rID; }

void Window::SetCompoundControl(bool bCompound)
{
    if (mpWindowImpl)
        mpWindowImpl->mbCompoundControl = bCompound;
}

vcl::Window* Window::ImplGetFrameWindow() const
{
    return mpWindowImpl ? mpWindowImpl->mpFrameWindow.get() : nullptr;
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

ImplFrameData* Window::ImplGetFrameData()
{
    return mpWindowImpl ? mpWindowImpl->mpFrameData : nullptr;
}

SalFrame* Window::ImplGetFrame() const { return mpWindowImpl ? mpWindowImpl->mpFrame : nullptr; }

vcl::Window* Window::ImplGetWindow() const
{
    if (mpWindowImpl->mpClientWindow)
        return mpWindowImpl->mpClientWindow;
    else
        return const_cast<vcl::Window*>(this);
}

ImplWinData* Window::ImplGetWinData() const
{
    if (!mpWindowImpl->mpWinData)
    {
        static const char* pNoNWF = getenv("SAL_NO_NWF");

        const_cast<vcl::Window*>(this)->mpWindowImpl->mpWinData.reset(new ImplWinData);
        mpWindowImpl->mpWinData->mbEnableNativeWidget
            = !(pNoNWF && *pNoNWF); // true: try to draw this control with native theme API
    }

    return mpWindowImpl->mpWinData.get();
}

vcl::Window* Window::ImplGetClientWindow() const
{
    return mpWindowImpl ? mpWindowImpl->mpClientWindow.get() : nullptr;
}

bool Window::ImplIsFloatingWindow() const { return mpWindowImpl && mpWindowImpl->mbFloatWin; }

void Window::ImplDisposeFrameData()
{
    // remove BorderWindow or Frame window data
    mpWindowImpl->mpBorderWindow.disposeAndClear();

    if (!mpWindowImpl->mbFrame)
        return;

    ImplSVData* pSVData = ImplGetSVData();

    if (pSVData->maFrameData.mpFirstFrame == this)
    {
        pSVData->maFrameData.mpFirstFrame = mpWindowImpl->mpFrameData->mpNextFrame;
    }
    else
    {
        sal_Int32 nWindows = 0;
        vcl::Window* pSysWin = pSVData->maFrameData.mpFirstFrame;
        while (pSysWin && pSysWin->mpWindowImpl->mpFrameData->mpNextFrame.get() != this)
        {
            pSysWin = pSysWin->mpWindowImpl->mpFrameData->mpNextFrame;
            nWindows++;
        }

        if (pSysWin)
        {
            assert(mpWindowImpl->mpFrameData->mpNextFrame.get() != pSysWin);
            pSysWin->mpWindowImpl->mpFrameData->mpNextFrame
                = mpWindowImpl->mpFrameData->mpNextFrame;
        }
        else // if it is not in the list, we can't remove it.
        {
            SAL_WARN("vcl.window", "Window " << this
                                             << " marked as frame window, "
                                                "is missing from list of "
                                             << nWindows << " frames");
        }
    }

    if (mpWindowImpl->mpFrame) // otherwise exception during init
    {
        mpWindowImpl->mpFrame->SetCallback(nullptr, nullptr);
        pSVData->mpDefInst->DestroyFrame(mpWindowImpl->mpFrame);
    }

    assert(mpWindowImpl->mpFrameData->mnFocusId == nullptr);
    assert(mpWindowImpl->mpFrameData->mnMouseMoveId == nullptr);

    mpWindowImpl->mpFrameData->mpBuffer.disposeAndClear();
    delete mpWindowImpl->mpFrameData;
    mpWindowImpl->mpFrameData = nullptr;
}

#if OSL_DEBUG_LEVEL > 0
OString lcl_createWindowInfo(const vcl::Window* pWindow)
{
    // skip border windows, they do not carry information that
    // would help with diagnosing the problem
    const vcl::Window* pTempWin(pWindow);

    while (pTempWin && pTempWin->GetType() == WindowType::BORDERWINDOW)
    {
        pTempWin = pTempWin->GetWindow(GetWindowType::FirstChild);
    }

    // check if pTempWin is not null, otherwise use the
    // original address
    if (pTempWin)
        pWindow = pTempWin;

    return OString::Concat(" ") + typeid(*pWindow).name() + "("
           + OUStringToOString(pWindow->GetText(), RTL_TEXTENCODING_UTF8) + ")";
}

void Window::ImplCheckLiveChildrenOnDestroy()
{
    OStringBuffer aErrorStr;
    bool bError = false;
    vcl::Window* pTempWin;

    if (mpWindowImpl->mpHierarchy->mpFirstChild)
    {
        OStringBuffer aTempStr
            = "Window (" + lcl_createWindowInfo(this) + ") with live children destroyed: ";
        pTempWin = mpWindowImpl->mpHierarchy->mpFirstChild;
        while (pTempWin)
        {
            aTempStr.append(lcl_createWindowInfo(pTempWin));
            pTempWin = pTempWin->mpWindowImpl->mpHierarchy->mpNext;
        }
        OSL_FAIL(aTempStr.getStr());
        Application::Abort(OStringToOUString(aTempStr, RTL_TEXTENCODING_UTF8));
    }

    if (mpWindowImpl->mpFrameData != nullptr)
    {
        pTempWin = mpWindowImpl->mpFrameData->mpFirstOverlap;
        while (pTempWin)
        {
            if (IsAncestorOf(*pTempWin))
            {
                bError = true;
                aErrorStr.append(lcl_createWindowInfo(pTempWin));
            }
            pTempWin = pTempWin->mpWindowImpl->mpHierarchy->mpNextOverlap;
        }
        if (bError)
        {
            OString aTempStr = "Window (" + lcl_createWindowInfo(this)
                               + ") with live SystemWindows destroyed: " + aErrorStr;
            OSL_FAIL(aTempStr.getStr());
            Application::Abort(OStringToOUString(aTempStr, RTL_TEXTENCODING_UTF8));
        }
    }

    bError = false;
    ImplSVData* pSVData = ImplGetSVData();
    pTempWin = pSVData->maFrameData.mpFirstFrame;
    while (pTempWin)
    {
        if (IsAncestorOf(*pTempWin))
        {
            bError = true;
            aErrorStr.append(lcl_createWindowInfo(pTempWin));
        }
        pTempWin = pTempWin->mpWindowImpl->mpFrameData->mpNextFrame;
    }
    if (bError)
    {
        OString aTempStr = "Window (" + lcl_createWindowInfo(this)
                           + ") with live SystemWindows destroyed: " + aErrorStr;
        OSL_FAIL(aTempStr.getStr());
        Application::Abort(OStringToOUString(aTempStr, RTL_TEXTENCODING_UTF8));
    }

    if (mpWindowImpl->mpHierarchy->mpFirstOverlap)
    {
        OStringBuffer aTempStr
            = "Window (" + lcl_createWindowInfo(this) + ") with live SystemWindows destroyed: ";
        pTempWin = mpWindowImpl->mpHierarchy->mpFirstOverlap;
        while (pTempWin)
        {
            aTempStr.append(lcl_createWindowInfo(pTempWin));
            pTempWin = pTempWin->mpWindowImpl->mpHierarchy->mpNext;
        }
        OSL_FAIL(aTempStr.getStr());
        Application::Abort(OStringToOUString(aTempStr, RTL_TEXTENCODING_UTF8));
    }

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
    if (pMySysWin && pMySysWin->ImplIsInTaskPaneList(this))
    {
        OString aTempStr = "Window (" + lcl_createWindowInfo(this) + ") still in TaskPanelList!";
        OSL_FAIL(aTempStr.getStr());
        Application::Abort(OStringToOUString(aTempStr, RTL_TEXTENCODING_UTF8));
    }
}
#endif

void Window::ImplDeregisterTopWindowChild()
{
    if (!mpWindowImpl->mbFrame)
        return;

    bool bIsTopWindow = mpWindowImpl->mpWinData && (mpWindowImpl->mpWinData->mnIsTopWindow == 1);
    if (!bIsTopWindow || !mpWindowImpl->mpHierarchy->mpRealParent)
        return;

    ImplWinData* pParentWinData = mpWindowImpl->mpHierarchy->mpRealParent->ImplGetWinData();

    auto myPos = std::find(pParentWinData->maTopWindowChildren.begin(),
                           pParentWinData->maTopWindowChildren.end(), VclPtr<vcl::Window>(this));

    SAL_WARN_IF(myPos == pParentWinData->maTopWindowChildren.end(), "vcl.window",
                "Window::~Window: inconsistency in top window chain!");

    if (myPos != pParentWinData->maTopWindowChildren.end())
        pParentWinData->maTopWindowChildren.erase(myPos);
}

void Window::ImplResetGlobalWindowPointers()
{
    ImplSVData* pSVData = ImplGetSVData();

    // reset hint for DefModalDialogParent
    if (pSVData->maFrameData.mpActiveApplicationFrame == this)
        pSVData->maFrameData.mpActiveApplicationFrame = nullptr;

    // reset hint of what was the last wheeled window
    if (pSVData->mpWinData->mpLastWheelWindow == this)
        pSVData->mpWinData->mpLastWheelWindow = nullptr;

    // reset Deactivate-Window
    if (pSVData->mpWinData->mpLastDeacWin == this)
        pSVData->mpWinData->mpLastDeacWin = nullptr;
}

void Window::ImplResetFrameDataPointers()
{
    if (mpWindowImpl->mpFrameData == nullptr)
        return;

    // reset marked windows
    if (mpWindowImpl->mpFrameData->mpFocusWin == this)
        mpWindowImpl->mpFrameData->mpFocusWin = nullptr;

    if (mpWindowImpl->mpFrameData->mpMouseMoveWin == this)
        mpWindowImpl->mpFrameData->mpMouseMoveWin = nullptr;

    if (mpWindowImpl->mpFrameData->mpMouseDownWin == this)
        mpWindowImpl->mpFrameData->mpMouseDownWin = nullptr;

    // remove pending user events
    if (mpWindowImpl->mbFrame)
    {
        if (mpWindowImpl->mpFrameData->mnFocusId)
            Application::RemoveUserEvent(mpWindowImpl->mpFrameData->mnFocusId);
        mpWindowImpl->mpFrameData->mnFocusId = nullptr;

        if (mpWindowImpl->mpFrameData->mnMouseMoveId)
            Application::RemoveUserEvent(mpWindowImpl->mpFrameData->mnMouseMoveId);
        mpWindowImpl->mpFrameData->mnMouseMoveId = nullptr;
    }
}

bool Window::ImplIsSplitter() const { return mpWindowImpl && mpWindowImpl->mbSplitter; }

bool Window::ImplIsPushButton() const { return mpWindowImpl && mpWindowImpl->mbPushButton; }
} /* namespace vcl */

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
