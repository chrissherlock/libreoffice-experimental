/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <tools/color.hxx>

#include <vcl/font.hxx>
#include <vcl/ptrstyle.hxx>
#include <vcl/salnativewidgets.hxx>
#include <vcl/vclenum.hxx>

#include <optional>

namespace vcl
{
class Cursor;
}

struct WindowControlAppearance
{
    std::optional<vcl::Font> mpControlFont;
    Color maControlForeground = COL_TRANSPARENT;
    Color maControlBackground = COL_TRANSPARENT;
    ControlPart meNativeBackground = ControlPart::NONE;

    vcl::Cursor* mpCursor = nullptr;
    PointerStyle mePointer = PointerStyle::Arrow;

    bool mbControlForeground = false;
    bool mbControlBackground = false;

    WindowControlAppearance() = default;

    void setPointer(PointerStyle ePointer);

    void hideCursor();
    void showCursor();
};

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
