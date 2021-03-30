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
    mpPrevGraphics = nullptr;
    mpNextGraphics = nullptr;
    mpMetaFile = nullptr;
    meOutDevViewType = OutDevViewType::DontKnow;
}

OutputDevice::~OutputDevice() { disposeOnce(); }

void OutputDevice::dispose()
{
    mpPrevGraphics.clear();
    mpNextGraphics.clear();
}

void OutputDevice::SetConnectMetaFile(GDIMetaFile* pMtf) { mpMetaFile = pMtf; }

// Helper public function

bool OutputDevice::SupportsOperation(OutDevSupportType eType) const
{
    if (!mpGraphics && !AcquireGraphics())
        return false;
    const bool bHasSupport = mpGraphics->supportsOperation(eType);
    return bHasSupport;
}

// Direct OutputDevice drawing public functions

void OutputDevice::DrawOutDev(Point const& rDestPt, Size const& rDestSize, Point const& rSrcPt,
                              Size const& rSrcSize)
{
    if (ImplIsRecordLayout())
        return;

    if (mpMetaFile)
    {
        if (meRasterOp == RasterOp::Invert)
        {
            mpMetaFile->AddAction(new MetaRectAction(tools::Rectangle(rDestPt, rDestSize)));
            return;
        }

        const Bitmap aBmp(GetBitmap(rSrcPt, rSrcSize));
        mpMetaFile->AddAction(new MetaBmpScaleAction(rDestPt, rDestSize, aBmp));
    }

    RenderContext2::DrawOutDev(rDestPt, rDestSize, rSrcPt, rSrcSize);
}

void OutputDevice::DrawOutDev(Point const& rDestPt, Size const& rDestSize, Point const& rSrcPt,
                              Size const& rSrcSize, RenderContext2 const& rOutDev)
{
    if (ImplIsRecordLayout())
        return;

    if (mpMetaFile)
    {
        if (meRasterOp == RasterOp::Invert)
        {
            mpMetaFile->AddAction(new MetaRectAction(tools::Rectangle(rDestPt, rDestSize)));
            return;
        }

        if (rOutDev.UsesAlphaVDev())
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

    RenderContext2::DrawOutDev(rDestPt, rDestSize, rSrcPt, rSrcSize, rOutDev);
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
