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

#include <vcl/RenderContext2.hxx>
#include <vcl/ViewTransformer.hxx>

// #i75163#
basegfx::B2DHomMatrix RenderContext2::GetInverseViewTransformation() const
{
    if (IsMapModeEnabled() && mpViewTransformer)
    {
        if (!mpViewTransformer->mpInverseViewTransform)
        {
            GetViewTransformation();
            mpViewTransformer->mpInverseViewTransform
                = new basegfx::B2DHomMatrix(*mpViewTransformer->mpViewTransform);
            mpViewTransformer->mpInverseViewTransform->invert();
        }

        return *mpViewTransformer->mpInverseViewTransform;
    }
    else
    {
        return basegfx::B2DHomMatrix();
    }
}

// #i75163#
basegfx::B2DHomMatrix RenderContext2::GetViewTransformation() const
{
    if (IsMapModeEnabled() && mpViewTransformer)
    {
        if (!mpViewTransformer->mpViewTransform)
        {
            mpViewTransformer->mpViewTransform = new basegfx::B2DHomMatrix;

            const double fScaleFactorX(static_cast<double>(GetDPIX())
                                       * static_cast<double>(GetXMapNumerator())
                                       / static_cast<double>(GetXMapDenominator()));
            const double fScaleFactorY(static_cast<double>(GetDPIY())
                                       * static_cast<double>(GetYMapNumerator())
                                       / static_cast<double>(GetYMapDenominator()));
            const double fZeroPointX((static_cast<double>(GetXMapOffset()) * fScaleFactorX)
                                     + static_cast<double>(GetXOffsetFromOriginInPixels()));
            const double fZeroPointY((static_cast<double>(GetYMapOffset()) * fScaleFactorY)
                                     + static_cast<double>(GetYOffsetFromOriginInPixels()));

            mpViewTransformer->mpViewTransform->set(0, 0, fScaleFactorX);
            mpViewTransformer->mpViewTransform->set(1, 1, fScaleFactorY);
            mpViewTransformer->mpViewTransform->set(0, 2, fZeroPointX);
            mpViewTransformer->mpViewTransform->set(1, 2, fZeroPointY);
        }

        return *mpViewTransformer->mpViewTransform;
    }
    else
    {
        return basegfx::B2DHomMatrix();
    }
}

// #i75163#
basegfx::B2DHomMatrix RenderContext2::GetViewTransformation(MapMode const& rMapMode) const
{
    // #i82615#
    MappingMetrics aMappingMetric(rMapMode, GetDPIX(), GetDPIY());

    basegfx::B2DHomMatrix aTransform;

    const double fScaleFactorX(static_cast<double>(GetDPIX())
                               * static_cast<double>(aMappingMetric.mnMapScNumX)
                               / static_cast<double>(aMappingMetric.mnMapScDenomX));
    const double fScaleFactorY(static_cast<double>(GetDPIY())
                               * static_cast<double>(aMappingMetric.mnMapScNumY)
                               / static_cast<double>(aMappingMetric.mnMapScDenomY));
    const double fZeroPointX((static_cast<double>(aMappingMetric.mnMapOfsX) * fScaleFactorX)
                             + static_cast<double>(GetXOffsetFromOriginInPixels()));
    const double fZeroPointY((static_cast<double>(aMappingMetric.mnMapOfsY) * fScaleFactorY)
                             + static_cast<double>(GetYOffsetFromOriginInPixels()));

    aTransform.set(0, 0, fScaleFactorX);
    aTransform.set(1, 1, fScaleFactorY);
    aTransform.set(0, 2, fZeroPointX);
    aTransform.set(1, 2, fZeroPointY);

    return aTransform;
}

// #i75163#
basegfx::B2DHomMatrix RenderContext2::GetInverseViewTransformation(MapMode const& rMapMode) const
{
    basegfx::B2DHomMatrix aMatrix(GetViewTransformation(rMapMode));
    aMatrix.invert();
    return aMatrix;
}

basegfx::B2DHomMatrix RenderContext2::GetDeviceTransformation() const
{
    basegfx::B2DHomMatrix aTransformation = GetViewTransformation();

    // TODO: is it worth to cache the transformed result?
    if (GetXOffsetInPixels() || GetYOffsetInPixels())
        aTransformation.translate(GetXOffsetInPixels(), GetYOffsetInPixels());

    return aTransformation;
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
