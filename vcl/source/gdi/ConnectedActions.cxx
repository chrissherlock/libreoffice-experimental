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

#include <basegfx/polygon/b2dpolygontools.hxx>
#include <sal/log.hxx>
#include <officecfg/Office/Common.hxx>

#include <vcl/gdimtf.hxx>
#include <vcl/MetaRectAction.hxx>
#include <vcl/MetaPolygonAction.hxx>
#include <vcl/MetaPolyPolygonAction.hxx>
#include <vcl/MetaWallpaperAction.hxx>
#include <vcl/MetaTransparentAction.hxx>
#include <vcl/svapp.hxx>
#include <vcl/bitmapaccess.hxx>

#include "pdfwriter_impl.hxx"
#include "ConnectedActions.hxx"

bool ConnectedActions::AreBoundsOver(tools::Rectangle const& rBounds)
{
    return (!aBounds.IsEmpty() && !bIsFullyTransparent && aBounds.IsOver(rBounds));
}

/** Determines whether the action can handle transparency correctly
  (i.e. when painted on white background, does the action still look
  correct)?
 */
bool DoesActionHandleTransparency(const MetaAction& rAct)
{
    // MetaActionType::FLOATTRANSPARENT can contain a whole metafile,
    // which is to be rendered with the given transparent gradient. We
    // currently cannot emulate transparent painting on a white
    // background reliably.

    // the remainder can handle printing itself correctly on a uniform
    // white background.
    switch (rAct.GetType())
    {
        case MetaActionType::Transparent:
        case MetaActionType::BMPEX:
        case MetaActionType::BMPEXSCALE:
        case MetaActionType::BMPEXSCALEPART:
            return true;

        default:
            return false;
    }
}

void ConnectedActions::MarkSpecial(bool bTreatSpecial, MetaAction* pCurrAct)
{
    // now test whether the whole connected component must be
    // treated specially (i.e. rendered as a bitmap): if the
    // added action is the very first action, or all actions
    // before it are completely transparent, the connected
    // component need not be treated specially, not even if
    // the added action contains transparency. This is because
    // painting of transparent objects on _white background_
    // works without alpha compositing (you just calculate the
    // color). Note that for the test "all objects before me
    // are transparent" no sorting is necessary, since the
    // added metaaction pCurrAct is always in the order the
    // metafile is painted. Generally, the order of the
    // metaactions in the ConnectedActions are not
    // guaranteed to be the same as in the metafile.
    if (bTreatSpecial)
    {
        // prev component(s) special -> this one, too
        bIsSpecial = true;
    }
    else if (!pCurrAct->IsTransparent())
    {
        // added action and none of prev components special ->
        // this one normal, too
        bIsSpecial = false;
    }
    else
    {
        // added action is special and none of prev components
        // special -> do the detailed tests

        // can the action handle transparency correctly
        // (i.e. when painted on white background, does the
        // action still look correct)?
        if (!DoesActionHandleTransparency(*pCurrAct))
        {
            // no, action cannot handle its transparency on
            // a printer device, render to bitmap
            bIsSpecial = true;
        }
        else
        {
            // yes, action can handle its transparency, so
            // check whether we're on white background
            if (aActionList.empty())
            {
                // nothing between pCurrAct and page
                // background -> don't be special
                bIsSpecial = false;
            }
            else
            {
                // #107169# Fixes above now ensure that _no_
                // object in the list is fully transparent. Thus,
                // if the component list is not empty above, we
                // must assume that we have to treat this
                // component special.

                // there are non-transparent objects between
                // pCurrAct and the empty sheet of paper -> be
                // special, then
                bIsSpecial = true;
            }
        }
    }
}
template <typename T> tools::Rectangle GetBoundsRect(T) { return tools::Rectangle(); }

template <> tools::Rectangle GetBoundsRect<MetaRectAction*>(MetaRectAction* pAction)
{
    return pAction->GetRect();
}

