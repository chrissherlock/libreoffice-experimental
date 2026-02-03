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
#include <comphelper/scopeguard.hxx>

#include <vcl/hatch.hxx>
#include <vcl/metafile/ScopedMetaGroup.hxx>
#include <vcl/settings.hxx>
#include <vcl/virdev.hxx>
#include <vcl/HatchProcessor.hxx>

#include <ClippingController.hxx>
#include <CoordinateMapper.hxx>
#include <drawmode.hxx>
#include <salgdi.hxx>
#include <metafile/MetafileRecorder.hxx>

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
        tools::PolyPolygon aPolyPoly( LogicToPixel( rPolyPoly ) );
        aPolyPoly.Optimize( PolyOptimizeFlags::NO_SAME );

        // Guard MetaFile
        GDIMetaFile* pOldMetaFile = mpMetaFile;
        mpMetaFile = nullptr;
        comphelper::ScopeGuard aMetaFileGuard([this, pOldMetaFile]() {
            mpMetaFile = pOldMetaFile;
        });

        // Guard MapMode
        bool bOldMap = mpMapper->IsMapModeEnabled();
        mpMapper->EnableMapMode( false );
        comphelper::ScopeGuard aMapModeGuard([this, bOldMap]() {
            mpMapper->EnableMapMode( bOldMap );
        });

        // Guard Push/Pop
        Push( vcl::PushFlags::LINECOLOR );
        comphelper::ScopeGuard aPopGuard([this]() {
            Pop();
        });

        aHatch.SetDistance(LogicWidthToDevicePixel(aHatch.GetDistance()));
        SetLineColor( aHatch.GetColor() );
        InitLineColor();

        // --- Inlined Hatch Processing ---
        // Note: Curve handling is now done inside HatchProcessor

        tools::Rectangle aRect( aPolyPoly.GetBoundRect() );
        const tools::Long nLogPixelWidth = mpMapper->DevicePixelToLogicWidth(1);
        const tools::Long nWidth = mpMapper->DevicePixelToLogicWidth(std::max(aHatch.GetDistance(), tools::Long(3)));

        Point aRefPoint = IsRefPoint() ? GetRefPoint() : aRect.TopLeft();
        if (IsRefPoint())
             aRefPoint = LogicToPixel(GetRefPoint());

        vcl::HatchProcessor::Process(aPolyPoly, aHatch, aRect, aRefPoint, nLogPixelWidth, nWidth,
            [&](const Point& p1, const Point& p2) {
                // We keep calling DrawHatchLine to maintain consistent behavior
                DrawHatchLine(p1, p2);
            });
    }
}

void OutputDevice::DrawHatchLine(const Point& rStartPoint, const Point& rEndPoint)
{
    Point aPt1{LogicToDevicePixel(rStartPoint)}, aPt2{LogicToDevicePixel(rEndPoint)};
    mpGraphics->DrawLine(aPt1.X(), aPt1.Y(), aPt2.X(), aPt2.Y(), *this);
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
