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

#include <tools/gen.hxx>
#include <tools/long.hxx>

#include <vcl/dllapi.h>

class VCL_DLLPUBLIC Geometry
{
public:
    Geometry();

    bool IsMapModeEnabled() const;
    void EnableMapMode(bool bEnable = true);

    Size GetSizeInPixels() const;
    tools::Long GetWidthInPixels() const;
    tools::Long GetHeightInPixels() const;
    void SetSizeInPixels(Size const& rSize);
    void SetWidthInPixels(tools::Long nWidth);
    void SetHeightInPixels(tools::Long nHeight);

    tools::Long GetXOffsetFromOriginInPixels() const;
    void SetXOffsetFromOriginInPixels(tools::Long nOffsetFromOriginXpx);
    tools::Long GetYOffsetFromOriginInPixels() const;
    void SetYOffsetFromOriginInPixels(tools::Long nOffsetFromOriginYpx);

    tools::Long GetXOffsetFromOriginInLogicalUnits() const;
    void SetXOffsetFromOriginInLogicalUnits(tools::Long nOffsetFromOriginXInLogicalUnits);
    tools::Long GetYOffsetFromOriginInLogicalUnits() const;
    void SetYOffsetFromOriginInLogicalUnits(tools::Long nOffsetFromOriginYInLogicalUnits);

    tools::Long GetXOffsetInPixels() const;
    void SetXOffsetInPixels(tools::Long nOffsetXpx);
    tools::Long GetYOffsetInPixels() const;
    void SetYOffsetInPixels(tools::Long nOffsetYpx);

    sal_Int32 GetDPIX() const;
    sal_Int32 GetDPIY() const;
    void SetDPIX(sal_Int32 nDPIX);
    void SetDPIY(sal_Int32 nDPIY);
    float GetDPIScaleFactor() const;
    sal_Int32 GetDPIScalePercentage() const;
    void SetDPIScalePercentage(sal_Int32 nPercentage);

private:
    bool mbMap;

    tools::Long mnOutWidth;
    tools::Long mnOutHeight;
    tools::Long mnOutOffOrigX;
    tools::Long mnOutOffOrigY;
    tools::Long mnOutOffLogicX;
    tools::Long mnOutOffLogicY;
    tools::Long
        mnOutOffX; /// Output offset for device output in pixels (pseudo window offset within window system's frames)
    tools::Long
        mnOutOffY; /// Output offset for device output in pixels (pseudo window offset within window system's frames)

    sal_Int32 mnDPIX;
    sal_Int32 mnDPIY;
    sal_Int32
        mnDPIScalePercentage; ///< For HiDPI displays, we want to draw elements for a percentage larger
};

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
