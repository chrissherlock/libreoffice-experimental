/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <vcl/RenderContext2.hxx>

#include <com/sun/star/awt/DeviceCapability.hpp>

css::awt::DeviceInfo RenderContext2::GetCommonDeviceInfo(Size const& rDevSz) const
{
    css::awt::DeviceInfo aInfo;

    aInfo.Width = rDevSz.Width();
    aInfo.Height = rDevSz.Height();

    Size aTmpSz = LogicToPixel(Size(1000, 1000), MapMode(MapUnit::MapCM));
    aInfo.PixelPerMeterX = aTmpSz.Width() / 10;
    aInfo.PixelPerMeterY = aTmpSz.Height() / 10;
    aInfo.BitsPerPixel = GetBitCount();

    aInfo.Capabilities
        = css::awt::DeviceCapability::RASTEROPERATIONS | css::awt::DeviceCapability::GETBITS;

    return aInfo;
}

css::awt::DeviceInfo RenderContext2::GetDeviceInfo() const
{
    css::awt::DeviceInfo aInfo = GetCommonDeviceInfo(GetOutputSizePixel());

    aInfo.LeftInset = 0;
    aInfo.TopInset = 0;
    aInfo.RightInset = 0;
    aInfo.BottomInset = 0;

    return aInfo;
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
