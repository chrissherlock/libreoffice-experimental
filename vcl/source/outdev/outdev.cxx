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

#include <sal/config.h>

#include <sal/log.hxx>
#include <comphelper/processfactory.hxx>
#include <tools/debug.hxx>
#include <tools/mapunit.hxx>
#include <tools/lazydelete.hxx>

#include <vcl/deviceconcepts.hxx>
#include <vcl/graph.hxx>
#include <vcl/metafile/MetaAction.hxx>
#include <vcl/metafile/MetafileRecorder.hxx>
#include <vcl/rendercontext/AntialiasingFlags.hxx>
#include <vcl/rendercontext/DrawModeFlags.hxx>
#include <vcl/svapp.hxx>
#include <vcl/sysdata.hxx>
#include <vcl/text/TextRecordingState.hxx>
#include <vcl/toolkit/unowrap.hxx>
#include <vcl/virdev.hxx>

#include <CoordinateMapper.hxx>
#include <ClippingController.hxx>
#include <devicedispatcher.hxx>
#include <font/FontController.hxx>
#include <GraphicsState.hxx>
#include <font/PhysicalFontFaceCollection.hxx>
#include <salgdi.hxx>
#include <window.h>

#include <com/sun/star/awt/DeviceCapability.hpp>
#include <com/sun/star/awt/DeviceInfo.hpp>
#include <com/sun/star/awt/XWindow.hpp>
#include <com/sun/star/rendering/CanvasFactory.hpp>
#include <com/sun/star/rendering/XSpriteCanvas.hpp>

#ifdef DISABLE_DYNLOADING
// Linking all needed LO code into one .so/executable, these already
// exist in the tools library, so put them in the anonymous namespace
// here to avoid clash...
namespace {
#endif
#ifdef DISABLE_DYNLOADING
}
#endif

using namespace ::com::sun::star::uno;

// Begin initializer and accessor public functions

OutputDevice::OutputDevice(OutDevType eOutDevType)
    : mpMapper(std::make_unique<CoordinateMapper>())
    , mpGraphicsState(std::make_unique<vcl::GraphicsState>())
    , mpFontController(std::make_unique<vcl::font::FontController>())
    , mpFontRealization(std::make_unique<vcl::font::FontRealization>())
    , meOutDevType(eOutDevType)
    , moSettings(Application::GetSettings())
    , mpClippingController(std::make_unique<vcl::ClippingController>())
{
    SetGraphics(nullptr);
    mpUnoGraphicsList               = nullptr;
    mpPrevGraphics                  = nullptr;
    mpNextGraphics                  = nullptr;

    mpFontInstance                  = nullptr;
    mpForcedFallbackInstance        = nullptr;
    mpFontFaceCollection            = nullptr;
    mpExtOutDevData                 = nullptr;
    mpGraphicsState->mnTextLayoutMode                = vcl::text::ComplexTextLayoutFlags::Default;

    if( AllSettings::GetLayoutRTL() ) //#i84553# tip BiDi preference to RTL
        mpGraphicsState->mnTextLayoutMode            = vcl::text::ComplexTextLayoutFlags::BiDiRtl | vcl::text::ComplexTextLayoutFlags::TextOriginLeft;

    meOutDevViewType                = OutDevViewType::DontKnow;
    mbOutput                        = true;
    mbDevOutput                     = false;
    mpGraphicsState->meTextLanguage                  = LANGUAGE_SYSTEM;  // TODO: get default from configuration?
    mbLineColorDirty                 = true;
    mbInitTextColor                 = true;
    mpClippingController->SetNoClipRegion();
    mbEnableRTL                     = false;    // mirroring must be explicitly allowed (typically for windows only)
    mbSubpixelPositioning           = false; // tdf#168002 allow SubpixelPositioning (default: false)

    mpClippingController->SetDirty(true);
}

OutputDevice::~OutputDevice()
{
    disposeOnce();
}

