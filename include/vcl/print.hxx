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

#ifndef INCLUDED_VCL_PRINT_HXX
#define INCLUDED_VCL_PRINT_HXX

#include <config_options.h>
#include <rtl/ustring.hxx>
#include <i18nutil/paper.hxx>

#include <vcl/NupOrderType.hxx>
#include <vcl/PrinterSupport.hxx>
#include <vcl/errcode.hxx>
#include <vcl/dllapi.h>
#include <vcl/outdev.hxx>
#include <vcl/prntypes.hxx>
#include <vcl/jobset.hxx>

#include <com/sun/star/beans/PropertyValue.hpp>
#include <com/sun/star/uno/Sequence.hxx>
#include <com/sun/star/view/PrintableState.hpp>

#include <memory>
#include <unordered_map>

class GDIMetaFile;
class SalInfoPrinter;
struct SalPrinterQueueInfo;
class PrinterOptions;
class QueueInfo;
class SalPrinter;
class VirtualDevice;
enum class SalPrinterError;

namespace vcl {
    class PrinterController;
    class Window;
}

namespace weld { class Window; }

class VCL_DLLPUBLIC Printer : public OutputDevice
{
    friend class                ::OutputDevice;

private:
    SalInfoPrinter*             mpInfoPrinter;
    std::unique_ptr<SalPrinter> mpPrinter;
    SalGraphics*                mpJobGraphics;
    VclPtr<Printer>             mpPrev;
    VclPtr<Printer>             mpNext;
    VclPtr<VirtualDevice>       mpDisplayDev;
    std::unique_ptr<PrinterOptions> mpPrinterOptions;
    OUString                    maPrinterName;
    OUString                    maDriver;
    OUString                    maPrintFile;
    JobSetup                    maJobSetup;
    Point                       maPageOffset;
    Size                        maPaperSize;
    ErrCode                     mnError;
    sal_uInt16                  mnPageQueueSize;
    sal_uInt16                  mnCopyCount;
    bool                        mbDefPrinter;
    bool                        mbPrinting;
    bool                        mbJobActive;
    bool                        mbCollateCopy;
    bool                        mbPrintFile;
    bool                        mbInPrintPage;
    bool                        mbNewJobSetup;

    VCL_DLLPRIVATE void         ImplInitData();
    VCL_DLLPRIVATE void         ImplInit( SalPrinterQueueInfo* pInfo );
    VCL_DLLPRIVATE void         ImplInitDisplay();
    VCL_DLLPRIVATE static SalPrinterQueueInfo*
                                ImplGetQueueInfo( const OUString& rPrinterName, const OUString* pDriver );
    VCL_DLLPRIVATE void         ImplUpdatePageData();
    VCL_DLLPRIVATE void         ImplUpdateFontList();
    VCL_DLLPRIVATE void         ImplFindPaperFormatForUserSize( JobSetup&, bool bMatchNearest );

    VCL_DLLPRIVATE bool         StartJob( const OUString& rJobName, std::shared_ptr<vcl::PrinterController> const & );

    static VCL_DLLPRIVATE ErrCode
                                ImplSalPrinterErrorCodeToVCL( SalPrinterError nError );

private:
    VCL_DLLPRIVATE void         EndJob();
                                Printer( const Printer& rPrinter )    = delete;
    Printer&                    operator =( const Printer& rPrinter ) = delete;

public:
    VCL_DLLPRIVATE void         ImplStartPage();
    VCL_DLLPRIVATE void         ImplEndPage();

protected:
    virtual bool                AcquireGraphics() const override;
    virtual void                ReleaseGraphics( bool bRelease = true ) override;
    virtual void                ImplReleaseFonts() override;

    virtual long                GetGradientStepCount( long nMinRect ) override;
    virtual bool                UsePolyPolygonForComplexGradient() override;
    virtual void                ClipAndDrawGradientMetafile ( const Gradient &rGradient,
                                    const tools::PolyPolygon &rPolyPoly ) override;

    void                        ScaleBitmap ( Bitmap&, SalTwoRect& ) override { };
    vcl::Region                 ClipToDeviceBounds(vcl::Region aRegion) const override;

public:
    void                        DrawGradientEx( OutputDevice* pOut, const tools::Rectangle& rRect,
                                    const Gradient& rGradient );
    virtual Bitmap              GetBitmap( const Point& rSrcPt, const Size& rSize ) const override;
    virtual Size                GetButtonBorderSize() override;
    virtual Color               GetMonochromeButtonColor() override { return COL_LIGHTGRAY; }

