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

#include <vcl/gradient.hxx>
#include <vcl/virdev.hxx>

#include <cassert>

void RenderContext2::DrawWallpaper(tools::Rectangle const& rRect, Wallpaper const& rWallpaper)
{
    if (!IsDeviceOutputNecessary() || ImplIsRecordLayout())
        return;

    if (rWallpaper.GetStyle() != WallpaperStyle::NONE)
    {
        tools::Rectangle aRect = LogicToPixel(rRect);
        aRect.Justify();

        if (!aRect.IsEmpty())
        {
            DrawWallpaper(aRect.Left(), aRect.Top(), aRect.GetWidth(), aRect.GetHeight(),
                          rWallpaper);
        }
    }

    if (mpAlphaVDev)
        mpAlphaVDev->DrawWallpaper(rRect, rWallpaper);
}

void RenderContext2::DrawWallpaper(tools::Long nX, tools::Long nY, tools::Long nWidth,
                                   tools::Long nHeight, Wallpaper const& rWallpaper)
{
    assert(!is_double_buffered_window());

    if (rWallpaper.IsBitmap())
        DrawBitmapWallpaper(nX, nY, nWidth, nHeight, rWallpaper);
    else if (rWallpaper.IsGradient())
        DrawGradientWallpaper(nX, nY, nWidth, nHeight, rWallpaper);
    else
        DrawColorWallpaper(nX, nY, nWidth, nHeight, rWallpaper);
}

void RenderContext2::DrawColorWallpaper(tools::Long nX, tools::Long nY, tools::Long nWidth,
                                        tools::Long nHeight, Wallpaper const& rWallpaper)
{
    assert(!is_double_buffered_window());

    // draw wallpaper without border
    Color aOldLineColor = GetLineColor();
    Color aOldFillColor = GetFillColor();
    SetLineColor();
    SetFillColor(rWallpaper.GetColor());

    bool bMap = mbMap;
    EnableMapMode(false);
    DrawRect(tools::Rectangle(Point(nX, nY), Size(nWidth, nHeight)));
    SetLineColor(aOldLineColor);
    SetFillColor(aOldFillColor);
    EnableMapMode(bMap);
}

