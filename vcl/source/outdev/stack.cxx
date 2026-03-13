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

#include <sal/config.h>
#include <sal/log.hxx>
#include <osl/diagnose.h>
#include <tools/debug.hxx>
#include <comphelper/scopeguard.hxx>

#include <vcl/metafile/MetafileRecorder.hxx>
#include <vcl/rendercontext/State.hxx>
#include <vcl/virdev.hxx>
#include <vcl/settings.hxx>

#include <ClippingController.hxx>
#include <CoordinateMapper.hxx>
#include <GraphicsState.hxx>
#include <font/FontController.hxx>
#include <drawmode.hxx>
#include <salgdi.hxx>

bool OutputDevice::PrepareGraphicsOutput(vcl::PrepareOutputFlags nFlags, vcl::MapModePolicy eMapPolicy)
{
    if (!IsDeviceOutputNecessary() || IsLayoutCalculationNecessary())
        return false;

    if (eMapPolicy == vcl::MapModePolicy::ForcePixel && mpMapper->IsMapModeEnabled())
        EnableMapMode(false);
    else if (eMapPolicy == vcl::MapModePolicy::ForceLogic && !mpMapper->IsMapModeEnabled())
        EnableMapMode(true);

    bool bWillDrawLine = (nFlags & vcl::PrepareOutputFlags::Line) && mpGraphicsState->mbLineColor;
    bool bWillDrawFill = (nFlags & vcl::PrepareOutputFlags::Fill) && mpGraphicsState->mbFillColor;

    if (nFlags & (vcl::PrepareOutputFlags::Line | vcl::PrepareOutputFlags::Fill))
    {
        if (!bWillDrawLine && !bWillDrawFill)
            return false;
    }

    return FlushGraphicsState(nFlags);
}

bool OutputDevice::FlushGraphicsState(vcl::PrepareOutputFlags nFlags)
{
    if (!mpGraphics && !AcquireGraphics())
        return false;

    assert(mpGraphics);

    if (nFlags & vcl::PrepareOutputFlags::Clip)
    {
        if (mpClippingController->IsDirty())
            InitClipRegion();

        if (IsOutputCulled())
            return false;
    }

    if ((nFlags & vcl::PrepareOutputFlags::Line) && mbLineColorDirty)
        InitLineColor();

    if ((nFlags & vcl::PrepareOutputFlags::Fill) && mbFillColorDirty)
        InitFillColor();

    if ((nFlags & vcl::PrepareOutputFlags::Font) && !InitFont())
        return false;

    return true;
}

void OutputDevice::Push(vcl::PushFlags nFlags)
{
    maRecorder.RecordPush(nFlags);

    maOutDevStateStack.emplace_back();
    vcl::State& rState = maOutDevStateStack.back();

    rState.mnFlags = nFlags;

    if (nFlags & vcl::PushFlags::LINECOLOR && mpGraphicsState->mbLineColor)
        rState.mpLineColor = mpGraphicsState->maLineColor;

    if (nFlags & vcl::PushFlags::FILLCOLOR && mpGraphicsState->mbFillColor)
        rState.mpFillColor = mpGraphicsState->maFillColor;

    if (nFlags & vcl::PushFlags::FONT)
        rState.mpFont = mpGraphicsState->maFont;

    if (nFlags & vcl::PushFlags::TEXTCOLOR)
        rState.mpTextColor = GetTextColor();

    if (nFlags & vcl::PushFlags::TEXTFILLCOLOR && IsTextFillColor())
        rState.mpTextFillColor = GetTextFillColor();

    if (nFlags & vcl::PushFlags::TEXTLINECOLOR && IsTextLineColor())
        rState.mpTextLineColor = GetTextLineColor();

    if (nFlags & vcl::PushFlags::OVERLINECOLOR && IsOverlineColor())
        rState.mpOverlineColor = GetOverlineColor();

    if (nFlags & vcl::PushFlags::TEXTALIGN)
        rState.meTextAlign = GetTextAlign();

    if (nFlags & vcl::PushFlags::TEXTLAYOUTMODE)
        rState.mnTextLayoutMode = GetLayoutMode();

    if (nFlags & vcl::PushFlags::TEXTLANGUAGE)
        rState.meTextLanguage = GetDigitLanguage();

    if (nFlags & vcl::PushFlags::RASTEROP)
        rState.meRasterOp = GetRasterOp();

    if (nFlags & vcl::PushFlags::MAPMODE)
    {
        rState.mpMapMode = mpMapper->GetMapMode();
        rState.mbMapActive = mpMapper->IsMapModeEnabled();
    }

    if (nFlags & vcl::PushFlags::CLIPREGION)
    {
        if (mpClippingController->HasClipRegion())
            rState.mpClipRegion.reset(new vcl::Region(mpClippingController->GetClipRegion()));
        else
            rState.mpClipRegion.reset();
    }

    if (nFlags & vcl::PushFlags::REFPOINT && mpGraphicsState->mbRefPoint)
        rState.mpRefPoint = mpGraphicsState->maRefPoint;
}

