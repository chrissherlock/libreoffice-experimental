/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <vector>
#include <rtl/ustring.hxx>

class SalLayout;
namespace tools
{
class Rectangle;
}
namespace vcl
{
class Region;
}

namespace vcl::text
{
// Shared implementation for filtering glyphs against a clip region
// and appending them to the output vector.
// Also handles appending the corresponding text if pOutText is provided.
void FilterAndAppend(const SalLayout* pLayout, const OUString& rStr, sal_Int32 nIndex,
                     sal_Int32 nLen, const vcl::Region& rClip,
                     std::vector<tools::Rectangle>& rOutRects, OUString* pOutText = nullptr);

} // namespace vcl::text

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
