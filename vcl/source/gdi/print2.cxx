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
#include "ConnectedComponents.hxx"

#define MAX_TILE_WIDTH  1024
#define MAX_TILE_HEIGHT 1024

typedef ::std::pair< MetaAction*, int > Component; // MetaAction plus index in metafile

/** Determines whether the action can handle transparency correctly
  (i.e. when painted on white background, does the action still look
  correct)?
 */
static bool DoesActionHandleTransparency( const MetaAction& rAct )
{
    // MetaActionType::FLOATTRANSPARENT can contain a whole metafile,
    // which is to be rendered with the given transparent gradient. We
    // currently cannot emulate transparent painting on a white
    // background reliably.

    // the remainder can handle printing itself correctly on a uniform
    // white background.
    switch( rAct.GetType() )
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

/** #107169# Convert BitmapEx to Bitmap with appropriately blended
    color. Convert MetaTransparentAction to plain polygon,
    appropriately colored

    @param o_rMtf
    Add converted actions to this metafile
*/

template <typename T>
void ConvertTransparentAction(GDIMetaFile&, const T*, const OutputDevice&, Color)
{
    SAL_WARN("vcl.gdi", "Not a valid MetaAction type - needs a bitmap based action");
    assert(false);
}

template <>
void ConvertTransparentAction(GDIMetaFile& o_rMtf, const MetaTransparentAction* pTransAct, const OutputDevice& rStateOutDev, Color)
{
    sal_uInt16 nTransparency(pTransAct->GetTransparence());

    // #i10613# Respect transparency for draw color
    if (nTransparency)
    {
        o_rMtf.AddAction(new MetaPushAction(PushFlags::LINECOLOR|PushFlags::FILLCOLOR));

        // assume white background for alpha blending
        Color aLineColor(rStateOutDev.GetLineColor());
        aLineColor.SetRed(static_cast<sal_uInt8>((255*nTransparency + (100 - nTransparency) * aLineColor.GetRed()) / 100));
        aLineColor.SetGreen(static_cast<sal_uInt8>((255*nTransparency + (100 - nTransparency) * aLineColor.GetGreen()) / 100));
        aLineColor.SetBlue(static_cast<sal_uInt8>((255*nTransparency + (100 - nTransparency) * aLineColor.GetBlue()) / 100));
        o_rMtf.AddAction(new MetaLineColorAction(aLineColor, true));

        Color aFillColor(rStateOutDev.GetFillColor());
        aFillColor.SetRed(static_cast<sal_uInt8>((255*nTransparency + (100 - nTransparency)*aFillColor.GetRed()) / 100));
        aFillColor.SetGreen(static_cast<sal_uInt8>((255*nTransparency + (100 - nTransparency)*aFillColor.GetGreen()) / 100));
        aFillColor.SetBlue(static_cast<sal_uInt8>((255*nTransparency + (100 - nTransparency)*aFillColor.GetBlue()) / 100));
        o_rMtf.AddAction(new MetaFillColorAction(aFillColor, true));
    }

    o_rMtf.AddAction(new MetaPolyPolygonAction(pTransAct->GetPolyPolygon()));

    if(nTransparency)
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
void ConvertTransparentAction(GDIMetaFile& o_rMtf, const MetaBmpExAction* pAct, const OutputDevice&, Color aBgColor)
{
    Bitmap aBmp(Convert(pAct->GetBitmapEx(), aBgColor));
    o_rMtf.AddAction(new MetaBmpAction(pAct->GetPoint(), aBmp));
}

template <>
void ConvertTransparentAction(GDIMetaFile& o_rMtf, const MetaBmpExScaleAction* pAct, const OutputDevice&, Color aBgColor)
{
    Bitmap aBmp(Convert(pAct->GetBitmapEx(), aBgColor));
    o_rMtf.AddAction(new MetaBmpScaleAction(pAct->GetPoint(), pAct->GetSize(), aBmp));
}

template <>
void ConvertTransparentAction(GDIMetaFile& o_rMtf, const MetaBmpExScalePartAction* pAct, const OutputDevice&, Color aBgColor)
{
    Bitmap aBmp(Convert(pAct->GetBitmapEx(), aBgColor));
    o_rMtf.AddAction(new MetaBmpScalePartAction(pAct->GetDestPoint(), pAct->GetDestSize(), pAct->GetSrcPoint(), pAct->GetSrcSize(), aBmp));
}

static double GetReduceTransparencyMinArea()
{
    double fReduceTransparencyMinArea = officecfg::Office::Common::VCL::ReduceTransparencyMinArea::get() / 100.0;
    SAL_WARN_IF(fReduceTransparencyMinArea > 1.0, "vcl",
        "Value of ReduceTransparencyMinArea config option is too high");
    SAL_WARN_IF(fReduceTransparencyMinArea < 0.0, "vcl",
        "Value of ReduceTransparencyMinArea config option is too low");
    fReduceTransparencyMinArea = std::clamp(fReduceTransparencyMinArea, 0.0, 1.0);

    return fReduceTransparencyMinArea;
}

bool OutputDevice::RemoveTransparenciesFromMetaFile( const GDIMetaFile& rInMtf, GDIMetaFile& rOutMtf,
                                                     long nMaxBmpDPIX, long nMaxBmpDPIY,
                                                     bool bReduceTransparency, bool bTransparencyAutoMode,
                                                     bool bDownsampleBitmaps,
                                                     const Color& rBackground
                                                     )
{
    MetaAction*             pCurrAct;
    bool                    bTransparent( false );

    rOutMtf.Clear();

    if(!bReduceTransparency || bTransparencyAutoMode)
        bTransparent = rInMtf.HasTransparentActions();

    // #i10613# Determine set of connected components containing transparent objects. These are
    // then processed as bitmaps, the original actions are removed from the metafile.
    if( !bTransparent )
    {
        // nothing transparent -> just copy
        rOutMtf = rInMtf;
    }
    else
    {
        // #i10613#
        // This works as follows: we want a number of distinct sets of
        // connected components, where each set contains metafile
        // actions that are intersecting (note: there are possibly
        // more actions contained as are directly intersecting,
        // because we can only produce rectangular bitmaps later
        // on. Thus, each set of connected components is the smallest
        // enclosing, axis-aligned rectangle that completely bounds a
        // number of intersecting metafile actions, plus any action
        // that would otherwise be cut in two). Therefore, we
        // iteratively add metafile actions from the original metafile
        // to this connected components list (aCCList), by checking
        // each element's bounding box against intersection with the
        // metaaction at hand.
        // All those intersecting elements are removed from aCCList
        // and collected in a temporary list (aCCMergeList). After all
        // elements have been checked, the aCCMergeList elements are
        // merged with the metaaction at hand into one resulting
        // connected component, with one big bounding box, and
        // inserted into aCCList again.
        // The time complexity of this algorithm is O(n^3), where n is
        // the number of metafile actions, and it finds all distinct
        // regions of rectangle-bounded connected components. This
        // algorithm was designed by AF.

        //  STAGE 1: Detect background

        // Receives uniform background content, and is _not_ merged
        // nor checked for intersection against other aCCList elements
        ConnectedComponents aBackgroundComponent;

        // Read the configuration value of minimal object area where transparency will be removed
        double fReduceTransparencyMinArea = GetReduceTransparencyMinArea();

        // create an OutputDevice to record mapmode changes and the like
        ScopedVclPtrInstance< VirtualDevice > aMapModeVDev;
        aMapModeVDev->mnDPIX = mnDPIX;
        aMapModeVDev->mnDPIY = mnDPIY;
        aMapModeVDev->EnableOutput(false);

        int nLastBgAction, nActionNum;

        // weed out page-filling background objects (if they are
        // uniformly coloured). Keeping them outside the other
        // connected components often prevents whole-page bitmap
        // generation.
        bool bStillBackground=true; // true until first non-bg action
        nActionNum=0; nLastBgAction=-1;
        pCurrAct=const_cast<GDIMetaFile&>(rInMtf).FirstAction();
        if( rBackground != COL_TRANSPARENT )
        {
            aBackgroundComponent.SetBackground(rBackground, GetBackgroundComponentBounds());
        }
        while( pCurrAct && bStillBackground )
        {
            bool bIsTransparentAction = true;

            switch( pCurrAct->GetType() )
            {
                case MetaActionType::RECT:
                case MetaActionType::POLYGON:
                case MetaActionType::POLYPOLYGON:
                case MetaActionType::WALLPAPER:
                {
                    if (aBackgroundComponent.IsBackgroundNotCovered(pCurrAct, *aMapModeVDev))
                        aBackgroundComponent.SetBackgroundComponent(pCurrAct, aMapModeVDev.get());
                    else
                        nLastBgAction=nActionNum; // this _is_ background

                    break;
                }
                default:
                {
                    if (pCurrAct->IsTransparent(aMapModeVDev.get()))
                        // extend current bounds (next uniform action needs to fully cover this area)
                        aBackgroundComponent.SetBackgroundComponent(pCurrAct, aMapModeVDev.get());
                    else
                        bIsTransparentAction=false; // non-transparent action, possibly not uniform
                    break;
                }
            }

            bStillBackground = bIsTransparentAction && (nLastBgAction != nActionNum); // incomplete occlusion of background

            // execute action to get correct MapModes etc.
            pCurrAct->Execute( aMapModeVDev.get() );

            pCurrAct=const_cast<GDIMetaFile&>(rInMtf).NextAction();
            ++nActionNum;

        }

        aMapModeVDev->ClearStack(); // clean up aMapModeVDev

        // fast-forward until one after the last background action
        // (need to reconstruct map mode vdev state)
        nActionNum=0;
        pCurrAct=const_cast<GDIMetaFile&>(rInMtf).FirstAction();
        while( pCurrAct && nActionNum<=nLastBgAction )
        {
            // up to and including last ink-generating background
            // action go to background component
            aBackgroundComponent.aComponentList.emplace_back(
                    pCurrAct, nActionNum );

            // execute action to get correct MapModes etc.
            pCurrAct->Execute( aMapModeVDev.get() );
            pCurrAct=const_cast<GDIMetaFile&>(rInMtf).NextAction();
            ++nActionNum;
        }

        //  STAGE 2: Generate connected components list

        ::std::vector<ConnectedComponents> aCCList; // contains distinct sets of connected components as elements.

        // iterate over all actions (start where background action
        // search left off)
        for( ;
             pCurrAct;
             pCurrAct=const_cast<GDIMetaFile&>(rInMtf).NextAction(), ++nActionNum )
        {
            // execute action to get correct MapModes etc.
            pCurrAct->Execute( aMapModeVDev.get() );

            // cache bounds of current action
            const tools::Rectangle aBBCurrAct(pCurrAct->GetBoundsRect(aMapModeVDev.get()));

            // accumulate collected bounds here, initialize with current action
            tools::Rectangle aTotalBounds( aBBCurrAct ); // thus, aTotalComponents.aBounds is empty
                                                         // for non-output-generating actions
            bool bTreatSpecial( false );
            ConnectedComponents aTotalComponents;

            //  STAGE 2.1: Search for intersecting cc entries

            // if aBBCurrAct is empty, it will intersect with no
            // aCCList member. Thus, we can save the check.
            // Furthermore, this ensures that non-output-generating
            // actions get their own aCCList entry, which is necessary
            // when copying them to the output metafile (see stage 4
            // below).

            // #107169# Wholly transparent objects need
            // not be considered for connected components,
            // too. Just put each of them into a separate
            // component.
            aTotalComponents.bIsFullyTransparent = pCurrAct->IsTransparent(aMapModeVDev.get());

            if( !aBBCurrAct.IsEmpty() &&
                !aTotalComponents.bIsFullyTransparent )
            {
                if( !aBackgroundComponent.aComponentList.empty() &&
                    !aBackgroundComponent.aBounds.IsInside(aTotalBounds) )
                {
                    // it seems the background is not large enough. to
                    // be on the safe side, combine with this component.
                    aTotalBounds.Union( aBackgroundComponent.aBounds );

                    // extract all aCurr actions to aTotalComponents
                    aTotalComponents.aComponentList.splice( aTotalComponents.aComponentList.end(),
                                                            aBackgroundComponent.aComponentList );

                    if( aBackgroundComponent.bIsSpecial )
                        bTreatSpecial = true;
                }

                bool                                    bSomeComponentsChanged;

                // now, this is unfortunate: since changing anyone of
                // the aCCList elements (e.g. by merging or addition
                // of an action) might generate new intersection with
                // other aCCList elements, have to repeat the whole
                // element scanning, until nothing changes anymore.
                // Thus, this loop here makes us O(n^3) in the worst
                // case.
                do
                {
                    // only loop here if 'intersects' branch below was hit
                    bSomeComponentsChanged = false;

                    // iterate over all current members of aCCList
                    for( auto aCurrCC=aCCList.begin(); aCurrCC != aCCList.end(); )
                    {
                        // first check if current element's bounds are
                        // empty. This ensures that empty actions are not
                        // merged into one component, as a matter of fact,
                        // they have no position.

                        // #107169# Wholly transparent objects need
                        // not be considered for connected components,
                        // too. Just put each of them into a separate
                        // component.
                        if( !aCurrCC->aBounds.IsEmpty() &&
                            !aCurrCC->bIsFullyTransparent &&
                            aCurrCC->aBounds.IsOver( aTotalBounds ) )
                        {
                            // union the intersecting aCCList element into aTotalComponents

                            // calc union bounding box
                            aTotalBounds.Union( aCurrCC->aBounds );

                            // extract all aCurr actions to aTotalComponents
                            aTotalComponents.aComponentList.splice( aTotalComponents.aComponentList.end(),
                                                                    aCurrCC->aComponentList );

                            if( aCurrCC->bIsSpecial )
                                bTreatSpecial = true;

                            // remove and delete aCurrCC element from list (we've now merged its content)
                            aCurrCC = aCCList.erase( aCurrCC );

                            // at least one component changed, need to rescan everything
                            bSomeComponentsChanged = true;
                        }
                        else
                        {
                            ++aCurrCC;
                        }
                    }
                }
                while( bSomeComponentsChanged );
            }

            //  STAGE 2.2: Determine special state for cc element

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
            // metaactions in the ConnectedComponents are not
            // guaranteed to be the same as in the metafile.
            if( bTreatSpecial )
            {
                // prev component(s) special -> this one, too
                aTotalComponents.bIsSpecial = true;
            }
            else if(!pCurrAct->IsTransparent())
            {
                // added action and none of prev components special ->
                // this one normal, too
                aTotalComponents.bIsSpecial = false;
            }
            else
            {
                // added action is special and none of prev components
                // special -> do the detailed tests

                // can the action handle transparency correctly
                // (i.e. when painted on white background, does the
                // action still look correct)?
                if( !DoesActionHandleTransparency( *pCurrAct ) )
                {
                    // no, action cannot handle its transparency on
                    // a printer device, render to bitmap
                    aTotalComponents.bIsSpecial = true;
                }
                else
                {
                    // yes, action can handle its transparency, so
                    // check whether we're on white background
                    if( aTotalComponents.aComponentList.empty() )
                    {
                        // nothing between pCurrAct and page
                        // background -> don't be special
                        aTotalComponents.bIsSpecial = false;
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
                        aTotalComponents.bIsSpecial = true;
                    }
                }
            }

            //  STAGE 2.3: Add newly generated CC list element

            // set new bounds and add action to list
            aTotalComponents.aBounds = aTotalBounds;
            aTotalComponents.aComponentList.emplace_back(
                    pCurrAct, nActionNum );

            // add aTotalComponents as a new entry to aCCList
            aCCList.push_back( aTotalComponents );

            SAL_WARN_IF( aTotalComponents.aComponentList.empty(), "vcl",
                        "Printer::GetPreparedMetaFile empty component" );
            SAL_WARN_IF( aTotalComponents.aBounds.IsEmpty() && (aTotalComponents.aComponentList.size() != 1), "vcl",
                        "Printer::GetPreparedMetaFile non-output generating actions must be solitary");
            SAL_WARN_IF( aTotalComponents.bIsFullyTransparent && (aTotalComponents.aComponentList.size() != 1), "vcl",
                        "Printer::GetPreparedMetaFile fully transparent actions must be solitary");
        }

        // well now, we've got the list of disjunct connected
        // components. Now we've got to create a map, which contains
        // the corresponding aCCList element for every
        // metaaction. Later on, we always process the complete
        // metafile for each bitmap to be generated, but switch on
        // output only for actions contained in the then current
        // aCCList element. This ensures correct mapmode and attribute
        // settings for all cases.

        // maps mtf actions to CC list entries
        ::std::vector< const ConnectedComponents* > aCCList_MemberMap( rInMtf.GetActionSize() );

        // iterate over all aCCList members and their contained metaactions
        for (auto const& currentItem : aCCList)
        {
            for (auto const& currentAction : currentItem.aComponentList)
            {
                // set pointer to aCCList element for corresponding index
                aCCList_MemberMap[ currentAction.second ] = &currentItem;
            }
        }

        //  STAGE 3.1: Output background mtf actions (if there are any)

        for (auto & component : aBackgroundComponent.aComponentList)
        {
            // simply add this action (above, we inserted the actions
            // starting at index 0 up to and including nLastBgAction)
            rOutMtf.AddAction( component.first );
        }

        //  STAGE 3.2: Generate banded bitmaps for special regions

        Point aPageOffset;
        Size aTmpSize( GetOutputSizePixel() );
        if( meOutDevType == OUTDEV_PDF )
        {
            auto pPdfWriter = static_cast<vcl::PDFWriterImpl*>(this);
            aTmpSize = LogicToPixel(pPdfWriter->getCurPageSize(), MapMode(MapUnit::MapPoint));

            // also add error code to PDFWriter
            pPdfWriter->insertError(vcl::PDFWriter::Warning_Transparency_Converted);
        }
        else if( meOutDevType == OUTDEV_PRINTER )
        {
            Printer* pThis = dynamic_cast<Printer*>(this);
            assert(pThis);
            aPageOffset = pThis->GetPageOffsetPixel();
            aPageOffset = Point( 0, 0 ) - aPageOffset;
            aTmpSize  = pThis->GetPaperSizePixel();
        }
        const tools::Rectangle aOutputRect( aPageOffset, aTmpSize );
        bool bTiling = dynamic_cast<Printer*>(this) != nullptr;

        // iterate over all aCCList members and generate bitmaps for the special ones
        for (auto & currentItem : aCCList)
        {
            if( currentItem.bIsSpecial )
            {
                tools::Rectangle aBoundRect( currentItem.aBounds );
                aBoundRect.Intersection( aOutputRect );

                const double fBmpArea( static_cast<double>(aBoundRect.GetWidth()) * aBoundRect.GetHeight() );
                const double fOutArea( static_cast<double>(aOutputRect.GetWidth()) * aOutputRect.GetHeight() );

                // check if output doesn't exceed given size
                if( bReduceTransparency && bTransparencyAutoMode && ( fBmpArea > ( fReduceTransparencyMinArea * fOutArea ) ) )
                {
                    // output normally. Therefore, we simply clear the
                    // special attribute, as everything non-special is
                    // copied to rOutMtf further below.
                    currentItem.bIsSpecial = false;
                }
                else
                {
                    // create new bitmap action first
                    if( aBoundRect.GetWidth() && aBoundRect.GetHeight() )
                    {
                        Point           aDstPtPix( aBoundRect.TopLeft() );
                        Size            aDstSzPix;

                        ScopedVclPtrInstance<VirtualDevice> aMapVDev;   // here, we record only mapmode information
                        aMapVDev->EnableOutput(false);

                        ScopedVclPtrInstance<VirtualDevice> aPaintVDev; // into this one, we render.
                        aPaintVDev->SetBackground( aBackgroundComponent.aBgColor );

                        rOutMtf.AddAction( new MetaPushAction( PushFlags::MAPMODE ) );
                        rOutMtf.AddAction( new MetaMapModeAction() );

                        aPaintVDev->SetDrawMode( GetDrawMode() );

                        while( aDstPtPix.Y() <= aBoundRect.Bottom() )
                        {
                            aDstPtPix.setX( aBoundRect.Left() );
                            aDstSzPix = bTiling ? Size( MAX_TILE_WIDTH, MAX_TILE_HEIGHT ) : aBoundRect.GetSize();

                            if( ( aDstPtPix.Y() + aDstSzPix.Height() - 1 ) > aBoundRect.Bottom() )
                                aDstSzPix.setHeight( aBoundRect.Bottom() - aDstPtPix.Y() + 1 );

                            while( aDstPtPix.X() <= aBoundRect.Right() )
                            {
                                if( ( aDstPtPix.X() + aDstSzPix.Width() - 1 ) > aBoundRect.Right() )
                                    aDstSzPix.setWidth( aBoundRect.Right() - aDstPtPix.X() + 1 );

                                if( !tools::Rectangle( aDstPtPix, aDstSzPix ).Intersection( aBoundRect ).IsEmpty() &&
                                    aPaintVDev->SetOutputSizePixel( aDstSzPix ) )
                                {
                                    aPaintVDev->Push();
                                    aMapVDev->Push();

                                    aMapVDev->mnDPIX = aPaintVDev->mnDPIX = mnDPIX;
                                    aMapVDev->mnDPIY = aPaintVDev->mnDPIY = mnDPIY;

                                    aPaintVDev->EnableOutput(false);

                                    // iterate over all actions
                                    for( pCurrAct=const_cast<GDIMetaFile&>(rInMtf).FirstAction(), nActionNum=0;
                                         pCurrAct;
                                         pCurrAct=const_cast<GDIMetaFile&>(rInMtf).NextAction(), ++nActionNum )
                                    {
                                        // enable output only for
                                        // actions that are members of
                                        // the current aCCList element
                                        // (currentItem)
                                        if( aCCList_MemberMap[nActionNum] == &currentItem )
                                            aPaintVDev->EnableOutput();

                                        // but process every action
                                        const MetaActionType nType( pCurrAct->GetType() );

                                        if( MetaActionType::MAPMODE == nType )
                                        {
                                            pCurrAct->Execute( aMapVDev.get() );

                                            MapMode     aMtfMap( aMapVDev->GetMapMode() );
                                            const Point aNewOrg( aMapVDev->PixelToLogic( aDstPtPix ) );

                                            aMtfMap.SetOrigin( Point( -aNewOrg.X(), -aNewOrg.Y() ) );
                                            aPaintVDev->SetMapMode( aMtfMap );
                                        }
                                        else if( ( MetaActionType::PUSH == nType ) || MetaActionType::POP == nType )
                                        {
                                            pCurrAct->Execute( aMapVDev.get() );
                                            pCurrAct->Execute( aPaintVDev.get() );
                                        }
                                        else if( MetaActionType::GRADIENT == nType )
                                        {
                                            MetaGradientAction* pGradientAction = static_cast<MetaGradientAction*>(pCurrAct);
                                            Printer* pPrinter = dynamic_cast< Printer* >(this);
                                            if( pPrinter )
                                                pPrinter->DrawGradientEx( aPaintVDev.get(), pGradientAction->GetRect(), pGradientAction->GetGradient() );
                                            else
                                                DrawGradient( pGradientAction->GetRect(), pGradientAction->GetGradient() );
                                        }
                                        else
                                        {
                                            pCurrAct->Execute( aPaintVDev.get() );
                                        }

                                        Application::Reschedule( true );
                                    }

                                    const bool bOldMap = mbMap;
                                    mbMap = aPaintVDev->mbMap = false;

                                    Bitmap aBandBmp( aPaintVDev->GetBitmap( Point(), aDstSzPix ) );

                                    // scale down bitmap, if requested
                                    if( bDownsampleBitmaps )
                                    {
                                        aBandBmp = GetDownsampledBitmap( aDstSzPix,
                                                                         Point(), aBandBmp.GetSizePixel(),
                                                                         aBandBmp, nMaxBmpDPIX, nMaxBmpDPIY );
                                    }

                                    rOutMtf.AddAction( new MetaCommentAction( "PRNSPOOL_TRANSPARENTBITMAP_BEGIN" ) );
                                    rOutMtf.AddAction( new MetaBmpScaleAction( aDstPtPix, aDstSzPix, aBandBmp ) );
                                    rOutMtf.AddAction( new MetaCommentAction( "PRNSPOOL_TRANSPARENTBITMAP_END" ) );

                                    aPaintVDev->mbMap = true;
                                    mbMap = bOldMap;
                                    aMapVDev->Pop();
                                    aPaintVDev->Pop();
                                }

                                // overlapping bands to avoid missing lines (e.g. PostScript)
                                aDstPtPix.AdjustX(aDstSzPix.Width() );
                            }

                            // overlapping bands to avoid missing lines (e.g. PostScript)
                            aDstPtPix.AdjustY(aDstSzPix.Height() );
                        }

                        rOutMtf.AddAction( new MetaPopAction() );
                    }
                }
            }
        }

        aMapModeVDev->ClearStack(); // clean up aMapModeVDev

        //  STAGE 4: Copy actions to output metafile

        // iterate over all actions and duplicate the ones not in a
        // special aCCList member into rOutMtf
        for( pCurrAct=const_cast<GDIMetaFile&>(rInMtf).FirstAction(), nActionNum=0;
             pCurrAct;
             pCurrAct=const_cast<GDIMetaFile&>(rInMtf).NextAction(), ++nActionNum )
        {
            const ConnectedComponents* pCurrAssociatedComponent = aCCList_MemberMap[nActionNum];

            // NOTE: This relies on the fact that map-mode or draw
            // mode changing actions are solitary aCCList elements and
            // have empty bounding boxes, see comment on stage 2.1
            // above
            if( pCurrAssociatedComponent &&
                (pCurrAssociatedComponent->aBounds.IsEmpty() ||
                 !pCurrAssociatedComponent->bIsSpecial) )
            {
                // #107169# Treat transparent bitmaps special, if they
                // are the first (or sole) action in their bounds
                // list. Note that we previously ensured that no
                // fully-transparent objects are before us here.
                if( DoesActionHandleTransparency( *pCurrAct ) &&
                    pCurrAssociatedComponent->aComponentList.begin()->first == pCurrAct )
                {
                    // convert actions, where masked-out parts are of
                    // given background color
                    ConvertTransparentAction(rOutMtf, pCurrAct, *aMapModeVDev, aBackgroundComponent.aBgColor);
                }
                else
                {
                    // simply add this action
                    rOutMtf.AddAction( pCurrAct );
                }

                pCurrAct->Execute(aMapModeVDev.get());
            }
        }

        rOutMtf.SetPrefMapMode( rInMtf.GetPrefMapMode() );
        rOutMtf.SetPrefSize( rInMtf.GetPrefSize() );

#if OSL_DEBUG_LEVEL > 1
        // iterate over all aCCList members and generate rectangles for the bounding boxes
        rOutMtf.AddAction( new MetaFillColorAction( COL_WHITE, false ) );
        for(auto const& aCurr:aCCList)
        {
            if( aCurr.bIsSpecial )
                rOutMtf.AddAction( new MetaLineColorAction( COL_RED, true) );
            else
                rOutMtf.AddAction( new MetaLineColorAction( COL_BLUE, true) );

            rOutMtf.AddAction( new MetaRectAction( aMapModeVDev->PixelToLogic( aCurr.aBounds ) ) );
        }
#endif
    }
    return bTransparent;
}

void Printer::DrawGradientEx( OutputDevice* pOut, const tools::Rectangle& rRect, const Gradient& rGradient )
{
    const PrinterOptions& rPrinterOptions = GetPrinterOptions();

    if( rPrinterOptions.IsReduceGradients() )
    {
        if( PrinterGradientMode::Stripes == rPrinterOptions.GetReducedGradientMode() )
        {
            if( !rGradient.GetSteps() || ( rGradient.GetSteps() > rPrinterOptions.GetReducedGradientStepCount() ) )
            {
                Gradient aNewGradient( rGradient );

                aNewGradient.SetSteps( rPrinterOptions.GetReducedGradientStepCount() );
                pOut->DrawGradient( rRect, aNewGradient );
            }
            else
                pOut->DrawGradient( rRect, rGradient );
        }
        else
        {
            const Color&    rStartColor = rGradient.GetStartColor();
            const Color&    rEndColor = rGradient.GetEndColor();
            const long      nR = ( ( static_cast<long>(rStartColor.GetRed()) * rGradient.GetStartIntensity() ) / 100 +
                                   ( static_cast<long>(rEndColor.GetRed()) * rGradient.GetEndIntensity() ) / 100 ) >> 1;
            const long      nG = ( ( static_cast<long>(rStartColor.GetGreen()) * rGradient.GetStartIntensity() ) / 100 +
                                   ( static_cast<long>(rEndColor.GetGreen()) * rGradient.GetEndIntensity() ) / 100 ) >> 1;
            const long      nB = ( ( static_cast<long>(rStartColor.GetBlue()) * rGradient.GetStartIntensity() ) / 100 +
                                   ( static_cast<long>(rEndColor.GetBlue()) * rGradient.GetEndIntensity() ) / 100 ) >> 1;
            const Color     aColor( static_cast<sal_uInt8>(nR), static_cast<sal_uInt8>(nG), static_cast<sal_uInt8>(nB) );

            pOut->Push( PushFlags::LINECOLOR | PushFlags::FILLCOLOR );
            pOut->SetLineColor( aColor );
            pOut->SetFillColor( aColor );
            pOut->DrawRect( rRect );
            pOut->Pop();
        }
    }
    else
        pOut->DrawGradient( rRect, rGradient );
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