void OutputDevice::dispose()
{
    if ( GetUnoGraphicsList() )
    {
        UnoWrapperBase* pWrapper = UnoWrapperBase::GetUnoWrapper( false );
        if ( pWrapper )
            pWrapper->ReleaseAllGraphics( this );
        delete mpUnoGraphicsList;
        mpUnoGraphicsList = nullptr;
    }

    // #i75163#
    mpMapper->InvalidateViewTransform();

    // for some reason, we haven't removed state from the stack properly
    if ( !maOutDevStateStack.empty() )
        SAL_WARN( "vcl.gdi", "OutputDevice::~OutputDevice(): OutputDevice::Push() calls != OutputDevice::Pop() calls" );
    maOutDevStateStack.clear();

    // release the active font instance
    mpFontInstance.clear();
    mpForcedFallbackInstance.clear();

    // remove cached results of GetDevFontList/GetDevSizeList
    mpFontFaceCollection.reset();

    // release ImplFontCache specific to this OutputDevice
    ClearFontCache();

    // release ImplFontList specific to this OutputDevice
    SetFontCollection(nullptr);

    mpPrevGraphics.reset();
    mpNextGraphics.reset();
    VclReferenceBase::dispose();
}

bool OutputDevice::IsVirtual() const
{
    return false;
}

SalGraphics* OutputDevice::GetGraphics()
{
    DBG_TESTSOLARMUTEX();

    if (!mpGraphics && !AcquireGraphics())
        SAL_WARN("vcl.gdi", "No mpGraphics set");

    return mpGraphics;
}

void OutputDevice::SetGraphics(SalGraphics* pNewGraphics) const
{
    if (mpGraphics == pNewGraphics)
        return;

    mpGraphics = pNewGraphics;
}

SalGraphics const *OutputDevice::GetGraphics() const
{
    DBG_TESTSOLARMUTEX();

    if (!mpGraphics && !AcquireGraphics())
        SAL_WARN("vcl.gdi", "No mpGraphics set");

    return mpGraphics;
}

void OutputDevice::SetConnectMetaFile( GDIMetaFile* pMtf )
{
    maRecorder.SetConnectMetaFile(pMtf);
}

void OutputDevice::SetSettings(const AllSettings& rSettings)
{
    if (*moSettings == rSettings)
        return;

    *moSettings = rSettings;

    // Use our dispatch pattern to notify concrete devices
    // if they have specific resolution/DPI logic to run.
    vcl::DispatchDevice(*this, [](auto& rConcreteDevice) {
        if constexpr (requires { rConcreteDevice.ImplUpdateResolutionSettings(); })
        {
            rConcreteDevice.ImplUpdateResolutionSettings();
        }
    });
}

SystemGraphicsData OutputDevice::GetSystemGfxData() const
{
    if (!mpGraphics && !AcquireGraphics())
        return SystemGraphicsData();
    assert(mpGraphics);

#if USE_HEADLESS_CODE
    if (OUTDEV_WINDOW == GetOutDevType())
        mpGraphics->ApplyFullDamage();
#endif

    return mpGraphics->GetGraphicsData();
}

OUString OutputDevice::GetRenderBackendName() const
{
    if (!mpGraphics && !AcquireGraphics())
        return {};
    assert(mpGraphics);

    return mpGraphics->getRenderBackendName();
}

#if ENABLE_CAIRO_CANVAS

bool OutputDevice::SupportsCairo() const
{
    if (!mpGraphics && !AcquireGraphics())
        return false;
    assert(mpGraphics);

    return mpGraphics->SupportsCairo();
}

cairo::SurfaceSharedPtr OutputDevice::CreateSurface(const cairo::CairoSurfaceSharedPtr& rSurface) const
{
    if (!mpGraphics && !AcquireGraphics())
        return cairo::SurfaceSharedPtr();
    assert(mpGraphics);
    return mpGraphics->CreateSurface(rSurface);
}