void OutputDevice::Pop()
{
    DBG_TESTSOLARMUTEX();

    maRecorder.RecordPop();

    vcl::MetafileRecorder::ScopedSuspend aMetaFileSuspend(maRecorder);

    if ( maOutDevStateStack.empty() )
    {
        SAL_WARN( "vcl.gdi", "OutputDevice::Pop() without OutputDevice::Push()" );
        return;
    }
    const vcl::State& rState = maOutDevStateStack.back();

    if ( rState.mnFlags & vcl::PushFlags::LINECOLOR )
    {
        if ( rState.mpLineColor )
            SetLineColor( *rState.mpLineColor );
        else
            SetLineColor();
    }

    if ( rState.mnFlags & vcl::PushFlags::FILLCOLOR )
    {
        if ( rState.mpFillColor )
            SetFillColor( *rState.mpFillColor );
        else
            SetFillColor();
    }

    if ( rState.mnFlags & vcl::PushFlags::FONT )
        SetFont( *rState.mpFont );

    if ( rState.mnFlags & vcl::PushFlags::TEXTCOLOR )
        SetTextColor( *rState.mpTextColor );

    if ( rState.mnFlags & vcl::PushFlags::TEXTFILLCOLOR )
    {
        if ( rState.mpTextFillColor )
            SetTextFillColor( *rState.mpTextFillColor );
        else
            SetTextFillColor();
    }

    if ( rState.mnFlags & vcl::PushFlags::TEXTLINECOLOR )
    {
        if ( rState.mpTextLineColor )
            SetTextLineColor( *rState.mpTextLineColor );
        else
            SetTextLineColor();
    }

    if ( rState.mnFlags & vcl::PushFlags::OVERLINECOLOR )
    {
        if ( rState.mpOverlineColor )
            SetOverlineColor( *rState.mpOverlineColor );
        else
            SetOverlineColor();
    }

    if ( rState.mnFlags & vcl::PushFlags::TEXTALIGN )
        SetTextAlign( rState.meTextAlign );

    if( rState.mnFlags & vcl::PushFlags::TEXTLAYOUTMODE )
        SetLayoutMode( rState.mnTextLayoutMode );

    if( rState.mnFlags & vcl::PushFlags::TEXTLANGUAGE )
        SetDigitLanguage( rState.meTextLanguage );

    if ( rState.mnFlags & vcl::PushFlags::RASTEROP )
        SetRasterOp( rState.meRasterOp );

    if ( rState.mnFlags & vcl::PushFlags::MAPMODE )
    {
        if ( rState.mpMapMode )
            SetMapMode( *rState.mpMapMode );
        else
            SetMapMode();

        mpMapper->EnableMapMode(rState.mbMapActive);
    }

    if (rState.mnFlags & vcl::PushFlags::CLIPREGION)
    {
        if (!rState.mpClipRegion)
        {
            if (mpClippingController->HasClipRegion())
                mpClippingController->SetNoClipRegion(); // or ClearClipRegion(), depending on your API
        }
        else
        {
            // Pass the saved device-pixel region directly to the controller
            mpClippingController->SetClipRegion(*(rState.mpClipRegion));
        }
    }

    if ( rState.mnFlags & vcl::PushFlags::REFPOINT )
    {
        if ( rState.mpRefPoint )
            SetRefPoint( *rState.mpRefPoint );
        else
            SetRefPoint();
    }

    maOutDevStateStack.pop_back();
}

void OutputDevice::ClearStack()
{
    sal_uInt32 nCount = maOutDevStateStack.size();
    while( nCount-- )
        Pop();
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
