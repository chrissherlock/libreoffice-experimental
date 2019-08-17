/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 */

#ifndef INCLUDED_INCLUDE_VCL_DRAWABLE_POLYPOLYGONDRAWABLE_HXX
#define INCLUDED_INCLUDE_VCL_DRAWABLE_POLYPOLYGONDRAWABLE_HXX

#include <tools/poly.hxx>

#include <vcl/metaact.hxx>
#include <vcl/drawables/Drawable.hxx>

#include <memory>

class OutputDevice;

class VCL_DLLPUBLIC PolyPolygonDrawable : public Drawable
{
public:
    PolyPolygonDrawable(tools::PolyPolygon const aPolyPolygon)
        : maPolyPolygon(aPolyPolygon)
    {
        mpMetaAction = new MetaPolyPolygonAction(aPolyPolygon);
    }

protected:
    bool CanDraw(OutputDevice* pRenderContext) const override;
    bool UseAlphaVirtDev() const override { return false; }
    void AddAction(OutputDevice* pRenderContext) const override;

    bool DrawCommand(OutputDevice* pRenderContext) const override;

private:
    /** Render the given poly-polygon

        The given poly-polygon is stroked with the current LineColor,
        and filled with the current FillColor. If one of these colors
        are transparent, the corresponding stroke or fill stays
        invisible. Start and end points of the contained polygons are
        automatically connected.

        @see DrawPolyLine
     */
    bool Draw(OutputDevice* pRenderContext, tools::PolyPolygon const aPolyPolygon) const;

    tools::PolyPolygon maPolyPolygon;
};

#endif

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
