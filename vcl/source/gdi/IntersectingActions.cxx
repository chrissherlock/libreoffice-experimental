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
#include <vcl/svapp.hxx>
#include <vcl/bitmapaccess.hxx>

#include "pdfwriter_impl.hxx"
#include "IntersectingActions.hxx"

bool IntersectingActions::AreBoundsOver(tools::Rectangle const& rBounds)
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

void IntersectingActions::MarkSpecial(bool bTreatSpecial, MetaAction* pCurrAct)
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
    // metaactions in the IntersectingActions are not
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

void IntersectingActions::SetBackground(Color const& rBgColor, tools::Rectangle const& rBounds)
{
    aBgColor = rBgColor;
    aBounds = rBounds;
}

template <typename T> bool IntersectingActions::IsValidShape(T)
{
    SAL_WARN("vcl.gdi", "Should never call on this!");
    assert(false);
    return false;
}

template <> bool IntersectingActions::IsValidShape<MetaRectAction*>(MetaRectAction*)
{
    return true;
}

template <> bool IntersectingActions::IsValidShape<MetaPolygonAction*>(MetaPolygonAction* pAction)
{
    const tools::Polygon aPoly(pAction->GetPolygon());
    return !basegfx::utils::isRectangle(aPoly.getB2DPolygon());
}

template <>
bool IntersectingActions::IsValidShape<MetaPolyPolygonAction*>(MetaPolyPolygonAction* pAction)
{
    const tools::PolyPolygon aPoly(pAction->GetPolyPolygon());
    return aPoly.Count() != 1 || !basegfx::utils::isRectangle(aPoly[0].getB2DPolygon());
}

template <> bool IntersectingActions::IsValidShape<MetaWallpaperAction*>(MetaWallpaperAction*)
{
    return true;
}

bool IntersectingActions::IsBackgroundNotCovered(MetaAction* pAction,
                                                 OutputDevice const& rMapModeVDev)
{
    const tools::Rectangle& rCurrRect = GetBoundsRect(pAction);

    // shape needs to fully cover previous content, and have uniform
    // color
    return IsValidShape(pAction)
           || !(rMapModeVDev.LogicToPixel(rCurrRect).IsInside(aBounds)
                && rMapModeVDev.IsFillColor());
}

