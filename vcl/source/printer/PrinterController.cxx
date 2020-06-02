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

#include <vcl/MetaPushAction.hxx>
#include <vcl/MetaPopAction.hxx>
#include <vcl/PrinterOptions.hxx>
#include <vcl/Printer.hxx>
#include <vcl/weld.hxx>
#include <vcl/svapp.hxx>
#include <vcl/metaact.hxx>
#include <configsettings.hxx>
#include <tools/urlobj.hxx>
#include <comphelper/processfactory.hxx>
#include <comphelper/sequence.hxx>
#include <sal/types.h>
#include <sal/log.hxx>
#include <tools/debug.hxx>

#include <ImplPageCache.hxx>
#include <ImplPrinterControllerData.hxx>
#include <printdlg.hxx>
#include <svdata.hxx>
#include <salinst.hxx>
#include <salprn.hxx>
#include <strings.hrc>

#include <com/sun/star/ui/dialogs/FilePicker.hpp>
#include <com/sun/star/ui/dialogs/TemplateDescription.hpp>
#include <com/sun/star/ui/dialogs/ExecutableDialogResults.hpp>
#include <com/sun/star/view/DuplexMode.hpp>
#include <com/sun/star/lang/IllegalArgumentException.hpp>
#include <com/sun/star/awt/Size.hpp>

#include <unordered_map>
#include <unordered_set>

using namespace vcl;

PrinterController::PrinterController(const VclPtr<Printer>& i_xPrinter, weld::Window* i_pWindow)
    : mpImplData(new ImplPrinterControllerData)
{
    mpImplData->mxPrinter = i_xPrinter;
    mpImplData->mpWindow = i_pWindow;
}

PrinterController::~PrinterController() {}

css::view::PrintableState PrinterController::getJobState() const { return mpImplData->meJobState; }

void PrinterController::setJobState(css::view::PrintableState i_eState)
{
    mpImplData->meJobState = i_eState;
}

const VclPtr<Printer>& PrinterController::getPrinter() const { return mpImplData->mxPrinter; }

weld::Window* PrinterController::getWindow() const { return mpImplData->mpWindow; }

void PrinterController::setPrinter(const VclPtr<Printer>& i_rPrinter)
{
    VclPtr<Printer> xPrinter = mpImplData->mxPrinter;

    Size aPaperSize; // Save current paper size
    Orientation eOrientation = Orientation::Portrait; // Save current paper orientation
    bool bSavedSizeOrientation = false;

    // #tdf 126744 Transfer paper size and orientation settings to newly selected printer
    if (xPrinter)
    {
        aPaperSize = xPrinter->GetPaperSize();
        eOrientation = xPrinter->GetOrientation();
        bSavedSizeOrientation = true;
    }

    mpImplData->mxPrinter = i_rPrinter;
    setValue("Name", css::uno::makeAny(i_rPrinter->GetName()));
    mpImplData->mnDefaultPaperBin = mpImplData->mxPrinter->GetPaperBin();
    mpImplData->mxPrinter->Push();
    mpImplData->mxPrinter->SetMapMode(MapMode(MapUnit::Map100thMM));
    mpImplData->maDefaultPageSize = mpImplData->mxPrinter->GetPaperSize();

    if (bSavedSizeOrientation)
    {
        mpImplData->mxPrinter->SetPaperSizeUser(aPaperSize, !mpImplData->isFixedPageSize());
        mpImplData->mxPrinter->SetOrientation(eOrientation);
    }

    mpImplData->mbPapersizeFromUser = false;
    mpImplData->mxPrinter->Pop();
    mpImplData->mnFixedPaperBin = -1;
}

void PrinterController::resetPrinterOptions(bool i_bFileOutput)
{
    PrinterOptions aOpt;
    aOpt.ReadFromConfig(i_bFileOutput);
    mpImplData->mxPrinter->SetPrinterOptions(aOpt);
}

