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

#include <utility>
#include <list>
#include <vector>
#include <tuple>

#include <basegfx/polygon/b2dpolygontools.hxx>
#include <sal/log.hxx>
#include <officecfg/Office/Common.hxx>

#include <vcl/Printer.hxx>
#include <vcl/PrinterOptions.hxx>
#include <vcl/virdev.hxx>
#include <vcl/MetaPixelAction.hxx>
#include <vcl/MetaPointAction.hxx>
#include <vcl/MetaLineAction.hxx>
#include <vcl/MetaRectAction.hxx>
#include <vcl/MetaRoundRectAction.hxx>
#include <vcl/MetaEllipseAction.hxx>
#include <vcl/MetaArcAction.hxx>
#include <vcl/MetaPieAction.hxx>
#include <vcl/MetaChordAction.hxx>
#include <vcl/MetaPolyLineAction.hxx>
#include <vcl/MetaPolygonAction.hxx>
#include <vcl/MetaPolyPolygonAction.hxx>
#include <vcl/MetaTextAction.hxx>
#include <vcl/MetaTextArrayAction.hxx>
#include <vcl/MetaStretchTextAction.hxx>
#include <vcl/MetaTextRectAction.hxx>
#include <vcl/MetaBmpAction.hxx>
#include <vcl/MetaBmpScaleAction.hxx>
#include <vcl/MetaBmpExAction.hxx>
#include <vcl/MetaBmpScalePartAction.hxx>
#include <vcl/MetaBmpExScaleAction.hxx>
#include <vcl/MetaBmpExScalePartAction.hxx>
#include <vcl/MetaMaskAction.hxx>
#include <vcl/MetaMaskScaleAction.hxx>
#include <vcl/MetaMaskScalePartAction.hxx>
#include <vcl/MetaGradientAction.hxx>
#include <vcl/MetaGradientExAction.hxx>
#include <vcl/MetaHatchAction.hxx>
#include <vcl/MetaWallpaperAction.hxx>
#include <vcl/MetaLineColorAction.hxx>
#include <vcl/MetaFillColorAction.hxx>
#include <vcl/MetaMapModeAction.hxx>
#include <vcl/MetaPushAction.hxx>
#include <vcl/MetaPopAction.hxx>
#include <vcl/MetaTransparentAction.hxx>
#include <vcl/MetaFloatTransparentAction.hxx>
#include <vcl/MetaEPSAction.hxx>
#include <vcl/MetaCommentAction.hxx>
#include <vcl/gdimtf.hxx>
#include <vcl/svapp.hxx>
#include <vcl/bitmapaccess.hxx>

#include "pdfwriter_impl.hxx"
#include "IntersectingActions.hxx"
#include "IntersectingActionsSet.hxx"

#define MAX_TILE_WIDTH 1024
#define MAX_TILE_HEIGHT 1024

template <typename T> bool UsesTiling(T*) { return false; }

template <> bool UsesTiling(Printer*) { return true; }

template <typename T>
void DrawAction(T* pCurrAct, VirtualDevice* pPaintVDev, VirtualDevice*, Point, OutputDevice*)
{
    pCurrAct->Execute(pPaintVDev);
}

template <>
void DrawAction(MetaMapModeAction* pCurrAct, VirtualDevice* pPaintVDev, VirtualDevice* pMapVDev,
                Point aDstPtPix, OutputDevice*)
{
    pCurrAct->Execute(pMapVDev);

    MapMode aMtfMap(pMapVDev->GetMapMode());
    const Point aNewOrg(pMapVDev->PixelToLogic(aDstPtPix));

    aMtfMap.SetOrigin(Point(-aNewOrg.X(), -aNewOrg.Y()));
    pPaintVDev->SetMapMode(aMtfMap);
}

template <>
void DrawAction(MetaPushAction* pCurrAct, VirtualDevice* pPaintVDev, VirtualDevice* pMapVDev, Point,
                OutputDevice*)
{
    pCurrAct->Execute(pMapVDev);
    pCurrAct->Execute(pPaintVDev);
}

template <>
void DrawAction(MetaPopAction* pCurrAct, VirtualDevice* pPaintVDev, VirtualDevice* pMapVDev, Point,
                OutputDevice*)
{
    pCurrAct->Execute(pMapVDev);
    pCurrAct->Execute(pPaintVDev);
}
template <typename T> void DrawGradient(MetaGradientAction* pCurrAct, VirtualDevice*, T* pOutDev)
{
    pOutDev->DrawGradient(pCurrAct->GetRect(), pCurrAct->GetGradient());
}

