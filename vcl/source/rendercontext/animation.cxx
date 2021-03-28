/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <vcl/bitmapex.hxx>
#include <vcl/virdev.hxx>

#include <window.h>
#include <impanmvw.hxx>

bool RenderContext2::CanAnimate() { return false; }

void RenderContext2::DrawAnimation(Animation* const pAnim, Point const& rDestPt,
                                   Size const& rDestSz) const
{
    const size_t nCount = pAnim->Count();

    if (!nCount)
        return;

    sal_uLong nPos = pAnim->ImplGetCurPos();
    AnimationBitmap* pObj = const_cast<AnimationBitmap*>(&pAnim->Get(std::min(nPos, nCount - 1)));
    RenderContext2* pRC = const_cast<RenderContext2*>(this);

    if (pObj->mnWait == ANIMATION_TIMEOUT_ON_CLICK)
    {
        pAnim->GetBitmapEx().Draw(pRC, rDestPt, rDestSz);
    }
    else
    {
        const size_t nOldPos = nPos;

        if (pAnim->IsLoopTerminated())
            pAnim->ImplSetCurPos(nCount - 1);

        {
            ImplAnimView{ pAnim, pRC, rDestPt, rDestSz, 0 };
        }

        pAnim->ImplSetCurPos(nOldPos);
    }
}

void RenderContext2::DrawAnimationViewToPos(ImplAnimView& rAnimView, sal_uLong nPos)
{
    VclPtr<RenderContext2> pRenderContext = this;

    std::unique_ptr<vcl::PaintBufferGuard> pGuard;

    ScopedVclPtrInstance<VirtualDevice> aVDev;
    std::unique_ptr<vcl::Region> xOldClip(
        !rAnimView.maClip.IsNull() ? new vcl::Region(pRenderContext->GetClipRegion()) : nullptr);

    aVDev->SetOutputSizePixel(rAnimView.maSzPix, false);
    nPos = std::min(nPos, static_cast<sal_uLong>(rAnimView.mpParent->Count()) - 1);

    for (sal_uLong i = 0; i <= nPos; i++)
    {
        DrawAnimationView(rAnimView, i, aVDev.get());
    }

    if (xOldClip)
        pRenderContext->SetClipRegion(rAnimView.maClip);

    pRenderContext->DrawOutDev(rAnimView.maDispPt, rAnimView.maDispSz, Point(), rAnimView.maSzPix,
                               *aVDev);

    if (pGuard)
        pGuard->SetPaintRect(tools::Rectangle(rAnimView.maDispPt, rAnimView.maDispSz));

    if (xOldClip)
        pRenderContext->SetClipRegion(*xOldClip);
}

