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
#include <AnimationRenderers.hxx>
#include <AnimationRenderer.hxx>

std::vector<std::unique_ptr<AnimationRenderer>>& AnimationRenderers::GetRenderers()
{
    return maAnimationRenderers;
}

size_t AnimationRenderers::GetSize() { return maAnimationRenderers.size(); }

void AnimationRenderers::RemoveRenderers(OutputDevice* pOut, long nCallerId)
{
    GetRenderers().erase(
        std::remove_if(GetRenderers().begin(), GetRenderers().end(),
                       [=](const std::unique_ptr<AnimationRenderer>& pAnimView) -> bool {
                           return pAnimView->matches(pOut, nCallerId);
                       }),
        GetRenderers().end());
}

void AnimationRenderers::ClearAnimationRenderers() { GetRenderers().clear(); }

bool AnimationRenderers::RepaintRenderers(OutputDevice* pOut, sal_uLong nCallerId, const Point& rDestPt,
                                          const Size& rDestSz)
{
    bool bRepainted = true;

    // find first matching renderer that holds same rendercontext and caller id
    auto itAnimRenderer = std::find_if(
        GetRenderers().begin(), GetRenderers().end(),
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
            GetRenderers().erase(itAnimRenderer);
        }
    }

    return bRepainted;
}

bool AnimationRenderers::NoRenderersAreAvailable() { return GetRenderers().empty(); }

std::vector<std::unique_ptr<AnimationData>>
AnimationRenderers::CreateAnimationDataItems(Animation* pAnim)
{
    std::vector<std::unique_ptr<AnimationData>> aAnimationDataItems;

    for (auto const& rItem : GetRenderers())
    {
        aAnimationDataItems.emplace_back(rItem->createAnimationData());
    }

    pAnim->NotifyTimeout();

    return aAnimationDataItems;
}

void AnimationRenderers::CreateDefaultRenderer(Animation* pAnim, OutputDevice* pOut,
                                               const Point& rDestPt, const Size& rDestSz,
                                               sal_uLong nCallerId, OutputDevice* pFirstFrameOutDev)
{
    GetRenderers().emplace_back(
        new AnimationRenderer(pAnim, pOut, rDestPt, rDestSz, nCallerId, pFirstFrameOutDev));
}

void AnimationRenderers::PopulateRenderers(Animation* pAnim)
{
    for (auto& pAnimationData : CreateAnimationDataItems(pAnim))
    {
        AnimationRenderer* pRenderer = nullptr;
        if (!pAnimationData->pAnimationRenderer)
        {
            pRenderer
                = new AnimationRenderer(pAnim, pAnimationData->pOutDev, pAnimationData->aStartOrg,
                                        pAnimationData->aStartSize, pAnimationData->nCallerId);

            GetRenderers().push_back(std::unique_ptr<AnimationRenderer>(pRenderer));
        }
        else
        {
            pRenderer = static_cast<AnimationRenderer*>(pAnimationData->pAnimationRenderer);
        }

        pRenderer->pause(pAnimationData->bPause);
        pRenderer->setMarked(true);
    }
}

void AnimationRenderers::DeleteUnmarkedRenderers()
{
    // delete all unmarked views
    auto removeStart = std::remove_if(GetRenderers().begin(), GetRenderers().end(),
                                      [](const auto& pRenderer) { return !pRenderer->isMarked(); });
    GetRenderers().erase(removeStart, GetRenderers().cend());
}

bool AnimationRenderers::ResetMarkedRenderers()
{
    bool bGlobalPause = std::all_of(GetRenderers().cbegin(), GetRenderers().cend(),
                                    [](const auto& pRenderer) { return pRenderer->isPause(); });

    // reset marked state
    std::for_each(GetRenderers().cbegin(), GetRenderers().cend(),
                  [](const auto& pRenderer) { pRenderer->setMarked(false); });

    return bGlobalPause;
}

void AnimationRenderers::PaintRenderers(size_t nFrameIndex)
{
    std::for_each(GetRenderers().cbegin(), GetRenderers().cend(),
                  [nFrameIndex](const auto& pRenderer) { pRenderer->draw(nFrameIndex); });
}

void AnimationRenderers::EraseMarkedRenderers()
{
    // if view is marked; in this case remove view, because area
    // of output lies out of display area of window; mark state is
    // set from view itself
    auto removeStart = std::remove_if(GetRenderers().begin(), GetRenderers().end(),
                                      [](const auto& pRenderer) { return pRenderer->isMarked(); });
    GetRenderers().erase(removeStart, GetRenderers().cend());
}
