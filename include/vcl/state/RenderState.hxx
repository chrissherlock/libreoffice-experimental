/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <tools/color.hxx>
#include <o3tl/typed_flags_set.hxx>

#include <vcl/rendercontext/DrawModeFlags.hxx>
#include <vcl/vclenum.hxx>

namespace vcl::rstate
{
enum class RenderChangeMask : uint32_t
{
    None = 0,
    LineColor = 1 << 0,
    FillColor = 1 << 1,
    RasterOp = 1 << 2,
    DrawMode = 1 << 3,
    All = ~0U
};
}

namespace o3tl
{
template <>
struct typed_flags<vcl::rstate::RenderChangeMask>
    : is_typed_flags<vcl::rstate::RenderChangeMask, 0x07>
{
};
}

namespace vcl::rstate
{
inline RenderChangeMask operator|(RenderChangeMask a, RenderChangeMask b)
{
    return static_cast<RenderChangeMask>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}
inline bool operator&(RenderChangeMask a, RenderChangeMask b)
{
    return (static_cast<uint32_t>(a) & static_cast<uint32_t>(b)) != 0;
}

struct RenderState
{
    uint64_t epoch = 1;
    uint64_t lastSyncedEpoch = 0;
    RenderChangeMask changeMask = RenderChangeMask::All;

    Color lineColor = COL_BLACK;
    bool bLineColorSet = true;

    Color fillColor = COL_WHITE;
    bool bFillColorSet = true;

    RasterOp rasterOp = RasterOp::OverPaint;
    DrawModeFlags drawMode = DrawModeFlags::Default;
};

} // namespace vcl::state

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
