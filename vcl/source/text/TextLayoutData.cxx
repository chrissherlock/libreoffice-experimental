/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <vcl/text/TextLayoutData.hxx>
#include <vcl/ctrl.hxx>
#include <o3tl/safeint.hxx>

namespace vcl::text
{
TextLayoutData::TextLayoutData()
    : m_pParent(nullptr)
{
}

tools::Rectangle TextLayoutData::GetCharacterBounds(tools::Long nIndex) const
{
    return (nIndex >= 0 && o3tl::make_unsigned(nIndex) < m_aUnicodeBoundRects.size())
               ? m_aUnicodeBoundRects[nIndex]
               : tools::Rectangle();
}

tools::Long TextLayoutData::GetIndexForPoint(const Point& rPoint) const
{
    tools::Long nIndex = -1;
    for (tools::Long i = m_aUnicodeBoundRects.size() - 1; i >= 0; i--)
    {
        Point aTopLeft = m_aUnicodeBoundRects[i].TopLeft();
        Point aBottomRight = m_aUnicodeBoundRects[i].BottomRight();
        if (rPoint.X() >= aTopLeft.X() && rPoint.Y() >= aTopLeft.Y()
            && rPoint.X() <= aBottomRight.X() && rPoint.Y() <= aBottomRight.Y())
        {
            nIndex = i;
            break;
        }
    }
    return nIndex;
}

Pair TextLayoutData::GetLineStartEnd(tools::Long nLine) const
{
    Pair aPair(-1, -1);

    int nDisplayLines = m_aLineIndices.size();
    if (nLine >= 0 && nLine < nDisplayLines)
    {
        aPair.A() = m_aLineIndices[nLine];
        if (nLine + 1 < nDisplayLines)
            aPair.B() = m_aLineIndices[nLine + 1] - 1;
        else
            aPair.B() = m_aDisplayText.getLength() - 1;
    }
    else if (nLine == 0 && nDisplayLines == 0 && !m_aDisplayText.isEmpty())
    {
        // special case for single line controls so the implementations
        // in that case do not have to fill in the line indices
        aPair.A() = 0;
        aPair.B() = m_aDisplayText.getLength() - 1;
    }
    return aPair;
}

tools::Long TextLayoutData::ToRelativeLineIndex(tools::Long nIndex) const
{
    // is the index sensible at all ?
    if (nIndex >= 0 && nIndex < m_aDisplayText.getLength())
    {
        int nDisplayLines = m_aLineIndices.size();
        // if only 1 line exists, then absolute and relative index are
        // identical -> do nothing
        if (nDisplayLines > 1)
        {
            int nLine;
            for (nLine = nDisplayLines - 1; nLine >= 0; nLine--)
            {
                if (m_aLineIndices[nLine] <= nIndex)
                {
                    nIndex -= m_aLineIndices[nLine];
                    break;
                }
            }
            if (nLine < 0)
            {
                SAL_WARN_IF(nLine < 0, "vcl", "ToRelativeLineIndex failed");
                nIndex = -1;
            }
        }
    }
    else
    {
        nIndex = -1;
    }

    return nIndex;
}

TextLayoutData::~TextLayoutData()
{
    if (m_pParent)
        m_pParent->ImplClearLayoutData();
}

} // namespace vcl::text

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
