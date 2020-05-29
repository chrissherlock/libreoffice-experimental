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

#include <vcl/PrinterOptions.hxx>
#include <vcl/weld.hxx>
#include <vcl/print.hxx>
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

static OUString queryFile( Printer const * pPrinter )
{
    OUString aResult;

    css::uno::Reference< css::uno::XComponentContext > xContext( ::comphelper::getProcessComponentContext() );
    css::uno::Reference< css::ui::dialogs::XFilePicker3 > xFilePicker = css::ui::dialogs::FilePicker::createWithMode(xContext, css::ui::dialogs::TemplateDescription::FILESAVE_AUTOEXTENSION);

    try
    {
#ifdef UNX
        // add PostScript and PDF
        bool bPS = true, bPDF = true;
        if( pPrinter )
        {
            if( pPrinter->GetCapabilities( PrinterCapType::PDF ) )
                bPS = false;
            else
                bPDF = false;
        }
        if( bPS )
            xFilePicker->appendFilter( "PostScript", "*.ps" );
        if( bPDF )
            xFilePicker->appendFilter( "Portable Document Format", "*.pdf" );
#elif defined _WIN32
        (void)pPrinter;
        xFilePicker->appendFilter( "*.PRN", "*.prn" );
#endif
        // add arbitrary files
        xFilePicker->appendFilter(VclResId(SV_STDTEXT_ALLFILETYPES), "*.*");
    }
    catch (const css::lang::IllegalArgumentException&)
    {
        SAL_WARN( "vcl.gdi", "caught IllegalArgumentException when registering filter" );
    }

    if( xFilePicker->execute() == css::ui::dialogs::ExecutableDialogResults::OK )
    {
        css::uno::Sequence< OUString > aPathSeq( xFilePicker->getSelectedFiles() );
        INetURLObject aObj( aPathSeq[0] );
        aResult = aObj.PathToFileName();
    }
    return aResult;
}

namespace {

struct PrintJobAsync
{
    std::shared_ptr<PrinterController>  mxController;
    JobSetup                            maInitSetup;

    PrintJobAsync(const std::shared_ptr<PrinterController>& i_xController,
                  const JobSetup& i_rInitSetup)
    : mxController( i_xController ), maInitSetup( i_rInitSetup )
    {}

    DECL_LINK( ExecJob, void*, void );
};

}

IMPL_LINK_NOARG(PrintJobAsync, ExecJob, void*, void)
{
    Printer::ImplPrintJob(mxController, maInitSetup);

    // clean up, do not access members after this
    delete this;
}

void Printer::PrintJob(const std::shared_ptr<PrinterController>& i_xController,
                       const JobSetup& i_rInitSetup)
{
    bool bSynchronous = false;
    css::beans::PropertyValue* pVal = i_xController->getValue( "Wait" );
    if( pVal )
        pVal->Value >>= bSynchronous;

    if( bSynchronous )
        ImplPrintJob(i_xController, i_rInitSetup);
    else
    {
        PrintJobAsync* pAsync = new PrintJobAsync(i_xController, i_rInitSetup);
        Application::PostUserEvent( LINK( pAsync, PrintJobAsync, ExecJob ) );
    }
}

