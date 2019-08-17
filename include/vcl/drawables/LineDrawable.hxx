/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 */

#ifndef INCLUDED_INCLUDE_VCL_DRAWABLE_LINEDRAWABLE_HXX
#define INCLUDED_INCLUDE_VCL_DRAWABLE_LINEDRAWABLE_HXX

#include <basegfx/polygon/b2dpolypolygon.hxx>
#include <basegfx/polygon/b2dpolygon.hxx>
#include <tools/gen.hxx>

#include <vcl/metaact.hxx>
#include <vcl/lineinfo.hxx>
#include <vcl/drawables/Drawable.hxx>

#include <memory>

class OutputDevice;

class VCL_DLLPUBLIC LineDrawable : public Drawable
{
public:
    LineDrawable(Point const aStartPt, Point const aEndPt)
        : maStartPt(aStartPt)
        , maEndPt(aEndPt)
        , mpLinePolyPolygon(nullptr)
        , mbUsesLineInfo(false)
        , mbUsesPolyPolygon(false)
    {
        mpMetaAction = new MetaLineAction(maStartPt, maEndPt);
    }

    LineDrawable(Point const aStartPt, Point const aEndPt, LineInfo const aInfo)
        : maStartPt(aStartPt)
        , maEndPt(aEndPt)
        , maLineInfo(aInfo)
        , mpLinePolyPolygon(nullptr)
        , mbUsesLineInfo(true)
        , mbUsesPolyPolygon(false)
    {
        mpMetaAction = new MetaLineAction(maStartPt, maEndPt, aInfo);
    }

    LineDrawable(basegfx::B2DPolyPolygon* pLinePolyPolygon, const LineInfo aInfo)
        : Drawable(false)
        , maLineInfo(aInfo)
        , mbUsesLineInfo(true)
        , mbUsesPolyPolygon(true)
    {
        mpLinePolyPolygon = pLinePolyPolygon;
    }

protected:
    bool CanDraw(OutputDevice* pRenderContext) const override;
    bool ShouldInitClipRegion() const override { return UseScaffolding(); };
    bool ShouldInitColor() const override { return UseScaffolding(); };
    bool ShouldInitFillColor() const override { return UseScaffolding(); };
    bool UseAlphaVirtDev() const override { return UseScaffolding(); };

    bool DrawCommand(OutputDevice* pRenderContext) const override;

private:
    bool UsesScaffolding() const { return mbUsesScaffolding; }

    bool Draw(OutputDevice* pRenderContext) const;
    bool Draw(OutputDevice* pRenderContext, LineInfo aLineInfo) const;

    /** Helper for line geometry paint with support for graphic expansion (pattern and fat_to_area)
     */
    bool Draw(OutputDevice* pRenderContext, basegfx::B2DPolyPolygon* pLinePolyPolygon,
              LineInfo const& rInfo) const;

    Point maStartPt;
    Point maEndPt;
    LineInfo maLineInfo;
    basegfx::B2DPolyPolygon* mpLinePolyPolygon;
    bool mbUsesLineInfo;
    bool mbUsesPolyPolygon;
};

#endif

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
