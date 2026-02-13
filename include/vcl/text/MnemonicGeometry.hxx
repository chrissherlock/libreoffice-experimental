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
#include <rtl/ustring.hxx>

#include <tools/long.hxx>

namespace vcl::text
{
struct MnemonicGeometry
{
    tools::Long nX;
    tools::Long nY;
    tools::Long nWidth;
};

struct SAL_DLLPUBLIC MnemonicText
{
    OUString aText;
    sal_Int32 nIndex;
    sal_Int32 nLen;
    sal_Int32 nMnemonicPos;
};

struct MnemonicDeviceParams
{
    tools::Long nLogicalAscent;
    tools::Long nOutOffX;
    tools::Long nOutOffY;
};
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