    bool                        IsScreenComp() const override { return false; }

    void DrawBorder(tools::Rectangle aBorderRect) override
    {
        SetLineColor(COL_BLACK);
        DrawRect(aBorderRect);
    }

protected:
    virtual void                DrawDeviceMask( const Bitmap& rMask, const Color& rMaskColor,
                                    const Point& rDestPt, const Size& rDestSize,
                                    const Point& rSrcPtPixel, const Size& rSrcSizePixel) override;

    bool                        DrawTransformBitmapExDirect( const basegfx::B2DHomMatrix& aFullTransform,
                                    const BitmapEx& rBitmapEx) override;

    bool                        TransformAndReduceBitmapExToTargetRange( const basegfx::B2DHomMatrix& aFullTransform,
                                    basegfx::B2DRange &aVisibleRange, double &fMaximumArea) override;

    void                        DrawDeviceBitmap( const Point& rDestPt, const Size& rDestSize,
                                    const Point& rSrcPtPixel, const Size& rSrcSizePixel,
                                    BitmapEx& rBitmapEx ) override;

    virtual void                EmulateDrawTransparent( const tools::PolyPolygon& rPolyPoly,
                                    sal_uInt16 nTransparencePercent ) override;

    virtual void                SetFontOrientation( LogicalFontInstance* const pFontInstance ) const override;

public:
                                Printer();
                                Printer( const JobSetup& rJobSetup );
                                Printer( const QueueInfo& rQueueInfo );
                                Printer( const OUString& rPrinterName );
    virtual                     ~Printer() override;
    virtual void                dispose() override;

    virtual void SetMetafileMapMode(const MapMode& rNewMapMode, bool) override { SetMapMode(rNewMapMode); }

    static const std::vector< OUString >&
                                GetPrinterQueues();
    static const QueueInfo*     GetQueueInfo( const OUString& rPrinterName, bool bStatusUpdate );
    static OUString             GetDefaultPrinterName();

    const OUString&             GetName() const             { return maPrinterName; }
    const OUString&             GetDriverName() const       { return maDriver; }
    bool                        IsDefPrinter() const        { return mbDefPrinter; }
    bool                        IsDisplayPrinter() const    { return mpDisplayDev != nullptr; }
    bool                        IsValid() const             { return !IsDisplayPrinter(); }

    sal_uInt32                  GetCapabilities( PrinterCapType nType ) const;
    bool                        HasSupport( PrinterSupport eFeature ) const;

    bool                        SetJobSetup( const JobSetup& rSetup );
    const JobSetup&             GetJobSetup() const { return maJobSetup; }

    bool                        Setup(weld::Window* pWindow,
                                      PrinterSetupMode eMode = PrinterSetupMode::DocumentGlobal);
    bool                        SetPrinterProps( const Printer* pPrinter );

    Color                       GetBackgroundColor() const override { return COL_WHITE; }
    Color                       GetReadableFontColor(const Color&, const Color&) const override { return COL_BLACK; }

    /** SetPrinterOptions is used internally only now

        in earlier times it was used only to set the options loaded directly from the configuration
        in SfxPrinter::InitJob, this is now handled internally
        should the need arise to set the printer options outside vcl, also a method would have to be devised
        to not override these again internally
    */
    VCL_DLLPRIVATE void         SetPrinterOptions( const PrinterOptions& rOptions );
    const PrinterOptions&       GetPrinterOptions() const { return( *mpPrinterOptions ); }

    bool                        SetOrientation( Orientation eOrient );
    Orientation                 GetOrientation() const;
    void                        SetDuplexMode( DuplexMode );
    DuplexMode                  GetDuplexMode() const;

    bool                        SetPaperBin( sal_uInt16 nPaperBin );
    sal_uInt16                  GetPaperBin() const;
    void                        SetPaper( Paper ePaper );
    bool                        SetPaperSizeUser( const Size& rSize );
    bool                        SetPaperSizeUser( const Size& rSize, bool bMatchNearest );
    Paper                       GetPaper() const;
    static OUString             GetPaperName( Paper ePaper );

    /** @return Number of available paper formats */
    int                         GetPaperInfoCount() const;

    /** @return Info about paper format nPaper */
    const PaperInfo&            GetPaperInfo( int nPaper ) const;
    sal_uInt16                  GetPaperBinCount() const;
    OUString                    GetPaperBinName( sal_uInt16 nPaperBin ) const;

