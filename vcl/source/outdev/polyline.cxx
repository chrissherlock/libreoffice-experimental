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
#include <basegfx/matrix/b2dhommatrix.hxx>
#include <basegfx/polygon/b2dlinegeometry.hxx>

#include <vcl/metaact.hxx>
#include <vcl/outdev.hxx>

void OutputDevice::DrawPolyLine(tools::Polygon const& rPoly)
{
    if (mpMetaFile)
        mpMetaFile->AddAction(new MetaPolyLineAction(rPoly));

    RenderContext2::DrawPolyLine(rPoly);
}

void OutputDevice::DrawPolyLine(const tools::Polygon& rPoly, const LineInfo& rLineInfo)
{
    if (rLineInfo.IsDefault())
    {
        DrawPolyLine(rPoly);
        return;
    }

    // #i101491#
    // Try direct Fallback to B2D-Version of DrawPolyLine
    if (LineStyle::Solid == rLineInfo.GetStyle())
    {
        DrawPolyLine(rPoly.getB2DPolygon(), static_cast<double>(rLineInfo.GetWidth()),
                     rLineInfo.GetLineJoin(), rLineInfo.GetLineCap(),
                     basegfx::deg2rad(
                         15.0) /* default fMiterMinimumAngle, value not available in LineInfo */);
        return;
    }

    if (mpMetaFile)
        mpMetaFile->AddAction(new MetaPolyLineAction(rPoly, rLineInfo));

    RenderContext2::DrawPolyLine(rPoly, rLineInfo);
}

void OutputDevice::DrawPolyLine(basegfx::B2DPolygon const& rB2DPolygon, double fLineWidth,
                                basegfx::B2DLineJoin eLineJoin, css::drawing::LineCap eLineCap,
                                double fMiterMinimumAngle)
{
    // use b2dpolygon drawing if possible
    if (DrawPolyLineDirect(basegfx::B2DHomMatrix(), rB2DPolygon, fLineWidth, 0.0,
                           nullptr, // MM01
                           eLineJoin, eLineCap, fMiterMinimumAngle))
    {
        return;
    }

    if (mpMetaFile)
    {
        LineInfo aLineInfo;
        if (fLineWidth != 0.0)
            aLineInfo.SetWidth(static_cast<tools::Long>(fLineWidth + 0.5));

        // Transport known information, might be needed
        aLineInfo.SetLineJoin(eLineJoin);
        aLineInfo.SetLineCap(eLineCap);

        const tools::Polygon aToolsPolygon(rB2DPolygon);
        mpMetaFile->AddAction(new MetaPolyLineAction(aToolsPolygon, aLineInfo));
    }

    if (ImplIsRecordLayout()
        && !(fLineWidth >= 2.5 && rB2DPolygon.count() && rB2DPolygon.count() <= 1000))
    {
        return;
    }

    RenderContext2::DrawPolyLine(rB2DPolygon, fLineWidth, eLineJoin, eLineCap, fMiterMinimumAngle);
}

bool OutputDevice::DrawPolyLineDirect(basegfx::B2DHomMatrix const& rObjectTransform,
                                      basegfx::B2DPolygon const& rB2DPolygon, double fLineWidth,
                                      double fTransparency,
                                      std::vector<double> const* pStroke, // MM01
                                      basegfx::B2DLineJoin eLineJoin,
                                      css::drawing::LineCap eLineCap, double fMiterMinimumAngle)
{
    if (RenderContext2::DrawPolyLineDirect(rObjectTransform, rB2DPolygon, fLineWidth, fTransparency,
                                           pStroke, eLineJoin, eLineCap, fMiterMinimumAngle))
    {
        // Worked, add metafile action (if recorded). This is done only here,
        // because this function is public, other OutDev functions already add metafile
        // actions, so they call the internal function directly.
        if (mpMetaFile)
        {
            LineInfo aLineInfo;
            if (fLineWidth != 0.0)
                aLineInfo.SetWidth(static_cast<tools::Long>(fLineWidth + 0.5));

            // Transport known information, might be needed
            aLineInfo.SetLineJoin(eLineJoin);
            aLineInfo.SetLineCap(eLineCap);

            // MiterMinimumAngle does not exist yet in LineInfo
            const tools::Polygon aToolsPolygon(rB2DPolygon);
            mpMetaFile->AddAction(new MetaPolyLineAction(aToolsPolygon, aLineInfo));
        }

        return true;
    }

    return false;
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
