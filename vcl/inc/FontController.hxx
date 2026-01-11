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
#include <vcl/vclenum.hxx>

class ImplFontEntry;
class ImplFontList;

namespace vcl::font
{
class FontController
{
public:
    ImplFontEntry* mpFontEntry;
    ImplFontList* mpFontList;
    TextAlign meTextAlign;

    FontController()
        : mpFontEntry(nullptr)
        , mpFontList(nullptr)
        , meTextAlign(ALIGN_TOP)
    {
    }
};

} // namespace vcl

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
