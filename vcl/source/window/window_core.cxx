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
#include <comphelper/OAccessible.hxx>
#include <unotools/fontdefs.hxx>

#include <vcl/dockwin.hxx>
#include <vcl/dndlistenercontainer.hxx>
#include <vcl/rendercontext/GetDefaultFontFlags.hxx>
#include <vcl/svapp.hxx>
#include <vcl/taskpanelist.hxx>
#include <vcl/vclevent.hxx>
#include <vcl/window.hxx>
#include <vcl/uitest/uiobject.hxx>
#include <vcl/toolkit/fixed.hxx>
#include <vcl/toolkit/unowrap.hxx>

#include <ImplAccessibleInfos.hxx>
#include <ImplFrameData.hxx>
#include <ImplWinData.hxx>
#include <WindowPlatformState.hxx>
#include <WindowStyleState.hxx>
#include <WindowControlState.hxx>
#include <WindowFocusState.hxx>
#include <WindowVisibilityState.hxx>
#include <WindowHelpData.hxx>
#include <WindowClassification.hxx>
#include <WindowInput.hxx>
#include <WindowInvalidation.hxx>
#include <WindowEventHandlers.hxx>
#include <WindowLayoutData.hxx>
#include <WindowAccessibleData.hxx>
#include <WindowControlAppearance.hxx>
#include <WindowHierarchy.hxx>
#include <WindowGeometry.hxx>
#include <WindowViewport.hxx>
#include <WindowLOKData.hxx>
#include <WindowClippingState.hxx>
#include <WindowPointerState.hxx>
#include <dndeventdispatcher.hxx>
#include <helpwin.hxx>
#include <salframe.hxx>
#include <salinst.hxx>
#include <svdata.hxx>

#include <vector>

#include <com/sun/star/awt/XVclWindowPeer.hpp>
#include <com/sun/star/datatransfer/dnd/XDragGestureRecognizer.hpp>

