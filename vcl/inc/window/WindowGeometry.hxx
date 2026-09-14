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
#include <tools/gen.hxx>

struct WindowGeometry
{
    sal_Int32 mnLeftBorder = 0;
    sal_Int32 mnTopBorder = 0;
    sal_Int32 mnRightBorder = 0;
    sal_Int32 mnBottomBorder = 0;

    tools::Long mnX = 0;
    tools::Long mnY = 0;
    tools::Long mnAbsScreenX = 0;
    Point maPos;

    bool mbDefPos : 1 = true;
    bool mbDefSize : 1 = true;
    bool mbCallMove : 1 = true;
    bool mbCallResize : 1 = true;
    bool mbWaitSystemResize : 1 = true;
    bool mbAllResize : 1 = false;

    WindowGeometry() = default;
};

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
