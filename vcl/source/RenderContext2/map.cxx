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

bool RenderContext2::IsMapModeEnabled() const { return mbMap; }

void RenderContext2::EnableMapMode(bool bEnable) { mbMap = bEnable; }

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
    return Size(mnOffsetFromOriginXpx, mnOffsetFromOriginYpx);
}

tools::Long RenderContext2::GetXOffsetFromOriginInPixels() const { return mnOffsetFromOriginXpx; }

tools::Long RenderContext2::GetYOffsetFromOriginInPixels() const { return mnOffsetFromOriginYpx; }

void RenderContext2::SetOffsetFromOriginInPixels(Size const& rOffset)
{
    mnOffsetFromOriginXpx = rOffset.Width();
    mnOffsetFromOriginYpx = rOffset.Height();

    SetOffsetFromOriginXInLogicalUnits(ImplPixelToLogic(
        mnOffsetFromOriginXpx, mnDPIX, maMappingMetric.mnMapScNumX, maMappingMetric.mnMapScDenomX));
    SetOffsetFromOriginYInLogicalUnits(ImplPixelToLogic(
        mnOffsetFromOriginYpx, mnDPIY, maMappingMetric.mnMapScNumY, maMappingMetric.mnMapScDenomY));
}

sal_uInt32 RenderContext2::GetOffsetFromOriginXInLogicalUnits() const
{
    return mnOffsetFromOriginXInLogicalUnits;
}

void RenderContext2::SetOffsetFromOriginXInLogicalUnits(
    tools::Long nOffsetFromOriginXInLogicalUnits)
{
    mnOffsetFromOriginXInLogicalUnits = nOffsetFromOriginXInLogicalUnits;
}

sal_uInt32 RenderContext2::GetOffsetFromOriginYInLogicalUnits() const
{
    return mnOffsetFromOriginYInLogicalUnits;
}

void RenderContext2::SetOffsetFromOriginYInLogicalUnits(
    tools::Long nOffsetFromOriginYInLogicalUnits)
{
    mnOffsetFromOriginYInLogicalUnits = nOffsetFromOriginYInLogicalUnits;
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
