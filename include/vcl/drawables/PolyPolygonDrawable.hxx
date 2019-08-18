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

#define OUTDEV_POLYPOLY_STACKBUF 32

class OutputDevice;

/** Render the given poly-polygon

    The given poly-polygon is stroked with the current LineColor,
    and filled with the current FillColor. If one of these colors
    are transparent, the corresponding stroke or fill stays
    invisible. Start and end points of the contained polygons are
    automatically connected.
 */
class VCL_DLLPUBLIC PolyPolygonDrawable : public Drawable
{
public:
    PolyPolygonDrawable(tools::PolyPolygon const aPolyPolygon)
        : maPolyPolygon(aPolyPolygon)
        , mbProcessMtf(true)
        , mbRecursiveDraw(false)
        , mbUsesB2DPolyPolygon(false)
    {
        mpMetaAction = new MetaPolyPolygonAction(aPolyPolygon);
    }

    PolyPolygonDrawable(sal_uInt16 nPolygonCount, tools::PolyPolygon aPolyPolygon)
        : Drawable(false)
        , maPolyPolygon(aPolyPolygon)
        , mnPolygonCount(nPolygonCount)
        , mbProcessMtf(false)
        , mbRecursiveDraw(true)
        , mbUsesB2DPolyPolygon(false)
    {
    }

    PolyPolygonDrawable(basegfx::B2DPolyPolygon const aPolyPolygon, bool bProcessMtf = true)
        : maB2DPolyPolygon(aPolyPolygon)
        , mbProcessMtf(bProcessMtf)
        , mbRecursiveDraw(false)
        , mbUsesB2DPolyPolygon(true)
    {
        if (mbProcessMtf)
            mpMetaAction = new MetaPolyPolygonAction(tools::PolyPolygon(aPolyPolygon));
    }

protected:
    bool ShouldAddAction() const override { return mbProcessMtf; }
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
    bool Draw(OutputDevice* pRenderContext, sal_uInt16 nPolygonCount,
              tools::PolyPolygon aPolyPolygon) const;
    bool Draw(OutputDevice* pRenderContext, basegfx::B2DPolyPolygon const aPolyPolygon) const;

    tools::PolyPolygon maPolyPolygon;
    basegfx::B2DPolyPolygon maB2DPolyPolygon;
    sal_uInt16 mnPolygonCount;
    bool mbProcessMtf;
    bool mbRecursiveDraw;
    bool mbUsesB2DPolyPolygon;
};

#endif

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
