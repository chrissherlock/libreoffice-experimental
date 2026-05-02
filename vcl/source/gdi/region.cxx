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
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements. See the NOTICE file distributed
 * with this work for additional information regarding copyright
 * ownership. The ASF licenses this file to you under the Apache
 * License, Version 2.0 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.apache.org/licenses/LICENSE-2.0 .
 */

#include <memory>
#include <tools/vcompat.hxx>
#include <tools/stream.hxx>
#include <osl/diagnose.h>
#include <sal/log.hxx>
#include <vcl/canvastools.hxx>
#include <vcl/region.hxx>
#include <vcl/regiondata.hxx>
#include <regionband.hxx>

#include <basegfx/polygon/b2dpolypolygontools.hxx>
#include <basegfx/polygon/b2dpolygontools.hxx>
#include <basegfx/polygon/b2dpolygonclipper.hxx>
#include <basegfx/polygon/b2dpolypolygoncutter.hxx>
#include <basegfx/range/b2drange.hxx>
#include <basegfx/matrix/b2dhommatrixtools.hxx>
#include <tools/poly.hxx>
#include <comphelper/configuration.hxx>

/** Return <TRUE/> when the given polygon is rectilinear and oriented so that
    all sides are either horizontal or vertical.
*/
static bool lcl_IsPolygonRectilinear (const tools::PolyPolygon& rPolyPoly)
{
    // Iterate over all polygons.
    const sal_uInt16 nPolyCount = rPolyPoly.Count();
    for (sal_uInt16 nPoly = 0; nPoly < nPolyCount; ++nPoly)
    {
        const tools::Polygon&  aPoly = rPolyPoly.GetObject(nPoly);

        // Iterate over all edges of the current polygon.
        const sal_uInt16 nSize = aPoly.GetSize();

        if (nSize < 2)
            continue;
        Point aPoint (aPoly.GetPoint(0));
        const Point aLastPoint (aPoint);
        for (sal_uInt16 nPoint = 1; nPoint < nSize; ++nPoint)
        {
            const Point aNextPoint (aPoly.GetPoint(nPoint));
            // When there is at least one edge that is neither vertical nor
            // horizontal then the entire polygon is not rectilinear (and
            // oriented along primary axes.)
            if (aPoint.X() != aNextPoint.X() && aPoint.Y() != aNextPoint.Y())
                return false;

            aPoint = aNextPoint;
        }
        // Compare closing edge.
        if (aLastPoint.X() != aPoint.X() && aLastPoint.Y() != aPoint.Y())
            return false;
    }
    return true;
}

/** Convert a rectilinear polygon (that is oriented along the primary axes)
    to a list of bands.  For this special form of polygon we can use an
    optimization that prevents the creation of one band per y value.
    However, it still is possible that some temporary bands are created that
    later can be optimized away.
*/
static std::shared_ptr<RegionBand> lcl_RectilinearPolygonToBands(const tools::PolyPolygon& rPolyPoly)
{
    OSL_ASSERT(lcl_IsPolygonRectilinear (rPolyPoly));

    // Create a new RegionBand object as container of the bands.
    std::shared_ptr<RegionBand> pRegionBand( std::make_shared<RegionBand>() );
    tools::Long nLineId = 0;

    // Iterate over all polygons.
    const sal_uInt16 nPolyCount = rPolyPoly.Count();
    for (sal_uInt16 nPoly = 0; nPoly < nPolyCount; ++nPoly)
    {
        const tools::Polygon&  aPoly = rPolyPoly.GetObject(nPoly);

        // Iterate over all edges of the current polygon.
        const sal_uInt16 nSize = aPoly.GetSize();
        if (nSize < 2)
            continue;
        Point aStart (aPoly.GetPoint(0));
        Point aEnd;
        for (sal_uInt16 nPoint = 1; nPoint <= nSize; ++nPoint, aStart=aEnd)
        {
            aEnd = aPoly.GetPoint(nPoint%nSize);
            if (aStart.Y() == aEnd.Y())
            {
                continue;
            }

            OSL_ASSERT(aStart.X() == aEnd.X());

            const tools::Long nTop (::std::min(aStart.Y(), aEnd.Y()));
            const tools::Long nBottom (::std::max(aStart.Y(), aEnd.Y()));
            const LineType eLineType (aStart.Y() > aEnd.Y() ? LineType::Descending : LineType::Ascending);

            pRegionBand->ImplAddMissingBands(nTop,nBottom);

            ImplRegionBand* pBand = pRegionBand->ImplGetFirstRegionBand();
            while (pBand!=nullptr && pBand->mnYBottom < nTop)
                pBand = pBand->mpNextBand;
            ImplRegionBand* pTopBand = pBand;
            if (pBand!=nullptr
                && pBand->mnYTop<nTop
                && pBand->mnYBottom>=nTop
                && pBand->mnYTop<pBand->mnYBottom-1)
            {
                pTopBand = pBand->SplitBand(nTop);
            }

            while (pBand!=nullptr && pBand->mnYBottom < nBottom)
                pBand = pBand->mpNextBand;
            if (pBand!=nullptr
                && pBand->mnYTop<=nBottom
                && pBand->mnYBottom>nBottom
                && pBand->mnYTop<pBand->mnYBottom-1)
            {
                pBand->SplitBand(nBottom+1);
            }

            for (pBand=pTopBand; pBand!=nullptr&&pBand->mnYTop<=nBottom; pBand=pBand->mpNextBand)
                pBand->InsertPoint(aStart.X(), nLineId++, true, eLineType);
        }
    }

    return pRegionBand;
}