cairo::SurfaceSharedPtr OutputDevice::CreateSurface(int x, int y, int width, int height) const
{
    if (!mpGraphics && !AcquireGraphics())
        return cairo::SurfaceSharedPtr();
    assert(mpGraphics);
    return mpGraphics->CreateSurface(*this, x, y, width, height);
}

cairo::SurfaceSharedPtr OutputDevice::CreateBitmapSurface(const BitmapSystemData& rData, const Size& rSize) const
{
    if (!mpGraphics && !AcquireGraphics())
        return cairo::SurfaceSharedPtr();
    assert(mpGraphics);
    return mpGraphics->CreateBitmapSurface(*this, rData, rSize);
}

css::uno::Any OutputDevice::GetNativeSurfaceHandle(cairo::SurfaceSharedPtr& rSurface, const basegfx::B2ISize& rSize) const
{
    if (!mpGraphics && !AcquireGraphics())
        return css::uno::Any();
    assert(mpGraphics);
    return mpGraphics->GetNativeSurfaceHandle(rSurface, rSize);
}

#endif // ENABLE_CAIRO_CANVAS

css::uno::Any OutputDevice::GetSystemGfxDataAny() const
{
    const SystemGraphicsData aSysData = GetSystemGfxData();
    css::uno::Sequence< sal_Int8 > aSeq( reinterpret_cast<sal_Int8 const *>(&aSysData),
                                                      aSysData.nSize );

    return css::uno::Any(aSeq);
}

void OutputDevice::SetRefPoint()
{
    maRecorder.RecordRefPoint(Point(), false);

    mpGraphicsState->mbRefPoint = false;
    mpGraphicsState->maRefPoint.setX(0);
    mpGraphicsState->maRefPoint.setY(0);
}

void OutputDevice::SetRefPoint( const Point& rRefPoint )
{
    maRecorder.RecordRefPoint(rRefPoint, true);

    mpGraphicsState->mbRefPoint = true;
    mpGraphicsState->maRefPoint = rRefPoint;
}

RasterOp OutputDevice::GetRasterOp() const
{
    return mpGraphicsState->meRasterOp;
}

void OutputDevice::SetRasterOp( RasterOp eRasterOp )
{
    maRecorder.RecordRasterOp(eRasterOp);

    if ( mpGraphicsState->meRasterOp != eRasterOp )
    {
        mpGraphicsState->meRasterOp = eRasterOp;
        mbLineColorDirty = mbFillColorDirty = true;

        if( mpGraphics || AcquireGraphics() )
        {
            assert(mpGraphics);
            mpGraphics->SetXORMode( (RasterOp::Invert == mpGraphicsState->meRasterOp) || (RasterOp::Xor == mpGraphicsState->meRasterOp), RasterOp::Invert == mpGraphicsState->meRasterOp );
        }
    }
}

void OutputDevice::EnableOutput( bool bEnable )
{
    mbOutput = bEnable;
}

AntialiasingFlags OutputDevice::GetAntialiasing() const
{
    return mpGraphicsState->mnAntialiasing;
}

void OutputDevice::SetAntialiasing( AntialiasingFlags nMode )
{
    if (mpGraphicsState->mnAntialiasing != nMode)
    {
        mpGraphicsState->mnAntialiasing = nMode;
// mbFontDirty = true; // Removed

        if (mpGraphics)
            mpGraphics->setAntiAlias(bool(mpGraphicsState->mnAntialiasing & AntialiasingFlags::Enable));
    }
}

DrawModeFlags OutputDevice::GetDrawMode() const
{
    return mpGraphicsState->mnDrawMode;
}

void OutputDevice::SetDrawMode(DrawModeFlags nDrawMode)
{
    mpGraphicsState->mnDrawMode = nDrawMode;
}

