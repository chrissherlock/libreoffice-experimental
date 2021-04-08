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

enum class GlyphItemFlags
{
    NONE = 0,
    IS_IN_CLUSTER = 0x001,
    IS_RTL_GLYPH = 0x002,
    IS_DIACRITIC = 0x004,
    IS_VERTICAL = 0x008,
    IS_SPACING = 0x010,
    ALLOW_KASHIDA = 0x020,
    IS_DROPPED = 0x040,
    IS_CLUSTER_START = 0x080
};
namespace o3tl
{
template <> struct typed_flags<GlyphItemFlags> : is_typed_flags<GlyphItemFlags, 0xff>
{
};
};

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
