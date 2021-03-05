/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
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

#include <tools/debug.hxx>

#include <vcl/RenderContext2.hxx>
#include <vcl/settings.hxx>
#include <vcl/virdev.hxx>

#include <drawmode.hxx>
#include <salgdi.hxx>

bool RenderContext2::IsLineColor() const { return mbLineColor; }

Color const& RenderContext2::GetLineColor() const { return maLineColor; }

void RenderContext2::SetLineColor()
{
    if (mbLineColor)
    {
        mbInitLineColor = true;
        mbLineColor = false;
        maLineColor = COL_TRANSPARENT;
    }

    if (mpAlphaVDev)
        mpAlphaVDev->SetLineColor();
}

void RenderContext2::SetLineColor(const Color& rColor)
{
    Color aColor;

    if (rColor.IsTransparent())
        aColor = rColor;
    else
        aColor = GetDrawModeLineColor(rColor, GetDrawMode(), GetSettings().GetStyleSettings());

    if (aColor.IsTransparent())
    {
        if (mbLineColor)
        {
            mbInitLineColor = true;
            mbLineColor = false;
            maLineColor = COL_TRANSPARENT;
        }
    }
    else
    {
        if (maLineColor != aColor)
        {
            mbInitLineColor = true;
            mbLineColor = true;
            maLineColor = aColor;
        }
    }

    if (mpAlphaVDev)
        mpAlphaVDev->SetLineColor(COL_BLACK);
}

void RenderContext2::InitLineColor()
{
    DBG_TESTSOLARMUTEX();

    if (mbLineColor)
    {
        if (RasterOp::N0 == meRasterOp)
            mpGraphics->SetROPLineColor(SalROPColor::N0);
        else if (RasterOp::N1 == meRasterOp)
            mpGraphics->SetROPLineColor(SalROPColor::N1);
        else if (RasterOp::Invert == meRasterOp)
            mpGraphics->SetROPLineColor(SalROPColor::Invert);
        else
            mpGraphics->SetLineColor(maLineColor);
    }
    else
        mpGraphics->SetLineColor();

    mbInitLineColor = false;
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