template <> tools::Rectangle GetBoundsRect<MetaPolygonAction*>(MetaPolygonAction* pAction)
{
    return pAction->GetPolygon().GetBoundRect();
}

template <> tools::Rectangle GetBoundsRect<MetaPolyPolygonAction*>(MetaPolyPolygonAction* pAction)
{
    return pAction->GetPolyPolygon().GetBoundRect();
}

template <> tools::Rectangle GetBoundsRect<MetaWallpaperAction*>(MetaWallpaperAction* pAction)
{
    return pAction->GetRect();
}

void ConnectedActions::SetBackground(Color const& rBgColor, tools::Rectangle const& rBounds)
{
    aBgColor = rBgColor;
    aBounds = rBounds;
}

template <typename T> bool ConnectedActions::IsValidShape(T)
{
    SAL_WARN("vcl.gdi", "Should never call on this!");
    assert(false);
    return false;
}

template <> bool ConnectedActions::IsValidShape<MetaRectAction*>(MetaRectAction*) { return true; }

template <> bool ConnectedActions::IsValidShape<MetaPolygonAction*>(MetaPolygonAction* pAction)
{
    const tools::Polygon aPoly(pAction->GetPolygon());
    return !basegfx::utils::isRectangle(aPoly.getB2DPolygon());
}

template <>
bool ConnectedActions::IsValidShape<MetaPolyPolygonAction*>(MetaPolyPolygonAction* pAction)
{
    const tools::PolyPolygon aPoly(pAction->GetPolyPolygon());
    return aPoly.Count() != 1 || !basegfx::utils::isRectangle(aPoly[0].getB2DPolygon());
}

template <> bool ConnectedActions::IsValidShape<MetaWallpaperAction*>(MetaWallpaperAction*)
{
    return true;
}

bool ConnectedActions::IsBackgroundNotCovered(MetaAction* pAction, OutputDevice const& rMapModeVDev)
{
    const tools::Rectangle& rCurrRect = GetBoundsRect(pAction);

    // shape needs to fully cover previous content, and have uniform
    // color
    return IsValidShape(pAction)
           || !(rMapModeVDev.LogicToPixel(rCurrRect).IsInside(aBounds)
                && rMapModeVDev.IsFillColor());
}

std::tuple<int, MetaAction*> ConnectedActions::ExtendAllBounds(GDIMetaFile const& rMtf,
                                                               VirtualDevice* pMapModeVDev)
{
    MetaAction* pCurrAct = const_cast<GDIMetaFile&>(rMtf).FirstAction();

    bool bStillBackground = true; // true until first non-bg action
    int nActionNum = 0;
    int nLastBgAction = -1;

    while (pCurrAct && bStillBackground)
    {
        std::tie(nLastBgAction, bStillBackground)
            = ExtendCurrentBounds(pCurrAct, pMapModeVDev, nActionNum, nLastBgAction);

        // execute action to get correct MapModes etc.
        pCurrAct->Execute(pMapModeVDev);

        pCurrAct = const_cast<GDIMetaFile&>(rMtf).NextAction();
        ++nActionNum;
    }

    return std::make_tuple(nLastBgAction, pCurrAct);
}

template <typename T>
std::tuple<int, bool> ConnectedActions::ExtendCurrentBounds(T*, VirtualDevice*, int, int)
{
    SAL_WARN("vcl.gdi", "Should never match");
    assert(false);
    return std::make_tuple(-1, false);
}

template <>
std::tuple<int, bool> ConnectedActions::ExtendCurrentBounds(MetaAction*, VirtualDevice*, int, int)
{
    SAL_WARN("vcl.gdi", "Should never match");
    assert(false);
    return std::make_tuple(-1, false);
}

