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
#include <basegfx/matrix/b2dhommatrixtools.hxx>
#include <basegfx/polygon/WaveLine.hxx>
#include <comphelper/configuration.hxx>
#include <tools/gen.hxx>
#include <tools/helpers.hxx>
#include <tools/lazydelete.hxx>
#include <o3tl/hash_combine.hxx>
#include <o3tl/lru_map.hxx>
#include <comphelper/configuration.hxx>
#include <tools/lazydelete.hxx>

#include <vcl/dropcache.hxx>
#include <vcl/metafile/MetaAction.hxx>
#include <vcl/metafile/MetafileRecorder.hxx>
#include <vcl/rendercontext/AntialiasingFlags.hxx>
#include <vcl/rendercontext/PrimitiveRenderer.hxx>
#include <vcl/settings.hxx>
#include <vcl/text/TextDecorator.hxx>
#include <vcl/text/TextLineGeometry.hxx>
#include <vcl/virdev.hxx>
#include <vcl/skia/SkiaHelper.hxx>

#include <CoordinateMapper.hxx>
#include <ClippingController.hxx>
#include <GraphicsState.hxx>
#include <drawmode.hxx>
#include <font/EmphasisMark.hxx>
#include <font/FontController.hxx>
#include <impglyphitem.hxx>
#include <salgdi.hxx>
#include <text/TextLayoutEngine.hxx>

#include <cassert>
#include <iterator>

namespace {
    struct WavyLineCache final : public CacheOwner
    {
        WavyLineCache()
#if defined __cpp_lib_memory_resource
            : m_aItems(10, &GetMemoryResource())
#else
            : m_aItems(10)
#endif
        {
        }

        bool find( Color aLineColor, size_t nLineWidth, size_t nWaveHeight, size_t nWordWidth, Bitmap& rOutput )
        {
            Key aKey = { nWaveHeight, sal_uInt32(aLineColor) };
            auto item = m_aItems.find( aKey );
            if ( item == m_aItems.end() )
                return false;
            // needs update
            if ( item->second.m_aLineWidth != nLineWidth || item->second.m_aWordWidth < nWordWidth )
            {
                return false;
            }
            rOutput = item->second.m_Bitmap;
            return true;
        }

        void insert( const Bitmap& aBitmap, const Color& aLineColor, const size_t nLineWidth, const size_t nWaveHeight, const size_t nWordWidth, Bitmap& rOutput )
        {
            Key aKey = { nWaveHeight, sal_uInt32(aLineColor) };
            m_aItems.insert( std::pair< Key, WavyLineCacheItem>( aKey, { nLineWidth, nWordWidth, aBitmap } ) );
            rOutput = aBitmap;
        }

        virtual OUString getCacheName() const override { return "WavyLineCache"; }

        virtual bool dropCaches() override
        {
            m_aItems.clear();
            return true;
        }

        virtual void dumpState(rtl::OStringBuffer& rState) override
        {
            rState.append("\nWavyLineCache:\t");
            rState.append(static_cast<sal_Int32>(m_aItems.size()));
        }

        private:
        struct WavyLineCacheItem
        {
            size_t m_aLineWidth;
            size_t m_aWordWidth;
            Bitmap m_Bitmap;
        };

        struct Key
        {
            size_t m_aFirst;
            size_t m_aSecond;
            bool operator ==( const Key& rOther ) const
            {
                return ( m_aFirst == rOther.m_aFirst && m_aSecond == rOther.m_aSecond );
            }
        };

        struct Hash
        {
            size_t operator() ( const Key& rKey ) const
            {
                size_t aSeed = 0;
                o3tl::hash_combine(aSeed, rKey.m_aFirst);
                o3tl::hash_combine(aSeed, rKey.m_aSecond);
                return aSeed;
            }
        };

        o3tl::lru_map< Key, WavyLineCacheItem, Hash > m_aItems;
    };
}

bool OutputDevice::shouldDrawWavePixelAsRect(tools::Long nLineWidth) const
{
    if (nLineWidth > 1)
        return true;

    return false;
}

void OutputDevice::SetWaveLineColors(Color const& rColor, tools::Long nLineWidth)
{
    // On printers that output pixel via DrawRect()
    if (nLineWidth > 1)
    {
        if (mpGraphicsState->mbLineColor || mbLineColorDirty)
        {
            mpGraphics->SetLineColor();
            mbLineColorDirty = true;
        }

        mpGraphics->SetFillColor( rColor );
        mbFillColorDirty = true;
    }
    else
    {
        mpGraphics->SetLineColor( rColor );
        mbLineColorDirty = true;
    }
}

Size OutputDevice::GetWaveLineSize(tools::Long nLineWidth) const
{
    if (nLineWidth > 1)
        return Size(nLineWidth, ((nLineWidth*GetDPIX())+(GetDPIY()/2))/GetDPIY());

    return Size(1, 1);
}

namespace
{

class WavePixelRegion
{
public:
    // VCL's standard wavy line has a 2-pixel flat top/bottom
    static constexpr tools::Long WAVE_PEAK_WIDTH = 2;
    // Screen coordinates: -1 moves UP towards the top of the screen
    static constexpr tools::Long DIRECTION_UP = -1;

