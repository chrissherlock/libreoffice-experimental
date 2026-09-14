/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <vcl/dllapi.h>
#include <vcl/inputctx.hxx>
#include <vcl/toolkit/button.hxx>
#include <vcl/vclptr.hxx>
#include <vcl/wintypes.hxx>

namespace vcl
{
class Window;
}

class PushButton;

enum AlwaysInputMode
{
    AlwaysInputNone = 0,
    AlwaysInputEnabled = 1
};

class WindowInput
{
public:
    WindowInput();
    ~WindowInput();

    bool isMouseOver() const { return mbMouseOver; }
    void setMouseOver(bool bOver) { mbMouseOver = bOver; }
    void clearMouseButtonDown() { mbMouseButtonDown = false; }
    void clearMouseButtonUp() { mbMouseButtonUp = false; }
    void setMouseButtonDown() { mbMouseButtonDown = true; }
    void setMouseButtonUp() { mbMouseButtonUp = true; }
    bool isMouseButtonDown() const { return mbMouseButtonDown; }
    bool isMouseButtonUp() const { return mbMouseButtonUp; }

    bool hasKeyInput() const { return mbKeyInput; }
    void setKeyInput() { mbKeyInput = true; }
    void clearKeyInput() { mbKeyInput = false; }

    bool hasKeyUp() const { return mbKeyUp; }
    void setKeyUp() { mbKeyUp = true; }
    void clearKeyUp() { mbKeyUp = false; }

    bool hasCommand() const { return mbCommand; }
    void setCommand() { mbCommand = true; }
    void clearCommand() { mbCommand = false; }

    PushButton* getDialogControlDownWindow() const { return mpDlgCtrlDownWindow.get(); }
    void setDialogControlDownWindow(PushButton* pBtn) { mpDlgCtrlDownWindow = pBtn; }
    void clearDialogControlDownWindow() { mpDlgCtrlDownWindow = nullptr; }

    void enable() { mbInputDisabled = false; }
    void disable() { mbInputDisabled = true; }
    bool isEnabled() const { return !mbInputDisabled; }
    bool isDisabled() const { return mbInputDisabled; }

    void enableWindow() { mbDisabled = false; }
    void disableWindow() { mbDisabled = true; }
    bool isWindowEnabled() const { return !mbDisabled; }
    bool isWindowDisabled() const { return mbDisabled; }

    bool isAlwaysInputEnabled() const { return meAlwaysInputMode == AlwaysInputEnabled; }
    void setAlwaysInput(bool bAlways)
    {
        meAlwaysInputMode = bAlways ? AlwaysInputEnabled : AlwaysInputNone;
    }

    bool hasFakeFocus() const { return mbFakeFocusSet; }
    void setFakeFocus(bool bFocus) { mbFakeFocusSet = bFocus; }

    bool isMouseTransparent() const { return mbMouseTransparent; }
    void makeMouseTransparent() { mbMouseTransparent = true; }
    void makeMouseOpaque() { mbMouseTransparent = false; }

    vcl::Window* getLastFocusWindow() const { return mpLastFocusWindow; }
    void setLastFocusWindow(vcl::Window* pWin) { mpLastFocusWindow = pWin; }
    void clearLastFocusWindow() { mpLastFocusWindow = nullptr; }
    bool hasLastFocusWindow() const { return mpLastFocusWindow != nullptr; }

    void grabFocusToLastWindow()
    {
        if (mpLastFocusWindow)
            mpLastFocusWindow->GrabFocus();
    }

    bool isInFocusHdl() const { return mbInFocusHdl; }
    void setInFocusHdl(bool bInFocus) { mbInFocusHdl = bInFocus; }

    const InputContext& getInputContext() const { return maInputContext; }
    void setInputContext(const InputContext& rContext) { maInputContext = rContext; }

    bool hasExtendedTextInput() const { return mbExtTextInput; }
    void setExtendedTextInput(bool bActive) { mbExtTextInput = bActive; }

    void inheritFrom(const WindowInput& rParentInput)
    {
        mbDisabled = rParentInput.mbDisabled;
        mbInputDisabled = rParentInput.mbInputDisabled;
        meAlwaysInputMode = rParentInput.meAlwaysInputMode;
    }

private:
    VclPtr<vcl::Window> mpLastFocusWindow;
    VclPtr<PushButton> mpDlgCtrlDownWindow;
    InputContext maInputContext;
    AlwaysInputMode meAlwaysInputMode = AlwaysInputNone;

    bool mbKeyInput : 1 = false;
    bool mbKeyUp : 1 = false;
    bool mbMouseButtonDown : 1 = false;
    bool mbMouseButtonUp : 1 = false;
    bool mbMouseOver : 1 = false; //< tracks mouse over for native widget paint effect
    bool mbCommand : 1 = false;
    bool mbExtTextInput : 1 = false;
    bool mbInFocusHdl : 1 = false;
    bool mbFakeFocusSet : 1 = false;
    bool mbDisabled : 1 = false;
    bool mbInputDisabled : 1 = false;
    bool mbMouseTransparent : 1 = false;
};

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
