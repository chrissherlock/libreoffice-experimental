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

#include <vcl/window.hxx>

#include <salgdi.hxx>

// note: the coordinates to be remirrored are in frame coordinates !

void vcl::Window::ReMirror(Point& rPoint) const
{
    rPoint.setX(maGeometry.GetXFrameOffset() + maGeometry.GetWidthInPixels() - 1 - rPoint.X()
                + maGeometry.GetXFrameOffset());
}

void vcl::Window::ReMirror(tools::Rectangle& rRect) const
{
    tools::Long nWidth = rRect.Right() - rRect.Left();

    //long lc_x = rRect.nLeft - maGeometry.GetXFrameOffset();    // normalize
    //lc_x = maGeometry.GetWidthInPixels() - nWidth - 1 - lc_x;  // mirror
    //rRect.nLeft = lc_x + maGeometry.GetXFrameOffset();         // re-normalize

    rRect.SetLeft(maGeometry.GetXFrameOffset() + maGeometry.GetWidthInPixels() - nWidth - 1
                  - rRect.Left() + maGeometry.GetXFrameOffset());
    rRect.SetRight(rRect.Left() + nWidth);
}

void vcl::Window::ReMirror(vcl::Region& rRegion) const
{
    std::vector<tools::Rectangle> aRectangles;
    rRegion.GetRegionRectangles(aRectangles);
    vcl::Region aMirroredRegion;

    for (auto& rectangle : aRectangles)
    {
        ReMirror(rectangle);
        aMirroredRegion.Union(rectangle);
    }

    rRegion = aMirroredRegion;
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
