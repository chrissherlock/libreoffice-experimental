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

#include <vcl/metafile/MetaAction.hxx>
#include <vcl/metafile/MetafileRecorder.hxx>
#include <vcl/region.hxx>
#include <vcl/rendercontext/DrawModeFlags.hxx>
#include <vcl/virdev.hxx>

#include <CoordinateMapper.hxx>
#include <GraphicsState.hxx>
#include <salgdi.hxx>

#include <cassert>

Color OutputDevice::GetReadableFontColor(const Color& rFontColor, const Color& rBgColor) const
{
    if (rBgColor.IsDark() && rFontColor.IsDark())
        return COL_WHITE;
    else if (rBgColor.IsBright() && rFontColor.IsBright())
        return COL_BLACK;
    else
        return rFontColor;
}

void OutputDevice::DrawWallpaper( const tools::Rectangle& rRect,
                                  const Wallpaper& rWallpaper )
{
    assert(!is_double_buffered_window());

    maRecorder.RecordWallpaper(rRect, rWallpaper);

    if ( !IsDeviceOutputNecessary() || IsLayoutCalculationNecessary() )
        return;

    if ( rWallpaper.GetStyle() != WallpaperStyle::NONE )
    {
        tools::Rectangle aRect = LogicToPixel( rRect );
        aRect.Normalize();

        if ( !aRect.IsEmpty() )
        {
            DrawWallpaper( aRect.Left(), aRect.Top(), aRect.GetWidth(), aRect.GetHeight(),
                               rWallpaper );
        }
    }
}

void OutputDevice::DrawWallpaper( tools::Long nX, tools::Long nY,
                                  tools::Long nWidth, tools::Long nHeight,
                                  const Wallpaper& rWallpaper )
{
    assert(!is_double_buffered_window());

    if( rWallpaper.IsBitmap() )
        DrawBitmapWallpaper( nX, nY, nWidth, nHeight, rWallpaper );
    else if( rWallpaper.IsGradient() )
        DrawGradientWallpaper( nX, nY, nWidth, nHeight, rWallpaper );
    else
        DrawColorWallpaper(  nX, nY, nWidth, nHeight, rWallpaper );
}

void OutputDevice::DrawColorWallpaper( tools::Long nX, tools::Long nY,
                                       tools::Long nWidth, tools::Long nHeight,
                                       const Wallpaper& rWallpaper )
{
    assert(!is_double_buffered_window());

    // draw wallpaper without border
    bool bOldIsLineColor = IsLineColor();
    Color aOldLineColor = GetLineColor();
    bool bOldIsFillColor = IsFillColor();
    Color aOldFillColor = GetFillColor();
    bool bMap = mpMapper->IsMapModeEnabled();

    SetLineColor();
    SetFillColor( rWallpaper.GetColor() );
    mpMapper->EnableMapMode(false);

    DrawRect( tools::Rectangle( Point( nX, nY ), Size( nWidth, nHeight ) ) );

    mpMapper->EnableMapMode(bMap);

    if (bOldIsFillColor)
        SetFillColor(aOldFillColor);
    else
        SetFillColor();

    if (bOldIsLineColor)
        SetLineColor(aOldLineColor);
    else
        SetLineColor();
}

void OutputDevice::Erase()
{
    if ( !IsDeviceOutputNecessary() || IsLayoutCalculationNecessary() )
        return;

    if ( mbBackground )
    {
        RasterOp eRasterOp = GetRasterOp();
        if ( eRasterOp != RasterOp::OverPaint )
            SetRasterOp( RasterOp::OverPaint );
        DrawWallpaper( 0, 0, GetOutputWidthPixel(), GetOutputHeightPixel(), maBackground );
        if ( eRasterOp != RasterOp::OverPaint )
            SetRasterOp( eRasterOp );
    }
}

void OutputDevice::Erase(const tools::Rectangle& rRect)
{
    const RasterOp eRasterOp = GetRasterOp();
    if ( eRasterOp != RasterOp::OverPaint )
        SetRasterOp( RasterOp::OverPaint );

    DrawWallpaper(rRect, GetBackground());

    if ( eRasterOp != RasterOp::OverPaint )
        SetRasterOp( eRasterOp );
}