    class iterator
    {
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = Point;
        using difference_type = std::ptrdiff_t;
        using pointer = Point*;
        using reference = Point&;

        iterator(tools::Long nX, tools::Long nY, tools::Long nWidth, tools::Long nHeight, bool bIsEnd)
            : m_nX(nX)
            , m_nY(nY + std::max<tools::Long>(nHeight - 1, 0)) // Start at the bottom of the bounding box
            , m_nRemainingWidth(bIsEnd ? 0 : nWidth)
            , m_nDiffX(WAVE_PEAK_WIDTH)
            , m_nDiffY(std::max<tools::Long>(nHeight - 1, 0))
            , m_nOffY(DIRECTION_UP)
            , m_nPhaseStep(0)
            , m_bInSlant(nHeight > 1) // If height > 1, we have a vertical span to slant through
        {}

        Point operator*() const { return Point(m_nX, m_nY); }

        iterator& operator++()
        {
            if (m_nRemainingWidth <= 0) return *this;

            m_nX++;
            m_nRemainingWidth--;

            if (m_nDiffY == 0)
                return *this; // Flat wave fallback

            // State machine to alternate between slanting and flat peaks
            if (m_bInSlant)
            {
                m_nY += m_nOffY;
                m_nPhaseStep++;
                if (m_nPhaseStep >= m_nDiffY)
                {
                    m_bInSlant = false;
                    m_nPhaseStep = 0;
                }
            }
            else
            {
                m_nPhaseStep++;
                if (m_nPhaseStep >= m_nDiffX)
                {
                    m_bInSlant = true;
                    m_nPhaseStep = 0;
                    m_nOffY = -m_nOffY; // Flip vertical direction for next slant
                }
            }
            return *this;
        }

        bool operator!=(const iterator& rOther) const
        {
            return m_nRemainingWidth != rOther.m_nRemainingWidth;
        }

    private:
        tools::Long m_nX, m_nY;
        tools::Long m_nRemainingWidth;
        tools::Long m_nDiffX, m_nDiffY;
        tools::Long m_nOffY;
        tools::Long m_nPhaseStep;
        bool m_bInSlant;
    };

    WavePixelRegion(tools::Long nStartX, tools::Long nStartY, tools::Long nWidth, tools::Long nHeight)
        : m_nStartX(nStartX), m_nStartY(nStartY), m_nWidth(nWidth), m_nHeight(nHeight)
    {}

    iterator begin() const { return iterator(m_nStartX, m_nStartY, m_nWidth, m_nHeight, false); }
    iterator end() const { return iterator(m_nStartX, m_nStartY, m_nWidth, m_nHeight, true); }

private:
    tools::Long m_nStartX, m_nStartY, m_nWidth, m_nHeight;
};
} // anonymous namespace

struct WaveLineGeometry
{
    Point maBase;           // The layout origin (used as the center of rotation)
    Point maStart;          // The actual start of the wave (Base + Dist)
    Size maSize;            // The Width and Height bounds of the wave

    Size maWavePixelSize;   // The physical width/height of the "brush"
    bool mbDrawAsRect;      // Respects the virtual shouldDrawWavePixelAsRect()
    Degree10 mnOrientation;

    WaveLineGeometry(tools::Long nBaseX, tools::Long nBaseY,
                     tools::Long nDistX, tools::Long nDistY,
                     tools::Long nWidth, tools::Long nHeight,
                     Degree10 nOrientation,
                     const Size& rWavePixelSize,
                     bool bDrawAsRect)
        : maBase(nBaseX, nBaseY)
        , maStart(nBaseX + nDistX, nBaseY + nDistY)
        , maSize(nWidth, nHeight)
        , maWavePixelSize(rWavePixelSize)
        , mbDrawAsRect(bDrawAsRect)
        , mnOrientation(nOrientation)
    {}

    // Factory method to generate the iterator range
    WavePixelRegion GetRegion() const
    {
        return WavePixelRegion(maStart.X(), maStart.Y(), maSize.Width(), maSize.Height());
    }

    Point GetLineStart() const
    {
        Point aLineStart = maStart;

        if (mnOrientation)
            maBase.RotateAround(aLineStart, mnOrientation);

        return aLineStart;
    }

    Point GetLineEnd() const
    {
        Point aLineEnd(maStart.X() + maSize.Width(), maStart.Y());

        if (mnOrientation)
            maBase.RotateAround(aLineEnd, mnOrientation);

        return aLineEnd;
    }
};

struct TextLineGeometry
{
    Point maOrigin;
    tools::Long mnDistX;
    double mfWidth;
    FontStrikeout meStrikeout;
    FontLineStyle meUnderline;
    FontLineStyle meOverline;
    bool mbUnderlineAbove;
    Color maUnderlineColor;
    Color maOverlineColor;
    Color maStrikeoutColor;

