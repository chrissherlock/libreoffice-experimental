/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <vcl/dllapi.h>
#include <tools/color.hxx>

class SalGraphics;
class Bitmap;
struct SalTwoRect;

namespace basegfx
{
class B2DHomMatrix;
class B2DPoint;
}

namespace vcl::rendercontext
{
/**
 * Stateless Compositor for Raster/Bitmap Operations.
 *
 * This class handles the rendering of bitmaps, alpha bitmaps, and transformed
 * images. It acts as the bridge between the stateful OutputDevice and the
 * hardware abstraction layer (SalGraphics).
 *
 * All coordinates passed to these methods MUST be fully composed device pixels.
 */
class VCL_DLLPUBLIC BitmapRenderer
{
public:
    static void DrawBitmap(SalGraphics& rGraphics, const SalTwoRect& rPosAry,
                           const Bitmap& rBitmap);

    static bool DrawTransformedBitmap(SalGraphics& rGraphics, const basegfx::B2DPoint& rNull,
                                      const basegfx::B2DPoint& rX, const basegfx::B2DPoint& rY,
                                      const Bitmap& rBitmap, double fAlpha = 1.0);

    static void DrawMask(SalGraphics& rGraphics, const SalTwoRect& rPosAry, const Bitmap& rBitmap,
                         const Color& rMaskColor);

private:
    BitmapRenderer() = delete;
    ~BitmapRenderer() = delete;
};

} // namespace vcl::rendercontext

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
