/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <vcl/focus.hxx>

class WindowFocusState
{
private:
    ActivateModeFlags meActivateMode = ActivateModeFlags::NONE;
    GetFocusFlags meGetFocusFlags = GetFocusFlags::NONE;
    bool mbActive = false;
    bool mbCompoundControl = false;
    bool mbCompoundControlHasFocus = false;
    bool mbFocusVisible = false;
    bool mbUseNativeFocus = false;
    bool mbInShowFocus = false;
    bool mbNativeFocusVisible = false;
    bool mbInHideFocus = false;

public:
    WindowFocusState() = default;

    // Compound Control
    bool isCompoundControl() const { return mbCompoundControl; }
    void setCompoundControl(bool bCompound) { mbCompoundControl = bCompound; }

    // Compound Control has focus
    bool compoundControlHasFocus() const { return mbCompoundControlHasFocus; }
    void setCompoundControlHasFocus(bool bHasFocus) { mbCompoundControlHasFocus = bHasFocus; }

    void activate() { mbActive = true; }
    void deactivate() { mbActive = false; }

    // FocusVisible
    void makeFocusVisible() { mbFocusVisible = true; }
    void hideFocusVisible() { mbFocusVisible = false; }

    // Active state
    bool isActive() const { return mbActive; }
    void setActive(bool bActive) { mbActive = bActive; }

    // GetFocusFlags
    GetFocusFlags getFocusFlags() const { return meGetFocusFlags; }
    void setFocusFlags(GetFocusFlags eFlags) { meGetFocusFlags = eFlags; }
    void addFocusFlags(GetFocusFlags eFlags) { meGetFocusFlags |= eFlags; }
    void clearFocusFlags() { meGetFocusFlags = GetFocusFlags::NONE; }

    bool isFocusVisible() const { return mbFocusVisible; }

    // ActivateMode
    ActivateModeFlags getActivateMode() const { return meActivateMode; }

    bool canGrabFocusOnActivate() const
    {
        return bool(meActivateMode & ActivateModeFlags::GrabFocus);
    }

    /**
     * Updates activate mode.
     * @return true if the activate mode actually changed.
     */
    bool setActivateMode(ActivateModeFlags eMode)
    {
        if (meActivateMode == eMode)
            return false;

        meActivateMode = eMode;

        return true;
    }

    // UseNativeFocus
    bool usesNativeFocus() const { return mbUseNativeFocus; }
    void useNativeFocus() { mbUseNativeFocus = true; }
    void clearNativeFocus() { mbUseNativeFocus = false; }

    // InShowFocus
    bool isInShowFocus() const { return mbInShowFocus; }
    void enterShowFocus() { mbInShowFocus = true; }
    void leaveShowFocus() { mbInShowFocus = false; }

    // NativeFocusVisible
    bool isNativeFocusVisible() const { return mbNativeFocusVisible; }
    void makeNativeFocusVisible() { mbNativeFocusVisible = true; }
    void hideNativeFocusVisible() { mbNativeFocusVisible = false; }

    // HideFocus
    bool isInHideFocus() const { return mbInHideFocus; }
    void enterHideFocus() { mbInHideFocus = true; }
    void leaveHideFocus() { mbInHideFocus = false; }
};

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
