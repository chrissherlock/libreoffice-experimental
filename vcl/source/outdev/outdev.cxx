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

#include <tools/debug.hxx>
#include <vcl/gdimtf.hxx>
#include <vcl/graph.hxx>
#include <vcl/metaact.hxx>
#include <vcl/virdev.hxx>
#include <vcl/outdev.hxx>
#include <vcl/toolkit/unowrap.hxx>
#include <vcl/svapp.hxx>
#include <vcl/sysdata.hxx>

#include <outdev.h>
#include <window.h>
#include <ImplOutDevData.hxx>
#include <salgdi.hxx>

#include <com/sun/star/awt/DeviceCapability.hpp>

#ifdef DISABLE_DYNLOADING
// Linking all needed LO code into one .so/executable, these already
// exist in the tools library, so put them in the anonymous namespace
// here to avoid clash...
namespace
{
#endif
#ifdef DISABLE_DYNLOADING
}
#endif

// Begin initializer and accessor public functions

OutputDevice::OutputDevice(OutDevType eOutDevType)
    : meOutDevType(eOutDevType)
{
    mpUnoGraphicsList = nullptr;
    mpPrevGraphics = nullptr;
    mpNextGraphics = nullptr;
    mpMetaFile = nullptr;
    mpExtOutDevData = nullptr;
    meOutDevViewType = OutDevViewType::DontKnow;
}

OutputDevice::~OutputDevice() { disposeOnce(); }

void OutputDevice::dispose()
{
    if (GetUnoGraphicsList())
    {
        UnoWrapperBase* pWrapper = UnoWrapperBase::GetUnoWrapper(false);
        if (pWrapper)
            pWrapper->ReleaseAllGraphics(this);
        delete mpUnoGraphicsList;
        mpUnoGraphicsList = nullptr;
    }

    mpPrevGraphics.clear();
    mpNextGraphics.clear();
}

void OutputDevice::SetConnectMetaFile(GDIMetaFile* pMtf) { mpMetaFile = pMtf; }

SystemGraphicsData OutputDevice::GetSystemGfxData() const
{
    if (!mpGraphics && !AcquireGraphics())
        return SystemGraphicsData();
    assert(mpGraphics);

    return mpGraphics->GetGraphicsData();
}

css::uno::Any OutputDevice::GetSystemGfxDataAny() const
{
    const SystemGraphicsData aSysData = GetSystemGfxData();
    css::uno::Sequence<sal_Int8> aSeq(reinterpret_cast<sal_Int8 const*>(&aSysData), aSysData.nSize);

    return css::uno::makeAny(aSeq);
}

css::uno::Reference<css::awt::XGraphics> OutputDevice::CreateUnoGraphics()
{
    UnoWrapperBase* pWrapper = UnoWrapperBase::GetUnoWrapper();
    return pWrapper ? pWrapper->CreateGraphics(this) : css::uno::Reference<css::awt::XGraphics>();
}

std::vector<VCLXGraphics*>* OutputDevice::CreateUnoGraphicsList()
{
    mpUnoGraphicsList = new std::vector<VCLXGraphics*>;
    return mpUnoGraphicsList;
}

// Helper public function

bool OutputDevice::SupportsOperation(OutDevSupportType eType) const
{
    if (!mpGraphics && !AcquireGraphics())
        return false;
    const bool bHasSupport = mpGraphics->supportsOperation(eType);
    return bHasSupport;
}

// Direct OutputDevice drawing public functions

void OutputDevice::DrawOutDev(const Point& rDestPt, const Size& rDestSize, const Point& rSrcPt,
                              const Size& rSrcSize)
{
    if (ImplIsRecordLayout())
        return;

    if (RasterOp::Invert == meRasterOp)
    {
        DrawRect(tools::Rectangle(rDestPt, rDestSize));
        return;
    }

    if (mpMetaFile)
    {
        const Bitmap aBmp(GetBitmap(rSrcPt, rSrcSize));
        mpMetaFile->AddAction(new MetaBmpScaleAction(rDestPt, rDestSize, aBmp));
    }

    if (!IsDeviceOutputNecessary())
        return;

    if (!mpGraphics && !AcquireGraphics())
        return;

    assert(mpGraphics);

    if (mbInitClipRegion)
        InitClipRegion();

    if (mbOutputClipped)
        return;

    tools::Long nSrcWidth = ImplLogicWidthToDevicePixel(rSrcSize.Width());
    tools::Long nSrcHeight = ImplLogicHeightToDevicePixel(rSrcSize.Height());
    tools::Long nDestWidth = ImplLogicWidthToDevicePixel(rDestSize.Width());
    tools::Long nDestHeight = ImplLogicHeightToDevicePixel(rDestSize.Height());

    if (nSrcWidth && nSrcHeight && nDestWidth && nDestHeight)
    {
        SalTwoRect aPosAry(ImplLogicXToDevicePixel(rSrcPt.X()), ImplLogicYToDevicePixel(rSrcPt.Y()),
                           nSrcWidth, nSrcHeight, ImplLogicXToDevicePixel(rDestPt.X()),
                           ImplLogicYToDevicePixel(rDestPt.Y()), nDestWidth, nDestHeight);

        AdjustTwoRect(aPosAry, GetOutputRectPixel());

        if (aPosAry.mnSrcWidth && aPosAry.mnSrcHeight && aPosAry.mnDestWidth
            && aPosAry.mnDestHeight)
            mpGraphics->CopyBits(aPosAry, *this);
    }

    if (mpAlphaVDev)
        mpAlphaVDev->DrawOutDev(rDestPt, rDestSize, rSrcPt, rSrcSize);
}

