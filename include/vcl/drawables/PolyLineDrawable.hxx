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
    {
    }

    virtual bool execute(OutputDevice* pRenderContext) const override;

private:
    /** Render the given polygon as a line stroke

        The given polygon is stroked with the current LineColor, start
        and end point are not automatically connected
    */
    bool Draw(OutputDevice* pRenderContext, tools::Polygon const& rPolygon) const;

    tools::Polygon maPolygon;
    bool mbUsesLineInfo;
    bool mbUsesToolsPolygon;
};

#endif

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