void PrinterController::setupPrinter(weld::Window* i_pParent)
{
    bool bRet = false;

    // Important to hold printer alive while doing setup etc.
    VclPtr<Printer> xPrinter = mpImplData->mxPrinter;

    if (xPrinter)
    {
        xPrinter->Push();
        xPrinter->SetMapMode(MapMode(MapUnit::Map100thMM));

        // get current data
        Size aPaperSize(xPrinter->GetPaperSize());
        Orientation eOrientation = xPrinter->GetOrientation();
        sal_uInt16 nPaperBin = xPrinter->GetPaperBin();

        // reset paper size back to last configured size, not
        // whatever happens to be the current page
        // (but only if the printer config has changed, otherwise
        // don't override printer page auto-detection - tdf#91362)
        if (getPrinterModified() || getPapersizeFromSetup())
        {
            resetPaperToLastConfigured();
        }

        // call driver setup
        bRet = xPrinter->Setup(i_pParent, PrinterSetupMode::SingleJob);
        SAL_WARN_IF(xPrinter != mpImplData->mxPrinter, "vcl.gdi",
                    "Printer changed underneath us during setup");
        xPrinter = mpImplData->mxPrinter;

        Size aNewPaperSize(xPrinter->GetPaperSize());
        if (bRet)
        {
            bool bInvalidateCache = false;
            setPapersizeFromSetup(xPrinter->GetPrinterSettingsPreferred());

            // was papersize overridden ? if so we need to take action if we're
            // configured to use the driver papersize
            if (aNewPaperSize != mpImplData->maDefaultPageSize)
            {
                mpImplData->maDefaultPageSize = aNewPaperSize;
                bInvalidateCache = getPapersizeFromSetup();
            }

            // was bin overridden ? if so we need to take action
            sal_uInt16 nNewPaperBin = xPrinter->GetPaperBin();
            if (nNewPaperBin != nPaperBin)
            {
                mpImplData->mnFixedPaperBin = nNewPaperBin;
                bInvalidateCache = true;
            }

            if (bInvalidateCache)
            {
                mpImplData->maPageCache.invalidate();
            }
        }
        else
        {
            //restore to whatever it was before we entered this method
            xPrinter->SetOrientation(eOrientation);
            if (aPaperSize != aNewPaperSize)
                xPrinter->SetPaperSizeUser(aPaperSize, !mpImplData->isFixedPageSize());
        }
        xPrinter->Pop();
    }
}

int PrinterController::getPageCountProtected() const
{
    const MapMode aMapMode(MapUnit::Map100thMM);

    mpImplData->mxPrinter->Push();
    mpImplData->mxPrinter->SetMapMode(aMapMode);
    int nPages = getPageCount();
    mpImplData->mxPrinter->Pop();
    return nPages;
}

css::uno::Sequence<css::beans::PropertyValue>
PrinterController::getPageParametersProtected(int i_nPage) const
{
    const MapMode aMapMode(MapUnit::Map100thMM);

    mpImplData->mxPrinter->Push();
    mpImplData->mxPrinter->SetMapMode(aMapMode);
    css::uno::Sequence<css::beans::PropertyValue> aResult(getPageParameters(i_nPage));
    mpImplData->mxPrinter->Pop();
    return aResult;
}

PrinterController::PageSize PrinterController::getPageFile(int i_nUnfilteredPage,
                                                           GDIMetaFile& o_rMtf, bool i_bMayUseCache)
{
    // update progress if necessary
    if (mpImplData->mxProgress)
    {
        // do nothing if printing is canceled
        if (mpImplData->mxProgress->isCanceled())
            return PrinterController::PageSize();
        mpImplData->mxProgress->tick();
        Application::Reschedule(true);
    }

    if (i_bMayUseCache)
    {
        PrinterController::PageSize aPageSize;
        if (mpImplData->maPageCache.get(i_nUnfilteredPage, o_rMtf, aPageSize))
        {
            return aPageSize;
        }
    }
    else
        mpImplData->maPageCache.invalidate();

    o_rMtf.Clear();

    // get page parameters
    css::uno::Sequence<css::beans::PropertyValue> aPageParm(
        getPageParametersProtected(i_nUnfilteredPage));
    const MapMode aMapMode(MapUnit::Map100thMM);

    mpImplData->mxPrinter->Push();
    mpImplData->mxPrinter->SetMapMode(aMapMode);

    // modify job setup if necessary
    PrinterController::PageSize aPageSize = mpImplData->modifyJobSetup(aPageParm);

    o_rMtf.SetPrefSize(aPageSize.aSize);
    o_rMtf.SetPrefMapMode(aMapMode);

    mpImplData->mxPrinter->EnableOutput(false);

    o_rMtf.Record(mpImplData->mxPrinter.get());

    printPage(i_nUnfilteredPage);

    o_rMtf.Stop();
    o_rMtf.WindStart();
    mpImplData->mxPrinter->Pop();

    if (i_bMayUseCache)
        mpImplData->maPageCache.insert(i_nUnfilteredPage, o_rMtf, aPageSize);

    // reset "FirstPage" property to false now we've gotten at least our first one
    mpImplData->mbFirstPage = false;

    return aPageSize;
}

static void appendSubPage(GDIMetaFile& o_rMtf, const tools::Rectangle& i_rClipRect,
                          GDIMetaFile& io_rSubPage, bool i_bDrawBorder)
{
    // intersect all clipregion actions with our clip rect
    io_rSubPage.WindStart();
    io_rSubPage.Clip(i_rClipRect);

    // save gstate
    o_rMtf.AddAction(new MetaPushAction(PushFlags::ALL));

    // clip to page rect
    o_rMtf.AddAction(new MetaClipRegionAction(vcl::Region(i_rClipRect), true));

    // append the subpage
    io_rSubPage.WindStart();
    io_rSubPage.Play(o_rMtf);

    // restore gstate
    o_rMtf.AddAction(new MetaPopAction());

    // draw a border
    if (i_bDrawBorder)
    {
        // save gstate
        o_rMtf.AddAction(new MetaPushAction(PushFlags::LINECOLOR | PushFlags::FILLCOLOR
                                            | PushFlags::CLIPREGION | PushFlags::MAPMODE));
        o_rMtf.AddAction(new MetaMapModeAction(MapMode(MapUnit::Map100thMM)));

        tools::Rectangle aBorderRect(i_rClipRect);
        o_rMtf.AddAction(new MetaLineColorAction(COL_BLACK, true));
        o_rMtf.AddAction(new MetaFillColorAction(COL_TRANSPARENT, false));
        o_rMtf.AddAction(new MetaRectAction(aBorderRect));

        // restore gstate
        o_rMtf.AddAction(new MetaPopAction());
    }
}

