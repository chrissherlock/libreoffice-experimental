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

#ifndef INCLUDED_VCL_INC_ANIMATIONRENDERERS_HXX
#define INCLUDED_VCL_INC_ANIMATIONRENDERERS_HXX

#include <vcl/dllapi.h>

#include <vector>

class Animation;
class AnimationRenderer;
struct AnimationData;
class OutputDevice;
class Point;
class Size;

class VCL_DLLPUBLIC AnimationRenderers
{
public:
    std::vector<std::unique_ptr<AnimationData>> CreateAnimationDataItems(Animation* pAnim);
    void PopulateRenderers(Animation* pAnim);
    void DeleteUnmarkedRenderers();
    bool ResetMarkedRenderers();
    bool IsTimeoutSetup();
    bool SendTimeout();
    void PaintRenderers(size_t nFrameIndex);
    void EraseMarkedRenderers();
    AnimationBitmap* GetNextFrameBitmap();
    void RenderNextFrame();
    void ClearAnimationRenderers();
    bool RepaintRenderers(OutputDevice* pOut, sal_uLong nCallerId, const Point& rDestPt,
                          const Size& rDestSz);
    bool NoRenderersAreAvailable();
    void CreateDefaultRenderer(Animation* pAnim, OutputDevice* pOut, const Point& rDestPt,
                               const Size& rDestSz, sal_uLong nCallerId,
                               OutputDevice* pFirstFrameOutDev);
    void RemoveRenderers(OutputDevice* pOut, long nCallerId);

    std::vector<std::unique_ptr<AnimationRenderer>>& GetRenderers();

private:
    std::vector<std::unique_ptr<AnimationRenderer>> maAnimationRenderers;
};

#endif // INCLUDED_VCL_INC_ANIMATIONRENDERERS_HXX

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
