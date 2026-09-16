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

#include <window/accessibility/ImplAccessibleInfos.hxx>
#include <window/state/ImplFrameData.hxx>
#include <window/state/ImplWinData.hxx>
#include <window/state/WindowPlatformState.hxx>
#include <window/state/WindowStyleState.hxx>
#include <window/state/WindowControlState.hxx>
#include <window/state/WindowFocusState.hxx>
#include <window/state/WindowVisibilityState.hxx>
#include <window/state/WindowHelpData.hxx>
#include <window/state/WindowClassification.hxx>
#include <window/state/WindowInput.hxx>
#include <window/state/WindowInvalidation.hxx>
#include <window/state/WindowEventHandlers.hxx>
#include <window/state/WindowLayoutData.hxx>
#include <window/state/WindowA11y.hxx>
#include <window/state/WindowControlAppearance.hxx>
#include <window/state/WindowHierarchy.hxx>
#include <window/state/WindowGeometry.hxx>
#include <window/state/WindowViewport.hxx>
#include <window/state/WindowLOKData.hxx>
#include <window/state/WindowClippingState.hxx>
#include <window/state/WindowPointerState.hxx>
#include <window/dnd/dndeventdispatcher.hxx>
#include <window/subclasses/floating/helpwin.hxx>
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
    , mpAccessibleData(std::make_unique<WindowA11y>())
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
    , mpAccessibleData(std::make_unique<WindowA11y>())
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

    const vcl::Window* pParent = mpHierarchy ? mpHierarchy->getParent() : nullptr;
    assert((!pParent || pParent->ImplGetWindowClassification())
           && "vcl::Window child should have its parent disposed first");

    // Remove Key and Mouse events issued by Application::PostKey/MouseEvent
    Application::RemoveMouseAndKeyEvents(this);

    // Dispose of the canvas implementation (which, currently, has an
    // own wrapper window as a child to this one)
    if (OutputDevice* pDev = GetOutDev())
        pDev->ImplDisposeCanvas();

    mpClassification->mbInDispose = true;

    CallEventListeners(VclEventId::ObjectDying);

    // Do not send child events for frames that were registered as native frames
    if (!IsNativeFrame() && mpVisibilityState && mpVisibilityState->mbReallyVisible)
    {
        if (ImplIsAccessibleCandidate())
        {
            if (vcl::Window* pAccessibleParent = GetAccessibleParentWindow())
                pAccessibleParent->CallEventListeners(VclEventId::WindowChildDestroyed, this);
        }
    }

    // Remove associated data structures from dockingmanager
    ImplGetDockingManager()->RemoveWindow(this);

    // Remove ownerdraw decorated windows from list in the top-most frame window
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

    // Due to old compatibility
    if (pSVData->mpWinData->mpTrackWin == this)
        EndTracking();

    if (IsMouseCaptured())
        ReleaseMouse();

    if (mpControlState && mpControlState->mbIsInTaskPaneList)
        ImplRemoveFromTaskPaneList();

    // Remove from size-group if necessary
    remove_from_all_size_groups();

    // Clear mnemonic labels
    for (auto const& mnemonicLabel : list_mnemonic_labels())
    {
        remove_mnemonic_label(mnemonicLabel);
    }

    // Hide window in order to trigger the Paint-Handling
    Hide();

    // EndExtTextInputMode
    if (pSVData->mpWinData->mpExtTextInputWin == this)
    {
        EndExtTextInput();
        if (pSVData->mpWinData->mpExtTextInputWin == this)
            pSVData->mpWinData->mpExtTextInputWin = nullptr;
    }

    vcl::Window* pOverlapWindow = ImplTransferFocus();

    if (pOverlapWindow && pOverlapWindow->mpInput)
    {
        if (pOverlapWindow->mpInput->getLastFocusWindow() == this)
            pOverlapWindow->mpInput->clearLastFocusWindow();
    }

    ImplResetGlobalWindowPointers();
    ImplResetFrameDataPointers();

    // Release SalGraphics
    VclPtr<OutputDevice> pOutDev = GetOutDev();
    if (pOutDev)
        pOutDev->ReleaseGraphics();

    // Remove window from the lists
    ImplRemoveWindow(true);

    ImplDeregisterTopWindowChild();

    mpWinData.reset();

    ImplDisposeFrameData();

    if (mpAccessibleData)
        mpAccessibleData->disposeWindowPeer();

    // Should be the last statements
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

    if (pOutDev)
        pOutDev.disposeAndClear();

    // Just to make loplugin:vclwidgets happy
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
    // This is only the initial classification. ImplInitBorderWindow()
    // may promote an unparented window to a frame.
    const bool bInitiallyFrame = mpClassification && mpClassification->mbFrame;

    SAL_WARN_IF(!bInitiallyFrame && !pParent && GetType() != WindowType::FIXEDIMAGE,
                "vcl.window", "Window::Window(): pParent == NULL");

    // CRITICAL: Ensure we fallback to the default window to prevent
    // mpPlatformState->mpFrameData from remaining null, which causes
    // downstream segfaults when acquiring graphics contexts.
    if (!pParent && !bInitiallyFrame)
    {
        pParent = ImplGetDefaultWindow();
    }

    vcl::Window* pRealParent = pParent;

    // Delegate to the proper VCL helpers to evaluate 3D styles and border types.
    // This avoids the incomplete type error for BorderWindowStyle::App.
    nStyle = ImplApplyBorderAnd3DStyle(nStyle, pParent);
    auto nBorderTypeStyle = ImplGetBorderWindowStyle(nStyle);

    pParent = ImplInitBorderWindow(pParent, nStyle, nBorderTypeStyle);

    // ImplInitBorderWindow() may have changed mbFrame, so this must be
    // evaluated after it.
    const bool bIsFrame = mpClassification && mpClassification->mbFrame;

    // Insert window in list
    ImplInsertWindow(pParent);
    if (mpStyleState)
        mpStyleState->mnStyle = nStyle;

    if (pParent && !bIsFrame && mxOutDev)
        mxOutDev->mbEnableRTL = AllSettings::GetLayoutRTL();

    // Test for frame creation
    if (bIsFrame)
    {
        SalFrameStyleFlags nFrameStyle = ImplGetFrameStyle(nStyle);
        SalFrame* pFrame = ImplCreateFrame(pParent, pSystemParentData, nFrameStyle);
        ImplSetupFrame(pFrame, nStyle, pRealParent);
    }

    // Init data
    if (mpHierarchy)
        mpHierarchy->setRealParent(pRealParent);

    // #99318: make sure fontcache and list is available before call to SetSettings
    ImplFrameData* pFrameData = mpPlatformState ? mpPlatformState->mpFrameData : nullptr;
    if (mxOutDev && pFrameData)
    {
        mxOutDev->mxFontCollection = pFrameData->mxFontCollection;
        mxOutDev->mxFontCache = pFrameData->mxFontCache;
    }

    ImplInitResolution(pParent, nStyle);
    ImplInitSettings(nStyle);

    if (mxOutDev && pFrameData)
    {
        // Setup the scale factor for HiDPI displays
        mxOutDev->SetDPIScalePercentage(lcl_CountDPIScaleFactor(pFrameData->mnDPIY));
        mxOutDev->SetDPIX(pFrameData->mnDPIX);
        mxOutDev->SetDPIY(pFrameData->mnDPIY);
    }

    if (mxOutDev)
    {
        if (!comphelper::IsFuzzing())
        {
            const StyleSettings& rStyleSettings = mxOutDev->moSettings
                                                      ? mxOutDev->moSettings->GetStyleSettings()
                                                      : Application::GetSettings().GetStyleSettings();

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
            mxOutDev->maFont = OutputDevice::GetDefaultFont(DefaultFontType::FIXED,
                                                            LANGUAGE_ENGLISH_US,
                                                            GetDefaultFontFlags::NONE);
        }

        ImplPointToLogic(*mxOutDev, mxOutDev->maFont);
    }

    (void)ImplUpdatePos();

    ImplSVData* pSVData = ImplGetSVData();

    // Calculate app font res (except for the Intro Window or the default window)
    if (bIsFrame && pSVData && !pSVData->maGDIData.mnAppFontX
        && !(nStyle & (WB_INTROWIN | WB_DEFAULTWIN)))
    {
        ImplInitAppFontData(this);
    }
}