PrinterController::PageSize PrinterController::getFilteredPageFile(int i_nFilteredPage,
                                                                   GDIMetaFile& o_rMtf,
                                                                   bool i_bMayUseCache)
{
    const MultiPageSetup& rMPS(mpImplData->maMultiPage);
    int nSubPages = rMPS.nRows * rMPS.nColumns;
    if (nSubPages < 1)
        nSubPages = 1;

    // reverse sheet order
    if (mpImplData->mbReversePageOrder)
    {
        int nDocPages = getFilteredPageCount();
        i_nFilteredPage = nDocPages - 1 - i_nFilteredPage;
    }

    // there is no filtering to be done (and possibly the page size of the
    // original page is to be set), when N-Up is "neutral" that is there is
    // only one subpage and the margins are 0
    if (nSubPages == 1 && rMPS.nLeftMargin == 0 && rMPS.nRightMargin == 0 && rMPS.nTopMargin == 0
        && rMPS.nBottomMargin == 0)
    {
        PrinterController::PageSize aPageSize
            = getPageFile(i_nFilteredPage, o_rMtf, i_bMayUseCache);
        if (mpImplData->meJobState != css::view::PrintableState_JOB_STARTED)
        { // rhbz#657394: check that we are still printing...
            return PrinterController::PageSize();
        }
        Size aPaperSize = mpImplData->getRealPaperSize(aPageSize.aSize, true);
        mpImplData->mxPrinter->SetMapMode(MapMode(MapUnit::Map100thMM));
        mpImplData->mxPrinter->SetPaperSizeUser(aPaperSize, !mpImplData->isFixedPageSize());
        if (aPaperSize != aPageSize.aSize)
        {
            // user overridden page size, center Metafile
            o_rMtf.WindStart();
            long nDX = (aPaperSize.Width() - aPageSize.aSize.Width()) / 2;
            long nDY = (aPaperSize.Height() - aPageSize.aSize.Height()) / 2;
            o_rMtf.Move(nDX, nDY, mpImplData->mxPrinter->GetDPIX(),
                        mpImplData->mxPrinter->GetDPIY());
            o_rMtf.WindStart();
            o_rMtf.SetPrefSize(aPaperSize);
            aPageSize.aSize = aPaperSize;
        }
        return aPageSize;
    }

    // set last page property really only on the very last page to be rendered
    // that is on the last subpage of a NUp run
    bool bIsLastPage = mpImplData->mbLastPage;
    mpImplData->mbLastPage = false;

    Size aPaperSize(mpImplData->getRealPaperSize(mpImplData->maMultiPage.aPaperSize, false));

    // multi page area: page size minus margins + one time spacing right and down
    // the added spacing is so each subpage can be calculated including its spacing
    Size aMPArea(aPaperSize);
    aMPArea.AdjustWidth(-(rMPS.nLeftMargin + rMPS.nRightMargin));
    aMPArea.AdjustWidth(rMPS.nHorizontalSpacing);
    aMPArea.AdjustHeight(-(rMPS.nTopMargin + rMPS.nBottomMargin));
    aMPArea.AdjustHeight(rMPS.nVerticalSpacing);

    // determine offsets
    long nAdvX = aMPArea.Width() / rMPS.nColumns;
    long nAdvY = aMPArea.Height() / rMPS.nRows;

    // determine size of a "cell" subpage, leave a little space around pages
    Size aSubPageSize(nAdvX - rMPS.nHorizontalSpacing, nAdvY - rMPS.nVerticalSpacing);

    o_rMtf.Clear();
    o_rMtf.SetPrefSize(aPaperSize);
    o_rMtf.SetPrefMapMode(MapMode(MapUnit::Map100thMM));
    o_rMtf.AddAction(new MetaMapModeAction(MapMode(MapUnit::Map100thMM)));

    int nDocPages = getPageCountProtected();
    if (mpImplData->meJobState != css::view::PrintableState_JOB_STARTED)
    { // rhbz#657394: check that we are still printing...
        return PrinterController::PageSize();
    }
    for (int nSubPage = 0; nSubPage < nSubPages; nSubPage++)
    {
        // map current sub page to real page
        int nPage = i_nFilteredPage * nSubPages + nSubPage;
        if (nSubPage == nSubPages - 1 || nPage == nDocPages - 1)
        {
            mpImplData->mbLastPage = bIsLastPage;
        }
        if (nPage >= 0 && nPage < nDocPages)
        {
            GDIMetaFile aPageFile;
            PrinterController::PageSize aPageSize = getPageFile(nPage, aPageFile, i_bMayUseCache);
            if (aPageSize.aSize.Width() && aPageSize.aSize.Height())
            {
                long nCellX = 0, nCellY = 0;
                switch (rMPS.nOrder)
                {
                    case NupOrderType::LRTB:
                        nCellX = (nSubPage % rMPS.nColumns);
                        nCellY = (nSubPage / rMPS.nColumns);
                        break;
                    case NupOrderType::TBLR:
                        nCellX = (nSubPage / rMPS.nRows);
                        nCellY = (nSubPage % rMPS.nRows);
                        break;
                    case NupOrderType::RLTB:
                        nCellX = rMPS.nColumns - 1 - (nSubPage % rMPS.nColumns);
                        nCellY = (nSubPage / rMPS.nColumns);
                        break;
                    case NupOrderType::TBRL:
                        nCellX = rMPS.nColumns - 1 - (nSubPage / rMPS.nRows);
                        nCellY = (nSubPage % rMPS.nRows);
                        break;
                }
                // scale the metafile down to a sub page size
                double fScaleX = double(aSubPageSize.Width()) / double(aPageSize.aSize.Width());
                double fScaleY = double(aSubPageSize.Height()) / double(aPageSize.aSize.Height());
                double fScale = std::min(fScaleX, fScaleY);
                aPageFile.Scale(fScale, fScale);
                aPageFile.WindStart();

                // move the subpage so it is centered in its "cell"
                long nOffX
                    = (aSubPageSize.Width() - long(double(aPageSize.aSize.Width()) * fScale)) / 2;
                long nOffY
                    = (aSubPageSize.Height() - long(double(aPageSize.aSize.Height()) * fScale)) / 2;
                long nX = rMPS.nLeftMargin + nOffX + nAdvX * nCellX;
                long nY = rMPS.nTopMargin + nOffY + nAdvY * nCellY;
                aPageFile.Move(nX, nY, mpImplData->mxPrinter->GetDPIX(),
                               mpImplData->mxPrinter->GetDPIY());
                aPageFile.WindStart();
                // calculate border rectangle
                tools::Rectangle aSubPageRect(
                    Point(nX, nY), Size(long(double(aPageSize.aSize.Width()) * fScale),
                                        long(double(aPageSize.aSize.Height()) * fScale)));

                // append subpage to page
                appendSubPage(o_rMtf, aSubPageRect, aPageFile, rMPS.bDrawBorder);
            }
        }
    }
    o_rMtf.WindStart();

    // subsequent getPageFile calls have changed the paper, reset it to current value
    mpImplData->mxPrinter->SetMapMode(MapMode(MapUnit::Map100thMM));
    mpImplData->mxPrinter->SetPaperSizeUser(aPaperSize, !mpImplData->isFixedPageSize());

    return PrinterController::PageSize(aPaperSize, true);
}

