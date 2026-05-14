
/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <tools/long.hxx>
#include <tools/gen.hxx>

#include <vcl/mapmod.hxx>

#include <MappingCoefficients.hxx>

namespace vcl
{
namespace detail
{
struct MapConversion
{
    double mfScaleX = 1.0;
    double mfScaleY = 1.0;
    tools::Long mnOffsetX = 0;
    tools::Long mnOffsetY = 0;
};
}

/**
 * Single source of truth for all spatial configuration.
 * Strictly holds persistent state (DPI, Offsets, Legacy MapMode Config).
 * Contains NO matrix compilation or geometry transformation logic.
 */
class CoordinateState
{
private:
    sal_Int32 mnDPIX = 72;
    sal_Int32 mnDPIY = 72;
    sal_Int32 mnDPIScalePercentage = 100;

    tools::Long mnWindowToViewOffsetX = 0;
    tools::Long mnWindowToViewOffsetY = 0;
    tools::Long mnDeviceToWindowOffsetX = 0;
    tools::Long mnDeviceToWindowOffsetY = 0;
    tools::Long mnLogicToAbsoluteOffsetX = 0;
    tools::Long mnLogicToAbsoluteOffsetY = 0;

    MappingCoefficients maMapRes;
    vcl::detail::MapConversion maMapConversion;

public:
    // -- Getters --
    sal_Int32 GetDPIX() const { return mnDPIX; }
    sal_Int32 GetDPIY() const { return mnDPIY; }
    sal_Int32 GetDPIScalePercentage() const { return mnDPIScalePercentage; }
    float GetDPIScaleFactor() const { return mnDPIScalePercentage / 100.0f; }

    tools::Long GetWindowToViewOffsetX() const { return mnWindowToViewOffsetX; }
    tools::Long GetWindowToViewOffsetY() const { return mnWindowToViewOffsetY; }
    tools::Long GetDeviceToWindowOffsetX() const { return mnDeviceToWindowOffsetX; }
    tools::Long GetDeviceToWindowOffsetY() const { return mnDeviceToWindowOffsetY; }
    tools::Long GetLogicToAbsoluteOffsetX() const { return mnLogicToAbsoluteOffsetX; }
    tools::Long GetLogicToAbsoluteOffsetY() const { return mnLogicToAbsoluteOffsetY; }

    const MappingCoefficients& GetMapRes() const { return maMapRes; }
    const vcl::detail::MapConversion& GetMapConversion() const { return maMapConversion; }

    // -- Setters --
    void SetDPIX(sal_Int32 nDPIX) { mnDPIX = nDPIX; }
    void SetDPIY(sal_Int32 nDPIY) { mnDPIY = nDPIY; }
    void SetDPIScalePercentage(sal_Int32 nPercent) { mnDPIScalePercentage = nPercent; }

    void SetWindowToViewOffset(const Size& rOffset)
    {
        mnWindowToViewOffsetX = rOffset.Width();
        mnWindowToViewOffsetY = rOffset.Height();
    }
    void SetDeviceToWindowOffset(tools::Long nX, tools::Long nY)
    {
        mnDeviceToWindowOffsetX = nX;
        mnDeviceToWindowOffsetY = nY;
    }
    void SetLogicToAbsoluteOffset(const Size& rOffset)
    {
        mnLogicToAbsoluteOffsetX = rOffset.Width();
        mnLogicToAbsoluteOffsetY = rOffset.Height();
    }

    void SetMappingXOffset(tools::Long nOffset)
    {
        maMapRes.mnTranslationX = nOffset;
        maMapConversion.mnOffsetX = nOffset; // Sync legacy firewall
    }

    void SetMappingYOffset(tools::Long nOffset)
    {
        maMapRes.mnTranslationY = nOffset;
        maMapConversion.mnOffsetY = nOffset; // Sync legacy firewall
    }

    void SetMapResolutionScaleX(double fX)
    {
        maMapRes.mfScaleX = fX;
        maMapConversion.mfScaleX = fX; // Sync legacy firewall
    }

    void SetMapResolutionScaleY(double fY)
    {
        maMapRes.mfScaleY = fY;
        maMapConversion.mfScaleY = fY; // Sync legacy firewall
    }

    // -- State Resolution Boundary --
    // Moved from CoordinateMapper: Isolates the MapMode side-effects here.
    void UpdateFromMapMode(const MapMode& rMapMode)
    {
        maMapRes.CalcMapResolution(rMapMode, mnDPIX, mnDPIY);
        maMapConversion.mfScaleX = maMapRes.mfScaleX;
        maMapConversion.mfScaleY = maMapRes.mfScaleY;
        maMapConversion.mnOffsetX = maMapRes.mnTranslationX;
        maMapConversion.mnOffsetY = maMapRes.mnTranslationY;
    }

    MappingCoefficients ResolveMapResRelative(const MapMode* pTarget, const MapMode* pBaseline,
                                              vcl::MappingPolicy ePolicy) const
    {
        return maMapRes.ResolveMapRes(pTarget, *pBaseline, ePolicy, mnDPIX, mnDPIY);
    }
};

} // namespace vcl

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