bool Printer::PreparePrintJob(std::shared_ptr<PrinterController> xController,
                           const JobSetup& i_rInitSetup)
{
    // check if there is a default printer; if not, show an error box (if appropriate)
    if( GetDefaultPrinterName().isEmpty() )
    {
        if (xController->isShowDialogs())
        {
            std::unique_ptr<weld::Builder> xBuilder(Application::CreateBuilder(xController->getWindow(), "vcl/ui/errornoprinterdialog.ui"));
            std::unique_ptr<weld::MessageDialog> xBox(xBuilder->weld_message_dialog("ErrorNoPrinterDialog"));
            xBox->run();
        }
        xController->setValue( "IsDirect",
                               css::uno::makeAny( false ) );
    }

    // setup printer

    // #i114306# changed behavior back from persistence
    // if no specific printer is already set, create the default printer
    if (!xController->getPrinter())
    {
        OUString aPrinterName( i_rInitSetup.GetPrinterName() );
        VclPtrInstance<Printer> xPrinter( aPrinterName );
        xPrinter->SetJobSetup(i_rInitSetup);
        xController->setPrinter(xPrinter);
        xController->setPapersizeFromSetup(xPrinter->GetPrinterSettingsPreferred());
    }

    // reset last page property
    xController->setLastPage(false);

    // update "PageRange" property inferring from other properties:
    // case 1: "Pages" set from UNO API ->
    //         setup "Print Selection" and insert "PageRange" attribute
    // case 2: "All pages" is selected
    //         update "Page range" attribute to have a sensible default,
    //         but leave "All" as selected

    // "Pages" attribute from API is now equivalent to "PageRange"
    // AND "PrintContent" = 1 except calc where it is "PrintRange" = 1
    // Argh ! That sure needs cleaning up
    css::beans::PropertyValue* pContentVal = xController->getValue("PrintRange");
    if( ! pContentVal )
        pContentVal = xController->getValue("PrintContent");

    // case 1: UNO API has set "Pages"
    css::beans::PropertyValue* pPagesVal = xController->getValue("Pages");
    if( pPagesVal )
    {
        OUString aPagesVal;
        pPagesVal->Value >>= aPagesVal;
        if( !aPagesVal.isEmpty() )
        {
            // "Pages" attribute from API is now equivalent to "PageRange"
            // AND "PrintContent" = 1 except calc where it is "PrintRange" = 1
            // Argh ! That sure needs cleaning up
            if( pContentVal )
            {
                pContentVal->Value <<= sal_Int32( 1 );
                xController->setValue("PageRange", pPagesVal->Value);
            }
        }
    }
    // case 2: is "All" selected ?
    else if( pContentVal )
    {
        sal_Int32 nContent = -1;
        if( pContentVal->Value >>= nContent )
        {
            if( nContent == 0 )
            {
                // do not overwrite PageRange if it is already set
                css::beans::PropertyValue* pRangeVal = xController->getValue("PageRange");
                OUString aRange;
                if( pRangeVal )
                    pRangeVal->Value >>= aRange;
                if( aRange.isEmpty() )
                {
                    sal_Int32 nPages = xController->getPageCount();
                    if( nPages > 0 )
                    {
                        OUStringBuffer aBuf( 32 );
                        aBuf.append( "1" );
                        if( nPages > 1 )
                        {
                            aBuf.append( "-" );
                            aBuf.append( nPages );
                        }
                        xController->setValue("PageRange", css::uno::makeAny(aBuf.makeStringAndClear()));
                    }
                }
            }
        }
    }

    css::beans::PropertyValue* pReverseVal = xController->getValue("PrintReverse");
    if( pReverseVal )
    {
        bool bReverse = false;
        pReverseVal->Value >>= bReverse;
        xController->setReversePrint( bReverse );
    }

    css::beans::PropertyValue* pPapersizeFromSetupVal = xController->getValue("PapersizeFromSetup");
    if( pPapersizeFromSetupVal )
    {
        bool bPapersizeFromSetup = false;
        pPapersizeFromSetupVal->Value >>= bPapersizeFromSetup;
        xController->setPapersizeFromSetup(bPapersizeFromSetup);
    }

    // setup NUp printing from properties
    sal_Int32 nRows = xController->getIntProperty("NUpRows", 1);
    sal_Int32 nCols = xController->getIntProperty("NUpColumns", 1);
    if( nRows > 1 || nCols > 1 )
    {
        PrinterController::MultiPageSetup aMPS;
        aMPS.nRows         = std::max<sal_Int32>(nRows, 1);
        aMPS.nColumns      = std::max<sal_Int32>(nCols, 1);
        sal_Int32 nValue = xController->getIntProperty("NUpPageMarginLeft", aMPS.nLeftMargin);
        if( nValue >= 0 )
            aMPS.nLeftMargin = nValue;
        nValue = xController->getIntProperty("NUpPageMarginRight", aMPS.nRightMargin);
        if( nValue >= 0 )
            aMPS.nRightMargin = nValue;
        nValue = xController->getIntProperty( "NUpPageMarginTop", aMPS.nTopMargin );
        if( nValue >= 0 )
            aMPS.nTopMargin = nValue;
        nValue = xController->getIntProperty( "NUpPageMarginBottom", aMPS.nBottomMargin );
        if( nValue >= 0 )
            aMPS.nBottomMargin = nValue;
        nValue = xController->getIntProperty( "NUpHorizontalSpacing", aMPS.nHorizontalSpacing );
        if( nValue >= 0 )
            aMPS.nHorizontalSpacing = nValue;
        nValue = xController->getIntProperty( "NUpVerticalSpacing", aMPS.nVerticalSpacing );
        if( nValue >= 0 )
            aMPS.nVerticalSpacing = nValue;
        aMPS.bDrawBorder = xController->getBoolProperty( "NUpDrawBorder", aMPS.bDrawBorder );
        aMPS.nOrder = static_cast<NupOrderType>(xController->getIntProperty( "NUpSubPageOrder", static_cast<sal_Int32>(aMPS.nOrder) ));
        aMPS.aPaperSize = xController->getPrinter()->PixelToLogic( xController->getPrinter()->GetPaperSizePixel(), MapMode( MapUnit::Map100thMM ) );
        css::beans::PropertyValue* pPgSizeVal = xController->getValue( "NUpPaperSize" );
        css::awt::Size aSizeVal;
        if( pPgSizeVal && (pPgSizeVal->Value >>= aSizeVal) )
        {
            aMPS.aPaperSize.setWidth( aSizeVal.Width );
            aMPS.aPaperSize.setHeight( aSizeVal.Height );
        }

        xController->setMultipage( aMPS );
    }

    // in direct print case check whether there is anything to print.
    // if not, show an errorbox (if appropriate)
    if( xController->isShowDialogs() && xController->isDirectPrint() )
    {
        if( xController->getFilteredPageCount() == 0 )
        {
            std::unique_ptr<weld::Builder> xBuilder(Application::CreateBuilder(xController->getWindow(), "vcl/ui/errornocontentdialog.ui"));
            std::unique_ptr<weld::MessageDialog> xBox(xBuilder->weld_message_dialog("ErrorNoContentDialog"));
            xBox->run();
            return false;
        }
    }

    // check if the printer brings up its own dialog
    // in that case leave the work to that dialog
    if( ! xController->getPrinter()->GetCapabilities( PrinterCapType::ExternalDialog ) &&
        ! xController->isDirectPrint() &&
        xController->isShowDialogs()
        )
    {
        try
        {
            PrintDialog aDlg(xController->getWindow(), xController);
            if (!aDlg.run())
            {
                xController->abortJob();
                return false;
            }
            if (aDlg.isPrintToFile())
            {
                OUString aFile = queryFile( xController->getPrinter().get() );
                if( aFile.isEmpty() )
                {
                    xController->abortJob();
                    return false;
                }
                xController->setValue( "LocalFileName",
                                       css::uno::makeAny( aFile ) );
            }
            else if (aDlg.isSingleJobs())
            {
                xController->setValue( "PrintCollateAsSingleJobs",
                                        css::uno::makeAny( true ) );
            }
        }
        catch (const std::bad_alloc&)
        {
        }
    }

    xController->pushPropertiesToPrinter();
    return true;
}