    TextLineGeometry(Point aOrigin, tools::Long nDistX, double fWidth,
                     FontStrikeout eStrikeout, FontLineStyle eUnderline,
                     FontLineStyle eOverline, bool bUnderlineAbove)
        : maOrigin(aOrigin), mnDistX(nDistX), mfWidth(fWidth)
        , meStrikeout(eStrikeout), meUnderline(eUnderline)
        , meOverline(eOverline), mbUnderlineAbove(bUnderlineAbove)
    {}
};

void vcl::rendercontext::PrimitiveRenderer::DrawWaveLineHairline(OutputDevice& rOutDev, const WaveLineGeometry& rGeo, const Color& rColor)
{
    rOutDev.mpGraphics->SetLineColor(rColor);
    rOutDev.mbLineColorDirty = true;

    const Point aLineStart = rGeo.GetLineStart();
    const Point aLineEnd = rGeo.GetLineEnd();

    rOutDev.mpGraphics->DrawLine(aLineStart.X(), aLineStart.Y(), aLineEnd.X(), aLineEnd.Y(), rOutDev);
}

void vcl::rendercontext::PrimitiveRenderer::DrawWaveLineRasterized(OutputDevice& rOutDev, const WaveLineGeometry& rGeo, const Color& rColor)
{
    rOutDev.SetWaveLineColors(rColor, rGeo.maWavePixelSize.Height());

    for (Point aDrawPt : rGeo.GetRegion())
    {
        if (rGeo.mnOrientation)
            rGeo.maBase.RotateAround(aDrawPt, rGeo.mnOrientation);

        if (rGeo.mbDrawAsRect)
        {
            rOutDev.mpGraphics->DrawRect(aDrawPt.X(), aDrawPt.Y(),
                                 rGeo.maWavePixelSize.Width(),
                                 rGeo.maWavePixelSize.Height(),
                                 rOutDev);
        }
        else
        {
            rOutDev.mpGraphics->DrawPixel(aDrawPt.X(), aDrawPt.Y(), rOutDev);
        }
    }
}

void vcl::rendercontext::PrimitiveRenderer::DrawWaveLine(OutputDevice& rOutDev, const WaveLineGeometry& rGeo, const Color& rColor)
{
    if (rGeo.maWavePixelSize.Height() == 1 && rGeo.maSize.Height() == 1)
    {
        vcl::rendercontext::PrimitiveRenderer::DrawWaveLineHairline(rOutDev, rGeo, rColor);
        return;
    }

    vcl::rendercontext::PrimitiveRenderer::DrawWaveLineRasterized(rOutDev, rGeo, rColor);
}

void vcl::rendercontext::PrimitiveRenderer::DrawWaveTextLine(OutputDevice& rOutDev, const TextLineGeometry& rGeo, tools::Long nY, Color aColor, bool bIsAbove)
{
    vcl::text::WaveLineGeometry aWaveStyle = vcl::text::TextDecorator::CalculateWaveLineGeometry(
        *rOutDev.mpFontInstance->mxFontMetric, rGeo.meUnderline, bIsAbove, nY, rOutDev.GetDPIX(), rOutDev.GetDPIY());

    const Size aWavePixelSize = rOutDev.GetWaveLineSize(aWaveStyle.nLineWidth);
    const bool bDrawAsRect = rOutDev.shouldDrawWavePixelAsRect(aWaveStyle.nLineWidth);

    Degree10 nOrientation = rOutDev.mpFontInstance->mnOrientation;

    for (const auto& rSeg : aWaveStyle.aSegments)
    {
        WaveLineGeometry aWaveGeo(rGeo.maOrigin.X(), rGeo.maOrigin.Y(), rGeo.mnDistX, rSeg.nYOffset,
                                  rGeo.mfWidth, rSeg.nHeight,
                                  nOrientation, aWavePixelSize, bDrawAsRect);

        PrimitiveRenderer::DrawWaveLine(rOutDev, aWaveGeo, aColor);
    }
}

