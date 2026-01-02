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

#include <sal/types.h>
#include <tools/gen.hxx>
#include <tools/fract.hxx>
#include <tools/long.hxx>

#include <vcl/dllapi.h>
#include <vcl/mapmod.hxx>

class CoordinateMapper
{
private:
    bool mbMap;
    MapMode maMapMode;

    sal_Int32 mnDPIX;
    sal_Int32 mnDPIY;
    sal_Int32 mnDPIScalePercentage = 100;

    tools::Long mnOutWidth;
    tools::Long mnOutHeight;

    /// Output offset for device output in pixel (pseudo window offset within window system's frames)
    tools::Long mnOutOffX;
    /// Output offset for device output in pixel (pseudo window offset within window system's frames)
    tools::Long mnOutOffY;

public:
    SAL_DLLPRIVATE bool IsMapModeEnabled() const { return mbMap; }
    SAL_DLLPRIVATE void EnableMapMode(bool bEnable = true) { mbMap = bEnable; }

    SAL_DLLPRIVATE const MapMode& GetMapMode() const { return maMapMode; }
    SAL_DLLPRIVATE bool IsDefaultMapMode() const { return maMapMode.IsDefault(); }
    SAL_DLLPRIVATE void ResetMapMode() { maMapMode = MapMode(); }
    SAL_DLLPRIVATE void ResetMapMode(const MapMode& rMapMode) { maMapMode = rMapMode; }
    SAL_DLLPRIVATE MapUnit GetMapUnit() const { return maMapMode.GetMapUnit(); }

    SAL_DLLPRIVATE const Fraction& GetScaleX() const { return maMapMode.GetScaleX(); }
    SAL_DLLPRIVATE const Fraction& GetScaleY() const { return maMapMode.GetScaleY(); }
    SAL_DLLPRIVATE void SetScaleX(const Fraction& rScale) { maMapMode.SetScaleX(rScale); }
    SAL_DLLPRIVATE void SetScaleY(const Fraction& rScale) { maMapMode.SetScaleY(rScale); }

    SAL_DLLPRIVATE void SetOrigin(const Point& rPt) { maMapMode.SetOrigin(rPt); }

    SAL_DLLPRIVATE sal_Int32 GetDPIX() const;
    SAL_DLLPRIVATE sal_Int32 GetDPIY() const;

    SAL_DLLPRIVATE tools::Long GetOutputWidthPixel() const;
    SAL_DLLPRIVATE tools::Long GetOutputHeightPixel() const;
    SAL_DLLPRIVATE Size GetOutputSizePixel() const;

    SAL_DLLPRIVATE void SetOutputWidthPixel(tools::Long nWidth);
    SAL_DLLPRIVATE void SetOutputHeightPixel(tools::Long nHeight);

    SAL_DLLPRIVATE void SetDPIX(sal_Int32 nDPIX);
    SAL_DLLPRIVATE void SetDPIY(sal_Int32 nDPIY);

    SAL_DLLPRIVATE sal_Int32 GetDPIScalePercentage() const;
    SAL_DLLPRIVATE void SetDPIScalePercentage(sal_Int32 nPercentage);

    SAL_DLLPRIVATE float GetDPIScaleFactor() const;

    SAL_DLLPRIVATE tools::Long GetOutOffXPixel() const;
    SAL_DLLPRIVATE tools::Long GetOutOffYPixel() const;

    SAL_DLLPRIVATE void SetOutOffXPixel(tools::Long nOutOffX);
    SAL_DLLPRIVATE void SetOutOffYPixel(tools::Long nOutOffY);

    SAL_DLLPRIVATE Point GetOutputOffPixel() const;
};

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