bool Printer::ExecutePrintJob(const std::shared_ptr<PrinterController>& xController)
{
    OUString aJobName;
    css::beans::PropertyValue* pJobNameVal = xController->getValue( "JobName" );
    if( pJobNameVal )
        pJobNameVal->Value >>= aJobName;

    return xController->getPrinter()->StartJob( aJobName, xController );
}

void Printer::FinishPrintJob(const std::shared_ptr<PrinterController>& xController)
{
    xController->resetPaperToLastConfigured();
    xController->jobFinished( xController->getJobState() );
}

void Printer::ImplPrintJob(const std::shared_ptr<PrinterController>& xController,
                           const JobSetup& i_rInitSetup)
{
    if (PreparePrintJob(xController, i_rInitSetup))
    {
        ExecutePrintJob(xController);
    }
    FinishPrintJob(xController);
}

bool Printer::StartJob( const OUString& i_rJobName, std::shared_ptr<vcl::PrinterController> const & i_xController)
{
    mnError = ERRCODE_NONE;

    if ( IsDisplayPrinter() )
        return false;

    if ( IsJobActive() || IsPrinting() )
        return false;

    sal_uInt32 nCopies = mnCopyCount;
    bool    bCollateCopy = mbCollateCopy;
    bool    bUserCopy = false;

    if ( nCopies > 1 )
    {
        const sal_uInt32 nDevCopy = GetCapabilities( bCollateCopy
            ? PrinterCapType::CollateCopies
            : PrinterCapType::Copies );

        // need to do copies by hand ?
        if ( nCopies > nDevCopy )
        {
            bUserCopy = true;
            nCopies = 1;
            bCollateCopy = false;
        }
    }
    else
        bCollateCopy = false;

    ImplSVData* pSVData = ImplGetSVData();
    mpPrinter = pSVData->mpDefInst->CreatePrinter( mpInfoPrinter );

    if (!mpPrinter)
        return false;

    bool bSinglePrintJobs = false;
    css::beans::PropertyValue* pSingleValue = i_xController->getValue("PrintCollateAsSingleJobs");
    if( pSingleValue )
    {
        pSingleValue->Value >>= bSinglePrintJobs;
    }

    css::beans::PropertyValue* pFileValue = i_xController->getValue("LocalFileName");
    if( pFileValue )
    {
        OUString aFile;
        pFileValue->Value >>= aFile;
        if( !aFile.isEmpty() )
        {
            mbPrintFile = true;
            maPrintFile = aFile;
            bSinglePrintJobs = false;
        }
    }

    OUString* pPrintFile = nullptr;
    if ( mbPrintFile )
        pPrintFile = &maPrintFile;
    mpPrinterOptions->ReadFromConfig( mbPrintFile );

    mbPrinting              = true;
    if( GetCapabilities( PrinterCapType::UsePullModel ) )
    {
        mbJobActive             = true;
        // SAL layer does all necessary page printing
        // and also handles showing a dialog
        // that also means it must call jobStarted when the dialog is finished
        // it also must set the JobState of the Controller
        if( mpPrinter->StartJob( pPrintFile,
                                 i_rJobName,
                                 Application::GetDisplayName(),
                                 &maJobSetup.ImplGetData(),
                                 *i_xController) )
        {
            EndJob();
        }
        else
        {
            mnError = ImplSalPrinterErrorCodeToVCL(mpPrinter->GetErrorCode());
            if ( !mnError )
                mnError = PRINTER_GENERALERROR;
            mbPrinting = false;
            mpPrinter.reset();
            mbJobActive = false;

            GDIMetaFile aDummyFile;
            i_xController->setLastPage(true);
            i_xController->getFilteredPageFile(0, aDummyFile);

            return false;
        }
    }
    else
    {
        // possibly a dialog has been shown
        // now the real job starts
        i_xController->setJobState( css::view::PrintableState_JOB_STARTED );
        i_xController->jobStarted();

        int nJobs = 1;
        int nOuterRepeatCount = 1;
        int nInnerRepeatCount = 1;
        if( bUserCopy )
        {
            if( mbCollateCopy )
                nOuterRepeatCount = mnCopyCount;
            else
                nInnerRepeatCount = mnCopyCount;
        }
        if( bSinglePrintJobs )
        {
            nJobs = mnCopyCount;
            nCopies = 1;
            nOuterRepeatCount = nInnerRepeatCount = 1;
        }

        for( int nJobIteration = 0; nJobIteration < nJobs; nJobIteration++ )
        {
            bool bError = false;
            if( mpPrinter->StartJob( pPrintFile,
                                     i_rJobName,
                                     Application::GetDisplayName(),
                                     nCopies,
                                     bCollateCopy,
                                     i_xController->isDirectPrint(),
                                     &maJobSetup.ImplGetData() ) )
            {
                bool bAborted = false;
                mbJobActive             = true;
                i_xController->createProgressDialog();
                const int nPages = i_xController->getFilteredPageCount();
                // abort job, if no pages will be printed.
                if ( nPages == 0 )
                {
                    i_xController->abortJob();
                    bAborted = true;
                }
                for( int nOuterIteration = 0; nOuterIteration < nOuterRepeatCount && ! bAborted; nOuterIteration++ )
                {
                    for( int nPage = 0; nPage < nPages && ! bAborted; nPage++ )
                    {
                        for( int nInnerIteration = 0; nInnerIteration < nInnerRepeatCount && ! bAborted; nInnerIteration++ )
                        {
                            if( nPage == nPages-1 &&
                                nOuterIteration == nOuterRepeatCount-1 &&
                                nInnerIteration == nInnerRepeatCount-1 &&
                                nJobIteration == nJobs-1 )
                            {
                                i_xController->setLastPage(true);
                            }
                            i_xController->printFilteredPage(nPage);
                            if (i_xController->isProgressCanceled())
                            {
                                i_xController->abortJob();
                            }
                            if (i_xController->getJobState() ==
                                    css::view::PrintableState_JOB_ABORTED)
                            {
                                bAborted = true;
                            }
                        }
                    }
                    // FIXME: duplex ?
                }
                EndJob();

                if( nJobIteration < nJobs-1 )
                {
                    mpPrinter = pSVData->mpDefInst->CreatePrinter( mpInfoPrinter );

                    if ( mpPrinter )
                        mbPrinting              = true;
                    else
                        bError = true;
                }
            }
            else
                bError = true;

            if( bError )
            {
                mnError = mpPrinter ? ImplSalPrinterErrorCodeToVCL(mpPrinter->GetErrorCode()) : ERRCODE_NONE;
                if ( !mnError )
                    mnError = PRINTER_GENERALERROR;
                i_xController->setJobState( mnError == PRINTER_ABORT
                                            ? css::view::PrintableState_JOB_ABORTED
                                            : css::view::PrintableState_JOB_FAILED );
                mbPrinting = false;
                mpPrinter.reset();

                return false;
            }
        }

        if (i_xController->getJobState() == css::view::PrintableState_JOB_STARTED)
            i_xController->setJobState(css::view::PrintableState_JOB_SPOOLED);
    }

    // make last used printer persistent for UI jobs
    if (i_xController->isShowDialogs() && !i_xController->isDirectPrint())
    {
        SettingsConfigItem* pItem = SettingsConfigItem::get();
        pItem->setValue( "PrintDialog",
                         "LastPrinterUsed",
                         GetName()
                         );
    }

    return true;
}