void vcl::rendercontext::PrimitiveRenderer::DrawStraightTextLine(OutputDevice& rOutDev, const TextLineGeometry& rGeo, tools::Long nY, Color aColor, bool bIsAbove)
{
    static bool bFuzzing = comphelper::IsFuzzing();
    if (bFuzzing && rGeo.mfWidth > 25000)
    {
        SAL_WARN("vcl.gdi", "drawLine, skipping suspicious TextLine of length: "
                                << rGeo.mfWidth << " for fuzzing performance");
        return;
    }

    // Ask TextDecorator to calculate the metrics based on the font data
    vcl::text::StraightLineMetrics aMetrics(*rOutDev.mpFontInstance->mxFontMetric, rGeo.meUnderline, nY, bIsAbove);

    if (!aMetrics.nLineHeight)
        return;

    if (rOutDev.mpGraphicsState->mbLineColor || rOutDev.mbLineColorDirty)
    {
        rOutDev.mpGraphics->SetLineColor();
        rOutDev.mbLineColorDirty = true;
    }

    rOutDev.mpGraphics->SetFillColor(aColor);
    rOutDev.mbFillColorDirty = true;

    tools::Long nLeft = rGeo.mnDistX;

    // Dispatch to the actual rendering calls using the sanitized metrics
    switch (aMetrics.eUnderline)
    {
    case LINESTYLE_SINGLE:
    case LINESTYLE_BOLD:
        vcl::rendercontext::PrimitiveRenderer::DrawTextRect(*rOutDev.mpGraphics, &rOutDev, rGeo.maOrigin, tools::Rectangle(Point(nLeft, aMetrics.nLinePos), Size(rGeo.mfWidth, aMetrics.nLineHeight)), rOutDev.mpFontRealization->mxFont->mnOrientation);
        break;
    case LINESTYLE_DOUBLE:
        vcl::rendercontext::PrimitiveRenderer::DrawTextRect(*rOutDev.mpGraphics, &rOutDev, rGeo.maOrigin, tools::Rectangle(Point(nLeft, aMetrics.nLinePos), Size(rGeo.mfWidth, aMetrics.nLineHeight)), rOutDev.mpFontRealization->mxFont->mnOrientation);
        vcl::rendercontext::PrimitiveRenderer::DrawTextRect(*rOutDev.mpGraphics, &rOutDev, rGeo.maOrigin, tools::Rectangle(Point(nLeft, aMetrics.nLinePos2), Size(rGeo.mfWidth, aMetrics.nLineHeight)), rOutDev.mpFontRealization->mxFont->mnOrientation);
        break;
    default:
        {
            std::vector<vcl::text::TextDashSegment> aSegments =
                vcl::text::TextDecorator::CalculateTextLineSegments(rGeo.mfWidth, aMetrics.eUnderline, aMetrics.nLineHeight, rOutDev.GetDPIX(), rOutDev.GetDPIY());

            for (const auto& rSeg : aSegments)
            {
                vcl::rendercontext::PrimitiveRenderer::DrawTextRect(*rOutDev.mpGraphics, &rOutDev, rGeo.maOrigin, tools::Rectangle(Point(nLeft + rSeg.nX, aMetrics.nLinePos), Size(rSeg.nWidth, aMetrics.nLineHeight)), rOutDev.mpFontRealization->mxFont->mnOrientation);

            }
        }
        break;
    }
}

void vcl::rendercontext::PrimitiveRenderer::DrawStrikeoutLine(OutputDevice& rOutDev, const TextLineGeometry& rGeo, tools::Long nY, Color aColor)
{
    if (!rGeo.mfWidth)
        return;

    vcl::text::StrikeoutGeometry aGeo = vcl::text::TextDecorator::CalculateStrikeoutGeometry(
        *rOutDev.mpFontInstance->mxFontMetric, rGeo.meStrikeout, nY);

    if (aGeo.aSegments.empty())
        return;

    if (rOutDev.mpGraphicsState->mbLineColor || rOutDev.mbLineColorDirty)
    {
        rOutDev.mpGraphics->SetLineColor();
        rOutDev.mbLineColorDirty = true;
    }

    rOutDev.mpGraphics->SetFillColor(aColor);
    rOutDev.mbFillColorDirty = true;

    for (const auto& rSeg : aGeo.aSegments)
    {
        vcl::rendercontext::PrimitiveRenderer::DrawTextRect(*rOutDev.mpGraphics, &rOutDev, rGeo.maOrigin, tools::Rectangle(Point(rGeo.mnDistX, rSeg.nYOffset), Size(rGeo.mfWidth, rSeg.nHeight)), rOutDev.mpFontRealization->mxFont->mnOrientation);
    }
}

void vcl::rendercontext::PrimitiveRenderer::DrawStrikeoutChar(OutputDevice& rOutDev, const TextLineGeometry& rGeo, tools::Long nY, Color aColor)
{
    if (!rGeo.mfWidth)
        return;

    vcl::text::LayoutResources aRes{
        rOutDev.mpFontInstance.get(),
        *rOutDev.mpMapper,
        &rOutDev.GetFontCache(),
        rOutDev.GetFontCollection(),
        nullptr, // pForcedFallback
        [&]() { return rOutDev.mpGraphics; },
        rOutDev.IsRTLEnabled(),
        false, // bSubpixelPositioning
        *rOutDev.mpGraphicsState,
        *rOutDev.mpFontRealization
    };

    std::unique_ptr<SalLayout> pLayout = vcl::text::TextGeometry::GetStrikeoutCharLayout(
        aRes, rGeo.mfWidth, rGeo.meStrikeout);

    if (!pLayout)
        return;

    Point aOriginPt = rGeo.maOrigin;
    if (rGeo.mnDistX || nY)
    {
        tools::Long nTmpX = rGeo.mnDistX;
        tools::Long nTmpY = nY;

        if (rOutDev.mpFontInstance->mnOrientation)
        {
            Point aPivot(0, 0);
            aPivot.RotateAround(nTmpX, nTmpY, rOutDev.mpFontInstance->mnOrientation);
        }

        aOriginPt.AdjustX(nTmpX);
        aOriginPt.AdjustY(nTmpY);
    }

    const Color aOldColor = rOutDev.GetTextColor();
    rOutDev.SetTextColor(aColor);
    rOutDev.ImplInitTextColor();

    // CRITICAL FIX: rGeo.maOrigin already contains rOutDev.mpFontRealization offsets!
    // Do not add them again here, otherwise strikeout characters render completely out of bounds.
    pLayout->DrawBase() = basegfx::B2DPoint(aOriginPt.X(), aOriginPt.Y());

    // Fix the clipping rectangle to also use the un-shifted origin
    tools::Rectangle aPixelRect;
    aPixelRect.SetLeft(aOriginPt.X());
    aPixelRect.SetRight(aPixelRect.Left() + rGeo.mfWidth);
    aPixelRect.SetBottom(aOriginPt.Y() + rOutDev.mpFontInstance->mxFontMetric->GetDescent());
    aPixelRect.SetTop(aOriginPt.Y() - rOutDev.mpFontInstance->mxFontMetric->GetAscent());

    if (rOutDev.mpFontInstance->mnOrientation)
    {
        tools::Polygon aPoly(aPixelRect);
        aPoly.Rotate(aOriginPt, rOutDev.mpFontInstance->mnOrientation);
        aPixelRect = aPoly.GetBoundRect();
    }

    pLayout->DrawText(*rOutDev.mpGraphics);

    rOutDev.SetTextColor(aOldColor);
    rOutDev.ImplInitTextColor();
}

