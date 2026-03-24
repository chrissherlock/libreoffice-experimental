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

#include <osl/diagnose.h>
#include <officecfg/Office/Common.hxx>
#include <basegfx/polygon/b2dpolygontools.hxx>
#include <tools/mapunit.hxx>

#include <vcl/outdev.hxx>
#include <vcl/virdev.hxx>
#include <iostream>
#include <vcl/print.hxx>
#include <vcl/metafile/MetaAction.hxx>
#include <vcl/metafile/MetafileRecorder.hxx>
#include <vcl/metafile/MetaActionType.hxx>
#include <vcl/metafile/TransparencyFlattener.hxx>
#include <vcl/svapp.hxx>
#include <vcl/text/TextSpan.hxx>
#include <vcl/text/LayoutCacheData.hxx>
#include <vcl/BitmapWriteAccess.hxx>
#include <vcl/BitmapTools.hxx>

#include <pdf/pdfwriter_impl.hxx>
#include <text/TextLayoutEngine.hxx>

#include <list>
#include <memory>
#include <algorithm>

#define MAX_TILE_WIDTH 1024
#define MAX_TILE_HEIGHT 1024

using namespace vcl::metafile;

namespace
{
typedef ::std::pair<MetaAction*, int> Component; // MetaAction plus index in metafile

// List of (intersecting) actions, plus overall bounds
struct ConnectedComponents
{
    ConnectedComponents()
        : aComponentList()
        , aBounds()
        , aBgColor(COL_WHITE)
        , bIsSpecial(false)
        , bIsFullyTransparent(false)
    {
    }

    ::std::list<Component> aComponentList;
    tools::Rectangle aBounds;
    Color aBgColor;
    bool bIsSpecial;
    bool bIsFullyTransparent;
};

} // end anon namespace

/** Determines whether the action can handle transparency correctly
  (i.e. when painted on white background, does the action still look
  correct)?
 */
