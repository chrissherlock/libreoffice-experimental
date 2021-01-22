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

#include <rtl/instance.hxx>
#include <tools/bigint.hxx>
#include <tools/gen.hxx>
#include <tools/fract.hxx>
#include <tools/mapunit.hxx>
#include <tools/stream.hxx>
#include <tools/vcompat.hxx>

#include <vcl/mapmod.hxx>
#include <vcl/Geometry.hxx>
#include <vcl/MappingMetrics.hxx>
#include <vcl/TypeSerializer.hxx>

struct MapMode::ImplMapMode
{
    MapUnit         meUnit;
    Point           maOrigin;
    // NOTE: these Fraction must NOT have more than 32 bits precision
    // because ReadFraction / WriteFraction do only 32 bits, so more than
    // that cannot be stored in MetaFiles!
    // => call ReduceInaccurate whenever setting these
    Fraction        maScaleX;
    Fraction        maScaleY;
    bool            mbSimple;

    ImplMapMode();
    ImplMapMode(const ImplMapMode& rImpMapMode);

    bool operator==( const ImplMapMode& rImpMapMode ) const;
};

MapMode::ImplMapMode::ImplMapMode() :
    maOrigin( 0, 0 ),
    maScaleX( 1, 1 ),
    maScaleY( 1, 1 )
{
    meUnit   = MapUnit::MapPixel;
    mbSimple = true;
}

MapMode::ImplMapMode::ImplMapMode( const ImplMapMode& ) = default;

bool MapMode::ImplMapMode::operator==( const ImplMapMode& rImpMapMode ) const
{
    return meUnit == rImpMapMode.meUnit
        && maOrigin == rImpMapMode.maOrigin
        && maScaleX == rImpMapMode.maScaleX
        && maScaleY == rImpMapMode.maScaleY;
}

namespace
{
    struct theGlobalDefault :
        public rtl::Static< MapMode::ImplType, theGlobalDefault > {};
}

MapMode::MapMode() : mpImplMapMode(theGlobalDefault::get())
{
}

MapMode::MapMode( const MapMode& ) = default;

MapMode::MapMode( MapUnit eUnit ) : mpImplMapMode()
{
    mpImplMapMode->meUnit = eUnit;
}

MapMode::MapMode( MapUnit eUnit, const Point& rLogicOrg,
                  const Fraction& rScaleX, const Fraction& rScaleY )
{
    mpImplMapMode->meUnit   = eUnit;
    mpImplMapMode->maOrigin = rLogicOrg;
    mpImplMapMode->maScaleX = rScaleX;
    mpImplMapMode->maScaleY = rScaleY;
    mpImplMapMode->maScaleX.ReduceInaccurate(32);
    mpImplMapMode->maScaleY.ReduceInaccurate(32);
    mpImplMapMode->mbSimple = false;
}

MapMode::~MapMode() = default;

void MapMode::SetMapUnit( MapUnit eUnit )
{
    mpImplMapMode->meUnit = eUnit;
}

void MapMode::SetOrigin( const Point& rLogicOrg )
{
    mpImplMapMode->maOrigin = rLogicOrg;
    mpImplMapMode->mbSimple = false;
}

void MapMode::SetScaleX( const Fraction& rScaleX )
{
    mpImplMapMode->maScaleX = rScaleX;
    mpImplMapMode->maScaleX.ReduceInaccurate(32);
    mpImplMapMode->mbSimple = false;
}

void MapMode::SetScaleY( const Fraction& rScaleY )
{
    mpImplMapMode->maScaleY = rScaleY;
    mpImplMapMode->maScaleY.ReduceInaccurate(32);
    mpImplMapMode->mbSimple = false;
}

MapMode& MapMode::operator=( const MapMode& ) = default;

MapMode& MapMode::operator=( MapMode&& ) = default;

bool MapMode::operator==( const MapMode& rMapMode ) const
{
   return mpImplMapMode == rMapMode.mpImplMapMode;
}

bool MapMode::IsDefault() const
{
    return mpImplMapMode.same_object(theGlobalDefault::get());
}

SvStream& ReadMapMode( SvStream& rIStm, MapMode& rMapMode )
{
    VersionCompatRead aCompat(rIStm);
    sal_uInt16    nTmp16;

    TypeSerializer aSerializer(rIStm);

    rIStm.ReadUInt16( nTmp16 ); rMapMode.mpImplMapMode->meUnit = static_cast<MapUnit>(nTmp16);
    aSerializer.readPoint(rMapMode.mpImplMapMode->maOrigin);
    ReadFraction( rIStm, rMapMode.mpImplMapMode->maScaleX );
    ReadFraction( rIStm, rMapMode.mpImplMapMode->maScaleY );
    rIStm.ReadCharAsBool( rMapMode.mpImplMapMode->mbSimple );

    return rIStm;
}

