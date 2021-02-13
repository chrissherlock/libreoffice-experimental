/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <vcl/RenderContext2.hxx>
#include <vcl/settings.hxx>
#include <vcl/svapp.hxx>

#include <salgdi.hxx>

RenderContext2::RenderContext2()
    : mxSettings(new AllSettings(Application::GetSettings()))
    , maTextLineColor(COL_TRANSPARENT)
    , mnTextLayoutMode(ComplexTextLayoutFlags::Default)
    , meTextLanguage(LANGUAGE_SYSTEM) // TODO: get default from configuration?
    , maRegion(true)
    , maFillColor(COL_WHITE)
    , maOverlineColor(COL_TRANSPARENT)
    , mnDrawMode(DrawModeFlags::Default)
    , meRasterOp(RasterOp::OverPaint)
    , mnAntialiasing(AntialiasingFlags::NONE)
    , mbOpaqueLineColor(true)
    , mbOpaqueFillColor(true)
    , mbInitLineColor(true)
    , mbInitFillColor(true)
    , mbInitTextColor(true)
    , mbInitFont(true)
    , mbNewFont(true)
    , mbInitClipRegion(false)
    , mbClipRegion(false)
    , mbClipRegionSet(false)
    , mbOutput(true)
    , mbDevOutput(false)
    , mbEnableRTL(false)
{
}

void RenderContext2::dispose() { mxPhysicalFontFamilyCollection.reset(); }

sal_uInt16 RenderContext2::GetBitCount() const
{
    // we need a graphics instance
    if (!mpGraphics && !AcquireGraphics())
        return 0;

    return mpGraphics->GetBitCount();
}

void RenderContext2::EnableOutput() { mbOutput = true; }
void RenderContext2::DisableOutput() { mbOutput = false; }
bool RenderContext2::IsOutputEnabled() const { return mbOutput; }
bool RenderContext2::IsDeviceOutputEnabled() const { return mbDevOutput; }
bool RenderContext2::IsDeviceOutputNecessary() const { return (mbOutput && mbDevOutput); }
void RenderContext2::EnableDeviceOutput() { mbDevOutput = true; }
void RenderContext2::DisableDeviceOutput() { mbDevOutput = false; }

bool RenderContext2::IsRTLEnabled() const { return mbEnableRTL; }
void RenderContext2::EnableRTL() { mbEnableRTL = true; }
void RenderContext2::DisableRTL() { mbEnableRTL = false; }

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
