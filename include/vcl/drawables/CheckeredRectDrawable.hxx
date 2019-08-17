/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 */

#ifndef INCLUDED_INCLUDE_VCL_DRAWABLES_CHECKEREDRECTDRAWABLE_HXX
#define INCLUDED_INCLUDE_VCL_DRAWABLES_CHECKEREDRECTDRAWABLE_HXX

#include <tools/color.hxx>
#include <tools/gen.hxx>

#include <vcl/metaact.hxx>
#include <vcl/drawables/Drawable.hxx>

class OutputDevice;

class VCL_DLLPUBLIC CheckeredRectDrawable : public Drawable
{
public:
    CheckeredRectDrawable(Point aPos, Size aSize, sal_uInt32 nLen = 8,
                          Color aStartColor = COL_WHITE, Color aEndColor = COL_BLACK)
        : Drawable(false)
        , maPos(aPos)
        , maSize(aSize)
        , mnLen(nLen)
        , maStartColor(aStartColor)
        , maEndColor(aEndColor)
    {
        mpMetaAction = nullptr;
    }

protected:
    virtual bool DrawCommand(OutputDevice* pRenderContext) const override;

private:
    Point maPos;
    Size maSize;
    sal_uInt32 mnLen;
    Color maStartColor;
    Color maEndColor;
};

#endif

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