    bool                        GetPrinterSettingsPreferred() const;
    void                        SetPrinterSettingsPreferred( bool bPaperSizeFromSetup );

    const Size&                 GetPaperSizePixel() const { return maPaperSize; }
    Size                        GetPaperSize() const { return PixelToLogic( maPaperSize ); }
    Size                        GetPaperSize( int nPaper );
    const Point&                GetPageOffsetPixel() const { return maPageOffset; }
    Point                       GetPageOffset() const { return PixelToLogic( maPageOffset ); }

    void                        SetCopyCount( sal_uInt16 nCopy, bool bCollate );
    sal_uInt16                  GetCopyCount() const { return mnCopyCount; }
    bool                        IsCollateCopy() const { return mbCollateCopy; }

    bool                        IsPrinting() const { return mbPrinting; }

    bool                        IsJobActive() const { return mbJobActive; }

    /** Checks the printer list and updates it necessary

        sends a DataChanged event of type DataChangedEventType::PRINTER if the printer list changed
    */
    static void                 updatePrinters();

    /** Execute a print job

        starts a print job asynchronously that is will return
    */
    static void                 PrintJob( const std::shared_ptr<vcl::PrinterController>& i_pController,
                                    const JobSetup& i_rInitSetup );

    virtual bool                HasMirroredGraphics() const override;

    virtual void                DrawOutDev( const Point& rDestPt, const Size& rDestSize,
                                    const Point& rSrcPt,  const Size& rSrcSize ) override;

    virtual void                DrawOutDev( const Point& rDestPt, const Size& rDestSize,
                                    const Point& rSrcPt,  const Size& rSrcSize,
                                    const OutputDevice& rOutDev ) override;

    virtual void                CopyArea( const Point& rDestPt, const Point& rSrcPt,
                                    const Size& rSrcSize, bool bWindowInvalidate = false ) override;

    virtual tools::Rectangle    GetBackgroundComponentBounds() const override;

    // These 3 together are more modular PrintJob(), allowing printing more documents as one print job
    // by repeated calls to ExecutePrintJob(). Used by mailmerge.
    static bool                 PreparePrintJob( std::shared_ptr<vcl::PrinterController> i_pController,
                                    const JobSetup& i_rInitSetup );
    static bool ExecutePrintJob(const std::shared_ptr<vcl::PrinterController>& i_pController);
    static void                 FinishPrintJob( const std::shared_ptr<vcl::PrinterController>& i_pController );

    /** Implementation detail of PrintJob being asynchronous

        not exported, not usable outside vcl
    */
    static void VCL_DLLPRIVATE  ImplPrintJob( const std::shared_ptr<vcl::PrinterController>& i_pController,
                                    const JobSetup& i_rInitSetup );
};

namespace vcl
{
class ImplPrinterControllerData;

class VCL_DLLPUBLIC PrinterOptionsHelper
{
protected:
    std::unordered_map< OUString, css::uno::Any >
                         m_aPropertyMap;
    std::vector< css::beans::PropertyValue >
                         m_aUIProperties;

public:

                         /// Create without ui properties
                         PrinterOptionsHelper() {}

    /** Process a new set of properties

        merges changed properties and returns "true" if any occurred
    */
    bool                 processProperties( const css::uno::Sequence< css::beans::PropertyValue >& i_rNewProp );

    /** Append to a sequence of property values the ui property sequence passed at creation

        as the "ExtraPrintUIOptions" property. if that sequence was empty, no "ExtraPrintUIOptions" property
        will be appended.
    */
    void                 appendPrintUIOptions( css::uno::Sequence< css::beans::PropertyValue >& io_rProps ) const;

    /** @return An empty Any for not existing properties */
    css::uno::Any        getValue( const OUString& i_rPropertyName ) const;

    bool                 getBoolValue( const OUString& i_rPropertyName, bool i_bDefault ) const;
    // convenience for fixed strings
    bool                 getBoolValue( const char* i_pPropName, bool i_bDefault = false ) const
                             { return getBoolValue( OUString::createFromAscii( i_pPropName ), i_bDefault ); }

    sal_Int64            getIntValue( const OUString& i_rPropertyName, sal_Int64 i_nDefault ) const;
    // convenience for fixed strings
    sal_Int64            getIntValue( const char* i_pPropName, sal_Int64 i_nDefault ) const
                             { return getIntValue( OUString::createFromAscii( i_pPropName ), i_nDefault ); }

