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

#pragma once

#include <vcl/dllapi.h>
#include <tools/gen.hxx>
#include <tools/poly.hxx>
#include <basegfx/polygon/b2dpolypolygon.hxx>
#include <memory>
#include <optional>
#include <vector>
#include <cassert>

class RegionBand;

namespace vcl
{
typedef std::vector<tools::Rectangle> RectangleVector;

// TRUE IMMUTABLE SNAPSHOT
struct RegionData
{
    const std::optional<basegfx::B2DPolyPolygon> mpB2DPolyPolygon;
    const std::optional<tools::PolyPolygon> mpPolyPolygon;
    const std::shared_ptr<const RegionBand> mpRegionBand;
    const bool mbIsNull;

    // Constructors enforce creation-time population only.
    RegionData(bool bNull = false)
        : mbIsNull(bNull)
    {
        assert(!(mbIsNull && (mpB2DPolyPolygon || mpPolyPolygon || mpRegionBand))
               && "Null region cannot contain geometry");
    }

    RegionData(basegfx::B2DPolyPolygon aPoly)
        : mpB2DPolyPolygon(std::move(aPoly))
        , mbIsNull(false)
    {
    }

    RegionData(tools::PolyPolygon aPoly)
        : mpPolyPolygon(std::move(aPoly))
        , mbIsNull(false)
    {
    }

    RegionData(std::shared_ptr<const RegionBand> pBand)
        : mpRegionBand(std::move(pBand))
        , mbIsNull(false)
    {
    }

    // Multi-representation constructors for safe lazy evaluation and complex reading
    RegionData(basegfx::B2DPolyPolygon aPoly, std::shared_ptr<const RegionBand> pBand)
        : mpB2DPolyPolygon(std::move(aPoly))
        , mpRegionBand(std::move(pBand))
        , mbIsNull(false)
    {
    }

    RegionData(tools::PolyPolygon aPoly, std::shared_ptr<const RegionBand> pBand)
        : mpPolyPolygon(std::move(aPoly))
        , mpRegionBand(std::move(pBand))
        , mbIsNull(false)
    {
    }

    RegionData(const RegionData&) = default;
};

} // namespace vcl

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
