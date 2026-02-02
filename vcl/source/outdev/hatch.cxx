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
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements. See the NOTICE file distributed
 * with this work for additional information regarding copyright
 * ownership. The ASF licenses this file to you under the Apache
 * License, Version 2.0 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.apache.org/licenses/LICENSE-2.0 .
 */

#include <osl/diagnose.h>
#include <tools/line.hxx>
#include <tools/helpers.hxx>
#include <comphelper/configuration.hxx>

#include <vcl/hatch.hxx>
#include <metafile/MetafileRecorder.hxx>
#include <vcl/metafile/ScopedMetaGroup.hxx>
#include <vcl/settings.hxx>
#include <vcl/virdev.hxx>

#include <ClippingController.hxx>
#include <CoordinateMapper.hxx>
#include <HatchProcessor.hxx>
#include <drawmode.hxx>
#include <salgdi.hxx>

#include <cassert>
#include <cstdlib>
#include <algorithm>
#include <memory>

void OutputDevice::DrawHatch( const tools::PolyPolygon& rPolyPoly, const Hatch& rHatch )
{
    assert(!is_double_buffered_window());

    Hatch aHatch( rHatch );
    aHatch.SetColor(vcl::drawmode::GetHatchColor(rHatch.GetColor(), GetDrawMode(), GetSettings().GetStyleSettings()));

    vcl::MetafileRecorder(*this).RecordHatch( rPolyPoly, aHatch );

    if( !IsDeviceOutputNecessary() || IsLayoutCalculationNecessary() )
        return;

    if( !mpGraphics && !AcquireGraphics() )
        return;
    assert(mpGraphics);

    if ( mpClippingController->IsDirty() )
        InitClipRegion();

    if ( IsOutputCulled() )
        return;

    if( rPolyPoly.Count() )
    {
        tools::PolyPolygon     aPolyPoly( LogicToPixel( rPolyPoly ) );
        GDIMetaFile* pOldMetaFile = mpMetaFile;
        bool bOldMap = mpMapper->IsMapModeEnabled();

        aPolyPoly.Optimize( PolyOptimizeFlags::NO_SAME );
        aHatch.SetDistance(LogicWidthToDevicePixel(aHatch.GetDistance()));

        mpMetaFile = nullptr;
        mpMapper->EnableMapMode( false );
        Push( vcl::PushFlags::LINECOLOR );
        SetLineColor( aHatch.GetColor() );
        InitLineColor();
        DrawHatch( aPolyPoly, aHatch, false );
        Pop();
        mpMapper->EnableMapMode( bOldMap );
        mpMetaFile = pOldMetaFile;
    }
}

void OutputDevice::AddHatchActions( const tools::PolyPolygon& rPolyPoly, const Hatch& rHatch,
                                    GDIMetaFile& rMtf )
{

    tools::PolyPolygon aPolyPoly( rPolyPoly );
    aPolyPoly.Optimize( PolyOptimizeFlags::NO_SAME | PolyOptimizeFlags::CLOSE );

    if( aPolyPoly.Count() )
    {
        GDIMetaFile* pOldMtf = mpMetaFile;

        mpMetaFile = &rMtf;
        {
            vcl::ScopedMetaGroup aGroup(&rMtf, "DecomposedHatch");
            vcl::MetafileRecorder aRecorder(*this);
            aRecorder.RecordPush( vcl::PushFlags::ALL );
            aRecorder.RecordLineColor( rHatch.GetColor(), true );
            DrawHatch( aPolyPoly, rHatch, true );
            aRecorder.RecordPop();
        }
        mpMetaFile = pOldMtf;
    }
}

void OutputDevice::DrawHatch( const tools::PolyPolygon& rPolyPoly, const Hatch& rHatch, bool bMtf )
{
    assert(!is_double_buffered_window());

    if(!rPolyPoly.Count())
        return;

    // Note: Curve handling is now done inside HatchProcessor

    tools::Rectangle   aRect( rPolyPoly.GetBoundRect() );
    const tools::Long  nLogPixelWidth = mpMapper->DevicePixelToLogicWidth(1);
    const tools::Long nWidth = mpMapper->DevicePixelToLogicWidth(std::max(LogicWidthToDevicePixel(rHatch.GetDistance()), tools::Long(3)));

    Point aRefPoint = IsRefPoint() ? GetRefPoint() : aRect.TopLeft();

    vcl::HatchProcessor::Process(rPolyPoly, rHatch, aRect, aRefPoint, nLogPixelWidth, nWidth,
        [&](const Point& p1, const Point& p2) {
            if (bMtf) {
                vcl::MetafileRecorder aRecorder(*this);
                aRecorder.RecordLine(p1, p2);
            } else {
                DrawHatchLine(p1, p2);
            }
        });
}

void OutputDevice::DrawHatchLine(const Point& rStartPoint, const Point& rEndPoint)
{
    Point aPt1{LogicToDevicePixel(rStartPoint)}, aPt2{LogicToDevicePixel(rEndPoint)};
    mpGraphics->DrawLine(aPt1.X(), aPt1.Y(), aPt2.X(), aPt2.Y(), *this);
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
