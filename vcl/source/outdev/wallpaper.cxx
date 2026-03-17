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

void OutputDevice::DrawBitmapWallpaper( tools::Long nX, tools::Long nY,
                                        tools::Long nWidth, tools::Long nHeight,
                                        const Wallpaper& rWallpaper )
{
    assert(!is_double_buffered_window());
    if( IsLayoutCalculationNecessary() ) return;

    const Bitmap* pCached = rWallpaper.ImplGetCachedBitmap();
    const bool bOldMap = mpMapper->IsMapModeEnabled();

    Bitmap aBmp = pCached ? *pCached : rWallpaper.GetBitmap();

    const tools::Long nBmpWidth = aBmp.GetSizePixel().Width();
    const tools::Long nBmpHeight = aBmp.GetSizePixel().Height();
    const bool bTransparent = aBmp.HasAlpha();
    const WallpaperStyle eStyle = rWallpaper.GetStyle();

    bool bDrawGradientBackground = false;
    bool bDrawColorBackground = false;

    // Background Handling
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
                aVDev->DrawBitmap( Point(), aBmp );
                aBmp = aVDev->GetBitmap( Point(), aVDev->GetOutputSizePixel() );
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
        DrawGradientWallpaper( nX, nY, nWidth, nHeight, rWallpaper );
    }
    else if( bDrawColorBackground && bTransparent )
    {
        DrawColorWallpaper( nX, nY, nWidth, nHeight, rWallpaper );
        bDrawColorBackground = false; // Already drawn
    }

    // Bounds and Clipping
    Point aBasePos;
    Size aBoundingSize;

    if( rWallpaper.IsRect() )
    {
        const tools::Rectangle aBound( LogicToPixel( rWallpaper.GetRect() ) );
        aBasePos = aBound.TopLeft();
        aBoundingSize = aBound.GetSize();
    }
    else
    {
        aBasePos = Point( 0, 0 );
        aBoundingSize = Size( nWidth, nHeight );
    }

    vcl::MetafileRecorder::ScopedSuspend aMetaFileSuspend(maRecorder);
    mpMapper->EnableMapMode(false);

    Push( vcl::PushFlags::CLIPREGION );
    const tools::Rectangle aTargetRect(Point(nX, nY), Size(nWidth, nHeight));
    IntersectClipRegion( aTargetRect );

    bool bDrawn = false;
    Point aDrawPos = aBasePos;

    // Bitmap Placement & Rendering
    if (eStyle == WallpaperStyle::Scale)
    {
        if( !pCached || ( pCached->GetSizePixel() != aBoundingSize ) )
        {
            if( pCached ) rWallpaper.ImplReleaseCachedBitmap();
            aBmp = rWallpaper.GetBitmap();
            aBmp.Scale( aBoundingSize );
            aBmp = aBmp.CreateDisplayBitmap( this );
        }
    }
    else if (eStyle == WallpaperStyle::Tile || eStyle == WallpaperStyle::TopLeft)
    {
        if (eStyle == WallpaperStyle::Tile)
        {
            const tools::Long nFirstX = aBasePos.X();
            const tools::Long nFirstY = aBasePos.Y();
            const tools::Long nOffX = ( nFirstX - nX ) % nBmpWidth;
            const tools::Long nOffY = ( nFirstY - nY ) % nBmpHeight;
            tools::Long nStartX = nX + nOffX;
            tools::Long nStartY = nY + nOffY;

            if( nOffX > 0 ) nStartX -= nBmpWidth;
            if( nOffY > 0 ) nStartY -= nBmpHeight;

            // Hardware fast-path
            if( mpGraphicsState->meRasterOp == RasterOp::OverPaint && mpGraphicsState->mnDrawMode == DrawModeFlags::Default && nWidth > 0 && nHeight > 0 )
                bDrawn = mpGraphics->DrawBitmapWallpaper(nStartX, nStartY, aTargetRect.Right(), aTargetRect.Bottom(), nBmpWidth, nBmpHeight, *aBmp.ImplGetSalBitmap());

            // Software loop fallback
            if (!bDrawn)
            {
                for( tools::Long nBmpY = nStartY; nBmpY <= aTargetRect.Bottom(); nBmpY += nBmpHeight )
                    for( tools::Long nBmpX = nStartX; nBmpX <= aTargetRect.Right(); nBmpX += nBmpWidth )
                        DrawBitmap(Point( nBmpX, nBmpY), aBmp);
                bDrawn = true;
            }
        }
    }
    else
    {
        // Single Aligned Bitmap
        aDrawPos = lcl_CalculatePlacement(aBasePos, aBoundingSize, aBmp.GetSizePixel(), eStyle);
    }

    // Single draw execution and negative space filling
    if( !bDrawn )
    {
        if( bDrawColorBackground )
            DrawWallpaperNegativeSpace(aTargetRect, aDrawPos, aBmp.GetSizePixel(), rWallpaper);

        DrawBitmap( aDrawPos, aBmp );
    }

    rWallpaper.ImplSetCachedBitmap( aBmp );

    Pop();
    mpMapper->EnableMapMode(bOldMap);
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