sal_uInt16 OutputDevice::GetBitCount() const
{
    return vcl::DispatchDevice(*this, [this](auto& rConcreteDevice) -> sal_uInt16 {
        using DeviceType = std::decay_t<decltype(rConcreteDevice)>;

        if constexpr (vcl::StoredBitDepthDevice<DeviceType>)
        {
            // VirtualDevice / PDFWriterImpl path
            return rConcreteDevice.ImplGetBitCount();
        }
        else
        {
            // Window / Printer path: requires hardware access
            if (!mpGraphics && !AcquireGraphics())
                return 0;

            return mpGraphics->GetBitCount();
        }
    });
}

css::uno::Reference< css::awt::XGraphics > OutputDevice::CreateUnoGraphics()
{
    UnoWrapperBase* pWrapper = UnoWrapperBase::GetUnoWrapper();
    return pWrapper ? pWrapper->CreateGraphics( this ) : css::uno::Reference< css::awt::XGraphics >();
}

std::vector< VCLXGraphics* > *OutputDevice::CreateUnoGraphicsList()
{
    mpUnoGraphicsList = new std::vector< VCLXGraphics* >;
    return mpUnoGraphicsList;
}

// Helper public function

bool OutputDevice::SupportsOperation( OutDevSupportType eType ) const
{
    if( !mpGraphics && !AcquireGraphics() )
        return false;
    assert(mpGraphics);
    const bool bHasSupport = mpGraphics->supportsOperation( eType );
    return bHasSupport;
}

// Direct OutputDevice drawing public functions

void OutputDevice::DrawOutDev( const Point& rDestPt, const Size& rDestSize,
                               const Point& rSrcPt,  const Size& rSrcSize )
{
    if( IsLayoutCalculationNecessary() )
        return;

    if ( RasterOp::Invert == mpGraphicsState->meRasterOp )
    {
        DrawRect( tools::Rectangle( rDestPt, rDestSize ) );
        return;
    }

    if ( maRecorder.IsActive() )
    {
        const Bitmap aBmp( GetBitmap( rSrcPt, rSrcSize ) );
        maRecorder.RecordBitmapScale( rDestPt, rDestSize, aBmp );
    }

    if ( !IsDeviceOutputNecessary() )
        return;

    if ( !mpGraphics && !AcquireGraphics() )
        return;
    assert(mpGraphics);

    if ( mpClippingController->IsDirty() )
        InitClipRegion();

    if ( IsOutputCulled() )
        return;

    tools::Long nSrcWidth = LogicWidthToDevicePixel(rSrcSize.Width());
    tools::Long nSrcHeight  = LogicHeightToDevicePixel(rSrcSize.Height());
    tools::Long nDestWidth = LogicWidthToDevicePixel(rDestSize.Width());
    tools::Long nDestHeight = LogicHeightToDevicePixel(rDestSize.Height());

    if (nSrcWidth && nSrcHeight && nDestWidth && nDestHeight)
    {
        SalTwoRect aPosAry(LogicXToDevicePixel(rSrcPt.X()), LogicYToDevicePixel(rSrcPt.Y()),
                           nSrcWidth, nSrcHeight,
                           LogicXToDevicePixel(rDestPt.X()), LogicYToDevicePixel(rDestPt.Y()),
                           nDestWidth, nDestHeight);

        AdjustTwoRect( aPosAry, GetOutputRectPixel() );

        if (aPosAry.HasArea())
            mpGraphics->CopyBits(aPosAry, *this);
    }
}

