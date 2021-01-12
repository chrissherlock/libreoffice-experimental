/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <sal/log.hxx>
#include <tools/fract.hxx>

#include <vcl/RenderContext2.hxx>

void RenderContext2::InitMapModeObjects() {}

bool RenderContext2::IsMapModeEnabled() const { return maGeometry.IsMapModeEnabled(); }

void RenderContext2::EnableMapMode() { maGeometry.EnableMapMode(); }

void RenderContext2::DisableMapMode() { maGeometry.DisableMapMode(); }

MapMode const& RenderContext2::GetMapMode() const { return maMapMode; }

void RenderContext2::SetMapMode()
{
    if (IsMapModeEnabled() || !maMapMode.IsDefault())
    {
        DisableMapMode();
        maMapMode = MapMode();

        // create new objects (clip region are not re-scaled)
        SetNewFontFlag(true);
        SetInitFontFlag(true);
        InitMapModeObjects();

        // #106426# Adapt logical offset when changing mapmode
        ResetLogicalUnitsOffsetFromOrigin(); // no mapping -> equal offsets

        // #i75163#
        maGeometry.InvalidateViewTransform();
    }
}

/*
Reduces accuracy until it is a fraction (should become
ctor fraction once); we could also do this with BigInts
*/

static Fraction ImplMakeFraction(tools::Long nN1, tools::Long nN2, tools::Long nD1, tools::Long nD2)
{
    if (nD1 == 0
        || nD2 == 0) //under these bad circumstances the following while loop will be endless
    {
        SAL_WARN("vcl.gdi", "Invalid parameter for ImplMakeFraction");
        return Fraction(1, 1);
    }

    tools::Long i = 1;

    if (nN1 < 0)
    {
        i = -i;
        nN1 = -nN1;
    }
    if (nN2 < 0)
    {
        i = -i;
        nN2 = -nN2;
    }
    if (nD1 < 0)
    {
        i = -i;
        nD1 = -nD1;
    }
    if (nD2 < 0)
    {
        i = -i;
        nD2 = -nD2;
    }
    // all positive; i sign

    Fraction aF = Fraction(i * nN1, nD1) * Fraction(nN2, nD2);

    while (!aF.IsValid())
    {
        if (nN1 > nN2)
            nN1 = (nN1 + 1) / 2;
        else
            nN2 = (nN2 + 1) / 2;
        if (nD1 > nD2)
            nD1 = (nD1 + 1) / 2;
        else
            nD2 = (nD2 + 1) / 2;

        aF = Fraction(i * nN1, nD1) * Fraction(nN2, nD2);
    }

    aF.ReduceInaccurate(32);

    return aF;
}

void RenderContext2::SetMapMode(MapMode const& rNewMapMode)
{
    // do nothing if MapMode was not changed
    if (maMapMode == rNewMapMode)
        return;

    // if default MapMode calculate nothing
    bool bOldMap = IsMapModeEnabled();

    if (!rNewMapMode.IsDefault())
        EnableMapMode();
    else
        DisableMapMode();

    if (IsMapModeEnabled())
    {
        // if only the origin is converted, do not scale new
        if ((rNewMapMode.GetMapUnit() == maMapMode.GetMapUnit())
            && (rNewMapMode.GetScaleX() == maMapMode.GetScaleX())
            && (rNewMapMode.GetScaleY() == maMapMode.GetScaleY())
            && (bOldMap == IsMapModeEnabled()))
        {
            // set offset
            Point aOrigin = rNewMapMode.GetOrigin();
            SetXMapOffset(aOrigin.X());
            SetYMapOffset(aOrigin.Y());
            maMapMode = rNewMapMode;

            // #i75163#
            maGeometry.InvalidateViewTransform();

            return;
        }
        if (!bOldMap && (rNewMapMode.GetMapUnit() == MapUnit::MapRelative))
        {
            SetXMapNumerator(1);
            SetYMapNumerator(1);
            SetXMapDenominator(GetDPIX());
            SetYMapDenominator(GetDPIY());
            SetXMapOffset(0);
            SetYMapOffset(0);
        }

        // calculate new MapMode-resolution
        maGeometry.CalculateMappingMetrics(rNewMapMode, GetDPIX(), GetDPIY());
    }

    // set new MapMode
    if ((rNewMapMode.GetMapUnit() == MapUnit::MapRelative))
    {
        Point aOrigin(GetXMapOffset(), GetYMapOffset());
        // aScale? = maMapMode.GetScale?() * rNewMapMode.GetScale?()
        Fraction aScaleX = ImplMakeFraction(
            maMapMode.GetScaleX().GetNumerator(), rNewMapMode.GetScaleX().GetNumerator(),
            maMapMode.GetScaleX().GetDenominator(), rNewMapMode.GetScaleX().GetDenominator());
        Fraction aScaleY = ImplMakeFraction(
            maMapMode.GetScaleY().GetNumerator(), rNewMapMode.GetScaleY().GetNumerator(),
            maMapMode.GetScaleY().GetDenominator(), rNewMapMode.GetScaleY().GetDenominator());
        maMapMode.SetOrigin(aOrigin);
        maMapMode.SetScaleX(aScaleX);
        maMapMode.SetScaleY(aScaleY);
    }
    else
    {
        maMapMode = rNewMapMode;
    }

    // create new objects (clip region are not re-scaled)
    SetNewFontFlag(true);
    SetInitFontFlag(true);
    InitMapModeObjects();

    // #106426# Adapt logical offset when changing mapmode
    SetXOffsetFromOriginInLogicalUnits(maGeometry.PixelToLogic(
        GetXOffsetFromOriginInPixels(), GetDPIX(), GetXMapNumerator(), GetXMapDenominator()));
    SetYOffsetFromOriginInLogicalUnits(maGeometry.PixelToLogic(
        GetYOffsetFromOriginInPixels(), GetDPIY(), GetYMapNumerator(), GetYMapDenominator()));

    // #i75163#
    maGeometry.InvalidateViewTransform();
}

