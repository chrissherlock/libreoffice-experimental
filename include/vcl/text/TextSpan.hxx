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

#include <vcl/dllapi.h>

namespace vcl::text
{
/**
 * Represents a substring within a larger OUString context.
 * Used to avoid unnecessary string allocations during layout and analysis.
 */
struct VCL_DLLPUBLIC TextSpan
{
    OUString Text;
    sal_Int32 Index;
    sal_Int32 Length;

    TextSpan(OUString aText, sal_Int32 nIndex, sal_Int32 nLength)
        : Text(std::move(aText))
        , Index(nIndex)
        , Length(nLength)
    {
    }
};

} // namespace vcl::text
