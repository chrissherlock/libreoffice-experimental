/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
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

#pragma once

#include <o3tl/typed_flags_set.hxx>

enum class FontType
{
    NO_FONT = 0,
    SFNT_TTF = 1 << 1, ///< SFNT container with TrueType glyphs
    SFNT_CFF = 1 << 2, ///< SFNT container with CFF-container
    TYPE1_PFA = 1 << 3, ///< PSType1 Postscript Font Ascii
    TYPE1_PFB = 1 << 4, ///< PSType1 Postscript Font Binary
    CFF_FONT = 1 << 5, ///< CFF-container with PSType2 glyphs
    TYPE3_FONT = 1 << 6, ///< PSType3 Postscript font
    TYPE42_FONT = 1 << 7, ///< PSType42 wrapper for an SFNT_TTF
    ANY_SFNT = SFNT_TTF | SFNT_CFF,
    ANY_TYPE1 = TYPE1_PFA | TYPE1_PFB
};
namespace o3tl
{
template <> struct typed_flags<FontType> : is_typed_flags<FontType, (1 << 8) - 1>
{
};
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
