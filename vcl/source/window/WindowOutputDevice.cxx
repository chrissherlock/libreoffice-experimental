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

#include <rtl/strbuf.hxx>
#include <sal/log.hxx>

#include <sal/types.h>
#include <comphelper/diagnose_ex.hxx>
#include <comphelper/OAccessible.hxx>
#include <vcl/dndlistenercontainer.hxx>
#include <vcl/salgtype.hxx>
#include <vcl/event.hxx>
#include <vcl/cursor.hxx>
#include <vcl/rendercontext/AntialiasingFlags.hxx>
#include <vcl/rendercontext/GetDefaultFontFlags.hxx>
#include <vcl/svapp.hxx>
#include <vcl/transfer.hxx>
#include <vcl/vclevent.hxx>
#include <vcl/window.hxx>
#include <vcl/syswin.hxx>
#include <vcl/dockwin.hxx>
#include <vcl/wall.hxx>
#include <vcl/toolkit/fixed.hxx>
#include <vcl/toolkit/button.hxx>
#include <vcl/taskpanelist.hxx>
#include <vcl/toolkit/unowrap.hxx>
#include <tools/lazydelete.hxx>
#include <vcl/virdev.hxx>
#include <vcl/settings.hxx>
#include <vcl/sysdata.hxx>
#include <vcl/ptrstyle.hxx>
#include <vcl/IDialogRenderable.hxx>
#include <vcl/uitest/uiobject.hxx>
#include <vcl/CoordinateMapper.hxx>
#include <vcl/MappingPolicy.hxx>

#include <ImplOutDevData.hxx>
#include <clipping_window.hxx>
#include <impfontcache.hxx>
#include <salframe.hxx>
#include <salobj.hxx>
#include <salinst.hxx>
#include <salgdi.hxx>
#include <svdata.hxx>
#include <window.h>
#include <toolbox.h>
#include <brdwin.hxx>
#include <helpwin.hxx>
#include <dndeventdispatcher.hxx>

#include <com/sun/star/accessibility/AccessibleRelation.hpp>
#include <com/sun/star/accessibility/AccessibleRole.hpp>
#include <com/sun/star/accessibility/AccessibleStateType.hpp>
#include <com/sun/star/accessibility/XAccessible.hpp>
#include <com/sun/star/accessibility/XAccessibleEditableText.hpp>
#include <com/sun/star/awt/XVclWindowPeer.hpp>
#include <com/sun/star/datatransfer/clipboard/XClipboard.hpp>
#include <com/sun/star/datatransfer/dnd/XDragGestureRecognizer.hpp>
#include <com/sun/star/datatransfer/dnd/XDropTarget.hpp>
#include <com/sun/star/rendering/CanvasFactory.hpp>
#include <com/sun/star/rendering/XSpriteCanvas.hpp>
#include <comphelper/configuration.hxx>
#include <comphelper/lok.hxx>
#include <comphelper/processfactory.hxx>
#include <osl/diagnose.h>
#include <tools/debug.hxx>
#include <tools/json_writer.hxx>
#include <unotools/fontdefs.hxx>
#include <boost/property_tree/ptree.hpp>

#include <cassert>
#include <typeinfo>

#ifdef _WIN32 // see #140456#
#include <win/salframe.h>
#endif

#include "impldockingwrapper.hxx"

using namespace ::com::sun::star::uno;
using namespace ::com::sun::star::lang;
using namespace ::com::sun::star::datatransfer::clipboard;
using namespace ::com::sun::star::datatransfer::dnd;

