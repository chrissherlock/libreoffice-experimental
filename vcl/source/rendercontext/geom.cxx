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

#include <tools/gen.hxx>
#include <tools/long.hxx>

#include <vcl/RenderContext2.hxx>

#include <algorithm>

sal_Int32 RenderContext2::GetDPIX() const { return maGeometry.GetDPIX(); }

sal_Int32 RenderContext2::GetDPIY() const { return maGeometry.GetDPIY(); }

void RenderContext2::SetDPIX(sal_Int32 nDPIX) { maGeometry.SetDPIX(nDPIX); }

void RenderContext2::SetDPIY(sal_Int32 nDPIY) { maGeometry.SetDPIY(nDPIY); }

float RenderContext2::GetDPIScaleFactor() const { return maGeometry.GetDPIScaleFactor(); }

sal_Int32 RenderContext2::GetDPIScalePercentage() const
{
    return maGeometry.GetDPIScalePercentage();
}

void RenderContext2::SetDPIScalePercentage(sal_Int32 nPercent)
{
    maGeometry.SetDPIScalePercentage(std::max(100, nPercent));
}

Size RenderContext2::GetOutputSizePixel() const
{
    return Size(maGeometry.GetWidthInPixels(), maGeometry.GetHeightInPixels());
}

tools::Long RenderContext2::GetOutputWidthPixel() const { return maGeometry.GetWidthInPixels(); }

tools::Long RenderContext2::GetOutputHeightPixel() const { return maGeometry.GetHeightInPixels(); }

void RenderContext2::SetSizeInPixels(Size const& rSize) { maGeometry.SetSizeInPixels(rSize); }

void RenderContext2::SetWidthInPixels(tools::Long nWidth) { maGeometry.SetWidthInPixels(nWidth); }

void RenderContext2::SetHeightInPixels(tools::Long nHeight)
{
    maGeometry.SetHeightInPixels(nHeight);
}

tools::Long RenderContext2::GetOutOffXPixel() const { return maGeometry.GetXOffsetInPixels(); }

tools::Long RenderContext2::GetOutOffYPixel() const { return maGeometry.GetYOffsetInPixels(); }

void RenderContext2::SetOutOffXPixel(tools::Long nOutOffX)
{
    maGeometry.SetXOffsetInPixels(nOutOffX);
}

void RenderContext2::SetOutOffYPixel(tools::Long nOutOffY)
{
    maGeometry.SetYOffsetInPixels(nOutOffY);
}

Point RenderContext2::GetOutputOffPixel() const
{
    return Point(maGeometry.GetXOffsetInPixels(), maGeometry.GetYOffsetInPixels());
}

tools::Rectangle RenderContext2::GetOutputRectPixel() const
{
    return tools::Rectangle(GetOutputOffPixel(), GetOutputSizePixel());
}

Size RenderContext2::GetOutputSize() const { return PixelToLogic(GetOutputSizePixel()); }

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