void Window::ImplInitResolutionSettings()
{
    OutputDevice* pOutDev = GetOutDev();
    if (!pOutDev)
        return;

    const bool bIsFrame = mpClassification && mpClassification->mbFrame;

    // Recalculate AppFont-resolution and DPI-resolution
    if (bIsFrame)
    {
        ImplFrameData* pFrameData = mpPlatformState ? mpPlatformState->mpFrameData : nullptr;
        if (pFrameData)
        {
            pOutDev->SetDPIX(pFrameData->mnDPIX);
            pOutDev->SetDPIY(pFrameData->mnDPIY);

            // Setup the scale factor for HiDPI displays
            pOutDev->SetDPIScalePercentage(lcl_CountDPIScaleFactor(pFrameData->mnDPIY));
        }

        const StyleSettings& rStyleSettings = pOutDev->moSettings
                                                  ? pOutDev->moSettings->GetStyleSettings()
                                                  : Application::GetSettings().GetStyleSettings();
        SetPointFont(*pOutDev, rStyleSettings.GetAppFont());
    }
    else if (mpHierarchy)
    {
        if (const vcl::Window* pParent = mpHierarchy->getParent())
        {
            if (const OutputDevice* pParentOutDev = pParent->GetOutDev())
            {
                pOutDev->SetDPIX(pParentOutDev->GetDPIX());
                pOutDev->SetDPIY(pParentOutDev->GetDPIY());
                pOutDev->SetDPIScalePercentage(pParentOutDev->GetDPIScalePercentage());
            }
        }
    }

    // Update the recalculated values for logical units
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

void Window::CollectChildren(std::vector<vcl::Window*>& rAllChildren)
{
    rAllChildren.push_back(this);

    if (!mpHierarchy)
        return;

    WindowHierarchy::forEachSibling(mpHierarchy->getFirstChild(), [&rAllChildren](vcl::Window& rChild) {
        rChild.CollectChildren(rAllChildren);
    });
}

FactoryFunction Window::GetUITestFactory() const { return WindowUIObject::create; }

const OUString& Window::get_id() const
{
    static OUString empty;
    return mpHelpData ? mpHelpData->maID : empty;
}

void Window::set_id(const OUString& rID) { mpHelpData->maID = rID; }

void Window::SetCompoundControl(bool bCompound) { mpFocusState->setCompoundControl(bCompound); }

vcl::Window* Window::ImplGetFrameWindow() const
{
    return mpHierarchy ? mpHierarchy->getFrameWindow() : nullptr;
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
    vcl::Window* pClientWindow = mpHierarchy ? mpHierarchy->getClientWindow() : nullptr;
    if (pClientWindow)
        return pClientWindow;

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
    return mpHierarchy ? mpHierarchy->getClientWindow() : nullptr;
}

bool Window::ImplIsFloatingWindow() const
{
    return mpClassification && mpClassification->mbFloatWin;
}

void Window::ImplDisposeFrameData()
{
    // Remove BorderWindow or Frame window data
    if (mpHierarchy)
        mpHierarchy->disposeBorderWindow();

    if (!mpClassification || !mpClassification->mbFrame)
        return;

    ImplSVData* pSVData = ImplGetSVData();
    ImplFrameData* pFrameData = mpPlatformState ? mpPlatformState->mpFrameData : nullptr;

    if (pSVData->maFrameData.mpFirstFrame == this)
    {
        pSVData->maFrameData.mpFirstFrame = pFrameData ? pFrameData->mpNextFrame : nullptr;
    }
    else
    {
        sal_Int32 nWindows = 0;
        vcl::Window* pSysWin = pSVData->maFrameData.mpFirstFrame;
        while (pSysWin)
        {
            ImplFrameData* pSysFrameData = pSysWin->mpPlatformState ? pSysWin->mpPlatformState->mpFrameData : nullptr;
            if (!pSysFrameData || pSysFrameData->mpNextFrame.get() == this)
                break;

            pSysWin = pSysFrameData->mpNextFrame;
            nWindows++;
        }

        if (pSysWin)
        {
            ImplFrameData* pSysFrameData = pSysWin->mpPlatformState ? pSysWin->mpPlatformState->mpFrameData : nullptr;
            if (pSysFrameData && pFrameData)
            {
                assert(pFrameData->mpNextFrame.get() != pSysWin);
                pSysFrameData->mpNextFrame = pFrameData->mpNextFrame;
            }
        }
        else // If it is not in the list, we can't remove it.
        {
            SAL_WARN("vcl.window", "Window " << this
                                             << " marked as frame window, "
                                                "is missing from list of "
                                             << nWindows << " frames");
        }
    }

    if (mpPlatformState && mpPlatformState->mpFrame) // Otherwise exception during init
    {
        mpPlatformState->mpFrame->SetCallback(nullptr, nullptr);
        if (pSVData->mpDefInst)
            pSVData->mpDefInst->DestroyFrame(mpPlatformState->mpFrame);
    }

    if (pFrameData)
    {
        assert(pFrameData->mnFocusId == nullptr);
        assert(pFrameData->mnMouseMoveId == nullptr);

        pFrameData->mpBuffer.disposeAndClear();
        delete pFrameData;
        if (mpPlatformState)
            mpPlatformState->mpFrameData = nullptr;
    }
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
    if (vcl::Window* pFirstChild = mpHierarchy ? mpHierarchy->getFirstChild() : nullptr)
    {
        OStringBuffer aTempStr
            = "Window (" + lcl_createWindowInfo(this) + ") with live children destroyed: ";
        WindowHierarchy::forEachSibling(pFirstChild, [&aTempStr](const vcl::Window& rChild) {
            aTempStr.append(lcl_createWindowInfo(&rChild));
        });
        OSL_FAIL(aTempStr.getStr());
        Application::Abort(OStringToOUString(aTempStr, RTL_TEXTENCODING_UTF8));
    }

    ImplFrameData* pFrameData = mpPlatformState ? mpPlatformState->mpFrameData : nullptr;
    if (pFrameData)
    {
        OStringBuffer aErrorStr;
        WindowHierarchy::forEachOverlap(pFrameData->mpFirstOverlap, [&](const vcl::Window& rOverlapWin) {
            if (IsAncestorOf(rOverlapWin))
                aErrorStr.append(lcl_createWindowInfo(&rOverlapWin));
        });

        if (!aErrorStr.isEmpty())
        {
            OString aTempStr = "Window (" + lcl_createWindowInfo(this)
                               + ") with live SystemWindows destroyed: " + aErrorStr;
            OSL_FAIL(aTempStr.getStr());
            Application::Abort(OStringToOUString(aTempStr, RTL_TEXTENCODING_UTF8));
        }
    }

    ImplSVData* pSVData = ImplGetSVData();
    if (pSVData)
    {
        OStringBuffer aErrorStr;
        bool bError = false;
        vcl::Window* pTempWin = pSVData->maFrameData.mpFirstFrame;
        while (pTempWin)
        {
            if (IsAncestorOf(*pTempWin))
            {
                bError = true;
                aErrorStr.append(lcl_createWindowInfo(pTempWin));
            }
            WindowPlatformState* pTempPlatformState = pTempWin->ImplGetPlatformState();
            ImplFrameData* pTempFrameData = pTempPlatformState ? pTempPlatformState->mpFrameData : nullptr;
            pTempWin = pTempFrameData ? pTempFrameData->mpNextFrame : nullptr;
        }

        if (bError)
        {
            OString aTempStr = "Window (" + lcl_createWindowInfo(this)
                               + ") with live SystemWindows destroyed: " + aErrorStr;
            OSL_FAIL(aTempStr.getStr());
            Application::Abort(OStringToOUString(aTempStr, RTL_TEXTENCODING_UTF8));
        }
    }

    if (vcl::Window* pFirstOverlap = mpHierarchy ? mpHierarchy->getFirstOverlap() : nullptr)
    {
        OStringBuffer aTempStr
            = "Window (" + lcl_createWindowInfo(this) + ") with live SystemWindows destroyed: ";
        WindowHierarchy::forEachSibling(pFirstOverlap, [&aTempStr](const vcl::Window& rOverlapWin) {
            aTempStr.append(lcl_createWindowInfo(&rOverlapWin));
        });
        OSL_FAIL(aTempStr.getStr());
        Application::Abort(OStringToOUString(aTempStr, RTL_TEXTENCODING_UTF8));
    }

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

void Window::ImplDeregisterTopWindowChild()
{
    if (!mpClassification || !mpClassification->mbFrame)
        return;

    const bool bIsTopWindow = mpWinData && (mpWinData->mnIsTopWindow == 1);
    if (!bIsTopWindow)
        return;

    vcl::Window* pRealParent = mpHierarchy ? mpHierarchy->getRealParent() : nullptr;
    if (!pRealParent)
        return;

    ImplWinData* pParentWinData = pRealParent->ImplGetWinData();
    if (!pParentWinData)
        return;

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