void OutputDevice::DrawOutDev( const Point& rDestPt, const Size& rDestSize,
                               const Point& rSrcPt,  const Size& rSrcSize,
                               const OutputDevice& rOutDev )
{
    if ( IsLayoutCalculationNecessary() )
        return;

    if ( RasterOp::Invert == mpGraphicsState->meRasterOp )
    {
        DrawRect( tools::Rectangle( rDestPt, rDestSize ) );
        return;
    }

    if ( maRecorder.IsActive() )
    {
        const Bitmap aBmp(rOutDev.GetBitmap(rSrcPt, rSrcSize));
        maRecorder.RecordBitmapExScale(rDestPt, rDestSize, aBmp);
    }

    if ( !IsDeviceOutputNecessary() )
        return;

    if ( !mpGraphics && !AcquireGraphics() )
        return;
    assert(mpGraphics);

    if ( mpClippingController->IsDirty() )
        InitClipRegion();

    if ( IsOutputCulled() )
        return;

    SalTwoRect aPosAry(rOutDev.LogicXToDevicePixel(rSrcPt.X()),
                             rOutDev.LogicYToDevicePixel(rSrcPt.Y()),
                             rOutDev.LogicWidthToDevicePixel(rSrcSize.Width()),
                             rOutDev.LogicHeightToDevicePixel(rSrcSize.Height()),
                             LogicXToDevicePixel(rDestPt.X()),
                             LogicYToDevicePixel(rDestPt.Y()),
                             LogicWidthToDevicePixel(rDestSize.Width()),
                             LogicHeightToDevicePixel(rDestSize.Height()));

    // if we have alpha, this will blend source over destination
    drawOutDevDirect(rOutDev, aPosAry);
}

void OutputDevice::CopyArea( const Point& rDestPt,
                             const Point& rSrcPt,  const Size& rSrcSize )
{
    if ( IsLayoutCalculationNecessary() )
        return;

    RasterOp eOldRop = GetRasterOp();
    SetRasterOp( RasterOp::OverPaint );

    if ( !IsDeviceOutputNecessary() )
        return;

    if ( !mpGraphics && !AcquireGraphics() )
        return;
    assert(mpGraphics);

    if ( mpClippingController->IsDirty() )
        InitClipRegion();

    if ( IsOutputCulled() )
        return;

    tools::Long nSrcWidth = LogicWidthToDevicePixel(rSrcSize.Width());
    tools::Long nSrcHeight = LogicHeightToDevicePixel(rSrcSize.Height());
    if (nSrcWidth && nSrcHeight)
    {
        SalTwoRect aPosAry(LogicXToDevicePixel(rSrcPt.X()), LogicYToDevicePixel(rSrcPt.Y()),
                           nSrcWidth, nSrcHeight,
                           LogicXToDevicePixel(rDestPt.X()), LogicYToDevicePixel(rDestPt.Y()),
                           nSrcWidth, nSrcHeight);

        AdjustTwoRect( aPosAry, GetOutputRectPixel() );

        CopyDeviceArea( aPosAry );
    }

    SetRasterOp( eOldRop );
}

// Direct OutputDevice drawing protected function

void OutputDevice::CopyDeviceArea( SalTwoRect& aPosAry )
{
    if (!aPosAry.HasArea())
        return;

    aPosAry.mnDestWidth  = aPosAry.mnSrcWidth;
    aPosAry.mnDestHeight = aPosAry.mnSrcHeight;
    mpGraphics->CopyBits(aPosAry, *this);
}

// Direct OutputDevice drawing private function
void OutputDevice::drawOutDevDirect(const OutputDevice& rSrcDev, SalTwoRect& rPosAry)
{
    SalGraphics* pSrcGraphics;
    if (const OutputDevice* pCheckedSrc = DrawOutDevDirectCheck(rSrcDev))
    {
        if (!pCheckedSrc->mpGraphics && !pCheckedSrc->AcquireGraphics())
            return;
        pSrcGraphics = pCheckedSrc->mpGraphics;
    }
    else
        pSrcGraphics = nullptr;

    if (!mpGraphics && !AcquireGraphics())
        return;
    assert(mpGraphics);

    // #102532# Offset only has to be pseudo window offset

    AdjustTwoRect( rPosAry, rSrcDev.GetOutputRectPixel() );

    if (rPosAry.HasArea())
    {
        // if this is no window, but rSrcDev is a window
        // mirroring may be required
        // because only windows have a SalGraphicsLayout
        // mirroring is performed here
        DrawOutDevDirectProcess(rSrcDev, rPosAry, pSrcGraphics);
    }
}