void RenderContext2::DrawBitmapWallpaper(tools::Long nX, tools::Long nY, tools::Long nWidth,
                                         tools::Long nHeight, Wallpaper const& rWallpaper)
{
    assert(!is_double_buffered_window());

    BitmapEx aBmpEx;
    BitmapEx const* pCached = rWallpaper.ImplGetCachedBitmap();
    Point aPos;
    Size aSize;
    const WallpaperStyle eStyle = rWallpaper.GetStyle();
    const bool bOldMap = mbMap;
    bool bDrawn = false;
    bool bDrawGradientBackground = false;
    bool bDrawColorBackground = false;

    if (pCached)
        aBmpEx = *pCached;
    else
        aBmpEx = rWallpaper.GetBitmap();

    const tools::Long nBmpWidth = aBmpEx.GetSizePixel().Width();
    const tools::Long nBmpHeight = aBmpEx.GetSizePixel().Height();
    const bool bTransparent = aBmpEx.IsTransparent();

    // draw background
    if (bTransparent)
    {
        if (rWallpaper.IsGradient())
        {
            bDrawGradientBackground = true;
        }
        else
        {
            if (!pCached && !rWallpaper.GetColor().IsTransparent())
            {
                ScopedVclPtrInstance<VirtualDevice> aVDev(*this);
                aVDev->SetBackground(rWallpaper.GetColor());
                aVDev->SetOutputSizePixel(Size(nBmpWidth, nBmpHeight));
                aVDev->DrawBitmapEx(Point(), aBmpEx);
                aBmpEx = aVDev->GetBitmapEx(Point(), aVDev->GetOutputSizePixel());
            }

            bDrawColorBackground = true;
        }
    }
    else if (eStyle != WallpaperStyle::Tile && eStyle != WallpaperStyle::Scale)
    {
        if (rWallpaper.IsGradient())
            bDrawGradientBackground = true;
        else
            bDrawColorBackground = true;
    }

    // background of bitmap?
    if (bDrawGradientBackground)
    {
        DrawGradientWallpaper(nX, nY, nWidth, nHeight, rWallpaper);
    }
    else if (bDrawColorBackground && bTransparent)
    {
        DrawColorWallpaper(nX, nY, nWidth, nHeight, rWallpaper);
        bDrawColorBackground = false;
    }

    // calc pos and size
    if (rWallpaper.IsRect())
    {
        const tools::Rectangle aBound(LogicToPixel(rWallpaper.GetRect()));
        aPos = aBound.TopLeft();
        aSize = aBound.GetSize();
    }
    else
    {
        aPos = Point(0, 0);
        aSize = Size(nWidth, nHeight);
    }

    EnableMapMode(false);
    Push(PushFlags::CLIPREGION);
    IntersectClipRegion(tools::Rectangle(Point(nX, nY), Size(nWidth, nHeight)));

    switch (eStyle)
    {
        case WallpaperStyle::Scale:
            if (!pCached || (pCached->GetSizePixel() != aSize))
            {
                if (pCached)
                    rWallpaper.ImplReleaseCachedBitmap();

                aBmpEx = rWallpaper.GetBitmap();
                aBmpEx.Scale(aSize);
                aBmpEx = BitmapEx(aBmpEx.GetBitmap().CreateDisplayBitmap(this), aBmpEx.GetMask());
            }
            break;

        case WallpaperStyle::TopLeft:
            break;

        case WallpaperStyle::Top:
            aPos.AdjustX((aSize.Width() - nBmpWidth) >> 1);
            break;

        case WallpaperStyle::TopRight:
            aPos.AdjustX(aSize.Width() - nBmpWidth);
            break;

        case WallpaperStyle::Left:
            aPos.AdjustY((aSize.Height() - nBmpHeight) >> 1);
            break;

        case WallpaperStyle::Center:
            aPos.AdjustX((aSize.Width() - nBmpWidth) >> 1);
            aPos.AdjustY((aSize.Height() - nBmpHeight) >> 1);
            break;

        case WallpaperStyle::Right:
            aPos.AdjustX(aSize.Width() - nBmpWidth);
            aPos.AdjustY((aSize.Height() - nBmpHeight) >> 1);
            break;

        case WallpaperStyle::BottomLeft:
            aPos.AdjustY(aSize.Height() - nBmpHeight);
            break;

        case WallpaperStyle::Bottom:
            aPos.AdjustX((aSize.Width() - nBmpWidth) >> 1);
            aPos.AdjustY(aSize.Height() - nBmpHeight);
            break;

        case WallpaperStyle::BottomRight:
            aPos.AdjustX(aSize.Width() - nBmpWidth);
            aPos.AdjustY(aSize.Height() - nBmpHeight);
            break;

        default:
        {
            const tools::Long nRight = nX + nWidth - 1;
            const tools::Long nBottom = nY + nHeight - 1;
            tools::Long nFirstX;
            tools::Long nFirstY;

            if (eStyle == WallpaperStyle::Tile)
            {
                nFirstX = aPos.X();
                nFirstY = aPos.Y();
            }
            else
            {
                nFirstX = aPos.X() + ((aSize.Width() - nBmpWidth) >> 1);
                nFirstY = aPos.Y() + ((aSize.Height() - nBmpHeight) >> 1);
            }

            const tools::Long nOffX = (nFirstX - nX) % nBmpWidth;
            const tools::Long nOffY = (nFirstY - nY) % nBmpHeight;
            tools::Long nStartX = nX + nOffX;
            tools::Long nStartY = nY + nOffY;

            if (nOffX > 0)
                nStartX -= nBmpWidth;

            if (nOffY > 0)
                nStartY -= nBmpHeight;

            for (tools::Long nBmpY = nStartY; nBmpY <= nBottom; nBmpY += nBmpHeight)
            {
                for (tools::Long nBmpX = nStartX; nBmpX <= nRight; nBmpX += nBmpWidth)
                {
                    DrawBitmapEx(Point(nBmpX, nBmpY), aBmpEx);
                }
            }
            bDrawn = true;
        }
        break;
    }

    if (!bDrawn)
    {
        // optimized for non-transparent bitmaps
        if (bDrawColorBackground)
        {
            const Size aBmpSize(aBmpEx.GetSizePixel());
            const Point aTmpPoint;
            const tools::Rectangle aOutRect(aTmpPoint, GetOutputSizePixel());
            const tools::Rectangle aColRect(Point(nX, nY), Size(nWidth, nHeight));

            tools::Rectangle aWorkRect(0, 0, aOutRect.Right(), aPos.Y() - 1);
            aWorkRect.Justify();
            aWorkRect.Intersection(aColRect);
            if (!aWorkRect.IsEmpty())
            {
                DrawColorWallpaper(aWorkRect.Left(), aWorkRect.Top(), aWorkRect.GetWidth(),
                                   aWorkRect.GetHeight(), rWallpaper);
            }

            aWorkRect
                = tools::Rectangle(0, aPos.Y(), aPos.X() - 1, aPos.Y() + aBmpSize.Height() - 1);
            aWorkRect.Justify();
            aWorkRect.Intersection(aColRect);
            if (!aWorkRect.IsEmpty())
            {
                DrawColorWallpaper(aWorkRect.Left(), aWorkRect.Top(), aWorkRect.GetWidth(),
                                   aWorkRect.GetHeight(), rWallpaper);
            }

            aWorkRect = tools::Rectangle(aPos.X() + aBmpSize.Width(), aPos.Y(), aOutRect.Right(),
                                         aPos.Y() + aBmpSize.Height() - 1);
            aWorkRect.Justify();
            aWorkRect.Intersection(aColRect);
            if (!aWorkRect.IsEmpty())
            {
                DrawColorWallpaper(aWorkRect.Left(), aWorkRect.Top(), aWorkRect.GetWidth(),
                                   aWorkRect.GetHeight(), rWallpaper);
            }

            aWorkRect = tools::Rectangle(0, aPos.Y() + aBmpSize.Height(), aOutRect.Right(),
                                         aOutRect.Bottom());
            aWorkRect.Justify();
            aWorkRect.Intersection(aColRect);
            if (!aWorkRect.IsEmpty())
            {
                DrawColorWallpaper(aWorkRect.Left(), aWorkRect.Top(), aWorkRect.GetWidth(),
                                   aWorkRect.GetHeight(), rWallpaper);
            }
        }

        DrawBitmapEx(aPos, aBmpEx);
    }

    rWallpaper.ImplSetCachedBitmap(aBmpEx);

    Pop();
    EnableMapMode(bOldMap);
}

void RenderContext2::DrawGradientWallpaper(tools::Long nX, tools::Long nY, tools::Long nWidth,
                                           tools::Long nHeight, Wallpaper const& rWallpaper)
{
    assert(!is_double_buffered_window());

    tools::Rectangle aBound;
    const bool bOldMap = mbMap;

    aBound = tools::Rectangle(Point(nX, nY), Size(nWidth, nHeight));

    EnableMapMode(false);
    Push(PushFlags::CLIPREGION);
    IntersectClipRegion(tools::Rectangle(Point(nX, nY), Size(nWidth, nHeight)));

    DrawGradient(aBound, rWallpaper.GetGradient());

    Pop();
    EnableMapMode(bOldMap);
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
