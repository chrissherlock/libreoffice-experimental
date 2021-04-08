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

#pragma once

#include <vcl/bitmapex.hxx>
#include <vcl/metric.hxx>

#include <com/sun/star/awt/DeviceInfo.hpp>

class SalGraphics;

class DeviceInterface
{
public:
    virtual ~DeviceInterface() {}

    /** Get the graphic context that the output device uses to draw on.

     If no graphics device exists, then initialize it.

     @returns SalGraphics instance.
     */
    virtual SalGraphics const* GetGraphics() const = 0;
    virtual SalGraphics* GetGraphics() = 0;

    virtual css::awt::DeviceInfo GetDeviceInfo() const = 0;

    // TODO: implement function to get list of fonts available in system
    virtual FontMetric GetFontMetric(int nDevFontIndex) const = 0;

    virtual Bitmap GetBitmap(Point const& rSrcPt, Size const& rSize) const = 0;

    /** Query extended bitmap (with alpha channel, if available).
     */
    virtual BitmapEx GetBitmapEx(Point const& rSrcPt, Size const& rSize) const = 0;
};

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