void vcl::rendercontext::PrimitiveRenderer::DrawTextLine(OutputDevice& rOutDev, const TextLineGeometry& rGeo)
{
    // Ask TextDecorator to calculate the vertical offsets based on the font data
    vcl::text::TextLineOffsetInfo aInfo(*rOutDev.mpFontInstance->mxFontMetric, rGeo.meUnderline, rGeo.meOverline, rGeo.mbUnderlineAbove);

    Color aStrikeoutColor = rOutDev.GetTextColor();
    Color aUnderlineColor = rOutDev.GetTextLineColor();
    Color aOverlineColor  = rOutDev.GetOverlineColor();

    if (!rOutDev.IsTextLineColor())
        aUnderlineColor = rOutDev.GetTextColor();

    if (!rOutDev.IsOverlineColor())
        aOverlineColor = rOutDev.GetTextColor();

    TextLineGeometry aDrawGeo = rGeo;
    if (rOutDev.IsRTLEnabled())
    {
        tools::Long nXAdd = aDrawGeo.mfWidth - aDrawGeo.mnDistX;
        if (rOutDev.mpFontInstance->mnOrientation)
            nXAdd = basegfx::fround<tools::Long>(nXAdd * cos(toRadians(rOutDev.mpFontInstance->mnOrientation)));
        aDrawGeo.maOrigin.AdjustX(nXAdd - 1);
    }

    if (aDrawGeo.meUnderline != LINESTYLE_NONE)
    {
        if (aInfo.bUnderlineIsWave)
            vcl::rendercontext::PrimitiveRenderer::DrawWaveTextLine(rOutDev, aDrawGeo, aInfo.nUnderlineOffset, aUnderlineColor, aDrawGeo.mbUnderlineAbove);
        else
            // Straight lines manage their own offsets mathematically; pass 0
            vcl::rendercontext::PrimitiveRenderer::DrawStraightTextLine(rOutDev, aDrawGeo, 0, aUnderlineColor, aDrawGeo.mbUnderlineAbove);
    }

    if (aDrawGeo.meOverline != LINESTYLE_NONE)
    {
        // Trick the sub-routines into rendering the overline
        TextLineGeometry aOverlineGeo = aDrawGeo;
        aOverlineGeo.meUnderline = aDrawGeo.meOverline;

        if (aInfo.bOverlineIsWave)
            vcl::rendercontext::PrimitiveRenderer::DrawWaveTextLine(rOutDev, aOverlineGeo, aInfo.nOverlineOffset, aOverlineColor, true);
        else
            // Straight lines manage their own offsets mathematically; pass 0
            vcl::rendercontext::PrimitiveRenderer::DrawStraightTextLine(rOutDev, aOverlineGeo, 0, aOverlineColor, true);
    }

    if (aDrawGeo.meStrikeout != STRIKEOUT_NONE)
    {
        if (aDrawGeo.meStrikeout == STRIKEOUT_SLASH || aDrawGeo.meStrikeout == STRIKEOUT_X)
            vcl::rendercontext::PrimitiveRenderer::DrawStrikeoutChar(rOutDev, aDrawGeo, 0, aStrikeoutColor);
        else
            vcl::rendercontext::PrimitiveRenderer::DrawStrikeoutLine(rOutDev, aDrawGeo, aInfo.nStrikeoutOffset, aStrikeoutColor);
    }
}

