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

#ifndef INCLUDED_VCL_SOURCE_GDI_IMPANMVW_HXX
#define INCLUDED_VCL_SOURCE_GDI_IMPANMVW_HXX

#include <vcl/animate/Animation.hxx>
#include <vcl/window.hxx>
#include <vcl/vclptr.hxx>

class Animation;
class VirtualDevice;
struct AnimationBitmap;

struct AInfo
{
    Point           aStartOrg;
    Size            aStartSize;
    VclPtr<RenderContext2>   pOutDev;
    void*           pViewData;
    tools::Long            nExtraData;
    bool            bPause;

    AInfo();
};


class ImplAnimView
{
private:
    friend class Animation;
    friend void RenderContext2::DrawAnimation(Animation *const, Point const&, Size const&) const;
    friend void RenderContext2::DrawAnimationView(ImplAnimView&, sal_uLong, VirtualDevice*);
    friend void RenderContext2::DrawAnimationViewToPos(ImplAnimView&, sal_uLong);
    friend void vcl::Window::DrawAnimationView(ImplAnimView&, sal_uLong, VirtualDevice*);
    friend void vcl::Window::DrawAnimationViewToPos(ImplAnimView&, sal_uLong);

    Animation*      mpParent;
    VclPtr<RenderContext2>  mpRenderContext;
    tools::Long            mnExtraData;
    Point           maPt;
    Point           maDispPt;
    Point           maRestPt;
    Size            maSz;
    Size            maSzPix;
    Size            maDispSz;
    Size            maRestSz;
    vcl::Region     maClip;
    VclPtr<VirtualDevice>  mpBackground;
    VclPtr<VirtualDevice>  mpRestore;
    sal_uLong       mnActPos;
    Disposal        meLastDisposal;
    bool            mbIsPaused;
    bool            mbIsMarked;
    bool            mbIsMirroredHorizontally;
    bool            mbIsMirroredVertically;

public:
                    ~ImplAnimView();
private:
                    ImplAnimView( Animation* pParent, RenderContext2* pOut,
                                  const Point& rPt, const Size& rSz, sal_uLong nExtraData,
                                  RenderContext2* pFirstFrameOutDev = nullptr );

    bool            matches(const RenderContext2* pOut, tools::Long nExtraData) const;
    void            repaint();
    AInfo*          createAInfo() const;

    void            getPosSize( const AnimationBitmap& rAnm, Point& rPosPix, Size& rSizePix );

    const Point&    getOutPos() const { return maPt; }

    const Size&     getOutSizePix() const { return maSzPix; }

    void            pause( bool bIsPaused ) { mbIsPaused = bIsPaused; }
    bool            isPause() const { return mbIsPaused; }

    void            setMarked( bool bIsMarked ) { mbIsMarked = bIsMarked; }
    bool            isMarked() const { return mbIsMarked; }
};

#endif

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