void RenderContext2::DrawAnimationView(ImplAnimView& rAnimView, sal_uLong nPos,
                                       VirtualDevice* pVDev)
{
    VclPtr<RenderContext2> pRenderContext = this;

    std::unique_ptr<vcl::PaintBufferGuard> pGuard;

    tools::Rectangle aOutRect(pRenderContext->PixelToLogic(Point()),
                              pRenderContext->GetOutputSize());

    // check, if output lies out of display
    if (aOutRect.Intersection(tools::Rectangle(rAnimView.maDispPt, rAnimView.maDispSz)).IsEmpty())
    {
        rAnimView.setMarked(true);
    }
    else if (!rAnimView.mbIsPaused)
    {
        VclPtr<VirtualDevice> pDev;
        Point aPosPix;
        Point aBmpPosPix;
        Size aSizePix;
        Size aBmpSizePix;
        const sal_uLong nLastPos = rAnimView.mpParent->Count() - 1;
        rAnimView.mnActPos = std::min(nPos, nLastPos);
        const AnimationBitmap& rAnimationBitmap
            = rAnimView.mpParent->Get(static_cast<sal_uInt16>(rAnimView.mnActPos));

        rAnimView.getPosSize(rAnimationBitmap, aPosPix, aSizePix);

        // Mirrored horizontally?
        if (rAnimView.mbIsMirroredHorizontally)
        {
            aBmpPosPix.setX(aPosPix.X() + aSizePix.Width() - 1);
            aBmpSizePix.setWidth(-aSizePix.Width());
        }
        else
        {
            aBmpPosPix.setX(aPosPix.X());
            aBmpSizePix.setWidth(aSizePix.Width());
        }

        // Mirrored vertically?
        if (rAnimView.mbIsMirroredVertically)
        {
            aBmpPosPix.setY(aPosPix.Y() + aSizePix.Height() - 1);
            aBmpSizePix.setHeight(-aSizePix.Height());
        }
        else
        {
            aBmpPosPix.setY(aPosPix.Y());
            aBmpSizePix.setHeight(aSizePix.Height());
        }

        // get output device
        if (!pVDev)
        {
            pDev = VclPtr<VirtualDevice>::Create();
            pDev->SetOutputSizePixel(rAnimView.maSzPix, false);
            pDev->DrawOutDev(Point(), rAnimView.maSzPix, rAnimView.maDispPt, rAnimView.maDispSz,
                             *pRenderContext);
        }
        else
        {
            pDev = pVDev;
        }

        // restore background after each run
        if (!nPos)
        {
            rAnimView.meLastDisposal = Disposal::Back;
            rAnimView.maRestPt = Point();
            rAnimView.maRestSz = rAnimView.maSzPix;
        }

        // restore
        if ((Disposal::Not != rAnimView.meLastDisposal) && rAnimView.maRestSz.Width()
            && rAnimView.maRestSz.Height())
        {
            if (Disposal::Back == rAnimView.meLastDisposal)
                pDev->DrawOutDev(rAnimView.maRestPt, rAnimView.maRestSz, rAnimView.maRestPt,
                                 rAnimView.maRestSz, *(rAnimView.mpBackground));
            else
                pDev->DrawOutDev(rAnimView.maRestPt, rAnimView.maRestSz, Point(),
                                 rAnimView.maRestSz, *(rAnimView.mpRestore));
        }

        rAnimView.meLastDisposal = rAnimationBitmap.meDisposal;
        rAnimView.maRestPt = aPosPix;
        rAnimView.maRestSz = aSizePix;

        // What do we need to restore the next time?
        // Put it into a bitmap if needed, else delete
        // SaveBitmap to conserve memory
        if ((rAnimView.meLastDisposal == Disposal::Back)
            || (rAnimView.meLastDisposal == Disposal::Not))
        {
            rAnimView.mpRestore->SetOutputSizePixel(Size(1, 1), false);
        }
        else
        {
            rAnimView.mpRestore->SetOutputSizePixel(rAnimView.maRestSz, false);
            rAnimView.mpRestore->DrawOutDev(Point(), rAnimView.maRestSz, aPosPix, aSizePix, *pDev);
        }

        pDev->DrawBitmapEx(aBmpPosPix, aBmpSizePix, rAnimationBitmap.maBitmapEx);

        if (!pVDev)
        {
            std::unique_ptr<vcl::Region> xOldClip(
                !rAnimView.maClip.IsNull() ? new vcl::Region(pRenderContext->GetClipRegion())
                                           : nullptr);

            if (xOldClip)
                pRenderContext->SetClipRegion(rAnimView.maClip);

            pRenderContext->DrawOutDev(rAnimView.maDispPt, rAnimView.maDispSz, Point(),
                                       rAnimView.maSzPix, *pDev);
            if (pGuard)
                pGuard->SetPaintRect(tools::Rectangle(rAnimView.maDispPt, rAnimView.maDispSz));

            if (xOldClip)
            {
                pRenderContext->SetClipRegion(*xOldClip);
                xOldClip.reset();
            }

            pDev.disposeAndClear();
            pRenderContext->Flush();
        }
    }
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