int PrinterController::getFilteredPageCount() const
{
    int nDiv = mpImplData->maMultiPage.nRows * mpImplData->maMultiPage.nColumns;
    if (nDiv < 1)
        nDiv = 1;
    return (getPageCountProtected() + (nDiv - 1)) / nDiv;
}

DrawModeFlags PrinterController::removeTransparencies(GDIMetaFile const& i_rIn, GDIMetaFile& o_rOut)
{
    DrawModeFlags nRestoreDrawMode = mpImplData->mxPrinter->GetDrawMode();
    sal_Int32 nMaxBmpDPIX = mpImplData->mxPrinter->GetDPIX();
    sal_Int32 nMaxBmpDPIY = mpImplData->mxPrinter->GetDPIY();

    const PrinterOptions& rPrinterOptions = mpImplData->mxPrinter->GetPrinterOptions();

    static const sal_Int32 OPTIMAL_BMP_RESOLUTION = 300;
    static const sal_Int32 NORMAL_BMP_RESOLUTION = 200;

    if (rPrinterOptions.IsReduceBitmaps())
    {
        // calculate maximum resolution for bitmap graphics
        if (PrinterBitmapMode::Optimal == rPrinterOptions.GetReducedBitmapMode())
        {
            nMaxBmpDPIX = std::min(sal_Int32(OPTIMAL_BMP_RESOLUTION), nMaxBmpDPIX);
            nMaxBmpDPIY = std::min(sal_Int32(OPTIMAL_BMP_RESOLUTION), nMaxBmpDPIY);
        }
        else if (PrinterBitmapMode::Normal == rPrinterOptions.GetReducedBitmapMode())
        {
            nMaxBmpDPIX = std::min(sal_Int32(NORMAL_BMP_RESOLUTION), nMaxBmpDPIX);
            nMaxBmpDPIY = std::min(sal_Int32(NORMAL_BMP_RESOLUTION), nMaxBmpDPIY);
        }
        else
        {
            nMaxBmpDPIX
                = std::min(sal_Int32(rPrinterOptions.GetReducedBitmapResolution()), nMaxBmpDPIX);
            nMaxBmpDPIY
                = std::min(sal_Int32(rPrinterOptions.GetReducedBitmapResolution()), nMaxBmpDPIY);
        }
    }

    // convert to greyscales
    if (rPrinterOptions.IsConvertToGreyscales())
    {
        mpImplData->mxPrinter->SetDrawMode(mpImplData->mxPrinter->GetDrawMode()
                                           | (DrawModeFlags::GrayLine | DrawModeFlags::GrayFill
                                              | DrawModeFlags::GrayText | DrawModeFlags::GrayBitmap
                                              | DrawModeFlags::GrayGradient));
    }

    // disable transparency output
    if (rPrinterOptions.IsReduceTransparency()
        && (PrinterTransparencyMode::NONE == rPrinterOptions.GetReducedTransparencyMode()))
    {
        mpImplData->mxPrinter->SetDrawMode(mpImplData->mxPrinter->GetDrawMode()
                                           | DrawModeFlags::NoTransparency);
    }

    Color aBg(
        COL_TRANSPARENT); // default: let RemoveTransparenciesFromMetaFile do its own background logic
    if (mpImplData->maMultiPage.nRows * mpImplData->maMultiPage.nColumns > 1)
    {
        // in N-Up printing we have no "page" background operation
        // we also have no way to determine the paper color
        // so let's go for white, which will kill 99.9% of the real cases
        aBg = COL_WHITE;
    }
    mpImplData->mxPrinter->RemoveTransparenciesFromMetaFile(
        i_rIn, o_rOut, nMaxBmpDPIX, nMaxBmpDPIY, rPrinterOptions.IsReduceTransparency(),
        rPrinterOptions.GetReducedTransparencyMode() == PrinterTransparencyMode::Auto,
        rPrinterOptions.IsReduceBitmaps() && rPrinterOptions.IsReducedBitmapIncludesTransparency(),
        aBg);
    return nRestoreDrawMode;
}

