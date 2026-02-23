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

#include <sal/types.h>
#include <tools/poly.hxx>
#include <tools/helpers.hxx>
#include <comphelper/scopeguard.hxx>

#include <vcl/metafile/MetaAction.hxx>
#include <vcl/rendercontext/DrawGridFlags.hxx>
#include <vcl/rendercontext/PrimitiveRenderer.hxx>
#include <vcl/virdev.hxx>
#include <vcl/metafile/MetafileRecorder.hxx>

#include <ClippingController.hxx>
#include <CoordinateMapper.hxx>
#include <GraphicsState.hxx>
#include <salgdi.hxx>

#include <cassert>

void OutputDevice::DrawBorder(tools::Rectangle aBorderRect)
{
    sal_uInt16 nPixel = static_cast<sal_uInt16>(PixelToLogic(Size(1, 1)).Width());

    aBorderRect.AdjustLeft(nPixel);
    aBorderRect.AdjustTop(nPixel);

    SetLineColor(COL_LIGHTGRAY);
    DrawRect(aBorderRect);

    aBorderRect.AdjustLeft(-nPixel);
    aBorderRect.AdjustTop(-nPixel);
    aBorderRect.AdjustRight(-nPixel);
    aBorderRect.AdjustBottom(-nPixel);
    SetLineColor(COL_GRAY);

    DrawRect(aBorderRect);
}

void OutputDevice::DrawRect(const tools::Rectangle& rRect)
{
    assert(!is_double_buffered_window());

    if (rRect.IsEmpty())
        return;

    maRecorder.RecordRect(rRect);

    if (PrepareGraphicsOutput() && mpGraphics)
        vcl::rendercontext::PrimitiveRenderer::DrawRect(*mpGraphics, *mpMapper, this, rRect);
}

void OutputDevice::DrawRoundedRect(const tools::Rectangle& rRect,
                                   sal_uLong nHorzRound, sal_uLong nVertRound)
{
    assert(!is_double_buffered_window());

    if (rRect.IsEmpty())
        return;

    maRecorder.RecordRoundRect(rRect, nHorzRound, nVertRound);

    if (PrepareGraphicsOutput() && mpGraphics)
        vcl::rendercontext::PrimitiveRenderer::DrawRoundedRect(*mpGraphics, *mpMapper, this, rRect,
                                                               nHorzRound, nVertRound, mpGraphicsState->mbFillColor);
}

void OutputDevice::Invert(const tools::Rectangle& rRect, InvertFlags nFlags)
{
    assert(!is_double_buffered_window());

    if (rRect.IsEmpty())
        return;

    if (PrepareGraphicsOutput(vcl::PrepareOutputFlags::Clip) && mpGraphics)
        vcl::rendercontext::PrimitiveRenderer::Invert(*mpGraphics, *mpMapper, this, rRect, nFlags);
}

void OutputDevice::Invert(const tools::Polygon& rPoly, InvertFlags nFlags)
{
    assert(!is_double_buffered_window());

    if (!rPoly.GetSize())
        return;

    if (PrepareGraphicsOutput(vcl::PrepareOutputFlags::Clip) && mpGraphics)
        vcl::rendercontext::PrimitiveRenderer::Invert(*mpGraphics, *mpMapper, this, rPoly, nFlags);
}

void OutputDevice::DrawCheckered(const Point& rPos, const Size& rSize, sal_uInt32 nLen, Color aStart, Color aEnd)
{
    assert(!is_double_buffered_window());

    const sal_uInt32 nMaxX(rPos.X() + rSize.Width());
    const sal_uInt32 nMaxY(rPos.Y() + rSize.Height());

    auto popIt = ScopedPush(vcl::PushFlags::LINECOLOR | vcl::PushFlags::FILLCOLOR);
    SetLineColor();

    for(sal_uInt32 x(0), nX(rPos.X()); nX < nMaxX; x++, nX += nLen)
    {
        const sal_uInt32 nRight(std::min(nMaxX, nX + nLen));

        for(sal_uInt32 y(0), nY(rPos.Y()); nY < nMaxY; y++, nY += nLen)
        {
            const sal_uInt32 nBottom(std::min(nMaxY, nY + nLen));

            SetFillColor(((x & 0x0001) ^ (y & 0x0001)) ? aStart : aEnd);
            DrawRect(tools::Rectangle(nX, nY, nRight, nBottom));
        }
    }
}

