/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <vcl/cursor.hxx>

#include <window/WindowControlAppearance.hxx>

void WindowControlAppearance::useNativeDialogBackground()
{
    meNativeBackground = ControlPart::BackgroundDialog;
}

void WindowControlAppearance::useEntireNativeBackground()
{
    meNativeBackground = ControlPart::Entire;
}

void WindowControlAppearance::updateCursor()
{
    if (mpCursor)
        mpCursor->ImplNew();
}

void WindowControlAppearance::hideCursor()
{
    if (mpCursor)
        mpCursor->ImplHide();
}

void WindowControlAppearance::showCursor()
{
    if (mpCursor)
        mpCursor->ImplShow();
}

void WindowControlAppearance::setPointer(PointerStyle ePointer)
{
    if (mePointer == ePointer)
        return;

    mePointer = ePointer;
}

void WindowControlAppearance::setCursor(vcl::Cursor* pCursor)
{
    if (mpCursor == pCursor)
        return;

    hideCursor();
    mpCursor = pCursor;
    showCursor();
}

void WindowControlAppearance::resumeCursor(bool bRestore)
{
    if (mpCursor)
        mpCursor->ImplResume(bRestore);
}

bool WindowControlAppearance::suspendCursor()
{
    if (!mpCursor)
        return false;

    return mpCursor->ImplSuspend();
}

bool WindowControlAppearance::hasControlFont() const { return mpControlFont.has_value(); }

vcl::Font WindowControlAppearance::getControlFont() const
{
    if (mpControlFont)
        return *mpControlFont;

    return vcl::Font();
}

bool WindowControlAppearance::setControlFont()
{
    if (!mpControlFont)
        return false;

    mpControlFont.reset();
    return true; // State changed
}

bool WindowControlAppearance::setControlFont(const vcl::Font& rFont)
{
    if (rFont == vcl::Font())
        return setControlFont();

    if (!mpControlFont)
    {
        *mpControlFont = rFont;
        return true;
    }

    if (*mpControlFont == rFont)
        return false; // No change

    *mpControlFont = rFont;

    return true; // State changed
}

bool WindowControlAppearance::setControlForeground()
{
    if (mbControlForeground)
    {
        maControlForeground = COL_TRANSPARENT;
        mbControlForeground = false;
        return true; // State changed
    }
    return false;
}

bool WindowControlAppearance::setControlForeground(const Color& rColor)
{
    if (rColor.IsTransparent())
        return setControlForeground();

    if (!mbControlForeground || maControlForeground != rColor)
    {
        maControlForeground = rColor;
        mbControlForeground = true;
        return true; // State changed
    }

    return false;
}

bool WindowControlAppearance::setControlBackground()
{
    if (mbControlBackground)
    {
        maControlBackground = COL_TRANSPARENT;
        mbControlBackground = false;
        return true; // State changed
    }
    return false;
}

bool WindowControlAppearance::setControlBackground(const Color& rColor)
{
    if (rColor.IsTransparent())
        return setControlBackground();

    if (!mbControlBackground || maControlBackground != rColor)
    {
        maControlBackground = rColor;
        mbControlBackground = true;
        return true; // State changed
    }

    return false;
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