template <>
void DrawGradient(MetaGradientAction* pCurrAct, VirtualDevice* pPaintVDev, Printer* pPrinter)
{
    pPrinter->DrawGradientEx(pPaintVDev, pCurrAct->GetRect(), pCurrAct->GetGradient());
}

template <>
void DrawAction(MetaGradientAction* pCurrAct, VirtualDevice* pPaintVDev, VirtualDevice*, Point,
                OutputDevice* pOutDev)
{
    DrawGradient(pCurrAct, pPaintVDev, pOutDev);
}

bool IntersectingActionsSet::ProcessIntersections(IntersectingActions& rTotalActions,
                                                  tools::Rectangle& rTotalBounds,
                                                  bool bTreatSpecial)
{
    bool bSomeActionsChanged;

    // now, this is unfortunate: since changing anyone of
    // the aIntersectingActions elements (e.g. by merging or addition
    // of an action) might generate new intersection with
    // other aIntersectingActions elements, have to repeat the whole
    // element scanning, until nothing changes anymore.
    // Thus, this loop here makes us O(n^3) in the worst
    // case.
    do
    {
        // only loop here if 'intersects' branch below was hit
        bSomeActionsChanged = false;

        // iterate over all current members of aIntersectingActions
        for (auto currCC = maIntersectingActions.begin(); currCC != maIntersectingActions.end();)
        {
            // first check if current element's bounds are
            // empty. This ensures that empty actions are not
            // merged into one component, as a matter of fact,
            // they have no position.

            // #107169# Wholly transparent objects need
            // not be considered for connected components,
            // too. Just put each of them into a separate
            // component.
            if (currCC->AreBoundsOver(rTotalBounds))
            {
                // union the intersecting aIntersectingActions element into aTotalActions

                // calc union bounding box
                rTotalBounds.Union(currCC->GetBoundingRect());

                // extract all aCurr actions to aTotalActions
                rTotalActions.GetActionList().splice(rTotalActions.GetActionList().end(),
                                                     currCC->GetActionList());

                if (currCC->IsSpecial())
                    bTreatSpecial = true;

                // remove and delete currCC element from list (we've now merged its content)
                currCC = maIntersectingActions.erase(currCC);

                // at least one component changed, need to rescan everything
                bSomeActionsChanged = true;
            }
            else
            {
                ++currCC;
            }
        }
    } while (bSomeActionsChanged);

    return bTreatSpecial;
}

std::tuple<bool, IntersectingActions, tools::Rectangle>
IntersectingActionsSet::SearchForIntersectingEntries(MetaAction* pCurrAct,
                                                     VirtualDevice* pMapModeVDev,
                                                     IntersectingActions rBackgroundAction)
{
    // cache bounds of current action
    const tools::Rectangle aCurrActionBounds(pCurrAct->GetBoundsRect(pMapModeVDev));

    // accumulate collected bounds here, initialize with current action
    tools::Rectangle aTotalBounds(aCurrActionBounds); // thus, aTotalActions.aBounds is empty
        // for non-output-generating actions
    bool bTreatSpecial = false;
    IntersectingActions aTotalActions;

    //  STAGE 2.1: Search for intersecting cc entries

    // if aCurrActionBounds is empty, it will intersect with no
    // aIntersectingActions member. Thus, we can save the check.
    // Furthermore, this ensures that non-output-generating
    // actions get their own aIntersectingActions entry, which is necessary
    // when copying them to the output metafile (see stage 4
    // below).

    // #107169# Wholly transparent objects need
    // not be considered for connected components,
    // too. Just put each of them into a separate
    // component.
    aTotalActions.SetFullyTransparentFlag(pCurrAct->IsTransparent(pMapModeVDev));

    if (!aCurrActionBounds.IsEmpty() && !aTotalActions.IsFullyTransparent())
    {
        if (!rBackgroundAction.GetActionList().empty()
            && !rBackgroundAction.GetBoundingRect().IsInside(aTotalBounds))
        {
            // it seems the background is not large enough. to
            // be on the safe side, combine with this component.
            aTotalBounds.Union(rBackgroundAction.GetBoundingRect());

            // extract all aCurr actions to aTotalActions
            aTotalActions.GetActionList().splice(aTotalActions.GetActionList().end(),
                                                 rBackgroundAction.GetActionList());

            if (rBackgroundAction.IsSpecial())
                bTreatSpecial = true;
        }

        bTreatSpecial = ProcessIntersections(aTotalActions, aTotalBounds, bTreatSpecial);
    }

    return std::make_tuple(bTreatSpecial, aTotalActions, aTotalBounds);
}

