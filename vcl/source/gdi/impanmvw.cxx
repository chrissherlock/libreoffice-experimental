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

#include <memory>
#include <impanmvw.hxx>

#include <vcl/virdev.hxx>
#include <vcl/window.hxx>
#include <tools/helpers.hxx>

#include <window.h>

ImplAnimView::ImplAnimView( Animation* pParent, RenderContext2* pOut,
                            const Point& rPt, const Size& rSz,
                            sal_uLong nExtraData,
                            OutputDevice* pFirstFrameOutDev ) :
        mpParent        ( pParent ),
        mpRenderContext ( pFirstFrameOutDev ? pFirstFrameOutDev : pOut ),
        mnExtraData     ( nExtraData ),
        maPt            ( rPt ),
        maSz            ( rSz ),
        maSzPix         ( mpRenderContext->LogicToPixel( maSz ) ),
        maClip          ( mpRenderContext->GetClipRegion() ),
        mpBackground    ( VclPtr<VirtualDevice>::Create() ),
        mpRestore       ( VclPtr<VirtualDevice>::Create() ),
        meLastDisposal  ( Disposal::Back ),
        mbIsPaused      ( false ),
        mbIsMarked      ( false ),
        mbIsMirroredHorizontally         ( maSz.Width() < 0 ),
        mbIsMirroredVertically         ( maSz.Height() < 0 )
{
    Animation::ImplIncAnimCount();

    // Mirrored horizontally?
    if( mbIsMirroredHorizontally )
    {
        maDispPt.setX( maPt.X() + maSz.Width() + 1 );
        maDispSz.setWidth( -maSz.Width() );
        maSzPix.setWidth( -maSzPix.Width() );
    }
    else
    {
        maDispPt.setX( maPt.X() );
        maDispSz.setWidth( maSz.Width() );
    }

    // Mirrored vertically?
    if( mbIsMirroredVertically )
    {
        maDispPt.setY( maPt.Y() + maSz.Height() + 1 );
        maDispSz.setHeight( -maSz.Height() );
        maSzPix.setHeight( -maSzPix.Height() );
    }
    else
    {
        maDispPt.setY( maPt.Y() );
        maDispSz.setHeight( maSz.Height() );
    }

    // save background
    mpBackground->SetOutputSizePixel( maSzPix );
    mpRenderContext->SaveBackground(*mpBackground, maDispPt, maDispSz, maSzPix);

    // Initialize drawing to actual position
    mpRenderContext->DrawAnimationViewToPos(*this, mpParent->ImplGetCurPos());

    // If first frame OutputDevice is set, update variables now for real OutputDevice
    if( pFirstFrameOutDev )
    {
        mpRenderContext = pOut;
        maClip = mpRenderContext->GetClipRegion();
    }
}

ImplAnimView::~ImplAnimView()
{
    mpBackground.disposeAndClear();
    mpRestore.disposeAndClear();

    Animation::ImplDecAnimCount();
}

bool ImplAnimView::matches(const OutputDevice* pOut, tools::Long nExtraData) const
{
    return (!pOut || pOut == mpRenderContext) && (nExtraData == 0 || nExtraData == mnExtraData);
}

void ImplAnimView::getPosSize( const AnimationBitmap& rAnimationBitmap, Point& rPosPix, Size& rSizePix )
{
    const Size& rAnmSize = mpParent->GetDisplaySizePixel();
    Point       aPt2( rAnimationBitmap.maPositionPixel.X() + rAnimationBitmap.maSizePixel.Width() - 1,
                      rAnimationBitmap.maPositionPixel.Y() + rAnimationBitmap.maSizePixel.Height() - 1 );
    double      fFactX, fFactY;

    // calculate x scaling
    if( rAnmSize.Width() > 1 )
        fFactX = static_cast<double>( maSzPix.Width() - 1 ) / ( rAnmSize.Width() - 1 );
    else
        fFactX = 1.0;

    // calculate y scaling
    if( rAnmSize.Height() > 1 )
        fFactY = static_cast<double>( maSzPix.Height() - 1 ) / ( rAnmSize.Height() - 1 );
    else
        fFactY = 1.0;

    rPosPix.setX( FRound( rAnimationBitmap.maPositionPixel.X() * fFactX ) );
    rPosPix.setY( FRound( rAnimationBitmap.maPositionPixel.Y() * fFactY ) );

    aPt2.setX( FRound( aPt2.X() * fFactX ) );
    aPt2.setY( FRound( aPt2.Y() * fFactY ) );

    rSizePix.setWidth( aPt2.X() - rPosPix.X() + 1 );
    rSizePix.setHeight( aPt2.Y() - rPosPix.Y() + 1 );

    // Mirrored horizontally?
    if( mbIsMirroredHorizontally )
        rPosPix.setX( maSzPix.Width() - 1 - aPt2.X() );

    // Mirrored vertically?
    if( mbIsMirroredVertically )
        rPosPix.setY( maSzPix.Height() - 1 - aPt2.Y() );
}

void ImplAnimView::repaint()
{
    const bool bOldPause = mbIsPaused;

    mpRenderContext->SaveBackground(*mpBackground, maDispPt, maDispSz, maSzPix);

    mbIsPaused = false;
    mpRenderContext->DrawAnimationViewToPos(*this, mnActPos);
    mbIsPaused = bOldPause;
}

AInfo* ImplAnimView::createAInfo() const
{
    AInfo* pAInfo = new AInfo;

    pAInfo->aStartOrg = maPt;
    pAInfo->aStartSize = maSz;
    pAInfo->pOutDev = mpRenderContext;
    pAInfo->pViewData = const_cast<ImplAnimView *>(this);
    pAInfo->nExtraData = mnExtraData;
    pAInfo->bPause = mbIsPaused;

    return pAInfo;
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