void vcl::rendercontext::PrimitiveRenderer::DrawTextLines(OutputDevice& rOutDev, SalLayout& rSalLayout, FontStrikeout eStrikeout,
                                      FontLineStyle eUnderline, FontLineStyle eOverline,
                                      bool bWordLine, bool bUnderlineAbove )
{
    if( bWordLine )
    {
        const basegfx::B2DPoint aStartPt = rSalLayout.DrawBase();
        std::vector<std::pair<double, double>> aSegments;
        vcl::text::TextGeometry::GetWordLineSegments(rSalLayout, *rOutDev.mpFontRealization, aSegments);
        for (const auto& rSeg : aSegments)
        {
            {
                TextLineGeometry aLineGeo(Point(aStartPt.getX(), aStartPt.getY()), static_cast<tools::Long>(rSeg.first), rSeg.second, eStrikeout, eUnderline, eOverline, bUnderlineAbove);
                aLineGeo.maUnderlineColor = rOutDev.GetTextLineColor();
                vcl::rendercontext::PrimitiveRenderer::DrawTextLine(rOutDev, aLineGeo);
            }
        }
    }
    else
    {
        basegfx::B2DPoint aStartPt = rSalLayout.GetDrawPosition();
        {
            TextLineGeometry aLineGeo(Point(aStartPt.getX(), aStartPt.getY()), 0, rSalLayout.GetTextWidth(), eStrikeout, eUnderline, eOverline, bUnderlineAbove);
            aLineGeo.maUnderlineColor = rOutDev.GetTextLineColor();
            vcl::rendercontext::PrimitiveRenderer::DrawTextLine(rOutDev, aLineGeo);
        }
    }
}

void vcl::rendercontext::PrimitiveRenderer::DrawMnemonicLine(OutputDevice& rOutDev, tools::Long nX, tools::Long nY, tools::Long nWidth )
{
    tools::Long nBaseX = nX;
    if( /*HasMirroredGraphics() &&*/ rOutDev.IsRTLEnabled() )
    {
        // revert the hack that will be done later in ImplDrawTextLine
        nX = nBaseX - nWidth - (nX - nBaseX - 1);
    }

    {
        TextLineGeometry aLineGeo(Point(nX, nY), 0, nWidth, STRIKEOUT_NONE, LINESTYLE_SINGLE, LINESTYLE_NONE, false);
        aLineGeo.maUnderlineColor = rOutDev.GetTextLineColor();
        vcl::rendercontext::PrimitiveRenderer::DrawTextLine(rOutDev, aLineGeo);
    }
}

const Color& OutputDevice::GetTextLineColor() const
{
    return mpGraphicsState->maTextLineColor;
}

bool OutputDevice::IsTextLineColor() const
{
    return !mpGraphicsState->maTextLineColor.IsTransparent();
}

void OutputDevice::SetTextLineColor()
{
    maRecorder.RecordTextLineColor(Color(), false);

    mpGraphicsState->maTextLineColor = COL_TRANSPARENT;
}

void OutputDevice::SetTextLineColor( const Color& rColor )
{
    Color aColor(vcl::drawmode::GetTextColor(rColor, GetDrawMode(), GetSettings().GetStyleSettings()));

    maRecorder.RecordTextLineColor(aColor, true);

    mpGraphicsState->maTextLineColor = aColor;
}

const Color& OutputDevice::GetOverlineColor() const
{
    return mpGraphicsState->maOverlineColor;
}

bool OutputDevice::IsOverlineColor() const
{
    return !mpGraphicsState->maOverlineColor.IsTransparent();
}

void OutputDevice::SetOverlineColor()
{
    maRecorder.RecordOverlineColor(Color(), false);

    mpGraphicsState->maOverlineColor = COL_TRANSPARENT;
}

void OutputDevice::SetOverlineColor( const Color& rColor )
{
    Color aColor(vcl::drawmode::GetTextColor(rColor, GetDrawMode(), GetSettings().GetStyleSettings()));

    maRecorder.RecordOverlineColor(aColor, true);

    mpGraphicsState->maOverlineColor = aColor;
}

static constexpr bool lcl_HasNoTextDecoration(FontLineStyle eUnderline, FontLineStyle eOverline, FontStrikeout eStrikeout)
{
    return (eUnderline == LINESTYLE_NONE || eUnderline == LINESTYLE_DONTKNOW) &&
           (eOverline  == LINESTYLE_NONE || eOverline  == LINESTYLE_DONTKNOW) &&
           (eStrikeout == STRIKEOUT_NONE || eStrikeout == STRIKEOUT_DONTKNOW);
}

void OutputDevice::DrawTextLine( const Point& rPos, tools::Long nWidth,
                                 FontStrikeout eStrikeout,
                                 FontLineStyle eUnderline,
                                 FontLineStyle eOverline )
{
    assert(!is_double_buffered_window());

    if (lcl_HasNoTextDecoration(eUnderline, eOverline, eStrikeout))
        return;

    maRecorder.RecordTextLine(rPos, nWidth, eStrikeout, eUnderline, eOverline);

    if ( !IsDeviceOutputNecessary() || IsLayoutCalculationNecessary() )
        return;

    if ( mpClippingController->IsDirty() )
        InitClipRegion();

    if ( IsOutputCulled() )
        return;

    // initialize font if needed to get text offsets
    // TODO: only needed for mnTextOff!=(0,0)
    if (!InitFont())
        return;

    Point aPos = LogicToDevicePixel(rPos);
    double fWidth = LogicWidthToDeviceSubPixel(nWidth);
    aPos += Point( mpFontRealization->nXOffset, mpFontRealization->nYOffset );
    {
        TextLineGeometry aLineGeo(aPos, 0, fWidth, eStrikeout, eUnderline, eOverline, false);
        aLineGeo.maUnderlineColor = GetTextLineColor();
        vcl::rendercontext::PrimitiveRenderer::DrawTextLine(*this, aLineGeo);
    }
}