void IntersectingActionsSet::GenerateIntersectingActions(IntersectingActions& rBackgroundAction,
                                                         MetaAction* pInitialAction, int nActionNum,
                                                         GDIMetaFile const& rInMtf,
                                                         VirtualDevice* pMapModeVDev)
{
    // iterate over all actions (start where background action
    // search left off)
    for (MetaAction *pCurrAct = pInitialAction; pCurrAct;
         pCurrAct = const_cast<GDIMetaFile&>(rInMtf).NextAction(), ++nActionNum)
    {
        // execute action to get correct MapModes etc.
        pCurrAct->Execute(pMapModeVDev);

        bool bTreatSpecial;
        tools::Rectangle aTotalBounds;
        IntersectingActions aTotalActions;

        std::tie(bTreatSpecial, aTotalActions, aTotalBounds)
            = SearchForIntersectingEntries(pCurrAct, pMapModeVDev, rBackgroundAction);

        //  STAGE 2.2: Determine special state for cc element
        aTotalActions.MarkSpecial(bTreatSpecial, pCurrAct);

        //  STAGE 2.3: Add newly generated CC list element

        // set new bounds and add action to list
        aTotalActions.SetBoundingRect(aTotalBounds);
        aTotalActions.GetActionList().emplace_back(pCurrAct, nActionNum);

        // add aTotalActions as a new entry to rIntersectingActions
        maIntersectingActions.push_back(aTotalActions);

        SAL_WARN_IF(aTotalActions.GetActionList().empty(), "vcl",
                    "Printer::GetPreparedMetaFile empty component");
        SAL_WARN_IF(aTotalActions.GetBoundingRect().IsEmpty()
                        && (aTotalActions.GetActionList().size() != 1),
                    "vcl",
                    "Printer::GetPreparedMetaFile non-output generating actions must be solitary");
        SAL_WARN_IF(
            aTotalActions.IsFullyTransparent() && (aTotalActions.GetActionList().size() != 1),
            "vcl", "Printer::GetPreparedMetaFile fully transparent actions must be solitary");
    }
}

static double GetReduceTransparencyMinArea()
{
    double fReduceTransparencyMinArea
        = officecfg::Office::Common::VCL::ReduceTransparencyMinArea::get() / 100.0;
    SAL_WARN_IF(fReduceTransparencyMinArea > 1.0, "vcl",
                "Value of ReduceTransparencyMinArea config option is too high");
    SAL_WARN_IF(fReduceTransparencyMinArea < 0.0, "vcl",
                "Value of ReduceTransparencyMinArea config option is too low");
    fReduceTransparencyMinArea = std::clamp(fReduceTransparencyMinArea, 0.0, 1.0);

    return fReduceTransparencyMinArea;
}

static bool DoesOutputExceedBitmapArea(tools::Rectangle aBoundRect, tools::Rectangle aOutputRect)
{
    const double fBmpArea(static_cast<double>(aBoundRect.GetWidth()) * aBoundRect.GetHeight());
    const double fOutArea(static_cast<double>(aOutputRect.GetWidth()) * aOutputRect.GetHeight());

    // Read the configuration value of minimal object area where transparency will be removed
    return fBmpArea > (GetReduceTransparencyMinArea() * fOutArea);
}

void IntersectingActionsSet::UnmarkIntersectingActions(tools::Rectangle aOutputRect,
                                                       bool bReduceTransparency,
                                                       bool bTransparencyAutoMode)
{
    for (auto& currentItem : maIntersectingActions)
    {
        if (currentItem.IsSpecial())
        {
            tools::Rectangle aBoundRect(currentItem.GetBoundingRect());
            aBoundRect.Intersection(aOutputRect);

            if (bReduceTransparency && bTransparencyAutoMode
                && DoesOutputExceedBitmapArea(aBoundRect, aOutputRect))
            {
                // output normally. Therefore, we simply clear the
                // special attribute, as everything non-special is
                // copied to rOutMtf further below.
                currentItem.SetSpecialFlag(false);
            }
        }
    }
}