static std::shared_ptr<RegionBand> lcl_GeneralPolygonToBands(const tools::PolyPolygon& rPolyPoly, const tools::Rectangle& rPolygonBoundingBox)
{
    tools::Long nLineID = 0;

    std::shared_ptr<RegionBand> pRegionBand( std::make_shared<RegionBand>() );
    pRegionBand->CreateBandRange(rPolygonBoundingBox.Top(), rPolygonBoundingBox.Bottom());

    const sal_uInt16 nPolyCount = rPolyPoly.Count();

    for ( sal_uInt16 nPoly = 0; nPoly < nPolyCount; nPoly++ )
    {
        const tools::Polygon&  aPoly = rPolyPoly.GetObject( nPoly );
        const sal_uInt16    nSize = aPoly.GetSize();

        if ( nSize <= 2 )
            continue;

        for ( sal_uInt16 nPoint = 1; nPoint < nSize; nPoint++ )
        {
            pRegionBand->InsertLine( aPoly.GetPoint(nPoint-1), aPoly.GetPoint(nPoint), nLineID++ );
        }

        const Point rLastPoint = aPoly.GetPoint(nSize-1);
        const Point rFirstPoint = aPoly.GetPoint(0);

        if ( rLastPoint != rFirstPoint )
        {
            pRegionBand->InsertLine( rLastPoint, rFirstPoint, nLineID++ );
        }
    }

    return pRegionBand;
}