PrinterController::PageSize vcl::ImplPrinterControllerData::modifyJobSetup( const css::uno::Sequence< css::beans::PropertyValue >& i_rProps )
{
    PrinterController::PageSize aPageSize;
    aPageSize.aSize = mxPrinter->GetPaperSize();
    css::awt::Size aSetSize, aIsSize;
    sal_Int32 nPaperBin = mnDefaultPaperBin;
    for( const auto& rProp : i_rProps )
    {
        if ( rProp.Name == "PreferredPageSize" )
        {
            rProp.Value >>= aSetSize;
        }
        else if ( rProp.Name == "PageSize" )
        {
            rProp.Value >>= aIsSize;
        }
        else if ( rProp.Name == "PageIncludesNonprintableArea" )
        {
            bool bVal = false;
            rProp.Value >>= bVal;
            aPageSize.bFullPaper = bVal;
        }
        else if ( rProp.Name == "PrinterPaperTray" )
        {
            sal_Int32 nBin = -1;
            rProp.Value >>= nBin;
            if( nBin >= 0 && nBin < static_cast<sal_Int32>(mxPrinter->GetPaperBinCount()) )
                nPaperBin = nBin;
        }
    }

    Size aCurSize( mxPrinter->GetPaperSize() );
    if( aSetSize.Width && aSetSize.Height )
    {
        Size aSetPaperSize( aSetSize.Width, aSetSize.Height );
        Size aRealPaperSize( getRealPaperSize( aSetPaperSize, true/*bNoNUP*/ ) );
        if( aRealPaperSize != aCurSize )
            aIsSize = aSetSize;
    }

    if( aIsSize.Width && aIsSize.Height )
    {
        aPageSize.aSize.setWidth( aIsSize.Width );
        aPageSize.aSize.setHeight( aIsSize.Height );

        Size aRealPaperSize( getRealPaperSize( aPageSize.aSize, true/*bNoNUP*/ ) );
        if( aRealPaperSize != aCurSize )
            mxPrinter->SetPaperSizeUser( aRealPaperSize, ! isFixedPageSize() );
    }

    // paper bin set from properties in print dialog overrides
    // application default for a page
    if ( mnFixedPaperBin != -1 )
        nPaperBin = mnFixedPaperBin;

    if( nPaperBin != -1 && nPaperBin != mxPrinter->GetPaperBin() )
        mxPrinter->SetPaperBin( nPaperBin );

    return aPageSize;
}