void OutputDevice::DrawWallpaperNegativeSpace(const tools::Rectangle& rTargetRect,
                                              const Point& rBmpPos,
                                              const Size& rBmpSize,
                                              const Wallpaper& rWallpaper)
{
    vcl::Region aNegativeSpace(rTargetRect);

    aNegativeSpace.Exclude(tools::Rectangle(rBmpPos, rBmpSize));

    for (const tools::Rectangle& rRect : aNegativeSpace)
    {
        DrawColorWallpaper(rRect.Left(), rRect.Top(),
                           rRect.GetWidth(), rRect.GetHeight(),
                           rWallpaper);
    }
}

static Point lcl_CalculatePlacement(const Point& rBasePos, const Size& rBoundingSize, const Size& rBmpSize, WallpaperStyle eStyle)
{
    Point aPos = rBasePos;
    const tools::Long nBoundW = rBoundingSize.Width();
    const tools::Long nBoundH = rBoundingSize.Height();
    const tools::Long nBmpW = rBmpSize.Width();
    const tools::Long nBmpH = rBmpSize.Height();

    switch( eStyle )
    {
        case WallpaperStyle::Top:
            aPos.AdjustX((nBoundW - nBmpW) >> 1); break;
        case WallpaperStyle::TopRight:
            aPos.AdjustX(nBoundW - nBmpW); break;
        case WallpaperStyle::Left:
            aPos.AdjustY((nBoundH - nBmpH) >> 1); break;
        case WallpaperStyle::Center:
            aPos.AdjustX((nBoundW - nBmpW) >> 1);
            aPos.AdjustY((nBoundH - nBmpH) >> 1); break;
        case WallpaperStyle::Right:
            aPos.AdjustX(nBoundW - nBmpW);
            aPos.AdjustY((nBoundH - nBmpH) >> 1); break;
        case WallpaperStyle::BottomLeft:
            aPos.AdjustY(nBoundH - nBmpH); break;
        case WallpaperStyle::Bottom:
            aPos.AdjustX((nBoundW - nBmpW) >> 1);
            aPos.AdjustY(nBoundH - nBmpH); break;
        case WallpaperStyle::BottomRight:
            aPos.AdjustX(nBoundW - nBmpW);
            aPos.AdjustY(nBoundH - nBmpH); break;
        default:
            break; // TopLeft, Tile, Scale handled elsewhere
    }
    return aPos;
}

/**
 * Determines the logical bounding area for the wallpaper.
 * If the Wallpaper object has a specific rectangle set, we use that;
 * otherwise, we default to the full size of the target area anchored at (0,0).
 */
static tools::Rectangle lcl_CalculateBoundingRect(const Wallpaper& rWallpaper, const tools::Rectangle& rTargetRect)
{
    if (rWallpaper.IsRect())
        return rWallpaper.GetRect();

    // Default to the size of the target area, but at origin (0,0)
    // to allow relative placement calculation later.
    return tools::Rectangle(Point(0, 0), rTargetRect.GetSize());
}

