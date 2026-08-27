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

#include <o3tl/float_int_conversion.hxx>

#include <vcl/window.hxx>
#include <vcl/event.hxx>
#include <vcl/layout.hxx>
#include <vcl/vclevent.hxx>
#include <vcl/scrollable.hxx>
#include <vcl/svapp.hxx>

#include <salframe.hxx>
#include <svdata.hxx>
#include <helpwin.hxx>
#include <window.h>

#include "impldockingwrapper.hxx"

namespace vcl
{
void Window::SimulateKeyPress(sal_uInt16 nKeyCode) const
{
    mpWindowImpl->mpFrame->SimulateKeyPress(nKeyCode);
}

void Window::KeyInput(const KeyEvent& rKEvt)
{
#ifndef _WIN32 // On Windows, dialogs react to accelerators  without Alt (tdf#157649)
    KeyCode cod = rKEvt.GetKeyCode();

    // do not respond to accelerators unless Alt or Ctrl is held
    if (cod.GetCode() >= KEY_A && cod.GetCode() <= KEY_Z)
    {
        bool autoacc = ImplGetSVData()->maNWFData.mbAutoAccel;
        if (autoacc && cod.GetModifier() != KEY_MOD2 && !(cod.GetModifier() & KEY_MOD1))
            return;
    }
#endif

    NotifyEvent aNEvt(NotifyEventType::KEYINPUT, this, &rKEvt);
    if (!CompatNotify(aNEvt))
        mpWindowImpl->mbKeyInput = true;
}

void Window::KeyUp(const KeyEvent& rKEvt)
{
    NotifyEvent aNEvt(NotifyEventType::KEYUP, this, &rKEvt);
    if (!CompatNotify(aNEvt))
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
        mpWindowImpl->maHelpRequestHdl = rLink;
}

tools::Rectangle Window::ImplGetHelpScreenRect() const
{
    Point aPos = GetPosPixel();

    if (ImplGetParent() && !ImplIsOverlapWindow())
        aPos = OutputToScreenPixel(Point(0, 0));

    return tools::Rectangle(aPos, GetSizePixel());
}

void Window::ImplShowBalloonHelp(const HelpEvent& rHEvt)
{
    OUString rStr = GetHelpText();

    if (rStr.isEmpty())
        rStr = GetQuickHelpText();

    if (rStr.isEmpty() && ImplGetParent() && !ImplIsOverlapWindow())
    {
        ImplGetParent()->RequestHelp(rHEvt);
        return;
    }

    Help::ShowBalloon(this, rHEvt.GetMousePosPixel(), ImplGetHelpScreenRect(), rStr);
}

void Window::ImplShowQuickHelp(const HelpEvent& rHEvt)
{
    const OUString& rStr = GetQuickHelpText();

    if (rStr.isEmpty() && ImplGetParent() && !ImplIsOverlapWindow())
    {
        ImplGetParent()->RequestHelp(rHEvt);
        return;
    }

    Help::ShowQuickHelp(this, ImplGetHelpScreenRect(), rStr, QuickHelpFlags::CtrlText);
}

void Window::ImplStartHelp(const HelpEvent& rHEvt)
{
    OUString aStrHelpId(GetHelpId());

    if (aStrHelpId.isEmpty() && ImplGetParent())
    {
        ImplGetParent()->RequestHelp(rHEvt);
        return;
    }

    Help* pHelp = Application::GetHelp();

    if (!pHelp)
        return;

    if (!aStrHelpId.isEmpty())
        pHelp->Start(aStrHelpId, this);
    else
        pHelp->Start(u"" OOO_HELP_INDEX ""_ustr, this);
}

void Window::RequestHelp(const HelpEvent& rHEvt)
{
    // if Balloon-Help is requested, show the balloon with help text set
    if (rHEvt.GetMode() & HelpEventMode::BALLOON)
    {
        ImplShowBalloonHelp(rHEvt);
        return;
    }

    if (rHEvt.GetMode() & HelpEventMode::QUICK)
    {
        ImplShowQuickHelp(rHEvt);
        return;
    }

    if (!mpWindowImpl->maHelpRequestHdl.IsSet() || mpWindowImpl->maHelpRequestHdl.Call(*this))
        ImplStartHelp(rHEvt);
}

void Window::Command(const CommandEvent& rCEvt)
{
    if (mpWindowImpl && mpWindowImpl->maCommandHdl.Call(rCEvt))
        return;

    CallEventListeners(VclEventId::WindowCommand, const_cast<CommandEvent*>(&rCEvt));

    NotifyEvent aNEvt(NotifyEventType::COMMAND, this, &rCEvt);
    if (!CompatNotify(aNEvt))
        mpWindowImpl->mbCommand = true;
}

void Window::Tracking(const TrackingEvent& rTEvt)
{
    ImplDockingWindowWrapper* pWrapper = ImplGetDockingManager()->GetDockingWindowWrapper(this);
    if (pWrapper)
        pWrapper->Tracking(rTEvt);
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

void Window::CompatStateChanged(StateChangedType nStateChange)
{
    if (!mpWindowImpl || mpWindowImpl->mbInDispose)
        Window::StateChanged(nStateChange);
    else
        StateChanged(nStateChange);
}

void Window::CompatDataChanged(const DataChangedEvent& rDCEvt)
{
    if (!mpWindowImpl || mpWindowImpl->mbInDispose)
        Window::DataChanged(rDCEvt);
    else
        DataChanged(rDCEvt);
}

bool Window::CompatPreNotify(NotifyEvent& rNEvt)
{
    if (!mpWindowImpl || mpWindowImpl->mbInDispose)
        return Window::PreNotify(rNEvt);
    else
        return PreNotify(rNEvt);
}

bool Window::CompatNotify(NotifyEvent& rNEvt)
{
    if (!mpWindowImpl || mpWindowImpl->mbInDispose)
        return Window::EventNotify(rNEvt);
    else
        return EventNotify(rNEvt);
}

void Window::DataChanged(const DataChangedEvent&) {}

void Window::NotifyAllChildren(DataChangedEvent& rDCEvt)
{
    CompatDataChanged(rDCEvt);

    vcl::Window* pChild = mpWindowImpl->mpHierarchy->mpFirstChild;
    while (pChild)
    {
        pChild->NotifyAllChildren(rDCEvt);
        pChild = pChild->mpWindowImpl->mpHierarchy->mpNext;
    }
}

bool Window::ImplDelegatePreNotifyToParent(NotifyEvent& rNEvt)
{
    if (mpWindowImpl->mpHierarchy->mpParent && !ImplIsOverlapWindow())
        return mpWindowImpl->mpHierarchy->mpParent->CompatPreNotify(rNEvt);

    return false;
}

bool Window::ImplIsCompoundControlGainingFocus() const
{
    return mpWindowImpl->mbCompoundControl && !mpWindowImpl->mbCompoundControlHasFocus
           && HasChildPathFocus();
}

bool Window::ImplIsCompoundControlLosingFocus() const
{
    return mpWindowImpl->mbCompoundControl && mpWindowImpl->mbCompoundControlHasFocus
           && !HasChildPathFocus();
}

bool Window::ImplUpdateCompoundControlFocusGain()
{
    if (ImplIsCompoundControlGainingFocus())
    {
        mpWindowImpl->mbCompoundControlHasFocus = true;
        return true;
    }

    return false;
}

bool Window::ImplUpdateCompoundControlFocusLoss()
{
    if (ImplIsCompoundControlLosingFocus())
    {
        mpWindowImpl->mbCompoundControlHasFocus = false;
        return true;
    }

    return false;
}

void Window::ImplNotifyFocusListeners(NotifyEvent& rNEvt)
{
    if (rNEvt.GetType() == NotifyEventType::GETFOCUS)
    {
        if (ImplUpdateCompoundControlFocusGain() || (rNEvt.GetWindow() == this))
            CallEventListeners(VclEventId::WindowGetFocus);
    }
    else if (rNEvt.GetType() == NotifyEventType::LOSEFOCUS)
    {
        if (ImplUpdateCompoundControlFocusLoss() || (rNEvt.GetWindow() == this))
            CallEventListeners(VclEventId::WindowLoseFocus);
    }
}

bool Window::PreNotify(NotifyEvent& rNEvt)
{
    if (ImplDelegatePreNotifyToParent(rNEvt))
        return true;

    ImplNotifyFocusListeners(rNEvt);

    // #82968# mouse and key events will be notified after processing ( in ImplNotifyKeyMouseCommandEventListeners() )!
    //    see also ImplHandleMouseEvent(), ImplHandleKey()

    return false;
}

static bool lcl_ParentNotDialogControl(Window* pWindow)
{
    vcl::Window* pParent = getNonLayoutParent(pWindow);
    if (!pParent)
        return true;
    return ((pParent->GetStyle() & (WB_DIALOGCONTROL | WB_NODIALOGCONTROL)) != WB_DIALOGCONTROL);
}

static bool lcl_IsKeyEvent(const NotifyEvent& rNEvt)
{
    return rNEvt.GetType() == NotifyEventType::KEYINPUT
           || rNEvt.GetType() == NotifyEventType::KEYUP;
}

static bool lcl_IsFocusEvent(const NotifyEvent& rNEvt)
{
    return rNEvt.GetType() == NotifyEventType::GETFOCUS
           || rNEvt.GetType() == NotifyEventType::LOSEFOCUS;
}

static bool lcl_IsTopLevelDialogControl(Window* pWindow, bool bTopLevelFloatingWindow)
{
    return pWindow->ImplIsOverlapWindow() || lcl_ParentNotDialogControl(pWindow)
           || bTopLevelFloatingWindow;
}

bool Window::ImplShouldForwardFocusToChild(const NotifyEvent& rNEvt) const
{
    return (rNEvt.GetWindow() == this) && (rNEvt.GetType() == NotifyEventType::GETFOCUS)
           && !(GetStyle() & WB_TABSTOP)
           && !(mpWindowImpl->mnDlgCtrlFlags & DialogControlFlags::WantFocus);
}

bool Window::ImplDispatchDialogControlKeyEvent(NotifyEvent& rNEvt, bool bIsFloatingMode)
{
    // ScGridWindow has WB_DIALOGCONTROL set, so pressing tab in ScCheckListMenuControl won't
    // get processed here by the toplevel DockingWindow of ScCheckListMenuControl by
    // just checking if lcl_ParentNotDialogControl is true
    if (lcl_IsTopLevelDialogControl(this, bIsFloatingMode))
        return ImplDlgCtrl(*rNEvt.GetKeyEvent(), rNEvt.GetType() == NotifyEventType::KEYINPUT);

    return false;
}

bool Window::ImplDispatchDialogControlFocusEvent(NotifyEvent& rNEvt)
{
    if (!lcl_IsFocusEvent(rNEvt))
        return false;

    ImplDlgCtrlFocusChanged(rNEvt.GetWindow(), rNEvt.GetType() == NotifyEventType::GETFOCUS);

    if (ImplShouldForwardFocusToChild(rNEvt))
    {
        if (vcl::Window* pFirstChild = ImplGetDlgWindow(0, GetDlgWindowType::First); pFirstChild)
            pFirstChild->ImplControlFocus();
    }

    return false;
}

bool Window::ImplDispatchDialogControlEvent(NotifyEvent& rNEvt, bool bIsFloatingMode)
{
    if ((GetStyle() & (WB_DIALOGCONTROL | WB_NODIALOGCONTROL)) != WB_DIALOGCONTROL)
        return false;

    if (lcl_IsKeyEvent(rNEvt))
        return ImplDispatchDialogControlKeyEvent(rNEvt, bIsFloatingMode);

    return ImplDispatchDialogControlFocusEvent(rNEvt);
}

static bool lcl_CanToggleFloatingMode(Window* pWindow, const ImplDockingWindowWrapper* pWrapper)
{
    return (pWindow->GetStyle() & WB_DOCKABLE) && pWrapper
           && (pWrapper->IsFloatingMode() || !pWrapper->IsLocked());
}

static bool lcl_IsFloatingToggleKey(const NotifyEvent& rNEvt)
{
    if (rNEvt.GetType() != NotifyEventType::KEYINPUT)
        return false;

    const vcl::KeyCode& rKey = rNEvt.GetKeyEvent()->GetKeyCode();

    return rKey.GetCode() == KEY_F10 && rKey.GetModifier() && rKey.IsShift() && rKey.IsMod1();
}

static bool lcl_IsFloatingToggleKeyEvent(const NotifyEvent& rNEvt)
{
    return rNEvt.GetType() == NotifyEventType::KEYINPUT && lcl_IsFloatingToggleKey(rNEvt);
}

static bool lcl_IsSingleClickInDragArea(const MouseEvent* pMEvt,
                                        const ImplDockingWindowWrapper* pWrapper)
{
    return pMEvt->GetClicks() == 1 && pWrapper->IsMouseInDragArea(pMEvt->GetPosPixel());
}

static bool lcl_IsMod1DoubleClick(const MouseEvent* pMEvt)
{
    return pMEvt->IsMod1() && (pMEvt->GetClicks() == 2);
}

bool Window::ImplDispatchDockingMouseEvent(const NotifyEvent& rNEvt,
                                           ImplDockingWindowWrapper* pWrapper)
{
    const MouseEvent* pMEvt = rNEvt.GetMouseEvent();
    if (!pMEvt->IsLeft())
        return false;

    if (StyleSettings::GetDockingFloatsSupported() && lcl_IsMod1DoubleClick(pMEvt))
    {
        pWrapper->SetFloatingMode(!pWrapper->IsFloatingMode());
        return true;
    }

    if (!lcl_IsSingleClickInDragArea(pMEvt, pWrapper))
        return false;

    pWrapper->ImplEnableStartDocking();
    return true;
}

bool Window::ImplCanStartDocking(const MouseEvent* pMEvt,
                                 const ImplDockingWindowWrapper* pWrapper) const
{
    return pWrapper->ImplStartDockingEnabled() && !pWrapper->IsFloatingMode()
           && !pWrapper->IsDocking() && pWrapper->IsMouseInDragArea(pMEvt->GetPosPixel());
}

bool Window::ImplAttemptDockingSequence(const NotifyEvent& rNEvt,
                                        ImplDockingWindowWrapper* pWrapper)
{
    const MouseEvent* pMEvt = rNEvt.GetMouseEvent();

    if (!pMEvt->IsLeft() || !ImplCanStartDocking(pMEvt, pWrapper))
        return false;

    Point aPos = pMEvt->GetPosPixel();

    if (vcl::Window* pWindow = rNEvt.GetWindow(); pWindow != this)
    {
        aPos = pWindow->OutputToScreenPixel(aPos);
        aPos = ScreenToOutputPixel(aPos);
    }

    pWrapper->ImplStartDocking(aPos);

    return true;
}

bool Window::ImplDispatchDockingEvent(const NotifyEvent& rNEvt, ImplDockingWindowWrapper* pWrapper)
{
    if (rNEvt.GetType() == NotifyEventType::MOUSEBUTTONDOWN)
    {
        if (ImplDispatchDockingMouseEvent(rNEvt, pWrapper))
            return true;
    }
    else if (rNEvt.GetType() == NotifyEventType::MOUSEMOVE)
    {
        if (ImplAttemptDockingSequence(rNEvt, pWrapper))
            return true;
    }
    else if (lcl_IsFloatingToggleKeyEvent(rNEvt) && StyleSettings::GetDockingFloatsSupported())
    {
        pWrapper->SetFloatingMode(!pWrapper->IsFloatingMode());

        /* At this point the floating toolbar frame does not have the
         * input focus since these frames don't get the focus per default
         * To enable keyboard handling of this toolbar set the input focus
         * to the frame. This needs to be done with ToTop since GrabFocus
         * would not notice any change since "this" already has the focus.
         */
        if (pWrapper->IsFloatingMode())
            ToTop(ToTopFlags::GrabFocusOnly);

        return true;
    }

    return false;
}

bool Window::EventNotify(NotifyEvent& rNEvt)
{
    if (isDisposed())
        return false;

    // check for docking window
    // but do nothing if window is docked and locked
    ImplDockingWindowWrapper* pWrapper = ImplGetDockingManager()->GetDockingWindowWrapper(this);

    if (lcl_CanToggleFloatingMode(this, pWrapper) && ImplDispatchDockingEvent(rNEvt, pWrapper))
        return true;

    const bool bIsFloatingMode = (pWrapper && pWrapper->IsFloatingMode());

    if (ImplDispatchDialogControlEvent(rNEvt, bIsFloatingMode))
        return true;

    if (!mpWindowImpl->mpHierarchy->mpParent || ImplIsOverlapWindow())
        return false;

    return mpWindowImpl->mpHierarchy->mpParent->CompatNotify(rNEvt);
}

void Window::CallEventListeners(VclEventId nEvent, void* pData)
{
    VclWindowEvent aEvent(this, nEvent, pData);

    VclPtr<vcl::Window> xWindow = this;

    Application::ImplCallEventListeners(aEvent);

    // if we have ObjectDying, then the bIsDisposed flag has already been set,
    // but we still need to let listeners know.
    const bool bIgnoreDisposed = nEvent == VclEventId::ObjectDying;

    if (!bIgnoreDisposed && xWindow->isDisposed())
        return;

    // If maEventListeners is empty, the XVCLWindow has not yet been initialized.
    // Calling GetComponentInterface will do that.
    if (mpWindowImpl->maEventListeners.empty() && pData)
        xWindow->GetComponentInterface();

    if (!mpWindowImpl->maEventListeners.empty())
    {
        // Copy the list, because this can be destroyed when calling a Link...
        std::vector<Link<VclWindowEvent&, void>> aCopy(mpWindowImpl->maEventListeners);
        // we use an iterating counter/flag and a set of deleted Link's to avoid O(n^2) behaviour
        mpWindowImpl->mnEventListenersIteratingCount++;
        auto& rWindowImpl = *mpWindowImpl;
        comphelper::ScopeGuard aGuard([&rWindowImpl, &xWindow, &bIgnoreDisposed]() {
            if (bIgnoreDisposed || !xWindow->isDisposed())
            {
                rWindowImpl.mnEventListenersIteratingCount--;
                if (rWindowImpl.mnEventListenersIteratingCount == 0)
                    rWindowImpl.maEventListenersDeleted.clear();
            }
        });
        for (const Link<VclWindowEvent&, void>& rLink : aCopy)
        {
            if (!bIgnoreDisposed && xWindow->isDisposed())
                break;
            // check this hasn't been removed in some re-enterancy scenario fdo#47368
            if (rWindowImpl.maEventListenersDeleted.find(rLink)
                == rWindowImpl.maEventListenersDeleted.end())
                rLink.Call(aEvent);
        }
    }

    while (xWindow)
    {
        if (!bIgnoreDisposed && xWindow->isDisposed())
            return;

        if (!xWindow->mpWindowImpl)
            break;

        auto& rWindowImpl = *xWindow->mpWindowImpl;
        if (!rWindowImpl.maChildEventListeners.empty())
        {
            // Copy the list, because this can be destroyed when calling a Link...
            std::vector<Link<VclWindowEvent&, void>> aCopy(rWindowImpl.maChildEventListeners);
            // we use an iterating counter/flag and a set of deleted Link's to avoid O(n^2) behaviour
            rWindowImpl.mnChildEventListenersIteratingCount++;
            comphelper::ScopeGuard aGuard([&rWindowImpl, &xWindow, &bIgnoreDisposed]() {
                if (bIgnoreDisposed || !xWindow->isDisposed())
                {
                    rWindowImpl.mnChildEventListenersIteratingCount--;
                    if (rWindowImpl.mnChildEventListenersIteratingCount == 0)
                        rWindowImpl.maChildEventListenersDeleted.clear();
                }
            });
            for (const Link<VclWindowEvent&, void>& rLink : aCopy)
            {
                if (!bIgnoreDisposed && xWindow->isDisposed())
                    return;
                // Check this hasn't been removed in some re-enterancy scenario fdo#47368.
                if (rWindowImpl.maChildEventListenersDeleted.find(rLink)
                    == rWindowImpl.maChildEventListenersDeleted.end())
                    rLink.Call(aEvent);
            }
        }

        if (!bIgnoreDisposed && xWindow->isDisposed())
            return;

        xWindow = xWindow->GetParent();
    }
}

void Window::AddEventListener(const Link<VclWindowEvent&, void>& rEventListener)
{
    mpWindowImpl->maEventListeners.push_back(rEventListener);
}

void Window::RemoveEventListener(const Link<VclWindowEvent&, void>& rEventListener)
{
    if (mpWindowImpl)
    {
        auto& rListeners = mpWindowImpl->maEventListeners;
        std::erase(rListeners, rEventListener);
        if (mpWindowImpl->mnEventListenersIteratingCount)
            mpWindowImpl->maEventListenersDeleted.insert(rEventListener);
    }
}

void Window::AddChildEventListener(const Link<VclWindowEvent&, void>& rEventListener)
{
    mpWindowImpl->maChildEventListeners.push_back(rEventListener);
}

void Window::RemoveChildEventListener(const Link<VclWindowEvent&, void>& rEventListener)
{
    if (mpWindowImpl)
    {
        auto& rListeners = mpWindowImpl->maChildEventListeners;
        std::erase(rListeners, rEventListener);
        if (mpWindowImpl->mnChildEventListenersIteratingCount)
            mpWindowImpl->maChildEventListenersDeleted.insert(rEventListener);
    }
}

ImplSVEvent* Window::PostUserEvent(const Link<void*, void>& rLink, void* pCaller,
                                   bool bReferenceLink)
{
    std::unique_ptr<ImplSVEvent> pSVEvent(new ImplSVEvent);
    pSVEvent->mpData = pCaller;
    pSVEvent->maLink = rLink;
    pSVEvent->mpWindow = this;
    pSVEvent->mbCall = true;

    if (bReferenceLink)
        pSVEvent->mpInstanceRef = static_cast<vcl::Window*>(rLink.GetInstance());

    auto pTmpEvent = pSVEvent.get();

    if (!mpWindowImpl->mpFrame->PostEvent(std::move(pSVEvent)))
        return nullptr;

    return pTmpEvent;
}

void Window::RemoveUserEvent(ImplSVEvent* nUserEvent)
{
    SAL_WARN_IF(
        nUserEvent->mpWindow.get() != this, "vcl",
        "Window::RemoveUserEvent(): Event doesn't send to this window or is already removed");
    SAL_WARN_IF(!nUserEvent->mbCall, "vcl", "Window::RemoveUserEvent(): Event is already removed");

    if (nUserEvent->mpWindow)
        nUserEvent->mpWindow = nullptr;

    nUserEvent->mbCall = false;
}

static MouseEvent ImplTranslateMouseEvent(const MouseEvent& rE, vcl::Window const* pSource,
                                          vcl::Window const* pDest)
{
    // the mouse event occurred in a different window, we need to translate the coordinates of
    // the mouse cursor within that (source) window to the coordinates the mouse cursor would
    // be in the destination window
    Point aPos = pSource->OutputToScreenPixel(rE.GetPosPixel());
    return MouseEvent(pDest->ScreenToOutputPixel(aPos), rE.GetClicks(), rE.GetMode(),
                      rE.GetButtons(), rE.GetModifier());
}

void Window::ImplDispatchCompoundControlCommand(const NotifyEvent& rNEvt, const CommandEvent* pCEvt)
{
    if (!mpWindowImpl->mbCompoundControl || rNEvt.GetWindow() == this)
        return;

    CommandEvent aCommandEvent;

    if (!pCEvt->IsMouseEvent())
    {
        aCommandEvent = *pCEvt;
    }
    else
    {
        // the mouse event occurred in a different window, we need to translate the coordinates of
        // the mouse cursor within that window to the coordinates the mouse cursor would be in the
        // current window
        vcl::Window* pSource = rNEvt.GetWindow();
        Point aPos = pSource->OutputToScreenPixel(pCEvt->GetMousePosPixel());
        aCommandEvent = CommandEvent(ScreenToOutputPixel(aPos), pCEvt->GetCommand(),
                                     pCEvt->IsMouseEvent(), pCEvt->GetEventData());
    }

    CallEventListeners(VclEventId::WindowCommand, &aCommandEvent);
}

bool Window::ImplDispatchCommandEvent(const NotifyEvent& rNEvt)
{
    if (const CommandEvent* pCEvt = rNEvt.GetCommandEvent();
        pCEvt->GetCommand() == CommandEventId::ContextMenu)
    {
        ImplDispatchCompoundControlCommand(rNEvt, pCEvt);
        return true;
    }

    // non context menu events are not to be notified up the chain
    // so we return immediately
    return false;
}

static VclEventId lcl_MapMouseEventId(NotifyEventType eType)
{
    switch (eType)
    {
        case NotifyEventType::MOUSEMOVE:
            return VclEventId::WindowMouseMove;
        case NotifyEventType::MOUSEBUTTONUP:
            return VclEventId::WindowMouseButtonUp;
        default:
            return VclEventId::WindowMouseButtonDown;
    }
}

bool Window::ImplDispatchKeyMouseEvent(const NotifyEvent& rNEvt)
{
    VclPtr<vcl::Window> xWindow = this;

    if (mpWindowImpl->mbCompoundControl || (rNEvt.GetWindow() == this))
    {
        switch (rNEvt.GetType())
        {
            case NotifyEventType::MOUSEMOVE:
            case NotifyEventType::MOUSEBUTTONUP:
            case NotifyEventType::MOUSEBUTTONDOWN:
            {
                VclEventId nEventId = lcl_MapMouseEventId(rNEvt.GetType());

                if (rNEvt.GetWindow() == this)
                {
                    CallEventListeners(nEventId, const_cast<MouseEvent*>(rNEvt.GetMouseEvent()));
                }
                else
                {
                    MouseEvent aMouseEvent
                        = ImplTranslateMouseEvent(*rNEvt.GetMouseEvent(), rNEvt.GetWindow(), this);
                    CallEventListeners(nEventId, &aMouseEvent);
                }

                break;
            }

            case NotifyEventType::KEYINPUT:
                CallEventListeners(VclEventId::WindowKeyInput,
                                   const_cast<KeyEvent*>(rNEvt.GetKeyEvent()));
                break;

            case NotifyEventType::KEYUP:
                CallEventListeners(VclEventId::WindowKeyUp,
                                   const_cast<KeyEvent*>(rNEvt.GetKeyEvent()));
                break;

            default:
                break;
        }
    }

    // Return true if the window is still alive, false if we should abort
    return !xWindow->isDisposed();
}

void Window::ImplNotifyKeyMouseCommandEventListeners(NotifyEvent& rNEvt)
{
    if (rNEvt.GetType() == NotifyEventType::COMMAND && !ImplDispatchCommandEvent(rNEvt))
        return;

    // #82968# notify event listeners for mouse and key events separately and
    // not in PreNotify ( as for focus listeners )
    // this allows for processing those events internally first and pass it to
    // the toolkit later
    if (!ImplDispatchKeyMouseEvent(rNEvt))
        return;

    // #106721# check if we're part of a compound control and notify
    vcl::Window* pParent = ImplGetParent();
    while (pParent)
    {
        if (pParent->IsCompoundControl())
        {
            pParent->ImplNotifyKeyMouseCommandEventListeners(rNEvt);
            break;
        }
        pParent = pParent->ImplGetParent();
    }
}

void Window::ImplCallInitShow()
{
    mpWindowImpl->mbReallyShown = true;
    mpWindowImpl->mbInInitShow = true;
    CompatStateChanged(StateChangedType::InitShow);
    mpWindowImpl->mbInInitShow = false;

    vcl::Window* pWindow = mpWindowImpl->mpHierarchy->mpFirstOverlap;
    while (pWindow)
    {
        if (pWindow->mpWindowImpl->mbVisible)
            pWindow->ImplCallInitShow();
        pWindow = pWindow->mpWindowImpl->mpHierarchy->mpNext;
    }

    pWindow = mpWindowImpl->mpHierarchy->mpFirstChild;
    while (pWindow)
    {
        if (pWindow->mpWindowImpl->mbVisible)
            pWindow->ImplCallInitShow();
        pWindow = pWindow->mpWindowImpl->mpHierarchy->mpNext;
    }
}

void Window::ImplCallResize()
{
    mpWindowImpl->mbCallResize = false;

    // Normally we avoid blanking on re-size unless people might notice:
    if (GetBackground().IsGradient())
        Invalidate();

    Resize();

    // #88419# Most classes don't call the base class in Resize() and Move(),
    // => Call ImpleResize/Move instead of Resize/Move directly...
    CallEventListeners(VclEventId::WindowResize);
}

SalFrame* Window::ImplFindParentFrame() const
{
    vcl::Window* pParent = ImplGetParent();
    while (pParent)
    {
        if (pParent->mpWindowImpl && pParent->mpWindowImpl->mpFrame != mpWindowImpl->mpFrame)
            return pParent->mpWindowImpl->mpFrame;

        pParent = pParent->GetParent();
    }

    return nullptr;
}

void Window::ImplUpdateFramePos(SalFrame* pParentFrame)
{
    SalFrameGeometry g = mpWindowImpl->mpFrame->GetGeometry();
    mpWindowImpl->maPos = Point(g.x(), g.y());

    if (pParentFrame)
    {
        g = pParentFrame->GetGeometry();
        mpWindowImpl->maPos -= Point(g.x(), g.y());
    }
}

void Window::ImplUpdateFramePosition()
{
    if (!mpWindowImpl->mbFrame)
        return;

    SalFrame* pParentFrame = ImplFindParentFrame();

    ImplUpdateFramePos(pParentFrame);

    // the client window and all its subclients have the same position as the borderframe
    // this is important for floating toolbars where the borderwindow is a floating window
    // which has another borderwindow (ie the system floating window)
    vcl::Window* pClientWin = mpWindowImpl->mpClientWindow;
    while (pClientWin)
    {
        pClientWin->mpWindowImpl->maPos = mpWindowImpl->maPos;
        pClientWin = pClientWin->mpWindowImpl->mpClientWindow;
    }
}

void Window::ImplCallMove()
{
    mpWindowImpl->mbCallMove = false;

    ImplUpdateFramePosition();

    Move();

    CallEventListeners(VclEventId::WindowMove);
}

void Window::ImplCallFocusChangeActivate(vcl::Window* pNewOverlapWindow,
                                         vcl::Window* pOldOverlapWindow)
{
    ImplSVData* pSVData = ImplGetSVData();
    vcl::Window* pNewRealWindow;
    vcl::Window* pOldRealWindow;
    bool bCallActivate = true;
    bool bCallDeactivate = true;

    if (!pOldOverlapWindow)
    {
        return;
    }

    pOldRealWindow = pOldOverlapWindow->ImplGetWindow();
    if (!pNewOverlapWindow)
    {
        return;
    }

    pNewRealWindow = pNewOverlapWindow->ImplGetWindow();
    if ((pOldRealWindow->GetType() != WindowType::FLOATINGWINDOW)
        || pOldRealWindow->GetActivateMode() != ActivateModeFlags::NONE)
    {
        if ((pNewRealWindow->GetType() == WindowType::FLOATINGWINDOW)
            && pNewRealWindow->GetActivateMode() == ActivateModeFlags::NONE)
        {
            pSVData->mpWinData->mpLastDeacWin = pOldOverlapWindow;
            bCallDeactivate = false;
        }
    }
    else if ((pNewRealWindow->GetType() != WindowType::FLOATINGWINDOW)
             || pNewRealWindow->GetActivateMode() != ActivateModeFlags::NONE)
    {
        if (pSVData->mpWinData->mpLastDeacWin)
        {
            if (pSVData->mpWinData->mpLastDeacWin.get() == pNewOverlapWindow)
                bCallActivate = false;
            else
            {
                vcl::Window* pLastRealWindow = pSVData->mpWinData->mpLastDeacWin->ImplGetWindow();
                pSVData->mpWinData->mpLastDeacWin->mpWindowImpl->mbActive = false;
                pSVData->mpWinData->mpLastDeacWin->Deactivate();
                if (pLastRealWindow != pSVData->mpWinData->mpLastDeacWin.get())
                {
                    pLastRealWindow->mpWindowImpl->mbActive = true;
                    pLastRealWindow->Activate();
                }
            }
            pSVData->mpWinData->mpLastDeacWin = nullptr;
        }
    }

    if (bCallDeactivate)
    {
        if (pOldOverlapWindow->mpWindowImpl->mbActive)
        {
            pOldOverlapWindow->mpWindowImpl->mbActive = false;
            pOldOverlapWindow->Deactivate();
        }
        if (pOldRealWindow != pOldOverlapWindow)
        {
            if (pOldRealWindow->mpWindowImpl->mbActive)
            {
                pOldRealWindow->mpWindowImpl->mbActive = false;
                pOldRealWindow->Deactivate();
            }
        }
    }
    if (!bCallActivate || pNewOverlapWindow->mpWindowImpl->mbActive)
        return;

    pNewOverlapWindow->mpWindowImpl->mbActive = true;
    pNewOverlapWindow->Activate();

    if (pNewRealWindow != pNewOverlapWindow)
    {
        if (!pNewRealWindow->mpWindowImpl->mbActive)
        {
            pNewRealWindow->mpWindowImpl->mbActive = true;
            pNewRealWindow->Activate();
        }
    }
}

// returns how much was actually scrolled (so that abs(retval) <= abs(nN))
static double lcl_HandleScrollHelper(Scrollable* pScrl, double nN, bool isMultiplyByLineSize)
{
    if (!pScrl || !nN || pScrl->Inactive())
        return 0.0;

    tools::Long nNewPos = pScrl->GetThumbPos();
    double scrolled = nN;

    if (nN == double(-LONG_MAX))
        nNewPos += pScrl->GetPageSize();
    else if (nN == double(LONG_MAX))
        nNewPos -= pScrl->GetPageSize();
    else
    {
        // allowing both chunked and continuous scrolling
        if (isMultiplyByLineSize)
        {
            nN *= pScrl->GetLineSize();
        }

        // compute how many quantized units to scroll
        tools::Long magnitude = o3tl::saturating_cast<tools::Long>(fabs(nN));
        tools::Long change = copysign(magnitude, nN);

        nNewPos = nNewPos - change;

        scrolled = double(change);
        // convert back to chunked/continuous
        if (isMultiplyByLineSize)
        {
            scrolled /= pScrl->GetLineSize();
        }
    }

    pScrl->DoScroll(nNewPos);

    return scrolled;
}

bool Window::HandleScrollCommand(const CommandEvent& rCmd, Scrollable* pHScrl, Scrollable* pVScrl)
{
    bool bRet = false;

    if (pHScrl || pVScrl)
    {
        switch (rCmd.GetCommand())
        {
            case CommandEventId::StartAutoScroll:
            {
                StartAutoScrollFlags nFlags = StartAutoScrollFlags::NONE;
                if (pHScrl)
                {
                    if ((pHScrl->GetVisibleSize() < pHScrl->GetRangeMax()) && !pHScrl->Inactive())
                        nFlags |= StartAutoScrollFlags::Horz;
                }
                if (pVScrl)
                {
                    if ((pVScrl->GetVisibleSize() < pVScrl->GetRangeMax()) && !pVScrl->Inactive())
                        nFlags |= StartAutoScrollFlags::Vert;
                }

                if (nFlags != StartAutoScrollFlags::NONE)
                {
                    StartAutoScroll(nFlags);
                    bRet = true;
                }
            }
            break;

            case CommandEventId::Wheel:
            {
                const CommandWheelData* pData = rCmd.GetWheelData();

                if (pData && (CommandWheelMode::SCROLL == pData->GetMode()))
                {
                    if (!pData->IsDeltaPixel())
                    {
                        double nScrollLines = pData->GetScrollLines();
                        double nLines;
                        double* partialScroll = pData->IsHorz() ? &mpWindowImpl->mfPartialScrollX
                                                                : &mpWindowImpl->mfPartialScrollY;
                        if (nScrollLines == COMMAND_WHEEL_PAGESCROLL)
                        {
                            if (pData->GetDelta() < 0)
                                nLines = double(-LONG_MAX);
                            else
                                nLines = double(LONG_MAX);
                        }
                        else
                            nLines = *partialScroll + pData->GetNotchDelta() * nScrollLines;
                        if (nLines)
                        {
                            Scrollable* pScrl = pData->IsHorz() ? pHScrl : pVScrl;
                            double scrolled = lcl_HandleScrollHelper(pScrl, nLines, true);
                            *partialScroll = nLines - scrolled;
                            bRet = true;
                        }
                    }
                    else
                    {
                        // Mobile / touch scrolling section
                        const Point& deltaPoint = rCmd.GetMousePosPixel();

                        double deltaXInPixels = double(deltaPoint.X());
                        double deltaYInPixels = double(deltaPoint.Y());
                        Size winSize = GetOutputSizePixel();

                        if (pHScrl)
                        {
                            double visSizeX = double(pHScrl->GetVisibleSize());
                            double ratioX = deltaXInPixels / double(winSize.getWidth());
                            tools::Long deltaXInLogic = tools::Long(visSizeX * ratioX);
                            // Touch need to work by pixels. Did not apply this to
                            // Android, as android code may require adaptations
                            // to work with this scrolling code
#ifndef IOS
                            tools::Long lineSizeX = pHScrl->GetLineSize();

                            if (lineSizeX)
                            {
                                deltaXInLogic /= lineSizeX;
                            }
                            else
                            {
                                deltaXInLogic = 0;
                            }
#endif
                            if (deltaXInLogic)
                            {
#ifndef IOS
                                bool const isMultiplyByLineSize = true;
#else
                                bool const isMultiplyByLineSize = false;
#endif
                                lcl_HandleScrollHelper(pHScrl, deltaXInLogic, isMultiplyByLineSize);
                                bRet = true;
                            }
                        }
                        if (pVScrl)
                        {
                            double visSizeY = double(pVScrl->GetVisibleSize());
                            double ratioY = deltaYInPixels / double(winSize.getHeight());
                            tools::Long deltaYInLogic = tools::Long(visSizeY * ratioY);

                            // Touch need to work by pixels. Did not apply this to
                            // Android, as android code may require adaptations
                            // to work with this scrolling code
#ifndef IOS
                            tools::Long lineSizeY = pVScrl->GetLineSize();
                            if (lineSizeY)
                            {
                                deltaYInLogic /= lineSizeY;
                            }
                            else
                            {
                                deltaYInLogic = 0;
                            }
#endif
                            if (deltaYInLogic)
                            {
#ifndef IOS
                                bool const isMultiplyByLineSize = true;
#else
                                bool const isMultiplyByLineSize = false;
#endif
                                lcl_HandleScrollHelper(pVScrl, deltaYInLogic, isMultiplyByLineSize);

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
                if (pData && (pData->GetDeltaX() || pData->GetDeltaY()))
                {
                    ImplHandleScroll(pHScrl, pData->GetDeltaX(), pVScrl, pData->GetDeltaY());
                    bRet = true;
                }
            }
            break;

            default:
                break;
        }
    }

    return bRet;
}

void Window::ImplHandleScroll(Scrollable* pHScrl, double nX, Scrollable* pVScrl, double nY)
{
    lcl_HandleScrollHelper(pHScrl, nX, true);
    lcl_HandleScrollHelper(pVScrl, nY, true);
}

} /* namespace vcl */

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
