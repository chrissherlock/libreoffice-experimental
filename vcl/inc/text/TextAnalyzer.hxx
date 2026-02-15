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
#include <vcl/outdev.hxx>
#include <vcl/rendercontext/SalLayoutFlags.hxx>

#include <text/TextLayoutEngine.hxx>

class SalLayoutGlyphs;

namespace vcl
{
struct GraphicsState;
}
namespace vcl::font
{
struct FontRealization;
}

namespace vcl::text
{
class TextLayoutCache;

/**
 * TextAnalyzer handles pure string analysis, linguistic processing,
 * and flag calculation. It is stateless and does not depend on SalLayout.
 */
class VCL_DLLPUBLIC TextAnalyzer
{
public:
    static SalLayoutFlags GetBiDiLayoutFlags(vcl::text::ComplexTextLayoutFlags eLayoutMode,
                                             std::u16string_view rStr, sal_Int32 nMinIndex,
                                             sal_Int32 nEndIndex);

    static SalLayoutFlags CalculateLayoutFlags(const vcl::GraphicsState& rGraphicsState,
                                               const vcl::font::FontRealization& rFontRealization,
                                               bool bRTLWindow, std::u16string_view rStr,
                                               sal_Int32 nMinIndex, sal_Int32 nEndIndex,
                                               SalLayoutFlags nExistingFlags);

    static void ApplyDigitLocalization(const vcl::GraphicsState& rGraphicsState, OUString& rStr,
                                       sal_Int32 nMinIndex, sal_Int32& rEndIndex);

    static MnemonicText PrepareMnemonicText(const OUString& rStr, sal_Int32 nIndex, sal_Int32 nLen);

    static bool IsMnemonicInRange(sal_Int32 nMnemonicPos, sal_Int32 nIndex, sal_Int32 nLen);

    static sal_Int32 GetNormalizedLength(const OUString& rStr, sal_Int32 nIdx, sal_Int32 nLen);

    static bool GetTextIsRTL(const LayoutResources& rRes, const OUString& rString, sal_Int32 nIndex,
                             sal_Int32 nLen);

    static bool PrepareNormalizedLayoutInput(const OUString& rOrigStr, sal_Int32 nMinIndex,
                                             sal_Int32& rLen, OUString& rStr,
                                             const ::vcl::font::FontRealization& rFontRealization,
                                             const vcl::text::TextLayoutCache*& rpLayoutCache,
                                             const SalLayoutGlyphs*& rpGlyphs);
};

} // namespace vcl::text