SvStream& WriteMapMode( SvStream& rOStm, const MapMode& rMapMode )
{
    VersionCompatWrite aCompat(rOStm, 1);

    TypeSerializer aSerializer(rOStm);

    rOStm.WriteUInt16( static_cast<sal_uInt16>(rMapMode.mpImplMapMode->meUnit) );
    aSerializer.writePoint(rMapMode.mpImplMapMode->maOrigin);
    WriteFraction( rOStm, rMapMode.mpImplMapMode->maScaleX );
    WriteFraction( rOStm, rMapMode.mpImplMapMode->maScaleY );
    rOStm.WriteBool( rMapMode.mpImplMapMode->mbSimple );

    return rOStm;
}


MapUnit MapMode::GetMapUnit() const { return mpImplMapMode->meUnit; }

const Point& MapMode::GetOrigin() const { return mpImplMapMode->maOrigin; }

const Fraction& MapMode::GetScaleX() const { return mpImplMapMode->maScaleX; }

const Fraction& MapMode::GetScaleY() const { return mpImplMapMode->maScaleY; }

bool MapMode::IsSimple() const { return mpImplMapMode->mbSimple; }

std::tuple<MappingMetrics, MappingMetrics> MapMode::GetMappingMetrics(MapMode const& rMapMode, Geometry const& rGeometry) const
{
    MappingMetrics aMappingMetricSource;

    if (!rGeometry.IsMapModeEnabled())
    {
        if (GetMapUnit() == MapUnit::MapRelative)
            aMappingMetricSource = rGeometry.GetMappingMetrics();

        aMappingMetricSource.Calculate(*this, rGeometry.GetDPIX(), rGeometry.GetDPIY());
    }
    else
    {
        aMappingMetricSource = rGeometry.GetMappingMetrics();
    }

    MappingMetrics aMappingMetricDest;

    if (!rGeometry.IsMapModeEnabled() || rMapMode != *this)
    {
        if (rMapMode.GetMapUnit() == MapUnit::MapRelative)
            aMappingMetricDest = rGeometry.GetMappingMetrics();

        aMappingMetricDest.Calculate(rMapMode, rGeometry.GetDPIX(), rGeometry.GetDPIY());
    }
    else
    {
        aMappingMetricDest = rGeometry.GetMappingMetrics();
    }

    return std::make_tuple(aMappingMetricSource, aMappingMetricDest);
}

Point MapMode::MapTo(MapMode const& rMapMode, Point const& rPtSource, Geometry const& rGeometry) const
{
    if (*this == rMapMode)
        return rPtSource;

    MappingMetrics aMappingMetricSource;
    MappingMetrics aMappingMetricDest;

    std::tie(aMappingMetricSource, aMappingMetricDest) = GetMappingMetrics(rMapMode, rGeometry);

    return Point(MapMode::fn5(rPtSource.X() + aMappingMetricSource.mnMapOfsX,
                     aMappingMetricSource.mnMapScNumX, aMappingMetricDest.mnMapScDenomX,
                     aMappingMetricSource.mnMapScDenomX, aMappingMetricDest.mnMapScNumX)
                     - aMappingMetricDest.mnMapOfsX,
                 MapMode::fn5(rPtSource.Y() + aMappingMetricSource.mnMapOfsY,
                     aMappingMetricSource.mnMapScNumY, aMappingMetricDest.mnMapScDenomY,
                     aMappingMetricSource.mnMapScDenomY, aMappingMetricDest.mnMapScNumY)
                     - aMappingMetricDest.mnMapOfsY);
}

Size MapMode::MapTo(MapMode const& rMapMode, Size const& rSzSource, Geometry const& rGeometry) const
{
    if (*this == rMapMode)
        return rSzSource;

    MappingMetrics aMappingMetricSource;
    MappingMetrics aMappingMetricDest;

    std::tie(aMappingMetricSource, aMappingMetricDest) = GetMappingMetrics(rMapMode, rGeometry);

    return Size(
        MapMode::fn5(rSzSource.Width(), aMappingMetricSource.mnMapScNumX, aMappingMetricDest.mnMapScDenomX,
            aMappingMetricSource.mnMapScDenomX, aMappingMetricDest.mnMapScNumX),
        MapMode::fn5(rSzSource.Height(), aMappingMetricSource.mnMapScNumY, aMappingMetricDest.mnMapScDenomY,
            aMappingMetricSource.mnMapScDenomY, aMappingMetricDest.mnMapScNumY));
}