template <>
std::tuple<int, bool> ConnectedActions::ExtendCurrentBounds(MetaTransparentAction* pCurrAct,
                                                            VirtualDevice* pMapModeVDev, int,
                                                            int nLastBgAction)
{
    if (pCurrAct->IsTransparent(pMapModeVDev))
    {
        // extend current bounds (next uniform action needs to fully cover this area)
        SetBackgroundAction(pCurrAct, pMapModeVDev);
        return std::make_tuple(nLastBgAction, true);
    }
    else
    {
        return std::make_tuple(nLastBgAction, false);
    }
}

std::tuple<int, bool> ConnectedActions::ExtendCurrentBounds_implementation(
    MetaAction* pCurrAct, VirtualDevice* pMapModeVDev, int nActionNum, int nLastBgAction)
{
    if (IsBackgroundNotCovered(pCurrAct, *pMapModeVDev))
    {
        SetBackgroundAction(pCurrAct, pMapModeVDev);
        return std::make_tuple(nLastBgAction, true);
    }
    else
    {
        return std::make_tuple(nActionNum, false);
    }
}

template <>
std::tuple<int, bool> ConnectedActions::ExtendCurrentBounds(MetaRectAction* pCurrAct,
                                                            VirtualDevice* pMapModeVDev,
                                                            int nActionNum, int nLastBgAction)
{
    return ExtendCurrentBounds_implementation(pCurrAct, pMapModeVDev, nActionNum, nLastBgAction);
}

template <>
std::tuple<int, bool> ConnectedActions::ExtendCurrentBounds(MetaPolygonAction* pCurrAct,
                                                            VirtualDevice* pMapModeVDev,
                                                            int nActionNum, int nLastBgAction)
{
    return ExtendCurrentBounds_implementation(pCurrAct, pMapModeVDev, nActionNum, nLastBgAction);
}

template <>
std::tuple<int, bool> ConnectedActions::ExtendCurrentBounds(MetaPolyPolygonAction* pCurrAct,
                                                            VirtualDevice* pMapModeVDev,
                                                            int nActionNum, int nLastBgAction)
{
    return ExtendCurrentBounds_implementation(pCurrAct, pMapModeVDev, nActionNum, nLastBgAction);
}

template <>
std::tuple<int, bool> ConnectedActions::ExtendCurrentBounds(MetaWallpaperAction* pCurrAct,
                                                            VirtualDevice* pMapModeVDev,
                                                            int nActionNum, int nLastBgAction)
{
    return ExtendCurrentBounds_implementation(pCurrAct, pMapModeVDev, nActionNum, nLastBgAction);
}

int ConnectedActions::ReconstructVirtualDeviceMapMode(GDIMetaFile const& rMtf,
                                                      VirtualDevice* pMapModeVDev,
                                                      int nLastBgAction)
{
    int nActionNum = 0;

    pMapModeVDev->ClearStack(); // clean up pMapModeVDev

    MetaAction* pCurrAct = const_cast<GDIMetaFile&>(rMtf).FirstAction();
    while (pCurrAct && nActionNum <= nLastBgAction)
    {
        // up to and including last ink-generating background
        // action go to background component
        aActionList.emplace_back(pCurrAct, nActionNum);

        // execute action to get correct MapModes etc.
        pCurrAct->Execute(pMapModeVDev);
        pCurrAct = const_cast<GDIMetaFile&>(rMtf).NextAction();
        ++nActionNum;
    }

    return nActionNum;
}

std::tuple<int, MetaAction*> ConnectedActions::PruneBackgroundObjects(GDIMetaFile const& rMtf,
                                                                      VirtualDevice* pMapModeVDev)
{
    MetaAction* pCurrAct = nullptr;
    int nLastBgAction = 0;
    std::tie(nLastBgAction, pCurrAct) = ExtendAllBounds(rMtf, pMapModeVDev);

    // fast-forward until one after the last background action
    // (need to reconstruct map mode vdev state)
    int nActionNum = ReconstructVirtualDeviceMapMode(rMtf, pMapModeVDev, nLastBgAction);

    return std::make_tuple(nActionNum, pCurrAct);
}
/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
