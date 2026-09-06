/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <o3tl/typed_flags_set.hxx>

// GetFocusFlags
// must match constants in css:awt::FocusChangeReason
enum class GetFocusFlags
{
    NONE = 0x0000,
    Tab = 0x0001,
    CURSOR = 0x0002, // avoid name-clash with X11 #define
    Mnemonic = 0x0004,
    F6 = 0x0008,
    Forward = 0x0010,
    Backward = 0x0020,
    Around = 0x0040,
    UniqueMnemonic = 0x0100,
    Init = 0x0200,
    FloatWinPopupModeEndCancel = 0x0400,
};
namespace o3tl
{
template <> struct typed_flags<GetFocusFlags> : is_typed_flags<GetFocusFlags, 0x077f>
{
};
}

enum class ActivateModeFlags
{
    NONE = 0,
    GrabFocus = 0x0001,
};
namespace o3tl
{
template <> struct typed_flags<ActivateModeFlags> : is_typed_flags<ActivateModeFlags, 0x0001>
{
};
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
