/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 */

#ifndef INCLUDED_INCLUDE_VCL_DRAWABLES_GRIDRECTDRAWABLE_HXX
#define INCLUDED_INCLUDE_VCL_DRAWABLES_GRIDRECTDRAWABLE_HXX

#include <tools/gen.hxx>

#include <vcl/drawables/Drawable.hxx>

#include <o3tl/typed_flags_set.hxx>

class OutputDevice;

enum class DrawGridFlags
{
    NONE = 0x0000,
    Dots = 0x0001,
    HorzLines = 0x0002,
    VertLines = 0x0004
};
namespace o3tl
{
template <> struct typed_flags<DrawGridFlags> : is_typed_flags<DrawGridFlags, 0x0007>
{
};
}

class VCL_DLLPUBLIC GridRectDrawable : public Drawable
{
public:
    GridRectDrawable(tools::Rectangle aRect, Size aDistance, DrawGridFlags eFlags)
        : maRect(aRect)
        , maDistance(aDistance)
        , meFlags(eFlags)
    {
        mpMetaAction = nullptr;
    }

protected:
    bool CanDraw(OutputDevice*) const override;
    bool ShouldInitClipRegion() const override { return false; }

    bool DrawCommand(OutputDevice* pRenderContext) const override;

private:
    tools::Rectangle maRect;
    Size maDistance;
    DrawGridFlags meFlags;
};

#endif

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