void PrinterController::printFilteredPage(int i_nPage)
{
    if (mpImplData->meJobState != css::view::PrintableState_JOB_STARTED)
        return; // rhbz#657394: check that we are still printing...

    GDIMetaFile aPageFile;
    PrinterController::PageSize aPageSize = getFilteredPageFile(i_nPage, aPageFile);

    if (mpImplData->mxProgress)
    {
        // do nothing if printing is canceled
        if (mpImplData->mxProgress->isCanceled())
        {
            setJobState(css::view::PrintableState_JOB_ABORTED);
            return;
        }
    }

    // in N-Up printing set the correct page size
    mpImplData->mxPrinter->SetMapMode(MapMode(MapUnit::Map100thMM));
    // aPageSize was filtered through mpImplData->getRealPaperSize already by getFilteredPageFile()
    mpImplData->mxPrinter->SetPaperSizeUser(aPageSize.aSize, !mpImplData->isFixedPageSize());
    if (mpImplData->mnFixedPaperBin != -1
        && mpImplData->mxPrinter->GetPaperBin() != mpImplData->mnFixedPaperBin)
    {
        mpImplData->mxPrinter->SetPaperBin(mpImplData->mnFixedPaperBin);
    }

    // if full paper is meant to be used, move the output to accommodate for pageoffset
    if (aPageSize.bFullPaper)
    {
        Point aPageOffset(mpImplData->mxPrinter->GetPageOffset());
        aPageFile.WindStart();
        aPageFile.Move(-aPageOffset.X(), -aPageOffset.Y(), mpImplData->mxPrinter->GetDPIX(),
                       mpImplData->mxPrinter->GetDPIY());
    }

    GDIMetaFile aCleanedFile;
    DrawModeFlags nRestoreDrawMode = removeTransparencies(aPageFile, aCleanedFile);

    mpImplData->mxPrinter->EnableOutput();

    // actually print the page
    mpImplData->mxPrinter->ImplStartPage();

    mpImplData->mxPrinter->Push();
    aCleanedFile.WindStart();
    aCleanedFile.Play(mpImplData->mxPrinter.get());
    mpImplData->mxPrinter->Pop();

    mpImplData->mxPrinter->ImplEndPage();

    mpImplData->mxPrinter->SetDrawMode(nRestoreDrawMode);
}

void PrinterController::jobStarted() {}

void PrinterController::jobFinished(css::view::PrintableState) {}

void PrinterController::abortJob()
{
    setJobState(css::view::PrintableState_JOB_ABORTED);
    // applications (well, sw) depend on a page request with "IsLastPage" = true
    // to free resources, else they (well, sw) will crash eventually
    setLastPage(true);

    if (mpImplData->mxProgress)
    {
        mpImplData->mxProgress->response(RET_CANCEL);
        mpImplData->mxProgress.reset();
    }

    GDIMetaFile aMtf;
    getPageFile(0, aMtf);
}

void PrinterController::setLastPage(bool i_bLastPage) { mpImplData->mbLastPage = i_bLastPage; }

void PrinterController::setReversePrint(bool i_bReverse)
{
    mpImplData->mbReversePageOrder = i_bReverse;
}

