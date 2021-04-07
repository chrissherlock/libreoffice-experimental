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

#include <vcl/Geometry.hxx>

Geometry::Geometry()
    : mbMap(false)
    , mnOutWidth(0)
    , mnOutHeight(0)
    , mnOutOffOrigX(0)
    , mnOutOffOrigY(0)
    , mnOutOffLogicX(0)
    , mnOutOffLogicY(0)
    , mnOutOffX(0)
    , mnOutOffY(0)
    , mnDPIX(0)
    , mnDPIY(0)
    , mnDPIScalePercentage(100)
{
}

bool Geometry::IsMapModeEnabled() const { return mbMap; }

void Geometry::EnableMapMode(bool bEnable) { mbMap = bEnable; }

Size Geometry::GetSizeInPixels() const { return Size(mnOutWidth, mnOutHeight); }

void Geometry::SetSize(Size const& rSize)
{
    mnOutWidth = rSize.Width();
    mnOutHeight = rSize.Height();
}

void Geometry::SetWidth(tools::Long nWidth) { mnOutWidth = nWidth; }

void Geometry::SetHeight(tools::Long nHeight) { mnOutHeight = nHeight; }

tools::Long Geometry::GetWidthInPixels() const { return mnOutWidth; }

tools::Long Geometry::GetHeightInPixels() const { return mnOutHeight; }

tools::Long Geometry::GetXOffsetFromOriginInPixels() const { return mnOutOffOrigX; }

void Geometry::SetXOffsetFromOriginInPixels(tools::Long nOffsetFromOriginXpx)
{
    mnOutOffOrigX = nOffsetFromOriginXpx;
}

tools::Long Geometry::GetYOffsetFromOriginInPixels() const { return mnOutOffOrigY; }

void Geometry::SetYOffsetFromOriginInPixels(tools::Long nOffsetFromOriginYpx)
{
    mnOutOffOrigY = nOffsetFromOriginYpx;
}

tools::Long Geometry::GetXOffsetFromOriginInLogicalUnits() const { return mnOutOffLogicX; }

void Geometry::SetXOffsetFromOriginInLogicalUnits(tools::Long nOffsetFromOriginXInLogicalUnits)
{
    mnOutOffLogicX = nOffsetFromOriginXInLogicalUnits;
}

tools::Long Geometry::GetYOffsetFromOriginInLogicalUnits() const { return mnOutOffLogicX; }

void Geometry::SetYOffsetFromOriginInLogicalUnits(tools::Long nOffsetFromOriginYInLogicalUnits)
{
    mnOutOffLogicY = nOffsetFromOriginYInLogicalUnits;
}

tools::Long Geometry::GetXFrameOffset() const { return mnOutOffX; }

void Geometry::SetXFrameOffset(tools::Long nOffsetXpx) { mnOutOffX = nOffsetXpx; }

tools::Long Geometry::GetYFrameOffset() const { return mnOutOffY; }

void Geometry::SetYFrameOffset(tools::Long nOffsetYpx) { mnOutOffY = nOffsetYpx; }

sal_Int32 Geometry::GetDPIX() const { return mnDPIX; }

sal_Int32 Geometry::GetDPIY() const { return mnDPIY; }

void Geometry::SetDPIX(sal_Int32 nDPIX) { mnDPIX = nDPIX; }

void Geometry::SetDPIY(sal_Int32 nDPIY) { mnDPIY = nDPIY; }

float Geometry::GetDPIScaleFactor() const { return mnDPIScalePercentage / 100.0f; }

sal_Int32 Geometry::GetDPIScalePercentage() const { return mnDPIScalePercentage; }

void Geometry::SetDPIScalePercentage(sal_Int32 nPercentage) { mnDPIScalePercentage = nPercentage; }

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