std::tuple<int, MetaAction*> IntersectingActions::ExtendAllBounds(GDIMetaFile const& rMtf,
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
std::tuple<int, bool> IntersectingActions::ExtendCurrentBounds(T*, VirtualDevice*, int, int)
{
    SAL_WARN("vcl.gdi", "Should never match");
    assert(false);
    return std::make_tuple(-1, false);
}

template <>
std::tuple<int, bool> IntersectingActions::ExtendCurrentBounds(MetaAction*, VirtualDevice*, int,
                                                               int)
{
    SAL_WARN("vcl.gdi", "Should never match");
    assert(false);
    return std::make_tuple(-1, false);
}

template <>
std::tuple<int, bool> IntersectingActions::ExtendCurrentBounds(MetaTransparentAction* pCurrAct,
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

std::tuple<int, bool> IntersectingActions::ExtendCurrentBounds_implementation(
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
std::tuple<int, bool> IntersectingActions::ExtendCurrentBounds(MetaRectAction* pCurrAct,
                                                               VirtualDevice* pMapModeVDev,
                                                               int nActionNum, int nLastBgAction)
{
    return ExtendCurrentBounds_implementation(pCurrAct, pMapModeVDev, nActionNum, nLastBgAction);
}

template <>
std::tuple<int, bool> IntersectingActions::ExtendCurrentBounds(MetaPolygonAction* pCurrAct,
                                                               VirtualDevice* pMapModeVDev,
                                                               int nActionNum, int nLastBgAction)
{
    return ExtendCurrentBounds_implementation(pCurrAct, pMapModeVDev, nActionNum, nLastBgAction);
}

template <>
std::tuple<int, bool> IntersectingActions::ExtendCurrentBounds(MetaPolyPolygonAction* pCurrAct,
                                                               VirtualDevice* pMapModeVDev,
                                                               int nActionNum, int nLastBgAction)
{
    return ExtendCurrentBounds_implementation(pCurrAct, pMapModeVDev, nActionNum, nLastBgAction);
}

template <>
std::tuple<int, bool> IntersectingActions::ExtendCurrentBounds(MetaWallpaperAction* pCurrAct,
                                                               VirtualDevice* pMapModeVDev,
                                                               int nActionNum, int nLastBgAction)
{
    return ExtendCurrentBounds_implementation(pCurrAct, pMapModeVDev, nActionNum, nLastBgAction);
}

int IntersectingActions::ReconstructVirtualDeviceMapMode(GDIMetaFile const& rMtf,
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

std::tuple<int, MetaAction*>
IntersectingActions::PruneBackgroundObjects(GDIMetaFile const& rMtf, VirtualDevice* pMapModeVDev)
{
    MetaAction* pCurrAct = nullptr;
    int nLastBgAction = 0;
    std::tie(nLastBgAction, pCurrAct) = ExtendAllBounds(rMtf, pMapModeVDev);

    // fast-forward until one after the last background action
    // (need to reconstruct map mode vdev state)
    int nActionNum = ReconstructVirtualDeviceMapMode(rMtf, pMapModeVDev, nLastBgAction);

    return std::make_tuple(nActionNum, pCurrAct);
}

/** #107169# Convert BitmapEx to Bitmap with appropriately blended
    color. Convert MetaTransparentAction to plain polygon,
    appropriately colored

    @param o_rMtf
    Add converted actions to this metafile
*/

template <typename T>
void ConvertTransparentAction(GDIMetaFile&, const T*, const OutputDevice*, Color)
{
    SAL_WARN("vcl.gdi", "Not a valid MetaAction type - needs a bitmap based action");
    assert(false);
}

template <>
void ConvertTransparentAction(GDIMetaFile& o_rMtf, const MetaTransparentAction* pTransAct,
                              const OutputDevice* rStateOutDev, Color)
{
    sal_uInt16 nTransparency(pTransAct->GetTransparence());

    // #i10613# Respect transparency for draw color
    if (nTransparency)
    {
        o_rMtf.AddAction(new MetaPushAction(PushFlags::LINECOLOR | PushFlags::FILLCOLOR));

        // assume white background for alpha blending
        Color aLineColor(rStateOutDev->GetLineColor());
        aLineColor.SetRed(static_cast<sal_uInt8>(
            (255 * nTransparency + (100 - nTransparency) * aLineColor.GetRed()) / 100));
        aLineColor.SetGreen(static_cast<sal_uInt8>(
            (255 * nTransparency + (100 - nTransparency) * aLineColor.GetGreen()) / 100));
        aLineColor.SetBlue(static_cast<sal_uInt8>(
            (255 * nTransparency + (100 - nTransparency) * aLineColor.GetBlue()) / 100));
        o_rMtf.AddAction(new MetaLineColorAction(aLineColor, true));

        Color aFillColor(rStateOutDev->GetFillColor());
        aFillColor.SetRed(static_cast<sal_uInt8>(
            (255 * nTransparency + (100 - nTransparency) * aFillColor.GetRed()) / 100));
        aFillColor.SetGreen(static_cast<sal_uInt8>(
            (255 * nTransparency + (100 - nTransparency) * aFillColor.GetGreen()) / 100));
        aFillColor.SetBlue(static_cast<sal_uInt8>(
            (255 * nTransparency + (100 - nTransparency) * aFillColor.GetBlue()) / 100));
        o_rMtf.AddAction(new MetaFillColorAction(aFillColor, true));
    }

    o_rMtf.AddAction(new MetaPolyPolygonAction(pTransAct->GetPolyPolygon()));

    if (nTransparency)
        o_rMtf.AddAction(new MetaPopAction());
}

static Bitmap Convert(BitmapEx const& rBmpEx, Color aBgColor)
{
    Bitmap aBmp(rBmpEx.GetBitmap());

    if (!rBmpEx.IsAlpha())
    {
        // blend with mask
        Bitmap::ScopedReadAccess pRA(aBmp);

        if (!pRA)
        {
            SAL_WARN("vcl.gdi", "Cannot get access to bitmap"); // what else should I do?
            assert(false);
        }

        Color aActualColor(aBgColor);

        if (pRA->HasPalette())
            aActualColor = pRA->GetBestPaletteColor(aBgColor);

        pRA.reset();

        // did we get true white?
        if (aActualColor.GetColorError(aBgColor))
        {
            // no, create truecolor bitmap, then
            aBmp.Convert(BmpConversion::N24Bit);

            // fill masked out areas white
            aBmp.Replace(rBmpEx.GetMask(), aBgColor);
        }
        else
        {
            // fill masked out areas white
            aBmp.Replace(rBmpEx.GetMask(), aActualColor);
        }
    }
    else
    {
        // blend with alpha channel
        aBmp.Convert(BmpConversion::N24Bit);
        aBmp.Blend(rBmpEx.GetAlpha(), aBgColor);
    }

    return aBmp;
}

template <>
void ConvertTransparentAction(GDIMetaFile& o_rMtf, const MetaBmpExAction* pAct, const OutputDevice*,
                              Color aBgColor)
{
    Bitmap aBmp(Convert(pAct->GetBitmapEx(), aBgColor));
    o_rMtf.AddAction(new MetaBmpAction(pAct->GetPoint(), aBmp));
}

template <>
void ConvertTransparentAction(GDIMetaFile& o_rMtf, const MetaBmpExScaleAction* pAct,
                              const OutputDevice*, Color aBgColor)
{
    Bitmap aBmp(Convert(pAct->GetBitmapEx(), aBgColor));
    o_rMtf.AddAction(new MetaBmpScaleAction(pAct->GetPoint(), pAct->GetSize(), aBmp));
}

template <>
void ConvertTransparentAction(GDIMetaFile& o_rMtf, const MetaBmpExScalePartAction* pAct,
                              const OutputDevice*, Color aBgColor)
{
    Bitmap aBmp(Convert(pAct->GetBitmapEx(), aBgColor));
    o_rMtf.AddAction(new MetaBmpScalePartAction(pAct->GetDestPoint(), pAct->GetDestSize(),
                                                pAct->GetSrcPoint(), pAct->GetSrcSize(), aBmp));
}

void IntersectingActions::AddAction(GDIMetaFile& rOutMtf, Color const& rBgColor,
                                    MetaAction* pCurrAct, VirtualDevice* pMapModeVDev) const
{
    // NOTE: This relies on the fact that map-mode or draw
    // mode changing actions are solitary aIntersectingActions elements and
    // have empty bounding boxes, see comment on stage 2.1
    // above
    if (aBounds.IsEmpty() || !bIsSpecial)
    {
        // #107169# Treat transparent bitmaps special, if they
        // are the first (or sole) action in their bounds
        // list. Note that we previously ensured that no
        // fully-transparent objects are before us here.
        if (DoesActionHandleTransparency(*pCurrAct) && aActionList.begin()->first == pCurrAct)
        {
            // convert actions, where masked-out parts are of
            // given background color
            ConvertTransparentAction(rOutMtf, pCurrAct, pMapModeVDev, rBgColor);
        }
        else
        {
            // simply add this action
            rOutMtf.AddAction(pCurrAct);
        }

        pCurrAct->Execute(pMapModeVDev);
    }
}
/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