//fdo#61886

//when printing is finished, set the paper size of the printer to either what
//the user explicitly set as the desired paper size, or fallback to whatever
//the printer had before printing started. That way it doesn't contain the last
//paper size of a multiple paper size using document when we are in our normal
//auto accept document paper size mode and end up overwriting the original
//paper size setting for file->printer_settings just by pressing "ok" in the
//print dialog
void vcl::ImplPrinterControllerData::resetPaperToLastConfigured()
{
    mxPrinter->Push();
    mxPrinter->SetMapMode(MapMode(MapUnit::Map100thMM));
    Size aCurSize(mxPrinter->GetPaperSize());
    if (aCurSize != maDefaultPageSize)
        mxPrinter->SetPaperSizeUser(maDefaultPageSize, !isFixedPageSize());
    mxPrinter->Pop();
}

/*
 * PrinterOptionsHelper
**/
css::uno::Any PrinterOptionsHelper::getValue( const OUString& i_rPropertyName ) const
{
    css::uno::Any aRet;
    std::unordered_map< OUString, css::uno::Any >::const_iterator it =
        m_aPropertyMap.find( i_rPropertyName );
    if( it != m_aPropertyMap.end() )
        aRet = it->second;
    return aRet;
}

bool PrinterOptionsHelper::getBoolValue( const OUString& i_rPropertyName, bool i_bDefault ) const
{
    bool bRet = false;
    css::uno::Any aVal( getValue( i_rPropertyName ) );
    return (aVal >>= bRet) ? bRet : i_bDefault;
}