void OutputDevice::DrawGrid(const tools::Rectangle& rRect, const Size& rDist, DrawGridFlags nFlags)
{
    assert(!is_double_buffered_window());

    if (rRect.IsEmpty())
        return;

    tools::Rectangle aDstRect(PixelToLogic(Point()), GetOutputSize());
    aDstRect.Intersection(rRect);

    if (aDstRect.IsEmpty())
        return;

    const bool bOldMap = mpMapper->IsMapModeEnabled();
    comphelper::ScopeGuard aMapGuard([this, bOldMap]() { this->EnableMapMode(bOldMap); });

    if (!PrepareGraphicsOutput(vcl::PrepareOutputFlags::All, vcl::MapModePolicy::ForcePixel))
        return;

    vcl::rendercontext::PrimitiveRenderer::DrawGrid(*mpGraphics, *mpMapper, this, rRect, aDstRect, rDist, nFlags);
}

void OutputDevice::DrawGridOfCrosses(const tools::Rectangle& rGridArea, const Size& rGridDistance,
                                     const tools::Rectangle& rDrawingArea)
{
    assert(!is_double_buffered_window());

    if (rDrawingArea.IsEmpty())
        return;

    if (!PrepareGraphicsOutput(vcl::PrepareOutputFlags::Clip | vcl::PrepareOutputFlags::Line))
        return;

    const tools::Long nDistanceX = rGridDistance.Width();
    const tools::Long nDistanceY = rGridDistance.Height();
    tools::Long nX = rGridArea.Left();
    tools::Long nY = rGridArea.Top();

    // Collect horizontal positions of the grid points
    std::vector<tools::Long> aHorzBuffer;
    aHorzBuffer.reserve(rGridArea.GetWidth() / nDistanceX + 1);
    while (nX <= rGridArea.Right())
    {
        aHorzBuffer.push_back(nX);
        nX += nDistanceX;
    }

    // Collect vertical positions of the grid points
    std::vector<tools::Long> aVertBuffer;
    aVertBuffer.reserve(rGridArea.GetHeight() / nDistanceY + 1);
    while (nY <= rGridArea.Bottom())
    {
        aVertBuffer.push_back(nY);
        nY += nDistanceY;
    }

    const tools::Long nTopPixel = LogicYToDevicePixel(rDrawingArea.Top());
    const tools::Long nBottomPixel = LogicYToDevicePixel(rDrawingArea.Bottom());
    const tools::Long nLeftPixel = LogicXToDevicePixel(rDrawingArea.Left());
    const tools::Long nRightPixel = LogicXToDevicePixel(rDrawingArea.Right());

    // Draw 3x3 pixel crosses within the drawing area
    const tools::Long nHalfCrossSize = 1;
    for (const tools::Long nPositionX : aHorzBuffer)
    {
        const tools::Long nPositionXPixel = LogicXToDevicePixel(nPositionX);
        for (const tools::Long nPositionY : aVertBuffer)
        {
            const tools::Long nPositionYPixel = LogicYToDevicePixel(nPositionY);
            const tools::Long nStartXPixel = std::max(nPositionXPixel - nHalfCrossSize, nLeftPixel);
            if (nStartXPixel > nRightPixel)
            {
                continue;
            }
            const tools::Long nEndXPixel
                = std::min(nPositionXPixel + nHalfCrossSize + 1, nRightPixel);
            if (nEndXPixel < nLeftPixel)
            {
                continue;
            }
            const tools::Long nStartYPixel = std::max(nPositionYPixel - nHalfCrossSize, nTopPixel);
            if (nStartYPixel > nBottomPixel)
            {
                continue;
            }
            const tools::Long nEndYPixel
                = std::min(nPositionYPixel + nHalfCrossSize + 1, nBottomPixel);
            if (nEndYPixel < nTopPixel)
            {
                continue;
            }

            const bool bOldMap = mpMapper->IsMapModeEnabled();
            mpMapper->EnableMapMode(false);

            // Draw horizontal line if visible
            if (nPositionY >= rDrawingArea.Top() && nPositionY <= rDrawingArea.Bottom())
            {
                mpGraphics->DrawLine(nStartXPixel, nPositionYPixel, nEndXPixel, nPositionYPixel,
                                     *this);
            }

            // Draw vertical line if visible
            if (nPositionX >= rDrawingArea.Left() && nPositionX <= rDrawingArea.Right())
            {
                mpGraphics->DrawLine(nPositionXPixel, nStartYPixel, nPositionXPixel, nEndYPixel,
                                     *this);
            }

            mpMapper->EnableMapMode(bOldMap);
        }
    }
}