void PrinterController::setPapersizeFromSetup(bool i_bPapersizeFromSetup)
{
    mpImplData->mbPapersizeFromSetup = i_bPapersizeFromSetup;
    mpImplData->mxPrinter->SetPrinterSettingsPreferred(i_bPapersizeFromSetup);
    if (i_bPapersizeFromSetup)
        mpImplData->mbPapersizeFromUser = !i_bPapersizeFromSetup;
}

bool PrinterController::getPapersizeFromSetup() const { return mpImplData->mbPapersizeFromSetup; }

Size& PrinterController::getPaperSizeSetup() const { return mpImplData->maDefaultPageSize; }

void PrinterController::setPaperSizeFromUser(Size i_aUserSize)
{
    mpImplData->mbPapersizeFromUser = true;
    mpImplData->mbPapersizeFromSetup = false;
    mpImplData->mxPrinter->SetPrinterSettingsPreferred(false);

    mpImplData->maUserPageSize = i_aUserSize;
}

Size& PrinterController::getPaperSizeFromUser() const { return mpImplData->maUserPageSize; }

bool PrinterController::isPaperSizeFromUser() const { return mpImplData->mbPapersizeFromUser; }

void PrinterController::setPrinterModified(bool i_bPrinterModified)
{
    mpImplData->mbPrinterModified = i_bPrinterModified;
}

bool PrinterController::getPrinterModified() const { return mpImplData->mbPrinterModified; }

css::uno::Sequence<css::beans::PropertyValue> PrinterController::getJobProperties(
    const css::uno::Sequence<css::beans::PropertyValue>& i_rMergeList) const
{
    std::unordered_set<OUString> aMergeSet;
    size_t nResultLen = size_t(i_rMergeList.getLength()) + mpImplData->maUIProperties.size() + 3;
    for (const auto& rPropVal : i_rMergeList)
        aMergeSet.insert(rPropVal.Name);

    css::uno::Sequence<css::beans::PropertyValue> aResult(nResultLen);
    std::copy(i_rMergeList.begin(), i_rMergeList.end(), aResult.begin());
    int nCur = i_rMergeList.getLength();
    for (const css::beans::PropertyValue& rPropVal : mpImplData->maUIProperties)
    {
        if (aMergeSet.find(rPropVal.Name) == aMergeSet.end())
            aResult[nCur++] = rPropVal;
    }
    // append IsFirstPage
    if (aMergeSet.find("IsFirstPage") == aMergeSet.end())
    {
        css::beans::PropertyValue aVal;
        aVal.Name = "IsFirstPage";
        aVal.Value <<= mpImplData->mbFirstPage;
        aResult[nCur++] = aVal;
    }
    // append IsLastPage
    if (aMergeSet.find("IsLastPage") == aMergeSet.end())
    {
        css::beans::PropertyValue aVal;
        aVal.Name = "IsLastPage";
        aVal.Value <<= mpImplData->mbLastPage;
        aResult[nCur++] = aVal;
    }
    // append IsPrinter
    if (aMergeSet.find("IsPrinter") == aMergeSet.end())
    {
        css::beans::PropertyValue aVal;
        aVal.Name = "IsPrinter";
        aVal.Value <<= true;
        aResult[nCur++] = aVal;
    }
    aResult.realloc(nCur);
    return aResult;
}

const css::uno::Sequence<css::beans::PropertyValue>& PrinterController::getUIOptions() const
{
    return mpImplData->maUIOptions;
}

css::beans::PropertyValue* PrinterController::getValue(const OUString& i_rProperty)
{
    std::unordered_map<OUString, size_t>::const_iterator it
        = mpImplData->maPropertyToIndex.find(i_rProperty);
    return it != mpImplData->maPropertyToIndex.end() ? &mpImplData->maUIProperties[it->second]
                                                     : nullptr;
}

const css::beans::PropertyValue* PrinterController::getValue(const OUString& i_rProperty) const
{
    std::unordered_map<OUString, size_t>::const_iterator it
        = mpImplData->maPropertyToIndex.find(i_rProperty);
    return it != mpImplData->maPropertyToIndex.end() ? &mpImplData->maUIProperties[it->second]
                                                     : nullptr;
}

void PrinterController::setValue(const OUString& i_rPropertyName, const css::uno::Any& i_rValue)
{
    css::beans::PropertyValue aVal;
    aVal.Name = i_rPropertyName;
    aVal.Value = i_rValue;

    setValue(aVal);
}

void PrinterController::setValue(const css::beans::PropertyValue& i_rPropertyValue)
{
    std::unordered_map<OUString, size_t>::const_iterator it
        = mpImplData->maPropertyToIndex.find(i_rPropertyValue.Name);
    if (it != mpImplData->maPropertyToIndex.end())
        mpImplData->maUIProperties[it->second] = i_rPropertyValue;
    else
    {
        // insert correct index into property map
        mpImplData->maPropertyToIndex[i_rPropertyValue.Name] = mpImplData->maUIProperties.size();
        mpImplData->maUIProperties.push_back(i_rPropertyValue);
        mpImplData->maUIPropertyEnabled.push_back(true);
    }
}