sal_Int64 PrinterOptionsHelper::getIntValue( const OUString& i_rPropertyName, sal_Int64 i_nDefault ) const
{
    sal_Int64 nRet = 0;
    css::uno::Any aVal( getValue( i_rPropertyName ) );
    return (aVal >>= nRet) ? nRet : i_nDefault;
}

OUString PrinterOptionsHelper::getStringValue( const OUString& i_rPropertyName ) const
{
    OUString aRet;
    css::uno::Any aVal( getValue( i_rPropertyName ) );
    return (aVal >>= aRet) ? aRet : OUString();
}

bool PrinterOptionsHelper::processProperties( const css::uno::Sequence< css::beans::PropertyValue >& i_rNewProp )
{
    bool bChanged = false;

    for( const auto& rVal : i_rNewProp )
    {
        std::unordered_map< OUString, css::uno::Any >::iterator it =
            m_aPropertyMap.find( rVal.Name );

        bool bElementChanged = (it == m_aPropertyMap.end()) || (it->second != rVal.Value);
        if( bElementChanged )
        {
            m_aPropertyMap[ rVal.Name ] = rVal.Value;
            bChanged = true;
        }
    }
    return bChanged;
}

void PrinterOptionsHelper::appendPrintUIOptions( css::uno::Sequence< css::beans::PropertyValue >& io_rProps ) const
{
    if( !m_aUIProperties.empty() )
    {
        sal_Int32 nIndex = io_rProps.getLength();
        io_rProps.realloc( nIndex+1 );
        css::beans::PropertyValue aVal;
        aVal.Name = "ExtraPrintUIOptions";
        aVal.Value <<= comphelper::containerToSequence(m_aUIProperties);
        io_rProps[ nIndex ] = aVal;
    }
}

css::uno::Any PrinterOptionsHelper::setUIControlOpt(const css::uno::Sequence< OUString >& i_rIDs,
                                          const OUString& i_rTitle,
                                          const css::uno::Sequence< OUString >& i_rHelpIds,
                                          const OUString& i_rType,
                                          const css::beans::PropertyValue* i_pVal,
                                          const PrinterOptionsHelper::UIControlOptions& i_rControlOptions)
{
    sal_Int32 nElements =
        2                                                             // ControlType + ID
        + (i_rTitle.isEmpty() ? 0 : 1)                                // Text
        + (i_rHelpIds.hasElements() ? 1 : 0)                          // HelpId
        + (i_pVal ? 1 : 0)                                            // Property
        + i_rControlOptions.maAddProps.size()                         // additional props
        + (i_rControlOptions.maGroupHint.isEmpty() ? 0 : 1)           // grouping
        + (i_rControlOptions.mbInternalOnly ? 1 : 0)                  // internal hint
        + (i_rControlOptions.mbEnabled ? 0 : 1)                       // enabled
        ;
    if( !i_rControlOptions.maDependsOnName.isEmpty() )
    {
        nElements += 1;
        if( i_rControlOptions.mnDependsOnEntry != -1 )
            nElements += 1;
        if( i_rControlOptions.mbAttachToDependency )
            nElements += 1;
    }

    css::uno::Sequence< css::beans::PropertyValue > aCtrl( nElements );
    sal_Int32 nUsed = 0;
    if( !i_rTitle.isEmpty() )
    {
        aCtrl[nUsed  ].Name  = "Text";
        aCtrl[nUsed++].Value <<= i_rTitle;
    }
    if( i_rHelpIds.hasElements() )
    {
        aCtrl[nUsed  ].Name = "HelpId";
        aCtrl[nUsed++].Value <<= i_rHelpIds;
    }
    aCtrl[nUsed  ].Name  = "ControlType";
    aCtrl[nUsed++].Value <<= i_rType;
    aCtrl[nUsed  ].Name  = "ID";
    aCtrl[nUsed++].Value <<= i_rIDs;
    if( i_pVal )
    {
        aCtrl[nUsed  ].Name  = "Property";
        aCtrl[nUsed++].Value <<= *i_pVal;
    }
    if( !i_rControlOptions.maDependsOnName.isEmpty() )
    {
        aCtrl[nUsed  ].Name  = "DependsOnName";
        aCtrl[nUsed++].Value <<= i_rControlOptions.maDependsOnName;
        if( i_rControlOptions.mnDependsOnEntry != -1 )
        {
            aCtrl[nUsed  ].Name  = "DependsOnEntry";
            aCtrl[nUsed++].Value <<= i_rControlOptions.mnDependsOnEntry;
        }
        if( i_rControlOptions.mbAttachToDependency )
        {
            aCtrl[nUsed  ].Name  = "AttachToDependency";
            aCtrl[nUsed++].Value <<= i_rControlOptions.mbAttachToDependency;
        }
    }
    if( !i_rControlOptions.maGroupHint.isEmpty() )
    {
        aCtrl[nUsed  ].Name    = "GroupingHint";
        aCtrl[nUsed++].Value <<= i_rControlOptions.maGroupHint;
    }
    if( i_rControlOptions.mbInternalOnly )
    {
        aCtrl[nUsed  ].Name    = "InternalUIOnly";
        aCtrl[nUsed++].Value <<= true;
    }
    if( ! i_rControlOptions.mbEnabled )
    {
        aCtrl[nUsed  ].Name    = "Enabled";
        aCtrl[nUsed++].Value <<= false;
    }

    sal_Int32 nAddProps = i_rControlOptions.maAddProps.size();
    for( sal_Int32 i = 0; i < nAddProps; i++ )
        aCtrl[ nUsed++ ] = i_rControlOptions.maAddProps[i];

    SAL_WARN_IF( nUsed != nElements, "vcl.gdi", "nUsed != nElements, probable heap corruption" );

    return css::uno::makeAny( aCtrl );
}

