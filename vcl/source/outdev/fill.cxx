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
#include <tools/color.hxx>

#include <vcl/metafile/MetaAction.hxx>
#include <vcl/settings.hxx>
#include <vcl/virdev.hxx>

#include <GraphicsState.hxx>
#include <drawmode.hxx>
#include <salgdi.hxx>

const Color& OutputDevice::GetFillColor() const
{
    return mpGraphicsState->maFillColor;
}

bool OutputDevice::IsFillColor() const
{
    return mpGraphicsState->mbFillColor;
}

void OutputDevice::SetFillColor()
{
    if ( mpMetaFile )
        mpMetaFile->AddAction( new MetaFillColorAction( Color(), false ) );

    if (mpGraphicsState->mbFillColor)
    {
        mbFillColorDirty = true;
        mpGraphicsState->mbFillColor = false;
        mpGraphicsState->maFillColor = COL_TRANSPARENT;
    }
}

void OutputDevice::SetFillColor( const Color& rColor )
{
    Color aColor(vcl::drawmode::GetFillColor(rColor, GetDrawMode(), GetSettings().GetStyleSettings()));

    if ( mpMetaFile )
        mpMetaFile->AddAction( new MetaFillColorAction( aColor, true ) );

    if (mpGraphicsState->maFillColor != aColor)
    {
        mbFillColorDirty = true;
        mpGraphicsState->mbFillColor = true;
        mpGraphicsState->maFillColor = aColor;
    }
}

void OutputDevice::InitFillColor()
{
    DBG_TESTSOLARMUTEX();

    if (mpGraphicsState->mbFillColor)
    {
        if( RasterOp::N0 == mpGraphicsState->meRasterOp )
            mpGraphics->SetROPFillColor( SalROPColor::N0 );
        else if( RasterOp::N1 == mpGraphicsState->meRasterOp )
            mpGraphics->SetROPFillColor( SalROPColor::N1 );
        else if( RasterOp::Invert == mpGraphicsState->meRasterOp )
            mpGraphics->SetROPFillColor( SalROPColor::Invert );
        else
            mpGraphics->SetFillColor(mpGraphicsState->maFillColor);
    }
    else
    {
        mpGraphics->SetFillColor();
    }

    mbFillColorDirty = false;
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
