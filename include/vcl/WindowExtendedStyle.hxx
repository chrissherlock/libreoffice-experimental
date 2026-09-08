/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <o3tl/typed_flags_set.hxx>

enum class WindowExtendedStyle
{
    NONE = 0x0000,
    Document = 0x0001,
    DocModified = 0x0002,
    /**
     * This is a frame window that is requested to be hidden (not just "not yet
     * shown").
     */
    DocHidden = 0x0004,
};
namespace o3tl
{
template <> struct typed_flags<WindowExtendedStyle> : is_typed_flags<WindowExtendedStyle, 0x0007>
{
};
};

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