css::uno::Any PrinterOptionsHelper::setGroupControlOpt(const OUString& i_rID,
                                             const OUString& i_rTitle,
                                             const OUString& i_rHelpId)
{
    css::uno::Sequence< OUString > aHelpId;
    if( !i_rHelpId.isEmpty() )
    {
        aHelpId.realloc( 1 );
        *aHelpId.getArray() = i_rHelpId;
    }
    css::uno::Sequence< OUString > aIds { i_rID };
    return setUIControlOpt(aIds, i_rTitle, aHelpId, "Group");
}

css::uno::Any PrinterOptionsHelper::setSubgroupControlOpt(const OUString& i_rID,
                                                const OUString& i_rTitle,
                                                const OUString& i_rHelpId,
                                                const PrinterOptionsHelper::UIControlOptions& i_rControlOptions)
{
    css::uno::Sequence< OUString > aHelpId;
    if( !i_rHelpId.isEmpty() )
    {
        aHelpId.realloc( 1 );
        *aHelpId.getArray() = i_rHelpId;
    }
    css::uno::Sequence< OUString > aIds { i_rID };
    return setUIControlOpt(aIds, i_rTitle, aHelpId, "Subgroup", nullptr, i_rControlOptions);
}

css::uno::Any PrinterOptionsHelper::setBoolControlOpt(const OUString& i_rID,
                                            const OUString& i_rTitle,
                                            const OUString& i_rHelpId,
                                            const OUString& i_rProperty,
                                            bool i_bValue,
                                            const PrinterOptionsHelper::UIControlOptions& i_rControlOptions)
{
    css::uno::Sequence< OUString > aHelpId;
    if( !i_rHelpId.isEmpty() )
    {
        aHelpId.realloc( 1 );
        *aHelpId.getArray() = i_rHelpId;
    }
    css::beans::PropertyValue aVal;
    aVal.Name = i_rProperty;
    aVal.Value <<= i_bValue;
    css::uno::Sequence< OUString > aIds { i_rID };
    return setUIControlOpt(aIds, i_rTitle, aHelpId, "Bool", &aVal, i_rControlOptions);
}

css::uno::Any PrinterOptionsHelper::setChoiceRadiosControlOpt(const css::uno::Sequence< OUString >& i_rIDs,
                                              const OUString& i_rTitle,
                                              const css::uno::Sequence< OUString >& i_rHelpId,
                                              const OUString& i_rProperty,
                                              const css::uno::Sequence< OUString >& i_rChoices,
                                              sal_Int32 i_nValue,
                                              const css::uno::Sequence< sal_Bool >& i_rDisabledChoices,
                                              const PrinterOptionsHelper::UIControlOptions& i_rControlOptions)
{
    UIControlOptions aOpt( i_rControlOptions );
    sal_Int32 nUsed = aOpt.maAddProps.size();
    aOpt.maAddProps.resize( nUsed + 1 + (i_rDisabledChoices.hasElements() ? 1 : 0) );
    aOpt.maAddProps[nUsed].Name = "Choices";
    aOpt.maAddProps[nUsed].Value <<= i_rChoices;
    if( i_rDisabledChoices.hasElements() )
    {
        aOpt.maAddProps[nUsed+1].Name = "ChoicesDisabled";
        aOpt.maAddProps[nUsed+1].Value <<= i_rDisabledChoices;
    }

    css::beans::PropertyValue aVal;
    aVal.Name = i_rProperty;
    aVal.Value <<= i_nValue;
    return setUIControlOpt(i_rIDs, i_rTitle, i_rHelpId, "Radio", &aVal, aOpt);
}