const OutputDevice* OutputDevice::DrawOutDevDirectCheck(const OutputDevice& rSrcDev) const
{
    return this == &rSrcDev ? nullptr : &rSrcDev;
}

void OutputDevice::DrawOutDevDirectProcess(const OutputDevice& rSrcDev, SalTwoRect& rPosAry, SalGraphics* pSrcGraphics)
{
    if( pSrcGraphics && (pSrcGraphics->GetLayout() & SalLayoutFlags::BiDiRtl) )
    {
        SalTwoRect aPosAry2 = rPosAry;
        pSrcGraphics->mirror( aPosAry2.mnSrcX, aPosAry2.mnSrcWidth, rSrcDev );
        mpGraphics->CopyBits( aPosAry2, *pSrcGraphics, *this, rSrcDev );
        return;
    }
    if (pSrcGraphics)
        mpGraphics->CopyBits( rPosAry, *pSrcGraphics, *this, rSrcDev );
    else
        mpGraphics->CopyBits( rPosAry, *this );
}

// Layout public functions

void OutputDevice::EnableRTL(bool bEnable)
{
    if (mbEnableRTL == bEnable)
        return;

    mbEnableRTL = bEnable;

    vcl::DispatchDevice(*this, [bEnable](auto& rConcreteDevice) {
        // If the device has the hook (VirtualDevice), call it.
        // If not (Printer, Window), the compiler optimizes this away.
        if constexpr (requires { rConcreteDevice.ImplUpdateDeviceRTLState(bEnable); })
        {
            rConcreteDevice.ImplUpdateDeviceRTLState(bEnable);
        }
    });
}

bool OutputDevice::ImplIsAntiparallel() const
{
    bool bRet = false;
    if( AcquireGraphics() )
    {
        if( ( (mpGraphics->GetLayout() & SalLayoutFlags::BiDiRtl) && ! IsRTLEnabled() ) ||
            ( ! (mpGraphics->GetLayout() & SalLayoutFlags::BiDiRtl) && IsRTLEnabled() ) )
        {
            bRet = true;
        }
    }
    return bRet;
}

// note: the coordinates to be remirrored are in frame coordinates !

void    OutputDevice::ReMirror( Point &rPoint ) const
{
    rPoint.setX(GetOutOffXPixel() + GetOutputWidthPixel() - 1 - rPoint.X() + GetOutOffXPixel());
}
void    OutputDevice::ReMirror( tools::Rectangle &rRect ) const
{
    tools::Long nWidth = rRect.Right() - rRect.Left();

    //long lc_x = rRect.nLeft - mnOutOffX;    // normalize
    //lc_x = GetOutputWidthPixel() - nWidth - 1 - lc_x;  // mirror
    //rRect.nLeft = lc_x + mnOutOffX;         // re-normalize

    rRect.SetLeft(GetOutOffXPixel() + GetOutputWidthPixel() - nWidth - 1 - rRect.Left() + GetOutOffXPixel());
    rRect.SetRight( rRect.Left() + nWidth );
}

void OutputDevice::ReMirror( vcl::Region &rRegion ) const
{
    RectangleVector aRectangles;
    rRegion.GetRegionRectangles(aRectangles);
    vcl::Region aMirroredRegion;

    for (auto & rectangle : aRectangles)
    {
        ReMirror(rectangle);
        aMirroredRegion.Union(rectangle);
    }

    rRegion = std::move(aMirroredRegion);

}

bool OutputDevice::HasMirroredGraphics() const
{
    bool bRet = false;

    vcl::DispatchDevice(*this, [this, &bRet](auto& rDev) {
        using DevType = std::decay_t<decltype(rDev)>;

        if constexpr (vcl::AutoMirroringCapable<DevType>)
        {
            // Standard behavior: check if graphics are RTL
            bRet = (AcquireGraphics() && (mpGraphics->GetLayout() & SalLayoutFlags::BiDiRtl));
        }
        else
        {
            // Suppressed behavior (Printers): ignore graphics layout to prevent
            // text disappearance in RTL environments (AOO bug i55719).
            bRet = false;
        }
    });

    return bRet;
}

