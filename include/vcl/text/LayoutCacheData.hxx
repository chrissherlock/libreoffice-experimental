/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <sal/types.h>

#include <memory>

class SalLayout;
class SalLayoutGlyphs;

namespace vcl::text
{
class TextLayoutCache;

/**
 * Encapsulates cached layout results (glyphs and widths) to avoid
 * redundant layout calculations during caret positioning and rendering.
 */
struct LayoutCacheData
{
    const TextLayoutCache* pCache = nullptr;
    const SalLayoutGlyphs* pGlyphs = nullptr;
};

} // namespace vcl::text