css::uno::Any PrinterOptionsHelper::setChoiceListControlOpt(const OUString& i_rID,
                                              const OUString& i_rTitle,
                                              const css::uno::Sequence< OUString >& i_rHelpId,
                                              const OUString& i_rProperty,
                                              const css::uno::Sequence< OUString >& i_rChoices,
                                              sal_Int32 i_nValue,
                                              const css::uno::Sequence< sal_Bool >& i_rDisabledChoices,
                                              const PrinterOptionsHelper::UIControlOptions& i_rControlOptions)
{
    UIControlOptions aOpt( i_rControlOptions );
    sal_Int32 nUsed = aOpt.maAddProps.size();
    aOpt.maAddProps.resize( nUsed + 1 + (i_rDisabledChoices.hasElements() ? 1 : 0) );
    aOpt.maAddProps[nUsed].Name = "Choices";
    aOpt.maAddProps[nUsed].Value <<= i_rChoices;
    if( i_rDisabledChoices.hasElements() )
    {
        aOpt.maAddProps[nUsed+1].Name = "ChoicesDisabled";
        aOpt.maAddProps[nUsed+1].Value <<= i_rDisabledChoices;
    }

    css::beans::PropertyValue aVal;
    aVal.Name = i_rProperty;
    aVal.Value <<= i_nValue;
    css::uno::Sequence< OUString > aIds { i_rID };
    return setUIControlOpt(aIds, i_rTitle, i_rHelpId, "List", &aVal, aOpt);
}

css::uno::Any PrinterOptionsHelper::setRangeControlOpt(const OUString& i_rID,
                                             const OUString& i_rTitle,
                                             const OUString& i_rHelpId,
                                             const OUString& i_rProperty,
                                             sal_Int32 i_nValue,
                                             sal_Int32 i_nMinValue,
                                             sal_Int32 i_nMaxValue,
                                             const PrinterOptionsHelper::UIControlOptions& i_rControlOptions)
{
    UIControlOptions aOpt( i_rControlOptions );
    if( i_nMaxValue >= i_nMinValue )
    {
        sal_Int32 nUsed = aOpt.maAddProps.size();
        aOpt.maAddProps.resize( nUsed + 2 );
        aOpt.maAddProps[nUsed  ].Name  = "MinValue";
        aOpt.maAddProps[nUsed++].Value <<= i_nMinValue;
        aOpt.maAddProps[nUsed  ].Name  = "MaxValue";
        aOpt.maAddProps[nUsed++].Value <<= i_nMaxValue;
    }

    css::uno::Sequence< OUString > aHelpId;
    if( !i_rHelpId.isEmpty() )
    {
        aHelpId.realloc( 1 );
        *aHelpId.getArray() = i_rHelpId;
    }
    css::beans::PropertyValue aVal;
    aVal.Name = i_rProperty;
    aVal.Value <<= i_nValue;
    css::uno::Sequence< OUString > aIds { i_rID };
    return setUIControlOpt(aIds, i_rTitle, aHelpId, "Range", &aVal, aOpt);
}

css::uno::Any PrinterOptionsHelper::setEditControlOpt(const OUString& i_rID,
                                            const OUString& i_rTitle,
                                            const OUString& i_rHelpId,
                                            const OUString& i_rProperty,
                                            const OUString& i_rValue,
                                            const PrinterOptionsHelper::UIControlOptions& i_rControlOptions)
{
    css::uno::Sequence< OUString > aHelpId;
    if( !i_rHelpId.isEmpty() )
    {
        aHelpId.realloc( 1 );
        *aHelpId.getArray() = i_rHelpId;
    }
    css::beans::PropertyValue aVal;
    aVal.Name = i_rProperty;
    aVal.Value <<= i_rValue;
    css::uno::Sequence< OUString > aIds { i_rID };
    return setUIControlOpt(aIds, i_rTitle, aHelpId, "Edit", &aVal, i_rControlOptions);
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
