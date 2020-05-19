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

#include <vcl/animate/Animation.hxx>
#include <vcl/outdev.hxx>

#include <AnimationData.hxx>
#include <AnimationRenderer.hxx>

void Animation::RemoveRenderers(OutputDevice* pOut, long nCallerId)
{
    maAnimationRenderers.erase(
        std::remove_if(maAnimationRenderers.begin(), maAnimationRenderers.end(),
                       [=](const std::unique_ptr<AnimationRenderer>& pAnimView) -> bool {
                           return pAnimView->matches(pOut, nCallerId);
                       }),
        maAnimationRenderers.end());
}

void Animation::ClearAnimationRenderers() { maAnimationRenderers.clear(); }

bool Animation::RepaintRenderers(OutputDevice* pOut, sal_uLong nCallerId, const Point& rDestPt,
                                 const Size& rDestSz)
{
    bool bRepainted = false;

    // find first matching renderer that holds same rendercontext and caller id
    auto itAnimRenderer = std::find_if(
        maAnimationRenderers.begin(), maAnimationRenderers.end(),
        [pOut, nCallerId](const std::unique_ptr<AnimationRenderer>& pRenderer) -> bool {
            return pRenderer->matches(pOut, nCallerId);
        });

    // matching renderer was found
    if (itAnimRenderer != maAnimationRenderers.end())
    {
        // same position and size? repaint otherwise remove it from the renderers list
        if ((*itAnimRenderer)->getOutPos() == rDestPt
            && (*itAnimRenderer)->getOutSizePix() == pOut->LogicToPixel(rDestSz))
        {
            (*itAnimRenderer)->repaint();
            bRepainted = true;
        }
        else
        {
            maAnimationRenderers.erase(itAnimRenderer);
        }
    }

    return bRepainted;
}

bool Animation::NoRenderersAreAvailable() { return maAnimationRenderers.empty(); }

std::vector<std::unique_ptr<AnimationData>> Animation::CreateAnimationDataItems(Animation* pAnim)
{
    std::vector<std::unique_ptr<AnimationData>> aAnimationDataItems;

    for (auto const& rItem : maAnimationRenderers)
    {
        aAnimationDataItems.emplace_back(rItem->createAnimationData());
    }

    maTimeoutNotifier.Call(pAnim);

    return aAnimationDataItems;
}

void Animation::CreateDefaultRenderer(Animation* pAnim, OutputDevice* pOut, const Point& rDestPt,
                                      const Size& rDestSz, sal_uLong nCallerId,
                                      OutputDevice* pFirstFrameOutDev)
{
    maAnimationRenderers.emplace_back(
        new AnimationRenderer(pAnim, pOut, rDestPt, rDestSz, nCallerId, pFirstFrameOutDev));
}

void Animation::PopulateRenderers(Animation* pAnim)
{
    for (auto& pAnimationData : CreateAnimationDataItems(pAnim))
    {
        AnimationRenderer* pRenderer = nullptr;
        if (!pAnimationData->pAnimationRenderer)
        {
            pRenderer
                = new AnimationRenderer(this, pAnimationData->pOutDev, pAnimationData->aStartOrg,
                                        pAnimationData->aStartSize, pAnimationData->nCallerId);

            maAnimationRenderers.push_back(std::unique_ptr<AnimationRenderer>(pRenderer));
        }
        else
        {
            pRenderer = static_cast<AnimationRenderer*>(pAnimationData->pAnimationRenderer);
        }

        pRenderer->pause(pAnimationData->bPause);
        pRenderer->setMarked(true);
    }
}

void Animation::DeleteUnmarkedRenderers()
{
    // delete all unmarked views
    auto removeStart = std::remove_if(maAnimationRenderers.begin(), maAnimationRenderers.end(),
                                      [](const auto& pRenderer) { return !pRenderer->isMarked(); });
    maAnimationRenderers.erase(removeStart, maAnimationRenderers.cend());
}

bool Animation::ResetMarkedRenderers()
{
    bool bGlobalPause = std::all_of(maAnimationRenderers.cbegin(), maAnimationRenderers.cend(),
                                    [](const auto& pRenderer) { return pRenderer->isPause(); });

    // reset marked state
    std::for_each(maAnimationRenderers.cbegin(), maAnimationRenderers.cend(),
                  [](const auto& pRenderer) { pRenderer->setMarked(false); });

    return bGlobalPause;
}

void Animation::PaintRenderers(size_t nFrameIndex)
{
    std::for_each(maAnimationRenderers.cbegin(), maAnimationRenderers.cend(),
                  [nFrameIndex](const auto& pRenderer) { pRenderer->draw(nFrameIndex); });
}

void Animation::EraseMarkedRenderers()
{
    // if view is marked; in this case remove view, because area
    // of output lies out of display area of window; mark state is
    // set from view itself
    auto removeStart = std::remove_if(maAnimationRenderers.begin(), maAnimationRenderers.end(),
                                      [](const auto& pRenderer) { return pRenderer->isMarked(); });
    maAnimationRenderers.erase(removeStart, maAnimationRenderers.cend());
}
