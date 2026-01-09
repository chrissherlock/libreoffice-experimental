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
#include <tools/gen.hxx>

#include <vcl/font.hxx>
#include <vcl/region.hxx>
#include <vcl/wall.hxx>
#include <i18nlangtag/lang.h>
#include <vcl/rendercontext/State.hxx>
#include <vcl/rendercontext/AntialiasingFlags.hxx>
#include <vcl/rendercontext/DrawModeFlags.hxx>
#include <vcl/rendercontext/RasterOp.hxx>

namespace vcl
{
struct GraphicsState
{
    Color maLineColor;
    bool mbLineColor;
    Color maFillColor;
    bool mbFillColor;
    Point maRefPoint;
    bool mbRefPoint;
    RasterOp meRasterOp;

    Color maTextColor;
    Color maTextLineColor;
    Color maOverlineColor;

    AntialiasingFlags mnAntialiasing;
    DrawModeFlags mnDrawMode;

    vcl::Font maFont;

    vcl::Region maClipRegion;
    bool mbClipRegion;

    // Background State
    Wallpaper maBackground;
    bool mbBackground;

    // Text Layout State
    LanguageType meTextLanguage;
    vcl::text::ComplexTextLayoutFlags mnTextLayoutMode;

    GraphicsState()
        : maLineColor(COL_BLACK)
        , mbLineColor(true)
        , maFillColor(COL_WHITE)
        , mbFillColor(true)
        , mbRefPoint(false)
        , meRasterOp(RasterOp::OverPaint)
        , maTextColor(COL_BLACK)
        , maTextLineColor(COL_TRANSPARENT)
        , maOverlineColor(COL_TRANSPARENT)
        , mnAntialiasing(AntialiasingFlags::NONE)
        , mnDrawMode(DrawModeFlags::Default)
        , maFont()
        , maClipRegion(true)
        , mbClipRegion(false)
        , maBackground()
        , mbBackground(false)
        , meTextLanguage(LANGUAGE_NONE)
        , mnTextLayoutMode(vcl::text::ComplexTextLayoutFlags::Default)
    {
    }
};

} // namespace vcl

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