BmpMirrorFlags AdjustTwoRect( SalTwoRect& rTwoRect, const Size& rSizePix )
{
    BmpMirrorFlags nMirrFlags = BmpMirrorFlags::NONE;

    if ( rTwoRect.mnDestWidth < 0 )
    {
        rTwoRect.mnSrcX = rSizePix.Width() - rTwoRect.mnSrcX - rTwoRect.mnSrcWidth;
        rTwoRect.mnDestWidth = -rTwoRect.mnDestWidth;
        rTwoRect.mnDestX -= rTwoRect.mnDestWidth-1;
        nMirrFlags |= BmpMirrorFlags::Horizontal;
    }

    if ( rTwoRect.mnDestHeight < 0 )
    {
        rTwoRect.mnSrcY = rSizePix.Height() - rTwoRect.mnSrcY - rTwoRect.mnSrcHeight;
        rTwoRect.mnDestHeight = -rTwoRect.mnDestHeight;
        rTwoRect.mnDestY -= rTwoRect.mnDestHeight-1;
        nMirrFlags |= BmpMirrorFlags::Vertical;
    }

    if( ( rTwoRect.mnSrcX < 0 ) || ( rTwoRect.mnSrcX >= rSizePix.Width() ) ||
        ( rTwoRect.mnSrcY < 0 ) || ( rTwoRect.mnSrcY >= rSizePix.Height() ) ||
        ( ( rTwoRect.mnSrcX + rTwoRect.mnSrcWidth ) > rSizePix.Width() ) ||
        ( ( rTwoRect.mnSrcY + rTwoRect.mnSrcHeight ) > rSizePix.Height() ) )
    {
        const tools::Rectangle aSourceRect( Point( rTwoRect.mnSrcX, rTwoRect.mnSrcY ),
                                     Size( rTwoRect.mnSrcWidth, rTwoRect.mnSrcHeight ) );
        tools::Rectangle aCropRect( aSourceRect );

        aCropRect.Intersection( tools::Rectangle( Point(), rSizePix ) );

        if( aCropRect.IsEmpty() )
        {
            rTwoRect.mnSrcWidth = rTwoRect.mnSrcHeight = rTwoRect.mnDestWidth = rTwoRect.mnDestHeight = 0;
        }
        else
        {
            const double fFactorX = ( rTwoRect.mnSrcWidth > 1 ) ? static_cast<double>( rTwoRect.mnDestWidth - 1 ) / ( rTwoRect.mnSrcWidth - 1 ) : 0.0;
            const double fFactorY = ( rTwoRect.mnSrcHeight > 1 ) ? static_cast<double>( rTwoRect.mnDestHeight - 1 ) / ( rTwoRect.mnSrcHeight - 1 ) : 0.0;

            const tools::Long nDstX1 = rTwoRect.mnDestX + basegfx::fround<tools::Long>( fFactorX * ( aCropRect.Left() - rTwoRect.mnSrcX ) );
            const tools::Long nDstY1 = rTwoRect.mnDestY + basegfx::fround<tools::Long>( fFactorY * ( aCropRect.Top() - rTwoRect.mnSrcY ) );
            const tools::Long nDstX2 = rTwoRect.mnDestX + basegfx::fround<tools::Long>( fFactorX * ( aCropRect.Right() - rTwoRect.mnSrcX ) );
            const tools::Long nDstY2 = rTwoRect.mnDestY + basegfx::fround<tools::Long>( fFactorY * ( aCropRect.Bottom() - rTwoRect.mnSrcY ) );

            rTwoRect.mnSrcX = aCropRect.Left();
            rTwoRect.mnSrcY = aCropRect.Top();
            rTwoRect.mnSrcWidth = aCropRect.GetWidth();
            rTwoRect.mnSrcHeight = aCropRect.GetHeight();
            rTwoRect.mnDestX = nDstX1;
            rTwoRect.mnDestY = nDstY1;
            rTwoRect.mnDestWidth = nDstX2 - nDstX1 + 1;
            rTwoRect.mnDestHeight = nDstY2 - nDstY1 + 1;
        }
    }

    return nMirrFlags;
}

