/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <vcl/RenderContext2.hxx>

void RenderContext2::InitMapModeObjects() {}

bool RenderContext2::IsMapModeEnabled() const { return maGeometry.mbMap; }

void RenderContext2::EnableMapMode(bool bEnable) { maGeometry.mbMap = bEnable; }

static tools::Long ImplPixelToLogic(tools::Long n, tools::Long nDPI, tools::Long nMapNum,
                                    tools::Long nMapDenom)
{
    assert(nDPI > 0);

    if (nMapNum == 0)
        return 0;

    sal_Int64 nDenom = nDPI;
    nDenom *= nMapNum;

    sal_Int64 n64 = n;
    n64 *= nMapDenom;

    if (nDenom == 1)
    {
        n = static_cast<tools::Long>(n64);
    }
    else
    {
        n64 = 2 * n64 / nDenom;

        if (n64 < 0)
            --n64;
        else
            ++n64;

        n = static_cast<tools::Long>(n64 / 2);
    }

    return n;
}

Size RenderContext2::GetOffsetFromOriginInPixels() const
{
    return Size(maGeometry.mnOffsetFromOriginXpx, maGeometry.mnOffsetFromOriginYpx);
}

void RenderContext2::ResetLogicalUnitsOffsetFromOrigin()
{
    maGeometry.mnOffsetFromOriginXInLogicalUnits = maGeometry.mnOffsetFromOriginXpx;
    maGeometry.mnOffsetFromOriginYInLogicalUnits = maGeometry.mnOffsetFromOriginYpx;
}

tools::Long RenderContext2::GetXOffsetFromOriginInPixels() const
{
    return maGeometry.mnOffsetFromOriginXpx;
}

tools::Long RenderContext2::GetYOffsetFromOriginInPixels() const
{
    return maGeometry.mnOffsetFromOriginYpx;
}

void RenderContext2::SetOffsetFromOriginInPixels(Size const& rOffset)
{
    maGeometry.mnOffsetFromOriginXpx = rOffset.Width();
    maGeometry.mnOffsetFromOriginYpx = rOffset.Height();

    SetXOffsetFromOriginInLogicalUnits(
        ImplPixelToLogic(maGeometry.mnOffsetFromOriginXpx, maGeometry.mnDPIX,
                         maMappingMetric.mnMapScNumX, maMappingMetric.mnMapScDenomX));
    SetYOffsetFromOriginInLogicalUnits(
        ImplPixelToLogic(maGeometry.mnOffsetFromOriginYpx, maGeometry.mnDPIY,
                         maMappingMetric.mnMapScNumY, maMappingMetric.mnMapScDenomY));
}

sal_uInt32 RenderContext2::GetXOffsetFromOriginInLogicalUnits() const
{
    return maGeometry.mnOffsetFromOriginXInLogicalUnits;
}

void RenderContext2::SetXOffsetFromOriginInLogicalUnits(
    tools::Long nOffsetFromOriginXInLogicalUnits)
{
    maGeometry.mnOffsetFromOriginXInLogicalUnits = nOffsetFromOriginXInLogicalUnits;
}

sal_uInt32 RenderContext2::GetYOffsetFromOriginInLogicalUnits() const
{
    return maGeometry.mnOffsetFromOriginYInLogicalUnits;
}

void RenderContext2::SetYOffsetFromOriginInLogicalUnits(
    tools::Long nOffsetFromOriginYInLogicalUnits)
{
    maGeometry.mnOffsetFromOriginYInLogicalUnits = nOffsetFromOriginYInLogicalUnits;
}

tools::Long RenderContext2::GetXMapOffset() const { return maMappingMetric.mnMapOfsX; }
void RenderContext2::SetXMapOffset(tools::Long nXOffset) { maMappingMetric.mnMapOfsX = nXOffset; }

tools::Long RenderContext2::GetYMapOffset() const { return maMappingMetric.mnMapOfsY; }
void RenderContext2::SetYMapOffset(tools::Long nYOffset) { maMappingMetric.mnMapOfsY = nYOffset; }

tools::Long RenderContext2::GetXMapNumerator() const { return maMappingMetric.mnMapScNumX; }
void RenderContext2::SetXMapNumerator(tools::Long nNumerator)
{
    maMappingMetric.mnMapScNumX = nNumerator;
}

tools::Long RenderContext2::GetYMapNumerator() const { return maMappingMetric.mnMapScNumY; }
void RenderContext2::SetYMapNumerator(tools::Long nNumerator)
{
    maMappingMetric.mnMapScNumY = nNumerator;
}

tools::Long RenderContext2::GetXMapDenominator() const { return maMappingMetric.mnMapScDenomX; }
void RenderContext2::SetXMapDenominator(tools::Long nDenomerator)
{
    maMappingMetric.mnMapScDenomX = nDenomerator;
}

tools::Long RenderContext2::GetYMapDenominator() const { return maMappingMetric.mnMapScDenomY; }
void RenderContext2::SetYMapDenominator(tools::Long nDenomerator)
{
    maMappingMetric.mnMapScDenomY = nDenomerator;
}

MappingMetrics RenderContext2::GetMappingMetrics() const { return maMappingMetric; }

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
