/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <sal/log.hxx>

#include <vcl/glyphitemcache.hxx>
#include <vcl/text/LayoutCacheData.hxx>

#include <cassert>

namespace vcl::text
{
void LayoutCacheData::ValidateGlyphCache(const SalLayoutGlyphs* pGlyphs)
{
    if (!pGlyphs)
        return;

    if (!pGlyphs->IsValid())
    {
        SAL_WARN("vcl", "Trying to setup invalid cached glyphs - falling back to relayout!");
        return;
    }

#ifdef DBG_UTIL
    for (int level = 0;; ++level)
    {
        SalLayoutGlyphsImpl* glyphsImpl = pGlyphs->Impl(level);
        if (glyphsImpl == nullptr)
            break;
        assert(glyphsImpl->GetFlags() & SalLayoutFlags::GlyphItemsOnly);
    }
#endif
}
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
