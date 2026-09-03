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
#include <vcl/vclenum.hxx>

struct WindowLayoutData
{
    sal_Int32 mnWidthRequest = -1;
    sal_Int32 mnHeightRequest = -1;
    sal_Int32 mnOptimalWidthCache = -1;
    sal_Int32 mnOptimalHeightCache = -1;

    std::shared_ptr<VclSizeGroup> m_xSizeGroup;

    VclAlign meHalign = VclAlign::Fill;
    VclAlign meValign = VclAlign::Fill;
    VclPackType mePackType = VclPackType::Start;

    bool mbSecondary = false;
    bool mbHexpand = false;
    bool mbVexpand = false;
    bool mbExpand = false;
    bool mbFill = false;
    bool mbNonHomogeneous = false;

    sal_Int32 mnPadding = 0;
    sal_Int32 mnGridHeight = 1;
    sal_Int32 mnGridLeftAttach = -1;
    sal_Int32 mnGridTopAttach = -1;
    sal_Int32 mnGridWidth = 1;
    sal_Int32 mnBorderWidth = 0;
    sal_Int32 mnMarginLeft = 0;
    sal_Int32 mnMarginRight = 0;
    sal_Int32 mnMarginTop = 0;
    sal_Int32 mnMarginBottom = 0;

    WindowLayoutData() = default;
};

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