namespace vcl {

Color WindowOutputDevice::GetBackgroundColor() const
{
    return mxOwnerWindow->GetDisplayBackground().GetColor();
}

bool WindowOutputDevice::CanEnableNativeWidget() const
{
    return mxOwnerWindow->IsNativeWidgetEnabled();
}

bool WindowOutputDevice::AcquireGraphics() const
{
    DBG_TESTSOLARMUTEX();

    if (isDisposed())
        return false;

    if (mpGraphics)
        return true;

    mbInitFont          = true;
    mbInitTextColor     = true;
    maClipState.Invalidate();

    ImplSVData* pSVData = ImplGetSVData();

    mpGraphics = mxOwnerWindow->mpWindowImpl->mpFrame->AcquireGraphics();
    // try harder if no wingraphics was available directly
    if ( !mpGraphics )
    {
        // find another output device in the same frame
        vcl::WindowOutputDevice* pReleaseOutDev = pSVData->maGDIData.mpLastWinGraphics.get();
        while ( pReleaseOutDev )
        {
            if ( pReleaseOutDev->mxOwnerWindow && pReleaseOutDev->mxOwnerWindow->mpWindowImpl->mpFrame == mxOwnerWindow->mpWindowImpl->mpFrame )
                break;
            pReleaseOutDev = static_cast<vcl::WindowOutputDevice*>(pReleaseOutDev->mpPrevGraphics.get());
        }

        if ( pReleaseOutDev )
        {
            // steal the wingraphics from the other outdev
            mpGraphics = pReleaseOutDev->mpGraphics;
            pReleaseOutDev->ReleaseGraphics( false );
        }
        else
        {
            // if needed retry after releasing least recently used wingraphics
            while ( !mpGraphics )
            {
                if ( !pSVData->maGDIData.mpLastWinGraphics )
                    break;
                pSVData->maGDIData.mpLastWinGraphics->ReleaseGraphics();
                mpGraphics = mxOwnerWindow->mpWindowImpl->mpFrame->AcquireGraphics();
            }
        }
    }

    if ( mpGraphics )
    {
        // update global LRU list of wingraphics
        mpNextGraphics = pSVData->maGDIData.mpFirstWinGraphics.get();
        pSVData->maGDIData.mpFirstWinGraphics = const_cast<vcl::WindowOutputDevice*>(this);
        if ( mpNextGraphics )
            mpNextGraphics->mpPrevGraphics = const_cast<vcl::WindowOutputDevice*>(this);
        if ( !pSVData->maGDIData.mpLastWinGraphics )
            pSVData->maGDIData.mpLastWinGraphics = const_cast<vcl::WindowOutputDevice*>(this);

        mpGraphics->SetXORMode( (RasterOp::Invert == GetRasterOp()) || (RasterOp::Xor == GetRasterOp()), RasterOp::Invert == GetRasterOp() );
        mpGraphics->setAntiAlias(bool(mnAntialiasing & AntialiasingFlags::Enable));
    }

    // Force the pipeline to flush the window's expected state into the newly acquired (and potentially dirty) backend.
    ResetRenderStateSync();
    SyncRenderStateToBackend();

    return mpGraphics != nullptr;
}

void WindowOutputDevice::ReleaseGraphics( bool bRelease )
{
    DBG_TESTSOLARMUTEX();

    if ( !mpGraphics )
        return;

    // release the fonts of the physically released graphics device
    if( bRelease )
        ImplReleaseFonts();

    ImplSVData* pSVData = ImplGetSVData();

    vcl::Window* pWindow = mxOwnerWindow.get();
    if (!pWindow)
        return;

    if ( bRelease )
        pWindow->mpWindowImpl->mpFrame->ReleaseGraphics( mpGraphics );
    // remove from global LRU list of window graphics
    if ( mpPrevGraphics )
        mpPrevGraphics->mpNextGraphics = mpNextGraphics;
    else
        pSVData->maGDIData.mpFirstWinGraphics = static_cast<vcl::WindowOutputDevice*>(mpNextGraphics.get());
    if ( mpNextGraphics )
        mpNextGraphics->mpPrevGraphics = mpPrevGraphics;
    else
        pSVData->maGDIData.mpLastWinGraphics = static_cast<vcl::WindowOutputDevice*>(mpPrevGraphics.get());

    mpGraphics      = nullptr;
    mpPrevGraphics  = nullptr;
    mpNextGraphics  = nullptr;
}

void WindowOutputDevice::CopyDeviceArea( SalTwoRect& aPosAry )
{
    if (aPosAry.mnSrcWidth == 0 || aPosAry.mnSrcHeight == 0 || aPosAry.mnDestWidth == 0 || aPosAry.mnDestHeight == 0)
        return;

    OutputDevice::CopyDeviceArea(aPosAry);
}

const OutputDevice* WindowOutputDevice::DrawOutDevDirectCheck(const OutputDevice& rSrcDev) const
{
    const OutputDevice* pSrcDevChecked;
    if ( this == &rSrcDev )
        pSrcDevChecked = nullptr;
    else if (GetOutDevType() != rSrcDev.GetOutDevType())
        pSrcDevChecked = &rSrcDev;
    else if (mxOwnerWindow->mpWindowImpl->mpFrameWindow == static_cast<const vcl::WindowOutputDevice&>(rSrcDev).mxOwnerWindow->mpWindowImpl->mpFrameWindow)
        pSrcDevChecked = nullptr;
    else
        pSrcDevChecked = &rSrcDev;

    return pSrcDevChecked;
}

void WindowOutputDevice::DrawOutDevDirectProcess( const OutputDevice& rSrcDev, SalTwoRect& rPosAry, SalGraphics* pSrcGraphics )
{
    if (pSrcGraphics)
        mpGraphics->CopyBits(rPosAry, *pSrcGraphics, *this, rSrcDev);
    else
        mpGraphics->CopyBits(rPosAry, *this);
}

void WindowOutputDevice::Flush()
{
    if (mxOwnerWindow->mpWindowImpl)
        mxOwnerWindow->mpWindowImpl->mpFrame->Flush( GetOutputRectPixel() );
}

Reference< css::rendering::XCanvas > WindowOutputDevice::ImplGetCanvas( bool bSpriteCanvas ) const
{
    // Feed any with operating system's window handle

    // common: first any is VCL pointer to window (for VCL canvas)
    Sequence< Any > aArg{
        Any(reinterpret_cast<sal_Int64>(this)),
        Any(css::awt::Rectangle( GetDeviceOriginX(), GetDeviceOriginY(), GetOutputWidthPixel(), GetOutputHeightPixel() )),
        Any(mxOwnerWindow->mpWindowImpl->mbAlwaysOnTop),
        Any(Reference< css::awt::XWindow >(
                             mxOwnerWindow->GetComponentInterface(),
                             UNO_QUERY )),
        GetSystemGfxDataAny()
    };

    const Reference< XComponentContext >& xContext = comphelper::getProcessComponentContext();

    // Create canvas instance with window handle

    static tools::DeleteUnoReferenceOnDeinit<XMultiComponentFactory> xStaticCanvasFactory(
        css::rendering::CanvasFactory::create( xContext ) );
    Reference<XMultiComponentFactory> xCanvasFactory(xStaticCanvasFactory.get());
    Reference< css::rendering::XCanvas > xCanvas;

    if(xCanvasFactory.is())
    {
#ifdef _WIN32
        // see #140456# - if we're running on a multiscreen setup,
        // request special, multi-screen safe sprite canvas
        // implementation (not DX5 canvas, as it cannot cope with
        // surfaces spanning multiple displays). Note: canvas
        // (without sprite) stays the same)
        const sal_uInt32 nDisplay = static_cast< WinSalFrame* >( mxOwnerWindow->mpWindowImpl->mpFrame )->mnDisplay;
        if( nDisplay >= Application::GetScreenCount() )
        {
            xCanvas.set( xCanvasFactory->createInstanceWithArgumentsAndContext(
                                 bSpriteCanvas ?
                                 OUString( "com.sun.star.rendering.SpriteCanvas.MultiScreen" ) :
                                 OUString( "com.sun.star.rendering.Canvas.MultiScreen" ),
                                 aArg,
                                 xContext ),
                             UNO_QUERY );

        }
        else
#endif
        {
            xCanvas.set( xCanvasFactory->createInstanceWithArgumentsAndContext(
                             bSpriteCanvas ?
                             u"com.sun.star.rendering.SpriteCanvas"_ustr :
                             u"com.sun.star.rendering.Canvas"_ustr,
                             aArg,
                             xContext ),
                         UNO_QUERY );

        }
    }

    // no factory??? Empty reference, then.
    return xCanvas;
}

bool WindowOutputDevice::UsePolyPolygonForComplexGradient()
{
    return GetRasterOp() != RasterOp::OverPaint;
}

WindowOutputDevice::WindowOutputDevice(vcl::Window& rOwnerWindow) :
    ::OutputDevice(OUTDEV_WINDOW),
    mxOwnerWindow(&rOwnerWindow)
{
    assert(mxOwnerWindow);
}

WindowOutputDevice::~WindowOutputDevice()
{
    disposeOnce();
}

void WindowOutputDevice::dispose()
{
    assert((!mxOwnerWindow || mxOwnerWindow->isDisposed()) && "This belongs to the associated window and must be disposed after it");
    ::OutputDevice::dispose();
    // need to do this after OutputDevice::dispose so that the call to WindowOutputDevice::ReleaseGraphics
    // can release the graphics properly
    mxOwnerWindow.reset();
}

css::awt::DeviceInfo WindowOutputDevice::GetDeviceInfo() const
{
    css::awt::DeviceInfo aInfo = GetCommonDeviceInfo(mxOwnerWindow->GetSizePixel());
    mxOwnerWindow->GetBorder(aInfo.LeftInset, aInfo.TopInset, aInfo.RightInset, aInfo.BottomInset);
    return aInfo;
}

void WindowOutputDevice::ImplClearFontData(bool bNewFontLists)
{
    OutputDevice::ImplClearFontData(bNewFontLists);
    for (Window* pChild = mxOwnerWindow->mpWindowImpl->mpHierarchy->mpFirstChild; pChild;
         pChild = pChild->mpWindowImpl->mpHierarchy->mpNext)
        pChild->GetOutDev()->ImplClearFontData(bNewFontLists);
}

void WindowOutputDevice::ImplRefreshFontData(bool bNewFontLists)
{
    OutputDevice::ImplRefreshFontData(bNewFontLists);
    for (Window* pChild = mxOwnerWindow->mpWindowImpl->mpHierarchy->mpFirstChild; pChild;
         pChild = pChild->mpWindowImpl->mpHierarchy->mpNext)
        pChild->GetOutDev()->ImplRefreshFontData(bNewFontLists);
}

void WindowOutputDevice::ImplInitMapModeObjects()
{
    OutputDevice::ImplInitMapModeObjects();
    if (mxOwnerWindow->mpWindowImpl->mpCursor)
        mxOwnerWindow->mpWindowImpl->mpCursor->ImplNew();
}

vcl::Region WindowOutputDevice::GetOutputBoundsClipRegion() const
{
    vcl::Region aClip(GetClipRegion());
    aClip.Intersect(tools::Rectangle(Point(), GetOutputSize()));

    return aClip;
}

void WindowOutputDevice::SaveBackground(VirtualDevice& rSaveDevice, const Point& rPos, const Size& rSize, const Size&) const
{
    comphelper::ScopeGuard aResetMapMode([&rSaveDevice]() { rSaveDevice.SetMapMode(MapMode()); });

    if (!mxOwnerWindow || !mxOwnerWindow->mpWindowImpl || !mxOwnerWindow->mpWindowImpl->mpPaintRegion)
    {
        rSaveDevice.DrawOutDev(Point(), rSize, rPos, rSize, *this);
        return;
    }

    vcl::Region aClip(*mxOwnerWindow->mpWindowImpl->mpPaintRegion);
    aClip.Move(-GetDeviceOriginX(), -GetDeviceOriginY());

    const auto boundRect = convertTo<vcl::WindowRect>(vcl::LogicRect(tools::Rectangle(rPos, rSize)), GetMapMode());

    aClip.Intersect(boundRect.get());

    if (aClip.IsEmpty())
        return;

    const vcl::Region aOldClip(rSaveDevice.GetClipRegion());
    const vcl::MappingPolicy eOldPolicy = rSaveDevice.GetMappingPolicy();

    comphelper::ScopeGuard aDeviceGuard([&rSaveDevice, aOldClip, eOldPolicy]() {
        rSaveDevice.SetMappingPolicy(eOldPolicy);
        rSaveDevice.SetClipRegion(aOldClip);
    });

    const auto aPixPos = convertTo<vcl::WindowPoint>(vcl::LogicPoint(rPos), GetMapMode());
    const auto aPixOffset = rSaveDevice.convertTo<vcl::WindowPoint>(vcl::LogicPoint(0, 0), rSaveDevice.GetMapMode());

    // Move clip region to have the same distance to DestOffset
    aClip.Move(aPixOffset->X() - aPixPos->X(), aPixOffset->Y() - aPixPos->Y());

    // Set pixel clip region
    rSaveDevice.SetMappingPolicy(vcl::MappingPolicy::IgnoreMapMode);
    rSaveDevice.SetClipRegion(aClip);

    rSaveDevice.DrawOutDev(Point(), rSize, rPos, rSize, *this);
}
} /* namespace vcl */

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