static void
DrawAllActions(OutputDevice* pOutDev, GDIMetaFile const& rInMtf, VirtualDevice* pPaintVDev,
               VirtualDevice* pMapVDev,
               ::std::vector<const IntersectingActions*>& rIntersectingActions_MemberMap,
               IntersectingActions* pCurrentItem, Point aDstPtPix)
{
    int nActionNum = 0;

    for (MetaAction *pCurrAct = const_cast<GDIMetaFile&>(rInMtf).FirstAction(); pCurrAct;
         pCurrAct = const_cast<GDIMetaFile&>(rInMtf).NextAction(), ++nActionNum)
    {
        // enable output only for
        // actions that are members of
        // the current aIntersectingActions element
        // (currentItem)
        pPaintVDev->EnableOutput(false);

        if (rIntersectingActions_MemberMap[nActionNum] == pCurrentItem)
            pPaintVDev->EnableOutput();

        DrawAction(pCurrAct, pPaintVDev, pMapVDev, aDstPtPix, pOutDev);

        Application::Reschedule(true);
    }

    pPaintVDev->EnableOutput();
}

static Size GetDestSizeHeight(OutputDevice* pOutDev, tools::Rectangle aBoundRect, Point aDstPtPix)
{
    Size aDstSzPix;
    aDstSzPix = UsesTiling(pOutDev) ? Size(MAX_TILE_WIDTH, MAX_TILE_HEIGHT) : aBoundRect.GetSize();

    if ((aDstPtPix.Y() + aDstSzPix.Height() - 1) > aBoundRect.Bottom())
        aDstSzPix.setHeight(aBoundRect.Bottom() - aDstPtPix.Y() + 1);

    return aDstSzPix;
}

static Size GetDestSizeWidth(tools::Rectangle aBoundRect, Size aDstSzPix, Point aDstPtPix)
{
    if ((aDstPtPix.X() + aDstSzPix.Width() - 1) > aBoundRect.Right())
        aDstSzPix.setWidth(aBoundRect.Right() - aDstPtPix.X() + 1);

    return aDstSzPix;
}

class SavePaintStateGuard
{
public:
    SavePaintStateGuard(OutputDevice* pOutDev, VirtualDevice* pPaintVDev, VirtualDevice* pMapVDev)
        : mpOutDev(pOutDev)
        , mpPaintVDev(pPaintVDev)
        , mpMapVDev(pMapVDev)
        , mbMapState(pOutDev->IsMapModeEnabled())
    {
        pPaintVDev->Push();
        pMapVDev->Push();
    }

    ~SavePaintStateGuard()
    {
        mpPaintVDev->EnableMapMode();
        mpOutDev->EnableMapMode(mbMapState);
        mpMapVDev->Pop();
        mpPaintVDev->Pop();
    }

    void SaveAndDisableMapState()
    {
        mbMapState = mpOutDev->IsMapModeEnabled();
        mpOutDev->EnableMapMode(false);
        mpPaintVDev->EnableMapMode(false);
    }

private:
    OutputDevice* mpOutDev;
    VirtualDevice* mpPaintVDev;
    VirtualDevice* mpMapVDev;
    bool mbMapState;
};

void IntersectingActionsSet::PopulateIntersectingActionsMap(
    ::std::vector<const IntersectingActions*>& rIntersectingActions_MemberMap, size_t nSize)
{
    rIntersectingActions_MemberMap.reserve(nSize);

    // iterate over all aIntersectingActions members and their contained metaactions
    for (auto const& currentItem : maIntersectingActions)
    {
        for (auto const& currentAction : currentItem.GetActionList())
        {
            // set pointer to aIntersectingActions element for corresponding index
            rIntersectingActions_MemberMap[currentAction.second] = &currentItem;
        }
    }
}

