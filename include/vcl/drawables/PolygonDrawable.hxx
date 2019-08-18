/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 */

#ifndef INCLUDED_INCLUDE_VCL_DRAWABLE_POLYGONDRAWABLE_HXX
#define INCLUDED_INCLUDE_VCL_DRAWABLE_POLYGONDRAWABLE_HXX

#include <tools/poly.hxx>

#include <vcl/metaact.hxx>
#include <vcl/drawables/Drawable.hxx>

#include <memory>

class OutputDevice;

class VCL_DLLPUBLIC PolygonDrawable : public Drawable
{
public:
    PolygonDrawable(tools::Polygon const aPolygon, bool bUseScaffolding = true)
        : Drawable(bUseScaffolding)
        , maPolygon(aPolygon)
        , mbUsesB2DPolygon(false)
        , mbClipped(false)
    {
        mpMetaAction = new MetaPolygonAction(aPolygon);
    }

    PolygonDrawable(basegfx::B2DPolygon aPolygon)
        : Drawable(false)
        , maB2DPolygon(aPolygon)
        , mbUsesB2DPolygon(true)
        , mbClipped(false)
    {
    }

    PolygonDrawable(tools::Polygon aPolygon, tools::PolyPolygon aClipPolyPolygon)
        : Drawable(false)
        , maPolygon(aPolygon)
        , maClipPolyPolygon(aClipPolyPolygon)
        , mbUsesB2DPolygon(false)
        , mbClipped(true)
    {
    }

protected:
    virtual bool CanDraw(OutputDevice* pRenderContext) const override;

    virtual bool DrawCommand(OutputDevice* pRenderContext) const override;

private:
    bool Draw(OutputDevice* pRenderContext, tools::Polygon const aPolygon) const;

    /** Render the given polygon

        The given polygon is stroked with the current LineColor, and
        filled with the current FillColor. If one of these colors are
        transparent, the corresponding stroke or fill stays
        invisible. Start and end point of the polygon are
        automatically connected.
     */
    bool Draw(OutputDevice* pRenderContext, basegfx::B2DPolygon const aPolygon) const;
    bool Draw(OutputDevice* pRenderContext, tools::Polygon aPolygon,
              tools::PolyPolygon pClipPolyPolygon) const;

    tools::Polygon maPolygon;
    basegfx::B2DPolygon maB2DPolygon;
    tools::PolyPolygon maClipPolyPolygon;
    bool mbUsesB2DPolygon;
    bool mbClipped;
};

#endif

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
