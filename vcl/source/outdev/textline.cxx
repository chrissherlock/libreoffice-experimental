/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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

#include <sal/types.h>
#include <tools/helpers.hxx>

#include <vcl/gdimtf.hxx>
#include <vcl/metaact.hxx>
#include <vcl/outdev.hxx>
#include <vcl/settings.hxx>
#include <vcl/virdev.hxx>

#include <drawmode.hxx>
#include <salgdi.hxx>
#include <impglyphitem.hxx>

#include <cassert>

void OutputDevice::SetTextLineColor()
{
    if (mpMetaFile)
        mpMetaFile->AddAction(new MetaTextLineColorAction(Color(), false));

    RenderContext2::SetTextLineColor();
}

void OutputDevice::SetTextLineColor(const Color& rColor)
{
    if (mpMetaFile)
        mpMetaFile->AddAction(new MetaTextLineColorAction(
            GetDrawModeTextColor(rColor, GetDrawMode(), GetSettings().GetStyleSettings()), true));

    RenderContext2::SetTextLineColor(rColor);
}

void OutputDevice::SetOverlineColor()
{
    if (mpMetaFile)
        mpMetaFile->AddAction(new MetaOverlineColorAction(Color(), false));

    RenderContext2::SetOverlineColor();
}

void OutputDevice::SetOverlineColor(Color const& rColor)
{
    if (mpMetaFile)
        mpMetaFile->AddAction(new MetaOverlineColorAction(
            GetDrawModeTextColor(rColor, GetDrawMode(), GetSettings().GetStyleSettings()), true));

    RenderContext2::SetOverlineColor(rColor);
}

void OutputDevice::DrawTextLine(const Point& rPos, tools::Long nWidth, FontStrikeout eStrikeout,
                                FontLineStyle eUnderline, FontLineStyle eOverline,
                                bool bUnderlineAbove)
{
    assert(!is_double_buffered_window());

    if (ImplIsRecordLayout())
        return;

    if (mpMetaFile)
    {
        mpMetaFile->AddAction(
            new MetaTextLineAction(rPos, nWidth, eStrikeout, eUnderline, eOverline));
    }

    RenderContext2::DrawTextLine(rPos, nWidth, eStrikeout, eUnderline, eOverline, bUnderlineAbove);
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
