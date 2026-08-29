/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
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

#include <tools/mapunit.hxx>

#include <vcl/CoordinateMapper.hxx>
#include <vcl/font.hxx>
#include <vcl/outdev.hxx>
#include <vcl/window.hxx>

namespace vcl
{
vcl::Font Window::GetDrawPixelFont(OutputDevice const* pDev) const
{
    vcl::Font aFont = GetPointFont(*GetOutDev());
    MapMode aPtMapMode(MapUnit::MapPoint);
    const auto aFontSize
        = pDev->convertTo<vcl::WindowSize>(vcl::LogicSize(aFont.GetFontSize()), aPtMapMode);
    aFont.SetFontSize(aFontSize.get());
    return aFont;
}

tools::Long Window::GetDrawPixel(OutputDevice const* pDev, tools::Long nPixels) const
{
    tools::Long nP = nPixels;
    if (pDev->GetOutDevType() != OUTDEV_WINDOW)
    {
        MapMode aMap(MapUnit::Map100thMM);
        auto aSz = convertTo<vcl::WindowSize>(vcl::LogicSize(nP, 0), aMap);
        nP = aSz->Width();
    }
    return nP;
}

} // end vcl namespace

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
