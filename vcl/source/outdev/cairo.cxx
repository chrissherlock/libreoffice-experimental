/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
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

#if ENABLE_CAIRO_CANVAS

bool OutputDevice::SupportsCairo() const
{
    if (!mpGraphics && !AcquireGraphics())
        return false;

    assert(mpGraphics);

    return mpGraphics->SupportsCairo();
}

cairo::SurfaceSharedPtr
OutputDevice::CreateSurface(const cairo::CairoSurfaceSharedPtr& rSurface) const
{
    if (!mpGraphics && !AcquireGraphics())
        return cairo::SurfaceSharedPtr();
    return mpGraphics->CreateSurface(rSurface);
}

cairo::SurfaceSharedPtr OutputDevice::CreateSurface(int x, int y, int width, int height) const
{
    if (!mpGraphics && !AcquireGraphics())
        return cairo::SurfaceSharedPtr();

    assert(mpGraphics);

    return mpGraphics->CreateSurface(*this, x, y, width, height);
}

cairo::SurfaceSharedPtr OutputDevice::CreateBitmapSurface(const BitmapSystemData& rData,
                                                          const Size& rSize) const
{
    if (!mpGraphics && !AcquireGraphics())
        return cairo::SurfaceSharedPtr();

    assert(mpGraphics);

    return mpGraphics->CreateBitmapSurface(*this, rData, rSize);
}

css::uno::Any OutputDevice::GetNativeSurfaceHandle(cairo::SurfaceSharedPtr& rSurface,
                                                   const basegfx::B2ISize& rSize) const
{
    if (!mpGraphics && !AcquireGraphics())
        return css::uno::Any();

    assert(mpGraphics);

    return mpGraphics->GetNativeSurfaceHandle(rSurface, rSize);
}

#endif // ENABLE_CAIRO_CANVAS

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
