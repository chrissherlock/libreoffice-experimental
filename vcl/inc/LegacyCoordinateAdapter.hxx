/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <tools/gen.hxx>
#include <tools/mapunit.hxx>
#include <basegfx/polygon/b2dpolygon.hxx>
#include <basegfx/matrix/b2dhommatrix.hxx>

#include <vcl/dllapi.h>
#include <vcl/TransformTypes.hxx>
#include <vcl/mapmod.hxx>

class MapMode;

VCL_DLLPUBLIC Point LogicToLogic(const Point& rPtSource, const MapMode& rMapModeSource,
                                 const MapMode& rMapModeDest);

VCL_DLLPUBLIC Size LogicToLogic(const Size& rSzSource, const MapMode& rMapModeSource,
                                const MapMode& rMapModeDest);

VCL_DLLPUBLIC tools::Rectangle LogicToLogic(const tools::Rectangle& rRectSource,
                                            const MapMode& rMapModeSource,
                                            const MapMode& rMapModeDest);

VCL_DLLPUBLIC tools::Long LogicToLogic(tools::Long nLongSource, MapUnit eUnitSource,
                                       MapUnit eUnitDest);

VCL_DLLPUBLIC basegfx::B2DPolygon LogicToLogic(const basegfx::B2DPolygon& rPolySource,
                                               const MapMode& rMapModeSource,
                                               const MapMode& rMapModeDest);

VCL_DLLPUBLIC basegfx::B2DHomMatrix LogicToLogic(const MapMode& rMapModeSource,
                                                 const MapMode& rMapModeDest);

namespace vcl
{
template <typename Geom>
vcl::TypedGeom<vcl::SpaceLogic, Geom>
ConvertLogic(const vcl::TypedGeom<vcl::SpaceLogic, Geom>& rGeom, const MapMode& rSourceMode,
             const MapMode& rDestMode)
{
    if (rSourceMode == rDestMode)
        return rGeom;

    return vcl::TypedGeom<vcl::SpaceLogic, Geom>(
        ::LogicToLogic(rGeom.get(), rSourceMode, rDestMode));
}
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