css::awt::DeviceInfo OutputDevice::GetCommonDeviceInfo(Size const& rDevSz) const
{
    css::awt::DeviceInfo aInfo;

    aInfo.Width = rDevSz.Width();
    aInfo.Height = rDevSz.Height();

    Size aTmpSz = LogicToPixel(Size(1000, 1000), MapMode(MapUnit::MapMM));
    aInfo.PixelPerMeterX = aTmpSz.Width();
    aInfo.PixelPerMeterY = aTmpSz.Height();
    aInfo.BitsPerPixel = GetBitCount();

    aInfo.Capabilities = css::awt::DeviceCapability::RASTEROPERATIONS |
        css::awt::DeviceCapability::GETBITS;

    return aInfo;
}

css::awt::DeviceInfo OutputDevice::GetDeviceInfo() const
{
    css::awt::DeviceInfo aInfo = GetCommonDeviceInfo(GetOutputSizePixel());

    aInfo.LeftInset = 0;
    aInfo.TopInset = 0;
    aInfo.RightInset = 0;
    aInfo.BottomInset = 0;

    return aInfo;
}

Reference< css::rendering::XCanvas > OutputDevice::GetCanvas() const
{
    // try to retrieve hard reference from weak member
    Reference< css::rendering::XCanvas > xCanvas( mxCanvas );
    // canvas still valid? Then we're done.
    if( xCanvas.is() )
        return xCanvas;
    xCanvas = ImplGetCanvas( false );
    mxCanvas = xCanvas;
    return xCanvas;
}

Reference< css::rendering::XSpriteCanvas > OutputDevice::GetSpriteCanvas() const
{
    Reference< css::rendering::XCanvas > xCanvas( mxCanvas );
    Reference< css::rendering::XSpriteCanvas > xSpriteCanvas( xCanvas, UNO_QUERY );
    if( xSpriteCanvas.is() )
        return xSpriteCanvas;
    xCanvas = ImplGetCanvas( true );
    mxCanvas = xCanvas;
    return Reference< css::rendering::XSpriteCanvas >( xCanvas, UNO_QUERY );
}

// Generic implementation, Window will override.
css::uno::Reference< css::rendering::XCanvas > OutputDevice::ImplGetCanvas( bool bSpriteCanvas ) const
{
    /* Arguments:
       0: ptr to creating instance (Window or VirtualDevice)
       1: current bounds of creating instance
       2: bool, denoting always on top state for Window (always false for VirtualDevice)
       3: XWindow for creating Window (or empty for VirtualDevice)
       4: SystemGraphicsData as a streamed Any
     */
    Sequence< Any > aArg{
        Any(reinterpret_cast<sal_Int64>(this)),
        Any(css::awt::Rectangle(GetOutOffXPixel(), GetOutOffYPixel(), GetOutputWidthPixel(), GetOutputHeightPixel())),
        Any(false),
        Any(Reference< css::awt::XWindow >()),
        GetSystemGfxDataAny()
    };

    const Reference< XComponentContext >& xContext = comphelper::getProcessComponentContext();

    static tools::DeleteUnoReferenceOnDeinit<css::lang::XMultiComponentFactory> xStaticCanvasFactory(
        css::rendering::CanvasFactory::create( xContext ) );
    Reference<css::lang::XMultiComponentFactory> xCanvasFactory(xStaticCanvasFactory.get());
    Reference< css::rendering::XCanvas > xCanvas;

    if(xCanvasFactory.is())
    {
        xCanvas.set( xCanvasFactory->createInstanceWithArgumentsAndContext(
                         bSpriteCanvas ?
                         u"com.sun.star.rendering.SpriteCanvas"_ustr :
                         u"com.sun.star.rendering.Canvas"_ustr,
                         aArg,
                         xContext ),
                     UNO_QUERY );
    }

    // no factory??? Empty reference, then.
    return xCanvas;
}