static bool lcl_DoesActionHandleTransparency(const MetaAction& rAct)
{
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

static bool lcl_doesRectCoverWithUniformColor(tools::Rectangle const& rPrevRect,
                                              tools::Rectangle const& rCurrRect,
                                              OutputDevice const& rMapModeVDev)
{
    return (rMapModeVDev.LogicToPixel(rCurrRect).Contains(rPrevRect) && rMapModeVDev.IsFillColor());
}

/** Check whether rCurrRect rectangle fully covers io_rPrevRect - if
    yes, return true and update o_rBgColor
 */
static bool lcl_checkRect(tools::Rectangle& io_rPrevRect, Color& o_rBgColor,
                          const tools::Rectangle& rCurrRect, OutputDevice const& rMapModeVDev)
{
    bool bRet = lcl_doesRectCoverWithUniformColor(io_rPrevRect, rCurrRect, rMapModeVDev);

    if (bRet)
    {
        io_rPrevRect = rCurrRect;
        o_rBgColor = rMapModeVDev.GetFillColor();
    }

    return bRet;
}

/** Convert to Bitmap with appropriately blended color. Convert
    MetaTransparentAction to plain polygon, appropriately colored
*/
static void lcl_ConvertTransparentAction(GDIMetaFile& o_rMtf, const MetaAction& rAct,
                                         const OutputDevice& rStateOutDev, Color aBgColor)
{
    if (rAct.GetType() == MetaActionType::Transparent)
    {
        const MetaTransparentAction* pTransAct = static_cast<const MetaTransparentAction*>(&rAct);
        sal_uInt16 nTransparency(pTransAct->GetTransparence());

        if (nTransparency)
        {
            o_rMtf.AddAction(
                new MetaPushAction(vcl::PushFlags::LINECOLOR | vcl::PushFlags::FILLCOLOR));

            Color aLineColor(rStateOutDev.GetLineColor());
            aLineColor.SetRed(static_cast<sal_uInt8>(
                (255 * nTransparency + (100 - nTransparency) * aLineColor.GetRed()) / 100));
            aLineColor.SetGreen(static_cast<sal_uInt8>(
                (255 * nTransparency + (100 - nTransparency) * aLineColor.GetGreen()) / 100));
            aLineColor.SetBlue(static_cast<sal_uInt8>(
                (255 * nTransparency + (100 - nTransparency) * aLineColor.GetBlue()) / 100));
            o_rMtf.AddAction(new MetaLineColorAction(aLineColor, true));

            Color aFillColor(rStateOutDev.GetFillColor());
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
    else
    {
        Bitmap aBmp;

        switch (rAct.GetType())
        {
            case MetaActionType::BMPEX:
                aBmp = static_cast<const MetaBmpExAction&>(rAct).GetBitmap();
                break;
            case MetaActionType::BMPEXSCALE:
            case MetaActionType::BMPEXSCALEPART:
                aBmp = static_cast<const MetaBmpExScaleAction&>(rAct).GetBitmap();
                break;
            default:
                OSL_FAIL("TransparencyFlattener impossible state reached");
                break;
        }

        if (aBmp.HasAlpha())
        {
            AlphaMask aMask = aBmp.CreateAlphaMask();
            aBmp.Convert(BmpConversion::N24Bit);
            aBmp.Blend(aMask, aBgColor);
        }

        switch (rAct.GetType())
        {
            case MetaActionType::BMPEX:
                o_rMtf.AddAction(
                    new MetaBmpAction(static_cast<const MetaBmpExAction&>(rAct).GetPoint(), aBmp));
                break;
            case MetaActionType::BMPEXSCALE:
                o_rMtf.AddAction(new MetaBmpScaleAction(
                    static_cast<const MetaBmpExScaleAction&>(rAct).GetPoint(),
                    static_cast<const MetaBmpExScaleAction&>(rAct).GetSize(), aBmp));
                break;
            case MetaActionType::BMPEXSCALEPART:
                o_rMtf.AddAction(new MetaBmpScalePartAction(
                    static_cast<const MetaBmpExScalePartAction&>(rAct).GetDestPoint(),
                    static_cast<const MetaBmpExScalePartAction&>(rAct).GetDestSize(),
                    static_cast<const MetaBmpExScalePartAction&>(rAct).GetSrcPoint(),
                    static_cast<const MetaBmpExScalePartAction&>(rAct).GetSrcSize(), aBmp));
                break;
            default:
                break;
        }
    }
}

// Returns true, if given action creates visible (i.e. non-transparent) output
static bool lcl_IsNotTransparent(const MetaAction& rAct, const OutputDevice& rOut)
{
    const bool bLineTransparency(!rOut.IsLineColor() || rOut.GetLineColor().IsFullyTransparent());
    const bool bFillTransparency(!rOut.IsFillColor() || rOut.GetFillColor().IsFullyTransparent());
    bool bRet(false);

    switch (rAct.GetType())
    {
        case MetaActionType::POINT:
        case MetaActionType::LINE:
        case MetaActionType::POLYLINE:
            if (!bLineTransparency)
                bRet = true;
            break;
        case MetaActionType::RECT:
        case MetaActionType::ROUNDRECT:
        case MetaActionType::ELLIPSE:
        case MetaActionType::ARC:
        case MetaActionType::PIE:
        case MetaActionType::CHORD:
        case MetaActionType::POLYGON:
        case MetaActionType::POLYPOLYGON:
            if (!bLineTransparency || !bFillTransparency)
                bRet = true;
            break;
        case MetaActionType::TEXT:
        case MetaActionType::TEXTARRAY:
        {
            const MetaTextAction& rTextAct = static_cast<const MetaTextAction&>(rAct);
            const OUString aString(rTextAct.GetText().copy(rTextAct.GetIndex(), rTextAct.GetLen()));
            if (!aString.isEmpty())
                bRet = true;
            break;
        }
        case MetaActionType::PIXEL:
        case MetaActionType::BMP:
        case MetaActionType::BMPSCALE:
        case MetaActionType::BMPSCALEPART:
        case MetaActionType::BMPEX:
        case MetaActionType::BMPEXSCALE:
        case MetaActionType::BMPEXSCALEPART:
        case MetaActionType::MASK:
        case MetaActionType::MASKSCALE:
        case MetaActionType::MASKSCALEPART:
        case MetaActionType::GRADIENT:
        case MetaActionType::GRADIENTEX:
        case MetaActionType::HATCH:
        case MetaActionType::WALLPAPER:
        case MetaActionType::Transparent:
        case MetaActionType::FLOATTRANSPARENT:
        case MetaActionType::EPS:
        case MetaActionType::TEXTRECT:
        case MetaActionType::STRETCHTEXT:
            bRet = true;
            break;
        default:
            break;
    }
    return bRet;
}

static tools::Rectangle lcl_CalcActionBounds(const MetaAction& rAct, const OutputDevice& rOut)
{
    tools::Rectangle aActionBounds;

    switch (rAct.GetType())
    {
        case MetaActionType::PIXEL:
            aActionBounds = tools::Rectangle(static_cast<const MetaPixelAction&>(rAct).GetPoint(),
                                             Size(1, 1));
            break;
        case MetaActionType::POINT:
            aActionBounds = tools::Rectangle(static_cast<const MetaPointAction&>(rAct).GetPoint(),
                                             Size(1, 1));
            break;
        case MetaActionType::LINE:
        {
            const MetaLineAction& rMetaLineAction = static_cast<const MetaLineAction&>(rAct);
            aActionBounds
                = tools::Rectangle(rMetaLineAction.GetStartPoint(), rMetaLineAction.GetEndPoint());
            aActionBounds.Normalize();
            const tools::Long nLineWidth(rMetaLineAction.GetLineInfo().GetWidth());
            if (nLineWidth)
            {
                const tools::Long nHalfLineWidth((nLineWidth + 1) / 2);
                aActionBounds.AdjustLeft(-nHalfLineWidth);
                aActionBounds.AdjustTop(-nHalfLineWidth);
                aActionBounds.AdjustRight(nHalfLineWidth);
                aActionBounds.AdjustBottom(nHalfLineWidth);
            }
            break;
        }
        case MetaActionType::RECT:
            aActionBounds = static_cast<const MetaRectAction&>(rAct).GetRect();
            break;
        case MetaActionType::ROUNDRECT:
            aActionBounds
                = tools::Polygon(static_cast<const MetaRoundRectAction&>(rAct).GetRect(),
                                 static_cast<const MetaRoundRectAction&>(rAct).GetHorzRound(),
                                 static_cast<const MetaRoundRectAction&>(rAct).GetVertRound())
                      .GetBoundRect();
            break;
        case MetaActionType::ELLIPSE:
        {
            const tools::Rectangle& rRect = static_cast<const MetaEllipseAction&>(rAct).GetRect();
            aActionBounds
                = tools::Polygon(rRect.Center(), rRect.GetWidth() >> 1, rRect.GetHeight() >> 1)
                      .GetBoundRect();
            break;
        }
        case MetaActionType::ARC:
            aActionBounds = tools::Polygon(static_cast<const MetaArcAction&>(rAct).GetRect(),
                                           static_cast<const MetaArcAction&>(rAct).GetStartPoint(),
                                           static_cast<const MetaArcAction&>(rAct).GetEndPoint(),
                                           PolyStyle::Arc)
                                .GetBoundRect();
            break;
        case MetaActionType::PIE:
            aActionBounds = tools::Polygon(static_cast<const MetaPieAction&>(rAct).GetRect(),
                                           static_cast<const MetaPieAction&>(rAct).GetStartPoint(),
                                           static_cast<const MetaPieAction&>(rAct).GetEndPoint(),
                                           PolyStyle::Pie)
                                .GetBoundRect();
            break;
        case MetaActionType::CHORD:
            aActionBounds
                = tools::Polygon(static_cast<const MetaChordAction&>(rAct).GetRect(),
                                 static_cast<const MetaChordAction&>(rAct).GetStartPoint(),
                                 static_cast<const MetaChordAction&>(rAct).GetEndPoint(),
                                 PolyStyle::Chord)
                      .GetBoundRect();
            break;
        case MetaActionType::POLYLINE:
        {
            const MetaPolyLineAction& rMetaPolyLineAction
                = static_cast<const MetaPolyLineAction&>(rAct);
            aActionBounds = rMetaPolyLineAction.GetPolygon().GetBoundRect();
            const tools::Long nLineWidth(rMetaPolyLineAction.GetLineInfo().GetWidth());
            if (nLineWidth)
            {
                const tools::Long nHalfLineWidth((nLineWidth + 1) / 2);
                aActionBounds.AdjustLeft(-nHalfLineWidth);
                aActionBounds.AdjustTop(-nHalfLineWidth);
                aActionBounds.AdjustRight(nHalfLineWidth);
                aActionBounds.AdjustBottom(nHalfLineWidth);
            }
            break;
        }
        case MetaActionType::POLYGON:
            aActionBounds = static_cast<const MetaPolygonAction&>(rAct).GetPolygon().GetBoundRect();
            break;
        case MetaActionType::POLYPOLYGON:
            aActionBounds
                = static_cast<const MetaPolyPolygonAction&>(rAct).GetPolyPolygon().GetBoundRect();
            break;
        case MetaActionType::BMP:
            aActionBounds = tools::Rectangle(
                static_cast<const MetaBmpAction&>(rAct).GetPoint(),
                rOut.PixelToLogic(
                    static_cast<const MetaBmpAction&>(rAct).GetBitmap().GetSizePixel()));
            break;
        case MetaActionType::BMPSCALE:
            aActionBounds
                = tools::Rectangle(static_cast<const MetaBmpScaleAction&>(rAct).GetPoint(),
                                   static_cast<const MetaBmpScaleAction&>(rAct).GetSize());
            break;
        case MetaActionType::BMPSCALEPART:
            aActionBounds
                = tools::Rectangle(static_cast<const MetaBmpScalePartAction&>(rAct).GetDestPoint(),
                                   static_cast<const MetaBmpScalePartAction&>(rAct).GetDestSize());
            break;
        case MetaActionType::BMPEX:
            aActionBounds = tools::Rectangle(
                static_cast<const MetaBmpExAction&>(rAct).GetPoint(),
                rOut.PixelToLogic(
                    static_cast<const MetaBmpExAction&>(rAct).GetBitmap().GetSizePixel()));
            break;
        case MetaActionType::BMPEXSCALE:
            aActionBounds
                = tools::Rectangle(static_cast<const MetaBmpExScaleAction&>(rAct).GetPoint(),
                                   static_cast<const MetaBmpExScaleAction&>(rAct).GetSize());
            break;
        case MetaActionType::BMPEXSCALEPART:
            aActionBounds = tools::Rectangle(
                static_cast<const MetaBmpExScalePartAction&>(rAct).GetDestPoint(),
                static_cast<const MetaBmpExScalePartAction&>(rAct).GetDestSize());
            break;
        case MetaActionType::MASK:
            aActionBounds = tools::Rectangle(
                static_cast<const MetaMaskAction&>(rAct).GetPoint(),
                rOut.PixelToLogic(
                    static_cast<const MetaMaskAction&>(rAct).GetBitmap().GetSizePixel()));
            break;
        case MetaActionType::MASKSCALE:
            aActionBounds
                = tools::Rectangle(static_cast<const MetaMaskScaleAction&>(rAct).GetPoint(),
                                   static_cast<const MetaMaskScaleAction&>(rAct).GetSize());
            break;
        case MetaActionType::MASKSCALEPART:
            aActionBounds
                = tools::Rectangle(static_cast<const MetaMaskScalePartAction&>(rAct).GetDestPoint(),
                                   static_cast<const MetaMaskScalePartAction&>(rAct).GetDestSize());
            break;
        case MetaActionType::GRADIENT:
            aActionBounds = static_cast<const MetaGradientAction&>(rAct).GetRect();
            break;
        case MetaActionType::GRADIENTEX:
            aActionBounds
                = static_cast<const MetaGradientExAction&>(rAct).GetPolyPolygon().GetBoundRect();
            break;
        case MetaActionType::HATCH:
            aActionBounds
                = static_cast<const MetaHatchAction&>(rAct).GetPolyPolygon().GetBoundRect();
            break;
        case MetaActionType::WALLPAPER:
            aActionBounds = static_cast<const MetaWallpaperAction&>(rAct).GetRect();
            break;
        case MetaActionType::Transparent:
            aActionBounds
                = static_cast<const MetaTransparentAction&>(rAct).GetPolyPolygon().GetBoundRect();
            break;
        case MetaActionType::FLOATTRANSPARENT:
            aActionBounds
                = tools::Rectangle(static_cast<const MetaFloatTransparentAction&>(rAct).GetPoint(),
                                   static_cast<const MetaFloatTransparentAction&>(rAct).GetSize());
            break;
        case MetaActionType::EPS:
            aActionBounds = tools::Rectangle(static_cast<const MetaEPSAction&>(rAct).GetPoint(),
                                             static_cast<const MetaEPSAction&>(rAct).GetSize());
            break;
        case MetaActionType::TEXT:
        {
            const MetaTextAction& rTextAct = static_cast<const MetaTextAction&>(rAct);
            const OUString aString(rTextAct.GetText().copy(rTextAct.GetIndex(), rTextAct.GetLen()));
            if (!aString.isEmpty())
            {
                const Point aPtLog(rTextAct.GetPoint());
                rOut.GetLogicalTextBoundRect(aActionBounds, rTextAct.GetText(), rTextAct.GetIndex(),
                                             rTextAct.GetIndex(), rTextAct.GetLen());
                aActionBounds.Move(aPtLog.X(), aPtLog.Y());
            }
            break;
        }
        case MetaActionType::TEXTARRAY:
        {
            const MetaTextArrayAction& rTextAct = static_cast<const MetaTextArrayAction&>(rAct);
            const OUString aString(rTextAct.GetText().copy(rTextAct.GetIndex(), rTextAct.GetLen()));
            if (!aString.isEmpty())
            {
                std::unique_ptr<SalLayout> pSalLayout;
                if (rTextAct.GetLayoutContextIndex() >= 0)
                {
                    pSalLayout = rOut.LayoutText(
                        vcl::text::TextSpan{ rTextAct.GetText(), rTextAct.GetLayoutContextIndex(),
                                             rTextAct.GetLayoutContextLen() },
                        vcl::text::LayoutConstraints{ rTextAct.GetPoint(), 0, rTextAct.GetDXArray(),
                                                      rTextAct.GetKashidaArray(),
                                                      SalLayoutFlags::NONE },
                        vcl::text::LayoutCacheData{ nullptr, nullptr },
                        vcl::text::RenderSelection{ rTextAct.GetIndex(), rTextAct.GetIndex(),
                                                    rTextAct.GetIndex() + rTextAct.GetLen() });
                }
                else
                {
                    pSalLayout = rOut.LayoutText(
                        vcl::text::TextSpan{ rTextAct.GetText(), rTextAct.GetIndex(),
                                             rTextAct.GetLen() },
                        vcl::text::LayoutConstraints{ rTextAct.GetPoint(), 0, rTextAct.GetDXArray(),
                                                      rTextAct.GetKashidaArray(),
                                                      SalLayoutFlags::NONE },
                        vcl::text::LayoutCacheData{}, vcl::text::RenderSelection{});
                }

                if (pSalLayout)
                {
                    tools::Rectangle aBoundRect(vcl::text::TextGeometry::GetTextInkBounds(
                        *pSalLayout, *rOut.GetFontRealization()));
                    aActionBounds = rOut.PixelToLogic(aBoundRect);
                }
            }
            break;
        }
        case MetaActionType::TEXTRECT:
            aActionBounds = static_cast<const MetaTextRectAction&>(rAct).GetRect();
            break;
        case MetaActionType::STRETCHTEXT:
        {
            const MetaStretchTextAction& rTextAct = static_cast<const MetaStretchTextAction&>(rAct);
            const OUString aString(rTextAct.GetText().copy(rTextAct.GetIndex(), rTextAct.GetLen()));
            if (!aString.isEmpty())
            {
                std::unique_ptr<SalLayout> pSalLayout = rOut.LayoutText(
                    vcl::text::TextSpan{ rTextAct.GetText(), rTextAct.GetIndex(),
                                         rTextAct.GetLen() },
                    vcl::text::LayoutConstraints{ rTextAct.GetPoint(),
                                                  static_cast<tools::Long>(rTextAct.GetWidth()) },
                    vcl::text::LayoutCacheData{}, vcl::text::RenderSelection{});
                if (pSalLayout)
                {
                    tools::Rectangle aBoundRect(vcl::text::TextGeometry::GetTextInkBounds(
                        *pSalLayout, *rOut.GetFontRealization()));
                    aActionBounds = rOut.PixelToLogic(aBoundRect);
                }
            }
            break;
        }
        case MetaActionType::TEXTLINE:
            OSL_FAIL("MetaActionType::TEXTLINE not supported");
            break;
        default:
            break;
    }

    if (!aActionBounds.IsEmpty())
    {
        if (rOut.HasClipRegion())
            return rOut.LogicToPixel(
                rOut.GetClipRegion().GetBoundRect().Intersection(aActionBounds));
        else
            return rOut.LogicToPixel(aActionBounds);
    }
    else
        return tools::Rectangle();
}

bool TransparencyFlattener::Flatten(const GDIMetaFile& rInput, GDIMetaFile& rOutput,
                                    const OutputDevice& rRefDevice,
                                    const FlatteningOptions& rOptions)
{
    MetaAction* pCurrAct;
    bool bTransparent(false);

    rOutput.Clear();

#ifdef MACOSX
    if (rOptions.bReduceTransparency && !rOptions.bTransparencyAutoMode)
#else
    if (!rOptions.bReduceTransparency || rOptions.bTransparencyAutoMode)
#endif
        bTransparent = rInput.HasTransparentActions();

    if (!bTransparent)
    {
        rOutput = rInput;
    }
    else
    {
        ConnectedComponents aBackgroundComponent;

        double fReduceTransparencyMinArea
            = officecfg::Office::Common::VCL::ReduceTransparencyMinArea::get() / 100.0;
        SAL_WARN_IF(fReduceTransparencyMinArea > 1.0, "vcl",
                    "Value of ReduceTransparencyMinArea config option is too high");
        SAL_WARN_IF(fReduceTransparencyMinArea < 0.0, "vcl",
                    "Value of ReduceTransparencyMinArea config option is too low");
        fReduceTransparencyMinArea = std::clamp(fReduceTransparencyMinArea, 0.0, 1.0);

        ScopedVclPtrInstance<VirtualDevice> aMapModeVDev(rRefDevice, DeviceFormat::WITHOUT_ALPHA);
        aMapModeVDev->SetDPIX(rRefDevice.GetDPIX());
        aMapModeVDev->SetDPIY(rRefDevice.GetDPIY());
        aMapModeVDev->EnableOutput(false);

        bool bStillBackground = true;
        int nActionNum = 0, nLastBgAction = -1;
        pCurrAct = const_cast<GDIMetaFile&>(rInput).FirstAction();

        if (rOptions.aBackground != COL_TRANSPARENT)
        {
            aBackgroundComponent.aBgColor = rOptions.aBackground;
            aBackgroundComponent.aBounds = rRefDevice.GetBackgroundComponentBounds();
        }

        while (pCurrAct && bStillBackground)
        {
            switch (pCurrAct->GetType())
            {
                case MetaActionType::RECT:
                {
                    if (!lcl_checkRect(aBackgroundComponent.aBounds, aBackgroundComponent.aBgColor,
                                       static_cast<const MetaRectAction*>(pCurrAct)->GetRect(),
                                       *aMapModeVDev))
                        bStillBackground = false;
                    else
                        nLastBgAction = nActionNum;
                    break;
                }
                case MetaActionType::POLYGON:
                {
                    const tools::Polygon aPoly(
                        static_cast<const MetaPolygonAction*>(pCurrAct)->GetPolygon());
                    if (!basegfx::utils::isRectangle(aPoly.getB2DPolygon())
                        || !lcl_checkRect(aBackgroundComponent.aBounds,
                                          aBackgroundComponent.aBgColor, aPoly.GetBoundRect(),
                                          *aMapModeVDev))
                        bStillBackground = false;
                    else
                        nLastBgAction = nActionNum;
                    break;
                }
                case MetaActionType::POLYPOLYGON:
                {
                    const tools::PolyPolygon aPoly(
                        static_cast<const MetaPolyPolygonAction*>(pCurrAct)->GetPolyPolygon());
                    if (aPoly.Count() != 1 || !basegfx::utils::isRectangle(aPoly[0].getB2DPolygon())
                        || !lcl_checkRect(aBackgroundComponent.aBounds,
                                          aBackgroundComponent.aBgColor, aPoly.GetBoundRect(),
                                          *aMapModeVDev))
                        bStillBackground = false;
                    else
                        nLastBgAction = nActionNum;
                    break;
                }
                case MetaActionType::WALLPAPER:
                {
                    if (!lcl_checkRect(aBackgroundComponent.aBounds, aBackgroundComponent.aBgColor,
                                       static_cast<const MetaWallpaperAction*>(pCurrAct)->GetRect(),
                                       *aMapModeVDev))
                        bStillBackground = false;
                    else
                        nLastBgAction = nActionNum;
                    break;
                }
                default:
                {
                    if (lcl_IsNotTransparent(*pCurrAct, *aMapModeVDev))
                        bStillBackground = false;
                    else
                        aBackgroundComponent.aBounds.Union(
                            lcl_CalcActionBounds(*pCurrAct, *aMapModeVDev));
                    break;
                }
            }

            pCurrAct->Execute(aMapModeVDev.get());
            pCurrAct = const_cast<GDIMetaFile&>(rInput).NextAction();
            ++nActionNum;
        }

        if (nLastBgAction != -1)
        {
            size_t nActionSize = rInput.GetActionSize();
            for (size_t nPostLastBgAction = nLastBgAction + 1; nPostLastBgAction < nActionSize;
                 ++nPostLastBgAction)
            {
                if (rInput.GetAction(nPostLastBgAction)->GetType() != MetaActionType::POP)
                    break;
                nLastBgAction = nPostLastBgAction;
            }
        }

        aMapModeVDev->ClearStack();

        nActionNum = 0;
        pCurrAct = const_cast<GDIMetaFile&>(rInput).FirstAction();
        while (pCurrAct && nActionNum <= nLastBgAction)
        {
            aBackgroundComponent.aComponentList.emplace_back(pCurrAct, nActionNum);
            pCurrAct->Execute(aMapModeVDev.get());
            pCurrAct = const_cast<GDIMetaFile&>(rInput).NextAction();
            ++nActionNum;
        }

        ::std::vector<ConnectedComponents> aCCList;

        for (; pCurrAct; pCurrAct = const_cast<GDIMetaFile&>(rInput).NextAction(), ++nActionNum)
        {
            pCurrAct->Execute(aMapModeVDev.get());
            const tools::Rectangle aBBCurrAct(lcl_CalcActionBounds(*pCurrAct, *aMapModeVDev));
            tools::Rectangle aTotalBounds(aBBCurrAct);
            bool bTreatSpecial(false);
            ConnectedComponents aTotalComponents;

            aTotalComponents.bIsFullyTransparent = !lcl_IsNotTransparent(*pCurrAct, *aMapModeVDev);

            if (!aBBCurrAct.IsEmpty() && !aTotalComponents.bIsFullyTransparent)
            {
                if (!aBackgroundComponent.aComponentList.empty()
                    && !aBackgroundComponent.aBounds.Contains(aTotalBounds))
                {
                    aTotalBounds.Union(aBackgroundComponent.aBounds);
                    aTotalComponents.aComponentList.splice(aTotalComponents.aComponentList.end(),
                                                           aBackgroundComponent.aComponentList);
                    if (aBackgroundComponent.bIsSpecial)
                        bTreatSpecial = true;
                }

                bool bSomeComponentsChanged;
                do
                {
                    bSomeComponentsChanged = false;
                    for (auto aCurrCC = aCCList.begin(); aCurrCC != aCCList.end();)
                    {
                        if (!aCurrCC->aBounds.IsEmpty() && !aCurrCC->bIsFullyTransparent
                            && aCurrCC->aBounds.Overlaps(aTotalBounds))
                        {
                            aTotalBounds.Union(aCurrCC->aBounds);
                            aTotalComponents.aComponentList.splice(
                                aTotalComponents.aComponentList.end(), aCurrCC->aComponentList);
                            if (aCurrCC->bIsSpecial)
                                bTreatSpecial = true;
                            aCurrCC = aCCList.erase(aCurrCC);
                            bSomeComponentsChanged = true;
                        }
                        else
                        {
                            ++aCurrCC;
                        }
                    }
                } while (bSomeComponentsChanged);
            }

            if (bTreatSpecial)
            {
                aTotalComponents.bIsSpecial = true;
            }
            else if (!pCurrAct->IsTransparent())
            {
                aTotalComponents.bIsSpecial = false;
            }
            else
            {
                if (!lcl_DoesActionHandleTransparency(*pCurrAct))
                {
                    aTotalComponents.bIsSpecial = true;
                }
                else
                {
                    if (aTotalComponents.aComponentList.empty())
                    {
                        aTotalComponents.bIsSpecial = false;
                    }
                    else
                    {
                        aTotalComponents.bIsSpecial = true;
                    }
                }
            }

            aTotalComponents.aBounds = aTotalBounds;
            aTotalComponents.aComponentList.emplace_back(pCurrAct, nActionNum);
            aCCList.push_back(std::move(aTotalComponents));
        }

        ::std::vector<const ConnectedComponents*> aCCList_MemberMap(rInput.GetActionSize());
        for (auto const& currentItem : aCCList)
        {
            for (auto const& currentAction : currentItem.aComponentList)
            {
                aCCList_MemberMap[currentAction.second] = &currentItem;
            }
        }

        for (auto& component : aBackgroundComponent.aComponentList)
        {
            rOutput.AddAction(component.first);
        }

        Point aPageOffset;
        Size aTmpSize(rRefDevice.GetOutputSizePixel());

        if (rRefDevice.GetOutDevType() == OUTDEV_PDF)
        {
            auto pPdfWriter = const_cast<vcl::PDFWriterImpl*>(
                static_cast<const vcl::PDFWriterImpl*>(&rRefDevice));
            aTmpSize
                = rRefDevice.LogicToPixel(pPdfWriter->getCurPageSize(), MapMode(MapUnit::MapPoint));
            pPdfWriter->insertError(vcl::PDFWriter::Warning_Transparency_Converted);
        }
        else if (rRefDevice.GetOutDevType() == OUTDEV_PRINTER)
        {
            const Printer* pPrinter = dynamic_cast<const Printer*>(&rRefDevice);
            if (pPrinter)
            {
                aPageOffset = pPrinter->GetPageOffsetPixel();
                aPageOffset = Point(0, 0) - aPageOffset;
                aTmpSize = pPrinter->GetPaperSizePixel();
            }
        }

        const tools::Rectangle aOutputRect(aPageOffset, aTmpSize);
        bool bTiling = (rRefDevice.GetOutDevType() == OUTDEV_PRINTER);

        for (auto& currentItem : aCCList)
        {
            if (currentItem.bIsSpecial)
            {
                tools::Rectangle aBoundRect(currentItem.aBounds);
                aBoundRect.Intersection(aOutputRect);

                const double fBmpArea(static_cast<double>(aBoundRect.GetWidth())
                                      * aBoundRect.GetHeight());
                const double fOutArea(static_cast<double>(aOutputRect.GetWidth())
                                      * aOutputRect.GetHeight());

                if (rOptions.bReduceTransparency && rOptions.bTransparencyAutoMode
                    && (fBmpArea > (fReduceTransparencyMinArea * fOutArea)))
                {
                    currentItem.bIsSpecial = false;
                }
                else
                {
                    if (aBoundRect.GetWidth() && aBoundRect.GetHeight())
                    {
                        Point aDstPtPix(aBoundRect.TopLeft());
                        Size aDstSzPix;

                        ScopedVclPtrInstance<VirtualDevice> aMapVDev(rRefDevice,
                                                                     DeviceFormat::WITHOUT_ALPHA);
                        aMapVDev->EnableOutput(false);

                        ScopedVclPtrInstance<VirtualDevice> aPaintVDev(rRefDevice,
                                                                       DeviceFormat::WITHOUT_ALPHA);
                        aPaintVDev->SetBackground(aBackgroundComponent.aBgColor);

                        rOutput.AddAction(new MetaPushAction(vcl::PushFlags::MAPMODE));
                        rOutput.AddAction(new MetaMapModeAction());

                        aPaintVDev->SetDrawMode(rRefDevice.GetDrawMode());

                        while (aDstPtPix.Y() <= aBoundRect.Bottom())
                        {
                            aDstPtPix.setX(aBoundRect.Left());
                            aDstSzPix = bTiling ? Size(MAX_TILE_WIDTH, MAX_TILE_HEIGHT)
                                                : aBoundRect.GetSize();

                            if ((aDstPtPix.Y() + aDstSzPix.Height() - 1) > aBoundRect.Bottom())
                                aDstSzPix.setHeight(aBoundRect.Bottom() - aDstPtPix.Y() + 1);

                            while (aDstPtPix.X() <= aBoundRect.Right())
                            {
                                if ((aDstPtPix.X() + aDstSzPix.Width() - 1) > aBoundRect.Right())
                                    aDstSzPix.setWidth(aBoundRect.Right() - aDstPtPix.X() + 1);

                                if (!tools::Rectangle(aDstPtPix, aDstSzPix)
                                         .Intersection(aBoundRect)
                                         .IsEmpty()
                                    && aPaintVDev->SetOutputSizePixel(aDstSzPix))
                                {
                                    auto popIt1 = aPaintVDev->ScopedPush();
                                    auto popIt2 = aMapVDev->ScopedPush();

                                    aMapVDev->SetDPIX(rRefDevice.GetDPIX());
                                    aPaintVDev->SetDPIX(rRefDevice.GetDPIX());
                                    aMapVDev->SetDPIY(rRefDevice.GetDPIY());
                                    aPaintVDev->SetDPIY(rRefDevice.GetDPIY());

                                    aPaintVDev->EnableOutput(false);

                                    for (pCurrAct = const_cast<GDIMetaFile&>(rInput).FirstAction(),
                                        nActionNum = 0;
                                         pCurrAct;
                                         pCurrAct = const_cast<GDIMetaFile&>(rInput).NextAction(),
                                        ++nActionNum)
                                    {
                                        if (aCCList_MemberMap[nActionNum] == &currentItem)
                                            aPaintVDev->EnableOutput();

                                        const MetaActionType nType(pCurrAct->GetType());

                                        if (MetaActionType::MAPMODE == nType)
                                        {
                                            pCurrAct->Execute(aMapVDev.get());

                                            MapMode aMtfMap(aMapVDev->GetMapMode());
                                            const Point aNewOrg(aMapVDev->PixelToLogic(aDstPtPix));

                                            aMtfMap.SetOrigin(Point(-aNewOrg.X(), -aNewOrg.Y()));
                                            aPaintVDev->SetMapMode(aMtfMap);
                                        }
                                        else if ((MetaActionType::PUSH == nType)
                                                 || MetaActionType::POP == nType)
                                        {
                                            pCurrAct->Execute(aMapVDev.get());
                                            pCurrAct->Execute(aPaintVDev.get());
                                        }
                                        else if (MetaActionType::GRADIENT == nType)
                                        {
                                            MetaGradientAction* pGradientAction
                                                = static_cast<MetaGradientAction*>(pCurrAct);
                                            aPaintVDev->DrawGradient(
                                                pGradientAction->GetRect(),
                                                pGradientAction->GetGradient());
                                        }
                                        else
                                        {
                                            pCurrAct->Execute(aPaintVDev.get());
                                        }

                                        Application::Reschedule(true);
                                    }

                                    const bool bOldMap = rRefDevice.IsMapModeEnabled();
                                    const_cast<OutputDevice&>(rRefDevice).EnableMapMode(false);
                                    aPaintVDev->EnableMapMode(false);

                                    Bitmap aBandBmp(aPaintVDev->GetBitmap(Point(), aDstSzPix));
                                    std::cout << "\n[FLATTENER DEBUG] Band bitmap created from "
                                                 "VirtualDevice. HasAlpha="
                                              << aBandBmp.HasAlpha() << std::endl;

                                    if (aBandBmp.HasAlpha())
                                    {
                                        std::cout << "[FLATTENER DEBUG] Alpha channel detected. "
                                                     "Attempting to blend and strip..."
                                                  << std::endl;
                                        AlphaMask aMask = aBandBmp.CreateAlphaMask();
                                        aBandBmp.Convert(BmpConversion::N24Bit);
                                        aBandBmp.Blend(aMask, aBackgroundComponent.aBgColor);
                                        std::cout
                                            << "[FLATTENER DEBUG] After Convert & Blend, HasAlpha="
                                            << aBandBmp.HasAlpha() << std::endl;
                                    }

                                    if (rOptions.bDownsampleBitmaps)
                                    {
                                        aBandBmp = vcl::bitmap::GetDownsampledBitmap(
                                            rRefDevice.PixelToLogic(
                                                rRefDevice.LogicToPixel(aDstSzPix),
                                                MapMode(MapUnit::MapTwip)),
                                            Point(), aBandBmp.GetSizePixel(), aBandBmp,
                                            rOptions.nMaxBmpDPIX, rOptions.nMaxBmpDPIY);
                                        std::cout << "[FLATTENER DEBUG] After downsample, HasAlpha="
                                                  << aBandBmp.HasAlpha() << std::endl;
                                    }

                                    rOutput.AddAction(new MetaCommentAction(
                                        "PRNSPOOL_TRANSPARENTBITMAP_BEGIN"_ostr));

                                    if (aBandBmp.HasAlpha())
                                    {
                                        std::cout
                                            << "[FLATTENER DEBUG] Alpha is STILL present! Emitting "
                                               "MetaBmpExScaleAction to avoid assertion crash."
                                            << std::endl;
                                        rOutput.AddAction(new MetaBmpExScaleAction(
                                            aDstPtPix, aDstSzPix, aBandBmp));
                                    }
                                    else
                                    {
                                        std::cout << "[FLATTENER DEBUG] Alpha successfully "
                                                     "stripped. Emitting MetaBmpScaleAction."
                                                  << std::endl;
                                        rOutput.AddAction(
                                            new MetaBmpScaleAction(aDstPtPix, aDstSzPix, aBandBmp));
                                    }

                                    rOutput.AddAction(new MetaCommentAction(
                                        "PRNSPOOL_TRANSPARENTBITMAP_END"_ostr));

                                    aPaintVDev->EnableMapMode();
                                    const_cast<OutputDevice&>(rRefDevice).EnableMapMode(bOldMap);
                                }

                                aDstPtPix.AdjustX(aDstSzPix.Width());
                            }

                            aDstPtPix.AdjustY(aDstSzPix.Height());
                        }

                        rOutput.AddAction(new MetaPopAction());
                    }
                }
            }
        }

        aMapModeVDev->ClearStack();

        for (pCurrAct = const_cast<GDIMetaFile&>(rInput).FirstAction(), nActionNum = 0; pCurrAct;
             pCurrAct = const_cast<GDIMetaFile&>(rInput).NextAction(), ++nActionNum)
        {
            const ConnectedComponents* pCurrAssociatedComponent = aCCList_MemberMap[nActionNum];

            if (pCurrAssociatedComponent
                && (pCurrAssociatedComponent->aBounds.IsEmpty()
                    || !pCurrAssociatedComponent->bIsSpecial))
            {
                if (lcl_DoesActionHandleTransparency(*pCurrAct)
                    && pCurrAssociatedComponent->aComponentList.begin()->first == pCurrAct)
                {
                    lcl_ConvertTransparentAction(rOutput, *pCurrAct, *aMapModeVDev,
                                                 aBackgroundComponent.aBgColor);
                }
                else
                {
                    rOutput.AddAction(pCurrAct);
                }

                pCurrAct->Execute(aMapModeVDev.get());
            }
        }

        rOutput.SetPrefMapMode(rInput.GetPrefMapMode());
        rOutput.SetPrefSize(rInput.GetPrefSize());

#if OSL_DEBUG_LEVEL > 1
        rOutput.AddAction(new MetaFillColorAction(COL_WHITE, false));
        for (auto const& aCurr : aCCList)
        {
            if (aCurr.bIsSpecial)
                rOutput.AddAction(new MetaLineColorAction(COL_RED, true));
            else
                rOutput.AddAction(new MetaLineColorAction(COL_BLUE, true));

            rOutput.AddAction(new MetaRectAction(aMapModeVDev->PixelToLogic(aCurr.aBounds)));
        }
#endif
    }
    return bTransparent;
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
