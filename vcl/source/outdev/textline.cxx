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
#include <vcl/rendercontext/WaveLineGeometry.hxx>
#include <vcl/rendercontext/TextLineGeometry.hxx>
#include <vcl/settings.hxx>
#include <vcl/text/TextDecorator.hxx>
#include <vcl/text/TextGeometry.hxx>
#include <vcl/text/TextLineGeometry.hxx>
#include <vcl/virdev.hxx>

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

    if (!PrepareGraphicsOutput(vcl::PrepareOutputFlags::Clip | vcl::PrepareOutputFlags::Font))
        return;

    Point aPos = LogicToDevicePixel(rPos);
    double fWidth = LogicWidthToDeviceSubPixel(nWidth);
    aPos += Point( mpFontRealization->nXOffset, mpFontRealization->nYOffset );

    vcl::rendercontext::TextLineGeometry aLineGeo(aPos, 0, fWidth, eStrikeout, eUnderline, eOverline, false);
    aLineGeo.maUnderlineColor = GetTextLineColor();

    vcl::rendercontext::PrimitiveRenderer::DrawTextLine(*this, aLineGeo);
}

static void lcl_DrawCachedHorizontalWaveLine(OutputDevice& rOutDev, const Point& rStartPos,
                                             tools::Long nStartX, tools::Long nEndX,
                                             tools::Long nWaveHeight, tools::Long nLineWidth)
{
    static tools::DeleteOnDeinit< WavyLineCache > snLineCache {};
    if (!snLineCache.get())
        return;

    WavyLineCache& rLineCache = *snLineCache.get();
    Bitmap aWavylinebmp;
    Color aLineColor = rOutDev.GetLineColor();

    if (!rLineCache.find(aLineColor, nLineWidth, nWaveHeight, nEndX - nStartX, aWavylinebmp))
    {
        size_t nWordLength = nEndX - nStartX;

        // start with something big to avoid updating it frequently
        nWordLength = nWordLength < 1024 ? 1024 : nWordLength;

        ScopedVclPtrInstance<VirtualDevice> pVirtDev(rOutDev, DeviceFormat::WITH_ALPHA);
        pVirtDev->SetOutputSizePixel(Size(nWordLength, nWaveHeight * 2), false);
        pVirtDev->SetLineColor(aLineColor);
        pVirtDev->SetBackground( Wallpaper(COL_TRANSPARENT));
        pVirtDev->Erase();
        pVirtDev->SetAntialiasing( AntialiasingFlags::Enable );

        // Render the vector to the virtual canvas
        vcl::rendercontext::PrimitiveRenderer::DrawWaveLineBezier(*pVirtDev, *pVirtDev->GetGraphics(),
                                                                  0, 0, nWordLength, 0, nWaveHeight, 0.0, nLineWidth);

        Bitmap aBitmap(pVirtDev->GetBitmap(Point(0, 0), pVirtDev->GetOutputSize()));
        rLineCache.insert(aBitmap, aLineColor, nLineWidth, nWaveHeight, nWordLength, aWavylinebmp);
    }

    if (aWavylinebmp.ImplGetSalBitmap() != nullptr)
    {
        Size aSize(nEndX - nStartX, aWavylinebmp.GetSizePixel().Height());
        rOutDev.DrawBitmap(Point(rStartPos.X(), rStartPos.Y()), rOutDev.PixelToLogic(aSize), Point(), aSize, aWavylinebmp);
    }
}

void OutputDevice::DrawWaveLine(const Point& rStartPos, const Point& rEndPos, tools::Long nLineWidth, tools::Long nWaveHeight)
{
    assert(!is_double_buffered_window());

    if (!PrepareGraphicsOutput(vcl::PrepareOutputFlags::Clip | vcl::PrepareOutputFlags::Font))
        return;

    Point aStartPt = LogicToDevicePixel(rStartPos);
    Point aEndPt = LogicToDevicePixel(rEndPos);

    tools::Long nStartX = aStartPt.X();
    tools::Long nStartY = aStartPt.Y();
    tools::Long nEndX = aEndPt.X();
    tools::Long nEndY = aEndPt.Y();
    double fOrientation = vcl::text::TextGeometry::CalculateWaveLineOrientation(aStartPt, aEndPt);

    // Handle HiDPI
    float fScaleFactor = GetDPIScaleFactor();
    if (fScaleFactor > 1.0f)
    {
        nWaveHeight *= fScaleFactor;

        nStartY += fScaleFactor - 1; // Shift down additional pixel(s) to create more visual separation.

        // odd heights look better than even
        if (nWaveHeight % 2 == 0)
            nWaveHeight--;
    }

    LogicalFontInstance* pFontInstance = mpFontInstance.get();
    vcl::text::TextDecorator::SanitizeWaveLineHeight(nWaveHeight, nLineWidth, *pFontInstance->mxFontMetric);

    if (fOrientation == 0.0)
    {
        lcl_DrawCachedHorizontalWaveLine(*this, rStartPos, nStartX, nEndX, nWaveHeight, nLineWidth);
        return;
    }

    vcl::rendercontext::PrimitiveRenderer::DrawWaveLineBezier(*this, *mpGraphics, nStartX, nStartY, nEndX, nEndY, nWaveHeight, fOrientation, nLineWidth);
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