void OutputDevice::ImplDisposeCanvas()
{
    css::uno::Reference< css::rendering::XCanvas > xCanvas( mxCanvas );
    if( xCanvas.is() )
    {
        css::uno::Reference< css::lang::XComponent >  xCanvasComponent( xCanvas, css::uno::UNO_QUERY );
        if( xCanvasComponent.is() )
            xCanvasComponent->dispose();
    }
}

bool OutputDevice::CanDrawPolyline()
{
    if (!PrepareGraphicsOutput() || !mpGraphics)
        return false;

    return (GetRasterOp() == RasterOp::OverPaint && IsLineColor());
}

bool OutputDevice::CanDrawPolygon()
{
    if (!PrepareGraphicsOutput() || !mpGraphics)
        return false;

    return (GetRasterOp() == RasterOp::OverPaint && (IsLineColor() || IsFillColor()));
}

tools::Long OutputDevice::GetRTLFrameWidth() const
{
    return vcl::DispatchDevice(*this, [](auto& rConcreteDevice) -> tools::Long {
        using DeviceType = std::decay_t<decltype(rConcreteDevice)>;

        if constexpr (vcl::RTLCapableDevice<DeviceType>)
        {
            return rConcreteDevice.ImplGetRTLFrameWidth();
        }

        return 0; // Default for non-RTL-capable devices
    });
}

bool OutputDevice::IsPixelAreaOutOfBounds(tools::Long nX, tools::Long nY,
                                          tools::Long nWidth, tools::Long nHeight) const
{
    if (nWidth <= 0 || nHeight <= 0)
        return true;

    if (nX > (GetOutputWidthPixel() + GetOutOffXPixel()))
        return true;

    if (nY > (GetOutputHeightPixel() + GetOutOffYPixel()))
        return true;

    return false;
}

tools::Long OutputDevice::MirrorX(tools::Long nX, tools::Long nWidth) const
{
    // The logic: (OutputWidth - Width) - (RelativeX) + OffsetX
    return GetOutputWidthPixel() - nWidth - (nX - GetOutOffXPixel()) + GetOutOffXPixel();
}

bool OutputDevice::IsDoubleBuffered() const
{
    bool bRet = false;

    // Remove 'this' from the capture list []
    vcl::DispatchDevice(*this, [&bRet](auto& rDev) {
        using DevType = std::decay_t<decltype(rDev)>;

        if constexpr (vcl::DoubleBuffered<DevType>)
        {
            bRet = rDev.IsDoubleBufferedWindow();
        }
        else
        {
            bRet = false;
        }
    });

    return bRet;
}

bool OutputDevice::is_double_buffered_window() const
{
    return IsDoubleBuffered();
}

bool OutputDevice::CanEnableNativeWidget() const
{
    return vcl::DispatchDevice(*this, [](auto& rConcreteDevice) -> bool {
        using DeviceType = std::decay_t<decltype(rConcreteDevice)>;

        if constexpr (vcl::NativeWidgetCapable<DeviceType>)
        {
            // Only Window and VirtualDevice reach here.
            // Printers are compiled as 'return false'.
            return rConcreteDevice.ImplCanEnableNativeWidget();
        }

        return false;
    });
}

void OutputDevice::Flush(const tools::Rectangle& rRect)
{
    vcl::DispatchDevice(*this, [&rRect](auto& rConcreteDevice) {
        using DeviceType = std::decay_t<decltype(rConcreteDevice)>;

        if constexpr (vcl::FlushableDevice<DeviceType>)
        {
            rConcreteDevice.ImplFlush(rRect);
        }
    });
}

bool OutputDevice::HasMemoryBackend() const
{
    return vcl::DispatchDevice(*this, [](auto& rConcreteDevice) {
        using DeviceType = std::decay_t<decltype(rConcreteDevice)>;

        return vcl::StoredBitDepthDevice<DeviceType>;
    });
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
