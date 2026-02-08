/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <tools/fontenum.hxx>
#include <tools/gen.hxx>
#include <tools/long.hxx>
#include <tools/poly.hxx>
#include <basegfx/point/b2dpoint.hxx>

#include <vcl/dllapi.h>
#include <vcl/fntstyle.hxx>
#include <vcl/vclenum.hxx>
#include <vcl/rendercontext/DrawTextFlags.hxx>
#include <vcl/kernarray.hxx>

#include <text/MnemonicGeometry.hxx>

#include <functional>
#include <vector>

class Point;

namespace vcl::text
{
struct RotatedGeometry
{
    bool mbIsPolygon;
    tools::Rectangle maRect; // Optimization for 0, 90, 180, 270 deg
    tools::Polygon maPoly; // Fallback for arbitrary angles
};

struct MirroringContext
{
    tools::Long nX; // The original X coordinate
    tools::Long nGraphicsWidth; // Width of the underlying graphics
    tools::Long nOutputWidth; // Width of the OutputDevice
    tools::Long nOutOffX; // X Offset of the OutputDevice
    bool bHasMirroredGraphics;
    bool bIsRTL;
};

class VCL_DLLPUBLIC TextGeometry
{
public:
    static tools::Rectangle AlignAndRotateTextRect(const tools::Rectangle& rTargetRect,
                                                   tools::Long nContentWidth,
                                                   tools::Long nContentHeight, DrawTextFlags nStyle,
                                                   Degree10 nOrientation);

    static Point GetRotationOrigin(const Point& rPos, const Size& rTextSize, Degree10 nOrientation,
                                   TextAlign eAlign);

    static RotatedGeometry GetRotatedGeometry(const Point& rBase, const tools::Rectangle& rRect,
                                              Degree10 nOrientation);

    static Point GetRotatedImageOrigin(const Point& rBase, const tools::Rectangle& rLocalBounds,
                                       Degree10 nOrientation);

    static tools::Long GetMirroredX(const MirroringContext& rCtx);

    static tools::Long GetReliefOffset(sal_Int32 nDPIX, FontRelief eRelief);
    static tools::Long GetShadowOffset(tools::Long nLineHeight, bool bIsOutline);

    static const std::vector<basegfx::B2DPoint>& GetOutlineOffsets();

    static MnemonicGeometry
    GetMnemonicGeometry(std::function<double(tools::Long)> const& fnLogicWidthToDeviceSubPixel,
                        std::function<tools::Long(tools::Long)> const& fnLogicWidthToDevicePixel,
                        std::function<Point(const Point&)> const& fnLogicToPixel,
                        const MnemonicDeviceParams& rParams, KernArraySpan aDXArray,
                        sal_Int32 nRelPos, const Point& rLinePos, bool bTrailing = false);
};

} // namespace vcl::text

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
