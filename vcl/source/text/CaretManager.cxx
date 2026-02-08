/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <vcl/text/CaretManager.hxx>
#include <sallayout.hxx>
#include <CoordinateMapper.hxx>

namespace vcl::text
{
static double lcl_mirrorCoord(double nTotalWidth, double nOldPos)
{
    return nTotalWidth - nOldPos - 1.0;
}

void CaretManager::MirrorCaretPositions(std::vector<double>& rCaretPixelPos, double nWidth)
{
    for (double& rPos : rCaretPixelPos)
    {
        rPos = lcl_mirrorCoord(nWidth, rPos);
    }
}

void CaretManager::ConvertPixelsToLogic(const CoordinateMapper& rMapper,
                                        std::vector<double>& rCaretPixelPos)
{
    if (!rMapper.IsMapModeEnabled())
        return;

    for (double& rPos : rCaretPixelPos)
    {
        rPos = rMapper.DevicePixelToLogicWidthDouble(rPos);
    }
}

void CaretManager::GetCaretPositions(const LayoutResources& rRes, const TextSpan& rSpan,
                                     std::vector<double>& rCaretPositions, const SalLayout& rLayout)
{
    rLayout.GetCaretPositions(rCaretPositions, rSpan.Text);

    if (rRes.bRTLEnabled)
        MirrorCaretPositions(rCaretPositions, rLayout.GetTextWidth());

    ConvertPixelsToLogic(rRes.rMapper, rCaretPositions);

    const int nCaretPos = static_cast<int>(rCaretPositions.size());
    int nFirstValidIndex = nCaretPos;

    // Find first valid coordinate
    for (int i = 0; i < nCaretPos; ++i)
    {
        if (rCaretPositions[i] >= 0)
        {
            nFirstValidIndex = i;
            break;
        }
    }

    // Propagate coordinates forward
    double nXPos = (nFirstValidIndex < nCaretPos) ? rCaretPositions[nFirstValidIndex] : -1.0;
    for (int i = 0; i < nCaretPos; ++i)
    {
        if (rCaretPositions[i] >= 0)
            nXPos = rCaretPositions[i];
        else
            rCaretPositions[i] = nXPos;
    }
}

} // namespace vcl::text