static void
CreateBitmapAction(GDIMetaFile& rOutMtf, OutputDevice* pOutDev, GDIMetaFile const& rInMtf,
                   ::std::vector<const IntersectingActions*>& rIntersectingActions_MemberMap,
                   IntersectingActions* pCurrentItem, IntersectingActions const& rBackgroundAction,
                   tools::Rectangle aBoundRect, long nMaxBmpDPIX, long nMaxBmpDPIY,
                   bool bDownsampleBitmaps)
{
    // create new bitmap action first
    if (aBoundRect.GetWidth() && aBoundRect.GetHeight())
    {
        Point aDstPtPix(aBoundRect.TopLeft());

        // record only mapmode information
        ScopedVclPtrInstance<VirtualDevice> aMapVDev;
        aMapVDev->EnableOutput(false);

        ScopedVclPtrInstance<VirtualDevice> aPaintVDev; // into this one, we render.
        aPaintVDev->SetBackground(rBackgroundAction.GetBackgroundColor());

        rOutMtf.AddAction(new MetaPushAction(PushFlags::MAPMODE));
        rOutMtf.AddAction(new MetaMapModeAction());

        aPaintVDev->SetDrawMode(pOutDev->GetDrawMode());

        while (aDstPtPix.Y() <= aBoundRect.Bottom())
        {
            aDstPtPix.setX(aBoundRect.Left());
            Size aDstSzPix(GetDestSizeHeight(pOutDev, aBoundRect, aDstPtPix));

            while (aDstPtPix.X() <= aBoundRect.Right())
            {
                Size aDstBandSzPix(GetDestSizeWidth(aBoundRect, aDstSzPix, aDstPtPix));

                if (!tools::Rectangle(aDstPtPix, aDstSzPix).Intersection(aBoundRect).IsEmpty()
                    && aPaintVDev->SetOutputSizePixel(aDstSzPix))
                {
                    SavePaintStateGuard aPaintState(pOutDev, aPaintVDev.get(), aMapVDev.get());

                    aMapVDev->SetDPIX(pOutDev->GetDPIX());
                    aPaintVDev->SetDPIX(pOutDev->GetDPIX());
                    aMapVDev->SetDPIY(pOutDev->GetDPIY());
                    aPaintVDev->SetDPIY(pOutDev->GetDPIY());

                    DrawAllActions(pOutDev, rInMtf, aPaintVDev.get(), aMapVDev.get(),
                                   rIntersectingActions_MemberMap, pCurrentItem, aDstPtPix);

                    aPaintState.SaveAndDisableMapState();

                    Bitmap aBandBmp(aPaintVDev->GetBitmap(Point(), aDstSzPix));

                    // scale down bitmap, if requested
                    if (bDownsampleBitmaps)
                    {
                        aBandBmp = pOutDev->GetDownsampledBitmap(aDstBandSzPix, Point(),
                                                                 aBandBmp.GetSizePixel(), aBandBmp,
                                                                 nMaxBmpDPIX, nMaxBmpDPIY);
                    }

                    rOutMtf.AddAction(new MetaCommentAction("PRNSPOOL_TRANSPARENTBITMAP_BEGIN"));
                    rOutMtf.AddAction(new MetaBmpScaleAction(aDstPtPix, aDstBandSzPix, aBandBmp));
                    rOutMtf.AddAction(new MetaCommentAction("PRNSPOOL_TRANSPARENTBITMAP_END"));
                }

                // overlapping bands to avoid missing lines (e.g. PostScript)
                aDstPtPix.AdjustX(aDstSzPix.Width());
            }

            // overlapping bands to avoid missing lines (e.g. PostScript)
            aDstPtPix.AdjustY(aDstSzPix.Height());
        }

        rOutMtf.AddAction(new MetaPopAction());
    }
}

void IntersectingActionsSet::CreateBitmapActions(
    GDIMetaFile& rOutMtf, OutputDevice* pOutDev, GDIMetaFile const& rInMtf,
    ::std::vector<const IntersectingActions*>& rIntersectingActions_MemberMap,
    tools::Rectangle aOutputRect, IntersectingActions const& rBackgroundAction, long nMaxBmpDPIX,
    long nMaxBmpDPIY, bool bDownsampleBitmaps, bool bReduceTransparency, bool bTransparencyAutoMode)
{
    for (auto& currentItem : maIntersectingActions)
    {
        if (currentItem.IsSpecial())
        {
            tools::Rectangle aBoundRect(currentItem.GetBoundingRect());
            aBoundRect.Intersection(aOutputRect);

            if (!(bReduceTransparency && bTransparencyAutoMode
                  && DoesOutputExceedBitmapArea(aBoundRect, aOutputRect)))
            {
                CreateBitmapAction(rOutMtf, pOutDev, rInMtf, rIntersectingActions_MemberMap,
                                   &currentItem, rBackgroundAction, aBoundRect, nMaxBmpDPIX,
                                   nMaxBmpDPIY, bDownsampleBitmaps);
            }
        }
    }
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
