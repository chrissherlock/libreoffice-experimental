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

#include <utility>
#include <list>
#include <vector>

#include <basegfx/polygon/b2dpolygontools.hxx>
#include <sal/log.hxx>
#include <officecfg/Office/Common.hxx>

#include <vcl/gdimtf.hxx>
#include <vcl/MetaRectAction.hxx>
#include <vcl/MetaPolygonAction.hxx>
#include <vcl/MetaPolyPolygonAction.hxx>
#include <vcl/MetaWallpaperAction.hxx>
#include <vcl/svapp.hxx>
#include <vcl/bitmapaccess.hxx>

#include "pdfwriter_impl.hxx"
#include "ConnectedComponents.hxx"

typedef ::std::pair<MetaAction*, int> Component; // MetaAction plus index in metafile

template <typename T> tools::Rectangle GetBoundsRect(T) { return tools::Rectangle(); }

template <> tools::Rectangle GetBoundsRect<MetaRectAction*>(MetaRectAction* pAction)
{
    return pAction->GetRect();
}

template <> tools::Rectangle GetBoundsRect<MetaPolygonAction*>(MetaPolygonAction* pAction)
{
    return pAction->GetPolygon().GetBoundRect();
}

template <> tools::Rectangle GetBoundsRect<MetaPolyPolygonAction*>(MetaPolyPolygonAction* pAction)
{
    return pAction->GetPolyPolygon().GetBoundRect();
}

template <> tools::Rectangle GetBoundsRect<MetaWallpaperAction*>(MetaWallpaperAction* pAction)
{
    return pAction->GetRect();
}

void ConnectedComponents::SetBackground(Color const& rBgColor, tools::Rectangle const& rBounds)
{
    aBgColor = rBgColor;
    aBounds = rBounds;
}

template <typename T> bool ConnectedComponents::IsValidShape(T)
{
    SAL_WARN("vcl.gdi", "Should never call on this!");
    assert(false);
    return false;
}

template <> bool ConnectedComponents::IsValidShape<MetaRectAction*>(MetaRectAction*)
{
    return true;
}

template <> bool ConnectedComponents::IsValidShape<MetaPolygonAction*>(MetaPolygonAction* pAction)
{
    const tools::Polygon aPoly(pAction->GetPolygon());
    return !basegfx::utils::isRectangle(aPoly.getB2DPolygon());
}

template <>
bool ConnectedComponents::IsValidShape<MetaPolyPolygonAction*>(MetaPolyPolygonAction* pAction)
{
    const tools::PolyPolygon aPoly(pAction->GetPolyPolygon());
    return aPoly.Count() != 1 || !basegfx::utils::isRectangle(aPoly[0].getB2DPolygon());
}

template <> bool ConnectedComponents::IsValidShape<MetaWallpaperAction*>(MetaWallpaperAction*)
{
    return true;
}

bool ConnectedComponents::IsBackgroundNotCovered(MetaAction* pAction,
                                                 OutputDevice const& rMapModeVDev)
{
    const tools::Rectangle& rCurrRect = GetBoundsRect(pAction);

    // shape needs to fully cover previous content, and have uniform
    // color
    return IsValidShape(pAction)
           || !(rMapModeVDev.LogicToPixel(rCurrRect).IsInside(aBounds)
                && rMapModeVDev.IsFillColor());
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