namespace vcl
{
Window::Window(WindowType eType)
    : mpClassification(std::make_unique<WindowClassification>(eType))
    , mpInput(std::make_unique<WindowInput>())
    , mpHierarchy(std::make_unique<WindowHierarchy>())
    , mpHelpData(std::make_unique<WindowHelpData>())
    , mpEventHandlers(std::make_unique<WindowEventHandlers>())
    , mpLayoutData(std::make_unique<WindowLayoutData>())
    , mpAccessibleData(std::make_unique<WindowAccessibleData>())
    , mpControlAppearance(std::make_unique<WindowControlAppearance>())
    , mpGeometry(std::make_unique<WindowGeometry>())
    , mpViewport(std::make_unique<WindowViewport>())
    , mpLOKData(std::make_unique<WindowLOKData>())
    , mpClippingState(std::make_unique<WindowClippingState>())
    , mpInvalidation(std::make_unique<WindowInvalidation>())
    , mpFocusState(std::make_unique<WindowFocusState>())
    , mpVisibilityState(std::make_unique<WindowVisibilityState>())
    , mpPointerState(std::make_unique<WindowPointerState>())
    , mpControlState(std::make_unique<WindowControlState>())
    , mpStyleState(std::make_unique<WindowStyleState>())
    , mpPlatformState(std::make_unique<WindowPlatformState>())
{
    mpWinData = nullptr;
    mxOutDev = VclPtr<vcl::WindowOutputDevice>::Create(*this);

    // true: this outdev will be mirrored if RTL window layout (UI mirroring) is globally active
    mxOutDev->mbEnableRTL = AllSettings::GetLayoutRTL();
}

Window::Window(vcl::Window* pParent, WinBits nStyle)
    : mpClassification(std::make_unique<WindowClassification>(WindowType::WINDOW))
    , mpInput(std::make_unique<WindowInput>())
    , mpHierarchy(std::make_unique<WindowHierarchy>())
    , mpHelpData(std::make_unique<WindowHelpData>())
    , mpEventHandlers(std::make_unique<WindowEventHandlers>())
    , mpLayoutData(std::make_unique<WindowLayoutData>())
    , mpAccessibleData(std::make_unique<WindowAccessibleData>())
    , mpControlAppearance(std::make_unique<WindowControlAppearance>())
    , mpGeometry(std::make_unique<WindowGeometry>())
    , mpViewport(std::make_unique<WindowViewport>())
    , mpLOKData(std::make_unique<WindowLOKData>())
    , mpClippingState(std::make_unique<WindowClippingState>())
    , mpInvalidation(std::make_unique<WindowInvalidation>())
    , mpFocusState(std::make_unique<WindowFocusState>())
    , mpVisibilityState(std::make_unique<WindowVisibilityState>())
    , mpPointerState(std::make_unique<WindowPointerState>())
    , mpControlState(std::make_unique<WindowControlState>())
    , mpStyleState(std::make_unique<WindowStyleState>())
    , mpPlatformState(std::make_unique<WindowPlatformState>())
{
    mpWinData = nullptr;
    mxOutDev = VclPtr<vcl::WindowOutputDevice>::Create(*this);

    // true: this outdev will be mirrored if RTL window layout (UI mirroring) is globally active
    mxOutDev->mbEnableRTL = AllSettings::GetLayoutRTL();

    ImplInit(pParent, nStyle, nullptr);
}

Window::~Window() { disposeOnce(); }

void Window::dispose()
{
    assert(mpClassification);
    assert(!mpClassification->mbInDispose); // should only be called from disposeOnce()
    assert((!mpHierarchy->mpParent || mpHierarchy->mpParent->mpClassification)
           && "vcl::Window child should have its parent disposed first");

    // remove Key and Mouse events issued by Application::PostKey/MouseEvent
    Application::RemoveMouseAndKeyEvents(this);

    // Dispose of the canvas implementation (which, currently, has an
    // own wrapper window as a child to this one.
    GetOutDev()->ImplDisposeCanvas();

    mpClassification->mbInDispose = true;

    CallEventListeners(VclEventId::ObjectDying);

    // do not send child events for frames that were registered as native frames
    if (!IsNativeFrame() && mpVisibilityState->mbReallyVisible)
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

    if (mpControlState->mbIsInTaskPaneList)
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

    if (pOverlapWindow != nullptr && pOverlapWindow->mpInput->mpLastFocusWindow == this)
        pOverlapWindow->mpInput->mpLastFocusWindow = nullptr;

    ImplResetGlobalWindowPointers();
    ImplResetFrameDataPointers();

    // release SalGraphics
    VclPtr<OutputDevice> pOutDev = GetOutDev();
    pOutDev->ReleaseGraphics();

    // remove window from the lists
    ImplRemoveWindow(true);

    ImplDeregisterTopWindowChild();

    mpWinData.reset();

    ImplDisposeFrameData();

    if (mpAccessibleData->mxWindowPeer)
        mpAccessibleData->mxWindowPeer->dispose();

    // should be the last statements
    mpClassification.reset();
    mpInput.reset();
    mpHierarchy.reset();
    mpHelpData.reset();
    mpEventHandlers.reset();
    mpLayoutData.reset();
    mpAccessibleData.reset();
    mpControlAppearance.reset();
    mpGeometry.reset();
    mpViewport.reset();
    mpLOKData.reset();
    mpClippingState.reset();
    mpInvalidation.reset();
    mpFocusState.reset();
    mpVisibilityState.reset();
    mpPointerState.reset();
    mpControlState.reset();
    mpStyleState.reset();
    mpPlatformState.reset();

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
    SAL_WARN_IF(!mpClassification->mbFrame && !pParent && GetType() != WindowType::FIXEDIMAGE,
                "vcl.window", "Window::Window(): pParent == NULL");

    vcl::Window* pRealParent = pParent;

    nStyle = ImplApplyBorderAnd3DStyle(nStyle, pParent);
    BorderWindowStyle nBorderTypeStyle = ImplGetBorderWindowStyle(nStyle);

    pParent = ImplInitBorderWindow(pParent, nStyle, nBorderTypeStyle);

    // insert window in list
    ImplInsertWindow(pParent);
    mpStyleState->mnStyle = nStyle;

    if (pParent && !mpClassification->mbFrame)
        mxOutDev->mbEnableRTL = AllSettings::GetLayoutRTL();

    // test for frame creation
    if (mpClassification->mbFrame)
    {
        SalFrameStyleFlags nFrameStyle = ImplGetFrameStyle(nStyle);
        SalFrame* pFrame = ImplCreateFrame(pParent, pSystemParentData, nFrameStyle);
        ImplSetupFrame(pFrame, nStyle, pRealParent);
    }

    // init data
    mpHierarchy->mpRealParent = pRealParent;

    // #99318: make sure fontcache and list is available before call to SetSettings
    mxOutDev->mxFontCollection = mpPlatformState->mpFrameData->mxFontCollection;
    mxOutDev->mxFontCache = mpPlatformState->mpFrameData->mxFontCache;

    ImplInitResolution(pParent, nStyle);
    ImplInitSettings(nStyle);

    // setup the scale factor for HiDPI displays
    mxOutDev->SetDPIScalePercentage(lcl_CountDPIScaleFactor(mpPlatformState->mpFrameData->mnDPIY));
    mxOutDev->SetDPIX(mpPlatformState->mpFrameData->mnDPIX);
    mxOutDev->SetDPIY(mpPlatformState->mpFrameData->mnDPIY);

    if (!comphelper::IsFuzzing())
    {
        const StyleSettings& rStyleSettings = mxOutDev->moSettings->GetStyleSettings();
        mxOutDev->maFont = rStyleSettings.GetAppFont();

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
        mxOutDev->maFont = OutputDevice::GetDefaultFont(DefaultFontType::FIXED, LANGUAGE_ENGLISH_US,
                                                        GetDefaultFontFlags::NONE);
    }

    ImplPointToLogic(*GetOutDev(), mxOutDev->maFont);

    (void)ImplUpdatePos();

    ImplSVData* pSVData = ImplGetSVData();

    // calculate app font res (except for the Intro Window or the default window)
    if (mpClassification->mbFrame && !pSVData->maGDIData.mnAppFontX
        && !(nStyle & (WB_INTROWIN | WB_DEFAULTWIN)))
        ImplInitAppFontData(this);
}

void Window::ImplInitResolutionSettings()
{
    // recalculate AppFont-resolution and DPI-resolution
    if (mpClassification->mbFrame)
    {
        GetOutDev()->SetDPIX(mpPlatformState->mpFrameData->mnDPIX);
        GetOutDev()->SetDPIY(mpPlatformState->mpFrameData->mnDPIY);

        // setup the scale factor for HiDPI displays
        GetOutDev()->SetDPIScalePercentage(
            lcl_CountDPIScaleFactor(mpPlatformState->mpFrameData->mnDPIY));
        const StyleSettings& rStyleSettings = GetOutDev()->moSettings->GetStyleSettings();
        SetPointFont(*GetOutDev(), rStyleSettings.GetAppFont());
    }
    else if (mpHierarchy->mpParent)
    {
        GetOutDev()->SetDPIX(mpHierarchy->mpParent->GetOutDev()->GetDPIX());
        GetOutDev()->SetDPIY(mpHierarchy->mpParent->GetOutDev()->GetDPIY());
        GetOutDev()->SetDPIScalePercentage(
            mpHierarchy->mpParent->GetOutDev()->GetDPIScalePercentage());
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
    if (mpClassification)
        return mpClassification->meType;
    else
        return WindowType::NONE;
}

void Window::SetType(WindowType eType)
{
    if (mpClassification)
        mpClassification->meType = eType;
}

bool Window::IsSystemWindow() const { return mpClassification && mpClassification->mbSysWin; }

bool Window::IsDialog() const { return mpClassification && mpClassification->mbDialog; }

void Window::CollectChildren(::std::vector<vcl::Window*>& rAllChildren)
{
    rAllChildren.push_back(this);

    VclPtr<vcl::Window> pChild = mpHierarchy->mpFirstChild;
    while (pChild)
    {
        pChild->CollectChildren(rAllChildren);
        pChild = pChild->mpHierarchy->mpNext;
    }
}

FactoryFunction Window::GetUITestFactory() const { return WindowUIObject::create; }

const OUString& Window::get_id() const
{
    static OUString empty;
    return mpHelpData ? mpHelpData->maID : empty;
}

void Window::set_id(const OUString& rID) { mpHelpData->maID = rID; }

void Window::SetCompoundControl(bool bCompound)
{
    if (mpFocusState)
        mpFocusState->mbCompoundControl = bCompound;
}

vcl::Window* Window::ImplGetFrameWindow() const
{
    return mpHierarchy ? mpHierarchy->mpFrameWindow.get() : nullptr;
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
    return mpClassification ? mpPlatformState->mpFrameData : nullptr;
}

SalFrame* Window::ImplGetFrame() const
{
    return mpPlatformState ? mpPlatformState->mpFrame : nullptr;
}

vcl::Window* Window::ImplGetWindow() const
{
    if (mpHierarchy->mpClientWindow)
        return mpHierarchy->mpClientWindow;
    else
        return const_cast<vcl::Window*>(this);
}

ImplWinData* Window::ImplGetWinData() const
{
    if (!mpWinData)
    {
        static const char* pNoNWF = getenv("SAL_NO_NWF");

        const_cast<vcl::Window*>(this)->mpWinData.reset(new ImplWinData);
        mpWinData->mbEnableNativeWidget
            = !(pNoNWF && *pNoNWF); // true: try to draw this control with native theme API
    }

    return mpWinData.get();
}

vcl::Window* Window::ImplGetClientWindow() const
{
    return mpHierarchy ? mpHierarchy->mpClientWindow.get() : nullptr;
}

bool Window::ImplIsFloatingWindow() const
{
    return mpClassification && mpClassification->mbFloatWin;
}

void Window::ImplDisposeFrameData()
{
    // remove BorderWindow or Frame window data
    mpHierarchy->mpBorderWindow.disposeAndClear();

    if (!mpClassification->mbFrame)
        return;

    ImplSVData* pSVData = ImplGetSVData();

    if (pSVData->maFrameData.mpFirstFrame == this)
    {
        pSVData->maFrameData.mpFirstFrame = mpPlatformState->mpFrameData->mpNextFrame;
    }
    else
    {
        sal_Int32 nWindows = 0;
        vcl::Window* pSysWin = pSVData->maFrameData.mpFirstFrame;
        while (pSysWin && pSysWin->mpPlatformState->mpFrameData->mpNextFrame.get() != this)
        {
            pSysWin = pSysWin->mpPlatformState->mpFrameData->mpNextFrame;
            nWindows++;
        }

        if (pSysWin)
        {
            assert(mpPlatformState->mpFrameData->mpNextFrame.get() != pSysWin);
            pSysWin->mpPlatformState->mpFrameData->mpNextFrame
                = mpPlatformState->mpFrameData->mpNextFrame;
        }
        else // if it is not in the list, we can't remove it.
        {
            SAL_WARN("vcl.window", "Window " << this
                                             << " marked as frame window, "
                                                "is missing from list of "
                                             << nWindows << " frames");
        }
    }

    if (mpPlatformState->mpFrame) // otherwise exception during init
    {
        mpPlatformState->mpFrame->SetCallback(nullptr, nullptr);
        pSVData->mpDefInst->DestroyFrame(mpPlatformState->mpFrame);
    }

    assert(mpPlatformState->mpFrameData->mnFocusId == nullptr);
    assert(mpPlatformState->mpFrameData->mnMouseMoveId == nullptr);

    mpPlatformState->mpFrameData->mpBuffer.disposeAndClear();
    delete mpPlatformState->mpFrameData;
    mpPlatformState->mpFrameData = nullptr;
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

    if (mpHierarchy->mpFirstChild)
    {
        OStringBuffer aTempStr
            = "Window (" + lcl_createWindowInfo(this) + ") with live children destroyed: ";
        pTempWin = mpHierarchy->mpFirstChild;
        while (pTempWin)
        {
            aTempStr.append(lcl_createWindowInfo(pTempWin));
            pTempWin = pTempWin->mpHierarchy->mpNext;
        }
        OSL_FAIL(aTempStr.getStr());
        Application::Abort(OStringToOUString(aTempStr, RTL_TEXTENCODING_UTF8));
    }

    if (mpPlatformState->mpFrameData != nullptr)
    {
        pTempWin = mpPlatformState->mpFrameData->mpFirstOverlap;
        while (pTempWin)
        {
            if (IsAncestorOf(*pTempWin))
            {
                bError = true;
                aErrorStr.append(lcl_createWindowInfo(pTempWin));
            }
            pTempWin = pTempWin->mpHierarchy->mpNextOverlap;
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
        pTempWin = pTempWin->mpPlatformState->mpFrameData->mpNextFrame;
    }
    if (bError)
    {
        OString aTempStr = "Window (" + lcl_createWindowInfo(this)
                           + ") with live SystemWindows destroyed: " + aErrorStr;
        OSL_FAIL(aTempStr.getStr());
        Application::Abort(OStringToOUString(aTempStr, RTL_TEXTENCODING_UTF8));
    }

    if (mpHierarchy->mpFirstOverlap)
    {
        OStringBuffer aTempStr
            = "Window (" + lcl_createWindowInfo(this) + ") with live SystemWindows destroyed: ";
        pTempWin = mpHierarchy->mpFirstOverlap;
        while (pTempWin)
        {
            aTempStr.append(lcl_createWindowInfo(pTempWin));
            pTempWin = pTempWin->mpHierarchy->mpNext;
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
    if (!mpClassification->mbFrame)
        return;

    bool bIsTopWindow = mpWinData && (mpWinData->mnIsTopWindow == 1);
    if (!bIsTopWindow || !mpHierarchy->mpRealParent)
        return;

    ImplWinData* pParentWinData = mpHierarchy->mpRealParent->ImplGetWinData();

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
    if (mpPlatformState->mpFrameData == nullptr)
        return;

    // reset marked windows
    if (mpPlatformState->mpFrameData->mpFocusWin == this)
        mpPlatformState->mpFrameData->mpFocusWin = nullptr;

    if (mpPlatformState->mpFrameData->mpMouseMoveWin == this)
        mpPlatformState->mpFrameData->mpMouseMoveWin = nullptr;

    if (mpPlatformState->mpFrameData->mpMouseDownWin == this)
        mpPlatformState->mpFrameData->mpMouseDownWin = nullptr;

    // remove pending user events
    if (mpClassification->mbFrame)
    {
        if (mpPlatformState->mpFrameData->mnFocusId)
            Application::RemoveUserEvent(mpPlatformState->mpFrameData->mnFocusId);
        mpPlatformState->mpFrameData->mnFocusId = nullptr;

        if (mpPlatformState->mpFrameData->mnMouseMoveId)
            Application::RemoveUserEvent(mpPlatformState->mpFrameData->mnMouseMoveId);
        mpPlatformState->mpFrameData->mnMouseMoveId = nullptr;
    }
}

bool Window::ImplIsSplitter() const { return mpClassification && mpClassification->mbSplitter; }

bool Window::ImplIsPushButton() const { return mpClassification && mpClassification->mbPushButton; }

void Window::ImplRemoveOwnerDrawDecoratedFrame()
{
    if (!(GetStyle() & WB_OWNERDRAWDECORATION) || !mpClassification->mbFrame)
        return;

    auto& rList = ImplGetOwnerDrawList();
    auto p = std::find(rList.begin(), rList.end(), VclPtr<vcl::Window>(this));
    if (p != rList.end())
        rList.erase(p);
}

void Window::ImplDeInitDND()
{
    // shutdown drag and drop listener container
    if (mpLOKData->mxDNDListenerContainer.is())
        mpLOKData->mxDNDListenerContainer->dispose();

    if (!mpClassification->mbFrame || !mpPlatformState->mpFrameData)
        return;

    try
    {
        // deregister drop target listener
        if (mpPlatformState->mpFrameData->mxDropTargetListener.is())
        {
            css::uno::Reference<css::datatransfer::dnd::XDragGestureRecognizer>
                xDragGestureRecognizer(mpPlatformState->mpFrameData->mxDragSource,
                                       css::uno::UNO_QUERY);
            if (xDragGestureRecognizer.is())
            {
                xDragGestureRecognizer->removeDragGestureListener(
                    mpPlatformState->mpFrameData->mxDropTargetListener);
            }

            mpPlatformState->mpFrameData->mxDropTarget->removeDropTargetListener(
                mpPlatformState->mpFrameData->mxDropTargetListener);
            mpPlatformState->mpFrameData->mxDropTargetListener.clear();
        }

        // shutdown drag and drop for this frame window
        css::uno::Reference<css::lang::XComponent> xComponent(
            mpPlatformState->mpFrameData->mxDropTarget, css::uno::UNO_QUERY);

        // DNDEventDispatcher does not hold a reference of the DropTarget,
        // so it's ok if it does not support XComponent
        if (xComponent.is())
            xComponent->dispose();
    }
    catch (const css::uno::Exception&)
    {
        // can be safely ignored here.
    }
}

void Window::ImplDeInitAccessibility()
{
    UnoWrapperBase* pWrapper = UnoWrapperBase::GetUnoWrapper(false);
    if (pWrapper)
        pWrapper->WindowDestroyed(this);

    if (mpAccessibleData)
        mpAccessibleData->dispose();
}

void Window::ImplRemoveFromTaskPaneList()
{
    vcl::Window* pMyParent = GetParent();
    SystemWindow* pMySysWin = nullptr;

    while (pMyParent)
    {
        if (pMyParent->IsSystemWindow())
            pMySysWin = dynamic_cast<SystemWindow*>(pMyParent);

        pMyParent = pMyParent->GetParent();
    }

    if (pMySysWin && pMySysWin->ImplIsInTaskPaneList(this))
        pMySysWin->GetTaskPaneList()->RemoveWindow(this);
    else
        SAL_WARN("vcl", "Window (" << GetText() << ") not found in TaskPanelList");
}
} /* namespace vcl */

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