void OutputDevice::DrawBitmapWallpaper( tools::Long nX, tools::Long nY,
                                        tools::Long nWidth, tools::Long nHeight,
                                        const Wallpaper& rWallpaper )
{
    assert(!is_double_buffered_window());
    if (IsLayoutCalculationNecessary())
        return;

    const tools::Rectangle aTargetRect(Point(nX, nY), Size(nWidth, nHeight));
    if (aTargetRect.IsEmpty())
        return;

    const Bitmap* pCached = rWallpaper.ImplGetCachedBitmap();
    const bool bOldMap = mpMapper->IsMapModeEnabled();

    Bitmap aBmp = pCached ? *pCached : rWallpaper.GetBitmap();

    const bool bDrawColorBackground = DrawBitmapWallpaperBackground(aBmp, aTargetRect, rWallpaper, pCached);

    tools::Rectangle aBoundingRect = lcl_CalculateBoundingRect(rWallpaper, aTargetRect);
    if (rWallpaper.IsRect())
        aBoundingRect = LogicToPixel(aBoundingRect);

    vcl::MetafileRecorder::ScopedSuspend aMetaFileSuspend(maRecorder);

    comphelper::ScopeGuard aMapModeGuard([this, bOldMap]() {
        mpMapper->EnableMapMode(bOldMap);
    });
    mpMapper->EnableMapMode(false);

    auto aClipGuard = ScopedPush(vcl::PushFlags::CLIPREGION);
    IntersectClipRegion(aTargetRect);

    WallpaperLayout aLayout = GetBitmapWallpaperLayout(aBmp, aTargetRect, aBoundingRect, rWallpaper);

    bool bDrawn = DrawBitmapWallpaperContents(aBmp, aTargetRect, aBoundingRect, rWallpaper, pCached, aLayout);

    // Final Single-Instance Draw
    // If bDrawn is false, we are dealing with a single bitmap (Aligned, TopLeft, or Scaled)
    if (!bDrawn)
    {
        if (bDrawColorBackground)
        {
            DrawWallpaperNegativeSpace(aTargetRect, aLayout.maDrawPos, aBmp.GetSizePixel(), rWallpaper);
        }

        DrawBitmap(aLayout.maDrawPos, aBmp);
    }

    rWallpaper.ImplSetCachedBitmap(aBmp);
}

bool OutputDevice::DrawBitmapWallpaperBackground( Bitmap& rBmp,
                                                  const tools::Rectangle& rRect,
                                                  const Wallpaper& rWallpaper,
                                                  const Bitmap* pCached )
{
    const tools::Long nBmpWidth = rBmp.GetSizePixel().Width();
    const tools::Long nBmpHeight = rBmp.GetSizePixel().Height();
    const bool bTransparent = rBmp.HasAlpha();
    const WallpaperStyle eStyle = rWallpaper.GetStyle();

    bool bDrawGradientBackground = false;
    bool bDrawColorBackground = false;

    if( bTransparent )
    {
        if( rWallpaper.IsGradient() )
        {
            bDrawGradientBackground = true;
        }
        else
        {
            if( !pCached && !rWallpaper.GetColor().IsTransparent() )
            {
                ScopedVclPtrInstance< VirtualDevice > aVDev( *this );
                aVDev->SetBackground( rWallpaper.GetColor() );
                aVDev->SetOutputSizePixel( Size( nBmpWidth, nBmpHeight ) );
                aVDev->DrawBitmap( Point(), rBmp );
                rBmp = aVDev->GetBitmap( Point(), aVDev->GetOutputSizePixel() );
            }

            bDrawColorBackground = true;
        }
    }
    else if( eStyle != WallpaperStyle::Tile && eStyle != WallpaperStyle::Scale )
    {
        bDrawGradientBackground = rWallpaper.IsGradient();
        bDrawColorBackground = !bDrawGradientBackground;
    }

    if( bDrawGradientBackground )
    {
        DrawGradientWallpaper( rRect.Left(), rRect.Top(), rRect.GetWidth(), rRect.GetHeight(), rWallpaper );
    }
    else if( bDrawColorBackground && bTransparent )
    {
        DrawColorWallpaper( rRect.Left(), rRect.Top(), rRect.GetWidth(), rRect.GetHeight(), rWallpaper );
        bDrawColorBackground = false; // Already drawn
    }

    return bDrawColorBackground;
}

