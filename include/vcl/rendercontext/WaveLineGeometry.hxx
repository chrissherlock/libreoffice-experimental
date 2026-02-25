/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <tools/gen.hxx>
#include <tools/degree.hxx>
#include <tools/color.hxx>

#include <algorithm>
#include <iterator>

namespace vcl::rendercontext
{
class WavePixelRegion
{
public:
    static constexpr tools::Long WAVE_PEAK_WIDTH = 2;
    static constexpr tools::Long DIRECTION_UP = -1;

    class iterator
    {
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = Point;
        using difference_type = std::ptrdiff_t;
        using pointer = Point*;
        using reference = Point&;

        iterator(tools::Long nX, tools::Long nY, tools::Long nWidth, tools::Long nHeight,
                 bool bIsEnd)
            : m_nX(nX)
            , m_nY(nY + std::max<tools::Long>(nHeight - 1, 0))
            , m_nRemainingWidth(bIsEnd ? 0 : nWidth)
            , m_nDiffX(WAVE_PEAK_WIDTH)
            , m_nDiffY(std::max<tools::Long>(nHeight - 1, 0))
            , m_nOffY(DIRECTION_UP)
            , m_nPhaseStep(0)
            , m_bInSlant(nHeight > 1)
        {
        }

        Point operator*() const { return Point(m_nX, m_nY); }

        iterator& operator++()
        {
            if (m_nRemainingWidth <= 0)
                return *this;
            m_nX++;
            m_nRemainingWidth--;
            if (m_nDiffY == 0)
                return *this;
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
                    m_nOffY = -m_nOffY;
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

    WavePixelRegion(tools::Long nStartX, tools::Long nStartY, tools::Long nWidth,
                    tools::Long nHeight)
        : m_nStartX(nStartX)
        , m_nStartY(nStartY)
        , m_nWidth(nWidth)
        , m_nHeight(nHeight)
    {
    }

    iterator begin() const { return iterator(m_nStartX, m_nStartY, m_nWidth, m_nHeight, false); }
    iterator end() const { return iterator(m_nStartX, m_nStartY, m_nWidth, m_nHeight, true); }

private:
    tools::Long m_nStartX, m_nStartY, m_nWidth, m_nHeight;
};

struct WaveLineGeometry
{
    Point maBase;
    Point maStart;
    Size maSize;
    Size maWavePixelSize;
    bool mbDrawAsRect;
    Degree10 mnOrientation;

    WaveLineGeometry(tools::Long nBaseX, tools::Long nBaseY, tools::Long nDistX, tools::Long nDistY,
                     tools::Long nWidth, tools::Long nHeight, Degree10 nOrientation,
                     const Size& rWavePixelSize, bool bDrawAsRect)
        : maBase(nBaseX, nBaseY)
        , maStart(nBaseX + nDistX, nBaseY + nDistY)
        , maSize(nWidth, nHeight)
        , maWavePixelSize(rWavePixelSize)
        , mbDrawAsRect(bDrawAsRect)
        , mnOrientation(nOrientation)
    {
    }

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

} // namespace vcl::rendercontext

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