void OutputDevice::DrawWaveLine(const Point& rStartPos, const Point& rEndPos, tools::Long nLineWidth, tools::Long nWaveHeight)
{
    assert(!is_double_buffered_window());

    if ( !IsDeviceOutputNecessary() || IsLayoutCalculationNecessary() )
        return;

    // we need a graphics
    if( !mpGraphics && !AcquireGraphics() )
        return;
    assert(mpGraphics);

    if ( mpClippingController->IsDirty() )
        InitClipRegion();

    if ( IsOutputCulled() )
        return;

    if (!InitFont())
        return;

    Point aStartPt = LogicToDevicePixel(rStartPos);
    Point aEndPt = LogicToDevicePixel(rEndPos);

    tools::Long nStartX = aStartPt.X();
    tools::Long nStartY = aStartPt.Y();
    tools::Long nEndX = aEndPt.X();
    tools::Long nEndY = aEndPt.Y();
    double fOrientation = 0.0;

    // handle rotation
    if (nStartY != nEndY || nStartX > nEndX)
    {
        fOrientation = basegfx::rad2deg(std::atan2(nStartY - nEndY, nEndX - nStartX));
        // un-rotate the end point
        aStartPt.RotateAround(nEndX, nEndY, Degree10(static_cast<sal_Int16>(-fOrientation * 10.0)));
    }

    // Handle HiDPI
    float fScaleFactor = GetDPIScaleFactor();
    if (fScaleFactor > 1.0f)
    {
        nWaveHeight *= fScaleFactor;

        nStartY += fScaleFactor - 1; // Shift down additional pixel(s) to create more visual separation.

        // odd heights look better than even
        if (nWaveHeight % 2 == 0)
        {
            nWaveHeight--;
        }
    }

    // #109280# make sure the waveline does not exceed the descent to avoid paint problems
    LogicalFontInstance* pFontInstance = mpFontInstance.get();
    if (nWaveHeight > pFontInstance->mxFontMetric->GetWavelineUnderlineSize()
    // tdf#153223 polyline with lineheight >0 not drawn when skia is off
#ifdef MACOSX
        || !SkiaHelper::isVCLSkiaEnabled()
#endif
       )
    {
        nWaveHeight = pFontInstance->mxFontMetric->GetWavelineUnderlineSize();
        // tdf#124848 hairline
        nLineWidth = 0;
    }

    if ( fOrientation == 0.0 )
    {
        static tools::DeleteOnDeinit< WavyLineCache > snLineCache {};
        if ( !snLineCache.get() )
            return;
        WavyLineCache& rLineCache = *snLineCache.get();
        Bitmap aWavylinebmp;
        if ( !rLineCache.find( GetLineColor(), nLineWidth, nWaveHeight, nEndX - nStartX, aWavylinebmp ) )
        {
            size_t nWordLength = nEndX - nStartX;
            // start with something big to avoid updating it frequently
            nWordLength = nWordLength < 1024 ? 1024 : nWordLength;
            ScopedVclPtrInstance< VirtualDevice > pVirtDev( *this, DeviceFormat::WITH_ALPHA );
            pVirtDev->SetOutputSizePixel( Size( nWordLength, nWaveHeight * 2 ), false );
            pVirtDev->SetLineColor( GetLineColor() );
            pVirtDev->SetBackground( Wallpaper( COL_TRANSPARENT ) );
            pVirtDev->Erase();
            pVirtDev->SetAntialiasing( AntialiasingFlags::Enable );
            vcl::rendercontext::PrimitiveRenderer::DrawWaveLineBezier(*pVirtDev, 0, 0, nWordLength, 0, nWaveHeight, fOrientation, nLineWidth);
            Bitmap aBitmap(pVirtDev->GetBitmap(Point(0, 0), pVirtDev->GetOutputSize()));

            rLineCache.insert( aBitmap, GetLineColor(), nLineWidth, nWaveHeight, nWordLength, aWavylinebmp );
        }
        if ( aWavylinebmp.ImplGetSalBitmap() != nullptr )
        {
            Size _size( nEndX - nStartX, aWavylinebmp.GetSizePixel().Height() );
            DrawBitmap(Point( rStartPos.X(), rStartPos.Y() ), PixelToLogic( _size ), Point(), _size, aWavylinebmp);
        }
        return;
    }

    vcl::rendercontext::PrimitiveRenderer::DrawWaveLineBezier(*this, nStartX, nStartY, nEndX, nEndY, nWaveHeight, fOrientation, nLineWidth);
}