void AdjustTwoRect( SalTwoRect& rTwoRect, const tools::Rectangle& rValidSrcRect )
{
    if( !(( rTwoRect.mnSrcX < rValidSrcRect.Left() ) || ( rTwoRect.mnSrcX >= rValidSrcRect.Right() ) ||
        ( rTwoRect.mnSrcY < rValidSrcRect.Top() ) || ( rTwoRect.mnSrcY >= rValidSrcRect.Bottom() ) ||
        ( ( rTwoRect.mnSrcX + rTwoRect.mnSrcWidth ) > rValidSrcRect.Right() ) ||
        ( ( rTwoRect.mnSrcY + rTwoRect.mnSrcHeight ) > rValidSrcRect.Bottom() )) )
        return;

    const tools::Rectangle aSourceRect( Point( rTwoRect.mnSrcX, rTwoRect.mnSrcY ),
                                 Size( rTwoRect.mnSrcWidth, rTwoRect.mnSrcHeight ) );
    tools::Rectangle aCropRect( aSourceRect );

    aCropRect.Intersection( rValidSrcRect );

    if( aCropRect.IsEmpty() )
    {
        rTwoRect.mnSrcWidth = rTwoRect.mnSrcHeight = rTwoRect.mnDestWidth = rTwoRect.mnDestHeight = 0;
    }
    else
    {
        const double fFactorX = ( rTwoRect.mnSrcWidth > 1 ) ? static_cast<double>( rTwoRect.mnDestWidth - 1 ) / ( rTwoRect.mnSrcWidth - 1 ) : 0.0;
        const double fFactorY = ( rTwoRect.mnSrcHeight > 1 ) ? static_cast<double>( rTwoRect.mnDestHeight - 1 ) / ( rTwoRect.mnSrcHeight - 1 ) : 0.0;

        const tools::Long nDstX1 = rTwoRect.mnDestX + basegfx::fround<tools::Long>( fFactorX * ( aCropRect.Left() - rTwoRect.mnSrcX ) );
        const tools::Long nDstY1 = rTwoRect.mnDestY + basegfx::fround<tools::Long>( fFactorY * ( aCropRect.Top() - rTwoRect.mnSrcY ) );
        const tools::Long nDstX2 = rTwoRect.mnDestX + basegfx::fround<tools::Long>( fFactorX * ( aCropRect.Right() - rTwoRect.mnSrcX ) );
        const tools::Long nDstY2 = rTwoRect.mnDestY + basegfx::fround<tools::Long>( fFactorY * ( aCropRect.Bottom() - rTwoRect.mnSrcY ) );

        rTwoRect.mnSrcX = aCropRect.Left();
        rTwoRect.mnSrcY = aCropRect.Top();
        rTwoRect.mnSrcWidth = aCropRect.GetWidth();
        rTwoRect.mnSrcHeight = aCropRect.GetHeight();
        rTwoRect.mnDestX = nDstX1;
        rTwoRect.mnDestY = nDstY1;
        rTwoRect.mnDestWidth = nDstX2 - nDstX1 + 1;
        rTwoRect.mnDestHeight = nDstY2 - nDstY1 + 1;
    }
}