    OUString             getStringValue( const OUString& i_rPropertyName ) const;
    // convenience for fixed strings
    OUString             getStringValue( const char* i_pPropName ) const
                             { return getStringValue( OUString::createFromAscii( i_pPropName ) ); }

    // helper functions for user to create a single control
    struct UIControlOptions
    {
        OUString         maDependsOnName;
        sal_Int32        mnDependsOnEntry;
        bool             mbAttachToDependency;
        OUString         maGroupHint;
        bool             mbInternalOnly;
        bool             mbEnabled;
        std::vector< css::beans::PropertyValue >
                         maAddProps;

                         UIControlOptions( const OUString& i_rDependsOnName = OUString(),
                             sal_Int32 i_nDependsOnEntry = -1, bool i_bAttachToDependency = false)
                             : maDependsOnName( i_rDependsOnName )
                             , mnDependsOnEntry( i_nDependsOnEntry )
                             , mbAttachToDependency( i_bAttachToDependency )
                             , mbInternalOnly( false )
                             , mbEnabled( true ) {}
    };

    // note: in the following helper functions HelpIds are expected as an OUString
    // the normal HelpId form is OString (byte string instead of UTF16 string)
    // this is because the whole interface is base on UNO properties; in fact the structures
    // are passed over UNO interfaces. UNO does not know a byte string, hence the string is
    // transported via UTF16 strings.

    /// Show general control
    static css::uno::Any setUIControlOpt( const css::uno::Sequence< OUString >& i_rIDs, const OUString& i_rTitle,
                             const css::uno::Sequence< OUString >& i_rHelpId, const OUString& i_rType,
                             const css::beans::PropertyValue* i_pValue = nullptr,
                             const UIControlOptions& i_rControlOptions = UIControlOptions());

    /// Show and set the title of a TagPage of id i_rID
    static css::uno::Any setGroupControlOpt( const OUString& i_rID, const OUString& i_rTitle,
                             const OUString& i_rHelpId);

    /// Show and set the label of a VclFrame of id i_rID
    static css::uno::Any setSubgroupControlOpt( const OUString& i_rID, const OUString& i_rTitle, const OUString& i_rHelpId,
                             const UIControlOptions& i_rControlOptions = UIControlOptions());

    /// Show a bool option as a checkbox
    static css::uno::Any setBoolControlOpt( const OUString& i_rID, const OUString& i_rTitle, const OUString& i_rHelpId,
                             const OUString& i_rProperty, bool i_bValue,
                             const UIControlOptions& i_rControlOptions = UIControlOptions());

    /// Show a set of choices in a list box
    static css::uno::Any setChoiceListControlOpt( const OUString&  i_rID, const OUString& i_rTitle,
                             const css::uno::Sequence< OUString >& i_rHelpId, const OUString& i_rProperty,
                             const css::uno::Sequence< OUString >& i_rChoices, sal_Int32 i_nValue,
                             const css::uno::Sequence< sal_Bool >& i_rDisabledChoices = css::uno::Sequence< sal_Bool >(),
                             const UIControlOptions& i_rControlOptions = UIControlOptions());

    /// Show a set of choices as radio buttons
    static css::uno::Any setChoiceRadiosControlOpt( const css::uno::Sequence< OUString >& i_rIDs,
                             const OUString& i_rTitle,  const css::uno::Sequence< OUString >& i_rHelpId,
                             const OUString& i_rProperty, const css::uno::Sequence< OUString >& i_rChoices,
                             sal_Int32 i_nValue,
                             const css::uno::Sequence< sal_Bool >& i_rDisabledChoices = css::uno::Sequence< sal_Bool >(),
                             const UIControlOptions& i_rControlOptions  = UIControlOptions());

    /** Show an integer range (e.g. a spin field)

        note: max value < min value means do not apply min/max values
    */
    static css::uno::Any setRangeControlOpt( const OUString& i_rID, const OUString& i_rTitle, const OUString& i_rHelpId,
                             const OUString& i_rProperty, sal_Int32 i_nValue, sal_Int32 i_nMinValue,
                             sal_Int32 i_nMaxValue, const UIControlOptions& i_rControlOptions);

    /** Show a string field

        note: max value < min value means do not apply min/max values
    */
    static css::uno::Any setEditControlOpt( const OUString& i_rID, const OUString& i_rTitle, const OUString& i_rHelpId,
                             const OUString&  i_rProperty, const OUString& i_rValue,
                             const UIControlOptions& i_rControlOptions);
}; // class PrinterOptionsHelper

} // namespace vcl


#endif // INCLUDED_VCL_PRINT_HXX

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
