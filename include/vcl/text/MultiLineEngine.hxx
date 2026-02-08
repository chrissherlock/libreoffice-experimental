/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <vcl/dllapi.h>
#include <tools/gen.hxx>
#include <vcl/outdev.hxx>
#include <functional>

namespace vcl::text
{
class VCL_DLLPUBLIC MultiLineEngine
{
public:
    static OUString
    GetEllipsisString(const OUString& rStr, tools::Long nMaxWidth, DrawTextFlags nStyle,
                      const std::function<tools::Long(const OUString&)>& rfnGetTextWidth);
};

} // namespace vcl::text
