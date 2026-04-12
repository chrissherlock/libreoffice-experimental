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

#include <basegfx/matrix/b2dhommatrix.hxx>
#include <tools/gen.hxx>

#include <vcl/rendercontext/ImplMapRes.hxx>

#include <CoordinateMapper.hxx>

sal_Int32 CoordinateMapper::GetDPIX() const { return mnDPIX; }

sal_Int32 CoordinateMapper::GetDPIY() const { return mnDPIY; }

void CoordinateMapper::SetDPIX(sal_Int32 nDPIX) { mnDPIX = nDPIX; }

void CoordinateMapper::SetDPIY(sal_Int32 nDPIY) { mnDPIY = nDPIY; }

sal_Int32 CoordinateMapper::GetDPIScalePercentage() const { return mnDPIScalePercentage; }

void CoordinateMapper::SetDPIScalePercentage(sal_Int32 nPercent)
{
    mnDPIScalePercentage = nPercent;
}

float CoordinateMapper::GetDPIScaleFactor() const { return mnDPIScalePercentage / 100.0f; }

void CoordinateMapper::SetPixelOffset(const Size& rSize)
{
    mnOutOffOrigX = rSize.getWidth();
    mnOutOffOrigY = rSize.getHeight();
}

tools::Long CoordinateMapper::GetDeviceOriginX() const { return mnOutOffX; }

tools::Long CoordinateMapper::GetDeviceOriginY() const { return mnOutOffY; }

void CoordinateMapper::SetDeviceOriginX(tools::Long nOutOffX) { mnOutOffX = nOutOffX; }

void CoordinateMapper::SetOutOffYPixel(tools::Long nOutOffY) { mnOutOffY = nOutOffY; }

Point CoordinateMapper::GetOutputOffPixel() const { return Point(mnOutOffX, mnOutOffY); }

tools::Long CoordinateMapper::GetOutputWidthPixel() const { return mnOutWidth; }

tools::Long CoordinateMapper::GetOutputHeightPixel() const { return mnOutHeight; }

void CoordinateMapper::SetOutputWidthPixel(tools::Long nWidth) { mnOutWidth = nWidth; }

void CoordinateMapper::SetOutputHeightPixel(tools::Long nHeight) { mnOutHeight = nHeight; }

void CoordinateMapper::SetLogicalOffset(Size const& rOffset)
{
    mnOutOffLogicX = rOffset.getWidth();
    mnOutOffLogicY = rOffset.getHeight();
}

void CoordinateMapper::CalcMapResolution(const MapMode& rMapMode, tools::Long nDPIX,
                                         tools::Long nDPIY)
{
    maMapRes.CalcMapResolution(rMapMode, nDPIX, nDPIY);
}

ImplMapRes CoordinateMapper::ResolveMapRes(const MapMode* pMode)
{
    return maMapRes.ResolveMapRes(pMode, maMapMode, mbMap, mnDPIX, mnDPIY);
}

// #i75163#
void CoordinateMapper::InvalidateViewTransform()
{
    if (mpViewTransform)
    {
        delete mpViewTransform;
        mpViewTransform = nullptr;
    }

    if (mpInverseViewTransform)
    {
        delete mpInverseViewTransform;
        mpInverseViewTransform = nullptr;
    }
}

basegfx::B2DHomMatrix CoordinateMapper::GetDeviceTransformation() const
{
    basegfx::B2DHomMatrix aTransformation = GetViewTransformation();

    // TODO: is it worth to cache the transformed result?
    if (mnOutOffX || mnOutOffY)
        aTransformation.translate(mnOutOffX, mnOutOffY);

    return aTransformation;
}

basegfx::B2DHomMatrix CoordinateMapper::GetViewTransformation() const
{
    if (!IsMapModeEnabled())
        return basegfx::B2DHomMatrix();

    if (mpViewTransform)
        return *mpViewTransform;

    mpViewTransform = new basegfx::B2DHomMatrix;

    const double fScaleFactorX(static_cast<double>(GetDPIX()) * maMapRes.mfMapScX);
    const double fScaleFactorY(static_cast<double>(GetDPIY()) * maMapRes.mfMapScY);
    const double fZeroPointX((static_cast<double>(maMapRes.mnMapOfsX) * fScaleFactorX)
                             + static_cast<double>(GetPixelXOffset()));
    const double fZeroPointY((static_cast<double>(maMapRes.mnMapOfsY) * fScaleFactorY)
                             + static_cast<double>(GetPixelYOffset()));

    mpViewTransform->set(0, 0, fScaleFactorX);
    mpViewTransform->set(1, 1, fScaleFactorY);
    mpViewTransform->set(0, 2, fZeroPointX);
    mpViewTransform->set(1, 2, fZeroPointY);

    return *mpViewTransform;
}

basegfx::B2DHomMatrix CoordinateMapper::GetInverseViewTransformation() const
{
    if (!IsMapModeEnabled())
        return basegfx::B2DHomMatrix();

    if (mpInverseViewTransform)
        return *mpInverseViewTransform;

    GetViewTransformation();

    mpInverseViewTransform = new basegfx::B2DHomMatrix(*mpViewTransform);
    mpInverseViewTransform->invert();

    return *mpInverseViewTransform;
}

basegfx::B2DHomMatrix CoordinateMapper::GetViewTransformation(const MapMode& rMapMode) const
{
    // #i82615#
    ImplMapRes aMapRes;
    aMapRes.CalcMapResolution(rMapMode, GetDPIX(), GetDPIY());

    basegfx::B2DHomMatrix aTransform;

    const double fScaleFactorX(static_cast<double>(GetDPIX()) * aMapRes.mfMapScX);
    const double fScaleFactorY(static_cast<double>(GetDPIY()) * aMapRes.mfMapScY);
    const double fZeroPointX((static_cast<double>(aMapRes.mnMapOfsX) * fScaleFactorX)
                             + static_cast<double>(GetPixelXOffset()));
    const double fZeroPointY((static_cast<double>(aMapRes.mnMapOfsY) * fScaleFactorY)
                             + static_cast<double>(GetPixelYOffset()));

    aTransform.set(0, 0, fScaleFactorX);
    aTransform.set(1, 1, fScaleFactorY);
    aTransform.set(0, 2, fZeroPointX);
    aTransform.set(1, 2, fZeroPointY);

    return aTransform;
}

basegfx::B2DHomMatrix CoordinateMapper::GetInverseViewTransformation(const MapMode& rMapMode) const
{
    basegfx::B2DHomMatrix aMatrix(GetViewTransformation(rMapMode));
    aMatrix.invert();
    return aMatrix;
}

tools::Long CoordinateMapper::LogicToOffsetLogicX(tools::Long nX) const
{
    return nX + maMapRes.mnMapOfsX;
}

tools::Long CoordinateMapper::LogicToOffsetLogicY(tools::Long nY) const
{
    return nY + maMapRes.mnMapOfsY;
}

tools::Long CoordinateMapper::LogicToViewPixelX(tools::Long nX) const
{
    const double nViewLogicX = nX + maMapRes.mnMapOfsX;
    return std::llround(nViewLogicX * maMapRes.mfMapScX * mnDPIX);
}

tools::Long CoordinateMapper::LogicToViewPixelY(tools::Long nY) const
{
    const double nViewLogicY = nY + maMapRes.mnMapOfsY;
    return std::llround(nViewLogicY * maMapRes.mfMapScY * mnDPIY);
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
