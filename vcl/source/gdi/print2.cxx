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

template <typename T> tools::Rectangle GetOutputRect(T* pOutDev)
{
    return tools::Rectangle(Point(), pOutDev->GetOutputSizePixel());
}

template <> tools::Rectangle GetOutputRect(Printer* pPrinter)
{
    Point aPageOffset(pPrinter->GetPageOffsetPixel());
    aPageOffset = Point(0, 0) - aPageOffset;

    return tools::Rectangle(aPageOffset, pPrinter->GetPaperSizePixel());
}

template <> tools::Rectangle GetOutputRect(vcl::PDFWriterImpl* pPdfWriter)
{
    // also add error code to PDFWriter
    pPdfWriter->insertError(vcl::PDFWriter::Warning_Transparency_Converted);
    return tools::Rectangle(Point(), pPdfWriter->LogicToPixel(pPdfWriter->getCurPageSize(),
                                                              MapMode(MapUnit::MapPoint)));
}

bool OutputDevice::RemoveTransparenciesFromMetaFile(const GDIMetaFile& rInMtf, GDIMetaFile& rOutMtf,
                                                    long nMaxBmpDPIX, long nMaxBmpDPIY,
                                                    bool bReduceTransparency,
                                                    bool bTransparencyAutoMode,
                                                    bool bDownsampleBitmaps,
                                                    const Color& rBackground)
{
    rOutMtf.Clear();

    // #i10613# Determine set of connected components containing transparent objects. These are
    // then processed as bitmaps, the original actions are removed from the metafile.
    if (!bReduceTransparency || bTransparencyAutoMode)
    {
        if (rInMtf.HasTransparentActions())
        {
            // nothing transparent -> just copy
            rOutMtf = rInMtf;
            return true;
        }
    }

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
    // to this connected components list (aIntersectingActions), by checking
    // each element's bounding box against intersection with the
    // metaaction at hand.
    // All those intersecting elements are removed from aIntersectingActions
    // and collected in a temporary list (aCCMergeList). After all
    // elements have been checked, the aCCMergeList elements are
    // merged with the metaaction at hand into one resulting
    // connected component, with one big bounding box, and
    // inserted into aIntersectingActions again.
    // The time complexity of this algorithm is O(n^3), where n is
    // the number of metafile actions, and it finds all distinct
    // regions of rectangle-bounded connected components. This
    // algorithm was designed by AF.

    //  STAGE 1: Detect background

    // create an OutputDevice to record mapmode changes and the like
    ScopedVclPtrInstance<VirtualDevice> aMapModeVDev;
    aMapModeVDev->mnDPIX = mnDPIX;
    aMapModeVDev->mnDPIY = mnDPIY;
    aMapModeVDev->EnableOutput(false);

    // Receives uniform background content, and is _not_ merged
    // nor checked for intersection against other aIntersectingActions elements
    IntersectingActions aBackgroundAction;

    if (rBackground != COL_TRANSPARENT)
        aBackgroundAction.SetBackground(rBackground, GetBackgroundComponentBounds());

    MetaAction* pCurrAct = nullptr;
    int nActionNum = 0;

    std::tie(nActionNum, pCurrAct)
        = aBackgroundAction.PruneBackgroundObjects(rInMtf, aMapModeVDev.get());

    //  STAGE 2: Generate connected components list

    //::std::vector<IntersectingActions>
    //    aIntersectingActions; // contains distinct sets of connected components as elements.

    IntersectingActionsSet
        aIntersectingActions; // contains distinct sets of connected components as elements.

    aIntersectingActions.GenerateIntersectingActions(aBackgroundAction, pCurrAct, nActionNum,
                                                     rInMtf, aMapModeVDev.get());

    // well now, we've got the list of disjunct connected
    // components. Now we've got to create a map, which contains
    // the corresponding aIntersectingActions element for every
    // metaaction. Later on, we always process the complete
    // metafile for each bitmap to be generated, but switch on
    // output only for actions contained in the then current
    // aIntersectingActions element. This ensures correct mapmode and attribute
    // settings for all cases.

    // maps mtf actions to CC list entries
    ::std::vector<const IntersectingActions*> aIntersectingActions_MemberMap;
    aIntersectingActions.PopulateIntersectingActionsMap(aIntersectingActions_MemberMap,
                                                        rInMtf.GetActionSize());

    //  STAGE 3.1: Output background mtf actions (if there are any)

    for (auto& component : aBackgroundAction.aActionList)
    {
        // simply add this action (above, we inserted the actions
        // starting at index 0 up to and including nLastBgAction)
        rOutMtf.AddAction(component.first);
    }

    //  STAGE 3.2: Generate banded bitmaps for special regions
    const tools::Rectangle aOutputRect(GetOutputRect(this));

    // iterate over all aIntersectingActions members and generate bitmaps for the special ones
    aIntersectingActions.UnmarkIntersectingActions(aOutputRect, bReduceTransparency,
                                                   bTransparencyAutoMode);

    aIntersectingActions.CreateBitmapActions(
        rOutMtf, this, rInMtf, aIntersectingActions_MemberMap, aOutputRect, aBackgroundAction,
        nMaxBmpDPIX, nMaxBmpDPIY, bDownsampleBitmaps, bReduceTransparency, bTransparencyAutoMode);

    aMapModeVDev->ClearStack(); // clean up aMapModeVDev

    //  STAGE 4: Copy actions to output metafile

    // iterate over all actions and duplicate the ones not in a
    // special aIntersectingActions member into rOutMtf
    for (pCurrAct = const_cast<GDIMetaFile&>(rInMtf).FirstAction(), nActionNum = 0; pCurrAct;
         pCurrAct = const_cast<GDIMetaFile&>(rInMtf).NextAction(), ++nActionNum)
    {
        const IntersectingActions* pCurrAssociatedAction
            = aIntersectingActions_MemberMap[nActionNum];

        pCurrAssociatedAction->AddAction(rOutMtf, aBackgroundAction.aBgColor, pCurrAct,
                                         aMapModeVDev.get());
    }

    rOutMtf.SetPrefMapMode(rInMtf.GetPrefMapMode());
    rOutMtf.SetPrefSize(rInMtf.GetPrefSize());

#if OSL_DEBUG_LEVEL > 1
    // iterate over all aIntersectingActions members and generate rectangles for the bounding boxes
    rOutMtf.AddAction(new MetaFillColorAction(COL_WHITE, false));
    for (auto const& aCurr : aIntersectingActions)
    {
        if (aCurr.bIsSpecial)
            rOutMtf.AddAction(new MetaLineColorAction(COL_RED, true));
        else
            rOutMtf.AddAction(new MetaLineColorAction(COL_BLUE, true));

        rOutMtf.AddAction(new MetaRectAction(aMapModeVDev->PixelToLogic(aCurr.aBounds)));
    }
#endif

    return false;
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