void vcl::rendercontext::PrimitiveRenderer::DrawWaveLineBezier(OutputDevice& rOutDev, tools::Long nStartX, tools::Long nStartY, tools::Long nEndX, tools::Long nEndY, tools::Long nWaveHeight, double fOrientation, tools::Long nLineWidth)
{
    // we need a graphics
    if( !rOutDev.mpGraphics && !rOutDev.AcquireGraphics() )
        return;
    assert(rOutDev.mpGraphics);

    if ( rOutDev.mpClippingController->IsDirty() )
        rOutDev.InitClipRegion();

    if ( rOutDev.IsOutputCulled() )
        return;

    if (!rOutDev.InitFont())
        return;

    const basegfx::B2DRectangle aWaveLineRectangle(nStartX, nStartY, nEndX, nEndY + nWaveHeight);
    const basegfx::B2DPolygon aWaveLinePolygon = basegfx::createWaveLinePolygon(aWaveLineRectangle);
    const basegfx::B2DHomMatrix aRotationMatrix = basegfx::utils::createRotateAroundPoint(nStartX, nStartY, basegfx::deg2rad(-fOrientation));
    const bool bPixelSnapHairline(rOutDev.mpGraphicsState->mnAntialiasing & AntialiasingFlags::PixelSnapHairline);

    rOutDev.mpGraphics->SetLineColor(rOutDev.GetLineColor());
    rOutDev.mpGraphics->DrawPolyLine(
            aRotationMatrix,
            aWaveLinePolygon,
            0.0,
            nLineWidth,
            nullptr, // MM01
            basegfx::B2DLineJoin::NONE,
            css::drawing::LineCap_BUTT,
            basegfx::deg2rad(15.0),
            bPixelSnapHairline,
            rOutDev);
}

void OutputDevice::ImplDrawEmphasisMark(tools::Long nBaseX, tools::Long nX, tools::Long nY,
                                        const tools::PolyPolygon& rPolyPoly, bool bPolyLine,
                                        const tools::Rectangle& rRect1,
                                        const tools::Rectangle& rRect2)
{
    if (IsRTLEnabled())
        nX = nBaseX - (nX - nBaseX - 1);

    nX -= GetOutOffXPixel();
    nY -= GetOutOffYPixel();

    if (rPolyPoly.Count())
    {
        if (bPolyLine)
        {
            tools::Polygon aPoly = rPolyPoly.GetObject(0);
            aPoly.Move(nX, nY);
            DrawPolyLine(aPoly);
        }
        else
        {
            tools::PolyPolygon aPolyPoly = rPolyPoly;
            aPolyPoly.Move(nX, nY);
            DrawPolyPolygon(aPolyPoly);
        }
    }

    if (!rRect1.IsEmpty())
    {
        tools::Rectangle aRect(Point(nX + rRect1.Left(), nY + rRect1.Top()), rRect1.GetSize());
        DrawRect(aRect);
    }

    if (!rRect2.IsEmpty())
    {
        tools::Rectangle aRect(Point(nX + rRect2.Left(), nY + rRect2.Top()), rRect2.GetSize());
        DrawRect(aRect);
    }
}

void OutputDevice::ImplDrawEmphasisMarks(SalLayout& rSalLayout)
{
    vcl::font::FontRealization const* pRealization = mpFontRealization.get();
    if (!pRealization || !pRealization->mxFont)
        return;

    auto popIt = ScopedPush(vcl::PushFlags::FILLCOLOR | vcl::PushFlags::LINECOLOR | vcl::PushFlags::MAPMODE);
    vcl::MetafileRecorder::ScopedSuspend aMetaFileSuspend(maRecorder);
    mpMapper->EnableMapMode(false);

    FontEmphasisMark nEmphasisMark = mpGraphicsState->maFont.GetEmphasisMarkStyle();
    const bool bBelow = bool(nEmphasisMark & FontEmphasisMark::PosBelow);

    tools::Long nEmphasisHeight = bBelow ? pRealization->nEmphasisDescent : pRealization->nEmphasisAscent;
    vcl::font::EmphasisMark aEmphasisMark(nEmphasisMark, nEmphasisHeight, GetDPIY());

    if (aEmphasisMark.IsShapePolyLine())
    {
        SetLineColor(GetTextColor());
        SetFillColor();
    }
    else
    {
        SetLineColor();
        SetFillColor(GetTextColor());
    }

    std::vector<Point> aPositions;
    vcl::text::TextDecorator::GetEmphasisMarkPositions(rSalLayout, *pRealization, aEmphasisMark, bBelow, aPositions);

    // Draw the marks at the final calculated positions
    for (const Point& rPos : aPositions)
    {
        ImplDrawEmphasisMark(rSalLayout.DrawBase().getX(), rPos.X(), rPos.Y(),
                             aEmphasisMark.GetShape(), aEmphasisMark.IsShapePolyLine(),
                             aEmphasisMark.GetRect1(), aEmphasisMark.GetRect2());
    }
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