void OutputDevice::DrawOutDev(const Point& rDestPt, const Size& rDestSize, const Point& rSrcPt,
                              const Size& rSrcSize, const OutputDevice& rOutDev)
{
    if (ImplIsRecordLayout())
        return;

    if (RasterOp::Invert == meRasterOp)
    {
        DrawRect(tools::Rectangle(rDestPt, rDestSize));
        return;
    }

    if (mpMetaFile)
    {
        if (rOutDev.mpAlphaVDev)
        {
            const BitmapEx aBmpEx(rOutDev.GetBitmapEx(rSrcPt, rSrcSize));
            mpMetaFile->AddAction(new MetaBmpExScaleAction(rDestPt, rDestSize, aBmpEx));
        }
        else
        {
            const Bitmap aBmp(rOutDev.GetBitmap(rSrcPt, rSrcSize));
            mpMetaFile->AddAction(new MetaBmpScaleAction(rDestPt, rDestSize, aBmp));
        }
    }

    if (!IsDeviceOutputNecessary())
        return;

    if (!mpGraphics && !AcquireGraphics())
        return;

    assert(mpGraphics);

    if (mbInitClipRegion)
        InitClipRegion();

    if (mbOutputClipped)
        return;

    if (rOutDev.mpAlphaVDev)
    {
        // alpha-blend source over destination
        DrawBitmapEx(rDestPt, rDestSize, rOutDev.GetBitmapEx(rSrcPt, rSrcSize));
    }
    else
    {
        SalTwoRect aPosAry(rOutDev.ImplLogicXToDevicePixel(rSrcPt.X()),
                           rOutDev.ImplLogicYToDevicePixel(rSrcPt.Y()),
                           rOutDev.ImplLogicWidthToDevicePixel(rSrcSize.Width()),
                           rOutDev.ImplLogicHeightToDevicePixel(rSrcSize.Height()),
                           ImplLogicXToDevicePixel(rDestPt.X()),
                           ImplLogicYToDevicePixel(rDestPt.Y()),
                           ImplLogicWidthToDevicePixel(rDestSize.Width()),
                           ImplLogicHeightToDevicePixel(rDestSize.Height()));

        drawOutDevDirect(rOutDev, aPosAry);

        // #i32109#: make destination rectangle opaque - source has no alpha
        if (mpAlphaVDev)
            mpAlphaVDev->ImplFillOpaqueRectangle(tools::Rectangle(rDestPt, rDestSize));
    }
}

void OutputDevice::CopyArea(const Point& rDestPt, const Point& rSrcPt, const Size& rSrcSize,
                            bool bWindowInvalidate)
{
    if (ImplIsRecordLayout())
        return;

    RasterOp eOldRop = GetRasterOp();
    SetRasterOp(RasterOp::OverPaint);

    if (!IsDeviceOutputNecessary())
        return;

    if (!mpGraphics && !AcquireGraphics())
        return;

    assert(mpGraphics);

    if (mbInitClipRegion)
        InitClipRegion();

    if (mbOutputClipped)
        return;

    tools::Long nSrcWidth = ImplLogicWidthToDevicePixel(rSrcSize.Width());
    tools::Long nSrcHeight = ImplLogicHeightToDevicePixel(rSrcSize.Height());
    if (nSrcWidth && nSrcHeight)
    {
        SalTwoRect aPosAry(ImplLogicXToDevicePixel(rSrcPt.X()), ImplLogicYToDevicePixel(rSrcPt.Y()),
                           nSrcWidth, nSrcHeight, ImplLogicXToDevicePixel(rDestPt.X()),
                           ImplLogicYToDevicePixel(rDestPt.Y()), nSrcWidth, nSrcHeight);

        AdjustTwoRect(aPosAry, GetOutputRectPixel());

        CopyDeviceArea(aPosAry, bWindowInvalidate);
    }

    SetRasterOp(eOldRop);

    if (mpAlphaVDev)
        mpAlphaVDev->CopyArea(rDestPt, rSrcPt, rSrcSize, bWindowInvalidate);
}

// Direct OutputDevice drawing protected function

void OutputDevice::CopyDeviceArea(SalTwoRect& aPosAry, bool /*bWindowInvalidate*/)
{
    if (aPosAry.mnSrcWidth == 0 || aPosAry.mnSrcHeight == 0 || aPosAry.mnDestWidth == 0
        || aPosAry.mnDestHeight == 0)
        return;

    aPosAry.mnDestWidth = aPosAry.mnSrcWidth;
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

    AdjustTwoRect(rPosAry, rSrcDev.GetOutputRectPixel());

    if (rPosAry.mnSrcWidth && rPosAry.mnSrcHeight && rPosAry.mnDestWidth && rPosAry.mnDestHeight)
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

void OutputDevice::DrawOutDevDirectProcess(const OutputDevice& rSrcDev, SalTwoRect& rPosAry,
                                           SalGraphics* pSrcGraphics)
{
    if (pSrcGraphics && (pSrcGraphics->GetLayout() & SalLayoutFlags::BiDiRtl))
    {
        SalTwoRect aPosAry2 = rPosAry;
        pSrcGraphics->mirror(aPosAry2.mnSrcX, aPosAry2.mnSrcWidth, rSrcDev);
        mpGraphics->CopyBits(aPosAry2, *pSrcGraphics, *this, rSrcDev);
        return;
    }
    if (pSrcGraphics)
        mpGraphics->CopyBits(rPosAry, *pSrcGraphics, *this, rSrcDev);
    else
        mpGraphics->CopyBits(rPosAry, *this);
}

tools::Rectangle OutputDevice::GetBackgroundComponentBounds() const
{
    return tools::Rectangle(Point(0, 0), GetOutputSizePixel());
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