Color OutputDevice::DrawSelectionBackground(const tools::Rectangle& rRect,
                                            Color aWinBackgroundColor,
                                            sal_uInt16 nHighlight,
                                            bool bChecked,
                                            bool bDrawBorder,
                                            bool bDrawExtBorderOnly,
                                            Color const * pWinControlForeground,
                                            tools::Long nCornerRadius,
                                            Color const * pPaintColor)
{
    if (rRect.IsEmpty())
        return COL_TRANSPARENT;

    bool bRoundEdges = nCornerRadius > 0;

    const StyleSettings& rStyles = GetSettings().GetStyleSettings();

    // colors used for item highlighting
    Color aSelectionBorderColor(pPaintColor ? *pPaintColor : rStyles.GetHighlightColor());
    Color aSelectionFillColor(aSelectionBorderColor);

    bool bDark = rStyles.GetFaceColor().IsDark();
    bool bBright = !bDark && rStyles.GetHighContrastMode();

    int c1 = aSelectionBorderColor.GetLuminance();
    int c2 = aWinBackgroundColor.GetLuminance();

    if (!bDark && !bBright && std::abs(c2 - c1) < (pPaintColor ? 40 : 75))
    {
        // contrast too low
        sal_uInt16 h, s, b;
        aSelectionFillColor.RGBtoHSB( h, s, b );
        if( b > 50 )    b -= 40;
        else            b += 40;
        aSelectionFillColor = Color::HSBtoRGB( h, s, b );
        aSelectionBorderColor = aSelectionFillColor;
    }

    if (bRoundEdges)
    {
        if (aSelectionBorderColor.IsDark())
            aSelectionBorderColor.IncreaseLuminance(128);
        else
            aSelectionBorderColor.DecreaseLuminance(128);
    }

    tools::Rectangle aRect(rRect);
    if (bDrawExtBorderOnly)
    {
        aRect.AdjustLeft( -1 );
        aRect.AdjustTop( -1 );
        aRect.AdjustRight(1 );
        aRect.AdjustBottom(1 );
    }
    auto popIt = ScopedPush(vcl::PushFlags::FILLCOLOR | vcl::PushFlags::LINECOLOR);

    if (bDrawBorder)
        SetLineColor(bDark ? COL_WHITE : (bBright ? COL_BLACK : aSelectionBorderColor));
    else
        SetLineColor();

    sal_uInt16 nPercent = 0;
    if (!nHighlight)
    {
        if (bDark)
            aSelectionFillColor = COL_BLACK;
        else
            nPercent = 80;  // just checked (light)
    }
    else
    {
        if (bChecked && nHighlight == 2)
        {
            if (bDark)
                aSelectionFillColor = COL_LIGHTGRAY;
            else if (bBright)
            {
                aSelectionFillColor = COL_BLACK;
                SetLineColor(COL_BLACK);
                nPercent = 0;
            }
            else
                nPercent = bRoundEdges ? 40 : 20; // selected, pressed or checked ( very dark )
        }
        else if (bChecked || nHighlight == 1)
        {
            if (bDark)
                aSelectionFillColor = COL_GRAY;
            else if (bBright)
            {
                aSelectionFillColor = COL_BLACK;
                SetLineColor(COL_BLACK);
                nPercent = 0;
            }
            else
                nPercent = bRoundEdges ? 60 : 35; // selected, pressed or checked ( very dark )
        }
        else
        {
            if (bDark)
                aSelectionFillColor = COL_LIGHTGRAY;
            else if (bBright)
            {
                aSelectionFillColor = COL_BLACK;
                SetLineColor(COL_BLACK);
                if (nHighlight == 3)
                    nPercent = 80;
                else
                    nPercent = 0;
            }
            else
                nPercent = 70; // selected ( dark )
        }
    }

    Color aSelectionTextColor;

    if (bDark && bDrawExtBorderOnly)
    {
        SetFillColor();
        aSelectionTextColor = rStyles.GetHighlightTextColor();
    }
    else
    {
        SetFillColor(aSelectionFillColor);

        Color aTextColor = pWinControlForeground ? *pWinControlForeground : rStyles.GetButtonTextColor();
        Color aHLTextColor = rStyles.GetHighlightTextColor();
        int nTextDiff = std::abs(aSelectionFillColor.GetLuminance() - aTextColor.GetLuminance());
        int nHLDiff = std::abs(aSelectionFillColor.GetLuminance() - aHLTextColor.GetLuminance());
        aSelectionTextColor = (nHLDiff >= nTextDiff) ? aHLTextColor : aTextColor;
    }

    if (bDark)
    {
        DrawRect(aRect);
    }
    else
    {
        if (bRoundEdges)
        {
            tools::Polygon aPoly(aRect, nCornerRadius, nCornerRadius);
            tools::PolyPolygon aPolyPoly(aPoly);
            DrawTransparent(aPolyPoly, nPercent);
        }
        else
        {
            tools::Polygon aPoly(aRect);
            tools::PolyPolygon aPolyPoly(aPoly);
            DrawTransparent(aPolyPoly, nPercent);
        }
    }

    return aSelectionTextColor;
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