void PrinterController::setUIOptions(
    const css::uno::Sequence<css::beans::PropertyValue>& i_rOptions)
{
    SAL_WARN_IF(mpImplData->maUIOptions.hasElements(), "vcl.gdi", "setUIOptions called twice !");

    mpImplData->maUIOptions = i_rOptions;

    for (const auto& rOpt : i_rOptions)
    {
        css::uno::Sequence<css::beans::PropertyValue> aOptProp;
        rOpt.Value >>= aOptProp;
        bool bIsEnabled = true;
        bool bHaveProperty = false;
        OUString aPropName;
        vcl::ImplPrinterControllerData::ControlDependency aDep;
        css::uno::Sequence<sal_Bool> aChoicesDisabled;
        for (const css::beans::PropertyValue& rEntry : std::as_const(aOptProp))
        {
            if (rEntry.Name == "Property")
            {
                css::beans::PropertyValue aVal;
                rEntry.Value >>= aVal;
                DBG_ASSERT(mpImplData->maPropertyToIndex.find(aVal.Name)
                               == mpImplData->maPropertyToIndex.end(),
                           "duplicate property entry");
                setValue(aVal);
                aPropName = aVal.Name;
                bHaveProperty = true;
            }
            else if (rEntry.Name == "Enabled")
            {
                bool bValue = true;
                rEntry.Value >>= bValue;
                bIsEnabled = bValue;
            }
            else if (rEntry.Name == "DependsOnName")
            {
                rEntry.Value >>= aDep.maDependsOnName;
            }
            else if (rEntry.Name == "DependsOnEntry")
            {
                rEntry.Value >>= aDep.mnDependsOnEntry;
            }
            else if (rEntry.Name == "ChoicesDisabled")
            {
                rEntry.Value >>= aChoicesDisabled;
            }
        }
        if (bHaveProperty)
        {
            vcl::ImplPrinterControllerData::PropertyToIndexMap::const_iterator it
                = mpImplData->maPropertyToIndex.find(aPropName);
            // sanity check
            if (it != mpImplData->maPropertyToIndex.end())
            {
                mpImplData->maUIPropertyEnabled[it->second] = bIsEnabled;
            }
            if (!aDep.maDependsOnName.isEmpty())
                mpImplData->maControlDependencies[aPropName] = aDep;
            if (aChoicesDisabled.hasElements())
                mpImplData->maChoiceDisableMap[aPropName] = aChoicesDisabled;
        }
    }
}

bool PrinterController::isUIOptionEnabled(const OUString& i_rProperty) const
{
    bool bEnabled = false;
    std::unordered_map<OUString, size_t>::const_iterator prop_it
        = mpImplData->maPropertyToIndex.find(i_rProperty);
    if (prop_it != mpImplData->maPropertyToIndex.end())
    {
        bEnabled = mpImplData->maUIPropertyEnabled[prop_it->second];

        if (bEnabled)
        {
            // check control dependencies
            vcl::ImplPrinterControllerData::ControlDependencyMap::const_iterator it
                = mpImplData->maControlDependencies.find(i_rProperty);
            if (it != mpImplData->maControlDependencies.end())
            {
                // check if the dependency is enabled
                // if the dependency is disabled, we are too
                bEnabled = isUIOptionEnabled(it->second.maDependsOnName);

                if (bEnabled)
                {
                    // does the dependency have the correct value ?
                    const css::beans::PropertyValue* pVal = getValue(it->second.maDependsOnName);
                    OSL_ENSURE(pVal, "unknown property in dependency");
                    if (pVal)
                    {
                        sal_Int32 nDepVal = 0;
                        bool bDepVal = false;
                        if (pVal->Value >>= nDepVal)
                        {
                            bEnabled = (nDepVal == it->second.mnDependsOnEntry)
                                       || (it->second.mnDependsOnEntry == -1);
                        }
                        else if (pVal->Value >>= bDepVal)
                        {
                            // could be a dependency on a checked boolean
                            // in this case the dependency is on a non zero for checked value
                            bEnabled = (bDepVal && it->second.mnDependsOnEntry != 0)
                                       || (!bDepVal && it->second.mnDependsOnEntry == 0);
                        }
                        else
                        {
                            // if the type does not match something is awry
                            OSL_FAIL("strange type in control dependency");
                            bEnabled = false;
                        }
                    }
                }
            }
        }
    }
    return bEnabled;
}

bool PrinterController::isUIChoiceEnabled(const OUString& i_rProperty, sal_Int32 i_nValue) const
{
    bool bEnabled = true;
    ImplPrinterControllerData::ChoiceDisableMap::const_iterator it
        = mpImplData->maChoiceDisableMap.find(i_rProperty);
    if (it != mpImplData->maChoiceDisableMap.end())
    {
        const css::uno::Sequence<sal_Bool>& rDisabled(it->second);
        if (i_nValue >= 0 && i_nValue < rDisabled.getLength())
            bEnabled = !rDisabled[i_nValue];
    }
    return bEnabled;
}