void RenderContext2::ResetLogicalUnitsOffsetFromOrigin()
{
    maGeometry.ResetLogicalUnitsOffsetFromOrigin();
}

Size RenderContext2::GetOffsetFromOriginInPixels() const
{
    return maGeometry.GetOffsetFromOriginInPixels();
}

tools::Long RenderContext2::GetXOffsetFromOriginInPixels() const
{
    return maGeometry.GetXOffsetFromOriginInPixels();
}

tools::Long RenderContext2::GetYOffsetFromOriginInPixels() const
{
    return maGeometry.GetYOffsetFromOriginInPixels();
}

void RenderContext2::SetOffsetFromOriginInPixels(Size const& rOffset)
{
    maGeometry.SetOffsetFromOriginInPixels(rOffset);
}

sal_uInt32 RenderContext2::GetXOffsetFromOriginInLogicalUnits() const
{
    return maGeometry.GetXOffsetFromOriginInLogicalUnits();
}

void RenderContext2::SetXOffsetFromOriginInLogicalUnits(
    tools::Long nOffsetFromOriginXInLogicalUnits)
{
    maGeometry.SetXOffsetFromOriginInLogicalUnits(nOffsetFromOriginXInLogicalUnits);
}

sal_uInt32 RenderContext2::GetYOffsetFromOriginInLogicalUnits() const
{
    return maGeometry.GetYOffsetFromOriginInLogicalUnits();
}

void RenderContext2::SetYOffsetFromOriginInLogicalUnits(
    tools::Long nOffsetFromOriginYInLogicalUnits)
{
    maGeometry.SetYOffsetFromOriginInLogicalUnits(nOffsetFromOriginYInLogicalUnits);
}

tools::Long RenderContext2::GetXMapOffset() const { return maGeometry.GetXMapOffset(); }
void RenderContext2::SetXMapOffset(tools::Long nXOffset) { maGeometry.SetXMapOffset(nXOffset); }

tools::Long RenderContext2::GetYMapOffset() const { return maGeometry.GetYMapOffset(); }
void RenderContext2::SetYMapOffset(tools::Long nYOffset) { maGeometry.SetYMapOffset(nYOffset); }

tools::Long RenderContext2::GetXMapNumerator() const { return maGeometry.GetXMapNumerator(); }
void RenderContext2::SetXMapNumerator(tools::Long nNumerator)
{
    maGeometry.SetXMapNumerator(nNumerator);
}

tools::Long RenderContext2::GetYMapNumerator() const { return maGeometry.GetYMapNumerator(); }
void RenderContext2::SetYMapNumerator(tools::Long nNumerator)
{
    maGeometry.SetYMapNumerator(nNumerator);
}

tools::Long RenderContext2::GetXMapDenominator() const { return maGeometry.GetXMapDenominator(); }
void RenderContext2::SetXMapDenominator(tools::Long nDenomerator)
{
    maGeometry.SetXMapDenominator(nDenomerator);
}

tools::Long RenderContext2::GetYMapDenominator() const { return maGeometry.GetYMapDenominator(); }

void RenderContext2::SetYMapDenominator(tools::Long nDenomerator)
{
    maGeometry.SetYMapDenominator(nDenomerator);
}

MappingMetrics RenderContext2::GetMappingMetrics() const { return maGeometry.GetMappingMetrics(); }

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
