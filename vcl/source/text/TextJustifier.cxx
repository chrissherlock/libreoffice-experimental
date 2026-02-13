/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <text/TextJustifier.hxx>
#include <sallayout.hxx>
#include <unicode/uchar.h>

namespace vcl::text
{
TextJustifier::TextJustifier() {}

TextJustifier::~TextJustifier() {}

void TextJustifier::GetWordKashidaPositions(const SalLayout& rLayout, std::u16string_view rText,
                                            std::vector<bool>& rOutMap)
{
    rOutMap.clear();
    if (!rLayout.HasFontKashidaPositions())
        return;

    size_t nEnd = rText.length();
    rOutMap.resize(nEnd, false);

    for (size_t i = 0; i < nEnd; ++i)
    {
        size_t nNextPos = i + 1;
        while (nNextPos < nEnd
               && u_getIntPropertyValue(rText[nNextPos], UCHAR_JOINING_TYPE) == U_JT_TRANSPARENT)
            ++nNextPos;

        rOutMap[i] = rLayout.IsKashidaPosValid(i, nNextPos);
    }
}

} // namespace vcl::text
