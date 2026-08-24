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

#include <vcl/window.hxx>
#include <vcl/event.hxx>
#include <vcl/vclevent.hxx>
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
    if (cod.GetCode() >= 0x200 && cod.GetCode() <= 0x219)
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

    Point aPos = GetPosPixel();
    if (ImplGetParent() && !ImplIsOverlapWindow())
        aPos = OutputToScreenPixel(Point(0, 0));

    tools::Rectangle aRect(aPos, GetSizePixel());
    Help::ShowBalloon(this, rHEvt.GetMousePosPixel(), aRect, rStr);
}

void Window::ImplShowQuickHelp(const HelpEvent& rHEvt)
{
    const OUString& rStr = GetQuickHelpText();

    if (rStr.isEmpty() && ImplGetParent() && !ImplIsOverlapWindow())
    {
        ImplGetParent()->RequestHelp(rHEvt);
        return;
    }

    Point aPos = GetPosPixel();
    if (ImplGetParent() && !ImplIsOverlapWindow())
        aPos = OutputToScreenPixel(Point(0, 0));

    tools::Rectangle aRect(aPos, GetSizePixel());
    Help::ShowQuickHelp(this, aRect, rStr, QuickHelpFlags::CtrlText);
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

} /* namespace vcl */

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