namespace vcl {

// Immutable statics prevent accidental sentinel mutation
static const std::shared_ptr<const RegionData>& lcl_GetNullData()
{
    static std::shared_ptr<const RegionData> g_Null = std::make_shared<RegionData>(true);
    return g_Null;
}

static const std::shared_ptr<const RegionData>& lcl_GetEmptyData()
{
    static std::shared_ptr<const RegionData> g_Empty = std::make_shared<RegionData>(false);
    return g_Empty;
}

// --- SHARED_PTR COW LIFECYCLE ---

void Region::detach()
{
    if (mpData.use_count() > 1 || mpData == lcl_GetNullData() || mpData == lcl_GetEmptyData())
    {
        mpData = std::make_shared<RegionData>(*mpData);
    }
}

Region::~Region() = default;

// --- QUERIES ---

bool vcl::Region::IsEmpty() const
{
    if (mpData->mbIsNull)
        return false;
    return !mpData->mpB2DPolyPolygon && !mpData->mpPolyPolygon && !mpData->mpRegionBand;
}

bool vcl::Region::IsNull() const
{
    return mpData->mbIsNull;
}

const std::optional<basegfx::B2DPolyPolygon>& vcl::Region::getB2DPolyPolygon() const
{
    return mpData->mpB2DPolyPolygon;
}

const std::optional<tools::PolyPolygon>& vcl::Region::getPolyPolygon() const
{
    return mpData->mpPolyPolygon;
}

const RegionBand* vcl::Region::getRegionBand() const
{
    return mpData->mpRegionBand.get();
}

bool vcl::Region::HasPolyPolygonOrB2DPolyPolygon() const
{
    return mpData->mpB2DPolyPolygon.has_value() || mpData->mpPolyPolygon.has_value();
}

static std::shared_ptr<RegionBand> lcl_CreateRegionBandFromPolyPolygon(const tools::PolyPolygon& rPolyPolygon)
{
    std::shared_ptr<RegionBand> pRetval;

    if(rPolyPolygon.Count())
    {
        tools::PolyPolygon aPolyPolygon;
        rPolyPolygon.AdaptiveSubdivide(aPolyPolygon);

        if(aPolyPolygon.Count())
        {
            const tools::Rectangle aRect(aPolyPolygon.GetBoundRect());

            if(!aRect.IsEmpty())
            {
                if(lcl_IsPolygonRectilinear(aPolyPolygon))
                {
                    pRetval = lcl_RectilinearPolygonToBands(aPolyPolygon);
                }
                else
                {
                    pRetval = lcl_GeneralPolygonToBands(aPolyPolygon, aRect);
                }

                if(pRetval)
                {
                    pRetval->processPoints();
                    if(!pRetval->OptimizeBandList())
                    {
                        pRetval.reset();
                    }
                }
            }
        }
    }

    return pRetval;
}

tools::PolyPolygon vcl::Region::ImplCreatePolyPolygonFromRegionBand() const
{
    tools::PolyPolygon aRetval;

    if(mpData && mpData->mpRegionBand)
    {
        RectangleVector aRectangles;
        mpData->mpRegionBand->GetRegionRectangles(aRectangles);

        for (auto const& rectangle : aRectangles)
        {
            aRetval.Insert( tools::Polygon(rectangle) );
        }
    }
    else
    {
        OSL_ENSURE(false, "Called with no local RegionBand (!)");
    }

    return aRetval;
}

basegfx::B2DPolyPolygon vcl::Region::ImplCreateB2DPolyPolygonFromRegionBand() const
{
    tools::PolyPolygon aPoly(ImplCreatePolyPolygonFromRegionBand());

    return aPoly.getB2DPolyPolygon();
}

Region::Region(bool bIsNull)
{
    if (bIsNull)
        mpData = lcl_GetNullData();
    else
        mpData = lcl_GetEmptyData();
}

Region::Region(const tools::Rectangle& rRect)
{
    if (!rRect.IsEmpty())
    {
        mpData = std::make_shared<RegionData>(std::make_shared<RegionBand>(rRect));
    }
    else
    {
        mpData = lcl_GetEmptyData();
    }
}

Region::Region(const tools::Polygon& rPolygon)
{
    mpData = lcl_GetEmptyData();

    if(rPolygon.GetSize())
    {
        ImplCreatePolyPolyRegion(tools::PolyPolygon(rPolygon));
    }
}

Region::Region(const tools::PolyPolygon& rPolyPoly)
{
    mpData = lcl_GetEmptyData();

    if(rPolyPoly.Count())
    {
        ImplCreatePolyPolyRegion(rPolyPoly);
    }
}

Region::Region(const basegfx::B2DPolyPolygon& rPolyPoly)
{
    mpData = lcl_GetEmptyData();

    if(rPolyPoly.count())
    {
        ImplCreatePolyPolyRegion(tools::PolyPolygon(rPolyPoly));
    }
}

Region::Region(const Region& rRegion)
    : mpData(rRegion.mpData)
{
    // NOTE:
    // mpxRectCache is intentionally NOT copied.
    // Cache is per-instance and derived; copying it risks staleness
    // and violates the "immutable shared state" model.
}

Region::Region(Region&& rRegion) noexcept
    : mpData(std::move(rRegion.mpData))
    , mpxRectCache(std::move(rRegion.mpxRectCache))
{
    // A moved-from region should be strictly Null (infinite space) according to VCL tests
    rRegion.mpData = lcl_GetNullData();
}

void vcl::Region::ImplCreatePolyPolyRegion( const tools::PolyPolygon& rPolyPoly )
{
    const sal_uInt16 nPolyCount = rPolyPoly.Count();

    if(!nPolyCount)
        return;

    const tools::Rectangle aRect(rPolyPoly.GetBoundRect());

    if(aRect.IsEmpty())
        return;

    detach();

    if((1 == aRect.GetWidth()) || (1 == aRect.GetHeight()) || rPolyPoly.IsRect())
    {
        mpData = std::make_shared<RegionData>(std::make_shared<RegionBand>(aRect));
    }
    else
    {
        mpData = std::make_shared<RegionData>(rPolyPoly);
    }

    InvalidateCache();
}

void vcl::Region::ImplCreatePolyPolyRegion( const basegfx::B2DPolyPolygon& rPolyPoly )
{
    if(rPolyPoly.count() && !rPolyPoly.getB2DRange().isEmpty())
    {
        detach();
        mpData = std::make_shared<RegionData>(rPolyPoly);
        InvalidateCache();
    }
}

void vcl::Region::Move( tools::Long nHorzMove, tools::Long nVertMove )
{
    if(IsNull() || IsEmpty() || (!nHorzMove && !nVertMove))
    {
        return;
    }

    detach();

    if(mpData->mpB2DPolyPolygon)
    {
        basegfx::B2DPolyPolygon aPoly = *mpData->mpB2DPolyPolygon;
        aPoly.translate(nHorzMove, nVertMove);
        mpData = std::make_shared<RegionData>(std::move(aPoly));
    }
    else if(mpData->mpPolyPolygon)
    {
        tools::PolyPolygon aPoly = *mpData->mpPolyPolygon;
        aPoly.Move(nHorzMove, nVertMove);
        mpData = std::make_shared<RegionData>(std::move(aPoly));
    }
    else if(mpData->mpRegionBand)
    {
        auto pNewBand = std::make_shared<RegionBand>(*mpData->mpRegionBand);
        pNewBand->Move(nHorzMove, nVertMove);
        mpData = std::make_shared<RegionData>(std::move(pNewBand));
    }
    else
    {
        OSL_ENSURE(false, "Region::Move error: impossible combination (!)");
    }

    InvalidateCache();
}

void vcl::Region::Scale( double fScaleX, double fScaleY )
{
    if(IsNull() || IsEmpty() || (basegfx::fTools::equalZero(fScaleX) && basegfx::fTools::equalZero(fScaleY)))
    {
        return;
    }

    detach();

    if(mpData->mpB2DPolyPolygon)
    {
        basegfx::B2DPolyPolygon aPoly = *mpData->mpB2DPolyPolygon;
        aPoly.transform(basegfx::utils::createScaleB2DHomMatrix(fScaleX, fScaleY));
        mpData = std::make_shared<RegionData>(std::move(aPoly));
    }
    else if(mpData->mpPolyPolygon)
    {
        tools::PolyPolygon aPoly = *mpData->mpPolyPolygon;
        aPoly.Scale(fScaleX, fScaleY);
        mpData = std::make_shared<RegionData>(std::move(aPoly));
    }
    else if(mpData->mpRegionBand)
    {
        auto pNewBand = std::make_shared<RegionBand>(*mpData->mpRegionBand);
        pNewBand->Scale(fScaleX, fScaleY);
        mpData = std::make_shared<RegionData>(std::move(pNewBand));
    }
    else
    {
        OSL_ENSURE(false, "Region::Scale error: impossible combination (!)");
    }

    InvalidateCache();
}

void vcl::Region::Union( const tools::Rectangle& rRect )
{
    if(rRect.IsEmpty())
    {
        return;
    }

    if(IsEmpty())
    {
        *this = rRect;
        return;
    }

    if(IsNull()) { return; }

    detach();

    if(HasPolyPolygonOrB2DPolyPolygon())
    {
        basegfx::B2DPolyPolygon aThisPolyPoly(GetAsB2DPolyPolygon());
        aThisPolyPoly = basegfx::utils::prepareForPolygonOperation(aThisPolyPoly);

        if(!aThisPolyPoly.count())
        {
            *this = rRect;
        }
        else
        {
            const basegfx::B2DPolygon aRectPoly(
                basegfx::utils::createPolygonFromRect(
                        vcl::unotools::b2DRectangleFromRectangle(rRect)));
            const basegfx::B2DPolyPolygon aClip(
                basegfx::utils::solvePolygonOperationOr(
                    aThisPolyPoly,
                    basegfx::B2DPolyPolygon(aRectPoly)));
            *this = vcl::Region(aClip);
        }
        return;
    }

    const RegionBand* pCurrent = mpData->mpRegionBand.get();
    if(!pCurrent)
    {
        *this = rRect;
        return;
    }

    std::shared_ptr<RegionBand> pNewBand = std::make_shared<RegionBand>(*pCurrent);
    const tools::Long nLeft(std::min(rRect.Left(), rRect.Right()));
    const tools::Long nTop(std::min(rRect.Top(), rRect.Bottom()));
    const tools::Long nRight(std::max(rRect.Left(), rRect.Right()));
    const tools::Long nBottom(std::max(rRect.Top(), rRect.Bottom()));

    pNewBand->InsertBands(nTop, nBottom);
    pNewBand->Union(nLeft, nTop, nRight, nBottom);

    if(!pNewBand->OptimizeBandList())
    {
        mpData = lcl_GetEmptyData();
    }
    else
    {
        mpData = std::make_shared<RegionData>(std::move(pNewBand));
    }

    InvalidateCache();
}

void vcl::Region::Intersect( const tools::Rectangle& rRect )
{
    if ( rRect.IsEmpty() )
    {
        SetEmpty();
        return;
    }

    if(IsNull())
    {
        *this = rRect;
        return;
    }

    if(IsEmpty())
    {
        return;
    }

    detach();

    if(HasPolyPolygonOrB2DPolyPolygon())
    {
        if(mpData->mpB2DPolyPolygon)
        {
            basegfx::B2DPolyPolygon aClip = basegfx::utils::clipPolyPolygonOnRange(
                *mpData->mpB2DPolyPolygon,
                basegfx::B2DRange(
                    rRect.Left(),
                    rRect.Top(),
                    rRect.Right() + 1,
                    rRect.Bottom() + 1),
                true,
                false);

            if (aClip.count() == 0)
                mpData = lcl_GetEmptyData();
            else
                mpData = std::make_shared<RegionData>(std::move(aClip));
        }
        else
        {
            tools::PolyPolygon aPoly(*mpData->mpPolyPolygon);
            aPoly.Clip(rRect);

            if (aPoly.Count())
                mpData = std::make_shared<RegionData>(std::move(aPoly));
            else
                mpData = lcl_GetEmptyData();
        }

        InvalidateCache();
        return;
    }

    const RegionBand* pCurrent = mpData->mpRegionBand.get();
    if(!pCurrent)
    {
        return;
    }

    std::shared_ptr<RegionBand> pNewBand = std::make_shared<RegionBand>(*pCurrent);
    const tools::Long nLeft(std::min(rRect.Left(), rRect.Right()));
    const tools::Long nTop(std::min(rRect.Top(), rRect.Bottom()));
    const tools::Long nRight(std::max(rRect.Left(), rRect.Right()));
    const tools::Long nBottom(std::max(rRect.Top(), rRect.Bottom()));

    pNewBand->InsertBands(nTop, nBottom);
    pNewBand->Intersect(nLeft, nTop, nRight, nBottom);

    if(!pNewBand->OptimizeBandList())
    {
        mpData = lcl_GetEmptyData();
    }
    else
    {
        mpData = std::make_shared<RegionData>(std::move(pNewBand));
    }

    InvalidateCache();
}

void vcl::Region::Exclude( const tools::Rectangle& rRect )
{
    if ( rRect.IsEmpty() || IsEmpty() )
    {
        return;
    }

    if(IsNull())
    {
        OSL_ENSURE(false, "Region::Exclude error: Cannot exclude from null region (!)");
        return;
    }

    detach();

    if( HasPolyPolygonOrB2DPolyPolygon() )
    {
        basegfx::B2DPolyPolygon aThisPolyPoly(GetAsB2DPolyPolygon());
        aThisPolyPoly = basegfx::utils::prepareForPolygonOperation(aThisPolyPoly);

        if(!aThisPolyPoly.count())
        {
            return;
        }

        const basegfx::B2DPolygon aRectPoly(
            basegfx::utils::createPolygonFromRect(
                vcl::unotools::b2DRectangleFromRectangle(rRect)));
        const basegfx::B2DPolyPolygon aOtherPolyPoly(aRectPoly);
        const basegfx::B2DPolyPolygon aClip = basegfx::utils::solvePolygonOperationDiff(aThisPolyPoly, aOtherPolyPoly);

        *this = vcl::Region(aClip);
        return;
    }

    if(!mpData->mpRegionBand)
    {
        return;
    }

    std::shared_ptr<RegionBand> pNewBand = std::make_shared<RegionBand>(*mpData->mpRegionBand);

    const tools::Long nLeft(std::min(rRect.Left(), rRect.Right()));
    const tools::Long nTop(std::min(rRect.Top(), rRect.Bottom()));
    const tools::Long nRight(std::max(rRect.Left(), rRect.Right()));
    const tools::Long nBottom(std::max(rRect.Top(), rRect.Bottom()));

    pNewBand->InsertBands(nTop, nBottom);
    pNewBand->Exclude(nLeft, nTop, nRight, nBottom);

    if(!pNewBand->OptimizeBandList())
    {
        mpData = lcl_GetEmptyData();
    }
    else
    {
        mpData = std::make_shared<RegionData>(std::move(pNewBand));
    }

    InvalidateCache();
}

void vcl::Region::XOr( const tools::Rectangle& rRect )
{
    if ( rRect.IsEmpty() )
    {
        return;
    }

    if(IsEmpty())
    {
        *this = rRect;
        return;
    }

    if(IsNull())
    {
        OSL_ENSURE(false, "Region::XOr error: Cannot XOr with null region (!)");
        return;
    }

    detach();

    if( HasPolyPolygonOrB2DPolyPolygon() )
    {
        basegfx::B2DPolyPolygon aThisPolyPoly(GetAsB2DPolyPolygon());
        aThisPolyPoly = basegfx::utils::prepareForPolygonOperation( aThisPolyPoly );

        if(!aThisPolyPoly.count())
        {
            *this = rRect;
            return;
        }

        const basegfx::B2DPolygon aRectPoly(
            basegfx::utils::createPolygonFromRect(
                vcl::unotools::b2DRectangleFromRectangle(rRect)));
        const basegfx::B2DPolyPolygon aOtherPolyPoly(aRectPoly);
        const basegfx::B2DPolyPolygon aClip = basegfx::utils::solvePolygonOperationXor(aThisPolyPoly, aOtherPolyPoly);

        *this = vcl::Region(aClip);
        return;
    }

    const RegionBand* pCurrent = mpData->mpRegionBand.get();
    if(!pCurrent)
    {
        *this = rRect;
        return;
    }

    std::shared_ptr<RegionBand> pNewBand = std::make_shared<RegionBand>(*pCurrent);
    const tools::Long nLeft(std::min(rRect.Left(), rRect.Right()));
    const tools::Long nTop(std::min(rRect.Top(), rRect.Bottom()));
    const tools::Long nRight(std::max(rRect.Left(), rRect.Right()));
    const tools::Long nBottom(std::max(rRect.Top(), rRect.Bottom()));

    pNewBand->InsertBands(nTop, nBottom);
    pNewBand->XOr(nLeft, nTop, nRight, nBottom);

    if(!pNewBand->OptimizeBandList())
    {
        mpData = lcl_GetEmptyData();
    }
    else
    {
        mpData = std::make_shared<RegionData>(std::move(pNewBand));
    }

    InvalidateCache();
}

void vcl::Region::Union( const vcl::Region& rRegion )
{
    if(rRegion.IsEmpty() || IsNull() || mpData == rRegion.mpData)
    {
        return;
    }

    if(rRegion.IsNull())
    {
        *this = vcl::Region(true);
        return;
    }

    if(IsEmpty())
    {
        *this = rRegion;
        return;
    }

    detach();

    if( rRegion.HasPolyPolygonOrB2DPolyPolygon() || HasPolyPolygonOrB2DPolyPolygon() )
    {
        basegfx::B2DPolyPolygon aThisPolyPoly(GetAsB2DPolyPolygon());
        aThisPolyPoly = basegfx::utils::prepareForPolygonOperation(aThisPolyPoly);

        if(!aThisPolyPoly.count())
        {
            *this = rRegion;
            return;
        }

        basegfx::B2DPolyPolygon aOtherPolyPoly(rRegion.GetAsB2DPolyPolygon());
        aOtherPolyPoly = basegfx::utils::prepareForPolygonOperation(aOtherPolyPoly);

        basegfx::B2DPolyPolygon aClip(basegfx::utils::solvePolygonOperationOr(aThisPolyPoly, aOtherPolyPoly));

        *this = vcl::Region( aClip );
        return;
    }

    const RegionBand* pCurrent = mpData->mpRegionBand.get();
    if(!pCurrent)
    {
        *this = rRegion;
        return;
    }

    const RegionBand* pSource = rRegion.getRegionBand();
    if(!pSource)
    {
        return;
    }

    std::shared_ptr<RegionBand> pNewBand = std::make_shared<RegionBand>(*pCurrent);
    pNewBand->Union(*pSource);

    if(!pNewBand->OptimizeBandList())
    {
        mpData = lcl_GetEmptyData();
    }
    else
    {
        mpData = std::make_shared<RegionData>(std::move(pNewBand));
    }

    InvalidateCache();
}

void vcl::Region::Intersect( const vcl::Region& rRegion )
{
    if(mpData == rRegion.mpData || rRegion.IsNull() || IsEmpty())
    {
        return;
    }

    if(rRegion.IsEmpty())
    {
        SetEmpty();
        return;
    }

    if(IsNull())
    {
        *this = rRegion;
        return;
    }

    detach();

    if( rRegion.HasPolyPolygonOrB2DPolyPolygon() || HasPolyPolygonOrB2DPolyPolygon() )
    {
        basegfx::B2DPolyPolygon aThisPolyPoly(GetAsB2DPolyPolygon());
        if(!aThisPolyPoly.count())
        {
            return;
        }

        basegfx::B2DPolyPolygon aOtherPolyPoly(rRegion.GetAsB2DPolyPolygon());
        if(!aOtherPolyPoly.count())
        {
            SetEmpty();
            return;
        }

        static size_t gPointLimit = !comphelper::IsFuzzing() ? SAL_MAX_SIZE : 8192;
        size_t nPointLimit(gPointLimit);
        const basegfx::B2DPolyPolygon aClip(
            basegfx::utils::clipPolyPolygonOnPolyPolygon(
                aOtherPolyPoly,
                aThisPolyPoly,
                true,
                false,
                &nPointLimit));
        *this = vcl::Region( aClip );
        return;
    }

    const RegionBand* pCurrent = mpData->mpRegionBand.get();
    if(!pCurrent)
    {
        return;
    }

    const RegionBand* pSource = rRegion.getRegionBand();
    if(!pSource)
    {
        SetEmpty();
        return;
    }

    if(pCurrent->getRectangleCount() + 2 < pSource->getRectangleCount())
    {
        vcl::Region aTempRegion = rRegion;
        aTempRegion.Intersect( *this );
        *this = std::move(aTempRegion);
    }
    else
    {
        std::shared_ptr<RegionBand> pNewBand = std::make_shared<RegionBand>(*pCurrent);
        pNewBand->Intersect(*pSource);

        if(!pNewBand->OptimizeBandList())
        {
            mpData = lcl_GetEmptyData();
        }
        else
        {
            mpData = std::make_shared<RegionData>(std::move(pNewBand));
        }

        InvalidateCache();
    }
}

void vcl::Region::Exclude( const vcl::Region& rRegion )
{
    if ( rRegion.IsEmpty() || IsEmpty() || mpData == rRegion.mpData)
    {
        return;
    }

    if ( rRegion.IsNull() )
    {
        SetEmpty();
        return;
    }

    if(IsNull())
    {
        OSL_ENSURE(false, "Region::Exclude error: Cannot exclude from null region (!)");
        return;
    }

    detach();

    if( rRegion.HasPolyPolygonOrB2DPolyPolygon() || HasPolyPolygonOrB2DPolyPolygon() )
    {
        basegfx::B2DPolyPolygon aThisPolyPoly(GetAsB2DPolyPolygon());
        if(!aThisPolyPoly.count())
        {
            return;
        }

        aThisPolyPoly = basegfx::utils::prepareForPolygonOperation( aThisPolyPoly );
        basegfx::B2DPolyPolygon aOtherPolyPoly(rRegion.GetAsB2DPolyPolygon());
        aOtherPolyPoly = basegfx::utils::prepareForPolygonOperation( aOtherPolyPoly );

        basegfx::B2DPolyPolygon aClip = basegfx::utils::solvePolygonOperationDiff( aThisPolyPoly, aOtherPolyPoly );
        *this = vcl::Region( aClip );
        return;
    }

    const RegionBand* pCurrent = mpData->mpRegionBand.get();
    if(!pCurrent)
    {
        return;
    }

    const RegionBand* pSource = rRegion.getRegionBand();
    if(!pSource)
    {
        return;
    }

    std::shared_ptr<RegionBand> pNewBand = std::make_shared<RegionBand>(*pCurrent);

    if(!pNewBand->Exclude(*pSource) || !pNewBand->OptimizeBandList())
    {
        mpData = lcl_GetEmptyData();
    }
    else
    {
        mpData = std::make_shared<RegionData>(std::move(pNewBand));
    }

    InvalidateCache();
}

bool vcl::Region::XOr( const vcl::Region& rRegion )
{
    if ( rRegion.IsEmpty() )
    {
        return true;
    }

    if ( rRegion.IsNull() )
    {
        OSL_ENSURE(false, "Region::XOr error: Cannot XOr with null region (!)");
        return true;
    }

    if(IsEmpty())
    {
        *this = rRegion;
        return true;
    }

    if(IsNull())
    {
        OSL_ENSURE(false, "Region::XOr error: Cannot XOr with null region (!)");
        return false;
    }

    detach();

    if( rRegion.HasPolyPolygonOrB2DPolyPolygon() || HasPolyPolygonOrB2DPolyPolygon() )
    {
        basegfx::B2DPolyPolygon aThisPolyPoly(GetAsB2DPolyPolygon());
        if(!aThisPolyPoly.count())
        {
            *this = rRegion;
            return true;
        }

        aThisPolyPoly = basegfx::utils::prepareForPolygonOperation( aThisPolyPoly );
        basegfx::B2DPolyPolygon aOtherPolyPoly(rRegion.GetAsB2DPolyPolygon());
        aOtherPolyPoly = basegfx::utils::prepareForPolygonOperation( aOtherPolyPoly );

        basegfx::B2DPolyPolygon aClip = basegfx::utils::solvePolygonOperationXor( aThisPolyPoly, aOtherPolyPoly );
        *this = vcl::Region( aClip );
        return true;
    }

    const RegionBand* pCurrent = mpData->mpRegionBand.get();
    if(!pCurrent)
    {
        *this = rRegion;
        return true;
    }

    const RegionBand* pSource = rRegion.getRegionBand();
    if(!pSource)
    {
        return true;
    }

    std::shared_ptr<RegionBand> pNewBand = std::make_shared<RegionBand>(*pCurrent);
    pNewBand->XOr(*pSource);

    if(!pNewBand->OptimizeBandList())
    {
        mpData = lcl_GetEmptyData();
    }
    else
    {
        mpData = std::make_shared<RegionData>(std::move(pNewBand));
    }

    InvalidateCache();

    return true;
}

tools::Rectangle vcl::Region::GetBoundRect() const
{
    if(mpData->mbIsNull || IsEmpty())
    {
        return tools::Rectangle();
    }

    if(mpData->mpB2DPolyPolygon)
    {
        const basegfx::B2DRange aRange(mpData->mpB2DPolyPolygon->getB2DRange());

        if(aRange.isEmpty())
        {
            return tools::Rectangle();
        }
        else
        {
            return tools::Rectangle(
                basegfx::fround<tools::Long>(aRange.getMinX()), basegfx::fround<tools::Long>(aRange.getMinY()),
                basegfx::fround<tools::Long>(aRange.getMaxX()), basegfx::fround<tools::Long>(aRange.getMaxY()));
        }
    }

    if(mpData->mpPolyPolygon)
    {
        return mpData->mpPolyPolygon->GetBoundRect();
    }

    if(mpData->mpRegionBand)
    {
        return mpData->mpRegionBand->GetBoundRect();
    }

    return tools::Rectangle();
}

tools::PolyPolygon vcl::Region::GetAsPolyPolygon() const
{
    if(mpData->mpPolyPolygon)
    {
        return *mpData->mpPolyPolygon;
    }

    if(mpData->mpB2DPolyPolygon)
    {
        const tools::PolyPolygon aPolyPolgon(*mpData->mpB2DPolyPolygon);
        return aPolyPolgon;
    }

    if(mpData->mpRegionBand)
    {
        return ImplCreatePolyPolygonFromRegionBand();
    }

    return tools::PolyPolygon();
}

basegfx::B2DPolyPolygon vcl::Region::GetAsB2DPolyPolygon() const
{
    if(mpData->mpB2DPolyPolygon)
    {
        return *mpData->mpB2DPolyPolygon;
    }

    if(mpData->mpPolyPolygon)
    {
        return mpData->mpPolyPolygon->getB2DPolyPolygon();
    }

    if(mpData->mpRegionBand)
    {
        return ImplCreateB2DPolyPolygonFromRegionBand();
    }

    return basegfx::B2DPolyPolygon();
}

const RegionBand* vcl::Region::GetAsRegionBand() const
{
    if(!mpData->mpRegionBand)
    {
        std::shared_ptr<RegionBand> pBands;
        if(mpData->mpB2DPolyPolygon)
        {
            pBands = lcl_CreateRegionBandFromPolyPolygon(tools::PolyPolygon(*mpData->mpB2DPolyPolygon));
            if (pBands)
            {
                mpData = std::make_shared<RegionData>(*mpData->mpB2DPolyPolygon, std::move(pBands));
            }
        }
        else if(mpData->mpPolyPolygon)
        {
            pBands = lcl_CreateRegionBandFromPolyPolygon(*mpData->mpPolyPolygon);
            if (pBands)
            {
                mpData = std::make_shared<RegionData>(*mpData->mpPolyPolygon, std::move(pBands));
            }
        }
    }

    return mpData->mpRegionBand.get();
}

bool vcl::Region::Contains( const Point& rPoint ) const
{
    if(IsEmpty())
    {
        return false;
    }

    if(mpData->mbIsNull)
    {
        return true;
    }

    const RegionBand* pRegionBand = GetAsRegionBand();

    if(pRegionBand)
    {
        return pRegionBand->Contains(rPoint);
    }

    return false;
}

bool vcl::Region::Overlaps( const tools::Rectangle& rRect ) const
{
    if(IsEmpty())
    {
        return false;
    }

    if(IsNull())
    {
        return true;
    }

    vcl::Region aRegion(rRect);
    aRegion.Intersect( *this );

    return !aRegion.IsEmpty();
}

bool vcl::Region::IsRectangle() const
{
    if( IsEmpty() || mpData->mbIsNull )
        return false;

    if( mpData->mpB2DPolyPolygon )
        return basegfx::utils::isRectangle( *mpData->mpB2DPolyPolygon );

    if( mpData->mpPolyPolygon )
        return mpData->mpPolyPolygon->IsRect();

    if( mpData->mpRegionBand )
        return (mpData->mpRegionBand->getRectangleCount() == 1);

    return false;
}

void vcl::Region::SetNull()
{
    mpData = lcl_GetNullData();
    InvalidateCache();
}

void vcl::Region::SetEmpty()
{
    mpData = lcl_GetEmptyData();
    InvalidateCache();
}

Region& vcl::Region::operator=(const vcl::Region& rRegion)
{
    if (this != &rRegion && mpData != rRegion.mpData)
    {
        mpData = rRegion.mpData;
        InvalidateCache();
    }

    return *this;
}

Region& vcl::Region::operator=( vcl::Region&& rRegion ) noexcept
{
    if (this != &rRegion)
    {
        mpData = std::move(rRegion.mpData);
        mpxRectCache = std::move(rRegion.mpxRectCache);

        // A moved-from region should be strictly Null (infinite space) according to VCL tests
        rRegion.mpData = lcl_GetNullData();
    }

    return *this;
}

Region& vcl::Region::operator=( const tools::Rectangle& rRect )
{
    if (!rRect.IsEmpty())
    {
        mpData = std::make_shared<RegionData>(std::make_shared<RegionBand>(rRect));
    }
    else
    {
        mpData = lcl_GetEmptyData();
    }
    InvalidateCache();

    return *this;
}

bool vcl::Region::operator==( const vcl::Region& rRegion ) const
{
    if(mpData == rRegion.mpData)
    {
        return true;
    }

    if(mpData->mbIsNull && rRegion.mpData->mbIsNull)
    {
        return true;
    }

    if(IsEmpty() && rRegion.IsEmpty())
    {
        return true;
    }

    if(mpData->mbIsNull || IsEmpty() || rRegion.mpData->mbIsNull || rRegion.IsEmpty())
    {
        return false;
    }

    return GetAsB2DPolyPolygon() == rRegion.GetAsB2DPolyPolygon();
}

SvStream& ReadRegion(SvStream& rIStm, vcl::Region& rRegion)
{
    VersionCompatRead aCompat(rIStm);
    sal_uInt16 nVersion(0);
    sal_uInt16 nTmp16(0);

    rRegion.SetEmpty();

    rIStm.ReadUInt16( nVersion );
    rIStm.ReadUInt16( nTmp16 );

    enum RegionType { REGION_NULL, REGION_EMPTY, REGION_RECTANGLE, REGION_COMPLEX };
    auto eStreamedType = nTmp16;

    switch (eStreamedType)
    {
        case REGION_NULL:
        {
            rRegion.SetNull();
            break;
        }

        case REGION_EMPTY:
        {
            rRegion.SetEmpty();
            break;
        }

        default:
        {
            std::shared_ptr<RegionBand> xNewRegionBand(std::make_shared<RegionBand>());
            bool bSuccess = xNewRegionBand->load(rIStm);

            bool bHasPolyPolygon = false;
            tools::PolyPolygon aNewPoly;

            if (aCompat.GetVersion() >= 2)
            {
                rIStm.ReadCharAsBool( bHasPolyPolygon );

                if (bHasPolyPolygon)
                {
                    ReadPolyPolygon(rIStm, aNewPoly);
                    const auto nPolygons = aNewPoly.Count();
                    if (nPolygons > 128)
                    {
                        SAL_WARN("vcl.gdi", "suspiciously high no of polygons in clip:" << nPolygons);
                        if (comphelper::IsFuzzing())
                            aNewPoly.Clear();
                    }
                }
            }

            if (!bSuccess && !bHasPolyPolygon)
            {
                SAL_WARN("vcl.gdi", "bad region band:" << bHasPolyPolygon);
                rRegion.SetNull();
            }
            else
            {
                rRegion = vcl::Region();
                if (bHasPolyPolygon && bSuccess)
                    rRegion.mpData = std::make_shared<RegionData>(std::move(aNewPoly), std::move(xNewRegionBand));
                else if (bHasPolyPolygon)
                    rRegion.mpData = std::make_shared<RegionData>(std::move(aNewPoly));
                else
                    rRegion.mpData = std::make_shared<RegionData>(std::move(xNewRegionBand));
            }

            rRegion.InvalidateCache();
            break;
        }
    }

    return rIStm;
}

SvStream& WriteRegion( SvStream& rOStm, const vcl::Region& rRegion )
{
    const sal_uInt16 nVersion(2);
    VersionCompatWrite aCompat(rOStm, nVersion);

    rOStm.WriteUInt16( nVersion );

    enum RegionType { REGION_NULL, REGION_EMPTY, REGION_RECTANGLE, REGION_COMPLEX };
    RegionType aRegionType(REGION_COMPLEX);
    bool bEmpty(rRegion.IsEmpty());

    if(!bEmpty && rRegion.getB2DPolyPolygon() && 0 == rRegion.getB2DPolyPolygon()->count())
    {
        OSL_ENSURE(false, "Region with empty B2DPolyPolygon, should not be created (!)");
        bEmpty = true;
    }

    if(!bEmpty && rRegion.getPolyPolygon() && 0 == rRegion.getPolyPolygon()->Count())
    {
        OSL_ENSURE(false, "Region with empty PolyPolygon, should not be created (!)");
        bEmpty = true;
    }

    if(bEmpty)
    {
        aRegionType = REGION_EMPTY;
    }
    else if(rRegion.IsNull())
    {
        aRegionType = REGION_NULL;
    }
    else if(rRegion.getRegionBand() && rRegion.getRegionBand()->isSingleRectangle())
    {
        aRegionType = REGION_RECTANGLE;
    }

    rOStm.WriteUInt16( aRegionType );

    const RegionBand* pRegionBand = rRegion.getRegionBand();

    if(pRegionBand)
    {
        pRegionBand->save(rOStm);
    }
    else
    {
        const RegionBand aRegionBand;
        aRegionBand.save(rOStm);
    }

    const bool bHasPolyPolygon(rRegion.HasPolyPolygonOrB2DPolyPolygon());
    rOStm.WriteBool( bHasPolyPolygon );

    if(bHasPolyPolygon)
    {
        tools::PolyPolygon aNoCurvePolyPolygon;
        rRegion.GetAsPolyPolygon().AdaptiveSubdivide(aNoCurvePolyPolygon);

        WritePolyPolygon( rOStm, aNoCurvePolyPolygon );
    }

    return rOStm;
}

void vcl::Region::GetRegionRectangles(RectangleVector& rTarget) const
{
    rTarget.clear();

    const RegionBand* pRegionBand = GetAsRegionBand();

    if(pRegionBand)
    {
        pRegionBand->GetRegionRectangles(rTarget);
    }
}

static bool lcl_PolygonRectTest( const tools::Polygon& rPoly, tools::Rectangle* pRectOut = nullptr )
{
    bool bIsRect = false;
    const Point* pPoints = rPoly.GetConstPointAry();
    sal_uInt16 nPoints = rPoly.GetSize();

    if( nPoints == 4 || (nPoints == 5 && pPoints[0] == pPoints[4]) )
    {
        tools::Long nX1 = pPoints[0].X(), nX2 = pPoints[2].X(), nY1 = pPoints[0].Y(), nY2 = pPoints[2].Y();

        if( ( (pPoints[1].X() == nX1 && pPoints[3].X() == nX2) && (pPoints[1].Y() == nY2 && pPoints[3].Y() == nY1) )
         || ( (pPoints[1].X() == nX2 && pPoints[3].X() == nX1) && (pPoints[1].Y() == nY1 && pPoints[3].Y() == nY2) ) )
        {
            bIsRect = true;

            if( pRectOut )
            {
                tools::Long nSwap;

                if( nX2 < nX1 )
                {
                    nSwap = nX2;
                    nX2 = nX1;
                    nX1 = nSwap;
                }

                if( nY2 < nY1 )
                {
                    nSwap = nY2;
                    nY2 = nY1;
                    nY1 = nSwap;
                }

                if( nX2 != nX1 )
                {
                    nX2--;
                }

                if( nY2 != nY1 )
                {
                    nY2--;
                }

                pRectOut->SetLeft( nX1 );
                pRectOut->SetRight( nX2 );
                pRectOut->SetTop( nY1 );
                pRectOut->SetBottom( nY2 );
            }
        }
    }

    return bIsRect;
}

vcl::Region vcl::Region::GetRegionFromPolyPolygon( const tools::PolyPolygon& rPolyPoly )
{
    int nPolygonRects = 0, nPolygonPolygons = 0;
    int nPolygons = rPolyPoly.Count();

    for( int i = 0; i < nPolygons; i++ )
    {
        const tools::Polygon& rPoly = rPolyPoly[i];

        if( lcl_PolygonRectTest( rPoly ) )
        {
            nPolygonRects++;
        }
        else
        {
            nPolygonPolygons++;
        }
    }

    if( nPolygonPolygons > nPolygonRects )
    {
        return vcl::Region( rPolyPoly );
    }

    vcl::Region aResult;
    tools::Rectangle aRect;

    for( int i = 0; i < nPolygons; i++ )
    {
        const tools::Polygon& rPoly = rPolyPoly[i];

        if( lcl_PolygonRectTest( rPoly, &aRect ) )
        {
            aResult.XOr( aRect );
        }
        else
        {
            aResult.XOr( vcl::Region(rPoly) );
        }
    }

    return aResult;
}

static const RectangleVector g_EmptyRegionRects;

Region::const_iterator Region::begin() const
{
    if (IsEmpty())
    {
        return g_EmptyRegionRects.begin();
    }

    if (!mpxRectCache)
    {
        mpxRectCache = std::make_unique<RectangleVector>();
        const_cast<Region*>(this)->GetRegionRectangles(*mpxRectCache);
    }
    return mpxRectCache->begin();
}

Region::const_iterator Region::end() const
{
    if (IsEmpty())
    {
        return g_EmptyRegionRects.end();
    }

    if (!mpxRectCache)
    {
        mpxRectCache = std::make_unique<RectangleVector>();
        const_cast<Region*>(this)->GetRegionRectangles(*mpxRectCache);
    }
    return mpxRectCache->end();
}

void Region::InvalidateCache() const
{
    mpxRectCache.reset();
}

std::ostream& operator<<(std::ostream& s, const vcl::Region& rRegion)
{
    if (rRegion.IsNull())
        return s << "Region(NULL)";
    if (rRegion.IsEmpty())
        return s << "Region(EMPTY)";

    tools::Rectangle aBound = rRegion.GetBoundRect();
    return s << "Region(Bound=" << aBound.Left() << "," << aBound.Top() << " "
             << aBound.GetWidth() << "x" << aBound.GetHeight() << ")";
}

} /* namespace vcl */

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
