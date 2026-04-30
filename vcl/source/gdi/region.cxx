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
#include <regionband.hxx>

#include <basegfx/polygon/b2dpolypolygontools.hxx>
#include <basegfx/polygon/b2dpolygontools.hxx>
#include <basegfx/polygon/b2dpolygonclipper.hxx>
#include <basegfx/polygon/b2dpolypolygoncutter.hxx>
#include <basegfx/range/b2drange.hxx>
#include <basegfx/matrix/b2dhommatrixtools.hxx>
#include <tools/poly.hxx>
#include <comphelper/configuration.hxx>

namespace
{
    /** Return <TRUE/> when the given polygon is rectilinear and oriented so that
        all sides are either horizontal or vertical.
    */
    bool ImplIsPolygonRectilinear (const tools::PolyPolygon& rPolyPoly)
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
    std::shared_ptr<RegionBand> ImplRectilinearPolygonToBands(const tools::PolyPolygon& rPolyPoly)
    {
        OSL_ASSERT(ImplIsPolygonRectilinear (rPolyPoly));

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

    std::shared_ptr<RegionBand> ImplGeneralPolygonToBands(const tools::PolyPolygon& rPolyPoly, const tools::Rectangle& rPolygonBoundingBox)
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
} // end of anonymous namespace

namespace vcl {

bool vcl::Region::IsEmpty() const
{
    return !mbIsNull && !mpB2DPolyPolygon && !mpPolyPolygon && !mpRegionBand;
}


static std::shared_ptr<RegionBand> ImplCreateRegionBandFromPolyPolygon(const tools::PolyPolygon& rPolyPolygon)
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
                if(ImplIsPolygonRectilinear(aPolyPolygon))
                {
                    pRetval = ImplRectilinearPolygonToBands(aPolyPolygon);
                }
                else
                {
                    pRetval = ImplGeneralPolygonToBands(aPolyPolygon, aRect);
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

    if(getRegionBand())
    {
        RectangleVector aRectangles;
        GetRegionRectangles(aRectangles);

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
:   mbIsNull(bIsNull)
{
}

Region::Region(const tools::Rectangle& rRect)
:   mbIsNull(false)
{
    if (!rRect.IsEmpty())
        mpRegionBand = std::make_shared<RegionBand>(rRect);
}

Region::Region(const tools::Polygon& rPolygon)
:   mbIsNull(false)
{

    if(rPolygon.GetSize())
    {
        ImplCreatePolyPolyRegion(tools::PolyPolygon(rPolygon));
    }
}

Region::Region(const tools::PolyPolygon& rPolyPoly)
:   mbIsNull(false)
{

    if(rPolyPoly.Count())
    {
        ImplCreatePolyPolyRegion(rPolyPoly);
    }
}

Region::Region(const basegfx::B2DPolyPolygon& rPolyPoly)
:   mbIsNull(false)
{

    if(rPolyPoly.count())
    {
        ImplCreatePolyPolyRegion(rPolyPoly);
    }
}

Region::Region(const Region& rRegion)
    : mpB2DPolyPolygon(rRegion.mpB2DPolyPolygon)
    , mpPolyPolygon(rRegion.mpPolyPolygon)
    , mpRegionBand(rRegion.mpRegionBand)
    , mbIsNull(rRegion.mbIsNull)
{
}

Region::Region(Region&& rRegion) noexcept
    : mpB2DPolyPolygon(std::move(rRegion.mpB2DPolyPolygon))
    , mpPolyPolygon(std::move(rRegion.mpPolyPolygon))
    , mpRegionBand(std::move(rRegion.mpRegionBand))
    , mbIsNull(rRegion.mbIsNull)
{
    rRegion.mbIsNull = true;
    rRegion.mpxRectCache.reset();
}

Region::~Region() = default;

void vcl::Region::ImplCreatePolyPolyRegion( const tools::PolyPolygon& rPolyPoly )
{
    const sal_uInt16 nPolyCount = rPolyPoly.Count();

    if(!nPolyCount)
        return;

    const tools::Rectangle aRect(rPolyPoly.GetBoundRect());

    if(aRect.IsEmpty())
        return;

    if((1 == aRect.GetWidth()) || (1 == aRect.GetHeight()) || rPolyPoly.IsRect())
    {
        mpRegionBand = std::make_shared<RegionBand>(aRect);
    }
    else
    {
        mpPolyPolygon = rPolyPoly;
    }

    mbIsNull = false;
    InvalidateCache();
}

void vcl::Region::ImplCreatePolyPolyRegion( const basegfx::B2DPolyPolygon& rPolyPoly )
{
    if(rPolyPoly.count() && !rPolyPoly.getB2DRange().isEmpty())
    {
        mpB2DPolyPolygon = rPolyPoly;
        mbIsNull = false;
        InvalidateCache();
    }
}

void vcl::Region::Move( tools::Long nHorzMove, tools::Long nVertMove )
{
    if(IsNull() || IsEmpty())
    {
        return;
    }

    if(!nHorzMove && !nVertMove)
    {
        return;
    }

    if(getB2DPolyPolygon())
    {
        basegfx::B2DPolyPolygon aPoly(*getB2DPolyPolygon());

        aPoly.translate(nHorzMove, nVertMove);
        if (aPoly.count())
            mpB2DPolyPolygon = aPoly;
        else
            mpB2DPolyPolygon.reset();
        mpPolyPolygon.reset();
        mpRegionBand.reset();
    }
    else if(getPolyPolygon())
    {
        tools::PolyPolygon aPoly(*getPolyPolygon());

        aPoly.Move(nHorzMove, nVertMove);
        mpB2DPolyPolygon.reset();
        if (aPoly.Count())
            mpPolyPolygon = aPoly;
        else
            mpPolyPolygon.reset();
        mpRegionBand.reset();
    }
    else if(getRegionBand())
    {
        std::shared_ptr<RegionBand> pNew = std::make_shared<RegionBand>(*getRegionBand());

        pNew->Move(nHorzMove, nVertMove);
        mpB2DPolyPolygon.reset();
        mpPolyPolygon.reset();
        mpRegionBand = std::move(pNew);
    }
    else
    {
        OSL_ENSURE(false, "Region::Move error: impossible combination (!)");
    }

    InvalidateCache();
}

void vcl::Region::Scale( double fScaleX, double fScaleY )
{
    if(IsNull() || IsEmpty())
    {
        return;
    }

    if(basegfx::fTools::equalZero(fScaleX) && basegfx::fTools::equalZero(fScaleY))
    {
        return;
    }

    if(getB2DPolyPolygon())
    {
        basegfx::B2DPolyPolygon aPoly(*getB2DPolyPolygon());

        aPoly.transform(basegfx::utils::createScaleB2DHomMatrix(fScaleX, fScaleY));
        if (aPoly.count())
            mpB2DPolyPolygon = aPoly;
        else
            mpB2DPolyPolygon.reset();
        mpPolyPolygon.reset();
        mpRegionBand.reset();
    }
    else if(getPolyPolygon())
    {
        tools::PolyPolygon aPoly(*getPolyPolygon());

        aPoly.Scale(fScaleX, fScaleY);
        mpB2DPolyPolygon.reset();
        if (aPoly.Count())
            mpPolyPolygon = aPoly;
        else
            mpPolyPolygon.reset();
        mpRegionBand.reset();
    }
    else if(getRegionBand())
    {
        std::shared_ptr<RegionBand> pNew = std::make_shared<RegionBand>(*getRegionBand());

        pNew->Scale(fScaleX, fScaleY);
        mpB2DPolyPolygon.reset();
        mpPolyPolygon.reset();
        mpRegionBand = std::move(pNew);
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
        *this = rRect; // operator= handles invalidation
        return;
    }

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

    const RegionBand* pCurrent = getRegionBand();
    if(!pCurrent)
    {
        *this = rRect;
        return;
    }

    std::shared_ptr<RegionBand> pNew = std::make_shared<RegionBand>(*pCurrent);
    const tools::Long nLeft(std::min(rRect.Left(), rRect.Right()));
    const tools::Long nTop(std::min(rRect.Top(), rRect.Bottom()));
    const tools::Long nRight(std::max(rRect.Left(), rRect.Right()));
    const tools::Long nBottom(std::max(rRect.Top(), rRect.Bottom()));

    pNew->InsertBands(nTop, nBottom);
    pNew->Union(nLeft, nTop, nRight, nBottom);

    if(!pNew->OptimizeBandList())
    {
        pNew.reset();
    }

    mpRegionBand = std::move(pNew);
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

    if(HasPolyPolygonOrB2DPolyPolygon())
    {
        if(getB2DPolyPolygon())
        {
            const basegfx::B2DPolyPolygon aPoly(
                basegfx::utils::clipPolyPolygonOnRange(
                    *getB2DPolyPolygon(),
                    basegfx::B2DRange(
                        rRect.Left(),
                        rRect.Top(),
                        rRect.Right() + 1,
                        rRect.Bottom() + 1),
                    true,
                    false));

            if (aPoly.count())
                mpB2DPolyPolygon = aPoly;
            else
                mpB2DPolyPolygon.reset();
            mpPolyPolygon.reset();
            mpRegionBand.reset();
        }
        else
        {
            tools::PolyPolygon aPoly(*getPolyPolygon());
            aPoly.Clip(rRect);

            mpB2DPolyPolygon.reset();
            if (aPoly.Count())
                mpPolyPolygon = aPoly;
            else
                mpPolyPolygon.reset();
            mpRegionBand.reset();
        }

        InvalidateCache();
        return;
    }

    const RegionBand* pCurrent = getRegionBand();
    if(!pCurrent)
    {
        return;
    }

    std::shared_ptr<RegionBand> pNew( std::make_shared<RegionBand>(*pCurrent));
    const tools::Long nLeft(std::min(rRect.Left(), rRect.Right()));
    const tools::Long nTop(std::min(rRect.Top(), rRect.Bottom()));
    const tools::Long nRight(std::max(rRect.Left(), rRect.Right()));
    const tools::Long nBottom(std::max(rRect.Top(), rRect.Bottom()));

    pNew->InsertBands(nTop, nBottom);
    pNew->Intersect(nLeft, nTop, nRight, nBottom);

    if(!pNew->OptimizeBandList())
    {
        pNew.reset();
    }

    mpRegionBand = std::move(pNew);
    InvalidateCache();
}

void vcl::Region::Exclude( const tools::Rectangle& rRect )
{
    if ( rRect.IsEmpty() )
    {
        return;
    }

    if(IsEmpty())
    {
        return;
    }

    if(IsNull())
    {
        OSL_ENSURE(false, "Region::Exclude error: Cannot exclude from null region (!)");
        return;
    }

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

    if(!mpRegionBand)
    {
        return;
    }

    std::shared_ptr<RegionBand>& pNew = mpRegionBand;
    if (pNew.use_count() > 1)
        pNew = std::make_shared<RegionBand>(*pNew);

    const tools::Long nLeft(std::min(rRect.Left(), rRect.Right()));
    const tools::Long nTop(std::min(rRect.Top(), rRect.Bottom()));
    const tools::Long nRight(std::max(rRect.Left(), rRect.Right()));
    const tools::Long nBottom(std::max(rRect.Top(), rRect.Bottom()));

    pNew->InsertBands(nTop, nBottom);
    pNew->Exclude(nLeft, nTop, nRight, nBottom);

    if(!pNew->OptimizeBandList())
        pNew.reset();

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

    const RegionBand* pCurrent = getRegionBand();
    if(!pCurrent)
    {
        *this = rRect;
        return;
    }

    std::shared_ptr<RegionBand> pNew( std::make_shared<RegionBand>(*getRegionBand()));
    const tools::Long nLeft(std::min(rRect.Left(), rRect.Right()));
    const tools::Long nTop(std::min(rRect.Top(), rRect.Bottom()));
    const tools::Long nRight(std::max(rRect.Left(), rRect.Right()));
    const tools::Long nBottom(std::max(rRect.Top(), rRect.Bottom()));

    pNew->InsertBands(nTop, nBottom);
    pNew->XOr(nLeft, nTop, nRight, nBottom);

    if(!pNew->OptimizeBandList())
    {
        pNew.reset();
    }

    mpRegionBand = std::move(pNew);
    InvalidateCache();
}

void vcl::Region::Union( const vcl::Region& rRegion )
{
    if(rRegion.IsEmpty())
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

    if(IsNull())
    {
        return;
    }

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

    const RegionBand* pCurrent = getRegionBand();
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

    std::shared_ptr<RegionBand> pNew( std::make_shared<RegionBand>(*pCurrent));
    pNew->Union(*pSource);

    if(!pNew->OptimizeBandList())
    {
        pNew.reset();
    }

    mpRegionBand = std::move(pNew);
    InvalidateCache();
}

void vcl::Region::Intersect( const vcl::Region& rRegion )
{
    if(getB2DPolyPolygon() && getB2DPolyPolygon() == rRegion.getB2DPolyPolygon())
    {
        return;
    }

    if(getPolyPolygon() && getPolyPolygon() == rRegion.getPolyPolygon())
    {
        return;
    }

    if(getRegionBand() && getRegionBand() == rRegion.getRegionBand())
    {
        return;
    }

    if(rRegion.IsNull())
    {
        return;
    }

    if(IsNull())
    {
        *this = rRegion;
        return;
    }

    if(rRegion.IsEmpty())
    {
        SetEmpty();
        return;
    }

    if(IsEmpty())
    {
        return;
    }

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

    const RegionBand* pCurrent = getRegionBand();
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
        std::shared_ptr<RegionBand> pNew( std::make_shared<RegionBand>(*pCurrent));
        pNew->Intersect(*pSource);

        if(!pNew->OptimizeBandList())
        {
            pNew.reset();
        }

        mpRegionBand = std::move(pNew);
        InvalidateCache();
    }
}

void vcl::Region::Exclude( const vcl::Region& rRegion )
{
    if ( rRegion.IsEmpty() )
    {
        return;
    }

    if ( rRegion.IsNull() )
    {
        SetEmpty();
        return;
    }

    if(IsEmpty())
    {
        return;
    }

    if(IsNull())
    {
        OSL_ENSURE(false, "Region::Exclude error: Cannot exclude from null region (!)");
        return;
    }

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

    const RegionBand* pCurrent = getRegionBand();
    if(!pCurrent)
    {
        return;
    }

    const RegionBand* pSource = rRegion.getRegionBand();
    if(!pSource)
    {
        return;
    }

    std::shared_ptr<RegionBand> pNew( std::make_shared<RegionBand>(*pCurrent));
    const bool bSuccess(pNew->Exclude(*pSource));

    if(!bSuccess)
    {
        pNew.reset();
    }

    mpRegionBand = std::move(pNew);
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

    const RegionBand* pCurrent = getRegionBand();
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

    std::shared_ptr<RegionBand> pNew( std::make_shared<RegionBand>(*pCurrent));
    pNew->XOr(*pSource);

    if(!pNew->OptimizeBandList())
    {
        pNew.reset();
    }

    mpRegionBand = std::move(pNew);
    InvalidateCache();

    return true;
}

tools::Rectangle vcl::Region::GetBoundRect() const
{
    if(IsEmpty())
    {
        return tools::Rectangle();
    }

    if(IsNull())
    {
        return tools::Rectangle();
    }

    if(getB2DPolyPolygon())
    {
        const basegfx::B2DRange aRange(getB2DPolyPolygon()->getB2DRange());

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

    if(getPolyPolygon())
    {
        return getPolyPolygon()->GetBoundRect();
    }

    if(getRegionBand())
    {
        return getRegionBand()->GetBoundRect();
    }

    return tools::Rectangle();
}

tools::PolyPolygon vcl::Region::GetAsPolyPolygon() const
{
    if(getPolyPolygon())
    {
        return *getPolyPolygon();
    }

    if(getB2DPolyPolygon())
    {
        const tools::PolyPolygon aPolyPolgon(*getB2DPolyPolygon());
        const_cast< vcl::Region* >(this)->mpPolyPolygon = aPolyPolgon;

        return *getPolyPolygon();
    }

    if(getRegionBand())
    {
        const tools::PolyPolygon aPolyPolgon(ImplCreatePolyPolygonFromRegionBand());
        const_cast< vcl::Region* >(this)->mpPolyPolygon = aPolyPolgon;

        return *getPolyPolygon();
    }

    return tools::PolyPolygon();
}

basegfx::B2DPolyPolygon vcl::Region::GetAsB2DPolyPolygon() const
{
    if(getB2DPolyPolygon())
    {
        return *getB2DPolyPolygon();
    }

    if(getPolyPolygon())
    {
        const basegfx::B2DPolyPolygon aB2DPolyPolygon(getPolyPolygon()->getB2DPolyPolygon());
        const_cast< vcl::Region* >(this)->mpB2DPolyPolygon = aB2DPolyPolygon;

        return *getB2DPolyPolygon();
    }

    if(getRegionBand())
    {
        const basegfx::B2DPolyPolygon aB2DPolyPolygon(ImplCreateB2DPolyPolygonFromRegionBand());
        const_cast< vcl::Region* >(this)->mpB2DPolyPolygon = aB2DPolyPolygon;

        return *getB2DPolyPolygon();
    }

    return basegfx::B2DPolyPolygon();
}

const RegionBand* vcl::Region::GetAsRegionBand() const
{
    if(!getRegionBand())
    {
        if(getB2DPolyPolygon())
        {
            const_cast< vcl::Region* >(this)->mpRegionBand = ImplCreateRegionBandFromPolyPolygon(tools::PolyPolygon(*getB2DPolyPolygon()));
        }
        else if(getPolyPolygon())
        {
            const_cast< vcl::Region* >(this)->mpRegionBand = ImplCreateRegionBandFromPolyPolygon(*getPolyPolygon());
        }
    }

    return getRegionBand();
}

bool vcl::Region::Contains( const Point& rPoint ) const
{
    if(IsEmpty())
    {
        return false;
    }

    if(IsNull())
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
    if( IsEmpty() || IsNull() )
        return false;

    if( getB2DPolyPolygon() )
        return basegfx::utils::isRectangle( *getB2DPolyPolygon() );

    if( getPolyPolygon() )
        return getPolyPolygon()->IsRect();

    if( getRegionBand() )
        return (getRegionBand()->getRectangleCount() == 1);

    return false;
}

void vcl::Region::SetNull()
{
    mpB2DPolyPolygon.reset();
    mpPolyPolygon.reset();
    mpRegionBand.reset();
    mbIsNull = true;
    InvalidateCache();
}

void vcl::Region::SetEmpty()
{
    mpB2DPolyPolygon.reset();
    mpPolyPolygon.reset();
    mpRegionBand.reset();
    mbIsNull = false;
    InvalidateCache();
}

Region& vcl::Region::operator=(const vcl::Region& rRegion)
{
    if (this != &rRegion)
    {
        mpB2DPolyPolygon = rRegion.mpB2DPolyPolygon;
        mpPolyPolygon = rRegion.mpPolyPolygon;
        mpRegionBand = rRegion.mpRegionBand;
        mbIsNull = rRegion.mbIsNull;
        InvalidateCache();
    }

    return *this;
}

Region& vcl::Region::operator=( vcl::Region&& rRegion ) noexcept
{
    if (this != &rRegion)
    {
        mpB2DPolyPolygon = std::move(rRegion.mpB2DPolyPolygon);
        mpPolyPolygon = std::move(rRegion.mpPolyPolygon);
        mpRegionBand = std::move(rRegion.mpRegionBand);
        mbIsNull = rRegion.mbIsNull;
        rRegion.mbIsNull = true;

        InvalidateCache();
        rRegion.InvalidateCache();
    }

    return *this;
}

Region& vcl::Region::operator=( const tools::Rectangle& rRect )
{
    mpB2DPolyPolygon.reset();
    mpPolyPolygon.reset();
    if (!rRect.IsEmpty())
        mpRegionBand = std::make_shared<RegionBand>(rRect);
    else
        mpRegionBand.reset();
    mbIsNull = false;

    InvalidateCache();

    return *this;
}

bool vcl::Region::operator==( const vcl::Region& rRegion ) const
{
    if(IsNull() && rRegion.IsNull())
    {
        return true;
    }

    if(IsEmpty() && rRegion.IsEmpty())
    {
        return true;
    }

    if(getB2DPolyPolygon() && getB2DPolyPolygon() == rRegion.getB2DPolyPolygon())
    {
        return true;
    }

    if(getPolyPolygon() && getPolyPolygon() == rRegion.getPolyPolygon())
    {
        return true;
    }

    if(getRegionBand() && getRegionBand() == rRegion.getRegionBand())
    {
        return true;
    }

    if(IsNull() || IsEmpty())
    {
        return false;
    }

    if(rRegion.IsNull() || rRegion.IsEmpty())
    {
        return false;
    }

    if(rRegion.getB2DPolyPolygon() || getB2DPolyPolygon())
    {
        GetAsB2DPolyPolygon();
        rRegion.GetAsB2DPolyPolygon();

        return *rRegion.getB2DPolyPolygon() == *getB2DPolyPolygon();
    }

    if(rRegion.getPolyPolygon() || getPolyPolygon())
    {
        GetAsPolyPolygon();
        rRegion.GetAsPolyPolygon();

        return *rRegion.getPolyPolygon() == *getPolyPolygon();
    }

    if(rRegion.getRegionBand() && getRegionBand())
    {
        return *rRegion.getRegionBand() == *getRegionBand();
    }

    return false;
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
            rRegion.mpRegionBand = std::move(xNewRegionBand);

            bool bHasPolyPolygon(false);
            if (aCompat.GetVersion() >= 2)
            {
                rIStm.ReadCharAsBool( bHasPolyPolygon );

                if (bHasPolyPolygon)
                {
                    tools::PolyPolygon aNewPoly;
                    ReadPolyPolygon(rIStm, aNewPoly);
                    const auto nPolygons = aNewPoly.Count();
                    if (nPolygons > 128)
                    {
                        SAL_WARN("vcl.gdi", "suspiciously high no of polygons in clip:" << nPolygons);
                        if (comphelper::IsFuzzing())
                            aNewPoly.Clear();
                    }
                    rRegion.mpPolyPolygon = aNewPoly;
                }
            }

            if (!bSuccess && !bHasPolyPolygon)
            {
                SAL_WARN("vcl.gdi", "bad region band:" << bHasPolyPolygon);
                rRegion.SetNull();
            }

            rRegion.InvalidateCache(); // Trap 1: Write-through update
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

static bool ImplPolygonRectTest( const tools::Polygon& rPoly, tools::Rectangle* pRectOut = nullptr )
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

        if( ImplPolygonRectTest( rPoly ) )
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

        if( ImplPolygonRectTest( rPoly, &aRect ) )
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

Region::const_iterator Region::begin() const
{
    if (!mpxRectCache)
    {
        mpxRectCache = std::make_unique<RectangleVector>();
        GetRegionRectangles(*mpxRectCache);
    }
    return mpxRectCache->begin();
}

Region::const_iterator Region::end() const
{
    if (!mpxRectCache)
    {
        mpxRectCache = std::make_unique<RectangleVector>();
        GetRegionRectangles(*mpxRectCache);
    }
    return mpxRectCache->end();
}

void Region::InvalidateCache() const
{
    mpxRectCache.reset();
}

} /* namespace vcl */

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
