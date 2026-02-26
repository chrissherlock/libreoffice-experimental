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

#include <vcl/text/TextEffects.hxx>

namespace vcl::text
{
ReliefColors TextEffects::GetReliefColors(Color aTextColor, Color aLineColor, Color aOverlineColor)
{
    ReliefColors aColors;

    // Black text is always drawn on white in VCL logic
    aColors.maTextColor = (aTextColor == COL_BLACK) ? Color(COL_WHITE) : aTextColor;

    // Relief color is black for white text, otherwise light gray
    aColors.maReliefColor
        = (aColors.maTextColor == COL_WHITE) ? Color(COL_BLACK) : Color(COL_LIGHTGRAY);

    aColors.maLineColor = (aLineColor == COL_BLACK) ? Color(COL_WHITE) : aLineColor;
    aColors.maOverlineColor = (aOverlineColor == COL_BLACK) ? Color(COL_WHITE) : aOverlineColor;

    return aColors;
}

Color TextEffects::GetShadowColor(Color aTextColor)
{
    if ((aTextColor == COL_BLACK) || (aTextColor.GetLuminance() < 8))
        return Color(COL_LIGHTGRAY);
    else
        return Color(COL_BLACK);
}

} // namespace vcl::text

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