OutputDevice::WallpaperLayout OutputDevice::GetBitmapWallpaperLayout(
    const Bitmap& rBmp, const tools::Rectangle& rTargetRect,
    const tools::Rectangle& rBoundingRect, const Wallpaper& rWallpaper)
{
    WallpaperLayout aLayout;
    const WallpaperStyle eStyle = rWallpaper.GetStyle();
    const Size aBmpSize = rBmp.GetSizePixel();

    // Default: TopLeft of the bounding area
    aLayout.maDrawPos = rBoundingRect.TopLeft();

    if (eStyle == WallpaperStyle::Tile)
    {
        aLayout.mbTiled = true;
        const tools::Long nBmpWidth = aBmpSize.Width();
        const tools::Long nBmpHeight = aBmpSize.Height();

        // Calculate the offset relative to the target area
        const tools::Long nOffX = (aLayout.maDrawPos.X() - rTargetRect.Left()) % nBmpWidth;
        const tools::Long nOffY = (aLayout.maDrawPos.Y() - rTargetRect.Top()) % nBmpHeight;

        aLayout.mnStartX = rTargetRect.Left() + nOffX;
        aLayout.mnStartY = rTargetRect.Top() + nOffY;

        if (nOffX > 0)
            aLayout.mnStartX -= nBmpWidth;

        if (nOffY > 0)
            aLayout.mnStartY -= nBmpHeight;
    }
    else if (eStyle != WallpaperStyle::Scale && eStyle != WallpaperStyle::TopLeft)
    {
        // Align single-instance styles (Center, Right, etc.)
        aLayout.maDrawPos = lcl_CalculatePlacement(
            rBoundingRect.TopLeft(), rBoundingRect.GetSize(), aBmpSize, eStyle);
    }

    return aLayout;
}

bool OutputDevice::DrawBitmapWallpaperContents(
    Bitmap& rBmp, const tools::Rectangle& rTargetRect,
    const tools::Rectangle& rBoundingRect, const Wallpaper& rWallpaper,
    const Bitmap* pCached, const WallpaperLayout& rLayout)
{
    const WallpaperStyle eStyle = rWallpaper.GetStyle();

    // Handle Scaling
    if (eStyle == WallpaperStyle::Scale)
    {
        if (!pCached || (pCached->GetSizePixel() != rBoundingRect.GetSize()))
        {
            if (pCached) rWallpaper.ImplReleaseCachedBitmap();
            rBmp = rWallpaper.GetBitmap();
            rBmp.Scale(rBoundingRect.GetSize());
            rBmp = rBmp.CreateDisplayBitmap(this);
        }
        return false; // Orchestrator will draw the single scaled bitmap
    }

    // Execute Tiling if the layout plan calls for it
    if (rLayout.mbTiled)
    {
        const Size aBmpSize = rBmp.GetSizePixel();
        bool bDrawn = false;

        if (mpGraphicsState->meRasterOp == RasterOp::OverPaint &&
            mpGraphicsState->mnDrawMode == DrawModeFlags::Default)
        {
            bDrawn = mpGraphics->DrawBitmapWallpaper(
                rLayout.mnStartX, rLayout.mnStartY, rTargetRect.Right(), rTargetRect.Bottom(),
                aBmpSize.Width(), aBmpSize.Height(), *rBmp.ImplGetSalBitmap());
        }

        if (!bDrawn)
        {
            for (tools::Long nY = rLayout.mnStartY; nY <= rTargetRect.Bottom(); nY += aBmpSize.Height())
            {
                for (tools::Long nX = rLayout.mnStartX; nX <= rTargetRect.Right(); nX += aBmpSize.Width())
                {
                    DrawBitmap(Point(nX, nY), rBmp);
                }
            }
        }
        return true;
    }

    return false; // Single bitmap (Aligned or TopLeft), Orchestrator will draw
}

void OutputDevice::DrawGradientWallpaper( tools::Long nX, tools::Long nY,
                                          tools::Long nWidth, tools::Long nHeight,
                                          const Wallpaper& rWallpaper )
{
    assert(!is_double_buffered_window());

    const bool bOldMap = mpMapper->IsMapModeEnabled();

    tools::Rectangle aBound(Point(nX, nY), Size(nWidth, nHeight));

    vcl::MetafileRecorder::ScopedSuspend aMetaFileSuspend(maRecorder);

    comphelper::ScopeGuard aMapModeGuard([this, bOldMap]() {
        mpMapper->EnableMapMode(bOldMap);
    });
    mpMapper->EnableMapMode(false);

    auto aClipGuard = ScopedPush(vcl::PushFlags::CLIPREGION);
    IntersectClipRegion(tools::Rectangle(Point(nX, nY), Size(nWidth, nHeight)));

    DrawGradient(aBound, rWallpaper.GetGradient());
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