OUString PrinterController::makeEnabled(const OUString& i_rProperty)
{
    OUString aDependency;

    vcl::ImplPrinterControllerData::ControlDependencyMap::const_iterator it
        = mpImplData->maControlDependencies.find(i_rProperty);
    if (it != mpImplData->maControlDependencies.end())
    {
        if (isUIOptionEnabled(it->second.maDependsOnName))
        {
            aDependency = it->second.maDependsOnName;
            const css::beans::PropertyValue* pVal = getValue(aDependency);
            OSL_ENSURE(pVal, "unknown property in dependency");
            if (pVal)
            {
                sal_Int32 nDepVal = 0;
                bool bDepVal = false;
                if (pVal->Value >>= nDepVal)
                {
                    if (it->second.mnDependsOnEntry != -1)
                    {
                        setValue(aDependency,
                                 css::uno::makeAny(sal_Int32(it->second.mnDependsOnEntry)));
                    }
                }
                else if (pVal->Value >>= bDepVal)
                {
                    setValue(aDependency, css::uno::makeAny(it->second.mnDependsOnEntry != 0));
                }
                else
                {
                    // if the type does not match something is awry
                    OSL_FAIL("strange type in control dependency");
                }
            }
        }
    }

    return aDependency;
}

void PrinterController::createProgressDialog()
{
    if (!mpImplData->mxProgress)
    {
        bool bShow = true;
        css::beans::PropertyValue* pMonitor = getValue("MonitorVisible");
        if (pMonitor)
            pMonitor->Value >>= bShow;
        else
        {
            const css::beans::PropertyValue* pVal = getValue("IsApi");
            if (pVal)
            {
                bool bApi = false;
                pVal->Value >>= bApi;
                bShow = !bApi;
            }
        }

        if (bShow && !Application::IsHeadlessModeEnabled())
        {
            mpImplData->mxProgress
                = std::make_shared<PrintProgressDialog>(getWindow(), getPageCountProtected());
            weld::DialogController::runAsync(mpImplData->mxProgress, [](sal_Int32 /*nResult*/) {});
        }
    }
    else
    {
        mpImplData->mxProgress->response(RET_CANCEL);
        mpImplData->mxProgress.reset();
    }
}

bool PrinterController::isProgressCanceled() const
{
    return mpImplData->mxProgress && mpImplData->mxProgress->isCanceled();
}

void PrinterController::setMultipage(const MultiPageSetup& i_rMPS)
{
    mpImplData->maMultiPage = i_rMPS;
}

const PrinterController::MultiPageSetup& PrinterController::getMultipage() const
{
    return mpImplData->maMultiPage;
}

void PrinterController::resetPaperToLastConfigured() { mpImplData->resetPaperToLastConfigured(); }

void PrinterController::pushPropertiesToPrinter()
{
    sal_Int32 nCopyCount = 1;
    // set copycount and collate
    const css::beans::PropertyValue* pVal = getValue("CopyCount");
    if (pVal)
        pVal->Value >>= nCopyCount;
    bool bCollate = false;
    pVal = getValue("Collate");
    if (pVal)
        pVal->Value >>= bCollate;
    mpImplData->mxPrinter->SetCopyCount(static_cast<sal_uInt16>(nCopyCount), bCollate);

    // duplex mode
    pVal = getValue("DuplexMode");
    if (pVal)
    {
        sal_Int16 nDuplex = css::view::DuplexMode::UNKNOWN;
        pVal->Value >>= nDuplex;
        switch (nDuplex)
        {
            case css::view::DuplexMode::OFF:
                mpImplData->mxPrinter->SetDuplexMode(DuplexMode::Off);
                break;
            case css::view::DuplexMode::LONGEDGE:
                mpImplData->mxPrinter->SetDuplexMode(DuplexMode::LongEdge);
                break;
            case css::view::DuplexMode::SHORTEDGE:
                mpImplData->mxPrinter->SetDuplexMode(DuplexMode::ShortEdge);
                break;
        }
    }
}

bool PrinterController::isShowDialogs() const
{
    bool bApi = getBoolProperty("IsApi", false);
    return !bApi && !Application::IsHeadlessModeEnabled();
}

bool PrinterController::isDirectPrint() const
{
    bool bDirect = getBoolProperty("IsDirect", false);
    return bDirect;
}

bool PrinterController::getBoolProperty(const OUString& i_rProperty, bool i_bFallback) const
{
    bool bRet = i_bFallback;
    const css::beans::PropertyValue* pVal = getValue(i_rProperty);
    if (pVal)
        pVal->Value >>= bRet;
    return bRet;
}

sal_Int32 PrinterController::getIntProperty(const OUString& i_rProperty,
                                            sal_Int32 i_nFallback) const
{
    sal_Int32 nRet = i_nFallback;
    const css::beans::PropertyValue* pVal = getValue(i_rProperty);
    if (pVal)
        pVal->Value >>= nRet;
    return nRet;
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
