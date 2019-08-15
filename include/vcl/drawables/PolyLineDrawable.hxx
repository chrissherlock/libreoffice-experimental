/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 */

#ifndef INCLUDED_INCLUDE_VCL_DRAWABLE_POLYLINEDRAWABLE_HXX
#define INCLUDED_INCLUDE_VCL_DRAWABLE_POLYLINEDRAWABLE_HXX

#include <basegfx/polygon/b2dpolypolygon.hxx>
#include <basegfx/polygon/b2dpolygon.hxx>
#include <tools/poly.hxx>

#include <vcl/lineinfo.hxx>
#include <vcl/metaact.hxx>
#include <vcl/drawables/Drawable.hxx>

#include <memory>

class OutputDevice;

class VCL_DLLPUBLIC PolyLineDrawable : public Drawable
{
public:
    PolyLineDrawable(tools::Polygon const aPolygon)
        : maPolygon(aPolygon)
        , mbUsesLineInfo(false)
        , mbUsesToolsPolygon(true)
        , mbUsesB2DPolygon(false)
    {
        mpMetaAction = new MetaPolyLineAction(aPolygon);
    }

    /** Render the given polygon as a line stroke

        The given polygon is stroked with the current LineColor, start
        and end point are not automatically connected. The line is
        rendered according to the specified LineInfo, e.g. supplying a
        dash pattern, or a line thickness.
    */
    PolyLineDrawable(tools::Polygon const aPolygon, LineInfo const aLineInfo)
        : maPolygon(aPolygon)
        , maLineInfo(aLineInfo)
        , mbUsesLineInfo(true)
        , mbUsesToolsPolygon(true)
        , mbUsesB2DPolygon(false)
    {
        mpMetaAction = new MetaPolyLineAction(aPolygon, aLineInfo);
    }

    PolyLineDrawable(basegfx::B2DPolygon aPolygon, LineInfo aLineInfo = LineInfo(),
                     double fMiterMinimumAngle = basegfx::deg2rad(15.0))
        : maLineInfo(aLineInfo)
        , maB2DPolygon(aPolygon)
        , mfMiterMinimumAngle(fMiterMinimumAngle)
        , mbUsesLineInfo(true)
        , mbUsesToolsPolygon(false)
        , mbUsesB2DPolygon(true)
    {
        if (aLineInfo.GetWidth() != 0.0)
            aLineInfo.SetWidth(static_cast<long>(aLineInfo.GetWidth() + 0.5));

        const tools::Polygon aToolsPolygon(aPolygon);
        mpMetaAction = new MetaPolyLineAction(aToolsPolygon, aLineInfo);
    }

    virtual bool execute(OutputDevice* pRenderContext) const override;

private:
    /** Render the given polygon as a line stroke

        The given polygon is stroked with the current LineColor, start
        and end point are not automatically connected
    */
    bool Draw(OutputDevice* pRenderContext, tools::Polygon const& rPolygon) const;

    /** Render the given polygon as a line stroke

        The given polygon is stroked with the current LineColor, start
        and end point are not automatically connected. The line is
        rendered according to the specified LineInfo, e.g. supplying a
        dash pattern, or a line thickness.
     */
    bool Draw(OutputDevice* pRenderContext, tools::Polygon const& rPolygon,
              LineInfo const aLineInfo) const;

    /** Render the given B2DPolygon as a line stroke

        The given polygon is stroked with the current LineColor, start
        and end point are not automatically connected. The line is
        rendered according to the specified LineInfo, e.g. supplying a
        dash pattern, or a line thickness.
     */
    bool Draw(OutputDevice* pRenderContext, const basegfx::B2DPolygon& rB2DPolygon,
              LineInfo aLineInfo, double fMiterMinimumAngle) const;

    /** Helper who tries to use SalGDI's DrawPolyLine direct and returns it's bool. See #i101491#
    */
    bool DrawPolyLineDirect(OutputDevice* pRenderContext,
                            basegfx::B2DHomMatrix const& rObjectTransform,
                            basegfx::B2DPolygon const& rB2DPolygon, LineInfo aLineInfo,
                            double fTransparency = 0.0,
                            double fMiterMinimumAngle = basegfx::deg2rad(15.0)) const;

    tools::Polygon maPolygon;
    LineInfo maLineInfo;
    basegfx::B2DPolygon maB2DPolygon;
    double mfMiterMinimumAngle;
    bool mbUsesLineInfo;
    bool mbUsesToolsPolygon;
    bool mbUsesB2DPolygon;
};

#endif

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