tools::Rectangle MapMode::MapTo(MapMode const& rMapMode, tools::Rectangle const& rRectSource, Geometry const& rGeometry) const
{
    if (*this == rMapMode)
        return rRectSource;

    MappingMetrics aMappingMetricSource;
    MappingMetrics aMappingMetricDest;

    std::tie(aMappingMetricSource, aMappingMetricDest) = GetMappingMetrics(rMapMode, rGeometry);

    return tools::Rectangle(MapMode::fn5(rRectSource.Left() + aMappingMetricSource.mnMapOfsX,
                                aMappingMetricSource.mnMapScNumX, aMappingMetricDest.mnMapScDenomX,
                                aMappingMetricSource.mnMapScDenomX, aMappingMetricDest.mnMapScNumX)
                                - aMappingMetricDest.mnMapOfsX,
                            MapMode::fn5(rRectSource.Top() + aMappingMetricSource.mnMapOfsY,
                                aMappingMetricSource.mnMapScNumY, aMappingMetricDest.mnMapScDenomY,
                                aMappingMetricSource.mnMapScDenomY, aMappingMetricDest.mnMapScNumY)
                                - aMappingMetricDest.mnMapOfsY,
                            MapMode::fn5(rRectSource.Right() + aMappingMetricSource.mnMapOfsX,
                                aMappingMetricSource.mnMapScNumX, aMappingMetricDest.mnMapScDenomX,
                                aMappingMetricSource.mnMapScDenomX, aMappingMetricDest.mnMapScNumX)
                                - aMappingMetricDest.mnMapOfsX,
                            MapMode::fn5(rRectSource.Bottom() + aMappingMetricSource.mnMapOfsY,
                                aMappingMetricSource.mnMapScNumY, aMappingMetricDest.mnMapScDenomY,
                                aMappingMetricSource.mnMapScDenomY, aMappingMetricDest.mnMapScNumY)
                                - aMappingMetricDest.mnMapOfsY);
}

// return (n1 * n2 * n3) / (n4 * n5)
tools::Long MapMode::fn5(const tools::Long n1, const tools::Long n2, const tools::Long n3,
                          const tools::Long n4, const tools::Long n5)
{
    if (n1 == 0 || n2 == 0 || n3 == 0 || n4 == 0 || n5 == 0)
        return 0;
    if (std::numeric_limits<tools::Long>::max() / std::abs(n2) < std::abs(n3))
    {
        // a6 is skipped
        BigInt a7 = n2;
        a7 *= n3;
        a7 *= n1;

        if (std::numeric_limits<tools::Long>::max() / std::abs(n4) < std::abs(n5))
        {
            BigInt a8 = n4;
            a8 *= n5;

            BigInt a9 = a8;
            a9 /= 2;
            if (a7.IsNeg())
                a7 -= a9;
            else
                a7 += a9;

            a7 /= a8;
        }
        else
        {
            tools::Long n8 = n4 * n5;

            if (a7.IsNeg())
                a7 -= n8 / 2;
            else
                a7 += n8 / 2;

            a7 /= n8;
        }

        return static_cast<tools::Long>(a7);
    }
    else
    {
        tools::Long n6 = n2 * n3;

        if (std::numeric_limits<tools::Long>::max() / std::abs(n1) < std::abs(n6))
        {
            BigInt a7 = n1;
            a7 *= n6;

            if (std::numeric_limits<tools::Long>::max() / std::abs(n4) < std::abs(n5))
            {
                BigInt a8 = n4;
                a8 *= n5;

                BigInt a9 = a8;
                a9 /= 2;
                if (a7.IsNeg())
                    a7 -= a9;
                else
                    a7 += a9;

                a7 /= a8;
            }
            else
            {
                tools::Long n8 = n4 * n5;

                if (a7.IsNeg())
                    a7 -= n8 / 2;
                else
                    a7 += n8 / 2;

                a7 /= n8;
            }
            return static_cast<tools::Long>(a7);
        }
        else
        {
            tools::Long n7 = n1 * n6;

            if (std::numeric_limits<tools::Long>::max() / std::abs(n4) < std::abs(n5))
            {
                BigInt a7 = n7;
                BigInt a8 = n4;
                a8 *= n5;

                BigInt a9 = a8;
                a9 /= 2;

                if (a7.IsNeg())
                    a7 -= a9;
                else
                    a7 += a9;

                a7 /= a8;

                return static_cast<tools::Long>(a7);
            }
            else
            {
                const tools::Long n8 = n4 * n5;
                const tools::Long n8_2 = n8 / 2;

                if (n7 < 0)
                {
                    if ((n7 - std::numeric_limits<tools::Long>::min()) >= n8_2)
                        n7 -= n8_2;
                }
                else if ((std::numeric_limits<tools::Long>::max() - n7) >= n8_2)
                {
                    n7 += n8_2;
                }

                return n7 / n8;
            }
        }
    }
}

// return (n1 * n2) / n3
tools::Long MapMode::fn3(const tools::Long n1, const tools::Long n2, const tools::Long n3)
{
    if (n1 == 0 || n2 == 0 || n3 == 0)
        return 0;

    if (std::numeric_limits<tools::Long>::max() / std::abs(n1) < std::abs(n2))
    {
        BigInt a4 = n1;
        a4 *= n2;

        if (a4.IsNeg())
            a4 -= n3 / 2;
        else
            a4 += n3 / 2;

        a4 /= n3;

        return static_cast<tools::Long>(a4);
    }
    else
    {
        tools::Long n4 = n1 * n2;
        const tools::Long n3_2 = n3 / 2;

        if (n4 < 0)
        {
            if ((n4 - std::numeric_limits<tools::Long>::min()) >= n3_2)
                n4 -= n3_2;
        }
        else if ((std::numeric_limits<tools::Long>::max() - n4) >= n3_2)
        {
            n4 += n3_2;
        }

        return n4 / n3;
    }
}
/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
